#include "sefirah/runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int falhas = 0;

static void verificar(bool condicao, const char *mensagem) {
    if (!condicao) {
        fprintf(stderr, "FALHOU: %s\n", mensagem);
        falhas++;
    }
}

static SefValor avaliar(SefRuntime *runtime, const char *codigo) {
    SefErro erro;
    SefValor valor = sef_runtime_avaliar_texto(runtime, codigo, &erro);
    if (valor == NULL) {
        fprintf(stderr, "avaliacao falhou para %s: %s\n", codigo, erro.mensagem);
        falhas++;
    }
    return valor;
}

static void verificar_texto(SefRuntime *runtime, const char *codigo, const char *esperado) {
    SefValor valor = avaliar(runtime, codigo);
    if (valor == NULL)
        return;
    SefErro erro;
    char *texto = sef_valor_para_texto(runtime, valor, true, &erro);
    verificar(texto != NULL, "impressor devolveu texto");
    if (texto != NULL) {
        if (strcmp(texto, esperado) != 0) {
            fprintf(stderr, "FALHOU: %s => %s; esperado %s\n", codigo, texto, esperado);
            falhas++;
        }
        sef_texto_liberar(texto);
    }
}

int main(void) {
    SefErro erro;
    SefRuntime *runtime = sef_runtime_criar(&erro);
    if (runtime == NULL) {
        fprintf(stderr, "runtime nao iniciou: %s\n", erro.mensagem);
        return 1;
    }

    verificar_texto(runtime, "(+ 1 2 39)", "42");
    verificar_texto(runtime, "(list (<= 1 1 2) (>= 3 2 2) (/= 1 2 3) (/= 1 2 1))", "(T T T NIL)");
    verificar_texto(runtime, "'(a b . c)", "(A B . C)");
    verificar_texto(runtime, "(let ((x 20) (y 22)) (+ x y))", "42");
    verificar_texto(runtime, "(defun fat (n) (if (< n 2) 1 (* n (fat (- n 1))))) (fat 6)", "720");
    verificar_texto(runtime,
                    "(defmacro primeiro-quando (teste valor) (list 'if teste valor nil)) "
                    "(primeiro-quando t (+ 40 2))",
                    "42");
    verificar_texto(runtime,
                    "(defmacro incrementar (lugar) `(setq ,lugar (+ ,lugar 1))) "
                    "(let ((x 41)) (incrementar x) x)",
                    "42");
    verificar_texto(runtime,
                    "(defmacro fazer-lista (&rest itens) `(list ,@itens)) "
                    "(fazer-lista 40 41 42)",
                    "(40 41 42)");
    verificar_texto(runtime, "((lambda (x &rest xs) (length xs)) 1 2 3 4)", "3");
    verificar_texto(runtime, "(type-of \"sefirah\")", "STRING");
    verificar_texto(runtime,
                    "(define celula-separada 41) "
                    "(defun celula-separada () 42) "
                    "(list celula-separada (celula-separada))",
                    "(41 42)");
    verificar_texto(runtime, "(funcall #'+ 20 22)", "42");
    verificar_texto(runtime,
                    "(list (boundp 'celula-separada) "
                    "(fboundp 'celula-separada) (functionp #'+))",
                    "(T T T)");
    verificar_texto(runtime, "(let ((x 2)) (let* ((x 20) (y (+ x 22))) y))", "42");
    verificar_texto(runtime, "(cond ((> 1 2) 0) ((< 1 2) 42) (t 99))", "42");
    verificar_texto(runtime, "(flet ((dobro (x) (* x 2))) (dobro 21))", "42");
    verificar_texto(runtime,
                    "(labels ((soma-ate (n) (if (< n 1) 0 (+ n (soma-ate (- n 1)))))) "
                    "(soma-ate 9))",
                    "45");
    verificar_texto(runtime, "(list (when t 42) (unless nil 43) :palavra-chave)",
                    "(42 43 :PALAVRA-CHAVE)");
    verificar_texto(runtime, "(block pronto (+ 1 (return-from pronto 42) 100))", "42");
    verificar_texto(runtime, "(block nil (return 42) 99)", "42");
    verificar_texto(runtime, "(catch 'fim (+ 1 (throw 'fim 42) 100))", "42");
    verificar_texto(runtime,
                    "(let ((limpo nil)) "
                    "(list (block fim (unwind-protect (return-from fim 42) "
                    "(setq limpo t))) limpo))",
                    "(42 T)");
    verificar_texto(runtime, "(ignore-errors (+ 1 simbolo-inexistente))", "NIL");
    verificar_texto(runtime,
                    "(handler-case (error \"falha controlada\") "
                    "(error (condicao) (list (type-of condicao) condicao)))",
                    "(ERROR #<ERROR falha controlada>)");
    SefValor transiente = avaliar(runtime, "(list 'objeto 'retido 42)");
    SefRaiz *raiz = transiente == NULL ? NULL : sef_raiz_criar(runtime, transiente, &erro);
    verificar(raiz != NULL, "handle de raiz foi criado");
    verificar_texto(runtime, "(list 1 2 3 4 5)", "(1 2 3 4 5)");
    if (raiz != NULL) {
        char *texto_retido = sef_valor_para_texto(runtime, sef_raiz_valor(raiz), true, &erro);
        verificar(texto_retido != NULL && strcmp(texto_retido, "(OBJETO RETIDO 42)") == 0,
                  "raiz preservou valor entre avaliacoes");
        sef_texto_liberar(texto_retido);
        sef_raiz_liberar(raiz);
    }
    verificar_texto(runtime,
                    "(define condicao-salva "
                    "(handler-case (error \"persistente\") (error (c) c)))",
                    "CONDICAO-SALVA");
    verificar_texto(runtime,
                    "(defpackage :alpha (:use :common-lisp) "
                    "(:export :resposta-do-pacote)) "
                    "(in-package :alpha) "
                    "(defun resposta-do-pacote () (+ 40 2)) "
                    "(in-package :common-lisp-user) "
                    "(list (alpha:resposta-do-pacote) "
                    "(package-name (find-package \"alpha\")) "
                    "(find-symbol \"RESPOSTA-DO-PACOTE\" \"ALPHA\"))",
                    "(42 \"ALPHA\" ALPHA:RESPOSTA-DO-PACOTE)");
    verificar_texto(runtime,
                    "(list (eq (intern \"X\" \"ALPHA\") "
                    "(intern \"X\" \"COMMON-LISP-USER\")) "
                    "(packagep *package*) :chave)",
                    "(NIL T :CHAVE)");
    verificar_texto(runtime, "(intern \"PRIVADO\" \"ALPHA\")", "ALPHA::PRIVADO");
    SefValor privado = sef_runtime_avaliar_texto(runtime, "alpha:privado", &erro);
    verificar(privado == NULL && erro.ocorreu, "dois-pontos simples rejeita simbolo interno");
    verificar_texto(runtime,
                    "(make-package :origem-a) (make-package :origem-b) "
                    "(make-package :destino) "
                    "(export (intern \"CHOQUE\" \"ORIGEM-A\") \"ORIGEM-A\") "
                    "(export (intern \"CHOQUE\" \"ORIGEM-B\") \"ORIGEM-B\") "
                    "(use-package \"ORIGEM-A\" \"DESTINO\") "
                    "(handler-case (use-package \"ORIGEM-B\" \"DESTINO\") "
                    "(error (c) :conflito-detectado))",
                    ":CONFLITO-DETECTADO");
    verificar_texto(runtime,
                    "(defun calcular-compilado (x y) "
                    "(if (< x y) (+ (* x 2) y) (- x y)))",
                    "CALCULAR-COMPILADO");
    SefFuncaoCompilada *compilada =
        sef_runtime_compilar_funcao_i64(runtime, "CALCULAR-COMPILADO", &erro);
    verificar(compilada != NULL, "funcao Lisp foi baixada para IR e codigo nativo");
    if (compilada != NULL) {
        int64_t argumentos_compilados[2] = {10, 22};
        int64_t resultado_compilado = 0;
        verificar(sef_funcao_compilada_executar_i64(compilada, argumentos_compilados, 2,
                                                    &resultado_compilado, &erro) &&
                      resultado_compilado == 42,
                  "funcao Lisp compilada executou consequente");
        argumentos_compilados[0] = 50;
        argumentos_compilados[1] = 8;
        verificar(sef_funcao_compilada_executar_i64(compilada, argumentos_compilados, 2,
                                                    &resultado_compilado, &erro) &&
                      resultado_compilado == 42,
                  "funcao Lisp compilada executou alternativa");
        sef_funcao_compilada_liberar(compilada);
    }
    verificar_texto(runtime,
                    "(list (compile 'calcular-compilado) "
                    "(calcular-compilado 10 22) "
                    "(calcular-compilado 50 8) "
                    "(funcall #'calcular-compilado 10 22) "
                    "(apply #'calcular-compilado '(50 8)) "
                    "(type-of #'calcular-compilado) "
                    "(compiled-function-p #'calcular-compilado))",
                    "(CALCULAR-COMPILADO 42 42 42 42 COMPILED-FUNCTION T)");
    verificar_texto(runtime,
                    "(handler-case (calcular-compilado 1.5 2) "
                    "(error (c) :tipo-compilado-rejeitado))",
                    ":TIPO-COMPILADO-REJEITADO");
    verificar_texto(runtime, "(defun operacao-nao-compilavel (x) (print x))",
                    "OPERACAO-NAO-COMPILAVEL");
    compilada = sef_runtime_compilar_funcao_i64(runtime, "OPERACAO-NAO-COMPILAVEL", &erro);
    verificar(compilada == NULL && erro.ocorreu,
              "frontend recusou operacao fora do subconjunto i64");
    verificar_texto(runtime,
                    "(list (streamp *standard-input*) (streamp *standard-output*) "
                    "(type-of *error-output*))",
                    "(T T STREAM)");
    verificar_texto(runtime,
                    "(define fluxo-saida "
                    "(open \"teste-stream-sefirah.txt\" :direction :output "
                    ":if-exists :supersede)) "
                    "(write-string \"linha 42\" fluxo-saida) "
                    "(terpri fluxo-saida) (finish-output fluxo-saida) "
                    "(close fluxo-saida)",
                    "T");
    verificar_texto(runtime,
                    "(define fluxo-entrada (open \"teste-stream-sefirah.txt\")) "
                    "(list (read-line fluxo-entrada) (read-line fluxo-entrada) "
                    "(close fluxo-entrada))",
                    "(\"linha 42\" NIL T)");
    verificar_texto(runtime,
                    "(define fluxo-imagem "
                    "(open \"teste-stream-imagem.txt\" :direction :output))",
                    "FLUXO-IMAGEM");
    verificar(!sef_runtime_imagem_salvar(runtime, "imagem-nao-deve-existir.imagem", &erro) &&
                  erro.ocorreu,
              "imagem rejeita stream de arquivo aberto");
    verificar_texto(runtime, "(close fluxo-imagem)", "T");
    remove("imagem-nao-deve-existir.imagem");
    remove("teste-stream-imagem.txt");
    remove("teste-stream-sefirah.txt");

    SefValor invalido = sef_runtime_avaliar_texto(runtime, "(+ 1 desconhecido)", &erro);
    verificar(invalido == NULL && erro.ocorreu, "erro de simbolo nao vinculado");
    verificar(sef_runtime_objetos_vivos(runtime) > 0, "runtime possui objetos vivos");

    verificar_texto(runtime,
                    "(define base-da-imagem 40) "
                    "(defun usar-imagem (x) (+ base-da-imagem x))",
                    "USAR-IMAGEM");
    verificar(sef_runtime_imagem_salvar(runtime, "teste-sefirah.imagem", &erro),
              "imagem foi salva");
    sef_runtime_destruir(runtime);
    runtime = sef_runtime_imagem_abrir("teste-sefirah.imagem", &erro);
    verificar(runtime != NULL, "imagem foi reaberta");
    if (runtime != NULL)
        verificar_texto(runtime, "(usar-imagem 2)", "42");
    if (runtime != NULL)
        verificar_texto(runtime, "condicao-salva", "#<ERROR persistente>");
    if (runtime != NULL)
        verificar_texto(runtime, "(alpha:resposta-do-pacote)", "42");
    if (runtime != NULL)
        verificar_texto(runtime,
                        "(list (calcular-compilado 10 22) "
                        "(type-of #'calcular-compilado))",
                        "(42 FUNCTION)");
    if (runtime != NULL)
        verificar_texto(runtime,
                        "(compile 'calcular-compilado) "
                        "(list (calcular-compilado 50 8) "
                        "(compiled-function-p #'calcular-compilado))",
                        "(42 T)");
    if (runtime != NULL)
        verificar_texto(runtime,
                        "(list (streamp *standard-input*) (streamp *standard-output*) "
                        "(streamp *error-output*))",
                        "(T T T)");
    remove("teste-sefirah.imagem");

    sef_runtime_destruir(runtime);
    if (falhas == 0)
        puts("nucleo: todos os testes passaram");
    return falhas == 0 ? 0 : 1;
}
