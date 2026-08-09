#include "ide.h"

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
    SefComponente raiz;
    SefComponente area_principal;
    SefComponente editor;
    SefComponente inspetor;
    SefComponente ouvinte;
} EstadoIde;

static bool iniciar_componentes(EstadoIde *estado) {
    estado->tema = sef_tema_gui_classico();
    sef_componente_iniciar(&estado->raiz, SEF_COMPONENTE_PAINEL, NULL);
    sef_componente_iniciar(&estado->area_principal, SEF_COMPONENTE_PAINEL, NULL);
    sef_componente_iniciar(&estado->editor, SEF_COMPONENTE_PAINEL, "EDITOR ESTRUTURAL");
    sef_componente_iniciar(&estado->inspetor, SEF_COMPONENTE_PAINEL, "INSPETOR");
    sef_componente_iniciar(&estado->ouvinte, SEF_COMPONENTE_PAINEL, "OUVINTE");
    estado->raiz.espacamento = 10;
    estado->area_principal.direcao = SEF_LAYOUT_LINHA;
    estado->area_principal.espacamento = 10;
    estado->area_principal.peso = 2;
    estado->editor.peso = 2;
    estado->inspetor.peso = 1;
    estado->ouvinte.peso = 1;
    return sef_componente_adicionar(&estado->raiz, &estado->area_principal) &&
           sef_componente_adicionar(&estado->raiz, &estado->ouvinte) &&
           sef_componente_adicionar(&estado->area_principal, &estado->editor) &&
           sef_componente_adicionar(&estado->area_principal, &estado->inspetor);
}

static void painel(SefSuperficie *s, int x, int y, int largura, int altura, const char *titulo) {
    const SefCor fundo = SEF_COR(244, 238, 211);
    const SefCor tinta = SEF_COR(43, 54, 45);
    const SefCor titulo_fundo = SEF_COR(177, 196, 154);
    sef_superficie_retangulo(s, x, y, largura, altura, fundo);
    sef_superficie_contorno(s, x, y, largura, altura, 2, tinta);
    sef_superficie_retangulo(s, x + 2, y + 2, largura - 4, 24, titulo_fundo);
    sef_superficie_linha(s, x + 2, y + 27, x + largura - 3, y + 27, tinta);
    sef_superficie_texto(s, x + 9, y + 8, titulo, 1, tinta);
}

static void desenhar_ide(SefSuperficie *s, void *dados) {
    EstadoIde *estado = dados;
    const SefCor fundo = SEF_COR(207, 198, 164);
    const SefCor tinta = SEF_COR(43, 54, 45);
    const SefCor destaque = SEF_COR(107, 39, 55);
    const SefCor papel = SEF_COR(244, 238, 211);
    sef_superficie_limpar(s, fundo);

    sef_superficie_retangulo(s, 0, 0, s->largura, 30, tinta);
    sef_superficie_texto(s, 12, 8, "SEFIRAH LISP  SISTEMA VIVO", 2, SEF_COR(231, 218, 168));

    int rodape = 34;
    sef_componente_organizar(
        &estado->raiz, (SefRetangulo){0, 30, s->largura, s->altura - 30 - rodape}, &estado->tema);
    sef_componente_desenhar(&estado->raiz, s, &estado->tema);
    SefRetangulo editor = estado->editor.limites;
    SefRetangulo inspetor = estado->inspetor.limites;
    SefRetangulo ouvinte = estado->ouvinte.limites;

    painel(s, editor.x, editor.y, editor.largura, editor.altura, "EDITOR ESTRUTURAL");
    sef_superficie_texto(s, editor.x + 14, editor.y + 42,
                         "(DEFUN SAUDAR (NOME)\n  (PRINT (LIST 'OLA NOME)))", 2, tinta);
    sef_superficie_retangulo(s, editor.x + 12, editor.y + 99, 8, 18, destaque);

    painel(s, inspetor.x, inspetor.y, inspetor.largura, inspetor.altura, "INSPETOR");
    sef_superficie_texto(s, inspetor.x + 12, inspetor.y + 42,
                         "OBJETO: FUNCTION\nNOME: SAUDAR\nESTADO: COMPILAVEL", 1, tinta);

    painel(s, ouvinte.x, ouvinte.y, ouvinte.largura, ouvinte.altura, "OUVINTE");
    sef_superficie_texto(s, ouvinte.x + 14, ouvinte.y + 40, estado->transcricao, 2, tinta);
    int linha_entrada = 0;
    for (const char *p = estado->transcricao; *p != '\0'; p++) {
        if (*p == '\n')
            linha_entrada++;
    }
    int cursor_x = ouvinte.x + 14 + (int)(strlen("SEFIRAH> ") + estado->tamanho_entrada) * 12;
    int cursor_y = ouvinte.y + 40 + linha_entrada * 18;
    sef_superficie_retangulo(s, cursor_x, cursor_y, 7, 14, destaque);

    sef_superficie_retangulo(s, 0, s->altura - rodape, s->largura, rodape, tinta);
    sef_superficie_texto(s, 12, s->altura - rodape + 10,
                         "PRONTO  IMAGEM: DESENVOLVIMENTO  ESC FECHA", 1, papel);
}

static void atualizar_transcricao(EstadoIde *estado, const char *resultado) {
    snprintf(estado->transcricao, sizeof(estado->transcricao), "SEFIRAH> %s\n%s\nSEFIRAH> ",
             estado->entrada, resultado);
}

static void apagar_ultimo_utf8(EstadoIde *estado) {
    if (estado->tamanho_entrada == 0)
        return;
    do {
        estado->tamanho_entrada--;
    } while (estado->tamanho_entrada > 0 &&
             ((unsigned char)estado->entrada[estado->tamanho_entrada] & 0xc0u) == 0x80u);
    estado->entrada[estado->tamanho_entrada] = '\0';
}

static bool tratar_evento(const SefEventoJanela *evento, void *dados) {
    EstadoIde *estado = dados;
    bool alterou = false;
    if (evento->tipo == SEF_EVENTO_APAGAR) {
        apagar_ultimo_utf8(estado);
        alterou = true;
    } else if (evento->tipo == SEF_EVENTO_TEXTO) {
        size_t tamanho = strlen(evento->texto_utf8);
        if (estado->tamanho_entrada + tamanho + 1 < sizeof(estado->entrada)) {
            memcpy(estado->entrada + estado->tamanho_entrada, evento->texto_utf8, tamanho + 1);
            estado->tamanho_entrada += tamanho;
            alterou = true;
        }
    } else if (evento->tipo == SEF_EVENTO_ENTER) {
        if (estado->tamanho_entrada == 0)
            return false;
        SefErro erro;
        SefValor valor = sef_runtime_avaliar_texto(estado->runtime, estado->entrada, &erro);
        if (valor == NULL) {
            char mensagem[640];
            snprintf(mensagem, sizeof(mensagem), "ERRO: %s", erro.mensagem);
            atualizar_transcricao(estado, mensagem);
        } else {
            char *texto = sef_valor_para_texto(estado->runtime, valor, true, &erro);
            atualizar_transcricao(estado, texto == NULL ? erro.mensagem : texto);
            sef_texto_liberar(texto);
        }
        estado->tamanho_entrada = 0;
        estado->entrada[0] = '\0';
        alterou = true;
    }

    if (alterou && evento->tipo != SEF_EVENTO_ENTER) {
        snprintf(estado->transcricao, sizeof(estado->transcricao), "SEFIRAH> %s", estado->entrada);
    }
    return alterou;
}

int sef_ide_executar(void) {
    SefConfigJanela configuracao = {"Sefirah Lisp — ambiente vivo", 1040, 720};
    SefErro erro_runtime;
    EstadoIde estado = {0};
    estado.runtime = sef_runtime_criar(&erro_runtime);
    if (estado.runtime == NULL) {
        fprintf(stderr, "Erro ao iniciar runtime da IDE: %s\n", erro_runtime.mensagem);
        return 1;
    }
    if (!iniciar_componentes(&estado)) {
        fprintf(stderr, "Erro ao montar componentes da IDE\n");
        sef_runtime_destruir(estado.runtime);
        return 1;
    }
    strcpy(estado.transcricao, "SEFIRAH> ");
    char erro[512] = {0};
    int resultado = sef_janela_executar(&configuracao, desenhar_ide, tratar_evento, &estado, erro,
                                        (int)sizeof(erro));
    if (resultado != 0)
        fprintf(stderr, "Erro grafico: %s\n", erro);
    sef_componente_liberar(&estado.raiz);
    sef_runtime_destruir(estado.runtime);
    return resultado;
}
