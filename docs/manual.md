# Guia do bootstrap Sefirah Lisp

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
| valores | inteiros, reais, strings, símbolos, listas próprias e pares |
| avaliação | `QUOTE`, `IF`, `PROGN`, `LAMBDA`, `FUNCTION` |
| bindings | `DEFINE`, `DEFVAR`, `DEFPARAMETER`, `SETQ`, `LET`, `LET*` |
| funções e macros | `DEFUN`, `DEFMACRO`, `FLET`, `LABELS`, `MACROLET`, `&REST` |
| composição | `COND`, `WHEN`, `UNLESS`, `AND`, `OR`, quasiquote, `,` e `,@` |
| controle | `BLOCK`, `RETURN-FROM`, `RETURN`, `CATCH`, `THROW`, `UNWIND-PROTECT` |
| condições | `ERROR`, `HANDLER-CASE`, `IGNORE-ERRORS` |
| listas | `CONS`, `CAR`, `CDR`, `LIST`, `LENGTH`, `ATOM`, `NULL` |
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
aceita `:SUPERSEDE`, `:APPEND` ou `:ERROR`. `READ-LINE` devolve `NIL` no fim do
arquivo enquanto valores múltiplos não estiverem disponíveis.

## Imagem persistente

O formato binário v6 `.imagem` preserva o grafo do heap, símbolos, packages,
ambientes, funções, macros, condições e recursos restauráveis. A gravação usa
arquivo temporário e substituição atômica. Primitivas C são restauradas pelo
nome, nunca pelo endereço.

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

## GUI própria

`sefirah ide` abre a composição inicial rasterizada pelo próprio projeto:

```bash
sefirah ide
```

A API C17 oferece `SefComponente` para painéis, rótulos, botões e campos;
layouts em linha/coluna com pesos; `SefTemaGui`; hit-testing; navegação de foco;
e despacho de ações. `SefInteracaoGui` converte Tab, Enter e ponteiro em foco e
acionamento.

X11 e Win32 já entregam teclado e ponteiro ao ouvinte. A ponte macOS ainda não
encaminha teclado, e o editor e o inspetor permanecem demonstrativos.

## Leitor e impressão

O leitor converte nomes ASCII para maiúsculas, reconhece comentários iniciados
por `;`, listas pontuadas, quote, function quote, quasiquote com `,`/`,@` e
escapes simples em strings.

Objetos opacos possuem representações legíveis para diagnóstico, como streams,
funções compiladas, condições e bibliotecas compartilhadas.

## Limitações atuais

- GC mark-and-sweep somente entre unidades de avaliação;
- ausência de bignums, racionais, complexos e numeric tower completo;
- divisão sempre produz `FLOAT`;
- REPL textual com uma linha por interação;
- condições e restarts ainda incompletos;
- compilação limitada ao subconjunto i64;
- FFI sem floats, ponteiros, structs e callbacks gerais;
- GUI sem fontes vetoriais, HiDPI, IME e acessibilidade.

Consulte o [roteiro para 1.0](roteiro.md) para os próximos marcos.
