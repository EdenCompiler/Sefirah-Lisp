# Sefirah Lisp 1.0 Roadmap

**English** · [Português do Brasil](../docs-ptbr/roteiro.md)

Version 1.0 is a complete desktop Lisp platform, not merely a runtime or GUI
demonstration. This document separates completed work from intent and defines
the evidence required for each milestone.

## Milestone status

| Milestone | Goal | Status |
| --- | --- | --- |
| 0 | executable foundation | functional base; listed items complete |
| 1 | compilable runtime | in progress |
| 2 | hosted Common Lisp and live image | pending |
| 3 | desktop GUI | initial base; desktop capabilities pending |
| 4 | 1.0 environment and distribution | pending |

A checked box means a corresponding implementation and test exist. It does
not mean the entire milestone is complete.

## Milestone 0 — executable foundation

- [x] Modular C17 build and CMake tests.
- [x] Objects, lexical environments, reader, printer, and evaluator.
- [x] Functions, macros, recursion, and essential primitives.
- [x] Mark-and-sweep collector at evaluation boundaries.
- [x] CPU rasterizer, bitmap font, and custom visual composition.
- [x] X11, Win32, and Cocoa raster windows with native event bridges.
- [x] Graphical listener connected to the runtime on Linux, Windows, and macOS.
- [x] Multiline textual REPL with complete-form detection and multiple-value
  printing.
- [x] Green build-and-test CI matrix for Linux, Windows, and macOS.
- [x] Versioned image with atomic saving and heap restoration.
- [x] Explicit root handles for safe C, IDE, and FFI integration.
- [x] Cross-platform CI installation audit for binaries, public headers, and documentation.
- [x] Quasiquote, unquote, and splice for readable macros.
- [x] Heap packages, qualified symbols, `KEYWORD`, `:USE` inheritance, and
  image persistence.
- [x] Standard and file streams, text I/O, and a safe image policy.
- [x] Internal `SEFIRAH` package for implementation types and operations.

## Milestone 1 — compilable runtime

### Runtime and language

- [ ] Explicit roots at every native boundary, safe points, and a precise
  generational GC.
- [ ] Complete conditions and restarts.
- [ ] Composite streams and the complete stream protocol.
- [ ] Common Lisp numeric tower.
- [x] Separate value/function namespaces and non-local control with cleanup.
- [x] Initial condition objects and recoverable handling with `HANDLER-CASE`.
- [x] Dynamic `HANDLER-BIND`/`SIGNAL` protocol integrated with active restarts.
- [x] Initial named restart protocol with dynamic discovery, invocation,
  multiple values, and `UNWIND-PROTECT` cleanup.
- [x] First-class restart objects with identity, anonymous discovery,
  inspection, inactive-state safety, GC, and image persistence.
- [x] Named helpers `ABORT`, `CONTINUE`, `MUFFLE-WARNING`, `STORE-VALUE`, and
  `USE-VALUE`.
- [x] Mutable global value/function cells with `FDEFINITION`, `MAKUNBOUND`,
  `FMAKUNBOUND`, and generalized `SETF` access.
- [x] Persistent symbol property lists with `SYMBOL-PLIST`, `GET`, `REMPROP`,
  `NIL` support, and generalized `SETF` access.
- [x] Uninterned `MAKE-SYMBOL`/`COPY-SYMBOL` identities and deterministic
  `GENSYM` with persistent `*GENSYM-COUNTER*`.
- [x] Package `IMPORT` and `UNINTERN`, including uninterned-symbol adoption,
  imported export, status transitions, and image persistence.
- [x] Persistent `SHADOW`, `SHADOWING-IMPORT`, shadowing-symbol inquiry, and
  inherited-conflict protection.
- [x] Phased `DEFPACKAGE` support for `:SHADOW`,
  `:SHADOWING-IMPORT-FROM`, `:USE`, `:IMPORT-FROM`, `:INTERN`, and `:EXPORT`.
- [x] Reversible `USE-PACKAGE`/`UNUSE-PACKAGE` and `EXPORT`/`UNEXPORT`, with
  fresh package-use and used-by relationship inquiries.
- [x] Persistent, unique package nicknames through `DEFPACKAGE`,
  `MAKE-PACKAGE`, `FIND-PACKAGE`, and `PACKAGE-NICKNAMES`.
- [x] Collision-safe `RENAME-PACKAGE` with atomic nickname replacement and
  world-image persistence.
- [x] Guarded `DELETE-PACKAGE` with relationship checks, metadata cleanup,
  surviving symbol identity, and world-image persistence.
- [x] Read-only package discovery with `DO-SYMBOLS`, `DO-EXTERNAL-SYMBOLS`, and
  `DO-ALL-SYMBOLS`, including implicit `NIL` blocks and image reconciliation.
- [x] Global registered-package name lookup through `FIND-ALL-SYMBOLS`, with
  `NIL`, uninterned/deleted filtering, and image reconciliation.
- [x] Persistent counter-based `GENTEMP` with package-aware collision skipping.

### Compiler

- [x] Typed SSA IR, dominance verifier, and reference interpreter.
- [x] System V/Microsoft x86-64 emitter and W^X loading under both ABIs.
- [x] `DEFUN` → SSA → native-code frontend integrated with `COMPILE`.
- [x] AArch64 AAPCS64 emitter with native frontend selection.
- [x] Relocatable x86-64/AArch64 ELF64 writer and `compile-elf` command.
- [x] AMD64/ARM64 COFF writer and `compile-coff` command.
- [x] x86-64/ARM64 Mach-O writer and `compile-macho` command.
- [x] Desktop relocations and external i64 calls with one or two inputs.
- [x] Lisp `EXTERNAL-I64` form for imports in native objects.
- [x] x86-64/AArch64 JIT trampolines with explicit binding and W^X.
- [ ] Native representation of tagged Lisp values.
- [ ] General Lisp calls, allocation, safe points, and exception metadata.
- [ ] Register allocation and optimizations beyond bootstrap.
- [ ] Bootstrap compiler written in Sefirah and reproducible self-hosting.

### FFI

- [x] `.so`, `.dylib`, and `.dll` loading through `COMPILE-EXTERNAL-I64`.
- [x] Library objects with explicit closing and safe references.
- [x] Image policy for shared libraries.
- [ ] Foreign type descriptors.
- [ ] General signatures with integers, floats, pointers, strings, structs,
  and `void` returns.
- [ ] C-to-Lisp callbacks with defined ownership and error handling.

## Milestone 2 — hosted Common Lisp and live image

- [x] Simple heap vectors with `#(...)`, access, mutation, GC, and persistence.
- [x] Unicode characters, `#\` syntax, UTF-8 strings, and the initial `ELT`
  sequence protocol.
- [x] Initial sequence algorithms for lists, vectors, and strings:
  `COPY-SEQ`, `REVERSE`, `SUBSEQ`, and `FILL`.
- [x] Initial list protocol with composition, navigation, search, and
  multi-list `MAPCAR`/`MAPC`.
- [x] `EQL` hash tables with `GETHASH`, `SETF`, removal, GC, and persistence.
- [x] Initial Common Lisp multiple values with binding, collection, calls,
  non-local propagation, `GETHASH` presence, package status, and `READ-LINE`
  newline flags.
- [x] Backward-compatible v6 through v9 image loading and v10 emission.
- [x] Targeted image migration for canonical `NIL` and newly added primitives.
- [x] Initial explicit ANSI conformance report with known deviations.
- [ ] Complete ANSI Common Lisp symbol and semantic coverage.
- [ ] Documented CLOS and MOP.
- [ ] Complete conditions and restarts integrated with the compiler.
- [ ] Threads, synchronization, and safe GC/FFI interaction.
- [ ] General migration infrastructure for future image versions.
- [ ] Declarative recovery of non-serializable external resources.
- [ ] ASDF, Quicklisp, and a Sefirah manager with lockfiles.
- [ ] Compiler and core libraries loadable from the image.

## Milestone 3 — desktop GUI

### Rendering and text

- [x] RGB surface, CPU rasterizer, and initial bitmap font.
- [x] Component tree, layout, themes, hit testing, focus, and actions.
- [ ] Paths, general clipping, alpha, and composition.
- [ ] Images, vector fonts, shaping, and font fallback.
- [ ] HiDPI and fractional scaling.

### Platform and accessibility

- [x] Initial X11 and Win32 windows and events.
- [x] Cocoa/CoreGraphics bridge written in C with keyboard, shortcuts, pointer,
  and redraw propagation.
- [ ] Wayland.
- [ ] Complete IME composition and advanced text-input protocols.
- [ ] Semantic tree and assistive-technology integration.
- [ ] Clipboard, drag and drop, dialogs, printing, and notifications.

### Application services

- [ ] Networking and TLS.
- [ ] Processes and inter-process communication.
- [ ] SQLite.
- [ ] Basic audio.

## Milestone 4 — 1.0 environment and distribution

### Lisp environment

- [x] Initial editable `.lisp` buffer with whole-buffer evaluation and
  load/save.
- [x] Multiline listener, persistent transcript, and initial result inspector.
- [x] UTF-8-aware cursor movement and insertion in the text editor.
- [x] Top-level form evaluation at the cursor and navigable listener events.
- [x] Bounded linear undo/redo and a GC-rooted multiple-value inspector.
- [x] Named-definition browser and incremental evaluation of changed top-level forms.
- [x] Symbol-at-point definition lookup and structural caller/reference navigation.
- [x] Save and restore the live Lisp world from the IDE.
- [x] Recursive, GC-rooted inspector for compound object graphs.
- [x] Bounded, GC-rooted unhandled-condition history connected to the inspector.
- [x] UTF-8 range selection and top-level structural selection with atomic
  replacement/removal in undo history.
- [x] Lisp-workstation shell with a warm historical palette, persistent editor
  tabs, independent undo history, cursor/selection state, and unsaved indicators.
- [x] Recursive workspace Explorer with deterministic Lisp-source indexing and
  direct keyboard/pointer opening into editor tabs.
- [x] Quick Open, searchable command palette, and working file/folder toolbar
  actions with refresh and in-workbench error feedback.
- [x] Case-preserving path prompts with visually distinct uppercase and
  lowercase bitmap glyphs.
- [ ] Multi-file and runtime-aware code browser.
- [ ] Suspendable debugger, profiler, and interactive restart navigation.
- [ ] Reversible DWIM and selective/out-of-order transactional history.
- [ ] Git integration, autosave, and full desktop-session restoration.
- [ ] IDE recompiles the compiler and itself.

### Distribution

- [ ] MSIX package for Windows.
- [ ] DMG application for Intel and Apple Silicon macOS.
- [ ] AppImage, DEB, and RPM for Linux.
- [ ] Signing hooks, metadata, icons, and updates.
- [ ] Official Windows x64, macOS Intel/ARM, and Linux x64/ARM matrix with
  X11/Wayland.

## 1.0 delivery criteria

The goal is complete only when evidence exists for all of these items:

1. a conformance suite documenting the supported Common Lisp surface;
2. reproducible self-hosting on x86-64 and AArch64;
3. an image capable of restoring a real development session;
4. a functional Sefirah desktop application on Windows, Linux, and macOS;
5. an IDE with integrated editing, evaluation, inspection, and debugging;
6. installable packages and installation tests on official platforms;
7. user, architecture, SDK, and migration documentation for the delivered
   version.

While any evidence is missing, the version remains pre-1.0.
