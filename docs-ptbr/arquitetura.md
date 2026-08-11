# Arquitetura do Sefirah Lisp

[English](../docs-en/architecture.md) · **Português do Brasil**

## Objetivo

Sefirah é uma plataforma Lisp viva para aplicações desktop, não apenas um
interpretador embutido em uma GUI. A arquitetura mantém linguagem, compilador,
imagem, gráficos e integração nativa como módulos explícitos para que cada
camada possa evoluir sem esconder ownership ou depender de um único sistema
operacional.

Camadas principais:

1. **Núcleo Lisp** — objetos, packages, ambientes, leitor, avaliador,
   primitivas, condições, streams, GC e imagem persistente.
2. **Compilador** — IR SSA verificada, interpretador de referência, backends
   x86-64/AArch64, JIT W^X e gravadores ELF/COFF/Mach-O.
3. **Gráficos** — superfície RGB, primitivas raster e fonte bitmap sem
   dependência de uma API de janela.
4. **GUI** — árvore de componentes, layout flexível, temas, hit-testing, foco e
   ações sobre o rasterizador próprio.
5. **Plataforma** — apresentação e eventos em X11, Win32 ou
   Cocoa/CoreGraphics.
6. **CLI e IDE** — comandos públicos e REPL no executável textual; ambiente de
   desenvolvimento gráfico em um executável separado.

## Módulos e dependências

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

| Módulo | Conhece | Não conhece |
| --- | --- | --- |
| `nucleo` | objetos, compilador e recursos do processo | janelas e widgets |
| `compilador` | IR, ABIs e formatos de objeto | packages, GUI e IDE |
| `graficos` | pixels, formas e texto bitmap | X11, Win32 e Cocoa |
| `plataforma` | janela, apresentação e eventos nativos | avaliação Lisp |
| `cli` | APIs públicas de runtime e comandos textuais | GUI, plataforma e detalhes internos do heap |
| `ide_nucleo` | runtime e sessão de editor/ouvinte por `ide/ide.h` | janelas e eventos de plataforma |
| `ide` | motor de sessão, GUI e plataforma | comandos da CLI e detalhes internos do núcleo |

Cada build compila exatamente um backend de janela. O backend macOS permanece
C puro e confina as chamadas tipadas ao runtime Objective-C dentro do adaptador
de plataforma.
`sefirah/interno.h` reúne contratos privados do núcleo para o build e não
oferece estabilidade de SDK ou ABI; aplicações devem usar
`sefirah/runtime.h`.

## Fluxo da linguagem

```text
texto .lisp
    │
    ▼
  leitor ──► forma Lisp ──► avaliador ──► valor do heap
                                  │
                                  ├── ambiente léxico/global
                                  ├── célula de valor/função
                                  ├── condição e controle não local
                                  └── imagem persistente
```

O leitor e o avaliador são a implementação de referência durante o bootstrap.
Símbolos possuem células separadas de valor e função; packages, vetores,
caracteres e tabelas hash também são objetos do heap e preservam identidade e
referências na imagem. Strings armazenam UTF-8 e são indexadas por ponto de
código na API Lisp.

Valores múltiplos vivem em estado explícito do runtime. Posições comuns de
argumento consomem o valor primário, enquanto formas de valores múltiplos e
transferências não locais preservam o conjunto completo. O mesmo analisador de
forma completa atende ao REPL da CLI e ao ouvinte gráfico, evitando divergência
no comportamento multilinha.

## Sessão da IDE

`sefirah_ide_nucleo` possui o buffer editável, entrada do ouvinte, transcrição,
inspetor, caminho atual e runtime. Ele executa, abre e grava sem uma janela, o
que torna seu comportamento testável na CI. `sefirah_ide` apenas organiza os
painéis, desenha o estado e converte eventos X11/Win32/Cocoa em ações da sessão.

## Fluxo do compilador

`COMPILE` baixa uma função compatível para uma IR SSA de inteiros de 64 bits. A
IR é verificada antes de chegar ao interpretador ou a um backend:

```text
DEFUN → frontend i64 → IR SSA → verificador de fluxo/dominância
                                      ├── interpretador de referência
                                      ├── x86-64 SysV/Microsoft
                                      └── AArch64 AAPCS64
                                                │
                               ┌────────────────┴───────────────┐
                               ▼                                ▼
                         memória JIT W^X             ELF / COFF / Mach-O
```

A definição Lisp portátil permanece no objeto de função. A imagem não grava o
cache nativo: depois de restaurada em outro processo ou arquitetura, a função
pode ser recompilada.

## Ownership e coleta

O heap atual usa mark-and-sweep nos limites de uma unidade de avaliação.
Valores entregues a código C podem ser preservados entre coletas por
`SefRaiz`. O handle é explícito e removível, evitando depender de varredura
conservadora da pilha hospedeira.

### Streams

Streams padrão encapsulam `stdin`, `stdout` e `stderr` sem possuir esses
recursos. Streams retornados por `OPEN` possuem seu `FILE` e o fecham em
`CLOSE`, na destruição do runtime ou na coleta.

### Bibliotecas compartilhadas

O objeto Lisp e as funções JIT compartilham um recurso nativo contado por
referências:

```text
objeto SHARED-LIBRARY ──┐
                        ├── recurso (.so/.dylib/.dll) ── handle nativo
função compilada A ─────┤
função compilada B ─────┘
```

`CLOSE-SHARED-LIBRARY` libera a referência do objeto e impede novas
vinculações. Uma função compilada existente continua válida porque mantém sua
própria referência. O último proprietário executa `dlclose` ou `FreeLibrary`.

## Imagem persistente

O formato binário v9 preserva o grafo de objetos, símbolos, packages, vetores,
caracteres, tabelas hash, ambientes, funções, macros, condições e streams
restauráveis. A gravação usa um arquivo temporário e substituição atômica. O
carregador reconhece v6, v7, v8 e v9; uma imagem antiga carregada e salva
novamente é emitida no formato corrente.

Recursos do processo seguem uma política explícita:

| Recurso | Política ao salvar |
| --- | --- |
| stream padrão | preservado e religado ao processo seguinte |
| stream de arquivo fechado | preservado como fechado |
| stream de arquivo aberto | gravação rejeitada |
| biblioteca compartilhada fechada | preservada como fechada |
| biblioteca compartilhada aberta | gravação rejeitada |
| cache JIT | descartado; recompilação sob demanda |

Essa política evita serializar descritores, ponteiros ou bytes de máquina sem
semântica válida no processo seguinte.

## GUI própria

A GUI não encapsula widgets do sistema. Componentes são organizados e
desenhados pelo Sefirah sobre `SefSuperficie`; o backend de plataforma apenas
apresenta os pixels e traduz eventos.

```text
SefComponente
    ├── layout em linha/coluna e pesos
    ├── tema e estados visuais
    ├── hit-testing e foco
    └── ação
          │
          ▼
SefSuperficie → rasterizador CPU → janela nativa
```

Esse desenho permite que aplicações comuns usem a mesma GUI da IDE. Fontes
vetoriais, clipping geral, HiDPI, IME e acessibilidade ainda pertencem aos
próximos marcos.

## Estratégia de evolução

O compilador self-hosted entrará depois que o bootstrap tiver representação de
valores Lisp, chamadas gerais, alocação, pontos seguros e metadados de exceção
na IR. A GUI crescerá por capacidades — texto, composição, acessibilidade e
integrações desktop — sem abandonar o rasterizador próprio.

Os critérios e pendências de cada fase ficam no
[roteiro para 1.0](roteiro.md).
