#include "apoio.h"

#include <ctype.h>
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
