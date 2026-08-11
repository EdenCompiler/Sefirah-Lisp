#include "sefirah/interno.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sef_erro_limpar(SefErro *erro) {
    if (erro == NULL)
        return;
    erro->ocorreu = false;
    erro->linha = 0;
    erro->coluna = 0;
    erro->mensagem[0] = '\0';
}

void sef_erro_definir(SefErro *erro, size_t linha, size_t coluna, const char *formato, ...) {
    if (erro == NULL || erro->ocorreu)
        return;
    erro->ocorreu = true;
    erro->linha = linha;
    erro->coluna = coluna;
    va_list argumentos;
    va_start(argumentos, formato);
    vsnprintf(erro->mensagem, sizeof(erro->mensagem), formato, argumentos);
    va_end(argumentos);
}

static void marcar(SefValor valor) {
    if (valor == NULL || valor->marcado)
        return;
    valor->marcado = true;

    switch (valor->tipo) {
    case SEF_TIPO_SIMBOLO:
        marcar(valor->como.simbolo.pacote);
        break;
    case SEF_TIPO_PAR:
        marcar(valor->como.par.primeiro);
        marcar(valor->como.par.resto);
        break;
    case SEF_TIPO_FUNCAO:
        marcar(valor->como.funcao.parametros);
        marcar(valor->como.funcao.corpo);
        marcar(valor->como.funcao.ambiente);
        break;
    case SEF_TIPO_AMBIENTE:
        marcar(valor->como.ambiente.pai);
        for (SefVinculo *vinculo = valor->como.ambiente.vinculos; vinculo != NULL;
             vinculo = vinculo->proximo) {
            marcar(vinculo->simbolo);
            marcar(vinculo->valor);
        }
        for (SefVinculo *vinculo = valor->como.ambiente.funcoes; vinculo != NULL;
             vinculo = vinculo->proximo) {
            marcar(vinculo->simbolo);
            marcar(vinculo->valor);
        }
        break;
    case SEF_TIPO_CONDICAO:
        marcar(valor->como.condicao.classe);
        marcar(valor->como.condicao.mensagem);
        break;
    case SEF_TIPO_PACOTE:
        for (size_t i = 0; i < valor->como.pacote.quantidade_simbolos; i++)
            marcar(valor->como.pacote.simbolos[i]);
        for (size_t i = 0; i < valor->como.pacote.quantidade_usados; i++)
            marcar(valor->como.pacote.usados[i]);
        for (size_t i = 0; i < valor->como.pacote.quantidade_exportados; i++)
            marcar(valor->como.pacote.exportados[i]);
        break;
    case SEF_TIPO_VETOR:
        for (size_t i = 0; i < valor->como.vetor.tamanho; i++)
            marcar(valor->como.vetor.itens[i]);
        break;
    case SEF_TIPO_TABELA_HASH:
        for (size_t i = 0; i < valor->como.tabela_hash.capacidade; i++) {
            SefEntradaHash *entrada = &valor->como.tabela_hash.entradas[i];
            if (entrada->estado == SEF_ENTRADA_HASH_OCUPADA) {
                marcar(entrada->chave);
                marcar(entrada->valor);
            }
        }
        break;
    case SEF_TIPO_STREAM:
    case SEF_TIPO_BIBLIOTECA:
        break;
    default:
        break;
    }
}

static void objeto_conteudo_liberar(SefValor objeto) {
    if (objeto->tipo == SEF_TIPO_TEXTO) {
        free(objeto->como.texto.dados);
    } else if (objeto->tipo == SEF_TIPO_SIMBOLO) {
        free(objeto->como.simbolo.nome);
    } else if (objeto->tipo == SEF_TIPO_PACOTE) {
        free(objeto->como.pacote.nome);
        free(objeto->como.pacote.simbolos);
        free(objeto->como.pacote.usados);
        free(objeto->como.pacote.exportados);
    } else if (objeto->tipo == SEF_TIPO_STREAM) {
        if (objeto->como.stream.possui_arquivo && !objeto->como.stream.fechado &&
            objeto->como.stream.arquivo != NULL)
            fclose(objeto->como.stream.arquivo);
        free(objeto->como.stream.caminho);
    } else if (objeto->tipo == SEF_TIPO_BIBLIOTECA) {
        if (!objeto->como.biblioteca.fechada)
            sef_biblioteca_recurso_liberar(objeto->como.biblioteca.recurso);
    } else if (objeto->tipo == SEF_TIPO_FUNCAO) {
        sef_funcao_compilada_liberar(objeto->como.funcao.compilada_i64);
    } else if (objeto->tipo == SEF_TIPO_VETOR) {
        free(objeto->como.vetor.itens);
    } else if (objeto->tipo == SEF_TIPO_TABELA_HASH) {
        free(objeto->como.tabela_hash.entradas);
    } else if (objeto->tipo == SEF_TIPO_AMBIENTE) {
        SefVinculo *vinculo = objeto->como.ambiente.vinculos;
        while (vinculo != NULL) {
            SefVinculo *proximo = vinculo->proximo;
            free(vinculo);
            vinculo = proximo;
        }
        vinculo = objeto->como.ambiente.funcoes;
        while (vinculo != NULL) {
            SefVinculo *proximo = vinculo->proximo;
            free(vinculo);
            vinculo = proximo;
        }
    }
}

size_t sef_runtime_coletar(SefRuntime *runtime, SefValor raiz_temporaria) {
    if (runtime == NULL)
        return 0;
    marcar(runtime->nulo);
    marcar(runtime->verdadeiro);
    marcar(runtime->ambiente_global);
    marcar(runtime->entrada_padrao);
    marcar(runtime->saida_padrao);
    marcar(runtime->erro_padrao);
    marcar(runtime->valor_transferencia);
    for (SefRaiz *raiz = runtime->raizes; raiz != NULL; raiz = raiz->proxima)
        marcar(raiz->valor);
    for (SefQuadroControle *quadro = runtime->controle; quadro != NULL; quadro = quadro->anterior)
        marcar(quadro->nome_ou_etiqueta);
    marcar(raiz_temporaria);
    for (size_t i = 0; i < runtime->quantidade_simbolos; i++) {
        marcar(runtime->simbolos[i]);
    }
    for (size_t i = 0; i < runtime->quantidade_pacotes; i++)
        marcar(runtime->pacotes[i]);

    size_t removidos = 0;
    SefValor *endereco = &runtime->objetos;
    while (*endereco != NULL) {
        SefValor objeto = *endereco;
        if (!objeto->marcado) {
            *endereco = objeto->proximo_alocado;
            objeto_conteudo_liberar(objeto);
            free(objeto);
            runtime->quantidade_objetos--;
            removidos++;
        } else {
            objeto->marcado = false;
            endereco = &objeto->proximo_alocado;
        }
    }
    return removidos;
}

size_t sef_runtime_objetos_vivos(const SefRuntime *runtime) {
    return runtime == NULL ? 0 : runtime->quantidade_objetos;
}

SefRuntime *sef_runtime_criar(SefErro *erro) {
    sef_erro_limpar(erro);
    SefRuntime *runtime = calloc(1, sizeof(*runtime));
    if (runtime == NULL) {
        sef_erro_definir(erro, 0, 0, "memoria insuficiente ao criar runtime");
        return NULL;
    }

    runtime->nulo = sef_objeto_novo(runtime, SEF_TIPO_NULO, erro);
    if (runtime->nulo == NULL)
        goto falhou;
    runtime->pacote_common_lisp = sef_pacote_novo(runtime, "COMMON-LISP", erro);
    runtime->pacote_keyword = sef_pacote_novo(runtime, "KEYWORD", erro);
    SefValor pacote_sefirah = sef_pacote_novo(runtime, "SEFIRAH", erro);
    SefValor pacote_usuario = sef_pacote_novo(runtime, "COMMON-LISP-USER", erro);
    if (runtime->pacote_common_lisp == NULL || runtime->pacote_keyword == NULL ||
        pacote_sefirah == NULL || pacote_usuario == NULL)
        goto falhou;
    runtime->pacote_atual = pacote_usuario;
    if (!sef_pacote_usar(runtime, pacote_usuario, runtime->pacote_common_lisp, erro))
        goto falhou;
    runtime->verdadeiro =
        sef_simbolo_internar_em(runtime, runtime->pacote_common_lisp, "T", 1, erro);
    if (runtime->verdadeiro == NULL)
        goto falhou;
    if (!sef_pacote_exportar(runtime, runtime->pacote_common_lisp, runtime->verdadeiro, erro))
        goto falhou;
    runtime->ambiente_global = sef_ambiente_novo(runtime, runtime->nulo, erro);
    if (runtime->ambiente_global == NULL)
        goto falhou;

    if (!sef_ambiente_definir(runtime, runtime->ambiente_global, runtime->verdadeiro,
                              runtime->verdadeiro, erro)) {
        goto falhou;
    }
    runtime->entrada_padrao = sef_stream_novo(runtime, stdin, NULL, false, 1, erro);
    runtime->saida_padrao = sef_stream_novo(runtime, stdout, NULL, false, 2, erro);
    runtime->erro_padrao = sef_stream_novo(runtime, stderr, NULL, false, 3, erro);
    if (runtime->entrada_padrao == NULL || runtime->saida_padrao == NULL ||
        runtime->erro_padrao == NULL)
        goto falhou;
    static const struct {
        const char *nome;
        size_t tamanho;
    } nomes_streams[] = {
        {"*STANDARD-INPUT*", 16}, {"*STANDARD-OUTPUT*", 17}, {"*ERROR-OUTPUT*", 14}};
    SefValor valores_streams[] = {runtime->entrada_padrao, runtime->saida_padrao,
                                  runtime->erro_padrao};
    for (size_t i = 0; i < 3; i++) {
        SefValor simbolo =
            sef_simbolo_internar_em(runtime, runtime->pacote_common_lisp, nomes_streams[i].nome,
                                    nomes_streams[i].tamanho, erro);
        if (simbolo == NULL ||
            !sef_pacote_exportar(runtime, runtime->pacote_common_lisp, simbolo, erro) ||
            !sef_ambiente_definir(runtime, runtime->ambiente_global, simbolo, valores_streams[i],
                                  erro))
            goto falhou;
    }
    static const char *formas_common_lisp[] = {
        "QUOTE",          "QUASIQUOTE",    "IF",           "PROGN",  "LAMBDA",
        "FUNCTION",       "DEFUN",         "DEFMACRO",     "DEFVAR", "DEFPARAMETER",
        "SETQ",           "SETF",          "LET",          "LET*",   "COND",
        "WHEN",           "UNLESS",        "FLET",         "LABELS", "MACROLET",
        "BLOCK",          "RETURN-FROM",   "RETURN",       "CATCH",  "THROW",
        "UNWIND-PROTECT", "IGNORE-ERRORS", "HANDLER-CASE", "AND",    "OR",
        "IN-PACKAGE",     "DEFPACKAGE"};
    for (size_t i = 0; i < sizeof(formas_common_lisp) / sizeof(formas_common_lisp[0]); i++) {
        SefValor forma =
            sef_simbolo_internar_em(runtime, runtime->pacote_common_lisp, formas_common_lisp[i],
                                    strlen(formas_common_lisp[i]), erro);
        if (forma == NULL ||
            !sef_pacote_exportar(runtime, runtime->pacote_common_lisp, forma, erro))
            goto falhou;
    }
    SefValor simbolo_pacote =
        sef_simbolo_internar_em(runtime, runtime->pacote_common_lisp, "*PACKAGE*", 9, erro);
    if (simbolo_pacote == NULL ||
        !sef_pacote_exportar(runtime, runtime->pacote_common_lisp, simbolo_pacote, erro) ||
        !sef_ambiente_definir(runtime, runtime->ambiente_global, simbolo_pacote, pacote_usuario,
                              erro))
        goto falhou;
    if (!sef_primitivas_instalar(runtime, erro))
        goto falhou;
    return runtime;

falhou:
    sef_runtime_destruir(runtime);
    return NULL;
}

void sef_runtime_destruir(SefRuntime *runtime) {
    if (runtime == NULL)
        return;
    SefRaiz *raiz = runtime->raizes;
    while (raiz != NULL) {
        SefRaiz *proxima = raiz->proxima;
        raiz->runtime = NULL;
        free(raiz);
        raiz = proxima;
    }
    SefValor objeto = runtime->objetos;
    while (objeto != NULL) {
        SefValor proximo = objeto->proximo_alocado;
        objeto_conteudo_liberar(objeto);
        free(objeto);
        objeto = proximo;
    }
    free(runtime->simbolos);
    free(runtime->pacotes);
    free(runtime);
}

SefRaiz *sef_raiz_criar(SefRuntime *runtime, SefValor valor, SefErro *erro) {
    sef_erro_limpar(erro);
    if (runtime == NULL || valor == NULL) {
        sef_erro_definir(erro, 0, 0, "runtime ou valor ausente ao criar raiz");
        return NULL;
    }
    SefRaiz *raiz = malloc(sizeof(*raiz));
    if (raiz == NULL) {
        sef_erro_definir(erro, 0, 0, "memoria insuficiente para raiz do coletor");
        return NULL;
    }
    raiz->runtime = runtime;
    raiz->valor = valor;
    raiz->anterior = NULL;
    raiz->proxima = runtime->raizes;
    if (runtime->raizes != NULL)
        runtime->raizes->anterior = raiz;
    runtime->raizes = raiz;
    return raiz;
}

SefValor sef_raiz_valor(const SefRaiz *raiz) { return raiz == NULL ? NULL : raiz->valor; }

void sef_raiz_definir(SefRaiz *raiz, SefValor valor) {
    if (raiz != NULL)
        raiz->valor = valor;
}

void sef_raiz_liberar(SefRaiz *raiz) {
    if (raiz == NULL || raiz->runtime == NULL)
        return;
    if (raiz->anterior != NULL)
        raiz->anterior->proxima = raiz->proxima;
    else
        raiz->runtime->raizes = raiz->proxima;
    if (raiz->proxima != NULL)
        raiz->proxima->anterior = raiz->anterior;
    raiz->runtime = NULL;
    free(raiz);
}

SefValor sef_runtime_avaliar_texto(SefRuntime *runtime, const char *codigo, SefErro *erro) {
    sef_erro_limpar(erro);
    if (runtime == NULL || codigo == NULL) {
        sef_erro_definir(erro, 0, 0, "runtime ou codigo ausente");
        return NULL;
    }

    SefLeitor leitor;
    sef_leitor_iniciar(&leitor, runtime, codigo);
    SefValor ultimo = runtime->nulo;
    for (;;) {
        bool encontrou = false;
        SefValor forma = sef_ler_forma(&leitor, &encontrou, erro);
        if (erro != NULL && erro->ocorreu)
            return NULL;
        if (!encontrou)
            break;
        ultimo = sef_avaliar(runtime, forma, runtime->ambiente_global, erro);
        if (ultimo == NULL)
            return NULL;
    }
    sef_runtime_coletar(runtime, ultimo);
    return ultimo;
}

static char *arquivo_ler(const char *caminho, SefErro *erro) {
    FILE *arquivo = fopen(caminho, "rb");
    if (arquivo == NULL) {
        sef_erro_definir(erro, 0, 0, "nao foi possivel abrir '%s': %s", caminho, strerror(errno));
        return NULL;
    }
    if (fseek(arquivo, 0, SEEK_END) != 0) {
        fclose(arquivo);
        sef_erro_definir(erro, 0, 0, "nao foi possivel medir '%s'", caminho);
        return NULL;
    }
    long tamanho = ftell(arquivo);
    if (tamanho < 0 || fseek(arquivo, 0, SEEK_SET) != 0) {
        fclose(arquivo);
        sef_erro_definir(erro, 0, 0, "arquivo '%s' nao e pesquisavel", caminho);
        return NULL;
    }
    char *conteudo = malloc((size_t)tamanho + 1);
    if (conteudo == NULL) {
        fclose(arquivo);
        sef_erro_definir(erro, 0, 0, "memoria insuficiente para ler '%s'", caminho);
        return NULL;
    }
    size_t lidos = fread(conteudo, 1, (size_t)tamanho, arquivo);
    fclose(arquivo);
    if (lidos != (size_t)tamanho) {
        free(conteudo);
        sef_erro_definir(erro, 0, 0, "leitura incompleta de '%s'", caminho);
        return NULL;
    }
    conteudo[lidos] = '\0';
    return conteudo;
}

bool sef_runtime_executar_arquivo(SefRuntime *runtime, const char *caminho, SefValor *ultimo,
                                  SefErro *erro) {
    sef_erro_limpar(erro);
    char *conteudo = arquivo_ler(caminho, erro);
    if (conteudo == NULL)
        return false;
    SefValor resultado = sef_runtime_avaliar_texto(runtime, conteudo, erro);
    free(conteudo);
    if (resultado == NULL)
        return false;
    if (ultimo != NULL)
        *ultimo = resultado;
    return true;
}

int sef_runtime_repl(SefRuntime *runtime, FILE *entrada, FILE *saida) {
    char linha[8192];
    SefErro erro;
    fputs("Sefirah Lisp 0.0.1 — bootstrap C17\n", saida);
    fputs("Digite (SAIR) ou Ctrl-D para encerrar.\n", saida);

    for (;;) {
        fputs("sefirah> ", saida);
        fflush(saida);
        if (fgets(linha, sizeof(linha), entrada) == NULL)
            break;
        if (strcmp(linha, "(SAIR)\n") == 0 || strcmp(linha, "(sair)\n") == 0)
            break;
        SefValor valor = sef_runtime_avaliar_texto(runtime, linha, &erro);
        if (valor == NULL) {
            if (erro.linha > 0) {
                fprintf(saida, "Erro em %zu:%zu: %s\n", erro.linha, erro.coluna, erro.mensagem);
            } else {
                fprintf(saida, "Erro: %s\n", erro.mensagem);
            }
            continue;
        }
        char *texto = sef_valor_para_texto(runtime, valor, true, &erro);
        if (texto == NULL) {
            fprintf(saida, "Erro ao imprimir: %s\n", erro.mensagem);
            continue;
        }
        fprintf(saida, "%s\n", texto);
        sef_texto_liberar(texto);
    }
    fputc('\n', saida);
    return 0;
}
