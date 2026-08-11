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
        sef_erro_definir(erro, 0, 0, "vinculo de funcao invalido");
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
        sef_erro_definir(erro, 0, 0, "memoria insuficiente ao definir funcao");
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
        sef_erro_definir(erro, 0, 0, "vinculo de ambiente invalido");
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
        sef_erro_definir(erro, 0, 0, "memoria insuficiente ao definir simbolo");
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
