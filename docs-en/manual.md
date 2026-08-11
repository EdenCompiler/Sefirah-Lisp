# Sefirah Lisp Bootstrap Guide

**English** · [Português do Brasil](../docs-ptbr/manual.md)

The current bootstrap establishes the first vertical path from the language to
the desktop. It is not complete ANSI Common Lisp yet: this guide describes
only behavior implemented and tested in the repository.

Programs exclusively use the `.lisp` extension. The former `.sef` extension is
not part of the public format.

## Quick start

```bash
./construir/sefirah avaliar "(+ 20 22)"
./construir/sefirah executar exemplos/inicio.lisp
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
| conditions | `ERROR`, `HANDLER-CASE`, `IGNORE-ERRORS` |
| multiple values | `VALUES`, `VALUES-LIST`, `MULTIPLE-VALUE-BIND`, `MULTIPLE-VALUE-LIST`, `MULTIPLE-VALUE-CALL`, `MULTIPLE-VALUE-PROG1`, `NTH-VALUE` |
| lists | `CONS`, `CAR`, `CDR`, `FIRST`, `REST`, `LIST`, `APPEND`, `NCONC`, `NTH`, `NTHCDR`, `LAST` |
| vectors | `VECTOR`, `MAKE-ARRAY`, `AREF`, `SVREF`, `VECTORP`, `ARRAYP` |
| hash tables | `MAKE-HASH-TABLE`, `GETHASH`, `HASH-TABLE-P`, `HASH-TABLE-COUNT`, `REMHASH`, `CLRHASH` |
| characters | `CHARACTERP`, `CHAR-CODE`, `CODE-CHAR`, and `CHAR...` comparisons |
| sequences | `LENGTH`, `ELT`, `CHAR`, `SCHAR`, `COPY-SEQ`, `REVERSE`, `SUBSEQ`, `FILL` |
| numbers | `+`, `-`, `*`, `/`, `<`, `>`, `<=`, `>=`, `=`, and `/=` |
| functions | `FUNCALL`, `APPLY`, `FUNCTIONP`, `FBOUNDP`, `SYMBOL-FUNCTION` |
| values | `BOUNDP`, `SYMBOL-VALUE`, `SET`, `EQ`, `NOT`, `TYPE-OF` |

Symbols have separate value and function cells, so a variable and a function
can share a name:

```lisp
(define answer 41)
(defun answer () 42)
(list answer (answer)) ; => (41 42)
```

`#'name` reads the function cell.

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
tools without exposing the private object layout.

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
sefirah compilar-elf exemplos/nativo.lisp calcular_nativo calcular.o
cc exemplos/integracao-c/chamar_nativo.c calcular.o -o chamar-nativo
./chamar-nativo

sefirah compilar-coff exemplos/nativo.lisp calcular_nativo calcular.obj
x86_64-w64-mingw32-gcc \
  exemplos/integracao-c/chamar_nativo.c calcular.obj \
  -o chamar-nativo.exe

sefirah compilar-macho \
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
sefirah compilar-elf exemplos/externa.lisp chamar_dobro chamar-dobro.o
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

- `DEFPACKAGE` with `:USE` and `:EXPORT`;
- `IN-PACKAGE`;
- `MAKE-PACKAGE`, `FIND-PACKAGE`, `PACKAGE-NAME`, and `PACKAGEP`;
- `USE-PACKAGE`, `EXPORT`, `INTERN`, and `FIND-SYMBOL`;
- `SYMBOL-NAME`, `SYMBOL-PACKAGE`, and `LIST-ALL-PACKAGES`.

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
accepts `:SUPERSEDE`, `:APPEND`, or `:ERROR`. The current `READ-LINE` returns
`NIL` at end of file; its Common Lisp secondary EOF indicator is still
pending.

## Persistent image

The v9 `.imagem` binary format preserves the heap graph, including vectors,
characters, hash tables, symbols, packages, environments, functions, macros,
conditions, and restorable resources. Saving uses a temporary file and atomic
replacement. C primitives are restored by name, never by address. The v9
reader accepts v6, v7, and v8 images; saving upgrades them to v9.

```bash
sefirah imagem salvar desenvolvimento.imagem exemplos/inicio.lisp
sefirah imagem abrir desenvolvimento.imagem "(fatorial 6)"
sefirah imagem abrir desenvolvimento.imagem
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
and prints every returned value. `:ajuda` lists commands and `:sair` exits.

The graphical IDE is split into a platform-independent session engine and a
window presentation. It provides an editable `.lisp` buffer, multiline
listener, persistent transcript, result inspector, whole-buffer evaluation,
structural form evaluation, linear undo/redo, and file load/save:

```bash
sefirah_ide
sefirah_ide path/to/program.lisp
```

Tab or a pointer click switches between editor and listener. Enter inserts a
line in the editor and submits a complete form in the listener. F5 or
Ctrl+Enter runs the whole buffer; F6 finds and runs only the complete top-level
form at the cursor. Ctrl+Z and Ctrl+Y traverse the editor's bounded linear
timeline. Ctrl+S saves and Ctrl+O reloads the current path. Arrow, Home, and End
move the UTF-8-aware editor cursor. In the listener, Up and Down recover up to
128 submitted events, including multiline forms.

The inspector roots every value from the latest evaluation in the garbage
collector, shows its type and readable representation, and exposes the whole
multiple-value result as a live object shelf. Click the inspector to select the
next object. The session engine, histories, structural scanner, and inspector
are covered by headless automated tests. On macOS, Command can replace Ctrl for
the shortcuts.

These facilities deliberately recover a small, practical part of the Lisp
machine workflow: Interlisp's Programmer's Assistant kept replayable events and
its editors operated on Lisp structure, while Genera connected its Listener and
Inspector to the same live world. Sefirah keeps those ideas behind a portable
session API instead of coupling them to a window backend. See the
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
`,`/`,@`, and simple string escapes.

Opaque objects have readable diagnostic representations, including streams,
compiled functions, conditions, hash tables, and shared libraries.

## Current limitations

- mark-and-sweep GC only between evaluation units;
- no bignums, ratios, complex numbers, or complete numeric tower;
- arrays limited to simple one-dimensional vectors;
- Unicode operations without normalization, grapheme clusters, or case
  folding;
- division always produces `FLOAT`;
- incomplete conditions and restarts;
- compilation limited to the i64 subset;
- FFI without general floats, pointers, structs, and callbacks;
- GUI without vector fonts, HiDPI, IME, and accessibility;
- IDE without selection, structural rewriting, file chooser, debugger,
  profiler, or Interlisp-style selective/out-of-order undo.

See the [1.0 roadmap](roadmap.md) for the next milestones.
