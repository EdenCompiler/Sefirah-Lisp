#include "sefirah/interno.h"

static SefValor primeiro(SefValor lista) { return lista->como.par.primeiro; }
static SefValor resto(SefValor lista) { return lista->como.par.resto; }

static bool quantidade(SefRuntime *runtime, SefValor argumentos, size_t minimo, size_t maximo,
                       const char *nome, SefErro *erro) {
    bool propria = false;
    size_t obtida = sef_lista_tamanho(runtime, argumentos, &propria);
    if (propria && obtida >= minimo && obtida <= maximo)
        return true;
    sef_erro_definir(erro, 0, 0, "%s recebeu uma quantidade invalida de argumentos", nome);
    return false;
}

static bool indice_lista(SefValor valor, const char *nome, size_t *indice, SefErro *erro) {
    if (valor->tipo != SEF_TIPO_INTEIRO || valor->como.inteiro < 0 ||
        (uint64_t)valor->como.inteiro > SIZE_MAX) {
        sef_erro_definir(erro, 0, 0, "%s exige um indice inteiro nao negativo", nome);
        return false;
    }
    *indice = (size_t)valor->como.inteiro;
    return true;
}

SefValor sef_primitiva_consp(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "CONSP", erro))
        return NULL;
    return primeiro(argumentos)->tipo == SEF_TIPO_PAR ? runtime->verdadeiro : runtime->nulo;
}

SefValor sef_primitiva_listp(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "LISTP", erro))
        return NULL;
    SefValor valor = primeiro(argumentos);
    return valor == runtime->nulo || valor->tipo == SEF_TIPO_PAR ? runtime->verdadeiro
                                                                 : runtime->nulo;
}

SefValor sef_primitiva_endp(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "ENDP", erro))
        return NULL;
    SefValor valor = primeiro(argumentos);
    if (valor == runtime->nulo)
        return runtime->verdadeiro;
    if (valor->tipo == SEF_TIPO_PAR)
        return runtime->nulo;
    sef_erro_definir(erro, 0, 0, "ENDP exige uma lista");
    return NULL;
}

static SefValor extremidade(SefRuntime *runtime, SefValor argumentos, bool cabeca, const char *nome,
                            SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, nome, erro))
        return NULL;
    SefValor valor = primeiro(argumentos);
    if (valor == runtime->nulo)
        return runtime->nulo;
    if (valor->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "%s exige uma lista", nome);
        return NULL;
    }
    return cabeca ? primeiro(valor) : resto(valor);
}

SefValor sef_primitiva_first(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    return extremidade(runtime, argumentos, true, "FIRST", erro);
}

SefValor sef_primitiva_rest(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    return extremidade(runtime, argumentos, false, "REST", erro);
}

static SefValor substituir_par(SefRuntime *runtime, SefValor argumentos, bool cabeca,
                               const char *nome, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, nome, erro))
        return NULL;
    SefValor par = primeiro(argumentos);
    SefValor valor = primeiro(resto(argumentos));
    if (par->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "%s exige um par", nome);
        return NULL;
    }
    if (cabeca)
        par->como.par.primeiro = valor;
    else
        par->como.par.resto = valor;
    return par;
}

SefValor sef_primitiva_rplaca(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    return substituir_par(runtime, argumentos, true, "RPLACA", erro);
}

SefValor sef_primitiva_rplacd(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    return substituir_par(runtime, argumentos, false, "RPLACD", erro);
}

SefValor sef_primitiva_nthcdr(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, "NTHCDR", erro))
        return NULL;
    size_t indice;
    if (!indice_lista(primeiro(argumentos), "NTHCDR", &indice, erro))
        return NULL;
    SefValor cursor = primeiro(resto(argumentos));
    if (cursor != runtime->nulo && cursor->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "NTHCDR exige uma lista");
        return NULL;
    }
    for (size_t i = 0; i < indice; i++) {
        if (cursor == runtime->nulo)
            return runtime->nulo;
        if (cursor->tipo != SEF_TIPO_PAR) {
            sef_erro_definir(erro, 0, 0, "NTHCDR recebeu uma lista impropria");
            return NULL;
        }
        cursor = resto(cursor);
    }
    if (cursor != runtime->nulo && cursor->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "NTHCDR recebeu uma lista impropria");
        return NULL;
    }
    return cursor;
}

SefValor sef_primitiva_nth(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    SefValor cauda = sef_primitiva_nthcdr(runtime, argumentos, erro);
    if (cauda == NULL || cauda == runtime->nulo)
        return cauda;
    if (cauda->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "NTH recebeu uma lista impropria");
        return NULL;
    }
    return primeiro(cauda);
}

SefValor sef_primitiva_last(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "LAST", erro))
        return NULL;
    SefValor lista = primeiro(argumentos);
    bool propria = false;
    size_t tamanho = sef_lista_tamanho(runtime, lista, &propria);
    if (!propria) {
        sef_erro_definir(erro, 0, 0, "LAST exige uma lista propria");
        return NULL;
    }
    size_t quantidade_final = 1;
    if (resto(argumentos) != runtime->nulo &&
        !indice_lista(primeiro(resto(argumentos)), "LAST", &quantidade_final, erro))
        return NULL;
    if (quantidade_final > tamanho)
        quantidade_final = tamanho;
    for (size_t i = 0; i < tamanho - quantidade_final; i++)
        lista = resto(lista);
    return lista;
}

SefValor sef_primitiva_append(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!sef_e_lista_propria(runtime, argumentos)) {
        sef_erro_definir(erro, 0, 0, "APPEND recebeu argumentos improprios");
        return NULL;
    }
    if (argumentos == runtime->nulo)
        return runtime->nulo;
    SefValor acumulada = runtime->nulo;
    while (resto(argumentos) != runtime->nulo) {
        SefValor lista = primeiro(argumentos);
        if (!sef_e_lista_propria(runtime, lista)) {
            sef_erro_definir(erro, 0, 0, "argumentos de APPEND, exceto o ultimo, devem ser listas");
            return NULL;
        }
        for (; lista != runtime->nulo; lista = resto(lista)) {
            acumulada = sef_par_novo(runtime, primeiro(lista), acumulada, erro);
            if (acumulada == NULL)
                return NULL;
        }
        argumentos = resto(argumentos);
    }
    SefValor resultado = primeiro(argumentos);
    while (acumulada != runtime->nulo) {
        resultado = sef_par_novo(runtime, primeiro(acumulada), resultado, erro);
        if (resultado == NULL)
            return NULL;
        acumulada = resto(acumulada);
    }
    return resultado;
}

SefValor sef_primitiva_nconc(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!sef_e_lista_propria(runtime, argumentos)) {
        sef_erro_definir(erro, 0, 0, "NCONC recebeu argumentos improprios");
        return NULL;
    }
    for (SefValor cursor = argumentos; cursor != runtime->nulo && resto(cursor) != runtime->nulo;
         cursor = resto(cursor)) {
        if (!sef_e_lista_propria(runtime, primeiro(cursor))) {
            sef_erro_definir(erro, 0, 0, "argumentos de NCONC, exceto o ultimo, devem ser listas");
            return NULL;
        }
    }
    SefValor resultado = runtime->nulo;
    SefValor ultima_celula = NULL;
    while (argumentos != runtime->nulo) {
        bool ultimo_argumento = resto(argumentos) == runtime->nulo;
        SefValor lista = primeiro(argumentos);
        if (resultado == runtime->nulo && lista != runtime->nulo)
            resultado = lista;
        if (ultima_celula != NULL)
            ultima_celula->como.par.resto = lista;
        if (!ultimo_argumento) {
            for (SefValor cursor = lista; cursor != runtime->nulo; cursor = resto(cursor))
                ultima_celula = cursor;
        }
        argumentos = resto(argumentos);
    }
    return resultado;
}

SefValor sef_primitiva_member(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, "MEMBER", erro))
        return NULL;
    SefValor item = primeiro(argumentos);
    SefValor lista = primeiro(resto(argumentos));
    for (SefValor cursor = lista; cursor != runtime->nulo; cursor = resto(cursor)) {
        if (cursor->tipo != SEF_TIPO_PAR) {
            sef_erro_definir(erro, 0, 0, "MEMBER exige uma lista propria");
            return NULL;
        }
        if (sef_valores_eql(item, primeiro(cursor)))
            return cursor;
    }
    return runtime->nulo;
}

SefValor sef_primitiva_assoc(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, "ASSOC", erro))
        return NULL;
    SefValor chave = primeiro(argumentos);
    SefValor alista = primeiro(resto(argumentos));
    for (SefValor cursor = alista; cursor != runtime->nulo; cursor = resto(cursor)) {
        if (cursor->tipo != SEF_TIPO_PAR) {
            sef_erro_definir(erro, 0, 0, "ASSOC exige uma lista de associacao propria");
            return NULL;
        }
        SefValor entrada = primeiro(cursor);
        if (entrada == runtime->nulo)
            continue;
        if (entrada->tipo != SEF_TIPO_PAR) {
            sef_erro_definir(erro, 0, 0, "entrada invalida na lista de associacao");
            return NULL;
        }
        if (sef_valores_eql(chave, primeiro(entrada)))
            return entrada;
    }
    return runtime->nulo;
}

static SefValor mapear(SefRuntime *runtime, SefValor argumentos, bool coletar, const char *nome,
                       SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, (size_t)-1, nome, erro))
        return NULL;
    SefValor funcao = primeiro(argumentos);
    SefValor listas = resto(argumentos);
    for (SefValor cursor = listas; cursor != runtime->nulo; cursor = resto(cursor)) {
        if (!sef_e_lista_propria(runtime, primeiro(cursor))) {
            sef_erro_definir(erro, 0, 0, "%s exige listas proprias", nome);
            return NULL;
        }
    }
    SefValor primeira_lista = primeiro(listas);
    SefValor resultados = runtime->nulo;
    for (;;) {
        SefValor chamada_invertida = runtime->nulo;
        bool terminou = false;
        for (SefValor cursor = listas; cursor != runtime->nulo; cursor = resto(cursor)) {
            SefValor lista = primeiro(cursor);
            if (lista == runtime->nulo) {
                terminou = true;
                break;
            }
            chamada_invertida = sef_par_novo(runtime, primeiro(lista), chamada_invertida, erro);
            if (chamada_invertida == NULL)
                return NULL;
        }
        if (terminou)
            break;
        SefValor chamada = sef_lista_inverter(runtime, chamada_invertida, erro);
        SefValor valor = chamada == NULL ? NULL : sef_aplicar(runtime, funcao, chamada, erro);
        if (valor == NULL)
            return NULL;
        if (coletar) {
            resultados = sef_par_novo(runtime, valor, resultados, erro);
            if (resultados == NULL)
                return NULL;
        }
        for (SefValor cursor = listas; cursor != runtime->nulo; cursor = resto(cursor))
            cursor->como.par.primeiro = resto(primeiro(cursor));
    }
    SefValor resultado = coletar ? sef_lista_inverter(runtime, resultados, erro) : primeira_lista;
    return resultado != NULL && sef_valores_definir_um(runtime, resultado, erro) ? resultado : NULL;
}

SefValor sef_primitiva_mapcar(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    return mapear(runtime, argumentos, true, "MAPCAR", erro);
}

SefValor sef_primitiva_mapc(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    return mapear(runtime, argumentos, false, "MAPC", erro);
}
