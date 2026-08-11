#include "apoio.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool delimitador(unsigned char caractere) {
    return caractere == '\0' || isspace(caractere) || caractere == '(' || caractere == ')' ||
           caractere == ';' || caractere == '\'' || caractere == '"' || caractere == '`' ||
           caractere == ',';
}

static void ignorar_espaco(const char *codigo, size_t tamanho, size_t *posicao) {
    for (;;) {
        while (*posicao < tamanho && isspace((unsigned char)codigo[*posicao]))
            (*posicao)++;
        if (*posicao >= tamanho || codigo[*posicao] != ';')
            return;
        while (*posicao < tamanho && codigo[*posicao] != '\n')
            (*posicao)++;
    }
}

static bool analisar_forma(const char *codigo, size_t tamanho, size_t *posicao) {
    ignorar_espaco(codigo, tamanho, posicao);
    if (*posicao >= tamanho || codigo[*posicao] == ')')
        return false;

    unsigned char caractere = (unsigned char)codigo[*posicao];
    if (caractere == '\'' || caractere == '`' || caractere == ',') {
        (*posicao)++;
        if (caractere == ',' && *posicao < tamanho && codigo[*posicao] == '@')
            (*posicao)++;
        return analisar_forma(codigo, tamanho, posicao);
    }
    if (caractere == '#' && *posicao + 1 < tamanho && codigo[*posicao + 1] == '\'') {
        *posicao += 2;
        return analisar_forma(codigo, tamanho, posicao);
    }
    if (caractere == '#' && *posicao + 1 < tamanho && codigo[*posicao + 1] == '\\') {
        *posicao += 2;
        if (*posicao >= tamanho)
            return false;
        if (delimitador((unsigned char)codigo[*posicao]))
            (*posicao)++;
        else
            while (*posicao < tamanho && !delimitador((unsigned char)codigo[*posicao]))
                (*posicao)++;
        return true;
    }
    if (caractere == '"') {
        bool escape = false;
        (*posicao)++;
        while (*posicao < tamanho) {
            unsigned char atual = (unsigned char)codigo[(*posicao)++];
            if (escape)
                escape = false;
            else if (atual == '\\')
                escape = true;
            else if (atual == '"')
                return true;
        }
        return false;
    }

    bool vetor = caractere == '#' && *posicao + 1 < tamanho && codigo[*posicao + 1] == '(';
    if (caractere == '(' || vetor) {
        if (vetor)
            (*posicao)++;
        (*posicao)++;
        for (;;) {
            ignorar_espaco(codigo, tamanho, posicao);
            if (*posicao >= tamanho)
                return false;
            if (codigo[*posicao] == ')') {
                (*posicao)++;
                return true;
            }
            if (!analisar_forma(codigo, tamanho, posicao))
                return false;
        }
    }

    size_t inicio = *posicao;
    bool entre_barras = false;
    bool escape = false;
    while (*posicao < tamanho) {
        caractere = (unsigned char)codigo[*posicao];
        if (escape) {
            escape = false;
            (*posicao)++;
        } else if (caractere == '\\') {
            escape = true;
            (*posicao)++;
        } else if (caractere == '|') {
            entre_barras = !entre_barras;
            (*posicao)++;
        } else if (!entre_barras && delimitador(caractere)) {
            break;
        } else {
            (*posicao)++;
        }
    }
    if (entre_barras || escape)
        return false;
    return *posicao > inicio;
}

bool sef_ide_forma_no_cursor(const char *codigo, size_t cursor, size_t *inicio, size_t *fim) {
    if (codigo == NULL || inicio == NULL || fim == NULL)
        return false;
    size_t tamanho = strlen(codigo);
    if (cursor > tamanho)
        cursor = tamanho;
    size_t posicao = 0;
    bool encontrou_anterior = false;
    size_t inicio_anterior = 0;
    size_t fim_anterior = 0;
    while (posicao < tamanho) {
        ignorar_espaco(codigo, tamanho, &posicao);
        if (posicao >= tamanho)
            break;
        size_t inicio_atual = posicao;
        if (!analisar_forma(codigo, tamanho, &posicao))
            return false;
        size_t fim_atual = posicao;
        if (cursor >= inicio_atual && cursor <= fim_atual) {
            *inicio = inicio_atual;
            *fim = fim_atual;
            return true;
        }
        if (fim_atual <= cursor) {
            encontrou_anterior = true;
            inicio_anterior = inicio_atual;
            fim_anterior = fim_atual;
        } else {
            break;
        }
    }
    if (!encontrou_anterior)
        return false;
    *inicio = inicio_anterior;
    *fim = fim_anterior;
    return true;
}

static uint64_t assinar_trecho(const char *codigo, size_t inicio, size_t fim) {
    uint64_t assinatura = UINT64_C(1469598103934665603);
    for (size_t i = inicio; i < fim; i++) {
        assinatura ^= (unsigned char)codigo[i];
        assinatura *= UINT64_C(1099511628211);
    }
    assinatura ^= (uint64_t)(fim - inicio);
    return assinatura;
}

static bool atomos_iguais_ascii(const char *codigo, size_t inicio, size_t fim,
                                const char *esperado) {
    size_t tamanho = fim - inicio;
    if (strlen(esperado) != tamanho)
        return false;
    for (size_t i = 0; i < tamanho; i++) {
        unsigned char caractere = (unsigned char)codigo[inicio + i];
        if ((unsigned char)toupper(caractere) != (unsigned char)esperado[i])
            return false;
    }
    return true;
}

static bool ler_atomo(const char *codigo, size_t limite, size_t *posicao, size_t *inicio,
                      size_t *fim) {
    ignorar_espaco(codigo, limite, posicao);
    if (*posicao >= limite || codigo[*posicao] == '(' || codigo[*posicao] == ')' ||
        codigo[*posicao] == '"')
        return false;
    *inicio = *posicao;
    bool entre_barras = false;
    bool escape = false;
    while (*posicao < limite) {
        unsigned char caractere = (unsigned char)codigo[*posicao];
        if (escape) {
            escape = false;
            (*posicao)++;
        } else if (caractere == '\\') {
            escape = true;
            (*posicao)++;
        } else if (caractere == '|') {
            entre_barras = !entre_barras;
            (*posicao)++;
        } else if (!entre_barras && delimitador(caractere)) {
            break;
        } else {
            (*posicao)++;
        }
    }
    *fim = *posicao;
    return *fim > *inicio && !entre_barras && !escape;
}

static void copiar_atomo(char *destino, size_t capacidade, const char *codigo, size_t inicio,
                         size_t fim) {
    if (capacidade == 0)
        return;
    size_t tamanho = fim - inicio;
    if (tamanho >= capacidade)
        tamanho = capacidade - 1;
    memcpy(destino, codigo + inicio, tamanho);
    destino[tamanho] = '\0';
}

static void classificar_forma(const char *codigo, SefFormaEstruturalIde *forma) {
    size_t posicao = forma->inicio;
    ignorar_espaco(codigo, forma->fim, &posicao);
    if (posicao >= forma->fim || codigo[posicao] != '(')
        return;
    posicao++;
    size_t inicio_operador = 0;
    size_t fim_operador = 0;
    size_t inicio_nome = 0;
    size_t fim_nome = 0;
    if (!ler_atomo(codigo, forma->fim, &posicao, &inicio_operador, &fim_operador) ||
        !ler_atomo(codigo, forma->fim, &posicao, &inicio_nome, &fim_nome))
        return;

    const char *categoria = NULL;
    if (atomos_iguais_ascii(codigo, inicio_operador, fim_operador, "DEFUN"))
        categoria = "FUNCAO";
    else if (atomos_iguais_ascii(codigo, inicio_operador, fim_operador, "DEFMACRO"))
        categoria = "MACRO";
    else if (atomos_iguais_ascii(codigo, inicio_operador, fim_operador, "DEFVAR"))
        categoria = "VARIAVEL";
    else if (atomos_iguais_ascii(codigo, inicio_operador, fim_operador, "DEFPARAMETER"))
        categoria = "PARAMETRO";
    else if (atomos_iguais_ascii(codigo, inicio_operador, fim_operador, "DEFCONSTANT"))
        categoria = "CONSTANTE";
    else if (atomos_iguais_ascii(codigo, inicio_operador, fim_operador, "DEFPACKAGE"))
        categoria = "PACOTE";
    else if (atomos_iguais_ascii(codigo, inicio_operador, fim_operador, "DEFINE"))
        categoria = "VALOR";
    if (categoria == NULL)
        return;

    forma->definicao = true;
    forma->inicio_nome = inicio_nome;
    snprintf(forma->categoria, sizeof(forma->categoria), "%s", categoria);
    copiar_atomo(forma->nome, sizeof(forma->nome), codigo, inicio_nome, fim_nome);
}

bool sef_ide_catalogar_formas(const char *codigo, SefFormaEstruturalIde **formas,
                              size_t *quantidade, SefErro *erro) {
    sef_erro_limpar(erro);
    if (codigo == NULL || formas == NULL || quantidade == NULL) {
        sef_erro_definir(erro, 0, 0, "argumento ausente ao catalogar formas da IDE");
        return false;
    }
    *formas = NULL;
    *quantidade = 0;
    size_t capacidade = 0;
    size_t tamanho = strlen(codigo);
    size_t posicao = 0;
    size_t linha = 1;
    while (posicao < tamanho) {
        size_t antes_espaco = posicao;
        ignorar_espaco(codigo, tamanho, &posicao);
        for (size_t i = antes_espaco; i < posicao; i++)
            if (codigo[i] == '\n')
                linha++;
        if (posicao >= tamanho)
            break;
        size_t inicio = posicao;
        size_t linha_forma = linha;
        if (!analisar_forma(codigo, tamanho, &posicao)) {
            free(*formas);
            *formas = NULL;
            *quantidade = 0;
            sef_erro_definir(erro, linha_forma, 1, "forma Lisp incompleta no catalogo da IDE");
            return false;
        }
        if (*quantidade == capacidade) {
            size_t nova_capacidade = capacidade == 0 ? 16 : capacidade * 2;
            if (nova_capacidade < capacidade ||
                nova_capacidade > SIZE_MAX / sizeof(**formas)) {
                free(*formas);
                *formas = NULL;
                *quantidade = 0;
                sef_erro_definir(erro, 0, 0, "catalogo estrutural da IDE excedeu o limite");
                return false;
            }
            SefFormaEstruturalIde *novas =
                realloc(*formas, nova_capacidade * sizeof(**formas));
            if (novas == NULL) {
                free(*formas);
                *formas = NULL;
                *quantidade = 0;
                sef_erro_definir(erro, 0, 0, "memoria insuficiente para catalogo da IDE");
                return false;
            }
            *formas = novas;
            capacidade = nova_capacidade;
        }
        SefFormaEstruturalIde *forma = &(*formas)[(*quantidade)++];
        memset(forma, 0, sizeof(*forma));
        forma->inicio = inicio;
        forma->fim = posicao;
        forma->linha = linha_forma;
        forma->inicio_nome = inicio;
        forma->assinatura = assinar_trecho(codigo, inicio, posicao);
        classificar_forma(codigo, forma);
        for (size_t i = inicio; i < posicao; i++)
            if (codigo[i] == '\n')
                linha++;
    }
    return true;
}

void sef_ide_catalogo_liberar(SefFormaEstruturalIde *formas) { free(formas); }
