#ifndef SEFIRAH_IDE_IDE_H
#define SEFIRAH_IDE_IDE_H

#include "sefirah/runtime.h"

#include <stdbool.h>

typedef struct SefSessaoIde SefSessaoIde;

typedef enum SefMovimentoCursorIde {
    SEF_CURSOR_ESQUERDA,
    SEF_CURSOR_DIREITA,
    SEF_CURSOR_CIMA,
    SEF_CURSOR_BAIXO,
    SEF_CURSOR_INICIO_LINHA,
    SEF_CURSOR_FIM_LINHA
} SefMovimentoCursorIde;

typedef enum SefMovimentoHistoricoIde {
    SEF_HISTORICO_ANTERIOR,
    SEF_HISTORICO_PROXIMO
} SefMovimentoHistoricoIde;

typedef enum SefMovimentoInspetorIde {
    SEF_INSPETOR_ANTERIOR,
    SEF_INSPETOR_PROXIMO
} SefMovimentoInspetorIde;

typedef enum SefMovimentoComponenteIde {
    SEF_COMPONENTE_INSPETOR_ANTERIOR,
    SEF_COMPONENTE_INSPETOR_PROXIMO
} SefMovimentoComponenteIde;

typedef enum SefMovimentoDefinicaoIde {
    SEF_DEFINICAO_ANTERIOR,
    SEF_DEFINICAO_PROXIMA
} SefMovimentoDefinicaoIde;

typedef enum SefMovimentoReferenciaIde {
    SEF_REFERENCIA_ANTERIOR,
    SEF_REFERENCIA_PROXIMA
} SefMovimentoReferenciaIde;

typedef enum SefMovimentoCondicaoIde {
    SEF_CONDICAO_ANTERIOR,
    SEF_CONDICAO_PROXIMA
} SefMovimentoCondicaoIde;

SefSessaoIde *sef_sessao_ide_criar(SefErro *erro);
void sef_sessao_ide_destruir(SefSessaoIde *sessao);

const char *sef_sessao_ide_editor(const SefSessaoIde *sessao);
const char *sef_sessao_ide_ouvinte(const SefSessaoIde *sessao);
const char *sef_sessao_ide_transcricao(const SefSessaoIde *sessao);
const char *sef_sessao_ide_inspetor(const SefSessaoIde *sessao);
const char *sef_sessao_ide_navegador(const SefSessaoIde *sessao);
const char *sef_sessao_ide_depurador(const SefSessaoIde *sessao);
const char *sef_sessao_ide_estado(const SefSessaoIde *sessao);
const char *sef_sessao_ide_caminho(const SefSessaoIde *sessao);
size_t sef_sessao_ide_cursor_editor(const SefSessaoIde *sessao);

bool sef_sessao_ide_editor_definir(SefSessaoIde *sessao, const char *codigo, SefErro *erro);
bool sef_sessao_ide_editor_inserir(SefSessaoIde *sessao, const char *texto, SefErro *erro);
void sef_sessao_ide_editor_apagar(SefSessaoIde *sessao);
void sef_sessao_ide_editor_mover_cursor(SefSessaoIde *sessao, SefMovimentoCursorIde movimento);
bool sef_sessao_ide_editor_nova_linha(SefSessaoIde *sessao, SefErro *erro);
bool sef_sessao_ide_editor_desfazer(SefSessaoIde *sessao, SefErro *erro);
bool sef_sessao_ide_editor_refazer(SefSessaoIde *sessao, SefErro *erro);

bool sef_sessao_ide_ouvinte_inserir(SefSessaoIde *sessao, const char *texto, SefErro *erro);
void sef_sessao_ide_ouvinte_apagar(SefSessaoIde *sessao);
bool sef_sessao_ide_ouvinte_enviar(SefSessaoIde *sessao, SefErro *erro);
bool sef_sessao_ide_ouvinte_mover_historico(SefSessaoIde *sessao,
                                            SefMovimentoHistoricoIde movimento, SefErro *erro);

bool sef_sessao_ide_executar_editor(SefSessaoIde *sessao, SefErro *erro);
bool sef_sessao_ide_executar_forma_no_cursor(SefSessaoIde *sessao, SefErro *erro);
bool sef_sessao_ide_executar_alteracoes(SefSessaoIde *sessao, SefErro *erro);
bool sef_sessao_ide_navegar_definicao(SefSessaoIde *sessao, SefMovimentoDefinicaoIde movimento,
                                      SefErro *erro);
bool sef_sessao_ide_ir_para_definicao(SefSessaoIde *sessao, SefErro *erro);
bool sef_sessao_ide_navegar_referencia(SefSessaoIde *sessao, SefMovimentoReferenciaIde movimento,
                                       SefErro *erro);
bool sef_sessao_ide_navegar_condicao(SefSessaoIde *sessao, SefMovimentoCondicaoIde movimento,
                                     SefErro *erro);
bool sef_sessao_ide_inspecionar_condicao(SefSessaoIde *sessao, SefErro *erro);
bool sef_sessao_ide_inspetor_mover(SefSessaoIde *sessao, SefMovimentoInspetorIde movimento,
                                   SefErro *erro);
bool sef_sessao_ide_inspetor_mover_componente(SefSessaoIde *sessao,
                                              SefMovimentoComponenteIde movimento, SefErro *erro);
bool sef_sessao_ide_inspetor_entrar(SefSessaoIde *sessao, SefErro *erro);
bool sef_sessao_ide_inspetor_voltar(SefSessaoIde *sessao, SefErro *erro);
bool sef_sessao_ide_imagem_salvar(SefSessaoIde *sessao, SefErro *erro);
bool sef_sessao_ide_imagem_restaurar(SefSessaoIde *sessao, SefErro *erro);
bool sef_sessao_ide_salvar(SefSessaoIde *sessao, const char *caminho, SefErro *erro);
bool sef_sessao_ide_abrir(SefSessaoIde *sessao, const char *caminho, SefErro *erro);

int sef_ide_executar(const char *caminho_inicial);

#endif
