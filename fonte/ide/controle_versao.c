#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif

#include "controle_versao.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

enum { SEF_LIMITE_SAIDA_GIT = 1024 * 1024 };

typedef struct SaidaGitIde {
    char *dados;
    size_t tamanho;
    size_t capacidade;
} SaidaGitIde;

static bool saida_acrescentar(SaidaGitIde *saida, const char *dados, size_t tamanho,
                              SefErro *erro) {
    if (tamanho > SEF_LIMITE_SAIDA_GIT - saida->tamanho) {
        sef_erro_definir(erro, 0, 0, "Git status output exceeded the 1 MiB limit");
        return false;
    }
    size_t necessario = saida->tamanho + tamanho + 1;
    if (necessario > saida->capacidade) {
        size_t capacidade = saida->capacidade == 0 ? 4096 : saida->capacidade;
        while (capacidade < necessario)
            capacidade *= 2;
        char *dados_novos = realloc(saida->dados, capacidade);
        if (dados_novos == NULL) {
            sef_erro_definir(erro, 0, 0, "not enough memory for Git status output");
            return false;
        }
        saida->dados = dados_novos;
        saida->capacidade = capacidade;
    }
    memcpy(saida->dados + saida->tamanho, dados, tamanho);
    saida->tamanho += tamanho;
    saida->dados[saida->tamanho] = '\0';
    return true;
}

#ifdef _WIN32
static bool comando_acrescentar(char *comando, size_t capacidade, size_t *tamanho,
                                const char *texto) {
    size_t quantidade = strlen(texto);
    if (quantidade >= capacidade - *tamanho)
        return false;
    memcpy(comando + *tamanho, texto, quantidade);
    *tamanho += quantidade;
    comando[*tamanho] = '\0';
    return true;
}

static bool comando_argumento(char *comando, size_t capacidade, size_t *tamanho,
                              const char *argumento) {
    if (!comando_acrescentar(comando, capacidade, tamanho, " \""))
        return false;
    size_t barras = 0;
    for (const char *cursor = argumento;; cursor++) {
        if (*cursor == '\\') {
            barras++;
            continue;
        }
        size_t repeticoes = barras * (*cursor == '\"' || *cursor == '\0' ? 2u : 1u);
        while (repeticoes-- > 0)
            if (!comando_acrescentar(comando, capacidade, tamanho, "\\"))
                return false;
        barras = 0;
        if (*cursor == '\0')
            break;
        if (*cursor == '\"' && !comando_acrescentar(comando, capacidade, tamanho, "\\"))
            return false;
        char caractere[2] = {*cursor, '\0'};
        if (!comando_acrescentar(comando, capacidade, tamanho, caractere))
            return false;
    }
    return comando_acrescentar(comando, capacidade, tamanho, "\"");
}

static bool executar_git(const char *raiz, SaidaGitIde *saida, int *codigo_saida, SefErro *erro) {
    HANDLE leitura = NULL;
    HANDLE escrita = NULL;
    SECURITY_ATTRIBUTES atributos = {sizeof(atributos), NULL, TRUE};
    if (!CreatePipe(&leitura, &escrita, &atributos, 0) ||
        !SetHandleInformation(leitura, HANDLE_FLAG_INHERIT, 0)) {
        if (leitura != NULL)
            CloseHandle(leitura);
        if (escrita != NULL)
            CloseHandle(escrita);
        sef_erro_definir(erro, 0, 0, "could not create a pipe for Git status");
        return false;
    }
    size_t tamanho_raiz = strlen(raiz);
    if (tamanho_raiz > (SIZE_MAX - 256u) / 2u) {
        CloseHandle(leitura);
        CloseHandle(escrita);
        sef_erro_definir(erro, 0, 0, "workspace path is too large for Git status");
        return false;
    }
    size_t capacidade_comando = tamanho_raiz * 2u + 256u;
    char *comando = calloc(capacidade_comando, 1);
    size_t tamanho = 0;
    bool montou = comando != NULL && comando_acrescentar(comando, capacidade_comando, &tamanho,
                                                         "git") &&
                  comando_argumento(comando, capacidade_comando, &tamanho, "-c") &&
                  comando_argumento(comando, capacidade_comando, &tamanho, "core.quotepath=false") &&
                  comando_argumento(comando, capacidade_comando, &tamanho, "-C") &&
                  comando_argumento(comando, capacidade_comando, &tamanho, raiz) &&
                  comando_argumento(comando, capacidade_comando, &tamanho, "status") &&
                  comando_argumento(comando, capacidade_comando, &tamanho, "--porcelain=v1") &&
                  comando_argumento(comando, capacidade_comando, &tamanho, "--branch") &&
                  comando_argumento(comando, capacidade_comando, &tamanho,
                                    "--untracked-files=normal");
    if (!montou) {
        free(comando);
        CloseHandle(leitura);
        CloseHandle(escrita);
        sef_erro_definir(erro, 0, 0, "Git command exceeded the supported path size");
        return false;
    }
    STARTUPINFOA inicio = {0};
    inicio.cb = sizeof(inicio);
    inicio.dwFlags = STARTF_USESTDHANDLES;
    inicio.hStdOutput = escrita;
    inicio.hStdError = escrita;
    inicio.hStdInput = CreateFileA("NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   &atributos, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (inicio.hStdInput == INVALID_HANDLE_VALUE) {
        free(comando);
        CloseHandle(leitura);
        CloseHandle(escrita);
        sef_erro_definir(erro, 0, 0, "could not prepare standard input for Git status");
        return false;
    }
    PROCESS_INFORMATION processo = {0};
    BOOL criou = CreateProcessA(NULL, comando, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL,
                                &inicio, &processo);
    free(comando);
    CloseHandle(inicio.hStdInput);
    CloseHandle(escrita);
    if (!criou) {
        CloseHandle(leitura);
        sef_erro_definir(erro, 0, 0, "could not start Git; verify that it is installed");
        return false;
    }
    bool leu = true;
    char bloco[4096];
    DWORD quantidade = 0;
    while (ReadFile(leitura, bloco, sizeof(bloco), &quantidade, NULL) && quantidade > 0) {
        if (!saida_acrescentar(saida, bloco, (size_t)quantidade, erro)) {
            leu = false;
            break;
        }
    }
    CloseHandle(leitura);
    WaitForSingleObject(processo.hProcess, INFINITE);
    DWORD codigo = 1;
    GetExitCodeProcess(processo.hProcess, &codigo);
    CloseHandle(processo.hThread);
    CloseHandle(processo.hProcess);
    *codigo_saida = (int)codigo;
    return leu;
}
#else
static bool executar_git(const char *raiz, SaidaGitIde *saida, int *codigo_saida, SefErro *erro) {
    int descritores[2];
    if (pipe(descritores) != 0) {
        sef_erro_definir(erro, 0, 0, "could not create a pipe for Git status: %s",
                         strerror(errno));
        return false;
    }
    pid_t processo = fork();
    if (processo < 0) {
        close(descritores[0]);
        close(descritores[1]);
        sef_erro_definir(erro, 0, 0, "could not start Git: %s", strerror(errno));
        return false;
    }
    if (processo == 0) {
        close(descritores[0]);
        dup2(descritores[1], STDOUT_FILENO);
        dup2(descritores[1], STDERR_FILENO);
        close(descritores[1]);
        setenv("LC_ALL", "C", 1);
        execlp("git", "git", "-c", "core.quotepath=false", "-C", raiz, "status",
               "--porcelain=v1", "--branch", "--untracked-files=normal", (char *)NULL);
        _exit(127);
    }
    close(descritores[1]);
    bool leu = true;
    char bloco[4096];
    for (;;) {
        ssize_t quantidade = read(descritores[0], bloco, sizeof(bloco));
        if (quantidade == 0)
            break;
        if (quantidade < 0) {
            if (errno == EINTR)
                continue;
            sef_erro_definir(erro, 0, 0, "could not read Git status output: %s",
                             strerror(errno));
            leu = false;
            break;
        }
        if (!saida_acrescentar(saida, bloco, (size_t)quantidade, erro)) {
            leu = false;
            break;
        }
    }
    close(descritores[0]);
    int estado = 0;
    while (waitpid(processo, &estado, 0) < 0) {
        if (errno != EINTR) {
            sef_erro_definir(erro, 0, 0, "could not wait for Git status: %s", strerror(errno));
            return false;
        }
    }
    *codigo_saida = WIFEXITED(estado) ? WEXITSTATUS(estado) : 1;
    return leu;
}
#endif

bool sef_controle_versao_git_status(const char *raiz, char **saida, int *codigo_saida,
                                    SefErro *erro) {
    sef_erro_limpar(erro);
    if (raiz == NULL || raiz[0] == '\0' || saida == NULL || codigo_saida == NULL) {
        sef_erro_definir(erro, 0, 0, "missing workspace path for Git status");
        return false;
    }
    *saida = NULL;
    *codigo_saida = 1;
    SaidaGitIde captura = {0};
    if (!executar_git(raiz, &captura, codigo_saida, erro)) {
        free(captura.dados);
        return false;
    }
    if (captura.dados == NULL) {
        captura.dados = malloc(1);
        if (captura.dados == NULL) {
            sef_erro_definir(erro, 0, 0, "not enough memory for Git status output");
            return false;
        }
        captura.dados[0] = '\0';
    }
    *saida = captura.dados;
    return true;
}
