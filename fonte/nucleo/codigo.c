#include "sefirah/runtime.h"

#include <ctype.h>
#include <stddef.h>

typedef struct PosicaoCodigo {
    size_t linha;
    size_t coluna;
} PosicaoCodigo;

static void avancar(const char **cursor, PosicaoCodigo *posicao) {
    if (**cursor == '\0')
        return;
    if (**cursor == '\n') {
        posicao->linha++;
        posicao->coluna = 1;
    } else {
        posicao->coluna++;
    }
    (*cursor)++;
}

static bool delimitador(unsigned char caractere) {
    return caractere == '\0' || isspace(caractere) || caractere == '(' || caractere == ')' ||
           caractere == ';' || caractere == '\'' || caractere == '"' || caractere == '`' ||
           caractere == ',';
}

SefEstadoCodigo sef_runtime_estado_codigo(const char *codigo, SefErro *erro) {
    sef_erro_limpar(erro);
    if (codigo == NULL) {
        sef_erro_definir(erro, 0, 0, "codigo ausente");
        return SEF_CODIGO_INVALIDO;
    }

    const char *cursor = codigo;
    PosicaoCodigo posicao = {1, 1};
    size_t profundidade = 0;
    size_t prefixos_pendentes = 0;
    bool em_texto = false;
    bool escape = false;

    while (*cursor != '\0') {
        unsigned char caractere = (unsigned char)*cursor;
        if (em_texto) {
            if (escape) {
                escape = false;
            } else if (caractere == '\\') {
                escape = true;
            } else if (caractere == '"') {
                em_texto = false;
            }
            avancar(&cursor, &posicao);
            continue;
        }

        if (isspace(caractere)) {
            avancar(&cursor, &posicao);
            continue;
        }
        if (caractere == ';') {
            while (*cursor != '\0' && *cursor != '\n')
                avancar(&cursor, &posicao);
            continue;
        }
        if (caractere == ')') {
            if (prefixos_pendentes > 0) {
                sef_erro_definir(erro, posicao.linha, posicao.coluna,
                                 "prefixo de leitura sem forma antes do fechamento");
                return SEF_CODIGO_INVALIDO;
            }
            if (profundidade == 0) {
                sef_erro_definir(erro, posicao.linha, posicao.coluna,
                                 "parenteses de fechamento inesperado");
                return SEF_CODIGO_INVALIDO;
            }
            profundidade--;
            avancar(&cursor, &posicao);
            continue;
        }
        if (caractere == '\'' || caractere == '`' || caractere == ',') {
            prefixos_pendentes++;
            avancar(&cursor, &posicao);
            if (caractere == ',' && *cursor == '@')
                avancar(&cursor, &posicao);
            continue;
        }
        if (caractere == '#' && cursor[1] == '\'') {
            prefixos_pendentes++;
            avancar(&cursor, &posicao);
            avancar(&cursor, &posicao);
            continue;
        }
        if (caractere == '#' && cursor[1] == '\\') {
            prefixos_pendentes = 0;
            avancar(&cursor, &posicao);
            avancar(&cursor, &posicao);
            if (*cursor == '\0')
                return SEF_CODIGO_INCOMPLETO;
            if (delimitador((unsigned char)*cursor)) {
                avancar(&cursor, &posicao);
            } else {
                while (!delimitador((unsigned char)*cursor))
                    avancar(&cursor, &posicao);
            }
            continue;
        }
        if (caractere == '(' || (caractere == '#' && cursor[1] == '(')) {
            prefixos_pendentes = 0;
            if (caractere == '#')
                avancar(&cursor, &posicao);
            profundidade++;
            avancar(&cursor, &posicao);
            continue;
        }
        if (caractere == '"') {
            prefixos_pendentes = 0;
            em_texto = true;
            avancar(&cursor, &posicao);
            continue;
        }

        prefixos_pendentes = 0;
        while (!delimitador((unsigned char)*cursor))
            avancar(&cursor, &posicao);
    }

    return em_texto || escape || profundidade > 0 || prefixos_pendentes > 0 ? SEF_CODIGO_INCOMPLETO
                                                                            : SEF_CODIGO_COMPLETO;
}
