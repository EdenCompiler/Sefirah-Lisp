#ifndef SEFIRAH_GUI_H
#define SEFIRAH_GUI_H

#include "sefirah/janela.h"

#include <stdbool.h>
#include <stddef.h>

typedef struct SefRetangulo {
    int x;
    int y;
    int largura;
    int altura;
} SefRetangulo;

typedef enum SefTipoComponente {
    SEF_COMPONENTE_PAINEL,
    SEF_COMPONENTE_ROTULO,
    SEF_COMPONENTE_BOTAO,
    SEF_COMPONENTE_CAMPO_TEXTO
} SefTipoComponente;

typedef enum SefDirecaoLayout { SEF_LAYOUT_LINHA, SEF_LAYOUT_COLUNA } SefDirecaoLayout;

typedef struct SefTemaGui {
    SefCor fundo;
    SefCor superficie;
    SefCor superficie_destaque;
    SefCor texto;
    SefCor borda;
    SefCor foco;
    int margem;
    int espacamento;
} SefTemaGui;

typedef void (*SefAoAcionar)(void *dados);

typedef struct SefComponente {
    SefTipoComponente tipo;
    const char *texto;
    SefRetangulo limites;
    int largura_preferida;
    int altura_preferida;
    int peso;
    int margem;
    int espacamento;
    SefDirecaoLayout direcao;
    bool visivel;
    bool habilitado;
    bool aceita_foco;
    bool tem_foco;
    struct SefComponente *pai;
    struct SefComponente **filhos;
    size_t quantidade_filhos;
    size_t capacidade_filhos;
    SefAoAcionar ao_acionar;
    void *dados_acao;
} SefComponente;

typedef struct SefInteracaoGui {
    SefComponente *foco;
    SefComponente *pressionado;
} SefInteracaoGui;

SefTemaGui sef_tema_gui_classico(void);
void sef_componente_iniciar(SefComponente *componente, SefTipoComponente tipo, const char *texto);
bool sef_componente_adicionar(SefComponente *pai, SefComponente *filho);
void sef_componente_liberar(SefComponente *componente);
void sef_componente_organizar(SefComponente *raiz, SefRetangulo limites, const SefTemaGui *tema);
void sef_componente_desenhar(const SefComponente *raiz, SefSuperficie *superficie,
                             const SefTemaGui *tema);
SefComponente *sef_componente_em(SefComponente *raiz, int x, int y);
SefComponente *sef_componente_focar_proximo(SefComponente *raiz, SefComponente *atual,
                                            bool retroceder);
bool sef_componente_acionar(SefComponente *componente);
bool sef_gui_tratar_evento(SefComponente *raiz, SefInteracaoGui *interacao,
                           const SefEventoJanela *evento);

#endif
