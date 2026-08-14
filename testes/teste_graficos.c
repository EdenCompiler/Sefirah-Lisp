#include "sefirah/graficos.h"
#include "sefirah/gui.h"

#include <stdint.h>
#include <stdio.h>

static int acionamentos = 0;

static void contar_acionamento(void *dados) {
    int *contador = dados;
    (*contador)++;
}

static uint64_t capturar_glifo(SefSuperficie *superficie, const char *texto, SefCor fundo,
                               SefCor tinta) {
    uint64_t mascara = 0;
    sef_superficie_limpar(superficie, fundo);
    sef_superficie_texto(superficie, 0, 0, texto, 1, tinta);
    for (int y = 0; y < 7; y++) {
        for (int x = 0; x < 5; x++) {
            if (superficie->pixels[y * superficie->passo + x] == tinta)
                mascara |= UINT64_C(1) << (y * 5 + x);
        }
    }
    return mascara;
}

static bool testar_componentes(SefSuperficie *superficie) {
    SefTemaGui tema = sef_tema_gui_classico();
    SefComponente raiz, rotulo, linha, campo, botao;
    sef_componente_iniciar(&raiz, SEF_COMPONENTE_PAINEL, NULL);
    sef_componente_iniciar(&rotulo, SEF_COMPONENTE_ROTULO, "NOME");
    sef_componente_iniciar(&linha, SEF_COMPONENTE_PAINEL, NULL);
    sef_componente_iniciar(&campo, SEF_COMPONENTE_CAMPO_TEXTO, "SEFIRAH");
    sef_componente_iniciar(&botao, SEF_COMPONENTE_BOTAO, "OK");
    linha.direcao = SEF_LAYOUT_LINHA;
    linha.altura_preferida = 28;
    linha.peso = 0;
    campo.peso = 1;
    botao.largura_preferida = 48;
    botao.ao_acionar = contar_acionamento;
    botao.dados_acao = &acionamentos;
    bool montou =
        sef_componente_adicionar(&raiz, &rotulo) && sef_componente_adicionar(&raiz, &linha) &&
        sef_componente_adicionar(&linha, &campo) && sef_componente_adicionar(&linha, &botao);
    sef_componente_organizar(&raiz, (SefRetangulo){0, 0, 320, 100}, &tema);
    bool layout_valido =
        campo.limites.largura > botao.limites.largura && botao.limites.x > campo.limites.x &&
        sef_componente_em(&raiz, botao.limites.x + 2, botao.limites.y + 2) == &botao;
    SefComponente *foco = sef_componente_focar_proximo(&raiz, NULL, false);
    foco = sef_componente_focar_proximo(&raiz, foco, false);
    bool foco_valido = foco == &botao && botao.tem_foco && !campo.tem_foco;
    bool acionou = sef_componente_acionar(foco) && acionamentos == 1;
    SefInteracaoGui interacao = {foco, NULL};
    SefEventoJanela pressionar = {
        SEF_EVENTO_PONTEIRO_PRESSIONAR, {0}, botao.limites.x + 2, botao.limites.y + 2, false};
    SefEventoJanela soltar = {
        SEF_EVENTO_PONTEIRO_SOLTAR, {0}, botao.limites.x + 2, botao.limites.y + 2, false};
    bool ponteiro = sef_gui_tratar_evento(&raiz, &interacao, &pressionar) &&
                    sef_gui_tratar_evento(&raiz, &interacao, &soltar) && acionamentos == 2;
    sef_componente_desenhar(&raiz, superficie, &tema);
    sef_componente_liberar(&raiz);
    return montou && layout_valido && foco_valido && acionou && ponteiro;
}

int main(void) {
    SefSuperficie superficie = {0};
    if (!sef_superficie_criar(&superficie, 32, 24))
        return 1;
    SefCor fundo = SEF_COR(1, 2, 3);
    SefCor tinta = SEF_COR(200, 100, 50);
    sef_superficie_limpar(&superficie, fundo);
    sef_superficie_retangulo(&superficie, 3, 4, 5, 6, tinta);
    if (superficie.pixels[4 * superficie.passo + 3] != tinta || superficie.pixels[0] != fundo) {
        fprintf(stderr, "rectangle did not respect its bounds\n");
        return 1;
    }
    uint64_t maiuscula = capturar_glifo(&superficie, "A", fundo, tinta);
    uint64_t minuscula = capturar_glifo(&superficie, "a", fundo, tinta);
    if (maiuscula == 0 || minuscula == 0) {
        fprintf(stderr, "bitmap text did not render\n");
        return 1;
    }
    if (maiuscula == minuscula) {
        fprintf(stderr, "uppercase and lowercase glyphs are not visually distinct\n");
        return 1;
    }
    if (!testar_componentes(&superficie)) {
        fprintf(stderr, "GUI components failed\n");
        return 1;
    }
    sef_superficie_destruir(&superficie);
    puts("graphics: all tests passed");
    return 0;
}
