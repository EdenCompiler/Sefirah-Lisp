#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "espaco_trabalho.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

enum { SEF_LIMITE_ARQUIVOS_ESPACO_TRABALHO = 10000, SEF_LIMITE_PROFUNDIDADE_ESPACO_TRABALHO = 32 };

typedef struct SefArquivoEspacoIde {
    char *relativo;
    char *absoluto;
} SefArquivoEspacoIde;

struct SefEspacoTrabalhoIde {
    char *raiz;
    SefArquivoEspacoIde *arquivos;
    size_t quantidade;
    size_t capacidade;
};

static char *texto_duplicar(const char *texto, SefErro *erro) {
    size_t tamanho = strlen(texto);
    char *copia = malloc(tamanho + 1);
    if (copia == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for the workspace");
        return NULL;
    }
    memcpy(copia, texto, tamanho + 1);
    return copia;
}

static char *raiz_duplicar(const char *raiz, SefErro *erro) {
    size_t tamanho = strlen(raiz);
    while (tamanho > 1 && (raiz[tamanho - 1] == '/' || raiz[tamanho - 1] == '\\')) {
        if (tamanho == 3 && raiz[1] == ':')
            break;
        tamanho--;
    }
    char *copia = malloc(tamanho + 1);
    if (copia == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for the workspace root");
        return NULL;
    }
    memcpy(copia, raiz, tamanho);
    copia[tamanho] = '\0';
    return copia;
}

static void espaco_trabalho_limpar(SefEspacoTrabalhoIde *espaco) {
    if (espaco == NULL)
        return;
    free(espaco->raiz);
    for (size_t i = 0; i < espaco->quantidade; i++) {
        free(espaco->arquivos[i].relativo);
        free(espaco->arquivos[i].absoluto);
    }
    free(espaco->arquivos);
    memset(espaco, 0, sizeof(*espaco));
}

SefEspacoTrabalhoIde *sef_espaco_trabalho_ide_criar(SefErro *erro) {
    sef_erro_limpar(erro);
    SefEspacoTrabalhoIde *espaco = calloc(1, sizeof(*espaco));
    if (espaco == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for the workspace");
        return NULL;
    }
    espaco->raiz = texto_duplicar("", erro);
    if (espaco->raiz == NULL) {
        free(espaco);
        return NULL;
    }
    return espaco;
}

void sef_espaco_trabalho_ide_destruir(SefEspacoTrabalhoIde *espaco) {
    if (espaco == NULL)
        return;
    espaco_trabalho_limpar(espaco);
    free(espaco);
}

static bool termina_com_lisp(const char *nome) {
    size_t tamanho = strlen(nome);
    if (tamanho < 5)
        return false;
    const char *extensao = nome + tamanho - 5;
    return extensao[0] == '.' && tolower((unsigned char)extensao[1]) == 'l' &&
           tolower((unsigned char)extensao[2]) == 'i' &&
           tolower((unsigned char)extensao[3]) == 's' && tolower((unsigned char)extensao[4]) == 'p';
}

static bool diretorio_ignorado(const char *nome) {
    return strcmp(nome, ".") == 0 || strcmp(nome, "..") == 0 || strcmp(nome, ".git") == 0 ||
           strcmp(nome, "construir") == 0 || strcmp(nome, "build") == 0 ||
           strcmp(nome, "node_modules") == 0;
}

static char separador_caminho(void) {
#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

static char *caminho_unir(const char *base, const char *nome, SefErro *erro) {
    size_t tamanho_base = strlen(base);
    size_t tamanho_nome = strlen(nome);
    bool precisa_separador =
        tamanho_base > 0 && base[tamanho_base - 1] != '/' && base[tamanho_base - 1] != '\\';
    if (tamanho_base > SIZE_MAX - tamanho_nome - (precisa_separador ? 2u : 1u)) {
        sef_erro_definir(erro, 0, 0, "workspace path exceeded the supported size");
        return NULL;
    }
    char *resultado = malloc(tamanho_base + tamanho_nome + (precisa_separador ? 2u : 1u));
    if (resultado == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for a workspace path");
        return NULL;
    }
    memcpy(resultado, base, tamanho_base);
    size_t cursor = tamanho_base;
    if (precisa_separador)
        resultado[cursor++] = separador_caminho();
    memcpy(resultado + cursor, nome, tamanho_nome + 1);
    return resultado;
}

static bool espaco_trabalho_adicionar(SefEspacoTrabalhoIde *espaco, const char *relativo,
                                      const char *absoluto, SefErro *erro) {
    if (espaco->quantidade >= SEF_LIMITE_ARQUIVOS_ESPACO_TRABALHO) {
        sef_erro_definir(erro, 0, 0, "workspace contains more than %d Lisp files",
                         SEF_LIMITE_ARQUIVOS_ESPACO_TRABALHO);
        return false;
    }
    if (espaco->quantidade == espaco->capacidade) {
        size_t capacidade = espaco->capacidade == 0 ? 32 : espaco->capacidade * 2;
        if (capacidade < espaco->capacidade || capacidade > SIZE_MAX / sizeof(*espaco->arquivos)) {
            sef_erro_definir(erro, 0, 0, "workspace file index exceeded the supported size");
            return false;
        }
        SefArquivoEspacoIde *arquivos = realloc(espaco->arquivos, capacidade * sizeof(*arquivos));
        if (arquivos == NULL) {
            sef_erro_definir(erro, 0, 0, "not enough memory for the workspace file index");
            return false;
        }
        espaco->arquivos = arquivos;
        espaco->capacidade = capacidade;
    }
    char *copia_relativa = texto_duplicar(relativo, erro);
    if (copia_relativa == NULL)
        return false;
    char *copia_absoluta = texto_duplicar(absoluto, erro);
    if (copia_absoluta == NULL) {
        free(copia_relativa);
        return false;
    }
    espaco->arquivos[espaco->quantidade++] = (SefArquivoEspacoIde){copia_relativa, copia_absoluta};
    return true;
}

#ifdef _WIN32
static bool espaco_trabalho_visitar(SefEspacoTrabalhoIde *espaco, const char *diretorio,
                                    const char *prefixo, unsigned profundidade, SefErro *erro) {
    if (profundidade > SEF_LIMITE_PROFUNDIDADE_ESPACO_TRABALHO) {
        sef_erro_definir(erro, 0, 0, "workspace directory nesting exceeds %d levels",
                         SEF_LIMITE_PROFUNDIDADE_ESPACO_TRABALHO);
        return false;
    }
    char *padrao = caminho_unir(diretorio, "*", erro);
    if (padrao == NULL)
        return false;
    WIN32_FIND_DATAA entrada;
    HANDLE busca = FindFirstFileA(padrao, &entrada);
    free(padrao);
    if (busca == INVALID_HANDLE_VALUE) {
        sef_erro_definir(erro, 0, 0, "could not read workspace directory '%s'", diretorio);
        return false;
    }
    bool percorreu = true;
    do {
        const char *nome = entrada.cFileName;
        if (diretorio_ignorado(nome))
            continue;
        char *absoluto = caminho_unir(diretorio, nome, erro);
        char *relativo = caminho_unir(prefixo, nome, erro);
        if (absoluto == NULL || relativo == NULL) {
            free(absoluto);
            free(relativo);
            percorreu = false;
            break;
        }
        if ((entrada.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
            (entrada.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0)
            percorreu = espaco_trabalho_visitar(espaco, absoluto, relativo, profundidade + 1, erro);
        else if ((entrada.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
                 termina_com_lisp(nome))
            percorreu = espaco_trabalho_adicionar(espaco, relativo, absoluto, erro);
        free(absoluto);
        free(relativo);
    } while (percorreu && FindNextFileA(busca, &entrada));
    FindClose(busca);
    return percorreu;
}

static bool caminho_e_diretorio(const char *caminho) {
    DWORD atributos = GetFileAttributesA(caminho);
    return atributos != INVALID_FILE_ATTRIBUTES && (atributos & FILE_ATTRIBUTE_DIRECTORY) != 0;
}
#else
static bool espaco_trabalho_visitar(SefEspacoTrabalhoIde *espaco, const char *diretorio,
                                    const char *prefixo, unsigned profundidade, SefErro *erro) {
    if (profundidade > SEF_LIMITE_PROFUNDIDADE_ESPACO_TRABALHO) {
        sef_erro_definir(erro, 0, 0, "workspace directory nesting exceeds %d levels",
                         SEF_LIMITE_PROFUNDIDADE_ESPACO_TRABALHO);
        return false;
    }
    DIR *pasta = opendir(diretorio);
    if (pasta == NULL) {
        sef_erro_definir(erro, 0, 0, "could not read workspace directory '%s': %s", diretorio,
                         strerror(errno));
        return false;
    }
    bool percorreu = true;
    struct dirent *entrada = NULL;
    while (percorreu && (entrada = readdir(pasta)) != NULL) {
        const char *nome = entrada->d_name;
        if (diretorio_ignorado(nome))
            continue;
        char *absoluto = caminho_unir(diretorio, nome, erro);
        char *relativo = caminho_unir(prefixo, nome, erro);
        if (absoluto == NULL || relativo == NULL) {
            free(absoluto);
            free(relativo);
            percorreu = false;
            break;
        }
        struct stat estado;
        if (lstat(absoluto, &estado) != 0) {
            sef_erro_definir(erro, 0, 0, "could not inspect workspace entry '%s': %s", absoluto,
                             strerror(errno));
            percorreu = false;
        } else if (S_ISDIR(estado.st_mode))
            percorreu = espaco_trabalho_visitar(espaco, absoluto, relativo, profundidade + 1, erro);
        else if (S_ISREG(estado.st_mode) && termina_com_lisp(nome))
            percorreu = espaco_trabalho_adicionar(espaco, relativo, absoluto, erro);
        free(absoluto);
        free(relativo);
    }
    closedir(pasta);
    return percorreu;
}

static bool caminho_e_diretorio(const char *caminho) {
    struct stat estado;
    return lstat(caminho, &estado) == 0 && S_ISDIR(estado.st_mode);
}
#endif

static int comparar_arquivos(const void *esquerda, const void *direita) {
    const SefArquivoEspacoIde *arquivo_esquerdo = esquerda;
    const SefArquivoEspacoIde *arquivo_direito = direita;
    return strcmp(arquivo_esquerdo->relativo, arquivo_direito->relativo);
}

bool sef_espaco_trabalho_ide_abrir(SefEspacoTrabalhoIde *espaco, const char *raiz, SefErro *erro) {
    sef_erro_limpar(erro);
    if (espaco == NULL || raiz == NULL || raiz[0] == '\0') {
        sef_erro_definir(erro, 0, 0, "missing workspace directory");
        return false;
    }
    if (!caminho_e_diretorio(raiz)) {
        sef_erro_definir(erro, 0, 0, "workspace path is not a directory: '%s'", raiz);
        return false;
    }
    SefEspacoTrabalhoIde novo = {0};
    novo.raiz = raiz_duplicar(raiz, erro);
    bool abriu = novo.raiz != NULL && espaco_trabalho_visitar(&novo, novo.raiz, "", 0, erro);
    if (abriu && novo.quantidade > 1)
        qsort(novo.arquivos, novo.quantidade, sizeof(*novo.arquivos), comparar_arquivos);
    if (!abriu) {
        espaco_trabalho_limpar(&novo);
        return false;
    }
    espaco_trabalho_limpar(espaco);
    *espaco = novo;
    return true;
}

const char *sef_espaco_trabalho_ide_raiz(const SefEspacoTrabalhoIde *espaco) {
    return espaco == NULL || espaco->raiz == NULL ? "" : espaco->raiz;
}

size_t sef_espaco_trabalho_ide_quantidade(const SefEspacoTrabalhoIde *espaco) {
    return espaco == NULL ? 0 : espaco->quantidade;
}

const char *sef_espaco_trabalho_ide_arquivo_relativo(const SefEspacoTrabalhoIde *espaco,
                                                     size_t indice) {
    return espaco == NULL || indice >= espaco->quantidade ? NULL
                                                          : espaco->arquivos[indice].relativo;
}

const char *sef_espaco_trabalho_ide_arquivo_absoluto(const SefEspacoTrabalhoIde *espaco,
                                                     size_t indice) {
    return espaco == NULL || indice >= espaco->quantidade ? NULL
                                                          : espaco->arquivos[indice].absoluto;
}
