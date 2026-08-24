# Roteiro do Sefirah Lisp 1.0

[English](../docs-en/roadmap.md) · **Português do Brasil**

O 1.0 é uma plataforma Lisp desktop completa, não apenas uma demonstração do
runtime ou da GUI. Este documento separa trabalho concluído de intenção e
define as evidências necessárias para cada marco.

## Estado dos marcos

| Marco | Objetivo | Estado |
| --- | --- | --- |
| 0 | fundação executável | base funcional; itens listados concluídos |
| 1 | runtime compilável | em andamento |
| 2 | Common Lisp hospedado e imagem viva | pendente |
| 3 | GUI desktop | base inicial; capacidades desktop pendentes |
| 4 | ambiente e distribuição 1.0 | pendente |

Uma caixa marcada significa que existe implementação e teste correspondente;
não significa que o marco inteiro esteja concluído.

## Marco 0 — fundação executável

- [x] Build C17 modular e testes em CMake.
- [x] Objetos, ambientes lexicais, leitor, impressor e avaliador.
- [x] Funções, macros, recursão e primitivas essenciais.
- [x] Coletor mark-and-sweep nos limites de avaliação.
- [x] Rasterizador CPU, fonte bitmap e composição visual própria.
- [x] Janelas raster X11, Win32 e Cocoa com pontes de eventos nativas.
- [x] Ouvinte gráfico ligado ao runtime em Linux, Windows e macOS.
- [x] REPL textual multilinha com detecção de forma completa e impressão de
  valores múltiplos.
- [x] Matriz CI verde de build e testes para Linux, Windows e macOS.
- [x] Imagem versionada com gravação atômica e restauração do heap.
- [x] Handles de raiz explícitos para integração segura com C, IDE e FFI.
- [x] Auditoria de instalação na CI para binários, headers públicos e documentação.
- [x] Quasiquote, unquote e splice para macros legíveis.
- [x] Packages no heap, símbolos qualificados, `KEYWORD`, herança por `:USE` e
  persistência em imagem.
- [x] Streams padrão e de arquivo, leitura/escrita textual e política segura de
  imagem.
- [x] Package interno `SEFIRAH` para tipos e operações da implementação.

## Marco 1 — runtime compilável

### Runtime e linguagem

- [ ] Raízes explícitas em todos os limites nativos, pontos seguros e GC
  geracional preciso.
- [ ] Condições e restarts completos.
- [ ] Streams compostos e protocolo completo de streams.
- [ ] Numeric tower de Common Lisp.
- [x] Namespaces distintos de valor/função e controle não local com limpeza.
- [x] Objetos iniciais de condição e tratamento recuperável por
  `HANDLER-CASE`.
- [x] Protocolo dinâmico de `HANDLER-BIND`/`SIGNAL` integrado aos restarts ativos.
- [x] Protocolo inicial de restarts nomeados com descoberta dinâmica,
  invocação, valores múltiplos e limpeza por `UNWIND-PROTECT`.
- [x] Objetos restart de primeira classe com identidade, descoberta anônima,
  inspeção, segurança quando inativos, GC e persistência em imagem.
- [x] Auxiliares nomeados `ABORT`, `CONTINUE`, `MUFFLE-WARNING`, `STORE-VALUE`
  e `USE-VALUE`.
- [x] Limite síncrono para depurador hospedeiro com descoberta de restarts
  vivos, transferência de argumentos enraizada, invocação protegida e semântica
  explícita de recusa.
- [x] Células globais mutáveis de valor/função com `FDEFINITION`, `MAKUNBOUND`,
  `FMAKUNBOUND` e acesso generalizado por `SETF`.
- [x] Property lists persistentes de símbolos com `SYMBOL-PLIST`, `GET`,
  `REMPROP`, suporte a `NIL` e acesso generalizado por `SETF`.
- [x] Identidades não internadas por `MAKE-SYMBOL`/`COPY-SYMBOL` e `GENSYM`
  determinístico com `*GENSYM-COUNTER*` persistente.
- [x] `IMPORT` e `UNINTERN` de packages, incluindo adoção de símbolo não
  internado, exportação importada, transições de estado e persistência.
- [x] `SHADOW`, `SHADOWING-IMPORT`, consulta de símbolos de shadowing e
  proteção contra conflitos herdados, todos persistentes.
- [x] `DEFPACKAGE` em fases com `:SHADOW`, `:SHADOWING-IMPORT-FROM`, `:USE`,
  `:IMPORT-FROM`, `:INTERN` e `:EXPORT`.
- [x] `USE-PACKAGE`/`UNUSE-PACKAGE` e `EXPORT`/`UNEXPORT` reversíveis, com
  consultas novas das relações de uso e de packages usuários.
- [x] Apelidos de packages persistentes e únicos por `DEFPACKAGE`,
  `MAKE-PACKAGE`, `FIND-PACKAGE` e `PACKAGE-NICKNAMES`.
- [x] `RENAME-PACKAGE` seguro contra conflitos, com troca atômica de apelidos e
  persistência na imagem do mundo.
- [x] `DELETE-PACKAGE` protegido, com checagem de relações, limpeza de
  metadados, identidade dos símbolos sobreviventes e persistência.
- [x] Descoberta de packages somente para leitura por `DO-SYMBOLS`,
  `DO-EXTERNAL-SYMBOLS` e `DO-ALL-SYMBOLS`, com blocos `NIL` implícitos e
  reconciliação da imagem.
- [x] Consulta global de nomes de packages registrados por `FIND-ALL-SYMBOLS`,
  com `NIL`, filtragem de não internados/removidos e reconciliação de imagem.
- [x] `GENTEMP` persistente baseado em contador, com salto de colisões por
  package.

### Compilador

- [x] IR SSA tipada, verificador de dominância e interpretador de referência.
- [x] Emissor x86-64 System V/Microsoft e carregamento W^X nas duas ABIs.
- [x] Frontend `DEFUN` → SSA → código nativo integrado a `COMPILE`.
- [x] Emissor AArch64 AAPCS64 com seleção nativa no frontend.
- [x] Gravador ELF64 relocável x86-64/AArch64 e comando `compile-elf`.
- [x] Gravador COFF AMD64/ARM64 e comando `compile-coff`.
- [x] Gravador Mach-O x86-64/ARM64 e comando `compile-macho`.
- [x] Relocações desktop e chamadas externas i64 de uma ou duas entradas.
- [x] Forma Lisp `EXTERNAL-I64` para imports em objetos nativos.
- [x] Trampolins JIT x86-64/AArch64 com vinculação explícita e W^X.
- [ ] Representação nativa de valores Lisp etiquetados.
- [ ] Chamadas Lisp gerais, alocação, pontos seguros e metadados de exceção.
- [ ] Alocação de registradores e otimizações além do bootstrap.
- [ ] Compilador bootstrap escrito em Sefirah e self-hosting reproduzível.

### FFI

- [x] Carregamento `.so`, `.dylib` e `.dll` por `COMPILE-EXTERNAL-I64`.
- [x] Objetos de biblioteca com fechamento explícito e referências seguras.
- [x] Política de imagem para bibliotecas compartilhadas.
- [ ] Descritores de tipos estrangeiros.
- [ ] Assinaturas gerais com inteiros, floats, ponteiros, strings, structs e
  retorno `void`.
- [ ] Callbacks C para Lisp com ownership e tratamento de erros definidos.

## Marco 2 — Common Lisp hospedado e imagem viva

- [x] Vetores simples no heap, sintaxe `#(...)`, acesso, mutação, GC e
  persistência.
- [x] Caracteres Unicode, sintaxe `#\`, strings UTF-8 e protocolo inicial de
  sequências com `ELT`.
- [x] Algoritmos iniciais de sequência para listas, vetores e strings:
  `COPY-SEQ`, `REVERSE`, `SUBSEQ` e `FILL`.
- [x] Protocolo inicial de listas com composição, navegação, busca e
  `MAPCAR`/`MAPC` sobre múltiplas listas.
- [x] Tabelas hash `EQL` com `GETHASH`, `SETF`, remoção, GC e persistência.
- [x] Valores múltiplos iniciais de Common Lisp com binding, coleta, chamadas,
  propagação não local, presença de `GETHASH`, estado de package e indicador de
  quebra de linha de `READ-LINE`.
- [x] Leitura retrocompatível de imagens de v6 a v9 e emissão do formato v10.
- [x] Migração direcionada de imagem para `NIL` canônico e primitivas novas.
- [x] Relatório inicial explícito de conformidade ANSI e desvios conhecidos.
- [ ] Cobertura completa dos símbolos e da semântica ANSI Common Lisp.
- [ ] CLOS e MOP documentada.
- [ ] Condições e restarts completos integrados ao compilador.
- [ ] Threads, sincronização e interação segura com GC/FFI.
- [ ] Infraestrutura geral de migrações para versões futuras da imagem.
- [ ] Recuperação declarativa de recursos externos não serializáveis.
- [ ] ASDF, Quicklisp e gerenciador Sefirah com lockfile.
- [ ] Compilador e bibliotecas centrais carregáveis a partir da imagem.

## Marco 3 — GUI desktop

### Renderização e texto

- [x] Superfície RGB, rasterizador CPU e fonte bitmap inicial.
- [x] Árvore de componentes, layout, temas, hit-testing, foco e ações.
- [ ] Paths, clipping geral, alpha e composição.
- [ ] Imagens, fontes vetoriais, shaping e fallback tipográfico.
- [ ] HiDPI e escala fracionária.

### Plataforma e acessibilidade

- [x] Janela e eventos iniciais em X11 e Win32.
- [x] Ponte Cocoa/CoreGraphics escrita em C com teclado, atalhos, ponteiro e
  propagação de redesenho.
- [ ] Wayland.
- [ ] Composição IME completa e protocolos avançados de entrada textual.
- [ ] Árvore semântica e integração com tecnologias assistivas.
- [ ] Clipboard, drag-and-drop, diálogos, impressão e notificações.

### Serviços de aplicações

- [ ] Rede e TLS.
- [ ] Processos e comunicação entre processos.
- [ ] SQLite.
- [ ] Áudio básico.

## Marco 4 — ambiente e distribuição 1.0

### Ambiente Lisp

- [x] Buffer `.lisp` editável inicial com avaliação integral e abertura/gravação.
- [x] Ouvinte multilinha, transcrição persistente e inspetor inicial de resultados.
- [x] Movimento e inserção por cursor com consciência de UTF-8 no editor textual.
- [x] Avaliação da forma de nível superior no cursor e eventos navegáveis do ouvinte.
- [x] Desfazer/refazer linear limitado e inspetor de valores múltiplos enraizado no GC.
- [x] Navegador de definições nomeadas e avaliação incremental das formas de topo alteradas.
- [x] Definição do símbolo no cursor e navegação estrutural de callers/referências.
- [x] Gravação e restauração do mundo Lisp pela IDE.
- [x] Inspetor recursivo e enraizado no GC para grafos de objetos compostos.
- [x] Histórico limitado e enraizado no GC de condições não tratadas ligado ao inspetor.
- [x] Seleção de intervalo UTF-8 e seleção estrutural de forma de topo com
  substituição/remoção atômica no histórico de undo.
- [x] Shell de estação Lisp com paleta histórica quente, abas persistentes,
  histórico de undo independente, cursor/seleção e indicadores de alterações.
- [x] Explorer recursivo do workspace com índice determinístico de fontes Lisp e
  abertura direta por teclado/ponteiro nas abas do editor.
- [x] Quick Open, paleta pesquisável e ações funcionais de arquivo/pasta na barra,
  com atualização e feedback de erros dentro do workbench.
- [x] Prompts de caminho que preservam caixa, com glifos bitmap visualmente
  distintos para letras maiúsculas e minúsculas.
- [x] Find no editor ativo com Ctrl+F, acesso pela barra/paleta, consulta iniciada
  pela seleção, seleção UTF-8 segura anterior/próxima, contagem e retorno circular.
- [x] Go to Line com Ctrl+G, acesso pela barra/paleta, feedback validado em inglês
  e indicador persistente de linha/coluna UTF-8 iniciado em um.
- [x] Gutter adaptável com números de linha, destaque da linha ativa e rolagem
  centrada no cursor na paleta quente da estação Lisp.
- [x] Posicionamento do cursor por ponteiro seguro para UTF-8 e sincronizado com
  gutter visível, rolagem centrada e limite de final de linha.
- [x] Ctrl+A multiplataforma e Select All pela paleta usando o modelo de seleção
  UTF-8 segura e substituível atomicamente do editor.
- [x] Seleção por arraste do ponteiro pelas linhas visíveis com âncora estável,
  mapeamento UTF-8 de linha/coluna e intervalo atômico normalizado.
- [x] Delete adiante multiplataforma com limites por ponto de código UTF-8,
  remoção atômica da seleção, undo, indicador de alteração e Auto Save.
- [x] Navegação multiplataforma Ctrl/Command por palavra e seleção com Shift,
  usando limites Lisp de delimitador, espaços e símbolos UTF-8.
- [x] Navegação multiplataforma Ctrl/Command+Home/End pelo documento inteiro e
  seleção com Shift por âncora estável acompanhada pelo viewport.
- [x] Ciclo de criação/fechamento de abas com Ctrl+N/Ctrl+W, acesso pela
  barra/paleta, confirmação explícita para descartar alterações, seleção
  determinística da aba vizinha e invariante permanente de um editor.
- [x] Seletor multifonte de símbolos do workspace com índice estrutural de
  definições, buffers vivos não gravados, navegação exata, botão e Ctrl+T.
- [x] Metadados de binding de valor/função ligados ao runtime e somente de
  leitura nos resultados de símbolos do workspace.
- [x] Navegação estrutural de referências entre arquivos com buffers vivos não
  gravados, abertura exata, F12/Shift+F12 e ação na barra.
- [x] Localização F11 da definição do símbolo no cursor entre arquivos do
  workspace, com fallback para o buffer local.
- [x] Snapshots históricos enraizados no GC dos objetos restart disponíveis em
  um `ERROR` não tratado, retidos com o histórico do depurador e navegáveis no
  inspetor.
- [x] Profiler limitado de avaliações por tempo decorrido, com origem,
  sucesso/erro, total/média, aba/botão Profile e limpeza pela paleta de comandos.
- [ ] Debugger suspensível e invocação/navegação interativa por restarts.
- [ ] DWIM reversível e histórico transacional seletivo/fora de ordem.
- [x] Alternância de Auto Save por sessão com botão, paleta de comandos, barra
  de estado, persistência de buffers nomeados e caminhos exatos com caixa mista.
- [x] Painel somente de leitura Git Source Control com status de branch, árvore
  e índice, execução direta sem shell, atualização por botão/comando, saída
  limitada e caminhos exatos com caixa mista.
- [x] Dock de ferramentas em abas ao estilo VS Code que entrega toda a área a
  Inspector, Browser, Debugger, Source Control ou Profile e acompanha
  avaliação/navegação.
- [ ] Fluxos mutáveis de Git e restauração completa da sessão de desktop.
- [ ] IDE recompila o compilador e a si própria.

### Distribuição

- [ ] Pacote MSIX para Windows.
- [ ] Aplicativo DMG para macOS Intel e Apple Silicon.
- [ ] AppImage, DEB e RPM para Linux.
- [ ] Hooks de assinatura, metadados, ícones e atualização.
- [ ] Matriz oficial Windows x64, macOS Intel/ARM e Linux x64/ARM com
  X11/Wayland.

## Critérios de entrega do 1.0

O objetivo só estará concluído quando houver evidência para todos estes itens:

1. suíte de conformidade documentando a superfície Common Lisp suportada;
2. bootstrap self-hosted reproduzível em x86-64 e AArch64;
3. imagem capaz de restaurar uma sessão de desenvolvimento real;
4. aplicação desktop Sefirah funcional em Windows, Linux e macOS;
5. IDE com edição, avaliação, inspeção e depuração integradas;
6. pacotes instaláveis e testes de instalação nas plataformas oficiais;
7. documentação de usuário, arquitetura, SDK e migração correspondente à
   versão entregue.

Enquanto qualquer evidência estiver ausente, a versão permanece pré-1.0.
