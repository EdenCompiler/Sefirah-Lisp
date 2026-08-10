#include "interno.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

SefValor sef_objeto_novo(SefRuntime *runtime, SefTipo tipo, SefErro *erro) {
    SefValor objeto = calloc(1, sizeof(*objeto));
    if (objeto == NULL) {
        sef_erro_definir(erro, 0, 0, "memoria insuficiente ao criar objeto");
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
        sef_erro_definir(erro, 0, 0, "memoria insuficiente ao copiar texto");
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
        sef_erro_definir(erro, 0, 0, "acesso de caractere exige uma string");
        return NULL;
    }
    uint32_t codigo;
    if (!sef_utf8_localizar(texto->como.texto.dados, texto->como.texto.tamanho, indice, NULL, NULL,
                            &codigo)) {
        bool valido = false;
        size_t tamanho =
            sef_utf8_quantidade(texto->como.texto.dados, texto->como.texto.tamanho, &valido);
        sef_erro_definir(erro, 0, 0,
                         valido && indice >= tamanho ? "indice fora dos limites da string"
                                                     : "string contem UTF-8 invalido");
        return NULL;
    }
    return sef_caractere_novo(runtime, codigo, erro);
}

bool sef_texto_caractere_definir(SefRuntime *runtime, SefValor texto, size_t indice,
                                 SefValor caractere, SefErro *erro) {
    if (texto == NULL || texto->tipo != SEF_TIPO_TEXTO || caractere == NULL ||
        caractere->tipo != SEF_TIPO_CARACTERE) {
        sef_erro_definir(erro, 0, 0, "alteracao de string exige string e caractere");
        return false;
    }
    size_t inicio, anterior;
    if (!sef_utf8_localizar(texto->como.texto.dados, texto->como.texto.tamanho, indice, &inicio,
                            &anterior, NULL)) {
        bool valido = false;
        size_t tamanho =
            sef_utf8_quantidade(texto->como.texto.dados, texto->como.texto.tamanho, &valido);
        sef_erro_definir(erro, 0, 0,
                         valido && indice >= tamanho ? "indice fora dos limites da string"
                                                     : "string contem UTF-8 invalido");
        return false;
    }
    char novo[4];
    size_t quantidade_nova = sef_utf8_codificar(caractere->como.caractere, novo);
    size_t tamanho_novo = texto->como.texto.tamanho - anterior + quantidade_nova;
    if (quantidade_nova != anterior) {
        char *novos_dados = malloc(tamanho_novo + 1);
        if (novos_dados == NULL) {
            sef_erro_definir(erro, 0, 0, "memoria insuficiente ao alterar string");
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
        sef_erro_definir(erro, 0, 0, "runtime ou valor inicial ausente ao criar vetor");
        return NULL;
    }
    if (tamanho > SIZE_MAX / sizeof(SefValor)) {
        sef_erro_definir(erro, 0, 0, "vetor grande demais");
        return NULL;
    }
    SefValor valor = sef_objeto_novo(runtime, SEF_TIPO_VETOR, erro);
    if (valor == NULL)
        return NULL;
    if (tamanho > 0) {
        valor->como.vetor.itens = malloc(tamanho * sizeof(SefValor));
        if (valor->como.vetor.itens == NULL) {
            sef_erro_definir(erro, 0, 0, "memoria insuficiente ao criar vetor");
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
        sef_erro_definir(erro, 0, 0, "codigo Unicode invalido ao criar caractere");
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

static bool vetor_valores_crescer(SefValor **valores, size_t *capacidade, size_t minimo,
                                  SefErro *erro) {
    if (*capacidade >= minimo)
        return true;
    size_t nova = *capacidade == 0 ? 8 : *capacidade * 2;
    while (nova < minimo)
        nova *= 2;
    SefValor *realocado = realloc(*valores, nova * sizeof(*realocado));
    if (realocado == NULL) {
        sef_erro_definir(erro, 0, 0, "memoria insuficiente para tabela de objetos");
        return false;
    }
    *valores = realocado;
    *capacidade = nova;
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
        sef_erro_definir(erro, 0, 0, "pacote %s ja existe", nome);
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

static SefValor pacote_buscar_simbolo(SefValor pacote, const char *nome, size_t tamanho);

bool sef_pacote_usar(SefRuntime *runtime, SefValor pacote, SefValor usado, SefErro *erro) {
    (void)runtime;
    if (pacote == NULL || usado == NULL || pacote->tipo != SEF_TIPO_PACOTE ||
        usado->tipo != SEF_TIPO_PACOTE) {
        sef_erro_definir(erro, 0, 0, "USE-PACKAGE recebeu objeto que nao e pacote");
        return false;
    }
    for (size_t i = 0; i < pacote->como.pacote.quantidade_usados; i++) {
        if (pacote->como.pacote.usados[i] == usado)
            return true;
    }
    for (size_t i = 0; i < usado->como.pacote.quantidade_exportados; i++) {
        SefValor candidato = usado->como.pacote.exportados[i];
        SefValor existente = pacote_buscar_simbolo(pacote, candidato->como.simbolo.nome,
                                                   candidato->como.simbolo.tamanho);
        for (size_t j = 0; existente == NULL && j < pacote->como.pacote.quantidade_usados; j++) {
            SefValor origem = pacote->como.pacote.usados[j];
            SefValor herdado = pacote_buscar_simbolo(origem, candidato->como.simbolo.nome,
                                                     candidato->como.simbolo.tamanho);
            if (herdado != NULL && sef_pacote_simbolo_exportado(origem, herdado))
                existente = herdado;
        }
        if (existente != NULL && existente != candidato) {
            sef_erro_definir(erro, 0, 0, "conflito ao usar pacote: simbolo %s ja e acessivel",
                             candidato->como.simbolo.nome);
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
    (void)runtime;
    if (pacote == NULL || pacote->tipo != SEF_TIPO_PACOTE || simbolo == NULL ||
        simbolo->tipo != SEF_TIPO_SIMBOLO || simbolo->como.simbolo.pacote != pacote) {
        sef_erro_definir(erro, 0, 0, "EXPORT exige simbolo interno do pacote");
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
        if (simbolo->como.simbolo.tamanho == tamanho &&
            memcmp(simbolo->como.simbolo.nome, nome, tamanho) == 0)
            return simbolo;
    }
    return NULL;
}

SefValor sef_pacote_localizar_simbolo(SefValor pacote, const char *nome, size_t tamanho,
                                      bool incluir_herdados) {
    char *normalizado = copiar_nome_maiusculo(nome, tamanho);
    if (normalizado == NULL)
        return NULL;
    SefValor encontrado = pacote_buscar_simbolo(pacote, normalizado, tamanho);
    for (size_t i = 0;
         encontrado == NULL && incluir_herdados && i < pacote->como.pacote.quantidade_usados; i++) {
        SefValor usado = pacote->como.pacote.usados[i];
        SefValor candidato = pacote_buscar_simbolo(usado, normalizado, tamanho);
        if (candidato != NULL && sef_pacote_simbolo_exportado(usado, candidato))
            encontrado = candidato;
    }
    free(normalizado);
    return encontrado;
}

SefValor sef_simbolo_internar_em(SefRuntime *runtime, SefValor pacote, const char *nome,
                                 size_t tamanho, SefErro *erro) {
    if (pacote == NULL || pacote->tipo != SEF_TIPO_PACOTE) {
        sef_erro_definir(erro, 0, 0, "pacote invalido ao internar simbolo");
        return NULL;
    }
    char *normalizado = copiar_nome_maiusculo(nome, tamanho);
    if (normalizado == NULL) {
        sef_erro_definir(erro, 0, 0, "memoria insuficiente ao internar simbolo");
        return NULL;
    }

    SefValor existente = pacote_buscar_simbolo(pacote, normalizado, tamanho);
    if (existente != NULL) {
        free(normalizado);
        return existente;
    }
    if (!vetor_valores_crescer(&runtime->simbolos, &runtime->capacidade_simbolos,
                               runtime->quantidade_simbolos + 1, erro) ||
        !vetor_valores_crescer(&pacote->como.pacote.simbolos,
                               &pacote->como.pacote.capacidade_simbolos,
                               pacote->como.pacote.quantidade_simbolos + 1, erro)) {
        free(normalizado);
        return NULL;
    }

    SefValor simbolo = sef_objeto_novo(runtime, SEF_TIPO_SIMBOLO, erro);
    if (simbolo == NULL) {
        free(normalizado);
        return NULL;
    }
    simbolo->como.simbolo.nome = normalizado;
    simbolo->como.simbolo.tamanho = tamanho;
    simbolo->como.simbolo.pacote = pacote;
    runtime->bytes_aproximados += tamanho + 1;
    runtime->simbolos[runtime->quantidade_simbolos++] = simbolo;
    pacote->como.pacote.simbolos[pacote->como.pacote.quantidade_simbolos++] = simbolo;
    return simbolo;
}

SefValor sef_simbolo_internar(SefRuntime *runtime, const char *nome, size_t tamanho,
                              SefErro *erro) {
    if (runtime->pacote_atual == NULL) {
        sef_erro_definir(erro, 0, 0, "nao existe pacote atual");
        return NULL;
    }
    if (tamanho > 1 && nome[0] == ':')
        return sef_simbolo_internar_em(runtime, runtime->pacote_keyword, nome + 1, tamanho - 1,
                                       erro);

    const char *dois_pontos = memchr(nome, ':', tamanho);
    if (dois_pontos != NULL) {
        size_t tamanho_pacote = (size_t)(dois_pontos - nome);
        size_t separadores = dois_pontos + 1 < nome + tamanho && dois_pontos[1] == ':' ? 2 : 1;
        size_t inicio_nome = tamanho_pacote + separadores;
        SefValor pacote = sef_pacote_encontrar(runtime, nome, tamanho_pacote);
        if (pacote == NULL || inicio_nome >= tamanho) {
            sef_erro_definir(erro, 0, 0, "designador de simbolo com pacote invalido");
            return NULL;
        }
        SefValor simbolo =
            sef_pacote_localizar_simbolo(pacote, nome + inicio_nome, tamanho - inicio_nome, false);
        if (separadores == 1) {
            if (simbolo == NULL || !sef_pacote_simbolo_exportado(pacote, simbolo)) {
                sef_erro_definir(erro, 0, 0, "simbolo nao e externo no pacote indicado");
                return NULL;
            }
            return simbolo;
        }
        return simbolo != NULL ? simbolo
                               : sef_simbolo_internar_em(runtime, pacote, nome + inicio_nome,
                                                         tamanho - inicio_nome, erro);
    }

    char *normalizado = copiar_nome_maiusculo(nome, tamanho);
    if (normalizado == NULL) {
        sef_erro_definir(erro, 0, 0, "memoria insuficiente ao internar simbolo");
        return NULL;
    }
    SefValor encontrado = pacote_buscar_simbolo(runtime->pacote_atual, normalizado, tamanho);
    for (size_t i = 0;
         encontrado == NULL && i < runtime->pacote_atual->como.pacote.quantidade_usados; i++) {
        SefValor usado = runtime->pacote_atual->como.pacote.usados[i];
        SefValor candidato = pacote_buscar_simbolo(usado, normalizado, tamanho);
        if (candidato != NULL && sef_pacote_simbolo_exportado(usado, candidato))
            encontrado = candidato;
    }
    free(normalizado);
    return encontrado != NULL
               ? encontrado
               : sef_simbolo_internar_em(runtime, runtime->pacote_atual, nome, tamanho, erro);
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

SefValor sef_stream_novo(SefRuntime *runtime, FILE *arquivo, const char *caminho,
                         bool possui_arquivo, unsigned char padrao, SefErro *erro) {
    SefValor stream = sef_objeto_novo(runtime, SEF_TIPO_STREAM, erro);
    if (stream == NULL)
        return NULL;
    if (caminho != NULL) {
        stream->como.stream.caminho = malloc(strlen(caminho) + 1);
        if (stream->como.stream.caminho == NULL) {
            sef_erro_definir(erro, 0, 0, "memoria insuficiente para caminho de stream");
            return NULL;
        }
        strcpy(stream->como.stream.caminho, caminho);
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
            sef_erro_definir(erro, 0, 0, "nao e uma lista propria");
            return NULL;
        }
        resultado = sef_par_novo(runtime, lista->como.par.primeiro, resultado, erro);
        if (resultado == NULL)
            return NULL;
        lista = lista->como.par.resto;
    }
    return resultado;
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
        sef_erro_definir(erro, 0, 0, "valor nao e um vetor");
        return false;
    }
    if (indice >= vetor->como.vetor.tamanho) {
        sef_erro_definir(erro, 0, 0, "indice fora dos limites do vetor");
        return false;
    }
    if (valor == NULL) {
        sef_erro_definir(erro, 0, 0, "valor ausente ao alterar vetor");
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
