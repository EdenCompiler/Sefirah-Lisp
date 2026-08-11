#ifndef SEFIRAH_IDE_APOIO_H
#define SEFIRAH_IDE_APOIO_H

#include "sefirah/runtime.h"

#include <stdbool.h>
#include <stdint.h>
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

typedef struct SefFormaEstruturalIde {
    size_t inicio;
    size_t fim;
    size_t linha;
    size_t inicio_nome;
    size_t fim_nome;
    uint64_t assinatura;
    bool definicao;
    char categoria[20];
    char nome[96];
} SefFormaEstruturalIde;

typedef struct SefReferenciaEstruturalIde {
    size_t inicio;
    size_t fim;
    size_t linha;
    size_t indice_forma;
} SefReferenciaEstruturalIde;

bool sef_ide_catalogar_formas(const char *codigo, SefFormaEstruturalIde **formas,
                              size_t *quantidade, SefErro *erro);
void sef_ide_catalogo_liberar(SefFormaEstruturalIde *formas);
bool sef_ide_atomo_no_cursor(const char *codigo, size_t cursor, size_t *inicio, size_t *fim);
bool sef_ide_atomos_iguais(const char *codigo, size_t primeiro_inicio, size_t primeiro_fim,
                           size_t segundo_inicio, size_t segundo_fim);
bool sef_ide_catalogar_referencias(const char *codigo, size_t nome_inicio, size_t nome_fim,
                                   const SefFormaEstruturalIde *formas, size_t quantidade_formas,
                                   SefReferenciaEstruturalIde **referencias,
                                   size_t *quantidade_referencias, SefErro *erro);
void sef_ide_referencias_liberar(SefReferenciaEstruturalIde *referencias);

#endif
