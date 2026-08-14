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
é instalado nem oferece estabilidade de SDK ou ABI; aplicações devem usar
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

Restarts dinâmicos reutilizam a pilha de controle não local. Cada registro
ativo aponta para um objeto `RESTART` de primeira classe no heap e mantém os
dados executáveis da cláusula somente durante seu escopo dinâmico. Cada quadro
registra o limite da pilha de restarts ativo quando foi instalado. As
transferências descartam registros internos antes do salto, enquanto quadros
de limpeza executam primeiro; os dados da cláusula escolhida permanecem
enraizados no estado de transferência até seu ambiente léxico ser reconstruído.
Um objeto restart pode sobreviver para inspeção ou persistência em imagem sem
reter um destino `setjmp` morto depois de `RETURN-FROM`, `THROW` ou retorno
normal.

Handlers dinâmicos usam uma pilha paralela no runtime. Os bindings de um mesmo
`HANDLER-BIND` compartilham um limite externo; selecionar um handler mascara o
grupo completo enquanto ele executa. Um handler que retorna recusa o tratamento
e a busca prossegue nos grupos externos; um handler que invoca um restart segue
as mesmas regras de limpeza e limites das demais transferências não locais.

## Sessão da IDE

`sefirah_ide_nucleo` possui os documentos do editor, entrada do ouvinte, transcrição,
inspetor, navegador de definições, histórico de condições, histórico de
instalação incremental, caminho ativo e runtime. Cada documento em segundo
plano mantém texto, caminho, caminho da imagem, cursor, seleção, indicador de
alteração, histórico do editor e assinaturas incrementais. A movimentação
desses valores entre o slot do documento e o editor ativo torna a troca de abas
sem perda e sem duplicação de buffers.
`espaco_trabalho.c` constrói o índice limitado e ordenado do projeto por enumeração
nativa de diretórios, evita recursão por links simbólicos/reparse points e
separa caminhos absolutos daqueles relativos exibidos pelo Explorer.
Os adaptadores de janela convertem Ctrl+P, Ctrl+Shift+P e Escape em eventos
portáveis. A apresentação mantém a sobreposição filtrada de Quick Open/paleta
de comandos; descoberta, criação exclusiva de arquivo/pasta, atualização e
abertura permanecem na camada de sessão testável. A sessão
executa, abre, grava, captura e restaura sem uma janela, o
que torna seu comportamento testável na CI. `sefirah_ide` apenas organiza os
painéis, desenha o estado e converte eventos X11/Win32/Cocoa em ações da sessão.

Dentro do motor de sessão, `historico.c` mantém a linha do tempo limitada do
editor e o histórico de eventos do ouvinte, enquanto `estrutura.c` localiza
formas Lisp completas de nível superior, calcula suas assinaturas para avaliação
incremental e cataloga definições nomeadas sem avaliar o fonte. Sua passagem
léxica também resolve o átomo no cursor e cataloga referências, excluindo
comentários, strings, literais de caractere e ocorrências que nomeiam a própria
definição. A sessão possui um intervalo normalizado de seleção; sua extensão
pelo cursor respeita limites de pontos de código UTF-8, e a seleção estrutural
reutiliza o analisador de formas completas. Uma substituição de intervalo é
registrada como um único estado do histórico do editor. O inspetor retém os
objetos devolvidos e cada passo da navegação
recursiva por meio de raízes públicas do GC. A API pública de introspecção de
componentes apresenta arestas rotuladas dos objetos compostos sem expor à IDE a
union privada dos objetos. A restauração abre a imagem substituta antes de
liberar essas raízes e o runtime antigo; assim, um snapshot ausente ou danificado
não destrói o mundo ativo. O código de apresentação enxerga somente o estado
formatado da sessão e nunca acessa os detalhes internos do runtime.

O runtime mantém a última condição não tratada enraizada até a próxima
avaliação e a expõe como valor emprestado do SDK. A sessão converte esse valor
em seu próprio conjunto limitado de raízes públicas, impedindo que coletas
posteriores invalidem o histórico do depurador. Restaurar outro mundo libera
todas as raízes de condições e inspeção antes de destruir o runtime anterior.
Quadros de restart não são retidos depois do desenrolamento; um depurador
interativo futuro deverá suspender a avaliação, em vez de armazenar destinos
`setjmp` mortos.

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
conservadora da pilha hospedeira. `sef_valor_quantidade_componentes` e
`sef_valor_componente` expõem uma visão rotulada e somente de leitura das
arestas do grafo para ferramentas residentes, preservando esse modelo de
ownership.

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

O formato binário v10 preserva o grafo de objetos, símbolos, packages, vetores,
caracteres, tabelas hash, ambientes, funções, macros, condições, objetos
restart e streams restauráveis. A gravação usa um arquivo temporário e
substituição atômica. O carregador reconhece de v6 a v10; uma imagem antiga
carregada e salva novamente é emitida no formato corrente. Depois de validar o
grafo, uma migração direcionada restaura a associação canônica de
`COMMON-LISP:NIL`,
remove conflitos locais legados de `NIL` nos packages que usam `COMMON-LISP` e
reinstala por nome os membros ausentes do conjunto atual de primitivas junto
aos símbolos exportados de formas especiais. Definições Lisp de função já
existentes são preservadas. Assim, um mundo antigo recebe novos built-ins sem
serializar nem confiar em endereços C obsoletos. As property lists dos símbolos
vivem em uma tabela hash interna do heap, enraizada pelo ambiente global;
portanto, a codificação existente do grafo v10 as preserva, enquanto imagens
antigas começam naturalmente com uma tabela vazia no primeiro uso.
Símbolos não internados referenciam uma sentinela de package privada da
implementação, ausente do registro público de packages. Isso mantém válido o
grafo de referências v10 enquanto `SYMBOL-PACKAGE`, impressão e inspeção
expõem a semântica não internada exigida.
As tabelas de símbolos de packages também podem conter identidades importadas
cujo package de origem é diferente. Tabelas de exportação referenciam os mesmos
objetos, e uninterning atualiza ambas antes de eventualmente devolver um
símbolo de origem à sentinela não internada privada. Assim, os vetores comuns
de packages do v10 preservam importações sem um registro paralelo.
Escolhas de shadowing usam outra tabela hash interna do heap, enraizada pelo
ambiente global. A validação de uso de package a consulta antes de aceitar ou
rejeitar um conflito de exportação, e uninterning se recusa a expor identidades
herdadas ambíguas. A mesma persistência do grafo v10 vale para esse registro.
Apelidos de packages usam o mesmo padrão de metadados enraizados no grafo. A
consulta tenta primeiro o nome canônico e depois listas validadas de apelidos;
ambos os caminhos usam a comparação case-insensitive ASCII do sistema de
packages. A consulta pública devolve strings copiadas, impedindo a mutação do
registro por seus chamadores.
`DEFPACKAGE` valida todas as opções antes de aplicá-las e então executa as declarações
em fases determinísticas: shadow e shadowing import, listas de uso, importação
comum e internamento, seguidos de exportação. Assim, um conflito tem o mesmo
resultado independentemente da ordem textual da opção que o resolve.

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

Esse desenho permite que aplicações comuns usem a mesma GUI da IDE. A fonte
bitmap 5x7 possui glifos ASCII separados para maiúsculas e minúsculas, portanto
caminhos e código preservam visualmente sua caixa. Fontes vetoriais, clipping
geral, HiDPI, IME e acessibilidade ainda pertencem aos próximos marcos.

## Estratégia de evolução

O compilador self-hosted entrará depois que o bootstrap tiver representação de
valores Lisp, chamadas gerais, alocação, pontos seguros e metadados de exceção
na IR. A GUI crescerá por capacidades — texto, composição, acessibilidade e
integrações desktop — sem abandonar o rasterizador próprio.

Os critérios e pendências de cada fase ficam no
[roteiro para 1.0](roteiro.md).
