# Conformidade ANSI Common Lisp

Este relatório descreve o bootstrap executável, não o 1.0 pretendido. Um
recurso é chamado de verificado somente quando seu contrato observável possui
teste automatizado. O Sefirah ainda não declara ser uma implementação conforme
ao ANSI Common Lisp.

## Legenda de estado

| Marca | Significado |
| --- | --- |
| ✅ | Implementado e coberto por testes automatizados no escopo declarado |
| ◐ | Existe um subconjunto útil, mas ainda falta comportamento ANSI relevante |
| — | Não implementado |

## Áreas da linguagem

| Área | Estado | Evidência atual e limite |
| --- | --- | --- |
| Leitor e impressor | ◐ | Listas, listas pontuadas, strings, números, caracteres, vetores, símbolos escapados, quote, function quote e quasiquote funcionam; faltam readtables, sintaxe de bases e notação circular. |
| Avaliação e binding | ◐ | Funções lexicais, macros, funções locais, variáveis especiais iniciais, lugares generalizados e controle não local funcionam; faltam declarações e o protocolo completo de lambda lists. |
| Símbolos | ◐ | `NIL` possui sua identidade de símbolo externo de `COMMON-LISP`, e os contratos testados de consulta/valor incluem `SYMBOLP`, `KEYWORDP`, `CONSTANTP`, `SYMBOL-NAME`, `SYMBOL-PACKAGE`, `BOUNDP` e `SYMBOL-VALUE`; ainda faltam property lists, criação/cópia de símbolos, gensyms e operações de unbinding. |
| Valores múltiplos | ✅ | As formas implementadas preservam valores entre chamadas, limpeza e transferência não local. `GETHASH`, `READ-LINE`, `INTERN` e `FIND-SYMBOL` expõem seus valores secundários testados. |
| Listas e sequências | ◐ | Operações centrais de listas e um protocolo inicial de sequências cobrem listas, vetores e strings UTF-8; faltam argumentos keyword e a família completa de funções de sequência. |
| Números | ◐ | Existem inteiros de largura fixa e doubles do hospedeiro; faltam bignums, razões, complexos, overflow exato e a biblioteca numérica completa. |
| Packages | ◐ | Packages no heap, `DEFPACKAGE`, listas de uso, exportação, consulta exata do nome do símbolo, estado de consulta, símbolos qualificados e persistência em imagem funcionam; faltam importação, shadowing, apelidos e uninterning. |
| Streams | ◐ | Streams padrão/de arquivo e I/O textual básico funcionam. Faltam streams compostos, element types, semântica de pathnames e o protocolo completo de designadores de stream. |
| Condições | ◐ | Objetos de condição, `ERROR`, `HANDLER-CASE` e `IGNORE-ERRORS` funcionam; faltam restarts, binding de handlers, hierarquia padrão completa e integração com debugger. |
| CLOS e MOP | — | Classes, funções genéricas, combinação de métodos e protocolo de metaobjetos não estão implementados. |
| Compilação | ◐ | `COMPILE` atende ao subconjunto i64 documentado por meio da IR e dos backends nativos próprios; faltam compilação Lisp geral e ambientes de compilação ANSI. |

## Contratos verificados de valores múltiplos

| Operador | Resultado verificado |
| --- | --- |
| `GETHASH` | valor e indicador booleano generalizado de presença |
| `READ-LINE` | linha/eof-value e `missing-newline-p`; as quatro posições opcionais padrão |
| `INTERN` | símbolo mais `NIL`, `:INTERNAL`, `:EXTERNAL` ou `:INHERITED` |
| `FIND-SYMBOL` | símbolo acessível e estado, ou dois valores `NIL` |
| `IGNORE-ERRORS` | valores normais, ou `NIL` e a condição capturada |

## Desvios conhecidos relevantes para este relatório

- EOF imediato em `READ-LINE` com `eof-error-p` verdadeiro sinaliza a condição
  geral inicial `ERROR`, ainda não uma classe distinta `END-OF-FILE`.
- Argumentos de stream aceitam atualmente objetos stream e o padrão omitido;
  nem todos os designadores de stream ANSI são aceitos.
- Designadores de nome de package ainda dobram caixa ASCII como extensão de
  compatibilidade; a semântica ANSI completa de nomes e apelidos está pendente.
- `CONSTANTP` reconhece os objetos autoavaliáveis obrigatórios, símbolos
  constantes e formas `QUOTE` no ambiente nulo; objetos de ambiente não nulo
  ainda não são expostos.
- Símbolos keyword são externos, autoavaliáveis, vinculados a si próprios e
  protegidos contra atribuição, mas as property lists de símbolos estão pendentes.

Os testes da implementação ficam em `testes/teste_nucleo.c`, com cobertura
direcionada da migração v6 em `testes/teste_imagem_legada.c`; o trabalho mais
amplo restante está no [roteiro para 1.0](roteiro.md). Os contratos corrigidos
são comparados às entradas do Common Lisp HyperSpec para
[`READ-LINE`](https://www.lispworks.com/documentation/HyperSpec/Body/f_rd_lin.htm),
[`INTERN`](https://www.lispworks.com/documentation/HyperSpec/Body/f_intern.htm),
[`FIND-SYMBOL`](https://www.lispworks.com/documentation/HyperSpec/Body/f_find_s.htm),
[`SYMBOLP`](https://www.lispworks.com/documentation/HyperSpec/Body/f_symbol.htm),
[`SYMBOL-NAME`](https://www.lispworks.com/documentation/HyperSpec/Body/f_symb_2.htm),
[`SYMBOL-PACKAGE`](https://www.lispworks.com/documentation/HyperSpec/Body/f_symb_3.htm)
e
[`CONSTANTP`](https://www.lispworks.com/documentation/HyperSpec/Body/f_consta.htm).
