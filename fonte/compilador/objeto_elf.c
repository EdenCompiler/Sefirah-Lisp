#include "sefirah/compilador.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define ELF_TAMANHO_CABECALHO 64u
#define ELF_TAMANHO_SECAO 64u
#define ELF_TAMANHO_SIMBOLO 24u
#define ELF_QUANTIDADE_SECOES 6u

static const unsigned char nomes_secoes[] = "\0.text\0.rela.text\0.symtab\0.strtab\0.shstrtab\0";

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

static size_t alinhar(size_t valor, size_t alinhamento) {
    size_t resto = valor % alinhamento;
    return resto == 0 ? valor : valor + alinhamento - resto;
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

static void escrever_cabecalho_secao(unsigned char *secao, uint32_t nome, uint32_t tipo,
                                     uint64_t atributos, uint64_t deslocamento, uint64_t tamanho,
                                     uint32_t ligada, uint32_t informacao, uint64_t alinhamento,
                                     uint64_t tamanho_entrada) {
    escrever_u32(secao, nome);
    escrever_u32(secao + 4, tipo);
    escrever_u64(secao + 8, atributos);
    escrever_u64(secao + 24, deslocamento);
    escrever_u64(secao + 32, tamanho);
    escrever_u32(secao + 40, ligada);
    escrever_u32(secao + 44, informacao);
    escrever_u64(secao + 48, alinhamento);
    escrever_u64(secao + 56, tamanho_entrada);
}

static bool substituir_arquivo(const char *temporario, const char *destino) {
#ifdef _WIN32
    return MoveFileExA(temporario, destino, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) !=
           0;
#else
    return rename(temporario, destino) == 0;
#endif
}

bool sef_codigo_nativo_gravar_elf(const SefCodigoNativo *codigo, const char *nome_simbolo,
                                  const char *caminho, SefErro *erro) {
    erro_limpar(erro);
    if (codigo == NULL || codigo->bytes == NULL || codigo->tamanho == 0 || nome_simbolo == NULL ||
        nome_simbolo[0] == '\0' || caminho == NULL || caminho[0] == '\0') {
        erro_definir(erro, "missing code, symbol, or path for ELF object");
        return false;
    }
    if (codigo->arquitetura != SEF_ARQUITETURA_X64 &&
        codigo->arquitetura != SEF_ARQUITETURA_AARCH64) {
        erro_definir(erro, "unknown architecture for ELF object");
        return false;
    }
    if (codigo->arquitetura == SEF_ARQUITETURA_X64 && codigo->abi_x64 != SEF_ABI_X64_SYSV) {
        erro_definir(erro, "x86-64 ELF object requires System V ABI code");
        return false;
    }
    for (size_t i = 0; i < codigo->quantidade_relocacoes; i++) {
        SefRelocacaoNativa relocacao = codigo->relocacoes[i];
        bool tipo_correto = codigo->arquitetura == SEF_ARQUITETURA_X64
                                ? relocacao.tipo == SEF_RELOCACAO_CHAMADA_REL32_X64
                                : relocacao.tipo == SEF_RELOCACAO_CHAMADA26_AARCH64;
        size_t tamanho_campo = codigo->arquitetura == SEF_ARQUITETURA_X64 ? 4u : 4u;
        if (!tipo_correto || relocacao.simbolo == NULL || relocacao.simbolo[0] == '\0' ||
            relocacao.deslocamento > codigo->tamanho ||
            tamanho_campo > codigo->tamanho - relocacao.deslocamento) {
            erro_definir(erro, "invalid relocation for ELF object");
            return false;
        }
    }
    size_t tamanho_nome = strlen(nome_simbolo);
    size_t quantidade_relocacoes = codigo->quantidade_relocacoes;
    if (quantidade_relocacoes > UINT32_MAX - 3u ||
        quantidade_relocacoes > SIZE_MAX / ELF_TAMANHO_SIMBOLO ||
        quantidade_relocacoes > SIZE_MAX / 24u || codigo->tamanho > UINT32_MAX ||
        tamanho_nome > UINT32_MAX - 2u || tamanho_nome > SIZE_MAX - 512u ||
        codigo->tamanho > SIZE_MAX - tamanho_nome - 512u) {
        erro_definir(erro, "code or name is too large for an ELF object");
        return false;
    }
    size_t deslocamento_texto = ELF_TAMANHO_CABECALHO;
    if (codigo->tamanho > SIZE_MAX - deslocamento_texto) {
        erro_definir(erro, "ELF object exceeded the address space");
        return false;
    }
    size_t deslocamento_relocacoes = alinhar(deslocamento_texto + codigo->tamanho, 8);
    size_t tamanho_relocacoes = quantidade_relocacoes * 24u;
    if (tamanho_relocacoes > SIZE_MAX - deslocamento_relocacoes) {
        erro_definir(erro, "ELF relocations exceeded the address space");
        return false;
    }
    size_t deslocamento_simbolos = alinhar(deslocamento_relocacoes + tamanho_relocacoes, 8);
    size_t tamanho_simbolos = ELF_TAMANHO_SIMBOLO * (3u + quantidade_relocacoes);
    if (tamanho_simbolos > SIZE_MAX - deslocamento_simbolos) {
        erro_definir(erro, "ELF symbols exceeded the address space");
        return false;
    }
    size_t deslocamento_strings = deslocamento_simbolos + tamanho_simbolos;
    size_t tamanho_strings = tamanho_nome + 2u;
    uint32_t *nomes_externos = NULL;
    if (quantidade_relocacoes > 0) {
        nomes_externos = malloc(quantidade_relocacoes * sizeof(*nomes_externos));
        if (nomes_externos == NULL) {
            erro_definir(erro, "not enough memory for external ELF strings");
            return false;
        }
    }
    for (size_t i = 0; i < quantidade_relocacoes; i++) {
        size_t tamanho_externo = strlen(codigo->relocacoes[i].simbolo) + 1u;
        if (tamanho_strings > UINT32_MAX - tamanho_externo ||
            tamanho_strings > SIZE_MAX - tamanho_externo) {
            free(nomes_externos);
            erro_definir(erro, "ELF string table is too large");
            return false;
        }
        nomes_externos[i] = (uint32_t)tamanho_strings;
        tamanho_strings += tamanho_externo;
    }
    if (tamanho_strings > SIZE_MAX - deslocamento_strings) {
        free(nomes_externos);
        erro_definir(erro, "ELF string table is too large");
        return false;
    }
    size_t deslocamento_nomes_secoes = deslocamento_strings + tamanho_strings;
    size_t tamanho_nomes_secoes = sizeof(nomes_secoes) - 1u;
    if (tamanho_nomes_secoes > SIZE_MAX - deslocamento_nomes_secoes) {
        free(nomes_externos);
        erro_definir(erro, "ELF section names exceeded the limit");
        return false;
    }
    size_t deslocamento_secoes = alinhar(deslocamento_nomes_secoes + tamanho_nomes_secoes, 8);
    size_t tamanho_secoes = ELF_TAMANHO_SECAO * ELF_QUANTIDADE_SECOES;
    if (tamanho_secoes > SIZE_MAX - deslocamento_secoes) {
        free(nomes_externos);
        erro_definir(erro, "ELF object exceeded the address space");
        return false;
    }
    size_t tamanho_total = deslocamento_secoes + tamanho_secoes;
    unsigned char *objeto = calloc(tamanho_total, 1);
    if (objeto == NULL) {
        free(nomes_externos);
        erro_definir(erro, "not enough memory for an ELF object");
        return false;
    }

    objeto[0] = 0x7f;
    objeto[1] = 'E';
    objeto[2] = 'L';
    objeto[3] = 'F';
    objeto[4] = 2;                /* ELFCLASS64 */
    objeto[5] = 1;                /* ELFDATA2LSB */
    objeto[6] = 1;                /* EV_CURRENT */
    escrever_u16(objeto + 16, 1); /* ET_REL */
    escrever_u16(objeto + 18, codigo->arquitetura == SEF_ARQUITETURA_X64 ? 62 : 183);
    escrever_u32(objeto + 20, 1);
    escrever_u64(objeto + 40, deslocamento_secoes);
    escrever_u16(objeto + 52, ELF_TAMANHO_CABECALHO);
    escrever_u16(objeto + 58, ELF_TAMANHO_SECAO);
    escrever_u16(objeto + 60, ELF_QUANTIDADE_SECOES);
    escrever_u16(objeto + 62, 5);
    memcpy(objeto + deslocamento_texto, codigo->bytes, codigo->tamanho);

    unsigned char *simbolo_secao = objeto + deslocamento_simbolos + ELF_TAMANHO_SIMBOLO;
    simbolo_secao[4] = 3; /* STB_LOCAL | STT_SECTION */
    escrever_u16(simbolo_secao + 6, 1);
    unsigned char *simbolo_funcao = simbolo_secao + ELF_TAMANHO_SIMBOLO;
    escrever_u32(simbolo_funcao, 1);
    simbolo_funcao[4] = 0x12; /* STB_GLOBAL | STT_FUNC */
    escrever_u16(simbolo_funcao + 6, 1);
    escrever_u64(simbolo_funcao + 16, codigo->tamanho);
    objeto[deslocamento_strings] = '\0';
    memcpy(objeto + deslocamento_strings + 1, nome_simbolo, tamanho_nome + 1u);
    for (size_t i = 0; i < quantidade_relocacoes; i++) {
        unsigned char *simbolo_externo =
            objeto + deslocamento_simbolos + ELF_TAMANHO_SIMBOLO * (3u + i);
        escrever_u32(simbolo_externo, nomes_externos[i]);
        simbolo_externo[4] = 0x10; /* STB_GLOBAL | STT_NOTYPE */
        const char *nome_externo = codigo->relocacoes[i].simbolo;
        memcpy(objeto + deslocamento_strings + nomes_externos[i], nome_externo,
               strlen(nome_externo) + 1u);

        unsigned char *relocacao = objeto + deslocamento_relocacoes + i * 24u;
        escrever_u64(relocacao, codigo->relocacoes[i].deslocamento);
        uint32_t tipo = codigo->arquitetura == SEF_ARQUITETURA_X64 ? 4u : 283u;
        escrever_u64(relocacao + 8, ((uint64_t)(3u + i) << 32u) | tipo);
        escrever_u64(relocacao + 16,
                     codigo->arquitetura == SEF_ARQUITETURA_X64 ? (uint64_t)-4 : 0u);
    }
    memcpy(objeto + deslocamento_nomes_secoes, nomes_secoes, tamanho_nomes_secoes);

    unsigned char *secoes = objeto + deslocamento_secoes;
    escrever_cabecalho_secao(secoes + ELF_TAMANHO_SECAO, 1, 1, 0x6, deslocamento_texto,
                             codigo->tamanho, 0, 0, 16, 0);
    escrever_cabecalho_secao(secoes + ELF_TAMANHO_SECAO * 2u, 7, 4, 0, deslocamento_relocacoes,
                             tamanho_relocacoes, 3, 1, 8, 24);
    escrever_cabecalho_secao(secoes + ELF_TAMANHO_SECAO * 3u, 18, 2, 0, deslocamento_simbolos,
                             tamanho_simbolos, 4, 2, 8, ELF_TAMANHO_SIMBOLO);
    escrever_cabecalho_secao(secoes + ELF_TAMANHO_SECAO * 4u, 26, 3, 0, deslocamento_strings,
                             tamanho_strings, 0, 0, 1, 0);
    escrever_cabecalho_secao(secoes + ELF_TAMANHO_SECAO * 5u, 34, 3, 0, deslocamento_nomes_secoes,
                             tamanho_nomes_secoes, 0, 0, 1, 0);

    size_t tamanho_caminho = strlen(caminho);
    char *temporario = malloc(tamanho_caminho + 5u);
    if (temporario == NULL) {
        free(nomes_externos);
        free(objeto);
        erro_definir(erro, "not enough memory for a temporary ELF path");
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
        if (arquivo == NULL)
            erro_definir(erro, "could not create ELF object");
        else
            erro_definir(erro, "failed to write or install ELF object");
    }
    free(temporario);
    free(nomes_externos);
    free(objeto);
    return sucesso;
}
