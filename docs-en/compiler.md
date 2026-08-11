# Sefirah Lisp Compiler

**English** · [Português do Brasil](../docs-ptbr/compilador.md)

## Overview

The compiler is separate from the runtime and GUI. The public
`sefirah/compilador.h` layer provides a typed SSA IR for 64-bit integers, a
reference interpreter, x86-64/AArch64 emitters, and desktop object writers.

| Layer | Current state |
| --- | --- |
| Lisp frontend | `DEFUN` with i64 parameters, constants, arithmetic, comparisons, `IF`, and `EXTERNAL-I64` |
| IR | basic blocks, SSA, `PHI`, control flow, and external calls |
| Reference | interpreter with a step limit |
| x86-64 | System V and Microsoft x64, JIT and objects |
| AArch64 | AAPCS64, native JIT when hosted, and objects |
| Objects | ELF64, COFF, and Mach-O for x86-64/ARM64 |

The general Common Lisp compiler—with tagged values, allocation, and safe
points—is not implemented yet. The current backend is a verifiable foundation
for that work.

## SSA IR

A function contains basic blocks. Each register has exactly one static
definition, and every flow ends with a jump, branch, or return. Current
operations cover:

- i64 parameters and constants;
- `PHI`;
- addition, subtraction, and multiplication;
- less-than and less-than-or-equal;
- C calls with one or two i64 inputs;
- jumps, branches, and returns.

Integer arithmetic has modular 64-bit semantics and does not rely on undefined
C overflow.

Before execution, the verifier checks:

- terminated blocks reachable from the entry;
- existing jump targets, parameters, and registers;
- exactly one definition for every SSA register;
- `PHI` instructions at block starts and connected to real predecessors;
- every definition dominating its uses;
- valid arity and symbols for external calls.

The reference interpreter consumes the same IR as the backends. Tests build
loops, `PHI` nodes, branches, and C calls, and exercise verifier rejection.

## i64 Lisp frontend

`COMPILE` lowers a compatible Lisp function and installs native code in the
function object itself:

```lisp
(defun choose (x y)
  (if (< x y)
      (+ (* x 2) y)
      (- x y)))

(compile 'choose)
(choose 10 22) ; => 42
```

Positional parameters become `PARAMETRO`; integers and operations produce SSA
values; every `IF` creates two paths and a join block with `PHI`. Normal calls,
`FUNCALL`, and `APPLY` observe the installed cache.

The Lisp definition remains available. Images save that portable definition
and discard bytes tied to the old architecture or process.

## x86-64 backend

The emitter covers the current IR under System V and Microsoft x64 ABIs. SSA
registers use stack slots during bootstrap, separating correctness from
register allocation. The backend:

- receives parameters through a stable vector contract;
- resolves `rel32` jumps;
- creates parallel copies on `PHI` edges;
- prepares shadow space for the Microsoft ABI;
- emits external calls in the correct registers for each ABI.

`sef_codigo_nativo_preparar` copies bytes to writable pages and then changes
them to read/execute only. Linux/macOS use `mmap`/`mprotect`; Windows uses
`VirtualAlloc`/`VirtualProtect`. No page remains writable and executable at the
same time.

## AArch64 backend

The AArch64 emitter uses AAPCS64 on Linux ARM64, Apple Silicon, and Windows
ARM64. The argument vector arrives in `x0`; the result returns in `x0`; `x9`
preserves the vector while `x0`/`x1` serve operations and external calls.

Constants use `MOVZ`/`MOVK`; jumps use 26-bit `B`, and branches use 19-bit
`CBZ`. SSA values and `PHI` temporaries occupy aligned stack slots.

Tests compare central machine words and verify object formats independently of
the host. On ARM64, the same suite prepares W^X pages and executes code
directly.

## Relocatable objects

A compatible function can be written in all three desktop formats:

```bash
sefirah compilar-elf exemplos/nativo.lisp calcular_nativo calcular.o
sefirah compilar-coff exemplos/nativo.lisp calcular_nativo calcular.obj
sefirah compilar-macho exemplos/nativo.lisp calcular_nativo calcular-macos.o
```

| Format | Architectures | External relocations |
| --- | --- | --- |
| ELF64 `ET_REL` | x86-64, AArch64 | `R_X86_64_PLT32`, `R_AARCH64_CALL26` |
| COFF | AMD64, ARM64 | `IMAGE_REL_AMD64_REL32`, `IMAGE_REL_ARM64_BRANCH26` |
| Mach-O `MH_OBJECT` | x86-64, ARM64 | Darwin external branch relocation |

### ELF64

The object contains `.text`, `.symtab`, `.strtab`, and `.shstrtab`. External
calls add `.rela.text` and undefined global symbols. `e_machine` is
`EM_X86_64` or `EM_AARCH64`.

Because x86-64 ELF requires System V, a frontend running on Windows retains the
IR and re-emits the correct target instead of copying Microsoft ABI bytes.

### COFF

The object contains `.text`, a symbol table, an auxiliary section record, and
a string table. x86-64 is re-emitted with the Microsoft ABI. The result is
input to a Windows linker, not a complete PE executable.

### Mach-O

The object contains `LC_SEGMENT_64`, `__TEXT,__text`, and `LC_SYMTAB`, and
prefixes C symbols with `_` as required by Darwin. x86-64 uses System V;
AArch64 uses AAPCS64.

## External i64 calls

In Lisp, `EXTERNAL-I64` accepts the C name and one or two inputs:

```lisp
(defun call-double (value)
  (external-i64 "dobrar_i64" value))

(defun combine (a b)
  (external-i64 "combinar_i64" a b))
```

In the SDK, `sef_funcao_ir_adicionar_externa_i64` and its `_binaria` variant
register a name and address. `SEF_IR_CHAMAR_EXTERNA_I64` produces another SSA
register. Object commands retain a relocation for the linker.

For in-process execution, binding associates an address with every relocation
for that symbol. W^X preparation adds one trampoline per call:

- x86-64: `call rel32` to `mov rax, address; jmp rax`;
- AArch64: `BL` to `ldr x16, literal; br x16`.

The external address therefore need not be within relative range of the JIT
page, and original bytes remain available to object writers.

`COMPILE-EXTERNAL-I64` resolves names with `LoadLibrary`/`GetProcAddress` on
Windows or `dlopen`/`dlsym` on Linux/macOS. The library object and every
compiled function hold independent references, preventing unload while a
trampoline can still call it.

## Validation

The automated suite verifies:

- interpreter and JIT on both branch paths and in loops;
- unary and binary external calls;
- x86-64 System V/Microsoft and AArch64 emission;
- ELF/COFF/Mach-O headers, symbols, and relocations;
- Windows x64 backend execution through Wine;
- W^X policy and unbound-symbol errors.

The next compiler levels are described in the [1.0 roadmap](roadmap.md).
