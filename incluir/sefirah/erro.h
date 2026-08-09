#ifndef SEFIRAH_ERRO_H
#define SEFIRAH_ERRO_H

#include <stdbool.h>
#include <stddef.h>

typedef struct SefErro {
    bool ocorreu;
    size_t linha;
    size_t coluna;
    char mensagem[512];
} SefErro;

void sef_erro_limpar(SefErro *erro);
void sef_erro_definir(SefErro *erro, size_t linha, size_t coluna, const char *formato, ...);

#endif
