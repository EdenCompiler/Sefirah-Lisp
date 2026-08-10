#ifndef SEFIRAH_COMPILADOR_H
#define SEFIRAH_COMPILADOR_H

#include "sefirah/erro.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum SefOperacaoIr {
    SEF_IR_CONSTANTE_I64,
    SEF_IR_PARAMETRO,
    SEF_IR_PHI,
    SEF_IR_SOMAR_I64,
    SEF_IR_SUBTRAIR_I64,
    SEF_IR_MULTIPLICAR_I64,
    SEF_IR_MENOR_I64,
    SEF_IR_MENOR_OU_IGUAL_I64,
    SEF_IR_CHAMAR_EXTERNA_I64,
    SEF_IR_SALTAR,
    SEF_IR_RAMIFICAR,
    SEF_IR_RETORNAR_I64
} SefOperacaoIr;

typedef struct SefInstrucaoIr {
    SefOperacaoIr operacao;
    uint32_t destino;
    uint32_t operando_a;
    uint32_t operando_b;
    int64_t imediato;
    uint32_t bloco_a; /* primeiro alvo; aridade em SEF_IR_CHAMAR_EXTERNA_I64 */
    uint32_t bloco_b;
} SefInstrucaoIr;

typedef struct SefBlocoIr {
    SefInstrucaoIr *instrucoes;
    size_t quantidade;
    size_t capacidade;
} SefBlocoIr;

typedef int64_t (*SefFuncaoExternaI64)(int64_t argumento);
typedef int64_t (*SefFuncaoExternaI64Binaria)(int64_t argumento_a, int64_t argumento_b);

typedef struct SefSimboloExternoIr {
    char *nome;
    SefFuncaoExternaI64 endereco;
} SefSimboloExternoIr;

typedef struct SefFuncaoIr {
    const char *nome;
    uint32_t quantidade_parametros;
    uint32_t quantidade_registradores;
    SefBlocoIr *blocos;
    size_t quantidade_blocos;
    size_t capacidade_blocos;
    SefSimboloExternoIr *externas;
    size_t quantidade_externas;
    size_t capacidade_externas;
} SefFuncaoIr;

typedef enum SefAbiX64 { SEF_ABI_X64_SYSV, SEF_ABI_X64_WINDOWS } SefAbiX64;

typedef enum SefArquiteturaNativa {
    SEF_ARQUITETURA_X64,
    SEF_ARQUITETURA_AARCH64
} SefArquiteturaNativa;

typedef enum SefTipoRelocacaoNativa {
    SEF_RELOCACAO_CHAMADA_REL32_X64,
    SEF_RELOCACAO_CHAMADA26_AARCH64
} SefTipoRelocacaoNativa;

typedef struct SefRelocacaoNativa {
    size_t deslocamento;
    char *simbolo;
    SefTipoRelocacaoNativa tipo;
    SefFuncaoExternaI64 endereco;
} SefRelocacaoNativa;

typedef struct SefCodigoNativo {
    unsigned char *bytes;
    size_t tamanho;
    size_t capacidade;
    void *memoria_executavel;
    size_t tamanho_executavel;
    uint32_t quantidade_parametros;
    SefArquiteturaNativa arquitetura;
    SefAbiX64 abi_x64;
    SefRelocacaoNativa *relocacoes;
    size_t quantidade_relocacoes;
    size_t capacidade_relocacoes;
} SefCodigoNativo;

void sef_funcao_ir_iniciar(SefFuncaoIr *funcao, const char *nome, uint32_t parametros,
                           uint32_t registradores);
void sef_funcao_ir_liberar(SefFuncaoIr *funcao);
bool sef_funcao_ir_adicionar_bloco(SefFuncaoIr *funcao, uint32_t *indice, SefErro *erro);
bool sef_funcao_ir_adicionar_externa_i64(SefFuncaoIr *funcao, const char *nome,
                                         SefFuncaoExternaI64 endereco, uint32_t *indice,
                                         SefErro *erro);
bool sef_funcao_ir_adicionar_externa_i64_binaria(SefFuncaoIr *funcao, const char *nome,
                                                 SefFuncaoExternaI64Binaria endereco,
                                                 uint32_t *indice, SefErro *erro);
bool sef_bloco_ir_emitir(SefFuncaoIr *funcao, uint32_t bloco, SefInstrucaoIr instrucao,
                         SefErro *erro);
bool sef_funcao_ir_verificar(const SefFuncaoIr *funcao, SefErro *erro);
bool sef_funcao_ir_executar_i64(const SefFuncaoIr *funcao, const int64_t *argumentos,
                                size_t quantidade_argumentos, int64_t *resultado, SefErro *erro);

void sef_codigo_nativo_iniciar(SefCodigoNativo *codigo);
void sef_codigo_nativo_liberar(SefCodigoNativo *codigo);
SefAbiX64 sef_abi_x64_hospedeiro(void);
bool sef_funcao_ir_emitir_x64(const SefFuncaoIr *funcao, SefAbiX64 abi, SefCodigoNativo *codigo,
                              SefErro *erro);
bool sef_funcao_ir_emitir_aarch64(const SefFuncaoIr *funcao, SefCodigoNativo *codigo,
                                  SefErro *erro);
bool sef_codigo_nativo_vincular_externa_i64(SefCodigoNativo *codigo, const char *simbolo,
                                            SefFuncaoExternaI64 endereco, SefErro *erro);
bool sef_codigo_nativo_vincular_externa_i64_binaria(SefCodigoNativo *codigo, const char *simbolo,
                                                    SefFuncaoExternaI64Binaria endereco,
                                                    SefErro *erro);
bool sef_codigo_nativo_preparar(SefCodigoNativo *codigo, SefErro *erro);
bool sef_codigo_nativo_executar_i64(const SefCodigoNativo *codigo, const int64_t *argumentos,
                                    size_t quantidade_argumentos, int64_t *resultado,
                                    SefErro *erro);
bool sef_codigo_nativo_gravar_elf(const SefCodigoNativo *codigo, const char *nome_simbolo,
                                  const char *caminho, SefErro *erro);
bool sef_codigo_nativo_gravar_coff(const SefCodigoNativo *codigo, const char *nome_simbolo,
                                   const char *caminho, SefErro *erro);
bool sef_codigo_nativo_gravar_macho(const SefCodigoNativo *codigo, const char *nome_simbolo,
                                    const char *caminho, SefErro *erro);

#endif
