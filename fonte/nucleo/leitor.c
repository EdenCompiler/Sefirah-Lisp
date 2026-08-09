#include "interno.h"

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

void sef_leitor_iniciar(SefLeitor *leitor, SefRuntime *runtime, const char *codigo) {
    leitor->runtime = runtime;
    leitor->inicio = codigo;
    leitor->atual = codigo;
    leitor->linha = 1;
    leitor->coluna = 1;
}

static char atual(const SefLeitor *leitor) { return *leitor->atual; }

static char avancar(SefLeitor *leitor) {
    char caractere = *leitor->atual;
    if (caractere == '\0')
        return caractere;
    leitor->atual++;
    if (caractere == '\n') {
        leitor->linha++;
        leitor->coluna = 1;
    } else {
        leitor->coluna++;
    }
    return caractere;
}

static void ignorar_espaco(SefLeitor *leitor) {
    for (;;) {
        while (isspace((unsigned char)atual(leitor)))
            avancar(leitor);
        if (atual(leitor) != ';')
            return;
        while (atual(leitor) != '\0' && atual(leitor) != '\n')
            avancar(leitor);
    }
}

static bool delimitador(char caractere) {
    return caractere == '\0' || isspace((unsigned char)caractere) || caractere == '(' ||
           caractere == ')' || caractere == ';' || caractere == '\'' || caractere == '"' ||
           caractere == '`' || caractere == ',';
}

static SefValor ler_interno(SefLeitor *leitor, SefErro *erro);

static SefValor formar_unaria(SefLeitor *leitor, const char *nome, SefValor argumento,
                              SefErro *erro) {
    SefValor simbolo = sef_simbolo_internar(leitor->runtime, nome, strlen(nome), erro);
    if (simbolo == NULL)
        return NULL;
    SefValor cauda = sef_par_novo(leitor->runtime, argumento, leitor->runtime->nulo, erro);
    if (cauda == NULL)
        return NULL;
    return sef_par_novo(leitor->runtime, simbolo, cauda, erro);
}

static SefValor ler_texto(SefLeitor *leitor, SefErro *erro) {
    size_t linha = leitor->linha;
    size_t coluna = leitor->coluna;
    avancar(leitor);

    size_t capacidade = 32;
    size_t tamanho = 0;
    char *dados = malloc(capacidade);
    if (dados == NULL) {
        sef_erro_definir(erro, linha, coluna, "memoria insuficiente para texto");
        return NULL;
    }

    while (atual(leitor) != '\0' && atual(leitor) != '"') {
        char caractere = avancar(leitor);
        if (caractere == '\\') {
            if (atual(leitor) == '\0')
                break;
            caractere = avancar(leitor);
            if (caractere == 'n')
                caractere = '\n';
            else if (caractere == 'r')
                caractere = '\r';
            else if (caractere == 't')
                caractere = '\t';
        }
        if (tamanho + 1 >= capacidade) {
            capacidade *= 2;
            char *novos = realloc(dados, capacidade);
            if (novos == NULL) {
                free(dados);
                sef_erro_definir(erro, linha, coluna, "memoria insuficiente para texto");
                return NULL;
            }
            dados = novos;
        }
        dados[tamanho++] = caractere;
    }

    if (atual(leitor) != '"') {
        free(dados);
        sef_erro_definir(erro, linha, coluna, "texto sem aspas de fechamento");
        return NULL;
    }
    avancar(leitor);
    SefValor valor = sef_texto_novo(leitor->runtime, dados, tamanho, erro);
    free(dados);
    return valor;
}

static SefValor ler_atomo(SefLeitor *leitor, SefErro *erro) {
    const char *inicio = leitor->atual;
    size_t linha = leitor->linha;
    size_t coluna = leitor->coluna;
    while (!delimitador(atual(leitor)))
        avancar(leitor);
    size_t tamanho = (size_t)(leitor->atual - inicio);
    if (tamanho == 0) {
        sef_erro_definir(erro, linha, coluna, "atomo vazio");
        return NULL;
    }

    char *token = malloc(tamanho + 1);
    if (token == NULL) {
        sef_erro_definir(erro, linha, coluna, "memoria insuficiente para token");
        return NULL;
    }
    memcpy(token, inicio, tamanho);
    token[tamanho] = '\0';

    errno = 0;
    char *fim = NULL;
    long long inteiro = strtoll(token, &fim, 10);
    if (errno == 0 && fim == token + tamanho && tamanho > 0) {
        free(token);
        return sef_inteiro_novo(leitor->runtime, inteiro, erro);
    }

    errno = 0;
    fim = NULL;
    double real = strtod(token, &fim);
    if (errno == 0 && fim == token + tamanho && tamanho > 0 && strpbrk(token, ".eE") != NULL) {
        free(token);
        return sef_real_novo(leitor->runtime, real, erro);
    }

    if ((tamanho == 3 && (memcmp(token, "nil", 3) == 0 || memcmp(token, "NIL", 3) == 0))) {
        free(token);
        return leitor->runtime->nulo;
    }

    SefValor simbolo = sef_simbolo_internar(leitor->runtime, token, tamanho, erro);
    free(token);
    return simbolo;
}

static SefValor ler_lista(SefLeitor *leitor, SefErro *erro) {
    size_t linha = leitor->linha;
    size_t coluna = leitor->coluna;
    avancar(leitor);
    ignorar_espaco(leitor);
    if (atual(leitor) == ')') {
        avancar(leitor);
        return leitor->runtime->nulo;
    }

    SefValor cabeca = leitor->runtime->nulo;
    SefValor ultimo = NULL;
    while (atual(leitor) != '\0' && atual(leitor) != ')') {
        if (atual(leitor) == '.' && delimitador(leitor->atual[1])) {
            if (cabeca == leitor->runtime->nulo) {
                sef_erro_definir(erro, leitor->linha, leitor->coluna,
                                 "ponto sem elemento anterior");
                return NULL;
            }
            avancar(leitor);
            ignorar_espaco(leitor);
            SefValor cauda = ler_interno(leitor, erro);
            if (cauda == NULL)
                return NULL;
            ignorar_espaco(leitor);
            if (atual(leitor) != ')') {
                sef_erro_definir(erro, leitor->linha, leitor->coluna,
                                 "lista pontuada deve terminar apos a cauda");
                return NULL;
            }
            avancar(leitor);
            ultimo->como.par.resto = cauda;
            return cabeca;
        }

        SefValor item = ler_interno(leitor, erro);
        if (item == NULL)
            return NULL;
        SefValor celula = sef_par_novo(leitor->runtime, item, leitor->runtime->nulo, erro);
        if (celula == NULL)
            return NULL;
        if (cabeca == leitor->runtime->nulo)
            cabeca = celula;
        else
            ultimo->como.par.resto = celula;
        ultimo = celula;
        ignorar_espaco(leitor);
    }

    if (atual(leitor) != ')') {
        sef_erro_definir(erro, linha, coluna, "lista sem parenteses de fechamento");
        return NULL;
    }
    avancar(leitor);
    return cabeca;
}

static SefValor ler_interno(SefLeitor *leitor, SefErro *erro) {
    ignorar_espaco(leitor);
    char caractere = atual(leitor);
    if (caractere == '\0') {
        sef_erro_definir(erro, leitor->linha, leitor->coluna, "fim inesperado da entrada");
        return NULL;
    }
    if (caractere == '(')
        return ler_lista(leitor, erro);
    if (caractere == ')') {
        sef_erro_definir(erro, leitor->linha, leitor->coluna,
                         "parenteses de fechamento inesperado");
        return NULL;
    }
    if (caractere == '"')
        return ler_texto(leitor, erro);
    if (caractere == '\'') {
        avancar(leitor);
        SefValor forma = ler_interno(leitor, erro);
        return forma == NULL ? NULL : formar_unaria(leitor, "QUOTE", forma, erro);
    }
    if (caractere == '`') {
        avancar(leitor);
        SefValor forma = ler_interno(leitor, erro);
        return forma == NULL ? NULL : formar_unaria(leitor, "QUASIQUOTE", forma, erro);
    }
    if (caractere == ',') {
        avancar(leitor);
        const char *nome = "UNQUOTE";
        if (atual(leitor) == '@') {
            avancar(leitor);
            nome = "UNQUOTE-SPLICING";
        }
        SefValor forma = ler_interno(leitor, erro);
        return forma == NULL ? NULL : formar_unaria(leitor, nome, forma, erro);
    }
    if (caractere == '#' && leitor->atual[1] == '\'') {
        avancar(leitor);
        avancar(leitor);
        SefValor forma = ler_interno(leitor, erro);
        return forma == NULL ? NULL : formar_unaria(leitor, "FUNCTION", forma, erro);
    }
    return ler_atomo(leitor, erro);
}

SefValor sef_ler_forma(SefLeitor *leitor, bool *encontrou, SefErro *erro) {
    ignorar_espaco(leitor);
    if (atual(leitor) == '\0') {
        *encontrou = false;
        return leitor->runtime->nulo;
    }
    *encontrou = true;
    return ler_interno(leitor, erro);
}
