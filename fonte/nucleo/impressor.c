#include "sefirah/interno.h"

#include <ctype.h>
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
        sef_erro_definir(erro, 0, 0, "not enough memory to print value");
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
        else if (caractere == '\0')
            escape = "\\0";
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

static bool imprimir_vetor(TextoDinamico *texto, SefRuntime *runtime, SefValor vetor, bool legivel,
                           int profundidade, SefErro *erro) {
    if (!anexar(texto, "#(", erro))
        return false;
    for (size_t i = 0; i < vetor->como.vetor.tamanho; i++) {
        if ((i > 0 && !anexar(texto, " ", erro)) ||
            !imprimir_valor(texto, runtime, vetor->como.vetor.itens[i], legivel, profundidade + 1,
                            erro))
            return false;
    }
    return anexar(texto, ")", erro);
}

static bool imprimir_caractere(TextoDinamico *texto, uint32_t codigo, bool legivel, SefErro *erro) {
    char codificado[4];
    size_t tamanho = sef_utf8_codificar(codigo, codificado);
    if (tamanho == 0) {
        sef_erro_definir(erro, 0, 0, "CHARACTER object has an invalid Unicode code point");
        return false;
    }
    if (!legivel)
        return anexar_n(texto, codificado, tamanho, erro);
    const char *nome = NULL;
    if (codigo == ' ')
        nome = "Space";
    else if (codigo == '\n')
        nome = "Newline";
    else if (codigo == '\t')
        nome = "Tab";
    else if (codigo == '\r')
        nome = "Return";
    else if (codigo == '\f')
        nome = "Page";
    else if (codigo == 0x7fu)
        nome = "Rubout";
    else if (codigo == 0)
        nome = "Null";
    if (nome != NULL)
        return anexar(texto, "#\\", erro) && anexar(texto, nome, erro);
    if (codigo < 0x20u) {
        char escape[16];
        snprintf(escape, sizeof(escape), "#\\U+%04X", (unsigned int)codigo);
        return anexar(texto, escape, erro);
    }
    return anexar(texto, "#\\", erro) && anexar_n(texto, codificado, tamanho, erro);
}

static bool nome_simbolo_precisa_escape(const char *nome, size_t tamanho) {
    if (tamanho == 0 || (tamanho == 3 && memcmp(nome, "NIL", 3) == 0))
        return true;
    unsigned char primeiro = (unsigned char)nome[0];
    if (isdigit(primeiro) || primeiro == '.' || primeiro == '#' ||
        ((primeiro == '+' || primeiro == '-') && tamanho > 1 &&
         (isdigit((unsigned char)nome[1]) || nome[1] == '.')))
        return true;
    for (size_t i = 0; i < tamanho; i++) {
        unsigned char caractere = (unsigned char)nome[i];
        if ((caractere < 128 && islower(caractere)) || isspace(caractere) || caractere == '(' ||
            caractere == ')' || caractere == ';' || caractere == '\'' || caractere == '"' ||
            caractere == '`' || caractere == ',' || caractere == ':' || caractere == '|' ||
            caractere == '\\')
            return true;
    }
    return false;
}

static bool imprimir_nome_simbolo(TextoDinamico *texto, const char *nome, size_t tamanho,
                                  bool legivel, SefErro *erro) {
    if (!legivel || !nome_simbolo_precisa_escape(nome, tamanho))
        return anexar_n(texto, nome, tamanho, erro);
    if (!anexar(texto, "|", erro))
        return false;
    for (size_t i = 0; i < tamanho; i++) {
        if ((nome[i] == '|' || nome[i] == '\\') && !anexar(texto, "\\", erro))
            return false;
        if (!anexar_n(texto, nome + i, 1, erro))
            return false;
    }
    return anexar(texto, "|", erro);
}

static bool imprimir_valor(TextoDinamico *texto, SefRuntime *runtime, SefValor valor, bool legivel,
                           int profundidade, SefErro *erro) {
    if (profundidade > 512) {
        return anexar(texto, "#<DEPTH-EXCEEDED>", erro);
    }
    if (valor == NULL)
        return anexar(texto, "#<INVALID-C-VALUE>", erro);
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
                   imprimir_nome_simbolo(texto, valor->como.simbolo.nome,
                                         valor->como.simbolo.tamanho, legivel, erro);
        if (valor->como.simbolo.pacote == runtime->pacote_atual ||
            sef_pacote_usa(runtime->pacote_atual, valor->como.simbolo.pacote))
            return imprimir_nome_simbolo(texto, valor->como.simbolo.nome,
                                         valor->como.simbolo.tamanho, legivel, erro);
        if (valor->como.simbolo.pacote == NULL)
            return anexar(texto, "#:", erro) &&
                   imprimir_nome_simbolo(texto, valor->como.simbolo.nome,
                                         valor->como.simbolo.tamanho, legivel, erro);
        return anexar(texto, valor->como.simbolo.pacote->como.pacote.nome, erro) &&
               anexar(texto,
                      sef_pacote_simbolo_exportado(valor->como.simbolo.pacote, valor) ? ":" : "::",
                      erro) &&
               imprimir_nome_simbolo(texto, valor->como.simbolo.nome, valor->como.simbolo.tamanho,
                                     legivel, erro);
    case SEF_TIPO_PAR:
        return imprimir_lista(texto, runtime, valor, legivel, profundidade, erro);
    case SEF_TIPO_NATIVA:
        return anexar(texto, "#<NATIVE-FUNCTION ", erro) &&
               anexar(texto, valor->como.nativa.nome, erro) && anexar(texto, ">", erro);
    case SEF_TIPO_FUNCAO:
        if (valor->como.funcao.compilada_i64 != NULL)
            return anexar(texto, "#<SEFIRAH-FUNCTION COMPILED-I64>", erro);
        return anexar(texto, valor->como.funcao.macro ? "#<SEFIRAH-MACRO>" : "#<SEFIRAH-FUNCTION>",
                      erro);
    case SEF_TIPO_AMBIENTE:
        return anexar(texto, "#<ENVIRONMENT>", erro);
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
    case SEF_TIPO_BIBLIOTECA:
        if (valor->como.biblioteca.fechada)
            return anexar(texto, "#<BIBLIOTECA-COMPARTILHADA FECHADA>", erro);
        return anexar(texto, "#<BIBLIOTECA-COMPARTILHADA ", erro) &&
               anexar(texto, sef_biblioteca_recurso_caminho(valor->como.biblioteca.recurso),
                      erro) &&
               anexar(texto, ">", erro);
    case SEF_TIPO_VETOR:
        return imprimir_vetor(texto, runtime, valor, legivel, profundidade, erro);
    case SEF_TIPO_CARACTERE:
        return imprimir_caractere(texto, valor->como.caractere, legivel, erro);
    case SEF_TIPO_TABELA_HASH:
        snprintf(numero, sizeof(numero), "#<HASH-TABLE %zu>", valor->como.tabela_hash.quantidade);
        return anexar(texto, numero, erro);
    case SEF_TIPO_REINICIO:
        if (valor->como.reinicio.nome == runtime->nulo)
            return anexar(texto, "#<RESTART ANONIMO>", erro);
        return anexar(texto, "#<RESTART ", erro) &&
               imprimir_valor(texto, runtime, valor->como.reinicio.nome, false, profundidade + 1,
                              erro) &&
               anexar(texto, ">", erro);
    }
    return anexar(texto, "#<UNKNOWN>", erro);
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
            sef_erro_definir(erro, 0, 0, "not enough memory while printing");
            return NULL;
        }
        texto.dados[0] = '\0';
    }
    return texto.dados;
}

void sef_texto_liberar(char *texto) { free(texto); }
