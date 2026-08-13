#include "sefirah/interno.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define SEF_IMAGEM_MAX_OBJETOS 10000000u
#define SEF_IMAGEM_MAX_TEXTO (64u * 1024u * 1024u)
#define SEF_ID_INVALIDO UINT32_MAX

static const unsigned char assinatura_v6[8] = {'S', 'E', 'F', 'I', 'M', 'G', 6, 0};
static const unsigned char assinatura_v7[8] = {'S', 'E', 'F', 'I', 'M', 'G', 7, 0};
static const unsigned char assinatura_v8[8] = {'S', 'E', 'F', 'I', 'M', 'G', 8, 0};
static const unsigned char assinatura_v9[8] = {'S', 'E', 'F', 'I', 'M', 'G', 9, 0};
static const unsigned char assinatura_v10[8] = {'S', 'E', 'F', 'I', 'M', 'G', 10, 0};

typedef struct RegistroImagem {
    SefTipo tipo;
    int64_t inteiro;
    uint32_t caractere;
    double real;
    char *texto;
    uint32_t tamanho_texto;
    uint32_t referencias[3];
    bool macro;
    uint32_t quantidade_vinculos;
    uint32_t *vinculos;
    uint32_t quantidade_funcoes;
    uint32_t *funcoes;
    uint32_t quantidade_simbolos_pacote;
    uint32_t *simbolos_pacote;
    uint32_t quantidade_usados;
    uint32_t *usados;
    uint32_t quantidade_exportados;
    uint32_t *exportados;
    uint32_t quantidade_itens_vetor;
    uint32_t *itens_vetor;
    uint32_t quantidade_itens_hash;
    uint32_t *itens_hash;
} RegistroImagem;

static bool escrever_bytes(FILE *arquivo, const void *dados, size_t tamanho, SefErro *erro) {
    if (fwrite(dados, 1, tamanho, arquivo) == tamanho)
        return true;
    sef_erro_definir(erro, 0, 0, "failed to write image: %s", strerror(errno));
    return false;
}

static bool escrever_u8(FILE *arquivo, uint8_t valor, SefErro *erro) {
    return escrever_bytes(arquivo, &valor, 1, erro);
}

static bool escrever_u32(FILE *arquivo, uint32_t valor, SefErro *erro) {
    unsigned char bytes[4] = {(unsigned char)valor, (unsigned char)(valor >> 8u),
                              (unsigned char)(valor >> 16u), (unsigned char)(valor >> 24u)};
    return escrever_bytes(arquivo, bytes, sizeof(bytes), erro);
}

static bool escrever_u64(FILE *arquivo, uint64_t valor, SefErro *erro) {
    unsigned char bytes[8];
    for (unsigned int i = 0; i < 8; i++)
        bytes[i] = (unsigned char)(valor >> (i * 8u));
    return escrever_bytes(arquivo, bytes, sizeof(bytes), erro);
}

static bool escrever_texto(FILE *arquivo, const char *texto, uint32_t tamanho, SefErro *erro) {
    return escrever_u32(arquivo, tamanho, erro) && escrever_bytes(arquivo, texto, tamanho, erro);
}

static bool ler_bytes(FILE *arquivo, void *dados, size_t tamanho, SefErro *erro) {
    if (fread(dados, 1, tamanho, arquivo) == tamanho)
        return true;
    sef_erro_definir(erro, 0, 0, "truncated or unreadable image");
    return false;
}

static bool ler_u8(FILE *arquivo, uint8_t *valor, SefErro *erro) {
    return ler_bytes(arquivo, valor, 1, erro);
}

static bool ler_u32(FILE *arquivo, uint32_t *valor, SefErro *erro) {
    unsigned char bytes[4];
    if (!ler_bytes(arquivo, bytes, sizeof(bytes), erro))
        return false;
    *valor = (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8u) | ((uint32_t)bytes[2] << 16u) |
             ((uint32_t)bytes[3] << 24u);
    return true;
}

static bool ler_u64(FILE *arquivo, uint64_t *valor, SefErro *erro) {
    unsigned char bytes[8];
    if (!ler_bytes(arquivo, bytes, sizeof(bytes), erro))
        return false;
    *valor = 0;
    for (unsigned int i = 0; i < 8; i++)
        *valor |= (uint64_t)bytes[i] << (i * 8u);
    return true;
}

static bool ler_texto(FILE *arquivo, char **texto, uint32_t *tamanho, SefErro *erro) {
    if (!ler_u32(arquivo, tamanho, erro))
        return false;
    if (*tamanho > SEF_IMAGEM_MAX_TEXTO) {
        sef_erro_definir(erro, 0, 0, "string exceeds the image limit");
        return false;
    }
    *texto = malloc((size_t)*tamanho + 1);
    if (*texto == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory to open image");
        return false;
    }
    if (!ler_bytes(arquivo, *texto, *tamanho, erro)) {
        free(*texto);
        *texto = NULL;
        return false;
    }
    (*texto)[*tamanho] = '\0';
    return true;
}

static uint32_t id_de(SefValor *objetos, uint32_t quantidade, SefValor procurado) {
    for (uint32_t i = 0; i < quantidade; i++) {
        if (objetos[i] == procurado)
            return i;
    }
    return SEF_ID_INVALIDO;
}

static bool escrever_referencia(FILE *arquivo, SefValor *objetos, uint32_t quantidade,
                                SefValor valor, SefErro *erro) {
    uint32_t id = id_de(objetos, quantidade, valor);
    if (id == SEF_ID_INVALIDO) {
        sef_erro_definir(erro, 0, 0, "image encountered a reference outside the heap");
        return false;
    }
    return escrever_u32(arquivo, id, erro);
}

static uint32_t contar_vinculos(SefVinculo *vinculo) {
    uint32_t quantidade = 0;
    while (vinculo != NULL) {
        quantidade++;
        vinculo = vinculo->proximo;
    }
    return quantidade;
}

static bool arquivo_substituir(const char *temporario, const char *destino) {
#ifdef _WIN32
    return MoveFileExA(temporario, destino, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) !=
           0;
#else
    return rename(temporario, destino) == 0;
#endif
}

static bool escrever_objeto(FILE *arquivo, SefValor objeto, SefValor *objetos, uint32_t quantidade,
                            SefErro *erro) {
    if (!escrever_u8(arquivo, (uint8_t)objeto->tipo, erro))
        return false;
    uint64_t bits;
    switch (objeto->tipo) {
    case SEF_TIPO_NULO:
        return true;
    case SEF_TIPO_INTEIRO:
        return escrever_u64(arquivo, (uint64_t)objeto->como.inteiro, erro);
    case SEF_TIPO_REAL:
        memcpy(&bits, &objeto->como.real, sizeof(bits));
        return escrever_u64(arquivo, bits, erro);
    case SEF_TIPO_TEXTO:
        if (objeto->como.texto.tamanho > UINT32_MAX) {
            sef_erro_definir(erro, 0, 0, "string is too large for image");
            return false;
        }
        return escrever_texto(arquivo, objeto->como.texto.dados,
                              (uint32_t)objeto->como.texto.tamanho, erro);
    case SEF_TIPO_SIMBOLO:
        if (objeto->como.simbolo.tamanho > UINT32_MAX) {
            sef_erro_definir(erro, 0, 0, "symbol name is too large for image");
            return false;
        }
        return escrever_texto(arquivo, objeto->como.simbolo.nome,
                              (uint32_t)objeto->como.simbolo.tamanho, erro) &&
               escrever_referencia(arquivo, objetos, quantidade, objeto->como.simbolo.pacote, erro);
    case SEF_TIPO_PAR:
        return escrever_referencia(arquivo, objetos, quantidade, objeto->como.par.primeiro, erro) &&
               escrever_referencia(arquivo, objetos, quantidade, objeto->como.par.resto, erro);
    case SEF_TIPO_NATIVA: {
        const char *nome = sef_primitiva_nome(objeto->como.nativa.funcao);
        if (nome == NULL)
            nome = objeto->como.nativa.nome;
        size_t tamanho = strlen(nome);
        return tamanho <= UINT32_MAX && escrever_texto(arquivo, nome, (uint32_t)tamanho, erro);
    }
    case SEF_TIPO_FUNCAO:
        return escrever_referencia(arquivo, objetos, quantidade, objeto->como.funcao.parametros,
                                   erro) &&
               escrever_referencia(arquivo, objetos, quantidade, objeto->como.funcao.corpo, erro) &&
               escrever_referencia(arquivo, objetos, quantidade, objeto->como.funcao.ambiente,
                                   erro) &&
               escrever_u8(arquivo, objeto->como.funcao.macro ? 1 : 0, erro);
    case SEF_TIPO_AMBIENTE: {
        if (!escrever_referencia(arquivo, objetos, quantidade, objeto->como.ambiente.pai, erro))
            return false;
        uint32_t total = contar_vinculos(objeto->como.ambiente.vinculos);
        if (!escrever_u32(arquivo, total, erro))
            return false;
        for (SefVinculo *vinculo = objeto->como.ambiente.vinculos; vinculo != NULL;
             vinculo = vinculo->proximo) {
            if (!escrever_referencia(arquivo, objetos, quantidade, vinculo->simbolo, erro) ||
                !escrever_referencia(arquivo, objetos, quantidade, vinculo->valor, erro))
                return false;
        }
        total = contar_vinculos(objeto->como.ambiente.funcoes);
        if (!escrever_u32(arquivo, total, erro))
            return false;
        for (SefVinculo *vinculo = objeto->como.ambiente.funcoes; vinculo != NULL;
             vinculo = vinculo->proximo) {
            if (!escrever_referencia(arquivo, objetos, quantidade, vinculo->simbolo, erro) ||
                !escrever_referencia(arquivo, objetos, quantidade, vinculo->valor, erro))
                return false;
        }
        return true;
    }
    case SEF_TIPO_CONDICAO:
        return escrever_referencia(arquivo, objetos, quantidade, objeto->como.condicao.classe,
                                   erro) &&
               escrever_referencia(arquivo, objetos, quantidade, objeto->como.condicao.mensagem,
                                   erro);
    case SEF_TIPO_PACOTE: {
        size_t tamanho = strlen(objeto->como.pacote.nome);
        if (tamanho > UINT32_MAX ||
            !escrever_texto(arquivo, objeto->como.pacote.nome, (uint32_t)tamanho, erro) ||
            !escrever_u32(arquivo, (uint32_t)objeto->como.pacote.quantidade_simbolos, erro))
            return false;
        for (size_t i = 0; i < objeto->como.pacote.quantidade_simbolos; i++) {
            if (!escrever_referencia(arquivo, objetos, quantidade, objeto->como.pacote.simbolos[i],
                                     erro))
                return false;
        }
        if (!escrever_u32(arquivo, (uint32_t)objeto->como.pacote.quantidade_usados, erro))
            return false;
        for (size_t i = 0; i < objeto->como.pacote.quantidade_usados; i++) {
            if (!escrever_referencia(arquivo, objetos, quantidade, objeto->como.pacote.usados[i],
                                     erro))
                return false;
        }
        if (!escrever_u32(arquivo, (uint32_t)objeto->como.pacote.quantidade_exportados, erro))
            return false;
        for (size_t i = 0; i < objeto->como.pacote.quantidade_exportados; i++) {
            if (!escrever_referencia(arquivo, objetos, quantidade,
                                     objeto->como.pacote.exportados[i], erro))
                return false;
        }
        return true;
    }
    case SEF_TIPO_STREAM:
        if (objeto->como.stream.padrao == 0 && !objeto->como.stream.fechado) {
            sef_erro_definir(erro, 0, 0, "close file streams before saving the image");
            return false;
        }
        return escrever_u8(arquivo, objeto->como.stream.padrao, erro) &&
               escrever_u8(arquivo, objeto->como.stream.fechado ? 1 : 0, erro);
    case SEF_TIPO_BIBLIOTECA:
        if (!objeto->como.biblioteca.fechada) {
            sef_erro_definir(erro, 0, 0, "close shared libraries before saving the image");
            return false;
        }
        return true;
    case SEF_TIPO_VETOR:
        if (objeto->como.vetor.tamanho > UINT32_MAX) {
            sef_erro_definir(erro, 0, 0, "vector is too large for image");
            return false;
        }
        if (!escrever_u32(arquivo, (uint32_t)objeto->como.vetor.tamanho, erro))
            return false;
        for (size_t i = 0; i < objeto->como.vetor.tamanho; i++) {
            if (!escrever_referencia(arquivo, objetos, quantidade, objeto->como.vetor.itens[i],
                                     erro))
                return false;
        }
        return true;
    case SEF_TIPO_CARACTERE:
        return escrever_u32(arquivo, objeto->como.caractere, erro);
    case SEF_TIPO_TABELA_HASH:
        if (objeto->como.tabela_hash.quantidade > UINT32_MAX ||
            !escrever_u32(arquivo, (uint32_t)objeto->como.tabela_hash.quantidade, erro))
            return false;
        for (size_t i = 0; i < objeto->como.tabela_hash.capacidade; i++) {
            SefEntradaHash *entrada = &objeto->como.tabela_hash.entradas[i];
            if (entrada->estado == SEF_ENTRADA_HASH_OCUPADA &&
                (!escrever_referencia(arquivo, objetos, quantidade, entrada->chave, erro) ||
                 !escrever_referencia(arquivo, objetos, quantidade, entrada->valor, erro)))
                return false;
        }
        return true;
    case SEF_TIPO_REINICIO:
        return escrever_referencia(arquivo, objetos, quantidade, objeto->como.reinicio.nome, erro);
    }
    return false;
}

bool sef_runtime_imagem_salvar(SefRuntime *runtime, const char *caminho, SefErro *erro) {
    sef_erro_limpar(erro);
    if (runtime == NULL || caminho == NULL || caminho[0] == '\0') {
        sef_erro_definir(erro, 0, 0, "missing runtime or image path");
        return false;
    }
    sef_runtime_coletar(runtime, runtime->nulo);
    if (runtime->quantidade_objetos > UINT32_MAX) {
        sef_erro_definir(erro, 0, 0, "heap is too large for the v1 image format");
        return false;
    }
    uint32_t quantidade = (uint32_t)runtime->quantidade_objetos;
    SefValor *objetos = malloc((size_t)quantidade * sizeof(*objetos));
    if (objetos == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory to index image");
        return false;
    }
    uint32_t indice = 0;
    for (SefValor objeto = runtime->objetos; objeto != NULL; objeto = objeto->proximo_alocado)
        objetos[indice++] = objeto;

    size_t tamanho_caminho = strlen(caminho);
    char *temporario = malloc(tamanho_caminho + 6);
    if (temporario == NULL) {
        free(objetos);
        sef_erro_definir(erro, 0, 0, "not enough memory for temporary path");
        return false;
    }
    snprintf(temporario, tamanho_caminho + 6, "%s.tmp", caminho);
    FILE *arquivo = fopen(temporario, "wb");
    if (arquivo == NULL) {
        sef_erro_definir(erro, 0, 0, "could not create '%s': %s", temporario, strerror(errno));
        free(temporario);
        free(objetos);
        return false;
    }

    bool sucesso =
        escrever_bytes(arquivo, assinatura_v10, sizeof(assinatura_v10), erro) &&
        escrever_u32(arquivo, quantidade, erro) &&
        escrever_u32(arquivo, id_de(objetos, quantidade, runtime->nulo), erro) &&
        escrever_u32(arquivo, id_de(objetos, quantidade, runtime->verdadeiro), erro) &&
        escrever_u32(arquivo, id_de(objetos, quantidade, runtime->ambiente_global), erro) &&
        escrever_u32(arquivo, (uint32_t)runtime->quantidade_simbolos, erro);
    for (size_t i = 0; sucesso && i < runtime->quantidade_simbolos; i++)
        sucesso = escrever_referencia(arquivo, objetos, quantidade, runtime->simbolos[i], erro);
    if (sucesso)
        sucesso =
            escrever_referencia(arquivo, objetos, quantidade, runtime->pacote_atual, erro) &&
            escrever_referencia(arquivo, objetos, quantidade, runtime->pacote_common_lisp, erro) &&
            escrever_referencia(arquivo, objetos, quantidade, runtime->pacote_keyword, erro) &&
            escrever_u32(arquivo, (uint32_t)runtime->quantidade_pacotes, erro);
    for (size_t i = 0; sucesso && i < runtime->quantidade_pacotes; i++)
        sucesso = escrever_referencia(arquivo, objetos, quantidade, runtime->pacotes[i], erro);
    if (sucesso)
        sucesso =
            escrever_referencia(arquivo, objetos, quantidade, runtime->entrada_padrao, erro) &&
            escrever_referencia(arquivo, objetos, quantidade, runtime->saida_padrao, erro) &&
            escrever_referencia(arquivo, objetos, quantidade, runtime->erro_padrao, erro);
    for (uint32_t i = 0; sucesso && i < quantidade; i++)
        sucesso = escrever_objeto(arquivo, objetos[i], objetos, quantidade, erro);
    if (sucesso && fflush(arquivo) != 0) {
        sef_erro_definir(erro, 0, 0, "failed to synchronize image: %s", strerror(errno));
        sucesso = false;
    }
    if (fclose(arquivo) != 0 && sucesso) {
        sef_erro_definir(erro, 0, 0, "failed to close image: %s", strerror(errno));
        sucesso = false;
    }
    if (sucesso) {
        if (!arquivo_substituir(temporario, caminho)) {
            sef_erro_definir(erro, 0, 0, "could not install image: %s", strerror(errno));
            sucesso = false;
        }
    }
    if (!sucesso)
        remove(temporario);
    free(temporario);
    free(objetos);
    return sucesso;
}

static void registros_liberar(RegistroImagem *registros, uint32_t quantidade) {
    if (registros == NULL)
        return;
    for (uint32_t i = 0; i < quantidade; i++) {
        free(registros[i].texto);
        free(registros[i].vinculos);
        free(registros[i].funcoes);
        free(registros[i].simbolos_pacote);
        free(registros[i].usados);
        free(registros[i].exportados);
        free(registros[i].itens_vetor);
        free(registros[i].itens_hash);
    }
    free(registros);
}

static bool ler_registro(FILE *arquivo, RegistroImagem *registro, unsigned int versao,
                         SefErro *erro) {
    uint8_t tipo;
    uint64_t bits;
    SefTipo maior_tipo = versao >= 10  ? SEF_TIPO_REINICIO
                         : versao >= 9 ? SEF_TIPO_TABELA_HASH
                         : versao >= 8 ? SEF_TIPO_CARACTERE
                         : versao >= 7 ? SEF_TIPO_VETOR
                                       : SEF_TIPO_BIBLIOTECA;
    if (!ler_u8(arquivo, &tipo, erro) || tipo > maior_tipo) {
        if (!erro->ocorreu)
            sef_erro_definir(erro, 0, 0, "invalid object type in image");
        return false;
    }
    registro->tipo = (SefTipo)tipo;
    switch (registro->tipo) {
    case SEF_TIPO_NULO:
        return true;
    case SEF_TIPO_INTEIRO:
        if (!ler_u64(arquivo, &bits, erro))
            return false;
        registro->inteiro = (int64_t)bits;
        return true;
    case SEF_TIPO_REAL:
        if (!ler_u64(arquivo, &bits, erro))
            return false;
        memcpy(&registro->real, &bits, sizeof(bits));
        return true;
    case SEF_TIPO_TEXTO:
    case SEF_TIPO_NATIVA:
        return ler_texto(arquivo, &registro->texto, &registro->tamanho_texto, erro);
    case SEF_TIPO_SIMBOLO:
        return ler_texto(arquivo, &registro->texto, &registro->tamanho_texto, erro) &&
               ler_u32(arquivo, &registro->referencias[0], erro);
    case SEF_TIPO_PAR:
        return ler_u32(arquivo, &registro->referencias[0], erro) &&
               ler_u32(arquivo, &registro->referencias[1], erro);
    case SEF_TIPO_FUNCAO: {
        uint8_t macro;
        if (!ler_u32(arquivo, &registro->referencias[0], erro) ||
            !ler_u32(arquivo, &registro->referencias[1], erro) ||
            !ler_u32(arquivo, &registro->referencias[2], erro) || !ler_u8(arquivo, &macro, erro))
            return false;
        registro->macro = macro != 0;
        return true;
    }
    case SEF_TIPO_AMBIENTE:
        if (!ler_u32(arquivo, &registro->referencias[0], erro) ||
            !ler_u32(arquivo, &registro->quantidade_vinculos, erro))
            return false;
        if (registro->quantidade_vinculos > SEF_IMAGEM_MAX_OBJETOS) {
            sef_erro_definir(erro, 0, 0, "too many bindings in image");
            return false;
        }
        registro->vinculos = malloc((size_t)registro->quantidade_vinculos * 2u * sizeof(uint32_t));
        if (registro->vinculos == NULL && registro->quantidade_vinculos > 0) {
            sef_erro_definir(erro, 0, 0, "not enough memory for image bindings");
            return false;
        }
        for (uint32_t i = 0; i < registro->quantidade_vinculos * 2u; i++) {
            if (!ler_u32(arquivo, &registro->vinculos[i], erro))
                return false;
        }
        if (!ler_u32(arquivo, &registro->quantidade_funcoes, erro))
            return false;
        if (registro->quantidade_funcoes > SEF_IMAGEM_MAX_OBJETOS) {
            sef_erro_definir(erro, 0, 0, "too many functions in image");
            return false;
        }
        registro->funcoes = malloc((size_t)registro->quantidade_funcoes * 2u * sizeof(uint32_t));
        if (registro->funcoes == NULL && registro->quantidade_funcoes > 0) {
            sef_erro_definir(erro, 0, 0, "not enough memory for image functions");
            return false;
        }
        for (uint32_t i = 0; i < registro->quantidade_funcoes * 2u; i++) {
            if (!ler_u32(arquivo, &registro->funcoes[i], erro))
                return false;
        }
        return true;
    case SEF_TIPO_CONDICAO:
        return ler_u32(arquivo, &registro->referencias[0], erro) &&
               ler_u32(arquivo, &registro->referencias[1], erro);
    case SEF_TIPO_PACOTE:
        if (!ler_texto(arquivo, &registro->texto, &registro->tamanho_texto, erro) ||
            !ler_u32(arquivo, &registro->quantidade_simbolos_pacote, erro) ||
            registro->quantidade_simbolos_pacote > SEF_IMAGEM_MAX_OBJETOS)
            return false;
        registro->simbolos_pacote =
            malloc((size_t)registro->quantidade_simbolos_pacote * sizeof(uint32_t));
        if (registro->simbolos_pacote == NULL && registro->quantidade_simbolos_pacote > 0) {
            sef_erro_definir(erro, 0, 0, "not enough memory for package symbols");
            return false;
        }
        for (uint32_t i = 0; i < registro->quantidade_simbolos_pacote; i++) {
            if (!ler_u32(arquivo, &registro->simbolos_pacote[i], erro))
                return false;
        }
        if (!ler_u32(arquivo, &registro->quantidade_usados, erro) ||
            registro->quantidade_usados > SEF_IMAGEM_MAX_OBJETOS)
            return false;
        registro->usados = malloc((size_t)registro->quantidade_usados * sizeof(uint32_t));
        if (registro->usados == NULL && registro->quantidade_usados > 0) {
            sef_erro_definir(erro, 0, 0, "not enough memory for used packages");
            return false;
        }
        for (uint32_t i = 0; i < registro->quantidade_usados; i++) {
            if (!ler_u32(arquivo, &registro->usados[i], erro))
                return false;
        }
        if (!ler_u32(arquivo, &registro->quantidade_exportados, erro) ||
            registro->quantidade_exportados > SEF_IMAGEM_MAX_OBJETOS)
            return false;
        registro->exportados = malloc((size_t)registro->quantidade_exportados * sizeof(uint32_t));
        if (registro->exportados == NULL && registro->quantidade_exportados > 0) {
            sef_erro_definir(erro, 0, 0, "not enough memory for exported symbols");
            return false;
        }
        for (uint32_t i = 0; i < registro->quantidade_exportados; i++) {
            if (!ler_u32(arquivo, &registro->exportados[i], erro))
                return false;
        }
        return true;
    case SEF_TIPO_STREAM: {
        uint8_t padrao, fechado;
        if (!ler_u8(arquivo, &padrao, erro) || !ler_u8(arquivo, &fechado, erro) || padrao > 3)
            return false;
        registro->inteiro = padrao;
        registro->macro = fechado != 0;
        return true;
    }
    case SEF_TIPO_BIBLIOTECA:
        return true;
    case SEF_TIPO_VETOR:
        if (!ler_u32(arquivo, &registro->quantidade_itens_vetor, erro) ||
            registro->quantidade_itens_vetor > SEF_IMAGEM_MAX_OBJETOS)
            return false;
        registro->itens_vetor = malloc((size_t)registro->quantidade_itens_vetor * sizeof(uint32_t));
        if (registro->itens_vetor == NULL && registro->quantidade_itens_vetor > 0) {
            sef_erro_definir(erro, 0, 0, "not enough memory for image vector");
            return false;
        }
        for (uint32_t i = 0; i < registro->quantidade_itens_vetor; i++) {
            if (!ler_u32(arquivo, &registro->itens_vetor[i], erro))
                return false;
        }
        return true;
    case SEF_TIPO_CARACTERE:
        return ler_u32(arquivo, &registro->caractere, erro);
    case SEF_TIPO_TABELA_HASH:
        if (!ler_u32(arquivo, &registro->quantidade_itens_hash, erro))
            return false;
        if (registro->quantidade_itens_hash > SEF_IMAGEM_MAX_OBJETOS) {
            sef_erro_definir(erro, 0, 0, "too many items in image hash table");
            return false;
        }
        registro->itens_hash =
            malloc((size_t)registro->quantidade_itens_hash * 2u * sizeof(uint32_t));
        if (registro->itens_hash == NULL && registro->quantidade_itens_hash > 0) {
            sef_erro_definir(erro, 0, 0, "not enough memory for image hash table");
            return false;
        }
        for (uint32_t i = 0; i < registro->quantidade_itens_hash * 2u; i++) {
            if (!ler_u32(arquivo, &registro->itens_hash[i], erro))
                return false;
        }
        return true;
    case SEF_TIPO_REINICIO:
        return ler_u32(arquivo, &registro->referencias[0], erro);
    }
    return false;
}

static bool id_valido(uint32_t id, uint32_t quantidade) { return id < quantidade; }

static bool validar_tabelas_de_pacotes(const SefRuntime *runtime, SefErro *erro) {
    for (size_t i = 0; i < runtime->quantidade_pacotes; i++) {
        SefValor pacote = runtime->pacotes[i];
        for (size_t j = 0; j < pacote->como.pacote.quantidade_simbolos; j++) {
            SefValor simbolo = pacote->como.pacote.simbolos[j];
            if (simbolo != runtime->nulo && simbolo->tipo != SEF_TIPO_SIMBOLO) {
                sef_erro_definir(erro, 0, 0, "corrupt package symbol table");
                return false;
            }
        }
        for (size_t j = 0; j < pacote->como.pacote.quantidade_exportados; j++) {
            SefValor simbolo = pacote->como.pacote.exportados[j];
            if (simbolo != runtime->nulo && simbolo->tipo != SEF_TIPO_SIMBOLO) {
                sef_erro_definir(erro, 0, 0, "corrupt package export table");
                return false;
            }
        }
        for (size_t j = 0; j < pacote->como.pacote.quantidade_usados; j++) {
            if (pacote->como.pacote.usados[j]->tipo != SEF_TIPO_PACOTE) {
                sef_erro_definir(erro, 0, 0, "corrupt package use list");
                return false;
            }
        }
    }
    return true;
}

static bool validar_registro(const RegistroImagem *registro, uint32_t quantidade, SefErro *erro) {
    if (registro->tipo == SEF_TIPO_CARACTERE) {
        uint32_t codigo = registro->caractere;
        if (codigo > 0x10ffffu || (codigo >= 0xd800u && codigo <= 0xdfffu)) {
            sef_erro_definir(erro, 0, 0, "invalid character in image");
            return false;
        }
    }
    uint32_t referencias = 0;
    if (registro->tipo == SEF_TIPO_PAR || registro->tipo == SEF_TIPO_CONDICAO)
        referencias = 2;
    else if (registro->tipo == SEF_TIPO_FUNCAO)
        referencias = 3;
    else if (registro->tipo == SEF_TIPO_AMBIENTE)
        referencias = 1;
    else if (registro->tipo == SEF_TIPO_SIMBOLO || registro->tipo == SEF_TIPO_REINICIO)
        referencias = 1;
    for (uint32_t i = 0; i < referencias; i++) {
        if (!id_valido(registro->referencias[i], quantidade)) {
            sef_erro_definir(erro, 0, 0, "invalid reference in image");
            return false;
        }
    }
    for (uint32_t i = 0; i < registro->quantidade_vinculos * 2u; i++) {
        if (!id_valido(registro->vinculos[i], quantidade)) {
            sef_erro_definir(erro, 0, 0, "invalid binding in image");
            return false;
        }
    }
    for (uint32_t i = 0; i < registro->quantidade_funcoes * 2u; i++) {
        if (!id_valido(registro->funcoes[i], quantidade)) {
            sef_erro_definir(erro, 0, 0, "invalid function in image");
            return false;
        }
    }
    for (uint32_t i = 0; i < registro->quantidade_simbolos_pacote; i++) {
        if (!id_valido(registro->simbolos_pacote[i], quantidade)) {
            sef_erro_definir(erro, 0, 0, "invalid package symbol in image");
            return false;
        }
    }
    for (uint32_t i = 0; i < registro->quantidade_usados; i++) {
        if (!id_valido(registro->usados[i], quantidade)) {
            sef_erro_definir(erro, 0, 0, "invalid used package in image");
            return false;
        }
    }
    for (uint32_t i = 0; i < registro->quantidade_exportados; i++) {
        if (!id_valido(registro->exportados[i], quantidade)) {
            sef_erro_definir(erro, 0, 0, "invalid exported symbol in image");
            return false;
        }
    }
    for (uint32_t i = 0; i < registro->quantidade_itens_hash * 2u; i++) {
        if (!id_valido(registro->itens_hash[i], quantidade)) {
            sef_erro_definir(erro, 0, 0, "invalid hash-table item in image");
            return false;
        }
    }
    for (uint32_t i = 0; i < registro->quantidade_itens_vetor; i++) {
        if (!id_valido(registro->itens_vetor[i], quantidade)) {
            sef_erro_definir(erro, 0, 0, "invalid vector item in image");
            return false;
        }
    }
    return true;
}

SefRuntime *sef_runtime_imagem_abrir(const char *caminho, SefErro *erro) {
    sef_erro_limpar(erro);
    FILE *arquivo = fopen(caminho, "rb");
    if (arquivo == NULL) {
        sef_erro_definir(erro, 0, 0, "could not open image '%s': %s", caminho, strerror(errno));
        return NULL;
    }
    unsigned char recebida[8];
    uint32_t quantidade = 0, id_nulo = 0, id_verdadeiro = 0, id_global = 0, total_simbolos = 0;
    uint32_t id_pacote_atual = 0, id_common_lisp = 0, id_keyword = 0, total_pacotes = 0;
    uint32_t id_entrada_padrao = 0, id_saida_padrao = 0, id_erro_padrao = 0;
    bool cabecalho_lido = ler_bytes(arquivo, recebida, sizeof(recebida), erro);
    unsigned int versao = 0;
    if (cabecalho_lido && memcmp(recebida, assinatura_v6, sizeof(assinatura_v6)) == 0)
        versao = 6;
    else if (cabecalho_lido && memcmp(recebida, assinatura_v7, sizeof(assinatura_v7)) == 0)
        versao = 7;
    else if (cabecalho_lido && memcmp(recebida, assinatura_v8, sizeof(assinatura_v8)) == 0)
        versao = 8;
    else if (cabecalho_lido && memcmp(recebida, assinatura_v9, sizeof(assinatura_v9)) == 0)
        versao = 9;
    else if (cabecalho_lido && memcmp(recebida, assinatura_v10, sizeof(assinatura_v10)) == 0)
        versao = 10;
    bool sucesso = cabecalho_lido && versao != 0 && ler_u32(arquivo, &quantidade, erro) &&
                   ler_u32(arquivo, &id_nulo, erro) && ler_u32(arquivo, &id_verdadeiro, erro) &&
                   ler_u32(arquivo, &id_global, erro) && ler_u32(arquivo, &total_simbolos, erro);
    if (sucesso && (quantidade == 0 || quantidade > SEF_IMAGEM_MAX_OBJETOS ||
                    total_simbolos > quantidade || !id_valido(id_nulo, quantidade) ||
                    !id_valido(id_verdadeiro, quantidade) || !id_valido(id_global, quantidade))) {
        sef_erro_definir(erro, 0, 0, "image header is invalid");
        sucesso = false;
    }
    if (!sucesso && !erro->ocorreu)
        sef_erro_definir(erro, 0, 0, "invalid image signature or version");

    uint32_t *ids_simbolos = sucesso ? malloc((size_t)total_simbolos * sizeof(uint32_t)) : NULL;
    RegistroImagem *registros = sucesso ? calloc(quantidade, sizeof(*registros)) : NULL;
    if (sucesso && ((total_simbolos > 0 && ids_simbolos == NULL) || registros == NULL)) {
        sef_erro_definir(erro, 0, 0, "not enough memory to open image");
        sucesso = false;
    }
    for (uint32_t i = 0; sucesso && i < total_simbolos; i++) {
        sucesso =
            ler_u32(arquivo, &ids_simbolos[i], erro) && id_valido(ids_simbolos[i], quantidade);
        if (!sucesso && !erro->ocorreu)
            sef_erro_definir(erro, 0, 0, "invalid symbol in image");
    }
    if (sucesso)
        sucesso = ler_u32(arquivo, &id_pacote_atual, erro) &&
                  ler_u32(arquivo, &id_common_lisp, erro) && ler_u32(arquivo, &id_keyword, erro) &&
                  ler_u32(arquivo, &total_pacotes, erro) && total_pacotes <= quantidade &&
                  id_valido(id_pacote_atual, quantidade) && id_valido(id_common_lisp, quantidade) &&
                  id_valido(id_keyword, quantidade);
    if (!sucesso && !erro->ocorreu)
        sef_erro_definir(erro, 0, 0, "invalid package header in image");
    uint32_t *ids_pacotes = sucesso ? malloc((size_t)total_pacotes * sizeof(uint32_t)) : NULL;
    if (sucesso && total_pacotes > 0 && ids_pacotes == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for package table");
        sucesso = false;
    }
    for (uint32_t i = 0; sucesso && i < total_pacotes; i++) {
        sucesso = ler_u32(arquivo, &ids_pacotes[i], erro) && id_valido(ids_pacotes[i], quantidade);
    }
    if (sucesso)
        sucesso = ler_u32(arquivo, &id_entrada_padrao, erro) &&
                  ler_u32(arquivo, &id_saida_padrao, erro) &&
                  ler_u32(arquivo, &id_erro_padrao, erro) &&
                  id_valido(id_entrada_padrao, quantidade) &&
                  id_valido(id_saida_padrao, quantidade) && id_valido(id_erro_padrao, quantidade);
    if (!sucesso && !erro->ocorreu)
        sef_erro_definir(erro, 0, 0, "invalid standard streams in image");
    for (uint32_t i = 0; sucesso && i < quantidade; i++)
        sucesso = ler_registro(arquivo, &registros[i], versao, erro) &&
                  validar_registro(&registros[i], quantidade, erro);
    fclose(arquivo);

    SefRuntime *runtime = sucesso ? calloc(1, sizeof(*runtime)) : NULL;
    SefValor *objetos = sucesso ? calloc(quantidade, sizeof(*objetos)) : NULL;
    if (sucesso && (runtime == NULL || objetos == NULL)) {
        sef_erro_definir(erro, 0, 0, "not enough memory to reconstruct image");
        sucesso = false;
    }
    for (uint32_t i = 0; sucesso && i < quantidade; i++) {
        objetos[i] = sef_objeto_novo(runtime, registros[i].tipo, erro);
        sucesso = objetos[i] != NULL;
    }
    for (uint32_t i = 0; sucesso && i < quantidade; i++) {
        SefValor objeto = objetos[i];
        RegistroImagem *registro = &registros[i];
        switch (registro->tipo) {
        case SEF_TIPO_NULO:
            break;
        case SEF_TIPO_INTEIRO:
            objeto->como.inteiro = registro->inteiro;
            break;
        case SEF_TIPO_REAL:
            objeto->como.real = registro->real;
            break;
        case SEF_TIPO_TEXTO:
            objeto->como.texto.dados = registro->texto;
            objeto->como.texto.tamanho = registro->tamanho_texto;
            runtime->bytes_aproximados += registro->tamanho_texto + 1u;
            registro->texto = NULL;
            break;
        case SEF_TIPO_SIMBOLO:
            objeto->como.simbolo.nome = registro->texto;
            objeto->como.simbolo.tamanho = registro->tamanho_texto;
            objeto->como.simbolo.pacote = objetos[registro->referencias[0]];
            runtime->bytes_aproximados += registro->tamanho_texto + 1u;
            registro->texto = NULL;
            if (objeto->como.simbolo.pacote->tipo != SEF_TIPO_PACOTE) {
                sef_erro_definir(erro, 0, 0, "symbol points to an invalid package");
                sucesso = false;
            }
            break;
        case SEF_TIPO_PAR:
            objeto->como.par.primeiro = objetos[registro->referencias[0]];
            objeto->como.par.resto = objetos[registro->referencias[1]];
            break;
        case SEF_TIPO_NATIVA: {
            SefFuncaoNativa funcao = sef_primitiva_buscar(registro->texto);
            if (funcao == NULL) {
                sef_erro_definir(erro, 0, 0, "primitive '%s' does not exist in this version",
                                 registro->texto);
                sucesso = false;
                break;
            }
            objeto->como.nativa.funcao = funcao;
            objeto->como.nativa.nome = sef_primitiva_nome(funcao);
            break;
        }
        case SEF_TIPO_FUNCAO:
            objeto->como.funcao.parametros = objetos[registro->referencias[0]];
            objeto->como.funcao.corpo = objetos[registro->referencias[1]];
            objeto->como.funcao.ambiente = objetos[registro->referencias[2]];
            objeto->como.funcao.macro = registro->macro;
            break;
        case SEF_TIPO_AMBIENTE:
            objeto->como.ambiente.pai = objetos[registro->referencias[0]];
            for (uint32_t j = 0; sucesso && j < registro->quantidade_vinculos; j++) {
                SefVinculo *vinculo = malloc(sizeof(*vinculo));
                if (vinculo == NULL) {
                    sef_erro_definir(erro, 0, 0, "not enough memory for image binding");
                    sucesso = false;
                    break;
                }
                vinculo->simbolo = objetos[registro->vinculos[j * 2u]];
                vinculo->valor = objetos[registro->vinculos[j * 2u + 1u]];
                vinculo->proximo = objeto->como.ambiente.vinculos;
                objeto->como.ambiente.vinculos = vinculo;
            }
            for (uint32_t j = 0; sucesso && j < registro->quantidade_funcoes; j++) {
                SefVinculo *vinculo = malloc(sizeof(*vinculo));
                if (vinculo == NULL) {
                    sef_erro_definir(erro, 0, 0, "not enough memory for image function");
                    sucesso = false;
                    break;
                }
                vinculo->simbolo = objetos[registro->funcoes[j * 2u]];
                vinculo->valor = objetos[registro->funcoes[j * 2u + 1u]];
                vinculo->proximo = objeto->como.ambiente.funcoes;
                objeto->como.ambiente.funcoes = vinculo;
            }
            break;
        case SEF_TIPO_CONDICAO:
            objeto->como.condicao.classe = objetos[registro->referencias[0]];
            objeto->como.condicao.mensagem = objetos[registro->referencias[1]];
            if (objeto->como.condicao.classe->tipo != SEF_TIPO_SIMBOLO ||
                objeto->como.condicao.mensagem->tipo != SEF_TIPO_TEXTO) {
                sef_erro_definir(erro, 0, 0, "corrupt condition in image");
                sucesso = false;
            }
            break;
        case SEF_TIPO_PACOTE:
            objeto->como.pacote.nome = registro->texto;
            registro->texto = NULL;
            objeto->como.pacote.quantidade_simbolos = registro->quantidade_simbolos_pacote;
            objeto->como.pacote.capacidade_simbolos = registro->quantidade_simbolos_pacote;
            if (registro->quantidade_simbolos_pacote > 0) {
                objeto->como.pacote.simbolos =
                    malloc((size_t)registro->quantidade_simbolos_pacote * sizeof(SefValor));
                if (objeto->como.pacote.simbolos == NULL) {
                    sef_erro_definir(erro, 0, 0, "not enough memory for package symbols");
                    sucesso = false;
                    break;
                }
                for (uint32_t j = 0; j < registro->quantidade_simbolos_pacote; j++)
                    objeto->como.pacote.simbolos[j] = objetos[registro->simbolos_pacote[j]];
            }
            objeto->como.pacote.quantidade_usados = registro->quantidade_usados;
            objeto->como.pacote.capacidade_usados = registro->quantidade_usados;
            if (registro->quantidade_usados > 0) {
                objeto->como.pacote.usados =
                    malloc((size_t)registro->quantidade_usados * sizeof(SefValor));
                if (objeto->como.pacote.usados == NULL) {
                    sef_erro_definir(erro, 0, 0, "not enough memory for package uses");
                    sucesso = false;
                    break;
                }
                for (uint32_t j = 0; j < registro->quantidade_usados; j++)
                    objeto->como.pacote.usados[j] = objetos[registro->usados[j]];
            }
            objeto->como.pacote.quantidade_exportados = registro->quantidade_exportados;
            objeto->como.pacote.capacidade_exportados = registro->quantidade_exportados;
            if (registro->quantidade_exportados > 0) {
                objeto->como.pacote.exportados =
                    malloc((size_t)registro->quantidade_exportados * sizeof(SefValor));
                if (objeto->como.pacote.exportados == NULL) {
                    sef_erro_definir(erro, 0, 0, "not enough memory for package exports");
                    sucesso = false;
                    break;
                }
                for (uint32_t j = 0; j < registro->quantidade_exportados; j++)
                    objeto->como.pacote.exportados[j] = objetos[registro->exportados[j]];
            }
            break;
        case SEF_TIPO_STREAM:
            objeto->como.stream.arquivo = NULL;
            objeto->como.stream.caminho = NULL;
            objeto->como.stream.possui_arquivo = false;
            objeto->como.stream.fechado = registro->macro;
            objeto->como.stream.padrao = (unsigned char)registro->inteiro;
            if (objeto->como.stream.padrao == 1)
                objeto->como.stream.arquivo = stdin;
            else if (objeto->como.stream.padrao == 2)
                objeto->como.stream.arquivo = stdout;
            else if (objeto->como.stream.padrao == 3)
                objeto->como.stream.arquivo = stderr;
            break;
        case SEF_TIPO_BIBLIOTECA:
            objeto->como.biblioteca.recurso = NULL;
            objeto->como.biblioteca.fechada = true;
            break;
        case SEF_TIPO_VETOR:
            objeto->como.vetor.tamanho = registro->quantidade_itens_vetor;
            if (registro->quantidade_itens_vetor > 0) {
                objeto->como.vetor.itens =
                    malloc((size_t)registro->quantidade_itens_vetor * sizeof(SefValor));
                if (objeto->como.vetor.itens == NULL) {
                    sef_erro_definir(erro, 0, 0, "not enough memory for image vector");
                    sucesso = false;
                    break;
                }
                for (uint32_t j = 0; j < registro->quantidade_itens_vetor; j++)
                    objeto->como.vetor.itens[j] = objetos[registro->itens_vetor[j]];
                runtime->bytes_aproximados +=
                    (size_t)registro->quantidade_itens_vetor * sizeof(SefValor);
            }
            break;
        case SEF_TIPO_CARACTERE:
            objeto->como.caractere = registro->caractere;
            break;
        case SEF_TIPO_TABELA_HASH:
            if (!sef_tabela_hash_inicializar(runtime, objeto, erro)) {
                sucesso = false;
                break;
            }
            for (uint32_t j = 0; j < registro->quantidade_itens_hash; j++) {
                SefValor chave = objetos[registro->itens_hash[j * 2u]];
                SefValor valor = objetos[registro->itens_hash[j * 2u + 1u]];
                if (!sef_tabela_hash_definir(runtime, objeto, chave, valor, erro)) {
                    sucesso = false;
                    break;
                }
            }
            break;
        case SEF_TIPO_REINICIO:
            objeto->como.reinicio.nome = objetos[registro->referencias[0]];
            if (objeto->como.reinicio.nome != objetos[id_nulo] &&
                objeto->como.reinicio.nome->tipo != SEF_TIPO_SIMBOLO) {
                sef_erro_definir(erro, 0, 0, "corrupt restart in image");
                sucesso = false;
            }
            break;
        }
    }

    if (sucesso) {
        runtime->nulo = objetos[id_nulo];
        runtime->verdadeiro = objetos[id_verdadeiro];
        runtime->ambiente_global = objetos[id_global];
        runtime->pacote_atual = objetos[id_pacote_atual];
        runtime->pacote_common_lisp = objetos[id_common_lisp];
        runtime->pacote_keyword = objetos[id_keyword];
        runtime->entrada_padrao = objetos[id_entrada_padrao];
        runtime->saida_padrao = objetos[id_saida_padrao];
        runtime->erro_padrao = objetos[id_erro_padrao];
        if (runtime->nulo->tipo != SEF_TIPO_NULO || runtime->verdadeiro->tipo != SEF_TIPO_SIMBOLO ||
            runtime->ambiente_global->tipo != SEF_TIPO_AMBIENTE ||
            runtime->pacote_atual->tipo != SEF_TIPO_PACOTE ||
            runtime->pacote_common_lisp->tipo != SEF_TIPO_PACOTE ||
            runtime->pacote_keyword->tipo != SEF_TIPO_PACOTE ||
            runtime->entrada_padrao->tipo != SEF_TIPO_STREAM ||
            runtime->saida_padrao->tipo != SEF_TIPO_STREAM ||
            runtime->erro_padrao->tipo != SEF_TIPO_STREAM) {
            sef_erro_definir(erro, 0, 0, "image roots have invalid types");
            sucesso = false;
        }
    }
    if (sucesso && total_pacotes > 0) {
        runtime->pacotes = malloc((size_t)total_pacotes * sizeof(*runtime->pacotes));
        if (runtime->pacotes == NULL) {
            sef_erro_definir(erro, 0, 0, "not enough memory for package table");
            sucesso = false;
        } else {
            runtime->quantidade_pacotes = total_pacotes;
            runtime->capacidade_pacotes = total_pacotes;
            for (uint32_t i = 0; i < total_pacotes; i++) {
                runtime->pacotes[i] = objetos[ids_pacotes[i]];
                if (runtime->pacotes[i]->tipo != SEF_TIPO_PACOTE) {
                    sef_erro_definir(erro, 0, 0, "corrupt package table");
                    sucesso = false;
                    break;
                }
            }
        }
    }
    if (sucesso && total_simbolos > 0) {
        runtime->simbolos = malloc((size_t)total_simbolos * sizeof(*runtime->simbolos));
        if (runtime->simbolos == NULL) {
            sef_erro_definir(erro, 0, 0, "not enough memory for symbol table");
            sucesso = false;
        } else {
            runtime->quantidade_simbolos = total_simbolos;
            runtime->capacidade_simbolos = total_simbolos;
            for (uint32_t i = 0; i < total_simbolos; i++) {
                runtime->simbolos[i] = objetos[ids_simbolos[i]];
                if (runtime->simbolos[i]->tipo != SEF_TIPO_SIMBOLO) {
                    sef_erro_definir(erro, 0, 0, "corrupt symbol table");
                    sucesso = false;
                    break;
                }
            }
        }
    }
    if (sucesso)
        sucesso = validar_tabelas_de_pacotes(runtime, erro) &&
                  sef_pacote_instalar_nulo(runtime, erro) &&
                  sef_primitivas_reconciliar(runtime, erro) &&
                  sef_formas_especiais_reconciliar(runtime, erro);

    free(objetos);
    free(ids_simbolos);
    free(ids_pacotes);
    registros_liberar(registros, quantidade);
    if (!sucesso) {
        sef_runtime_destruir(runtime);
        return NULL;
    }
    return runtime;
}
