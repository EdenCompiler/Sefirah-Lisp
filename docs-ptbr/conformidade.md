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
| Símbolos | ◐ | `NIL` possui sua identidade de símbolo externo de `COMMON-LISP`. Os contratos testados incluem consultas, células mutáveis separadas, unbinding, property lists persistentes, identidades novas e não internadas por `MAKE-SYMBOL`/`COPY-SYMBOL` e `GENSYM` com `*GENSYM-COUNTER*`; ainda faltam macros de símbolo, `GENTEMP` e designadores completos de nomes de função. |
| Valores múltiplos | ✅ | As formas implementadas preservam valores entre chamadas, limpeza e transferência não local. `GETHASH`, `READ-LINE`, `INTERN` e `FIND-SYMBOL` expõem seus valores secundários testados. |
| Listas e sequências | ◐ | Operações centrais de listas e um protocolo inicial de sequências cobrem listas, vetores e strings UTF-8; faltam argumentos keyword e a família completa de funções de sequência. |
| Números | ◐ | Existem inteiros de largura fixa e doubles do hospedeiro; faltam bignums, razões, complexos, overflow exato e a biblioteca numérica completa. |
| Packages | ◐ | Packages no heap, apelidos persistentes e renomeação, opções de símbolo em fases no `DEFPACKAGE`, topologia reversível e consultas de uso/exportação, importação/uninterning, shadowing explícito e shadowing-import, consulta exata e seu estado, símbolos qualificados, adoção de símbolos não internados, proteção contra conflitos e persistência funcionam; faltam remoção de packages e o restante da família ANSI. |
| Streams | ◐ | Streams padrão/de arquivo e I/O textual básico funcionam. Faltam streams compostos, element types, semântica de pathnames e o protocolo completo de designadores de stream. |
| Condições | ◐ | Objetos de condição, `ERROR`, `SIGNAL`, `HANDLER-BIND` com escopo dinâmico, `HANDLER-CASE`, `IGNORE-ERRORS`, objetos restart de primeira classe, descoberta/invocação dinâmica e cinco auxiliares padrão de restart nomeado funcionam; faltam associação por condição, hierarquia padrão completa e integração com debugger suspensível. |
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
  protegidos contra atribuição da célula de valor. As property lists continuam
  mutáveis como metadados ANSI e persistem na imagem.
- `FDEFINITION`, `FMAKUNBOUND` e a gravação da célula de função aceitam neste
  estágio nomes de função que sejam símbolos; nomes compostos `(SETF nome)`
  ainda estão pendentes.
- `FIND-RESTART` e `COMPUTE-RESTARTS` devolvem objetos `RESTART` de primeira
  classe; designadores por nome e objeto preservam identidade, e objetos
  inativos continuam inspecionáveis, mas não podem ser invocados. Associação a
  condições e as opções `:REPORT`, `:TEST` e `:INTERACTIVE` de `RESTART-CASE`
  ainda não foram implementadas.
- `SIGNAL` e `ERROR` aceitam atualmente um objeto condição ou uma única string.
  O protocolo completo de argumentos designadores de condição e a hierarquia
  padrão de tipos continuam pendentes.

Os testes da implementação ficam em `testes/teste_nucleo.c`, com cobertura
direcionada da migração v6 em `testes/teste_imagem_legada.c`; o trabalho mais
amplo restante está no [roteiro para 1.0](roteiro.md). Os contratos corrigidos
são comparados às entradas do Common Lisp HyperSpec para
[`READ-LINE`](https://www.lispworks.com/documentation/HyperSpec/Body/f_rd_lin.htm),
[`INTERN`](https://www.lispworks.com/documentation/HyperSpec/Body/f_intern.htm),
[`FIND-SYMBOL`](https://www.lispworks.com/documentation/HyperSpec/Body/f_find_s.htm),
[`SYMBOLP`](https://www.lispworks.com/documentation/HyperSpec/Body/f_symbol.htm),
[`SYMBOL-NAME`](https://www.lispworks.com/documentation/HyperSpec/Body/f_symb_2.htm),
[`SYMBOL-PACKAGE`](https://www.lispworks.com/documentation/HyperSpec/Body/f_symb_3.htm),
[`CONSTANTP`](https://www.lispworks.com/documentation/HyperSpec/Body/f_consta.htm),
[`HANDLER-BIND`](https://www.lispworks.com/documentation/HyperSpec/Body/m_handle.htm),
[`SIGNAL`](https://www.lispworks.com/documentation/HyperSpec/Body/f_signal.htm),
[`COMPUTE-RESTARTS`](https://www.lispworks.com/documentation/HyperSpec/Body/f_comp_1.htm),
[`INVOKE-RESTART`](https://www.lispworks.com/documentation/HyperSpec/Body/f_invo_1.htm),
[`RESTART-NAME`](https://www.lispworks.com/documentation/HyperSpec/Body/f_rst_na.htm)
e
[`RESTART-CASE`](https://www.lispworks.com/documentation/HyperSpec/Body/m_rst_ca.htm).
