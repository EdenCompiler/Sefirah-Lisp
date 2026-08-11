<a id="english"></a>

# Sefirah Lisp — a live Lisp environment for personal computers

**English** · [Português do Brasil](#português-do-brasil)

Sefirah Lisp is a Common Lisp implementation under construction, inspired by
Interlisp and Lisp Machines. Its goal is to bring the language, compiler,
persistent image, custom graphics system, and development tools together in a
readable platform for desktop applications on Windows, Linux, and macOS.

**Current version: 0.0.1 — executable bootstrap on the way to 1.0**

[![Integration](https://github.com/EdenCompiler/Sefirah-Lisp/actions/workflows/ci.yml/badge.svg)](https://github.com/EdenCompiler/Sefirah-Lisp/actions/workflows/ci.yml)
![C](https://img.shields.io/badge/C-17-informational)
![Platforms](https://img.shields.io/badge/platforms-Windows%20%7C%20Linux%20%7C%20macOS-blue)
![License](https://img.shields.io/badge/license-MIT-green)

## What the current bootstrap means

The repository already delivers an executable vertical path: reader,
evaluator, lexical environments, packages, initial conditions, streams,
garbage collection, persistent images, SSA compilation for x86-64/AArch64,
ELF/COFF/Mach-O objects, an initial FFI, CPU rasterizer, custom GUI, and native
windows.

This is not Sefirah 1.0 yet and must not be presented as a complete ANSI Common
Lisp implementation. CLOS, the numeric tower, complete restarts, a self-hosted
compiler, a complete IDE, accessibility, and native distribution remain on
the roadmap.

| Subsystem | Current state |
| --- | --- |
| Language | Common Lisp subset with functions, macros, multiple values, Unicode characters, vectors, hash tables, packages, and non-local control |
| Runtime | Object heap, mark-and-sweep GC, explicit roots, streams, and v9 binary images |
| Compiler | i64 SSA IR, reference interpreter, W^X x86-64/AArch64 JIT, and relocatable objects |
| FFI | Explicit shared libraries and C i64 calls with one or two inputs |
| Graphics | Custom RGB surface, CPU rasterization, and bitmap font |
| GUI | Component tree, layout, themes, focus, hit testing, and actions |
| Platform | X11, Win32, and Cocoa/CoreGraphics raster windows with keyboard, shortcuts, and pointer events |
| IDE | Editable `.lisp` buffer, structural form evaluation, undo/redo, navigable listener history, live object inspector, transcript, and file load/save on Linux, Windows, and macOS |

## Highlights

- modular C17 code with readable PT-BR identifiers and documentation in
  English and PT-BR;
- program files exclusively use `.lisp`; `.sef` is not a public format;
- public language symbols use English to converge with ANSI Common Lisp;
- separate value/function cells, macros, quasiquote, packages, and non-local
  control with cleanup;
- simple `#(...)` vectors, one-dimensional arrays, `AREF`/`SVREF`, and basic
  `SETF` mutation;
- `CHARACTER` objects, `#\` literals, code-point-indexed UTF-8 strings, and
  uniform `ELT` access;
- `EQL` hash tables with `GETHASH`, `SETF`, removal, GC, and persistence;
- Common Lisp multiple values, including the secondary `GETHASH` presence
  flag and propagation through non-local control;
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
./construir/sefirah avaliar "(+ 20 22)"
./construir/sefirah executar exemplos/inicio.lisp
./construir/sefirah repl
```

The REPL accepts multiline forms, changes to a continuation prompt while a
form is open, and prints every value returned by `VALUES`. Use `:ajuda` for
commands and `:sair` to leave.

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
./construir/sefirah compilar-elf exemplos/nativo.lisp calcular_nativo calcular.o
./construir/sefirah compilar-coff exemplos/nativo.lisp calcular_nativo calcular.obj
./construir/sefirah compilar-macho exemplos/nativo.lisp calcular_nativo calcular-macos.o
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
./construir/sefirah imagem salvar desenvolvimento.imagem exemplos/inicio.lisp
./construir/sefirah imagem abrir desenvolvimento.imagem "(fatorial 6)"
./construir/sefirah imagem abrir desenvolvimento.imagem
```

The v9 image preserves the portable Lisp graph, including vectors, characters,
and hash tables, but not JIT bytes or process handles. The reader still accepts
v6, v7, and v8. File streams and shared libraries must be closed before saving.

### GUI and IDE

```bash
./construir/sefirah_ide
./construir/sefirah_ide path/to/program.lisp
```

Sefirah rasterizes the composition over `SefSuperficie`. Panels, labels,
buttons, and fields use the same component tree, flexible layout, theme, focus,
and action dispatch exposed by `sefirah/gui.h`. The IDE provides an editable
buffer, persistent transcript, multiline listener with navigable event history,
a live multiple-value object inspector, linear undo/redo, structural form
evaluation, and `.lisp` file load/save. Tab or a pointer click changes focus;
F5 or Ctrl+Enter runs the buffer; F6 runs the complete form at the cursor;
Ctrl+Z/Ctrl+Y undo and redo; Ctrl+S saves; and Ctrl+O reloads the current path.
Arrow, Home, and End move the editor cursor. In the listener, Up and Down browse
submitted events. Clicking the inspector selects the next returned object. On
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
- structural rewriting, arbitrary file dialogs, debugger, profiler, and
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

[![Integração](https://github.com/EdenCompiler/Sefirah-Lisp/actions/workflows/ci.yml/badge.svg)](https://github.com/EdenCompiler/Sefirah-Lisp/actions/workflows/ci.yml)
![C](https://img.shields.io/badge/C-17-informational)
![Plataformas](https://img.shields.io/badge/plataformas-Windows%20%7C%20Linux%20%7C%20macOS-blue)
![Licença](https://img.shields.io/badge/licen%C3%A7a-MIT-green)

## O que o bootstrap atual significa

O repositório já entrega um caminho vertical executável: leitor, avaliador,
ambientes lexicais, packages, condições iniciais, streams, coleta de lixo,
imagem persistente, compilação SSA para x86-64/AArch64, objetos
ELF/COFF/Mach-O, FFI inicial, rasterizador CPU, GUI própria e janelas nativas.

Esse conjunto ainda não é o Sefirah 1.0 e não deve ser apresentado como uma
implementação ANSI Common Lisp completa. CLOS, numeric tower, restarts
completos, compilador self-hosted, IDE completa, acessibilidade e distribuição
nativa permanecem no roteiro.

| Subsistema | Estado atual |
| --- | --- |
| Linguagem | Subconjunto Common Lisp com funções, macros, valores múltiplos, caracteres Unicode, vetores, tabelas hash, packages e controle não local |
| Runtime | Heap de objetos, GC mark-and-sweep, raízes explícitas, streams e imagem binária v9 |
| Compilador | IR SSA i64, interpretador de referência, JIT W^X x86-64/AArch64 e objetos relocáveis |
| FFI | Bibliotecas compartilhadas explícitas e chamadas C i64 com uma ou duas entradas |
| Gráficos | Superfície RGB, rasterização CPU e fonte bitmap próprias |
| GUI | Árvore de componentes, layout, temas, foco, hit-testing e ações |
| Plataforma | Janelas raster X11, Win32 e Cocoa/CoreGraphics com teclado, atalhos e eventos de ponteiro |
| IDE | Buffer `.lisp` editável, avaliação estrutural de formas, desfazer/refazer, histórico navegável do ouvinte, inspetor de objetos vivos, transcrição e abertura/gravação em Linux, Windows e macOS |

## Destaques

- código C17 modular, com identificadores legíveis em PT-BR e documentação em
  inglês e PT-BR;
- arquivos de programa usam exclusivamente `.lisp`; `.sef` não é um formato
  público;
- símbolos públicos da linguagem em inglês para convergir com ANSI Common
  Lisp;
- células separadas de valor e função, macros, quasiquote, packages e controle
  não local com limpeza;
- vetores simples `#(...)`, arrays unidimensionais, `AREF`/`SVREF` e mutação
  básica por `SETF`;
- objetos `CHARACTER`, literais `#\`, strings UTF-8 indexadas por ponto de
  código e acesso uniforme por `ELT`;
- tabelas hash `EQL` com `GETHASH`, `SETF`, remoção, GC e persistência;
- valores múltiplos de Common Lisp, inclusive o indicador secundário de
  presença de `GETHASH` e propagação por controle não local;
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
./construir/sefirah avaliar "(+ 20 22)"
./construir/sefirah executar exemplos/inicio.lisp
./construir/sefirah repl
```

O REPL aceita formas multilinha, muda para um prompt de continuação enquanto
uma forma está aberta e imprime todos os valores devolvidos por `VALUES`. Use
`:ajuda` para consultar comandos e `:sair` para encerrar.

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
./construir/sefirah compilar-elf exemplos/nativo.lisp calcular_nativo calcular.o
./construir/sefirah compilar-coff exemplos/nativo.lisp calcular_nativo calcular.obj
./construir/sefirah compilar-macho exemplos/nativo.lisp calcular_nativo calcular-macos.o
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
./construir/sefirah imagem salvar desenvolvimento.imagem exemplos/inicio.lisp
./construir/sefirah imagem abrir desenvolvimento.imagem "(fatorial 6)"
./construir/sefirah imagem abrir desenvolvimento.imagem
```

A imagem v9 preserva o grafo Lisp portável, inclusive vetores, caracteres e
tabelas hash, mas não bytes JIT nem handles do processo. O leitor continua
aceitando v6, v7 e v8. Streams de arquivo e bibliotecas compartilhadas precisam
estar fechados antes da gravação.

### GUI e IDE

```bash
./construir/sefirah_ide
./construir/sefirah_ide caminho/para/programa.lisp
```

O Sefirah rasteriza a composição sobre `SefSuperficie`. Painéis, rótulos,
botões e campos usam a mesma árvore de componentes, layout flexível, tema,
foco e despacho de ações expostos por `sefirah/gui.h`. A IDE oferece buffer
editável, transcrição persistente, ouvinte multilinha com histórico navegável de
eventos, inspetor vivo dos valores múltiplos, desfazer/refazer linear, avaliação
estrutural de formas e abertura/gravação de `.lisp`. Tab ou clique muda o foco;
F5 ou Ctrl+Enter executa o buffer; F6 executa a forma completa no cursor;
Ctrl+Z/Ctrl+Y desfazem e refazem; Ctrl+S salva; e Ctrl+O recarrega o caminho
atual. Setas, Home e End movem o cursor do editor. No ouvinte, Cima e Baixo
percorrem os eventos enviados. Clicar no inspetor seleciona o próximo objeto
devolvido. No macOS, Command pode substituir Ctrl.

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
- reescrita estrutural, diálogos de arquivo arbitrário, debugger, profiler e
  histórico transacional seletivo fora de ordem ainda não estão completos.

O estado detalhado e verificável está no
[roteiro de implementação](docs-ptbr/roteiro.md).

## Licença

Sefirah Lisp é distribuído sob a licença MIT. Consulte [LICENSE](LICENSE).
