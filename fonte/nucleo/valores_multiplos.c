#include "sefirah/interno.h"

#include <stdlib.h>
#include <string.h>

static bool reservar_valores(SefRuntime *runtime, size_t quantidade, SefErro *erro) {
    if (quantidade <= runtime->capacidade_valores)
        return true;
    if (quantidade > SIZE_MAX / sizeof(*runtime->valores_multiplos)) {
        sef_erro_definir(erro, 0, 0, "multiple-value count exceeds the limit");
        return false;
    }

    size_t capacidade = runtime->capacidade_valores == 0 ? 4 : runtime->capacidade_valores;
    while (capacidade < quantidade) {
        if (capacidade > SIZE_MAX / 2u) {
            capacidade = quantidade;
            break;
        }
        capacidade *= 2u;
    }

    SefValor *novos = realloc(runtime->valores_multiplos, capacidade * sizeof(*novos));
    if (novos == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for multiple values");
        return false;
    }
    runtime->valores_multiplos = novos;
    runtime->capacidade_valores = capacidade;
    return true;
}

bool sef_valores_definir(SefRuntime *runtime, const SefValor *valores, size_t quantidade,
                         SefErro *erro) {
    if (runtime == NULL || (quantidade > 0 && valores == NULL)) {
        sef_erro_definir(erro, 0, 0, "invalid multiple-value set");
        return false;
    }
    if (!reservar_valores(runtime, quantidade, erro))
        return false;
    if (quantidade > 0)
        memmove(runtime->valores_multiplos, valores, quantidade * sizeof(*valores));
    runtime->quantidade_valores = quantidade;
    runtime->versao_valores++;
    if (runtime->versao_valores == 0)
        runtime->versao_valores = 1;
    return true;
}

bool sef_valores_definir_um(SefRuntime *runtime, SefValor valor, SefErro *erro) {
    if (valor == NULL) {
        sef_erro_definir(erro, 0, 0, "missing primary value");
        return false;
    }
    return sef_valores_definir(runtime, &valor, 1, erro);
}

bool sef_valores_definir_lista(SefRuntime *runtime, SefValor lista, SefErro *erro) {
    bool propria = false;
    size_t quantidade = sef_lista_tamanho(runtime, lista, &propria);
    if (!propria) {
        sef_erro_definir(erro, 0, 0, "VALUES-LIST requires a proper list");
        return false;
    }
    if (!reservar_valores(runtime, quantidade, erro))
        return false;
    SefValor cursor = lista;
    for (size_t i = 0; i < quantidade; i++) {
        runtime->valores_multiplos[i] = cursor->como.par.primeiro;
        cursor = cursor->como.par.resto;
    }
    runtime->quantidade_valores = quantidade;
    runtime->versao_valores++;
    if (runtime->versao_valores == 0)
        runtime->versao_valores = 1;
    return true;
}

SefValor sef_valores_primario(const SefRuntime *runtime) {
    return runtime->quantidade_valores == 0 ? runtime->nulo : runtime->valores_multiplos[0];
}

bool sef_valores_salvar(const SefRuntime *runtime, SefValoresSalvos *salvos, SefErro *erro) {
    salvos->itens = NULL;
    salvos->quantidade = runtime->quantidade_valores;
    if (salvos->quantidade == 0)
        return true;
    salvos->itens = malloc(salvos->quantidade * sizeof(*salvos->itens));
    if (salvos->itens == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory to preserve multiple values");
        return false;
    }
    memcpy(salvos->itens, runtime->valores_multiplos, salvos->quantidade * sizeof(*salvos->itens));
    return true;
}

bool sef_valores_restaurar(SefRuntime *runtime, const SefValoresSalvos *salvos, SefErro *erro) {
    return sef_valores_definir(runtime, salvos->itens, salvos->quantidade, erro);
}

void sef_valores_salvos_liberar(SefValoresSalvos *salvos) {
    free(salvos->itens);
    salvos->itens = NULL;
    salvos->quantidade = 0;
}

size_t sef_runtime_quantidade_valores(const SefRuntime *runtime) {
    return runtime == NULL ? 0 : runtime->quantidade_valores;
}

SefValor sef_runtime_valor(const SefRuntime *runtime, size_t indice) {
    return runtime == NULL || indice >= runtime->quantidade_valores
               ? NULL
               : runtime->valores_multiplos[indice];
}
