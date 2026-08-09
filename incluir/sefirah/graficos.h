#ifndef SEFIRAH_GRAFICOS_H
#define SEFIRAH_GRAFICOS_H

#include <stdbool.h>
#include <stdint.h>

typedef uint32_t SefCor;

#define SEF_COR(r, g, b)                                                                           \
    ((SefCor)((((uint32_t)(r)) << 16u) | (((uint32_t)(g)) << 8u) | (uint32_t)(b)))

typedef struct SefSuperficie {
    int largura;
    int altura;
    int passo;
    SefCor *pixels;
    bool possui_pixels;
} SefSuperficie;

bool sef_superficie_criar(SefSuperficie *superficie, int largura, int altura);
void sef_superficie_destruir(SefSuperficie *superficie);
bool sef_superficie_redimensionar(SefSuperficie *superficie, int largura, int altura);
void sef_superficie_limpar(SefSuperficie *superficie, SefCor cor);
void sef_superficie_retangulo(SefSuperficie *superficie, int x, int y, int largura, int altura,
                              SefCor cor);
void sef_superficie_contorno(SefSuperficie *superficie, int x, int y, int largura, int altura,
                             int espessura, SefCor cor);
void sef_superficie_linha(SefSuperficie *superficie, int x0, int y0, int x1, int y1, SefCor cor);
void sef_superficie_texto(SefSuperficie *superficie, int x, int y, const char *texto, int escala,
                          SefCor cor);

#endif
