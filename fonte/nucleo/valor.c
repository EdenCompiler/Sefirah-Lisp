#include "sefirah/interno.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

SefValor sef_objeto_novo(SefRuntime *runtime, SefTipo tipo, SefErro *erro) {
    SefValor objeto = calloc(1, sizeof(*objeto));
    if (objeto == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory to create object");
        return NULL;
    }
    objeto->tipo = tipo;
    objeto->proximo_alocado = runtime->objetos;
    runtime->objetos = objeto;
    runtime->quantidade_objetos++;
    runtime->bytes_aproximados += sizeof(*objeto);
    return objeto;
}

SefValor sef_inteiro_novo(SefRuntime *runtime, int64_t numero, SefErro *erro) {
    SefValor valor = sef_objeto_novo(runtime, SEF_TIPO_INTEIRO, erro);
    if (valor != NULL)
        valor->como.inteiro = numero;
    return valor;
}

SefValor sef_real_novo(SefRuntime *runtime, double numero, SefErro *erro) {
    SefValor valor = sef_objeto_novo(runtime, SEF_TIPO_REAL, erro);
    if (valor != NULL)
        valor->como.real = numero;
    return valor;
}

SefValor sef_texto_novo(SefRuntime *runtime, const char *texto, size_t tamanho, SefErro *erro) {
    SefValor valor = sef_objeto_novo(runtime, SEF_TIPO_TEXTO, erro);
    if (valor == NULL)
        return NULL;

    valor->como.texto.dados = malloc(tamanho + 1);
    if (valor->como.texto.dados == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory to copy string");
        return NULL;
    }
    memcpy(valor->como.texto.dados, texto, tamanho);
    valor->como.texto.dados[tamanho] = '\0';
    valor->como.texto.tamanho = tamanho;
    runtime->bytes_aproximados += tamanho + 1;
    return valor;
}

SefValor sef_texto_caractere_obter(SefRuntime *runtime, SefValor texto, size_t indice,
                                   SefErro *erro) {
    if (texto == NULL || texto->tipo != SEF_TIPO_TEXTO) {
        sef_erro_definir(erro, 0, 0, "character access requires a string");
        return NULL;
    }
    uint32_t codigo;
    if (!sef_utf8_localizar(texto->como.texto.dados, texto->como.texto.tamanho, indice, NULL, NULL,
                            &codigo)) {
        bool valido = false;
        size_t tamanho =
            sef_utf8_quantidade(texto->como.texto.dados, texto->como.texto.tamanho, &valido);
        sef_erro_definir(erro, 0, 0,
                         valido && indice >= tamanho ? "string index out of bounds"
                                                     : "string contains invalid UTF-8");
        return NULL;
    }
    return sef_caractere_novo(runtime, codigo, erro);
}

bool sef_texto_caractere_definir(SefRuntime *runtime, SefValor texto, size_t indice,
                                 SefValor caractere, SefErro *erro) {
    if (texto == NULL || texto->tipo != SEF_TIPO_TEXTO || caractere == NULL ||
        caractere->tipo != SEF_TIPO_CARACTERE) {
        sef_erro_definir(erro, 0, 0, "string mutation requires a string and character");
        return false;
    }
    size_t inicio, anterior;
    if (!sef_utf8_localizar(texto->como.texto.dados, texto->como.texto.tamanho, indice, &inicio,
                            &anterior, NULL)) {
        bool valido = false;
        size_t tamanho =
            sef_utf8_quantidade(texto->como.texto.dados, texto->como.texto.tamanho, &valido);
        sef_erro_definir(erro, 0, 0,
                         valido && indice >= tamanho ? "string index out of bounds"
                                                     : "string contains invalid UTF-8");
        return false;
    }
    char novo[4];
    size_t quantidade_nova = sef_utf8_codificar(caractere->como.caractere, novo);
    size_t tamanho_novo = texto->como.texto.tamanho - anterior + quantidade_nova;
    if (quantidade_nova != anterior) {
        char *novos_dados = malloc(tamanho_novo + 1);
        if (novos_dados == NULL) {
            sef_erro_definir(erro, 0, 0, "not enough memory to modify string");
            return false;
        }
        memcpy(novos_dados, texto->como.texto.dados, inicio);
        memcpy(novos_dados + inicio + quantidade_nova, texto->como.texto.dados + inicio + anterior,
               texto->como.texto.tamanho - inicio - anterior + 1);
        if (tamanho_novo > texto->como.texto.tamanho)
            runtime->bytes_aproximados += tamanho_novo - texto->como.texto.tamanho;
        free(texto->como.texto.dados);
        texto->como.texto.dados = novos_dados;
        texto->como.texto.tamanho = tamanho_novo;
    }
    memcpy(texto->como.texto.dados + inicio, novo, quantidade_nova);
    return true;
}

SefValor sef_vetor_novo(SefRuntime *runtime, size_t tamanho, SefValor inicial, SefErro *erro) {
    if (runtime == NULL || inicial == NULL) {
        sef_erro_definir(erro, 0, 0, "missing runtime or initial value while creating vector");
        return NULL;
    }
    if (tamanho > SIZE_MAX / sizeof(SefValor)) {
        sef_erro_definir(erro, 0, 0, "vector is too large");
        return NULL;
    }
    SefValor valor = sef_objeto_novo(runtime, SEF_TIPO_VETOR, erro);
    if (valor == NULL)
        return NULL;
    if (tamanho > 0) {
        valor->como.vetor.itens = malloc(tamanho * sizeof(SefValor));
        if (valor->como.vetor.itens == NULL) {
            sef_erro_definir(erro, 0, 0, "not enough memory to create vector");
            return NULL;
        }
        for (size_t i = 0; i < tamanho; i++)
            valor->como.vetor.itens[i] = inicial;
    }
    valor->como.vetor.tamanho = tamanho;
    runtime->bytes_aproximados += tamanho * sizeof(SefValor);
    return valor;
}

SefValor sef_caractere_novo(SefRuntime *runtime, uint32_t codigo, SefErro *erro) {
    char codificado[4];
    if (runtime == NULL || sef_utf8_codificar(codigo, codificado) == 0) {
        sef_erro_definir(erro, 0, 0, "invalid Unicode code point while creating character");
        return NULL;
    }
    SefValor valor = sef_objeto_novo(runtime, SEF_TIPO_CARACTERE, erro);
    if (valor != NULL)
        valor->como.caractere = codigo;
    return valor;
}

static char *copiar_nome_maiusculo(const char *nome, size_t tamanho) {
    char *copia = malloc(tamanho + 1);
    if (copia == NULL)
        return NULL;
    for (size_t i = 0; i < tamanho; i++) {
        unsigned char caractere = (unsigned char)nome[i];
        copia[i] = (char)(caractere < 128 ? toupper(caractere) : caractere);
    }
    copia[tamanho] = '\0';
    return copia;
}

static char *copiar_nome_exato(const char *nome, size_t tamanho) {
    char *copia = malloc(tamanho + 1);
    if (copia == NULL)
        return NULL;
    memcpy(copia, nome, tamanho);
    copia[tamanho] = '\0';
    return copia;
}

typedef struct NomeSimboloLido {
    char *dados;
    size_t tamanho;
    size_t separador_pacote;
    size_t quantidade_separadores;
} NomeSimboloLido;

static bool normalizar_nome_simbolo_lido(const char *nome, size_t tamanho, NomeSimboloLido *saida,
                                         SefErro *erro) {
    saida->dados = malloc(tamanho + 1);
    saida->tamanho = 0;
    saida->separador_pacote = SIZE_MAX;
    saida->quantidade_separadores = 0;
    if (saida->dados == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory to normalize symbol name");
        return false;
    }
    bool entre_barras = false;
    for (size_t i = 0; i < tamanho; i++) {
        unsigned char caractere = (unsigned char)nome[i];
        if (caractere == '\\') {
            if (++i >= tamanho) {
                free(saida->dados);
                saida->dados = NULL;
                sef_erro_definir(erro, 0, 0, "incomplete escape in symbol name");
                return false;
            }
            saida->dados[saida->tamanho++] = nome[i];
            continue;
        }
        if (caractere == '|') {
            entre_barras = !entre_barras;
            continue;
        }
        if (!entre_barras && caractere == ':') {
            if (saida->separador_pacote == SIZE_MAX) {
                saida->separador_pacote = saida->tamanho;
                saida->quantidade_separadores = 1;
            } else if (saida->quantidade_separadores == 1 &&
                       saida->tamanho == saida->separador_pacote + 1) {
                saida->quantidade_separadores = 2;
            } else {
                free(saida->dados);
                saida->dados = NULL;
                sef_erro_definir(erro, 0, 0, "invalid package separator in symbol");
                return false;
            }
        }
        saida->dados[saida->tamanho++] =
            (char)(!entre_barras && caractere < 128 ? toupper(caractere) : caractere);
    }
    if (entre_barras) {
        free(saida->dados);
        saida->dados = NULL;
        sef_erro_definir(erro, 0, 0, "symbol is missing its closing vertical bar");
        return false;
    }
    saida->dados[saida->tamanho] = '\0';
    return true;
}

static bool vetor_valores_crescer(SefValor **valores, size_t *capacidade, size_t minimo,
                                  SefErro *erro) {
    if (*capacidade >= minimo)
        return true;
    size_t nova = *capacidade == 0 ? 8 : *capacidade * 2;
    while (nova < minimo)
        nova *= 2;
    SefValor *realocado = realloc(*valores, nova * sizeof(*realocado));
    if (realocado == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for object table");
        return false;
    }
    *valores = realocado;
    *capacidade = nova;
    return true;
}

bool sef_valor_e_simbolo_logico(const SefRuntime *runtime, SefValor valor) {
    return runtime != NULL && valor != NULL &&
           (valor == runtime->nulo || valor->tipo == SEF_TIPO_SIMBOLO);
}

bool sef_simbolo_e_constante(const SefRuntime *runtime, SefValor simbolo) {
    return sef_valor_e_simbolo_logico(runtime, simbolo) &&
           (simbolo == runtime->nulo || simbolo == runtime->verdadeiro ||
            (simbolo->tipo == SEF_TIPO_SIMBOLO &&
             simbolo->como.simbolo.pacote == runtime->pacote_keyword));
}

bool sef_simbolo_nome_logico(const SefRuntime *runtime, SefValor simbolo, const char **nome,
                             size_t *tamanho) {
    if (!sef_valor_e_simbolo_logico(runtime, simbolo) || nome == NULL || tamanho == NULL)
        return false;
    if (simbolo == runtime->nulo) {
        *nome = "NIL";
        *tamanho = 3;
    } else {
        *nome = simbolo->como.simbolo.nome;
        *tamanho = simbolo->como.simbolo.tamanho;
    }
    return true;
}

SefValor sef_pacote_encontrar(SefRuntime *runtime, const char *nome, size_t tamanho) {
    for (size_t i = 0; i < runtime->quantidade_pacotes; i++) {
        SefValor pacote = runtime->pacotes[i];
        if (strlen(pacote->como.pacote.nome) != tamanho)
            continue;
        bool igual = true;
        for (size_t j = 0; j < tamanho; j++) {
            unsigned char caractere = (unsigned char)nome[j];
            char normalizado = (char)(caractere < 128 ? toupper(caractere) : caractere);
            if (pacote->como.pacote.nome[j] != normalizado) {
                igual = false;
                break;
            }
        }
        if (igual)
            return pacote;
    }
    return NULL;
}

SefValor sef_pacote_novo(SefRuntime *runtime, const char *nome, SefErro *erro) {
    size_t tamanho = strlen(nome);
    if (sef_pacote_encontrar(runtime, nome, tamanho) != NULL) {
        sef_erro_definir(erro, 0, 0, "package %s already exists", nome);
        return NULL;
    }
    char *normalizado = copiar_nome_maiusculo(nome, tamanho);
    if (normalizado == NULL ||
        !vetor_valores_crescer(&runtime->pacotes, &runtime->capacidade_pacotes,
                               runtime->quantidade_pacotes + 1, erro)) {
        free(normalizado);
        return NULL;
    }
    SefValor pacote = sef_objeto_novo(runtime, SEF_TIPO_PACOTE, erro);
    if (pacote == NULL) {
        free(normalizado);
        return NULL;
    }
    pacote->como.pacote.nome = normalizado;
    runtime->pacotes[runtime->quantidade_pacotes++] = pacote;
    return pacote;
}

static bool nome_de_simbolo_armazenado(SefValor simbolo, const char **nome, size_t *tamanho) {
    if (simbolo == NULL)
        return false;
    if (simbolo->tipo == SEF_TIPO_NULO) {
        *nome = "NIL";
        *tamanho = 3;
        return true;
    }
    if (simbolo->tipo != SEF_TIPO_SIMBOLO)
        return false;
    *nome = simbolo->como.simbolo.nome;
    *tamanho = simbolo->como.simbolo.tamanho;
    return true;
}

static SefValor pacote_buscar_simbolo(SefValor pacote, const char *nome, size_t tamanho);
static bool pacote_simbolo_sombreia(SefRuntime *runtime, SefValor pacote, SefValor simbolo,
                                    SefErro *erro);
static bool pacote_sombra_remover(SefRuntime *runtime, SefValor pacote, SefValor simbolo,
                                  SefErro *erro);

bool sef_pacote_usar(SefRuntime *runtime, SefValor pacote, SefValor usado, SefErro *erro) {
    if (pacote == NULL || usado == NULL || pacote->tipo != SEF_TIPO_PACOTE ||
        usado->tipo != SEF_TIPO_PACOTE) {
        sef_erro_definir(erro, 0, 0, "USE-PACKAGE received an object that is not a package");
        return false;
    }
    for (size_t i = 0; i < pacote->como.pacote.quantidade_usados; i++) {
        if (pacote->como.pacote.usados[i] == usado)
            return true;
    }
    for (size_t i = 0; i < usado->como.pacote.quantidade_exportados; i++) {
        SefValor candidato = usado->como.pacote.exportados[i];
        const char *nome_candidato = NULL;
        size_t tamanho_candidato = 0;
        if (!nome_de_simbolo_armazenado(candidato, &nome_candidato, &tamanho_candidato)) {
            sef_erro_definir(erro, 0, 0, "used package contains an object that is not a symbol");
            return false;
        }
        SefValor existente = pacote_buscar_simbolo(pacote, nome_candidato, tamanho_candidato);
        if (existente != NULL && existente != candidato &&
            pacote_simbolo_sombreia(runtime, pacote, existente, erro))
            continue;
        if (erro->ocorreu)
            return false;
        for (size_t j = 0; existente == NULL && j < pacote->como.pacote.quantidade_usados; j++) {
            SefValor origem = pacote->como.pacote.usados[j];
            SefValor herdado = pacote_buscar_simbolo(origem, nome_candidato, tamanho_candidato);
            if (herdado != NULL && sef_pacote_simbolo_exportado(origem, herdado))
                existente = herdado;
        }
        if (existente != NULL && existente != candidato) {
            sef_erro_definir(erro, 0, 0, "package-use conflict: symbol %.*s is already accessible",
                             (int)tamanho_candidato, nome_candidato);
            return false;
        }
    }
    if (!vetor_valores_crescer(&pacote->como.pacote.usados, &pacote->como.pacote.capacidade_usados,
                               pacote->como.pacote.quantidade_usados + 1, erro))
        return false;
    pacote->como.pacote.usados[pacote->como.pacote.quantidade_usados++] = usado;
    return true;
}

bool sef_pacote_usa(SefValor pacote, SefValor usado) {
    if (pacote == usado)
        return true;
    if (pacote == NULL || pacote->tipo != SEF_TIPO_PACOTE)
        return false;
    for (size_t i = 0; i < pacote->como.pacote.quantidade_usados; i++) {
        if (pacote->como.pacote.usados[i] == usado)
            return true;
    }
    return false;
}

bool sef_pacote_simbolo_exportado(SefValor pacote, SefValor simbolo) {
    if (pacote == NULL || pacote->tipo != SEF_TIPO_PACOTE)
        return false;
    for (size_t i = 0; i < pacote->como.pacote.quantidade_exportados; i++) {
        if (pacote->como.pacote.exportados[i] == simbolo)
            return true;
    }
    return false;
}

bool sef_pacote_exportar(SefRuntime *runtime, SefValor pacote, SefValor simbolo, SefErro *erro) {
    bool nulo_do_common_lisp =
        runtime != NULL && simbolo == runtime->nulo && pacote == runtime->pacote_common_lisp;
    const char *nome_simbolo = NULL;
    size_t tamanho_simbolo = 0;
    bool simbolo_interno = nome_de_simbolo_armazenado(simbolo, &nome_simbolo, &tamanho_simbolo) &&
                           pacote_buscar_simbolo(pacote, nome_simbolo, tamanho_simbolo) == simbolo;
    if (pacote == NULL || pacote->tipo != SEF_TIPO_PACOTE ||
        (!nulo_do_common_lisp && !simbolo_interno)) {
        sef_erro_definir(erro, 0, 0, "EXPORT requires a symbol internal to the package");
        return false;
    }
    if (sef_pacote_simbolo_exportado(pacote, simbolo))
        return true;
    if (!vetor_valores_crescer(&pacote->como.pacote.exportados,
                               &pacote->como.pacote.capacidade_exportados,
                               pacote->como.pacote.quantidade_exportados + 1, erro))
        return false;
    pacote->como.pacote.exportados[pacote->como.pacote.quantidade_exportados++] = simbolo;
    return true;
}

static SefValor pacote_buscar_simbolo(SefValor pacote, const char *nome, size_t tamanho) {
    for (size_t i = 0; i < pacote->como.pacote.quantidade_simbolos; i++) {
        SefValor simbolo = pacote->como.pacote.simbolos[i];
        const char *nome_simbolo = NULL;
        size_t tamanho_simbolo = 0;
        if (nome_de_simbolo_armazenado(simbolo, &nome_simbolo, &tamanho_simbolo) &&
            tamanho_simbolo == tamanho && memcmp(nome_simbolo, nome, tamanho) == 0)
            return simbolo;
    }
    return NULL;
}

SefValor sef_pacote_localizar_simbolo_com_estado(SefValor pacote, const char *nome, size_t tamanho,
                                                 bool incluir_herdados,
                                                 SefEstadoSimboloPacote *estado) {
    if (estado != NULL)
        *estado = SEF_SIMBOLO_AUSENTE;
    SefValor encontrado = pacote_buscar_simbolo(pacote, nome, tamanho);
    if (encontrado != NULL && estado != NULL)
        *estado = sef_pacote_simbolo_exportado(pacote, encontrado) ? SEF_SIMBOLO_EXTERNO
                                                                   : SEF_SIMBOLO_INTERNO;
    for (size_t i = 0;
         encontrado == NULL && incluir_herdados && i < pacote->como.pacote.quantidade_usados; i++) {
        SefValor usado = pacote->como.pacote.usados[i];
        SefValor candidato = pacote_buscar_simbolo(usado, nome, tamanho);
        if (candidato != NULL && sef_pacote_simbolo_exportado(usado, candidato)) {
            encontrado = candidato;
            if (estado != NULL)
                *estado = SEF_SIMBOLO_HERDADO;
        }
    }
    return encontrado;
}

SefValor sef_pacote_localizar_simbolo(SefValor pacote, const char *nome, size_t tamanho,
                                      bool incluir_herdados) {
    return sef_pacote_localizar_simbolo_com_estado(pacote, nome, tamanho, incluir_herdados, NULL);
}

SefValor sef_simbolo_internar_em(SefRuntime *runtime, SefValor pacote, const char *nome,
                                 size_t tamanho, SefErro *erro) {
    if (pacote == NULL || pacote->tipo != SEF_TIPO_PACOTE) {
        sef_erro_definir(erro, 0, 0, "invalid package while interning symbol");
        return NULL;
    }
    if (pacote == runtime->pacote_common_lisp && tamanho == 3 && memcmp(nome, "NIL", 3) == 0)
        return runtime->nulo;
    char *copia = copiar_nome_exato(nome, tamanho);
    if (copia == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory to intern symbol");
        return NULL;
    }

    SefValor existente = pacote_buscar_simbolo(pacote, copia, tamanho);
    if (existente != NULL) {
        free(copia);
        if (pacote == runtime->pacote_keyword &&
            !sef_pacote_exportar(runtime, pacote, existente, erro))
            return NULL;
        return existente;
    }
    if (!vetor_valores_crescer(&runtime->simbolos, &runtime->capacidade_simbolos,
                               runtime->quantidade_simbolos + 1, erro) ||
        !vetor_valores_crescer(&pacote->como.pacote.simbolos,
                               &pacote->como.pacote.capacidade_simbolos,
                               pacote->como.pacote.quantidade_simbolos + 1, erro)) {
        free(copia);
        return NULL;
    }

    SefValor simbolo = sef_objeto_novo(runtime, SEF_TIPO_SIMBOLO, erro);
    if (simbolo == NULL) {
        free(copia);
        return NULL;
    }
    simbolo->como.simbolo.nome = copia;
    simbolo->como.simbolo.tamanho = tamanho;
    simbolo->como.simbolo.pacote = pacote;
    runtime->bytes_aproximados += tamanho + 1;
    runtime->simbolos[runtime->quantidade_simbolos++] = simbolo;
    pacote->como.pacote.simbolos[pacote->como.pacote.quantidade_simbolos++] = simbolo;
    if (pacote == runtime->pacote_keyword && !sef_pacote_exportar(runtime, pacote, simbolo, erro))
        return NULL;
    return simbolo;
}

static const char nome_pacote_nao_internado[] = "SEFIRAH-PRIVATE-UNINTERNED-SYMBOLS";

static bool pacote_registrado(const SefRuntime *runtime, SefValor pacote) {
    for (size_t i = 0; i < runtime->quantidade_pacotes; i++) {
        if (runtime->pacotes[i] == pacote)
            return true;
    }
    return false;
}

static bool pacote_e_sentinela_interno(const SefRuntime *runtime, SefValor pacote) {
    return pacote != NULL && pacote->tipo == SEF_TIPO_PACOTE &&
           !pacote_registrado(runtime, pacote) &&
           strcmp(pacote->como.pacote.nome, nome_pacote_nao_internado) == 0;
}

static SefValor pacote_nao_internado_obter(SefRuntime *runtime, SefErro *erro) {
    for (size_t i = 0; i < runtime->quantidade_simbolos; i++) {
        SefValor pacote = runtime->simbolos[i]->como.simbolo.pacote;
        if (pacote_e_sentinela_interno(runtime, pacote))
            return pacote;
    }
    SefValor pacote = sef_objeto_novo(runtime, SEF_TIPO_PACOTE, erro);
    if (pacote == NULL)
        return NULL;
    pacote->como.pacote.nome =
        copiar_nome_exato(nome_pacote_nao_internado, sizeof(nome_pacote_nao_internado) - 1);
    if (pacote->como.pacote.nome == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for uninterned symbol storage");
        return NULL;
    }
    runtime->bytes_aproximados += sizeof(nome_pacote_nao_internado);
    return pacote;
}

SefValor sef_simbolo_novo_nao_internado(SefRuntime *runtime, const char *nome, size_t tamanho,
                                        SefErro *erro) {
    if (runtime == NULL || (nome == NULL && tamanho > 0)) {
        sef_erro_definir(erro, 0, 0, "invalid name while creating uninterned symbol");
        return NULL;
    }
    if (!vetor_valores_crescer(&runtime->simbolos, &runtime->capacidade_simbolos,
                               runtime->quantidade_simbolos + 1, erro))
        return NULL;
    SefValor pacote = pacote_nao_internado_obter(runtime, erro);
    char *copia = pacote == NULL ? NULL : copiar_nome_exato(nome == NULL ? "" : nome, tamanho);
    if (copia == NULL) {
        if (!erro->ocorreu)
            sef_erro_definir(erro, 0, 0, "not enough memory for uninterned symbol name");
        return NULL;
    }
    SefValor simbolo = sef_objeto_novo(runtime, SEF_TIPO_SIMBOLO, erro);
    if (simbolo == NULL) {
        free(copia);
        return NULL;
    }
    simbolo->como.simbolo.nome = copia;
    simbolo->como.simbolo.tamanho = tamanho;
    simbolo->como.simbolo.pacote = pacote;
    runtime->bytes_aproximados += tamanho + 1;
    runtime->simbolos[runtime->quantidade_simbolos++] = simbolo;
    return simbolo;
}

bool sef_simbolo_nao_internado(const SefRuntime *runtime, SefValor simbolo) {
    return runtime != NULL && simbolo != NULL && simbolo->tipo == SEF_TIPO_SIMBOLO &&
           pacote_e_sentinela_interno(runtime, simbolo->como.simbolo.pacote);
}

bool sef_pacote_importar(SefRuntime *runtime, SefValor pacote, SefValor simbolo, SefErro *erro) {
    if (runtime == NULL || pacote == NULL || pacote->tipo != SEF_TIPO_PACOTE ||
        !sef_valor_e_simbolo_logico(runtime, simbolo)) {
        sef_erro_definir(erro, 0, 0, "IMPORT requires a symbol and package");
        return false;
    }
    const char *nome = NULL;
    size_t tamanho = 0;
    if (!sef_simbolo_nome_logico(runtime, simbolo, &nome, &tamanho)) {
        sef_erro_definir(erro, 0, 0, "IMPORT received an invalid symbol");
        return false;
    }
    SefValor local = pacote_buscar_simbolo(pacote, nome, tamanho);
    if (local == simbolo)
        return true;
    if (local != NULL) {
        sef_erro_definir(erro, 0, 0, "IMPORT conflicts with a different local symbol named %.*s",
                         (int)tamanho, nome);
        return false;
    }
    SefValor acessivel = sef_pacote_localizar_simbolo(pacote, nome, tamanho, true);
    if (acessivel != NULL && acessivel != simbolo) {
        sef_erro_definir(erro, 0, 0,
                         "IMPORT conflicts with a different accessible symbol named %.*s",
                         (int)tamanho, nome);
        return false;
    }
    if (!vetor_valores_crescer(&pacote->como.pacote.simbolos,
                               &pacote->como.pacote.capacidade_simbolos,
                               pacote->como.pacote.quantidade_simbolos + 1, erro))
        return false;
    pacote->como.pacote.simbolos[pacote->como.pacote.quantidade_simbolos++] = simbolo;
    if (sef_simbolo_nao_internado(runtime, simbolo))
        simbolo->como.simbolo.pacote = pacote;
    return true;
}

bool sef_pacote_desinternar(SefRuntime *runtime, SefValor pacote, SefValor simbolo, bool *removeu,
                            SefErro *erro) {
    if (removeu != NULL)
        *removeu = false;
    if (runtime == NULL || pacote == NULL || pacote->tipo != SEF_TIPO_PACOTE ||
        !sef_valor_e_simbolo_logico(runtime, simbolo)) {
        sef_erro_definir(erro, 0, 0, "UNINTERN requires a symbol and package");
        return false;
    }
    if (pacote == runtime->pacote_common_lisp) {
        sef_erro_definir(erro, 0, 0, "the COMMON-LISP package is locked");
        return false;
    }
    size_t indice = 0;
    while (indice < pacote->como.pacote.quantidade_simbolos &&
           pacote->como.pacote.simbolos[indice] != simbolo)
        indice++;
    if (indice == pacote->como.pacote.quantidade_simbolos)
        return true;
    bool sombreia = pacote_simbolo_sombreia(runtime, pacote, simbolo, erro);
    if (erro->ocorreu)
        return false;
    if (sombreia) {
        const char *nome = NULL;
        size_t tamanho = 0;
        sef_simbolo_nome_logico(runtime, simbolo, &nome, &tamanho);
        SefValor herdado = NULL;
        for (size_t i = 0; i < pacote->como.pacote.quantidade_usados; i++) {
            SefValor origem = pacote->como.pacote.usados[i];
            SefValor candidato = pacote_buscar_simbolo(origem, nome, tamanho);
            if (candidato == NULL || !sef_pacote_simbolo_exportado(origem, candidato))
                continue;
            if (herdado != NULL && herdado != candidato) {
                sef_erro_definir(erro, 0, 0,
                                 "UNINTERN would expose conflicting inherited symbol %.*s",
                                 (int)tamanho, nome);
                return false;
            }
            herdado = candidato;
        }
    }
    if (!pacote_sombra_remover(runtime, pacote, simbolo, erro))
        return false;
    SefValor sentinela = NULL;
    if (simbolo != runtime->nulo && simbolo->como.simbolo.pacote == pacote) {
        sentinela = pacote_nao_internado_obter(runtime, erro);
        if (sentinela == NULL)
            return false;
    }
    memmove(&pacote->como.pacote.simbolos[indice], &pacote->como.pacote.simbolos[indice + 1],
            (pacote->como.pacote.quantidade_simbolos - indice - 1) * sizeof(SefValor));
    pacote->como.pacote.quantidade_simbolos--;
    for (size_t i = 0; i < pacote->como.pacote.quantidade_exportados;) {
        if (pacote->como.pacote.exportados[i] != simbolo) {
            i++;
            continue;
        }
        memmove(&pacote->como.pacote.exportados[i], &pacote->como.pacote.exportados[i + 1],
                (pacote->como.pacote.quantidade_exportados - i - 1) * sizeof(SefValor));
        pacote->como.pacote.quantidade_exportados--;
    }
    if (sentinela != NULL)
        simbolo->como.simbolo.pacote = sentinela;
    if (removeu != NULL)
        *removeu = true;
    return true;
}

static void substituir_simbolo_em_vetor(SefValor *valores, size_t quantidade, SefValor antigo,
                                        SefValor novo) {
    for (size_t i = 0; i < quantidade; i++)
        if (valores[i] == antigo)
            valores[i] = novo;
}

static void remover_simbolo_de_vetor(SefValor *valores, size_t *quantidade, SefValor removido) {
    size_t destino = 0;
    for (size_t i = 0; i < *quantidade; i++)
        if (valores[i] != removido)
            valores[destino++] = valores[i];
    *quantidade = destino;
}

bool sef_pacote_instalar_nulo(SefRuntime *runtime, SefErro *erro) {
    if (runtime == NULL || runtime->nulo == NULL || runtime->nulo->tipo != SEF_TIPO_NULO ||
        runtime->pacote_common_lisp == NULL ||
        runtime->pacote_common_lisp->tipo != SEF_TIPO_PACOTE) {
        sef_erro_definir(erro, 0, 0, "could not install NIL in COMMON-LISP");
        return false;
    }
    SefValor pacote = runtime->pacote_common_lisp;
    SefValor legado = pacote_buscar_simbolo(pacote, "NIL", 3);
    if (legado == NULL) {
        if (!vetor_valores_crescer(&pacote->como.pacote.simbolos,
                                   &pacote->como.pacote.capacidade_simbolos,
                                   pacote->como.pacote.quantidade_simbolos + 1, erro))
            return false;
        pacote->como.pacote.simbolos[pacote->como.pacote.quantidade_simbolos++] = runtime->nulo;
    } else if (legado != runtime->nulo) {
        substituir_simbolo_em_vetor(pacote->como.pacote.simbolos,
                                    pacote->como.pacote.quantidade_simbolos, legado, runtime->nulo);
        substituir_simbolo_em_vetor(pacote->como.pacote.exportados,
                                    pacote->como.pacote.quantidade_exportados, legado,
                                    runtime->nulo);
    }
    if (!sef_pacote_exportar(runtime, pacote, runtime->nulo, erro))
        return false;

    for (size_t i = 0; i < runtime->quantidade_pacotes; i++) {
        SefValor usuario = runtime->pacotes[i];
        if (usuario == pacote || !sef_pacote_usa(usuario, pacote))
            continue;
        SefValor conflito = pacote_buscar_simbolo(usuario, "NIL", 3);
        if (conflito != NULL && conflito != runtime->nulo) {
            remover_simbolo_de_vetor(usuario->como.pacote.simbolos,
                                     &usuario->como.pacote.quantidade_simbolos, conflito);
            remover_simbolo_de_vetor(usuario->como.pacote.exportados,
                                     &usuario->como.pacote.quantidade_exportados, conflito);
        }
    }
    return true;
}

SefValor sef_simbolo_internar(SefRuntime *runtime, const char *nome, size_t tamanho,
                              SefErro *erro) {
    if (runtime->pacote_atual == NULL) {
        sef_erro_definir(erro, 0, 0, "there is no current package");
        return NULL;
    }
    NomeSimboloLido nome_lido;
    if (!normalizar_nome_simbolo_lido(nome, tamanho, &nome_lido, erro))
        return NULL;
    char *normalizado = nome_lido.dados;
    tamanho = nome_lido.tamanho;
    if (nome_lido.separador_pacote == 0 && nome_lido.quantidade_separadores == 1) {
        SefValor simbolo = sef_simbolo_internar_em(runtime, runtime->pacote_keyword,
                                                   normalizado + 1, tamanho - 1, erro);
        free(normalizado);
        return simbolo;
    }

    if (nome_lido.separador_pacote != SIZE_MAX) {
        size_t tamanho_pacote = nome_lido.separador_pacote;
        size_t separadores = nome_lido.quantidade_separadores;
        size_t inicio_nome = tamanho_pacote + separadores;
        SefValor pacote = sef_pacote_encontrar(runtime, normalizado, tamanho_pacote);
        if (pacote == NULL || inicio_nome > tamanho) {
            free(normalizado);
            sef_erro_definir(erro, 0, 0, "symbol designator has an invalid package");
            return NULL;
        }
        SefValor simbolo = sef_pacote_localizar_simbolo(pacote, normalizado + inicio_nome,
                                                        tamanho - inicio_nome, false);
        if (separadores == 1) {
            if (simbolo == NULL || !sef_pacote_simbolo_exportado(pacote, simbolo)) {
                free(normalizado);
                sef_erro_definir(erro, 0, 0, "symbol is not external in the indicated package");
                return NULL;
            }
            free(normalizado);
            return simbolo;
        }
        if (simbolo == NULL)
            simbolo = sef_simbolo_internar_em(runtime, pacote, normalizado + inicio_nome,
                                              tamanho - inicio_nome, erro);
        free(normalizado);
        return simbolo;
    }

    if (tamanho == 3 && memcmp(normalizado, "NIL", 3) == 0) {
        free(normalizado);
        return runtime->nulo;
    }

    SefValor encontrado = pacote_buscar_simbolo(runtime->pacote_atual, normalizado, tamanho);
    for (size_t i = 0;
         encontrado == NULL && i < runtime->pacote_atual->como.pacote.quantidade_usados; i++) {
        SefValor usado = runtime->pacote_atual->como.pacote.usados[i];
        SefValor candidato = pacote_buscar_simbolo(usado, normalizado, tamanho);
        if (candidato != NULL && sef_pacote_simbolo_exportado(usado, candidato))
            encontrado = candidato;
    }
    if (encontrado == NULL)
        encontrado =
            sef_simbolo_internar_em(runtime, runtime->pacote_atual, normalizado, tamanho, erro);
    free(normalizado);
    return encontrado;
}

SefValor sef_par_novo(SefRuntime *runtime, SefValor primeiro, SefValor resto, SefErro *erro) {
    SefValor par = sef_objeto_novo(runtime, SEF_TIPO_PAR, erro);
    if (par != NULL) {
        par->como.par.primeiro = primeiro;
        par->como.par.resto = resto;
    }
    return par;
}

SefValor sef_nativa_nova(SefRuntime *runtime, const char *nome, SefFuncaoNativa funcao,
                         SefErro *erro) {
    SefValor nativa = sef_objeto_novo(runtime, SEF_TIPO_NATIVA, erro);
    if (nativa != NULL) {
        nativa->como.nativa.funcao = funcao;
        nativa->como.nativa.nome = nome;
    }
    return nativa;
}

SefValor sef_funcao_nova(SefRuntime *runtime, SefValor parametros, SefValor corpo,
                         SefValor ambiente, bool macro, SefErro *erro) {
    SefValor funcao = sef_objeto_novo(runtime, SEF_TIPO_FUNCAO, erro);
    if (funcao != NULL) {
        funcao->como.funcao.parametros = parametros;
        funcao->como.funcao.corpo = corpo;
        funcao->como.funcao.ambiente = ambiente;
        funcao->como.funcao.macro = macro;
    }
    return funcao;
}

SefValor sef_condicao_nova(SefRuntime *runtime, SefValor classe, const char *mensagem,
                           SefErro *erro) {
    SefValor texto = sef_texto_novo(runtime, mensagem, strlen(mensagem), erro);
    if (texto == NULL)
        return NULL;
    SefValor condicao = sef_objeto_novo(runtime, SEF_TIPO_CONDICAO, erro);
    if (condicao != NULL) {
        condicao->como.condicao.classe = classe;
        condicao->como.condicao.mensagem = texto;
    }
    return condicao;
}

SefValor sef_reinicio_novo(SefRuntime *runtime, SefValor nome, SefErro *erro) {
    if (!sef_valor_e_simbolo_logico(runtime, nome)) {
        sef_erro_definir(erro, 0, 0, "restart name must be a symbol or NIL");
        return NULL;
    }
    SefValor reinicio = sef_objeto_novo(runtime, SEF_TIPO_REINICIO, erro);
    if (reinicio != NULL)
        reinicio->como.reinicio.nome = nome;
    return reinicio;
}

SefValor sef_stream_novo(SefRuntime *runtime, FILE *arquivo, const char *caminho,
                         bool possui_arquivo, unsigned char padrao, SefErro *erro) {
    SefValor stream = sef_objeto_novo(runtime, SEF_TIPO_STREAM, erro);
    if (stream == NULL)
        return NULL;
    if (caminho != NULL) {
        size_t tamanho_caminho = strlen(caminho);
        stream->como.stream.caminho = malloc(tamanho_caminho + 1);
        if (stream->como.stream.caminho == NULL) {
            sef_erro_definir(erro, 0, 0, "not enough memory for stream path");
            return NULL;
        }
        memcpy(stream->como.stream.caminho, caminho, tamanho_caminho + 1);
    }
    stream->como.stream.arquivo = arquivo;
    stream->como.stream.possui_arquivo = possui_arquivo;
    stream->como.stream.padrao = padrao;
    return stream;
}

bool sef_simbolo_tem_nome(SefValor valor, const char *nome) {
    return valor != NULL && valor->tipo == SEF_TIPO_SIMBOLO &&
           strcmp(valor->como.simbolo.nome, nome) == 0;
}

static SefValor tabela_interna(SefRuntime *runtime, const char *nome, size_t tamanho, bool criar,
                               SefErro *erro) {
    SefValor pacote = sef_pacote_encontrar(runtime, "SEFIRAH", 7);
    if (pacote == NULL) {
        sef_erro_definir(erro, 0, 0, "internal SEFIRAH package is missing");
        return NULL;
    }
    SefValor simbolo = pacote_buscar_simbolo(pacote, nome, tamanho);
    if (simbolo == NULL && !criar)
        return NULL;
    if (simbolo == NULL)
        simbolo = sef_simbolo_internar_em(runtime, pacote, nome, tamanho, erro);
    if (simbolo == NULL)
        return NULL;
    SefValor tabela = NULL;
    if (sef_ambiente_obter(runtime->ambiente_global, simbolo, &tabela)) {
        if (tabela->tipo != SEF_TIPO_TABELA_HASH) {
            sef_erro_definir(erro, 0, 0, "internal runtime table %.*s has an invalid value",
                             (int)tamanho, nome);
            return NULL;
        }
        return tabela;
    }
    if (!criar)
        return NULL;
    tabela = sef_tabela_hash_nova(runtime, erro);
    return tabela != NULL &&
                   sef_ambiente_definir(runtime, runtime->ambiente_global, simbolo, tabela, erro)
               ? tabela
               : NULL;
}

static SefValor tabela_propriedades_simbolos(SefRuntime *runtime, bool criar, SefErro *erro) {
    static const char nome[] = "*SYMBOL-PROPERTY-LISTS*";
    return tabela_interna(runtime, nome, sizeof(nome) - 1, criar, erro);
}

static SefValor tabela_simbolos_sombreados(SefRuntime *runtime, bool criar, SefErro *erro) {
    static const char nome[] = "*PACKAGE-SHADOWING-SYMBOLS*";
    return tabela_interna(runtime, nome, sizeof(nome) - 1, criar, erro);
}

static bool lista_propria_limitada(SefRuntime *runtime, SefValor lista) {
    size_t visitados = 0;
    while (lista != runtime->nulo) {
        if (lista == NULL || lista->tipo != SEF_TIPO_PAR ||
            visitados++ > runtime->quantidade_objetos)
            return false;
        lista = lista->como.par.resto;
    }
    return true;
}

static SefValor pacote_lista_sombras(SefRuntime *runtime, SefValor pacote, SefErro *erro) {
    SefValor tabela = tabela_simbolos_sombreados(runtime, false, erro);
    if (tabela == NULL)
        return erro->ocorreu ? NULL : runtime->nulo;
    bool encontrou = false;
    SefValor lista =
        sef_tabela_hash_obter(runtime, tabela, pacote, runtime->nulo, &encontrou, erro);
    if (lista == NULL || !lista_propria_limitada(runtime, lista)) {
        if (!erro->ocorreu)
            sef_erro_definir(erro, 0, 0, "package shadowing-symbol list is malformed");
        return NULL;
    }
    return lista;
}

static bool pacote_lista_sombras_definir(SefRuntime *runtime, SefValor pacote, SefValor lista,
                                         SefErro *erro) {
    SefValor tabela = tabela_simbolos_sombreados(runtime, lista != runtime->nulo, erro);
    if (tabela == NULL)
        return !erro->ocorreu;
    if (lista != runtime->nulo)
        return sef_tabela_hash_definir(runtime, tabela, pacote, lista, erro);
    bool removeu = false;
    return sef_tabela_hash_remover(runtime, tabela, pacote, &removeu, erro);
}

static bool pacote_simbolo_sombreia(SefRuntime *runtime, SefValor pacote, SefValor simbolo,
                                    SefErro *erro) {
    SefValor lista = pacote_lista_sombras(runtime, pacote, erro);
    while (lista != NULL && lista != runtime->nulo) {
        if (lista->como.par.primeiro == simbolo)
            return true;
        lista = lista->como.par.resto;
    }
    return false;
}

static bool pacote_sombra_registrar(SefRuntime *runtime, SefValor pacote, SefValor simbolo,
                                    SefErro *erro) {
    if (pacote_simbolo_sombreia(runtime, pacote, simbolo, erro))
        return true;
    if (erro->ocorreu)
        return false;
    SefValor lista = pacote_lista_sombras(runtime, pacote, erro);
    if (lista == NULL)
        return false;
    SefValor nova = sef_par_novo(runtime, simbolo, lista, erro);
    return nova != NULL && pacote_lista_sombras_definir(runtime, pacote, nova, erro);
}

static bool pacote_sombra_remover(SefRuntime *runtime, SefValor pacote, SefValor simbolo,
                                  SefErro *erro) {
    SefValor lista = pacote_lista_sombras(runtime, pacote, erro);
    if (lista == NULL)
        return false;
    SefValor anterior = NULL;
    for (SefValor atual = lista; atual != runtime->nulo; atual = atual->como.par.resto) {
        if (atual->como.par.primeiro != simbolo) {
            anterior = atual;
            continue;
        }
        if (anterior != NULL)
            anterior->como.par.resto = atual->como.par.resto;
        else if (!pacote_lista_sombras_definir(runtime, pacote, atual->como.par.resto, erro))
            return false;
        return true;
    }
    return true;
}

bool sef_pacote_sombrear(SefRuntime *runtime, SefValor pacote, const char *nome, size_t tamanho,
                         SefErro *erro) {
    if (runtime == NULL || pacote == NULL || pacote->tipo != SEF_TIPO_PACOTE || nome == NULL) {
        sef_erro_definir(erro, 0, 0, "SHADOW requires a symbol name and package");
        return false;
    }
    if (pacote == runtime->pacote_common_lisp) {
        sef_erro_definir(erro, 0, 0, "the COMMON-LISP package is locked");
        return false;
    }
    SefValor simbolo = pacote_buscar_simbolo(pacote, nome, tamanho);
    if (simbolo == NULL)
        simbolo = sef_simbolo_internar_em(runtime, pacote, nome, tamanho, erro);
    return simbolo != NULL && pacote_sombra_registrar(runtime, pacote, simbolo, erro);
}

bool sef_pacote_importar_sombreando(SefRuntime *runtime, SefValor pacote, SefValor simbolo,
                                    SefErro *erro) {
    if (runtime == NULL || pacote == NULL || pacote->tipo != SEF_TIPO_PACOTE ||
        !sef_valor_e_simbolo_logico(runtime, simbolo)) {
        sef_erro_definir(erro, 0, 0, "SHADOWING-IMPORT requires a symbol and package");
        return false;
    }
    if (pacote == runtime->pacote_common_lisp) {
        sef_erro_definir(erro, 0, 0, "the COMMON-LISP package is locked");
        return false;
    }
    const char *nome = NULL;
    size_t tamanho = 0;
    if (!sef_simbolo_nome_logico(runtime, simbolo, &nome, &tamanho))
        return false;
    SefValor local = pacote_buscar_simbolo(pacote, nome, tamanho);
    if (local != NULL && local != simbolo) {
        bool removeu = false;
        if (!pacote_sombra_remover(runtime, pacote, local, erro) ||
            !sef_pacote_desinternar(runtime, pacote, local, &removeu, erro))
            return false;
    }
    if (local != simbolo) {
        if (!vetor_valores_crescer(&pacote->como.pacote.simbolos,
                                   &pacote->como.pacote.capacidade_simbolos,
                                   pacote->como.pacote.quantidade_simbolos + 1, erro))
            return false;
        pacote->como.pacote.simbolos[pacote->como.pacote.quantidade_simbolos++] = simbolo;
        if (sef_simbolo_nao_internado(runtime, simbolo))
            simbolo->como.simbolo.pacote = pacote;
    }
    return pacote_sombra_registrar(runtime, pacote, simbolo, erro);
}

SefValor sef_pacote_simbolos_sombreados(SefRuntime *runtime, SefValor pacote, SefErro *erro) {
    if (runtime == NULL || pacote == NULL || pacote->tipo != SEF_TIPO_PACOTE) {
        sef_erro_definir(erro, 0, 0, "PACKAGE-SHADOWING-SYMBOLS requires a package");
        return NULL;
    }
    SefValor lista = pacote_lista_sombras(runtime, pacote, erro);
    if (lista == NULL)
        return NULL;
    SefValor copia_invertida = runtime->nulo;
    for (SefValor atual = lista; atual != runtime->nulo; atual = atual->como.par.resto) {
        copia_invertida = sef_par_novo(runtime, atual->como.par.primeiro, copia_invertida, erro);
        if (copia_invertida == NULL)
            return NULL;
    }
    return sef_lista_inverter(runtime, copia_invertida, erro);
}

static bool lista_propriedades_valida(SefRuntime *runtime, SefValor lista) {
    size_t pares_visitados = 0;
    while (lista != runtime->nulo) {
        if (pares_visitados++ > runtime->quantidade_objetos / 2u)
            return false;
        if (lista == NULL || lista->tipo != SEF_TIPO_PAR)
            return false;
        lista = lista->como.par.resto;
        if (lista == runtime->nulo || lista == NULL || lista->tipo != SEF_TIPO_PAR)
            return false;
        lista = lista->como.par.resto;
    }
    return true;
}

SefValor sef_simbolo_lista_propriedades(SefRuntime *runtime, SefValor simbolo, SefErro *erro) {
    if (!sef_valor_e_simbolo_logico(runtime, simbolo)) {
        sef_erro_definir(erro, 0, 0, "SYMBOL-PLIST requires a symbol");
        return NULL;
    }
    SefValor tabela = tabela_propriedades_simbolos(runtime, false, erro);
    if (tabela == NULL)
        return erro->ocorreu ? NULL : runtime->nulo;
    bool encontrou = false;
    SefValor lista =
        sef_tabela_hash_obter(runtime, tabela, simbolo, runtime->nulo, &encontrou, erro);
    if (lista == NULL || !lista_propriedades_valida(runtime, lista)) {
        if (!erro->ocorreu)
            sef_erro_definir(erro, 0, 0, "symbol property list is malformed");
        return NULL;
    }
    return lista;
}

bool sef_simbolo_lista_propriedades_definir(SefRuntime *runtime, SefValor simbolo, SefValor lista,
                                            SefErro *erro) {
    if (!sef_valor_e_simbolo_logico(runtime, simbolo)) {
        sef_erro_definir(erro, 0, 0, "SETF of SYMBOL-PLIST requires a symbol");
        return false;
    }
    if (!lista_propriedades_valida(runtime, lista)) {
        sef_erro_definir(erro, 0, 0, "symbol property list must contain indicator/value pairs");
        return false;
    }
    SefValor tabela = tabela_propriedades_simbolos(runtime, lista != runtime->nulo, erro);
    if (tabela == NULL)
        return !erro->ocorreu;
    if (lista != runtime->nulo)
        return sef_tabela_hash_definir(runtime, tabela, simbolo, lista, erro);
    bool removeu = false;
    return sef_tabela_hash_remover(runtime, tabela, simbolo, &removeu, erro);
}

SefValor sef_simbolo_propriedade_obter(SefRuntime *runtime, SefValor simbolo, SefValor indicador,
                                       SefValor padrao, SefErro *erro) {
    SefValor lista = sef_simbolo_lista_propriedades(runtime, simbolo, erro);
    if (lista == NULL)
        return NULL;
    while (lista != runtime->nulo) {
        if (sef_valores_eql(lista->como.par.primeiro, indicador))
            return lista->como.par.resto->como.par.primeiro;
        lista = lista->como.par.resto->como.par.resto;
    }
    return padrao;
}

bool sef_simbolo_propriedade_definir(SefRuntime *runtime, SefValor simbolo, SefValor indicador,
                                     SefValor valor, SefErro *erro) {
    SefValor lista = sef_simbolo_lista_propriedades(runtime, simbolo, erro);
    if (lista == NULL)
        return false;
    for (SefValor atual = lista; atual != runtime->nulo;
         atual = atual->como.par.resto->como.par.resto) {
        if (sef_valores_eql(atual->como.par.primeiro, indicador)) {
            atual->como.par.resto->como.par.primeiro = valor;
            return true;
        }
    }
    SefValor celula_valor = sef_par_novo(runtime, valor, lista, erro);
    SefValor nova_lista =
        celula_valor == NULL ? NULL : sef_par_novo(runtime, indicador, celula_valor, erro);
    return nova_lista != NULL &&
           sef_simbolo_lista_propriedades_definir(runtime, simbolo, nova_lista, erro);
}

bool sef_simbolo_propriedade_remover(SefRuntime *runtime, SefValor simbolo, SefValor indicador,
                                     bool *removeu, SefErro *erro) {
    if (removeu != NULL)
        *removeu = false;
    SefValor lista = sef_simbolo_lista_propriedades(runtime, simbolo, erro);
    if (lista == NULL)
        return false;
    SefValor anterior_valor = NULL;
    for (SefValor atual = lista; atual != runtime->nulo;
         atual = atual->como.par.resto->como.par.resto) {
        SefValor proximo = atual->como.par.resto->como.par.resto;
        if (sef_valores_eql(atual->como.par.primeiro, indicador)) {
            if (anterior_valor == NULL) {
                if (!sef_simbolo_lista_propriedades_definir(runtime, simbolo, proximo, erro))
                    return false;
            } else {
                anterior_valor->como.par.resto = proximo;
            }
            if (removeu != NULL)
                *removeu = true;
            return true;
        }
        anterior_valor = atual->como.par.resto;
    }
    return true;
}

bool sef_valores_eql(SefValor a, SefValor b) {
    if (a == b)
        return true;
    if (a == NULL || b == NULL || a->tipo != b->tipo)
        return false;
    if (a->tipo == SEF_TIPO_INTEIRO)
        return a->como.inteiro == b->como.inteiro;
    if (a->tipo == SEF_TIPO_REAL)
        return a->como.real == b->como.real;
    if (a->tipo == SEF_TIPO_CARACTERE)
        return a->como.caractere == b->como.caractere;
    return false;
}

bool sef_e_lista_propria(SefRuntime *runtime, SefValor valor) {
    while (valor != runtime->nulo) {
        if (valor == NULL || valor->tipo != SEF_TIPO_PAR)
            return false;
        valor = valor->como.par.resto;
    }
    return true;
}

size_t sef_lista_tamanho(SefRuntime *runtime, SefValor lista, bool *propria) {
    size_t tamanho = 0;
    while (lista != runtime->nulo) {
        if (lista == NULL || lista->tipo != SEF_TIPO_PAR) {
            if (propria != NULL)
                *propria = false;
            return tamanho;
        }
        tamanho++;
        lista = lista->como.par.resto;
    }
    if (propria != NULL)
        *propria = true;
    return tamanho;
}

SefValor sef_lista_inverter(SefRuntime *runtime, SefValor lista, SefErro *erro) {
    SefValor resultado = runtime->nulo;
    while (lista != runtime->nulo) {
        if (lista == NULL || lista->tipo != SEF_TIPO_PAR) {
            sef_erro_definir(erro, 0, 0, "value is not a proper list");
            return NULL;
        }
        resultado = sef_par_novo(runtime, lista->como.par.primeiro, resultado, erro);
        if (resultado == NULL)
            return NULL;
        lista = lista->como.par.resto;
    }
    return resultado;
}

const char *sef_valor_nome_tipo(SefValor valor) {
    static const char *nomes[] = {"NULL",
                                  "INTEGER",
                                  "FLOAT",
                                  "STRING",
                                  "SYMBOL",
                                  "CONS",
                                  "COMPILED-FUNCTION",
                                  "FUNCTION",
                                  "SEFIRAH::ENVIRONMENT",
                                  "CONDITION",
                                  "PACKAGE",
                                  "STREAM",
                                  "SEFIRAH::SHARED-LIBRARY",
                                  "VECTOR",
                                  "CHARACTER",
                                  "HASH-TABLE",
                                  "RESTART"};
    if (valor == NULL)
        return "INVALID";
    if (valor->tipo == SEF_TIPO_FUNCAO && valor->como.funcao.compilada_i64 != NULL)
        return "COMPILED-FUNCTION";
    return nomes[valor->tipo];
}

bool sef_valor_e_inteiro(SefValor valor) {
    return valor != NULL && valor->tipo == SEF_TIPO_INTEIRO;
}

long long sef_valor_como_inteiro(SefValor valor) {
    return sef_valor_e_inteiro(valor) ? (long long)valor->como.inteiro : 0;
}

bool sef_valor_e_nulo(SefRuntime *runtime, SefValor valor) {
    return runtime != NULL && valor == runtime->nulo;
}

bool sef_valor_e_vetor(SefValor valor) { return valor != NULL && valor->tipo == SEF_TIPO_VETOR; }

SefValor sef_vetor_criar(SefRuntime *runtime, size_t tamanho, SefValor inicial, SefErro *erro) {
    sef_erro_limpar(erro);
    return sef_vetor_novo(runtime, tamanho, inicial, erro);
}

size_t sef_vetor_tamanho(SefValor vetor) {
    return sef_valor_e_vetor(vetor) ? vetor->como.vetor.tamanho : 0;
}

SefValor sef_vetor_obter(SefValor vetor, size_t indice) {
    if (!sef_valor_e_vetor(vetor) || indice >= vetor->como.vetor.tamanho)
        return NULL;
    return vetor->como.vetor.itens[indice];
}

bool sef_vetor_definir(SefValor vetor, size_t indice, SefValor valor, SefErro *erro) {
    sef_erro_limpar(erro);
    if (!sef_valor_e_vetor(vetor)) {
        sef_erro_definir(erro, 0, 0, "value is not a vector");
        return false;
    }
    if (indice >= vetor->como.vetor.tamanho) {
        sef_erro_definir(erro, 0, 0, "vector index out of bounds");
        return false;
    }
    if (valor == NULL) {
        sef_erro_definir(erro, 0, 0, "missing value while modifying vector");
        return false;
    }
    vetor->como.vetor.itens[indice] = valor;
    return true;
}

bool sef_valor_e_caractere(SefValor valor) {
    return valor != NULL && valor->tipo == SEF_TIPO_CARACTERE;
}

SefValor sef_caractere_criar(SefRuntime *runtime, uint32_t codigo, SefErro *erro) {
    sef_erro_limpar(erro);
    return sef_caractere_novo(runtime, codigo, erro);
}

uint32_t sef_caractere_codigo(SefValor caractere) {
    return sef_valor_e_caractere(caractere) ? caractere->como.caractere : 0;
}

static size_t quantidade_vinculos(const SefVinculo *vinculo) {
    size_t quantidade = 0;
    while (vinculo != NULL) {
        quantidade++;
        vinculo = vinculo->proximo;
    }
    return quantidade;
}

size_t sef_valor_quantidade_componentes(const SefRuntime *runtime, SefValor valor) {
    if (runtime == NULL || valor == NULL)
        return 0;
    switch (valor->tipo) {
    case SEF_TIPO_NULO:
        return 2;
    case SEF_TIPO_SIMBOLO:
        return 2;
    case SEF_TIPO_PAR:
        return 2;
    case SEF_TIPO_FUNCAO:
        return 3;
    case SEF_TIPO_AMBIENTE:
        return 1 + 2 * quantidade_vinculos(valor->como.ambiente.vinculos) +
               2 * quantidade_vinculos(valor->como.ambiente.funcoes);
    case SEF_TIPO_CONDICAO:
        return 2;
    case SEF_TIPO_PACOTE:
        return valor->como.pacote.quantidade_simbolos + valor->como.pacote.quantidade_usados +
               valor->como.pacote.quantidade_exportados;
    case SEF_TIPO_VETOR:
        return valor->como.vetor.tamanho;
    case SEF_TIPO_TABELA_HASH:
        return 2 * valor->como.tabela_hash.quantidade;
    case SEF_TIPO_REINICIO:
        return 2;
    default:
        return 0;
    }
}

static bool definir_rotulo(char *rotulo, size_t capacidade, const char *formato, ...) {
    if (rotulo == NULL || capacidade == 0)
        return false;
    va_list argumentos;
    va_start(argumentos, formato);
    int tamanho = vsnprintf(rotulo, capacidade, formato, argumentos);
    va_end(argumentos);
    return tamanho >= 0 && (size_t)tamanho < capacidade;
}

static bool componente_vinculo(SefVinculo *vinculo, size_t indice, const char *categoria,
                               SefValor *componente, char *rotulo, size_t capacidade_rotulo) {
    size_t numero = indice / 2;
    for (size_t i = 0; i < numero && vinculo != NULL; i++)
        vinculo = vinculo->proximo;
    if (vinculo == NULL)
        return false;
    bool simbolo = indice % 2 == 0;
    *componente = simbolo ? vinculo->simbolo : vinculo->valor;
    return definir_rotulo(rotulo, capacidade_rotulo, "%s %zu %s", categoria, numero + 1,
                          simbolo ? "SYMBOL" : "VALUE");
}

static bool componente_hash(SefValor valor, size_t indice, SefValor *componente, char *rotulo,
                            size_t capacidade_rotulo) {
    size_t numero = indice / 2;
    size_t encontrado = 0;
    for (size_t i = 0; i < valor->como.tabela_hash.capacidade; i++) {
        SefEntradaHash *entrada = &valor->como.tabela_hash.entradas[i];
        if (entrada->estado != SEF_ENTRADA_HASH_OCUPADA)
            continue;
        if (encontrado++ != numero)
            continue;
        bool chave = indice % 2 == 0;
        *componente = chave ? entrada->chave : entrada->valor;
        return definir_rotulo(rotulo, capacidade_rotulo, "%s %zu", chave ? "KEY" : "VALUE",
                              numero + 1);
    }
    return false;
}

static bool componente_propriedades(const SefRuntime *runtime, SefValor simbolo,
                                    SefValor *componente, char *rotulo, size_t capacidade_rotulo) {
    SefErro erro;
    sef_erro_limpar(&erro);
    *componente = sef_simbolo_lista_propriedades((SefRuntime *)runtime, simbolo, &erro);
    return *componente != NULL && definir_rotulo(rotulo, capacidade_rotulo, "PROPERTIES");
}

bool sef_valor_componente(const SefRuntime *runtime, SefValor valor, size_t indice,
                          SefValor *componente, char *rotulo, size_t capacidade_rotulo) {
    if (runtime == NULL || valor == NULL || componente == NULL || rotulo == NULL ||
        capacidade_rotulo == 0 || indice >= sef_valor_quantidade_componentes(runtime, valor))
        return false;
    switch (valor->tipo) {
    case SEF_TIPO_NULO:
        if (indice == 0) {
            *componente = runtime->pacote_common_lisp;
            return definir_rotulo(rotulo, capacidade_rotulo, "PACKAGE");
        }
        return componente_propriedades(runtime, valor, componente, rotulo, capacidade_rotulo);
    case SEF_TIPO_SIMBOLO:
        if (indice == 0) {
            *componente = sef_simbolo_nao_internado(runtime, valor) ? runtime->nulo
                                                                    : valor->como.simbolo.pacote;
            return definir_rotulo(rotulo, capacidade_rotulo, "PACKAGE");
        }
        return componente_propriedades(runtime, valor, componente, rotulo, capacidade_rotulo);
    case SEF_TIPO_PAR:
        *componente = indice == 0 ? valor->como.par.primeiro : valor->como.par.resto;
        return definir_rotulo(rotulo, capacidade_rotulo, "%s", indice == 0 ? "CAR" : "CDR");
    case SEF_TIPO_FUNCAO: {
        SefValor componentes[] = {valor->como.funcao.parametros, valor->como.funcao.corpo,
                                  valor->como.funcao.ambiente};
        const char *rotulos[] = {"PARAMETERS", "BODY", "ENVIRONMENT"};
        *componente = componentes[indice];
        return definir_rotulo(rotulo, capacidade_rotulo, "%s", rotulos[indice]);
    }
    case SEF_TIPO_AMBIENTE: {
        if (indice == 0) {
            *componente = valor->como.ambiente.pai;
            return definir_rotulo(rotulo, capacidade_rotulo, "PARENT");
        }
        indice--;
        size_t quantidade_variaveis = quantidade_vinculos(valor->como.ambiente.vinculos);
        if (indice < 2 * quantidade_variaveis)
            return componente_vinculo(valor->como.ambiente.vinculos, indice, "VARIABLE", componente,
                                      rotulo, capacidade_rotulo);
        return componente_vinculo(valor->como.ambiente.funcoes, indice - 2 * quantidade_variaveis,
                                  "FUNCTION", componente, rotulo, capacidade_rotulo);
    }
    case SEF_TIPO_CONDICAO:
        *componente = indice == 0 ? valor->como.condicao.classe : valor->como.condicao.mensagem;
        return definir_rotulo(rotulo, capacidade_rotulo, "%s", indice == 0 ? "CLASS" : "MESSAGE");
    case SEF_TIPO_PACOTE:
        if (indice < valor->como.pacote.quantidade_simbolos) {
            *componente = valor->como.pacote.simbolos[indice];
            return definir_rotulo(rotulo, capacidade_rotulo, "SYMBOL %zu", indice + 1);
        }
        indice -= valor->como.pacote.quantidade_simbolos;
        if (indice < valor->como.pacote.quantidade_usados) {
            *componente = valor->como.pacote.usados[indice];
            return definir_rotulo(rotulo, capacidade_rotulo, "USES %zu", indice + 1);
        }
        indice -= valor->como.pacote.quantidade_usados;
        *componente = valor->como.pacote.exportados[indice];
        return definir_rotulo(rotulo, capacidade_rotulo, "EXPORTS %zu", indice + 1);
    case SEF_TIPO_VETOR:
        *componente = valor->como.vetor.itens[indice];
        return definir_rotulo(rotulo, capacidade_rotulo, "[%zu]", indice);
    case SEF_TIPO_TABELA_HASH:
        return componente_hash(valor, indice, componente, rotulo, capacidade_rotulo);
    case SEF_TIPO_REINICIO:
        if (indice == 0) {
            *componente = valor->como.reinicio.nome;
            return definir_rotulo(rotulo, capacidade_rotulo, "NAME");
        }
        *componente = runtime->nulo;
        for (SefReinicioDinamico *ativo = runtime->reinicios; ativo != NULL;
             ativo = ativo->anterior) {
            if (ativo->objeto == valor) {
                *componente = runtime->verdadeiro;
                break;
            }
        }
        return definir_rotulo(rotulo, capacidade_rotulo, "ACTIVE");
    default:
        return false;
    }
}
