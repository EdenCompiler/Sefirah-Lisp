#include "ide/ide.h"

#include "sefirah/gui.h"
#include "sefirah/janela.h"
#include "sefirah/runtime.h"

#include <stdio.h>
#include <string.h>

typedef struct EstadoIde {
    SefRuntime *runtime;
    char entrada[1024];
    size_t tamanho_entrada;
    char transcricao[4096];
    SefTemaGui tema;
    SefComponente raiz, area_principal, editor, inspetor, ouvinte;
} EstadoIde;

static bool iniciar_componentes(EstadoIde *estado) {
    estado->tema = sef_tema_gui_classico();
    sef_componente_iniciar(&estado->raiz, SEF_COMPONENTE_PAINEL, NULL);
    sef_componente_iniciar(&estado->area_principal, SEF_COMPONENTE_PAINEL, NULL);
    sef_componente_iniciar(&estado->editor, SEF_COMPONENTE_PAINEL, "EDITOR ESTRUTURAL");
    sef_componente_iniciar(&estado->inspetor, SEF_COMPONENTE_PAINEL, "INSPETOR");
    sef_componente_iniciar(&estado->ouvinte, SEF_COMPONENTE_PAINEL, "OUVINTE");
    estado->raiz.espacamento = estado->area_principal.espacamento = 10;
    estado->area_principal.direcao = SEF_LAYOUT_LINHA;
    estado->area_principal.peso = estado->editor.peso = 2;
    estado->inspetor.peso = estado->ouvinte.peso = 1;
    return sef_componente_adicionar(&estado->raiz, &estado->area_principal) &&
           sef_componente_adicionar(&estado->raiz, &estado->ouvinte) &&
           sef_componente_adicionar(&estado->area_principal, &estado->editor) &&
           sef_componente_adicionar(&estado->area_principal, &estado->inspetor);
}

static void painel(SefSuperficie *s, SefRetangulo r, const char *titulo) {
    const SefCor tinta = SEF_COR(43, 54, 45);
    sef_superficie_retangulo(s, r.x, r.y, r.largura, r.altura, SEF_COR(244, 238, 211));
    sef_superficie_contorno(s, r.x, r.y, r.largura, r.altura, 2, tinta);
    sef_superficie_retangulo(s, r.x + 2, r.y + 2, r.largura - 4, 24, SEF_COR(177, 196, 154));
    sef_superficie_texto(s, r.x + 9, r.y + 8, titulo, 1, tinta);
}

static void desenhar_ide(SefSuperficie *s, void *dados) {
    EstadoIde *e = dados;
    const SefCor tinta = SEF_COR(43, 54, 45);
    sef_superficie_limpar(s, SEF_COR(207, 198, 164));
    sef_superficie_retangulo(s, 0, 0, s->largura, 30, tinta);
    sef_superficie_texto(s, 12, 8, "SEFIRAH LISP  SISTEMA VIVO", 2, SEF_COR(231, 218, 168));
    sef_componente_organizar(&e->raiz, (SefRetangulo){0, 30, s->largura, s->altura - 64}, &e->tema);
    SefRetangulo editor = e->editor.limites, inspetor = e->inspetor.limites, ouvinte = e->ouvinte.limites;
    painel(s, editor, "EDITOR ESTRUTURAL");
    painel(s, inspetor, "INSPETOR");
    painel(s, ouvinte, "OUVINTE");
    sef_superficie_texto(s, editor.x + 14, editor.y + 42, "(DEFUN SAUDAR (NOME)\n  (PRINT (LIST 'OLA NOME)))", 2, tinta);
    sef_superficie_texto(s, inspetor.x + 12, inspetor.y + 42, "OBJETO: FUNCTION\nNOME: SAUDAR\nESTADO: COMPILAVEL", 1, tinta);
    sef_superficie_texto(s, ouvinte.x + 14, ouvinte.y + 40, e->transcricao, 2, tinta);
}

static bool tratar_evento(const SefEventoJanela *evento, void *dados) {
    EstadoIde *e = dados;
    if (evento->tipo == SEF_EVENTO_TEXTO) {
        size_t n = strlen(evento->texto_utf8);
        if (e->tamanho_entrada + n + 1 < sizeof(e->entrada)) {
            memcpy(e->entrada + e->tamanho_entrada, evento->texto_utf8, n + 1);
            e->tamanho_entrada += n;
        }
    } else if (evento->tipo == SEF_EVENTO_APAGAR && e->tamanho_entrada > 0) {
        e->entrada[--e->tamanho_entrada] = '\0';
    } else if (evento->tipo == SEF_EVENTO_ENTER && e->tamanho_entrada > 0) {
        SefErro erro;
        SefValor valor = sef_runtime_avaliar_texto(e->runtime, e->entrada, &erro);
        char *texto = valor == NULL ? NULL : sef_valor_para_texto(e->runtime, valor, true, &erro);
        snprintf(e->transcricao, sizeof(e->transcricao), "SEFIRAH> %s\n%s\nSEFIRAH> ", e->entrada,
                 texto == NULL ? erro.mensagem : texto);
        sef_texto_liberar(texto);
        e->entrada[0] = '\0'; e->tamanho_entrada = 0;
    }
    if (evento->tipo != SEF_EVENTO_ENTER)
        snprintf(e->transcricao, sizeof(e->transcricao), "SEFIRAH> %s", e->entrada);
    return true;
}

int sef_ide_executar(void) {
    SefErro erro; EstadoIde estado = {0};
    estado.runtime = sef_runtime_criar(&erro);
    if (estado.runtime == NULL || !iniciar_componentes(&estado)) return 1;
    strcpy(estado.transcricao, "SEFIRAH> ");
    char mensagem[512] = {0};
    int resultado = sef_janela_executar(&(SefConfigJanela){"Sefirah Lisp — ambiente vivo", 1040, 720}, desenhar_ide, tratar_evento, &estado, mensagem, sizeof(mensagem));
    sef_componente_liberar(&estado.raiz);
    sef_runtime_destruir(estado.runtime);
    return resultado;
}
