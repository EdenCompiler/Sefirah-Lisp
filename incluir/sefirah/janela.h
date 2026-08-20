#ifndef SEFIRAH_JANELA_H
#define SEFIRAH_JANELA_H

#include "sefirah/graficos.h"

typedef struct SefConfigJanela {
    const char *titulo;
    int largura;
    int altura;
} SefConfigJanela;

typedef void (*SefAoDesenhar)(SefSuperficie *superficie, void *dados);

typedef enum SefTipoEventoJanela {
    SEF_EVENTO_TEXTO,
    SEF_EVENTO_ENTER,
    SEF_EVENTO_APAGAR,
    SEF_EVENTO_TAB,
    SEF_EVENTO_ATIVAR,
    SEF_EVENTO_EXECUTAR,
    SEF_EVENTO_EXECUTAR_FORMA,
    SEF_EVENTO_EXECUTAR_ALTERACOES,
    SEF_EVENTO_NAVEGAR_DEFINICAO,
    SEF_EVENTO_IR_PARA_DEFINICAO,
    SEF_EVENTO_NAVEGAR_REFERENCIA,
    SEF_EVENTO_SALVAR_IMAGEM,
    SEF_EVENTO_RESTAURAR_IMAGEM,
    SEF_EVENTO_DESFAZER,
    SEF_EVENTO_REFAZER,
    SEF_EVENTO_SALVAR,
    SEF_EVENTO_ABRIR,
    SEF_EVENTO_ABRIR_RAPIDO,
    SEF_EVENTO_PALETA_COMANDOS,
    SEF_EVENTO_BUSCAR_SIMBOLOS,
    SEF_EVENTO_BUSCAR_EDITOR,
    SEF_EVENTO_CANCELAR,
    SEF_EVENTO_CURSOR_ESQUERDA,
    SEF_EVENTO_CURSOR_DIREITA,
    SEF_EVENTO_CURSOR_CIMA,
    SEF_EVENTO_CURSOR_BAIXO,
    SEF_EVENTO_CURSOR_INICIO,
    SEF_EVENTO_CURSOR_FIM,
    SEF_EVENTO_PONTEIRO_MOVER,
    SEF_EVENTO_PONTEIRO_PRESSIONAR,
    SEF_EVENTO_PONTEIRO_SOLTAR
} SefTipoEventoJanela;

typedef struct SefEventoJanela {
    SefTipoEventoJanela tipo;
    char texto_utf8[8];
    int x;
    int y;
    bool modificador_shift;
} SefEventoJanela;

/* Retorna verdadeiro quando o evento alterou algo que precisa ser redesenhado. */
typedef bool (*SefAoEvento)(const SefEventoJanela *evento, void *dados);

int sef_janela_executar(const SefConfigJanela *configuracao, SefAoDesenhar ao_desenhar,
                        SefAoEvento ao_evento, void *dados, char *mensagem_erro,
                        int capacidade_erro);

#endif
