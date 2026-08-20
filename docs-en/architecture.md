# Sefirah Lisp Architecture

**English** · [Português do Brasil](../docs-ptbr/arquitetura.md)

## Purpose

Sefirah is a live Lisp platform for desktop applications, not merely an
interpreter embedded in a GUI. The architecture keeps the language, compiler,
image, graphics, and native integration as explicit modules. Each layer can
therefore evolve without hiding ownership or depending on a single operating
system.

Main layers:

1. **Lisp core** — objects, packages, environments, reader, evaluator,
   primitives, conditions, streams, GC, and persistent images.
2. **Compiler** — verified SSA IR, reference interpreter, x86-64/AArch64
   backends, W^X JIT, and ELF/COFF/Mach-O writers.
3. **Graphics** — RGB surfaces, raster primitives, and a bitmap font without a
   window API dependency.
4. **GUI** — component tree, flexible layout, themes, hit testing, focus, and
   actions over the custom rasterizer.
5. **Platform** — presentation and events through X11, Win32, or
   Cocoa/CoreGraphics.
6. **CLI and IDE** — public commands and REPL in the CLI; the graphical
   development environment in a separate executable.

## Modules and dependencies

```text
sefirah_compilador
       ▲
       │
sefirah_nucleo ───────────────┐
                              ├── sefirah (CLI)
                              └── sefirah_ide_nucleo ── sefirah_ide (IDE)
sefirah_graficos ◄── sefirah_plataforma
       ▲                      │
       └──────────────────────┘
```

| Module | Knows about | Does not know about |
| --- | --- | --- |
| `nucleo` | objects, compiler, and process resources | windows and widgets |
| `compilador` | IR, ABIs, and object formats | packages, GUI, and IDE |
| `graficos` | pixels, shapes, and bitmap text | X11, Win32, and Cocoa |
| `plataforma` | native windows, presentation, and events | Lisp evaluation |
| `cli` | public runtime APIs and text commands | GUI, platform, and heap internals |
| `ide_nucleo` | runtime and editor/listener session through `ide/ide.h` | windows and platform events |
| `ide` | session engine, GUI, and platform | CLI commands and core internals |

Each build compiles exactly one window backend. The macOS backend remains pure
C and confines typed Objective-C runtime calls to the platform adapter.
`sefirah/interno.h` groups private core contracts for the build and does not
promise SDK or ABI stability; it is not installed, and applications must use
`sefirah/runtime.h`.

## Language flow

```text
.lisp text
    │
    ▼
 reader ──► Lisp form ──► evaluator ──► heap value
                                  │
                                  ├── lexical/global environment
                                  ├── value/function cell
                                  ├── condition and non-local control
                                  └── persistent image
```

The reader and evaluator are the reference implementation during bootstrap.
Symbols have separate value and function cells. Packages, vectors, characters,
and hash tables are also heap objects and preserve identity and references in
an image. Strings store UTF-8 and the Lisp API indexes them by code point.

Multiple values live in explicit runtime state. Ordinary argument positions
consume the primary value, while multiple-value forms and non-local transfers
preserve the complete set. The same complete-form scanner drives the CLI REPL
and graphical listener, preventing their multiline behavior from diverging.

Dynamic restarts reuse the non-local control stack. Each active record points
to a first-class heap `RESTART` object and keeps executable clause data only
for its dynamic extent. Control frames record the restart-stack boundary that
was active when installed. Transfers discard inner records before jumping,
while cleanup frames run first; selected clause data is rooted in runtime
transfer state until its lexical environment is rebuilt. A restart object may
survive for inspection or image persistence without retaining a dead `setjmp`
destination after `RETURN-FROM`, `THROW`, or normal return.

Dynamic handlers use a parallel runtime stack. Bindings from one
`HANDLER-BIND` retain a shared outer boundary, so selecting one handler masks
the complete binding group while it runs. A returning handler declines and
search continues in outer groups; a handler that invokes a restart follows the
same cleanup and stack-boundary rules as every other non-local transfer.

## IDE session

`sefirah_ide_nucleo` owns the editor documents, listener input, transcript,
inspector, definition browser, condition history, incremental-installation
history, active path, and runtime. Each background document retains its text,
path, image path, cursor, selection, modified flag, editor history, and
incremental fingerprints. Moving those owned values between a document slot
and the active editor makes tab switching lossless without duplicating buffers.
`espaco_trabalho.c` builds the bounded, sorted project index through native
directory enumeration, skips symlink/reparse-point recursion, and keeps
absolute paths separate from the relative paths shown by the Explorer.
The window adapters translate Ctrl+P, Ctrl+Shift+P, and Escape into portable
events. Presentation code owns the filtered Quick Open/command-palette overlay;
file discovery, exclusive file/folder creation, refresh, and opening remain in
the testable session layer.
The session can execute, load, save, snapshot, and restore without a window,
which makes behavior testable on CI. `sefirah_ide`
only lays out the panels,
draws the state, and translates X11/Win32/Cocoa events into session actions.

Inside the session engine, `historico.c` owns the bounded editor timeline and
listener event history, while `estrutura.c` locates complete top-level Lisp
forms, fingerprints them for incremental evaluation, and catalogs named
definitions without evaluating source. Its lexical pass also resolves the atom
at the cursor and catalogs references while excluding comments, strings,
character literals, and definition-name occurrences. The session owns a
normalized byte range for selection; cursor extension respects UTF-8 code-point
boundaries, and structural selection reuses the complete-form scanner. A range
replacement is recorded as one editor-history state. The inspector retains
returned objects and every recursive navigation step through public GC roots.
The public component-introspection API presents labeled edges for compound
objects without exposing the private object union to the IDE. World restoration
opens the replacement image before releasing those roots and the old runtime,
so a damaged or absent snapshot does not destroy the active world. Presentation
code sees only formatted session state and never reaches into runtime internals.

The runtime keeps the last unhandled condition rooted until the next evaluation
and exposes it as a borrowed SDK value. The session converts that value into its
own bounded set of public roots, so later collections cannot invalidate the
debugger history. An unhandled `ERROR` also records a borrowed, GC-visible
vector of the restart objects active at the signal point. The vector is reset at
the next evaluation, while the session copies its members into roots owned by
the corresponding history entry. Restoring another world releases all
condition, restart, and inspector roots before destroying the old runtime.
Restart frames are intentionally not retained after unwinding; a future
interactive debugger must suspend evaluation instead of storing dead `setjmp`
destinations.

## Compiler flow

`COMPILE` lowers a compatible function to a 64-bit integer SSA IR. The IR is
verified before it reaches the interpreter or a backend:

```text
DEFUN → i64 frontend → SSA IR → flow/dominance verifier
                                      ├── reference interpreter
                                      ├── x86-64 SysV/Microsoft
                                      └── AArch64 AAPCS64
                                                │
                               ┌────────────────┴───────────────┐
                               ▼                                ▼
                         W^X JIT memory              ELF / COFF / Mach-O
```

The portable Lisp definition remains in the function object. Images do not
save native caches: after restoration in another process or architecture, the
function can be compiled again.

## Ownership and collection

The current heap uses mark-and-sweep at evaluation-unit boundaries. Values
passed to C code can be retained across collections with `SefRaiz`. The handle
is explicit and removable, avoiding conservative scans of the host stack.
`sef_valor_quantidade_componentes` and `sef_valor_componente` expose a read-only,
labeled view of object-graph edges for resident tools while preserving this
ownership model.

### Streams

Standard streams wrap `stdin`, `stdout`, and `stderr` without owning them.
Streams returned by `OPEN` own their `FILE` and close it through `CLOSE`, when
the runtime is destroyed, or during collection.

### Shared libraries

The Lisp object and JIT functions share a reference-counted native resource:

```text
SHARED-LIBRARY object ──┐
                        ├── resource (.so/.dylib/.dll) ── native handle
compiled function A ────┤
compiled function B ────┘
```

`CLOSE-SHARED-LIBRARY` releases the object's reference and prevents new
bindings. An existing compiled function remains valid because it owns another
reference. The final owner calls `dlclose` or `FreeLibrary`.

## Persistent image

The v10 binary format preserves the object graph, including symbols, packages,
vectors, characters, hash tables, environments, functions, macros, conditions,
restart objects, and restorable streams. Saving uses a temporary file followed
by atomic replacement. The loader recognizes v6 through v10; loading and
saving an older image emits the current format. After graph validation, a
targeted migration restores canonical `COMMON-LISP:NIL` membership, removes legacy
local `NIL` conflicts from packages that use `COMMON-LISP`, and reinstalls the
missing members of the current primitive set by name together with exported
special-form symbols. Existing Lisp function definitions are preserved. This
lets an older world acquire new built-ins without serializing or trusting stale
C addresses. Symbol property lists live in an internal heap hash table rooted
by the global environment, so the existing v10 graph encoding persists them;
older images naturally begin with an empty table on first use.
Uninterned symbols reference an implementation-private package sentinel that
is absent from the public package registry. This keeps the v10 reference graph
valid while `SYMBOL-PACKAGE`, printing, and inspection expose the required
uninterned semantics.
Package symbol tables may also contain imported identities whose home package
is different. Export tables reference those same objects, and uninterning
updates both tables before optionally returning a home symbol to the private
uninterned sentinel. The ordinary v10 package arrays therefore preserve import
relationships without a parallel registry.
Shadowing choices use another internal heap hash table rooted by the global
environment. Package-use validation consults it before accepting or rejecting
an export conflict, and uninterning refuses to expose ambiguous inherited
identities. The same v10 graph persistence applies to this registry.
Package nicknames use the same graph-rooted metadata pattern. Canonical-name
lookup is attempted first, followed by validated nickname lists; both paths use
the package system's ASCII case-insensitive designator comparison. Inquiry
returns copied strings so callers cannot mutate the registry through its public
surface.
Deleting a package removes it from the runtime package registry and clears its
nickname, shadowing, and outgoing-use metadata. The package heap object remains
reachable through any surviving symbols, so their identity and
`SYMBOL-PACKAGE` relationship survive image restoration; the printer renders
such symbols with the uninterned `#:` notation and the package as explicitly
deleted.
`DEFPACKAGE` validates every option before applying them and then applies declarations
in deterministic phases: shadow and shadowing import, use lists, ordinary import
and intern, then export. A conflict therefore has the same result regardless of
the textual order of its resolving option.

Process resources follow an explicit policy:

| Resource | Save policy |
| --- | --- |
| standard stream | preserved and rebound in the next process |
| closed file stream | preserved as closed |
| open file stream | save is rejected |
| closed shared library | preserved as closed |
| open shared library | save is rejected |
| JIT cache | discarded; recompiled on demand |

This policy avoids serializing descriptors, pointers, or machine bytes with no
valid meaning in the next process.

## Custom GUI

The GUI does not wrap operating-system widgets. Sefirah arranges and draws
components on `SefSuperficie`; the platform backend only presents pixels and
translates events.

```text
SefComponente
    ├── row/column layout and weights
    ├── theme and visual states
    ├── hit testing and focus
    └── action
          │
          ▼
SefSuperficie → CPU rasterizer → native window
```

This design lets ordinary applications use the same GUI as the IDE. Its 5x7
bitmap font has separate uppercase and lowercase ASCII glyphs, so path and
source text preserve their visible case. Vector fonts, general clipping,
HiDPI, IME, and accessibility belong to later milestones.

## Evolution strategy

The self-hosted compiler will follow once the bootstrap IR supports Lisp value
representation, general calls, allocation, safe points, and exception
metadata. The GUI will grow capability by capability—text, composition,
accessibility, and desktop integration—without abandoning the custom
rasterizer.

Criteria and pending work for every phase are in the
[1.0 roadmap](roadmap.md).
