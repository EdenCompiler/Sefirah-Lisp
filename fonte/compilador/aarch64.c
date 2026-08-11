#include "sefirah/compilador.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

typedef struct CorrecaoAarch64 {
    size_t posicao;
    uint32_t bloco;
} CorrecaoAarch64;

typedef struct EmissorAarch64 {
    SefCodigoNativo *codigo;
    const SefFuncaoIr *funcao;
    size_t *posicoes_blocos;
    CorrecaoAarch64 *correcoes;
    size_t quantidade_correcoes;
    size_t capacidade_correcoes;
    uint32_t tamanho_quadro;
    SefErro *erro;
} EmissorAarch64;

static void erro_limpar(SefErro *erro) {
    if (erro != NULL)
        memset(erro, 0, sizeof(*erro));
}

static void erro_definir(SefErro *erro, const char *mensagem) {
    if (erro == NULL || erro->ocorreu)
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

static bool reservar(EmissorAarch64 *emissor, size_t adicional) {
    SefCodigoNativo *codigo = emissor->codigo;
    if (adicional > SIZE_MAX - codigo->tamanho) {
        erro_definir(emissor->erro, "codigo AArch64 excedeu o limite de tamanho");
        return false;
    }
    size_t necessario = codigo->tamanho + adicional;
    if (necessario <= codigo->capacidade)
        return true;
    size_t capacidade = codigo->capacidade == 0 ? 256 : codigo->capacidade;
    while (capacidade < necessario) {
        if (capacidade > SIZE_MAX / 2) {
            capacidade = necessario;
            break;
        }
        capacidade *= 2;
    }
    unsigned char *bytes = realloc(codigo->bytes, capacidade);
    if (bytes == NULL) {
        erro_definir(emissor->erro, "memoria insuficiente para codigo AArch64");
        return false;
    }
    codigo->bytes = bytes;
    codigo->capacidade = capacidade;
    return true;
}

static bool emitir_instrucao(EmissorAarch64 *emissor, uint32_t instrucao) {
    if (!reservar(emissor, 4))
        return false;
    for (unsigned int i = 0; i < 4; i++)
        emissor->codigo->bytes[emissor->codigo->tamanho++] = (unsigned char)(instrucao >> (i * 8u));
    return true;
}

static bool adicionar_relocacao(EmissorAarch64 *emissor, size_t deslocamento, const char *simbolo,
                                SefFuncaoExternaI64 endereco) {
    SefCodigoNativo *codigo = emissor->codigo;
    if (codigo->quantidade_relocacoes == codigo->capacidade_relocacoes) {
        size_t capacidade =
            codigo->capacidade_relocacoes == 0 ? 4u : codigo->capacidade_relocacoes * 2u;
        SefRelocacaoNativa *relocacoes =
            realloc(codigo->relocacoes, capacidade * sizeof(*relocacoes));
        if (relocacoes == NULL) {
            erro_definir(emissor->erro, "memoria insuficiente para relocacao AArch64");
            return false;
        }
        codigo->relocacoes = relocacoes;
        codigo->capacidade_relocacoes = capacidade;
    }
    size_t tamanho = strlen(simbolo) + 1u;
    char *copia = malloc(tamanho);
    if (copia == NULL) {
        erro_definir(emissor->erro, "memoria insuficiente para simbolo de relocacao AArch64");
        return false;
    }
    memcpy(copia, simbolo, tamanho);
    codigo->relocacoes[codigo->quantidade_relocacoes++] =
        (SefRelocacaoNativa){deslocamento, copia, SEF_RELOCACAO_CHAMADA26_AARCH64, endereco};
    return true;
}

static void sobrescrever_instrucao(SefCodigoNativo *codigo, size_t posicao, uint32_t instrucao) {
    for (unsigned int i = 0; i < 4; i++)
        codigo->bytes[posicao + i] = (unsigned char)(instrucao >> (i * 8u));
}

static uint32_t deslocamento_slot(const EmissorAarch64 *emissor, uint32_t registrador,
                                  bool temporario) {
    uint32_t indice = registrador;
    if (temporario)
        indice += emissor->funcao->quantidade_registradores;
    return indice * 8u;
}

static uint32_t deslocamento_argumentos(const EmissorAarch64 *emissor) {
    return emissor->funcao->quantidade_registradores * 16u;
}

static bool carregar_x0(EmissorAarch64 *emissor, uint32_t registrador, bool temporario) {
    uint32_t deslocamento = deslocamento_slot(emissor, registrador, temporario);
    return emitir_instrucao(emissor, 0xf94003e0u | ((deslocamento / 8u) << 10u));
}

static bool carregar_x1(EmissorAarch64 *emissor, uint32_t registrador) {
    uint32_t deslocamento = deslocamento_slot(emissor, registrador, false);
    return emitir_instrucao(emissor, 0xf94003e1u | ((deslocamento / 8u) << 10u));
}

static bool armazenar_x0(EmissorAarch64 *emissor, uint32_t registrador, bool temporario) {
    uint32_t deslocamento = deslocamento_slot(emissor, registrador, temporario);
    return emitir_instrucao(emissor, 0xf90003e0u | ((deslocamento / 8u) << 10u));
}

static bool carregar_constante(EmissorAarch64 *emissor, uint64_t valor) {
    uint32_t primeira = 0xd2800000u | ((uint32_t)(valor & 0xffffu) << 5u);
    if (!emitir_instrucao(emissor, primeira))
        return false;
    for (unsigned int parte = 1; parte < 4; parte++) {
        uint32_t imediato = (uint32_t)((valor >> (parte * 16u)) & 0xffffu);
        if (imediato != 0 &&
            !emitir_instrucao(emissor, 0xf2800000u | (parte << 21u) | (imediato << 5u)))
            return false;
    }
    return true;
}

static bool adicionar_correcao(EmissorAarch64 *emissor, uint32_t bloco) {
    if (emissor->quantidade_correcoes == emissor->capacidade_correcoes) {
        size_t capacidade =
            emissor->capacidade_correcoes == 0 ? 8 : emissor->capacidade_correcoes * 2;
        CorrecaoAarch64 *correcoes = realloc(emissor->correcoes, capacidade * sizeof(*correcoes));
        if (correcoes == NULL) {
            erro_definir(emissor->erro, "memoria insuficiente para saltos AArch64");
            return false;
        }
        emissor->correcoes = correcoes;
        emissor->capacidade_correcoes = capacidade;
    }
    emissor->correcoes[emissor->quantidade_correcoes++] =
        (CorrecaoAarch64){emissor->codigo->tamanho, bloco};
    return emitir_instrucao(emissor, 0x14000000u);
}

static bool emitir_copias_phi(EmissorAarch64 *emissor, uint32_t destino, uint32_t predecessor) {
    const SefBlocoIr *bloco = &emissor->funcao->blocos[destino];
    size_t quantidade = 0;
    while (quantidade < bloco->quantidade && bloco->instrucoes[quantidade].operacao == SEF_IR_PHI) {
        SefInstrucaoIr phi = bloco->instrucoes[quantidade];
        uint32_t origem = phi.bloco_a == predecessor ? phi.operando_a : phi.operando_b;
        if (!carregar_x0(emissor, origem, false) || !armazenar_x0(emissor, phi.destino, true))
            return false;
        quantidade++;
    }
    for (size_t i = 0; i < quantidade; i++) {
        uint32_t registrador = bloco->instrucoes[i].destino;
        if (!carregar_x0(emissor, registrador, true) || !armazenar_x0(emissor, registrador, false))
            return false;
    }
    return true;
}

static bool emitir_salto_bloco(EmissorAarch64 *emissor, uint32_t destino, uint32_t predecessor) {
    return emitir_copias_phi(emissor, destino, predecessor) && adicionar_correcao(emissor, destino);
}

static bool emitir_valor(EmissorAarch64 *emissor, SefInstrucaoIr ins) {
    if (ins.operacao == SEF_IR_CONSTANTE_I64)
        return carregar_constante(emissor, (uint64_t)ins.imediato) &&
               armazenar_x0(emissor, ins.destino, false);
    if (ins.operacao == SEF_IR_PARAMETRO) {
        uint64_t deslocamento = (uint64_t)ins.imediato * 8u;
        if (deslocamento / 8u > 4095u) {
            erro_definir(emissor->erro, "parametro excedeu o alcance do endereco AArch64");
            return false;
        }
        uint32_t slot_argumentos = deslocamento_argumentos(emissor);
        return emitir_instrucao(emissor, 0xf94003e9u | ((slot_argumentos / 8u) << 10u)) &&
               emitir_instrucao(emissor, 0xf9400120u | ((uint32_t)(deslocamento / 8u) << 10u)) &&
               armazenar_x0(emissor, ins.destino, false);
    }
    if (ins.operacao == SEF_IR_CHAMAR_EXTERNA_I64) {
        SefSimboloExternoIr externa = emissor->funcao->externas[ins.imediato];
        uint32_t aridade = ins.bloco_a == 0 ? 1u : ins.bloco_a;
        if (!carregar_x0(emissor, ins.operando_a, false) ||
            (aridade == 2 && !carregar_x1(emissor, ins.operando_b)))
            return false;
        size_t deslocamento_relocacao = emissor->codigo->tamanho;
        return adicionar_relocacao(emissor, deslocamento_relocacao, externa.nome,
                                   externa.endereco) &&
               emitir_instrucao(emissor, 0x94000000u) && armazenar_x0(emissor, ins.destino, false);
    }
    if (!carregar_x0(emissor, ins.operando_a, false) || !carregar_x1(emissor, ins.operando_b))
        return false;
    uint32_t operacao;
    if (ins.operacao == SEF_IR_SOMAR_I64)
        operacao = 0x8b010000u;
    else if (ins.operacao == SEF_IR_SUBTRAIR_I64)
        operacao = 0xcb010000u;
    else if (ins.operacao == SEF_IR_MULTIPLICAR_I64)
        operacao = 0x9b017c00u;
    else if (ins.operacao == SEF_IR_MENOR_I64)
        return emitir_instrucao(emissor, 0xeb01001fu) && emitir_instrucao(emissor, 0x9a9fa7e0u) &&
               armazenar_x0(emissor, ins.destino, false);
    else
        return emitir_instrucao(emissor, 0xeb01001fu) && emitir_instrucao(emissor, 0x9a9fc7e0u) &&
               armazenar_x0(emissor, ins.destino, false);
    return emitir_instrucao(emissor, operacao) && armazenar_x0(emissor, ins.destino, false);
}

static bool corrigir_cbz_local(EmissorAarch64 *emissor, size_t posicao, size_t destino) {
    int64_t distancia = (int64_t)destino - (int64_t)posicao;
    if (distancia % 4 != 0) {
        erro_definir(emissor->erro, "ramificacao AArch64 perdeu alinhamento");
        return false;
    }
    int64_t palavras = distancia / 4;
    if (palavras < -(1 << 18) || palavras >= (1 << 18)) {
        erro_definir(emissor->erro, "ramificacao AArch64 excedeu imm19");
        return false;
    }
    uint32_t instrucao = 0xb4000000u | (((uint32_t)palavras & 0x7ffffu) << 5u);
    sobrescrever_instrucao(emissor->codigo, posicao, instrucao);
    return true;
}

static bool emitir_bloco(EmissorAarch64 *emissor, uint32_t indice) {
    const SefBlocoIr *bloco = &emissor->funcao->blocos[indice];
    emissor->posicoes_blocos[indice] = emissor->codigo->tamanho;
    for (size_t i = 0; i < bloco->quantidade; i++) {
        SefInstrucaoIr ins = bloco->instrucoes[i];
        if (ins.operacao == SEF_IR_PHI)
            continue;
        if (ins.operacao <= SEF_IR_CHAMAR_EXTERNA_I64) {
            if (!emitir_valor(emissor, ins))
                return false;
        } else if (ins.operacao == SEF_IR_SALTAR) {
            if (!emitir_salto_bloco(emissor, ins.bloco_a, indice))
                return false;
        } else if (ins.operacao == SEF_IR_RAMIFICAR) {
            if (!carregar_x0(emissor, ins.operando_a, false))
                return false;
            size_t posicao_cbz = emissor->codigo->tamanho;
            if (!emitir_instrucao(emissor, 0xb4000000u) ||
                !emitir_salto_bloco(emissor, ins.bloco_a, indice) ||
                !corrigir_cbz_local(emissor, posicao_cbz, emissor->codigo->tamanho) ||
                !emitir_salto_bloco(emissor, ins.bloco_b, indice))
                return false;
        } else if (ins.operacao == SEF_IR_RETORNAR_I64) {
            if (!carregar_x0(emissor, ins.operando_a, false) ||
                !emitir_instrucao(emissor, 0x910003ffu | (emissor->tamanho_quadro << 10u)) ||
                !emitir_instrucao(emissor, 0xa8c17bfdu) || !emitir_instrucao(emissor, 0xd65f03c0u))
                return false;
        }
    }
    return true;
}

bool sef_funcao_ir_emitir_aarch64(const SefFuncaoIr *funcao, SefCodigoNativo *codigo,
                                  SefErro *erro) {
    erro_limpar(erro);
    if (funcao == NULL || codigo == NULL || codigo->bytes != NULL || codigo->relocacoes != NULL ||
        codigo->memoria_executavel != NULL) {
        erro_definir(erro, "funcao ausente ou objeto de codigo AArch64 nao esta vazio");
        return false;
    }
    if (!sef_funcao_ir_verificar(funcao, erro))
        return false;
    uint64_t quadro = (uint64_t)funcao->quantidade_registradores * 16u + 8u;
    quadro = (quadro + 15u) & ~(uint64_t)15u;
    if (quadro > 4080u) {
        erro_definir(erro, "quadro AArch64 excedeu o limite inicial de 4080 bytes");
        return false;
    }
    EmissorAarch64 emissor = {codigo, funcao, NULL, NULL, 0, 0, (uint32_t)quadro, erro};
    emissor.posicoes_blocos = malloc(funcao->quantidade_blocos * sizeof(*emissor.posicoes_blocos));
    if (emissor.posicoes_blocos == NULL) {
        erro_definir(erro, "memoria insuficiente para blocos AArch64");
        return false;
    }
    codigo->arquitetura = SEF_ARQUITETURA_AARCH64;
    codigo->quantidade_parametros = funcao->quantidade_parametros;
    bool sucesso =
        emitir_instrucao(&emissor, 0xa9bf7bfdu) && emitir_instrucao(&emissor, 0x910003fdu) &&
        emitir_instrucao(&emissor, 0xd10003ffu | ((uint32_t)quadro << 10u)) &&
        emitir_instrucao(&emissor, 0xf90003e0u | ((deslocamento_argumentos(&emissor) / 8u) << 10u));
    for (uint32_t b = 0; sucesso && b < funcao->quantidade_blocos; b++)
        sucesso = emitir_bloco(&emissor, b);
    for (size_t i = 0; sucesso && i < emissor.quantidade_correcoes; i++) {
        CorrecaoAarch64 correcao = emissor.correcoes[i];
        int64_t distancia =
            (int64_t)emissor.posicoes_blocos[correcao.bloco] - (int64_t)correcao.posicao;
        if (distancia % 4 != 0) {
            erro_definir(erro, "salto AArch64 perdeu alinhamento");
            sucesso = false;
            break;
        }
        int64_t palavras = distancia / 4;
        if (palavras < -(1ll << 25) || palavras >= (1ll << 25)) {
            erro_definir(erro, "salto AArch64 excedeu imm26");
            sucesso = false;
            break;
        }
        sobrescrever_instrucao(codigo, correcao.posicao,
                               0x14000000u | ((uint32_t)palavras & 0x03ffffffu));
    }
    free(emissor.posicoes_blocos);
    free(emissor.correcoes);
    if (!sucesso)
        sef_codigo_nativo_liberar(codigo);
    return sucesso;
}
