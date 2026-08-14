#include "ide/ide.h"
#include "sefirah/runtime.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

static int falhas = 0;

static void verificar(bool condicao, const char *mensagem) {
    if (!condicao) {
        fprintf(stderr, "FAILED: %s\n", mensagem);
        falhas++;
    }
}

static bool texto_contem(const char *texto, const char *trecho) {
    return texto != NULL && strstr(texto, trecho) != NULL;
}

static bool texto_contem_portugues(const char *texto) {
    static const char *marcadores[] = {
        " memoria ", " nao ",      " exige ",   " invalido",  " invalida", " simbolo", " funcao",
        " arquivo",  " caminho",   " condicao", " reinicio",  " nenhum",   " nenhuma", " erro:",
        " falha",    " esperava ", " recebeu ", " argumento", " codigo",   " objeto",
    };
    char normalizado[sizeof(((SefErro *)0)->mensagem) + 3];
    size_t tamanho = texto == NULL ? 0 : strlen(texto);
    if (tamanho > sizeof(((SefErro *)0)->mensagem))
        tamanho = sizeof(((SefErro *)0)->mensagem);
    normalizado[0] = ' ';
    for (size_t i = 0; i < tamanho; i++)
        normalizado[i + 1] = (char)tolower((unsigned char)texto[i]);
    normalizado[tamanho + 1] = ' ';
    normalizado[tamanho + 2] = '\0';
    for (size_t i = 0; i < sizeof(marcadores) / sizeof(marcadores[0]); i++) {
        if (strstr(normalizado, marcadores[i]) != NULL)
            return true;
    }
    return false;
}

int main(void) {
    SefErro erro;
    SefRuntime *runtime = sef_runtime_criar(&erro);
    verificar(runtime != NULL, "runtime starts for language-policy checks");
    if (runtime != NULL) {
        SefValor resultado = sef_runtime_avaliar_texto(runtime, "(+ 1 'word)", &erro);
        verificar(resultado == NULL && erro.ocorreu && texto_contem(erro.mensagem, "numbers") &&
                      !texto_contem_portugues(erro.mensagem),
                  "evaluator diagnostics are English");

        resultado = sef_runtime_avaliar_texto(runtime, "(list 1", &erro);
        verificar(resultado == NULL && erro.ocorreu && texto_contem(erro.mensagem, "closing") &&
                      !texto_contem_portugues(erro.mensagem),
                  "reader diagnostics are English");

        SefFuncaoCompilada *funcao =
            sef_runtime_compilar_funcao_i64(runtime, "missing-function", &erro);
        verificar(funcao == NULL && erro.ocorreu && texto_contem(erro.mensagem, "compilable") &&
                      !texto_contem_portugues(erro.mensagem),
                  "compiler diagnostics are English");

        resultado = sef_runtime_avaliar_texto(runtime, "(makunbound nil)", &erro);
        verificar(resultado == NULL && erro.ocorreu &&
                      texto_contem(erro.mensagem, "constant symbol") &&
                      !texto_contem_portugues(erro.mensagem),
                  "symbol binding diagnostics are English");

        resultado =
            sef_runtime_avaliar_texto(runtime, "(setf (symbol-plist 'item) (list :odd))", &erro);
        verificar(resultado == NULL && erro.ocorreu &&
                      texto_contem(erro.mensagem, "indicator/value pairs") &&
                      !texto_contem_portugues(erro.mensagem),
                  "symbol property diagnostics are English");

        resultado = sef_runtime_avaliar_texto(runtime, "(gensym -1)", &erro);
        verificar(resultado == NULL && erro.ocorreu &&
                      texto_contem(erro.mensagem, "non-negative integer") &&
                      !texto_contem_portugues(erro.mensagem),
                  "uninterned symbol diagnostics are English");

        resultado = sef_runtime_avaliar_texto(runtime, "(unintern 'car \"COMMON-LISP\")", &erro);
        verificar(resultado == NULL && erro.ocorreu && texto_contem(erro.mensagem, "locked") &&
                      !texto_contem_portugues(erro.mensagem),
                  "package mutation diagnostics are English");

        resultado = sef_runtime_avaliar_texto(runtime, "(shadow 42)", &erro);
        verificar(resultado == NULL && erro.ocorreu &&
                      texto_contem(erro.mensagem, "string designator") &&
                      !texto_contem_portugues(erro.mensagem),
                  "package shadowing diagnostics are English");

        resultado = sef_runtime_avaliar_texto(
            runtime, "(defpackage :broken-definition (:import-from :missing-package \"X\"))",
            &erro);
        verificar(resultado == NULL && erro.ocorreu &&
                      texto_contem(erro.mensagem, "source package does not exist") &&
                      !texto_contem_portugues(erro.mensagem),
                  "DEFPACKAGE import diagnostics are English");

        resultado = sef_runtime_avaliar_texto(runtime, "(unexport 'car \"COMMON-LISP\")", &erro);
        verificar(resultado == NULL && erro.ocorreu &&
                      texto_contem(erro.mensagem, "package is locked") &&
                      !texto_contem_portugues(erro.mensagem),
                  "package topology mutation diagnostics are English");

        resultado = sef_runtime_avaliar_texto(
            runtime, "(make-package \"COLLIDING-NICKNAME\" :nicknames '(\"COMMON-LISP\"))", &erro);
        verificar(resultado == NULL && erro.ocorreu &&
                      texto_contem(erro.mensagem, "nickname COMMON-LISP is already in use") &&
                      !texto_contem_portugues(erro.mensagem),
                  "package nickname diagnostics are English");

        resultado = sef_runtime_avaliar_texto(runtime, "*standard-input*", &erro);
        char *impresso =
            resultado == NULL ? NULL : sef_valor_para_texto(runtime, resultado, true, &erro);
        verificar(impresso != NULL && texto_contem(impresso, "STANDARD-STREAM") &&
                      !texto_contem_portugues(impresso),
                  "runtime object descriptions are English");
        sef_texto_liberar(impresso);
        sef_runtime_destruir(runtime);
    }

    SefSessaoIde *sessao = sef_sessao_ide_criar(&erro);
    verificar(sessao != NULL, "IDE session starts for language-policy checks");
    if (sessao != NULL) {
        verificar(texto_contem(sef_sessao_ide_inspetor(sessao), "OBJECTS") &&
                      texto_contem(sef_sessao_ide_navegador(sessao), "DEFINITIONS") &&
                      texto_contem(sef_sessao_ide_depurador(sessao), "CONDITIONS") &&
                      texto_contem(sef_sessao_ide_explorador(sessao), "NO FOLDER OPEN"),
                  "IDE panels use English labels");
        verificar(!sef_sessao_ide_editor_selecionar_forma(sessao, &erro) && erro.ocorreu &&
                      texto_contem(erro.mensagem, "complete Lisp form") &&
                      !texto_contem_portugues(erro.mensagem),
                  "IDE diagnostics are English");
        verificar(!sef_sessao_ide_diretorio_criar(sessao, "", &erro) && erro.ocorreu &&
                      texto_contem(erro.mensagem, "missing path") &&
                      !texto_contem_portugues(erro.mensagem),
                  "IDE file and folder diagnostics are English");
        sef_sessao_ide_destruir(sessao);
    }

    if (falhas == 0)
        puts("english output: all tests passed");
    return falhas == 0 ? 0 : 1;
}
