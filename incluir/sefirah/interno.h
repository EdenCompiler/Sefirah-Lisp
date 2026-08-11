#ifndef SEFIRAH_NUCLEO_INTERNO_H
#define SEFIRAH_NUCLEO_INTERNO_H

/* Contratos privados do nucleo. Este cabecalho nao possui estabilidade de SDK. */

#include "sefirah/runtime.h"

#include <setjmp.h>
#include <stdint.h>

typedef enum SefTipo {
    SEF_TIPO_NULO,
    SEF_TIPO_INTEIRO,
    SEF_TIPO_REAL,
    SEF_TIPO_TEXTO,
    SEF_TIPO_SIMBOLO,
    SEF_TIPO_PAR,
    SEF_TIPO_NATIVA,
    SEF_TIPO_FUNCAO,
    SEF_TIPO_AMBIENTE,
    SEF_TIPO_CONDICAO,
    SEF_TIPO_PACOTE,
    SEF_TIPO_STREAM,
    SEF_TIPO_BIBLIOTECA,
    SEF_TIPO_VETOR,
    SEF_TIPO_CARACTERE,
    SEF_TIPO_TABELA_HASH
} SefTipo;

typedef struct SefRecursoBiblioteca SefRecursoBiblioteca;

typedef SefValor (*SefFuncaoNativa)(SefRuntime *runtime, SefValor argumentos, SefErro *erro);

typedef struct SefVinculo {
    SefValor simbolo;
    SefValor valor;
    struct SefVinculo *proximo;
} SefVinculo;

typedef enum SefEstadoEntradaHash {
    SEF_ENTRADA_HASH_VAZIA,
    SEF_ENTRADA_HASH_OCUPADA,
    SEF_ENTRADA_HASH_REMOVIDA
} SefEstadoEntradaHash;

typedef enum SefEstadoSimboloPacote {
    SEF_SIMBOLO_AUSENTE,
    SEF_SIMBOLO_INTERNO,
    SEF_SIMBOLO_EXTERNO,
    SEF_SIMBOLO_HERDADO
} SefEstadoSimboloPacote;

typedef struct SefEntradaHash {
    SefValor chave;
    SefValor valor;
    SefEstadoEntradaHash estado;
} SefEntradaHash;

typedef struct SefValoresSalvos {
    SefValor *itens;
    size_t quantidade;
} SefValoresSalvos;

typedef enum SefTipoControle {
    SEF_CONTROLE_BLOCO,
    SEF_CONTROLE_CAPTURA,
    SEF_CONTROLE_LIMPEZA
} SefTipoControle;

typedef struct SefQuadroControle {
    SefTipoControle tipo;
    SefValor nome_ou_etiqueta;
    jmp_buf salto;
    struct SefQuadroControle *anterior;
} SefQuadroControle;

struct SefRaiz {
    SefRuntime *runtime;
    SefValor valor;
    struct SefRaiz *anterior;
    struct SefRaiz *proxima;
};

struct SefObjeto {
    SefTipo tipo;
    bool marcado;
    struct SefObjeto *proximo_alocado;
    union {
        int64_t inteiro;
        double real;
        struct {
            char *dados;
            size_t tamanho;
        } texto;
        struct {
            char *nome;
            size_t tamanho;
            SefValor pacote;
        } simbolo;
        struct {
            SefValor primeiro;
            SefValor resto;
        } par;
        struct {
            SefFuncaoNativa funcao;
            const char *nome;
        } nativa;
        struct {
            SefValor parametros;
            SefValor corpo;
            SefValor ambiente;
            bool macro;
            SefFuncaoCompilada *compilada_i64;
        } funcao;
        struct {
            SefValor pai;
            SefVinculo *vinculos;
            SefVinculo *funcoes;
        } ambiente;
        struct {
            SefValor classe;
            SefValor mensagem;
        } condicao;
        struct {
            char *nome;
            SefValor *simbolos;
            size_t quantidade_simbolos;
            size_t capacidade_simbolos;
            SefValor *usados;
            size_t quantidade_usados;
            size_t capacidade_usados;
            SefValor *exportados;
            size_t quantidade_exportados;
            size_t capacidade_exportados;
        } pacote;
        struct {
            FILE *arquivo;
            char *caminho;
            bool possui_arquivo;
            bool fechado;
            unsigned char padrao;
        } stream;
        struct {
            SefRecursoBiblioteca *recurso;
            bool fechada;
        } biblioteca;
        struct {
            SefValor *itens;
            size_t tamanho;
        } vetor;
        uint32_t caractere;
        struct {
            SefEntradaHash *entradas;
            size_t capacidade, quantidade, ocupadas;
        } tabela_hash;
    } como;
};

struct SefRuntime {
    SefValor objetos;
    size_t quantidade_objetos;
    size_t bytes_aproximados;

    SefValor nulo;
    SefValor verdadeiro;
    SefValor ambiente_global;

    SefValor *simbolos;
    size_t quantidade_simbolos;
    size_t capacidade_simbolos;
    SefValor *pacotes;
    size_t quantidade_pacotes;
    size_t capacidade_pacotes;
    SefValor pacote_atual;
    SefValor pacote_common_lisp;
    SefValor pacote_keyword;
    SefValor entrada_padrao;
    SefValor saida_padrao;
    SefValor erro_padrao;

    SefQuadroControle *controle;
    SefQuadroControle *destino_transferencia;
    SefValor valor_transferencia;
    SefValor *valores_multiplos;
    size_t quantidade_valores;
    size_t capacidade_valores;
    uint64_t versao_valores;
    SefValoresSalvos valores_transferencia;
    SefRaiz *raizes;
};

typedef struct SefLeitor {
    SefRuntime *runtime;
    const char *inicio;
    const char *atual;
    size_t linha;
    size_t coluna;
} SefLeitor;

SefValor sef_objeto_novo(SefRuntime *runtime, SefTipo tipo, SefErro *erro);
SefValor sef_inteiro_novo(SefRuntime *runtime, int64_t numero, SefErro *erro);
SefValor sef_real_novo(SefRuntime *runtime, double numero, SefErro *erro);
SefValor sef_texto_novo(SefRuntime *runtime, const char *texto, size_t tamanho, SefErro *erro);
SefValor sef_texto_caractere_obter(SefRuntime *runtime, SefValor texto, size_t indice,
                                   SefErro *erro);
bool sef_texto_caractere_definir(SefRuntime *runtime, SefValor texto, size_t indice,
                                 SefValor caractere, SefErro *erro);
SefValor sef_simbolo_internar(SefRuntime *runtime, const char *nome, size_t tamanho, SefErro *erro);
SefValor sef_simbolo_internar_em(SefRuntime *runtime, SefValor pacote, const char *nome,
                                 size_t tamanho, SefErro *erro);
bool sef_valor_e_simbolo_logico(const SefRuntime *runtime, SefValor valor);
bool sef_simbolo_e_constante(const SefRuntime *runtime, SefValor simbolo);
bool sef_simbolo_nome_logico(const SefRuntime *runtime, SefValor simbolo, const char **nome,
                             size_t *tamanho);
SefValor sef_pacote_novo(SefRuntime *runtime, const char *nome, SefErro *erro);
SefValor sef_pacote_encontrar(SefRuntime *runtime, const char *nome, size_t tamanho);
bool sef_pacote_usar(SefRuntime *runtime, SefValor pacote, SefValor usado, SefErro *erro);
bool sef_pacote_usa(SefValor pacote, SefValor usado);
bool sef_pacote_exportar(SefRuntime *runtime, SefValor pacote, SefValor simbolo, SefErro *erro);
bool sef_pacote_instalar_nulo(SefRuntime *runtime, SefErro *erro);
bool sef_pacote_simbolo_exportado(SefValor pacote, SefValor simbolo);
SefValor sef_pacote_localizar_simbolo(SefValor pacote, const char *nome, size_t tamanho,
                                      bool incluir_herdados);
SefValor sef_pacote_localizar_simbolo_com_estado(SefValor pacote, const char *nome, size_t tamanho,
                                                 bool incluir_herdados,
                                                 SefEstadoSimboloPacote *estado);
SefValor sef_par_novo(SefRuntime *runtime, SefValor primeiro, SefValor resto, SefErro *erro);
SefValor sef_nativa_nova(SefRuntime *runtime, const char *nome, SefFuncaoNativa funcao,
                         SefErro *erro);
SefValor sef_funcao_nova(SefRuntime *runtime, SefValor parametros, SefValor corpo,
                         SefValor ambiente, bool macro, SefErro *erro);
SefValor sef_ambiente_novo(SefRuntime *runtime, SefValor pai, SefErro *erro);
SefValor sef_condicao_nova(SefRuntime *runtime, SefValor classe, const char *mensagem,
                           SefErro *erro);
SefValor sef_stream_novo(SefRuntime *runtime, FILE *arquivo, const char *caminho,
                         bool possui_arquivo, unsigned char padrao, SefErro *erro);
SefValor sef_biblioteca_nova(SefRuntime *runtime, const char *caminho, SefErro *erro);
SefValor sef_vetor_novo(SefRuntime *runtime, size_t tamanho, SefValor inicial, SefErro *erro);
SefValor sef_caractere_novo(SefRuntime *runtime, uint32_t codigo, SefErro *erro);
SefValor sef_tabela_hash_nova(SefRuntime *runtime, SefErro *erro);
bool sef_tabela_hash_inicializar(SefRuntime *runtime, SefValor tabela, SefErro *erro);
bool sef_tabela_hash_definir(SefRuntime *runtime, SefValor tabela, SefValor chave, SefValor valor,
                             SefErro *erro);
SefValor sef_tabela_hash_obter(SefRuntime *runtime, SefValor tabela, SefValor chave,
                               SefValor padrao, bool *encontrou, SefErro *erro);
bool sef_tabela_hash_remover(SefRuntime *runtime, SefValor tabela, SefValor chave, bool *removeu,
                             SefErro *erro);
void sef_tabela_hash_limpar(SefValor tabela);
bool sef_valores_definir(SefRuntime *runtime, const SefValor *valores, size_t quantidade,
                         SefErro *erro);
bool sef_valores_definir_um(SefRuntime *runtime, SefValor valor, SefErro *erro);
bool sef_valores_definir_lista(SefRuntime *runtime, SefValor lista, SefErro *erro);
SefValor sef_valores_primario(const SefRuntime *runtime);
bool sef_valores_salvar(const SefRuntime *runtime, SefValoresSalvos *salvos, SefErro *erro);
bool sef_valores_restaurar(SefRuntime *runtime, const SefValoresSalvos *salvos, SefErro *erro);
void sef_valores_salvos_liberar(SefValoresSalvos *salvos);
bool sef_biblioteca_fechar(SefValor biblioteca, SefErro *erro);
SefRecursoBiblioteca *sef_biblioteca_recurso_abrir(const char *caminho, SefErro *erro);
void sef_biblioteca_recurso_reter(SefRecursoBiblioteca *recurso);
void sef_biblioteca_recurso_liberar(SefRecursoBiblioteca *recurso);
SefFuncaoExternaI64 sef_biblioteca_recurso_resolver(SefRecursoBiblioteca *recurso,
                                                    const char *simbolo, SefErro *erro);
const char *sef_biblioteca_recurso_caminho(const SefRecursoBiblioteca *recurso);
bool sef_funcao_compilada_instalar_i64(SefRuntime *runtime, SefValor simbolo, SefErro *erro);
bool sef_funcao_compilada_instalar_biblioteca_i64(SefRuntime *runtime, SefValor simbolo,
                                                  const char *caminho, SefErro *erro);
bool sef_funcao_compilada_instalar_objeto_biblioteca_i64(SefRuntime *runtime, SefValor simbolo,
                                                         SefValor biblioteca, SefErro *erro);

bool sef_simbolo_tem_nome(SefValor valor, const char *nome);
bool sef_valores_eql(SefValor a, SefValor b);
bool sef_e_lista_propria(SefRuntime *runtime, SefValor valor);
size_t sef_lista_tamanho(SefRuntime *runtime, SefValor lista, bool *propria);
SefValor sef_lista_inverter(SefRuntime *runtime, SefValor lista, SefErro *erro);

bool sef_utf8_decodificar(const char *dados, size_t tamanho, size_t *consumidos, uint32_t *codigo);
size_t sef_utf8_codificar(uint32_t codigo, char saida[4]);
size_t sef_utf8_quantidade(const char *dados, size_t tamanho, bool *valido);
bool sef_utf8_localizar(const char *dados, size_t tamanho, size_t indice, size_t *inicio,
                        size_t *comprimento, uint32_t *codigo);

bool sef_ambiente_definir(SefRuntime *runtime, SefValor ambiente, SefValor simbolo, SefValor valor,
                          SefErro *erro);
bool sef_ambiente_obter(SefValor ambiente, SefValor simbolo, SefValor *valor);
bool sef_ambiente_atribuir(SefValor ambiente, SefValor simbolo, SefValor valor);
bool sef_ambiente_definir_funcao(SefRuntime *runtime, SefValor ambiente, SefValor simbolo,
                                 SefValor valor, SefErro *erro);
bool sef_ambiente_obter_funcao(SefValor ambiente, SefValor simbolo, SefValor *valor);

void sef_leitor_iniciar(SefLeitor *leitor, SefRuntime *runtime, const char *codigo);
SefValor sef_ler_forma(SefLeitor *leitor, bool *encontrou, SefErro *erro);

SefValor sef_avaliar(SefRuntime *runtime, SefValor forma, SefValor ambiente, SefErro *erro);
SefValor sef_aplicar(SefRuntime *runtime, SefValor funcao, SefValor argumentos, SefErro *erro);

bool sef_primitivas_instalar(SefRuntime *runtime, SefErro *erro);
bool sef_primitivas_reconciliar(SefRuntime *runtime, SefErro *erro);
SefFuncaoNativa sef_primitiva_buscar(const char *nome);
const char *sef_primitiva_nome(SefFuncaoNativa funcao);
SefValor sef_primitiva_copy_seq(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_reverse(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_subseq(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_fill(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_consp(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_listp(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_endp(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_first(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_rest(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_rplaca(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_rplacd(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_nth(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_nthcdr(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_last(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_append(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_nconc(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_member(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_assoc(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_mapcar(SefRuntime *runtime, SefValor argumentos, SefErro *erro);
SefValor sef_primitiva_mapc(SefRuntime *runtime, SefValor argumentos, SefErro *erro);

#endif
