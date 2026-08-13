#include "sefirah/runtime.h"

#include <stdio.h>
#include <string.h>

static void uso(FILE *saida) {
    fputs("Usage: sefirah <command> [arguments]\n"
          "\n"
          "Commands:\n"
          "  repl                 open the text REPL\n"
          "  evaluate <form>      read and evaluate one or more forms\n"
          "  run <file>           execute a .lisp file\n"
          "  compile-elf <file.lisp> <function> <output.o>\n"
          "  compile-coff <file.lisp> <function> <output.obj>\n"
          "  compile-macho <file.lisp> <function> <output.o>\n"
          "  image save <image> <file.lisp>\n"
          "  image open <image> [form]\n"
          "  version              show the version\n",
          saida);
}

static int mostrar_erro(const SefErro *erro) {
    if (erro->linha > 0) {
        fprintf(stderr, "Error at %zu:%zu: %s\n", erro->linha, erro->coluna, erro->mensagem);
    } else {
        fprintf(stderr, "Error: %s\n", erro->mensagem);
    }
    return 1;
}

static int imprimir_resultado(SefRuntime *runtime, SefValor valor) {
    SefErro erro;
    char *texto = sef_valor_para_texto(runtime, valor, true, &erro);
    if (texto == NULL)
        return mostrar_erro(&erro);
    puts(texto);
    sef_texto_liberar(texto);
    return 0;
}

static bool caminho_e_lisp(const char *caminho) {
    size_t tamanho = strlen(caminho);
    return tamanho >= 5 && strcmp(caminho + tamanho - 5, ".lisp") == 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        uso(stderr);
        return 2;
    }
    if (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "versao") == 0 ||
        strcmp(argv[1], "--version") == 0) {
        puts("Sefirah Lisp 0.0.1 (bootstrap C17)");
        return 0;
    }
    if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "ajuda") == 0 ||
        strcmp(argv[1], "--help") == 0) {
        uso(stdout);
        return 0;
    }

    SefErro erro;
    SefRuntime *runtime = sef_runtime_criar(&erro);
    if (runtime == NULL)
        return mostrar_erro(&erro);
    int resultado = 0;
    if (strcmp(argv[1], "repl") == 0) {
        resultado = sef_runtime_repl(runtime, stdin, stdout);
    } else if (strcmp(argv[1], "evaluate") == 0 || strcmp(argv[1], "avaliar") == 0) {
        if (argc != 3) {
            fputs("evaluate requires a source-code string\n", stderr);
            resultado = 2;
        } else {
            SefValor valor = sef_runtime_avaliar_texto(runtime, argv[2], &erro);
            resultado = valor == NULL ? mostrar_erro(&erro) : imprimir_resultado(runtime, valor);
        }
    } else if (strcmp(argv[1], "run") == 0 || strcmp(argv[1], "executar") == 0) {
        if (argc != 3) {
            fputs("run requires a file path\n", stderr);
            resultado = 2;
        } else if (!caminho_e_lisp(argv[2])) {
            fputs("Sefirah source files must use the .lisp extension\n", stderr);
            resultado = 2;
        } else {
            SefValor valor = NULL;
            resultado = sef_runtime_executar_arquivo(runtime, argv[2], &valor, &erro)
                            ? imprimir_resultado(runtime, valor)
                            : mostrar_erro(&erro);
        }
    } else if (strcmp(argv[1], "compile-elf") == 0 || strcmp(argv[1], "compilar-elf") == 0) {
        if (argc != 5) {
            fputs("compile-elf requires file.lisp, a function name, and output.o\n", stderr);
            resultado = 2;
        } else if (!caminho_e_lisp(argv[2])) {
            fputs("Sefirah source files must use the .lisp extension\n", stderr);
            resultado = 2;
        } else {
            SefValor ignorado = NULL;
            SefFuncaoCompilada *funcao = NULL;
            if (!sef_runtime_executar_arquivo(runtime, argv[2], &ignorado, &erro) ||
                (funcao = sef_runtime_compilar_objeto_i64(runtime, argv[3], &erro)) == NULL ||
                !sef_funcao_compilada_gravar_elf(funcao, argv[3], argv[4], &erro)) {
                resultado = mostrar_erro(&erro);
            } else {
                printf("ELF object saved to %s\n", argv[4]);
            }
            sef_funcao_compilada_liberar(funcao);
        }
    } else if (strcmp(argv[1], "compile-coff") == 0 || strcmp(argv[1], "compilar-coff") == 0) {
        if (argc != 5) {
            fputs("compile-coff requires file.lisp, a function name, and output.obj\n", stderr);
            resultado = 2;
        } else if (!caminho_e_lisp(argv[2])) {
            fputs("Sefirah source files must use the .lisp extension\n", stderr);
            resultado = 2;
        } else {
            SefValor ignorado = NULL;
            SefFuncaoCompilada *funcao = NULL;
            if (!sef_runtime_executar_arquivo(runtime, argv[2], &ignorado, &erro) ||
                (funcao = sef_runtime_compilar_objeto_i64(runtime, argv[3], &erro)) == NULL ||
                !sef_funcao_compilada_gravar_coff(funcao, argv[3], argv[4], &erro)) {
                resultado = mostrar_erro(&erro);
            } else {
                printf("COFF object saved to %s\n", argv[4]);
            }
            sef_funcao_compilada_liberar(funcao);
        }
    } else if (strcmp(argv[1], "compile-macho") == 0 || strcmp(argv[1], "compilar-macho") == 0) {
        if (argc != 5) {
            fputs("compile-macho requires file.lisp, a function name, and output.o\n", stderr);
            resultado = 2;
        } else if (!caminho_e_lisp(argv[2])) {
            fputs("Sefirah source files must use the .lisp extension\n", stderr);
            resultado = 2;
        } else {
            SefValor ignorado = NULL;
            SefFuncaoCompilada *funcao = NULL;
            if (!sef_runtime_executar_arquivo(runtime, argv[2], &ignorado, &erro) ||
                (funcao = sef_runtime_compilar_objeto_i64(runtime, argv[3], &erro)) == NULL ||
                !sef_funcao_compilada_gravar_macho(funcao, argv[3], argv[4], &erro)) {
                resultado = mostrar_erro(&erro);
            } else {
                printf("Mach-O object saved to %s\n", argv[4]);
            }
            sef_funcao_compilada_liberar(funcao);
        }
    } else if (strcmp(argv[1], "image") == 0 || strcmp(argv[1], "imagem") == 0) {
        if (argc == 5 && (strcmp(argv[2], "save") == 0 || strcmp(argv[2], "salvar") == 0)) {
            SefValor valor = NULL;
            if (!caminho_e_lisp(argv[4])) {
                fputs("Sefirah source files must use the .lisp extension\n", stderr);
                resultado = 2;
            } else if (!sef_runtime_executar_arquivo(runtime, argv[4], &valor, &erro) ||
                       !sef_runtime_imagem_salvar(runtime, argv[3], &erro)) {
                resultado = mostrar_erro(&erro);
            } else {
                printf("Image saved to %s\n", argv[3]);
            }
        } else if ((argc == 4 || argc == 5) &&
                   (strcmp(argv[2], "open") == 0 || strcmp(argv[2], "abrir") == 0)) {
            sef_runtime_destruir(runtime);
            runtime = sef_runtime_imagem_abrir(argv[3], &erro);
            if (runtime == NULL) {
                return mostrar_erro(&erro);
            }
            if (argc == 5) {
                SefValor valor = sef_runtime_avaliar_texto(runtime, argv[4], &erro);
                resultado =
                    valor == NULL ? mostrar_erro(&erro) : imprimir_resultado(runtime, valor);
            } else {
                resultado = sef_runtime_repl(runtime, stdin, stdout);
            }
        } else {
            fputs("usage: sefirah image save <image> <file.lisp>\n"
                  "       sefirah image open <image> [form]\n",
                  stderr);
            resultado = 2;
        }
    } else {
        fprintf(stderr, "unknown command: %s\n\n", argv[1]);
        uso(stderr);
        resultado = 2;
    }
    sef_runtime_destruir(runtime);
    return resultado;
}
