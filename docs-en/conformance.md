# ANSI Common Lisp conformance

This report describes the executable bootstrap, not the intended 1.0. A
feature is called verified only when its observable contract has an automated
test. Sefirah does not yet claim to be a conforming ANSI Common Lisp
implementation.

## Status legend

| Mark | Meaning |
| --- | --- |
| ✅ | Implemented and covered by automated tests for the stated scope |
| ◐ | A useful subset exists, with material ANSI behavior still missing |
| — | Not implemented |

## Language areas

| Area | Status | Current evidence and boundary |
| --- | --- | --- |
| Reader and printer | ◐ | Lists, dotted lists, strings, numbers, characters, vectors, escaped symbols, quote, function quote, and quasiquote work; readtables, radix syntax, and circular notation are missing. |
| Evaluation and binding | ◐ | Lexical functions, macros, local functions, basic special variables, generalized places, and non-local control work; declarations and the full lambda-list protocol are missing. |
| Symbols | ◐ | `NIL` has its external `COMMON-LISP` symbol identity. Tested contracts include inquiries, separate mutable cells, unbinding, persistent property lists, fresh uninterned `MAKE-SYMBOL`/`COPY-SYMBOL` identities, and `GENSYM` with `*GENSYM-COUNTER*`; symbol macros, `GENTEMP`, and complete function-name designators remain missing. |
| Multiple values | ✅ | The implemented multiple-value forms preserve values across calls, cleanup, and non-local transfer. `GETHASH`, `READ-LINE`, `INTERN`, and `FIND-SYMBOL` expose their tested secondary values. |
| Lists and sequences | ◐ | Core list operations and an initial sequence protocol cover lists, vectors, and UTF-8 strings; keyword arguments and the complete sequence function family are missing. |
| Numbers | ◐ | Fixed-width integers and host doubles exist; bignums, ratios, complex numbers, exact overflow behavior, and the full numeric library are missing. |
| Packages | ◐ | Heap packages, phased `DEFPACKAGE` symbol options, reversible use/export topology and inquiries, import/unintern, explicit shadowing and shadowing-import, exact lookup status, qualified symbols, uninterned-symbol adoption, conflict protection, and image persistence work; nicknames, package deletion, and the remaining ANSI package family are missing. |
| Streams | ◐ | Standard/file streams and basic text I/O work. Composite streams, element types, pathname semantics, and the complete stream designator protocol are missing. |
| Conditions | ◐ | Condition objects, `ERROR`, `SIGNAL`, dynamically scoped `HANDLER-BIND`, `HANDLER-CASE`, `IGNORE-ERRORS`, first-class restart objects, dynamic discovery/invocation, and five standard named-restart helpers work; condition association, standard hierarchy coverage, and suspendable debugger integration are missing. |
| CLOS and MOP | — | Classes, generic functions, method combination, and the metaobject protocol are not implemented. |
| Compilation | ◐ | `COMPILE` handles the documented i64 subset through Sefirah's own IR and native backends; general Lisp compilation and ANSI compilation environments are missing. |

## Verified multiple-value contracts

| Operator | Verified result |
| --- | --- |
| `GETHASH` | value and generalized-boolean presence flag |
| `READ-LINE` | line/eof-value and `missing-newline-p`; the four standard optional argument positions |
| `INTERN` | symbol plus `NIL`, `:INTERNAL`, `:EXTERNAL`, or `:INHERITED` |
| `FIND-SYMBOL` | accessible symbol and status, or two `NIL` values |
| `IGNORE-ERRORS` | normal values, or `NIL` and the captured condition |

## Known deviations relevant to this report

- Immediate EOF from `READ-LINE` with `eof-error-p` true signals the initial
  general `ERROR` condition, not a distinct `END-OF-FILE` class yet.
- Stream arguments currently accept stream objects and the omitted default;
  all ANSI stream designators are not supported.
- Package-name designators currently fold ASCII case as a compatibility
  extension; complete ANSI package-name and nickname behavior is pending.
- `CONSTANTP` recognizes the required self-evaluating objects, constant symbols,
  and `QUOTE` forms in the null environment; non-null environment objects are
  not exposed yet.
- Keyword symbols are external, self-evaluating, bound to themselves, and
  protected from value-cell assignment. Property lists remain mutable as ANSI
  metadata and persist with the image.
- `FDEFINITION`, `FMAKUNBOUND`, and writable function-cell access currently
  accept symbol function names; compound `(SETF name)` function names remain
  pending.
- `FIND-RESTART` and `COMPUTE-RESTARTS` return first-class `RESTART` objects;
  name and object designators preserve identity and inactive objects remain
  inspectable but cannot be invoked. Condition association and the `:REPORT`,
  `:TEST`, and `:INTERACTIVE` `RESTART-CASE` options are not implemented yet.
- `SIGNAL` and `ERROR` currently accept a condition object or a single string.
  The full condition-designator argument protocol and standard condition type
  hierarchy remain pending.

The implementation tests are in `testes/teste_nucleo.c`, with targeted v6
migration coverage in `testes/teste_imagem_legada.c`; the broader remaining
work is tracked in the [1.0 roadmap](roadmap.md). The corrected contracts are
compared against the Common Lisp HyperSpec entries for
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
[`RESTART-NAME`](https://www.lispworks.com/documentation/HyperSpec/Body/f_rst_na.htm),
and
[`RESTART-CASE`](https://www.lispworks.com/documentation/HyperSpec/Body/m_rst_ca.htm).
