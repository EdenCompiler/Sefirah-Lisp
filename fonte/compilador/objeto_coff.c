#include "sefirah/compilador.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define COFF_CABECALHO 20u
#define COFF_SECAO 40u
#define COFF_SIMBOLO 18u
#define COFF_RELOCACAO 10u
#define COFF_SIMBOLOS_FIXOS 3u

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

static void escrever_u16(unsigned char *destino, uint16_t valor) {
    destino[0] = (unsigned char)valor;
    destino[1] = (unsigned char)(valor >> 8u);
}

static void escrever_u32(unsigned char *destino, uint32_t valor) {
    for (unsigned int i = 0; i < 4; i++)
        destino[i] = (unsigned char)(valor >> (i * 8u));
}

static bool substituir_arquivo(const char *temporario, const char *destino) {
#ifdef _WIN32
    return MoveFileExA(temporario, destino, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) !=
           0;
#else
    return rename(temporario, destino) == 0;
#endif
}

bool sef_codigo_nativo_gravar_coff(const SefCodigoNativo *codigo, const char *nome_simbolo,
                                   const char *caminho, SefErro *erro) {
    erro_limpar(erro);
    if (codigo == NULL || codigo->bytes == NULL || codigo->tamanho == 0 || nome_simbolo == NULL ||
        nome_simbolo[0] == '\0' || caminho == NULL || caminho[0] == '\0') {
        erro_definir(erro, "missing code, symbol, or path for COFF object");
        return false;
    }
    if (codigo->arquitetura != SEF_ARQUITETURA_X64 &&
        codigo->arquitetura != SEF_ARQUITETURA_AARCH64) {
        erro_definir(erro, "unknown architecture for COFF object");
        return false;
    }
    if (codigo->arquitetura == SEF_ARQUITETURA_X64 && codigo->abi_x64 != SEF_ABI_X64_WINDOWS) {
        erro_definir(erro, "x86-64 COFF object requires Microsoft ABI code");
        return false;
    }
    for (size_t i = 0; i < codigo->quantidade_relocacoes; i++) {
        SefRelocacaoNativa relocacao = codigo->relocacoes[i];
        bool tipo_correto = codigo->arquitetura == SEF_ARQUITETURA_X64
                                ? relocacao.tipo == SEF_RELOCACAO_CHAMADA_REL32_X64
                                : relocacao.tipo == SEF_RELOCACAO_CHAMADA26_AARCH64;
        if (!tipo_correto || relocacao.simbolo == NULL || relocacao.simbolo[0] == '\0' ||
            relocacao.deslocamento > codigo->tamanho ||
            4u > codigo->tamanho - relocacao.deslocamento) {
            erro_definir(erro, "invalid relocation for COFF object");
            return false;
        }
    }
    size_t tamanho_nome = strlen(nome_simbolo);
    bool nome_longo = tamanho_nome > 8u;
    size_t tamanho_strings = 4u + (nome_longo ? tamanho_nome + 1u : 0u);
    size_t quantidade_relocacoes = codigo->quantidade_relocacoes;
    if (quantidade_relocacoes > UINT16_MAX || quantidade_relocacoes > UINT32_MAX - 3u ||
        quantidade_relocacoes > SIZE_MAX / COFF_RELOCACAO ||
        quantidade_relocacoes > SIZE_MAX / COFF_SIMBOLO || codigo->tamanho > UINT32_MAX ||
        tamanho_nome > UINT32_MAX - 5u || tamanho_nome > SIZE_MAX - 128u ||
        codigo->tamanho > SIZE_MAX - tamanho_nome - 128u) {
        erro_definir(erro, "code or name is too large for a COFF object");
        return false;
    }
    uint32_t *nomes_externos = NULL;
    if (quantidade_relocacoes > 0) {
        nomes_externos = calloc(quantidade_relocacoes, sizeof(*nomes_externos));
        if (nomes_externos == NULL) {
            erro_definir(erro, "not enough memory for external COFF strings");
            return false;
        }
    }
    for (size_t i = 0; i < quantidade_relocacoes; i++) {
        size_t tamanho_externo = strlen(codigo->relocacoes[i].simbolo);
        if (tamanho_externo > 8u) {
            if (tamanho_strings > UINT32_MAX - tamanho_externo - 1u ||
                tamanho_strings > SIZE_MAX - tamanho_externo - 1u) {
                free(nomes_externos);
                erro_definir(erro, "COFF string table is too large");
                return false;
            }
            nomes_externos[i] = (uint32_t)tamanho_strings;
            tamanho_strings += tamanho_externo + 1u;
        }
    }
    size_t deslocamento_codigo = COFF_CABECALHO + COFF_SECAO;
    size_t deslocamento_relocacoes = deslocamento_codigo + codigo->tamanho;
    size_t tamanho_relocacoes = quantidade_relocacoes * COFF_RELOCACAO;
    if (tamanho_relocacoes > SIZE_MAX - deslocamento_relocacoes) {
        free(nomes_externos);
        erro_definir(erro, "COFF object exceeded the address space");
        return false;
    }
    size_t deslocamento_simbolos = deslocamento_relocacoes + tamanho_relocacoes;
    size_t quantidade_simbolos = COFF_SIMBOLOS_FIXOS + quantidade_relocacoes;
    size_t tamanho_simbolos = COFF_SIMBOLO * quantidade_simbolos;
    if (deslocamento_simbolos > UINT32_MAX || tamanho_simbolos > SIZE_MAX - deslocamento_simbolos ||
        tamanho_strings > SIZE_MAX - deslocamento_simbolos - tamanho_simbolos) {
        free(nomes_externos);
        erro_definir(erro, "COFF object exceeded the address space");
        return false;
    }
    size_t tamanho_total = deslocamento_simbolos + tamanho_simbolos + tamanho_strings;
    unsigned char *objeto = calloc(tamanho_total, 1);
    if (objeto == NULL) {
        free(nomes_externos);
        erro_definir(erro, "not enough memory for a COFF object");
        return false;
    }

    escrever_u16(objeto, codigo->arquitetura == SEF_ARQUITETURA_X64 ? 0x8664u : 0xaa64u);
    escrever_u16(objeto + 2, 1);
    escrever_u32(objeto + 8, (uint32_t)deslocamento_simbolos);
    escrever_u32(objeto + 12, (uint32_t)quantidade_simbolos);

    unsigned char *secao = objeto + COFF_CABECALHO;
    memcpy(secao, ".text", 5);
    escrever_u32(secao + 16, (uint32_t)codigo->tamanho);
    escrever_u32(secao + 20, (uint32_t)deslocamento_codigo);
    escrever_u32(secao + 24, quantidade_relocacoes == 0 ? 0u : (uint32_t)deslocamento_relocacoes);
    escrever_u16(secao + 32, (uint16_t)quantidade_relocacoes);
    escrever_u32(secao + 36, 0x60500020u); /* codigo, RX, alinhamento 16 */
    memcpy(objeto + deslocamento_codigo, codigo->bytes, codigo->tamanho);

    unsigned char *simbolo_secao = objeto + deslocamento_simbolos;
    memcpy(simbolo_secao, ".text", 5);
    escrever_u16(simbolo_secao + 12, 1);
    simbolo_secao[16] = 3; /* IMAGE_SYM_CLASS_STATIC */
    simbolo_secao[17] = 1;
    unsigned char *auxiliar_secao = simbolo_secao + COFF_SIMBOLO;
    escrever_u32(auxiliar_secao, (uint32_t)codigo->tamanho);
    escrever_u16(auxiliar_secao + 4, (uint16_t)quantidade_relocacoes);

    unsigned char *simbolo_funcao = auxiliar_secao + COFF_SIMBOLO;
    if (nome_longo) {
        escrever_u32(simbolo_funcao + 4, 4);
    } else {
        memcpy(simbolo_funcao, nome_simbolo, tamanho_nome);
    }
    escrever_u16(simbolo_funcao + 12, 1);
    escrever_u16(simbolo_funcao + 14, 0x20u);
    simbolo_funcao[16] = 2; /* IMAGE_SYM_CLASS_EXTERNAL */
    for (size_t i = 0; i < quantidade_relocacoes; i++) {
        unsigned char *relocacao = objeto + deslocamento_relocacoes + i * COFF_RELOCACAO;
        escrever_u32(relocacao, (uint32_t)codigo->relocacoes[i].deslocamento);
        escrever_u32(relocacao + 4, (uint32_t)(COFF_SIMBOLOS_FIXOS + i));
        escrever_u16(relocacao + 8, codigo->arquitetura == SEF_ARQUITETURA_X64 ? 0x0004u : 0x0003u);

        const char *nome_externo = codigo->relocacoes[i].simbolo;
        size_t tamanho_externo = strlen(nome_externo);
        unsigned char *simbolo_externo =
            objeto + deslocamento_simbolos + COFF_SIMBOLO * (COFF_SIMBOLOS_FIXOS + i);
        if (nomes_externos[i] != 0)
            escrever_u32(simbolo_externo + 4, nomes_externos[i]);
        else
            memcpy(simbolo_externo, nome_externo, tamanho_externo);
        simbolo_externo[16] = 2; /* IMAGE_SYM_CLASS_EXTERNAL */
    }
    unsigned char *strings = objeto + deslocamento_simbolos + tamanho_simbolos;
    escrever_u32(strings, (uint32_t)tamanho_strings);
    if (nome_longo)
        memcpy(strings + 4, nome_simbolo, tamanho_nome + 1u);
    for (size_t i = 0; i < quantidade_relocacoes; i++) {
        if (nomes_externos[i] != 0) {
            const char *nome_externo = codigo->relocacoes[i].simbolo;
            memcpy(strings + nomes_externos[i], nome_externo, strlen(nome_externo) + 1u);
        }
    }

    size_t tamanho_caminho = strlen(caminho);
    char *temporario = malloc(tamanho_caminho + 5u);
    if (temporario == NULL) {
        free(nomes_externos);
        free(objeto);
        erro_definir(erro, "not enough memory for a temporary COFF path");
        return false;
    }
    snprintf(temporario, tamanho_caminho + 5u, "%s.tmp", caminho);
    FILE *arquivo = fopen(temporario, "wb");
    bool sucesso = arquivo != NULL && fwrite(objeto, 1, tamanho_total, arquivo) == tamanho_total &&
                   fflush(arquivo) == 0;
    if (arquivo != NULL && fclose(arquivo) != 0)
        sucesso = false;
    if (sucesso)
        sucesso = substituir_arquivo(temporario, caminho);
    if (!sucesso) {
        remove(temporario);
        erro_definir(erro, arquivo == NULL ? "could not create COFF object"
                                           : "failed to write or install COFF object");
    }
    free(temporario);
    free(nomes_externos);
    free(objeto);
    return sucesso;
}
