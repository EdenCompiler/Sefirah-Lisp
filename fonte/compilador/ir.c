#include "sefirah/compilador.h"

#include <stdlib.h>
#include <string.h>

#define SEF_IR_LIMITE_PASSOS 10000000u

static uint32_t aridade_chamada_externa(SefInstrucaoIr instrucao) {
    return instrucao.bloco_a == 0 ? 1u : instrucao.bloco_a;
}

static void erro_definir(SefErro *erro, const char *mensagem) {
    if (erro == NULL)
        return;
    erro->ocorreu = true;
    erro->linha = 0;
    erro->coluna = 0;
    size_t tamanho = strlen(mensagem);
    if (tamanho >= sizeof(erro->mensagem))
        tamanho = sizeof(erro->mensagem) - 1;
    memcpy(erro->mensagem, mensagem, tamanho);
    erro->mensagem[tamanho] = '\0';
}

static void erro_limpar(SefErro *erro) {
    if (erro != NULL)
        memset(erro, 0, sizeof(*erro));
}

void sef_funcao_ir_iniciar(SefFuncaoIr *funcao, const char *nome, uint32_t parametros,
                           uint32_t registradores) {
    if (funcao == NULL)
        return;
    memset(funcao, 0, sizeof(*funcao));
    funcao->nome = nome;
    funcao->quantidade_parametros = parametros;
    funcao->quantidade_registradores = registradores;
}

void sef_funcao_ir_liberar(SefFuncaoIr *funcao) {
    if (funcao == NULL)
        return;
    for (size_t i = 0; i < funcao->quantidade_blocos; i++)
        free(funcao->blocos[i].instrucoes);
    for (size_t i = 0; i < funcao->quantidade_externas; i++)
        free(funcao->externas[i].nome);
    free(funcao->blocos);
    free(funcao->externas);
    memset(funcao, 0, sizeof(*funcao));
}

static bool adicionar_externa_i64(SefFuncaoIr *funcao, const char *nome,
                                  SefFuncaoExternaI64 endereco, uint32_t *indice, SefErro *erro) {
    erro_limpar(erro);
    if (funcao == NULL || nome == NULL || nome[0] == '\0' || indice == NULL) {
        erro_definir(erro, "funcao IR, nome externo ou indice ausente");
        return false;
    }
    if (funcao->quantidade_externas >= UINT32_MAX) {
        erro_definir(erro, "funcao IR excedeu o limite de simbolos externos");
        return false;
    }
    if (funcao->quantidade_externas == funcao->capacidade_externas) {
        size_t capacidade =
            funcao->capacidade_externas == 0 ? 4u : funcao->capacidade_externas * 2u;
        SefSimboloExternoIr *externas = realloc(funcao->externas, capacidade * sizeof(*externas));
        if (externas == NULL) {
            erro_definir(erro, "memoria insuficiente para simbolo externo IR");
            return false;
        }
        funcao->externas = externas;
        funcao->capacidade_externas = capacidade;
    }
    size_t tamanho = strlen(nome) + 1u;
    char *copia = malloc(tamanho);
    if (copia == NULL) {
        erro_definir(erro, "memoria insuficiente para nome externo IR");
        return false;
    }
    memcpy(copia, nome, tamanho);
    *indice = (uint32_t)funcao->quantidade_externas;
    funcao->externas[funcao->quantidade_externas++] = (SefSimboloExternoIr){copia, endereco};
    return true;
}

bool sef_funcao_ir_adicionar_externa_i64(SefFuncaoIr *funcao, const char *nome,
                                         SefFuncaoExternaI64 endereco, uint32_t *indice,
                                         SefErro *erro) {
    return adicionar_externa_i64(funcao, nome, endereco, indice, erro);
}

bool sef_funcao_ir_adicionar_externa_i64_binaria(SefFuncaoIr *funcao, const char *nome,
                                                 SefFuncaoExternaI64Binaria endereco,
                                                 uint32_t *indice, SefErro *erro) {
    SefFuncaoExternaI64 endereco_armazenado = NULL;
    _Static_assert(sizeof(endereco) == sizeof(endereco_armazenado),
                   "ponteiros de funcoes i64 devem ter o mesmo tamanho");
    memcpy(&endereco_armazenado, &endereco, sizeof(endereco_armazenado));
    return adicionar_externa_i64(funcao, nome, endereco_armazenado, indice, erro);
}

bool sef_funcao_ir_adicionar_bloco(SefFuncaoIr *funcao, uint32_t *indice, SefErro *erro) {
    erro_limpar(erro);
    if (funcao == NULL || indice == NULL) {
        erro_definir(erro, "funcao IR ou indice ausente");
        return false;
    }
    if (funcao->quantidade_blocos >= UINT32_MAX) {
        erro_definir(erro, "funcao IR excedeu o limite de blocos");
        return false;
    }
    if (funcao->quantidade_blocos == funcao->capacidade_blocos) {
        size_t capacidade = funcao->capacidade_blocos == 0 ? 4 : funcao->capacidade_blocos * 2;
        SefBlocoIr *blocos = realloc(funcao->blocos, capacidade * sizeof(*blocos));
        if (blocos == NULL) {
            erro_definir(erro, "memoria insuficiente para bloco IR");
            return false;
        }
        memset(blocos + funcao->capacidade_blocos, 0,
               (capacidade - funcao->capacidade_blocos) * sizeof(*blocos));
        funcao->blocos = blocos;
        funcao->capacidade_blocos = capacidade;
    }
    *indice = (uint32_t)funcao->quantidade_blocos;
    funcao->quantidade_blocos++;
    return true;
}

bool sef_bloco_ir_emitir(SefFuncaoIr *funcao, uint32_t bloco, SefInstrucaoIr instrucao,
                         SefErro *erro) {
    erro_limpar(erro);
    if (funcao == NULL || bloco >= funcao->quantidade_blocos) {
        erro_definir(erro, "bloco IR inexistente");
        return false;
    }
    SefBlocoIr *destino = &funcao->blocos[bloco];
    if (destino->quantidade == destino->capacidade) {
        size_t capacidade = destino->capacidade == 0 ? 8 : destino->capacidade * 2;
        SefInstrucaoIr *instrucoes = realloc(destino->instrucoes, capacidade * sizeof(*instrucoes));
        if (instrucoes == NULL) {
            erro_definir(erro, "memoria insuficiente para instrucao IR");
            return false;
        }
        destino->instrucoes = instrucoes;
        destino->capacidade = capacidade;
    }
    destino->instrucoes[destino->quantidade++] = instrucao;
    return true;
}

static bool e_terminador(SefOperacaoIr operacao) {
    return operacao == SEF_IR_SALTAR || operacao == SEF_IR_RAMIFICAR ||
           operacao == SEF_IR_RETORNAR_I64;
}

static bool verificar_registrador(uint32_t registrador, uint32_t quantidade, SefErro *erro) {
    if (registrador < quantidade)
        return true;
    erro_definir(erro, "instrucao referencia registrador inexistente");
    return false;
}

static bool definicao_domina_uso(uint32_t registrador, uint32_t bloco_uso, size_t posicao_uso,
                                 const uint32_t *blocos_definicao, const size_t *posicoes_definicao,
                                 const bool *dominadores, size_t quantidade_blocos) {
    uint32_t bloco_definicao = blocos_definicao[registrador];
    if (bloco_definicao == bloco_uso)
        return posicoes_definicao[registrador] < posicao_uso;
    return dominadores[bloco_uso * quantidade_blocos + bloco_definicao];
}

static bool verificar_fluxo_ssa(const SefFuncaoIr *funcao, const uint32_t *blocos_definicao,
                                const size_t *posicoes_definicao, SefErro *erro) {
    size_t n = funcao->quantidade_blocos;
    if (n > SIZE_MAX / n) {
        erro_definir(erro, "funcao IR possui blocos demais");
        return false;
    }
    bool *arestas = calloc(n * n, sizeof(*arestas));
    bool *alcancaveis = calloc(n, sizeof(*alcancaveis));
    bool *dominadores = calloc(n * n, sizeof(*dominadores));
    if (arestas == NULL || alcancaveis == NULL || dominadores == NULL) {
        free(arestas);
        free(alcancaveis);
        free(dominadores);
        erro_definir(erro, "memoria insuficiente para analisar fluxo SSA");
        return false;
    }
    for (size_t b = 0; b < n; b++) {
        SefInstrucaoIr fim = funcao->blocos[b].instrucoes[funcao->blocos[b].quantidade - 1];
        if (fim.operacao == SEF_IR_SALTAR) {
            arestas[b * n + fim.bloco_a] = true;
        } else if (fim.operacao == SEF_IR_RAMIFICAR) {
            arestas[b * n + fim.bloco_a] = true;
            arestas[b * n + fim.bloco_b] = true;
        }
    }
    alcancaveis[0] = true;
    bool mudou = true;
    while (mudou) {
        mudou = false;
        for (size_t origem = 0; origem < n; origem++) {
            if (!alcancaveis[origem])
                continue;
            for (size_t destino = 0; destino < n; destino++) {
                if (arestas[origem * n + destino] && !alcancaveis[destino]) {
                    alcancaveis[destino] = true;
                    mudou = true;
                }
            }
        }
    }
    for (size_t b = 0; b < n; b++) {
        if (!alcancaveis[b]) {
            erro_definir(erro, "funcao IR possui bloco inalcançavel");
            free(arestas);
            free(alcancaveis);
            free(dominadores);
            return false;
        }
        for (size_t d = 0; d < n; d++)
            dominadores[b * n + d] = b == 0 ? d == 0 : true;
    }
    mudou = true;
    while (mudou) {
        mudou = false;
        for (size_t b = 1; b < n; b++) {
            for (size_t d = 0; d < n; d++) {
                bool novo = true;
                bool possui_predecessor = false;
                for (size_t predecessor = 0; predecessor < n; predecessor++) {
                    if (arestas[predecessor * n + b]) {
                        possui_predecessor = true;
                        novo = novo && dominadores[predecessor * n + d];
                    }
                }
                novo = (possui_predecessor && novo) || d == b;
                if (dominadores[b * n + d] != novo) {
                    dominadores[b * n + d] = novo;
                    mudou = true;
                }
            }
        }
    }

    bool valido = true;
    for (size_t b = 0; valido && b < n; b++) {
        bool terminou_phis = false;
        const SefBlocoIr *bloco = &funcao->blocos[b];
        for (size_t i = 0; valido && i < bloco->quantidade; i++) {
            SefInstrucaoIr ins = bloco->instrucoes[i];
            if (ins.operacao == SEF_IR_PHI) {
                if (terminou_phis || !arestas[(size_t)ins.bloco_a * n + b] ||
                    !arestas[(size_t)ins.bloco_b * n + b] ||
                    !definicao_domina_uso(ins.operando_a, ins.bloco_a,
                                          funcao->blocos[ins.bloco_a].quantidade, blocos_definicao,
                                          posicoes_definicao, dominadores, n) ||
                    !definicao_domina_uso(ins.operando_b, ins.bloco_b,
                                          funcao->blocos[ins.bloco_b].quantidade, blocos_definicao,
                                          posicoes_definicao, dominadores, n)) {
                    erro_definir(erro, "PHI possui predecessor ou definicao nao dominante");
                    valido = false;
                }
                continue;
            }
            terminou_phis = true;
            uint32_t usos[2] = {0, 0};
            size_t quantidade_usos = 0;
            if (ins.operacao >= SEF_IR_SOMAR_I64 && ins.operacao <= SEF_IR_MENOR_OU_IGUAL_I64) {
                usos[0] = ins.operando_a;
                usos[1] = ins.operando_b;
                quantidade_usos = 2;
            } else if (ins.operacao == SEF_IR_CHAMAR_EXTERNA_I64) {
                usos[0] = ins.operando_a;
                usos[1] = ins.operando_b;
                quantidade_usos = aridade_chamada_externa(ins);
            } else if (ins.operacao == SEF_IR_RAMIFICAR || ins.operacao == SEF_IR_RETORNAR_I64) {
                usos[0] = ins.operando_a;
                quantidade_usos = 1;
            }
            for (size_t u = 0; u < quantidade_usos; u++) {
                if (!definicao_domina_uso(usos[u], (uint32_t)b, i, blocos_definicao,
                                          posicoes_definicao, dominadores, n)) {
                    erro_definir(erro, "uso SSA nao e dominado por sua definicao");
                    valido = false;
                    break;
                }
            }
        }
    }
    free(arestas);
    free(alcancaveis);
    free(dominadores);
    return valido;
}

bool sef_funcao_ir_verificar(const SefFuncaoIr *funcao, SefErro *erro) {
    erro_limpar(erro);
    if (funcao == NULL || funcao->quantidade_blocos == 0 || funcao->quantidade_registradores == 0) {
        erro_definir(erro, "funcao IR vazia ou incompleta");
        return false;
    }
#if SIZE_MAX < UINT32_MAX
    if ((size_t)funcao->quantidade_registradores > SIZE_MAX / sizeof(size_t)) {
        erro_definir(erro, "funcao IR possui registradores demais");
        return false;
    }
#endif
    bool *definidos = calloc(funcao->quantidade_registradores, sizeof(*definidos));
    uint32_t *blocos_definicao =
        malloc(funcao->quantidade_registradores * sizeof(*blocos_definicao));
    size_t *posicoes_definicao =
        malloc(funcao->quantidade_registradores * sizeof(*posicoes_definicao));
    if (definidos == NULL || blocos_definicao == NULL || posicoes_definicao == NULL) {
        free(definidos);
        free(blocos_definicao);
        free(posicoes_definicao);
        erro_definir(erro, "memoria insuficiente para verificar IR");
        return false;
    }
    for (uint32_t i = 0; i < funcao->quantidade_registradores; i++)
        blocos_definicao[i] = UINT32_MAX;
    bool valido = true;
    for (size_t b = 0; valido && b < funcao->quantidade_blocos; b++) {
        const SefBlocoIr *bloco = &funcao->blocos[b];
        if (bloco->quantidade == 0 ||
            !e_terminador(bloco->instrucoes[bloco->quantidade - 1].operacao)) {
            erro_definir(erro, "bloco IR nao termina com salto, ramificacao ou retorno");
            valido = false;
            break;
        }
        for (size_t i = 0; valido && i < bloco->quantidade; i++) {
            const SefInstrucaoIr *ins = &bloco->instrucoes[i];
            if (ins->operacao < SEF_IR_CONSTANTE_I64 || ins->operacao > SEF_IR_RETORNAR_I64) {
                erro_definir(erro, "operacao IR desconhecida");
                valido = false;
                break;
            }
            if (e_terminador(ins->operacao) != (i + 1 == bloco->quantidade)) {
                erro_definir(erro, "terminador IR deve ser a ultima instrucao do bloco");
                valido = false;
                break;
            }
            bool produz = ins->operacao <= SEF_IR_CHAMAR_EXTERNA_I64;
            if (produz) {
                valido =
                    verificar_registrador(ins->destino, funcao->quantidade_registradores, erro);
                if (valido && definidos[ins->destino]) {
                    erro_definir(erro, "registrador SSA possui mais de uma definicao");
                    valido = false;
                }
                if (valido) {
                    definidos[ins->destino] = true;
                    blocos_definicao[ins->destino] = (uint32_t)b;
                    posicoes_definicao[ins->destino] = i;
                }
            }
            if (ins->operacao == SEF_IR_PARAMETRO &&
                (ins->imediato < 0 || (uint64_t)ins->imediato >= funcao->quantidade_parametros)) {
                erro_definir(erro, "indice de parametro IR invalido");
                valido = false;
            } else if (ins->operacao == SEF_IR_PHI) {
                valido = verificar_registrador(ins->operando_a, funcao->quantidade_registradores,
                                               erro) &&
                         verificar_registrador(ins->operando_b, funcao->quantidade_registradores,
                                               erro) &&
                         ins->bloco_a < funcao->quantidade_blocos &&
                         ins->bloco_b < funcao->quantidade_blocos;
                if (!valido && (erro == NULL || !erro->ocorreu))
                    erro_definir(erro, "entrada de PHI invalida");
            } else if (ins->operacao >= SEF_IR_SOMAR_I64 &&
                       ins->operacao <= SEF_IR_MENOR_OU_IGUAL_I64) {
                valido =
                    verificar_registrador(ins->operando_a, funcao->quantidade_registradores,
                                          erro) &&
                    verificar_registrador(ins->operando_b, funcao->quantidade_registradores, erro);
            } else if (ins->operacao == SEF_IR_CHAMAR_EXTERNA_I64) {
                uint32_t aridade = aridade_chamada_externa(*ins);
                valido = aridade <= 2 &&
                         verificar_registrador(ins->operando_a, funcao->quantidade_registradores,
                                               erro) &&
                         (aridade != 2 ||
                          verificar_registrador(ins->operando_b, funcao->quantidade_registradores,
                                                erro)) &&
                         ins->imediato >= 0 &&
                         (uint64_t)ins->imediato < funcao->quantidade_externas;
            } else if (ins->operacao == SEF_IR_SALTAR) {
                valido = ins->bloco_a < funcao->quantidade_blocos;
            } else if (ins->operacao == SEF_IR_RAMIFICAR) {
                valido = verificar_registrador(ins->operando_a, funcao->quantidade_registradores,
                                               erro) &&
                         ins->bloco_a < funcao->quantidade_blocos &&
                         ins->bloco_b < funcao->quantidade_blocos;
            } else if (ins->operacao == SEF_IR_RETORNAR_I64) {
                valido =
                    verificar_registrador(ins->operando_a, funcao->quantidade_registradores, erro);
            }
            if (!valido && (erro == NULL || !erro->ocorreu))
                erro_definir(erro, "instrucao IR invalida");
        }
    }
    for (uint32_t i = 0; valido && i < funcao->quantidade_registradores; i++) {
        if (!definidos[i]) {
            erro_definir(erro, "registrador SSA nao possui definicao");
            valido = false;
        }
    }
    if (valido)
        valido = verificar_fluxo_ssa(funcao, blocos_definicao, posicoes_definicao, erro);
    free(definidos);
    free(blocos_definicao);
    free(posicoes_definicao);
    return valido;
}

static int64_t somar_sem_ub(int64_t a, int64_t b) { return (int64_t)((uint64_t)a + (uint64_t)b); }

static int64_t subtrair_sem_ub(int64_t a, int64_t b) {
    return (int64_t)((uint64_t)a - (uint64_t)b);
}

static int64_t multiplicar_sem_ub(int64_t a, int64_t b) {
    return (int64_t)((uint64_t)a * (uint64_t)b);
}

bool sef_funcao_ir_executar_i64(const SefFuncaoIr *funcao, const int64_t *argumentos,
                                size_t quantidade_argumentos, int64_t *resultado, SefErro *erro) {
    erro_limpar(erro);
    if (funcao == NULL || resultado == NULL ||
        quantidade_argumentos != funcao->quantidade_parametros ||
        (quantidade_argumentos > 0 && argumentos == NULL)) {
        erro_definir(erro, "argumentos invalidos para executar IR");
        return false;
    }
    if (!sef_funcao_ir_verificar(funcao, erro))
        return false;
    int64_t *reg = calloc(funcao->quantidade_registradores, sizeof(*reg));
    int64_t *valores_phi = calloc(funcao->quantidade_registradores, sizeof(*valores_phi));
    if (reg == NULL || valores_phi == NULL) {
        free(reg);
        free(valores_phi);
        erro_definir(erro, "memoria insuficiente para executar IR");
        return false;
    }
    uint32_t bloco_atual = 0, bloco_anterior = UINT32_MAX;
    size_t passos = 0;
    bool terminou = false, sucesso = true;
    while (!terminou && sucesso) {
        if (++passos > SEF_IR_LIMITE_PASSOS) {
            erro_definir(erro, "limite de passos excedido ao executar IR");
            sucesso = false;
            break;
        }
        const SefBlocoIr *bloco = &funcao->blocos[bloco_atual];
        uint32_t proximo = bloco_atual;
        size_t primeira_instrucao = 0;
        while (primeira_instrucao < bloco->quantidade &&
               bloco->instrucoes[primeira_instrucao].operacao == SEF_IR_PHI) {
            SefInstrucaoIr phi = bloco->instrucoes[primeira_instrucao];
            if (bloco_anterior == phi.bloco_a)
                valores_phi[phi.destino] = reg[phi.operando_a];
            else if (bloco_anterior == phi.bloco_b)
                valores_phi[phi.destino] = reg[phi.operando_b];
            else {
                erro_definir(erro, "PHI nao possui entrada para o bloco predecessor");
                sucesso = false;
                break;
            }
            primeira_instrucao++;
        }
        for (size_t i = 0; i < primeira_instrucao; i++) {
            uint32_t destino = bloco->instrucoes[i].destino;
            reg[destino] = valores_phi[destino];
        }
        for (size_t i = primeira_instrucao; sucesso && i < bloco->quantidade; i++) {
            SefInstrucaoIr ins = bloco->instrucoes[i];
            switch (ins.operacao) {
            case SEF_IR_CONSTANTE_I64:
                reg[ins.destino] = ins.imediato;
                break;
            case SEF_IR_PARAMETRO:
                reg[ins.destino] = argumentos[ins.imediato];
                break;
            case SEF_IR_PHI:
                erro_definir(erro, "PHI apareceu fora do inicio do bloco");
                sucesso = false;
                break;
            case SEF_IR_SOMAR_I64:
                reg[ins.destino] = somar_sem_ub(reg[ins.operando_a], reg[ins.operando_b]);
                break;
            case SEF_IR_SUBTRAIR_I64:
                reg[ins.destino] = subtrair_sem_ub(reg[ins.operando_a], reg[ins.operando_b]);
                break;
            case SEF_IR_MULTIPLICAR_I64:
                reg[ins.destino] = multiplicar_sem_ub(reg[ins.operando_a], reg[ins.operando_b]);
                break;
            case SEF_IR_MENOR_I64:
                reg[ins.destino] = reg[ins.operando_a] < reg[ins.operando_b];
                break;
            case SEF_IR_MENOR_OU_IGUAL_I64:
                reg[ins.destino] = reg[ins.operando_a] <= reg[ins.operando_b];
                break;
            case SEF_IR_CHAMAR_EXTERNA_I64: {
                SefFuncaoExternaI64 externa = funcao->externas[ins.imediato].endereco;
                if (externa == NULL) {
                    erro_definir(erro, "simbolo externo IR nao possui endereco no interpretador");
                    sucesso = false;
                    break;
                }
                if (aridade_chamada_externa(ins) == 2) {
                    SefFuncaoExternaI64Binaria binaria = NULL;
                    _Static_assert(sizeof(binaria) == sizeof(externa),
                                   "ponteiros de funcoes i64 devem ter o mesmo tamanho");
                    memcpy(&binaria, &externa, sizeof(binaria));
                    reg[ins.destino] = binaria(reg[ins.operando_a], reg[ins.operando_b]);
                } else {
                    reg[ins.destino] = externa(reg[ins.operando_a]);
                }
                break;
            }
            case SEF_IR_SALTAR:
                proximo = ins.bloco_a;
                break;
            case SEF_IR_RAMIFICAR:
                proximo = reg[ins.operando_a] != 0 ? ins.bloco_a : ins.bloco_b;
                break;
            case SEF_IR_RETORNAR_I64:
                *resultado = reg[ins.operando_a];
                terminou = true;
                break;
            }
            if (!sucesso)
                break;
        }
        bloco_anterior = bloco_atual;
        bloco_atual = proximo;
    }
    free(reg);
    free(valores_phi);
    return sucesso && terminou;
}
