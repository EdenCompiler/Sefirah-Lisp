#include "sefirah/interno.h"

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
        sef_erro_definir(erro, 0, 0, "%s expects %zu argument(s)", nome, minimo);
    } else {
        sef_erro_definir(erro, 0, 0, "%s expects between %zu and %zu arguments", nome, minimo,
                         maximo);
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
            sef_erro_definir(erro, 0, 0, "+ accepts only numbers");
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
            sef_erro_definir(erro, 0, 0, "* accepts only numbers");
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
        sef_erro_definir(erro, 0, 0, "- accepts only numbers");
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
            sef_erro_definir(erro, 0, 0, "- accepts only numbers");
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
        sef_erro_definir(erro, 0, 0, "/ accepts only numbers");
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
            sef_erro_definir(erro, 0, 0, "/ accepts only numbers");
            return NULL;
        }
        if (divisor == 0.0)
            goto divisao_zero;
        acumulado /= divisor;
        argumentos = cdr(argumentos);
    }
    return sef_real_novo(runtime, acumulado, erro);

divisao_zero:
    sef_erro_definir(erro, 0, 0, "division by zero");
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
    sef_erro_definir(erro, 0, 0, "%s accepts only numbers", nome);
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
            sef_erro_definir(erro, 0, 0, "/= accepts only numbers");
            return NULL;
        }
        for (SefValor restante = cdr(cursor); restante != runtime->nulo; restante = cdr(restante)) {
            double b;
            if (!numero(car(restante), &b, &ignorado)) {
                sef_erro_definir(erro, 0, 0, "/= accepts only numbers");
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
        sef_erro_definir(erro, 0, 0, "CAR requires a list");
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
        sef_erro_definir(erro, 0, 0, "CDR requires a list");
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
        sef_erro_definir(erro, 0, 0, "VECTOR received improper arguments");
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
        sef_erro_definir(erro, 0, 0, "MAKE-ARRAY requires a non-negative integer dimension");
        return NULL;
    }
    SefValor inicial = runtime->nulo;
    argumentos = cdr(argumentos);
    while (argumentos != runtime->nulo) {
        SefValor chave = car(argumentos);
        argumentos = cdr(argumentos);
        if (argumentos == runtime->nulo) {
            sef_erro_definir(erro, 0, 0, "MAKE-ARRAY received an option without a value");
            return NULL;
        }
        if (!sef_simbolo_tem_nome(chave, "INITIAL-ELEMENT")) {
            sef_erro_definir(erro, 0, 0, "unknown MAKE-ARRAY option");
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
        sef_erro_definir(erro, 0, 0, "%s requires a vector", nome);
        return NULL;
    }
    if (indice->tipo != SEF_TIPO_INTEIRO || indice->como.inteiro < 0 ||
        (uint64_t)indice->como.inteiro >= vetor->como.vetor.tamanho) {
        sef_erro_definir(erro, 0, 0, "index out of bounds in %s", nome);
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
        sef_erro_definir(erro, 0, 0, "%s requires a non-negative integer index", nome);
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
        sef_erro_definir(erro, 0, 0, "%s requires a string", nome);
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
            sef_erro_definir(erro, 0, 0, "index out of bounds in ELT");
            return NULL;
        }
        return sequencia->como.vetor.itens[indice];
    }
    SefValor cursor = sequencia;
    for (size_t i = 0; i < indice; i++) {
        if (cursor == runtime->nulo || cursor->tipo != SEF_TIPO_PAR) {
            sef_erro_definir(erro, 0, 0, "index out of bounds or improper list in ELT");
            return NULL;
        }
        cursor = cdr(cursor);
    }
    if (cursor == runtime->nulo || cursor->tipo != SEF_TIPO_PAR) {
        sef_erro_definir(erro, 0, 0, "ELT requires a sequence and valid index");
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
        sef_erro_definir(erro, 0, 0, "CHAR-CODE requires a character");
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
        sef_erro_definir(erro, 0, 0, "CODE-CHAR requires a Unicode scalar value");
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
            sef_erro_definir(erro, 0, 0, "%s accepts only characters", nome);
            return NULL;
        }
        SefValor proximos = cdr(cursor);
        if (!todos_pares && proximos != runtime->nulo) {
            SefValor proximo = car(proximos);
            if (proximo->tipo != SEF_TIPO_CARACTERE) {
                sef_erro_definir(erro, 0, 0, "%s accepts only characters", nome);
                return NULL;
            }
            if (!comparador(atual->como.caractere, proximo->como.caractere))
                return runtime->nulo;
        } else if (todos_pares) {
            for (; proximos != runtime->nulo; proximos = cdr(proximos)) {
                SefValor proximo = car(proximos);
                if (proximo->tipo != SEF_TIPO_CARACTERE) {
                    sef_erro_definir(erro, 0, 0, "%s accepts only characters", nome);
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

static bool valores_equal(SefRuntime *runtime, SefValor a, SefValor b, unsigned int profundidade) {
    if (sef_valores_eql(a, b))
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
    return sef_valores_eql(car(argumentos), car(cdr(argumentos))) ? runtime->verdadeiro
                                                                  : runtime->nulo;
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
            sef_erro_definir(erro, 0, 0, "LENGTH received a string with invalid UTF-8");
            return NULL;
        }
        return sef_inteiro_novo(runtime, (int64_t)tamanho, erro);
    }
    if (valor->tipo == SEF_TIPO_VETOR)
        return sef_inteiro_novo(runtime, (int64_t)valor->como.vetor.tamanho, erro);
    bool propria = false;
    size_t tamanho = sef_lista_tamanho(runtime, valor, &propria);
    if (!propria) {
        sef_erro_definir(erro, 0, 0, "LENGTH requires a string, vector, or proper list");
        return NULL;
    }
    return sef_inteiro_novo(runtime, (int64_t)tamanho, erro);
}

static SefValor exigir_stream_aberto(SefValor valor, const char *operacao, SefErro *erro) {
    if (valor->tipo != SEF_TIPO_STREAM) {
        sef_erro_definir(erro, 0, 0, "%s requires a stream", operacao);
        return NULL;
    }
    if (valor->como.stream.fechado || valor->como.stream.arquivo == NULL) {
        sef_erro_definir(erro, 0, 0, "%s received a closed stream", operacao);
        return NULL;
    }
    return valor;
}

static bool escrever_stream(SefValor stream, const char *dados, size_t tamanho,
                            const char *operacao, SefErro *erro) {
    if (tamanho > 0 && fwrite(dados, 1, tamanho, stream->como.stream.arquivo) != tamanho) {
        sef_erro_definir(erro, 0, 0, "%s failed: %s", operacao, strerror(errno));
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
        sef_erro_definir(erro, 0, 0, "OPEN-SHARED-LIBRARY requires a path string");
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
        sef_erro_definir(erro, 0, 0, "OPEN requires a path string");
        return NULL;
    }

    const char *direcao = "INPUT";
    const char *se_existe = "SUPERSEDE";
    argumentos = cdr(argumentos);
    while (argumentos != runtime->nulo) {
        SefValor chave = car(argumentos);
        argumentos = cdr(argumentos);
        if (argumentos == runtime->nulo) {
            sef_erro_definir(erro, 0, 0, "OPEN received an option without a value");
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
                sef_erro_definir(erro, 0, 0, ":DIRECTION must be :INPUT, :OUTPUT, or :IO");
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
                sef_erro_definir(erro, 0, 0, ":IF-EXISTS must be :SUPERSEDE, :APPEND, or :ERROR");
                return NULL;
            }
        } else {
            sef_erro_definir(erro, 0, 0, "unknown OPEN option");
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
            sef_erro_definir(erro, 0, 0, "OPEN did not replace the existing file '%s'",
                             caminho->como.texto.dados);
            return NULL;
        }
    }
    FILE *arquivo = fopen(caminho->como.texto.dados, modo);
    if (arquivo == NULL) {
        sef_erro_definir(erro, 0, 0, "could not open '%s': %s", caminho->como.texto.dados,
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
        sef_erro_definir(erro, 0, 0, "CLOSE requires a stream");
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
        sef_erro_definir(erro, 0, 0, "CLOSE failed: %s", strerror(errno));
        return NULL;
    }
    return runtime->verdadeiro;
}

static SefValor primitiva_write_string(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "WRITE-STRING", erro))
        return NULL;
    SefValor texto = car(argumentos);
    if (texto->tipo != SEF_TIPO_TEXTO) {
        sef_erro_definir(erro, 0, 0, "WRITE-STRING requires a string");
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
    if (!quantidade(runtime, argumentos, 0, 4, "READ-LINE", erro))
        return NULL;
    SefValor stream = argumentos == runtime->nulo ? runtime->entrada_padrao : car(argumentos);
    SefValor restantes = argumentos == runtime->nulo ? runtime->nulo : cdr(argumentos);
    bool erro_no_fim = restantes == runtime->nulo || car(restantes) != runtime->nulo;
    SefValor valor_no_fim = runtime->nulo;
    if (restantes != runtime->nulo) {
        restantes = cdr(restantes);
        if (restantes != runtime->nulo)
            valor_no_fim = car(restantes);
    }
    if (exigir_stream_aberto(stream, "READ-LINE", erro) == NULL)
        return NULL;

    size_t tamanho = 0, capacidade = 128;
    char *linha = malloc(capacidade);
    if (linha == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for READ-LINE");
        return NULL;
    }
    int caractere;
    while ((caractere = fgetc(stream->como.stream.arquivo)) != EOF && caractere != '\n') {
        if (tamanho + 1 >= capacidade) {
            size_t nova_capacidade = capacidade * 2;
            char *nova_linha = realloc(linha, nova_capacidade);
            if (nova_linha == NULL) {
                free(linha);
                sef_erro_definir(erro, 0, 0, "not enough memory for READ-LINE");
                return NULL;
            }
            linha = nova_linha;
            capacidade = nova_capacidade;
        }
        linha[tamanho++] = (char)caractere;
    }
    if (caractere == EOF && ferror(stream->como.stream.arquivo)) {
        free(linha);
        sef_erro_definir(erro, 0, 0, "READ-LINE failed: %s", strerror(errno));
        return NULL;
    }
    if (caractere == EOF && tamanho == 0) {
        free(linha);
        if (erro_no_fim) {
            sef_erro_definir(erro, 0, 0, "end of file in READ-LINE");
            return NULL;
        }
        SefValor valores[2] = {valor_no_fim, runtime->verdadeiro};
        return sef_valores_definir(runtime, valores, 2, erro) ? valor_no_fim : NULL;
    }
    if (tamanho > 0 && linha[tamanho - 1] == '\r')
        tamanho--;
    SefValor resultado = sef_texto_novo(runtime, linha, tamanho, erro);
    free(linha);
    if (resultado == NULL)
        return NULL;
    SefValor valores[2] = {resultado, caractere == EOF ? runtime->verdadeiro : runtime->nulo};
    return sef_valores_definir(runtime, valores, 2, erro) ? resultado : NULL;
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
        sef_erro_definir(erro, 0, 0, "FINISH-OUTPUT failed: %s", strerror(errno));
        return NULL;
    }
    return runtime->nulo;
}

static SefValor primitiva_type_of(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "TYPE-OF", erro))
        return NULL;
    SefValor valor = car(argumentos);
    if (valor->tipo == SEF_TIPO_CONDICAO)
        return valor->como.condicao.classe;
    const char *nome = sef_valor_nome_tipo(valor);
    return sef_simbolo_internar(runtime, nome, strlen(nome), erro);
}

static SefValor primitiva_funcall(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (argumentos == runtime->nulo) {
        sef_erro_definir(erro, 0, 0, "FUNCALL requires a function");
        return NULL;
    }
    return sef_aplicar(runtime, car(argumentos), cdr(argumentos), erro);
}

static SefValor primitiva_apply(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, "APPLY", erro))
        return NULL;
    SefValor lista = car(cdr(argumentos));
    if (!sef_e_lista_propria(runtime, lista)) {
        sef_erro_definir(erro, 0, 0, "final APPLY argument must be a list");
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
    if (!sef_valor_e_simbolo_logico(runtime, simbolo)) {
        sef_erro_definir(erro, 0, 0, "BOUNDP requires a symbol");
        return NULL;
    }
    if (sef_simbolo_e_constante(runtime, simbolo))
        return runtime->verdadeiro;
    return sef_ambiente_obter(runtime->ambiente_global, simbolo, &ignorado) ? runtime->verdadeiro
                                                                            : runtime->nulo;
}

static SefValor primitiva_fboundp(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "FBOUNDP", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    SefValor ignorado;
    if (!sef_valor_e_simbolo_logico(runtime, simbolo)) {
        sef_erro_definir(erro, 0, 0, "FBOUNDP requires a symbol");
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
    if (!sef_valor_e_simbolo_logico(runtime, simbolo)) {
        sef_erro_definir(erro, 0, 0, "SYMBOL-VALUE requires a symbol");
        return NULL;
    }
    if (sef_simbolo_e_constante(runtime, simbolo))
        return simbolo;
    if (!sef_ambiente_obter(runtime->ambiente_global, simbolo, &valor)) {
        sef_erro_definir(erro, 0, 0, "symbol has no global value");
        return NULL;
    }
    return valor;
}

static SefValor primitiva_symbol_function(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "SYMBOL-FUNCTION", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    SefValor valor;
    if (!sef_valor_e_simbolo_logico(runtime, simbolo)) {
        sef_erro_definir(erro, 0, 0, "SYMBOL-FUNCTION requires a symbol");
        return NULL;
    }
    if (!sef_ambiente_obter_funcao(runtime->ambiente_global, simbolo, &valor)) {
        sef_erro_definir(erro, 0, 0, "symbol has no global function");
        return NULL;
    }
    return valor;
}

static SefValor primitiva_fdefinition(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "FDEFINITION", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    SefValor valor;
    if (!sef_valor_e_simbolo_logico(runtime, simbolo)) {
        sef_erro_definir(erro, 0, 0, "FDEFINITION currently requires a symbol function name");
        return NULL;
    }
    if (!sef_ambiente_obter_funcao(runtime->ambiente_global, simbolo, &valor)) {
        sef_erro_definir(erro, 0, 0, "function name has no global definition");
        return NULL;
    }
    return valor;
}

static SefValor primitiva_makunbound(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "MAKUNBOUND", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    if (!sef_valor_e_simbolo_logico(runtime, simbolo)) {
        sef_erro_definir(erro, 0, 0, "MAKUNBOUND requires a symbol");
        return NULL;
    }
    if (sef_simbolo_e_constante(runtime, simbolo)) {
        sef_erro_definir(erro, 0, 0, "MAKUNBOUND cannot modify a constant symbol");
        return NULL;
    }
    sef_ambiente_remover(runtime->ambiente_global, simbolo);
    return simbolo;
}

static SefValor primitiva_fmakunbound(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "FMAKUNBOUND", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    if (!sef_valor_e_simbolo_logico(runtime, simbolo)) {
        sef_erro_definir(erro, 0, 0, "FMAKUNBOUND currently requires a symbol function name");
        return NULL;
    }
    sef_ambiente_remover_funcao(runtime->ambiente_global, simbolo);
    return simbolo;
}

static SefValor primitiva_set(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, "SET", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    SefValor valor = car(cdr(argumentos));
    if (!sef_valor_e_simbolo_logico(runtime, simbolo)) {
        sef_erro_definir(erro, 0, 0, "SET requires a symbol");
        return NULL;
    }
    if (sef_simbolo_e_constante(runtime, simbolo)) {
        sef_erro_definir(erro, 0, 0, "SET cannot modify a constant symbol");
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

static SefValor primitiva_symbolp(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "SYMBOLP", erro))
        return NULL;
    return sef_valor_e_simbolo_logico(runtime, car(argumentos)) ? runtime->verdadeiro
                                                                : runtime->nulo;
}

static SefValor primitiva_keywordp(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "KEYWORDP", erro))
        return NULL;
    SefValor valor = car(argumentos);
    return valor->tipo == SEF_TIPO_SIMBOLO && valor->como.simbolo.pacote == runtime->pacote_keyword
               ? runtime->verdadeiro
               : runtime->nulo;
}

static SefValor primitiva_constantp(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "CONSTANTP", erro))
        return NULL;
    if (cdr(argumentos) != runtime->nulo && car(cdr(argumentos)) != runtime->nulo) {
        sef_erro_definir(erro, 0, 0, "CONSTANTP currently accepts only the NIL environment");
        return NULL;
    }
    SefValor forma = car(argumentos);
    if (sef_simbolo_e_constante(runtime, forma))
        return runtime->verdadeiro;
    if (forma->tipo == SEF_TIPO_SIMBOLO)
        return runtime->nulo;
    if (forma->tipo == SEF_TIPO_PAR) {
        SefValor operador = car(forma);
        SefValor cauda = cdr(forma);
        bool quote = operador->tipo == SEF_TIPO_SIMBOLO &&
                     sef_simbolo_tem_nome(operador, "QUOTE") && cauda != runtime->nulo &&
                     cauda->tipo == SEF_TIPO_PAR && cdr(cauda) == runtime->nulo;
        return quote ? runtime->verdadeiro : runtime->nulo;
    }
    return runtime->verdadeiro;
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
        sef_erro_definir(erro, 0, 0, "COMPILE-EXTERNAL-I64 requires a path or shared library");
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

static const char *nome_designador(SefRuntime *runtime, SefValor valor, size_t *tamanho) {
    if (valor->tipo == SEF_TIPO_TEXTO) {
        *tamanho = valor->como.texto.tamanho;
        return valor->como.texto.dados;
    }
    const char *nome_simbolo = NULL;
    if (sef_simbolo_nome_logico(runtime, valor, &nome_simbolo, tamanho))
        return nome_simbolo;
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
    const char *nome = nome_designador(runtime, valor, &tamanho);
    SefValor pacote = nome == NULL ? NULL : sef_pacote_encontrar(runtime, nome, tamanho);
    if (pacote == NULL)
        sef_erro_definir(erro, 0, 0, "designator does not name an existing package");
    return pacote;
}

static SefValor primitiva_make_package(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "MAKE-PACKAGE", erro))
        return NULL;
    size_t tamanho = 0;
    const char *nome = nome_designador(runtime, car(argumentos), &tamanho);
    if (nome == NULL || tamanho == 0) {
        sef_erro_definir(erro, 0, 0, "MAKE-PACKAGE requires a string name");
        return NULL;
    }
    char *copia = malloc(tamanho + 1);
    if (copia == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for package name");
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
    const char *nome = nome_designador(runtime, car(argumentos), &tamanho);
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
    SefValor destino = cdr(argumentos) == runtime->nulo
                           ? runtime->pacote_atual
                           : pacote_designador(runtime, car(cdr(argumentos)), erro);
    if (destino == NULL)
        return NULL;
    SefValor designadores = car(argumentos);
    bool lista = designadores == runtime->nulo || designadores->tipo == SEF_TIPO_PAR;
    if (lista && !sef_e_lista_propria(runtime, designadores)) {
        sef_erro_definir(erro, 0, 0, "USE-PACKAGE requires a package designator or proper list");
        return NULL;
    }
    while (lista && designadores != runtime->nulo) {
        SefValor designador = car(designadores);
        SefValor usado = pacote_designador(runtime, designador, erro);
        if (usado == NULL || !sef_pacote_usar(runtime, destino, usado, erro))
            return NULL;
        designadores = cdr(designadores);
    }
    if (!lista) {
        SefValor usado = pacote_designador(runtime, designadores, erro);
        if (usado == NULL || !sef_pacote_usar(runtime, destino, usado, erro))
            return NULL;
    }
    return runtime->verdadeiro;
}

static SefValor primitiva_unuse_package(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "UNUSE-PACKAGE", erro))
        return NULL;
    SefValor destino = cdr(argumentos) == runtime->nulo
                           ? runtime->pacote_atual
                           : pacote_designador(runtime, car(cdr(argumentos)), erro);
    if (destino == NULL)
        return NULL;
    SefValor designadores = car(argumentos);
    bool lista = designadores == runtime->nulo || designadores->tipo == SEF_TIPO_PAR;
    if (lista && !sef_e_lista_propria(runtime, designadores)) {
        sef_erro_definir(erro, 0, 0, "UNUSE-PACKAGE requires a package designator or proper list");
        return NULL;
    }
    while (lista && designadores != runtime->nulo) {
        SefValor designador = car(designadores);
        SefValor usado = pacote_designador(runtime, designador, erro);
        if (usado == NULL || !sef_pacote_deixar_de_usar(runtime, destino, usado, erro))
            return NULL;
        designadores = cdr(designadores);
    }
    if (!lista) {
        SefValor usado = pacote_designador(runtime, designadores, erro);
        if (usado == NULL || !sef_pacote_deixar_de_usar(runtime, destino, usado, erro))
            return NULL;
    }
    return runtime->verdadeiro;
}

static SefValor lista_pacotes_usados(SefRuntime *runtime, SefValor pacote, SefErro *erro) {
    SefValor resultado = runtime->nulo;
    for (size_t i = pacote->como.pacote.quantidade_usados; i > 0; i--) {
        resultado = sef_par_novo(runtime, pacote->como.pacote.usados[i - 1], resultado, erro);
        if (resultado == NULL)
            return NULL;
    }
    return resultado;
}

static SefValor primitiva_package_use_list(SefRuntime *runtime, SefValor argumentos,
                                           SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "PACKAGE-USE-LIST", erro))
        return NULL;
    SefValor pacote = pacote_designador(runtime, car(argumentos), erro);
    return pacote == NULL ? NULL : lista_pacotes_usados(runtime, pacote, erro);
}

static SefValor primitiva_package_used_by_list(SefRuntime *runtime, SefValor argumentos,
                                               SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "PACKAGE-USED-BY-LIST", erro))
        return NULL;
    SefValor pacote = pacote_designador(runtime, car(argumentos), erro);
    if (pacote == NULL)
        return NULL;
    SefValor resultado = runtime->nulo;
    for (size_t i = runtime->quantidade_pacotes; i > 0; i--) {
        SefValor candidato = runtime->pacotes[i - 1];
        if (!sef_pacote_usa(candidato, pacote) || candidato == pacote)
            continue;
        resultado = sef_par_novo(runtime, candidato, resultado, erro);
        if (resultado == NULL)
            return NULL;
    }
    return resultado;
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
    if (sef_valor_e_simbolo_logico(runtime, simbolos))
        return sef_pacote_exportar(runtime, pacote, simbolos, erro) ? runtime->verdadeiro : NULL;
    if (!sef_e_lista_propria(runtime, simbolos)) {
        sef_erro_definir(erro, 0, 0, "EXPORT requires a symbol or list of symbols");
        return NULL;
    }
    while (simbolos != runtime->nulo) {
        if (!sef_pacote_exportar(runtime, pacote, car(simbolos), erro))
            return NULL;
        simbolos = cdr(simbolos);
    }
    return runtime->verdadeiro;
}

static SefValor primitiva_unexport(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "UNEXPORT", erro))
        return NULL;
    SefValor pacote = cdr(argumentos) == runtime->nulo
                          ? runtime->pacote_atual
                          : pacote_designador(runtime, car(cdr(argumentos)), erro);
    if (pacote == NULL)
        return NULL;
    SefValor simbolos = car(argumentos);
    if (sef_valor_e_simbolo_logico(runtime, simbolos))
        return sef_pacote_deixar_de_exportar(runtime, pacote, simbolos, erro) ? runtime->verdadeiro
                                                                              : NULL;
    if (!sef_e_lista_propria(runtime, simbolos)) {
        sef_erro_definir(erro, 0, 0, "UNEXPORT requires a symbol or list of symbols");
        return NULL;
    }
    while (simbolos != runtime->nulo) {
        if (!sef_pacote_deixar_de_exportar(runtime, pacote, car(simbolos), erro))
            return NULL;
        simbolos = cdr(simbolos);
    }
    return runtime->verdadeiro;
}

static SefValor primitiva_import(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "IMPORT", erro))
        return NULL;
    SefValor pacote = cdr(argumentos) == runtime->nulo
                          ? runtime->pacote_atual
                          : pacote_designador(runtime, car(cdr(argumentos)), erro);
    if (pacote == NULL)
        return NULL;
    SefValor simbolos = car(argumentos);
    if (sef_valor_e_simbolo_logico(runtime, simbolos))
        return sef_pacote_importar(runtime, pacote, simbolos, erro) ? runtime->verdadeiro : NULL;
    if (!sef_e_lista_propria(runtime, simbolos)) {
        sef_erro_definir(erro, 0, 0, "IMPORT requires a symbol or list of symbols");
        return NULL;
    }
    while (simbolos != runtime->nulo) {
        if (!sef_pacote_importar(runtime, pacote, car(simbolos), erro))
            return NULL;
        simbolos = cdr(simbolos);
    }
    return runtime->verdadeiro;
}

static SefValor primitiva_unintern(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "UNINTERN", erro))
        return NULL;
    SefValor pacote = cdr(argumentos) == runtime->nulo
                          ? runtime->pacote_atual
                          : pacote_designador(runtime, car(cdr(argumentos)), erro);
    if (pacote == NULL)
        return NULL;
    bool removeu = false;
    if (!sef_pacote_desinternar(runtime, pacote, car(argumentos), &removeu, erro))
        return NULL;
    return removeu ? runtime->verdadeiro : runtime->nulo;
}

static bool sombrear_designador(SefRuntime *runtime, SefValor pacote, SefValor designador,
                                SefErro *erro) {
    size_t tamanho = 0;
    const char *nome = NULL;
    if (designador->tipo == SEF_TIPO_TEXTO) {
        nome = designador->como.texto.dados;
        tamanho = designador->como.texto.tamanho;
    } else if (!sef_simbolo_nome_logico(runtime, designador, &nome, &tamanho)) {
        sef_erro_definir(erro, 0, 0, "SHADOW names must be string designators");
        return false;
    }
    return sef_pacote_sombrear(runtime, pacote, nome, tamanho, erro);
}

static SefValor primitiva_shadow(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "SHADOW", erro))
        return NULL;
    SefValor pacote = cdr(argumentos) == runtime->nulo
                          ? runtime->pacote_atual
                          : pacote_designador(runtime, car(cdr(argumentos)), erro);
    if (pacote == NULL)
        return NULL;
    SefValor nomes = car(argumentos);
    if (nomes->tipo == SEF_TIPO_TEXTO || sef_valor_e_simbolo_logico(runtime, nomes))
        return sombrear_designador(runtime, pacote, nomes, erro) ? runtime->verdadeiro : NULL;
    if (!sef_e_lista_propria(runtime, nomes)) {
        sef_erro_definir(erro, 0, 0, "SHADOW requires a string designator or list");
        return NULL;
    }
    while (nomes != runtime->nulo) {
        if (!sombrear_designador(runtime, pacote, car(nomes), erro))
            return NULL;
        nomes = cdr(nomes);
    }
    return runtime->verdadeiro;
}

static SefValor primitiva_shadowing_import(SefRuntime *runtime, SefValor argumentos,
                                           SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "SHADOWING-IMPORT", erro))
        return NULL;
    SefValor pacote = cdr(argumentos) == runtime->nulo
                          ? runtime->pacote_atual
                          : pacote_designador(runtime, car(cdr(argumentos)), erro);
    if (pacote == NULL)
        return NULL;
    SefValor simbolos = car(argumentos);
    if (sef_valor_e_simbolo_logico(runtime, simbolos))
        return sef_pacote_importar_sombreando(runtime, pacote, simbolos, erro) ? runtime->verdadeiro
                                                                               : NULL;
    if (!sef_e_lista_propria(runtime, simbolos)) {
        sef_erro_definir(erro, 0, 0, "SHADOWING-IMPORT requires a symbol or list of symbols");
        return NULL;
    }
    while (simbolos != runtime->nulo) {
        if (!sef_pacote_importar_sombreando(runtime, pacote, car(simbolos), erro))
            return NULL;
        simbolos = cdr(simbolos);
    }
    return runtime->verdadeiro;
}

static SefValor primitiva_package_shadowing_symbols(SefRuntime *runtime, SefValor argumentos,
                                                    SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "PACKAGE-SHADOWING-SYMBOLS", erro))
        return NULL;
    SefValor pacote = pacote_designador(runtime, car(argumentos), erro);
    return pacote == NULL ? NULL : sef_pacote_simbolos_sombreados(runtime, pacote, erro);
}

static SefValor estado_simbolo_para_lisp(SefRuntime *runtime, SefEstadoSimboloPacote estado,
                                         SefErro *erro) {
    if (estado == SEF_SIMBOLO_AUSENTE)
        return runtime->nulo;
    static const char *nomes[] = {NULL, "INTERNAL", "EXTERNAL", "INHERITED"};
    const char *nome = nomes[estado];
    return sef_simbolo_internar_em(runtime, runtime->pacote_keyword, nome, strlen(nome), erro);
}

static SefValor primitiva_intern(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "INTERN", erro))
        return NULL;
    size_t tamanho = 0;
    const char *nome = nome_designador(runtime, car(argumentos), &tamanho);
    SefValor pacote = cdr(argumentos) == runtime->nulo
                          ? runtime->pacote_atual
                          : pacote_designador(runtime, car(cdr(argumentos)), erro);
    if (nome == NULL || pacote == NULL) {
        if (!erro->ocorreu)
            sef_erro_definir(erro, 0, 0, "INTERN requires a string name");
        return NULL;
    }
    SefEstadoSimboloPacote estado = SEF_SIMBOLO_AUSENTE;
    SefValor simbolo =
        sef_pacote_localizar_simbolo_com_estado(pacote, nome, tamanho, true, &estado);
    if (simbolo == NULL)
        simbolo = sef_simbolo_internar_em(runtime, pacote, nome, tamanho, erro);
    if (simbolo == NULL)
        return NULL;
    SefValor estado_lisp = estado_simbolo_para_lisp(runtime, estado, erro);
    if (estado_lisp == NULL)
        return NULL;
    SefValor valores[2] = {simbolo, estado_lisp};
    return sef_valores_definir(runtime, valores, 2, erro) ? simbolo : NULL;
}

static SefValor primitiva_find_symbol(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "FIND-SYMBOL", erro))
        return NULL;
    size_t tamanho = 0;
    const char *nome = nome_designador(runtime, car(argumentos), &tamanho);
    SefValor pacote = cdr(argumentos) == runtime->nulo
                          ? runtime->pacote_atual
                          : pacote_designador(runtime, car(cdr(argumentos)), erro);
    if (nome == NULL || pacote == NULL)
        return NULL;
    SefEstadoSimboloPacote estado = SEF_SIMBOLO_AUSENTE;
    SefValor simbolo =
        sef_pacote_localizar_simbolo_com_estado(pacote, nome, tamanho, true, &estado);
    SefValor estado_lisp = estado_simbolo_para_lisp(runtime, estado, erro);
    if (estado_lisp == NULL)
        return NULL;
    SefValor resultado = simbolo == NULL ? runtime->nulo : simbolo;
    SefValor valores[2] = {resultado, estado_lisp};
    return sef_valores_definir(runtime, valores, 2, erro) ? resultado : NULL;
}

static SefValor primitiva_symbol_name(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "SYMBOL-NAME", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    const char *nome = NULL;
    size_t tamanho = 0;
    if (!sef_simbolo_nome_logico(runtime, simbolo, &nome, &tamanho)) {
        sef_erro_definir(erro, 0, 0, "SYMBOL-NAME requires a symbol");
        return NULL;
    }
    return sef_texto_novo(runtime, nome, tamanho, erro);
}

static SefValor primitiva_symbol_package(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "SYMBOL-PACKAGE", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    if (!sef_valor_e_simbolo_logico(runtime, simbolo)) {
        sef_erro_definir(erro, 0, 0, "SYMBOL-PACKAGE requires a symbol");
        return NULL;
    }
    if (simbolo == runtime->nulo)
        return runtime->pacote_common_lisp;
    return sef_simbolo_nao_internado(runtime, simbolo) || simbolo->como.simbolo.pacote == NULL
               ? runtime->nulo
               : simbolo->como.simbolo.pacote;
}

static SefValor primitiva_make_symbol(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "MAKE-SYMBOL", erro))
        return NULL;
    SefValor nome = car(argumentos);
    if (nome->tipo != SEF_TIPO_TEXTO) {
        sef_erro_definir(erro, 0, 0, "MAKE-SYMBOL requires a string name");
        return NULL;
    }
    return sef_simbolo_novo_nao_internado(runtime, nome->como.texto.dados, nome->como.texto.tamanho,
                                          erro);
}

static SefValor copiar_lista_propriedades(SefRuntime *runtime, SefValor lista, SefErro *erro) {
    SefValor copia = runtime->nulo;
    SefValor *fim = &copia;
    while (lista != runtime->nulo) {
        SefValor chave = sef_par_novo(runtime, car(lista), runtime->nulo, erro);
        SefValor valor =
            chave == NULL ? NULL : sef_par_novo(runtime, car(cdr(lista)), runtime->nulo, erro);
        if (valor == NULL)
            return NULL;
        chave->como.par.resto = valor;
        *fim = chave;
        fim = &valor->como.par.resto;
        lista = cdr(cdr(lista));
    }
    return copia;
}

static SefValor primitiva_copy_symbol(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "COPY-SYMBOL", erro))
        return NULL;
    SefValor original = car(argumentos);
    const char *nome = NULL;
    size_t tamanho = 0;
    if (!sef_simbolo_nome_logico(runtime, original, &nome, &tamanho)) {
        sef_erro_definir(erro, 0, 0, "COPY-SYMBOL requires a symbol");
        return NULL;
    }
    SefValor copia = sef_simbolo_novo_nao_internado(runtime, nome, tamanho, erro);
    bool copiar_propriedades =
        cdr(argumentos) != runtime->nulo && car(cdr(argumentos)) != runtime->nulo;
    if (copia == NULL || !copiar_propriedades)
        return copia;

    SefValor lista = sef_simbolo_lista_propriedades(runtime, original, erro);
    SefValor lista_copiada = lista == NULL ? NULL : copiar_lista_propriedades(runtime, lista, erro);
    if (lista_copiada == NULL ||
        !sef_simbolo_lista_propriedades_definir(runtime, copia, lista_copiada, erro))
        return NULL;

    SefValor valor = NULL;
    bool vinculado = sef_simbolo_e_constante(runtime, original);
    if (vinculado)
        valor = original;
    else
        vinculado = sef_ambiente_obter(runtime->ambiente_global, original, &valor);
    if (vinculado && !sef_ambiente_definir(runtime, runtime->ambiente_global, copia, valor, erro))
        return NULL;
    if (sef_ambiente_obter_funcao(runtime->ambiente_global, original, &valor) &&
        !sef_ambiente_definir_funcao(runtime, runtime->ambiente_global, copia, valor, erro))
        return NULL;
    return copia;
}

static bool contador_gensym_instalar(SefRuntime *runtime, SefErro *erro) {
    static const char nome[] = "*GENSYM-COUNTER*";
    SefValor simbolo =
        sef_simbolo_internar_em(runtime, runtime->pacote_common_lisp, nome, sizeof(nome) - 1, erro);
    if (simbolo == NULL ||
        !sef_pacote_exportar(runtime, runtime->pacote_common_lisp, simbolo, erro))
        return false;
    SefValor existente = NULL;
    if (sef_ambiente_obter(runtime->ambiente_global, simbolo, &existente))
        return true;
    SefValor zero = sef_inteiro_novo(runtime, 0, erro);
    return zero != NULL &&
           sef_ambiente_definir(runtime, runtime->ambiente_global, simbolo, zero, erro);
}

static SefValor primitiva_gensym(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 0, 1, "GENSYM", erro))
        return NULL;
    const char *prefixo = "G";
    size_t tamanho_prefixo = 1;
    int64_t sufixo = 0;
    bool incrementar = true;
    if (argumentos != runtime->nulo) {
        SefValor opcao = car(argumentos);
        if (opcao->tipo == SEF_TIPO_TEXTO) {
            prefixo = opcao->como.texto.dados;
            tamanho_prefixo = opcao->como.texto.tamanho;
        } else if (opcao->tipo == SEF_TIPO_INTEIRO && opcao->como.inteiro >= 0) {
            sufixo = opcao->como.inteiro;
            incrementar = false;
        } else {
            sef_erro_definir(erro, 0, 0,
                             "GENSYM argument must be a string or non-negative integer");
            return NULL;
        }
    }
    static const char nome_contador[] = "*GENSYM-COUNTER*";
    SefValor simbolo_contador = NULL;
    SefValor contador = NULL;
    if (incrementar) {
        simbolo_contador = sef_pacote_localizar_simbolo(runtime->pacote_common_lisp, nome_contador,
                                                        sizeof(nome_contador) - 1, false);
        if (simbolo_contador == NULL ||
            !sef_ambiente_obter(runtime->ambiente_global, simbolo_contador, &contador)) {
            sef_erro_definir(erro, 0, 0, "GENSYM counter is unbound");
            return NULL;
        }
        if (contador->tipo != SEF_TIPO_INTEIRO || contador->como.inteiro < 0) {
            sef_erro_definir(erro, 0, 0, "*GENSYM-COUNTER* must be a non-negative integer");
            return NULL;
        }
        sufixo = contador->como.inteiro;
    }
    if (incrementar && contador->como.inteiro == INT64_MAX) {
        sef_erro_definir(erro, 0, 0, "*GENSYM-COUNTER* exceeded its maximum value");
        return NULL;
    }
    char texto_sufixo[32];
    int escritos = snprintf(texto_sufixo, sizeof(texto_sufixo), "%lld", (long long)sufixo);
    if (escritos < 0 || (size_t)escritos >= sizeof(texto_sufixo) ||
        tamanho_prefixo > SIZE_MAX - (size_t)escritos) {
        sef_erro_definir(erro, 0, 0, "GENSYM name is too large");
        return NULL;
    }
    size_t tamanho = tamanho_prefixo + (size_t)escritos;
    char *nome = malloc(tamanho == 0 ? 1 : tamanho);
    if (nome == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for GENSYM name");
        return NULL;
    }
    memcpy(nome, prefixo, tamanho_prefixo);
    memcpy(nome + tamanho_prefixo, texto_sufixo, (size_t)escritos);
    SefValor simbolo = sef_simbolo_novo_nao_internado(runtime, nome, tamanho, erro);
    free(nome);
    if (simbolo == NULL || !incrementar)
        return simbolo;
    SefValor proximo = sef_inteiro_novo(runtime, contador->como.inteiro + 1, erro);
    return proximo != NULL &&
                   sef_ambiente_atribuir(runtime->ambiente_global, simbolo_contador, proximo)
               ? simbolo
               : NULL;
}

static SefValor primitiva_symbol_plist(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "SYMBOL-PLIST", erro))
        return NULL;
    return sef_simbolo_lista_propriedades(runtime, car(argumentos), erro);
}

static SefValor primitiva_get(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 3, "GET", erro))
        return NULL;
    SefValor simbolo = car(argumentos);
    SefValor indicador = car(cdr(argumentos));
    SefValor padrao =
        cdr(cdr(argumentos)) == runtime->nulo ? runtime->nulo : car(cdr(cdr(argumentos)));
    return sef_simbolo_propriedade_obter(runtime, simbolo, indicador, padrao, erro);
}

static SefValor primitiva_remprop(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, "REMPROP", erro))
        return NULL;
    bool removeu = false;
    if (!sef_simbolo_propriedade_remover(runtime, car(argumentos), car(cdr(argumentos)), &removeu,
                                         erro))
        return NULL;
    return removeu ? runtime->verdadeiro : runtime->nulo;
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

static SefValor primitiva_values(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!sef_valores_definir_lista(runtime, argumentos, erro))
        return NULL;
    return sef_valores_primario(runtime);
}

static SefValor primitiva_values_list(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "VALUES-LIST", erro))
        return NULL;
    if (!sef_valores_definir_lista(runtime, car(argumentos), erro))
        return NULL;
    return sef_valores_primario(runtime);
}

static SefValor primitiva_make_hash_table(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 0, 0, "MAKE-HASH-TABLE", erro))
        return NULL;
    return sef_tabela_hash_nova(runtime, erro);
}
static SefValor primitiva_gethash(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 3, "GETHASH", erro))
        return NULL;
    SefValor padrao =
        cdr(cdr(argumentos)) == runtime->nulo ? runtime->nulo : car(cdr(cdr(argumentos)));
    bool encontrou = false;
    SefValor valor = sef_tabela_hash_obter(runtime, car(cdr(argumentos)), car(argumentos), padrao,
                                           &encontrou, erro);
    if (valor == NULL)
        return NULL;
    SefValor valores[2] = {valor, encontrou ? runtime->verdadeiro : runtime->nulo};
    return sef_valores_definir(runtime, valores, 2, erro) ? valor : NULL;
}
static SefValor primitiva_hash_table_p(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "HASH-TABLE-P", erro))
        return NULL;
    return car(argumentos)->tipo == SEF_TIPO_TABELA_HASH ? runtime->verdadeiro : runtime->nulo;
}
static SefValor primitiva_hash_table_count(SefRuntime *runtime, SefValor argumentos,
                                           SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "HASH-TABLE-COUNT", erro))
        return NULL;
    SefValor t = car(argumentos);
    if (t->tipo != SEF_TIPO_TABELA_HASH) {
        sef_erro_definir(erro, 0, 0, "HASH-TABLE-COUNT requires a hash table");
        return NULL;
    }
    return sef_inteiro_novo(runtime, (int64_t)t->como.tabela_hash.quantidade, erro);
}
static SefValor primitiva_remhash(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 2, 2, "REMHASH", erro))
        return NULL;
    bool r = false;
    return sef_tabela_hash_remover(runtime, car(cdr(argumentos)), car(argumentos), &r, erro)
               ? (r ? runtime->verdadeiro : runtime->nulo)
               : NULL;
}
static SefValor primitiva_clrhash(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "CLRHASH", erro))
        return NULL;
    SefValor t = car(argumentos);
    if (t->tipo != SEF_TIPO_TABELA_HASH) {
        sef_erro_definir(erro, 0, 0, "CLRHASH requires a hash table");
        return NULL;
    }
    sef_tabela_hash_limpar(t);
    return t;
}

static SefValor primitiva_error(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "ERROR", erro))
        return NULL;
    SefValor designador = car(argumentos);
    SefValor condicao = designador;
    if (designador->tipo == SEF_TIPO_TEXTO) {
        SefValor classe = sef_simbolo_internar(runtime, "ERROR", 5, erro);
        condicao = classe == NULL
                       ? NULL
                       : sef_condicao_nova(runtime, classe, designador->como.texto.dados, erro);
    } else if (designador->tipo != SEF_TIPO_CONDICAO) {
        sef_erro_definir(erro, 0, 0, "ERROR requires a string or condition");
        return NULL;
    }
    if (condicao == NULL || !sef_condicao_sinalizar(runtime, condicao, erro))
        return NULL;
    runtime->ultima_condicao = condicao;
    sef_erro_definir(erro, 0, 0, "%s", condicao->como.condicao.mensagem->como.texto.dados);
    return NULL;
}

static SefValor primitiva_signal(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "SIGNAL", erro))
        return NULL;
    SefValor designador = car(argumentos);
    SefValor condicao = designador;
    if (designador->tipo == SEF_TIPO_TEXTO) {
        SefValor classe = sef_simbolo_internar(runtime, "CONDITION", 9, erro);
        condicao = classe == NULL
                       ? NULL
                       : sef_condicao_nova(runtime, classe, designador->como.texto.dados, erro);
    } else if (designador->tipo != SEF_TIPO_CONDICAO) {
        sef_erro_definir(erro, 0, 0, "SIGNAL requires a string or condition");
        return NULL;
    }
    if (condicao == NULL || !sef_condicao_sinalizar(runtime, condicao, erro))
        return NULL;
    return sef_valores_definir_um(runtime, runtime->nulo, erro) ? runtime->nulo : NULL;
}

static bool reinicio_condicao_suportada(SefRuntime *runtime, SefValor argumentos, const char *nome,
                                        SefErro *erro) {
    if (argumentos == runtime->nulo || cdr(argumentos) == runtime->nulo)
        return true;
    if (car(cdr(argumentos)) == runtime->nulo)
        return true;
    sef_erro_definir(erro, 0, 0, "%s does not yet associate restarts with a specific condition",
                     nome);
    return false;
}

static SefReinicioDinamico *reinicio_encontrar(SefRuntime *runtime, SefValor designador) {
    bool por_objeto = designador != NULL && designador->tipo == SEF_TIPO_REINICIO;
    for (SefReinicioDinamico *reinicio = runtime->reinicios; reinicio != NULL;
         reinicio = reinicio->anterior) {
        if ((por_objeto && reinicio->objeto == designador) ||
            (!por_objeto && reinicio->objeto->como.reinicio.nome == designador))
            return reinicio;
    }
    return NULL;
}

static SefValor primitiva_invoke_restart(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, (size_t)-1, "INVOKE-RESTART", erro))
        return NULL;
    return sef_reinicio_invocar(runtime, car(argumentos), cdr(argumentos), erro);
}

static SefValor primitiva_find_restart(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, "FIND-RESTART", erro) ||
        !reinicio_condicao_suportada(runtime, argumentos, "FIND-RESTART", erro))
        return NULL;
    SefValor designador = car(argumentos);
    bool nome = designador != runtime->nulo && designador->tipo == SEF_TIPO_SIMBOLO;
    bool objeto = designador != NULL && designador->tipo == SEF_TIPO_REINICIO;
    if (!nome && !objeto) {
        sef_erro_definir(erro, 0, 0, "FIND-RESTART requires a non-NIL symbol or RESTART object");
        return NULL;
    }
    SefReinicioDinamico *reinicio = reinicio_encontrar(runtime, designador);
    return reinicio == NULL ? runtime->nulo : reinicio->objeto;
}

static SefValor primitiva_compute_restarts(SefRuntime *runtime, SefValor argumentos,
                                           SefErro *erro) {
    if (!quantidade(runtime, argumentos, 0, 1, "COMPUTE-RESTARTS", erro))
        return NULL;
    if (argumentos != runtime->nulo && car(argumentos) != runtime->nulo) {
        sef_erro_definir(erro, 0, 0,
                         "COMPUTE-RESTARTS does not yet filter by a specific condition");
        return NULL;
    }
    SefValor invertida = runtime->nulo;
    for (SefReinicioDinamico *reinicio = runtime->reinicios; reinicio != NULL;
         reinicio = reinicio->anterior) {
        invertida = sef_par_novo(runtime, reinicio->objeto, invertida, erro);
        if (invertida == NULL)
            return NULL;
    }
    return sef_lista_inverter(runtime, invertida, erro);
}

static SefValor primitiva_restart_name(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 1, "RESTART-NAME", erro))
        return NULL;
    SefValor reinicio = car(argumentos);
    if (reinicio == NULL || reinicio->tipo != SEF_TIPO_REINICIO) {
        sef_erro_definir(erro, 0, 0, "RESTART-NAME requires a RESTART object");
        return NULL;
    }
    return reinicio->como.reinicio.nome;
}

static SefValor reinicio_padrao_invocar(SefRuntime *runtime, const char *nome,
                                        SefValor argumentos_reinicio, SefValor condicao,
                                        SefErro *erro) {
    if (condicao != runtime->nulo) {
        sef_erro_definir(erro, 0, 0, "%s does not yet select a restart by specific condition",
                         nome);
        return NULL;
    }
    SefValor simbolo =
        sef_simbolo_internar_em(runtime, runtime->pacote_common_lisp, nome, strlen(nome), erro);
    return simbolo == NULL ? NULL
                           : sef_reinicio_invocar(runtime, simbolo, argumentos_reinicio, erro);
}

static SefValor primitiva_abort(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 0, 1, "ABORT", erro))
        return NULL;
    SefValor condicao = argumentos == runtime->nulo ? runtime->nulo : car(argumentos);
    return reinicio_padrao_invocar(runtime, "ABORT", runtime->nulo, condicao, erro);
}

static SefValor primitiva_continue(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 0, 1, "CONTINUE", erro))
        return NULL;
    SefValor condicao = argumentos == runtime->nulo ? runtime->nulo : car(argumentos);
    return reinicio_padrao_invocar(runtime, "CONTINUE", runtime->nulo, condicao, erro);
}

static SefValor primitiva_muffle_warning(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 0, 1, "MUFFLE-WARNING", erro))
        return NULL;
    SefValor condicao = argumentos == runtime->nulo ? runtime->nulo : car(argumentos);
    return reinicio_padrao_invocar(runtime, "MUFFLE-WARNING", runtime->nulo, condicao, erro);
}

static SefValor reinicio_com_valor_invocar(SefRuntime *runtime, SefValor argumentos,
                                           const char *nome, SefErro *erro) {
    if (!quantidade(runtime, argumentos, 1, 2, nome, erro))
        return NULL;
    SefValor condicao = cdr(argumentos) == runtime->nulo ? runtime->nulo : car(cdr(argumentos));
    SefValor valores = sef_par_novo(runtime, car(argumentos), runtime->nulo, erro);
    return valores == NULL ? NULL : reinicio_padrao_invocar(runtime, nome, valores, condicao, erro);
}

static SefValor primitiva_store_value(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    return reinicio_com_valor_invocar(runtime, argumentos, "STORE-VALUE", erro);
}

static SefValor primitiva_use_value(SefRuntime *runtime, SefValor argumentos, SefErro *erro) {
    return reinicio_com_valor_invocar(runtime, argumentos, "USE-VALUE", erro);
}

static bool instalar(SefRuntime *runtime, const char *nome, SefFuncaoNativa funcao,
                     bool preservar_existente, SefErro *erro) {
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
        sef_erro_definir(erro, 0, 0, "internal primitive package does not exist");
        return false;
    }
    SefValor simbolo =
        sef_simbolo_internar_em(runtime, pacote, nome_simbolo, strlen(nome_simbolo), erro);
    SefValor existente = NULL;
    if (simbolo != NULL && preservar_existente &&
        sef_ambiente_obter_funcao(runtime->ambiente_global, simbolo, &existente))
        return !exportar || sef_pacote_exportar(runtime, pacote, simbolo, erro);
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
                  {"CONSP", sef_primitiva_consp},
                  {"LISTP", sef_primitiva_listp},
                  {"ENDP", sef_primitiva_endp},
                  {"FIRST", sef_primitiva_first},
                  {"REST", sef_primitiva_rest},
                  {"RPLACA", sef_primitiva_rplaca},
                  {"RPLACD", sef_primitiva_rplacd},
                  {"NTH", sef_primitiva_nth},
                  {"NTHCDR", sef_primitiva_nthcdr},
                  {"LAST", sef_primitiva_last},
                  {"APPEND", sef_primitiva_append},
                  {"NCONC", sef_primitiva_nconc},
                  {"MEMBER", sef_primitiva_member},
                  {"ASSOC", sef_primitiva_assoc},
                  {"MAPCAR", sef_primitiva_mapcar},
                  {"MAPC", sef_primitiva_mapc},
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
                  {"FDEFINITION", primitiva_fdefinition},
                  {"MAKUNBOUND", primitiva_makunbound},
                  {"FMAKUNBOUND", primitiva_fmakunbound},
                  {"SET", primitiva_set},
                  {"SYMBOLP", primitiva_symbolp},
                  {"KEYWORDP", primitiva_keywordp},
                  {"CONSTANTP", primitiva_constantp},
                  {"FUNCTIONP", primitiva_functionp},
                  {"COMPILE", primitiva_compile},
                  {"COMPILE-EXTERNAL-I64", primitiva_compile_external_i64},
                  {"COMPILED-FUNCTION-P", primitiva_compiled_function_p},
                  {"ERROR", primitiva_error},
                  {"SIGNAL", primitiva_signal},
                  {"INVOKE-RESTART", primitiva_invoke_restart},
                  {"FIND-RESTART", primitiva_find_restart},
                  {"COMPUTE-RESTARTS", primitiva_compute_restarts},
                  {"RESTART-NAME", primitiva_restart_name},
                  {"ABORT", primitiva_abort},
                  {"CONTINUE", primitiva_continue},
                  {"MUFFLE-WARNING", primitiva_muffle_warning},
                  {"STORE-VALUE", primitiva_store_value},
                  {"USE-VALUE", primitiva_use_value},
                  {"MAKE-PACKAGE", primitiva_make_package},
                  {"FIND-PACKAGE", primitiva_find_package},
                  {"PACKAGE-NAME", primitiva_package_name},
                  {"PACKAGEP", primitiva_packagep},
                  {"USE-PACKAGE", primitiva_use_package},
                  {"UNUSE-PACKAGE", primitiva_unuse_package},
                  {"PACKAGE-USE-LIST", primitiva_package_use_list},
                  {"PACKAGE-USED-BY-LIST", primitiva_package_used_by_list},
                  {"EXPORT", primitiva_export},
                  {"UNEXPORT", primitiva_unexport},
                  {"IMPORT", primitiva_import},
                  {"UNINTERN", primitiva_unintern},
                  {"SHADOW", primitiva_shadow},
                  {"SHADOWING-IMPORT", primitiva_shadowing_import},
                  {"PACKAGE-SHADOWING-SYMBOLS", primitiva_package_shadowing_symbols},
                  {"INTERN", primitiva_intern},
                  {"FIND-SYMBOL", primitiva_find_symbol},
                  {"SYMBOL-NAME", primitiva_symbol_name},
                  {"SYMBOL-PACKAGE", primitiva_symbol_package},
                  {"MAKE-SYMBOL", primitiva_make_symbol},
                  {"COPY-SYMBOL", primitiva_copy_symbol},
                  {"GENSYM", primitiva_gensym},
                  {"SYMBOL-PLIST", primitiva_symbol_plist},
                  {"GET", primitiva_get},
                  {"REMPROP", primitiva_remprop},
                  {"LIST-ALL-PACKAGES", primitiva_list_all_packages},
                  {"VALUES", primitiva_values},
                  {"VALUES-LIST", primitiva_values_list},
                  {"MAKE-HASH-TABLE", primitiva_make_hash_table},
                  {"GETHASH", primitiva_gethash},
                  {"HASH-TABLE-P", primitiva_hash_table_p},
                  {"HASH-TABLE-COUNT", primitiva_hash_table_count},
                  {"REMHASH", primitiva_remhash},
                  {"CLRHASH", primitiva_clrhash},
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
        if (!instalar(runtime, primitivas[i].nome, primitivas[i].funcao, false, erro)) {
            return false;
        }
    }
    return contador_gensym_instalar(runtime, erro);
}

bool sef_primitivas_reconciliar(SefRuntime *runtime, SefErro *erro) {
    for (size_t i = 0; i < sizeof(primitivas) / sizeof(primitivas[0]); i++) {
        if (!instalar(runtime, primitivas[i].nome, primitivas[i].funcao, true, erro))
            return false;
    }
    return contador_gensym_instalar(runtime, erro);
}
