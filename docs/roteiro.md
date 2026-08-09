# Roteiro de implementacao

O plano de 1.0 e intencionalmente maior que o marco de bootstrap. Este arquivo
impede que uma demonstracao visual seja confundida com a plataforma terminada.

## Marco 0 — fundacao executavel

- [x] Build C17 modular e testes em CMake.
- [x] Objetos, ambientes lexicais, leitor, impressor e avaliador.
- [x] Funcoes, macros, recursao e primitivas essenciais.
- [x] Coletor mark-and-sweep nos limites de avaliacao.
- [x] Rasterizador CPU, fonte bitmap e composicao visual propria.
- [x] Janela X11 e Win32; ponte macOS escrita em C puro.
- [x] Ouvinte grafico ligado ao runtime em X11 e Win32.
- [x] CI declarada para Linux, Windows e macOS.
- [x] Primeira imagem versionada com gravacao atomica e restauracao do heap.
- [x] Handles de raiz explicitos para integracao segura com C/IDE/FFI.
- [x] Quasiquote, unquote e splice para macros legiveis.
- [x] Packages no heap, símbolos qualificados, `KEYWORD`, herança por `:USE` e imagens.
- [x] Streams padrão e de arquivo, leitura/escrita textual e política segura de imagem.

## Marco 1 — runtime compilavel

- [ ] Raizes explicitas, pontos seguros e GC geracional preciso.
- [ ] Condicoes e restarts completos, streams compostos e numeric tower.
- [x] Namespaces distintos de valor/funcao e controle nao local com limpeza.
- [x] Primeiros objetos de condicao e tratamento recuperavel por `HANDLER-CASE`.
- [x] Primeira IR SSA tipada, verificador de dominancia e interpretador de referencia.
- [x] Primeiro emissor x86-64 SysV/Windows e carregamento W^X validado nas duas ABIs.
- [x] Primeiro frontend `DEFUN` -> SSA -> x86-64, integrado a `COMPILE` e chamadas Lisp.
- [x] Primeiro emissor AArch64 AAPCS64 com selecao nativa no frontend Lisp.
- [x] Primeiro gravador ELF64 relocavel x86-64/AArch64 e comando `compilar-elf`.
- [x] Primeiro gravador COFF AMD64/ARM64 e comando `compilar-coff`.
- [x] Primeiro gravador Mach-O x86-64/ARM64 e comando `compilar-macho`.
- [x] Primeiras relocacoes ELF/COFF/Mach-O e chamada externa unaria na IR.
- [ ] Compilador bootstrap escrito em Sefirah e self-hosting reproduzivel.

## Marco 2 — Common Lisp e imagem viva

- [ ] Cobertura completa ANSI Common Lisp com relatorio de conformidade.
- [ ] CLOS, condicoes/restarts completos, threads e FFI C.
- [ ] Migracoes de imagem e recuperacao de recursos externos nao serializaveis.
- [ ] ASDF, Quicklisp e gerenciador Sefirah com lockfile.

## Marco 3 — GUI desktop

- [ ] Wayland, entrada Unicode/IME e ponte de teclado macOS.
- [ ] Paths, clipping, alpha, imagens, fontes vetoriais e HiDPI.
- [ ] Layout, componentes, temas, foco, comandos e acessibilidade (base de
  componentes, layout, tema, hit-testing, foco, ações e integração da IDE
  concluída).
- [ ] Clipboard, drag-and-drop, dialogs, impressao e notificacoes.
- [ ] Rede/TLS, processos, SQLite e audio basico.

## Marco 4 — ambiente e distribuicao 1.0

- [ ] Editor textual/estrutural, inspetor, debugger, profiler e DWIM reversivel.
- [ ] Historico transacional, Git, autosave e restauracao de sessao.
- [ ] IDE recompila o compilador e a si propria.
- [ ] Pacotes MSIX, DMG, AppImage, DEB e RPM com hooks de assinatura.
- [ ] Matriz oficial Windows x64, macOS Intel/ARM e Linux x64/ARM com X11/Wayland.
