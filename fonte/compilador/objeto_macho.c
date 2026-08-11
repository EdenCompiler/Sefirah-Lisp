#include "sefirah/compilador.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define MACHO_CABECALHO_64 32u
#define MACHO_COMANDO_SEGMENTO_64 72u
#define MACHO_SECAO_64 80u
#define MACHO_COMANDO_SIMBOLOS 24u
#define MACHO_SIMBOLO_64 16u
#define MACHO_RELOCACAO 8u

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

static void escrever_u64(unsigned char *destino, uint64_t valor) {
    for (unsigned int i = 0; i < 8; i++)
        destino[i] = (unsigned char)(valor >> (i * 8u));
}

static size_t alinhar(size_t valor, size_t alinhamento) {
    return (valor + alinhamento - 1u) & ~(alinhamento - 1u);
}

static bool substituir_arquivo(const char *temporario, const char *destino) {
#ifdef _WIN32
    return MoveFileExA(temporario, destino, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) !=
           0;
#else
    return rename(temporario, destino) == 0;
#endif
}

bool sef_codigo_nativo_gravar_macho(const SefCodigoNativo *codigo, const char *nome_simbolo,
                                    const char *caminho, SefErro *erro) {
    erro_limpar(erro);
    if (codigo == NULL || codigo->bytes == NULL || codigo->tamanho == 0 || nome_simbolo == NULL ||
        nome_simbolo[0] == '\0' || caminho == NULL || caminho[0] == '\0') {
        erro_definir(erro, "codigo, simbolo ou caminho ausente para objeto Mach-O");
        return false;
    }
    if (codigo->arquitetura != SEF_ARQUITETURA_X64 &&
        codigo->arquitetura != SEF_ARQUITETURA_AARCH64) {
        erro_definir(erro, "arquitetura desconhecida para objeto Mach-O");
        return false;
    }
    if (codigo->arquitetura == SEF_ARQUITETURA_X64 && codigo->abi_x64 != SEF_ABI_X64_SYSV) {
        erro_definir(erro, "objeto Mach-O x86-64 exige codigo da ABI System V");
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
            erro_definir(erro, "relocacao invalida para objeto Mach-O");
            return false;
        }
    }

    size_t tamanho_nome = strlen(nome_simbolo);
    bool precisa_sublinhado = nome_simbolo[0] != '_';
    size_t quantidade_relocacoes = codigo->quantidade_relocacoes;
    size_t tamanho_comandos = MACHO_COMANDO_SEGMENTO_64 + MACHO_SECAO_64 + MACHO_COMANDO_SIMBOLOS;
    size_t deslocamento_codigo = MACHO_CABECALHO_64 + tamanho_comandos;
    if (quantidade_relocacoes > UINT32_MAX - 1u || quantidade_relocacoes > 0x00fffffeu ||
        quantidade_relocacoes > SIZE_MAX / MACHO_RELOCACAO ||
        quantidade_relocacoes > SIZE_MAX / MACHO_SIMBOLO_64 || codigo->tamanho > UINT32_MAX ||
        tamanho_nome > UINT32_MAX - 3u || tamanho_nome > SIZE_MAX - 256u ||
        codigo->tamanho > SIZE_MAX - tamanho_nome - 256u) {
        erro_definir(erro, "codigo ou nome grande demais para objeto Mach-O");
        return false;
    }
    size_t deslocamento_relocacoes = alinhar(deslocamento_codigo + codigo->tamanho, 4u);
    size_t tamanho_relocacoes = quantidade_relocacoes * MACHO_RELOCACAO;
    if (tamanho_relocacoes > SIZE_MAX - deslocamento_relocacoes) {
        erro_definir(erro, "objeto Mach-O excedeu o espaco de enderecamento");
        return false;
    }
    size_t deslocamento_simbolo = alinhar(deslocamento_relocacoes + tamanho_relocacoes, 8u);
    size_t tamanho_strings = 1u + (precisa_sublinhado ? 1u : 0u) + tamanho_nome + 1u;
    uint32_t *nomes_externos = NULL;
    if (quantidade_relocacoes > 0) {
        nomes_externos = malloc(quantidade_relocacoes * sizeof(*nomes_externos));
        if (nomes_externos == NULL) {
            erro_definir(erro, "memoria insuficiente para strings externas Mach-O");
            return false;
        }
    }
    for (size_t i = 0; i < quantidade_relocacoes; i++) {
        const char *externo = codigo->relocacoes[i].simbolo;
        size_t tamanho_externo = strlen(externo);
        size_t adicional = (externo[0] == '_' ? 0u : 1u) + tamanho_externo + 1u;
        if (tamanho_strings > UINT32_MAX - adicional || tamanho_strings > SIZE_MAX - adicional) {
            free(nomes_externos);
            erro_definir(erro, "tabela de strings Mach-O grande demais");
            return false;
        }
        nomes_externos[i] = (uint32_t)tamanho_strings;
        tamanho_strings += adicional;
    }
    size_t tamanho_simbolos = MACHO_SIMBOLO_64 * (1u + quantidade_relocacoes);
    if (tamanho_simbolos > SIZE_MAX - deslocamento_simbolo) {
        free(nomes_externos);
        erro_definir(erro, "objeto Mach-O excedeu o espaco de enderecamento");
        return false;
    }
    size_t deslocamento_strings = deslocamento_simbolo + tamanho_simbolos;
    if (deslocamento_relocacoes > UINT32_MAX || deslocamento_simbolo > UINT32_MAX ||
        deslocamento_strings > UINT32_MAX || tamanho_strings > SIZE_MAX - deslocamento_strings) {
        free(nomes_externos);
        erro_definir(erro, "objeto Mach-O excedeu o espaco de enderecamento");
        return false;
    }
    size_t tamanho_total = deslocamento_strings + tamanho_strings;
    unsigned char *objeto = calloc(tamanho_total, 1);
    if (objeto == NULL) {
        free(nomes_externos);
        erro_definir(erro, "memoria insuficiente para objeto Mach-O");
        return false;
    }

    escrever_u32(objeto, 0xfeedfacfu); /* MH_MAGIC_64 */
    escrever_u32(objeto + 4,
                 codigo->arquitetura == SEF_ARQUITETURA_X64 ? 0x01000007u : 0x0100000cu);
    escrever_u32(objeto + 8, codigo->arquitetura == SEF_ARQUITETURA_X64 ? 3u : 0u);
    escrever_u32(objeto + 12, 1u); /* MH_OBJECT */
    escrever_u32(objeto + 16, 2u);
    escrever_u32(objeto + 20, (uint32_t)tamanho_comandos);
    escrever_u32(objeto + 24, 0x2000u); /* MH_SUBSECTIONS_VIA_SYMBOLS */

    unsigned char *segmento = objeto + MACHO_CABECALHO_64;
    escrever_u32(segmento, 0x19u); /* LC_SEGMENT_64 */
    escrever_u32(segmento + 4, MACHO_COMANDO_SEGMENTO_64 + MACHO_SECAO_64);
    escrever_u64(segmento + 32, codigo->tamanho);
    escrever_u64(segmento + 40, deslocamento_codigo);
    escrever_u64(segmento + 48, codigo->tamanho);
    escrever_u32(segmento + 56, 7u);
    escrever_u32(segmento + 60, 7u);
    escrever_u32(segmento + 64, 1u);

    unsigned char *secao = segmento + MACHO_COMANDO_SEGMENTO_64;
    memcpy(secao, "__text", 6);
    memcpy(secao + 16, "__TEXT", 6);
    escrever_u64(secao + 40, codigo->tamanho);
    escrever_u32(secao + 48, (uint32_t)deslocamento_codigo);
    escrever_u32(secao + 52, 4u);
    escrever_u32(secao + 56, quantidade_relocacoes == 0 ? 0u : (uint32_t)deslocamento_relocacoes);
    escrever_u32(secao + 60, (uint32_t)quantidade_relocacoes);
    escrever_u32(secao + 64, 0x80000400u);

    unsigned char *comando_simbolos = secao + MACHO_SECAO_64;
    escrever_u32(comando_simbolos, 2u); /* LC_SYMTAB */
    escrever_u32(comando_simbolos + 4, MACHO_COMANDO_SIMBOLOS);
    escrever_u32(comando_simbolos + 8, (uint32_t)deslocamento_simbolo);
    escrever_u32(comando_simbolos + 12, (uint32_t)(1u + quantidade_relocacoes));
    escrever_u32(comando_simbolos + 16, (uint32_t)deslocamento_strings);
    escrever_u32(comando_simbolos + 20, (uint32_t)tamanho_strings);

    memcpy(objeto + deslocamento_codigo, codigo->bytes, codigo->tamanho);
    unsigned char *simbolo = objeto + deslocamento_simbolo;
    escrever_u32(simbolo, 1u);
    simbolo[4] = 0x0fu; /* N_SECT | N_EXT */
    simbolo[5] = 1u;
    escrever_u16(simbolo + 6, 0u);
    unsigned char *strings = objeto + deslocamento_strings;
    size_t inicio_nome = 1u;
    if (precisa_sublinhado)
        strings[inicio_nome++] = '_';
    memcpy(strings + inicio_nome, nome_simbolo, tamanho_nome + 1u);
    for (size_t i = 0; i < quantidade_relocacoes; i++) {
        unsigned char *relocacao = objeto + deslocamento_relocacoes + i * MACHO_RELOCACAO;
        escrever_u32(relocacao, (uint32_t)codigo->relocacoes[i].deslocamento);
        uint32_t atributos =
            (uint32_t)(1u + i) | (1u << 24u) | (2u << 25u) | (1u << 27u) | (2u << 28u);
        escrever_u32(relocacao + 4, atributos);

        unsigned char *simbolo_externo =
            objeto + deslocamento_simbolo + MACHO_SIMBOLO_64 * (1u + i);
        escrever_u32(simbolo_externo, nomes_externos[i]);
        simbolo_externo[4] = 0x01u; /* N_UNDF | N_EXT */
        const char *nome_externo = codigo->relocacoes[i].simbolo;
        size_t inicio_externo = nomes_externos[i];
        if (nome_externo[0] != '_')
            strings[inicio_externo++] = '_';
        memcpy(strings + inicio_externo, nome_externo, strlen(nome_externo) + 1u);
    }

    size_t tamanho_caminho = strlen(caminho);
    char *temporario = malloc(tamanho_caminho + 5u);
    if (temporario == NULL) {
        free(nomes_externos);
        free(objeto);
        erro_definir(erro, "memoria insuficiente para caminho Mach-O temporario");
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
        erro_definir(erro, arquivo == NULL ? "nao foi possivel criar objeto Mach-O"
                                           : "falha ao gravar ou instalar objeto Mach-O");
    }
    free(temporario);
    free(nomes_externos);
    free(objeto);
    return sucesso;
}
