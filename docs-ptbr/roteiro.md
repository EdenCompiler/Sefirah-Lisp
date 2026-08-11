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

### Compilador

- [x] IR SSA tipada, verificador de dominância e interpretador de referência.
- [x] Emissor x86-64 System V/Microsoft e carregamento W^X nas duas ABIs.
- [x] Frontend `DEFUN` → SSA → código nativo integrado a `COMPILE`.
- [x] Emissor AArch64 AAPCS64 com seleção nativa no frontend.
- [x] Gravador ELF64 relocável x86-64/AArch64 e comando `compilar-elf`.
- [x] Gravador COFF AMD64/ARM64 e comando `compilar-coff`.
- [x] Gravador Mach-O x86-64/ARM64 e comando `compilar-macho`.
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
- [x] Leitura retrocompatível de imagens v6/v7/v8 e emissão do formato v9.
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
- [ ] Seleção e edição estrutural.
- [ ] Inspetor geral de objetos e navegador de código.
- [ ] Debugger, profiler e navegação por condições/restarts.
- [ ] DWIM reversível e histórico transacional seletivo/fora de ordem.
- [ ] Integração Git, autosave e restauração completa da sessão de desktop.
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
