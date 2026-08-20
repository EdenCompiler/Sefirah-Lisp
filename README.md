<a id="english"></a>

# Sefirah Lisp — a live Lisp environment for personal computers

**English** · [Português do Brasil](#português-do-brasil)

Sefirah Lisp is a Common Lisp implementation under construction, inspired by
Interlisp and Lisp Machines. Its goal is to bring the language, compiler,
persistent image, custom graphics system, and development tools together in a
readable platform for desktop applications on Windows, Linux, and macOS.

**Current version: 0.0.1 — executable bootstrap on the way to 1.0**

Sefirah's implementation uses PT-BR identifiers and comments. Its public
interface is English: CLI commands and help, REPL messages,
reader/evaluator/compiler diagnostics, IDE labels and status messages,
installation checks, and release tooling. Legacy Portuguese CLI command names
remain accepted as compatibility aliases, while documentation and new
automation use the English names.

[![Integration](https://github.com/EdenCompiler/Sefirah-Lisp/actions/workflows/ci.yml/badge.svg)](https://github.com/EdenCompiler/Sefirah-Lisp/actions/workflows/ci.yml)
![C](https://img.shields.io/badge/C-17-informational)
![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20Linux%20%7C%20macOS-blue)
![License](https://img.shields.io/badge/license-MIT-green)

## What the current bootstrap means

The repository already delivers an executable vertical path: reader,
evaluator, lexical environments, packages, dynamic condition handlers and
named restarts, streams,
garbage collection, persistent images, SSA compilation for x86-64/AArch64,
ELF/COFF/Mach-O objects, an initial FFI, CPU rasterizer, custom GUI, and native
windows.

This is not Sefirah 1.0 yet and must not be presented as a complete ANSI Common
Lisp implementation. CLOS, the numeric tower, complete restarts, a self-hosted
compiler, a complete IDE, accessibility, and native distribution remain on
the roadmap.

| Subsystem | Current state |
| --- | --- |
| Language | Common Lisp subset with functions, macros, multiple values, Unicode characters, vectors, hash tables, packages, dynamic condition handlers, named restarts, and non-local control |
| Runtime | Object heap, mark-and-sweep GC, explicit roots, streams, and v10 binary images |
| Compiler | i64 SSA IR, reference interpreter, W^X x86-64/AArch64 JIT, and relocatable objects |
| FFI | Explicit shared libraries and C i64 calls with one or two inputs |
| Graphics | Custom RGB surface, CPU rasterization, and bitmap font |
| GUI | Component tree, layout, themes, focus, hit testing, and actions |
| Platform | X11, Win32, and Cocoa/CoreGraphics raster windows with keyboard, shortcuts, and pointer events |
| IDE | Lisp-workstation desktop, persistent `.lisp` editor tabs, incremental structural evaluation, source browser, recursive live-object inspector, rooted condition history, world snapshots, undo/redo, listener history, and file load/save on Linux, Windows, and macOS |
| Delivery | CI builds, tests, installs, and audits the public SDK on Linux, Windows, and macOS |

## Highlights

- modular C17 code with readable PT-BR identifiers and documentation in
  English and PT-BR;
- program files exclusively use `.lisp`; `.sef` is not a public format;
- public language symbols use English to converge with ANSI Common Lisp;
- separate value/function cells, macros, quasiquote, packages, and non-local
  control with cleanup;
- live installation, replacement, and removal of global bindings through
  `FDEFINITION`, `MAKUNBOUND`, `FMAKUNBOUND`, and generalized `SETF` places;
- persistent symbol metadata through `SYMBOL-PLIST`, `GET`, `REMPROP`, and
  generalized `SETF` places;
- fresh uninterned symbols through `MAKE-SYMBOL`, `COPY-SYMBOL`, and `GENSYM`,
  including world-image persistence;
- collision-free interned temporary symbols through `GENTEMP`;
- package import/unintern operations with preserved symbol identity and status;
- persistent package shadowing with explicit inherited-conflict resolution;
- declarative package construction with phased shadow, use, import, intern, and
  export options;
- reversible package use/export topology with fresh relationship inquiries;
- case-insensitive persistent package nicknames in `DEFPACKAGE` and
  keyword-aware `MAKE-PACKAGE`;
- collision-safe package renaming with atomic nickname-set replacement;
- guarded package deletion with registry cleanup and surviving symbol identity;
- package discovery through `DO-SYMBOLS`, `DO-EXTERNAL-SYMBOLS`, and
  `DO-ALL-SYMBOLS`;
- global registered-package symbol lookup through `FIND-ALL-SYMBOLS`;
- first-class restart objects with dynamic discovery, invocation by name or
  identity, multiple values, and cleanup through `UNWIND-PROTECT`;
- `SIGNAL` and dynamically scoped `HANDLER-BIND`, including recovery by
  choosing an active restart in the signaler's context;
- simple `#(...)` vectors, one-dimensional arrays, `AREF`/`SVREF`, and basic
  `SETF` mutation;
- `CHARACTER` objects, `#\` literals, code-point-indexed UTF-8 strings, and
  uniform `ELT` access;
- `EQL` hash tables with `GETHASH`, `SETF`, removal, GC, and persistence;
- Common Lisp multiple values, including the secondary `GETHASH` presence
  flag and propagation through non-local control;
- canonical `COMMON-LISP:NIL` symbol behavior, constant keyword values, and
  tested symbol/package inquiries;
- `COPY-SEQ`, `REVERSE`, `SUBSEQ`, and `FILL` shared by lists, vectors, and
  strings;
- list composition, destructive operations, search, navigation, and
  multi-list mapping;
- `DEFUN` → SSA IR → x86-64/AArch64 compilation integrated with `COMPILE`;
- W^X JIT pages and System V, Microsoft x64, and AAPCS64 ABI support;
- direct ELF64, COFF, and Mach-O object generation for x86-64 and ARM64;
- `.so`, `.dylib`, and `.dll` objects with explicit closing and safe retention
  by compiled functions;
- persistent images with atomic saving and an explicit external-resource
  policy;
- a GUI drawn by Sefirah itself rather than native widgets;
- green build-and-test CI for Linux, Windows, and macOS.

## Building

Minimum requirements are CMake 3.24, a C17 compiler, and platform
dependencies. On Linux, the current window backend requires X11 development
headers.

```bash
cmake -S . -B construir \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSEFIRAH_AVISOS_COMO_ERROS=ON
cmake --build construir --parallel
ctest --test-dir construir --output-on-failure
```

Install the executables, modular libraries, SDK headers, and both documentation
languages:

```bash
cmake --install construir --prefix instalar
```

On Windows, use Visual Studio or another C17 compiler accepted by CMake. On
macOS, the backend links AppKit, Foundation, and CoreGraphics.

## Quick tour

### Evaluation and REPL

```bash
./construir/sefirah evaluate "(+ 20 22)"
./construir/sefirah run exemplos/inicio.lisp
./construir/sefirah repl
```

The REPL accepts multiline forms, changes to a continuation prompt while a
form is open, and prints every value returned by `VALUES`. Use `:help` for
commands and `:quit` to leave.

```lisp
(defun factorial (n)
  (if (< n 2)
      1
      (* n (factorial (- n 1)))))

(factorial 6) ; => 720
```

Vectors and hash tables are heap objects, participate in GC, and survive a
live image:

```lisp
(let ((settings (make-hash-table)))
  (setf (gethash 'theme settings) :dark)
  (gethash 'theme settings)) ; => :DARK
```

### Native compilation

The i64 subset can be installed in the current process or written as a
relocatable object:

```lisp
(defun calculate (x y)
  (if (< x y)
      (+ (* x 2) y)
      (- x y)))

(compile 'calculate)
(calculate 10 22) ; => 42
```

```bash
./construir/sefirah compile-elf exemplos/nativo.lisp calcular_nativo calcular.o
./construir/sefirah compile-coff exemplos/nativo.lisp calcular_nativo calcular.obj
./construir/sefirah compile-macho exemplos/nativo.lisp calcular_nativo calcular-macos.o
```

The C files under `exemplos/integracao-c/` demonstrate consumption of the C17
SDK and generated objects. They are deliberately integration examples, not
Sefirah source files.

### Shared libraries

```lisp
(defun call-double (value)
  (external-i64 "dobrar_i64" value))

(define calculations (open-shared-library "./libcalculos.so"))
(compile-external-i64 'call-double calculations)
(close-shared-library calculations)

; The compiled function retains its own resource reference.
(call-double 21) ; => 42
```

Use a `.dll` on Windows or a `.dylib` on macOS. The native handle unloads only
after the object and all compiled functions release it.

### Live image

```bash
./construir/sefirah image save desenvolvimento.imagem exemplos/inicio.lisp
./construir/sefirah image open desenvolvimento.imagem "(fatorial 6)"
./construir/sefirah image open desenvolvimento.imagem
```

The v10 image preserves the portable Lisp graph, including vectors, characters,
hash tables, and first-class restart objects, but not JIT bytes or process
handles. The reader still accepts v6 through v9. On load, targeted migrations
restore canonical `NIL` package membership and primitives added after an older
world was saved. File streams and shared libraries must be closed before saving.

### GUI and IDE

```bash
./construir/sefirah_ide
./construir/sefirah_ide path/to/program.lisp
./construir/sefirah_ide path/to/project-directory
```

Sefirah rasterizes the composition over `SefSuperficie`. Panels, labels,
buttons, and fields use the same component tree, flexible layout, theme, focus,
and action dispatch exposed by `sefirah/gui.h`. The IDE provides an editable
buffer, persistent transcript, multiline listener with navigable event history,
a recursive multiple-value object inspector, linear undo/redo, structural form
evaluation, an indexed definition browser, incremental installation of changed
top-level forms, symbol-at-point definition lookup, structural caller/reference
navigation, restorable world snapshots, and `.lisp` file load/save. The warm
cream, sage, olive, and amber workstation palette evokes Interlisp and Lisp
Machines without giving up a modern workbench layout. Files open in persistent
editor tabs; each tab preserves its own
cursor, selection, undo/redo timeline, incremental state, and unsaved marker.
Opening a project directory enables the recursive Explorer sidebar, which
indexes `.lisp` sources in deterministic order. Up/Down selects a source and
Enter or a pointer click opens it. A command toolbar exposes Run, Run Form, Run
Changes, Save, Snapshot, Restore, file/folder creation and opening, Explorer
refresh, Symbols, References, and Commands as pointer-accessible buttons. Ctrl+P opens a filtered
workspace file picker; Ctrl+Shift+P opens a searchable command palette whose
actions operate on the live Lisp world. Ctrl+T or the Symbols toolbar button
opens a case-insensitive workspace symbol picker. It indexes real top-level
definitions across every project source, includes unsaved editor buffers,
preserves mixed-case names and paths, and opens the selected definition at its
exact source position. Each result is marked `SOURCE ONLY`, `INTERNED`,
`LIVE VALUE`, `LIVE FUNCTION`, or `LIVE VALUE/FUNCTION` from a read-only query
against the current Lisp world. Clicking a
tab activates it. Tab or a pointer
click elsewhere changes focus among the editor, inspector, debugger, and listener;
F5 or Ctrl+Enter runs the buffer; F6 runs the complete form at the cursor;
Shift+F6 selects that complete form for structural replacement or deletion;
F7 runs only forms changed since their last successful installation; F8 and
Shift+F8 navigate named definitions; F9/F10 save and restore the live Lisp
world beside the current source as `.imagem`; Shift+F9/Shift+F10 navigate the
bounded history of unhandled conditions; F11 jumps to the definition of
the symbol at the cursor across project files; and F12/Shift+F12 cycle its structural references
across the workspace (or the current buffer when no folder is open). Reference
results include unsaved tabs and exclude strings, comments, and definition
sites. Ctrl+Z/Ctrl+Y
undo and redo; Ctrl+S saves; and Ctrl+O opens the path prompt. Arrow, Home,
and End move the editor cursor; holding Shift extends a UTF-8-safe selection,
and typing or Backspace replaces or removes it as one undoable edit. In the
listener, Up and Down browse submitted events. In the inspector, Up/Down select an object component, Enter opens it,
Backspace returns to its parent, and Left/Right switch between returned roots.
The complete navigation path remains rooted in the garbage collector. Clicking
the definition browser advances to the next definition. In the debugger,
Up/Down select a condition and Enter opens its Lisp object in the recursive
inspector; restart objects expose their name and current active state there. On
macOS, Command can replace Ctrl.

## Architecture

```text
program .lisp
      │
      ├── reader → evaluator → objects/environments → image
      │                         │
      │                         └── streams and shared libraries
      │
      └── i64 frontend → verified SSA IR → reference interpreter
                                      ├── x86-64/AArch64 JIT
                                      └── ELF / COFF / Mach-O

GUI components → layout/interaction → CPU rasterizer → window backend
                                                       ├── X11
                                                       ├── Win32
                                                       └── Cocoa/CoreGraphics
```

The runtime does not know about windows, the rasterizer does not know about
X11/Win32/Cocoa, and the compiler keeps its IR independent of object writers.

| Module | Responsibility | Public API |
| --- | --- | --- |
| `nucleo` | objects, reader, evaluator, GC, streams, packages, and images | `sefirah/runtime.h` |
| `compilador` | SSA IR, verifier, backends, JIT, and objects | `sefirah/compilador.h` |
| `graficos` | RGB surfaces, rasterization, and bitmap font | `sefirah/graficos.h` |
| `gui` | components, layout, themes, and interaction | `sefirah/gui.h` |
| `plataforma` | host windows and events | `sefirah/janela.h` |
| `cli` | text commands, REPL, compilation, and images | `sefirah` executable |
| `ide` | graphical listener, editor, and inspector | `ide/ide.h`, `sefirah_ide` executable |

## Examples

| File | Demonstrates |
| --- | --- |
| `exemplos/inicio.lisp` | evaluation, functions, and images |
| `exemplos/nativo.lisp` | a function compatible with the i64 frontend |
| `exemplos/externa.lisp` | C imports with one and two i64 inputs |
| `exemplos/integracao-c/chamar_nativo.c` | linking a Sefirah object from C |
| `exemplos/integracao-c/chamar_externa.c` | linking an object with an external C symbol |
| `exemplos/integracao-c/gerar_objeto_externo.c` | direct compiler SDK use |

## Documentation

| Guide | English | Português do Brasil |
| --- | --- | --- |
| Bootstrap manual | [manual](docs-en/manual.md) | [manual](docs-ptbr/manual.md) |
| Architecture | [architecture](docs-en/architecture.md) | [arquitetura](docs-ptbr/arquitetura.md) |
| Compiler | [compiler](docs-en/compiler.md) | [compilador](docs-ptbr/compilador.md) |
| ANSI conformance | [conformance](docs-en/conformance.md) | [conformidade](docs-ptbr/conformidade.md) |
| 1.0 roadmap | [roadmap](docs-en/roadmap.md) | [roteiro](docs-ptbr/roteiro.md) |

## Current limitations

- ANSI Common Lisp conformance is incomplete;
- the integrated compiler accepts only the documented i64 subset;
- the typed FFI does not cover general structs, floats, pointers, or callbacks;
- the GC is not generational and lacks native-code stack maps;
- the GUI lacks vector fonts, HiDPI, IME, accessibility, and complete desktop
  integration;
- Wayland, complete IME composition, and distribution packages remain pending;
- structural rewriting, arbitrary file dialogs, interactive restart selection,
  profiler, and
  selective out-of-order transactional history are not complete.

Detailed, verifiable status is in the [implementation roadmap](docs-en/roadmap.md).

## License

Sefirah Lisp is distributed under the MIT License. See [LICENSE](LICENSE).

---

<a id="português-do-brasil"></a>

# Sefirah Lisp — ambiente Lisp vivo para computadores pessoais

[English](#english) ·
**Português do Brasil**

Sefirah Lisp é uma implementação de Common Lisp em construção, inspirada no
Interlisp e nas Lisp Machines. O objetivo é reunir linguagem, compilador,
imagem persistente, sistema gráfico próprio e ferramentas de desenvolvimento
em uma plataforma legível para aplicações desktop no Windows, Linux e macOS.

**Versão atual: 0.0.1 — bootstrap executável rumo ao 1.0**

A implementação do Sefirah usa identificadores e comentários em PT-BR. A
interface pública usa inglês: comandos e ajuda da CLI, mensagens do REPL,
diagnósticos do leitor/avaliador/compilador, rótulos e estados da IDE,
verificações de instalação e ferramentas de release. Os nomes antigos dos
comandos em português continuam aceitos como aliases de compatibilidade, mas a
documentação e novas automações usam os nomes em inglês.

[![Integração](https://github.com/EdenCompiler/Sefirah-Lisp/actions/workflows/ci.yml/badge.svg)](https://github.com/EdenCompiler/Sefirah-Lisp/actions/workflows/ci.yml)
![C](https://img.shields.io/badge/C-17-informational)
![Plataformas](https://img.shields.io/badge/plataformas-Windows%20%7C%20Linux%20%7C%20macOS-blue)
![Licença](https://img.shields.io/badge/licen%C3%A7a-MIT-green)

## O que o bootstrap atual significa

O repositório já entrega um caminho vertical executável: leitor, avaliador,
ambientes lexicais, packages, handlers dinâmicos de condições e restarts
nomeados, streams, coleta de lixo,
imagem persistente, compilação SSA para x86-64/AArch64, objetos
ELF/COFF/Mach-O, FFI inicial, rasterizador CPU, GUI própria e janelas nativas.

Esse conjunto ainda não é o Sefirah 1.0 e não deve ser apresentado como uma
implementação ANSI Common Lisp completa. CLOS, numeric tower, restarts
completos, compilador self-hosted, IDE completa, acessibilidade e distribuição
nativa permanecem no roteiro.

| Subsistema | Estado atual |
| --- | --- |
| Linguagem | Subconjunto Common Lisp com funções, macros, valores múltiplos, caracteres Unicode, vetores, tabelas hash, packages, handlers dinâmicos de condições, restarts nomeados e controle não local |
| Runtime | Heap de objetos, GC mark-and-sweep, raízes explícitas, streams e imagem binária v10 |
| Compilador | IR SSA i64, interpretador de referência, JIT W^X x86-64/AArch64 e objetos relocáveis |
| FFI | Bibliotecas compartilhadas explícitas e chamadas C i64 com uma ou duas entradas |
| Gráficos | Superfície RGB, rasterização CPU e fonte bitmap próprias |
| GUI | Árvore de componentes, layout, temas, foco, hit-testing e ações |
| Plataforma | Janelas raster X11, Win32 e Cocoa/CoreGraphics com teclado, atalhos e eventos de ponteiro |
| IDE | Desktop de estação Lisp, abas persistentes de edição `.lisp`, avaliação estrutural incremental, navegador de fontes, inspetor recursivo de objetos vivos, histórico enraizado de condições, snapshots do mundo, desfazer/refazer, histórico do ouvinte e abertura/gravação em Linux, Windows e macOS |
| Entrega | A CI compila, testa, instala e audita o SDK público em Linux, Windows e macOS |

## Destaques

- código C17 modular, com identificadores legíveis em PT-BR e documentação em
  inglês e PT-BR;
- arquivos de programa usam exclusivamente `.lisp`; `.sef` não é um formato
  público;
- símbolos públicos da linguagem em inglês para convergir com ANSI Common
  Lisp;
- células separadas de valor e função, macros, quasiquote, packages e controle
  não local com limpeza;
- instalação, substituição e remoção de bindings globais no mundo vivo por
  `FDEFINITION`, `MAKUNBOUND`, `FMAKUNBOUND` e lugares `SETF`;
- metadados persistentes de símbolos por `SYMBOL-PLIST`, `GET`, `REMPROP` e
  lugares generalizados de `SETF`;
- símbolos novos e não internados por `MAKE-SYMBOL`, `COPY-SYMBOL` e `GENSYM`,
  incluindo persistência na imagem do mundo;
- símbolos temporários internados sem colisão por `GENTEMP`;
- importação/uninterning em packages com identidade e estado preservados;
- shadowing persistente de packages com resolução explícita de conflitos herdados;
- construção declarativa de packages com opções em fases para shadow, use,
  importação, internamento e exportação;
- topologia reversível de uso/exportação com consultas novas de relacionamentos;
- apelidos persistentes e case-insensitive de packages em `DEFPACKAGE` e
  `MAKE-PACKAGE` com keywords;
- renomeação de packages com detecção de conflitos e troca atômica dos apelidos;
- remoção protegida de packages com limpeza do registro e símbolos sobreviventes;
- descoberta de símbolos de packages por `DO-SYMBOLS`, `DO-EXTERNAL-SYMBOLS` e
  `DO-ALL-SYMBOLS`;
- consulta global de símbolos de packages registrados por `FIND-ALL-SYMBOLS`;
- objetos restart de primeira classe com descoberta dinâmica, invocação por
  nome ou identidade, valores múltiplos e limpeza por `UNWIND-PROTECT`;
- `SIGNAL` e `HANDLER-BIND` com escopo dinâmico, inclusive recuperação pela
  escolha de um restart ativo no contexto do sinalizador;
- vetores simples `#(...)`, arrays unidimensionais, `AREF`/`SVREF` e mutação
  básica por `SETF`;
- objetos `CHARACTER`, literais `#\`, strings UTF-8 indexadas por ponto de
  código e acesso uniforme por `ELT`;
- tabelas hash `EQL` com `GETHASH`, `SETF`, remoção, GC e persistência;
- valores múltiplos de Common Lisp, inclusive o indicador secundário de
  presença de `GETHASH` e propagação por controle não local;
- comportamento canônico do símbolo `COMMON-LISP:NIL`, valores keyword
  constantes e consultas testadas de símbolos/packages;
- `COPY-SEQ`, `REVERSE`, `SUBSEQ` e `FILL` compartilhados por listas, vetores e
  strings;
- composição de listas, operações destrutivas, busca, navegação e mapeamento
  sobre múltiplas listas;
- compilação `DEFUN` → IR SSA → x86-64/AArch64 integrada a `COMPILE`;
- páginas JIT W^X e suporte às ABIs System V, Microsoft x64 e AAPCS64;
- geração direta de objetos ELF64, COFF e Mach-O para x86-64 e ARM64;
- objetos `.so`, `.dylib` e `.dll` com fechamento explícito e retenção segura
  por funções compiladas;
- imagem persistente com gravação atômica e política explícita para recursos
  externos;
- GUI desenhada pelo próprio Sefirah, sem depender de widgets nativos;
- CI verde de build e testes para Linux, Windows e macOS.

## Compilação

Requisitos mínimos: CMake 3.24, compilador C17 e as dependências da plataforma.
No Linux, o backend de janela atual exige os headers de desenvolvimento do X11.

```bash
cmake -S . -B construir \
  -DCMAKE_BUILD_TYPE=Debug \
  -DSEFIRAH_AVISOS_COMO_ERROS=ON
cmake --build construir --parallel
ctest --test-dir construir --output-on-failure
```

Instale os executáveis, bibliotecas modulares, headers do SDK e os dois idiomas
da documentação:

```bash
cmake --install construir --prefix instalar
```

No Windows, use o Visual Studio ou outro compilador C17 aceito pelo CMake. No
macOS, o backend liga AppKit, Foundation e CoreGraphics.

## Visão rápida

### Avaliação e REPL

```bash
./construir/sefirah evaluate "(+ 20 22)"
./construir/sefirah run exemplos/inicio.lisp
./construir/sefirah repl
```

O REPL aceita formas multilinha, muda para um prompt de continuação enquanto
uma forma está aberta e imprime todos os valores devolvidos por `VALUES`. Use
`:help` para consultar comandos e `:quit` para encerrar.

```lisp
(defun fatorial (n)
  (if (< n 2)
      1
      (* n (fatorial (- n 1)))))

(fatorial 6) ; => 720
```

Vetores e tabelas hash são objetos do heap, participam do GC e sobrevivem à
imagem viva:

```lisp
(let ((configuracao (make-hash-table)))
  (setf (gethash 'tema configuracao) :escuro)
  (gethash 'tema configuracao)) ; => :ESCURO
```

### Compilação nativa

O subconjunto i64 pode ser instalado no processo atual ou gravado como objeto
relocável:

```lisp
(defun calcular (x y)
  (if (< x y)
      (+ (* x 2) y)
      (- x y)))

(compile 'calcular)
(calcular 10 22) ; => 42
```

```bash
./construir/sefirah compile-elf exemplos/nativo.lisp calcular_nativo calcular.o
./construir/sefirah compile-coff exemplos/nativo.lisp calcular_nativo calcular.obj
./construir/sefirah compile-macho exemplos/nativo.lisp calcular_nativo calcular-macos.o
```

Os arquivos C em `exemplos/integracao-c/` demonstram o consumo do SDK C17 e
dos objetos gerados. Eles são deliberadamente exemplos de integração, não
fontes Sefirah.

### Bibliotecas compartilhadas

```lisp
(defun chamar-dobro (valor)
  (external-i64 "dobrar_i64" valor))

(define calculos (open-shared-library "./libcalculos.so"))
(compile-external-i64 'chamar-dobro calculos)
(close-shared-library calculos)

; A função compilada mantém sua própria referência ao recurso.
(chamar-dobro 21) ; => 42
```

No Windows use uma `.dll`; no macOS, uma `.dylib`. O handle nativo só é
descarregado quando o objeto e todas as funções compiladas o liberam.

### Imagem viva

```bash
./construir/sefirah image save desenvolvimento.imagem exemplos/inicio.lisp
./construir/sefirah image open desenvolvimento.imagem "(fatorial 6)"
./construir/sefirah image open desenvolvimento.imagem
```

A imagem v10 preserva o grafo Lisp portável, inclusive vetores, caracteres,
tabelas hash e objetos restart de primeira classe, mas não bytes JIT nem
handles do processo. O leitor continua aceitando de v6 a v9. Ao abrir,
migrações direcionadas restauram a associação canônica de `NIL` ao package e
primitivas adicionadas depois da gravação de um mundo antigo. Streams de
arquivo e bibliotecas compartilhadas precisam estar fechados antes da gravação.

### GUI e IDE

```bash
./construir/sefirah_ide
./construir/sefirah_ide caminho/para/programa.lisp
./construir/sefirah_ide caminho/para/diretorio-do-projeto
```

O Sefirah rasteriza a composição sobre `SefSuperficie`. Painéis, rótulos,
botões e campos usam a mesma árvore de componentes, layout flexível, tema,
foco e despacho de ações expostos por `sefirah/gui.h`. A IDE oferece buffer
editável, transcrição persistente, ouvinte multilinha com histórico navegável de
eventos, inspetor recursivo dos valores múltiplos, desfazer/refazer linear, avaliação
estrutural de formas, navegador indexado de definições, instalação incremental
das formas de topo alteradas, localização da definição do símbolo no cursor,
navegação estrutural de callers/referências, snapshots restauráveis do mundo e
abertura/gravação de `.lisp`. A paleta quente em creme, sálvia, oliva e âmbar
evoca Interlisp e Lisp Machines sem abandonar o layout de workbench moderno.
Arquivos abrem em abas persistentes; cada aba preserva cursor, seleção, linha do tempo de
desfazer/refazer, estado incremental e indicador de alterações não gravadas.
Abrir um diretório de projeto ativa a barra lateral Explorer recursiva, que
indexa fontes `.lisp` em ordem determinística. Cima/Baixo selecionam um fonte e
Enter ou clique o abre. Uma barra de comandos expõe Run, Run Form, Run Changes,
Save, Snapshot, Restore, criação/abertura de arquivos e pastas, atualização do
Explorer, Symbols, References e Commands como botões acessíveis pelo ponteiro. Ctrl+P abre o seletor
filtrado de fontes do workspace; Ctrl+Shift+P abre uma paleta pesquisável cujas
ações operam sobre o mundo Lisp vivo. Ctrl+T ou o botão Symbols abre um seletor
de símbolos do workspace sem diferenciar caixa. Ele indexa definições reais de
topo em todos os fontes do projeto, inclui buffers ainda não gravados, preserva
nomes e caminhos com caixa mista e abre a definição na posição exata. Cada
resultado recebe `SOURCE ONLY`, `INTERNED`, `LIVE VALUE`, `LIVE FUNCTION` ou
`LIVE VALUE/FUNCTION` por uma consulta somente de leitura ao mundo Lisp atual.
Os prompts
preservam exatamente as letras
maiúsculas e minúsculas digitadas em caminhos, e a fonte bitmap desenha as duas
caixas de modo distinto, inclusive em sistemas de arquivos sensíveis a caixa.
Clicar em uma
aba a ativa. Tab ou clique nas demais
áreas alterna o foco entre editor, inspetor, depurador e ouvinte; F5 ou Ctrl+Enter
executa o buffer; F6 executa a forma completa no cursor; Shift+F6 seleciona a
forma completa para substituição ou remoção estrutural; F7 executa somente as
formas alteradas desde a última instalação bem-sucedida; F8 e Shift+F8 navegam
pelas definições nomeadas; F9/F10 salvam e restauram o mundo Lisp ao lado do
fonte como `.imagem`; Shift+F9/Shift+F10 navegam pelo histórico limitado de
condições não tratadas; F11 vai à definição do símbolo no cursor mesmo quando
ela está em outro arquivo do projeto; e
F12/Shift+F12 percorrem suas referências estruturais em todo o workspace (ou no
buffer atual quando nenhuma pasta está aberta). Os resultados incluem abas não
gravadas e excluem strings, comentários e locais de definição. Ctrl+Z/Ctrl+Y desfazem e refazem;
Ctrl+S salva; e Ctrl+O abre o prompt de caminho. Setas, Home e End movem o
cursor do editor; com Shift, estendem uma seleção segura para UTF-8, e digitar
ou usar Backspace a substitui ou remove como uma única edição reversível. No
ouvinte, Cima e Baixo percorrem os eventos enviados. No
inspetor, Cima/Baixo selecionam um componente, Enter o abre, Backspace volta ao
pai e Esquerda/Direita alternam entre as raízes devolvidas. O caminho completo
permanece enraizado no coletor de lixo. Clicar no navegador avança para a
próxima definição. No depurador, Cima/Baixo selecionam uma condição e Enter
abre seu objeto Lisp no inspetor recursivo; objetos restart expõem ali o nome e
o estado ativo corrente. No macOS, Command pode substituir Ctrl.

## Arquitetura

```text
programa .lisp
      │
      ├── leitor → avaliador → objetos/ambientes → imagem
      │                         │
      │                         └── streams e bibliotecas compartilhadas
      │
      └── frontend i64 → IR SSA verificada → interpretador de referência
                                      ├── JIT x86-64/AArch64
                                      └── ELF / COFF / Mach-O

componentes GUI → layout/interação → rasterizador CPU → backend de janela
                                                      ├── X11
                                                      ├── Win32
                                                      └── Cocoa/CoreGraphics
```

O runtime não conhece janelas, o rasterizador não conhece X11/Win32/Cocoa e o
compilador mantém a IR independente dos gravadores de objetos.

| Módulo | Responsabilidade | API pública |
| --- | --- | --- |
| `nucleo` | objetos, leitor, avaliador, GC, streams, packages e imagens | `sefirah/runtime.h` |
| `compilador` | IR SSA, verificador, backends, JIT e objetos | `sefirah/compilador.h` |
| `graficos` | superfícies RGB, rasterização e fonte bitmap | `sefirah/graficos.h` |
| `gui` | componentes, layout, temas e interação | `sefirah/gui.h` |
| `plataforma` | janelas e eventos do hospedeiro | `sefirah/janela.h` |
| `cli` | comandos textuais, REPL, compilação e imagens | executável `sefirah` |
| `ide` | ouvinte, editor e inspetor gráficos | `ide/ide.h`, executável `sefirah_ide` |

## Exemplos

| Arquivo | Demonstração |
| --- | --- |
| `exemplos/inicio.lisp` | avaliação, funções e imagens |
| `exemplos/nativo.lisp` | função compatível com o frontend i64 |
| `exemplos/externa.lisp` | imports C com uma e duas entradas i64 |
| `exemplos/integracao-c/chamar_nativo.c` | ligação de objeto Sefirah por C |
| `exemplos/integracao-c/chamar_externa.c` | ligação de objeto com símbolo C externo |
| `exemplos/integracao-c/gerar_objeto_externo.c` | uso direto do SDK do compilador |

## Documentação

| Guia | English | Português do Brasil |
| --- | --- | --- |
| Manual do bootstrap | [manual](docs-en/manual.md) | [manual](docs-ptbr/manual.md) |
| Arquitetura | [architecture](docs-en/architecture.md) | [arquitetura](docs-ptbr/arquitetura.md) |
| Compilador | [compiler](docs-en/compiler.md) | [compilador](docs-ptbr/compilador.md) |
| Conformidade ANSI | [conformance](docs-en/conformance.md) | [conformidade](docs-ptbr/conformidade.md) |
| Roteiro para 1.0 | [roadmap](docs-en/roadmap.md) | [roteiro](docs-ptbr/roteiro.md) |

## Limitações atuais

- a conformidade ANSI Common Lisp ainda está incompleta;
- o compilador integrado aceita somente o subconjunto i64 documentado;
- a FFI tipada não cobre structs, floats, ponteiros ou callbacks gerais;
- o GC não é geracional e não possui mapas de pilha do código nativo;
- a GUI ainda não possui fontes vetoriais, HiDPI, IME, acessibilidade ou
  integração desktop completa;
- Wayland, composição IME completa e pacotes de distribuição continuam pendentes;
- reescrita estrutural, diálogos de arquivo arbitrário, seleção interativa de
  restarts, profiler e
  histórico transacional seletivo fora de ordem ainda não estão completos.

O estado detalhado e verificável está no
[roteiro de implementação](docs-ptbr/roteiro.md).

## Licença

Sefirah Lisp é distribuído sob a licença MIT. Consulte [LICENSE](LICENSE).
