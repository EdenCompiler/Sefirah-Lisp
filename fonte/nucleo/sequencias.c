#include "sefirah/interno.h"

#include <stdlib.h>
#include <string.h>

static SefValor primeiro(SefValor lista) { return lista->como.par.primeiro; }
static SefValor resto(SefValor lista) { return lista->como.par.resto; }

static bool quantidade(SefRuntime *runtime, SefValor argumentos, size_t minimo, size_t maximo,
                       const char *nome, SefErro *erro) {
    bool propria = false;
    size_t obtida = sef_lista_tamanho(runtime, argumentos, &propria);
    if (propria && obtida >= minimo && obtida <= maximo)
        return true;
    sef_erro_definir(erro, 0, 0, "%s received an invalid number of arguments", nome);
    return false;
}

static bool indice(SefValor valor, const char *nome, size_t *resultado, SefErro *erro) {
    if (valor->tipo != SEF_TIPO_INTEIRO || valor->como.inteiro < 0 ||
        (uint64_t)valor->como.inteiro > SIZE_MAX) {
        sef_erro_definir(erro, 0, 0, "%s requires non-negative integer indices", nome);
        return false;
    }
    *resultado = (size_t)valor->como.inteiro;
    return true;
}

static bool tamanho_sequencia(SefRuntime *runtime, SefValor sequencia, const char *nome,
                              size_t *tamanho, SefErro *erro) {
    if (sequencia->tipo == SEF_TIPO_VETOR) {
        *tamanho = sequencia->como.vetor.tamanho;
        return true;
    }
    if (sequencia->tipo == SEF_TIPO_TEXTO) {
        bool valido = false;
        *tamanho = sef_utf8_quantidade(sequencia->como.texto.dados, sequencia->como.texto.tamanho,
                                       &valido);
        if (valido)
            return true;
        sef_erro_definir(erro, 0, 0, "%s received a string with invalid UTF-8", nome);
        return false;
    }
    bool propria = false;
    *tamanho = sef_lista_tamanho(runtime, sequencia, &propria);
    if (propria)
        return true;
    sef_erro_definir(erro, 0, 0, "%s requires a sequence", nome);
    return false;
}

static SefValor copiar_lista(SefRuntime *runtime, SefValor lista, bool inverter, SefErro *erro) {
    SefValor acumulada = runtime->nulo;
    for (SefValor cursor = lista; cursor != runtime->nulo; cursor = resto(cursor)) {
        if (cursor->tipo != SEF_TIPO_PAR) {
            sef_erro_definir(erro, 0, 0, "sequence operation received an improper list");
            return NULL;
        }
        acumulada = sef_par_novo(runtime, primeiro(cursor), acumulada, erro);
        if (acumulada == NULL)
            return NULL;
    }
    return inverter ? acumulada : sef_lista_inverter(runtime, acumulada, erro);
}

SefValor sef_primitiva_copy_seq(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "COPY-SEQ", erro))
        return NULL;
    SefValor sequencia = primeiro(argumentos);
    if (sequencia->tipo == SEF_TIPO_TEXTO)
        return sef_texto_novo(runtime, sequencia->como.texto.dados, sequencia->como.texto.tamanho,
                              erro);
    if (sequencia->tipo == SEF_TIPO_VETOR) {
        SefValor copia =
            sef_vetor_novo(runtime, sequencia->como.vetor.tamanho, runtime->nulo, erro);
        if (copia != NULL && sequencia->como.vetor.tamanho > 0)
            memcpy(copia->como.vetor.itens, sequencia->como.vetor.itens,
                   sequencia->como.vetor.tamanho * sizeof(SefValor));
        return copia;
    }
    return copiar_lista(runtime, sequencia, false, erro);
}

SefValor sef_primitiva_reverse(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "REVERSE", erro))
        return NULL;
    SefValor sequencia = primeiro(argumentos);
    if (sequencia->tipo == SEF_TIPO_VETOR) {
        size_t tamanho = sequencia->como.vetor.tamanho;
        SefValor invertida = sef_vetor_novo(runtime, tamanho, runtime->nulo, erro);
        if (invertida == NULL)
            return NULL;
        for (size_t i = 0; i < tamanho; i++)
            invertida->como.vetor.itens[tamanho - i - 1] = sequencia->como.vetor.itens[i];
        return invertida;
    }
    if (sequencia->tipo == SEF_TIPO_TEXTO) {
        size_t tamanho = sequencia->como.texto.tamanho;
        char *dados = tamanho == 0 ? NULL : malloc(tamanho);
        if (tamanho > 0 && dados == NULL) {
            sef_erro_definir(erro, 0, 0, "not enough memory in REVERSE");
            return NULL;
        }
        size_t origem = 0;
        size_t destino = tamanho;
        while (origem < tamanho) {
            size_t consumidos;
            uint32_t codigo;
            if (!sef_utf8_decodificar(sequencia->como.texto.dados + origem, tamanho - origem,
                                      &consumidos, &codigo)) {
                free(dados);
                sef_erro_definir(erro, 0, 0, "REVERSE received a string with invalid UTF-8");
                return NULL;
            }
            destino -= consumidos;
            memcpy(dados + destino, sequencia->como.texto.dados + origem, consumidos);
            origem += consumidos;
        }
        SefValor invertida = sef_texto_novo(runtime, dados == NULL ? "" : dados, tamanho, erro);
        free(dados);
        return invertida;
    }
    return copiar_lista(runtime, sequencia, true, erro);
}

static bool intervalo(SefRuntime *runtime, SefValor sequencia, SefValor inicio_valor,
                      SefValor fim_valor, bool possui_fim, const char *nome, size_t *inicio,
                      size_t *fim, SefErro *erro) {
    size_t tamanho;
    if (!indice(inicio_valor, nome, inicio, erro) ||
        (possui_fim && !indice(fim_valor, nome, fim, erro)) ||
        !tamanho_sequencia(runtime, sequencia, nome, &tamanho, erro))
        return false;
    if (!possui_fim)
        *fim = tamanho;
    if (*inicio > *fim || *fim > tamanho) {
        sef_erro_definir(erro, 0, 0, "invalid range in %s", nome);
        return false;
    }
    return true;
}

static bool intervalo_texto_bytes(SefValor texto, size_t inicio, size_t fim, size_t *byte_inicio,
                                  size_t *byte_fim) {
    size_t deslocamento = 0;
    size_t posicao = 0;
    *byte_inicio = inicio == 0 ? 0 : SIZE_MAX;
    *byte_fim = fim == 0 ? 0 : SIZE_MAX;
    while (deslocamento < texto->como.texto.tamanho) {
        if (posicao == inicio)
            *byte_inicio = deslocamento;
        if (posicao == fim)
            *byte_fim = deslocamento;
        size_t consumidos;
        uint32_t codigo;
        if (!sef_utf8_decodificar(texto->como.texto.dados + deslocamento,
                                  texto->como.texto.tamanho - deslocamento, &consumidos, &codigo))
            return false;
        deslocamento += consumidos;
        posicao++;
    }
    if (posicao == inicio)
        *byte_inicio = deslocamento;
    if (posicao == fim)
        *byte_fim = deslocamento;
    return *byte_inicio != SIZE_MAX && *byte_fim != SIZE_MAX;
}

SefValor sef_primitiva_subseq(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 3, "SUBSEQ", erro))
        return NULL;
    SefValor sequencia = primeiro(argumentos);
    SefValor restantes = resto(argumentos);
    SefValor inicio_valor = primeiro(restantes);
    restantes = resto(restantes);
    bool possui_fim = restantes != runtime->nulo;
    SefValor fim_valor = possui_fim ? primeiro(restantes) : runtime->nulo;
    size_t inicio, fim;
    if (!intervalo(runtime, sequencia, inicio_valor, fim_valor, possui_fim, "SUBSEQ", &inicio, &fim,
                   erro))
        return NULL;
    if (sequencia->tipo == SEF_TIPO_VETOR) {
        SefValor trecho = sef_vetor_novo(runtime, fim - inicio, runtime->nulo, erro);
        if (trecho != NULL && fim > inicio)
            memcpy(trecho->como.vetor.itens, sequencia->como.vetor.itens + inicio,
                   (fim - inicio) * sizeof(SefValor));
        return trecho;
    }
    if (sequencia->tipo == SEF_TIPO_TEXTO) {
        size_t byte_inicio, byte_fim;
        if (!intervalo_texto_bytes(sequencia, inicio, fim, &byte_inicio, &byte_fim)) {
            sef_erro_definir(erro, 0, 0, "SUBSEQ received a string with invalid UTF-8");
            return NULL;
        }
        return sef_texto_novo(runtime, sequencia->como.texto.dados + byte_inicio,
                              byte_fim - byte_inicio, erro);
    }
    SefValor cursor = sequencia;
    for (size_t i = 0; i < inicio; i++)
        cursor = resto(cursor);
    SefValor acumulada = runtime->nulo;
    for (size_t i = inicio; i < fim; i++) {
        acumulada = sef_par_novo(runtime, primeiro(cursor), acumulada, erro);
        if (acumulada == NULL)
            return NULL;
        cursor = resto(cursor);
    }
    return sef_lista_inverter(runtime, acumulada, erro);
}

SefValor sef_primitiva_fill(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, (size_t)-1, "FILL", erro))
        return NULL;
    SefValor sequencia = primeiro(argumentos);
    argumentos = resto(argumentos);
    SefValor item = primeiro(argumentos);
    argumentos = resto(argumentos);
    SefValor inicio_valor = NULL;
    SefValor fim_valor = NULL;
    while (argumentos != runtime->nulo) {
        SefValor chave = primeiro(argumentos);
        argumentos = resto(argumentos);
        if (argumentos == runtime->nulo) {
            sef_erro_definir(erro, 0, 0, "FILL received an option without a value");
            return NULL;
        }
        SefValor valor = primeiro(argumentos);
        argumentos = resto(argumentos);
        if (sef_simbolo_tem_nome(chave, "START"))
            inicio_valor = valor;
        else if (sef_simbolo_tem_nome(chave, "END"))
            fim_valor = valor;
        else {
            sef_erro_definir(erro, 0, 0, "unknown FILL option");
            return NULL;
        }
    }
    SefErro erro_local;
    sef_erro_limpar(&erro_local);
    SefValor zero = sef_inteiro_novo(runtime, 0, &erro_local);
    if (zero == NULL) {
        sef_erro_definir(erro, 0, 0, "%s", erro_local.mensagem);
        return NULL;
    }
    if (inicio_valor == NULL)
        inicio_valor = zero;
    size_t inicio, fim;
    if (!intervalo(runtime, sequencia, inicio_valor, fim_valor, fim_valor != NULL, "FILL", &inicio,
                   &fim, erro))
        return NULL;
    if (sequencia->tipo == SEF_TIPO_VETOR) {
        for (size_t i = inicio; i < fim; i++)
            sequencia->como.vetor.itens[i] = item;
        return sequencia;
    }
    if (sequencia->tipo == SEF_TIPO_TEXTO) {
        if (item->tipo != SEF_TIPO_CARACTERE) {
            sef_erro_definir(erro, 0, 0, "FILL on a string requires a character");
            return NULL;
        }
        for (size_t i = inicio; i < fim; i++) {
            if (!sef_texto_caractere_definir(runtime, sequencia, i, item, erro))
                return NULL;
        }
        return sequencia;
    }
    SefValor cursor = sequencia;
    for (size_t i = 0; i < inicio; i++)
        cursor = resto(cursor);
    for (size_t i = inicio; i < fim; i++) {
        cursor->como.par.primeiro = item;
        cursor = resto(cursor);
    }
    return sequencia;
}
