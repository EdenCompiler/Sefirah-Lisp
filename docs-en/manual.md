# Sefirah Lisp Bootstrap Guide

**English** · [Português do Brasil](../docs-ptbr/manual.md)

The current bootstrap establishes the first vertical path from the language to
the desktop. It is not complete ANSI Common Lisp yet: this guide describes
only behavior implemented and tested in the repository.

Implementation identifiers and comments are written in PT-BR. All public
commands, help, diagnostics, REPL text, IDE copy, and tooling output are in
English. Legacy Portuguese CLI spellings remain compatibility aliases.

Programs exclusively use the `.lisp` extension. The former `.sef` extension is
not part of the public format.

## Quick start

```bash
./construir/sefirah evaluate "(+ 20 22)"
./construir/sefirah run exemplos/inicio.lisp
./construir/sefirah repl
```

```lisp
(defun factorial (n)
  (if (< n 2)
      1
      (* n (factorial (- n 1)))))

(factorial 6) ; => 720
```

## Language surface

| Area | Main forms and functions |
| --- | --- |
| values | integers, reals, Unicode characters, strings, symbols, lists, pairs, vectors, and hash tables |
| evaluation | `QUOTE`, `IF`, `PROGN`, `LAMBDA`, `FUNCTION` |
| bindings | `DEFINE`, `DEFVAR`, `DEFPARAMETER`, `SETQ`, `SETF`, `LET`, `LET*` |
| functions and macros | `DEFUN`, `DEFMACRO`, `FLET`, `LABELS`, `MACROLET`, `&REST` |
| composition | `COND`, `WHEN`, `UNLESS`, `AND`, `OR`, quasiquote, `,`, and `,@` |
| control | `BLOCK`, `RETURN-FROM`, `RETURN`, `CATCH`, `THROW`, `UNWIND-PROTECT` |
| conditions and restarts | `ERROR`, `SIGNAL`, `HANDLER-BIND`, `HANDLER-CASE`, `IGNORE-ERRORS`, `RESTART-CASE`, `INVOKE-RESTART`, discovery, and standard named-restart helpers |
| multiple values | `VALUES`, `VALUES-LIST`, `MULTIPLE-VALUE-BIND`, `MULTIPLE-VALUE-LIST`, `MULTIPLE-VALUE-CALL`, `MULTIPLE-VALUE-PROG1`, `NTH-VALUE` |
| lists | `CONS`, `CAR`, `CDR`, `FIRST`, `REST`, `LIST`, `APPEND`, `NCONC`, `NTH`, `NTHCDR`, `LAST` |
| vectors | `VECTOR`, `MAKE-ARRAY`, `AREF`, `SVREF`, `VECTORP`, `ARRAYP` |
| hash tables | `MAKE-HASH-TABLE`, `GETHASH`, `HASH-TABLE-P`, `HASH-TABLE-COUNT`, `REMHASH`, `CLRHASH` |
| characters | `CHARACTERP`, `CHAR-CODE`, `CODE-CHAR`, and `CHAR...` comparisons |
| sequences | `LENGTH`, `ELT`, `CHAR`, `SCHAR`, `COPY-SEQ`, `REVERSE`, `SUBSEQ`, `FILL` |
| numbers | `+`, `-`, `*`, `/`, `<`, `>`, `<=`, `>=`, `=`, and `/=` |
| functions | `FUNCALL`, `APPLY`, `FUNCTIONP`, `FBOUNDP`, `SYMBOL-FUNCTION`, `FDEFINITION`, `FMAKUNBOUND` |
| symbols and values | `SYMBOLP`, `KEYWORDP`, `CONSTANTP`, `MAKE-SYMBOL`, `COPY-SYMBOL`, `GENSYM`, `SYMBOL-NAME`, `SYMBOL-PACKAGE`, `SYMBOL-PLIST`, `GET`, `REMPROP`, `BOUNDP`, `SYMBOL-VALUE`, `SET`, `MAKUNBOUND`, `EQ`, `NOT`, `TYPE-OF` |

Symbols have separate value and function cells, so a variable and a function
can share a name:

```lisp
(define answer 41)
(defun answer () 42)
(list answer (answer)) ; => (41 42)
```

`#'name` reads the function cell.

`MAKUNBOUND` and `FMAKUNBOUND` remove global value and function bindings and
return their symbol even when the cell was already empty. `FDEFINITION` reads
the function cell. `SYMBOL-VALUE`, `SYMBOL-FUNCTION`, and `FDEFINITION` are
writable `SETF` places, so a live world can install or replace definitions
without restarting:

```lisp
(setf (symbol-value 'answer) 41)
(setf (fdefinition 'sum) (symbol-function '+))
(sum answer 1) ; => 42
```

Function names are currently limited to symbols; `(SETF name)` function names
remain pending.

Symbols also carry property lists. `GET` reads an indicator with an optional
default, `REMPROP` removes one entry, and both `GET` and `SYMBOL-PLIST` are
writable `SETF` places. Property lists must contain alternating indicator/value
pairs. They work for `NIL` as well as ordinary symbols and persist in v10 world
images:

```lisp
(setf (get 'answer :documentation) "The ultimate answer")
(get 'answer :documentation) ; => "The ultimate answer"
(remprop 'answer :documentation) ; => T
```

`MAKE-SYMBOL` creates a fresh uninterned symbol whose exact string name is
preserved. `COPY-SYMBOL` also creates a fresh symbol; with a non-`NIL` second
argument it shallow-copies the property-list spine and copies bound value and
function cells. `GENSYM` accepts the standard optional string prefix or
non-negative integer suffix and uses the mutable `*GENSYM-COUNTER*` otherwise:

```lisp
(setf *gensym-counter* 41)
(gensym)        ; => #:G41
(gensym "tmp-") ; => #:|tmp-42|
(gensym 7)      ; => #:G7, without changing the counter
```

Uninterned identities, copied cells, property metadata, and the gensym counter
survive world-image restoration. The live inspector exposes `PACKAGE` as `NIL`
and a `PROPERTIES` edge for such symbols.

## Lists

`CONSP`, `LISTP`, and `ENDP` distinguish pairs, lists, and iteration ends.
`FIRST`/`REST` are reading variants of `CAR`/`CDR`; `RPLACA`, `RPLACD`, and the
corresponding `SETF` places perform explicit mutation.

```lisp
(append '(1 2) '(3 4) 'end) ; => (1 2 3 4 . END)
(nth 2 '(0 1 2 3))          ; => 2
(last '(0 1 2 3) 2)         ; => (2 3)
```

`APPEND` copies earlier lists and reuses its final argument. `NCONC`
destructively links earlier arguments. `MEMBER` and `ASSOC` currently use
`EQL`.

`MAPCAR` accepts one or more lists, stops at the shortest, and returns results.
`MAPC` makes the same calls for side effects and returns its first list:

```lisp
(mapcar #'+ '(1 2 3) '(10 20)) ; => (11 22)
```

Variants with `:KEY`, configurable `:TEST`, and the complete mapping family
remain on the conformance roadmap.

## Vectors and `SETF`

`#(...)` reads a self-evaluating vector literal. As in Common Lisp, forms
inside the literal are not evaluated; `VECTOR` constructs a vector from
evaluated arguments:

```lisp
#(1 (+ 1 1))        ; => #(1 (+ 1 1))
(vector 1 (+ 1 1)) ; => #(1 2)
```

The first array model is simple and one-dimensional. `MAKE-ARRAY` receives a
non-negative integer dimension and accepts `:INITIAL-ELEMENT`:

```lisp
(let ((values (make-array 3 :initial-element 7)))
  (setf (aref values 1) 42)
  (list values (length values) (svref values 1)))
; => (#(7 42 7) 3 42)
```

The initial `SETF` supports variables and `AREF`, `SVREF`, `CAR`, `CDR`, and
`GETHASH` places. Other generalized-place protocols, multidimensional arrays,
specialized element types, and adjustable vectors remain pending.

In the C SDK, `sef_vetor_criar`, `sef_vetor_tamanho`, `sef_vetor_obter`, and
`sef_vetor_definir` operate on the same collected object. A value retained by
the host across evaluations must stay protected by `SefRaiz`.
`sef_valor_nome_tipo` exposes the stable diagnostic type name used by resident
tools without exposing the private object layout. Generic tools can use
`sef_valor_quantidade_componentes` and `sef_valor_componente` to traverse
labeled child values without including the private `sefirah/interno.h` header.
Components are borrowed: the parent must remain rooted, and a retained child
needs its own `SefRaiz` before another evaluation or collection.

## Unicode characters and strings

`#\A`, `#\Space`, `#\Newline`, and `#\λ` produce real `CHARACTER` objects. The
readable `#\U+NNNN` form covers unnamed control codes. `CHAR-CODE` and
`CODE-CHAR` convert characters and Unicode scalar values; surrogates and values
above `U+10FFFF` are rejected.

Strings remain stored as UTF-8, but `LENGTH`, `CHAR`, `SCHAR`, and `ELT` count
characters rather than bytes:

```lisp
(length "ação") ; => 4
(char "ação" 1) ; => #\ç

(let ((text "abc"))
  (setf (char text 1) #\é)
  text) ; => "aéc"
```

Assignment rebuilds UTF-8 when the new character uses a different byte count.
`ELT` reads and changes lists, vectors, and strings. `CHAR=`, `CHAR/=`,
`CHAR<`, `CHAR>`, `CHAR<=`, and `CHAR>=` compare code points; case-insensitive
variants are not implemented yet.

The SDK exposes `sef_caractere_criar`, `sef_valor_e_caractere`, and
`sef_caractere_codigo`.

### Sequence algorithms

Proper lists, vectors, and strings share copying, reversing, slicing, and
filling. Non-destructive operations preserve the input type:

```lisp
(reverse #(1 2 3))        ; => #(3 2 1)
(reverse "ação")          ; => "oãça"
(subseq '(0 1 2 3) 1 3)  ; => (1 2)
```

`COPY-SEQ` creates an independent outer structure while retaining elements.
`FILL` mutates the received sequence and accepts `:START` and `:END`; a string
item must be a character:

```lisp
(let ((values #(1 2 3 4)))
  (fill values 9 :start 1 :end 3)
  values) ; => #(1 9 9 4)
```

Intervals have exclusive ends and are validated before mutation.

## Hash tables

`MAKE-HASH-TABLE` creates a runtime-collected table. The implemented key test
is `EQL`: equal integers, reals, and characters find the same entry; other
objects retain identity. Collisions use open addressing, and tables grow
automatically.

```lisp
(let ((score (make-hash-table)))
  (setf (gethash 'blue score) 40
        (gethash 'gold score) 42)
  (list (gethash 'gold score)
        (gethash 'missing score :no-value)
        (hash-table-count score)))
; => (42 :NO-VALUE 2)
```

`REMHASH` returns true when it removes a key. `CLRHASH` empties and returns the
table. `GETHASH` returns the found value or default as its primary value and a
secondary presence flag, so a stored `NIL` can be distinguished from a missing
key. Other key tests and resizing options remain pending.

Keys and values participate in GC marking. Images preserve entries, shared
identity, and cycles, including a table containing itself.

## Initial native compilation

`COMPILE` lowers a compatible function to SSA IR and installs x86-64 or
AArch64 code according to the host. The current subset accepts integer
parameters, constants, `+`, `-`, `*`, `<`, `<=`, `IF`, and exactly one body
form.

```lisp
(defun calculate (x y)
  (if (< x y)
      (+ (* x 2) y)
      (- x y)))

(compile 'calculate)
(calculate 10 22)                    ; => 42
(compiled-function-p #'calculate)    ; => T
```

Normal calls, `FUNCALL`, and `APPLY` automatically observe installed code.
Current compiled functions require and return i64 values; general Lisp values
and safe points are not in the IR yet.

An image preserves the Lisp definition and discards the machine cache. Call
`COMPILE` again after opening it on another architecture.

## Native objects

The i64 frontend produces relocatable objects for all three desktop formats:

```bash
sefirah compile-elf exemplos/nativo.lisp calcular_nativo calcular.o
cc exemplos/integracao-c/chamar_nativo.c calcular.o -o chamar-nativo
./chamar-nativo

sefirah compile-coff exemplos/nativo.lisp calcular_nativo calcular.obj
x86_64-w64-mingw32-gcc \
  exemplos/integracao-c/chamar_nativo.c calcular.obj \
  -o chamar-nativo.exe

sefirah compile-macho \
  exemplos/nativo.lisp calcular_nativo calcular-macos.o
```

The exported symbol follows `int64_t function(const int64_t *args)`. The
frontend retains IR and selects System V for ELF/Mach-O, Microsoft x64 for
COFF, and AAPCS64 on ARM64.

The `.c` files live under `exemplos/integracao-c/` because they demonstrate SDK
and generated-object consumption. They are not Sefirah source syntax.

## i64 FFI

`EXTERNAL-I64` represents a C call returning i64 with one or two i64 inputs:

```lisp
(defun call-double (value)
  (external-i64 "dobrar_i64" value))

(defun combine (a b)
  (external-i64 "combinar_i64" a b))
```

To produce an object with an undefined symbol:

```bash
sefirah compile-elf exemplos/externa.lisp chamar_dobro chamar-dobro.o
cc exemplos/integracao-c/chamar_externa.c chamar-dobro.o -o chamar-externa
```

Writers emit the relocation required by the format and architecture. An
unresolved symbol cannot run in the JIT.

### Binding by path

```lisp
(compile-external-i64 'call-double "./libcalculos.so")
(call-double 21) ; => 42
```

Use a `.dll` on Windows and a `.dylib` on macOS. The runtime resolves names
through `LoadLibrary`/`GetProcAddress` or `dlopen`/`dlsym`.

### Library object

Open a library once when multiple functions share the resource:

```lisp
(define calculations (open-shared-library "./libcalculos.so"))

(shared-library-p calculations)       ; => T
(shared-library-open-p calculations)  ; => T

(compile-external-i64 'call-double calculations)
(close-shared-library calculations)

(shared-library-open-p calculations)  ; => NIL
(call-double 21)                       ; => 42
```

Closing is idempotent and prevents new bindings through the object. The
compiled function remains valid because it owns another reference. The
library unloads only after both the object and all compiled functions release
it.

### C SDK

A function returned by `sef_runtime_compilar_objeto_i64` can receive addresses
through `sef_funcao_compilada_vincular_externa_i64` or its `_binaria` variant
and be finalized with `sef_funcao_compilada_preparar_jit`.

The JIT creates nearby trampolines that reach any 64-bit address without ever
making a page writable and executable at once.

## Packages

The runtime starts in `COMMON-LISP-USER`, which uses `COMMON-LISP`. `:name`
symbols belong to `KEYWORD` and self-evaluate. The `SEFIRAH` package holds
implementation-specific names such as `SEFIRAH::ENVIRONMENT` and
`SEFIRAH::SHARED-LIBRARY`.

Available forms and functions:

- `DEFPACKAGE` with `:SHADOW`, `:SHADOWING-IMPORT-FROM`, `:USE`,
  `:IMPORT-FROM`, `:INTERN`, and `:EXPORT`;
- `IN-PACKAGE`;
- `MAKE-PACKAGE`, `FIND-PACKAGE`, `PACKAGE-NAME`, and `PACKAGEP`;
- `USE-PACKAGE`, `UNUSE-PACKAGE`, `PACKAGE-USE-LIST`,
  `PACKAGE-USED-BY-LIST`, `EXPORT`, `UNEXPORT`, `IMPORT`, `UNINTERN`,
  `SHADOW`, `SHADOWING-IMPORT`, `PACKAGE-SHADOWING-SYMBOLS`, `INTERN`, and
  `FIND-SYMBOL`;
- `SYMBOLP`, `KEYWORDP`, `CONSTANTP`, `SYMBOL-NAME`, `SYMBOL-PACKAGE`, and
  `LIST-ALL-PACKAGES`.

`INTERN` and `FIND-SYMBOL` return the ANSI secondary status value:
`:INTERNAL`, `:EXTERNAL`, `:INHERITED`, or `NIL`. A newly created symbol has
status `NIL`; a later lookup reports its actual status. Symbols in `KEYWORD`
are external and self-evaluating.

`IMPORT` makes one symbol or a list of symbols directly present in a package
without changing an already interned symbol's home package. Importing a fresh
`MAKE-SYMBOL` identity adopts it into the destination package. Imported symbols
can be exported, and `UNINTERN` removes both their local presence and export
status. Removing a symbol from its home package makes it uninterned again;
`COMMON-LISP` is implementation-locked against `UNINTERN` so the live runtime
cannot lose its required operators.

`SHADOW` creates or designates a local symbol that intentionally hides
inherited names, allowing another package with a conflicting export to be
used. `SHADOWING-IMPORT` selects an existing identity as that local winner.
`PACKAGE-SHADOWING-SYMBOLS` returns a fresh list of the choices. Removing a
shadowing symbol is rejected when it would expose two different inherited
symbols; all shadowing choices survive image restoration.

`DEFPACKAGE` validates its complete declaration and applies symbol options in
semantic phases rather than textual order. Shadow and shadowing-import choices
are installed before use lists; ordinary imports and interned names follow;
exports are last. Both import options name a source package followed by the
symbol names to reuse.

`USE-PACKAGE` and `UNUSE-PACKAGE` accept one package designator or a proper
list. `PACKAGE-USE-LIST` and `PACKAGE-USED-BY-LIST` return fresh relationship
lists. `UNEXPORT` makes an external symbol internal without changing its
identity; the implementation lock protects `COMMON-LISP` from topology edits.

`NIL` is simultaneously the empty list, false, and the external symbol named
`"NIL"` in `COMMON-LISP`. It is inherited by `COMMON-LISP-USER`; `SYMBOLP`,
`SYMBOL-NAME`, `SYMBOL-PACKAGE`, `BOUNDP`, and `SYMBOL-VALUE` observe those
symbol semantics. `T`, `NIL`, and keyword symbols are protected against
assignment through the implemented binding forms and `SET`.

```lisp
(defpackage :calculations
  (:use :common-lisp)
  (:export :answer))

(in-package :calculations)
(defun answer () 42)

(in-package :common-lisp-user)
(calculations:answer) ; => 42
```

Packages, use relationships, and symbol identity are part of the image.

## Conditions and named restarts

The initial condition layer catches errors with `HANDLER-CASE` or
`IGNORE-ERRORS`. `HANDLER-BIND` installs dynamically scoped functions called
in the signaler's context by `SIGNAL` and `ERROR`. A handler can return to
decline, or transfer control by choosing a recovery choice established with
`RESTART-CASE`. `INVOKE-RESTART` runs every intervening `UNWIND-PROTECT`
cleanup before entering the selected clause. Normal and restarted paths
preserve their multiple values.

```lisp
(restart-case
    (handler-bind ((error (lambda (condition)
                            (invoke-restart 'use-value 42))))
      (error "recoverable"))
  (use-value (value)
    (+ value 1))) ; => 43

(restart-case
    (let ((restart (find-restart 'retry)))
      (list (type-of restart)
            (restart-name restart)
            (eq restart (first (compute-restarts)))))
  (retry () :retried)) ; => (RESTART RETRY T)
```

`FIND-RESTART` and `COMPUTE-RESTARTS` return first-class `RESTART` objects in
dynamic precedence order, including anonymous and same-named choices.
`INVOKE-RESTART` accepts either an active object or a non-`NIL` name. An object
may outlive its dynamic extent for persistence and inspection, but invoking it
after it becomes inactive signals an error. Condition association, the full
standard condition hierarchy and type specifiers, and the `:REPORT`, `:TEST`,
and `:INTERACTIVE` clause options remain pending. `ABORT`, `CONTINUE`,
`MUFFLE-WARNING`, `STORE-VALUE`, and `USE-VALUE` invoke an active restart of
the corresponding name; their optional condition filtering is not available
yet.

## Streams and files

`*STANDARD-INPUT*`, `*STANDARD-OUTPUT*`, and `*ERROR-OUTPUT*` belong to the host
process. `OPEN` uses binary mode for predictable behavior on all three
systems.

```lisp
(let ((output (open "result.txt"
                    :direction :output
                    :if-exists :supersede)))
  (write-string "Sefirah" output)
  (terpri output)
  (finish-output output)
  (close output))

(let ((input (open "result.txt")))
  (print (read-line input))
  (close input))
```

`:DIRECTION` accepts `:INPUT`, `:OUTPUT`, and `:IO`. For output, `:IF-EXISTS`
accepts `:SUPERSEDE`, `:APPEND`, or `:ERROR`. `READ-LINE` accepts its four ANSI
optional arguments and returns `(line, missing-newline-p)`. At immediate EOF,
`eof-error-p` defaults to true; when false, the supplied `eof-value` and true
are returned. The initial condition system currently signals a general
`ERROR`, rather than the more specific `END-OF-FILE` condition.

## Persistent image

The v10 `.imagem` binary format preserves the heap graph, including vectors,
characters, hash tables, symbols, packages, environments, functions, macros,
conditions, restart objects, and restorable resources. Saving uses a temporary
file and atomic replacement. C primitives are restored by name, never by
address. The v10 reader accepts v6 through v9 images; loading performs targeted
migrations that restore newly available primitives, exported special-form
symbols, and canonical `NIL` package membership, while saving upgrades the
result to v10.

```bash
sefirah image save desenvolvimento.imagem exemplos/inicio.lisp
sefirah image open desenvolvimento.imagem "(fatorial 6)"
sefirah image open desenvolvimento.imagem
```

Before saving:

- close file streams;
- close shared-library objects;
- do not depend on the JIT cache, which is rebuilt on demand.

Standard streams are rebound to the new process. Closed file streams and
libraries remain closed objects.

## REPL and IDE

The textual REPL accepts complete Lisp programs rather than isolated physical
lines. An open list, string, vector, or reader prefix changes `sefirah>` to the
`......>` continuation prompt. It evaluates only after the form is complete
and prints every returned value. `:help` lists commands and `:quit` exits.

The graphical IDE is split into a platform-independent session engine and a
Lisp-workstation desktop presentation. Its cream, sage, olive, and amber palette
keeps the visual lineage of Interlisp and Lisp Machines while the workbench
layout follows modern editor conventions. It provides persistent `.lisp` editor tabs, multiline
listener, persistent transcript, result inspector, whole-buffer evaluation,
structural and incremental evaluation, definition navigation, live-world
snapshots, linear undo/redo, and file load/save:

```bash
sefirah_ide
sefirah_ide path/to/program.lisp
sefirah_ide path/to/project-directory
```

Opening another source file creates a tab instead of replacing the current
buffer. Every tab retains its cursor, selection, bounded undo/redo timeline,
incremental-evaluation history, and unsaved state. Modified tabs show `*`, and
a pointer click on a tab activates its preserved editor state.

Passing a directory opens it as a workspace. The Explorer recursively indexes
up to 10,000 `.lisp` files, ignores generated/dependency directories and
symbolic-link loops, and presents paths in deterministic order. With Explorer
focus, Up/Down wraps through the index and Enter opens the selected source in a
tab; a pointer click selects and opens a visible entry. The initial source file
reuses an untouched empty tab.

The command toolbar provides pointer buttons for Run, Run Form, Run Changes,
Save, Snapshot, Restore, Commands, Open File, Open Folder, New File, New Folder,
and Refresh. File and folder controls use an in-IDE path prompt and report
failures in English inside that prompt. Creation is exclusive—it does not
overwrite an existing path—and Refresh preserves the selected source when it
still exists. The evaluation and snapshot buttons invoke the same live-world
operations as F5/F6/F7, Ctrl+S, and F9/F10; the toolbar does not create a
separate batch-mode workflow. Path input preserves the exact uppercase and
lowercase letters typed by the user, and the bitmap font renders both cases
distinctly, which keeps paths usable on case-sensitive file systems.

Ctrl+P opens Quick Open and filters the workspace index without case
sensitivity. Ctrl+Shift+P or the Commands button opens a searchable command
palette covering file/folder operations, evaluation, snapshots, structural
navigation, focus, and undo/redo. Up/Down wraps through results, Enter invokes
the selected item, Escape closes the overlay, and visible results accept pointer
clicks. Ctrl+O opens the file path prompt.

Tab, Shift+Tab, or a pointer click switches among editor, inspector, debugger,
and listener. Enter inserts a line in the editor, opens the selected inspector
component or debugger condition, or submits a complete form in the listener.
F5 or Ctrl+Enter runs the whole buffer; F6 finds and runs only the complete
top-level form at the cursor, while Shift+F6 selects that form for structural
replacement or deletion. Ctrl+Z and Ctrl+Y traverse the editor's bounded linear
timeline. F7 evaluates only top-level forms whose contents changed since their
last successful evaluation. F8 visits the next named definition and Shift+F8
the previous one; the browser recognizes functions, macros, variables,
parameters, constants, packages, and `DEFINE` forms while ignoring comments and
strings. F11 jumps from the symbol at the cursor to its named definition. F12
visits the next structural reference and Shift+F12 the previous one, wrapping
at either end; references inside strings and comments are excluded. Ctrl+S
saves and Ctrl+O opens the file path prompt. Arrow, Home, and End move the
UTF-8-aware editor cursor; holding Shift extends a range by code point. Typing
or Backspace replaces or removes the selected range as one undoable edit. In
the listener, Up and Down recover up to 128 submitted events, including
multiline forms.

F9 writes the current Lisp world to an `.imagem` file beside the current source;
F10 replaces the active runtime with that snapshot while preserving the editor,
listener history, and transcript. A failed restore leaves the active world
untouched. Process-local resources such as open streams and shared libraries
retain the image-format restrictions described above. This is a world snapshot,
not yet a complete desktop-session restoration facility.

The right-hand tools contain the source browser, inspector, and initial
debugger. Every unhandled evaluation condition is retained through a public GC
root in a bounded 32-entry history. Shift+F9/Shift+F10 or Up/Down while the
debugger is focused navigate that history; Enter opens the selected condition
object in the general inspector. Internal evaluator and reader failures are
represented by synthesized `ERROR` conditions, while syntax and external I/O
diagnostics remain labeled entries without a Lisp object.

Restart objects may survive for inspection, but their executable records
belong to the evaluation stack and correctly become inactive when evaluation
unwinds. The inspector exposes their `NAME` and live `ACTIVE` state. The panel
therefore does not pretend that a completed failure still has invokable
restarts. Interactive restart selection requires a suspendable
evaluator/debugger continuation and remains pending.

The browser can
show the definition catalog or the references/callers for the selected symbol,
mark the current match, and move the editor cursor directly to it. The inspector
roots every value from the latest evaluation in the garbage
collector, shows its type and readable representation, and exposes the whole
multiple-value result as a live object shelf. It recursively exposes labeled
components of pairs, vectors, functions, lexical environments, conditions,
restarts, symbols, packages, and hash tables. Up/Down select a component, Enter
opens it,
Backspace returns to its parent, and Left/Right switch roots; every path node is
held by an explicit GC root. The session engine, histories, structural scanner,
debugger history, and inspector are covered by headless automated tests. On
macOS, Command can replace Ctrl for the shortcuts.

These facilities deliberately recover a small, practical part of the Lisp
machine workflow: Interlisp's Programmer's Assistant kept replayable events and
its editors operated on Lisp structure, while Genera connected source
navigation, its Listener, Inspector, and saved worlds to the same live system.
Sefirah keeps those ideas behind a portable session API instead of coupling
them to a window backend. See the
[Interlisp timeline](https://interlisp.org/history/timeline/) and the
[Genera User's Guide](https://www.bitsavers.org/pdf/symbolics/software/genera_8/Genera_User_s_Guide.pdf)
for the historical systems that motivate this direction.

## Custom GUI

The C17 API provides `SefComponente` for panels, labels, buttons, and fields;
weighted row/column layouts; `SefTemaGui`; hit testing; focus navigation; and
action dispatch. `SefInteracaoGui` turns Tab, Enter, and pointer input into
focus and activation.

X11, Win32, and Cocoa deliver keyboard, shortcuts, and pointer events to the
IDE. The Cocoa adapter is written in C over the Objective-C runtime and redraws
the custom raster surface after each state-changing event.

## Reader and printing

The reader uppercases ASCII names, recognizes `;` comments, dotted lists,
`#(...)` vectors, `#\` characters, quote, function quote, quasiquote with
`,`/`,@`, vertical-bar symbol names, single symbol escapes, and simple string
escapes. The readable printer adds `|...|` and escapes when a symbol name would
otherwise lose case or be parsed as syntax.

Opaque objects have readable diagnostic representations, including streams,
compiled functions, conditions, hash tables, and shared libraries.

## Current limitations

- mark-and-sweep GC only between evaluation units;
- no bignums, ratios, complex numbers, or complete numeric tower;
- arrays limited to simple one-dimensional vectors;
- Unicode operations without normalization, grapheme clusters, or case
  folding;
- division always produces `FLOAT`;
- conditions without the complete hierarchy, condition-associated restarts,
  or suspendable restart selection;
- compilation limited to the i64 subset;
- FFI without general floats, pointers, structs, and callbacks;
- GUI without vector fonts, HiDPI, IME, and accessibility;
- IDE without selection, structural rewriting, file chooser, debugger,
  profiler, or Interlisp-style selective/out-of-order undo.

See the [1.0 roadmap](roadmap.md) for the next milestones.
