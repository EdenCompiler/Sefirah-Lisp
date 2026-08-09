#ifndef SEFIRAH_RUNTIME_H
#define SEFIRAH_RUNTIME_H

#include "sefirah/erro.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct SefRuntime SefRuntime;
typedef struct SefObjeto *SefValor;
typedef struct SefRaiz SefRaiz;
typedef struct SefFuncaoCompilada SefFuncaoCompilada;

SefRuntime *sef_runtime_criar(SefErro *erro);
void sef_runtime_destruir(SefRuntime *runtime);

SefValor sef_runtime_avaliar_texto(SefRuntime *runtime, const char *codigo, SefErro *erro);
bool sef_runtime_executar_arquivo(SefRuntime *runtime, const char *caminho, SefValor *ultimo,
                                  SefErro *erro);
int sef_runtime_repl(SefRuntime *runtime, FILE *entrada, FILE *saida);

bool sef_runtime_imagem_salvar(SefRuntime *runtime, const char *caminho, SefErro *erro);
SefRuntime *sef_runtime_imagem_abrir(const char *caminho, SefErro *erro);

char *sef_valor_para_texto(SefRuntime *runtime, SefValor valor, bool legivel, SefErro *erro);
void sef_texto_liberar(char *texto);

size_t sef_runtime_coletar(SefRuntime *runtime, SefValor raiz_temporaria);
size_t sef_runtime_objetos_vivos(const SefRuntime *runtime);

SefRaiz *sef_raiz_criar(SefRuntime *runtime, SefValor valor, SefErro *erro);
SefValor sef_raiz_valor(const SefRaiz *raiz);
void sef_raiz_definir(SefRaiz *raiz, SefValor valor);
void sef_raiz_liberar(SefRaiz *raiz);

bool sef_valor_e_inteiro(SefValor valor);
long long sef_valor_como_inteiro(SefValor valor);
bool sef_valor_e_nulo(SefRuntime *runtime, SefValor valor);

SefFuncaoCompilada *sef_runtime_compilar_funcao_i64(SefRuntime *runtime, const char *nome,
                                                    SefErro *erro);
bool sef_funcao_compilada_executar_i64(const SefFuncaoCompilada *funcao, const int64_t *argumentos,
                                       size_t quantidade_argumentos, int64_t *resultado,
                                       SefErro *erro);
bool sef_funcao_compilada_gravar_elf(const SefFuncaoCompilada *funcao, const char *nome_simbolo,
                                     const char *caminho, SefErro *erro);
bool sef_funcao_compilada_gravar_coff(const SefFuncaoCompilada *funcao, const char *nome_simbolo,
                                      const char *caminho, SefErro *erro);
bool sef_funcao_compilada_gravar_macho(const SefFuncaoCompilada *funcao, const char *nome_simbolo,
                                       const char *caminho, SefErro *erro);
void sef_funcao_compilada_liberar(SefFuncaoCompilada *funcao);

#endif
