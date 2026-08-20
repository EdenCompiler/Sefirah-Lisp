# Guia do bootstrap Sefirah Lisp

[English](../docs-en/manual.md) · **Português do Brasil**

O bootstrap atual estabelece o primeiro caminho vertical da linguagem ao
desktop. Ele ainda não é ANSI Common Lisp completo: este guia descreve apenas
o comportamento implementado e testado no repositório.

Os identificadores e comentários da implementação são escritos em PT-BR. Todos
os comandos públicos, ajuda, diagnósticos, textos do REPL e da IDE e saídas de
ferramentas usam inglês. As grafias antigas da CLI em português permanecem
como aliases de compatibilidade.

Programas usam exclusivamente a extensão `.lisp`. A extensão antiga `.sef` não
faz parte do formato público.

## Início rápido

```bash
./construir/sefirah evaluate "(+ 20 22)"
./construir/sefirah run exemplos/inicio.lisp
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
| condições e restarts | `ERROR`, `SIGNAL`, `HANDLER-BIND`, `HANDLER-CASE`, `IGNORE-ERRORS`, `RESTART-CASE`, `INVOKE-RESTART`, descoberta e auxiliares padrão de restarts nomeados |
| valores múltiplos | `VALUES`, `VALUES-LIST`, `MULTIPLE-VALUE-BIND`, `MULTIPLE-VALUE-LIST`, `MULTIPLE-VALUE-CALL`, `MULTIPLE-VALUE-PROG1`, `NTH-VALUE` |
| listas | `CONS`, `CAR`, `CDR`, `FIRST`, `REST`, `LIST`, `APPEND`, `NCONC`, `NTH`, `NTHCDR`, `LAST` |
| vetores | `VECTOR`, `MAKE-ARRAY`, `AREF`, `SVREF`, `VECTORP`, `ARRAYP` |
| tabelas hash | `MAKE-HASH-TABLE`, `GETHASH`, `HASH-TABLE-P`, `HASH-TABLE-COUNT`, `REMHASH`, `CLRHASH` |
| caracteres | `CHARACTERP`, `CHAR-CODE`, `CODE-CHAR` e comparadores `CHAR...` |
| sequências | `LENGTH`, `ELT`, `CHAR`, `SCHAR`, `COPY-SEQ`, `REVERSE`, `SUBSEQ`, `FILL` |
| números | `+`, `-`, `*`, `/`, `<`, `>`, `<=`, `>=`, `=` e `/=` |
| funções | `FUNCALL`, `APPLY`, `FUNCTIONP`, `FBOUNDP`, `SYMBOL-FUNCTION`, `FDEFINITION`, `FMAKUNBOUND` |
| símbolos e valores | `SYMBOLP`, `KEYWORDP`, `CONSTANTP`, `MAKE-SYMBOL`, `COPY-SYMBOL`, `GENSYM`, `GENTEMP`, `SYMBOL-NAME`, `SYMBOL-PACKAGE`, `SYMBOL-PLIST`, `GET`, `REMPROP`, `BOUNDP`, `SYMBOL-VALUE`, `SET`, `MAKUNBOUND`, `EQ`, `NOT`, `TYPE-OF` |

Símbolos possuem células separadas de valor e função. Portanto, uma variável e
uma função podem compartilhar o mesmo nome:

```lisp
(define resposta 41)
(defun resposta () 42)
(list resposta (resposta)) ; => (41 42)
```

`#'nome` consulta a célula de função.

`MAKUNBOUND` e `FMAKUNBOUND` removem bindings globais de valor e função e
devolvem o símbolo mesmo quando a célula já estava vazia. `FDEFINITION`
consulta a célula de função. `SYMBOL-VALUE`, `SYMBOL-FUNCTION` e `FDEFINITION`
são lugares graváveis de `SETF`, permitindo instalar ou substituir definições
no mundo vivo sem reiniciá-lo:

```lisp
(setf (symbol-value 'resposta) 41)
(setf (fdefinition 'soma) (symbol-function '+))
(soma resposta 1) ; => 42
```

Nomes de função estão limitados a símbolos neste estágio; nomes da forma
`(SETF nome)` ainda estão pendentes.

Símbolos também possuem listas de propriedades. `GET` consulta um indicador
com valor padrão opcional, `REMPROP` remove uma entrada e tanto `GET` quanto
`SYMBOL-PLIST` são lugares graváveis de `SETF`. A lista deve alternar pares de
indicador/valor. Ela funciona para `NIL` e símbolos comuns e persiste nas
imagens v10 do mundo:

```lisp
(setf (get 'resposta :documentation) "A resposta definitiva")
(get 'resposta :documentation) ; => "A resposta definitiva"
(remprop 'resposta :documentation) ; => T
```

`MAKE-SYMBOL` cria um símbolo novo e não internado cujo nome preserva
exatamente a string. `COPY-SYMBOL` também cria um símbolo novo; com segundo
argumento diferente de `NIL`, copia superficialmente a espinha da property
list e as células vinculadas de valor e função. `GENSYM` aceita o prefixo
string ou sufixo inteiro não negativo padrão e, nos demais casos, usa o
`*GENSYM-COUNTER*` mutável:

```lisp
(setf *gensym-counter* 41)
(gensym)         ; => #:G41
(gensym "tmp-") ; => #:|tmp-42|
(gensym 7)       ; => #:G7, sem alterar o contador
```

Identidades não internadas, células copiadas, metadados e o contador gensym
permanecem após restaurar a imagem do mundo. O inspetor vivo expõe `PACKAGE`
como `NIL` e uma aresta `PROPERTIES` nesses símbolos.

`GENTEMP` usa o mesmo contador persistente para internar um nome novo no
package selecionado (ou no package atual). A forma aceita um prefixo string
opcional e um designador de package, ignora toda colisão acessível local ou
herdada e devolve um símbolo interno:

```lisp
(gentemp "TEMP-" "CALCULATIONS") ; => CALCULATIONS::|TEMP-43|
```

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
`sef_valor_nome_tipo` expõe o nome de tipo estável usado para diagnóstico pelas
ferramentas residentes sem revelar o layout privado do objeto. Ferramentas
genéricas podem usar `sef_valor_quantidade_componentes` e
`sef_valor_componente` para percorrer valores filhos rotulados sem incluir o
header privado `sefirah/interno.h`. Componentes são emprestados: o pai precisa
continuar enraizado, e um filho retido precisa de sua própria `SefRaiz` antes de
outra avaliação ou coleta.

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
sefirah compile-elf exemplos/nativo.lisp calcular_nativo calcular.o
cc exemplos/integracao-c/chamar_nativo.c calcular.o -o chamar-nativo
./chamar-nativo

sefirah compile-coff exemplos/nativo.lisp calcular_nativo calcular.obj
x86_64-w64-mingw32-gcc \
  exemplos/integracao-c/chamar_nativo.c calcular.obj \
  -o chamar-nativo.exe

sefirah compile-macho \
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
sefirah compile-elf exemplos/externa.lisp chamar_dobro chamar-dobro.o
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

Ferramentas residentes podem chamar `sef_runtime_consultar_vinculos_simbolo`
com um designador de símbolo Lisp. A consulta somente de leitura informa se o
símbolo está acessível no package atual e se suas células globais de valor e
função possuem binding. Um nome ausente continua não internado, e a consulta
não avalia Lisp nem substitui os últimos valores múltiplos do runtime. A IDE usa
esse contrato para distinguir definições apenas no fonte das instaladas no
mundo vivo.

## Packages

O runtime inicia em `COMMON-LISP-USER`, que usa `COMMON-LISP`. Símbolos
`:nome` pertencem a `KEYWORD` e se autoavaliam. O package `SEFIRAH` guarda
nomes específicos da implementação, como `SEFIRAH::ENVIRONMENT` e
`SEFIRAH::SHARED-LIBRARY`.

Formas e funções disponíveis:

- `DEFPACKAGE` com `:NICKNAMES`, `:SHADOW`, `:SHADOWING-IMPORT-FROM`, `:USE`,
  `:IMPORT-FROM`, `:INTERN` e `:EXPORT`;
- `IN-PACKAGE`;
- `MAKE-PACKAGE` com `:NICKNAMES` e `:USE`, `FIND-PACKAGE`, `PACKAGE-NAME`,
  `PACKAGE-NICKNAMES` e `PACKAGEP`;
- `RENAME-PACKAGE` com substituição de apelidos;
- `DELETE-PACKAGE` protegido;
- `DO-SYMBOLS`, `DO-EXTERNAL-SYMBOLS` e `DO-ALL-SYMBOLS`;
- `FIND-ALL-SYMBOLS`;
- `USE-PACKAGE`, `UNUSE-PACKAGE`, `PACKAGE-USE-LIST`,
  `PACKAGE-USED-BY-LIST`, `EXPORT`, `UNEXPORT`, `IMPORT`, `UNINTERN`, `SHADOW`,
  `SHADOWING-IMPORT`, `PACKAGE-SHADOWING-SYMBOLS`, `INTERN` e `FIND-SYMBOL`;
- `SYMBOLP`, `KEYWORDP`, `CONSTANTP`, `SYMBOL-NAME`, `SYMBOL-PACKAGE` e
  `LIST-ALL-PACKAGES`.

`INTERN` e `FIND-SYMBOL` devolvem o valor secundário de estado definido pelo
ANSI: `:INTERNAL`, `:EXTERNAL`, `:INHERITED` ou `NIL`. Um símbolo recém-criado
tem estado `NIL`; uma consulta posterior informa seu estado real. Símbolos de
`KEYWORD` são externos e se autoavaliam.

`IMPORT` torna um símbolo ou lista de símbolos diretamente presente em um
package sem alterar o package de origem de um símbolo já internado. Importar
uma identidade nova de `MAKE-SYMBOL` a adota no destino. Símbolos importados
podem ser exportados, e `UNINTERN` remove sua presença local e estado externo.
Remover um símbolo de seu package de origem o torna não internado novamente;
`COMMON-LISP` possui lock da implementação contra `UNINTERN`, evitando que o
runtime vivo perca seus operadores obrigatórios.

`SHADOW` cria ou designa um símbolo local que oculta nomes herdados de modo
intencional, permitindo usar outro package com exportação conflitante.
`SHADOWING-IMPORT` escolhe uma identidade existente como vencedora local.
`PACKAGE-SHADOWING-SYMBOLS` devolve uma lista nova dessas escolhas. A remoção
de um símbolo de shadowing é rejeitada quando exporia dois símbolos herdados
distintos; todas as escolhas persistem na restauração da imagem.

`DEFPACKAGE` valida a declaração completa e aplica opções de símbolo em fases
semânticas em vez da ordem textual. Escolhas de shadow e shadowing-import são
instaladas antes das listas de uso; seguem importações comuns e nomes
internados; exportações ficam por último. As duas opções de importação recebem
um package de origem seguido pelos nomes de símbolos que serão reutilizados.

`USE-PACKAGE` e `UNUSE-PACKAGE` aceitam um designador de package ou uma lista
própria. `PACKAGE-USE-LIST` e `PACKAGE-USED-BY-LIST` devolvem listas novas de
relacionamentos. `UNEXPORT` torna interno um símbolo externo sem mudar sua
identidade; o lock da implementação protege `COMMON-LISP` de alterações na
topologia.

Nomes e apelidos de packages são comparados sem distinção de caixa para
designadores ASCII. Apelidos declarados por `DEFPACKAGE :NICKNAMES` ou
`MAKE-PACKAGE :NICKNAMES` são únicos entre nomes canônicos e outros apelidos,
persistem nas imagens do mundo e são devolvidos como strings copiadas por
`PACKAGE-NICKNAMES`. `MAKE-PACKAGE :USE` aceita uma lista própria; sua omissão
mantém o padrão conveniente da implementação de usar `COMMON-LISP`.

`RENAME-PACKAGE` substitui atomicamente o nome canônico e o conjunto completo
de apelidos depois de verificar conflitos em todos os designadores novos e
apelidos duplicados. Nomes antigos deixam de resolver, exceto quando incluídos
explicitamente na nova lista de apelidos. Renomeações persistem nas imagens do
mundo.

`DELETE-PACKAGE` devolve falso para um package já ausente/removido. A operação
rejeita packages da implementação, o package atual e packages ainda usados por
outro; o chamador pode consultar e desligar essas relações primeiro. A remoção
bem-sucedida limpa metadados do registro e usos de saída. Símbolos sobreviventes
mantêm a identidade do objeto package, são impressos com `#:` e continuam
válidos após restaurar uma imagem; `PACKAGE-NAME` e `PACKAGE-NICKNAMES` devolvem
`NIL` para esse objeto removido.

As três formas de iteração de símbolos vinculam uma variável léxica de símbolo
e fornecem um bloco implícito `NIL`, permitindo que `RETURN` encerre a
iteração. `DO-SYMBOLS` visita uma vez os símbolos locais e herdados acessíveis
do package; `DO-EXTERNAL-SYMBOLS` visita suas exportações; e `DO-ALL-SYMBOLS`
visita cada símbolo cujo package de origem continua registrado, inclusive `NIL`
e excluindo símbolos não internados/de packages removidos. A forma de resultado
opcional é avaliada depois do loop com a variável iteradora ligada a `NIL`.

`FIND-ALL-SYMBOLS` devolve as identidades internadas distintas com um nome
exato entre packages registrados, incluindo `NIL` quando solicitado. A função
exclui intencionalmente símbolos não internados e símbolos cujo package de
origem foi removido; a ordem do resultado não é prometida.

`NIL` é simultaneamente a lista vazia, falso e o símbolo externo chamado
`"NIL"` em `COMMON-LISP`. Ele é herdado por `COMMON-LISP-USER`; `SYMBOLP`,
`SYMBOL-NAME`, `SYMBOL-PACKAGE`, `BOUNDP` e `SYMBOL-VALUE` observam essa
semântica de símbolo. `T`, `NIL` e símbolos keyword são protegidos contra
atribuição pelas formas de binding implementadas e por `SET`.

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

## Condições e restarts nomeados

A camada inicial de condições captura erros com `HANDLER-CASE` ou
`IGNORE-ERRORS`. `HANDLER-BIND` instala funções com escopo dinâmico, chamadas no
contexto do sinalizador por `SIGNAL` e `ERROR`. Um handler pode retornar para
recusar ou transferir o controle ao escolher uma recuperação estabelecida com
`RESTART-CASE`. `INVOKE-RESTART` executa todas as limpezas intermediárias de
`UNWIND-PROTECT` antes de entrar na cláusula escolhida. Os caminhos normal e
reiniciado preservam seus valores múltiplos.

```lisp
(restart-case
    (handler-bind ((error (lambda (condicao)
                            (invoke-restart 'usar-valor 42))))
      (error "recuperavel"))
  (usar-valor (valor)
    (+ valor 1))) ; => 43

(restart-case
    (let ((reinicio (find-restart 'repetir)))
      (list (type-of reinicio)
            (restart-name reinicio)
            (eq reinicio (first (compute-restarts)))))
  (repetir () :repetido)) ; => (RESTART REPETIR T)
```

`FIND-RESTART` e `COMPUTE-RESTARTS` devolvem objetos `RESTART` de primeira
classe na ordem de precedência dinâmica, inclusive escolhas anônimas ou com o
mesmo nome. `INVOKE-RESTART` aceita um objeto ativo ou um nome não-`NIL`. Um
objeto pode sobreviver ao seu escopo dinâmico para persistência e inspeção, mas
invocá-lo depois que se torna inativo sinaliza erro. Associação a condições, a
hierarquia e os especificadores de tipo padrão completos e as opções de
cláusula `:REPORT`, `:TEST` e `:INTERACTIVE` continuam pendentes. `ABORT`,
`CONTINUE`, `MUFFLE-WARNING`, `STORE-VALUE` e `USE-VALUE` invocam um restart
ativo com o nome correspondente; a filtragem pela condição opcional ainda não
está disponível.

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
aceita `:SUPERSEDE`, `:APPEND` ou `:ERROR`. `READ-LINE` aceita seus quatro
argumentos opcionais ANSI e devolve `(linha, missing-newline-p)`. Em EOF
imediato, `eof-error-p` é verdadeiro por padrão; quando falso, são devolvidos o
`eof-value` fornecido e verdadeiro. O sistema inicial de condições ainda
sinaliza um `ERROR` geral, em vez da condição mais específica `END-OF-FILE`.

## Imagem persistente

O formato binário v10 `.imagem` preserva o grafo do heap, incluindo vetores,
caracteres, tabelas hash, símbolos, packages, ambientes, funções, macros,
condições, objetos restart e recursos restauráveis. A gravação usa arquivo
temporário e substituição atômica. Primitivas C são restauradas pelo nome,
nunca pelo endereço. O leitor v10 aceita imagens de v6 a v9; a abertura realiza
migrações direcionadas que restauram primitivas novas e a associação canônica
de `NIL` ao package, além dos símbolos exportados de formas especiais novas,
enquanto uma nova gravação atualiza o resultado para v10.

```bash
sefirah image save desenvolvimento.imagem exemplos/inicio.lisp
sefirah image open desenvolvimento.imagem "(fatorial 6)"
sefirah image open desenvolvimento.imagem
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
está completa e imprime todos os valores devolvidos. `:help` lista os comandos
e `:quit` encerra a sessão.

A IDE gráfica está dividida entre um motor de sessão independente da plataforma
e uma apresentação desktop de estação Lisp. Sua paleta em creme, sálvia, oliva
e âmbar preserva a linhagem visual de Interlisp e Lisp Machines, enquanto o
layout de workbench segue convenções de editores modernos. Ela oferece abas
persistentes de edição `.lisp`, ouvinte
multilinha, transcrição persistente, inspetor de resultados, avaliação do
buffer inteiro, avaliação estrutural e incremental, navegação de definições,
snapshots do mundo vivo, desfazer/refazer linear e abertura/gravação de arquivo:

```bash
sefirah_ide
sefirah_ide caminho/para/programa.lisp
sefirah_ide caminho/para/diretorio-do-projeto
```

Abrir outro fonte cria uma aba em vez de substituir o buffer corrente. Cada aba
mantém cursor, seleção, linha do tempo limitada de desfazer/refazer, histórico
de avaliação incremental e estado não gravado. Abas alteradas exibem `*`, e um
clique na aba ativa seu estado de edição preservado.

Passar um diretório o abre como workspace. O Explorer indexa recursivamente até
10.000 arquivos `.lisp`, ignora diretórios gerados/de dependências e ciclos por
links simbólicos e apresenta os caminhos em ordem determinística. Com foco no
Explorer, Cima/Baixo percorrem circularmente o índice e Enter abre o fonte
selecionado em uma aba; um clique seleciona e abre uma entrada visível. O
primeiro fonte reutiliza uma aba vazia ainda intocada.

A barra de comandos fornece botões para Run, Run Form, Run Changes, Save, Auto
Save, Snapshot, Restore, Commands, Symbols, References, New Tab, Close Tab, Open
File, Open Folder, New File, New Folder, Find, Source Control, Profile e Refresh. A barra passa para
uma segunda linha em vez de ocultar ações quando a janela fica estreita. Os
controles de arquivos e pastas usam um prompt de caminho interno à IDE e exibem falhas em
inglês nesse prompt. A criação é exclusiva — não
sobrescreve um caminho existente — e Refresh preserva o fonte selecionado
quando ele ainda existe. Os botões de avaliação e snapshot acionam as mesmas
operações sobre o mundo vivo que F5/F6/F7, Ctrl+S e F9/F10; a barra não cria um
fluxo batch separado. A entrada de caminho preserva exatamente as letras
maiúsculas e minúsculas digitadas, e a fonte bitmap desenha as duas caixas de
modo distinto, mantendo caminhos válidos em sistemas de arquivos sensíveis a
caixa.

Auto Save começa desligado. O botão `AUTO ON/OFF` ou o comando pesquisável
`Toggle Auto Save` muda o modo da sessão atual, e a barra de estado sempre o
mostra. Quando ligado, cada edição de conteúdo, remoção, undo ou redo
bem-sucedido grava imediatamente uma aba nomeada no caminho exato e remove seu
marcador de alteração. Buffers sem nome continuam modificados até o usuário lhes
dar um caminho com Save. A API headless da sessão oferece o mesmo comportamento
por `sef_sessao_ide_salvamento_automatico` e
`sef_sessao_ide_salvamento_automatico_definir`.

Ctrl+N, `NEW TAB` ou `New Untitled Tab` cria outro documento vazio no editor sem
pedir um caminho no sistema de arquivos. Ctrl+W, `CLOSE TAB` ou `Close Active
Tab` fecha o documento ativo e seleciona seu vizinho à direita, ou o vizinho à
esquerda ao fechar a última aba. Um documento alterado abre a confirmação
`UNSAVED CHANGES` dentro do workbench; Enter descarta e fecha explicitamente,
enquanto Escape mantém a aba. Fechar a única aba a substitui por um novo
`untitled.lisp`, impedindo que a IDE fique sem editor. `NEW FILE` continua sendo
a operação separada que cria um arquivo nomeado no disco.

Abrir ou atualizar um workspace também atualiza o painel somente de leitura
Source Control. O botão `SOURCE`, um clique no painel ou `Refresh Source Control`
na paleta de comandos o atualiza de modo independente. O painel mostra o
cabeçalho de branch e o status porcelain estável de duas colunas do Git para
alterações no índice e na árvore de trabalho, com legenda em inglês para
caminhos modificados, adicionados, removidos, renomeados e não rastreados. A
Sefirah inicia o Git diretamente com vetores de argumentos ou uma linha de
comando Windows cotada com segurança; caminhos do workspace nunca são
interpolados em um shell. A saída é limitada a 1 MiB, a caixa exata dos caminhos
é preservada, e Git indisponível ou workspaces sem repositório recebem status em
inglês dentro da IDE. Stage, commits, branches e outras ações mutáveis do Git
continuam pendentes.

Ctrl+P abre Quick Open e filtra o índice do workspace sem diferenciar caixa.
Ctrl+Shift+P ou o botão Commands abre uma paleta pesquisável com operações de
arquivo/pasta, avaliação, snapshots, navegação estrutural, foco e desfazer/refazer.
Cima/Baixo percorrem resultados circularmente, Enter aciona o item selecionado,
Escape fecha a sobreposição e resultados visíveis aceitam clique. Ctrl+O abre o
prompt de caminho de arquivo.

Ctrl+F, o botão `FIND` ou `Find in Active Editor` na paleta de comandos abre a
sobreposição Find do editor. Uma seleção de linha única inicia sua consulta;
caso contrário, a última consulta enviada é restaurada. A correspondência não
diferencia letras ASCII por caixa e preserva limites UTF-8. Enter seleciona o
próximo resultado, Shift+Enter seleciona o anterior, as duas direções retornam
circularmente, e a sobreposição informa em inglês o resultado atual e o total.
Escape fecha Find sem alterar o texto-fonte.

Tab, Shift+Tab ou clique alterna entre editor, inspetor, depurador e ouvinte.
Enter insere linha no editor, abre o componente ou a condição selecionada ou
envia uma forma completa no ouvinte. F5 ou Ctrl+Enter executa o buffer inteiro;
F6 encontra e executa somente a forma completa de nível superior no cursor,
enquanto Shift+F6 seleciona essa forma para substituição ou remoção estrutural.
Ctrl+Z e Ctrl+Y percorrem a linha do tempo linear e limitada do editor. Ctrl+S
salva e Ctrl+O abre o prompt de caminho. F7 avalia somente as formas de topo
cujo conteúdo mudou desde a última avaliação bem-sucedida. F8 visita a próxima
definição nomeada e Shift+F8 visita a anterior; o navegador reconhece funções,
macros, variáveis, parâmetros, constantes, packages e formas `DEFINE`, ignorando
comentários e strings. F11 salta do símbolo no cursor para sua definição
nomeada. F12 visita a próxima referência estrutural e Shift+F12 visita a
anterior, com retorno circular nas duas extremidades; ocorrências em strings e
comentários são excluídas. Setas, Home e End movem o cursor do editor com
consciência de UTF-8; com Shift, estendem um intervalo por ponto de código.
Digitar ou usar Backspace substitui ou remove a seleção como uma única edição
reversível. No ouvinte, Cima e Baixo recuperam até 128 eventos enviados,
inclusive formas multilinha.

F9 grava o mundo Lisp atual em um arquivo `.imagem` ao lado do fonte atual; F10
substitui o runtime ativo por esse snapshot, preservando editor, histórico do
ouvinte e transcrição. Uma falha de restauração não altera o mundo ativo.
Recursos locais do processo, como streams abertas e bibliotecas compartilhadas,
mantêm as restrições do formato de imagem descritas acima. Trata-se de um
snapshot do mundo, ainda não da restauração completa da sessão de desktop.

O dock de ferramentas à direita entrega toda a sua área a uma de cinco abas:
Inspector, Browser, Debugger, Source Control ou Profile. Clicar em uma aba troca
a ferramenta sem descartar seu estado vivo de sessão. Avaliações selecionam
Inspector no sucesso e Debugger na falha; comandos de definição/referência
selecionam Browser, e a ação `SOURCE` seleciona Source Control. Assim, o layout
segue um workbench moderno sem substituir as ferramentas conectadas de Lisp
Machine por janelas sem relação.

Cada avaliação do runtime iniciada por Run, Run Form, Run Changes ou pelo
ouvinte adiciona uma entrada local da sessão à ferramenta Profile. O histórico
limitado retém as 64 entradas mais recentes e identifica cada uma como `EDITOR`,
`FORM AT CURSOR`, `CHANGE` ou `REPL`, com resultado `OK`/`ERROR` e tempo
monotônico decorrido em milissegundos. O painel informa tempo total e médio e
mostra primeiro seus 16 eventos mais novos. O botão `PROFILE` o abre, enquanto
`Clear Evaluation Profile` na paleta de comandos zera as medições. Falhas da
verificação sintática não são cronometradas porque o avaliador do runtime não é
executado. O recurso ainda não amostra pilhas nem atribui tempo a funções
individuais.

Cada condição de avaliação não tratada é retida por uma raiz pública do GC em um
histórico limitado a 32 entradas. Shift+F9/Shift+F10 ou Cima/Baixo com foco no
depurador navegam por esse histórico; Enter abre o objeto condição selecionado
no inspetor geral. Falhas internas do avaliador e leitor recebem condições
`ERROR` sintetizadas, enquanto diagnósticos de sintaxe e I/O externo permanecem
entradas rotuladas sem objeto Lisp.

Quando um `ERROR` não tratado retorna através de `RESTART-CASE`, o runtime
registra os objetos restart de primeira classe que estavam ativos no ponto da
sinalização. A IDE os copia para raízes públicas do GC na mesma entrada limitada
de histórico da condição. Enter abre uma prateleira com a condição seguida
desses snapshots de restart; Esquerda/Direita navega pela prateleira, e o
inspetor expõe `NAME` e `ACTIVE` de cada restart.

O SDK C expõe o snapshot emprestado por
`sef_runtime_quantidade_reinicios_ultima_condicao` e
`sef_runtime_reinicio_ultima_condicao`. Sua vida termina quando a próxima
avaliação começa; portanto, ferramentas residentes devem criar raízes `SefRaiz`
para valores retidos. Os registros executáveis dos restarts continuam
pertencendo à pilha da avaliação e tornam-se corretamente inativos quando ela é
desenrolada. O painel marca os snapshots como históricos e não finge que uma
falha encerrada ainda possui restarts invocáveis. Invocá-los no ponto da falha
ainda exige uma continuação suspensível do avaliador/depurador e permanece
pendente.

O navegador
mostra o catálogo de definições ou as referências/callers do símbolo
selecionado, marca a ocorrência atual e move o cursor do editor diretamente até
ela. O inspetor enraíza no coletor de lixo todos os valores da última avaliação,
mostra seu tipo e representação legível e expõe o resultado de valores
múltiplos como uma prateleira viva de objetos. Ele expõe recursivamente os
componentes rotulados de pares, vetores, funções, ambientes lexicais,
condições, restarts, símbolos, packages e hash tables. Cima/Baixo selecionam um
componente, Enter o
abre, Backspace volta ao pai e Esquerda/Direita alternam as raízes; cada nó do
caminho possui uma raiz explícita no GC. O motor da sessão, os históricos, o
analisador estrutural, o histórico do depurador e o inspetor possuem testes
automatizados sem janela. No macOS, Command pode substituir Ctrl nos atalhos.

Essas ferramentas recuperam de propósito uma parte pequena e prática do fluxo
das Lisp machines: o Programmer's Assistant do Interlisp mantinha eventos que
podiam ser repetidos e seus editores operavam sobre estruturas Lisp, enquanto o
Genera conectava navegação de fontes, Listener, Inspector e mundos salvos ao
mesmo sistema vivo. O Sefirah mantém essas ideias atrás de uma API de sessão
portátil, sem acoplá-las a um backend de janela. Consulte a
[linha do tempo do Interlisp](https://interlisp.org/history/timeline/) e o
[guia do usuário do Genera](https://www.bitsavers.org/pdf/symbolics/software/genera_8/Genera_User_s_Guide.pdf)
para conhecer os sistemas históricos que motivam essa direção.

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
quote, quasiquote com `,`/`,@`, nomes de símbolos entre barras verticais,
escapes individuais de símbolo e escapes simples em strings. O impressor
legível adiciona `|...|` e escapes quando o nome perderia caixa ou seria lido
como sintaxe.

Objetos opacos possuem representações legíveis para diagnóstico, como streams,
funções compiladas, condições e bibliotecas compartilhadas.

## Limitações atuais

- GC mark-and-sweep somente entre unidades de avaliação;
- ausência de bignums, racionais, complexos e numeric tower completo;
- arrays limitados a vetores simples unidimensionais;
- operações Unicode sem normalização, grapheme clusters ou case folding;
- divisão sempre produz `FLOAT`;
- condições sem a hierarquia completa, restarts associados a condições ou
  seleção suspensível de restart;
- compilação limitada ao subconjunto i64;
- FFI sem floats, ponteiros, structs e callbacks gerais;
- GUI sem fontes vetoriais, HiDPI, IME e acessibilidade;
- IDE sem seletor de arquivo nativo, debugger suspensível, profiler por
  amostragem/grafo de chamadas ou desfazer seletivo/fora de ordem no estilo
  Interlisp.

Consulte o [roteiro para 1.0](roteiro.md) para os próximos marcos.
