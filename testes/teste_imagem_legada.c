#include "sefirah/interno.h"

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

static void remover_de_vetor(SefValor *valores, size_t *quantidade, SefValor removido) {
    for (size_t i = 0; i < *quantidade; i++) {
        if (valores[i] != removido)
            continue;
        memmove(valores + i, valores + i + 1, (*quantidade - i - 1) * sizeof(*valores));
        (*quantidade)--;
        return;
    }
}

static void remover_funcao_global(SefRuntime *runtime, SefValor simbolo) {
    SefVinculo **endereco = &runtime->ambiente_global->como.ambiente.funcoes;
    while (*endereco != NULL) {
        if ((*endereco)->simbolo == simbolo) {
            SefVinculo *removido = *endereco;
            *endereco = removido->proximo;
            free(removido);
            return;
        }
        endereco = &(*endereco)->proximo;
    }
}

static void converter_para_v6(const char *caminho) {
    FILE *arquivo = fopen(caminho, "r+b");
    verificar(arquivo != NULL, "imagem legada foi aberta para conversao");
    if (arquivo == NULL)
        return;
    verificar(fseek(arquivo, 6, SEEK_SET) == 0 && fputc(6, arquivo) == 6,
              "imagem recebeu assinatura v6");
    verificar(fclose(arquivo) == 0, "imagem legada foi fechada");
}

int main(void) {
    const char *caminho = "teste-migracao-v6.imagem";
    SefErro erro;
    SefRuntime *runtime = sef_runtime_criar(&erro);
    verificar(runtime != NULL, "runtime para imagem legada foi criado");
    if (runtime == NULL)
        return 1;

    SefValor simbolo_symbolp =
        sef_pacote_localizar_simbolo(runtime->pacote_common_lisp, "SYMBOLP", 7, false);
    verificar(simbolo_symbolp != NULL, "SYMBOLP existia antes da simulacao legada");
    if (simbolo_symbolp != NULL) {
        remover_funcao_global(runtime, simbolo_symbolp);
        remover_de_vetor(runtime->pacote_common_lisp->como.pacote.exportados,
                         &runtime->pacote_common_lisp->como.pacote.quantidade_exportados,
                         simbolo_symbolp);
        remover_de_vetor(runtime->pacote_common_lisp->como.pacote.simbolos,
                         &runtime->pacote_common_lisp->como.pacote.quantidade_simbolos,
                         simbolo_symbolp);
        remover_de_vetor(runtime->simbolos, &runtime->quantidade_simbolos, simbolo_symbolp);
    }
    remover_de_vetor(runtime->pacote_common_lisp->como.pacote.exportados,
                     &runtime->pacote_common_lisp->como.pacote.quantidade_exportados,
                     runtime->nulo);
    remover_de_vetor(runtime->pacote_common_lisp->como.pacote.simbolos,
                     &runtime->pacote_common_lisp->como.pacote.quantidade_simbolos,
                     runtime->nulo);
    SefValor nulo_legado =
        sef_simbolo_internar_em(runtime, runtime->pacote_atual, "NIL", 3, &erro);
    verificar(nulo_legado != NULL && nulo_legado != runtime->nulo,
              "imagem simulada recebeu NIL local legado");
    verificar(sef_runtime_avaliar_texto(runtime, "(defun keywordp (x) :preservada)", &erro) !=
                  NULL,
              "imagem simulada recebeu redefinicao Lisp deliberada");

    verificar(sef_runtime_imagem_salvar(runtime, caminho, &erro),
              "estado anterior aos contratos de NIL foi salvo");
    sef_runtime_destruir(runtime);
    converter_para_v6(caminho);

    runtime = sef_runtime_imagem_abrir(caminho, &erro);
    verificar(runtime != NULL, "imagem v6 incompleta foi migrada");
    if (runtime != NULL) {
        SefEstadoSimboloPacote estado = SEF_SIMBOLO_AUSENTE;
        SefValor nulo = sef_pacote_localizar_simbolo_com_estado(
            runtime->pacote_common_lisp, "NIL", 3, false, &estado);
        verificar(nulo == runtime->nulo && estado == SEF_SIMBOLO_EXTERNO,
                  "migracao reinstalou NIL externo em COMMON-LISP");
        SefValor resultado = sef_runtime_avaliar_texto(
            runtime,
            "(list (symbolp nil) (symbol-name nil) "
            "(multiple-value-list (find-symbol \"NIL\" \"COMMON-LISP-USER\")))",
            &erro);
        char *texto = resultado == NULL
                          ? NULL
                          : sef_valor_para_texto(runtime, resultado, true, &erro);
        verificar(texto != NULL && strcmp(texto, "(T \"NIL\" (NIL :INHERITED))") == 0,
                  "migracao reinstalou primitiva e visibilidade herdada");
        sef_texto_liberar(texto);
        resultado = sef_runtime_avaliar_texto(runtime, "(keywordp nil)", &erro);
        texto = resultado == NULL
                    ? NULL
                    : sef_valor_para_texto(runtime, resultado, true, &erro);
        verificar(texto != NULL && strcmp(texto, ":PRESERVADA") == 0,
                  "migracao preservou redefinicao Lisp existente");
        sef_texto_liberar(texto);
        sef_runtime_destruir(runtime);
    }
    remove(caminho);
    if (falhas == 0)
        puts("imagem legada: todos os testes passaram");
    return falhas == 0 ? 0 : 1;
}
