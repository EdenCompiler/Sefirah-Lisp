#include "sefirah/graficos.h"

#include <ctype.h>
#include <stddef.h>
#include <stdint.h>

typedef struct Glifo {
    char caractere;
    uint8_t linhas[7];
} Glifo;

/* Fonte minima 5x7, deliberadamente quadrada para a identidade Lisp Machine. */
static const Glifo glifos[] = {
    {'A', {14, 17, 17, 31, 17, 17, 17}}, {'B', {30, 17, 17, 30, 17, 17, 30}},
    {'C', {14, 17, 16, 16, 16, 17, 14}}, {'D', {30, 17, 17, 17, 17, 17, 30}},
    {'E', {31, 16, 16, 30, 16, 16, 31}}, {'F', {31, 16, 16, 30, 16, 16, 16}},
    {'G', {14, 17, 16, 23, 17, 17, 15}}, {'H', {17, 17, 17, 31, 17, 17, 17}},
    {'I', {14, 4, 4, 4, 4, 4, 14}},      {'J', {7, 2, 2, 2, 2, 18, 12}},
    {'K', {17, 18, 20, 24, 20, 18, 17}}, {'L', {16, 16, 16, 16, 16, 16, 31}},
    {'M', {17, 27, 21, 21, 17, 17, 17}}, {'N', {17, 25, 21, 19, 17, 17, 17}},
    {'O', {14, 17, 17, 17, 17, 17, 14}}, {'P', {30, 17, 17, 30, 16, 16, 16}},
    {'Q', {14, 17, 17, 17, 21, 18, 13}}, {'R', {30, 17, 17, 30, 20, 18, 17}},
    {'S', {15, 16, 16, 14, 1, 1, 30}},   {'T', {31, 4, 4, 4, 4, 4, 4}},
    {'U', {17, 17, 17, 17, 17, 17, 14}}, {'V', {17, 17, 17, 17, 17, 10, 4}},
    {'W', {17, 17, 17, 21, 21, 21, 10}}, {'X', {17, 17, 10, 4, 10, 17, 17}},
    {'Y', {17, 17, 10, 4, 4, 4, 4}},     {'Z', {31, 1, 2, 4, 8, 16, 31}},
    {'0', {14, 17, 19, 21, 25, 17, 14}}, {'1', {4, 12, 4, 4, 4, 4, 14}},
    {'2', {14, 17, 1, 2, 4, 8, 31}},     {'3', {30, 1, 1, 14, 1, 1, 30}},
    {'4', {2, 6, 10, 18, 31, 2, 2}},     {'5', {31, 16, 16, 30, 1, 1, 30}},
    {'6', {14, 16, 16, 30, 17, 17, 14}}, {'7', {31, 1, 2, 4, 8, 8, 8}},
    {'8', {14, 17, 17, 14, 17, 17, 14}}, {'9', {14, 17, 17, 15, 1, 1, 14}},
    {'(', {2, 4, 8, 8, 8, 4, 2}},        {')', {8, 4, 2, 2, 2, 4, 8}},
    {'[', {14, 8, 8, 8, 8, 8, 14}},      {']', {14, 2, 2, 2, 2, 2, 14}},
    {'-', {0, 0, 0, 31, 0, 0, 0}},       {'+', {0, 4, 4, 31, 4, 4, 0}},
    {'.', {0, 0, 0, 0, 0, 12, 12}},      {':', {0, 12, 12, 0, 12, 12, 0}},
    {'/', {1, 2, 2, 4, 8, 8, 16}},       {'>', {16, 8, 4, 2, 4, 8, 16}},
    {'<', {1, 2, 4, 8, 4, 2, 1}},        {'=', {0, 0, 31, 0, 31, 0, 0}},
    {'_', {0, 0, 0, 0, 0, 0, 31}},       {'?', {14, 17, 1, 2, 4, 0, 4}},
    {'!', {4, 4, 4, 4, 4, 0, 4}},        {' ', {0, 0, 0, 0, 0, 0, 0}}};

static const uint8_t *buscar(char caractere) {
    unsigned char codigo = (unsigned char)caractere;
    if (codigo < 128)
        caractere = (char)toupper(codigo);
    for (size_t i = 0; i < sizeof(glifos) / sizeof(glifos[0]); i++) {
        if (glifos[i].caractere == caractere)
            return glifos[i].linhas;
    }
    static const uint8_t desconhecido[7] = {31, 17, 2, 4, 4, 0, 4};
    return desconhecido;
}

void sef_superficie_texto(SefSuperficie *superficie, int x, int y, const char *texto, int escala,
                          SefCor cor) {
    if (superficie == NULL || superficie->pixels == NULL || texto == NULL || escala <= 0)
        return;
    int origem_x = x;
    for (const char *atual = texto; *atual != '\0'; atual++) {
        if (*atual == '\n') {
            x = origem_x;
            y += 9 * escala;
            continue;
        }
        const uint8_t *linhas = buscar(*atual);
        for (int linha = 0; linha < 7; linha++) {
            for (int coluna = 0; coluna < 5; coluna++) {
                if ((linhas[linha] & (1u << (4 - coluna))) != 0) {
                    sef_superficie_retangulo(superficie, x + coluna * escala, y + linha * escala,
                                             escala, escala, cor);
                }
            }
        }
        x += 6 * escala;
    }
}
