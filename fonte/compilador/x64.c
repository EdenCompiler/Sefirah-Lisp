#if !defined(_WIN32)
#define _DEFAULT_SOURCE
#endif

#include "sefirah/compilador.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#endif

typedef struct CorrecaoSalto {
    size_t posicao;
    uint32_t bloco;
} CorrecaoSalto;

typedef struct EmissorX64 {
    SefCodigoNativo *codigo;
    const SefFuncaoIr *funcao;
    size_t *posicoes_blocos;
    CorrecaoSalto *correcoes;
    size_t quantidade_correcoes;
    size_t capacidade_correcoes;
    SefErro *erro;
} EmissorX64;

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

void sef_codigo_nativo_iniciar(SefCodigoNativo *codigo) {
    if (codigo != NULL)
        memset(codigo, 0, sizeof(*codigo));
}

void sef_codigo_nativo_liberar(SefCodigoNativo *codigo) {
    if (codigo == NULL)
        return;
#ifdef _WIN32
    if (codigo->memoria_executavel != NULL)
        VirtualFree(codigo->memoria_executavel, 0, MEM_RELEASE);
#else
    if (codigo->memoria_executavel != NULL)
        munmap(codigo->memoria_executavel, codigo->tamanho_executavel);
#endif
    for (size_t i = 0; i < codigo->quantidade_relocacoes; i++)
        free(codigo->relocacoes[i].simbolo);
    free(codigo->relocacoes);
    free(codigo->bytes);
    memset(codigo, 0, sizeof(*codigo));
}

SefAbiX64 sef_abi_x64_hospedeiro(void) {
#ifdef _WIN32
    return SEF_ABI_X64_WINDOWS;
#else
    return SEF_ABI_X64_SYSV;
#endif
}

static bool reservar(EmissorX64 *emissor, size_t adicional) {
    SefCodigoNativo *codigo = emissor->codigo;
    if (adicional > SIZE_MAX - codigo->tamanho) {
        erro_definir(emissor->erro, "x86-64 code exceeded the size limit");
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
        erro_definir(emissor->erro, "not enough memory for x86-64 code");
        return false;
    }
    codigo->bytes = bytes;
    codigo->capacidade = capacidade;
    return true;
}

static bool emitir_u8(EmissorX64 *emissor, unsigned char valor) {
    if (!reservar(emissor, 1))
        return false;
    emissor->codigo->bytes[emissor->codigo->tamanho++] = valor;
    return true;
}

static bool emitir_u32(EmissorX64 *emissor, uint32_t valor) {
    if (!reservar(emissor, 4))
        return false;
    for (unsigned int i = 0; i < 4; i++)
        emissor->codigo->bytes[emissor->codigo->tamanho++] = (unsigned char)(valor >> (i * 8u));
    return true;
}

static bool emitir_u64(EmissorX64 *emissor, uint64_t valor) {
    if (!reservar(emissor, 8))
        return false;
    for (unsigned int i = 0; i < 8; i++)
        emissor->codigo->bytes[emissor->codigo->tamanho++] = (unsigned char)(valor >> (i * 8u));
    return true;
}

static bool adicionar_relocacao(EmissorX64 *emissor, size_t deslocamento, const char *simbolo,
                                SefFuncaoExternaI64 endereco) {
    SefCodigoNativo *codigo = emissor->codigo;
    if (codigo->quantidade_relocacoes == codigo->capacidade_relocacoes) {
        size_t capacidade =
            codigo->capacidade_relocacoes == 0 ? 4u : codigo->capacidade_relocacoes * 2u;
        SefRelocacaoNativa *relocacoes =
            realloc(codigo->relocacoes, capacidade * sizeof(*relocacoes));
        if (relocacoes == NULL) {
            erro_definir(emissor->erro, "not enough memory for an x86-64 relocation");
            return false;
        }
        codigo->relocacoes = relocacoes;
        codigo->capacidade_relocacoes = capacidade;
    }
    size_t tamanho = strlen(simbolo) + 1u;
    char *copia = malloc(tamanho);
    if (copia == NULL) {
        erro_definir(emissor->erro, "not enough memory for an x86-64 relocation symbol");
        return false;
    }
    memcpy(copia, simbolo, tamanho);
    codigo->relocacoes[codigo->quantidade_relocacoes++] =
        (SefRelocacaoNativa){deslocamento, copia, SEF_RELOCACAO_CHAMADA_REL32_X64, endereco};
    return true;
}

static uint32_t deslocamento_pilha(uint32_t registrador, uint32_t quantidade, bool temporario) {
    uint64_t indice = (uint64_t)registrador + 1u;
    if (temporario)
        indice += quantidade;
    return (uint32_t)(0u - (uint32_t)(indice * 8u));
}

static uint32_t deslocamento_argumentos(uint32_t quantidade) {
    return (uint32_t)(0u - (uint32_t)(((uint64_t)quantidade * 2u + 1u) * 8u));
}

static bool carregar_rax(EmissorX64 *emissor, uint32_t registrador, bool temporario) {
    return emitir_u8(emissor, 0x48) && emitir_u8(emissor, 0x8b) && emitir_u8(emissor, 0x85) &&
           emitir_u32(emissor,
                      deslocamento_pilha(registrador, emissor->funcao->quantidade_registradores,
                                         temporario));
}

static bool armazenar_rax(EmissorX64 *emissor, uint32_t registrador, bool temporario) {
    return emitir_u8(emissor, 0x48) && emitir_u8(emissor, 0x89) && emitir_u8(emissor, 0x85) &&
           emitir_u32(emissor,
                      deslocamento_pilha(registrador, emissor->funcao->quantidade_registradores,
                                         temporario));
}

static bool operacao_rax_memoria(EmissorX64 *emissor, unsigned char operacao,
                                 uint32_t registrador) {
    return emitir_u8(emissor, 0x48) && emitir_u8(emissor, operacao) && emitir_u8(emissor, 0x85) &&
           emitir_u32(emissor, deslocamento_pilha(
                                   registrador, emissor->funcao->quantidade_registradores, false));
}

static bool adicionar_correcao(EmissorX64 *emissor, uint32_t bloco) {
    if (emissor->quantidade_correcoes == emissor->capacidade_correcoes) {
        size_t capacidade =
            emissor->capacidade_correcoes == 0 ? 8 : emissor->capacidade_correcoes * 2;
        CorrecaoSalto *correcoes = realloc(emissor->correcoes, capacidade * sizeof(*correcoes));
        if (correcoes == NULL) {
            erro_definir(emissor->erro, "not enough memory for x86-64 jumps");
            return false;
        }
        emissor->correcoes = correcoes;
        emissor->capacidade_correcoes = capacidade;
    }
    emissor->correcoes[emissor->quantidade_correcoes++] =
        (CorrecaoSalto){emissor->codigo->tamanho, bloco};
    return emitir_u32(emissor, 0);
}

static bool emitir_copias_phi(EmissorX64 *emissor, uint32_t destino, uint32_t predecessor) {
    const SefBlocoIr *bloco = &emissor->funcao->blocos[destino];
    size_t quantidade = 0;
    while (quantidade < bloco->quantidade && bloco->instrucoes[quantidade].operacao == SEF_IR_PHI) {
        SefInstrucaoIr phi = bloco->instrucoes[quantidade];
        uint32_t origem = phi.bloco_a == predecessor ? phi.operando_a : phi.operando_b;
        if (!carregar_rax(emissor, origem, false) || !armazenar_rax(emissor, phi.destino, true))
            return false;
        quantidade++;
    }
    for (size_t i = 0; i < quantidade; i++) {
        uint32_t registrador = bloco->instrucoes[i].destino;
        if (!carregar_rax(emissor, registrador, true) ||
            !armazenar_rax(emissor, registrador, false))
            return false;
    }
    return true;
}

static bool emitir_salto_bloco(EmissorX64 *emissor, uint32_t destino, uint32_t predecessor) {
    return emitir_copias_phi(emissor, destino, predecessor) && emitir_u8(emissor, 0xe9) &&
           adicionar_correcao(emissor, destino);
}

static bool emitir_instrucao_valor(EmissorX64 *e, SefInstrucaoIr ins) {
    if (ins.operacao == SEF_IR_CONSTANTE_I64)
        return emitir_u8(e, 0x48) && emitir_u8(e, 0xb8) && emitir_u64(e, (uint64_t)ins.imediato) &&
               armazenar_rax(e, ins.destino, false);
    if (ins.operacao == SEF_IR_PARAMETRO) {
        uint64_t deslocamento = (uint64_t)ins.imediato * 8u;
        if (deslocamento > INT32_MAX) {
            erro_definir(e->erro, "parameter exceeded the x86-64 address range");
            return false;
        }
        return emitir_u8(e, 0x4c) && emitir_u8(e, 0x8b) && emitir_u8(e, 0x95) &&
               emitir_u32(e, deslocamento_argumentos(e->funcao->quantidade_registradores)) &&
               emitir_u8(e, 0x49) && emitir_u8(e, 0x8b) && emitir_u8(e, 0x82) &&
               emitir_u32(e, (uint32_t)deslocamento) && armazenar_rax(e, ins.destino, false);
    }
    if (ins.operacao == SEF_IR_CHAMAR_EXTERNA_I64) {
        SefSimboloExternoIr externa = e->funcao->externas[ins.imediato];
        uint32_t aridade = ins.bloco_a == 0 ? 1u : ins.bloco_a;
        unsigned char destino_argumento_a =
            e->codigo->abi_x64 == SEF_ABI_X64_WINDOWS ? 0xc1u : 0xc7u;
        unsigned char destino_argumento_b =
            e->codigo->abi_x64 == SEF_ABI_X64_WINDOWS ? 0xc2u : 0xc6u;
        if (!carregar_rax(e, ins.operando_a, false) || !emitir_u8(e, 0x48) || !emitir_u8(e, 0x89) ||
            !emitir_u8(e, destino_argumento_a))
            return false;
        if (aridade == 2 && (!carregar_rax(e, ins.operando_b, false) || !emitir_u8(e, 0x48) ||
                             !emitir_u8(e, 0x89) || !emitir_u8(e, destino_argumento_b)))
            return false;
        if (!emitir_u8(e, 0xe8))
            return false;
        size_t deslocamento_relocacao = e->codigo->tamanho;
        return adicionar_relocacao(e, deslocamento_relocacao, externa.nome, externa.endereco) &&
               emitir_u32(e, 0) && armazenar_rax(e, ins.destino, false);
    }
    if (!carregar_rax(e, ins.operando_a, false))
        return false;
    if (ins.operacao == SEF_IR_SOMAR_I64 && !operacao_rax_memoria(e, 0x03, ins.operando_b))
        return false;
    if (ins.operacao == SEF_IR_SUBTRAIR_I64 && !operacao_rax_memoria(e, 0x2b, ins.operando_b))
        return false;
    if (ins.operacao == SEF_IR_MULTIPLICAR_I64 &&
        !(emitir_u8(e, 0x48) && emitir_u8(e, 0x0f) && emitir_u8(e, 0xaf) && emitir_u8(e, 0x85) &&
          emitir_u32(
              e, deslocamento_pilha(ins.operando_b, e->funcao->quantidade_registradores, false))))
        return false;
    if (ins.operacao == SEF_IR_MENOR_I64 || ins.operacao == SEF_IR_MENOR_OU_IGUAL_I64) {
        unsigned char condicao = ins.operacao == SEF_IR_MENOR_I64 ? 0x9c : 0x9e;
        if (!operacao_rax_memoria(e, 0x3b, ins.operando_b) || !emitir_u8(e, 0x0f) ||
            !emitir_u8(e, condicao) || !emitir_u8(e, 0xc0) || !emitir_u8(e, 0x48) ||
            !emitir_u8(e, 0x0f) || !emitir_u8(e, 0xb6) || !emitir_u8(e, 0xc0))
            return false;
    }
    return armazenar_rax(e, ins.destino, false);
}

static bool emitir_bloco(EmissorX64 *e, uint32_t indice) {
    const SefBlocoIr *bloco = &e->funcao->blocos[indice];
    e->posicoes_blocos[indice] = e->codigo->tamanho;
    for (size_t i = 0; i < bloco->quantidade; i++) {
        SefInstrucaoIr ins = bloco->instrucoes[i];
        if (ins.operacao == SEF_IR_PHI)
            continue;
        if (ins.operacao <= SEF_IR_CHAMAR_EXTERNA_I64) {
            if (!emitir_instrucao_valor(e, ins))
                return false;
        } else if (ins.operacao == SEF_IR_SALTAR) {
            if (!emitir_salto_bloco(e, ins.bloco_a, indice))
                return false;
        } else if (ins.operacao == SEF_IR_RAMIFICAR) {
            if (!carregar_rax(e, ins.operando_a, false) || !emitir_u8(e, 0x48) ||
                !emitir_u8(e, 0x85) || !emitir_u8(e, 0xc0) || !emitir_u8(e, 0x0f) ||
                !emitir_u8(e, 0x84))
                return false;
            size_t posicao_falso = e->codigo->tamanho;
            if (!emitir_u32(e, 0) || !emitir_salto_bloco(e, ins.bloco_a, indice))
                return false;
            int64_t distancia = (int64_t)e->codigo->tamanho - (int64_t)(posicao_falso + 4);
            if (distancia < INT32_MIN || distancia > INT32_MAX) {
                erro_definir(e->erro, "x86-64 branch exceeded the rel32 range");
                return false;
            }
            uint32_t rel = (uint32_t)(int32_t)distancia;
            for (unsigned int byte = 0; byte < 4; byte++)
                e->codigo->bytes[posicao_falso + byte] = (unsigned char)(rel >> (byte * 8u));
            if (!emitir_salto_bloco(e, ins.bloco_b, indice))
                return false;
        } else if (ins.operacao == SEF_IR_RETORNAR_I64) {
            if (!carregar_rax(e, ins.operando_a, false) || !emitir_u8(e, 0xc9) ||
                !emitir_u8(e, 0xc3))
                return false;
        }
    }
    return true;
}

bool sef_funcao_ir_emitir_x64(const SefFuncaoIr *funcao, SefAbiX64 abi, SefCodigoNativo *codigo,
                              SefErro *erro) {
    erro_limpar(erro);
    if (funcao == NULL || codigo == NULL || codigo->bytes != NULL || codigo->relocacoes != NULL ||
        codigo->memoria_executavel != NULL) {
        erro_definir(erro, "missing function or nonempty x86-64 code object");
        return false;
    }
    if (abi != SEF_ABI_X64_SYSV && abi != SEF_ABI_X64_WINDOWS) {
        erro_definir(erro, "unknown x86-64 ABI");
        return false;
    }
    if (!sef_funcao_ir_verificar(funcao, erro))
        return false;
    uint64_t quadro = (uint64_t)funcao->quantidade_registradores * 16u + 8u;
    quadro = (quadro + 15u) & ~(uint64_t)15u;
    if (abi == SEF_ABI_X64_WINDOWS)
        quadro += 32u;
    if (quadro > INT32_MAX) {
        erro_definir(erro, "x86-64 stack frame is too large");
        return false;
    }
    EmissorX64 e = {codigo, funcao, NULL, NULL, 0, 0, erro};
    e.posicoes_blocos = malloc(funcao->quantidade_blocos * sizeof(*e.posicoes_blocos));
    if (e.posicoes_blocos == NULL) {
        erro_definir(erro, "not enough memory for x86-64 blocks");
        return false;
    }
    codigo->arquitetura = SEF_ARQUITETURA_X64;
    codigo->abi_x64 = abi;
    codigo->quantidade_parametros = funcao->quantidade_parametros;
    bool sucesso = emitir_u8(&e, 0x55) && emitir_u8(&e, 0x48) && emitir_u8(&e, 0x89) &&
                   emitir_u8(&e, 0xe5) && emitir_u8(&e, 0x48) && emitir_u8(&e, 0x81) &&
                   emitir_u8(&e, 0xec) && emitir_u32(&e, (uint32_t)quadro) && emitir_u8(&e, 0x49) &&
                   emitir_u8(&e, 0x89) && emitir_u8(&e, abi == SEF_ABI_X64_WINDOWS ? 0xca : 0xfa) &&
                   emitir_u8(&e, 0x4c) && emitir_u8(&e, 0x89) && emitir_u8(&e, 0x95) &&
                   emitir_u32(&e, deslocamento_argumentos(funcao->quantidade_registradores));
    for (uint32_t b = 0; sucesso && b < funcao->quantidade_blocos; b++)
        sucesso = emitir_bloco(&e, b);
    for (size_t i = 0; sucesso && i < e.quantidade_correcoes; i++) {
        CorrecaoSalto correcao = e.correcoes[i];
        int64_t distancia =
            (int64_t)e.posicoes_blocos[correcao.bloco] - (int64_t)(correcao.posicao + 4);
        if (distancia < INT32_MIN || distancia > INT32_MAX) {
            erro_definir(erro, "x86-64 jump exceeded the rel32 range");
            sucesso = false;
            break;
        }
        uint32_t rel = (uint32_t)(int32_t)distancia;
        for (unsigned int byte = 0; byte < 4; byte++)
            codigo->bytes[correcao.posicao + byte] = (unsigned char)(rel >> (byte * 8u));
    }
    free(e.posicoes_blocos);
    free(e.correcoes);
    if (!sucesso)
        sef_codigo_nativo_liberar(codigo);
    return sucesso;
}

static bool vincular_externa_i64(SefCodigoNativo *codigo, const char *simbolo,
                                 SefFuncaoExternaI64 endereco, SefErro *erro) {
    erro_limpar(erro);
    if (codigo == NULL || simbolo == NULL || simbolo[0] == '\0' || endereco == NULL ||
        codigo->memoria_executavel != NULL) {
        erro_definir(erro, "invalid code, symbol, or address for JIT binding");
        return false;
    }
    bool encontrou = false;
    for (size_t i = 0; i < codigo->quantidade_relocacoes; i++) {
        if (strcmp(codigo->relocacoes[i].simbolo, simbolo) == 0) {
            codigo->relocacoes[i].endereco = endereco;
            encontrou = true;
        }
    }
    if (!encontrou)
        erro_definir(erro, "external symbol does not belong to the native code");
    return encontrou;
}

bool sef_codigo_nativo_vincular_externa_i64(SefCodigoNativo *codigo, const char *simbolo,
                                            SefFuncaoExternaI64 endereco, SefErro *erro) {
    return vincular_externa_i64(codigo, simbolo, endereco, erro);
}

bool sef_codigo_nativo_vincular_externa_i64_binaria(SefCodigoNativo *codigo, const char *simbolo,
                                                    SefFuncaoExternaI64Binaria endereco,
                                                    SefErro *erro) {
    SefFuncaoExternaI64 endereco_armazenado = NULL;
    _Static_assert(sizeof(endereco) == sizeof(endereco_armazenado),
                   "i64 function pointers must have the same size");
    memcpy(&endereco_armazenado, &endereco, sizeof(endereco_armazenado));
    return vincular_externa_i64(codigo, simbolo, endereco_armazenado, erro);
}

static void escrever_u32_memoria(unsigned char *destino, uint32_t valor) {
    for (unsigned int i = 0; i < 4; i++)
        destino[i] = (unsigned char)(valor >> (i * 8u));
}

static void escrever_u64_memoria(unsigned char *destino, uint64_t valor) {
    for (unsigned int i = 0; i < 8; i++)
        destino[i] = (unsigned char)(valor >> (i * 8u));
}

static uint64_t endereco_externo_u64(SefFuncaoExternaI64 endereco) {
    uint64_t valor = 0;
    _Static_assert(sizeof(endereco) <= sizeof(valor),
                   "external function pointer must fit in 64 bits");
    memcpy(&valor, &endereco, sizeof(endereco));
    return valor;
}

static bool aplicar_trampolins(SefCodigoNativo *codigo, unsigned char *memoria,
                               size_t inicio_trampolins, size_t tamanho_trampolim, SefErro *erro) {
    memcpy(memoria, codigo->bytes, codigo->tamanho);
    for (size_t i = 0; i < codigo->quantidade_relocacoes; i++) {
        SefRelocacaoNativa relocacao = codigo->relocacoes[i];
        unsigned char *trampolim = memoria + inicio_trampolins + i * tamanho_trampolim;
        if (relocacao.endereco == NULL || relocacao.deslocamento > codigo->tamanho ||
            4u > codigo->tamanho - relocacao.deslocamento) {
            erro_definir(erro, "external symbol has not been bound to the JIT");
            return false;
        }
        if (codigo->arquitetura == SEF_ARQUITETURA_X64) {
            if (relocacao.tipo != SEF_RELOCACAO_CHAMADA_REL32_X64) {
                erro_definir(erro, "invalid x86-64 relocation type for JIT");
                return false;
            }
            int64_t distancia = (int64_t)(trampolim - (memoria + relocacao.deslocamento + 4u));
            if (distancia < INT32_MIN || distancia > INT32_MAX) {
                erro_definir(erro, "x86-64 trampoline exceeded the rel32 range");
                return false;
            }
            escrever_u32_memoria(memoria + relocacao.deslocamento, (uint32_t)(int32_t)distancia);
            trampolim[0] = 0x48;
            trampolim[1] = 0xb8; /* mov rax, endereco */
            escrever_u64_memoria(trampolim + 2, endereco_externo_u64(relocacao.endereco));
            trampolim[10] = 0xff;
            trampolim[11] = 0xe0; /* jmp rax */
        } else {
            if (relocacao.tipo != SEF_RELOCACAO_CHAMADA26_AARCH64 ||
                relocacao.deslocamento % 4u != 0) {
                erro_definir(erro, "invalid AArch64 relocation type or alignment for JIT");
                return false;
            }
            int64_t distancia = (int64_t)(trampolim - (memoria + relocacao.deslocamento));
            int64_t palavras = distancia / 4;
            if (distancia % 4 != 0 || palavras < -(1ll << 25) || palavras >= (1ll << 25)) {
                erro_definir(erro, "AArch64 trampoline exceeded the BL range");
                return false;
            }
            escrever_u32_memoria(memoria + relocacao.deslocamento,
                                 0x94000000u | ((uint32_t)palavras & 0x03ffffffu));
            escrever_u32_memoria(trampolim, 0x58000050u);     /* ldr x16, literal */
            escrever_u32_memoria(trampolim + 4, 0xd61f0200u); /* br x16 */
            escrever_u64_memoria(trampolim + 8, endereco_externo_u64(relocacao.endereco));
        }
    }
    return true;
}

bool sef_codigo_nativo_preparar(SefCodigoNativo *codigo, SefErro *erro) {
    erro_limpar(erro);
    if (codigo == NULL || codigo->bytes == NULL || codigo->tamanho == 0 ||
        codigo->memoria_executavel != NULL) {
        erro_definir(erro, "native code is missing or already prepared");
        return false;
    }
#if defined(__x86_64__) || defined(_M_X64)
    if (codigo->arquitetura != SEF_ARQUITETURA_X64 || codigo->abi_x64 != sef_abi_x64_hospedeiro()) {
        erro_definir(erro, "code architecture or ABI does not match the JIT host");
        return false;
    }
#elif defined(__aarch64__) || defined(_M_ARM64)
    if (codigo->arquitetura != SEF_ARQUITETURA_AARCH64) {
        erro_definir(erro, "code architecture does not match the JIT host");
        return false;
    }
#else
    erro_definir(erro, "host does not have a supported JIT backend");
    return false;
#endif
    size_t tamanho_trampolim = codigo->arquitetura == SEF_ARQUITETURA_X64 ? 12u : 16u;
    if (codigo->arquitetura == SEF_ARQUITETURA_AARCH64 && codigo->tamanho > SIZE_MAX - 7u) {
        erro_definir(erro, "trampoline alignment exceeded the address space");
        return false;
    }
    size_t inicio_trampolins = codigo->arquitetura == SEF_ARQUITETURA_X64
                                   ? codigo->tamanho
                                   : (codigo->tamanho + 7u) & ~(size_t)7u;
    if (codigo->quantidade_relocacoes > (SIZE_MAX - inicio_trampolins) / tamanho_trampolim) {
        erro_definir(erro, "trampolines exceeded the address space");
        return false;
    }
    size_t tamanho_total = inicio_trampolins + codigo->quantidade_relocacoes * tamanho_trampolim;
#ifdef _WIN32
    void *memoria = VirtualAlloc(NULL, tamanho_total, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (memoria == NULL) {
        erro_definir(erro, "Windows refused memory for native code");
        return false;
    }
    if (!aplicar_trampolins(codigo, memoria, inicio_trampolins, tamanho_trampolim, erro)) {
        VirtualFree(memoria, 0, MEM_RELEASE);
        return false;
    }
    DWORD protecao_anterior;
    if (!VirtualProtect(memoria, tamanho_total, PAGE_EXECUTE_READ, &protecao_anterior)) {
        VirtualFree(memoria, 0, MEM_RELEASE);
        erro_definir(erro, "Windows refused to make native code executable");
        return false;
    }
    FlushInstructionCache(GetCurrentProcess(), memoria, tamanho_total);
#else
    void *memoria =
        mmap(NULL, tamanho_total, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memoria == MAP_FAILED) {
        erro_definir(erro, "the system refused memory for native code");
        return false;
    }
    if (!aplicar_trampolins(codigo, memoria, inicio_trampolins, tamanho_trampolim, erro)) {
        munmap(memoria, tamanho_total);
        return false;
    }
    if (mprotect(memoria, tamanho_total, PROT_READ | PROT_EXEC) != 0) {
        munmap(memoria, tamanho_total);
        erro_definir(erro, "the system refused to make native code executable");
        return false;
    }
    __builtin___clear_cache((char *)memoria, (char *)memoria + tamanho_total);
#endif
    codigo->memoria_executavel = memoria;
    codigo->tamanho_executavel = tamanho_total;
    return true;
}

bool sef_codigo_nativo_executar_i64(const SefCodigoNativo *codigo, const int64_t *argumentos,
                                    size_t quantidade_argumentos, int64_t *resultado,
                                    SefErro *erro) {
    erro_limpar(erro);
#if defined(__x86_64__) || defined(_M_X64)
    if (codigo == NULL || codigo->memoria_executavel == NULL || resultado == NULL ||
        quantidade_argumentos != codigo->quantidade_parametros ||
        (quantidade_argumentos > 0 && argumentos == NULL) ||
        codigo->arquitetura != SEF_ARQUITETURA_X64 || codigo->abi_x64 != sef_abi_x64_hospedeiro()) {
        erro_definir(erro, "native code or ABI is incompatible with the call");
        return false;
    }
    typedef int64_t (*FuncaoNativa)(const int64_t *);
    _Static_assert(sizeof(FuncaoNativa) == sizeof(void *),
                   "function pointer must have the size of void*");
    FuncaoNativa funcao;
    memcpy(&funcao, &codigo->memoria_executavel, sizeof(funcao));
    *resultado = funcao(argumentos);
    return true;
#elif defined(__aarch64__) || defined(_M_ARM64)
    if (codigo == NULL || codigo->memoria_executavel == NULL || resultado == NULL ||
        quantidade_argumentos != codigo->quantidade_parametros ||
        (quantidade_argumentos > 0 && argumentos == NULL) ||
        codigo->arquitetura != SEF_ARQUITETURA_AARCH64) {
        erro_definir(erro, "native code or architecture is incompatible with the call");
        return false;
    }
    typedef int64_t (*FuncaoNativa)(const int64_t *);
    _Static_assert(sizeof(FuncaoNativa) == sizeof(void *),
                   "function pointer must have the size of void*");
    FuncaoNativa funcao;
    memcpy(&funcao, &codigo->memoria_executavel, sizeof(funcao));
    *resultado = funcao(argumentos);
    return true;
#else
    (void)codigo;
    (void)argumentos;
    (void)quantidade_argumentos;
    (void)resultado;
    erro_definir(erro, "native execution is unavailable on this architecture");
    return false;
#endif
}
