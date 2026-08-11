#include "sefirah/interno.h"

#include <stdlib.h>
#include <string.h>

static size_t misturar(size_t x) {
    x ^= x >> 16u;
    x *= (size_t)0x9e3779b97f4a7c15ULL;
    return x ^ (x >> 16u);
}

static size_t calcular_hash(SefValor valor) {
    if (valor->tipo == SEF_TIPO_INTEIRO)
        return misturar((size_t)valor->como.inteiro);
    if (valor->tipo == SEF_TIPO_CARACTERE)
        return misturar(valor->como.caractere);
    if (valor->tipo == SEF_TIPO_REAL) {
        uint64_t bits = 0;
        if (valor->como.real == 0.0)
            return misturar(0);
        memcpy(&bits, &valor->como.real, sizeof(bits));
        return misturar((size_t)bits);
    }
    return misturar((size_t)(uintptr_t)valor);
}

static bool redimensionar(SefRuntime *runtime, SefValor tabela, SefErro *erro) {
    size_t capacidade_anterior = tabela->como.tabela_hash.capacidade;
    if (capacidade_anterior > SIZE_MAX / 2u) {
        sef_erro_definir(erro, 0, 0, "tabela hash excedeu a capacidade maxima");
        return false;
    }

    size_t nova_capacidade = capacidade_anterior == 0 ? 16 : capacidade_anterior * 2u;
    SefEntradaHash *novas_entradas = calloc(nova_capacidade, sizeof(*novas_entradas));
    if (novas_entradas == NULL) {
        sef_erro_definir(erro, 0, 0, "memoria insuficiente para tabela hash");
        return false;
    }

    SefEntradaHash *entradas_anteriores = tabela->como.tabela_hash.entradas;
    tabela->como.tabela_hash.entradas = novas_entradas;
    tabela->como.tabela_hash.capacidade = nova_capacidade;
    tabela->como.tabela_hash.quantidade = 0;
    tabela->como.tabela_hash.ocupadas = 0;

    for (size_t i = 0; i < capacidade_anterior; i++) {
        if (entradas_anteriores[i].estado == SEF_ENTRADA_HASH_OCUPADA) {
            size_t destino = calcular_hash(entradas_anteriores[i].chave) % nova_capacidade;
            while (novas_entradas[destino].estado == SEF_ENTRADA_HASH_OCUPADA)
                destino = (destino + 1) % nova_capacidade;
            novas_entradas[destino] = entradas_anteriores[i];
            tabela->como.tabela_hash.quantidade++;
            tabela->como.tabela_hash.ocupadas++;
        }
    }

    free(entradas_anteriores);
    runtime->bytes_aproximados += nova_capacidade * sizeof(*novas_entradas);
    return true;
}

static SefEntradaHash *procurar_entrada(SefValor tabela, SefValor chave, bool *encontrou) {
    size_t capacidade = tabela->como.tabela_hash.capacidade;
    size_t indice = calcular_hash(chave) % capacidade;
    SefEntradaHash *removida = NULL;

    for (;;) {
        SefEntradaHash *entrada = &tabela->como.tabela_hash.entradas[indice];
        if (entrada->estado == SEF_ENTRADA_HASH_VAZIA) {
            *encontrou = false;
            return removida != NULL ? removida : entrada;
        }
        if (entrada->estado == SEF_ENTRADA_HASH_OCUPADA && sef_valores_eql(entrada->chave, chave)) {
            *encontrou = true;
            return entrada;
        }
        if (entrada->estado == SEF_ENTRADA_HASH_REMOVIDA && removida == NULL)
            removida = entrada;
        indice = (indice + 1) % capacidade;
    }
}

SefValor sef_tabela_hash_nova(SefRuntime *runtime, SefErro *erro) {
    SefValor tabela = sef_objeto_novo(runtime, SEF_TIPO_TABELA_HASH, erro);
    return tabela != NULL && sef_tabela_hash_inicializar(runtime, tabela, erro) ? tabela : NULL;
}

bool sef_tabela_hash_inicializar(SefRuntime *runtime, SefValor tabela, SefErro *erro) {
    if (tabela == NULL || tabela->tipo != SEF_TIPO_TABELA_HASH) {
        sef_erro_definir(erro, 0, 0, "objeto invalido ao inicializar tabela hash");
        return false;
    }
    return redimensionar(runtime, tabela, erro);
}

bool sef_tabela_hash_definir(SefRuntime *runtime, SefValor tabela, SefValor chave, SefValor valor,
                             SefErro *erro) {
    if (tabela == NULL || tabela->tipo != SEF_TIPO_TABELA_HASH || chave == NULL || valor == NULL) {
        sef_erro_definir(erro, 0, 0, "SETF de GETHASH exige tabela, chave e valor");
        return false;
    }
    if ((tabela->como.tabela_hash.ocupadas + 1) * 10 >= tabela->como.tabela_hash.capacidade * 7 &&
        !redimensionar(runtime, tabela, erro))
        return false;

    bool encontrou;
    SefEntradaHash *entrada = procurar_entrada(tabela, chave, &encontrou);
    if (!encontrou) {
        if (entrada->estado == SEF_ENTRADA_HASH_VAZIA)
            tabela->como.tabela_hash.ocupadas++;
        tabela->como.tabela_hash.quantidade++;
        entrada->estado = SEF_ENTRADA_HASH_OCUPADA;
        entrada->chave = chave;
    }
    entrada->valor = valor;
    return true;
}

SefValor sef_tabela_hash_obter(SefRuntime *runtime, SefValor tabela, SefValor chave,
                               SefValor padrao, SefErro *erro) {
    (void)runtime;
    if (tabela == NULL || tabela->tipo != SEF_TIPO_TABELA_HASH || chave == NULL) {
        sef_erro_definir(erro, 0, 0, "GETHASH exige tabela hash e chave");
        return NULL;
    }

    bool encontrou;
    SefEntradaHash *entrada = procurar_entrada(tabela, chave, &encontrou);
    return encontrou ? entrada->valor : padrao;
}

bool sef_tabela_hash_remover(SefRuntime *runtime, SefValor tabela, SefValor chave, bool *removeu,
                             SefErro *erro) {
    (void)runtime;
    if (tabela == NULL || tabela->tipo != SEF_TIPO_TABELA_HASH || chave == NULL) {
        sef_erro_definir(erro, 0, 0, "REMHASH exige tabela hash e chave");
        return false;
    }

    bool encontrou;
    SefEntradaHash *entrada = procurar_entrada(tabela, chave, &encontrou);
    if (encontrou) {
        entrada->estado = SEF_ENTRADA_HASH_REMOVIDA;
        entrada->chave = NULL;
        entrada->valor = NULL;
        tabela->como.tabela_hash.quantidade--;
    }
    *removeu = encontrou;
    return true;
}

void sef_tabela_hash_limpar(SefValor tabela) {
    memset(tabela->como.tabela_hash.entradas, 0,
           tabela->como.tabela_hash.capacidade * sizeof(SefEntradaHash));
    tabela->como.tabela_hash.quantidade = 0;
    tabela->como.tabela_hash.ocupadas = 0;
}
