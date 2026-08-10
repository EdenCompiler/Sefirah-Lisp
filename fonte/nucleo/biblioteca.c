#include "interno.h"

#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dlfcn.h>
#endif

struct SefRecursoBiblioteca {
    void *handle;
    char *caminho;
    size_t referencias;
};

static void *abrir_handle(const char *caminho, SefErro *erro) {
#ifdef _WIN32
    HMODULE biblioteca = LoadLibraryA(caminho);
    if (biblioteca == NULL)
        sef_erro_definir(erro, 0, 0, "Windows nao abriu a biblioteca compartilhada");
    return biblioteca;
#else
    dlerror();
    void *biblioteca = dlopen(caminho, RTLD_NOW | RTLD_LOCAL);
    if (biblioteca == NULL) {
        const char *mensagem = dlerror();
        sef_erro_definir(erro, 0, 0, "nao foi possivel abrir biblioteca: %s",
                         mensagem != NULL ? mensagem : "erro desconhecido");
    }
    return biblioteca;
#endif
}

static void fechar_handle(void *handle) {
#ifdef _WIN32
    FreeLibrary((HMODULE)handle);
#else
    dlclose(handle);
#endif
}

SefRecursoBiblioteca *sef_biblioteca_recurso_abrir(const char *caminho, SefErro *erro) {
    sef_erro_limpar(erro);
    if (caminho == NULL || caminho[0] == '\0') {
        sef_erro_definir(erro, 0, 0, "caminho de biblioteca compartilhada ausente");
        return NULL;
    }
    void *handle = abrir_handle(caminho, erro);
    if (handle == NULL)
        return NULL;
    SefRecursoBiblioteca *recurso = calloc(1, sizeof(*recurso));
    char *copia = malloc(strlen(caminho) + 1u);
    if (recurso == NULL || copia == NULL) {
        free(recurso);
        free(copia);
        fechar_handle(handle);
        sef_erro_definir(erro, 0, 0, "memoria insuficiente para biblioteca compartilhada");
        return NULL;
    }
    strcpy(copia, caminho);
    recurso->handle = handle;
    recurso->caminho = copia;
    recurso->referencias = 1;
    return recurso;
}

void sef_biblioteca_recurso_reter(SefRecursoBiblioteca *recurso) {
    if (recurso != NULL)
        recurso->referencias++;
}

void sef_biblioteca_recurso_liberar(SefRecursoBiblioteca *recurso) {
    if (recurso == NULL || recurso->referencias == 0)
        return;
    recurso->referencias--;
    if (recurso->referencias == 0) {
        fechar_handle(recurso->handle);
        free(recurso->caminho);
        free(recurso);
    }
}

SefFuncaoExternaI64 sef_biblioteca_recurso_resolver(SefRecursoBiblioteca *recurso,
                                                    const char *simbolo, SefErro *erro) {
    sef_erro_limpar(erro);
    if (recurso == NULL || simbolo == NULL || simbolo[0] == '\0') {
        sef_erro_definir(erro, 0, 0, "biblioteca ou simbolo ausente para resolucao");
        return NULL;
    }
    SefFuncaoExternaI64 endereco = NULL;
#ifdef _WIN32
    FARPROC encontrado = GetProcAddress((HMODULE)recurso->handle, simbolo);
    _Static_assert(sizeof(encontrado) == sizeof(endereco),
                   "ponteiros de simbolo Windows devem ter o mesmo tamanho");
    if (encontrado != NULL)
        memcpy(&endereco, &encontrado, sizeof(endereco));
    else
        sef_erro_definir(erro, 0, 0, "simbolo %s nao existe na biblioteca Windows", simbolo);
#else
    dlerror();
    void *encontrado = dlsym(recurso->handle, simbolo);
    const char *mensagem = dlerror();
    _Static_assert(sizeof(encontrado) == sizeof(endereco),
                   "ponteiros de simbolo POSIX devem ter o mesmo tamanho");
    if (mensagem == NULL)
        memcpy(&endereco, &encontrado, sizeof(endereco));
    else
        sef_erro_definir(erro, 0, 0, "simbolo %s nao foi resolvido: %s", simbolo, mensagem);
#endif
    return endereco;
}

const char *sef_biblioteca_recurso_caminho(const SefRecursoBiblioteca *recurso) {
    return recurso == NULL ? NULL : recurso->caminho;
}

SefValor sef_biblioteca_nova(SefRuntime *runtime, const char *caminho, SefErro *erro) {
    SefRecursoBiblioteca *recurso = sef_biblioteca_recurso_abrir(caminho, erro);
    if (recurso == NULL)
        return NULL;
    SefValor biblioteca = sef_objeto_novo(runtime, SEF_TIPO_BIBLIOTECA, erro);
    if (biblioteca == NULL) {
        sef_biblioteca_recurso_liberar(recurso);
        return NULL;
    }
    biblioteca->como.biblioteca.recurso = recurso;
    return biblioteca;
}

bool sef_biblioteca_fechar(SefValor biblioteca, SefErro *erro) {
    sef_erro_limpar(erro);
    if (biblioteca == NULL || biblioteca->tipo != SEF_TIPO_BIBLIOTECA) {
        sef_erro_definir(erro, 0, 0, "CLOSE-SHARED-LIBRARY exige uma biblioteca");
        return false;
    }
    if (biblioteca->como.biblioteca.fechada)
        return true;
    sef_biblioteca_recurso_liberar(biblioteca->como.biblioteca.recurso);
    biblioteca->como.biblioteca.recurso = NULL;
    biblioteca->como.biblioteca.fechada = true;
    return true;
}
