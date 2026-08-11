#ifndef SEFIRAH_IDE_APOIO_H
#define SEFIRAH_IDE_APOIO_H

#include "sefirah/runtime.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct SefHistoricoTextoIde SefHistoricoTextoIde;
typedef struct SefHistoricoEditorIde SefHistoricoEditorIde;

SefHistoricoTextoIde *sef_historico_texto_criar(SefErro *erro);
void sef_historico_texto_destruir(SefHistoricoTextoIde *historico);
bool sef_historico_texto_registrar(SefHistoricoTextoIde *historico, const char *texto,
                                   size_t tamanho, SefErro *erro);
const char *sef_historico_texto_anterior(SefHistoricoTextoIde *historico);
const char *sef_historico_texto_proximo(SefHistoricoTextoIde *historico);
void sef_historico_texto_ir_ao_fim(SefHistoricoTextoIde *historico);
size_t sef_historico_texto_quantidade(const SefHistoricoTextoIde *historico);

SefHistoricoEditorIde *sef_historico_editor_criar(const char *texto, size_t cursor, SefErro *erro);
void sef_historico_editor_destruir(SefHistoricoEditorIde *historico);
bool sef_historico_editor_registrar(SefHistoricoEditorIde *historico, const char *texto,
                                    size_t cursor, SefErro *erro);
bool sef_historico_editor_desfazer(SefHistoricoEditorIde *historico, const char **texto,
                                   size_t *cursor);
bool sef_historico_editor_refazer(SefHistoricoEditorIde *historico, const char **texto,
                                  size_t *cursor);

bool sef_ide_forma_no_cursor(const char *codigo, size_t cursor, size_t *inicio, size_t *fim);

#endif
