#ifndef SEFIRAH_IDE_ESPACO_TRABALHO_H
#define SEFIRAH_IDE_ESPACO_TRABALHO_H

#include "sefirah/runtime.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct SefEspacoTrabalhoIde SefEspacoTrabalhoIde;

SefEspacoTrabalhoIde *sef_espaco_trabalho_ide_criar(SefErro *erro);
void sef_espaco_trabalho_ide_destruir(SefEspacoTrabalhoIde *espaco);

bool sef_espaco_trabalho_ide_abrir(SefEspacoTrabalhoIde *espaco, const char *raiz, SefErro *erro);
const char *sef_espaco_trabalho_ide_raiz(const SefEspacoTrabalhoIde *espaco);
size_t sef_espaco_trabalho_ide_quantidade(const SefEspacoTrabalhoIde *espaco);
const char *sef_espaco_trabalho_ide_arquivo_relativo(const SefEspacoTrabalhoIde *espaco,
                                                     size_t indice);
const char *sef_espaco_trabalho_ide_arquivo_absoluto(const SefEspacoTrabalhoIde *espaco,
                                                     size_t indice);

#endif
