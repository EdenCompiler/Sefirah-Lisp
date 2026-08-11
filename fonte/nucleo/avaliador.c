#include "sefirah/interno.h"

#include <stdlib.h>
#include <string.h>

static SefValor primeiro(SefValor lista) { return lista->como.par.primeiro; }

static SefValor resto(SefValor lista) { return lista->como.par.resto; }

static bool vincular_parametros(SefRuntime *runtime, SefValor ambiente, SefValor parametros,
                                SefValor argumentos, SefErro *erro);

static SefValor valor_unico(SefRuntime *runtime, SefValor valor, SefErro *erro) {
    return valor != NULL && sef_valores_definir_um(runtime, valor, erro) ? valor : NULL;
}

static bool exigir_lista(SefRuntime *runtime, SefValor lista, const char *contexto, SefErro *erro) {
    if (sef_e_lista_propria(runtime, lista))
        return true;
    sef_erro_definir(erro, 0, 0, "%s exige uma lista propria", contexto);
    return false;
}

static SefValor avaliar_sequencia(SefRuntime *runtime, SefValor formas, SefValor ambiente,
                                  SefErro *erro) {
    if (!exigir_lista(runtime, formas, "sequencia", erro))
        return NULL;
    if (formas == runtime->nulo)
        return valor_unico(runtime, runtime->nulo, erro);
    SefValor resultado = runtime->nulo;
    while (formas != runtime->nulo) {
        resultado = sef_avaliar(runtime, primeiro(formas), ambiente, erro);
        if (resultado == NULL)
            return NULL;
        formas = resto(formas);
    }
    return resultado;
}

static SefValor avaliar_argumentos(SefRuntime *runtime, SefValor formas, SefValor ambiente,
                                   SefErro *erro) {
    if (!exigir_lista(runtime, formas, "chamada", erro))
        return NULL;
    SefValor invertida = runtime->nulo;
    while (formas != runtime->nulo) {
        SefValor valor = sef_avaliar(runtime, primeiro(formas), ambiente, erro);
        if (valor == NULL)
            return NULL;
        invertida = sef_par_novo(runtime, valor, invertida, erro);
        if (invertida == NULL)
            return NULL;
        formas = resto(formas);
    }
    return sef_lista_inverter(runtime, invertida, erro);
}

static bool contar_exato(SefRuntime *runtime, SefValor argumentos, size_t esperado,
                         const char *nome, SefErro *erro) {
    bool propria = false;
    size_t obtido = sef_lista_tamanho(runtime, argumentos, &propria);
    if (propria && obtido == esperado)
        return true;
    sef_erro_definir(erro, 0, 0, "%s esperava %zu argumento(s), recebeu %zu", nome, esperado,
                     obtido);
    return false;
}

static bool exigir_nome_variavel(SefRuntime *runtime, SefValor nome, const char *contexto,
                                 SefErro *erro) {
    if (!sef_valor_e_simbolo_logico(runtime, nome)) {
        sef_erro_definir(erro, 0, 0, "%s exige um simbolo", contexto);
        return false;
    }
    if (sef_simbolo_e_constante(runtime, nome)) {
        sef_erro_definir(erro, 0, 0, "%s nao pode vincular um simbolo constante", contexto);
        return false;
    }
    return true;
}

static SefValor especial_quote(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!contar_exato(runtime, argumentos, 1, "QUOTE", erro))
        return NULL;
    return primeiro(argumentos);
}

static SefValor lista_unaria(SefRuntime *runtime, const char *nome, SefValor argumento,
                             SefErro *erro) {
    SefValor simbolo = sef_simbolo_internar(runtime, nome, strlen(nome), erro);
    if (simbolo == NULL)
        return NULL;
    SefValor cauda = sef_par_novo(runtime, argumento, runtime->nulo, erro);
    return cauda == NULL ? NULL : sef_par_novo(runtime, simbolo, cauda, erro);
}

static SefValor avaliar_quasiquote(SefRuntime *runtime, SefValor forma, SefValor ambiente,
                                   unsigned int profundidade, SefErro *erro) {
    if (forma == runtime->nulo || forma->tipo != SEF_TIPO_PAR)
        return forma;
    SefValor operador = primeiro(forma);
    SefValor argumentos = resto(forma);
    if (sef_simbolo_tem_nome(operador, "UNQUOTE")) {
        if (!contar_exato(runtime, argumentos, 1, "UNQUOTE", erro))
            return NULL;
        if (profundidade == 1)
            return sef_avaliar(runtime, primeiro(argumentos), ambiente, erro);
        SefValor interno =
            avaliar_quasiquote(runtime, primeiro(argumentos), ambiente, profundidade - 1, erro);
        return interno == NULL ? NULL : lista_unaria(runtime, "UNQUOTE", interno, erro);
    }
    if (sef_simbolo_tem_nome(operador, "QUASIQUOTE")) {
        if (!contar_exato(runtime, argumentos, 1, "QUASIQUOTE", erro))
            return NULL;
        SefValor interno =
            avaliar_quasiquote(runtime, primeiro(argumentos), ambiente, profundidade + 1, erro);
        return interno == NULL ? NULL : lista_unaria(runtime, "QUASIQUOTE", interno, erro);
    }
    if (sef_simbolo_tem_nome(operador, "UNQUOTE-SPLICING") && profundidade == 1) {
        sef_erro_definir(erro, 0, 0, "UNQUOTE-SPLICING so pode aparecer dentro de uma lista");
        return NULL;
    }

    SefValor resultado = runtime->nulo;
    SefValor ultima = NULL;
    SefValor cursor = forma;
    while (cursor != runtime->nulo && cursor->tipo == SEF_TIPO_PAR) {
        SefValor item = primeiro(cursor);
        if (profundidade == 1 && item != runtime->nulo && item->tipo == SEF_TIPO_PAR &&
            sef_simbolo_tem_nome(primeiro(item), "UNQUOTE-SPLICING")) {
            SefValor itens_splice = resto(item);
            if (!contar_exato(runtime, itens_splice, 1, "UNQUOTE-SPLICING", erro))
                return NULL;
            SefValor avaliados = sef_avaliar(runtime, primeiro(itens_splice), ambiente, erro);
            if (avaliados == NULL)
                return NULL;
            if (!sef_e_lista_propria(runtime, avaliados)) {
                sef_erro_definir(erro, 0, 0, "UNQUOTE-SPLICING exige uma lista");
                return NULL;
            }
            while (avaliados != runtime->nulo) {
                SefValor celula = sef_par_novo(runtime, primeiro(avaliados), runtime->nulo, erro);
                if (celula == NULL)
                    return NULL;
                if (resultado == runtime->nulo)
                    resultado = celula;
                else
                    ultima->como.par.resto = celula;
                ultima = celula;
                avaliados = resto(avaliados);
            }
        } else {
            SefValor expandido = avaliar_quasiquote(runtime, item, ambiente, profundidade, erro);
            if (expandido == NULL)
                return NULL;
            SefValor celula = sef_par_novo(runtime, expandido, runtime->nulo, erro);
            if (celula == NULL)
                return NULL;
            if (resultado == runtime->nulo)
                resultado = celula;
            else
                ultima->como.par.resto = celula;
            ultima = celula;
        }
        cursor = resto(cursor);
    }
    if (cursor != runtime->nulo) {
        SefValor cauda = avaliar_quasiquote(runtime, cursor, ambiente, profundidade, erro);
        if (cauda == NULL)
            return NULL;
        if (ultima == NULL)
            return cauda;
        ultima->como.par.resto = cauda;
    }
    return resultado;
}

static SefValor especial_quasiquote(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                                    SefErro *erro) {
    if (!contar_exato(runtime, argumentos, 1, "QUASIQUOTE", erro))
        return NULL;
    SefValor resultado = avaliar_quasiquote(runtime, primeiro(argumentos), ambiente, 1, erro);
    return resultado == NULL ? NULL : valor_unico(runtime, resultado, erro);
}

static SefValor especial_if(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                            SefErro *erro) {
    bool propria = false;
    size_t quantidade = sef_lista_tamanho(runtime, argumentos, &propria);
    if (!propria || (quantidade != 2 && quantidade != 3)) {
        sef_erro_definir(erro, 0, 0, "IF espera teste, consequente e alternativa opcional");
        return NULL;
    }
    SefValor teste = sef_avaliar(runtime, primeiro(argumentos), ambiente, erro);
    if (teste == NULL)
        return NULL;
    argumentos = resto(argumentos);
    if (teste != runtime->nulo) {
        return sef_avaliar(runtime, primeiro(argumentos), ambiente, erro);
    }
    argumentos = resto(argumentos);
    return argumentos == runtime->nulo ? valor_unico(runtime, runtime->nulo, erro)
                                       : sef_avaliar(runtime, primeiro(argumentos), ambiente, erro);
}

static SefValor especial_lambda(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                                bool macro, SefErro *erro) {
    if (argumentos == runtime->nulo || argumentos->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "LAMBDA exige parametros e corpo");
        return NULL;
    }
    SefValor parametros = primeiro(argumentos);
    if (!exigir_lista(runtime, parametros, "lista de parametros", erro))
        return NULL;
    return sef_funcao_nova(runtime, parametros, resto(argumentos), ambiente, macro, erro);
}

static SefValor especial_definicao(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                                   bool macro, SefErro *erro) {
    if (argumentos == runtime->nulo || resto(argumentos) == runtime->nulo) {
        sef_erro_definir(erro, 0, 0, "%s exige nome, parametros e corpo",
                         macro ? "DEFMACRO" : "DEFUN");
        return NULL;
    }
    SefValor nome = primeiro(argumentos);
    if (nome->tipo != SEF_TIPO_SIMBOLO) {
        sef_erro_definir(erro, 0, 0, "nome da definicao deve ser simbolo");
        return NULL;
    }
    SefValor funcao = especial_lambda(runtime, resto(argumentos), ambiente, macro, erro);
    if (funcao == NULL)
        return NULL;
    if (!sef_ambiente_definir_funcao(runtime, runtime->ambiente_global, nome, funcao, erro)) {
        return NULL;
    }
    return nome;
}

static SefValor especial_define(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                                SefErro *erro) {
    if (!contar_exato(runtime, argumentos, 2, "DEFINE", erro))
        return NULL;
    SefValor nome = primeiro(argumentos);
    if (!exigir_nome_variavel(runtime, nome, "DEFINE", erro)) {
        return NULL;
    }
    SefValor valor = sef_avaliar(runtime, primeiro(resto(argumentos)), ambiente, erro);
    if (valor == NULL)
        return NULL;
    return sef_ambiente_definir(runtime, runtime->ambiente_global, nome, valor, erro)
               ? valor_unico(runtime, nome, erro)
               : NULL;
}

static SefValor especial_variavel_global(SefRuntime *runtime, SefValor argumentos,
                                         SefValor ambiente, bool sempre_atribuir, SefErro *erro) {
    bool propria = false;
    size_t quantidade = sef_lista_tamanho(runtime, argumentos, &propria);
    if (!propria || quantidade < 1 || quantidade > 3 || (sempre_atribuir && quantidade < 2)) {
        sef_erro_definir(erro, 0, 0, "%s possui lista de argumentos invalida",
                         sempre_atribuir ? "DEFPARAMETER" : "DEFVAR");
        return NULL;
    }
    SefValor nome = primeiro(argumentos);
    if (!exigir_nome_variavel(runtime, nome, "definicao de variavel global", erro)) {
        return NULL;
    }
    SefValor existente;
    if (!sempre_atribuir && sef_ambiente_obter(runtime->ambiente_global, nome, &existente))
        return nome;
    SefValor valor = runtime->nulo;
    if (quantidade >= 2) {
        valor = sef_avaliar(runtime, primeiro(resto(argumentos)), ambiente, erro);
        if (valor == NULL)
            return NULL;
    }
    return sef_ambiente_definir(runtime, runtime->ambiente_global, nome, valor, erro)
               ? valor_unico(runtime, nome, erro)
               : NULL;
}

static SefValor especial_setq(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                              SefErro *erro) {
    bool propria = false;
    size_t quantidade = sef_lista_tamanho(runtime, argumentos, &propria);
    if (!propria || quantidade == 0 || quantidade % 2 != 0) {
        sef_erro_definir(erro, 0, 0, "SETQ exige pares de simbolo e forma");
        return NULL;
    }
    SefValor resultado = runtime->nulo;
    while (argumentos != runtime->nulo) {
        SefValor nome = primeiro(argumentos);
        argumentos = resto(argumentos);
        if (!exigir_nome_variavel(runtime, nome, "SETQ", erro)) {
            return NULL;
        }
        resultado = sef_avaliar(runtime, primeiro(argumentos), ambiente, erro);
        if (resultado == NULL)
            return NULL;
        if (!sef_ambiente_atribuir(ambiente, nome, resultado)) {
            sef_erro_definir(erro, 0, 0, "simbolo %s nao esta vinculado", nome->como.simbolo.nome);
            return NULL;
        }
        argumentos = resto(argumentos);
    }
    return valor_unico(runtime, resultado, erro);
}

static SefValor atribuir_lugar(SefRuntime *runtime, SefValor lugar, SefValor forma_valor,
                               SefValor ambiente, SefErro *erro) {
    if (lugar->tipo == SEF_TIPO_SIMBOLO) {
        if (!exigir_nome_variavel(runtime, lugar, "SETF", erro))
            return NULL;
        SefValor valor = sef_avaliar(runtime, forma_valor, ambiente, erro);
        if (valor == NULL)
            return NULL;
        if (!sef_ambiente_atribuir(ambiente, lugar, valor)) {
            sef_erro_definir(erro, 0, 0, "simbolo %s nao esta vinculado", lugar->como.simbolo.nome);
            return NULL;
        }
        return valor;
    }
    if (lugar == runtime->nulo || lugar->tipo != SEF_TIPO_PAR ||
        !sef_e_lista_propria(runtime, lugar)) {
        sef_erro_definir(erro, 0, 0, "lugar invalido em SETF");
        return NULL;
    }

    SefValor operador = primeiro(lugar);
    SefValor argumentos = resto(lugar);
    if (operador->tipo != SEF_TIPO_SIMBOLO) {
        sef_erro_definir(erro, 0, 0, "operador de lugar invalido em SETF");
        return NULL;
    }
    bool lugar_aref =
        sef_simbolo_tem_nome(operador, "AREF") || sef_simbolo_tem_nome(operador, "SVREF");
    bool lugar_texto =
        sef_simbolo_tem_nome(operador, "CHAR") || sef_simbolo_tem_nome(operador, "SCHAR");
    bool lugar_elt = sef_simbolo_tem_nome(operador, "ELT");
    if (sef_simbolo_tem_nome(operador, "GETHASH")) {
        if (!contar_exato(runtime, argumentos, 2, "lugar GETHASH de SETF", erro))
            return NULL;
        SefValor chave = sef_avaliar(runtime, primeiro(argumentos), ambiente, erro);
        SefValor tabela = chave == NULL
                              ? NULL
                              : sef_avaliar(runtime, primeiro(resto(argumentos)), ambiente, erro);
        SefValor valor = tabela == NULL ? NULL : sef_avaliar(runtime, forma_valor, ambiente, erro);
        return valor != NULL && sef_tabela_hash_definir(runtime, tabela, chave, valor, erro) ? valor
                                                                                             : NULL;
    }
    if (lugar_aref || lugar_texto || lugar_elt) {
        if (!contar_exato(runtime, argumentos, 2, "lugar indexado de SETF", erro))
            return NULL;
        SefValor sequencia = sef_avaliar(runtime, primeiro(argumentos), ambiente, erro);
        if (sequencia == NULL)
            return NULL;
        SefValor indice = sef_avaliar(runtime, primeiro(resto(argumentos)), ambiente, erro);
        if (indice == NULL)
            return NULL;
        SefValor valor = sef_avaliar(runtime, forma_valor, ambiente, erro);
        if (valor == NULL)
            return NULL;
        if (indice->tipo != SEF_TIPO_INTEIRO || indice->como.inteiro < 0 ||
            (uint64_t)indice->como.inteiro > SIZE_MAX) {
            sef_erro_definir(erro, 0, 0, "SETF indexado exige indice inteiro nao negativo");
            return NULL;
        }
        size_t posicao = (size_t)indice->como.inteiro;
        if (lugar_aref && sequencia->tipo != SEF_TIPO_VETOR) {
            sef_erro_definir(erro, 0, 0, "SETF de AREF exige um vetor");
            return NULL;
        }
        if (lugar_texto && sequencia->tipo != SEF_TIPO_TEXTO) {
            sef_erro_definir(erro, 0, 0, "SETF de CHAR exige uma string");
            return NULL;
        }
        if ((lugar_aref || (lugar_elt && sequencia->tipo == SEF_TIPO_VETOR)) &&
            posicao >= sequencia->como.vetor.tamanho) {
            sef_erro_definir(erro, 0, 0, "indice fora dos limites em SETF de AREF");
            return NULL;
        }
        if (lugar_aref || (lugar_elt && sequencia->tipo == SEF_TIPO_VETOR)) {
            sequencia->como.vetor.itens[posicao] = valor;
            return valor;
        }
        if (lugar_texto || (lugar_elt && sequencia->tipo == SEF_TIPO_TEXTO))
            return sef_texto_caractere_definir(runtime, sequencia, posicao, valor, erro) ? valor
                                                                                         : NULL;
        if (lugar_elt) {
            SefValor cursor = sequencia;
            for (size_t i = 0; i < posicao; i++) {
                if (cursor == runtime->nulo || cursor->tipo != SEF_TIPO_PAR) {
                    sef_erro_definir(erro, 0, 0,
                                     "indice fora dos limites ou lista impropria em SETF de ELT");
                    return NULL;
                }
                cursor = resto(cursor);
            }
            if (cursor == runtime->nulo || cursor->tipo != SEF_TIPO_PAR) {
                sef_erro_definir(erro, 0, 0, "SETF de ELT exige uma sequencia e indice valido");
                return NULL;
            }
            cursor->como.par.primeiro = valor;
            return valor;
        }
    }
    if (sef_simbolo_tem_nome(operador, "CAR") || sef_simbolo_tem_nome(operador, "CDR")) {
        if (!contar_exato(runtime, argumentos, 1, "lugar de lista em SETF", erro))
            return NULL;
        SefValor par = sef_avaliar(runtime, primeiro(argumentos), ambiente, erro);
        if (par == NULL)
            return NULL;
        SefValor valor = sef_avaliar(runtime, forma_valor, ambiente, erro);
        if (valor == NULL)
            return NULL;
        if (par->tipo != SEF_TIPO_PAR) {
            sef_erro_definir(erro, 0, 0, "SETF de CAR ou CDR exige um par");
            return NULL;
        }
        if (sef_simbolo_tem_nome(operador, "CAR"))
            par->como.par.primeiro = valor;
        else
            par->como.par.resto = valor;
        return valor;
    }
    sef_erro_definir(erro, 0, 0, "lugar ainda nao suportado por SETF: %s",
                     operador->como.simbolo.nome);
    return NULL;
}

static SefValor especial_setf(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                              SefErro *erro) {
    bool propria = false;
    size_t quantidade = sef_lista_tamanho(runtime, argumentos, &propria);
    if (!propria || quantidade == 0 || quantidade % 2 != 0) {
        sef_erro_definir(erro, 0, 0, "SETF exige pares de lugar e forma");
        return NULL;
    }
    SefValor resultado = runtime->nulo;
    while (argumentos != runtime->nulo) {
        SefValor lugar = primeiro(argumentos);
        argumentos = resto(argumentos);
        resultado = atribuir_lugar(runtime, lugar, primeiro(argumentos), ambiente, erro);
        if (resultado == NULL)
            return NULL;
        argumentos = resto(argumentos);
    }
    return valor_unico(runtime, resultado, erro);
}

static SefValor especial_let(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                             bool sequencial, SefErro *erro) {
    if (argumentos == runtime->nulo || argumentos->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "LET exige vinculos e corpo");
        return NULL;
    }
    SefValor descricoes = primeiro(argumentos);
    if (!exigir_lista(runtime, descricoes, "vinculos de LET", erro))
        return NULL;
    SefValor novo = sef_ambiente_novo(runtime, ambiente, erro);
    if (novo == NULL)
        return NULL;

    while (descricoes != runtime->nulo) {
        SefValor descricao = primeiro(descricoes);
        SefValor nome;
        SefValor valor = runtime->nulo;
        if (descricao->tipo == SEF_TIPO_SIMBOLO) {
            nome = descricao;
        } else if (descricao->tipo == SEF_TIPO_PAR && sef_e_lista_propria(runtime, descricao)) {
            bool propria = false;
            size_t tamanho = sef_lista_tamanho(runtime, descricao, &propria);
            if (!propria || tamanho < 1 || tamanho > 2) {
                sef_erro_definir(erro, 0, 0, "vinculo LET invalido");
                return NULL;
            }
            nome = primeiro(descricao);
            if (tamanho == 2) {
                valor = sef_avaliar(runtime, primeiro(resto(descricao)),
                                    sequencial ? novo : ambiente, erro);
                if (valor == NULL)
                    return NULL;
            }
        } else {
            sef_erro_definir(erro, 0, 0, "vinculo LET invalido");
            return NULL;
        }
        if (!exigir_nome_variavel(runtime, nome, "LET", erro) ||
            !sef_ambiente_definir(runtime, novo, nome, valor, erro))
            return NULL;
        descricoes = resto(descricoes);
    }
    return avaliar_sequencia(runtime, resto(argumentos), novo, erro);
}

static SefValor especial_multiple_value_bind(SefRuntime *runtime, SefValor argumentos,
                                             SefValor ambiente, SefErro *erro) {
    bool propria = false;
    size_t quantidade = sef_lista_tamanho(runtime, argumentos, &propria);
    if (!propria || quantidade < 2) {
        sef_erro_definir(erro, 0, 0,
                         "MULTIPLE-VALUE-BIND exige variaveis, forma de valores e corpo");
        return NULL;
    }
    SefValor variaveis = primeiro(argumentos);
    if (!exigir_lista(runtime, variaveis, "variaveis de MULTIPLE-VALUE-BIND", erro))
        return NULL;

    SefValor forma_valores = primeiro(resto(argumentos));
    if (sef_avaliar(runtime, forma_valores, ambiente, erro) == NULL)
        return NULL;
    SefValoresSalvos salvos = {0};
    if (!sef_valores_salvar(runtime, &salvos, erro))
        return NULL;

    SefValor local = sef_ambiente_novo(runtime, ambiente, erro);
    if (local == NULL) {
        sef_valores_salvos_liberar(&salvos);
        return NULL;
    }
    size_t indice = 0;
    while (variaveis != runtime->nulo) {
        SefValor nome = primeiro(variaveis);
        if (!exigir_nome_variavel(runtime, nome, "MULTIPLE-VALUE-BIND", erro)) {
            sef_valores_salvos_liberar(&salvos);
            return NULL;
        }
        SefValor valor = indice < salvos.quantidade ? salvos.itens[indice] : runtime->nulo;
        if (!sef_ambiente_definir(runtime, local, nome, valor, erro)) {
            sef_valores_salvos_liberar(&salvos);
            return NULL;
        }
        indice++;
        variaveis = resto(variaveis);
    }
    sef_valores_salvos_liberar(&salvos);
    return avaliar_sequencia(runtime, resto(resto(argumentos)), local, erro);
}

static SefValor especial_multiple_value_list(SefRuntime *runtime, SefValor argumentos,
                                             SefValor ambiente, SefErro *erro) {
    if (!contar_exato(runtime, argumentos, 1, "MULTIPLE-VALUE-LIST", erro))
        return NULL;
    if (sef_avaliar(runtime, primeiro(argumentos), ambiente, erro) == NULL)
        return NULL;

    SefValor lista = runtime->nulo;
    for (size_t i = runtime->quantidade_valores; i > 0; i--) {
        lista = sef_par_novo(runtime, runtime->valores_multiplos[i - 1], lista, erro);
        if (lista == NULL)
            return NULL;
    }
    return valor_unico(runtime, lista, erro);
}

static SefValor especial_multiple_value_prog1(SefRuntime *runtime, SefValor argumentos,
                                              SefValor ambiente, SefErro *erro) {
    if (argumentos == runtime->nulo || argumentos->tipo != SEF_TIPO_PAR ||
        !sef_e_lista_propria(runtime, argumentos)) {
        sef_erro_definir(erro, 0, 0, "MULTIPLE-VALUE-PROG1 exige ao menos uma forma");
        return NULL;
    }
    if (sef_avaliar(runtime, primeiro(argumentos), ambiente, erro) == NULL)
        return NULL;
    SefValoresSalvos salvos = {0};
    if (!sef_valores_salvar(runtime, &salvos, erro))
        return NULL;
    if (resto(argumentos) != runtime->nulo &&
        avaliar_sequencia(runtime, resto(argumentos), ambiente, erro) == NULL) {
        sef_valores_salvos_liberar(&salvos);
        return NULL;
    }
    bool restaurou = sef_valores_restaurar(runtime, &salvos, erro);
    sef_valores_salvos_liberar(&salvos);
    return restaurou ? sef_valores_primario(runtime) : NULL;
}

static SefValor especial_multiple_value_call(SefRuntime *runtime, SefValor argumentos,
                                             SefValor ambiente, SefErro *erro) {
    if (argumentos == runtime->nulo || argumentos->tipo != SEF_TIPO_PAR ||
        !sef_e_lista_propria(runtime, argumentos)) {
        sef_erro_definir(erro, 0, 0, "MULTIPLE-VALUE-CALL exige uma forma de funcao");
        return NULL;
    }
    SefValor funcao = sef_avaliar(runtime, primeiro(argumentos), ambiente, erro);
    if (funcao == NULL)
        return NULL;

    SefValor valores = runtime->nulo;
    SefValor ultima = NULL;
    for (SefValor formas = resto(argumentos); formas != runtime->nulo; formas = resto(formas)) {
        if (sef_avaliar(runtime, primeiro(formas), ambiente, erro) == NULL)
            return NULL;
        for (size_t i = 0; i < runtime->quantidade_valores; i++) {
            SefValor celula =
                sef_par_novo(runtime, runtime->valores_multiplos[i], runtime->nulo, erro);
            if (celula == NULL)
                return NULL;
            if (valores == runtime->nulo)
                valores = celula;
            else
                ultima->como.par.resto = celula;
            ultima = celula;
        }
    }
    return sef_aplicar(runtime, funcao, valores, erro);
}

static SefValor especial_nth_value(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                                   SefErro *erro) {
    if (!contar_exato(runtime, argumentos, 2, "NTH-VALUE", erro))
        return NULL;
    SefValor indice = sef_avaliar(runtime, primeiro(argumentos), ambiente, erro);
    if (indice == NULL)
        return NULL;
    if (indice->tipo != SEF_TIPO_INTEIRO || indice->como.inteiro < 0 ||
        (uint64_t)indice->como.inteiro > SIZE_MAX) {
        sef_erro_definir(erro, 0, 0, "NTH-VALUE exige indice inteiro nao negativo");
        return NULL;
    }
    if (sef_avaliar(runtime, primeiro(resto(argumentos)), ambiente, erro) == NULL)
        return NULL;
    size_t posicao = (size_t)indice->como.inteiro;
    SefValor valor =
        posicao < runtime->quantidade_valores ? runtime->valores_multiplos[posicao] : runtime->nulo;
    return valor_unico(runtime, valor, erro);
}

static SefValor especial_cond(SefRuntime *runtime, SefValor clausulas, SefValor ambiente,
                              SefErro *erro) {
    if (!exigir_lista(runtime, clausulas, "COND", erro))
        return NULL;
    while (clausulas != runtime->nulo) {
        SefValor clausula = primeiro(clausulas);
        if (clausula == runtime->nulo || clausula->tipo != SEF_TIPO_PAR ||
            !sef_e_lista_propria(runtime, clausula)) {
            sef_erro_definir(erro, 0, 0, "clausula COND invalida");
            return NULL;
        }
        SefValor teste = sef_avaliar(runtime, primeiro(clausula), ambiente, erro);
        if (teste == NULL)
            return NULL;
        SefValor consequentes = resto(clausula);
        if (teste != runtime->nulo) {
            return consequentes == runtime->nulo
                       ? teste
                       : avaliar_sequencia(runtime, consequentes, ambiente, erro);
        }
        clausulas = resto(clausulas);
    }
    return valor_unico(runtime, runtime->nulo, erro);
}

static SefValor especial_when(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                              bool quando_verdadeiro, SefErro *erro) {
    if (argumentos == runtime->nulo || argumentos->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "%s exige um teste", quando_verdadeiro ? "WHEN" : "UNLESS");
        return NULL;
    }
    SefValor teste = sef_avaliar(runtime, primeiro(argumentos), ambiente, erro);
    if (teste == NULL)
        return NULL;
    bool executar = quando_verdadeiro ? teste != runtime->nulo : teste == runtime->nulo;
    return executar ? avaliar_sequencia(runtime, resto(argumentos), ambiente, erro)
                    : valor_unico(runtime, runtime->nulo, erro);
}

static bool controle_eql(SefValor a, SefValor b) {
    if (a == b)
        return true;
    if (a->tipo == SEF_TIPO_INTEIRO && b->tipo == SEF_TIPO_INTEIRO)
        return a->como.inteiro == b->como.inteiro;
    if (a->tipo == SEF_TIPO_REAL && b->tipo == SEF_TIPO_REAL)
        return a->como.real == b->como.real;
    return false;
}

static void reinicios_descartar_ate(SefRuntime *runtime, SefReinicioDinamico *limite) {
    while (runtime->reinicios != limite) {
        SefReinicioDinamico *reinicio = runtime->reinicios;
        if (reinicio == NULL)
            return;
        runtime->reinicios = reinicio->anterior;
        free(reinicio);
    }
}

static void handlers_descartar_ate(SefRuntime *runtime, SefHandlerDinamico *limite) {
    while (runtime->handlers != limite) {
        SefHandlerDinamico *handler = runtime->handlers;
        if (handler == NULL)
            return;
        runtime->handlers = handler->anterior;
        free(handler);
    }
    runtime->handlers_visiveis = runtime->handlers;
}

static void transferencia_limpar(SefRuntime *runtime) {
    sef_valores_salvos_liberar(&runtime->valores_transferencia);
    runtime->destino_transferencia = NULL;
    runtime->valor_transferencia = NULL;
    runtime->parametros_transferencia = NULL;
    runtime->corpo_transferencia = NULL;
    runtime->ambiente_transferencia = NULL;
}

static void transferir_controle(SefRuntime *runtime, SefQuadroControle *destino) {
    for (SefQuadroControle *quadro = runtime->controle; quadro != NULL && quadro != destino;
         quadro = quadro->anterior) {
        if (quadro->tipo == SEF_CONTROLE_LIMPEZA) {
            reinicios_descartar_ate(runtime, quadro->reinicios_anteriores);
            handlers_descartar_ate(runtime, quadro->handlers_anteriores);
            runtime->controle = quadro;
            longjmp(quadro->salto, 1);
        }
    }
    reinicios_descartar_ate(runtime, destino->reinicios_anteriores);
    handlers_descartar_ate(runtime, destino->handlers_anteriores);
    runtime->controle = destino;
    longjmp(destino->salto, 1);
}

static bool iniciar_transferencia(SefRuntime *runtime, SefQuadroControle *destino, SefValor valor,
                                  SefErro *erro) {
    sef_valores_salvos_liberar(&runtime->valores_transferencia);
    if (!sef_valores_salvar(runtime, &runtime->valores_transferencia, erro))
        return false;
    runtime->destino_transferencia = destino;
    runtime->valor_transferencia = valor;
    return true;
}

static SefValor executar_com_controle(SefRuntime *runtime, SefTipoControle tipo,
                                      SefValor nome_ou_etiqueta, SefValor corpo, SefValor ambiente,
                                      SefErro *erro) {
    SefQuadroControle quadro;
    quadro.tipo = tipo;
    quadro.nome_ou_etiqueta = nome_ou_etiqueta;
    quadro.anterior = runtime->controle;
    quadro.reinicios_anteriores = runtime->reinicios;
    quadro.handlers_anteriores = runtime->handlers;
    runtime->controle = &quadro;
    int transferencia = setjmp(quadro.salto);
    if (transferencia == 0) {
        SefValor resultado = avaliar_sequencia(runtime, corpo, ambiente, erro);
        runtime->controle = quadro.anterior;
        return resultado;
    }
    runtime->controle = quadro.anterior;
    SefValor resultado = runtime->valor_transferencia;
    bool restaurou = sef_valores_restaurar(runtime, &runtime->valores_transferencia, erro);
    transferencia_limpar(runtime);
    return restaurou ? resultado : NULL;
}

static SefValor especial_block(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                               SefErro *erro) {
    if (argumentos == runtime->nulo || argumentos->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "BLOCK exige um nome");
        return NULL;
    }
    SefValor nome = primeiro(argumentos);
    if (nome != runtime->nulo && nome->tipo != SEF_TIPO_SIMBOLO) {
        sef_erro_definir(erro, 0, 0, "nome de BLOCK deve ser simbolo ou NIL");
        return NULL;
    }
    return executar_com_controle(runtime, SEF_CONTROLE_BLOCO, nome, resto(argumentos), ambiente,
                                 erro);
}

static SefValor especial_return_from(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                                     SefErro *erro) {
    bool propria = false;
    size_t quantidade = sef_lista_tamanho(runtime, argumentos, &propria);
    if (!propria || quantidade < 1 || quantidade > 2) {
        sef_erro_definir(erro, 0, 0, "RETURN-FROM exige nome e valor opcional");
        return NULL;
    }
    SefValor nome = primeiro(argumentos);
    if (nome != runtime->nulo && nome->tipo != SEF_TIPO_SIMBOLO) {
        sef_erro_definir(erro, 0, 0, "nome de RETURN-FROM deve ser simbolo ou NIL");
        return NULL;
    }
    SefValor valor = quantidade == 2
                         ? sef_avaliar(runtime, primeiro(resto(argumentos)), ambiente, erro)
                         : valor_unico(runtime, runtime->nulo, erro);
    if (valor == NULL)
        return NULL;
    for (SefQuadroControle *quadro = runtime->controle; quadro != NULL; quadro = quadro->anterior) {
        if (quadro->tipo == SEF_CONTROLE_BLOCO && quadro->nome_ou_etiqueta == nome) {
            if (!iniciar_transferencia(runtime, quadro, valor, erro))
                return NULL;
            transferir_controle(runtime, quadro);
        }
    }
    sef_erro_definir(erro, 0, 0, "nao existe BLOCK ativo com esse nome");
    return NULL;
}

static SefValor especial_catch(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                               SefErro *erro) {
    if (argumentos == runtime->nulo || argumentos->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "CATCH exige uma etiqueta");
        return NULL;
    }
    SefValor etiqueta = sef_avaliar(runtime, primeiro(argumentos), ambiente, erro);
    if (etiqueta == NULL)
        return NULL;
    return executar_com_controle(runtime, SEF_CONTROLE_CAPTURA, etiqueta, resto(argumentos),
                                 ambiente, erro);
}

static SefValor especial_throw(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                               SefErro *erro) {
    if (!contar_exato(runtime, argumentos, 2, "THROW", erro))
        return NULL;
    SefValor etiqueta = sef_avaliar(runtime, primeiro(argumentos), ambiente, erro);
    if (etiqueta == NULL)
        return NULL;
    SefValor valor = sef_avaliar(runtime, primeiro(resto(argumentos)), ambiente, erro);
    if (valor == NULL)
        return NULL;
    for (SefQuadroControle *quadro = runtime->controle; quadro != NULL; quadro = quadro->anterior) {
        if (quadro->tipo == SEF_CONTROLE_CAPTURA &&
            controle_eql(quadro->nome_ou_etiqueta, etiqueta)) {
            if (!iniciar_transferencia(runtime, quadro, valor, erro))
                return NULL;
            transferir_controle(runtime, quadro);
        }
    }
    sef_erro_definir(erro, 0, 0, "nenhum CATCH ativo aceita essa etiqueta");
    return NULL;
}

static SefValor especial_unwind_protect(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                                        SefErro *erro) {
    if (argumentos == runtime->nulo || argumentos->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "UNWIND-PROTECT exige uma forma protegida");
        return NULL;
    }
    SefValor protegida = primeiro(argumentos);
    SefValor limpezas = resto(argumentos);
    SefQuadroControle quadro;
    quadro.tipo = SEF_CONTROLE_LIMPEZA;
    quadro.nome_ou_etiqueta = runtime->nulo;
    quadro.anterior = runtime->controle;
    quadro.reinicios_anteriores = runtime->reinicios;
    quadro.handlers_anteriores = runtime->handlers;
    runtime->controle = &quadro;
    int transferencia = setjmp(quadro.salto);
    if (transferencia == 0) {
        SefValor resultado = sef_avaliar(runtime, protegida, ambiente, erro);
        SefValoresSalvos salvos = {0};
        bool preservou = resultado != NULL && sef_valores_salvar(runtime, &salvos, erro);
        runtime->controle = quadro.anterior;
        SefErro erro_limpeza;
        sef_erro_limpar(&erro_limpeza);
        SefValor resultado_limpeza = avaliar_sequencia(runtime, limpezas, ambiente, &erro_limpeza);
        if (resultado_limpeza == NULL && erro_limpeza.ocorreu) {
            sef_valores_salvos_liberar(&salvos);
            *erro = erro_limpeza;
            return NULL;
        }
        if (resultado == NULL || !preservou) {
            sef_valores_salvos_liberar(&salvos);
            return NULL;
        }
        bool restaurou = sef_valores_restaurar(runtime, &salvos, erro);
        sef_valores_salvos_liberar(&salvos);
        return restaurou ? resultado : NULL;
    }

    SefQuadroControle *destino = runtime->destino_transferencia;
    runtime->controle = quadro.anterior;
    SefErro erro_limpeza;
    sef_erro_limpar(&erro_limpeza);
    if (avaliar_sequencia(runtime, limpezas, ambiente, &erro_limpeza) == NULL &&
        erro_limpeza.ocorreu) {
        transferencia_limpar(runtime);
        *erro = erro_limpeza;
        return NULL;
    }
    transferir_controle(runtime, destino);
    return NULL;
}

static bool clausula_reinicio_analisar(SefRuntime *runtime, SefValor clausula, SefValor *nome,
                                       SefValor *parametros, SefValor *corpo, SefErro *erro) {
    bool propria = false;
    if (clausula == runtime->nulo || clausula->tipo != SEF_TIPO_PAR ||
        sef_lista_tamanho(runtime, clausula, &propria) < 2 || !propria) {
        sef_erro_definir(erro, 0, 0, "clausula RESTART-CASE invalida");
        return false;
    }
    *nome = primeiro(clausula);
    if (!sef_valor_e_simbolo_logico(runtime, *nome)) {
        sef_erro_definir(erro, 0, 0, "nome de reinicio deve ser simbolo ou NIL");
        return false;
    }
    SefValor cauda = resto(clausula);
    *parametros = primeiro(cauda);
    if (!sef_e_lista_propria(runtime, *parametros)) {
        sef_erro_definir(erro, 0, 0, "parametros de reinicio devem formar lista propria");
        return false;
    }
    *corpo = resto(cauda);
    if (*corpo != runtime->nulo && (*corpo)->tipo == SEF_TIPO_PAR) {
        SefValor primeira_forma = primeiro(*corpo);
        bool opcao_keyword = primeira_forma != runtime->nulo &&
                             primeira_forma->tipo == SEF_TIPO_SIMBOLO &&
                             primeira_forma->como.simbolo.pacote == runtime->pacote_keyword;
        if (opcao_keyword && (sef_simbolo_tem_nome(primeira_forma, "INTERACTIVE") ||
                              sef_simbolo_tem_nome(primeira_forma, "REPORT") ||
                              sef_simbolo_tem_nome(primeira_forma, "TEST"))) {
            sef_erro_definir(erro, 0, 0,
                             "opcoes :INTERACTIVE, :REPORT e :TEST ainda nao sao suportadas");
            return false;
        }
    }
    return true;
}

static SefValor especial_restart_case(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                                      SefErro *erro) {
    if (argumentos == runtime->nulo || argumentos->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "RESTART-CASE exige uma forma protegida");
        return NULL;
    }
    if (!sef_e_lista_propria(runtime, argumentos)) {
        sef_erro_definir(erro, 0, 0, "RESTART-CASE exige argumentos proprios");
        return NULL;
    }

    SefReinicioDinamico *limite = runtime->reinicios;
    SefQuadroControle quadro;
    quadro.tipo = SEF_CONTROLE_REINICIO;
    quadro.nome_ou_etiqueta = runtime->nulo;
    quadro.anterior = runtime->controle;
    quadro.reinicios_anteriores = limite;
    quadro.handlers_anteriores = runtime->handlers;

    SefReinicioDinamico *primeiro_reinicio = NULL;
    SefReinicioDinamico *ultimo_reinicio = NULL;
    for (SefValor clausulas = resto(argumentos); clausulas != runtime->nulo;
         clausulas = resto(clausulas)) {
        SefValor nome = NULL;
        SefValor parametros = NULL;
        SefValor corpo = NULL;
        if (!clausula_reinicio_analisar(runtime, primeiro(clausulas), &nome, &parametros, &corpo,
                                        erro)) {
            while (primeiro_reinicio != NULL) {
                SefReinicioDinamico *proximo = primeiro_reinicio->anterior;
                free(primeiro_reinicio);
                primeiro_reinicio = proximo;
            }
            return NULL;
        }
        SefReinicioDinamico *reinicio = calloc(1, sizeof(*reinicio));
        if (reinicio == NULL) {
            while (primeiro_reinicio != NULL) {
                SefReinicioDinamico *proximo = primeiro_reinicio->anterior;
                free(primeiro_reinicio);
                primeiro_reinicio = proximo;
            }
            sef_erro_definir(erro, 0, 0, "memoria insuficiente para registrar reinicio");
            return NULL;
        }
        reinicio->objeto = sef_reinicio_novo(runtime, nome, erro);
        if (reinicio->objeto == NULL) {
            free(reinicio);
            while (primeiro_reinicio != NULL) {
                SefReinicioDinamico *proximo = primeiro_reinicio->anterior;
                free(primeiro_reinicio);
                primeiro_reinicio = proximo;
            }
            return NULL;
        }
        reinicio->parametros = parametros;
        reinicio->corpo = corpo;
        reinicio->ambiente = ambiente;
        reinicio->destino = &quadro;
        if (ultimo_reinicio == NULL)
            primeiro_reinicio = reinicio;
        else
            ultimo_reinicio->anterior = reinicio;
        ultimo_reinicio = reinicio;
    }
    if (ultimo_reinicio != NULL)
        ultimo_reinicio->anterior = limite;
    runtime->reinicios = primeiro_reinicio == NULL ? limite : primeiro_reinicio;
    runtime->controle = &quadro;

    int transferencia = setjmp(quadro.salto);
    if (transferencia == 0) {
        SefValor resultado = sef_avaliar(runtime, primeiro(argumentos), ambiente, erro);
        runtime->controle = quadro.anterior;
        reinicios_descartar_ate(runtime, limite);
        return resultado;
    }

    runtime->controle = quadro.anterior;
    SefRaiz *raiz_parametros = sef_raiz_criar(runtime, runtime->parametros_transferencia, erro);
    SefRaiz *raiz_corpo = raiz_parametros == NULL
                              ? NULL
                              : sef_raiz_criar(runtime, runtime->corpo_transferencia, erro);
    SefRaiz *raiz_ambiente =
        raiz_corpo == NULL ? NULL : sef_raiz_criar(runtime, runtime->ambiente_transferencia, erro);
    SefRaiz *raiz_argumentos =
        raiz_ambiente == NULL ? NULL : sef_raiz_criar(runtime, runtime->valor_transferencia, erro);
    if (raiz_argumentos == NULL) {
        sef_raiz_liberar(raiz_ambiente);
        sef_raiz_liberar(raiz_corpo);
        sef_raiz_liberar(raiz_parametros);
        transferencia_limpar(runtime);
        return NULL;
    }
    transferencia_limpar(runtime);

    SefValor local = sef_ambiente_novo(runtime, sef_raiz_valor(raiz_ambiente), erro);
    SefValor resultado = NULL;
    if (local != NULL && vincular_parametros(runtime, local, sef_raiz_valor(raiz_parametros),
                                             sef_raiz_valor(raiz_argumentos), erro)) {
        resultado = avaliar_sequencia(runtime, sef_raiz_valor(raiz_corpo), local, erro);
    }
    sef_raiz_liberar(raiz_argumentos);
    sef_raiz_liberar(raiz_ambiente);
    sef_raiz_liberar(raiz_corpo);
    sef_raiz_liberar(raiz_parametros);
    return resultado;
}

SefValor sef_reinicio_invocar(SefRuntime *runtime, SefValor designador, SefValor argumentos,
                              SefErro *erro) {
    bool por_nome = designador != runtime->nulo && designador->tipo == SEF_TIPO_SIMBOLO;
    bool por_objeto = designador != NULL && designador->tipo == SEF_TIPO_REINICIO;
    if (!por_nome && !por_objeto) {
        sef_erro_definir(erro, 0, 0,
                         "designador de reinicio deve ser simbolo nao-NIL ou objeto RESTART");
        return NULL;
    }
    for (SefReinicioDinamico *reinicio = runtime->reinicios; reinicio != NULL;
         reinicio = reinicio->anterior) {
        SefValor nome = reinicio->objeto->como.reinicio.nome;
        if ((por_nome && nome != designador) || (por_objeto && reinicio->objeto != designador))
            continue;
        if (!iniciar_transferencia(runtime, reinicio->destino, argumentos, erro))
            return NULL;
        runtime->parametros_transferencia = reinicio->parametros;
        runtime->corpo_transferencia = reinicio->corpo;
        runtime->ambiente_transferencia = reinicio->ambiente;
        transferir_controle(runtime, reinicio->destino);
    }
    sef_erro_definir(erro, 0, 0, "reinicio solicitado nao esta ativo");
    return NULL;
}

static bool handler_aceita_condicao(SefValor tipo, SefValor condicao) {
    if (sef_simbolo_tem_nome(tipo, "CONDITION"))
        return true;
    if (condicao->tipo != SEF_TIPO_CONDICAO)
        return false;
    SefValor classe = condicao->como.condicao.classe;
    return tipo == classe ||
           (sef_simbolo_tem_nome(tipo, "ERROR") && sef_simbolo_tem_nome(classe, "ERROR"));
}

bool sef_condicao_sinalizar(SefRuntime *runtime, SefValor condicao, SefErro *erro) {
    if (condicao == NULL || condicao->tipo != SEF_TIPO_CONDICAO) {
        sef_erro_definir(erro, 0, 0, "SIGNAL exige um objeto de condicao");
        return false;
    }
    SefHandlerDinamico *handler = runtime->handlers_visiveis;
    while (handler != NULL) {
        if (!handler_aceita_condicao(handler->tipo, condicao)) {
            handler = handler->anterior;
            continue;
        }
        SefHandlerDinamico *visiveis_salvos = runtime->handlers_visiveis;
        runtime->handlers_visiveis = handler->grupo_anterior;
        SefValor argumentos = sef_par_novo(runtime, condicao, runtime->nulo, erro);
        SefValor resultado =
            argumentos == NULL ? NULL : sef_aplicar(runtime, handler->funcao, argumentos, erro);
        runtime->handlers_visiveis = visiveis_salvos;
        if (resultado == NULL)
            return false;
        handler = handler->grupo_anterior;
    }
    return true;
}

static void handlers_liberar_lista(SefHandlerDinamico *primeiro_handler) {
    while (primeiro_handler != NULL) {
        SefHandlerDinamico *proximo = primeiro_handler->anterior;
        free(primeiro_handler);
        primeiro_handler = proximo;
    }
}

static SefValor especial_handler_bind(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                                      SefErro *erro) {
    if (argumentos == runtime->nulo || argumentos->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "HANDLER-BIND exige uma lista de bindings");
        return NULL;
    }
    SefValor bindings = primeiro(argumentos);
    if (!sef_e_lista_propria(runtime, bindings) || !sef_e_lista_propria(runtime, argumentos)) {
        sef_erro_definir(erro, 0, 0, "HANDLER-BIND exige listas proprias");
        return NULL;
    }

    SefHandlerDinamico *limite = runtime->handlers;
    SefHandlerDinamico *primeiro_handler = NULL;
    SefHandlerDinamico *ultimo_handler = NULL;
    for (; bindings != runtime->nulo; bindings = resto(bindings)) {
        SefValor binding = primeiro(bindings);
        bool proprio = false;
        if (binding == runtime->nulo || binding->tipo != SEF_TIPO_PAR ||
            sef_lista_tamanho(runtime, binding, &proprio) != 2 || !proprio) {
            handlers_liberar_lista(primeiro_handler);
            sef_erro_definir(erro, 0, 0, "binding de HANDLER-BIND deve ser (tipo handler)");
            return NULL;
        }
        SefValor tipo = primeiro(binding);
        if (!sef_valor_e_simbolo_logico(runtime, tipo)) {
            handlers_liberar_lista(primeiro_handler);
            sef_erro_definir(erro, 0, 0, "tipo de handler deve ser simbolo");
            return NULL;
        }
        SefValor funcao = sef_avaliar(runtime, primeiro(resto(binding)), ambiente, erro);
        if (funcao == NULL) {
            handlers_liberar_lista(primeiro_handler);
            return NULL;
        }
        if (sef_valor_e_simbolo_logico(runtime, funcao)) {
            SefValor designada = NULL;
            if (!sef_ambiente_obter_funcao(ambiente, funcao, &designada)) {
                handlers_liberar_lista(primeiro_handler);
                sef_erro_definir(erro, 0, 0, "funcao designada para handler nao existe");
                return NULL;
            }
            funcao = designada;
        }
        if (funcao->tipo != SEF_TIPO_FUNCAO && funcao->tipo != SEF_TIPO_NATIVA) {
            handlers_liberar_lista(primeiro_handler);
            sef_erro_definir(erro, 0, 0, "handler deve avaliar para uma funcao");
            return NULL;
        }
        SefHandlerDinamico *handler = calloc(1, sizeof(*handler));
        if (handler == NULL) {
            handlers_liberar_lista(primeiro_handler);
            sef_erro_definir(erro, 0, 0, "memoria insuficiente para registrar handler");
            return NULL;
        }
        handler->tipo = tipo;
        handler->funcao = funcao;
        handler->grupo_anterior = limite;
        if (ultimo_handler == NULL)
            primeiro_handler = handler;
        else
            ultimo_handler->anterior = handler;
        ultimo_handler = handler;
    }
    if (ultimo_handler != NULL)
        ultimo_handler->anterior = limite;
    runtime->handlers = primeiro_handler == NULL ? limite : primeiro_handler;
    runtime->handlers_visiveis = runtime->handlers;
    SefValor resultado = avaliar_sequencia(runtime, resto(argumentos), ambiente, erro);
    handlers_descartar_ate(runtime, limite);
    return resultado;
}

static SefValor especial_ignore_errors(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                                       SefErro *erro) {
    if (!contar_exato(runtime, argumentos, 1, "IGNORE-ERRORS", erro))
        return NULL;
    SefErro ignorado;
    sef_erro_limpar(&ignorado);
    SefValor resultado = sef_avaliar(runtime, primeiro(argumentos), ambiente, &ignorado);
    if (resultado == NULL && ignorado.ocorreu) {
        SefValor classe = sef_simbolo_internar(runtime, "ERROR", 5, erro);
        SefValor condicao =
            classe == NULL ? NULL : sef_condicao_nova(runtime, classe, ignorado.mensagem, erro);
        if (condicao == NULL)
            return NULL;
        SefValor valores[2] = {runtime->nulo, condicao};
        return sef_valores_definir(runtime, valores, 2, erro) ? runtime->nulo : NULL;
    }
    return resultado;
}

static SefValor especial_handler_case(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                                      SefErro *erro) {
    if (argumentos == runtime->nulo || argumentos->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "HANDLER-CASE exige uma forma protegida");
        return NULL;
    }
    SefErro capturado;
    sef_erro_limpar(&capturado);
    SefValor resultado = sef_avaliar(runtime, primeiro(argumentos), ambiente, &capturado);
    if (resultado != NULL || !capturado.ocorreu)
        return resultado;

    SefValor classe = sef_simbolo_internar(runtime, "ERROR", 5, erro);
    SefValor condicao =
        classe == NULL ? NULL : sef_condicao_nova(runtime, classe, capturado.mensagem, erro);
    if (condicao == NULL)
        return NULL;

    SefValor clausulas = resto(argumentos);
    while (clausulas != runtime->nulo) {
        if (clausulas->tipo != SEF_TIPO_PAR) {
            sef_erro_definir(erro, 0, 0, "clausulas de HANDLER-CASE improprias");
            return NULL;
        }
        SefValor clausula = primeiro(clausulas);
        bool propria = false;
        if (clausula == runtime->nulo || clausula->tipo != SEF_TIPO_PAR ||
            sef_lista_tamanho(runtime, clausula, &propria) < 2 || !propria) {
            sef_erro_definir(erro, 0, 0, "clausula HANDLER-CASE invalida");
            return NULL;
        }
        SefValor tipo = primeiro(clausula);
        bool aceita = sef_simbolo_tem_nome(tipo, "ERROR") ||
                      sef_simbolo_tem_nome(tipo, "CONDITION") || tipo == classe;
        if (aceita) {
            SefValor cauda = resto(clausula);
            SefValor parametros = primeiro(cauda);
            bool parametros_proprios = false;
            size_t total_parametros = sef_lista_tamanho(runtime, parametros, &parametros_proprios);
            if (!parametros_proprios || total_parametros > 1) {
                sef_erro_definir(erro, 0, 0, "handler aceita zero ou um parametro");
                return NULL;
            }
            SefValor local = sef_ambiente_novo(runtime, ambiente, erro);
            if (local == NULL)
                return NULL;
            if (total_parametros == 1 &&
                !sef_ambiente_definir(runtime, local, primeiro(parametros), condicao, erro))
                return NULL;
            return avaliar_sequencia(runtime, resto(cauda), local, erro);
        }
        clausulas = resto(clausulas);
    }
    *erro = capturado;
    return NULL;
}

static const char *nome_pacote_literal(SefValor designador, size_t *tamanho) {
    if (designador->tipo == SEF_TIPO_TEXTO) {
        *tamanho = designador->como.texto.tamanho;
        return designador->como.texto.dados;
    }
    if (designador->tipo == SEF_TIPO_SIMBOLO) {
        *tamanho = designador->como.simbolo.tamanho;
        return designador->como.simbolo.nome;
    }
    if (designador->tipo == SEF_TIPO_PACOTE) {
        *tamanho = strlen(designador->como.pacote.nome);
        return designador->como.pacote.nome;
    }
    return NULL;
}

static SefValor especial_in_package(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!contar_exato(runtime, argumentos, 1, "IN-PACKAGE", erro))
        return NULL;
    size_t tamanho = 0;
    const char *nome = nome_pacote_literal(primeiro(argumentos), &tamanho);
    SefValor pacote = nome == NULL ? NULL : sef_pacote_encontrar(runtime, nome, tamanho);
    if (pacote == NULL) {
        sef_erro_definir(erro, 0, 0, "IN-PACKAGE nomeia pacote inexistente");
        return NULL;
    }
    runtime->pacote_atual = pacote;
    SefValor simbolo_pacote =
        sef_simbolo_internar_em(runtime, runtime->pacote_common_lisp, "*PACKAGE*", 9, erro);
    if (simbolo_pacote == NULL ||
        !sef_ambiente_atribuir(runtime->ambiente_global, simbolo_pacote, pacote)) {
        sef_erro_definir(erro, 0, 0, "nao foi possivel atualizar *PACKAGE*");
        return NULL;
    }
    return pacote;
}

static SefValor especial_defpackage(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (argumentos == runtime->nulo || argumentos->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "DEFPACKAGE exige um nome");
        return NULL;
    }
    size_t tamanho = 0;
    const char *nome = nome_pacote_literal(primeiro(argumentos), &tamanho);
    if (nome == NULL || tamanho == 0) {
        sef_erro_definir(erro, 0, 0, "nome de DEFPACKAGE invalido");
        return NULL;
    }
    SefValor pacote = sef_pacote_encontrar(runtime, nome, tamanho);
    if (pacote == NULL) {
        char *copia = malloc(tamanho + 1);
        if (copia == NULL) {
            sef_erro_definir(erro, 0, 0, "memoria insuficiente para DEFPACKAGE");
            return NULL;
        }
        memcpy(copia, nome, tamanho);
        copia[tamanho] = '\0';
        pacote = sef_pacote_novo(runtime, copia, erro);
        free(copia);
        if (pacote == NULL)
            return NULL;
    }
    SefValor opcoes = resto(argumentos);
    while (opcoes != runtime->nulo) {
        if (opcoes->tipo != SEF_TIPO_PAR) {
            sef_erro_definir(erro, 0, 0, "opcoes de DEFPACKAGE improprias");
            return NULL;
        }
        SefValor opcao = primeiro(opcoes);
        if (opcao == runtime->nulo || opcao->tipo != SEF_TIPO_PAR ||
            !sef_e_lista_propria(runtime, opcao)) {
            sef_erro_definir(erro, 0, 0, "opcao de DEFPACKAGE invalida");
            return NULL;
        }
        SefValor chave = primeiro(opcao);
        SefValor itens = resto(opcao);
        if (sef_simbolo_tem_nome(chave, "USE")) {
            while (itens != runtime->nulo) {
                size_t tamanho_usado = 0;
                const char *nome_usado = nome_pacote_literal(primeiro(itens), &tamanho_usado);
                SefValor usado = nome_usado == NULL
                                     ? NULL
                                     : sef_pacote_encontrar(runtime, nome_usado, tamanho_usado);
                if (usado == NULL || !sef_pacote_usar(runtime, pacote, usado, erro)) {
                    if (!erro->ocorreu)
                        sef_erro_definir(erro, 0, 0, "pacote de :USE nao existe");
                    return NULL;
                }
                itens = resto(itens);
            }
        } else if (sef_simbolo_tem_nome(chave, "EXPORT")) {
            while (itens != runtime->nulo) {
                size_t tamanho_exportado = 0;
                const char *nome_exportado =
                    nome_pacote_literal(primeiro(itens), &tamanho_exportado);
                SefValor simbolo = nome_exportado == NULL
                                       ? NULL
                                       : sef_simbolo_internar_em(runtime, pacote, nome_exportado,
                                                                 tamanho_exportado, erro);
                if (simbolo == NULL || !sef_pacote_exportar(runtime, pacote, simbolo, erro))
                    return NULL;
                itens = resto(itens);
            }
        } else {
            sef_erro_definir(erro, 0, 0, "esta versao aceita as opcoes :USE e :EXPORT");
            return NULL;
        }
        opcoes = resto(opcoes);
    }
    return pacote;
}

static SefValor especial_funcoes_locais(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                                        bool recursivas, bool macros, SefErro *erro) {
    if (argumentos == runtime->nulo || argumentos->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "forma de funcoes locais exige definicoes e corpo");
        return NULL;
    }
    SefValor definicoes = primeiro(argumentos);
    if (!exigir_lista(runtime, definicoes, "definicoes locais", erro))
        return NULL;
    SefValor local = sef_ambiente_novo(runtime, ambiente, erro);
    if (local == NULL)
        return NULL;
    while (definicoes != runtime->nulo) {
        SefValor definicao = primeiro(definicoes);
        bool propria = false;
        if (definicao == runtime->nulo || definicao->tipo != SEF_TIPO_PAR ||
            sef_lista_tamanho(runtime, definicao, &propria) < 2 || !propria) {
            sef_erro_definir(erro, 0, 0, "definicao de funcao local invalida");
            return NULL;
        }
        SefValor nome = primeiro(definicao);
        if (nome->tipo != SEF_TIPO_SIMBOLO) {
            sef_erro_definir(erro, 0, 0, "nome de funcao local deve ser simbolo");
            return NULL;
        }
        SefValor funcao =
            especial_lambda(runtime, resto(definicao), recursivas ? local : ambiente, macros, erro);
        if (funcao == NULL || !sef_ambiente_definir_funcao(runtime, local, nome, funcao, erro))
            return NULL;
        definicoes = resto(definicoes);
    }
    return avaliar_sequencia(runtime, resto(argumentos), local, erro);
}

static SefValor especial_logico(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                                bool e_logico, SefErro *erro) {
    SefValor resultado = e_logico ? runtime->verdadeiro : runtime->nulo;
    while (argumentos != runtime->nulo) {
        if (argumentos->tipo != SEF_TIPO_PAR) {
            sef_erro_definir(erro, 0, 0, "lista de argumentos impropria");
            return NULL;
        }
        resultado = sef_avaliar(runtime, primeiro(argumentos), ambiente, erro);
        if (resultado == NULL)
            return NULL;
        if (e_logico && resultado == runtime->nulo)
            return runtime->nulo;
        if (!e_logico && resultado != runtime->nulo)
            return resultado;
        argumentos = resto(argumentos);
    }
    return resultado;
}

static SefValor especial_function(SefRuntime *runtime, SefValor argumentos, SefValor ambiente,
                                  SefErro *erro) {
    if (!contar_exato(runtime, argumentos, 1, "FUNCTION", erro))
        return NULL;
    SefValor designador = primeiro(argumentos);
    if (designador->tipo == SEF_TIPO_SIMBOLO) {
        SefValor funcao = NULL;
        if (!sef_ambiente_obter_funcao(ambiente, designador, &funcao)) {
            sef_erro_definir(erro, 0, 0, "funcao %s nao definida", designador->como.simbolo.nome);
            return NULL;
        }
        return funcao;
    }
    if (designador->tipo == SEF_TIPO_PAR && sef_simbolo_tem_nome(primeiro(designador), "LAMBDA")) {
        return especial_lambda(runtime, resto(designador), ambiente, false, erro);
    }
    sef_erro_definir(erro, 0, 0, "designador FUNCTION invalido");
    return NULL;
}

static bool vincular_parametros(SefRuntime *runtime, SefValor ambiente, SefValor parametros,
                                SefValor argumentos, SefErro *erro) {
    while (parametros != runtime->nulo) {
        if (parametros->tipo != SEF_TIPO_PAR) {
            sef_erro_definir(erro, 0, 0, "lista de parametros impropria");
            return false;
        }
        SefValor parametro = primeiro(parametros);
        parametros = resto(parametros);
        if (sef_simbolo_tem_nome(parametro, "&REST")) {
            if (parametros == runtime->nulo ||
                !exigir_nome_variavel(runtime, primeiro(parametros), "&REST", erro) ||
                resto(parametros) != runtime->nulo) {
                if (!erro->ocorreu)
                    sef_erro_definir(erro, 0, 0, "uso invalido de &REST");
                return false;
            }
            return sef_ambiente_definir(runtime, ambiente, primeiro(parametros), argumentos, erro);
        }
        if (!exigir_nome_variavel(runtime, parametro, "parametro", erro)) {
            return false;
        }
        if (argumentos == runtime->nulo || argumentos->tipo != SEF_TIPO_PAR) {
            sef_erro_definir(erro, 0, 0, "faltam argumentos para a funcao");
            return false;
        }
        if (!sef_ambiente_definir(runtime, ambiente, parametro, primeiro(argumentos), erro))
            return false;
        argumentos = resto(argumentos);
    }
    if (argumentos != runtime->nulo) {
        sef_erro_definir(erro, 0, 0, "argumentos demais para a funcao");
        return false;
    }
    return true;
}

static SefValor aplicar_funcao(SefRuntime *runtime, SefValor funcao, SefValor argumentos,
                               SefErro *erro) {
    if (funcao == NULL)
        return NULL;
    if (funcao->tipo == SEF_TIPO_NATIVA) {
        return funcao->como.nativa.funcao(runtime, argumentos, erro);
    }
    if (funcao->tipo != SEF_TIPO_FUNCAO) {
        sef_erro_definir(erro, 0, 0, "objeto chamado nao e uma funcao");
        return NULL;
    }
    if (funcao->como.funcao.compilada_i64 != NULL) {
        bool propria = false;
        size_t quantidade = sef_lista_tamanho(runtime, argumentos, &propria);
        if (!propria || quantidade > SIZE_MAX / sizeof(int64_t)) {
            sef_erro_definir(erro, 0, 0, "argumentos invalidos para funcao compilada");
            return NULL;
        }
        int64_t *inteiros = quantidade == 0 ? NULL : malloc(quantidade * sizeof(*inteiros));
        if (quantidade > 0 && inteiros == NULL) {
            sef_erro_definir(erro, 0, 0, "memoria insuficiente para chamada compilada");
            return NULL;
        }
        SefValor cursor = argumentos;
        for (size_t i = 0; i < quantidade; i++) {
            SefValor argumento = primeiro(cursor);
            if (argumento->tipo != SEF_TIPO_INTEIRO) {
                free(inteiros);
                sef_erro_definir(erro, 0, 0, "funcao compilada i64 exige argumentos inteiros");
                return NULL;
            }
            inteiros[i] = argumento->como.inteiro;
            cursor = resto(cursor);
        }
        int64_t resultado;
        bool sucesso = sef_funcao_compilada_executar_i64(funcao->como.funcao.compilada_i64,
                                                         inteiros, quantidade, &resultado, erro);
        free(inteiros);
        return sucesso ? sef_inteiro_novo(runtime, resultado, erro) : NULL;
    }
    SefValor ambiente = sef_ambiente_novo(runtime, funcao->como.funcao.ambiente, erro);
    if (ambiente == NULL ||
        !vincular_parametros(runtime, ambiente, funcao->como.funcao.parametros, argumentos, erro))
        return NULL;
    return avaliar_sequencia(runtime, funcao->como.funcao.corpo, ambiente, erro);
}

SefValor sef_aplicar(SefRuntime *runtime, SefValor funcao, SefValor argumentos, SefErro *erro) {
    uint64_t versao_anterior = runtime->versao_valores;
    SefValor resultado = aplicar_funcao(runtime, funcao, argumentos, erro);
    if (resultado != NULL && runtime->versao_valores == versao_anterior &&
        !sef_valores_definir_um(runtime, resultado, erro))
        return NULL;
    return resultado;
}

static SefValor avaliar_forma(SefRuntime *runtime, SefValor forma, SefValor ambiente,
                              SefErro *erro) {
    if (forma == NULL)
        return NULL;
    switch (forma->tipo) {
    case SEF_TIPO_NULO:
    case SEF_TIPO_INTEIRO:
    case SEF_TIPO_REAL:
    case SEF_TIPO_TEXTO:
    case SEF_TIPO_NATIVA:
    case SEF_TIPO_FUNCAO:
    case SEF_TIPO_CONDICAO:
    case SEF_TIPO_PACOTE:
    case SEF_TIPO_STREAM:
    case SEF_TIPO_BIBLIOTECA:
    case SEF_TIPO_VETOR:
    case SEF_TIPO_CARACTERE:
    case SEF_TIPO_TABELA_HASH:
    case SEF_TIPO_REINICIO:
        return forma;
    case SEF_TIPO_SIMBOLO: {
        if (forma == runtime->verdadeiro || forma->como.simbolo.pacote == runtime->pacote_keyword)
            return forma;
        SefValor valor = NULL;
        if (sef_ambiente_obter(ambiente, forma, &valor))
            return valor;
        sef_erro_definir(erro, 0, 0, "simbolo %s nao esta vinculado", forma->como.simbolo.nome);
        return NULL;
    }
    case SEF_TIPO_PAR:
        break;
    case SEF_TIPO_AMBIENTE:
        return forma;
    }

    SefValor operador = primeiro(forma);
    SefValor argumentos = resto(forma);
    if (operador->tipo == SEF_TIPO_SIMBOLO) {
        if (sef_simbolo_tem_nome(operador, "QUOTE"))
            return especial_quote(runtime, argumentos, erro);
        if (sef_simbolo_tem_nome(operador, "QUASIQUOTE"))
            return especial_quasiquote(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "IF"))
            return especial_if(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "PROGN"))
            return avaliar_sequencia(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "LAMBDA"))
            return especial_lambda(runtime, argumentos, ambiente, false, erro);
        if (sef_simbolo_tem_nome(operador, "DEFUN"))
            return especial_definicao(runtime, argumentos, ambiente, false, erro);
        if (sef_simbolo_tem_nome(operador, "DEFMACRO"))
            return especial_definicao(runtime, argumentos, ambiente, true, erro);
        if (sef_simbolo_tem_nome(operador, "DEFINE"))
            return especial_define(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "DEFVAR"))
            return especial_variavel_global(runtime, argumentos, ambiente, false, erro);
        if (sef_simbolo_tem_nome(operador, "DEFPARAMETER"))
            return especial_variavel_global(runtime, argumentos, ambiente, true, erro);
        if (sef_simbolo_tem_nome(operador, "SETQ"))
            return especial_setq(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "SETF"))
            return especial_setf(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "LET"))
            return especial_let(runtime, argumentos, ambiente, false, erro);
        if (sef_simbolo_tem_nome(operador, "LET*"))
            return especial_let(runtime, argumentos, ambiente, true, erro);
        if (sef_simbolo_tem_nome(operador, "MULTIPLE-VALUE-BIND"))
            return especial_multiple_value_bind(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "MULTIPLE-VALUE-LIST"))
            return especial_multiple_value_list(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "MULTIPLE-VALUE-PROG1"))
            return especial_multiple_value_prog1(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "MULTIPLE-VALUE-CALL"))
            return especial_multiple_value_call(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "NTH-VALUE"))
            return especial_nth_value(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "COND"))
            return especial_cond(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "WHEN"))
            return especial_when(runtime, argumentos, ambiente, true, erro);
        if (sef_simbolo_tem_nome(operador, "UNLESS"))
            return especial_when(runtime, argumentos, ambiente, false, erro);
        if (sef_simbolo_tem_nome(operador, "FLET"))
            return especial_funcoes_locais(runtime, argumentos, ambiente, false, false, erro);
        if (sef_simbolo_tem_nome(operador, "LABELS"))
            return especial_funcoes_locais(runtime, argumentos, ambiente, true, false, erro);
        if (sef_simbolo_tem_nome(operador, "MACROLET"))
            return especial_funcoes_locais(runtime, argumentos, ambiente, false, true, erro);
        if (sef_simbolo_tem_nome(operador, "BLOCK"))
            return especial_block(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "RETURN-FROM"))
            return especial_return_from(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "RETURN")) {
            SefValor com_nome = sef_par_novo(runtime, runtime->nulo, argumentos, erro);
            return com_nome == NULL ? NULL
                                    : especial_return_from(runtime, com_nome, ambiente, erro);
        }
        if (sef_simbolo_tem_nome(operador, "CATCH"))
            return especial_catch(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "THROW"))
            return especial_throw(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "UNWIND-PROTECT"))
            return especial_unwind_protect(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "IGNORE-ERRORS"))
            return especial_ignore_errors(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "HANDLER-CASE"))
            return especial_handler_case(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "HANDLER-BIND"))
            return especial_handler_bind(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "RESTART-CASE"))
            return especial_restart_case(runtime, argumentos, ambiente, erro);
        if (sef_simbolo_tem_nome(operador, "IN-PACKAGE"))
            return especial_in_package(runtime, argumentos, erro);
        if (sef_simbolo_tem_nome(operador, "DEFPACKAGE"))
            return especial_defpackage(runtime, argumentos, erro);
        if (sef_simbolo_tem_nome(operador, "AND"))
            return especial_logico(runtime, argumentos, ambiente, true, erro);
        if (sef_simbolo_tem_nome(operador, "OR"))
            return especial_logico(runtime, argumentos, ambiente, false, erro);
        if (sef_simbolo_tem_nome(operador, "FUNCTION"))
            return especial_function(runtime, argumentos, ambiente, erro);
    }

    SefValor funcao = NULL;
    if (operador->tipo == SEF_TIPO_SIMBOLO) {
        if (!sef_ambiente_obter_funcao(ambiente, operador, &funcao)) {
            sef_erro_definir(erro, 0, 0, "funcao %s nao esta definida",
                             operador->como.simbolo.nome);
            return NULL;
        }
    } else {
        funcao = sef_avaliar(runtime, operador, ambiente, erro);
        if (funcao == NULL)
            return NULL;
    }
    if (funcao->tipo == SEF_TIPO_FUNCAO && funcao->como.funcao.macro) {
        SefValor expansao = sef_aplicar(runtime, funcao, argumentos, erro);
        return expansao == NULL ? NULL : sef_avaliar(runtime, expansao, ambiente, erro);
    }
    SefValor valores = avaliar_argumentos(runtime, argumentos, ambiente, erro);
    return valores == NULL ? NULL : sef_aplicar(runtime, funcao, valores, erro);
}

SefValor sef_avaliar(SefRuntime *runtime, SefValor forma, SefValor ambiente, SefErro *erro) {
    uint64_t versao_anterior = runtime->versao_valores;
    SefValor resultado = avaliar_forma(runtime, forma, ambiente, erro);
    if (resultado != NULL && runtime->versao_valores == versao_anterior &&
        !sef_valores_definir_um(runtime, resultado, erro))
        return NULL;
    return resultado;
}
