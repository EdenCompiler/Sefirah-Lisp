#include "ide/ide.h"

#include "sefirah/gui.h"
#include "sefirah/janela.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum FocoIde {
    FOCO_EXPLORADOR,
    FOCO_EDITOR,
    FOCO_INSPETOR,
    FOCO_DEPURADOR,
    FOCO_OUVINTE
} FocoIde;

typedef enum AcaoBotaoIde {
    ACAO_BOTAO_EXECUTAR,
    ACAO_BOTAO_EXECUTAR_FORMA,
    ACAO_BOTAO_EXECUTAR_ALTERACOES,
    ACAO_BOTAO_SALVAR,
    ACAO_BOTAO_SNAPSHOT,
    ACAO_BOTAO_RESTAURAR
} AcaoBotaoIde;

typedef struct BotaoIde {
    const char *rotulo;
    AcaoBotaoIde acao;
    SefRetangulo limites;
} BotaoIde;

typedef struct EstadoIde {
    SefSessaoIde *sessao;
    FocoIde foco;
    SefTemaGui tema;
    SefComponente raiz;
    SefComponente area_principal;
    SefComponente explorador;
    SefComponente editor;
    SefComponente ferramentas;
    SefComponente inspetor;
    SefComponente navegador;
    SefComponente depurador;
    SefComponente ouvinte;
    BotaoIde botoes[6];
} EstadoIde;

static bool iniciar_componentes(EstadoIde *estado) {
    estado->tema = (SefTemaGui){SEF_COR(207, 198, 164),
                                SEF_COR(244, 238, 211),
                                SEF_COR(203, 211, 177),
                                SEF_COR(43, 54, 45),
                                SEF_COR(101, 112, 86),
                                SEF_COR(181, 112, 52),
                                6,
                                4};
    sef_componente_iniciar(&estado->raiz, SEF_COMPONENTE_PAINEL, NULL);
    sef_componente_iniciar(&estado->area_principal, SEF_COMPONENTE_PAINEL, NULL);
    sef_componente_iniciar(&estado->explorador, SEF_COMPONENTE_PAINEL, "EXPLORER");
    sef_componente_iniciar(&estado->editor, SEF_COMPONENTE_PAINEL, "EDITOR");
    sef_componente_iniciar(&estado->ferramentas, SEF_COMPONENTE_PAINEL, NULL);
    sef_componente_iniciar(&estado->inspetor, SEF_COMPONENTE_PAINEL, "INSPECTOR");
    sef_componente_iniciar(&estado->navegador, SEF_COMPONENTE_PAINEL, "BROWSER");
    sef_componente_iniciar(&estado->depurador, SEF_COMPONENTE_PAINEL, "DEBUGGER");
    sef_componente_iniciar(&estado->ouvinte, SEF_COMPONENTE_PAINEL, "REPL");
    estado->botoes[0] = (BotaoIde){"RUN  F5", ACAO_BOTAO_EXECUTAR, {0}};
    estado->botoes[1] = (BotaoIde){"FORM  F6", ACAO_BOTAO_EXECUTAR_FORMA, {0}};
    estado->botoes[2] = (BotaoIde){"CHANGES  F7", ACAO_BOTAO_EXECUTAR_ALTERACOES, {0}};
    estado->botoes[3] = (BotaoIde){"SAVE", ACAO_BOTAO_SALVAR, {0}};
    estado->botoes[4] = (BotaoIde){"SNAPSHOT", ACAO_BOTAO_SNAPSHOT, {0}};
    estado->botoes[5] = (BotaoIde){"RESTORE", ACAO_BOTAO_RESTAURAR, {0}};
    estado->raiz.espacamento = 8;
    estado->area_principal.espacamento = 8;
    estado->area_principal.direcao = SEF_LAYOUT_LINHA;
    estado->ferramentas.espacamento = 8;
    estado->area_principal.peso = 2;
    estado->explorador.peso = 2;
    estado->editor.peso = 5;
    estado->ferramentas.peso = 3;
    estado->inspetor.peso = 1;
    estado->navegador.peso = 1;
    estado->depurador.peso = 1;
    estado->ouvinte.peso = 1;
    return sef_componente_adicionar(&estado->raiz, &estado->area_principal) &&
           sef_componente_adicionar(&estado->raiz, &estado->ouvinte) &&
           sef_componente_adicionar(&estado->area_principal, &estado->explorador) &&
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
        estado->foco = estado->foco == FOCO_EXPLORADOR ? FOCO_OUVINTE : (FocoIde)(estado->foco - 1);
    else
        estado->foco = estado->foco == FOCO_OUVINTE ? FOCO_EXPLORADOR : (FocoIde)(estado->foco + 1);
}

static void mover_cursor_editor(EstadoIde *estado, const SefEventoJanela *evento,
                                SefMovimentoCursorIde movimento) {
    if (evento->modificador_shift)
        sef_sessao_ide_editor_mover_cursor_selecionando(estado->sessao, movimento);
    else
        sef_sessao_ide_editor_mover_cursor(estado->sessao, movimento);
}

static void desenhar_painel(SefSuperficie *superficie, SefRetangulo limites, const char *titulo,
                            bool ativo) {
    const SefCor tinta = ativo ? SEF_COR(244, 238, 211) : SEF_COR(43, 54, 45);
    SefCor barra = ativo ? SEF_COR(67, 88, 70) : SEF_COR(177, 196, 154);
    sef_superficie_retangulo(superficie, limites.x, limites.y, limites.largura, limites.altura,
                             SEF_COR(244, 238, 211));
    sef_superficie_contorno(superficie, limites.x, limites.y, limites.largura, limites.altura,
                            ativo ? 2 : 1, ativo ? SEF_COR(181, 112, 52) : SEF_COR(101, 112, 86));
    sef_superficie_retangulo(superficie, limites.x + 2, limites.y + 2, limites.largura - 4, 28,
                             barra);
    sef_superficie_texto(superficie, limites.x + 9, limites.y + 8, titulo, 2, tinta);
}

static void desenhar_barra_comandos(SefSuperficie *superficie, EstadoIde *estado) {
    sef_superficie_retangulo(superficie, 0, 30, superficie->largura, 36, SEF_COR(218, 211, 182));
    sef_superficie_retangulo(superficie, 0, 65, superficie->largura, 1, SEF_COR(101, 112, 86));
    int x = 10;
    for (size_t i = 0; i < sizeof(estado->botoes) / sizeof(estado->botoes[0]); i++) {
        BotaoIde *botao = &estado->botoes[i];
        int largura = (int)strlen(botao->rotulo) * 6 + 20;
        botao->limites = (SefRetangulo){x, 36, largura, 24};
        sef_superficie_retangulo(superficie, x, 36, largura, 24, SEF_COR(244, 238, 211));
        sef_superficie_contorno(superficie, x, 36, largura, 24, 1, SEF_COR(101, 112, 86));
        sef_superficie_retangulo(superficie, x + 1, 58, largura - 2, 1, SEF_COR(181, 112, 52));
        sef_superficie_texto(superficie, x + 10, 44, botao->rotulo, 1, SEF_COR(43, 54, 45));
        x += largura + 8;
    }
}

static void acionar_botao(EstadoIde *estado, AcaoBotaoIde acao, SefErro *erro) {
    switch (acao) {
    case ACAO_BOTAO_EXECUTAR:
        estado->foco = FOCO_EDITOR;
        sef_sessao_ide_executar_editor(estado->sessao, erro);
        break;
    case ACAO_BOTAO_EXECUTAR_FORMA:
        estado->foco = FOCO_EDITOR;
        sef_sessao_ide_executar_forma_no_cursor(estado->sessao, erro);
        break;
    case ACAO_BOTAO_EXECUTAR_ALTERACOES:
        estado->foco = FOCO_EDITOR;
        sef_sessao_ide_executar_alteracoes(estado->sessao, erro);
        break;
    case ACAO_BOTAO_SALVAR:
        sef_sessao_ide_salvar(estado->sessao, sef_sessao_ide_caminho(estado->sessao), erro);
        break;
    case ACAO_BOTAO_SNAPSHOT:
        sef_sessao_ide_imagem_salvar(estado->sessao, erro);
        break;
    case ACAO_BOTAO_RESTAURAR:
        sef_sessao_ide_imagem_restaurar(estado->sessao, erro);
        break;
    }
}

static void desenhar_abas(SefSuperficie *superficie, SefRetangulo limites,
                          const SefSessaoIde *sessao) {
    const int largura_aba = 180;
    const int y = limites.y + 31;
    sef_superficie_retangulo(superficie, limites.x + 2, limites.y + 31, limites.largura - 4, 26,
                             SEF_COR(218, 211, 182));
    size_t quantidade = sef_sessao_ide_quantidade_documentos(sessao);
    size_t ativa = sef_sessao_ide_documento_ativo(sessao);
    for (size_t i = 0; i < quantidade; i++) {
        int x = limites.x + 2 + (int)i * largura_aba;
        if (x >= limites.x + limites.largura - 2)
            break;
        int largura = largura_aba;
        if (x + largura > limites.x + limites.largura - 2)
            largura = limites.x + limites.largura - 2 - x;
        bool ativa_agora = i == ativa;
        sef_superficie_retangulo(superficie, x, y, largura, 26,
                                 ativa_agora ? SEF_COR(244, 238, 211) : SEF_COR(203, 211, 177));
        sef_superficie_retangulo(superficie, x + largura - 1, y, 1, 26, SEF_COR(101, 112, 86));
        if (ativa_agora)
            sef_superficie_retangulo(superficie, x, y + 24, largura, 2, SEF_COR(181, 112, 52));

        const char *caminho = sef_sessao_ide_documento_caminho(sessao, i);
        const char *nome = caminho == NULL ? "untitled.lisp" : caminho;
        for (const char *cursor = nome; *cursor != '\0'; cursor++)
            if (*cursor == '/' || *cursor == '\\')
                nome = cursor + 1;
        char titulo[32];
        snprintf(titulo, sizeof(titulo), "%.22s%s", nome,
                 sef_sessao_ide_documento_modificado(sessao, i) ? "  *" : "");
        sef_superficie_texto(superficie, x + 10, y + 8, titulo, 1,
                             ativa_agora ? SEF_COR(43, 54, 45) : SEF_COR(75, 84, 67));
    }
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

static void desenhar_texto_limitado_escala(SefSuperficie *superficie, SefRetangulo limites,
                                           const char *texto, bool mostrar_final, int escala) {
    int largura_util = limites.largura - 24;
    int altura_util = limites.altura - 44;
    if (largura_util <= 0 || altura_util <= 0 || escala <= 0)
        return;
    size_t colunas = (size_t)(largura_util / (6 * escala));
    size_t linhas = (size_t)(altura_util / (9 * escala));
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
        sef_superficie_texto(superficie, limites.x + 12, y, linha, escala, SEF_COR(43, 54, 45));
        y += 9 * escala;
    }
}

static void desenhar_texto_limitado(SefSuperficie *superficie, SefRetangulo limites,
                                    const char *texto, bool mostrar_final) {
    desenhar_texto_limitado_escala(superficie, limites, texto, mostrar_final, 2);
}

static void desenhar_editor(SefSuperficie *superficie, SefRetangulo limites,
                            const SefSessaoIde *sessao) {
    const char *codigo = sef_sessao_ide_editor(sessao);
    size_t tamanho = strlen(codigo);
    size_t cursor = sef_sessao_ide_cursor_editor(sessao);
    if (cursor > tamanho)
        cursor = tamanho;
    size_t inicio_selecao = 0;
    size_t fim_selecao = 0;
    bool selecionado = sef_sessao_ide_selecao_editor(sessao, &inicio_selecao, &fim_selecao);
    char *com_cursor = malloc(tamanho + (selecionado ? 4u : 2u));
    if (com_cursor == NULL) {
        desenhar_texto_limitado(superficie, limites, codigo, true);
        return;
    }
    size_t escrito = 0;
    size_t cursor_visual = 0;
    for (size_t i = 0; i <= tamanho; i++) {
        if (selecionado && i == inicio_selecao)
            com_cursor[escrito++] = '[';
        if (i == cursor) {
            com_cursor[escrito++] = '|';
            cursor_visual = escrito;
        }
        if (selecionado && i == fim_selecao)
            com_cursor[escrito++] = ']';
        if (i < tamanho)
            com_cursor[escrito++] = codigo[i];
    }
    com_cursor[escrito] = '\0';
    size_t linhas = limites.altura > 44 ? (size_t)(limites.altura - 44) / 18u : 1;
    size_t linhas_anteriores = cursor == tamanho ? linhas : linhas / 2u + 1u;
    const char *inicio = inicio_antes_da_posicao(com_cursor, cursor_visual, linhas_anteriores);
    desenhar_texto_limitado(superficie, limites, inicio, false);
    free(com_cursor);
}

static void desenhar_ide(SefSuperficie *superficie, void *dados) {
    EstadoIde *estado = dados;
    const SefCor tinta = SEF_COR(43, 54, 45);
    sef_superficie_limpar(superficie, SEF_COR(207, 198, 164));
    sef_superficie_retangulo(superficie, 0, 0, superficie->largura, 30, SEF_COR(43, 54, 45));
    sef_superficie_texto(superficie, 12, 8, "SEFIRAH LISP  LIVE WORLD", 2, SEF_COR(231, 218, 168));
    desenhar_barra_comandos(superficie, estado);
    sef_componente_organizar(&estado->raiz,
                             (SefRetangulo){0, 66, superficie->largura, superficie->altura - 102},
                             &estado->tema);

    desenhar_painel(superficie, estado->explorador.limites, "EXPLORER",
                    estado->foco == FOCO_EXPLORADOR);
    desenhar_painel(superficie, estado->editor.limites, "EDITOR  [F5 ALL] [F6 FORM] [SHIFT+F6]",
                    estado->foco == FOCO_EDITOR);
    desenhar_painel(superficie, estado->inspetor.limites, "INSPECTOR [ENTER] [BACK]",
                    estado->foco == FOCO_INSPETOR);
    desenhar_painel(superficie, estado->navegador.limites, "BROWSER [F11] [F12]", false);
    desenhar_painel(superficie, estado->depurador.limites, "DEBUGGER [ENTER]",
                    estado->foco == FOCO_DEPURADOR);
    desenhar_painel(superficie, estado->ouvinte.limites, "LISTENER / REPL  [ENTER] [UP HISTORY]",
                    estado->foco == FOCO_OUVINTE);
    desenhar_abas(superficie, estado->editor.limites, estado->sessao);
    SefRetangulo area_codigo = estado->editor.limites;
    area_codigo.y += 24;
    area_codigo.altura -= 24;
    desenhar_editor(superficie, area_codigo, estado->sessao);
    desenhar_texto_limitado_escala(superficie, estado->explorador.limites,
                                   sef_sessao_ide_explorador(estado->sessao), false, 1);
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
    snprintf(estado_barra, sizeof(estado_barra), "%s  |  %s  |  CTRL+S SAVE  CTRL+O OPEN",
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
        if (estado->foco == FOCO_EXPLORADOR) {
            if (sef_sessao_ide_espaco_trabalho_abrir_selecionado(estado->sessao, &erro))
                estado->foco = FOCO_EDITOR;
        } else if (estado->foco == FOCO_EDITOR)
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
        if (evento->modificador_shift)
            sef_sessao_ide_editor_selecionar_forma(estado->sessao, &erro);
        else
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
            mover_cursor_editor(estado, evento, SEF_CURSOR_ESQUERDA);
        else if (estado->foco == FOCO_INSPETOR)
            sef_sessao_ide_inspetor_mover(estado->sessao, SEF_INSPETOR_ANTERIOR, &erro);
        break;
    case SEF_EVENTO_CURSOR_DIREITA:
        if (estado->foco == FOCO_EDITOR)
            mover_cursor_editor(estado, evento, SEF_CURSOR_DIREITA);
        else if (estado->foco == FOCO_INSPETOR)
            sef_sessao_ide_inspetor_mover(estado->sessao, SEF_INSPETOR_PROXIMO, &erro);
        break;
    case SEF_EVENTO_CURSOR_CIMA:
        if (estado->foco == FOCO_EXPLORADOR)
            sef_sessao_ide_espaco_trabalho_mover(estado->sessao, SEF_ARQUIVO_ANTERIOR, &erro);
        else if (estado->foco == FOCO_EDITOR)
            mover_cursor_editor(estado, evento, SEF_CURSOR_CIMA);
        else if (estado->foco == FOCO_OUVINTE)
            sef_sessao_ide_ouvinte_mover_historico(estado->sessao, SEF_HISTORICO_ANTERIOR, &erro);
        else if (estado->foco == FOCO_DEPURADOR)
            sef_sessao_ide_navegar_condicao(estado->sessao, SEF_CONDICAO_ANTERIOR, &erro);
        else
            sef_sessao_ide_inspetor_mover_componente(estado->sessao,
                                                     SEF_COMPONENTE_INSPETOR_ANTERIOR, &erro);
        break;
    case SEF_EVENTO_CURSOR_BAIXO:
        if (estado->foco == FOCO_EXPLORADOR)
            sef_sessao_ide_espaco_trabalho_mover(estado->sessao, SEF_ARQUIVO_PROXIMO, &erro);
        else if (estado->foco == FOCO_EDITOR)
            mover_cursor_editor(estado, evento, SEF_CURSOR_BAIXO);
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
            mover_cursor_editor(estado, evento, SEF_CURSOR_INICIO_LINHA);
        break;
    case SEF_EVENTO_CURSOR_FIM:
        if (estado->foco == FOCO_EDITOR)
            mover_cursor_editor(estado, evento, SEF_CURSOR_FIM_LINHA);
        break;
    case SEF_EVENTO_PONTEIRO_PRESSIONAR:
        for (size_t i = 0; i < sizeof(estado->botoes) / sizeof(estado->botoes[0]); i++) {
            if (ponto_dentro(estado->botoes[i].limites, evento->x, evento->y)) {
                acionar_botao(estado, estado->botoes[i].acao, &erro);
                return true;
            }
        }
        if (ponto_dentro(estado->explorador.limites, evento->x, evento->y)) {
            estado->foco = FOCO_EXPLORADOR;
            int inicio_arquivos = estado->explorador.limites.y + 60;
            if (evento->y >= inicio_arquivos) {
                size_t indice = (size_t)(evento->y - inicio_arquivos) / 9u;
                if (indice < sef_sessao_ide_espaco_trabalho_quantidade(estado->sessao) &&
                    sef_sessao_ide_espaco_trabalho_selecionar(estado->sessao, indice, &erro))
                    sef_sessao_ide_espaco_trabalho_abrir_selecionado(estado->sessao, &erro);
            }
        } else if (ponto_dentro(estado->editor.limites, evento->x, evento->y)) {
            estado->foco = FOCO_EDITOR;
            if (evento->y >= estado->editor.limites.y + 30 &&
                evento->y < estado->editor.limites.y + 60) {
                size_t indice = (size_t)(evento->x - estado->editor.limites.x - 2) / 180u;
                if (indice < sef_sessao_ide_quantidade_documentos(estado->sessao))
                    sef_sessao_ide_documento_ativar(estado->sessao, indice, &erro);
            }
        } else if (ponto_dentro(estado->inspetor.limites, evento->x, evento->y)) {
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
        fprintf(stderr, "IDE failed to start: %s\n", erro.mensagem);
        sef_sessao_ide_destruir(estado.sessao);
        return 1;
    }
    if (caminho_inicial != NULL) {
        if (sef_sessao_ide_espaco_trabalho_abrir(estado.sessao, caminho_inicial, &erro))
            estado.foco = FOCO_EXPLORADOR;
        else if (sef_sessao_ide_abrir(estado.sessao, caminho_inicial, &erro))
            estado.foco = FOCO_EDITOR;
        else
            fprintf(stderr, "IDE could not open '%s': %s\n", caminho_inicial, erro.mensagem);
    }

    char mensagem[512] = {0};
    SefConfigJanela configuracao = {"Sefirah Lisp — live environment", 1120, 760};
    int resultado = sef_janela_executar(&configuracao, desenhar_ide, tratar_evento, &estado,
                                        mensagem, (int)sizeof(mensagem));
    if (resultado != 0 && mensagem[0] != '\0')
        fprintf(stderr, "IDE: %s\n", mensagem);
    sef_componente_liberar(&estado.raiz);
    sef_sessao_ide_destruir(estado.sessao);
    return resultado;
}
