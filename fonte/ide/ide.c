#include "ide/ide.h"

#include "sefirah/gui.h"
#include "sefirah/janela.h"

#include <ctype.h>
#include <errno.h>
#include <stdint.h>
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

typedef enum FerramentaIde {
    FERRAMENTA_INSPETOR,
    FERRAMENTA_NAVEGADOR,
    FERRAMENTA_DEPURADOR,
    FERRAMENTA_CONTROLE_VERSAO,
    FERRAMENTA_PERFIL,
    FERRAMENTA_QUANTIDADE
} FerramentaIde;

typedef enum AcaoBotaoIde {
    ACAO_BOTAO_EXECUTAR,
    ACAO_BOTAO_EXECUTAR_FORMA,
    ACAO_BOTAO_EXECUTAR_ALTERACOES,
    ACAO_BOTAO_SALVAR,
    ACAO_BOTAO_SALVAMENTO_AUTOMATICO,
    ACAO_BOTAO_SNAPSHOT,
    ACAO_BOTAO_RESTAURAR,
    ACAO_BOTAO_COMANDOS,
    ACAO_BOTAO_SIMBOLOS,
    ACAO_BOTAO_REFERENCIAS,
    ACAO_BOTAO_BUSCAR_EDITOR,
    ACAO_BOTAO_IR_PARA_LINHA,
    ACAO_BOTAO_NOVA_ABA,
    ACAO_BOTAO_FECHAR_ABA,
    ACAO_BOTAO_CONTROLE_VERSAO,
    ACAO_BOTAO_PERFIL,
    ACAO_BOTAO_ABRIR_ARQUIVO,
    ACAO_BOTAO_ABRIR_PASTA,
    ACAO_BOTAO_CRIAR_ARQUIVO,
    ACAO_BOTAO_CRIAR_PASTA,
    ACAO_BOTAO_ATUALIZAR_EXPLORADOR
} AcaoBotaoIde;

typedef enum ModoSobreposicaoIde {
    SOBREPOSICAO_NENHUMA,
    SOBREPOSICAO_ABRIR_RAPIDO,
    SOBREPOSICAO_SIMBOLOS,
    SOBREPOSICAO_BUSCAR_EDITOR,
    SOBREPOSICAO_IR_PARA_LINHA,
    SOBREPOSICAO_CONFIRMAR_FECHAMENTO,
    SOBREPOSICAO_COMANDOS,
    SOBREPOSICAO_ABRIR_ARQUIVO,
    SOBREPOSICAO_ABRIR_PASTA,
    SOBREPOSICAO_CRIAR_ARQUIVO,
    SOBREPOSICAO_CRIAR_PASTA
} ModoSobreposicaoIde;

typedef enum AcaoComandoIde {
    COMANDO_ABRIR_RAPIDO,
    COMANDO_BUSCAR_SIMBOLOS,
    COMANDO_BUSCAR_EDITOR,
    COMANDO_IR_PARA_LINHA,
    COMANDO_NOVA_ABA,
    COMANDO_FECHAR_ABA,
    COMANDO_ABRIR_ARQUIVO,
    COMANDO_ABRIR_PASTA,
    COMANDO_CRIAR_ARQUIVO,
    COMANDO_CRIAR_PASTA,
    COMANDO_ATUALIZAR_EXPLORADOR,
    COMANDO_EXECUTAR,
    COMANDO_EXECUTAR_FORMA,
    COMANDO_EXECUTAR_ALTERACOES,
    COMANDO_SALVAR,
    COMANDO_ALTERNAR_SALVAMENTO_AUTOMATICO,
    COMANDO_SNAPSHOT,
    COMANDO_RESTAURAR,
    COMANDO_IR_DEFINICAO,
    COMANDO_NAVEGAR_REFERENCIA,
    COMANDO_ATUALIZAR_CONTROLE_VERSAO,
    COMANDO_LIMPAR_PERFIL,
    COMANDO_FOCAR_EXPLORADOR,
    COMANDO_FOCAR_OUVINTE,
    COMANDO_SELECIONAR_TUDO,
    COMANDO_DESFAZER,
    COMANDO_REFAZER
} AcaoComandoIde;

typedef struct ComandoIde {
    const char *rotulo;
    AcaoComandoIde acao;
} ComandoIde;

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
    SefComponente ouvinte;
    FerramentaIde ferramenta;
    SefRetangulo abas_ferramentas[FERRAMENTA_QUANTIDADE];
    BotaoIde botoes[21];
    ModoSobreposicaoIde sobreposicao;
    char consulta[1024];
    char ultima_busca[1024];
    char mensagem_sobreposicao[256];
    size_t tamanho_consulta;
    size_t item_sobreposicao;
    SefRetangulo limites_sobreposicao;
    size_t primeiro_item_sobreposicao;
    size_t quantidade_itens_visiveis;
    bool selecionando_com_ponteiro;
    bool selecao_ponteiro_moveu;
    int selecao_ponteiro_x;
    int selecao_ponteiro_y;
} EstadoIde;

static const ComandoIde comandos[] = {
    {"Quick Open File", COMANDO_ABRIR_RAPIDO},
    {"Go to Symbol in Workspace", COMANDO_BUSCAR_SIMBOLOS},
    {"Find in Active Editor", COMANDO_BUSCAR_EDITOR},
    {"Go to Line", COMANDO_IR_PARA_LINHA},
    {"New Untitled Tab", COMANDO_NOVA_ABA},
    {"Close Active Tab", COMANDO_FECHAR_ABA},
    {"Open File by Path", COMANDO_ABRIR_ARQUIVO},
    {"Open Folder", COMANDO_ABRIR_PASTA},
    {"Create New File", COMANDO_CRIAR_ARQUIVO},
    {"Create New Folder", COMANDO_CRIAR_PASTA},
    {"Refresh Workspace Explorer", COMANDO_ATUALIZAR_EXPLORADOR},
    {"Run Buffer in Live World", COMANDO_EXECUTAR},
    {"Run Form at Cursor", COMANDO_EXECUTAR_FORMA},
    {"Run Changed Top-Level Forms", COMANDO_EXECUTAR_ALTERACOES},
    {"Save Current File", COMANDO_SALVAR},
    {"Toggle Auto Save", COMANDO_ALTERNAR_SALVAMENTO_AUTOMATICO},
    {"Save Live World Snapshot", COMANDO_SNAPSHOT},
    {"Restore Live World Snapshot", COMANDO_RESTAURAR},
    {"Go to Definition at Cursor", COMANDO_IR_DEFINICAO},
    {"Find Next Reference at Cursor", COMANDO_NAVEGAR_REFERENCIA},
    {"Refresh Source Control", COMANDO_ATUALIZAR_CONTROLE_VERSAO},
    {"Clear Evaluation Profile", COMANDO_LIMPAR_PERFIL},
    {"Focus Workspace Explorer", COMANDO_FOCAR_EXPLORADOR},
    {"Focus Listener / REPL", COMANDO_FOCAR_OUVINTE},
    {"Select All Editor Text", COMANDO_SELECIONAR_TUDO},
    {"Undo Editor Change", COMANDO_DESFAZER},
    {"Redo Editor Change", COMANDO_REFAZER},
};

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
    sef_componente_iniciar(&estado->ferramentas, SEF_COMPONENTE_PAINEL, "TOOLS");
    sef_componente_iniciar(&estado->ouvinte, SEF_COMPONENTE_PAINEL, "REPL");
    estado->botoes[0] = (BotaoIde){"RUN", ACAO_BOTAO_EXECUTAR, {0}};
    estado->botoes[1] = (BotaoIde){"FORM  F6", ACAO_BOTAO_EXECUTAR_FORMA, {0}};
    estado->botoes[2] = (BotaoIde){"CHANGES  F7", ACAO_BOTAO_EXECUTAR_ALTERACOES, {0}};
    estado->botoes[3] = (BotaoIde){"SAVE", ACAO_BOTAO_SALVAR, {0}};
    estado->botoes[4] = (BotaoIde){"AUTO OFF", ACAO_BOTAO_SALVAMENTO_AUTOMATICO, {0}};
    estado->botoes[5] = (BotaoIde){"SNAPSHOT", ACAO_BOTAO_SNAPSHOT, {0}};
    estado->botoes[6] = (BotaoIde){"RESTORE", ACAO_BOTAO_RESTAURAR, {0}};
    estado->botoes[7] = (BotaoIde){"COMMANDS", ACAO_BOTAO_COMANDOS, {0}};
    estado->botoes[8] = (BotaoIde){"SYMBOLS", ACAO_BOTAO_SIMBOLOS, {0}};
    estado->botoes[9] = (BotaoIde){"REFERENCES", ACAO_BOTAO_REFERENCIAS, {0}};
    estado->botoes[10] = (BotaoIde){"FIND", ACAO_BOTAO_BUSCAR_EDITOR, {0}};
    estado->botoes[11] = (BotaoIde){"GO LINE", ACAO_BOTAO_IR_PARA_LINHA, {0}};
    estado->botoes[12] = (BotaoIde){"NEW TAB", ACAO_BOTAO_NOVA_ABA, {0}};
    estado->botoes[13] = (BotaoIde){"CLOSE TAB", ACAO_BOTAO_FECHAR_ABA, {0}};
    estado->botoes[14] = (BotaoIde){"SOURCE", ACAO_BOTAO_CONTROLE_VERSAO, {0}};
    estado->botoes[15] = (BotaoIde){"PROFILE", ACAO_BOTAO_PERFIL, {0}};
    estado->botoes[16] = (BotaoIde){"OPEN FILE", ACAO_BOTAO_ABRIR_ARQUIVO, {0}};
    estado->botoes[17] = (BotaoIde){"OPEN FOLDER", ACAO_BOTAO_ABRIR_PASTA, {0}};
    estado->botoes[18] = (BotaoIde){"NEW FILE", ACAO_BOTAO_CRIAR_ARQUIVO, {0}};
    estado->botoes[19] = (BotaoIde){"NEW FOLDER", ACAO_BOTAO_CRIAR_PASTA, {0}};
    estado->botoes[20] = (BotaoIde){"REFRESH", ACAO_BOTAO_ATUALIZAR_EXPLORADOR, {0}};
    estado->raiz.espacamento = 8;
    estado->area_principal.espacamento = 8;
    estado->area_principal.direcao = SEF_LAYOUT_LINHA;
    estado->ferramenta = FERRAMENTA_INSPETOR;
    estado->area_principal.peso = 2;
    estado->explorador.peso = 2;
    estado->editor.peso = 5;
    estado->ferramentas.peso = 3;
    estado->ouvinte.peso = 1;
    return sef_componente_adicionar(&estado->raiz, &estado->area_principal) &&
           sef_componente_adicionar(&estado->raiz, &estado->ouvinte) &&
           sef_componente_adicionar(&estado->area_principal, &estado->explorador) &&
           sef_componente_adicionar(&estado->area_principal, &estado->editor) &&
           sef_componente_adicionar(&estado->area_principal, &estado->ferramentas);
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
    if (estado->foco == FOCO_INSPETOR)
        estado->ferramenta = FERRAMENTA_INSPETOR;
    else if (estado->foco == FOCO_DEPURADOR)
        estado->ferramenta = FERRAMENTA_DEPURADOR;
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

static const char *nome_ferramenta(FerramentaIde ferramenta) {
    static const char *nomes[] = {"INSPECTOR", "BROWSER", "DEBUGGER", "SOURCE", "PROFILE"};
    return ferramenta < FERRAMENTA_QUANTIDADE ? nomes[ferramenta] : "TOOLS";
}

static bool ferramenta_focada(const EstadoIde *estado) {
    return (estado->ferramenta == FERRAMENTA_INSPETOR && estado->foco == FOCO_INSPETOR) ||
           (estado->ferramenta == FERRAMENTA_DEPURADOR && estado->foco == FOCO_DEPURADOR);
}

static void desenhar_abas_ferramentas(SefSuperficie *superficie, EstadoIde *estado) {
    SefRetangulo limites = estado->ferramentas.limites;
    int largura = limites.largura / (int)FERRAMENTA_QUANTIDADE;
    for (int i = 0; i < (int)FERRAMENTA_QUANTIDADE; i++) {
        int x = limites.x + i * largura;
        int largura_atual = i + 1 == (int)FERRAMENTA_QUANTIDADE
                                ? limites.x + limites.largura - x
                                : largura;
        estado->abas_ferramentas[i] = (SefRetangulo){x, limites.y + 31, largura_atual, 24};
        bool ativa = estado->ferramenta == (FerramentaIde)i;
        sef_superficie_retangulo(superficie, x, limites.y + 31, largura_atual, 24,
                                 ativa ? SEF_COR(244, 238, 211) : SEF_COR(203, 211, 177));
        sef_superficie_contorno(superficie, x, limites.y + 31, largura_atual, 24, 1,
                                SEF_COR(101, 112, 86));
        if (ativa)
            sef_superficie_retangulo(superficie, x + 1, limites.y + 53, largura_atual - 2, 2,
                                     SEF_COR(181, 112, 52));
        sef_superficie_texto(superficie, x + 5, limites.y + 39, nome_ferramenta((FerramentaIde)i),
                             1, SEF_COR(43, 54, 45));
    }
}

static bool selecionar_aba_ferramenta(EstadoIde *estado, int x, int y) {
    for (int i = 0; i < (int)FERRAMENTA_QUANTIDADE; i++) {
        if (!ponto_dentro(estado->abas_ferramentas[i], x, y))
            continue;
        estado->ferramenta = (FerramentaIde)i;
        if (estado->ferramenta == FERRAMENTA_INSPETOR)
            estado->foco = FOCO_INSPETOR;
        else if (estado->ferramenta == FERRAMENTA_DEPURADOR)
            estado->foco = FOCO_DEPURADOR;
        else
            estado->foco = FOCO_EDITOR;
        return true;
    }
    return false;
}

static void desenhar_barra_comandos(SefSuperficie *superficie, EstadoIde *estado) {
    sef_superficie_retangulo(superficie, 0, 30, superficie->largura, 68, SEF_COR(218, 211, 182));
    sef_superficie_retangulo(superficie, 0, 97, superficie->largura, 1, SEF_COR(101, 112, 86));
    int x = 10;
    int y = 36;
    for (size_t i = 0; i < sizeof(estado->botoes) / sizeof(estado->botoes[0]); i++)
        estado->botoes[i].limites = (SefRetangulo){0};
    for (size_t i = 0; i < sizeof(estado->botoes) / sizeof(estado->botoes[0]); i++) {
        BotaoIde *botao = &estado->botoes[i];
        if (i == 8) {
            x = 10;
            y = 66;
        }
        if (botao->acao == ACAO_BOTAO_SALVAMENTO_AUTOMATICO)
            botao->rotulo = sef_sessao_ide_salvamento_automatico(estado->sessao) ? "AUTO ON"
                                                                               : "AUTO OFF";
        int largura = (int)strlen(botao->rotulo) * 6 + 20;
        if (x + largura > superficie->largura - 10)
            continue;
        botao->limites = (SefRetangulo){x, y, largura, 24};
        sef_superficie_retangulo(superficie, x, y, largura, 24, SEF_COR(244, 238, 211));
        sef_superficie_contorno(superficie, x, y, largura, 24, 1, SEF_COR(101, 112, 86));
        sef_superficie_retangulo(superficie, x + 1, y + 22, largura - 2, 1,
                                 SEF_COR(181, 112, 52));
        sef_superficie_texto(superficie, x + 10, y + 8, botao->rotulo, 1, SEF_COR(43, 54, 45));
        x += largura + 8;
    }
}

static void abrir_sobreposicao(EstadoIde *estado, ModoSobreposicaoIde modo) {
    estado->sobreposicao = modo;
    estado->consulta[0] = '\0';
    estado->mensagem_sobreposicao[0] = '\0';
    estado->tamanho_consulta = 0;
    estado->item_sobreposicao = 0;
}

static bool sobreposicao_e_caminho(ModoSobreposicaoIde modo) {
    return modo == SOBREPOSICAO_ABRIR_ARQUIVO || modo == SOBREPOSICAO_ABRIR_PASTA ||
           modo == SOBREPOSICAO_CRIAR_ARQUIVO || modo == SOBREPOSICAO_CRIAR_PASTA;
}

static void abrir_sobreposicao_caminho(EstadoIde *estado, ModoSobreposicaoIde modo) {
    abrir_sobreposicao(estado, modo);
    const char *raiz = sef_sessao_ide_espaco_trabalho_raiz(estado->sessao);
    if (raiz[0] == '\0')
        return;
    snprintf(estado->consulta, sizeof(estado->consulta), "%s", raiz);
    estado->tamanho_consulta = strlen(estado->consulta);
    if (modo != SOBREPOSICAO_ABRIR_PASTA &&
        estado->tamanho_consulta + 1 < sizeof(estado->consulta) &&
        estado->consulta[estado->tamanho_consulta - 1] != '/' &&
        estado->consulta[estado->tamanho_consulta - 1] != '\\') {
        estado->consulta[estado->tamanho_consulta++] = '/';
        estado->consulta[estado->tamanho_consulta] = '\0';
    }
}

static void carregar_simbolos_espaco_trabalho(EstadoIde *estado) {
    SefErro erro;
    if (sef_sessao_ide_simbolos_espaco_trabalho_buscar(estado->sessao, "", &erro))
        estado->mensagem_sobreposicao[0] = '\0';
    else
        snprintf(estado->mensagem_sobreposicao, sizeof(estado->mensagem_sobreposicao), "%.255s",
                 erro.mensagem);
    size_t quantidade = sef_sessao_ide_simbolos_espaco_trabalho_quantidade(estado->sessao);
    if (quantidade == 0)
        estado->item_sobreposicao = 0;
    else if (estado->item_sobreposicao >= quantidade)
        estado->item_sobreposicao = quantidade - 1;
}

static void abrir_sobreposicao_simbolos(EstadoIde *estado) {
    abrir_sobreposicao(estado, SOBREPOSICAO_SIMBOLOS);
    carregar_simbolos_espaco_trabalho(estado);
}

static void abrir_sobreposicao_busca_editor(EstadoIde *estado) {
    abrir_sobreposicao(estado, SOBREPOSICAO_BUSCAR_EDITOR);
    size_t inicio = 0;
    size_t fim = 0;
    const char *editor = sef_sessao_ide_editor(estado->sessao);
    if (sef_sessao_ide_selecao_editor(estado->sessao, &inicio, &fim) &&
        fim - inicio < sizeof(estado->consulta) &&
        memchr(editor + inicio, '\n', fim - inicio) == NULL) {
        memcpy(estado->consulta, editor + inicio, fim - inicio);
        estado->tamanho_consulta = fim - inicio;
        estado->consulta[estado->tamanho_consulta] = '\0';
        return;
    }
    snprintf(estado->consulta, sizeof(estado->consulta), "%s", estado->ultima_busca);
    estado->tamanho_consulta = strlen(estado->consulta);
}

static void solicitar_fechar_aba(EstadoIde *estado, SefErro *erro) {
    size_t ativa = sef_sessao_ide_documento_ativo(estado->sessao);
    if (!sef_sessao_ide_documento_modificado(estado->sessao, ativa)) {
        sef_sessao_ide_documento_fechar_ativo(estado->sessao, false, erro);
        estado->foco = FOCO_EDITOR;
        return;
    }
    abrir_sobreposicao(estado, SOBREPOSICAO_CONFIRMAR_FECHAMENTO);
    const char *caminho = sef_sessao_ide_documento_caminho(estado->sessao, ativa);
    snprintf(estado->mensagem_sobreposicao, sizeof(estado->mensagem_sobreposicao),
             "Close %.220s without saving?", caminho == NULL ? "untitled.lisp" : caminho);
}

static void acionar_botao(EstadoIde *estado, AcaoBotaoIde acao, SefErro *erro) {
    switch (acao) {
    case ACAO_BOTAO_EXECUTAR:
        estado->foco = FOCO_EDITOR;
        estado->ferramenta = sef_sessao_ide_executar_editor(estado->sessao, erro)
                                 ? FERRAMENTA_INSPETOR
                                 : FERRAMENTA_DEPURADOR;
        break;
    case ACAO_BOTAO_EXECUTAR_FORMA:
        estado->foco = FOCO_EDITOR;
        estado->ferramenta = sef_sessao_ide_executar_forma_no_cursor(estado->sessao, erro)
                                 ? FERRAMENTA_INSPETOR
                                 : FERRAMENTA_DEPURADOR;
        break;
    case ACAO_BOTAO_EXECUTAR_ALTERACOES:
        estado->foco = FOCO_EDITOR;
        estado->ferramenta = sef_sessao_ide_executar_alteracoes(estado->sessao, erro)
                                 ? FERRAMENTA_INSPETOR
                                 : FERRAMENTA_DEPURADOR;
        break;
    case ACAO_BOTAO_SALVAR:
        sef_sessao_ide_salvar(estado->sessao, sef_sessao_ide_caminho(estado->sessao), erro);
        break;
    case ACAO_BOTAO_SALVAMENTO_AUTOMATICO:
        sef_sessao_ide_salvamento_automatico_definir(
            estado->sessao, !sef_sessao_ide_salvamento_automatico(estado->sessao), erro);
        break;
    case ACAO_BOTAO_SNAPSHOT:
        sef_sessao_ide_imagem_salvar(estado->sessao, erro);
        break;
    case ACAO_BOTAO_RESTAURAR:
        sef_sessao_ide_imagem_restaurar(estado->sessao, erro);
        break;
    case ACAO_BOTAO_COMANDOS:
        abrir_sobreposicao(estado, SOBREPOSICAO_COMANDOS);
        break;
    case ACAO_BOTAO_SIMBOLOS:
        abrir_sobreposicao_simbolos(estado);
        break;
    case ACAO_BOTAO_REFERENCIAS:
        estado->foco = FOCO_EDITOR;
        estado->ferramenta = FERRAMENTA_NAVEGADOR;
        sef_sessao_ide_navegar_referencia_espaco_trabalho(
            estado->sessao, SEF_REFERENCIA_PROXIMA, erro);
        break;
    case ACAO_BOTAO_BUSCAR_EDITOR:
        abrir_sobreposicao_busca_editor(estado);
        break;
    case ACAO_BOTAO_IR_PARA_LINHA:
        abrir_sobreposicao(estado, SOBREPOSICAO_IR_PARA_LINHA);
        break;
    case ACAO_BOTAO_NOVA_ABA:
        if (sef_sessao_ide_documento_novo(estado->sessao, erro))
            estado->foco = FOCO_EDITOR;
        break;
    case ACAO_BOTAO_FECHAR_ABA:
        solicitar_fechar_aba(estado, erro);
        break;
    case ACAO_BOTAO_CONTROLE_VERSAO:
        estado->ferramenta = FERRAMENTA_CONTROLE_VERSAO;
        sef_sessao_ide_controle_versao_atualizar(estado->sessao, erro);
        break;
    case ACAO_BOTAO_PERFIL:
        estado->ferramenta = FERRAMENTA_PERFIL;
        break;
    case ACAO_BOTAO_ABRIR_ARQUIVO:
        abrir_sobreposicao_caminho(estado, SOBREPOSICAO_ABRIR_ARQUIVO);
        break;
    case ACAO_BOTAO_ABRIR_PASTA:
        abrir_sobreposicao_caminho(estado, SOBREPOSICAO_ABRIR_PASTA);
        break;
    case ACAO_BOTAO_CRIAR_ARQUIVO:
        abrir_sobreposicao_caminho(estado, SOBREPOSICAO_CRIAR_ARQUIVO);
        break;
    case ACAO_BOTAO_CRIAR_PASTA:
        abrir_sobreposicao_caminho(estado, SOBREPOSICAO_CRIAR_PASTA);
        break;
    case ACAO_BOTAO_ATUALIZAR_EXPLORADOR:
        sef_sessao_ide_espaco_trabalho_atualizar(estado->sessao, erro);
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

static bool contem_sem_diferenciar_caixa(const char *texto, const char *consulta) {
    if (consulta[0] == '\0')
        return true;
    size_t tamanho_consulta = strlen(consulta);
    for (const char *inicio = texto; *inicio != '\0'; inicio++) {
        size_t i = 0;
        while (i < tamanho_consulta && inicio[i] != '\0' &&
               tolower((unsigned char)inicio[i]) == tolower((unsigned char)consulta[i]))
            i++;
        if (i == tamanho_consulta)
            return true;
    }
    return false;
}

static const char *item_sobreposicao(const EstadoIde *estado, size_t indice_filtrado,
                                     size_t *indice_real) {
    size_t encontrado = 0;
    if (estado->sobreposicao == SOBREPOSICAO_ABRIR_RAPIDO) {
        size_t quantidade = sef_sessao_ide_espaco_trabalho_quantidade(estado->sessao);
        for (size_t i = 0; i < quantidade; i++) {
            const char *arquivo = sef_sessao_ide_espaco_trabalho_arquivo(estado->sessao, i);
            if (!contem_sem_diferenciar_caixa(arquivo, estado->consulta))
                continue;
            if (encontrado++ == indice_filtrado) {
                if (indice_real != NULL)
                    *indice_real = i;
                return arquivo;
            }
        }
    } else if (estado->sobreposicao == SOBREPOSICAO_SIMBOLOS) {
        size_t quantidade = sef_sessao_ide_simbolos_espaco_trabalho_quantidade(estado->sessao);
        for (size_t i = 0; i < quantidade; i++) {
            const char *simbolo = sef_sessao_ide_simbolo_espaco_trabalho(estado->sessao, i);
            if (!contem_sem_diferenciar_caixa(simbolo, estado->consulta))
                continue;
            if (encontrado++ == indice_filtrado) {
                if (indice_real != NULL)
                    *indice_real = i;
                return simbolo;
            }
        }
        return NULL;
    } else if (estado->sobreposicao == SOBREPOSICAO_COMANDOS) {
        for (size_t i = 0; i < sizeof(comandos) / sizeof(comandos[0]); i++) {
            if (!contem_sem_diferenciar_caixa(comandos[i].rotulo, estado->consulta))
                continue;
            if (encontrado++ == indice_filtrado) {
                if (indice_real != NULL)
                    *indice_real = i;
                return comandos[i].rotulo;
            }
        }
    }
    return NULL;
}

static size_t quantidade_itens_sobreposicao(const EstadoIde *estado) {
    size_t quantidade = 0;
    if (estado->sobreposicao == SOBREPOSICAO_ABRIR_RAPIDO) {
        size_t total = sef_sessao_ide_espaco_trabalho_quantidade(estado->sessao);
        for (size_t i = 0; i < total; i++)
            if (contem_sem_diferenciar_caixa(
                    sef_sessao_ide_espaco_trabalho_arquivo(estado->sessao, i), estado->consulta))
                quantidade++;
    } else if (estado->sobreposicao == SOBREPOSICAO_SIMBOLOS) {
        size_t total = sef_sessao_ide_simbolos_espaco_trabalho_quantidade(estado->sessao);
        for (size_t i = 0; i < total; i++)
            if (contem_sem_diferenciar_caixa(
                    sef_sessao_ide_simbolo_espaco_trabalho(estado->sessao, i), estado->consulta))
                quantidade++;
    } else if (estado->sobreposicao == SOBREPOSICAO_COMANDOS) {
        for (size_t i = 0; i < sizeof(comandos) / sizeof(comandos[0]); i++)
            if (contem_sem_diferenciar_caixa(comandos[i].rotulo, estado->consulta))
                quantidade++;
    }
    return quantidade;
}

static void desenhar_sobreposicao(SefSuperficie *superficie, EstadoIde *estado) {
    if (estado->sobreposicao == SOBREPOSICAO_NENHUMA)
        return;
    int largura = superficie->largura - 100;
    if (largura > 700)
        largura = 700;
    if (largura < 320)
        largura = 320;
    int altura = superficie->altura - 180;
    if (altura > 390)
        altura = 390;
    if (altura < 220)
        altura = 220;
    int x = (superficie->largura - largura) / 2;
    int y = 82;
    estado->limites_sobreposicao = (SefRetangulo){x, y, largura, altura};

    sef_superficie_retangulo(superficie, x + 6, y + 6, largura, altura, SEF_COR(101, 112, 86));
    sef_superficie_retangulo(superficie, x, y, largura, altura, SEF_COR(244, 238, 211));
    sef_superficie_contorno(superficie, x, y, largura, altura, 2, SEF_COR(181, 112, 52));
    sef_superficie_retangulo(superficie, x + 2, y + 2, largura - 4, 30, SEF_COR(67, 88, 70));
    const char *titulo = "COMMAND PALETTE  CTRL+SHIFT+P";
    if (estado->sobreposicao == SOBREPOSICAO_ABRIR_RAPIDO)
        titulo = "QUICK OPEN  CTRL+P";
    else if (estado->sobreposicao == SOBREPOSICAO_SIMBOLOS)
        titulo = "WORKSPACE SYMBOLS  CTRL+T";
    else if (estado->sobreposicao == SOBREPOSICAO_BUSCAR_EDITOR)
        titulo = "FIND IN ACTIVE EDITOR  CTRL+F";
    else if (estado->sobreposicao == SOBREPOSICAO_IR_PARA_LINHA)
        titulo = "GO TO LINE  CTRL+G";
    else if (estado->sobreposicao == SOBREPOSICAO_CONFIRMAR_FECHAMENTO)
        titulo = "UNSAVED CHANGES";
    else if (estado->sobreposicao == SOBREPOSICAO_ABRIR_ARQUIVO)
        titulo = "OPEN FILE";
    else if (estado->sobreposicao == SOBREPOSICAO_ABRIR_PASTA)
        titulo = "OPEN FOLDER";
    else if (estado->sobreposicao == SOBREPOSICAO_CRIAR_ARQUIVO)
        titulo = "NEW FILE";
    else if (estado->sobreposicao == SOBREPOSICAO_CRIAR_PASTA)
        titulo = "NEW FOLDER";
    sef_superficie_texto(superficie, x + 12, y + 10, titulo, 2, SEF_COR(244, 238, 211));
    sef_superficie_retangulo(superficie, x + 12, y + 42, largura - 24, 32, SEF_COR(231, 224, 194));
    sef_superficie_contorno(superficie, x + 12, y + 42, largura - 24, 32, 1, SEF_COR(101, 112, 86));
    char entrada[160];
    if (estado->sobreposicao == SOBREPOSICAO_CONFIRMAR_FECHAMENTO) {
        snprintf(entrada, sizeof(entrada), "%.159s", estado->mensagem_sobreposicao);
    } else {
        const char *consulta_visivel = estado->consulta;
        if (estado->tamanho_consulta > 150)
            consulta_visivel = estado->consulta + estado->tamanho_consulta - 150;
        snprintf(entrada, sizeof(entrada), "> %.150s|", consulta_visivel);
    }
    limitar_linha(entrada, largura > 48 ? (size_t)(largura - 48) / 12u : 0);
    sef_superficie_texto(superficie, x + 22, y + 50, entrada, 2, SEF_COR(43, 54, 45));

    size_t quantidade = quantidade_itens_sobreposicao(estado);
    if (estado->sobreposicao == SOBREPOSICAO_CONFIRMAR_FECHAMENTO) {
        sef_superficie_texto(superficie, x + 22, y + 92,
                             "This discards the unsaved editor changes.", 2,
                             SEF_COR(143, 65, 45));
        estado->primeiro_item_sobreposicao = 0;
        estado->quantidade_itens_visiveis = 0;
    } else if (estado->sobreposicao == SOBREPOSICAO_IR_PARA_LINHA) {
        const char *mensagem = estado->mensagem_sobreposicao[0] == '\0'
                                   ? "TYPE A LINE NUMBER AND PRESS ENTER"
                                   : estado->mensagem_sobreposicao;
        char linha_mensagem[128];
        snprintf(linha_mensagem, sizeof(linha_mensagem), "%.127s", mensagem);
        limitar_linha(linha_mensagem, largura > 48 ? (size_t)(largura - 48) / 12u : 0);
        sef_superficie_texto(superficie, x + 22, y + 92, linha_mensagem, 2,
                             estado->mensagem_sobreposicao[0] == '\0' ? SEF_COR(75, 84, 67)
                                                                      : SEF_COR(143, 65, 45));
        estado->primeiro_item_sobreposicao = 0;
        estado->quantidade_itens_visiveis = 0;
    } else if (estado->sobreposicao == SOBREPOSICAO_BUSCAR_EDITOR) {
        const char *mensagem = estado->mensagem_sobreposicao[0] == '\0'
                                   ? "ENTER NEXT   SHIFT+ENTER PREVIOUS"
                                   : estado->mensagem_sobreposicao;
        char linha_mensagem[128];
        snprintf(linha_mensagem, sizeof(linha_mensagem), "%.127s", mensagem);
        limitar_linha(linha_mensagem, largura > 48 ? (size_t)(largura - 48) / 12u : 0);
        sef_superficie_texto(superficie, x + 22, y + 92, linha_mensagem, 2,
                             SEF_COR(75, 84, 67));
        estado->primeiro_item_sobreposicao = 0;
        estado->quantidade_itens_visiveis = 0;
    } else if (sobreposicao_e_caminho(estado->sobreposicao)) {
        const char *mensagem = estado->mensagem_sobreposicao[0] == '\0'
                                   ? "TYPE A PATH AND PRESS ENTER"
                                   : estado->mensagem_sobreposicao;
        char linha_mensagem[128];
        snprintf(linha_mensagem, sizeof(linha_mensagem), "%.127s", mensagem);
        limitar_linha(linha_mensagem, largura > 48 ? (size_t)(largura - 48) / 12u : 0);
        sef_superficie_texto(superficie, x + 22, y + 92, linha_mensagem, 2,
                             estado->mensagem_sobreposicao[0] == '\0' ? SEF_COR(75, 84, 67)
                                                                      : SEF_COR(143, 65, 45));
        estado->primeiro_item_sobreposicao = 0;
        estado->quantidade_itens_visiveis = 0;
    } else if (quantidade == 0) {
        char linha_mensagem[128];
        snprintf(linha_mensagem, sizeof(linha_mensagem), "%.127s",
                 estado->mensagem_sobreposicao[0] == '\0' ? "NO MATCHING ITEMS"
                                                            : estado->mensagem_sobreposicao);
        limitar_linha(linha_mensagem, largura > 48 ? (size_t)(largura - 48) / 12u : 0);
        sef_superficie_texto(superficie, x + 22, y + 92, linha_mensagem, 2,
                             estado->mensagem_sobreposicao[0] == '\0' ? SEF_COR(75, 84, 67)
                                                                      : SEF_COR(143, 65, 45));
        estado->primeiro_item_sobreposicao = 0;
        estado->quantidade_itens_visiveis = 0;
    } else {
        size_t capacidade = altura > 126 ? (size_t)(altura - 126) / 22u : 1;
        if (capacidade == 0)
            capacidade = 1;
        size_t primeiro = estado->item_sobreposicao >= capacidade
                              ? estado->item_sobreposicao - capacidade + 1
                              : 0;
        estado->primeiro_item_sobreposicao = primeiro;
        estado->quantidade_itens_visiveis =
            quantidade - primeiro < capacidade ? quantidade - primeiro : capacidade;
        for (size_t linha = 0; linha < estado->quantidade_itens_visiveis; linha++) {
            size_t indice = primeiro + linha;
            int item_y = y + 84 + (int)linha * 22;
            bool selecionado = indice == estado->item_sobreposicao;
            if (selecionado)
                sef_superficie_retangulo(superficie, x + 12, item_y - 3, largura - 24, 20,
                                         SEF_COR(177, 196, 154));
            const char *item = item_sobreposicao(estado, indice, NULL);
            char linha_texto[128];
            snprintf(linha_texto, sizeof(linha_texto), "%c %s", selecionado ? '>' : ' ', item);
            limitar_linha(linha_texto, largura > 48 ? (size_t)(largura - 48) / 12u : 0);
            sef_superficie_texto(superficie, x + 22, item_y, linha_texto, 2, SEF_COR(43, 54, 45));
        }
    }
    const char *ajuda = "ENTER SELECT   ESC CLOSE   UP/DOWN NAVIGATE";
    if (sobreposicao_e_caminho(estado->sobreposicao))
        ajuda = "ENTER CONFIRM   ESC CLOSE";
    else if (estado->sobreposicao == SOBREPOSICAO_BUSCAR_EDITOR)
        ajuda = "ENTER NEXT   SHIFT+ENTER PREVIOUS   ESC CLOSE";
    else if (estado->sobreposicao == SOBREPOSICAO_IR_PARA_LINHA)
        ajuda = "ENTER GO TO LINE   ESC CLOSE";
    else if (estado->sobreposicao == SOBREPOSICAO_CONFIRMAR_FECHAMENTO)
        ajuda = "ENTER DISCARD AND CLOSE   ESC CANCEL";
    sef_superficie_texto(superficie, x + 12, y + altura - 18, ajuda, 1, SEF_COR(75, 84, 67));
}

static size_t quantidade_linhas_editor(const SefSessaoIde *sessao) {
    size_t quantidade = 1;
    const char *codigo = sef_sessao_ide_editor(sessao);
    for (const char *caractere = codigo; *caractere != '\0'; caractere++)
        if (*caractere == '\n')
            quantidade++;
    return quantidade;
}

static size_t digitos_numero_linha(size_t quantidade_linhas) {
    size_t digitos = 1;
    for (size_t valor = quantidade_linhas; valor >= 10; valor /= 10)
        digitos++;
    return digitos;
}

static size_t primeira_linha_visivel_editor(const SefSessaoIde *sessao, size_t linhas_visiveis) {
    size_t linha_cursor = 1;
    sef_sessao_ide_editor_linha_coluna(sessao, &linha_cursor, NULL);
    size_t cursor = sef_sessao_ide_cursor_editor(sessao);
    size_t tamanho = strlen(sef_sessao_ide_editor(sessao));
    size_t linhas_anteriores = cursor == tamanho ? linhas_visiveis : linhas_visiveis / 2u + 1u;
    return linha_cursor >= linhas_anteriores ? linha_cursor - linhas_anteriores + 1u : 1u;
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

    size_t quantidade_linhas = quantidade_linhas_editor(sessao);
    size_t primeira_linha = primeira_linha_visivel_editor(sessao, linhas);
    size_t linha_cursor = 1;
    sef_sessao_ide_editor_linha_coluna(sessao, &linha_cursor, NULL);

    size_t digitos = digitos_numero_linha(quantidade_linhas);
    int largura_glifo = 12;
    int altura_linha = 18;
    int largura_marcador = (int)(digitos + 2u) * largura_glifo;
    int x_marcador = limites.x + 12;
    int x_codigo = x_marcador + largura_marcador;
    int y = limites.y + 36;
    int largura_codigo = limites.largura - 24 - largura_marcador;
    size_t colunas = largura_codigo > 0 ? (size_t)largura_codigo / (size_t)largura_glifo : 0;
    if (colunas > 511)
        colunas = 511;

    int y_inicial = y;
    const char *caractere = inicio;
    char linha_texto[512];
    for (size_t indice = 0;
         indice < linhas && primeira_linha + indice <= quantidade_linhas; indice++) {
        size_t numero_linha = primeira_linha + indice;
        if (numero_linha == linha_cursor)
            sef_superficie_retangulo(superficie, x_marcador - 6, y - 4,
                                     limites.largura - 18, altura_linha,
                                     SEF_COR(235, 229, 199));

        char marcador[32];
        snprintf(marcador, sizeof(marcador), "%*zu", (int)digitos, numero_linha);
        sef_superficie_texto(superficie, x_marcador, y, marcador, 2,
                             numero_linha == linha_cursor ? SEF_COR(143, 65, 45)
                                                          : SEF_COR(101, 112, 86));

        size_t escrito_linha = 0;
        while (*caractere != '\0' && *caractere != '\n') {
            if (escrito_linha < colunas)
                linha_texto[escrito_linha++] = *caractere;
            caractere++;
        }
        if (*caractere == '\n')
            caractere++;
        linha_texto[escrito_linha] = '\0';
        sef_superficie_texto(superficie, x_codigo, y, linha_texto, 2, SEF_COR(43, 54, 45));
        y += altura_linha;
    }
    sef_superficie_retangulo(superficie, x_codigo - 7, y_inicial - 5, 1,
                             (int)linhas * altura_linha, SEF_COR(177, 196, 154));
    free(com_cursor);
}

static bool posicionar_cursor_com_ponteiro(EstadoIde *estado, const SefEventoJanela *evento,
                                           bool selecionar, SefErro *erro) {
    SefRetangulo area_codigo = estado->editor.limites;
    area_codigo.y += 24;
    area_codigo.altura -= 24;
    int y_codigo = area_codigo.y + 36;
    if (evento->y < y_codigo)
        return false;

    size_t linhas_visiveis =
        area_codigo.altura > 44 ? (size_t)(area_codigo.altura - 44) / 18u : 1u;
    size_t indice_linha = (size_t)(evento->y - y_codigo) / 18u;
    if (indice_linha >= linhas_visiveis)
        return false;
    size_t linha = primeira_linha_visivel_editor(estado->sessao, linhas_visiveis) + indice_linha;
    size_t quantidade_linhas = quantidade_linhas_editor(estado->sessao);
    if (linha > quantidade_linhas)
        return false;

    size_t digitos = digitos_numero_linha(quantidade_linhas);
    int x_codigo = area_codigo.x + 12 + (int)(digitos + 2u) * 12;
    size_t coluna = 1;
    if (evento->x > x_codigo)
        coluna = (size_t)(evento->x - x_codigo + 6) / 12u + 1u;
    if (selecionar)
        return sef_sessao_ide_editor_posicionar_selecionando(estado->sessao, linha, coluna, erro);
    return sef_sessao_ide_editor_posicionar(estado->sessao, linha, coluna, erro);
}

static void desenhar_ide(SefSuperficie *superficie, void *dados) {
    EstadoIde *estado = dados;
    const SefCor tinta = SEF_COR(43, 54, 45);
    sef_superficie_limpar(superficie, SEF_COR(207, 198, 164));
    sef_superficie_retangulo(superficie, 0, 0, superficie->largura, 30, SEF_COR(43, 54, 45));
    sef_superficie_texto(superficie, 12, 8, "SEFIRAH LISP  LIVE WORLD", 2, SEF_COR(231, 218, 168));
    desenhar_barra_comandos(superficie, estado);
    sef_componente_organizar(&estado->raiz,
                             (SefRetangulo){0, 98, superficie->largura, superficie->altura - 134},
                             &estado->tema);

    desenhar_painel(superficie, estado->explorador.limites, "EXPLORER",
                    estado->foco == FOCO_EXPLORADOR);
    desenhar_painel(superficie, estado->editor.limites, "EDITOR  [F5 ALL] [F6 FORM] [SHIFT+F6]",
                    estado->foco == FOCO_EDITOR);
    char titulo_ferramenta[64];
    snprintf(titulo_ferramenta, sizeof(titulo_ferramenta), "TOOLS: %s",
             nome_ferramenta(estado->ferramenta));
    desenhar_painel(superficie, estado->ferramentas.limites, titulo_ferramenta,
                    ferramenta_focada(estado));
    desenhar_abas_ferramentas(superficie, estado);
    desenhar_painel(superficie, estado->ouvinte.limites, "LISTENER / REPL  [ENTER] [UP HISTORY]",
                    estado->foco == FOCO_OUVINTE);
    desenhar_abas(superficie, estado->editor.limites, estado->sessao);
    SefRetangulo area_codigo = estado->editor.limites;
    area_codigo.y += 24;
    area_codigo.altura -= 24;
    desenhar_editor(superficie, area_codigo, estado->sessao);
    desenhar_texto_limitado_escala(superficie, estado->explorador.limites,
                                   sef_sessao_ide_explorador(estado->sessao), false, 1);
    SefRetangulo conteudo_ferramenta = estado->ferramentas.limites;
    conteudo_ferramenta.y += 24;
    conteudo_ferramenta.altura -= 24;
    if (estado->ferramenta == FERRAMENTA_INSPETOR)
        desenhar_texto_limitado(superficie, conteudo_ferramenta,
                                sef_sessao_ide_inspetor(estado->sessao), false);
    else if (estado->ferramenta == FERRAMENTA_NAVEGADOR)
        desenhar_texto_limitado(superficie, conteudo_ferramenta,
                                sef_sessao_ide_navegador(estado->sessao), false);
    else if (estado->ferramenta == FERRAMENTA_DEPURADOR)
        desenhar_texto_limitado(superficie, conteudo_ferramenta,
                                sef_sessao_ide_depurador(estado->sessao), false);
    else if (estado->ferramenta == FERRAMENTA_CONTROLE_VERSAO)
        desenhar_texto_limitado_escala(superficie, conteudo_ferramenta,
                                       sef_sessao_ide_controle_versao(estado->sessao), false, 1);
    else
        desenhar_texto_limitado_escala(superficie, conteudo_ferramenta,
                                       sef_sessao_ide_perfil(estado->sessao), false, 1);

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
    size_t linha_cursor = 1;
    size_t coluna_cursor = 1;
    sef_sessao_ide_editor_linha_coluna(estado->sessao, &linha_cursor, &coluna_cursor);
    char estado_barra[768];
    snprintf(estado_barra, sizeof(estado_barra),
             "LN %zu, COL %zu  |  %s  |  AUTO SAVE: %s  |  %s  |  CTRL+S SAVE", linha_cursor,
             coluna_cursor, sef_sessao_ide_caminho(estado->sessao),
             sef_sessao_ide_salvamento_automatico(estado->sessao) ? "ON" : "OFF",
             sef_sessao_ide_estado(estado->sessao));
    size_t colunas_estado = superficie->largura > 20 ? (size_t)(superficie->largura - 20) / 12u : 0;
    limitar_linha(estado_barra, colunas_estado);
    sef_superficie_texto(superficie, 10, superficie->altura - 25, estado_barra, 2, tinta);
    desenhar_sobreposicao(superficie, estado);
}

static void executar_comando(EstadoIde *estado, AcaoComandoIde acao, SefErro *erro) {
    switch (acao) {
    case COMANDO_ABRIR_RAPIDO:
        abrir_sobreposicao(estado, SOBREPOSICAO_ABRIR_RAPIDO);
        break;
    case COMANDO_BUSCAR_SIMBOLOS:
        abrir_sobreposicao_simbolos(estado);
        break;
    case COMANDO_BUSCAR_EDITOR:
        abrir_sobreposicao_busca_editor(estado);
        break;
    case COMANDO_IR_PARA_LINHA:
        abrir_sobreposicao(estado, SOBREPOSICAO_IR_PARA_LINHA);
        break;
    case COMANDO_NOVA_ABA:
        if (sef_sessao_ide_documento_novo(estado->sessao, erro))
            estado->foco = FOCO_EDITOR;
        break;
    case COMANDO_FECHAR_ABA:
        solicitar_fechar_aba(estado, erro);
        break;
    case COMANDO_ABRIR_ARQUIVO:
        abrir_sobreposicao_caminho(estado, SOBREPOSICAO_ABRIR_ARQUIVO);
        break;
    case COMANDO_ABRIR_PASTA:
        abrir_sobreposicao_caminho(estado, SOBREPOSICAO_ABRIR_PASTA);
        break;
    case COMANDO_CRIAR_ARQUIVO:
        abrir_sobreposicao_caminho(estado, SOBREPOSICAO_CRIAR_ARQUIVO);
        break;
    case COMANDO_CRIAR_PASTA:
        abrir_sobreposicao_caminho(estado, SOBREPOSICAO_CRIAR_PASTA);
        break;
    case COMANDO_ATUALIZAR_EXPLORADOR:
        sef_sessao_ide_espaco_trabalho_atualizar(estado->sessao, erro);
        break;
    case COMANDO_EXECUTAR:
        acionar_botao(estado, ACAO_BOTAO_EXECUTAR, erro);
        break;
    case COMANDO_EXECUTAR_FORMA:
        acionar_botao(estado, ACAO_BOTAO_EXECUTAR_FORMA, erro);
        break;
    case COMANDO_EXECUTAR_ALTERACOES:
        acionar_botao(estado, ACAO_BOTAO_EXECUTAR_ALTERACOES, erro);
        break;
    case COMANDO_SALVAR:
        acionar_botao(estado, ACAO_BOTAO_SALVAR, erro);
        break;
    case COMANDO_ALTERNAR_SALVAMENTO_AUTOMATICO:
        acionar_botao(estado, ACAO_BOTAO_SALVAMENTO_AUTOMATICO, erro);
        break;
    case COMANDO_SNAPSHOT:
        acionar_botao(estado, ACAO_BOTAO_SNAPSHOT, erro);
        break;
    case COMANDO_RESTAURAR:
        acionar_botao(estado, ACAO_BOTAO_RESTAURAR, erro);
        break;
    case COMANDO_IR_DEFINICAO:
        estado->foco = FOCO_EDITOR;
        estado->ferramenta = FERRAMENTA_NAVEGADOR;
        sef_sessao_ide_ir_para_definicao_espaco_trabalho(estado->sessao, erro);
        break;
    case COMANDO_NAVEGAR_REFERENCIA:
        estado->foco = FOCO_EDITOR;
        estado->ferramenta = FERRAMENTA_NAVEGADOR;
        sef_sessao_ide_navegar_referencia_espaco_trabalho(
            estado->sessao, SEF_REFERENCIA_PROXIMA, erro);
        break;
    case COMANDO_ATUALIZAR_CONTROLE_VERSAO:
        estado->ferramenta = FERRAMENTA_CONTROLE_VERSAO;
        sef_sessao_ide_controle_versao_atualizar(estado->sessao, erro);
        break;
    case COMANDO_LIMPAR_PERFIL:
        estado->ferramenta = FERRAMENTA_PERFIL;
        sef_sessao_ide_perfil_limpar(estado->sessao, erro);
        break;
    case COMANDO_FOCAR_EXPLORADOR:
        estado->foco = FOCO_EXPLORADOR;
        break;
    case COMANDO_FOCAR_OUVINTE:
        estado->foco = FOCO_OUVINTE;
        break;
    case COMANDO_SELECIONAR_TUDO:
        estado->foco = FOCO_EDITOR;
        sef_sessao_ide_editor_selecionar_tudo(estado->sessao, erro);
        break;
    case COMANDO_DESFAZER:
        estado->foco = FOCO_EDITOR;
        sef_sessao_ide_editor_desfazer(estado->sessao, erro);
        break;
    case COMANDO_REFAZER:
        estado->foco = FOCO_EDITOR;
        sef_sessao_ide_editor_refazer(estado->sessao, erro);
        break;
    }
}

static void executar_item_sobreposicao(EstadoIde *estado, SefErro *erro) {
    size_t indice_real = 0;
    if (item_sobreposicao(estado, estado->item_sobreposicao, &indice_real) == NULL)
        return;
    ModoSobreposicaoIde modo = estado->sobreposicao;
    estado->sobreposicao = SOBREPOSICAO_NENHUMA;
    if (modo == SOBREPOSICAO_ABRIR_RAPIDO) {
        if (sef_sessao_ide_espaco_trabalho_selecionar(estado->sessao, indice_real, erro) &&
            sef_sessao_ide_espaco_trabalho_abrir_selecionado(estado->sessao, erro))
            estado->foco = FOCO_EDITOR;
    } else if (modo == SOBREPOSICAO_SIMBOLOS) {
        if (sef_sessao_ide_simbolo_espaco_trabalho_abrir(estado->sessao, indice_real, erro)) {
            estado->foco = FOCO_EDITOR;
            estado->ferramenta = FERRAMENTA_NAVEGADOR;
        }
    } else {
        executar_comando(estado, comandos[indice_real].acao, erro);
    }
}

static void executar_caminho_sobreposicao(EstadoIde *estado, SefErro *erro) {
    bool executou = false;
    FocoIde foco = estado->foco;
    switch (estado->sobreposicao) {
    case SOBREPOSICAO_ABRIR_ARQUIVO:
        executou = sef_sessao_ide_abrir(estado->sessao, estado->consulta, erro);
        foco = FOCO_EDITOR;
        break;
    case SOBREPOSICAO_ABRIR_PASTA:
        executou = sef_sessao_ide_espaco_trabalho_abrir(estado->sessao, estado->consulta, erro);
        foco = FOCO_EXPLORADOR;
        break;
    case SOBREPOSICAO_CRIAR_ARQUIVO:
        executou = sef_sessao_ide_arquivo_criar(estado->sessao, estado->consulta, erro);
        foco = FOCO_EDITOR;
        break;
    case SOBREPOSICAO_CRIAR_PASTA:
        executou = sef_sessao_ide_diretorio_criar(estado->sessao, estado->consulta, erro);
        foco = FOCO_EXPLORADOR;
        break;
    default:
        return;
    }
    if (executou) {
        estado->sobreposicao = SOBREPOSICAO_NENHUMA;
        estado->foco = foco;
    } else {
        snprintf(estado->mensagem_sobreposicao, sizeof(estado->mensagem_sobreposicao), "%.255s",
                 erro->mensagem);
    }
}

static void executar_busca_sobreposicao(EstadoIde *estado, bool anterior, SefErro *erro) {
    snprintf(estado->ultima_busca, sizeof(estado->ultima_busca), "%s", estado->consulta);
    bool executou = sef_sessao_ide_editor_buscar(
        estado->sessao, estado->consulta, anterior ? SEF_BUSCA_ANTERIOR : SEF_BUSCA_PROXIMA, erro);
    const char *mensagem = executou ? sef_sessao_ide_estado(estado->sessao) : erro->mensagem;
    snprintf(estado->mensagem_sobreposicao, sizeof(estado->mensagem_sobreposicao), "%.255s",
             mensagem);
    estado->foco = FOCO_EDITOR;
}

static void executar_linha_sobreposicao(EstadoIde *estado, SefErro *erro) {
    errno = 0;
    char *fim = NULL;
    unsigned long long valor = strtoull(estado->consulta, &fim, 10);
    while (fim != NULL && isspace((unsigned char)*fim))
        fim++;
    if (estado->consulta[0] == '\0' || fim == estado->consulta || fim == NULL || *fim != '\0' ||
        errno == ERANGE || valor == 0 || valor > (unsigned long long)SIZE_MAX) {
        snprintf(estado->mensagem_sobreposicao, sizeof(estado->mensagem_sobreposicao),
                 "Enter a valid line number greater than zero");
        return;
    }
    if (sef_sessao_ide_editor_ir_para_linha(estado->sessao, (size_t)valor, erro)) {
        estado->sobreposicao = SOBREPOSICAO_NENHUMA;
        estado->foco = FOCO_EDITOR;
    } else {
        snprintf(estado->mensagem_sobreposicao, sizeof(estado->mensagem_sobreposicao), "%.255s",
                 erro->mensagem);
    }
}

static bool tratar_evento_sobreposicao(EstadoIde *estado, const SefEventoJanela *evento,
                                       SefErro *erro) {
    if (evento->tipo == SEF_EVENTO_CANCELAR) {
        estado->sobreposicao = SOBREPOSICAO_NENHUMA;
        return true;
    }
    if (estado->sobreposicao == SOBREPOSICAO_CONFIRMAR_FECHAMENTO) {
        if (evento->tipo == SEF_EVENTO_ENTER) {
            if (sef_sessao_ide_documento_fechar_ativo(estado->sessao, true, erro)) {
                estado->sobreposicao = SOBREPOSICAO_NENHUMA;
                estado->foco = FOCO_EDITOR;
            } else {
                snprintf(estado->mensagem_sobreposicao,
                         sizeof(estado->mensagem_sobreposicao), "%.255s", erro->mensagem);
            }
        } else if (evento->tipo == SEF_EVENTO_PONTEIRO_PRESSIONAR &&
                   !ponto_dentro(estado->limites_sobreposicao, evento->x, evento->y)) {
            estado->sobreposicao = SOBREPOSICAO_NENHUMA;
        }
        return true;
    }
    if (evento->tipo == SEF_EVENTO_TEXTO) {
        size_t tamanho = strlen(evento->texto_utf8);
        if (tamanho > 0 && estado->tamanho_consulta + tamanho < sizeof(estado->consulta)) {
            memcpy(estado->consulta + estado->tamanho_consulta, evento->texto_utf8, tamanho + 1);
            estado->tamanho_consulta += tamanho;
            estado->item_sobreposicao = 0;
            if (estado->sobreposicao != SOBREPOSICAO_SIMBOLOS)
                estado->mensagem_sobreposicao[0] = '\0';
        }
        return true;
    }
    if (evento->tipo == SEF_EVENTO_APAGAR) {
        if (estado->tamanho_consulta > 0) {
            estado->tamanho_consulta--;
            while (estado->tamanho_consulta > 0 &&
                   ((unsigned char)estado->consulta[estado->tamanho_consulta] & 0xc0u) == 0x80u)
                estado->tamanho_consulta--;
            estado->consulta[estado->tamanho_consulta] = '\0';
            estado->item_sobreposicao = 0;
            if (estado->sobreposicao != SOBREPOSICAO_SIMBOLOS)
                estado->mensagem_sobreposicao[0] = '\0';
        }
        return true;
    }
    size_t quantidade = quantidade_itens_sobreposicao(estado);
    if (evento->tipo == SEF_EVENTO_CURSOR_CIMA && quantidade > 0) {
        estado->item_sobreposicao =
            estado->item_sobreposicao == 0 ? quantidade - 1 : estado->item_sobreposicao - 1;
        return true;
    }
    if (evento->tipo == SEF_EVENTO_CURSOR_BAIXO && quantidade > 0) {
        estado->item_sobreposicao = (estado->item_sobreposicao + 1) % quantidade;
        return true;
    }
    if (evento->tipo == SEF_EVENTO_ENTER) {
        if (estado->sobreposicao == SOBREPOSICAO_IR_PARA_LINHA)
            executar_linha_sobreposicao(estado, erro);
        else if (estado->sobreposicao == SOBREPOSICAO_BUSCAR_EDITOR)
            executar_busca_sobreposicao(estado, evento->modificador_shift, erro);
        else if (sobreposicao_e_caminho(estado->sobreposicao))
            executar_caminho_sobreposicao(estado, erro);
        else
            executar_item_sobreposicao(estado, erro);
        return true;
    }
    if (evento->tipo == SEF_EVENTO_PONTEIRO_PRESSIONAR) {
        if (!ponto_dentro(estado->limites_sobreposicao, evento->x, evento->y)) {
            estado->sobreposicao = SOBREPOSICAO_NENHUMA;
            return true;
        }
        if (estado->sobreposicao == SOBREPOSICAO_BUSCAR_EDITOR)
            return true;
        int inicio_y = estado->limites_sobreposicao.y + 81;
        if (evento->y >= inicio_y) {
            size_t linha = (size_t)(evento->y - inicio_y) / 22u;
            if (linha < estado->quantidade_itens_visiveis) {
                estado->item_sobreposicao = estado->primeiro_item_sobreposicao + linha;
                executar_item_sobreposicao(estado, erro);
            }
        }
        return true;
    }
    return true;
}

static bool tratar_evento(const SefEventoJanela *evento, void *dados) {
    EstadoIde *estado = dados;
    SefErro erro;
    sef_erro_limpar(&erro);

    if (evento->tipo == SEF_EVENTO_ABRIR_RAPIDO) {
        abrir_sobreposicao(estado, SOBREPOSICAO_ABRIR_RAPIDO);
        return true;
    }
    if (evento->tipo == SEF_EVENTO_PALETA_COMANDOS) {
        abrir_sobreposicao(estado, SOBREPOSICAO_COMANDOS);
        return true;
    }
    if (evento->tipo == SEF_EVENTO_BUSCAR_SIMBOLOS) {
        abrir_sobreposicao_simbolos(estado);
        return true;
    }
    if (evento->tipo == SEF_EVENTO_BUSCAR_EDITOR) {
        abrir_sobreposicao_busca_editor(estado);
        return true;
    }
    if (evento->tipo == SEF_EVENTO_IR_PARA_LINHA) {
        abrir_sobreposicao(estado, SOBREPOSICAO_IR_PARA_LINHA);
        return true;
    }
    if (evento->tipo == SEF_EVENTO_NOVO_DOCUMENTO) {
        estado->sobreposicao = SOBREPOSICAO_NENHUMA;
        if (sef_sessao_ide_documento_novo(estado->sessao, &erro))
            estado->foco = FOCO_EDITOR;
        return true;
    }
    if (evento->tipo == SEF_EVENTO_FECHAR_DOCUMENTO) {
        estado->sobreposicao = SOBREPOSICAO_NENHUMA;
        solicitar_fechar_aba(estado, &erro);
        return true;
    }
    if (estado->sobreposicao != SOBREPOSICAO_NENHUMA)
        return tratar_evento_sobreposicao(estado, evento, &erro);
    if (evento->tipo == SEF_EVENTO_CANCELAR)
        return true;

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
    case SEF_EVENTO_APAGAR_ADIANTE:
        if (estado->foco == FOCO_EDITOR)
            sef_sessao_ide_editor_apagar_adiante(estado->sessao);
        break;
    case SEF_EVENTO_ENTER:
        if (estado->foco == FOCO_EXPLORADOR) {
            if (sef_sessao_ide_espaco_trabalho_abrir_selecionado(estado->sessao, &erro))
                estado->foco = FOCO_EDITOR;
        } else if (estado->foco == FOCO_EDITOR)
            sef_sessao_ide_editor_nova_linha(estado->sessao, &erro);
        else if (estado->foco == FOCO_OUVINTE) {
            estado->ferramenta = sef_sessao_ide_ouvinte_enviar(estado->sessao, &erro)
                                     ? FERRAMENTA_INSPETOR
                                     : FERRAMENTA_DEPURADOR;
        } else if (estado->foco == FOCO_DEPURADOR) {
            if (sef_sessao_ide_inspecionar_condicao(estado->sessao, &erro)) {
                estado->foco = FOCO_INSPETOR;
                estado->ferramenta = FERRAMENTA_INSPETOR;
            }
        } else
            sef_sessao_ide_inspetor_entrar(estado->sessao, &erro);
        break;
    case SEF_EVENTO_TAB:
        alternar_foco(estado, evento->modificador_shift);
        break;
    case SEF_EVENTO_EXECUTAR:
        estado->ferramenta = sef_sessao_ide_executar_editor(estado->sessao, &erro)
                                 ? FERRAMENTA_INSPETOR
                                 : FERRAMENTA_DEPURADOR;
        break;
    case SEF_EVENTO_EXECUTAR_FORMA:
        if (evento->modificador_shift)
            sef_sessao_ide_editor_selecionar_forma(estado->sessao, &erro);
        else
            estado->ferramenta = sef_sessao_ide_executar_forma_no_cursor(estado->sessao, &erro)
                                     ? FERRAMENTA_INSPETOR
                                     : FERRAMENTA_DEPURADOR;
        break;
    case SEF_EVENTO_EXECUTAR_ALTERACOES:
        estado->ferramenta = sef_sessao_ide_executar_alteracoes(estado->sessao, &erro)
                                 ? FERRAMENTA_INSPETOR
                                 : FERRAMENTA_DEPURADOR;
        break;
    case SEF_EVENTO_NAVEGAR_DEFINICAO:
        estado->foco = FOCO_EDITOR;
        estado->ferramenta = FERRAMENTA_NAVEGADOR;
        sef_sessao_ide_navegar_definicao(
            estado->sessao,
            evento->modificador_shift ? SEF_DEFINICAO_ANTERIOR : SEF_DEFINICAO_PROXIMA, &erro);
        break;
    case SEF_EVENTO_IR_PARA_DEFINICAO:
        estado->foco = FOCO_EDITOR;
        estado->ferramenta = FERRAMENTA_NAVEGADOR;
        sef_sessao_ide_ir_para_definicao_espaco_trabalho(estado->sessao, &erro);
        break;
    case SEF_EVENTO_NAVEGAR_REFERENCIA:
        estado->foco = FOCO_EDITOR;
        estado->ferramenta = FERRAMENTA_NAVEGADOR;
        sef_sessao_ide_navegar_referencia_espaco_trabalho(
            estado->sessao,
            evento->modificador_shift ? SEF_REFERENCIA_ANTERIOR : SEF_REFERENCIA_PROXIMA, &erro);
        break;
    case SEF_EVENTO_SALVAR_IMAGEM:
        if (evento->modificador_shift) {
            estado->foco = FOCO_DEPURADOR;
            estado->ferramenta = FERRAMENTA_DEPURADOR;
            sef_sessao_ide_navegar_condicao(estado->sessao, SEF_CONDICAO_ANTERIOR, &erro);
        } else {
            sef_sessao_ide_imagem_salvar(estado->sessao, &erro);
        }
        break;
    case SEF_EVENTO_RESTAURAR_IMAGEM:
        if (evento->modificador_shift) {
            estado->foco = FOCO_DEPURADOR;
            estado->ferramenta = FERRAMENTA_DEPURADOR;
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
    case SEF_EVENTO_SELECIONAR_TUDO:
        if (estado->foco == FOCO_EDITOR)
            sef_sessao_ide_editor_selecionar_tudo(estado->sessao, &erro);
        break;
    case SEF_EVENTO_SALVAR:
        sef_sessao_ide_salvar(estado->sessao, sef_sessao_ide_caminho(estado->sessao), &erro);
        break;
    case SEF_EVENTO_ABRIR:
        abrir_sobreposicao_caminho(estado, SOBREPOSICAO_ABRIR_ARQUIVO);
        break;
    case SEF_EVENTO_CURSOR_ESQUERDA:
        if (estado->foco == FOCO_EDITOR)
            mover_cursor_editor(estado, evento,
                                evento->modificador_controle ? SEF_CURSOR_PALAVRA_ANTERIOR
                                                            : SEF_CURSOR_ESQUERDA);
        else if (estado->foco == FOCO_INSPETOR)
            sef_sessao_ide_inspetor_mover(estado->sessao, SEF_INSPETOR_ANTERIOR, &erro);
        break;
    case SEF_EVENTO_CURSOR_DIREITA:
        if (estado->foco == FOCO_EDITOR)
            mover_cursor_editor(estado, evento,
                                evento->modificador_controle ? SEF_CURSOR_PALAVRA_SEGUINTE
                                                            : SEF_CURSOR_DIREITA);
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
    case SEF_EVENTO_PONTEIRO_MOVER:
        if (estado->selecionando_com_ponteiro &&
            ponto_dentro(estado->editor.limites, evento->x, evento->y) &&
            (evento->x != estado->selecao_ponteiro_x || evento->y != estado->selecao_ponteiro_y) &&
            posicionar_cursor_com_ponteiro(estado, evento, true, &erro))
            estado->selecao_ponteiro_moveu = true;
        break;
    case SEF_EVENTO_PONTEIRO_SOLTAR:
        if (estado->selecionando_com_ponteiro &&
            (estado->selecao_ponteiro_moveu || evento->x != estado->selecao_ponteiro_x ||
             evento->y != estado->selecao_ponteiro_y) &&
            ponto_dentro(estado->editor.limites, evento->x, evento->y))
            posicionar_cursor_com_ponteiro(estado, evento, true, &erro);
        estado->selecionando_com_ponteiro = false;
        estado->selecao_ponteiro_moveu = false;
        break;
    case SEF_EVENTO_PONTEIRO_PRESSIONAR:
        estado->selecionando_com_ponteiro = false;
        estado->selecao_ponteiro_moveu = false;
        estado->selecao_ponteiro_x = evento->x;
        estado->selecao_ponteiro_y = evento->y;
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
            } else
                estado->selecionando_com_ponteiro =
                    posicionar_cursor_com_ponteiro(estado, evento, false, &erro);
        } else if (ponto_dentro(estado->ferramentas.limites, evento->x, evento->y)) {
            if (selecionar_aba_ferramenta(estado, evento->x, evento->y))
                break;
            if (estado->ferramenta == FERRAMENTA_INSPETOR) {
                if (estado->foco == FOCO_INSPETOR)
                    sef_sessao_ide_inspetor_mover_componente(
                        estado->sessao, SEF_COMPONENTE_INSPETOR_PROXIMO, &erro);
                estado->foco = FOCO_INSPETOR;
            } else if (estado->ferramenta == FERRAMENTA_NAVEGADOR) {
                estado->foco = FOCO_EDITOR;
                sef_sessao_ide_navegar_definicao(estado->sessao, SEF_DEFINICAO_PROXIMA, &erro);
            } else if (estado->ferramenta == FERRAMENTA_DEPURADOR) {
                if (estado->foco == FOCO_DEPURADOR)
                    sef_sessao_ide_navegar_condicao(estado->sessao, SEF_CONDICAO_PROXIMA, &erro);
                estado->foco = FOCO_DEPURADOR;
            } else if (estado->ferramenta == FERRAMENTA_CONTROLE_VERSAO) {
                sef_sessao_ide_controle_versao_atualizar(estado->sessao, &erro);
            }
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
