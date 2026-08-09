# Manual do bootstrap

Esta versao estabelece o primeiro caminho vertical do Sefirah. Ela nao deve
ser apresentada como ANSI Common Lisp: CLOS, restarts completos e numeric
tower ainda pertencem aos marcos seguintes; a compilacao nativa atual cobre
somente o subconjunto i64 documentado abaixo.

Arquivos de codigo-fonte usam exclusivamente a extensao `.lisp`. A extensao
antiga `.sef` nao faz parte do formato publico do projeto.

## Formas disponiveis

- Valores inteiros, reais, strings, simbolos, listas proprias e pares.
- `QUOTE`, `IF`, `PROGN`, `LAMBDA`, `FUNCTION`, `LET`, `LET*`, `COND`,
  `WHEN`, `UNLESS`, `AND` e `OR`.
- `DEFINE`, `DEFVAR`, `DEFPARAMETER`, `SETQ`, `DEFUN` e `DEFMACRO`.
- `FLET`, `LABELS` e `MACROLET` com celulas lexicais de funcao.
- Controle nao local por `BLOCK`, `RETURN-FROM`, `RETURN`, `CATCH` e `THROW`.
- `UNWIND-PROTECT` executa limpezas tambem durante transferencias nao locais.
- Objetos de condicao iniciais com `ERROR`, `HANDLER-CASE` e `IGNORE-ERRORS`.
- Parametros posicionais e `&REST`.
- Aritmetica `+`, `-`, `*`, `/` e comparacoes `<`, `>`, `<=`, `>=`, `=` e `/=`.
- `CONS`, `CAR`, `CDR`, `LIST`, `EQ`, `ATOM`, `NULL`, `NOT`, `LENGTH`,
  `PRINT`, `TYPE-OF`, `FUNCALL` e `APPLY`.
- `BOUNDP`, `FBOUNDP`, `SYMBOL-VALUE`, `SYMBOL-FUNCTION`, `SET` e `FUNCTIONP`.
- `COMPILE` e `COMPILED-FUNCTION-P` para o primeiro subconjunto nativo i64.
- Streams com `OPEN`, `CLOSE`, `STREAMP`, `READ-LINE`, `WRITE-STRING`, `TERPRI`
  e `FINISH-OUTPUT`. `PRINT` aceita um stream opcional.

Como exige Common Lisp, cada simbolo possui celulas separadas de valor e de
funcao. Assim, uma variavel e uma funcao podem compartilhar o mesmo nome;
`#'nome` consulta a celula de funcao.

## Compilacao nativa inicial

`COMPILE` baixa uma funcao Lisp para IR SSA e instala codigo x86-64 ou AArch64
na propria funcao, conforme o computador hospedeiro. O subconjunto atual aceita
parametros posicionais inteiros, constantes, `+`, `-`, `*`, `<`, `<=` e `IF`,
com exatamente uma forma no corpo.

```lisp
(defun calcular (x y)
  (if (< x y)
      (+ (* x 2) y)
      (- x y)))

(compile 'calcular)
(calcular 10 22) ; => 42
(compiled-function-p #'calcular) ; => T
```

Chamadas comuns, `FUNCALL` e `APPLY` observam automaticamente o codigo
instalado. Enquanto a representacao geral de valores Lisp e pontos seguros nao
entrarem na IR, essas funcoes exigem argumentos i64 e retornam i64. A imagem
preserva a definicao Lisp portavel e descarta o cache de maquina; `COMPILE` pode
recria-lo depois de abrir a imagem em outra plataforma.

Uma funcao do subconjunto i64 também pode virar um objeto relocavel para os
tres formatos desktop:

```sh
sefirah compilar-elf exemplos/nativo.lisp calcular_nativo calcular.o
cc exemplos/chamar_nativo.c calcular.o -o chamar-nativo
./chamar-nativo

sefirah compilar-coff exemplos/nativo.lisp calcular_nativo calcular.obj
x86_64-w64-mingw32-gcc exemplos/chamar_nativo.c calcular.obj -o chamar-nativo.exe

sefirah compilar-macho exemplos/nativo.lisp calcular_nativo calcular-macos.o
```

O simbolo exportado segue o contrato C `int64_t funcao(const int64_t *args)`.
ELF64 inclui `.text`, `.symtab`, `.strtab` e `.shstrtab`; COFF inclui `.text`,
tabela de simbolos e string table; Mach-O inclui `__TEXT,__text`, `LC_SYMTAB` e
o prefixo de simbolo `_` exigido pela ABI Darwin. Todos os gravadores escolhem
x86-64 ou ARM64 a partir do backend, escrevem little-endian e instalam o arquivo
atomicamente. Quando necessario, o frontend preserva a IR e reemite x86-64 com
a ABI System V para ELF/Mach-O ou Microsoft x64 para COFF.

O SDK do compilador também oferece `SEF_IR_CHAMAR_EXTERNA_I64`. A instrucao
chama uma funcao C unaria `int64_t funcao(int64_t)`, e os gravadores produzem
`R_X86_64_PLT32`/`R_AARCH64_CALL26`, `IMAGE_REL_*_BRANCH` ou a relocacao de
branch Mach-O correspondente. Os exemplos `gerar_objeto_externo.c` e
`chamar_externa.c` mostram o fluxo completo para os tres formatos.

O carregador JIT ainda rejeita objetos com simbolos externos nao resolvidos;
eles devem passar pelo linker da plataforma. O frontend Lisp inicial também
ainda nao possui uma forma publica para declarar imports C, portanto esta
capacidade pertence por enquanto ao SDK C17 do compilador.

## Packages

O runtime inicia em `COMMON-LISP-USER`, que usa `COMMON-LISP`; símbolos `:nome`
pertencem a `KEYWORD` e se autoavaliam. Já estão disponíveis `DEFPACKAGE` com
`:USE`/`:EXPORT`, `IN-PACKAGE`, `MAKE-PACKAGE`, `FIND-PACKAGE`, `PACKAGE-NAME`,
`PACKAGEP`, `USE-PACKAGE`, `EXPORT`, `INTERN`, `FIND-SYMBOL`, `SYMBOL-NAME`,
`SYMBOL-PACKAGE` e `LIST-ALL-PACKAGES`. Packages, relações de uso e identidade
dos símbolos também fazem parte da imagem persistente.

## Streams e arquivos

`*STANDARD-INPUT*`, `*STANDARD-OUTPUT*` e `*ERROR-OUTPUT*` sao streams ligados
ao processo hospedeiro. `OPEN` abre arquivos em modo binario para que o mesmo
programa tenha comportamento previsivel nos tres sistemas suportados.

```lisp
(let ((saida (open "resultado.txt"
                   :direction :output
                   :if-exists :supersede)))
  (write-string "Sefirah" saida)
  (terpri saida)
  (finish-output saida)
  (close saida))

(let ((entrada (open "resultado.txt")))
  (print (read-line entrada))
  (close entrada))
```

`:DIRECTION` aceita `:INPUT`, `:OUTPUT` e `:IO`. Para saida, `:IF-EXISTS`
aceita `:SUPERSEDE`, `:APPEND` ou `:ERROR`. Na implementacao atual `READ-LINE`
devolve `NIL` ao chegar ao fim do arquivo, pois valores multiplos ainda nao
foram introduzidos.

O leitor converte nomes ASCII para maiusculas, reconhece comentarios iniciados
por `;`, listas pontuadas, quote, function quote, quasiquote com `,`/`,@` e
escapes simples em strings.

## Limites deliberados

O coletor atual e mark-and-sweep e roda somente ao terminar uma unidade de
avaliacao. Isso evita esconder raizes na pilha C antes da introducao dos mapas
de pilha do compilador. Divisao sempre produz `FLOAT`; racionais e bignums
ainda nao existem. O REPL textual aceita uma linha por interacao.

`sefirah ide` abre uma composicao inicial rasterizada pelo proprio projeto. O
ouvinte aceita expressoes e mostra seu resultado em X11 e Win32. A composicao
usa a mesma arvore de componentes e o mesmo layout flexivel da API publica em
`sefirah/gui.h`. O editor e o inspetor ainda sao demonstrativos; a ponte atual
do macOS ainda nao encaminha teclado ao ouvinte.

## GUI propria

A GUI nao encapsula widgets nativos: todos os componentes sao organizados e
desenhados pelo Sefirah sobre `SefSuperficie`. A API C17 oferece `SefComponente`
para paineis, rotulos, botoes e campos, layouts em linha/coluna com pesos,
`SefTemaGui`, hit-testing, navegacao de foco e despacho de acoes. O estado de
interacao `SefInteracaoGui` converte eventos de Tab, Enter e ponteiro em foco e
acionamento. X11 e Win32 ja entregam teclado e ponteiro; a ponte interativa do
macOS permanece no roteiro.

## Imagens persistentes

O formato binario v6 `.imagem` preserva o grafo do heap, simbolos, packages,
ambientes, funcoes, macros e streams fechados/padrao. A gravacao usa um arquivo
temporario e substituicao atomica. Primitivas C sao restauradas pelo nome, nunca
por enderecos de memoria. Um stream de arquivo aberto deve ser fechado antes de
salvar, evitando persistir um descritor externo sem semantica de restauracao.

```sh
sefirah imagem salvar desenvolvimento.imagem exemplos/inicio.lisp
sefirah imagem abrir desenvolvimento.imagem "(fatorial 6)"
sefirah imagem abrir desenvolvimento.imagem
```
