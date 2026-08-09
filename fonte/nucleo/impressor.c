#include "interno.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct TextoDinamico {
    char *dados;
    size_t tamanho;
    size_t capacidade;
} TextoDinamico;

static bool reservar(TextoDinamico *texto, size_t adicional, SefErro *erro) {
    if (texto->tamanho + adicional + 1 <= texto->capacidade)
        return true;
    size_t capacidade = texto->capacidade == 0 ? 64 : texto->capacidade;
    while (capacidade < texto->tamanho + adicional + 1)
        capacidade *= 2;
    char *novos = realloc(texto->dados, capacidade);
    if (novos == NULL) {
        sef_erro_definir(erro, 0, 0, "memoria insuficiente ao imprimir valor");
        return false;
    }
    texto->dados = novos;
    texto->capacidade = capacidade;
    return true;
}

static bool anexar_n(TextoDinamico *texto, const char *dados, size_t tamanho, SefErro *erro) {
    if (!reservar(texto, tamanho, erro))
        return false;
    memcpy(texto->dados + texto->tamanho, dados, tamanho);
    texto->tamanho += tamanho;
    texto->dados[texto->tamanho] = '\0';
    return true;
}

static bool anexar(TextoDinamico *texto, const char *dados, SefErro *erro) {
    return anexar_n(texto, dados, strlen(dados), erro);
}

static bool imprimir_valor(TextoDinamico *texto, SefRuntime *runtime, SefValor valor, bool legivel,
                           int profundidade, SefErro *erro);

static bool imprimir_texto(TextoDinamico *saida, SefValor valor, bool legivel, SefErro *erro) {
    if (!legivel)
        return anexar_n(saida, valor->como.texto.dados, valor->como.texto.tamanho, erro);
    if (!anexar(saida, "\"", erro))
        return false;
    for (size_t i = 0; i < valor->como.texto.tamanho; i++) {
        char caractere = valor->como.texto.dados[i];
        const char *escape = NULL;
        if (caractere == '\n')
            escape = "\\n";
        else if (caractere == '\r')
            escape = "\\r";
        else if (caractere == '\t')
            escape = "\\t";
        else if (caractere == '\\')
            escape = "\\\\";
        else if (caractere == '"')
            escape = "\\\"";
        if (escape != NULL) {
            if (!anexar(saida, escape, erro))
                return false;
        } else if (!anexar_n(saida, &caractere, 1, erro)) {
            return false;
        }
    }
    return anexar(saida, "\"", erro);
}

static bool imprimir_lista(TextoDinamico *texto, SefRuntime *runtime, SefValor lista, bool legivel,
                           int profundidade, SefErro *erro) {
    if (!anexar(texto, "(", erro))
        return false;
    bool primeiro_item = true;
    while (lista != runtime->nulo && lista->tipo == SEF_TIPO_PAR) {
        if (!primeiro_item && !anexar(texto, " ", erro))
            return false;
        if (!imprimir_valor(texto, runtime, lista->como.par.primeiro, legivel, profundidade + 1,
                            erro))
            return false;
        primeiro_item = false;
        lista = lista->como.par.resto;
    }
    if (lista != runtime->nulo) {
        if (!anexar(texto, " . ", erro) ||
            !imprimir_valor(texto, runtime, lista, legivel, profundidade + 1, erro)) {
            return false;
        }
    }
    return anexar(texto, ")", erro);
}

static bool imprimir_valor(TextoDinamico *texto, SefRuntime *runtime, SefValor valor, bool legivel,
                           int profundidade, SefErro *erro) {
    if (profundidade > 512) {
        return anexar(texto, "#<PROFUNDIDADE-EXCEDIDA>", erro);
    }
    if (valor == NULL)
        return anexar(texto, "#<VALOR-C-INVALIDO>", erro);
    char numero[128];
    switch (valor->tipo) {
    case SEF_TIPO_NULO:
        return anexar(texto, "NIL", erro);
    case SEF_TIPO_INTEIRO:
        snprintf(numero, sizeof(numero), "%lld", (long long)valor->como.inteiro);
        return anexar(texto, numero, erro);
    case SEF_TIPO_REAL:
        snprintf(numero, sizeof(numero), "%.17g", valor->como.real);
        return anexar(texto, numero, erro);
    case SEF_TIPO_TEXTO:
        return imprimir_texto(texto, valor, legivel, erro);
    case SEF_TIPO_SIMBOLO:
        if (valor->como.simbolo.pacote == runtime->pacote_keyword)
            return anexar(texto, ":", erro) &&
                   anexar_n(texto, valor->como.simbolo.nome, valor->como.simbolo.tamanho, erro);
        if (valor->como.simbolo.pacote == runtime->pacote_atual ||
            sef_pacote_usa(runtime->pacote_atual, valor->como.simbolo.pacote))
            return anexar_n(texto, valor->como.simbolo.nome, valor->como.simbolo.tamanho, erro);
        if (valor->como.simbolo.pacote == NULL)
            return anexar(texto, "#:", erro) &&
                   anexar_n(texto, valor->como.simbolo.nome, valor->como.simbolo.tamanho, erro);
        return anexar(texto, valor->como.simbolo.pacote->como.pacote.nome, erro) &&
               anexar(texto,
                      sef_pacote_simbolo_exportado(valor->como.simbolo.pacote, valor) ? ":" : "::",
                      erro) &&
               anexar_n(texto, valor->como.simbolo.nome, valor->como.simbolo.tamanho, erro);
    case SEF_TIPO_PAR:
        return imprimir_lista(texto, runtime, valor, legivel, profundidade, erro);
    case SEF_TIPO_NATIVA:
        return anexar(texto, "#<FUNCAO-NATIVA ", erro) &&
               anexar(texto, valor->como.nativa.nome, erro) && anexar(texto, ">", erro);
    case SEF_TIPO_FUNCAO:
        if (valor->como.funcao.compilada_i64 != NULL)
            return anexar(texto, "#<FUNCAO-SEFIRAH COMPILADA-I64>", erro);
        return anexar(texto, valor->como.funcao.macro ? "#<MACRO-SEFIRAH>" : "#<FUNCAO-SEFIRAH>",
                      erro);
    case SEF_TIPO_AMBIENTE:
        return anexar(texto, "#<AMBIENTE>", erro);
    case SEF_TIPO_CONDICAO:
        return anexar(texto, "#<", erro) &&
               imprimir_valor(texto, runtime, valor->como.condicao.classe, false, profundidade + 1,
                              erro) &&
               anexar(texto, " ", erro) &&
               imprimir_valor(texto, runtime, valor->como.condicao.mensagem, false,
                              profundidade + 1, erro) &&
               anexar(texto, ">", erro);
    case SEF_TIPO_PACOTE:
        return anexar(texto, "#<PACKAGE ", erro) && anexar(texto, valor->como.pacote.nome, erro) &&
               anexar(texto, ">", erro);
    case SEF_TIPO_STREAM:
        if (valor->como.stream.fechado)
            return anexar(texto, "#<CLOSED-STREAM>", erro);
        if (valor->como.stream.caminho != NULL)
            return anexar(texto, "#<FILE-STREAM ", erro) &&
                   anexar(texto, valor->como.stream.caminho, erro) && anexar(texto, ">", erro);
        return anexar(texto, "#<STREAM PADRAO>", erro);
    }
    return anexar(texto, "#<DESCONHECIDO>", erro);
}

char *sef_valor_para_texto(SefRuntime *runtime, SefValor valor, bool legivel, SefErro *erro) {
    sef_erro_limpar(erro);
    TextoDinamico texto = {0};
    if (!imprimir_valor(&texto, runtime, valor, legivel, 0, erro)) {
        free(texto.dados);
        return NULL;
    }
    if (texto.dados == NULL) {
        texto.dados = malloc(1);
        if (texto.dados == NULL) {
            sef_erro_definir(erro, 0, 0, "memoria insuficiente ao imprimir");
            return NULL;
        }
        texto.dados[0] = '\0';
    }
    return texto.dados;
}

void sef_texto_liberar(char *texto) { free(texto); }
