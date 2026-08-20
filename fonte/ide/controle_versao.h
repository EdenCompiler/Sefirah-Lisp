#ifndef SEFIRAH_IDE_CONTROLE_VERSAO_H
#define SEFIRAH_IDE_CONTROLE_VERSAO_H

#include "sefirah/runtime.h"

#include <stdbool.h>

bool sef_controle_versao_git_status(const char *raiz, char **saida, int *codigo_saida,
                                    SefErro *erro);

#endif
