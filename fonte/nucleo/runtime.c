#include "sefirah/interno.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *formas_common_lisp[] = {"QUOTE",
                                           "QUASIQUOTE",
                                           "IF",
                                           "PROGN",
                                           "LAMBDA",
                                           "FUNCTION",
                                           "DEFUN",
                                           "DEFMACRO",
                                           "DEFVAR",
                                           "DEFPARAMETER",
                                           "SETQ",
                                           "SETF",
                                           "LET",
                                           "LET*",
                                           "COND",
                                           "MULTIPLE-VALUE-BIND",
                                           "MULTIPLE-VALUE-LIST",
                                           "MULTIPLE-VALUE-PROG1",
                                           "MULTIPLE-VALUE-CALL",
                                           "NTH-VALUE",
                                           "WHEN",
                                           "UNLESS",
                                           "FLET",
                                           "LABELS",
                                           "MACROLET",
                                           "BLOCK",
                                           "RETURN-FROM",
                                           "RETURN",
                                           "CATCH",
                                           "THROW",
                                           "UNWIND-PROTECT",
                                           "IGNORE-ERRORS",
                                           "HANDLER-CASE",
                                           "HANDLER-BIND",
                                           "RESTART-CASE",
                                           "AND",
                                           "OR",
                                           "IN-PACKAGE",
                                           "DEFPACKAGE"};

bool sef_formas_especiais_reconciliar(SefRuntime *runtime, SefErro *erro) {
    for (size_t i = 0; i < sizeof(formas_common_lisp) / sizeof(formas_common_lisp[0]); i++) {
        SefValor forma =
            sef_simbolo_internar_em(runtime, runtime->pacote_common_lisp, formas_common_lisp[i],
                                    strlen(formas_common_lisp[i]), erro);
        if (forma == NULL ||
            !sef_pacote_exportar(runtime, runtime->pacote_common_lisp, forma, erro))
            return false;
    }
    return true;
}

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
    marcar(runtime->parametros_transferencia);
    marcar(runtime->corpo_transferencia);
    marcar(runtime->ambiente_transferencia);
    for (size_t i = 0; i < runtime->quantidade_valores; i++)
        marcar(runtime->valores_multiplos[i]);
    for (size_t i = 0; i < runtime->valores_transferencia.quantidade; i++)
        marcar(runtime->valores_transferencia.itens[i]);
    for (SefRaiz *raiz = runtime->raizes; raiz != NULL; raiz = raiz->proxima)
        marcar(raiz->valor);
    for (SefQuadroControle *quadro = runtime->controle; quadro != NULL; quadro = quadro->anterior)
        marcar(quadro->nome_ou_etiqueta);
    for (SefReinicioDinamico *reinicio = runtime->reinicios; reinicio != NULL;
         reinicio = reinicio->anterior) {
        marcar(reinicio->nome);
        marcar(reinicio->parametros);
        marcar(reinicio->corpo);
        marcar(reinicio->ambiente);
    }
    for (SefHandlerDinamico *handler = runtime->handlers; handler != NULL;
         handler = handler->anterior) {
        marcar(handler->tipo);
        marcar(handler->funcao);
    }
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
    if (!sef_pacote_instalar_nulo(runtime, erro))
        goto falhou;
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
    if (!sef_formas_especiais_reconciliar(runtime, erro))
        goto falhou;
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
    while (runtime->reinicios != NULL) {
        SefReinicioDinamico *anterior = runtime->reinicios->anterior;
        free(runtime->reinicios);
        runtime->reinicios = anterior;
    }
    while (runtime->handlers != NULL) {
        SefHandlerDinamico *anterior = runtime->handlers->anterior;
        free(runtime->handlers);
        runtime->handlers = anterior;
    }
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
    free(runtime->valores_multiplos);
    sef_valores_salvos_liberar(&runtime->valores_transferencia);
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
    if (!sef_valores_definir(runtime, NULL, 0, erro))
        return NULL;

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

static bool texto_acrescentar(char **destino, size_t *tamanho, size_t *capacidade,
                              const char *origem, size_t quantidade) {
    if (quantidade > SIZE_MAX - *tamanho - 1)
        return false;
    size_t necessario = *tamanho + quantidade + 1;
    if (necessario > *capacidade) {
        size_t nova_capacidade = *capacidade == 0 ? 256 : *capacidade;
        while (nova_capacidade < necessario) {
            if (nova_capacidade > SIZE_MAX / 2u) {
                nova_capacidade = necessario;
                break;
            }
            nova_capacidade *= 2u;
        }
        char *novo = realloc(*destino, nova_capacidade);
        if (novo == NULL)
            return false;
        *destino = novo;
        *capacidade = nova_capacidade;
    }
    memcpy(*destino + *tamanho, origem, quantidade);
    *tamanho += quantidade;
    (*destino)[*tamanho] = '\0';
    return true;
}

static bool repl_ler_linha(FILE *entrada, char **linha, size_t *tamanho, size_t *capacidade) {
    *tamanho = 0;
    int caractere;
    while ((caractere = fgetc(entrada)) != EOF) {
        char byte = (char)caractere;
        if (!texto_acrescentar(linha, tamanho, capacidade, &byte, 1))
            return false;
        if (caractere == '\n')
            break;
    }
    return *tamanho > 0;
}

static bool repl_comando_igual(const char *linha, size_t tamanho, const char *comando) {
    while (tamanho > 0 && (linha[tamanho - 1] == '\n' || linha[tamanho - 1] == '\r'))
        tamanho--;
    return strlen(comando) == tamanho && memcmp(linha, comando, tamanho) == 0;
}

static void repl_imprimir_erro(FILE *saida, const SefErro *erro) {
    if (erro->linha > 0)
        fprintf(saida, "Erro em %zu:%zu: %s\n", erro->linha, erro->coluna, erro->mensagem);
    else
        fprintf(saida, "Erro: %s\n", erro->mensagem);
}

static bool repl_imprimir_valores(SefRuntime *runtime, FILE *saida, SefErro *erro) {
    size_t quantidade = sef_runtime_quantidade_valores(runtime);
    for (size_t i = 0; i < quantidade; i++) {
        char *texto = sef_valor_para_texto(runtime, sef_runtime_valor(runtime, i), true, erro);
        if (texto == NULL)
            return false;
        fprintf(saida, "%s\n", texto);
        sef_texto_liberar(texto);
    }
    return true;
}

int sef_runtime_repl(SefRuntime *runtime, FILE *entrada, FILE *saida) {
    char *linha = NULL;
    char *codigo = NULL;
    size_t tamanho_linha = 0, capacidade_linha = 0;
    size_t tamanho_codigo = 0, capacidade_codigo = 0;
    int resultado = 0;
    SefErro erro;

    fputs("Sefirah Lisp 0.0.1 — ambiente interativo\n", saida);
    fputs("Use :sair ou Ctrl-D para encerrar; :ajuda mostra os comandos.\n", saida);

    for (;;) {
        fputs(tamanho_codigo == 0 ? "sefirah> " : "......> ", saida);
        fflush(saida);
        if (!repl_ler_linha(entrada, &linha, &tamanho_linha, &capacidade_linha)) {
            if (ferror(entrada)) {
                fputs("Erro: falha ao ler a entrada.\n", saida);
                resultado = 1;
            } else if (tamanho_codigo > 0) {
                fputs("Erro: codigo incompleto ao final da entrada.\n", saida);
                resultado = 1;
            }
            break;
        }

        if (tamanho_codigo == 0 && (repl_comando_igual(linha, tamanho_linha, ":sair") ||
                                    repl_comando_igual(linha, tamanho_linha, ":quit") ||
                                    repl_comando_igual(linha, tamanho_linha, "(SAIR)") ||
                                    repl_comando_igual(linha, tamanho_linha, "(sair)"))) {
            break;
        }
        if (tamanho_codigo == 0 && repl_comando_igual(linha, tamanho_linha, ":ajuda")) {
            fputs(":ajuda  mostra esta ajuda\n:sair   encerra o REPL\n", saida);
            continue;
        }
        if (!texto_acrescentar(&codigo, &tamanho_codigo, &capacidade_codigo, linha,
                               tamanho_linha)) {
            fputs("Erro: memoria insuficiente para a entrada.\n", saida);
            resultado = 1;
            break;
        }

        SefEstadoCodigo estado = sef_runtime_estado_codigo(codigo, &erro);
        if (estado == SEF_CODIGO_INCOMPLETO)
            continue;
        if (estado == SEF_CODIGO_INVALIDO) {
            repl_imprimir_erro(saida, &erro);
            tamanho_codigo = 0;
            codigo[0] = '\0';
            continue;
        }

        SefValor valor = sef_runtime_avaliar_texto(runtime, codigo, &erro);
        tamanho_codigo = 0;
        codigo[0] = '\0';
        if (valor == NULL) {
            repl_imprimir_erro(saida, &erro);
            continue;
        }
        if (!repl_imprimir_valores(runtime, saida, &erro)) {
            fprintf(saida, "Erro ao imprimir: %s\n", erro.mensagem);
            continue;
        }
    }

    free(linha);
    free(codigo);
    fputc('\n', saida);
    return resultado;
}
