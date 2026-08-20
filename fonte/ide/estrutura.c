#include "apoio.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool delimitador(unsigned char caractere) {
    return caractere == '\0' || isspace(caractere) || caractere == '(' || caractere == ')' ||
           caractere == ';' || caractere == '\'' || caractere == '"' || caractere == '`' ||
           caractere == ',';
}

static void ignorar_espaco(const char *codigo, size_t tamanho, size_t *posicao) {
    for (;;) {
        while (*posicao < tamanho && isspace((unsigned char)codigo[*posicao]))
            (*posicao)++;
        if (*posicao >= tamanho || codigo[*posicao] != ';')
            return;
        while (*posicao < tamanho && codigo[*posicao] != '\n')
            (*posicao)++;
    }
}

static bool analisar_forma(const char *codigo, size_t tamanho, size_t *posicao) {
    ignorar_espaco(codigo, tamanho, posicao);
    if (*posicao >= tamanho || codigo[*posicao] == ')')
        return false;

    unsigned char caractere = (unsigned char)codigo[*posicao];
    if (caractere == '\'' || caractere == '`' || caractere == ',') {
        (*posicao)++;
        if (caractere == ',' && *posicao < tamanho && codigo[*posicao] == '@')
            (*posicao)++;
        return analisar_forma(codigo, tamanho, posicao);
    }
    if (caractere == '#' && *posicao + 1 < tamanho && codigo[*posicao + 1] == '\'') {
        *posicao += 2;
        return analisar_forma(codigo, tamanho, posicao);
    }
    if (caractere == '#' && *posicao + 1 < tamanho && codigo[*posicao + 1] == '\\') {
        *posicao += 2;
        if (*posicao >= tamanho)
            return false;
        if (delimitador((unsigned char)codigo[*posicao]))
            (*posicao)++;
        else
            while (*posicao < tamanho && !delimitador((unsigned char)codigo[*posicao]))
                (*posicao)++;
        return true;
    }
    if (caractere == '"') {
        bool escape = false;
        (*posicao)++;
        while (*posicao < tamanho) {
            unsigned char atual = (unsigned char)codigo[(*posicao)++];
            if (escape)
                escape = false;
            else if (atual == '\\')
                escape = true;
            else if (atual == '"')
                return true;
        }
        return false;
    }

    bool vetor = caractere == '#' && *posicao + 1 < tamanho && codigo[*posicao + 1] == '(';
    if (caractere == '(' || vetor) {
        if (vetor)
            (*posicao)++;
        (*posicao)++;
        for (;;) {
            ignorar_espaco(codigo, tamanho, posicao);
            if (*posicao >= tamanho)
                return false;
            if (codigo[*posicao] == ')') {
                (*posicao)++;
                return true;
            }
            if (!analisar_forma(codigo, tamanho, posicao))
                return false;
        }
    }

    size_t inicio = *posicao;
    bool entre_barras = false;
    bool escape = false;
    while (*posicao < tamanho) {
        caractere = (unsigned char)codigo[*posicao];
        if (escape) {
            escape = false;
            (*posicao)++;
        } else if (caractere == '\\') {
            escape = true;
            (*posicao)++;
        } else if (caractere == '|') {
            entre_barras = !entre_barras;
            (*posicao)++;
        } else if (!entre_barras && delimitador(caractere)) {
            break;
        } else {
            (*posicao)++;
        }
    }
    if (entre_barras || escape)
        return false;
    return *posicao > inicio;
}

bool sef_ide_forma_no_cursor(const char *codigo, size_t cursor, size_t *inicio, size_t *fim) {
    if (codigo == NULL || inicio == NULL || fim == NULL)
        return false;
    size_t tamanho = strlen(codigo);
    if (cursor > tamanho)
        cursor = tamanho;
    size_t posicao = 0;
    bool encontrou_anterior = false;
    size_t inicio_anterior = 0;
    size_t fim_anterior = 0;
    while (posicao < tamanho) {
        ignorar_espaco(codigo, tamanho, &posicao);
        if (posicao >= tamanho)
            break;
        size_t inicio_atual = posicao;
        if (!analisar_forma(codigo, tamanho, &posicao))
            return false;
        size_t fim_atual = posicao;
        if (cursor >= inicio_atual && cursor <= fim_atual) {
            *inicio = inicio_atual;
            *fim = fim_atual;
            return true;
        }
        if (fim_atual <= cursor) {
            encontrou_anterior = true;
            inicio_anterior = inicio_atual;
            fim_anterior = fim_atual;
        } else {
            break;
        }
    }
    if (!encontrou_anterior)
        return false;
    *inicio = inicio_anterior;
    *fim = fim_anterior;
    return true;
}

static uint64_t assinar_trecho(const char *codigo, size_t inicio, size_t fim) {
    uint64_t assinatura = UINT64_C(1469598103934665603);
    for (size_t i = inicio; i < fim; i++) {
        assinatura ^= (unsigned char)codigo[i];
        assinatura *= UINT64_C(1099511628211);
    }
    assinatura ^= (uint64_t)(fim - inicio);
    return assinatura;
}

static bool atomos_iguais_ascii(const char *codigo, size_t inicio, size_t fim,
                                const char *esperado) {
    size_t tamanho = fim - inicio;
    if (strlen(esperado) != tamanho)
        return false;
    for (size_t i = 0; i < tamanho; i++) {
        unsigned char caractere = (unsigned char)codigo[inicio + i];
        if ((unsigned char)toupper(caractere) != (unsigned char)esperado[i])
            return false;
    }
    return true;
}

static bool ler_atomo(const char *codigo, size_t limite, size_t *posicao, size_t *inicio,
                      size_t *fim) {
    ignorar_espaco(codigo, limite, posicao);
    if (*posicao >= limite || codigo[*posicao] == '(' || codigo[*posicao] == ')' ||
        codigo[*posicao] == '"')
        return false;
    *inicio = *posicao;
    bool entre_barras = false;
    bool escape = false;
    while (*posicao < limite) {
        unsigned char caractere = (unsigned char)codigo[*posicao];
        if (escape) {
            escape = false;
            (*posicao)++;
        } else if (caractere == '\\') {
            escape = true;
            (*posicao)++;
        } else if (caractere == '|') {
            entre_barras = !entre_barras;
            (*posicao)++;
        } else if (!entre_barras && delimitador(caractere)) {
            break;
        } else {
            (*posicao)++;
        }
    }
    *fim = *posicao;
    return *fim > *inicio && !entre_barras && !escape;
}

static void copiar_atomo(char *destino, size_t capacidade, const char *codigo, size_t inicio,
                         size_t fim) {
    if (capacidade == 0)
        return;
    size_t tamanho = fim - inicio;
    if (tamanho >= capacidade)
        tamanho = capacidade - 1;
    memcpy(destino, codigo + inicio, tamanho);
    destino[tamanho] = '\0';
}

static void classificar_forma(const char *codigo, SefFormaEstruturalIde *forma) {
    size_t posicao = forma->inicio;
    ignorar_espaco(codigo, forma->fim, &posicao);
    if (posicao >= forma->fim || codigo[posicao] != '(')
        return;
    posicao++;
    size_t inicio_operador = 0;
    size_t fim_operador = 0;
    size_t inicio_nome = 0;
    size_t fim_nome = 0;
    if (!ler_atomo(codigo, forma->fim, &posicao, &inicio_operador, &fim_operador) ||
        !ler_atomo(codigo, forma->fim, &posicao, &inicio_nome, &fim_nome))
        return;

    const char *categoria = NULL;
    if (atomos_iguais_ascii(codigo, inicio_operador, fim_operador, "DEFUN"))
        categoria = "FUNCTION";
    else if (atomos_iguais_ascii(codigo, inicio_operador, fim_operador, "DEFMACRO"))
        categoria = "MACRO";
    else if (atomos_iguais_ascii(codigo, inicio_operador, fim_operador, "DEFVAR"))
        categoria = "VARIABLE";
    else if (atomos_iguais_ascii(codigo, inicio_operador, fim_operador, "DEFPARAMETER"))
        categoria = "PARAMETER";
    else if (atomos_iguais_ascii(codigo, inicio_operador, fim_operador, "DEFCONSTANT"))
        categoria = "CONSTANT";
    else if (atomos_iguais_ascii(codigo, inicio_operador, fim_operador, "DEFPACKAGE"))
        categoria = "PACKAGE";
    else if (atomos_iguais_ascii(codigo, inicio_operador, fim_operador, "DEFINE"))
        categoria = "VALUE";
    if (categoria == NULL)
        return;

    forma->definicao = true;
    forma->inicio_nome = inicio_nome;
    forma->fim_nome = fim_nome;
    snprintf(forma->categoria, sizeof(forma->categoria), "%s", categoria);
    copiar_atomo(forma->nome, sizeof(forma->nome), codigo, inicio_nome, fim_nome);
}

bool sef_ide_catalogar_formas(const char *codigo, SefFormaEstruturalIde **formas,
                              size_t *quantidade, SefErro *erro) {
    sef_erro_limpar(erro);
    if (codigo == NULL || formas == NULL || quantidade == NULL) {
        sef_erro_definir(erro, 0, 0, "missing argument while cataloging IDE forms");
        return false;
    }
    *formas = NULL;
    *quantidade = 0;
    size_t capacidade = 0;
    size_t tamanho = strlen(codigo);
    size_t posicao = 0;
    size_t linha = 1;
    while (posicao < tamanho) {
        size_t antes_espaco = posicao;
        ignorar_espaco(codigo, tamanho, &posicao);
        for (size_t i = antes_espaco; i < posicao; i++)
            if (codigo[i] == '\n')
                linha++;
        if (posicao >= tamanho)
            break;
        size_t inicio = posicao;
        size_t linha_forma = linha;
        if (!analisar_forma(codigo, tamanho, &posicao)) {
            free(*formas);
            *formas = NULL;
            *quantidade = 0;
            sef_erro_definir(erro, linha_forma, 1, "incomplete Lisp form in IDE catalog");
            return false;
        }
        if (*quantidade == capacidade) {
            size_t nova_capacidade = capacidade == 0 ? 16 : capacidade * 2;
            if (nova_capacidade < capacidade || nova_capacidade > SIZE_MAX / sizeof(**formas)) {
                free(*formas);
                *formas = NULL;
                *quantidade = 0;
                sef_erro_definir(erro, 0, 0, "IDE structural catalog exceeded the limit");
                return false;
            }
            SefFormaEstruturalIde *novas = realloc(*formas, nova_capacidade * sizeof(**formas));
            if (novas == NULL) {
                free(*formas);
                *formas = NULL;
                *quantidade = 0;
                sef_erro_definir(erro, 0, 0, "not enough memory for IDE catalog");
                return false;
            }
            *formas = novas;
            capacidade = nova_capacidade;
        }
        SefFormaEstruturalIde *forma = &(*formas)[(*quantidade)++];
        memset(forma, 0, sizeof(*forma));
        forma->inicio = inicio;
        forma->fim = posicao;
        forma->linha = linha_forma;
        forma->inicio_nome = inicio;
        forma->assinatura = assinar_trecho(codigo, inicio, posicao);
        classificar_forma(codigo, forma);
        for (size_t i = inicio; i < posicao; i++)
            if (codigo[i] == '\n')
                linha++;
    }
    return true;
}

void sef_ide_catalogo_liberar(SefFormaEstruturalIde *formas) { free(formas); }

static bool proximo_atomo(const char *codigo, size_t tamanho, size_t *posicao, size_t *inicio,
                          size_t *fim) {
    while (*posicao < tamanho) {
        ignorar_espaco(codigo, tamanho, posicao);
        if (*posicao >= tamanho)
            return false;
        unsigned char caractere = (unsigned char)codigo[*posicao];
        if (caractere == '(' || caractere == ')' || caractere == '\'' || caractere == '`') {
            (*posicao)++;
            continue;
        }
        if (caractere == ',') {
            (*posicao)++;
            if (*posicao < tamanho && codigo[*posicao] == '@')
                (*posicao)++;
            continue;
        }
        if (caractere == '"') {
            bool escape = false;
            (*posicao)++;
            while (*posicao < tamanho) {
                unsigned char atual = (unsigned char)codigo[(*posicao)++];
                if (escape)
                    escape = false;
                else if (atual == '\\')
                    escape = true;
                else if (atual == '"')
                    break;
            }
            continue;
        }
        if (caractere == '#' && *posicao + 1 < tamanho &&
            (codigo[*posicao + 1] == '(' || codigo[*posicao + 1] == '\'')) {
            *posicao += 2;
            continue;
        }
        if (caractere == '#' && *posicao + 1 < tamanho && codigo[*posicao + 1] == '\\') {
            *posicao += 2;
            if (*posicao < tamanho && delimitador((unsigned char)codigo[*posicao]))
                (*posicao)++;
            else
                while (*posicao < tamanho && !delimitador((unsigned char)codigo[*posicao]))
                    (*posicao)++;
            continue;
        }
        if (ler_atomo(codigo, tamanho, posicao, inicio, fim))
            return true;
        (*posicao)++;
    }
    return false;
}

bool sef_ide_atomo_no_cursor(const char *codigo, size_t cursor, size_t *inicio, size_t *fim) {
    if (codigo == NULL || inicio == NULL || fim == NULL)
        return false;
    size_t tamanho = strlen(codigo);
    if (cursor > tamanho)
        cursor = tamanho;
    size_t posicao = 0;
    while (proximo_atomo(codigo, tamanho, &posicao, inicio, fim))
        if (cursor >= *inicio && cursor <= *fim)
            return true;
    return false;
}

static bool atomo_possui_escape(const char *codigo, size_t inicio, size_t fim) {
    for (size_t i = inicio; i < fim; i++)
        if (codigo[i] == '|' || codigo[i] == '\\')
            return true;
    return false;
}

static bool atomos_iguais_entre(const char *primeiro_codigo, size_t primeiro_inicio,
                                size_t primeiro_fim, const char *segundo_codigo,
                                size_t segundo_inicio, size_t segundo_fim) {
    if (primeiro_codigo == NULL || segundo_codigo == NULL || primeiro_fim < primeiro_inicio ||
        segundo_fim < segundo_inicio)
        return false;
    size_t primeiro_tamanho = primeiro_fim - primeiro_inicio;
    size_t segundo_tamanho = segundo_fim - segundo_inicio;
    if (primeiro_tamanho != segundo_tamanho)
        return false;
    bool exato = atomo_possui_escape(primeiro_codigo, primeiro_inicio, primeiro_fim) ||
                 atomo_possui_escape(segundo_codigo, segundo_inicio, segundo_fim);
    for (size_t i = 0; i < primeiro_tamanho; i++) {
        unsigned char primeiro = (unsigned char)primeiro_codigo[primeiro_inicio + i];
        unsigned char segundo = (unsigned char)segundo_codigo[segundo_inicio + i];
        if (exato ? primeiro != segundo : toupper(primeiro) != toupper(segundo))
            return false;
    }
    return true;
}

bool sef_ide_atomos_iguais(const char *codigo, size_t primeiro_inicio, size_t primeiro_fim,
                           size_t segundo_inicio, size_t segundo_fim) {
    return atomos_iguais_entre(codigo, primeiro_inicio, primeiro_fim, codigo, segundo_inicio,
                               segundo_fim);
}

static bool nome_de_definicao(const SefFormaEstruturalIde *formas, size_t quantidade, size_t inicio,
                              size_t fim) {
    for (size_t i = 0; i < quantidade; i++)
        if (formas[i].definicao && formas[i].inicio_nome == inicio && formas[i].fim_nome == fim)
            return true;
    return false;
}

static size_t forma_que_contem(const SefFormaEstruturalIde *formas, size_t quantidade,
                               size_t posicao) {
    for (size_t i = 0; i < quantidade; i++)
        if (posicao >= formas[i].inicio && posicao < formas[i].fim)
            return i;
    return SIZE_MAX;
}

static bool catalogar_referencias_entre(const char *codigo, const char *codigo_nome,
                                        size_t nome_inicio, size_t nome_fim,
                                        const SefFormaEstruturalIde *formas,
                                        size_t quantidade_formas,
                                        SefReferenciaEstruturalIde **referencias,
                                        size_t *quantidade_referencias, SefErro *erro) {
    if (codigo == NULL || referencias == NULL || quantidade_referencias == NULL ||
        codigo_nome == NULL || (quantidade_formas > 0 && formas == NULL) ||
        nome_fim <= nome_inicio) {
        sef_erro_definir(erro, 0, 0, "invalid structural reference query");
        return false;
    }
    *referencias = NULL;
    *quantidade_referencias = 0;
    size_t capacidade = 0;
    size_t tamanho = strlen(codigo);
    size_t posicao = 0;
    size_t contado_ate = 0;
    size_t linha = 1;
    size_t inicio = 0;
    size_t fim = 0;
    while (proximo_atomo(codigo, tamanho, &posicao, &inicio, &fim)) {
        for (size_t i = contado_ate; i < inicio; i++)
            if (codigo[i] == '\n')
                linha++;
        contado_ate = inicio;
        if (!atomos_iguais_entre(codigo_nome, nome_inicio, nome_fim, codigo, inicio, fim) ||
            nome_de_definicao(formas, quantidade_formas, inicio, fim))
            continue;
        if (*quantidade_referencias == capacidade) {
            size_t nova_capacidade = capacidade == 0 ? 16 : capacidade * 2;
            if (nova_capacidade < capacidade ||
                nova_capacidade > SIZE_MAX / sizeof(**referencias)) {
                free(*referencias);
                *referencias = NULL;
                *quantidade_referencias = 0;
                sef_erro_definir(erro, 0, 0, "reference catalog exceeded the limit");
                return false;
            }
            SefReferenciaEstruturalIde *novas =
                realloc(*referencias, nova_capacidade * sizeof(**referencias));
            if (novas == NULL) {
                free(*referencias);
                *referencias = NULL;
                *quantidade_referencias = 0;
                sef_erro_definir(erro, 0, 0, "not enough memory for IDE references");
                return false;
            }
            *referencias = novas;
            capacidade = nova_capacidade;
        }
        (*referencias)[(*quantidade_referencias)++] = (SefReferenciaEstruturalIde){
            inicio, fim, linha, forma_que_contem(formas, quantidade_formas, inicio)};
    }
    return true;
}

bool sef_ide_catalogar_referencias(const char *codigo, size_t nome_inicio, size_t nome_fim,
                                   const SefFormaEstruturalIde *formas, size_t quantidade_formas,
                                   SefReferenciaEstruturalIde **referencias,
                                   size_t *quantidade_referencias, SefErro *erro) {
    sef_erro_limpar(erro);
    if (codigo == NULL || nome_fim > strlen(codigo)) {
        sef_erro_definir(erro, 0, 0, "queried symbol is outside the buffer");
        return false;
    }
    return catalogar_referencias_entre(codigo, codigo, nome_inicio, nome_fim, formas,
                                       quantidade_formas, referencias, quantidade_referencias,
                                       erro);
}

bool sef_ide_catalogar_referencias_nome(const char *codigo, const char *nome,
                                        const SefFormaEstruturalIde *formas,
                                        size_t quantidade_formas,
                                        SefReferenciaEstruturalIde **referencias,
                                        size_t *quantidade_referencias, SefErro *erro) {
    sef_erro_limpar(erro);
    if (nome == NULL || nome[0] == '\0') {
        sef_erro_definir(erro, 0, 0, "workspace reference query requires a symbol name");
        return false;
    }
    return catalogar_referencias_entre(codigo, nome, 0, strlen(nome), formas, quantidade_formas,
                                       referencias, quantidade_referencias, erro);
}

void sef_ide_referencias_liberar(SefReferenciaEstruturalIde *referencias) { free(referencias); }
