# Guia do bootstrap Sefirah Lisp

[English](../docs-en/manual.md) · **Português do Brasil**

O bootstrap atual estabelece o primeiro caminho vertical da linguagem ao
desktop. Ele ainda não é ANSI Common Lisp completo: este guia descreve apenas
o comportamento implementado e testado no repositório.

Programas usam exclusivamente a extensão `.lisp`. A extensão antiga `.sef` não
faz parte do formato público.

## Início rápido

```bash
./construir/sefirah avaliar "(+ 20 22)"
./construir/sefirah executar exemplos/inicio.lisp
./construir/sefirah repl
```

```lisp
(defun fatorial (n)
  (if (< n 2)
      1
      (* n (fatorial (- n 1)))))

(fatorial 6) ; => 720
```

## Superfície da linguagem

| Área | Formas e funções principais |
| --- | --- |
| valores | inteiros, reais, caracteres Unicode, strings, símbolos, listas, pares, vetores e tabelas hash |
| avaliação | `QUOTE`, `IF`, `PROGN`, `LAMBDA`, `FUNCTION` |
| bindings | `DEFINE`, `DEFVAR`, `DEFPARAMETER`, `SETQ`, `SETF`, `LET`, `LET*` |
| funções e macros | `DEFUN`, `DEFMACRO`, `FLET`, `LABELS`, `MACROLET`, `&REST` |
| composição | `COND`, `WHEN`, `UNLESS`, `AND`, `OR`, quasiquote, `,` e `,@` |
| controle | `BLOCK`, `RETURN-FROM`, `RETURN`, `CATCH`, `THROW`, `UNWIND-PROTECT` |
| condições | `ERROR`, `HANDLER-CASE`, `IGNORE-ERRORS` |
| valores múltiplos | `VALUES`, `VALUES-LIST`, `MULTIPLE-VALUE-BIND`, `MULTIPLE-VALUE-LIST`, `MULTIPLE-VALUE-CALL`, `MULTIPLE-VALUE-PROG1`, `NTH-VALUE` |
| listas | `CONS`, `CAR`, `CDR`, `FIRST`, `REST`, `LIST`, `APPEND`, `NCONC`, `NTH`, `NTHCDR`, `LAST` |
| vetores | `VECTOR`, `MAKE-ARRAY`, `AREF`, `SVREF`, `VECTORP`, `ARRAYP` |
| tabelas hash | `MAKE-HASH-TABLE`, `GETHASH`, `HASH-TABLE-P`, `HASH-TABLE-COUNT`, `REMHASH`, `CLRHASH` |
| caracteres | `CHARACTERP`, `CHAR-CODE`, `CODE-CHAR` e comparadores `CHAR...` |
| sequências | `LENGTH`, `ELT`, `CHAR`, `SCHAR`, `COPY-SEQ`, `REVERSE`, `SUBSEQ`, `FILL` |
| números | `+`, `-`, `*`, `/`, `<`, `>`, `<=`, `>=`, `=` e `/=` |
| funções | `FUNCALL`, `APPLY`, `FUNCTIONP`, `FBOUNDP`, `SYMBOL-FUNCTION` |
| valores | `BOUNDP`, `SYMBOL-VALUE`, `SET`, `EQ`, `NOT`, `TYPE-OF` |

Símbolos possuem células separadas de valor e função. Portanto, uma variável e
uma função podem compartilhar o mesmo nome:

```lisp
(define resposta 41)
(defun resposta () 42)
(list resposta (resposta)) ; => (41 42)
```

`#'nome` consulta a célula de função.

## Listas

`CONSP`, `LISTP` e `ENDP` distinguem pares, listas e o fim de uma iteração.
`FIRST`/`REST` são as variantes de leitura de `CAR`/`CDR`; `RPLACA`, `RPLACD` e
os lugares correspondentes de `SETF` fazem alteração explícita.

```lisp
(append '(1 2) '(3 4) 'fim) ; => (1 2 3 4 . FIM)
(nth 2 '(0 1 2 3))          ; => 2
(last '(0 1 2 3) 2)         ; => (2 3)
```

`APPEND` copia as listas anteriores e reutiliza o último argumento. `NCONC`
liga destrutivamente os argumentos anteriores. `MEMBER` e `ASSOC` usam `EQL`
no estágio atual.

`MAPCAR` aceita uma ou mais listas, para na mais curta e devolve os resultados;
`MAPC` executa os mesmos chamados por efeito e devolve a primeira lista:

```lisp
(mapcar #'+ '(1 2 3) '(10 20)) ; => (11 22)
```

As variantes com `:KEY`, `:TEST` configurável e a família completa de
mapeamento permanecem no roteiro de conformidade.

## Vetores e `SETF`

`#(...)` lê um vetor literal autoavaliável. Assim como em Common Lisp, as
formas contidas no literal não são avaliadas; `VECTOR` constrói um vetor com
argumentos avaliados:

```lisp
#(1 (+ 1 1))          ; => #(1 (+ 1 1))
(vector 1 (+ 1 1))   ; => #(1 2)
```

O primeiro modelo de array é simples e unidimensional. `MAKE-ARRAY` recebe uma
dimensão inteira não negativa e aceita `:INITIAL-ELEMENT`:

```lisp
(let ((valores (make-array 3 :initial-element 7)))
  (setf (aref valores 1) 42)
  (list valores
        (length valores)
        (svref valores 1)))
; => (#(7 42 7) 3 42)
```

O `SETF` inicial aceita variáveis e os lugares `AREF`, `SVREF`, `CAR`, `CDR` e
`GETHASH`.
Outros protocolos de lugares generalizados, arrays multidimensionais,
element-types especializados e vetores ajustáveis permanecem pendentes.

No SDK C, `sef_vetor_criar`, `sef_vetor_tamanho`, `sef_vetor_obter` e
`sef_vetor_definir` operam sobre o mesmo objeto coletado. Um valor mantido pelo
hospedeiro entre avaliações deve continuar protegido por `SefRaiz`.

## Caracteres Unicode e strings

`#\A`, `#\Space`, `#\Newline` e `#\λ` produzem objetos `CHARACTER` reais. A
forma legível `#\U+NNNN` cobre códigos de controle sem nome. `CHAR-CODE` e
`CODE-CHAR` convertem entre caracteres e valores escalares Unicode; surrogates
e valores acima de `U+10FFFF` são rejeitados.

Strings permanecem armazenadas em UTF-8, mas `LENGTH`, `CHAR`, `SCHAR` e `ELT`
contam caracteres, não bytes:

```lisp
(length "ação")       ; => 4
(char "ação" 1)       ; => #\ç

(let ((texto "abc"))
  (setf (char texto 1) #\é)
  texto)               ; => "aéc"
```

A atribuição recompõe o UTF-8 quando o novo caractere usa outra quantidade de
bytes. `ELT` consulta e altera listas, vetores e strings. Os comparadores
`CHAR=`, `CHAR/=`, `CHAR<`, `CHAR>`, `CHAR<=` e `CHAR>=` usam a ordem dos pontos
de código; variantes sem distinção de caixa ainda não foram implementadas.

O SDK expõe `sef_caractere_criar`, `sef_valor_e_caractere` e
`sef_caractere_codigo`.

### Algoritmos de sequência

Listas próprias, vetores e strings compartilham cópia, inversão, recorte e
preenchimento. As operações não destrutivas mantêm o tipo da entrada:

```lisp
(reverse #(1 2 3))        ; => #(3 2 1)
(reverse "ação")          ; => "oãça"
(subseq '(0 1 2 3) 1 3)  ; => (1 2)
```

`COPY-SEQ` cria a estrutura externa independente e mantém os elementos. `FILL`
altera a sequência recebida e aceita `:START` e `:END`; para strings, o item
precisa ser um caractere:

```lisp
(let ((valores #(1 2 3 4)))
  (fill valores 9 :start 1 :end 3)
  valores) ; => #(1 9 9 4)
```

Os intervalos usam fim exclusivo e são validados antes da mutação.

## Tabelas hash

`MAKE-HASH-TABLE` cria uma tabela coletada pelo runtime. O teste de chaves
implementado nesta etapa é `EQL`: inteiros, reais e caracteres iguais localizam
a mesma entrada; os demais objetos conservam identidade. Colisões usam
endereçamento aberto e a tabela cresce automaticamente.

```lisp
(let ((placar (make-hash-table)))
  (setf (gethash 'azul placar) 40
        (gethash 'dourado placar) 42)
  (list (gethash 'dourado placar)
        (gethash 'ausente placar :sem-valor)
        (hash-table-count placar)))
; => (42 :SEM-VALOR 2)
```

`REMHASH` devolve verdadeiro quando remove uma chave. `CLRHASH` esvazia a
tabela e devolve a própria tabela. `GETHASH` devolve o valor encontrado ou
padrão como valor primário e um indicador secundário de presença, permitindo
distinguir um `NIL` armazenado de uma chave ausente. Outros testes de chave e
opções de redimensionamento permanecem pendentes.

As chaves e os valores participam da marcação do GC. A imagem preserva as
entradas, a identidade compartilhada e ciclos, inclusive uma tabela que
contenha referência para si mesma.

## Compilação nativa inicial

`COMPILE` baixa uma função compatível para a IR SSA e instala código x86-64 ou
AArch64 conforme o hospedeiro. O subconjunto atual aceita parâmetros inteiros,
constantes, `+`, `-`, `*`, `<`, `<=`, `IF` e exatamente uma forma no corpo.

```lisp
(defun calcular (x y)
  (if (< x y)
      (+ (* x 2) y)
      (- x y)))

(compile 'calcular)
(calcular 10 22)                    ; => 42
(compiled-function-p #'calcular)    ; => T
```

Chamadas comuns, `FUNCALL` e `APPLY` observam automaticamente o código
instalado. As funções compiladas atuais exigem argumentos i64 e retornam i64;
valores Lisp gerais e pontos seguros ainda não entraram na IR.

A imagem preserva a definição Lisp e descarta o cache de máquina. Depois de
abrir a imagem em outra arquitetura, chame `COMPILE` novamente.

## Objetos nativos

O frontend i64 pode produzir objetos relocáveis para os três formatos desktop:

```bash
sefirah compilar-elf exemplos/nativo.lisp calcular_nativo calcular.o
cc exemplos/integracao-c/chamar_nativo.c calcular.o -o chamar-nativo
./chamar-nativo

sefirah compilar-coff exemplos/nativo.lisp calcular_nativo calcular.obj
x86_64-w64-mingw32-gcc \
  exemplos/integracao-c/chamar_nativo.c calcular.obj \
  -o chamar-nativo.exe

sefirah compilar-macho \
  exemplos/nativo.lisp calcular_nativo calcular-macos.o
```

O símbolo exportado segue `int64_t funcao(const int64_t *args)`. O frontend
preserva a IR e escolhe a ABI correta: System V para ELF/Mach-O, Microsoft x64
para COFF e AAPCS64 em ARM64.

Os arquivos `.c` ficam em `exemplos/integracao-c/` porque demonstram o consumo
do SDK e dos objetos gerados. Eles não pertencem à sintaxe Sefirah.

## FFI i64

`EXTERNAL-I64` representa uma chamada C com retorno i64 e uma ou duas entradas
i64:

```lisp
(defun chamar-dobro (valor)
  (external-i64 "dobrar_i64" valor))

(defun combinar (a b)
  (external-i64 "combinar_i64" a b))
```

Para produzir um objeto com símbolo indefinido:

```bash
sefirah compilar-elf exemplos/externa.lisp chamar_dobro chamar-dobro.o
cc exemplos/integracao-c/chamar_externa.c chamar-dobro.o -o chamar-externa
```

Os gravadores emitem a relocação correspondente ao formato e à arquitetura. Um
símbolo não resolvido não pode ser executado no JIT.

### Vinculação pelo caminho

```lisp
(compile-external-i64 'chamar-dobro "./libcalculos.so")
(chamar-dobro 21) ; => 42
```

No Windows use uma `.dll`; no macOS, uma `.dylib`. O runtime resolve os nomes
com `LoadLibrary`/`GetProcAddress` ou `dlopen`/`dlsym`.

### Objeto de biblioteca

Abra uma biblioteca uma vez quando várias funções compartilham o recurso:

```lisp
(define calculos (open-shared-library "./libcalculos.so"))

(shared-library-p calculos)       ; => T
(shared-library-open-p calculos)  ; => T

(compile-external-i64 'chamar-dobro calculos)
(close-shared-library calculos)

(shared-library-open-p calculos)  ; => NIL
(chamar-dobro 21)                 ; => 42
```

Fechar é idempotente e impede novas vinculações pelo objeto. A função
compilada continua válida porque possui outra referência. A biblioteca só é
descarregada quando o objeto e todas as funções compiladas forem liberados.

### SDK C

Uma função obtida com `sef_runtime_compilar_objeto_i64` pode receber endereços
por `sef_funcao_compilada_vincular_externa_i64` ou pela variante `_binaria` e
ser finalizada com `sef_funcao_compilada_preparar_jit`.

O JIT cria trampolins próximos ao código para alcançar qualquer endereço de 64
bits sem tornar a página simultaneamente gravável e executável.

## Packages

O runtime inicia em `COMMON-LISP-USER`, que usa `COMMON-LISP`. Símbolos
`:nome` pertencem a `KEYWORD` e se autoavaliam. O package `SEFIRAH` guarda
nomes específicos da implementação, como `SEFIRAH::ENVIRONMENT` e
`SEFIRAH::SHARED-LIBRARY`.

Formas e funções disponíveis:

- `DEFPACKAGE` com `:USE` e `:EXPORT`;
- `IN-PACKAGE`;
- `MAKE-PACKAGE`, `FIND-PACKAGE`, `PACKAGE-NAME` e `PACKAGEP`;
- `USE-PACKAGE`, `EXPORT`, `INTERN` e `FIND-SYMBOL`;
- `SYMBOL-NAME`, `SYMBOL-PACKAGE` e `LIST-ALL-PACKAGES`.

```lisp
(defpackage :calculos
  (:use :common-lisp)
  (:export :resposta))

(in-package :calculos)
(defun resposta () 42)

(in-package :common-lisp-user)
(calculos:resposta) ; => 42
```

Packages, relações de uso e identidade dos símbolos fazem parte da imagem.

## Streams e arquivos

`*STANDARD-INPUT*`, `*STANDARD-OUTPUT*` e `*ERROR-OUTPUT*` pertencem ao processo
hospedeiro. `OPEN` usa modo binário para manter o comportamento previsível nos
três sistemas.

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

`:DIRECTION` aceita `:INPUT`, `:OUTPUT` e `:IO`. Para saída, `:IF-EXISTS`
aceita `:SUPERSEDE`, `:APPEND` ou `:ERROR`. O `READ-LINE` atual devolve `NIL` no
fim do arquivo; seu indicador secundário de EOF de Common Lisp ainda está
pendente.

## Imagem persistente

O formato binário v9 `.imagem` preserva o grafo do heap, incluindo vetores,
caracteres, tabelas hash, símbolos, packages, ambientes, funções, macros,
condições e recursos restauráveis. A gravação usa arquivo temporário e
substituição atômica. Primitivas C são restauradas pelo nome, nunca pelo
endereço. O leitor v9 aceita imagens v6, v7 e v8; uma nova gravação as atualiza
para v9.

```bash
sefirah imagem salvar desenvolvimento.imagem exemplos/inicio.lisp
sefirah imagem abrir desenvolvimento.imagem "(fatorial 6)"
sefirah imagem abrir desenvolvimento.imagem
```

Antes de salvar:

- feche streams de arquivo;
- feche objetos de biblioteca compartilhada;
- não dependa do cache JIT, pois ele será recompilado sob demanda.

Streams padrão são religados ao novo processo. Streams de arquivo e bibliotecas
fechados permanecem objetos fechados.

## REPL e IDE

O REPL textual aceita programas Lisp completos em vez de linhas físicas
isoladas. Lista, string, vetor ou prefixo de leitura aberto muda `sefirah>` para
o prompt de continuação `......>`. A avaliação ocorre somente quando a forma
está completa e imprime todos os valores devolvidos. `:ajuda` lista os comandos
e `:sair` encerra a sessão.

A IDE gráfica está dividida entre um motor de sessão independente da plataforma
e a apresentação em janela. Ela oferece buffer `.lisp` editável, ouvinte
multilinha, transcrição persistente, inspetor de resultados, avaliação do
buffer inteiro e abertura/gravação de arquivo:

```bash
sefirah_ide
sefirah_ide caminho/para/programa.lisp
```

Tab ou clique alterna entre editor e ouvinte. Enter insere linha no editor e
envia uma forma completa no ouvinte. F5 ou Ctrl+Enter executa o editor, Ctrl+S
salva e Ctrl+O recarrega o caminho atual. Setas, Home e End movem o cursor do
editor com consciência de UTF-8. O motor da sessão possui testes automatizados
sem janela. No macOS, Command pode substituir Ctrl nesses atalhos.

## GUI própria

A API C17 oferece `SefComponente` para painéis, rótulos, botões e campos;
layouts em linha/coluna com pesos; `SefTemaGui`; hit-testing; navegação de foco;
e despacho de ações. `SefInteracaoGui` converte Tab, Enter e ponteiro em foco e
acionamento.

X11, Win32 e Cocoa entregam teclado, atalhos e ponteiro à IDE. O adaptador Cocoa
é escrito em C sobre o runtime Objective-C e redesenha a superfície raster
própria depois de cada evento que altera o estado.

## Leitor e impressão

O leitor converte nomes ASCII para maiúsculas, reconhece comentários iniciados
por `;`, listas pontuadas, vetores `#(...)`, caracteres `#\`, quote, function
quote, quasiquote com `,`/`,@` e escapes simples em strings.

Objetos opacos possuem representações legíveis para diagnóstico, como streams,
funções compiladas, condições e bibliotecas compartilhadas.

## Limitações atuais

- GC mark-and-sweep somente entre unidades de avaliação;
- ausência de bignums, racionais, complexos e numeric tower completo;
- arrays limitados a vetores simples unidimensionais;
- operações Unicode sem normalização, grapheme clusters ou case folding;
- divisão sempre produz `FLOAT`;
- condições e restarts ainda incompletos;
- compilação limitada ao subconjunto i64;
- FFI sem floats, ponteiros, structs e callbacks gerais;
- GUI sem fontes vetoriais, HiDPI, IME e acessibilidade;
- IDE sem seleção, edição estrutural, seletor de arquivo, debugger,
  profiler e histórico transacional.

Consulte o [roteiro para 1.0](roteiro.md) para os próximos marcos.
