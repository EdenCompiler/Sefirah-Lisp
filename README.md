# Sefirah Lisp — ambiente Lisp vivo para computadores pessoais

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
| Linguagem | Subconjunto Common Lisp com funções, macros, packages, controle não local e condições iniciais |
| Runtime | Heap de objetos, GC mark-and-sweep, raízes explícitas, streams e imagem binária v6 |
| Compilador | IR SSA i64, interpretador de referência, JIT W^X x86-64/AArch64 e objetos relocáveis |
| FFI | Bibliotecas compartilhadas explícitas e chamadas C i64 com uma ou duas entradas |
| Gráficos | Superfície RGB, rasterização CPU e fonte bitmap próprias |
| GUI | Árvore de componentes, layout, temas, foco, hit-testing e ações |
| Plataforma | X11, Win32 e ponte Cocoa/CoreGraphics; integração macOS ainda parcial |
| IDE | Ouvinte gráfico funcional em X11/Win32; editor e inspetor ainda demonstrativos |

## Destaques

- código C17 modular, com nomes, comentários e documentação em PT-BR;
- arquivos de programa exclusivamente `.lisp` — a extensão `.sef` não faz
  parte do formato público;
- símbolos públicos da linguagem em inglês para convergir com ANSI Common
  Lisp;
- células separadas de valor e função, macros, quasiquote, packages e controle
  não local com limpeza;
- compilação `DEFUN` → IR SSA → x86-64/AArch64 integrada a `COMPILE`;
- páginas JIT W^X e suporte às ABIs System V, Microsoft x64 e AAPCS64;
- geração direta de objetos ELF64, COFF e Mach-O para x86-64 e ARM64;
- bibliotecas `.so`, `.dylib` e `.dll` representadas por objetos Lisp com
  fechamento explícito e retenção segura por funções compiladas;
- imagem persistente com gravação atômica e política explícita para recursos
  externos;
- GUI desenhada pelo próprio Sefirah, sem depender de widgets nativos;
- CI declarada para Linux, Windows e macOS.

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

Instale o executável, as bibliotecas modulares, os headers do SDK e a
documentação:

```bash
cmake --install construir --prefix instalar
```

No Windows, use o gerador do Visual Studio ou outro compilador C17 aceito pelo
CMake. No macOS, o backend liga AppKit, Foundation e CoreGraphics.

## Visão rápida

### Avaliação e REPL

```bash
./construir/sefirah avaliar "(+ 20 22)"
./construir/sefirah executar exemplos/inicio.lisp
./construir/sefirah repl
```

Um programa Sefirah comum continua legível como Lisp:

```lisp
(defun fatorial (n)
  (if (< n 2)
      1
      (* n (fatorial (- n 1)))))

(fatorial 6) ; => 720
```

### Compilação nativa

O subconjunto i64 pode ser instalado no próprio processo ou gravado como
objeto relocável:

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

Os arquivos C usados para demonstrar ligação nativa ficam deliberadamente em
`exemplos/integracao-c/`. Eles são consumidores do SDK C17, não código-fonte da
linguagem Sefirah.

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
descarregado quando o objeto e todas as funções compiladas deixam de possuí-lo.

### Imagem viva

```bash
./construir/sefirah imagem salvar desenvolvimento.imagem exemplos/inicio.lisp
./construir/sefirah imagem abrir desenvolvimento.imagem "(fatorial 6)"
./construir/sefirah imagem abrir desenvolvimento.imagem
```

A imagem preserva o grafo Lisp portável, mas não bytes JIT nem handles do
processo. Streams de arquivo e bibliotecas compartilhadas precisam estar
fechados antes da gravação.

### GUI e IDE

```bash
./construir/sefirah ide
```

A composição é rasterizada pelo Sefirah sobre `SefSuperficie`. Painéis,
rótulos, botões e campos usam a mesma árvore de componentes, layout flexível,
tema, foco e despacho de ações expostos pelo SDK em `sefirah/gui.h`.

## Arquitetura sugerida

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

As camadas não escondem ownership nem formato de saída. O runtime não conhece
janelas; o rasterizador não conhece X11, Win32 ou Cocoa; e o compilador mantém
a IR independente dos gravadores de objeto.

## Módulos

| Módulo | Responsabilidade | API pública |
| --- | --- | --- |
| `nucleo` | objetos, leitor, avaliador, GC, streams, packages e imagem | `sefirah/runtime.h` |
| `compilador` | IR SSA, verificador, backends, JIT e objetos | `sefirah/compilador.h` |
| `graficos` | superfície RGB, rasterização e fonte bitmap | `sefirah/graficos.h` |
| `gui` | componentes, layout, temas e interação | `sefirah/gui.h` |
| `plataforma` | janela e eventos do sistema hospedeiro | `sefirah/janela.h` |
| `cli` | comandos públicos e composição inicial da IDE | executável `sefirah` |

## Exemplos

| Arquivo | Demonstração |
| --- | --- |
| `exemplos/inicio.lisp` | avaliação, funções e imagem |
| `exemplos/nativo.lisp` | função compatível com o frontend i64 |
| `exemplos/externa.lisp` | imports C com uma e duas entradas i64 |
| `exemplos/integracao-c/chamar_nativo.c` | ligação de objeto Sefirah por um programa C |
| `exemplos/integracao-c/chamar_externa.c` | ligação de um objeto com símbolo C externo |
| `exemplos/integracao-c/gerar_objeto_externo.c` | uso direto do SDK do compilador |

## Documentação

| Guia | Conteúdo |
| --- | --- |
| [Manual do bootstrap](docs/manual.md) | linguagem disponível, streams, packages, FFI, GUI e imagens |
| [Arquitetura](docs/arquitetura.md) | camadas, ownership e limites entre módulos |
| [Compilador](docs/compilador.md) | IR SSA, backends, JIT, objetos e chamadas externas |
| [Roteiro para 1.0](docs/roteiro.md) | marcos, pendências e critérios de entrega |

## Limitações atuais

- ainda não existe conformidade ANSI Common Lisp completa;
- o compilador integrado aceita somente o subconjunto i64 documentado;
- a FFI tipada ainda não cobre structs, floats, ponteiros ou callbacks gerais;
- o GC ainda não é geracional e não possui mapas de pilha do código nativo;
- a GUI ainda não possui fontes vetoriais, HiDPI, IME, acessibilidade ou
  integração desktop completa;
- teclado no backend macOS, Wayland e empacotadores de distribuição continuam
  pendentes;
- o editor, inspetor, debugger, profiler e histórico transacional da IDE ainda
  não estão prontos.

O estado detalhado e verificável está no
[roteiro de implementação](docs/roteiro.md).

## Licença

Sefirah Lisp é distribuído sob a licença MIT. Consulte [LICENSE](LICENSE).
