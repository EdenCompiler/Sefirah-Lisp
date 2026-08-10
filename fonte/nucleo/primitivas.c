#include "interno.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SefValor car(SefValor lista) { return lista->como.par.primeiro; }
static SefValor cdr(SefValor lista) { return lista->como.par.resto; }

static bool quantidade(SefRuntime *runtime, SefValor argumentos, size_t minimo, size_t maximo,
                       const char *nome, SefErro *erro) {
    bool propria = false;
    size_t obtida = sef_lista_tamanho(runtime, argumentos, &propria);
    if (propria && obtida >= minimo && obtida <= maximo)
        return true;
    if (minimo == maximo) {
        sef_erro_definir(erro, 0, 0, "%s espera %zu argumento(s)", nome, minimo);
    } else {
        sef_erro_definir(erro, 0, 0, "%s espera entre %zu e %zu argumentos", nome, minimo, maximo);
    }
    return false;
}

static bool numero(SefValor valor, double *real, bool *inteiro) {
    if (valor->tipo == SEF_TIPO_INTEIRO) {
        *real = (double)valor->como.inteiro;
        *inteiro = true;
        return true;
    }
    if (valor->tipo == SEF_TIPO_REAL) {
        *real = valor->como.real;
        *inteiro = false;
        return true;
    }
    return false;
}

static SefValor primitiva_somar(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    int64_t soma_inteira = 0;
    double soma_real = 0.0;
    bool todos_inteiros = true;
    while (argumentos != runtime->nulo) {
        double valor;
        bool e_inteiro;
        if (!numero(car(argumentos), &valor, &e_inteiro)) {
            sef_erro_definir(erro, 0, 0, "+ aceita somente numeros");
            return NULL;
        }
        if (todos_inteiros && e_inteiro)
            soma_inteira += car(argumentos)->como.inteiro;
        else {
            if (todos_inteiros)
                soma_real = (double)soma_inteira;
            soma_real += valor;
            todos_inteiros = false;
        }
        argumentos = cdr(argumentos);
    }
    return todos_inteiros ? sef_inteiro_novo(runtime, soma_inteira, erro)
                          : sef_real_novo(runtime, soma_real, erro);
}

static SefValor primitiva_multiplicar(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    int64_t produto_inteiro = 1;
    double produto_real = 1.0;
    bool todos_inteiros = true;
    while (argumentos != runtime->nulo) {
        double valor;
        bool e_inteiro;
        if (!numero(car(argumentos), &valor, &e_inteiro)) {
            sef_erro_definir(erro, 0, 0, "* aceita somente numeros");
            return NULL;
        }
        if (todos_inteiros && e_inteiro)
            produto_inteiro *= car(argumentos)->como.inteiro;
        else {
            if (todos_inteiros)
                produto_real = (double)produto_inteiro;
            produto_real *= valor;
            todos_inteiros = false;
        }
        argumentos = cdr(argumentos);
    }
    return todos_inteiros ? sef_inteiro_novo(runtime, produto_inteiro, erro)
                          : sef_real_novo(runtime, produto_real, erro);
}

static SefValor primitiva_subtrair(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, (size_t)-1, "-", erro))
        return NULL;
    double acumulado;
    bool primeiro_inteiro;
    if (!numero(car(argumentos), &acumulado, &primeiro_inteiro)) {
        sef_erro_definir(erro, 0, 0, "- aceita somente numeros");
        return NULL;
    }
    bool todos_inteiros = primeiro_inteiro;
    argumentos = cdr(argumentos);
    if (argumentos == runtime->nulo)
        acumulado = -acumulado;
    while (argumentos != runtime->nulo) {
        double valor;
        bool e_inteiro;
        if (!numero(car(argumentos), &valor, &e_inteiro)) {
            sef_erro_definir(erro, 0, 0, "- aceita somente numeros");
            return NULL;
        }
        acumulado -= valor;
        todos_inteiros = todos_inteiros && e_inteiro;
        argumentos = cdr(argumentos);
    }
    return todos_inteiros ? sef_inteiro_novo(runtime, (int64_t)acumulado, erro)
                          : sef_real_novo(runtime, acumulado, erro);
}

static SefValor primitiva_dividir(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, (size_t)-1, "/", erro))
        return NULL;
    double acumulado;
    bool ignorado;
    if (!numero(car(argumentos), &acumulado, &ignorado)) {
        sef_erro_definir(erro, 0, 0, "/ aceita somente numeros");
        return NULL;
    }
    argumentos = cdr(argumentos);
    if (argumentos == runtime->nulo) {
        if (acumulado == 0.0)
            goto divisao_zero;
        acumulado = 1.0 / acumulado;
    }
    while (argumentos != runtime->nulo) {
        double divisor;
        if (!numero(car(argumentos), &divisor, &ignorado)) {
            sef_erro_definir(erro, 0, 0, "/ aceita somente numeros");
            return NULL;
        }
        if (divisor == 0.0)
            goto divisao_zero;
        acumulado /= divisor;
        argumentos = cdr(argumentos);
    }
    return sef_real_novo(runtime, acumulado, erro);

divisao_zero:
    sef_erro_definir(erro, 0, 0, "divisao por zero");
    return NULL;
}

typedef bool (*Comparador)(double, double);
static bool menor(double a, double b) { return a < b; }
static bool maior(double a, double b) { return a > b; }
static bool menor_igual(double a, double b) { return a <= b; }
static bool maior_igual(double a, double b) { return a >= b; }
static bool igual_numero(double a, double b) { return a == b; }

static SefValor comparar(SefRuntime *runtime, SefValor argumentos, Comparador comparador,
                         const char *nome, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, (size_t)-1, nome, erro))
        return NULL;
    double anterior;
    bool ignorado;
    if (!numero(car(argumentos), &anterior, &ignorado))
        goto tipo_invalido;
    argumentos = cdr(argumentos);
    while (argumentos != runtime->nulo) {
        double corrente;
        if (!numero(car(argumentos), &corrente, &ignorado))
            goto tipo_invalido;
        if (!comparador(anterior, corrente))
            return runtime->nulo;
        anterior = corrente;
        argumentos = cdr(argumentos);
    }
    return runtime->verdadeiro;

tipo_invalido:
    sef_erro_definir(erro, 0, 0, "%s aceita somente numeros", nome);
    return NULL;
}

static SefValor primitiva_menor(SefRuntime *r, SefValor a, SefErro *e) {
    return comparar(r, a, menor, "<", e);
}
static SefValor primitiva_maior(SefRuntime *r, SefValor a, SefErro *e) {
    return comparar(r, a, maior, ">", e);
}
static SefValor primitiva_igual_numero(SefRuntime *r, SefValor a, SefErro *e) {
    return comparar(r, a, igual_numero, "=", e);
}
static SefValor primitiva_menor_igual(SefRuntime *r, SefValor a, SefErro *e) {
    return comparar(r, a, menor_igual, "<=", e);
}
static SefValor primitiva_maior_igual(SefRuntime *r, SefValor a, SefErro *e) {
    return comparar(r, a, maior_igual, ">=", e);
}

static SefValor primitiva_numeros_diferentes(SefRuntime *runtime, SefValor argumentos,
                                             SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, (size_t)-1, "/=", erro))
        return NULL;
    for (SefValor cursor = argumentos; cursor != runtime->nulo; cursor = cdr(cursor)) {
        double a;
        bool ignorado;
        if (!numero(car(cursor), &a, &ignorado)) {
            sef_erro_definir(erro, 0, 0, "/= aceita somente numeros");
            return NULL;
        }
        for (SefValor restante = cdr(cursor); restante != runtime->nulo; restante = cdr(restante)) {
            double b;
            if (!numero(car(restante), &b, &ignorado)) {
                sef_erro_definir(erro, 0, 0, "/= aceita somente numeros");
                return NULL;
            }
            if (a == b)
                return runtime->nulo;
        }
    }
    return runtime->verdadeiro;
}

static SefValor primitiva_cons(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, "CONS", erro))
        return NULL;
    return sef_par_novo(runtime, car(argumentos), car(cdr(argumentos)), erro);
}

static SefValor primitiva_car(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "CAR", erro))
        return NULL;
    SefValor valor = car(argumentos);
    if (valor == runtime->nulo)
        return runtime->nulo;
    if (valor->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "CAR exige uma lista");
        return NULL;
    }
    return car(valor);
}

static SefValor primitiva_cdr(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "CDR", erro))
        return NULL;
    SefValor valor = car(argumentos);
    if (valor == runtime->nulo)
        return runtime->nulo;
    if (valor->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "CDR exige uma lista");
        return NULL;
    }
    return cdr(valor);
}

static SefValor primitiva_list(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    (void)runtime;
    (void)erro;
    return argumentos;
}

static SefValor primitiva_vector(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    bool propria = false;
    size_t tamanho = sef_lista_tamanho(runtime, argumentos, &propria);
    if (!propria) {
        sef_erro_definir(erro, 0, 0, "VECTOR recebeu argumentos improprios");
        return NULL;
    }
    SefValor vetor = sef_vetor_novo(runtime, tamanho, runtime->nulo, erro);
    if (vetor == NULL)
        return NULL;
    for (size_t i = 0; i < tamanho; i++) {
        vetor->como.vetor.itens[i] = car(argumentos);
        argumentos = cdr(argumentos);
    }
    return vetor;
}

static SefValor primitiva_make_array(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, (size_t)-1, "MAKE-ARRAY", erro))
        return NULL;
    SefValor dimensao = car(argumentos);
    if (dimensao->tipo != SEF_TIPO_INTEIRO || dimensao->como.inteiro < 0 ||
        (uint64_t)dimensao->como.inteiro > SIZE_MAX) {
        sef_erro_definir(erro, 0, 0, "MAKE-ARRAY exige uma dimensao inteira nao negativa");
        return NULL;
    }
    SefValor inicial = runtime->nulo;
    argumentos = cdr(argumentos);
    while (argumentos != runtime->nulo) {
        SefValor chave = car(argumentos);
        argumentos = cdr(argumentos);
        if (argumentos == runtime->nulo) {
            sef_erro_definir(erro, 0, 0, "MAKE-ARRAY recebeu uma opcao sem valor");
            return NULL;
        }
        if (!sef_simbolo_tem_nome(chave, "INITIAL-ELEMENT")) {
            sef_erro_definir(erro, 0, 0, "opcao desconhecida para MAKE-ARRAY");
            return NULL;
        }
        inicial = car(argumentos);
        argumentos = cdr(argumentos);
    }
    return sef_vetor_novo(runtime, (size_t)dimensao->como.inteiro, inicial, erro);
}

static SefValor acessar_vetor(SefRuntime *runtime, SefValor argumentos, const char *nome,
                              SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, nome, erro))
        return NULL;
    SefValor vetor = car(argumentos);
    SefValor indice = car(cdr(argumentos));
    if (vetor->tipo != SEF_TIPO_VETOR) {
        sef_erro_definir(erro, 0, 0, "%s exige um vetor", nome);
        return NULL;
    }
    if (indice->tipo != SEF_TIPO_INTEIRO || indice->como.inteiro < 0 ||
        (uint64_t)indice->como.inteiro >= vetor->como.vetor.tamanho) {
        sef_erro_definir(erro, 0, 0, "indice fora dos limites em %s", nome);
        return NULL;
    }
    return vetor->como.vetor.itens[(size_t)indice->como.inteiro];
}

static SefValor primitiva_aref(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    return acessar_vetor(runtime, argumentos, "AREF", erro);
}

static SefValor primitiva_svref(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    return acessar_vetor(runtime, argumentos, "SVREF", erro);
}

static SefValor primitiva_vectorp(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "VECTORP", erro))
        return NULL;
    return car(argumentos)->tipo == SEF_TIPO_VETOR ? runtime->verdadeiro : runtime->nulo;
}

static SefValor primitiva_arrayp(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "ARRAYP", erro))
        return NULL;
    return car(argumentos)->tipo == SEF_TIPO_VETOR ? runtime->verdadeiro : runtime->nulo;
}

static bool obter_indice(SefValor valor, const char *nome, size_t *indice, SefErro *erro) {
    if (valor->tipo != SEF_TIPO_INTEIRO || valor->como.inteiro < 0 ||
        (uint64_t)valor->como.inteiro > SIZE_MAX) {
        sef_erro_definir(erro, 0, 0, "%s exige um indice inteiro nao negativo", nome);
        return false;
    }
    *indice = (size_t)valor->como.inteiro;
    return true;
}

static SefValor acessar_texto(SefRuntime *runtime, SefValor argumentos, const char *nome,
                              SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, nome, erro))
        return NULL;
    SefValor texto = car(argumentos);
    size_t indice;
    if (texto->tipo != SEF_TIPO_TEXTO) {
        sef_erro_definir(erro, 0, 0, "%s exige uma string", nome);
        return NULL;
    }
    if (!obter_indice(car(cdr(argumentos)), nome, &indice, erro))
        return NULL;
    return sef_texto_caractere_obter(runtime, texto, indice, erro);
}

static SefValor primitiva_char(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    return acessar_texto(runtime, argumentos, "CHAR", erro);
}

static SefValor primitiva_schar(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    return acessar_texto(runtime, argumentos, "SCHAR", erro);
}

static SefValor primitiva_elt(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, "ELT", erro))
        return NULL;
    SefValor sequencia = car(argumentos);
    size_t indice;
    if (!obter_indice(car(cdr(argumentos)), "ELT", &indice, erro))
        return NULL;
    if (sequencia->tipo == SEF_TIPO_TEXTO)
        return sef_texto_caractere_obter(runtime, sequencia, indice, erro);
    if (sequencia->tipo == SEF_TIPO_VETOR) {
        if (indice >= sequencia->como.vetor.tamanho) {
            sef_erro_definir(erro, 0, 0, "indice fora dos limites em ELT");
            return NULL;
        }
        return sequencia->como.vetor.itens[indice];
    }
    SefValor cursor = sequencia;
    for (size_t i = 0; i < indice; i++) {
        if (cursor == runtime->nulo || cursor->tipo != SEF_TIPO_PAR) {
            sef_erro_definir(erro, 0, 0, "indice fora dos limites ou lista impropria em ELT");
            return NULL;
        }
        cursor = cdr(cursor);
    }
    if (cursor == runtime->nulo || cursor->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "ELT exige uma sequencia e indice valido");
        return NULL;
    }
    return car(cursor);
}

static SefValor primitiva_characterp(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "CHARACTERP", erro))
        return NULL;
    return car(argumentos)->tipo == SEF_TIPO_CARACTERE ? runtime->verdadeiro : runtime->nulo;
}

static SefValor primitiva_stringp(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "STRINGP", erro))
        return NULL;
    return car(argumentos)->tipo == SEF_TIPO_TEXTO ? runtime->verdadeiro : runtime->nulo;
}

static SefValor primitiva_char_code(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "CHAR-CODE", erro))
        return NULL;
    SefValor caractere = car(argumentos);
    if (caractere->tipo != SEF_TIPO_CARACTERE) {
        sef_erro_definir(erro, 0, 0, "CHAR-CODE exige um caractere");
        return NULL;
    }
    return sef_inteiro_novo(runtime, (int64_t)caractere->como.caractere, erro);
}

static SefValor primitiva_code_char(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "CODE-CHAR", erro))
        return NULL;
    SefValor codigo = car(argumentos);
    if (codigo->tipo != SEF_TIPO_INTEIRO || codigo->como.inteiro < 0 ||
        codigo->como.inteiro > 0x10ffffll ||
        (codigo->como.inteiro >= 0xd800ll && codigo->como.inteiro <= 0xdfffll)) {
        sef_erro_definir(erro, 0, 0, "CODE-CHAR exige um valor escalar Unicode");
        return NULL;
    }
    return sef_caractere_novo(runtime, (uint32_t)codigo->como.inteiro, erro);
}

typedef bool (*ComparadorCaractere)(uint32_t, uint32_t);
static bool caractere_menor(uint32_t a, uint32_t b) { return a < b; }
static bool caractere_maior(uint32_t a, uint32_t b) { return a > b; }
static bool caractere_menor_igual(uint32_t a, uint32_t b) { return a <= b; }
static bool caractere_maior_igual(uint32_t a, uint32_t b) { return a >= b; }
static bool caractere_igual(uint32_t a, uint32_t b) { return a == b; }
static bool caractere_diferente(uint32_t a, uint32_t b) { return a != b; }

static SefValor comparar_caracteres(SefRuntime *runtime, SefValor argumentos,
                                    ComparadorCaractere comparador, const char *nome,
                                    bool todos_pares, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, (size_t)-1, nome, erro))
        return NULL;
    for (SefValor cursor = argumentos; cursor != runtime->nulo; cursor = cdr(cursor)) {
        SefValor atual = car(cursor);
        if (atual->tipo != SEF_TIPO_CARACTERE) {
            sef_erro_definir(erro, 0, 0, "%s aceita somente caracteres", nome);
            return NULL;
        }
        SefValor proximos = cdr(cursor);
        if (!todos_pares && proximos != runtime->nulo) {
            SefValor proximo = car(proximos);
            if (proximo->tipo != SEF_TIPO_CARACTERE) {
                sef_erro_definir(erro, 0, 0, "%s aceita somente caracteres", nome);
                return NULL;
            }
            if (!comparador(atual->como.caractere, proximo->como.caractere))
                return runtime->nulo;
        } else if (todos_pares) {
            for (; proximos != runtime->nulo; proximos = cdr(proximos)) {
                SefValor proximo = car(proximos);
                if (proximo->tipo != SEF_TIPO_CARACTERE) {
                    sef_erro_definir(erro, 0, 0, "%s aceita somente caracteres", nome);
                    return NULL;
                }
                if (!comparador(atual->como.caractere, proximo->como.caractere))
                    return runtime->nulo;
            }
        }
    }
    return runtime->verdadeiro;
}

#define DEFINIR_COMPARADOR_CARACTERE(nome_c, nome_lisp, funcao, pares)                             \
    static SefValor nome_c(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {              \
        return comparar_caracteres(runtime, argumentos, funcao, nome_lisp, pares, erro);           \
    }

DEFINIR_COMPARADOR_CARACTERE(primitiva_char_equal, "CHAR=", caractere_igual, false)
DEFINIR_COMPARADOR_CARACTERE(primitiva_char_not_equal, "CHAR/=", caractere_diferente, true)
DEFINIR_COMPARADOR_CARACTERE(primitiva_char_less, "CHAR<", caractere_menor, false)
DEFINIR_COMPARADOR_CARACTERE(primitiva_char_greater, "CHAR>", caractere_maior, false)
DEFINIR_COMPARADOR_CARACTERE(primitiva_char_not_greater, "CHAR<=", caractere_menor_igual, false)
DEFINIR_COMPARADOR_CARACTERE(primitiva_char_not_less, "CHAR>=", caractere_maior_igual, false)

static SefValor primitiva_eq(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, "EQ", erro))
        return NULL;
    SefValor a = car(argumentos);
    SefValor b = car(cdr(argumentos));
    bool iguais = a == b || (a->tipo == SEF_TIPO_INTEIRO && b->tipo == SEF_TIPO_INTEIRO &&
                             a->como.inteiro == b->como.inteiro);
    return iguais ? runtime->verdadeiro : runtime->nulo;
}

static bool valores_eql(SefValor a, SefValor b) {
    if (a == b)
        return true;
    if (a->tipo != b->tipo)
        return false;
    if (a->tipo == SEF_TIPO_INTEIRO)
        return a->como.inteiro == b->como.inteiro;
    if (a->tipo == SEF_TIPO_REAL)
        return a->como.real == b->como.real;
    if (a->tipo == SEF_TIPO_CARACTERE)
        return a->como.caractere == b->como.caractere;
    return false;
}

static bool valores_equal(SefRuntime *runtime, SefValor a, SefValor b, unsigned int profundidade) {
    if (valores_eql(a, b))
        return true;
    if (profundidade > 512 || a->tipo != b->tipo)
        return false;
    if (a->tipo == SEF_TIPO_TEXTO)
        return a->como.texto.tamanho == b->como.texto.tamanho &&
               memcmp(a->como.texto.dados, b->como.texto.dados, a->como.texto.tamanho) == 0;
    if (a->tipo == SEF_TIPO_PAR)
        return valores_equal(runtime, car(a), car(b), profundidade + 1) &&
               valores_equal(runtime, cdr(a), cdr(b), profundidade + 1);
    (void)runtime;
    return false;
}

static SefValor primitiva_eql(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, "EQL", erro))
        return NULL;
    return valores_eql(car(argumentos), car(cdr(argumentos))) ? runtime->verdadeiro : runtime->nulo;
}

static SefValor primitiva_equal(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, "EQUAL", erro))
        return NULL;
    return valores_equal(runtime, car(argumentos), car(cdr(argumentos)), 0) ? runtime->verdadeiro
                                                                            : runtime->nulo;
}

static SefValor primitiva_atom(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "ATOM", erro))
        return NULL;
    return car(argumentos)->tipo == SEF_TIPO_PAR ? runtime->nulo : runtime->verdadeiro;
}

static SefValor primitiva_not(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "NOT", erro))
        return NULL;
    return car(argumentos) == runtime->nulo ? runtime->verdadeiro : runtime->nulo;
}

static SefValor primitiva_length(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "LENGTH", erro))
        return NULL;
    SefValor valor = car(argumentos);
    if (valor->tipo == SEF_TIPO_TEXTO) {
        bool valido = false;
        size_t tamanho =
            sef_utf8_quantidade(valor->como.texto.dados, valor->como.texto.tamanho, &valido);
        if (!valido) {
            sef_erro_definir(erro, 0, 0, "LENGTH recebeu string com UTF-8 invalido");
            return NULL;
        }
        return sef_inteiro_novo(runtime, (int64_t)tamanho, erro);
    }
    if (valor->tipo == SEF_TIPO_VETOR)
        return sef_inteiro_novo(runtime, (int64_t)valor->como.vetor.tamanho, erro);
    bool propria = false;
    size_t tamanho = sef_lista_tamanho(runtime, valor, &propria);
    if (!propria) {
        sef_erro_definir(erro, 0, 0, "LENGTH exige texto, vetor ou lista propria");
        return NULL;
    }
    return sef_inteiro_novo(runtime, (int64_t)tamanho, erro);
}

static SefValor exigir_stream_aberto(SefValor valor, const char *operacao, SefErro *erro) {
    if (valor->tipo != SEF_TIPO_STREAM) {
        sef_erro_definir(erro, 0, 0, "%s exige um stream", operacao);
        return NULL;
    }
    if (valor->como.stream.fechado || valor->como.stream.arquivo == NULL) {
        sef_erro_definir(erro, 0, 0, "%s recebeu um stream fechado", operacao);
        return NULL;
    }
    return valor;
}

static bool escrever_stream(SefValor stream, const char *dados, size_t tamanho,
                            const char *operacao, SefErro *erro) {
    if (tamanho > 0 && fwrite(dados, 1, tamanho, stream->como.stream.arquivo) != tamanho) {
        sef_erro_definir(erro, 0, 0, "%s falhou: %s", operacao, strerror(errno));
        return false;
    }
    return true;
}

static SefValor primitiva_print(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "PRINT", erro))
        return NULL;
    SefValor valor = car(argumentos);
    SefValor stream =
        cdr(argumentos) == runtime->nulo ? runtime->saida_padrao : car(cdr(argumentos));
    if (exigir_stream_aberto(stream, "PRINT", erro) == NULL)
        return NULL;
    char *texto = sef_valor_para_texto(runtime, valor, true, erro);
    if (texto == NULL)
        return NULL;
    bool sucesso = escrever_stream(stream, texto, strlen(texto), "PRINT", erro) &&
                   escrever_stream(stream, "\n", 1, "PRINT", erro);
    sef_texto_liberar(texto);
    return sucesso ? valor : NULL;
}

static SefValor primitiva_streamp(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "STREAMP", erro))
        return NULL;
    return car(argumentos)->tipo == SEF_TIPO_STREAM ? runtime->verdadeiro : runtime->nulo;
}

static SefValor primitiva_open_shared_library(SefRuntime *runtime, SefValor argumentos,
                                              SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "OPEN-SHARED-LIBRARY", erro))
        return NULL;
    SefValor caminho = car(argumentos);
    if (caminho->tipo != SEF_TIPO_TEXTO || caminho->como.texto.tamanho == 0 ||
        memchr(caminho->como.texto.dados, '\0', caminho->como.texto.tamanho) != NULL) {
        sef_erro_definir(erro, 0, 0, "OPEN-SHARED-LIBRARY exige um caminho como string");
        return NULL;
    }
    return sef_biblioteca_nova(runtime, caminho->como.texto.dados, erro);
}

static SefValor primitiva_close_shared_library(SefRuntime *runtime, SefValor argumentos,
                                               SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "CLOSE-SHARED-LIBRARY", erro))
        return NULL;
    return sef_biblioteca_fechar(car(argumentos), erro) ? runtime->verdadeiro : NULL;
}

static SefValor primitiva_shared_library_p(SefRuntime *runtime, SefValor argumentos,
                                           SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "SHARED-LIBRARY-P", erro))
        return NULL;
    return car(argumentos)->tipo == SEF_TIPO_BIBLIOTECA ? runtime->verdadeiro : runtime->nulo;
}

static SefValor primitiva_shared_library_open_p(SefRuntime *runtime, SefValor argumentos,
                                                SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "SHARED-LIBRARY-OPEN-P", erro))
        return NULL;
    SefValor biblioteca = car(argumentos);
    return biblioteca->tipo == SEF_TIPO_BIBLIOTECA && !biblioteca->como.biblioteca.fechada
               ? runtime->verdadeiro
               : runtime->nulo;
}

static SefValor primitiva_open(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, (size_t)-1, "OPEN", erro))
        return NULL;
    SefValor caminho = car(argumentos);
    if (caminho->tipo != SEF_TIPO_TEXTO) {
        sef_erro_definir(erro, 0, 0, "OPEN exige um caminho textual");
        return NULL;
    }

    const char *direcao = "INPUT";
    const char *se_existe = "SUPERSEDE";
    argumentos = cdr(argumentos);
    while (argumentos != runtime->nulo) {
        SefValor chave = car(argumentos);
        argumentos = cdr(argumentos);
        if (argumentos == runtime->nulo) {
            sef_erro_definir(erro, 0, 0, "OPEN recebeu uma opcao sem valor");
            return NULL;
        }
        SefValor valor = car(argumentos);
        argumentos = cdr(argumentos);
        if (sef_simbolo_tem_nome(chave, "DIRECTION")) {
            if (sef_simbolo_tem_nome(valor, "INPUT"))
                direcao = "INPUT";
            else if (sef_simbolo_tem_nome(valor, "OUTPUT"))
                direcao = "OUTPUT";
            else if (sef_simbolo_tem_nome(valor, "IO"))
                direcao = "IO";
            else {
                sef_erro_definir(erro, 0, 0, ":DIRECTION deve ser :INPUT, :OUTPUT ou :IO");
                return NULL;
            }
        } else if (sef_simbolo_tem_nome(chave, "IF-EXISTS")) {
            if (sef_simbolo_tem_nome(valor, "SUPERSEDE"))
                se_existe = "SUPERSEDE";
            else if (sef_simbolo_tem_nome(valor, "APPEND"))
                se_existe = "APPEND";
            else if (sef_simbolo_tem_nome(valor, "ERROR"))
                se_existe = "ERROR";
            else {
                sef_erro_definir(erro, 0, 0, ":IF-EXISTS deve ser :SUPERSEDE, :APPEND ou :ERROR");
                return NULL;
            }
        } else {
            sef_erro_definir(erro, 0, 0, "opcao desconhecida para OPEN");
            return NULL;
        }
    }

    const char *modo = "rb";
    if (strcmp(direcao, "OUTPUT") == 0)
        modo = strcmp(se_existe, "APPEND") == 0 ? "ab" : "wb";
    else if (strcmp(direcao, "IO") == 0)
        modo = strcmp(se_existe, "APPEND") == 0      ? "a+b"
               : strcmp(se_existe, "SUPERSEDE") == 0 ? "w+b"
                                                     : "r+b";

    if (strcmp(direcao, "INPUT") != 0 && strcmp(se_existe, "ERROR") == 0) {
        FILE *existente = fopen(caminho->como.texto.dados, "rb");
        if (existente != NULL) {
            fclose(existente);
            sef_erro_definir(erro, 0, 0, "OPEN nao substituiu o arquivo existente '%s'",
                             caminho->como.texto.dados);
            return NULL;
        }
    }
    FILE *arquivo = fopen(caminho->como.texto.dados, modo);
    if (arquivo == NULL) {
        sef_erro_definir(erro, 0, 0, "nao foi possivel abrir '%s': %s", caminho->como.texto.dados,
                         strerror(errno));
        return NULL;
    }
    SefValor stream = sef_stream_novo(runtime, arquivo, caminho->como.texto.dados, true, 0, erro);
    if (stream == NULL)
        fclose(arquivo);
    return stream;
}

static SefValor primitiva_close(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "CLOSE", erro))
        return NULL;
    SefValor stream = car(argumentos);
    if (stream->tipo != SEF_TIPO_STREAM) {
        sef_erro_definir(erro, 0, 0, "CLOSE exige um stream");
        return NULL;
    }
    if (stream->como.stream.fechado)
        return runtime->verdadeiro;
    int resultado = 0;
    if (stream->como.stream.possui_arquivo && stream->como.stream.arquivo != NULL)
        resultado = fclose(stream->como.stream.arquivo);
    stream->como.stream.arquivo = NULL;
    stream->como.stream.fechado = true;
    if (resultado != 0) {
        sef_erro_definir(erro, 0, 0, "CLOSE falhou: %s", strerror(errno));
        return NULL;
    }
    return runtime->verdadeiro;
}

static SefValor primitiva_write_string(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "WRITE-STRING", erro))
        return NULL;
    SefValor texto = car(argumentos);
    if (texto->tipo != SEF_TIPO_TEXTO) {
        sef_erro_definir(erro, 0, 0, "WRITE-STRING exige um texto");
        return NULL;
    }
    SefValor stream =
        cdr(argumentos) == runtime->nulo ? runtime->saida_padrao : car(cdr(argumentos));
    if (exigir_stream_aberto(stream, "WRITE-STRING", erro) == NULL ||
        !escrever_stream(stream, texto->como.texto.dados, texto->como.texto.tamanho, "WRITE-STRING",
                         erro))
        return NULL;
    return texto;
}

static SefValor primitiva_read_line(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 0, 1, "READ-LINE", erro))
        return NULL;
    SefValor stream = argumentos == runtime->nulo ? runtime->entrada_padrao : car(argumentos);
    if (exigir_stream_aberto(stream, "READ-LINE", erro) == NULL)
        return NULL;

    size_t tamanho = 0, capacidade = 128;
    char *linha = malloc(capacidade);
    if (linha == NULL) {
        sef_erro_definir(erro, 0, 0, "memoria insuficiente para READ-LINE");
        return NULL;
    }
    int caractere;
    while ((caractere = fgetc(stream->como.stream.arquivo)) != EOF && caractere != '\n') {
        if (tamanho + 1 >= capacidade) {
            size_t nova_capacidade = capacidade * 2;
            char *nova_linha = realloc(linha, nova_capacidade);
            if (nova_linha == NULL) {
                free(linha);
                sef_erro_definir(erro, 0, 0, "memoria insuficiente para READ-LINE");
                return NULL;
            }
            linha = nova_linha;
            capacidade = nova_capacidade;
        }
        linha[tamanho++] = (char)caractere;
    }
    if (caractere == EOF && ferror(stream->como.stream.arquivo)) {
        free(linha);
        sef_erro_definir(erro, 0, 0, "READ-LINE falhou: %s", strerror(errno));
        return NULL;
    }
    if (caractere == EOF && tamanho == 0) {
        free(linha);
        return runtime->nulo;
    }
    if (tamanho > 0 && linha[tamanho - 1] == '\r')
        tamanho--;
    SefValor resultado = sef_texto_novo(runtime, linha, tamanho, erro);
    free(linha);
    return resultado;
}

static SefValor primitiva_terpri(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 0, 1, "TERPRI", erro))
        return NULL;
    SefValor stream = argumentos == runtime->nulo ? runtime->saida_padrao : car(argumentos);
    if (exigir_stream_aberto(stream, "TERPRI", erro) == NULL ||
        !escrever_stream(stream, "\n", 1, "TERPRI", erro))
        return NULL;
    return runtime->nulo;
}

static SefValor primitiva_finish_output(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 0, 1, "FINISH-OUTPUT", erro))
        return NULL;
    SefValor stream = argumentos == runtime->nulo ? runtime->saida_padrao : car(argumentos);
    if (exigir_stream_aberto(stream, "FINISH-OUTPUT", erro) == NULL)
        return NULL;
    if (fflush(stream->como.stream.arquivo) != 0) {
        sef_erro_definir(erro, 0, 0, "FINISH-OUTPUT falhou: %s", strerror(errno));
        return NULL;
    }
    return runtime->nulo;
}

static SefValor primitiva_type_of(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "TYPE-OF", erro))
        return NULL;
    static const char *nomes[] = {"NULL",
                                  "INTEGER",
                                  "FLOAT",
                                  "STRING",
                                  "SYMBOL",
                                  "CONS",
                                  "COMPILED-FUNCTION",
                                  "FUNCTION",
                                  "SEFIRAH::ENVIRONMENT",
                                  NULL,
                                  "PACKAGE",
                                  "STREAM",
                                  "SEFIRAH::SHARED-LIBRARY",
                                  "VECTOR",
                                  "CHARACTER"};
    SefValor valor = car(argumentos);
    if (valor->tipo == SEF_TIPO_CONDICAO)
        return valor->como.condicao.classe;
    if (valor->tipo == SEF_TIPO_FUNCAO && valor->como.funcao.compilada_i64 != NULL)
        return sef_simbolo_internar(runtime, "COMPILED-FUNCTION", 17, erro);
    const char *nome = nomes[valor->tipo];
    return sef_simbolo_internar(runtime, nome, strlen(nome), erro);
}

static SefValor primitiva_funcall(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (argumentos == runtime->nulo) {
        sef_erro_definir(erro, 0, 0, "FUNCALL exige uma funcao");
        return NULL;
    }
    return sef_aplicar(runtime, car(argumentos), cdr(argumentos), erro);
}

static SefValor primitiva_apply(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, "APPLY", erro))
        return NULL;
    SefValor lista = car(cdr(argumentos));
    if (!sef_e_lista_propria(runtime, lista)) {
        sef_erro_definir(erro, 0, 0, "ultimo argumento de APPLY deve ser lista");
        return NULL;
    }
    return sef_aplicar(runtime, car(argumentos), lista, erro);
}

static SefValor primitiva_contar_objetos(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 0, 0, "SEFIRAH::OBJECT-COUNT", erro)) {
        return NULL;
    }
    return sef_inteiro_novo(runtime, (int64_t)runtime->quantidade_objetos, erro);
}

static SefValor primitiva_boundp(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "BOUNDP", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    SefValor ignorado;
    if (simbolo->tipo != SEF_TIPO_SIMBOLO) {
        sef_erro_definir(erro, 0, 0, "BOUNDP exige um simbolo");
        return NULL;
    }
    return sef_ambiente_obter(runtime->ambiente_global, simbolo, &ignorado) ? runtime->verdadeiro
                                                                            : runtime->nulo;
}

static SefValor primitiva_fboundp(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "FBOUNDP", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    SefValor ignorado;
    if (simbolo->tipo != SEF_TIPO_SIMBOLO) {
        sef_erro_definir(erro, 0, 0, "FBOUNDP exige um simbolo");
        return NULL;
    }
    return sef_ambiente_obter_funcao(runtime->ambiente_global, simbolo, &ignorado)
               ? runtime->verdadeiro
               : runtime->nulo;
}

static SefValor primitiva_symbol_value(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "SYMBOL-VALUE", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    SefValor valor;
    if (simbolo->tipo != SEF_TIPO_SIMBOLO ||
        !sef_ambiente_obter(runtime->ambiente_global, simbolo, &valor)) {
        sef_erro_definir(erro, 0, 0, "simbolo nao possui valor global");
        return NULL;
    }
    return valor;
}

static SefValor primitiva_symbol_function(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "SYMBOL-FUNCTION", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    SefValor valor;
    if (simbolo->tipo != SEF_TIPO_SIMBOLO ||
        !sef_ambiente_obter_funcao(runtime->ambiente_global, simbolo, &valor)) {
        sef_erro_definir(erro, 0, 0, "simbolo nao possui funcao global");
        return NULL;
    }
    return valor;
}

static SefValor primitiva_set(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, "SET", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    SefValor valor = car(cdr(argumentos));
    if (simbolo->tipo != SEF_TIPO_SIMBOLO) {
        sef_erro_definir(erro, 0, 0, "SET exige um simbolo");
        return NULL;
    }
    return sef_ambiente_definir(runtime, runtime->ambiente_global, simbolo, valor, erro) ? valor
                                                                                         : NULL;
}

static SefValor primitiva_functionp(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "FUNCTIONP", erro))
        return NULL;
    SefTipo tipo = car(argumentos)->tipo;
    return tipo == SEF_TIPO_FUNCAO || tipo == SEF_TIPO_NATIVA ? runtime->verdadeiro : runtime->nulo;
}

static SefValor primitiva_compile(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "COMPILE", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    return sef_funcao_compilada_instalar_i64(runtime, simbolo, erro) ? simbolo : NULL;
}

static SefValor primitiva_compile_external_i64(SefRuntime *runtime, SefValor argumentos,
                                               SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, "COMPILE-EXTERNAL-I64", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    SefValor biblioteca = car(cdr(argumentos));
    if (biblioteca->tipo == SEF_TIPO_BIBLIOTECA)
        return sef_funcao_compilada_instalar_objeto_biblioteca_i64(runtime, simbolo, biblioteca,
                                                                   erro)
                   ? simbolo
                   : NULL;
    if (biblioteca->tipo != SEF_TIPO_TEXTO || biblioteca->como.texto.tamanho == 0 ||
        memchr(biblioteca->como.texto.dados, '\0', biblioteca->como.texto.tamanho) != NULL) {
        sef_erro_definir(erro, 0, 0,
                         "COMPILE-EXTERNAL-I64 exige caminho ou biblioteca compartilhada");
        return NULL;
    }
    return sef_funcao_compilada_instalar_biblioteca_i64(runtime, simbolo,
                                                        biblioteca->como.texto.dados, erro)
               ? simbolo
               : NULL;
}

static SefValor primitiva_compiled_function_p(SefRuntime *runtime, SefValor argumentos,
                                              SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "COMPILED-FUNCTION-P", erro))
        return NULL;
    SefValor valor = car(argumentos);
    bool compilada = valor->tipo == SEF_TIPO_NATIVA ||
                     (valor->tipo == SEF_TIPO_FUNCAO && valor->como.funcao.compilada_i64 != NULL);
    return compilada ? runtime->verdadeiro : runtime->nulo;
}

static const char *nome_designador(SefValor valor, size_t *tamanho) {
    if (valor->tipo == SEF_TIPO_TEXTO) {
        *tamanho = valor->como.texto.tamanho;
        return valor->como.texto.dados;
    }
    if (valor->tipo == SEF_TIPO_SIMBOLO) {
        *tamanho = valor->como.simbolo.tamanho;
        return valor->como.simbolo.nome;
    }
    if (valor->tipo == SEF_TIPO_PACOTE) {
        *tamanho = strlen(valor->como.pacote.nome);
        return valor->como.pacote.nome;
    }
    return NULL;
}

static SefValor pacote_designador(SefRuntime *runtime, SefValor valor, SefErro *erro) {
    if (valor->tipo == SEF_TIPO_PACOTE)
        return valor;
    size_t tamanho = 0;
    const char *nome = nome_designador(valor, &tamanho);
    SefValor pacote = nome == NULL ? NULL : sef_pacote_encontrar(runtime, nome, tamanho);
    if (pacote == NULL)
        sef_erro_definir(erro, 0, 0, "designador nao nomeia um pacote existente");
    return pacote;
}

static SefValor primitiva_make_package(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "MAKE-PACKAGE", erro))
        return NULL;
    size_t tamanho = 0;
    const char *nome = nome_designador(car(argumentos), &tamanho);
    if (nome == NULL || tamanho == 0) {
        sef_erro_definir(erro, 0, 0, "MAKE-PACKAGE exige nome textual");
        return NULL;
    }
    char *copia = malloc(tamanho + 1);
    if (copia == NULL) {
        sef_erro_definir(erro, 0, 0, "memoria insuficiente para nome de pacote");
        return NULL;
    }
    memcpy(copia, nome, tamanho);
    copia[tamanho] = '\0';
    SefValor pacote = sef_pacote_novo(runtime, copia, erro);
    free(copia);
    if (pacote != NULL && !sef_pacote_usar(runtime, pacote, runtime->pacote_common_lisp, erro))
        return NULL;
    return pacote;
}

static SefValor primitiva_find_package(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "FIND-PACKAGE", erro))
        return NULL;
    size_t tamanho = 0;
    const char *nome = nome_designador(car(argumentos), &tamanho);
    if (nome == NULL)
        return runtime->nulo;
    SefValor pacote = sef_pacote_encontrar(runtime, nome, tamanho);
    return pacote == NULL ? runtime->nulo : pacote;
}

static SefValor primitiva_package_name(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "PACKAGE-NAME", erro))
        return NULL;
    SefValor pacote = pacote_designador(runtime, car(argumentos), erro);
    return pacote == NULL ? NULL
                          : sef_texto_novo(runtime, pacote->como.pacote.nome,
                                           strlen(pacote->como.pacote.nome), erro);
}

static SefValor primitiva_packagep(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "PACKAGEP", erro))
        return NULL;
    return car(argumentos)->tipo == SEF_TIPO_PACOTE ? runtime->verdadeiro : runtime->nulo;
}

static SefValor primitiva_use_package(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "USE-PACKAGE", erro))
        return NULL;
    SefValor usado = pacote_designador(runtime, car(argumentos), erro);
    SefValor destino = cdr(argumentos) == runtime->nulo
                           ? runtime->pacote_atual
                           : pacote_designador(runtime, car(cdr(argumentos)), erro);
    return usado != NULL && destino != NULL && sef_pacote_usar(runtime, destino, usado, erro)
               ? runtime->verdadeiro
               : NULL;
}

static SefValor primitiva_export(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "EXPORT", erro))
        return NULL;
    SefValor pacote = cdr(argumentos) == runtime->nulo
                          ? runtime->pacote_atual
                          : pacote_designador(runtime, car(cdr(argumentos)), erro);
    if (pacote == NULL)
        return NULL;
    SefValor simbolos = car(argumentos);
    if (simbolos->tipo == SEF_TIPO_SIMBOLO)
        return sef_pacote_exportar(runtime, pacote, simbolos, erro) ? runtime->verdadeiro : NULL;
    if (!sef_e_lista_propria(runtime, simbolos)) {
        sef_erro_definir(erro, 0, 0, "EXPORT exige simbolo ou lista de simbolos");
        return NULL;
    }
    while (simbolos != runtime->nulo) {
        if (!sef_pacote_exportar(runtime, pacote, car(simbolos), erro))
            return NULL;
        simbolos = cdr(simbolos);
    }
    return runtime->verdadeiro;
}

static SefValor primitiva_intern(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "INTERN", erro))
        return NULL;
    size_t tamanho = 0;
    const char *nome = nome_designador(car(argumentos), &tamanho);
    SefValor pacote = cdr(argumentos) == runtime->nulo
                          ? runtime->pacote_atual
                          : pacote_designador(runtime, car(cdr(argumentos)), erro);
    if (nome == NULL || pacote == NULL) {
        if (!erro->ocorreu)
            sef_erro_definir(erro, 0, 0, "INTERN exige nome textual");
        return NULL;
    }
    return sef_simbolo_internar_em(runtime, pacote, nome, tamanho, erro);
}

static SefValor primitiva_find_symbol(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "FIND-SYMBOL", erro))
        return NULL;
    size_t tamanho = 0;
    const char *nome = nome_designador(car(argumentos), &tamanho);
    SefValor pacote = cdr(argumentos) == runtime->nulo
                          ? runtime->pacote_atual
                          : pacote_designador(runtime, car(cdr(argumentos)), erro);
    if (nome == NULL || pacote == NULL)
        return NULL;
    SefValor simbolo = sef_pacote_localizar_simbolo(pacote, nome, tamanho, true);
    return simbolo == NULL ? runtime->nulo : simbolo;
}

static SefValor primitiva_symbol_name(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "SYMBOL-NAME", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    if (simbolo->tipo != SEF_TIPO_SIMBOLO) {
        sef_erro_definir(erro, 0, 0, "SYMBOL-NAME exige simbolo");
        return NULL;
    }
    return sef_texto_novo(runtime, simbolo->como.simbolo.nome, simbolo->como.simbolo.tamanho, erro);
}

static SefValor primitiva_symbol_package(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "SYMBOL-PACKAGE", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    if (simbolo->tipo != SEF_TIPO_SIMBOLO) {
        sef_erro_definir(erro, 0, 0, "SYMBOL-PACKAGE exige simbolo");
        return NULL;
    }
    return simbolo->como.simbolo.pacote == NULL ? runtime->nulo : simbolo->como.simbolo.pacote;
}

static SefValor primitiva_list_all_packages(SefRuntime *runtime, SefValor argumentos,
                                            SefErro *erro) {
    if (!quantidade(runtime, argumentos, 0, 0, "LIST-ALL-PACKAGES", erro))
        return NULL;
    SefValor resultado = runtime->nulo;
    for (size_t i = runtime->quantidade_pacotes; i > 0; i--) {
        resultado = sef_par_novo(runtime, runtime->pacotes[i - 1], resultado, erro);
        if (resultado == NULL)
            return NULL;
    }
    return resultado;
}

static SefValor primitiva_error(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "ERROR", erro))
        return NULL;
    SefValor designador = car(argumentos);
    if (designador->tipo == SEF_TIPO_TEXTO) {
        sef_erro_definir(erro, 0, 0, "%s", designador->como.texto.dados);
    } else if (designador->tipo == SEF_TIPO_CONDICAO) {
        sef_erro_definir(erro, 0, 0, "%s", designador->como.condicao.mensagem->como.texto.dados);
    } else {
        sef_erro_definir(erro, 0, 0, "ERROR exige texto ou condicao");
    }
    return NULL;
}

static bool instalar(SefRuntime *runtime, const char *nome, SefFuncaoNativa funcao, SefErro *erro) {
    const char *separador = strstr(nome, "::");
    SefValor pacote = runtime->pacote_common_lisp;
    const char *nome_simbolo = nome;
    bool exportar = true;
    if (separador != NULL) {
        pacote = sef_pacote_encontrar(runtime, nome, (size_t)(separador - nome));
        nome_simbolo = separador + 2;
        exportar = false;
    }
    if (pacote == NULL || nome_simbolo[0] == '\0') {
        sef_erro_definir(erro, 0, 0, "pacote de primitiva interna nao existe");
        return false;
    }
    SefValor simbolo =
        sef_simbolo_internar_em(runtime, pacote, nome_simbolo, strlen(nome_simbolo), erro);
    SefValor valor = simbolo == NULL ? NULL : sef_nativa_nova(runtime, nome, funcao, erro);
    return valor != NULL &&
           sef_ambiente_definir_funcao(runtime, runtime->ambiente_global, simbolo, valor, erro) &&
           (!exportar || sef_pacote_exportar(runtime, pacote, simbolo, erro));
}

static const struct {
    const char *nome;
    SefFuncaoNativa funcao;
} primitivas[] = {{"+", primitiva_somar},
                  {"-", primitiva_subtrair},
                  {"*", primitiva_multiplicar},
                  {"/", primitiva_dividir},
                  {"<", primitiva_menor},
                  {">", primitiva_maior},
                  {"=", primitiva_igual_numero},
                  {"<=", primitiva_menor_igual},
                  {">=", primitiva_maior_igual},
                  {"/=", primitiva_numeros_diferentes},
                  {"CONS", primitiva_cons},
                  {"CAR", primitiva_car},
                  {"CDR", primitiva_cdr},
                  {"LIST", primitiva_list},
                  {"VECTOR", primitiva_vector},
                  {"MAKE-ARRAY", primitiva_make_array},
                  {"AREF", primitiva_aref},
                  {"SVREF", primitiva_svref},
                  {"VECTORP", primitiva_vectorp},
                  {"ARRAYP", primitiva_arrayp},
                  {"CHAR", primitiva_char},
                  {"SCHAR", primitiva_schar},
                  {"ELT", primitiva_elt},
                  {"CHARACTERP", primitiva_characterp},
                  {"STRINGP", primitiva_stringp},
                  {"CHAR-CODE", primitiva_char_code},
                  {"CODE-CHAR", primitiva_code_char},
                  {"CHAR=", primitiva_char_equal},
                  {"CHAR/=", primitiva_char_not_equal},
                  {"CHAR<", primitiva_char_less},
                  {"CHAR>", primitiva_char_greater},
                  {"CHAR<=", primitiva_char_not_greater},
                  {"CHAR>=", primitiva_char_not_less},
                  {"EQ", primitiva_eq},
                  {"EQL", primitiva_eql},
                  {"EQUAL", primitiva_equal},
                  {"ATOM", primitiva_atom},
                  {"NOT", primitiva_not},
                  {"NULL", primitiva_not},
                  {"LENGTH", primitiva_length},
                  {"COPY-SEQ", sef_primitiva_copy_seq},
                  {"REVERSE", sef_primitiva_reverse},
                  {"SUBSEQ", sef_primitiva_subseq},
                  {"FILL", sef_primitiva_fill},
                  {"PRINT", primitiva_print},
                  {"STREAMP", primitiva_streamp},
                  {"OPEN-SHARED-LIBRARY", primitiva_open_shared_library},
                  {"CLOSE-SHARED-LIBRARY", primitiva_close_shared_library},
                  {"SHARED-LIBRARY-P", primitiva_shared_library_p},
                  {"SHARED-LIBRARY-OPEN-P", primitiva_shared_library_open_p},
                  {"OPEN", primitiva_open},
                  {"CLOSE", primitiva_close},
                  {"WRITE-STRING", primitiva_write_string},
                  {"READ-LINE", primitiva_read_line},
                  {"TERPRI", primitiva_terpri},
                  {"FINISH-OUTPUT", primitiva_finish_output},
                  {"TYPE-OF", primitiva_type_of},
                  {"FUNCALL", primitiva_funcall},
                  {"APPLY", primitiva_apply},
                  {"BOUNDP", primitiva_boundp},
                  {"FBOUNDP", primitiva_fboundp},
                  {"SYMBOL-VALUE", primitiva_symbol_value},
                  {"SYMBOL-FUNCTION", primitiva_symbol_function},
                  {"SET", primitiva_set},
                  {"FUNCTIONP", primitiva_functionp},
                  {"COMPILE", primitiva_compile},
                  {"COMPILE-EXTERNAL-I64", primitiva_compile_external_i64},
                  {"COMPILED-FUNCTION-P", primitiva_compiled_function_p},
                  {"ERROR", primitiva_error},
                  {"MAKE-PACKAGE", primitiva_make_package},
                  {"FIND-PACKAGE", primitiva_find_package},
                  {"PACKAGE-NAME", primitiva_package_name},
                  {"PACKAGEP", primitiva_packagep},
                  {"USE-PACKAGE", primitiva_use_package},
                  {"EXPORT", primitiva_export},
                  {"INTERN", primitiva_intern},
                  {"FIND-SYMBOL", primitiva_find_symbol},
                  {"SYMBOL-NAME", primitiva_symbol_name},
                  {"SYMBOL-PACKAGE", primitiva_symbol_package},
                  {"LIST-ALL-PACKAGES", primitiva_list_all_packages},
                  {"SEFIRAH::OBJECT-COUNT", primitiva_contar_objetos}};

SefFuncaoNativa sef_primitiva_buscar(const char *nome) {
    for (size_t i = 0; i < sizeof(primitivas) / sizeof(primitivas[0]); i++) {
        if (strcmp(nome, primitivas[i].nome) == 0)
            return primitivas[i].funcao;
    }
    return NULL;
}

const char *sef_primitiva_nome(SefFuncaoNativa funcao) {
    for (size_t i = 0; i < sizeof(primitivas) / sizeof(primitivas[0]); i++) {
        if (funcao == primitivas[i].funcao)
            return primitivas[i].nome;
    }
    return NULL;
}

bool sef_primitivas_instalar(SefRuntime *runtime, SefErro *erro) {
    for (size_t i = 0; i < sizeof(primitivas) / sizeof(primitivas[0]); i++) {
        if (!instalar(runtime, primitivas[i].nome, primitivas[i].funcao, erro)) {
            return false;
        }
    }
    return true;
}
