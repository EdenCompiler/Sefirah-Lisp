#include "ide/ide.h"

#include "sefirah/gui.h"
#include "sefirah/janela.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum FocoIde { FOCO_EDITOR, FOCO_INSPETOR, FOCO_DEPURADOR, FOCO_OUVINTE } FocoIde;

typedef struct EstadoIde {
    SefSessaoIde *sessao;
    FocoIde foco;
    SefTemaGui tema;
    SefComponente raiz;
    SefComponente area_principal;
    SefComponente editor;
    SefComponente ferramentas;
    SefComponente inspetor;
    SefComponente navegador;
    SefComponente depurador;
    SefComponente ouvinte;
} EstadoIde;

static bool iniciar_componentes(EstadoIde *estado) {
    estado->tema = sef_tema_gui_classico();
    sef_componente_iniciar(&estado->raiz, SEF_COMPONENTE_PAINEL, NULL);
    sef_componente_iniciar(&estado->area_principal, SEF_COMPONENTE_PAINEL, NULL);
    sef_componente_iniciar(&estado->editor, SEF_COMPONENTE_PAINEL, "EDITOR");
    sef_componente_iniciar(&estado->ferramentas, SEF_COMPONENTE_PAINEL, NULL);
    sef_componente_iniciar(&estado->inspetor, SEF_COMPONENTE_PAINEL, "INSPETOR");
    sef_componente_iniciar(&estado->navegador, SEF_COMPONENTE_PAINEL, "NAVEGADOR");
    sef_componente_iniciar(&estado->depurador, SEF_COMPONENTE_PAINEL, "DEPURADOR");
    sef_componente_iniciar(&estado->ouvinte, SEF_COMPONENTE_PAINEL, "OUVINTE");
    estado->raiz.espacamento = 8;
    estado->area_principal.espacamento = 8;
    estado->area_principal.direcao = SEF_LAYOUT_LINHA;
    estado->ferramentas.espacamento = 8;
    estado->area_principal.peso = 2;
    estado->editor.peso = 3;
    estado->ferramentas.peso = 1;
    estado->inspetor.peso = 1;
    estado->navegador.peso = 1;
    estado->depurador.peso = 1;
    estado->ouvinte.peso = 1;
    return sef_componente_adicionar(&estado->raiz, &estado->area_principal) &&
           sef_componente_adicionar(&estado->raiz, &estado->ouvinte) &&
           sef_componente_adicionar(&estado->area_principal, &estado->editor) &&
           sef_componente_adicionar(&estado->area_principal, &estado->ferramentas) &&
           sef_componente_adicionar(&estado->ferramentas, &estado->inspetor) &&
           sef_componente_adicionar(&estado->ferramentas, &estado->navegador) &&
           sef_componente_adicionar(&estado->ferramentas, &estado->depurador);
}

static bool ponto_dentro(SefRetangulo retangulo, int x, int y) {
    return x >= retangulo.x && y >= retangulo.y && x < retangulo.x + retangulo.largura &&
           y < retangulo.y + retangulo.altura;
}

static void alternar_foco(EstadoIde *estado, bool anterior) {
    if (anterior)
        estado->foco = estado->foco == FOCO_EDITOR ? FOCO_OUVINTE : (FocoIde)(estado->foco - 1);
    else
        estado->foco = estado->foco == FOCO_OUVINTE ? FOCO_EDITOR : (FocoIde)(estado->foco + 1);
}

static void desenhar_painel(SefSuperficie *superficie, SefRetangulo limites, const char *titulo,
                            bool ativo) {
    const SefCor tinta = SEF_COR(43, 54, 45);
    SefCor barra = ativo ? SEF_COR(143, 173, 121) : SEF_COR(177, 196, 154);
    sef_superficie_retangulo(superficie, limites.x, limites.y, limites.largura, limites.altura,
                             SEF_COR(244, 238, 211));
    sef_superficie_contorno(superficie, limites.x, limites.y, limites.largura, limites.altura,
                            ativo ? 3 : 2, tinta);
    sef_superficie_retangulo(superficie, limites.x + 2, limites.y + 2, limites.largura - 4, 28,
                             barra);
    sef_superficie_texto(superficie, limites.x + 9, limites.y + 8, titulo, 2, tinta);
}

static const char *inicio_das_ultimas_linhas(const char *texto, size_t quantidade) {
    const char *fim = texto + strlen(texto);
    const char *cursor = fim;
    while (cursor > texto && quantidade > 0) {
        cursor--;
        if (*cursor == '\n')
            quantidade--;
    }
    if (cursor > texto && *cursor == '\n')
        cursor++;
    return cursor;
}

static const char *inicio_antes_da_posicao(const char *texto, size_t posicao, size_t quantidade) {
    const char *cursor = texto + posicao;
    while (cursor > texto && quantidade > 0) {
        cursor--;
        if (*cursor == '\n')
            quantidade--;
    }
    if (cursor > texto && *cursor == '\n')
        cursor++;
    return cursor;
}

static void limitar_linha(char *texto, size_t capacidade_visual) {
    size_t tamanho = strlen(texto);
    if (tamanho > capacidade_visual)
        texto[capacidade_visual] = '\0';
}

static void desenhar_texto_limitado(SefSuperficie *superficie, SefRetangulo limites,
                                    const char *texto, bool mostrar_final) {
    int largura_util = limites.largura - 24;
    int altura_util = limites.altura - 44;
    if (largura_util <= 0 || altura_util <= 0)
        return;
    size_t colunas = (size_t)(largura_util / 12);
    size_t linhas = (size_t)(altura_util / 18);
    if (colunas == 0 || linhas == 0)
        return;
    if (colunas > 511)
        colunas = 511;
    const char *cursor = mostrar_final ? inicio_das_ultimas_linhas(texto, linhas) : texto;
    char linha[512];
    int y = limites.y + 36;
    for (size_t indice_linha = 0; indice_linha < linhas && *cursor != '\0'; indice_linha++) {
        size_t tamanho = 0;
        while (*cursor != '\0' && *cursor != '\n') {
            if (tamanho < colunas)
                linha[tamanho++] = *cursor;
            cursor++;
        }
        if (*cursor == '\n')
            cursor++;
        linha[tamanho] = '\0';
        sef_superficie_texto(superficie, limites.x + 12, y, linha, 2, SEF_COR(43, 54, 45));
        y += 18;
    }
}

static void desenhar_editor(SefSuperficie *superficie, SefRetangulo limites,
                            const SefSessaoIde *sessao) {
    const char *codigo = sef_sessao_ide_editor(sessao);
    size_t tamanho = strlen(codigo);
    size_t cursor = sef_sessao_ide_cursor_editor(sessao);
    if (cursor > tamanho)
        cursor = tamanho;
    char *com_cursor = malloc(tamanho + 2);
    if (com_cursor == NULL) {
        desenhar_texto_limitado(superficie, limites, codigo, true);
        return;
    }
    memcpy(com_cursor, codigo, cursor);
    com_cursor[cursor] = '|';
    memcpy(com_cursor + cursor + 1, codigo + cursor, tamanho - cursor + 1);
    size_t linhas = limites.altura > 44 ? (size_t)(limites.altura - 44) / 18u : 1;
    size_t linhas_anteriores = cursor == tamanho ? linhas : linhas / 2u + 1u;
    const char *inicio = inicio_antes_da_posicao(com_cursor, cursor + 1, linhas_anteriores);
    desenhar_texto_limitado(superficie, limites, inicio, false);
    free(com_cursor);
}

static void desenhar_ide(SefSuperficie *superficie, void *dados) {
    EstadoIde *estado = dados;
    const SefCor tinta = SEF_COR(43, 54, 45);
    sef_superficie_limpar(superficie, SEF_COR(207, 198, 164));
    sef_superficie_retangulo(superficie, 0, 0, superficie->largura, 30, tinta);
    sef_superficie_texto(superficie, 12, 8, "SEFIRAH LISP  AMBIENTE VIVO", 2,
                         SEF_COR(231, 218, 168));
    sef_componente_organizar(&estado->raiz,
                             (SefRetangulo){0, 30, superficie->largura, superficie->altura - 66},
                             &estado->tema);

    desenhar_painel(superficie, estado->editor.limites, "EDITOR [F5 TUDO] [F6 FORMA] [F7 MUDANCAS]",
                    estado->foco == FOCO_EDITOR);
    desenhar_painel(superficie, estado->inspetor.limites, "INSPETOR [ENTER ABRE] [BACK VOLTA]",
                    estado->foco == FOCO_INSPETOR);
    desenhar_painel(superficie, estado->navegador.limites, "NAVEGADOR [F11 DEF.] [F12 REFS.]",
                    false);
    desenhar_painel(superficie, estado->depurador.limites, "DEPURADOR [ENTER]",
                    estado->foco == FOCO_DEPURADOR);
    desenhar_painel(superficie, estado->ouvinte.limites, "OUVINTE  [ENTER ENVIA] [CIMA HIST.]",
                    estado->foco == FOCO_OUVINTE);
    desenhar_editor(superficie, estado->editor.limites, estado->sessao);
    desenhar_texto_limitado(superficie, estado->inspetor.limites,
                            sef_sessao_ide_inspetor(estado->sessao), false);
    desenhar_texto_limitado(superficie, estado->navegador.limites,
                            sef_sessao_ide_navegador(estado->sessao), false);
    desenhar_texto_limitado(superficie, estado->depurador.limites,
                            sef_sessao_ide_depurador(estado->sessao), false);

    SefRetangulo transcricao = estado->ouvinte.limites;
    transcricao.altura -= 40;
    desenhar_texto_limitado(superficie, transcricao, sef_sessao_ide_transcricao(estado->sessao),
                            true);
    char entrada[512];
    snprintf(entrada, sizeof(entrada), "%s> %s",
             sef_runtime_estado_codigo(sef_sessao_ide_ouvinte(estado->sessao), NULL) ==
                     SEF_CODIGO_INCOMPLETO
                 ? "......"
                 : "SEFIRAH",
             sef_sessao_ide_ouvinte(estado->sessao));
    size_t colunas_entrada = estado->ouvinte.limites.largura > 24
                                 ? (size_t)(estado->ouvinte.limites.largura - 24) / 12u
                                 : 0;
    limitar_linha(entrada, colunas_entrada);
    sef_superficie_texto(superficie, estado->ouvinte.limites.x + 12,
                         estado->ouvinte.limites.y + estado->ouvinte.limites.altura - 26, entrada,
                         2, tinta);

    sef_superficie_retangulo(superficie, 0, superficie->altura - 34, superficie->largura, 34,
                             SEF_COR(177, 196, 154));
    char estado_barra[768];
    snprintf(estado_barra, sizeof(estado_barra), "%s  |  %s  |  CTRL+S SALVAR  CTRL+O ABRIR",
             sef_sessao_ide_caminho(estado->sessao), sef_sessao_ide_estado(estado->sessao));
    size_t colunas_estado = superficie->largura > 20 ? (size_t)(superficie->largura - 20) / 12u : 0;
    limitar_linha(estado_barra, colunas_estado);
    sef_superficie_texto(superficie, 10, superficie->altura - 25, estado_barra, 2, tinta);
}

static bool tratar_evento(const SefEventoJanela *evento, void *dados) {
    EstadoIde *estado = dados;
    SefErro erro;
    sef_erro_limpar(&erro);

    switch (evento->tipo) {
    case SEF_EVENTO_TEXTO:
        if (estado->foco == FOCO_EDITOR)
            sef_sessao_ide_editor_inserir(estado->sessao, evento->texto_utf8, &erro);
        else if (estado->foco == FOCO_OUVINTE)
            sef_sessao_ide_ouvinte_inserir(estado->sessao, evento->texto_utf8, &erro);
        break;
    case SEF_EVENTO_APAGAR:
        if (estado->foco == FOCO_EDITOR)
            sef_sessao_ide_editor_apagar(estado->sessao);
        else if (estado->foco == FOCO_OUVINTE)
            sef_sessao_ide_ouvinte_apagar(estado->sessao);
        else if (estado->foco == FOCO_INSPETOR)
            sef_sessao_ide_inspetor_voltar(estado->sessao, &erro);
        break;
    case SEF_EVENTO_ENTER:
        if (estado->foco == FOCO_EDITOR)
            sef_sessao_ide_editor_nova_linha(estado->sessao, &erro);
        else if (estado->foco == FOCO_OUVINTE)
            sef_sessao_ide_ouvinte_enviar(estado->sessao, &erro);
        else if (estado->foco == FOCO_DEPURADOR) {
            if (sef_sessao_ide_inspecionar_condicao(estado->sessao, &erro))
                estado->foco = FOCO_INSPETOR;
        } else
            sef_sessao_ide_inspetor_entrar(estado->sessao, &erro);
        break;
    case SEF_EVENTO_TAB:
        alternar_foco(estado, evento->modificador_shift);
        break;
    case SEF_EVENTO_EXECUTAR:
        sef_sessao_ide_executar_editor(estado->sessao, &erro);
        break;
    case SEF_EVENTO_EXECUTAR_FORMA:
        sef_sessao_ide_executar_forma_no_cursor(estado->sessao, &erro);
        break;
    case SEF_EVENTO_EXECUTAR_ALTERACOES:
        sef_sessao_ide_executar_alteracoes(estado->sessao, &erro);
        break;
    case SEF_EVENTO_NAVEGAR_DEFINICAO:
        estado->foco = FOCO_EDITOR;
        sef_sessao_ide_navegar_definicao(
            estado->sessao,
            evento->modificador_shift ? SEF_DEFINICAO_ANTERIOR : SEF_DEFINICAO_PROXIMA, &erro);
        break;
    case SEF_EVENTO_IR_PARA_DEFINICAO:
        estado->foco = FOCO_EDITOR;
        sef_sessao_ide_ir_para_definicao(estado->sessao, &erro);
        break;
    case SEF_EVENTO_NAVEGAR_REFERENCIA:
        estado->foco = FOCO_EDITOR;
        sef_sessao_ide_navegar_referencia(
            estado->sessao,
            evento->modificador_shift ? SEF_REFERENCIA_ANTERIOR : SEF_REFERENCIA_PROXIMA, &erro);
        break;
    case SEF_EVENTO_SALVAR_IMAGEM:
        if (evento->modificador_shift) {
            estado->foco = FOCO_DEPURADOR;
            sef_sessao_ide_navegar_condicao(estado->sessao, SEF_CONDICAO_ANTERIOR, &erro);
        } else {
            sef_sessao_ide_imagem_salvar(estado->sessao, &erro);
        }
        break;
    case SEF_EVENTO_RESTAURAR_IMAGEM:
        if (evento->modificador_shift) {
            estado->foco = FOCO_DEPURADOR;
            sef_sessao_ide_navegar_condicao(estado->sessao, SEF_CONDICAO_PROXIMA, &erro);
        } else {
            sef_sessao_ide_imagem_restaurar(estado->sessao, &erro);
        }
        break;
    case SEF_EVENTO_DESFAZER:
        if (estado->foco == FOCO_EDITOR)
            sef_sessao_ide_editor_desfazer(estado->sessao, &erro);
        break;
    case SEF_EVENTO_REFAZER:
        if (estado->foco == FOCO_EDITOR)
            sef_sessao_ide_editor_refazer(estado->sessao, &erro);
        break;
    case SEF_EVENTO_SALVAR:
        sef_sessao_ide_salvar(estado->sessao, sef_sessao_ide_caminho(estado->sessao), &erro);
        break;
    case SEF_EVENTO_ABRIR:
        sef_sessao_ide_abrir(estado->sessao, sef_sessao_ide_caminho(estado->sessao), &erro);
        break;
    case SEF_EVENTO_CURSOR_ESQUERDA:
        if (estado->foco == FOCO_EDITOR)
            sef_sessao_ide_editor_mover_cursor(estado->sessao, SEF_CURSOR_ESQUERDA);
        else if (estado->foco == FOCO_INSPETOR)
            sef_sessao_ide_inspetor_mover(estado->sessao, SEF_INSPETOR_ANTERIOR, &erro);
        break;
    case SEF_EVENTO_CURSOR_DIREITA:
        if (estado->foco == FOCO_EDITOR)
            sef_sessao_ide_editor_mover_cursor(estado->sessao, SEF_CURSOR_DIREITA);
        else if (estado->foco == FOCO_INSPETOR)
            sef_sessao_ide_inspetor_mover(estado->sessao, SEF_INSPETOR_PROXIMO, &erro);
        break;
    case SEF_EVENTO_CURSOR_CIMA:
        if (estado->foco == FOCO_EDITOR)
            sef_sessao_ide_editor_mover_cursor(estado->sessao, SEF_CURSOR_CIMA);
        else if (estado->foco == FOCO_OUVINTE)
            sef_sessao_ide_ouvinte_mover_historico(estado->sessao, SEF_HISTORICO_ANTERIOR, &erro);
        else if (estado->foco == FOCO_DEPURADOR)
            sef_sessao_ide_navegar_condicao(estado->sessao, SEF_CONDICAO_ANTERIOR, &erro);
        else
            sef_sessao_ide_inspetor_mover_componente(estado->sessao,
                                                     SEF_COMPONENTE_INSPETOR_ANTERIOR, &erro);
        break;
    case SEF_EVENTO_CURSOR_BAIXO:
        if (estado->foco == FOCO_EDITOR)
            sef_sessao_ide_editor_mover_cursor(estado->sessao, SEF_CURSOR_BAIXO);
        else if (estado->foco == FOCO_OUVINTE)
            sef_sessao_ide_ouvinte_mover_historico(estado->sessao, SEF_HISTORICO_PROXIMO, &erro);
        else if (estado->foco == FOCO_DEPURADOR)
            sef_sessao_ide_navegar_condicao(estado->sessao, SEF_CONDICAO_PROXIMA, &erro);
        else
            sef_sessao_ide_inspetor_mover_componente(estado->sessao,
                                                     SEF_COMPONENTE_INSPETOR_PROXIMO, &erro);
        break;
    case SEF_EVENTO_CURSOR_INICIO:
        if (estado->foco == FOCO_EDITOR)
            sef_sessao_ide_editor_mover_cursor(estado->sessao, SEF_CURSOR_INICIO_LINHA);
        break;
    case SEF_EVENTO_CURSOR_FIM:
        if (estado->foco == FOCO_EDITOR)
            sef_sessao_ide_editor_mover_cursor(estado->sessao, SEF_CURSOR_FIM_LINHA);
        break;
    case SEF_EVENTO_PONTEIRO_PRESSIONAR:
        if (ponto_dentro(estado->editor.limites, evento->x, evento->y))
            estado->foco = FOCO_EDITOR;
        else if (ponto_dentro(estado->inspetor.limites, evento->x, evento->y)) {
            if (estado->foco == FOCO_INSPETOR)
                sef_sessao_ide_inspetor_mover_componente(estado->sessao,
                                                         SEF_COMPONENTE_INSPETOR_PROXIMO, &erro);
            estado->foco = FOCO_INSPETOR;
        } else if (ponto_dentro(estado->navegador.limites, evento->x, evento->y)) {
            estado->foco = FOCO_EDITOR;
            sef_sessao_ide_navegar_definicao(estado->sessao, SEF_DEFINICAO_PROXIMA, &erro);
        } else if (ponto_dentro(estado->depurador.limites, evento->x, evento->y)) {
            if (estado->foco == FOCO_DEPURADOR)
                sef_sessao_ide_navegar_condicao(estado->sessao, SEF_CONDICAO_PROXIMA, &erro);
            estado->foco = FOCO_DEPURADOR;
        } else if (ponto_dentro(estado->ouvinte.limites, evento->x, evento->y))
            estado->foco = FOCO_OUVINTE;
        break;
    default:
        break;
    }
    return true;
}

int sef_ide_executar(const char *caminho_inicial) {
    SefErro erro;
    EstadoIde estado = {0};
    estado.sessao = sef_sessao_ide_criar(&erro);
    if (estado.sessao == NULL || !iniciar_componentes(&estado)) {
        fprintf(stderr, "IDE nao iniciou: %s\n", erro.mensagem);
        sef_sessao_ide_destruir(estado.sessao);
        return 1;
    }
    if (caminho_inicial != NULL && !sef_sessao_ide_abrir(estado.sessao, caminho_inicial, &erro))
        fprintf(stderr, "IDE nao abriu '%s': %s\n", caminho_inicial, erro.mensagem);

    char mensagem[512] = {0};
    SefConfigJanela configuracao = {"Sefirah Lisp — ambiente vivo", 1120, 760};
    int resultado = sef_janela_executar(&configuracao, desenhar_ide, tratar_evento, &estado,
                                        mensagem, (int)sizeof(mensagem));
    if (resultado != 0 && mensagem[0] != '\0')
        fprintf(stderr, "IDE: %s\n", mensagem);
    sef_componente_liberar(&estado.raiz);
    sef_sessao_ide_destruir(estado.sessao);
    return resultado;
}
