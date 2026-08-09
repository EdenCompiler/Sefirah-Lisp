#include "sefirah/graficos.h"

#include <stdlib.h>
#include <string.h>

bool sef_superficie_criar(SefSuperficie *superficie, int largura, int altura) {
    if (superficie == NULL || largura <= 0 || altura <= 0)
        return false;
    superficie->pixels = calloc((size_t)largura * (size_t)altura, sizeof(SefCor));
    if (superficie->pixels == NULL)
        return false;
    superficie->largura = largura;
    superficie->altura = altura;
    superficie->passo = largura;
    superficie->possui_pixels = true;
    return true;
}

void sef_superficie_destruir(SefSuperficie *superficie) {
    if (superficie == NULL)
        return;
    if (superficie->possui_pixels)
        free(superficie->pixels);
    memset(superficie, 0, sizeof(*superficie));
}

bool sef_superficie_redimensionar(SefSuperficie *superficie, int largura, int altura) {
    if (superficie == NULL || largura <= 0 || altura <= 0)
        return false;
    if (superficie->largura == largura && superficie->altura == altura &&
        superficie->pixels != NULL)
        return true;
    SefCor *pixels = calloc((size_t)largura * (size_t)altura, sizeof(SefCor));
    if (pixels == NULL)
        return false;
    if (superficie->possui_pixels)
        free(superficie->pixels);
    superficie->pixels = pixels;
    superficie->largura = largura;
    superficie->altura = altura;
    superficie->passo = largura;
    superficie->possui_pixels = true;
    return true;
}

void sef_superficie_limpar(SefSuperficie *superficie, SefCor cor) {
    if (superficie == NULL || superficie->pixels == NULL)
        return;
    size_t quantidade = (size_t)superficie->passo * (size_t)superficie->altura;
    for (size_t i = 0; i < quantidade; i++)
        superficie->pixels[i] = cor;
}

void sef_superficie_retangulo(SefSuperficie *superficie, int x, int y, int largura, int altura,
                              SefCor cor) {
    if (superficie == NULL || superficie->pixels == NULL || largura <= 0 || altura <= 0)
        return;
    int x0 = x < 0 ? 0 : x;
    int y0 = y < 0 ? 0 : y;
    int x1 = x + largura > superficie->largura ? superficie->largura : x + largura;
    int y1 = y + altura > superficie->altura ? superficie->altura : y + altura;
    for (int linha = y0; linha < y1; linha++) {
        SefCor *destino = superficie->pixels + linha * superficie->passo + x0;
        for (int coluna = x0; coluna < x1; coluna++)
            *destino++ = cor;
    }
}

void sef_superficie_contorno(SefSuperficie *superficie, int x, int y, int largura, int altura,
                             int espessura, SefCor cor) {
    if (espessura <= 0)
        return;
    sef_superficie_retangulo(superficie, x, y, largura, espessura, cor);
    sef_superficie_retangulo(superficie, x, y + altura - espessura, largura, espessura, cor);
    sef_superficie_retangulo(superficie, x, y, espessura, altura, cor);
    sef_superficie_retangulo(superficie, x + largura - espessura, y, espessura, altura, cor);
}

static void pixel(SefSuperficie *superficie, int x, int y, SefCor cor) {
    if (x < 0 || y < 0 || x >= superficie->largura || y >= superficie->altura)
        return;
    superficie->pixels[y * superficie->passo + x] = cor;
}

void sef_superficie_linha(SefSuperficie *superficie, int x0, int y0, int x1, int y1, SefCor cor) {
    if (superficie == NULL || superficie->pixels == NULL)
        return;
    int dx = abs(x1 - x0);
    int sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0);
    int sy = y0 < y1 ? 1 : -1;
    int erro = dx + dy;
    for (;;) {
        pixel(superficie, x0, y0, cor);
        if (x0 == x1 && y0 == y1)
            break;
        int dobro = 2 * erro;
        if (dobro >= dy) {
            erro += dy;
            x0 += sx;
        }
        if (dobro <= dx) {
            erro += dx;
            y0 += sy;
        }
    }
}
