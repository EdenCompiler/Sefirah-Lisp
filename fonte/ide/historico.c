#include "apoio.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define SEF_LIMITE_HISTORICO_IDE 128u

typedef struct EntradaEditorIde {
    char *texto;
    size_t cursor;
} EntradaEditorIde;

struct SefHistoricoTextoIde {
    char **itens;
    size_t quantidade;
    size_t capacidade;
    size_t posicao;
};

struct SefHistoricoEditorIde {
    EntradaEditorIde *itens;
    size_t quantidade;
    size_t capacidade;
    size_t posicao;
};

static char *duplicar_n(const char *texto, size_t tamanho, SefErro *erro) {
    if (tamanho == SIZE_MAX) {
        sef_erro_definir(erro, 0, 0, "text exceeds the IDE history limit");
        return NULL;
    }
    char *copia = malloc(tamanho + 1);
    if (copia == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for IDE history");
        return NULL;
    }
    if (tamanho > 0)
        memcpy(copia, texto, tamanho);
    copia[tamanho] = '\0';
    return copia;
}

static bool reservar_textos(SefHistoricoTextoIde *historico, size_t quantidade, SefErro *erro) {
    if (quantidade <= historico->capacidade)
        return true;
    size_t capacidade = historico->capacidade == 0 ? 16u : historico->capacidade * 2u;
    if (capacidade < quantidade)
        capacidade = quantidade;
    char **itens = realloc(historico->itens, capacidade * sizeof(*itens));
    if (itens == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for REPL history");
        return false;
    }
    historico->itens = itens;
    historico->capacidade = capacidade;
    return true;
}

SefHistoricoTextoIde *sef_historico_texto_criar(SefErro *erro) {
    SefHistoricoTextoIde *historico = calloc(1, sizeof(*historico));
    if (historico == NULL)
        sef_erro_definir(erro, 0, 0, "not enough memory for REPL history");
    return historico;
}

void sef_historico_texto_destruir(SefHistoricoTextoIde *historico) {
    if (historico == NULL)
        return;
    for (size_t i = 0; i < historico->quantidade; i++)
        free(historico->itens[i]);
    free(historico->itens);
    free(historico);
}

bool sef_historico_texto_registrar(SefHistoricoTextoIde *historico, const char *texto,
                                   size_t tamanho, SefErro *erro) {
    if (historico == NULL || texto == NULL || tamanho == 0)
        return true;
    if (historico->quantidade > 0 &&
        strlen(historico->itens[historico->quantidade - 1]) == tamanho &&
        memcmp(historico->itens[historico->quantidade - 1], texto, tamanho) == 0) {
        historico->posicao = historico->quantidade;
        return true;
    }
    char *copia = duplicar_n(texto, tamanho, erro);
    if (copia == NULL)
        return false;
    if (historico->quantidade == SEF_LIMITE_HISTORICO_IDE) {
        free(historico->itens[0]);
        memmove(historico->itens, historico->itens + 1,
                (historico->quantidade - 1) * sizeof(*historico->itens));
        historico->quantidade--;
    }
    if (!reservar_textos(historico, historico->quantidade + 1, erro)) {
        free(copia);
        return false;
    }
    historico->itens[historico->quantidade++] = copia;
    historico->posicao = historico->quantidade;
    return true;
}

const char *sef_historico_texto_anterior(SefHistoricoTextoIde *historico) {
    if (historico == NULL || historico->quantidade == 0 || historico->posicao == 0)
        return NULL;
    historico->posicao--;
    return historico->itens[historico->posicao];
}

const char *sef_historico_texto_proximo(SefHistoricoTextoIde *historico) {
    if (historico == NULL || historico->posicao >= historico->quantidade)
        return NULL;
    historico->posicao++;
    return historico->posicao == historico->quantidade ? "" : historico->itens[historico->posicao];
}

void sef_historico_texto_ir_ao_fim(SefHistoricoTextoIde *historico) {
    if (historico != NULL)
        historico->posicao = historico->quantidade;
}

size_t sef_historico_texto_quantidade(const SefHistoricoTextoIde *historico) {
    return historico == NULL ? 0 : historico->quantidade;
}

static bool reservar_editor(SefHistoricoEditorIde *historico, size_t quantidade, SefErro *erro) {
    if (quantidade <= historico->capacidade)
        return true;
    size_t capacidade = historico->capacidade == 0 ? 16u : historico->capacidade * 2u;
    if (capacidade < quantidade)
        capacidade = quantidade;
    EntradaEditorIde *itens = realloc(historico->itens, capacidade * sizeof(*itens));
    if (itens == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for editor timeline");
        return false;
    }
    historico->itens = itens;
    historico->capacidade = capacidade;
    return true;
}

SefHistoricoEditorIde *sef_historico_editor_criar(const char *texto, size_t cursor, SefErro *erro) {
    SefHistoricoEditorIde *historico = calloc(1, sizeof(*historico));
    if (historico == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for editor timeline");
        return NULL;
    }
    if (!sef_historico_editor_registrar(historico, texto, cursor, erro)) {
        sef_historico_editor_destruir(historico);
        return NULL;
    }
    return historico;
}

void sef_historico_editor_destruir(SefHistoricoEditorIde *historico) {
    if (historico == NULL)
        return;
    for (size_t i = 0; i < historico->quantidade; i++)
        free(historico->itens[i].texto);
    free(historico->itens);
    free(historico);
}

bool sef_historico_editor_registrar(SefHistoricoEditorIde *historico, const char *texto,
                                    size_t cursor, SefErro *erro) {
    if (historico == NULL || texto == NULL)
        return false;
    size_t tamanho = strlen(texto);
    if (historico->quantidade > 0 && historico->posicao < historico->quantidade &&
        strcmp(historico->itens[historico->posicao].texto, texto) == 0) {
        historico->itens[historico->posicao].cursor = cursor;
        return true;
    }
    char *copia = duplicar_n(texto, tamanho, erro);
    if (copia == NULL)
        return false;
    while (historico->quantidade > historico->posicao + 1)
        free(historico->itens[--historico->quantidade].texto);
    if (historico->quantidade == SEF_LIMITE_HISTORICO_IDE) {
        free(historico->itens[0].texto);
        memmove(historico->itens, historico->itens + 1,
                (historico->quantidade - 1) * sizeof(*historico->itens));
        historico->quantidade--;
        if (historico->posicao > 0)
            historico->posicao--;
    }
    if (!reservar_editor(historico, historico->quantidade + 1, erro)) {
        free(copia);
        return false;
    }
    historico->itens[historico->quantidade++] = (EntradaEditorIde){copia, cursor};
    historico->posicao = historico->quantidade - 1;
    return true;
}

bool sef_historico_editor_desfazer(SefHistoricoEditorIde *historico, const char **texto,
                                   size_t *cursor) {
    if (historico == NULL || historico->posicao == 0)
        return false;
    historico->posicao--;
    *texto = historico->itens[historico->posicao].texto;
    *cursor = historico->itens[historico->posicao].cursor;
    return true;
}

bool sef_historico_editor_refazer(SefHistoricoEditorIde *historico, const char **texto,
                                  size_t *cursor) {
    if (historico == NULL || historico->posicao + 1 >= historico->quantidade)
        return false;
    historico->posicao++;
    *texto = historico->itens[historico->posicao].texto;
    *cursor = historico->itens[historico->posicao].cursor;
    return true;
}
