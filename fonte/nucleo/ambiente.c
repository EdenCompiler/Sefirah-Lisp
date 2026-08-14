#include "sefirah/interno.h"

#include <stdlib.h>

SefValor sef_ambiente_novo(SefRuntime *runtime, SefValor pai, SefErro *erro) {
    SefValor ambiente = sef_objeto_novo(runtime, SEF_TIPO_AMBIENTE, erro);
    if (ambiente != NULL) {
        ambiente->como.ambiente.pai = pai;
        ambiente->como.ambiente.vinculos = NULL;
        ambiente->como.ambiente.funcoes = NULL;
    }
    return ambiente;
}

bool sef_ambiente_definir_funcao(SefRuntime *runtime, SefValor ambiente, SefValor simbolo,
                                 SefValor valor, SefErro *erro) {
    (void)runtime;
    if (ambiente == NULL || ambiente->tipo != SEF_TIPO_AMBIENTE || simbolo == NULL ||
        simbolo->tipo != SEF_TIPO_SIMBOLO) {
        sef_erro_definir(erro, 0, 0, "invalid function binding");
        return false;
    }
    for (SefVinculo *atual = ambiente->como.ambiente.funcoes; atual != NULL;
         atual = atual->proximo) {
        if (atual->simbolo == simbolo) {
            atual->valor = valor;
            return true;
        }
    }
    SefVinculo *novo = malloc(sizeof(*novo));
    if (novo == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory to define function");
        return false;
    }
    novo->simbolo = simbolo;
    novo->valor = valor;
    novo->proximo = ambiente->como.ambiente.funcoes;
    ambiente->como.ambiente.funcoes = novo;
    return true;
}

bool sef_ambiente_definir(SefRuntime *runtime, SefValor ambiente, SefValor simbolo, SefValor valor,
                          SefErro *erro) {
    (void)runtime;
    if (ambiente == NULL || ambiente->tipo != SEF_TIPO_AMBIENTE || simbolo == NULL ||
        simbolo->tipo != SEF_TIPO_SIMBOLO) {
        sef_erro_definir(erro, 0, 0, "invalid environment binding");
        return false;
    }

    for (SefVinculo *atual = ambiente->como.ambiente.vinculos; atual != NULL;
         atual = atual->proximo) {
        if (atual->simbolo == simbolo) {
            atual->valor = valor;
            return true;
        }
    }

    SefVinculo *novo = malloc(sizeof(*novo));
    if (novo == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory to define symbol");
        return false;
    }
    novo->simbolo = simbolo;
    novo->valor = valor;
    novo->proximo = ambiente->como.ambiente.vinculos;
    ambiente->como.ambiente.vinculos = novo;
    return true;
}

bool sef_ambiente_obter(SefValor ambiente, SefValor simbolo, SefValor *valor) {
    while (ambiente != NULL && ambiente->tipo == SEF_TIPO_AMBIENTE) {
        for (SefVinculo *atual = ambiente->como.ambiente.vinculos; atual != NULL;
             atual = atual->proximo) {
            if (atual->simbolo == simbolo) {
                *valor = atual->valor;
                return true;
            }
        }
        ambiente = ambiente->como.ambiente.pai;
    }
    return false;
}

bool sef_ambiente_atribuir(SefValor ambiente, SefValor simbolo, SefValor valor) {
    while (ambiente != NULL && ambiente->tipo == SEF_TIPO_AMBIENTE) {
        for (SefVinculo *atual = ambiente->como.ambiente.vinculos; atual != NULL;
             atual = atual->proximo) {
            if (atual->simbolo == simbolo) {
                atual->valor = valor;
                return true;
            }
        }
        ambiente = ambiente->como.ambiente.pai;
    }
    return false;
}

static bool remover_vinculo(SefVinculo **inicio, SefValor simbolo) {
    while (*inicio != NULL) {
        SefVinculo *atual = *inicio;
        if (atual->simbolo == simbolo) {
            *inicio = atual->proximo;
            free(atual);
            return true;
        }
        inicio = &atual->proximo;
    }
    return false;
}

bool sef_ambiente_remover(SefValor ambiente, SefValor simbolo) {
    if (ambiente == NULL || ambiente->tipo != SEF_TIPO_AMBIENTE)
        return false;
    return remover_vinculo(&ambiente->como.ambiente.vinculos, simbolo);
}

bool sef_ambiente_obter_funcao(SefValor ambiente, SefValor simbolo, SefValor *valor) {
    while (ambiente != NULL && ambiente->tipo == SEF_TIPO_AMBIENTE) {
        for (SefVinculo *atual = ambiente->como.ambiente.funcoes; atual != NULL;
             atual = atual->proximo) {
            if (atual->simbolo == simbolo) {
                *valor = atual->valor;
                return true;
            }
        }
        ambiente = ambiente->como.ambiente.pai;
    }
    return false;
}

bool sef_ambiente_remover_funcao(SefValor ambiente, SefValor simbolo) {
    if (ambiente == NULL || ambiente->tipo != SEF_TIPO_AMBIENTE)
        return false;
    return remover_vinculo(&ambiente->como.ambiente.funcoes, simbolo);
}
