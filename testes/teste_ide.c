#include "ide/ide.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

static int falhas = 0;

static void verificar(bool condicao, const char *mensagem) {
    if (!condicao) {
        fprintf(stderr, "FAILED: %s\n", mensagem);
        falhas++;
    }
}

static bool gravar_arquivo(const char *caminho, const char *texto) {
    FILE *arquivo = fopen(caminho, "wb");
    if (arquivo == NULL)
        return false;
    size_t tamanho = strlen(texto);
    bool gravou = fwrite(texto, 1, tamanho, arquivo) == tamanho;
    return fclose(arquivo) == 0 && gravou;
}

static bool arquivo_igual(const char *caminho, const char *esperado) {
    FILE *arquivo = fopen(caminho, "rb");
    if (arquivo == NULL)
        return false;
    char dados[1024];
    size_t lidos = fread(dados, 1, sizeof(dados) - 1, arquivo);
    bool terminou = feof(arquivo) != 0;
    dados[lidos] = '\0';
    return fclose(arquivo) == 0 && terminou && strcmp(dados, esperado) == 0;
}

static bool criar_diretorio(const char *caminho) {
#ifdef _WIN32
    return _mkdir(caminho) == 0;
#else
    return mkdir(caminho, 0700) == 0;
#endif
}

static void remover_diretorio(const char *caminho) {
#ifdef _WIN32
    _rmdir(caminho);
#else
    rmdir(caminho);
#endif
}

int main(void) {
    SefErro erro;
    SefSessaoIde *sessao = sef_sessao_ide_criar(&erro);
    verificar(sessao != NULL, "sessao da IDE foi criada");
    if (sessao == NULL)
        return 1;

    SefSessaoIde *sessao_sem_nome = sef_sessao_ide_criar(&erro);
    verificar(sessao_sem_nome != NULL &&
                  sef_sessao_ide_salvamento_automatico_definir(sessao_sem_nome, true, &erro) &&
                  sef_sessao_ide_editor_definir(sessao_sem_nome, "(+ 1 1)", &erro) &&
                  sef_sessao_ide_documento_modificado(sessao_sem_nome, 0),
              "Auto Save manteve buffer sem nome pendente ate receber um caminho");
    sef_sessao_ide_destruir(sessao_sem_nome);

    verificar(sef_sessao_ide_editor_definir(
                  sessao, "(defun resposta (x)\n  (+ x 2))\n(resposta 40)\n", &erro),
              "editor recebeu programa multilinha");
    verificar(sef_sessao_ide_executar_editor(sessao, &erro), "IDE executou o editor");
    verificar(strstr(sef_sessao_ide_transcricao(sessao), "42\n") != NULL,
              "transcricao recebeu o resultado do editor");
    verificar(strstr(sef_sessao_ide_inspetor(sessao), "VALUE: 42") != NULL,
              "inspetor acompanhou o valor avaliado");
    verificar(sef_sessao_ide_navegar_definicao(sessao, SEF_DEFINICAO_PROXIMA, &erro) &&
                  strstr(sef_sessao_ide_navegador(sessao), "DEFINITIONS: 1") != NULL &&
                  strstr(sef_sessao_ide_navegador(sessao), "FUNCTION   resposta") != NULL &&
                  sef_sessao_ide_cursor_editor(sessao) == strlen("(defun "),
              "navegador catalogou e visitou uma definicao estrutural");
    verificar(
        sef_sessao_ide_editor_definir(
            sessao, "; (defun falsa ())\n(defun primeira () 1)\n(defmacro segunda (x) x)", &erro) &&
            sef_sessao_ide_navegar_definicao(sessao, SEF_DEFINICAO_PROXIMA, &erro) &&
            strstr(sef_sessao_ide_navegador(sessao), "DEFINITIONS: 2") != NULL &&
            strstr(sef_sessao_ide_navegador(sessao), "L2  FUNCTION   primeira") != NULL &&
            sef_sessao_ide_navegar_definicao(sessao, SEF_DEFINICAO_PROXIMA, &erro) &&
            strstr(sef_sessao_ide_estado(sessao), "Definition: segunda") != NULL &&
            sef_sessao_ide_navegar_definicao(sessao, SEF_DEFINICAO_ANTERIOR, &erro) &&
            strstr(sef_sessao_ide_estado(sessao), "Definition: primeira") != NULL,
        "navegador ignorou comentarios e percorreu definicoes nos dois sentidos");
    const char *codigo_referencias = "(defun somar (a b) (+ a b))\n"
                                     "(defun usar-soma (x) (list \"somar\" (somar x 1)))\n"
                                     "; somar no comentario nao e referencia\n"
                                     "(somar 40 2)";
    const char *primeira_referencia = strstr(codigo_referencias, "(somar x");
    const char *segunda_referencia = strstr(codigo_referencias, "(somar 40");
    verificar(sef_sessao_ide_editor_definir(sessao, codigo_referencias, &erro) &&
                  sef_sessao_ide_navegar_definicao(sessao, SEF_DEFINICAO_PROXIMA, &erro) &&
                  sef_sessao_ide_navegar_referencia(sessao, SEF_REFERENCIA_PROXIMA, &erro) &&
                  primeira_referencia != NULL &&
                  sef_sessao_ide_cursor_editor(sessao) ==
                      (size_t)(primeira_referencia - codigo_referencias + 1) &&
                  strstr(sef_sessao_ide_navegador(sessao), "REFERENCES: 2") != NULL &&
                  strstr(sef_sessao_ide_navegador(sessao), "FUNCTION   usar-soma") != NULL,
              "consulta de callers ignorou texto/comentario e visitou a primeira referencia");
    verificar(sef_sessao_ide_ir_para_definicao(sessao, &erro) &&
                  sef_sessao_ide_cursor_editor(sessao) == strlen("(defun ") &&
                  strstr(sef_sessao_ide_estado(sessao), "Definition found: somar") != NULL,
              "comando de Lisp Machine localizou a definicao do simbolo no cursor");
    verificar(sef_sessao_ide_navegar_referencia(sessao, SEF_REFERENCIA_ANTERIOR, &erro) &&
                  segunda_referencia != NULL &&
                  sef_sessao_ide_cursor_editor(sessao) ==
                      (size_t)(segunda_referencia - codigo_referencias + 1) &&
                  strstr(sef_sessao_ide_estado(sessao), "Reference 2/2") != NULL,
              "consulta de referencias percorreu callers no sentido anterior com retorno");
    verificar(sef_sessao_ide_editor_definir(
                  sessao, "(defun resposta (x)\n  (+ x 2))\n(resposta 40)\n", &erro),
              "editor voltou ao fim depois da navegacao estrutural");

    verificar(sef_sessao_ide_ouvinte_inserir(sessao, "(let ((x 40))", &erro) &&
                  sef_sessao_ide_ouvinte_enviar(sessao, &erro),
              "ouvinte reteve primeira linha incompleta");
    verificar(strstr(sef_sessao_ide_estado(sessao), "continuation") != NULL,
              "estado informou continuacao do ouvinte");
    verificar(sef_sessao_ide_ouvinte_inserir(sessao, "(+ x 2))", &erro) &&
                  sef_sessao_ide_ouvinte_enviar(sessao, &erro),
              "ouvinte executou expressao multilinha");
    verificar(strlen(sef_sessao_ide_ouvinte(sessao)) == 0, "ouvinte limpou a entrada executada");

    verificar(sef_sessao_ide_ouvinte_inserir(sessao, "(values 40 41 42)", &erro) &&
                  sef_sessao_ide_ouvinte_enviar(sessao, &erro),
              "ouvinte aceitou valores multiplos");
    verificar(strstr(sef_sessao_ide_transcricao(sessao), "40\n41\n42\n") != NULL,
              "ouvinte mostrou todos os valores");
    verificar(strstr(sef_sessao_ide_inspetor(sessao), "OBJECTS: 3") != NULL,
              "inspetor reteve valores multiplos");
    verificar(sef_sessao_ide_inspetor_mover(sessao, SEF_INSPETOR_PROXIMO, &erro) &&
                  strstr(sef_sessao_ide_inspetor(sessao), "ROOT: 2/3") != NULL &&
                  strstr(sef_sessao_ide_inspetor(sessao), "VALUE: 41") != NULL,
              "inspetor navegou pela prateleira de objetos vivos");

    verificar(sef_sessao_ide_ouvinte_mover_historico(sessao, SEF_HISTORICO_ANTERIOR, &erro) &&
                  strcmp(sef_sessao_ide_ouvinte(sessao), "(values 40 41 42)") == 0,
              "ouvinte recuperou o evento anterior");
    verificar(sef_sessao_ide_ouvinte_mover_historico(sessao, SEF_HISTORICO_ANTERIOR, &erro) &&
                  strstr(sef_sessao_ide_ouvinte(sessao), "(let ((x 40))") != NULL &&
                  strstr(sef_sessao_ide_ouvinte(sessao), "(+ x 2))") != NULL,
              "historico preservou um evento multilinha");
    verificar(sef_sessao_ide_ouvinte_mover_historico(sessao, SEF_HISTORICO_PROXIMO, &erro) &&
                  strcmp(sef_sessao_ide_ouvinte(sessao), "(values 40 41 42)") == 0 &&
                  sef_sessao_ide_ouvinte_mover_historico(sessao, SEF_HISTORICO_PROXIMO, &erro) &&
                  strlen(sef_sessao_ide_ouvinte(sessao)) == 0,
              "ouvinte percorreu o historico ate o fim");

    verificar(sef_sessao_ide_ouvinte_inserir(sessao, "(values #(10 20) (cons 30 40))", &erro) &&
                  sef_sessao_ide_ouvinte_enviar(sessao, &erro) &&
                  strstr(sef_sessao_ide_inspetor(sessao), "COMPONENTS: 2") != NULL &&
                  strstr(sef_sessao_ide_inspetor(sessao), "> [0]: 10") != NULL,
              "inspetor geral abriu a estrutura do primeiro valor vivo");
    verificar(
        sef_sessao_ide_inspetor_mover_componente(sessao, SEF_COMPONENTE_INSPETOR_PROXIMO, &erro) &&
            strstr(sef_sessao_ide_inspetor(sessao), "> [1]: 20") != NULL &&
            sef_sessao_ide_inspetor_entrar(sessao, &erro) &&
            strstr(sef_sessao_ide_inspetor(sessao), "DEPTH: 1") != NULL &&
            strstr(sef_sessao_ide_inspetor(sessao), "PATH: ROOT > [1]") != NULL &&
            strstr(sef_sessao_ide_inspetor(sessao), "VALUE: 20") != NULL,
        "inspetor entrou em componente selecionado e manteve caminho enraizado");
    verificar(sef_sessao_ide_inspetor_voltar(sessao, &erro) &&
                  strstr(sef_sessao_ide_inspetor(sessao), "DEPTH: 0") != NULL &&
                  strstr(sef_sessao_ide_inspetor(sessao), "VALUE: #(10 20)") != NULL &&
                  sef_sessao_ide_inspetor_mover(sessao, SEF_INSPETOR_PROXIMO, &erro) &&
                  strstr(sef_sessao_ide_inspetor(sessao), "VALUE: (30 . 40)") != NULL &&
                  strstr(sef_sessao_ide_inspetor(sessao), "> CAR: 30") != NULL,
              "inspetor voltou e alternou entre raizes estruturadas");

    verificar(sef_sessao_ide_ouvinte_inserir(
                  sessao,
                  "(restart-case (error \"falha depuravel\") "
                  "(use-value (value) value) (abort () nil))",
                  &erro) &&
                  !sef_sessao_ide_ouvinte_enviar(sessao, &erro) && erro.ocorreu &&
                  strstr(sef_sessao_ide_depurador(sessao), "CONDITIONS: 1") != NULL &&
                  strstr(sef_sessao_ide_depurador(sessao), "TYPE: ERROR") != NULL &&
                  strstr(sef_sessao_ide_depurador(sessao), "falha depuravel") != NULL &&
                  strstr(sef_sessao_ide_depurador(sessao), "RESTARTS AT SIGNAL: 2") != NULL &&
                  strstr(sef_sessao_ide_depurador(sessao), "#<RESTART USE-VALUE>") != NULL &&
                  strstr(sef_sessao_ide_depurador(sessao), "#<RESTART ABORT>") != NULL,
              "depurador reteve uma condicao Lisp e seus restarts historicos");
    verificar(sef_sessao_ide_ouvinte_inserir(sessao, "(+ 20 22)", &erro) &&
                  sef_sessao_ide_ouvinte_enviar(sessao, &erro) &&
                  sef_sessao_ide_inspecionar_condicao(sessao, &erro) &&
                  strstr(sef_sessao_ide_inspetor(sessao), "OBJECTS: 3") != NULL &&
                  strstr(sef_sessao_ide_inspetor(sessao), "TYPE: CONDITION") != NULL &&
                  strstr(sef_sessao_ide_inspetor(sessao), "MESSAGE: \"falha depuravel\"") != NULL &&
                  sef_sessao_ide_inspetor_mover(sessao, SEF_INSPETOR_PROXIMO, &erro) &&
                  strstr(sef_sessao_ide_inspetor(sessao), "TYPE: RESTART") != NULL &&
                  strstr(sef_sessao_ide_inspetor(sessao), "ACTIVE: NIL") != NULL,
              "condicao e restarts historicos permaneceram enraizados no inspetor geral");
    verificar(sef_sessao_ide_ouvinte_inserir(sessao, "simbolo-sem-valor", &erro) &&
                  !sef_sessao_ide_ouvinte_enviar(sessao, &erro) && erro.ocorreu &&
                  strstr(sef_sessao_ide_depurador(sessao), "CONDITIONS: 2") != NULL &&
                  strstr(sef_sessao_ide_depurador(sessao), "symbol") != NULL &&
                  sef_sessao_ide_navegar_condicao(sessao, SEF_CONDICAO_ANTERIOR, &erro) &&
                  strstr(sef_sessao_ide_depurador(sessao), "SELECTED: 1/2") != NULL &&
                  sef_sessao_ide_navegar_condicao(sessao, SEF_CONDICAO_PROXIMA, &erro) &&
                  strstr(sef_sessao_ide_depurador(sessao), "SELECTED: 2/2") != NULL,
              "depurador sintetizou e navegou por condicoes de falhas internas");
    bool historico_limitado = true;
    for (size_t i = 0; i < 33; i++) {
        historico_limitado = historico_limitado &&
                             sef_sessao_ide_ouvinte_inserir(
                                 sessao, "(error \"condicao para limitar historico\")", &erro) &&
                             !sef_sessao_ide_ouvinte_enviar(sessao, &erro) && erro.ocorreu;
    }
    verificar(historico_limitado &&
                  strstr(sef_sessao_ide_depurador(sessao), "CONDITIONS: 32") != NULL,
              "historico do depurador descartou condicoes antigas acima do limite");

    verificar(sef_sessao_ide_editor_inserir(sessao, "; ação", &erro), "editor inseriu texto UTF-8");
    sef_sessao_ide_editor_apagar(sessao);
    verificar(strstr(sef_sessao_ide_editor(sessao), "; açã") != NULL,
              "editor apagou um ponto de codigo UTF-8");
    verificar(sef_sessao_ide_salvar(sessao, "teste-ide.lisp", &erro), "IDE salvou arquivo .lisp");
    verificar(sef_sessao_ide_editor_definir(sessao, "(+ 1 1)", &erro),
              "editor foi alterado depois de salvar");
    verificar(sef_sessao_ide_abrir(sessao, "teste-ide.lisp", &erro), "IDE reabriu arquivo .lisp");
    verificar(strstr(sef_sessao_ide_editor(sessao), "defun resposta") != NULL &&
                  strcmp(sef_sessao_ide_caminho(sessao), "teste-ide.lisp") == 0,
              "IDE restaurou conteudo e caminho");
    verificar(gravar_arquivo("teste-aba-2.lisp", "(defun second-tab () 42)\n") &&
                  sef_sessao_ide_editor_inserir(sessao, "; unsaved", &erro) &&
                  sef_sessao_ide_abrir(sessao, "teste-aba-2.lisp", &erro) &&
                  sef_sessao_ide_quantidade_documentos(sessao) == 2 &&
                  sef_sessao_ide_documento_ativo(sessao) == 1 &&
                  strstr(sef_sessao_ide_abas(sessao), "teste-ide.lisp *") != NULL &&
                  strstr(sef_sessao_ide_abas(sessao), ">[2] teste-aba-2.lisp") != NULL,
              "IDE opened a second file in a visible editor tab");
    verificar(sef_sessao_ide_documento_ativar(sessao, 0, &erro) &&
                  strstr(sef_sessao_ide_editor(sessao), "; unsaved") != NULL &&
                  sef_sessao_ide_documento_modificado(sessao, 0) &&
                  sef_sessao_ide_salvar(sessao, "teste-ide.lisp", &erro) &&
                  !sef_sessao_ide_documento_modificado(sessao, 0) &&
                  strstr(sef_sessao_ide_abas(sessao), "teste-ide.lisp *") == NULL &&
                  sef_sessao_ide_documento_ativar(sessao, 1, &erro) &&
                  strstr(sef_sessao_ide_editor(sessao), "second-tab") != NULL,
              "editor tabs preserved unsaved buffers and independent active state");
    remove("teste-ide.lisp");
    remove("teste-aba-2.lisp");

    verificar(criar_diretorio("teste-espaco") && criar_diretorio("teste-espaco/sub") &&
                  gravar_arquivo("teste-espaco/main.lisp",
                                 "(define main 1)\n"
                                 "(defun UseMixed () (MixedHelper))\n") &&
                  gravar_arquivo("teste-espaco/sub/helper.lisp",
                                 "; (defun FakeHelper () 0)\n"
                                 "(defun MixedHelper () 2)\n") &&
                  gravar_arquivo("teste-espaco/ignored.txt", "not Lisp\n") &&
                  sef_sessao_ide_espaco_trabalho_abrir(sessao, "teste-espaco", &erro) &&
                  sef_sessao_ide_espaco_trabalho_quantidade(sessao) == 2 &&
                  strcmp(sef_sessao_ide_espaco_trabalho_arquivo(sessao, 0), "main.lisp") == 0 &&
                  strstr(sef_sessao_ide_espaco_trabalho_arquivo(sessao, 1), "helper.lisp") !=
                      NULL &&
                  strstr(sef_sessao_ide_explorador(sessao), "LISP FILES: 2") != NULL,
              "workspace explorer indexed recursive Lisp sources and ignored other files");
    verificar(sef_sessao_ide_espaco_trabalho_mover(sessao, SEF_ARQUIVO_PROXIMO, &erro) &&
                  sef_sessao_ide_espaco_trabalho_selecionado(sessao) == 1 &&
                  sef_sessao_ide_espaco_trabalho_abrir_selecionado(sessao, &erro) &&
                  strstr(sef_sessao_ide_editor(sessao), "MixedHelper") != NULL &&
                  strstr(sef_sessao_ide_explorador(sessao), "helper.lisp") != NULL,
              "workspace explorer opened the selected file in an editor tab");
    verificar(sef_sessao_ide_simbolos_espaco_trabalho_buscar(sessao, "mixedhelper", &erro) &&
                  sef_sessao_ide_simbolos_espaco_trabalho_quantidade(sessao) == 1 &&
                  strstr(sef_sessao_ide_simbolo_espaco_trabalho(sessao, 0),
                         "sub/helper.lisp:2") != NULL &&
                  strstr(sef_sessao_ide_simbolo_espaco_trabalho(sessao, 0), "MixedHelper") !=
                      NULL &&
                  strstr(sef_sessao_ide_simbolo_espaco_trabalho(sessao, 0), "[SOURCE ONLY]") !=
                      NULL &&
                  strstr(sef_sessao_ide_navegador(sessao), "WORKSPACE SYMBOLS: 1") != NULL &&
                  sef_sessao_ide_simbolo_espaco_trabalho_abrir(sessao, 0, &erro) &&
                  strstr(sef_sessao_ide_editor(sessao), "MixedHelper") != NULL &&
                  sef_sessao_ide_cursor_editor(sessao) ==
                      (size_t)(strstr(sef_sessao_ide_editor(sessao), "MixedHelper") -
                               sef_sessao_ide_editor(sessao)),
              "workspace symbol search opened an exact definition across project files");
    verificar(sef_sessao_ide_executar_editor(sessao, &erro) &&
                  sef_sessao_ide_simbolos_espaco_trabalho_buscar(sessao, "mixedhelper", &erro) &&
                  strstr(sef_sessao_ide_simbolo_espaco_trabalho(sessao, 0),
                         "[LIVE FUNCTION]") != NULL,
              "workspace symbol search reported definitions installed in the live world");
    verificar(sef_sessao_ide_referencias_espaco_trabalho_buscar(sessao, "mixedhelper", &erro) &&
                  sef_sessao_ide_referencias_espaco_trabalho_quantidade(sessao) == 1 &&
                  strstr(sef_sessao_ide_referencia_espaco_trabalho(sessao, 0), "main.lisp:2") !=
                      NULL &&
                  strstr(sef_sessao_ide_referencia_espaco_trabalho(sessao, 0), "UseMixed") !=
                      NULL &&
                  strstr(sef_sessao_ide_navegador(sessao), "WORKSPACE REFERENCES: 1") != NULL &&
                  sef_sessao_ide_referencia_espaco_trabalho_abrir(sessao, 0, &erro) &&
                  strcmp(sef_sessao_ide_caminho(sessao), "teste-espaco/main.lisp") == 0 &&
                  strstr(sef_sessao_ide_estado(sessao), "Workspace reference 1/1") != NULL,
              "workspace reference search excluded the definition and opened its cross-file use");
    verificar(sef_sessao_ide_ir_para_definicao_espaco_trabalho(sessao, &erro) &&
                  strcmp(sef_sessao_ide_caminho(sessao), "teste-espaco/sub/helper.lisp") == 0 &&
                  sef_sessao_ide_cursor_editor(sessao) ==
                      (size_t)(strstr(sef_sessao_ide_editor(sessao), "MixedHelper") -
                               sef_sessao_ide_editor(sessao)) &&
                  strstr(sef_sessao_ide_estado(sessao), "Workspace definition:") != NULL,
              "F11-style navigation resolved a definition in another workspace file");
    verificar(sef_sessao_ide_navegar_referencia_espaco_trabalho(
                  sessao, SEF_REFERENCIA_PROXIMA, &erro) &&
                  strcmp(sef_sessao_ide_caminho(sessao), "teste-espaco/main.lisp") == 0 &&
                  strstr(sef_sessao_ide_estado(sessao), "Workspace reference 1/1") != NULL,
              "F12-style workspace reference navigation wrapped around its result set");
    verificar(sef_sessao_ide_simbolos_espaco_trabalho_buscar(sessao, "fakehelper", &erro) &&
                  sef_sessao_ide_simbolos_espaco_trabalho_quantidade(sessao) == 0,
              "workspace symbol search ignored definitions inside comments");
    verificar(
        sef_sessao_ide_diretorio_criar(sessao, "teste-espaco/Mixed-Folder", &erro) &&
            sef_sessao_ide_arquivo_criar(sessao, "teste-espaco/Mixed-Folder/NewFile.lisp", &erro) &&
            sef_sessao_ide_espaco_trabalho_quantidade(sessao) == 3 &&
            strcmp(sef_sessao_ide_caminho(sessao), "teste-espaco/Mixed-Folder/NewFile.lisp") == 0 &&
            strstr(sef_sessao_ide_explorador(sessao), "Mixed-Folder/NewFile.lisp") != NULL,
        "file and folder actions preserved mixed-case paths inside the workspace");
    verificar(
        sef_sessao_ide_editor_definir(sessao,
                                      "(defun CamelCaseDefinition () (MixedHelper))\n", &erro) &&
            sef_sessao_ide_simbolos_espaco_trabalho_buscar(sessao, "camelcasedefinition", &erro) &&
            sef_sessao_ide_simbolos_espaco_trabalho_quantidade(sessao) == 1 &&
            strstr(sef_sessao_ide_simbolo_espaco_trabalho(sessao, 0),
                   "Mixed-Folder/NewFile.lisp:1") != NULL &&
            sef_sessao_ide_simbolo_espaco_trabalho_abrir(sessao, 0, &erro) &&
            strcmp(sef_sessao_ide_editor(sessao),
                   "(defun CamelCaseDefinition () (MixedHelper))\n") == 0 &&
            sef_sessao_ide_cursor_editor(sessao) == strlen("(defun ") &&
            strstr(sef_sessao_ide_estado(sessao), "Workspace definition:") != NULL,
        "workspace symbols used unsaved live buffers and preserved mixed-case paths and names");
    verificar(sef_sessao_ide_referencias_espaco_trabalho_buscar(sessao, "MixedHelper", &erro) &&
                  sef_sessao_ide_referencias_espaco_trabalho_quantidade(sessao) == 2,
              "workspace references found saved and unsaved callers");
    const char *primeira_referencia_nova =
        sef_sessao_ide_referencia_espaco_trabalho(sessao, 0);
    size_t referencia_nova =
        primeira_referencia_nova != NULL &&
                strstr(primeira_referencia_nova, "Mixed-Folder/NewFile.lisp:1") != NULL
            ? 0
            : 1;
    const char *rotulo_referencia_nova =
        sef_sessao_ide_referencia_espaco_trabalho(sessao, referencia_nova);
    verificar(rotulo_referencia_nova != NULL &&
                  strstr(rotulo_referencia_nova, "Mixed-Folder/NewFile.lisp:1") != NULL &&
                  sef_sessao_ide_referencia_espaco_trabalho_abrir(sessao, referencia_nova,
                                                                 &erro) &&
                  strstr(sef_sessao_ide_editor(sessao), "CamelCaseDefinition") != NULL,
              "workspace references included unsaved callers and preserved their exact path");
    verificar(gravar_arquivo("teste-espaco/refreshed.lisp", "(define refreshed 3)\n") &&
                  sef_sessao_ide_espaco_trabalho_atualizar(sessao, &erro) &&
                  sef_sessao_ide_espaco_trabalho_quantidade(sessao) == 4 &&
                  strstr(sef_sessao_ide_estado(sessao), "Explorer refreshed") != NULL,
              "refresh action detected a source created outside the IDE");
    const char *codigo_autosalvo = "(defun AutoSavedMixedCase () 42)\n";
    verificar(sef_sessao_ide_salvamento_automatico_definir(sessao, true, &erro) &&
                  sef_sessao_ide_salvamento_automatico(sessao) &&
                  sef_sessao_ide_editor_definir(sessao, codigo_autosalvo, &erro) &&
                  !sef_sessao_ide_documento_modificado(
                      sessao, sef_sessao_ide_documento_ativo(sessao)) &&
                  arquivo_igual("teste-espaco/Mixed-Folder/NewFile.lisp", codigo_autosalvo) &&
                  strstr(sef_sessao_ide_estado(sessao), "Auto saved:") != NULL,
              "Auto Save gravou imediatamente uma aba nomeada em caminho com caixa mista");
    verificar(sef_sessao_ide_editor_inserir(sessao, "; autosaved", &erro) &&
                  arquivo_igual("teste-espaco/Mixed-Folder/NewFile.lisp",
                                "(defun AutoSavedMixedCase () 42)\n; autosaved") &&
                  sef_sessao_ide_editor_desfazer(sessao, &erro) &&
                  arquivo_igual("teste-espaco/Mixed-Folder/NewFile.lisp", codigo_autosalvo) &&
                  !sef_sessao_ide_documento_modificado(
                      sessao, sef_sessao_ide_documento_ativo(sessao)),
              "Auto Save acompanhou insercao e undo na linha do tempo do editor");
    verificar(sef_sessao_ide_salvamento_automatico_definir(sessao, false, &erro) &&
                  !sef_sessao_ide_salvamento_automatico(sessao) &&
                  sef_sessao_ide_editor_inserir(sessao, "; pending", &erro) &&
                  sef_sessao_ide_documento_modificado(
                      sessao, sef_sessao_ide_documento_ativo(sessao)) &&
                  arquivo_igual("teste-espaco/Mixed-Folder/NewFile.lisp", codigo_autosalvo),
              "Auto Save desligado preservou a edicao pendente sem alterar o arquivo");
    remove("teste-espaco/main.lisp");
    remove("teste-espaco/sub/helper.lisp");
    remove("teste-espaco/ignored.txt");
    remove("teste-espaco/Mixed-Folder/NewFile.lisp");
    remove("teste-espaco/refreshed.lisp");
    remover_diretorio("teste-espaco/Mixed-Folder");
    remover_diretorio("teste-espaco/sub");
    remover_diretorio("teste-espaco");

    verificar(sef_sessao_ide_editor_definir(sessao, "abc\ndef", &erro),
              "editor preparou teste de cursor");
    sef_sessao_ide_editor_mover_cursor(sessao, SEF_CURSOR_CIMA);
    sef_sessao_ide_editor_mover_cursor(sessao, SEF_CURSOR_INICIO_LINHA);
    sef_sessao_ide_editor_mover_cursor(sessao, SEF_CURSOR_DIREITA);
    verificar(sef_sessao_ide_editor_inserir(sessao, "X", &erro) &&
                  strcmp(sef_sessao_ide_editor(sessao), "aXbc\ndef") == 0,
              "editor inseriu texto na posicao do cursor");
    sef_sessao_ide_editor_apagar(sessao);
    verificar(strcmp(sef_sessao_ide_editor(sessao), "abc\ndef") == 0 &&
                  sef_sessao_ide_cursor_editor(sessao) == 1,
              "editor apagou antes do cursor e preservou o restante");

    verificar(sef_sessao_ide_editor_definir(sessao, "abc", &erro) &&
                  sef_sessao_ide_editor_inserir(sessao, "X", &erro) &&
                  sef_sessao_ide_editor_desfazer(sessao, &erro) &&
                  strcmp(sef_sessao_ide_editor(sessao), "abc") == 0,
              "linha do tempo desfez uma edicao");
    verificar(sef_sessao_ide_editor_refazer(sessao, &erro) &&
                  strcmp(sef_sessao_ide_editor(sessao), "abcX") == 0,
              "linha do tempo refez uma edicao");
    verificar(sef_sessao_ide_editor_desfazer(sessao, &erro) &&
                  sef_sessao_ide_editor_inserir(sessao, "Y", &erro) &&
                  sef_sessao_ide_editor_refazer(sessao, &erro) &&
                  strcmp(sef_sessao_ide_editor(sessao), "abcY") == 0,
              "nova edicao descartou o ramo antigo de refazer");

    const char *codigo_estrutural = "(defun selecionada () 42)\n(+ 1 2)";
    size_t inicio_selecao = 0;
    size_t fim_selecao = 0;
    verificar(sef_sessao_ide_editor_definir(sessao, codigo_estrutural, &erro),
              "editor preparou selecao estrutural");
    sef_sessao_ide_editor_mover_cursor(sessao, SEF_CURSOR_CIMA);
    verificar(sef_sessao_ide_editor_selecionar_forma(sessao, &erro) &&
                  sef_sessao_ide_selecao_editor(sessao, &inicio_selecao, &fim_selecao) &&
                  inicio_selecao == 0 && fim_selecao == strlen("(defun selecionada () 42)"),
              "Shift+F6 selecionou a forma Lisp completa no cursor");
    verificar(sef_sessao_ide_editor_inserir(sessao, "(defun substituida () 43)", &erro) &&
                  strcmp(sef_sessao_ide_editor(sessao), "(defun substituida () 43)\n(+ 1 2)") ==
                      0 &&
                  !sef_sessao_ide_selecao_editor(sessao, &inicio_selecao, &fim_selecao) &&
                  sef_sessao_ide_editor_desfazer(sessao, &erro) &&
                  strcmp(sef_sessao_ide_editor(sessao), codigo_estrutural) == 0,
              "digitacao substituiu a forma selecionada como uma unica edicao reversivel");
    sef_sessao_ide_editor_mover_cursor(sessao, SEF_CURSOR_CIMA);
    verificar(sef_sessao_ide_editor_selecionar_forma(sessao, &erro),
              "forma voltou a ser selecionada para apagar");
    sef_sessao_ide_editor_apagar(sessao);
    verificar(strcmp(sef_sessao_ide_editor(sessao), "\n(+ 1 2)") == 0 &&
                  sef_sessao_ide_editor_desfazer(sessao, &erro) &&
                  strcmp(sef_sessao_ide_editor(sessao), codigo_estrutural) == 0,
              "Backspace apagou a selecao estrutural e undo a restaurou");

    verificar(sef_sessao_ide_editor_definir(sessao, "ação", &erro),
              "editor preparou selecao UTF-8");
    sef_sessao_ide_editor_mover_cursor_selecionando(sessao, SEF_CURSOR_ESQUERDA);
    sef_sessao_ide_editor_mover_cursor_selecionando(sessao, SEF_CURSOR_ESQUERDA);
    verificar(sef_sessao_ide_selecao_editor(sessao, &inicio_selecao, &fim_selecao) &&
                  fim_selecao - inicio_selecao == strlen("ão") &&
                  sef_sessao_ide_editor_inserir(sessao, "X", &erro) &&
                  strcmp(sef_sessao_ide_editor(sessao), "açX") == 0,
              "Shift+setas selecionou pontos de codigo UTF-8 sem cortar bytes");

    verificar(sef_sessao_ide_editor_definir(sessao, "(+ simbolo-inexistente 1)\n(+ 7 8)", &erro) &&
                  sef_sessao_ide_executar_forma_no_cursor(sessao, &erro),
              "editor avaliou somente a forma completa no cursor");
    verificar(strstr(sef_sessao_ide_inspetor(sessao), "VALUE: 15") != NULL,
              "avaliacao estrutural atualizou o inspetor");
    verificar(sef_sessao_ide_editor_definir(sessao, "(+ simbolo-inexistente 1)\n'|Nome com espaço|",
                                            &erro) &&
                  sef_sessao_ide_executar_forma_no_cursor(sessao, &erro) &&
                  strstr(sef_sessao_ide_inspetor(sessao), "VALUE: |Nome com espaço|") != NULL,
              "avaliacao estrutural aceitou simbolo escapado com espacos");

    verificar(sef_sessao_ide_editor_definir(
                  sessao, "(define valor-incremental 40)\n(+ valor-incremental 2)", &erro) &&
                  sef_sessao_ide_executar_editor(sessao, &erro) &&
                  sef_sessao_ide_executar_alteracoes(sessao, &erro) &&
                  strstr(sef_sessao_ide_estado(sessao), "No changed forms") != NULL,
              "avaliacao incremental ignorou formas ja instaladas no mundo");
    verificar(sef_sessao_ide_editor_definir(
                  sessao, "(define valor-incremental 41)\n(+ valor-incremental 2)", &erro) &&
                  sef_sessao_ide_executar_alteracoes(sessao, &erro) &&
                  strstr(sef_sessao_ide_estado(sessao), "1 changed form(s)") != NULL &&
                  sef_sessao_ide_ouvinte_inserir(sessao, "valor-incremental", &erro) &&
                  sef_sessao_ide_ouvinte_enviar(sessao, &erro) &&
                  strstr(sef_sessao_ide_transcricao(sessao), "\n41\n") != NULL,
              "avaliacao incremental reinstalou somente a forma modificada");
    verificar(
        sef_sessao_ide_editor_definir(sessao,
                                      "(define contador-incremental 0)\n"
                                      "(set 'contador-incremental (+ contador-incremental 1))",
                                      &erro) &&
            sef_sessao_ide_executar_editor(sessao, &erro) &&
            sef_sessao_ide_editor_definir(sessao,
                                          "(define contador-incremental 0)\n"
                                          "(set 'contador-incremental (+ contador-incremental 1))\n"
                                          "(set 'contador-incremental (+ contador-incremental 1))",
                                          &erro) &&
            sef_sessao_ide_executar_alteracoes(sessao, &erro) &&
            strstr(sef_sessao_ide_estado(sessao), "1 changed form(s)") != NULL &&
            sef_sessao_ide_ouvinte_inserir(sessao, "contador-incremental", &erro) &&
            sef_sessao_ide_ouvinte_enviar(sessao, &erro) &&
            strstr(sef_sessao_ide_transcricao(sessao), "\n2\n") != NULL,
        "avaliacao incremental distinguiu ocorrencias de formas identicas");

    verificar(sef_sessao_ide_editor_definir(sessao, "(define estado-do-mundo 40)", &erro) &&
                  sef_sessao_ide_salvar(sessao, "teste-mundo.lisp", &erro) &&
                  sef_sessao_ide_executar_editor(sessao, &erro) &&
                  sef_sessao_ide_imagem_salvar(sessao, &erro) &&
                  sef_sessao_ide_ouvinte_inserir(sessao, "(set 'estado-do-mundo 99)", &erro) &&
                  sef_sessao_ide_ouvinte_enviar(sessao, &erro) &&
                  sef_sessao_ide_imagem_restaurar(sessao, &erro) &&
                  strstr(sef_sessao_ide_inspetor(sessao), "WORLD RESTORED") != NULL &&
                  sef_sessao_ide_ouvinte_inserir(sessao, "estado-do-mundo", &erro) &&
                  sef_sessao_ide_ouvinte_enviar(sessao, &erro) &&
                  strstr(sef_sessao_ide_transcricao(sessao), "\n40\n") != NULL,
              "IDE salvou e restaurou um snapshot do mundo Lisp");
    remove("teste-mundo.imagem");
    verificar(!sef_sessao_ide_imagem_restaurar(sessao, &erro) && erro.ocorreu &&
                  sef_sessao_ide_ouvinte_inserir(sessao, "(+ estado-do-mundo 2)", &erro) &&
                  sef_sessao_ide_ouvinte_enviar(sessao, &erro) &&
                  strstr(sef_sessao_ide_transcricao(sessao), "\n42\n") != NULL,
              "falha ao restaurar preservou o mundo Lisp ativo");
    remove("teste-mundo.lisp");

    sef_sessao_ide_destruir(sessao);
    SefSessaoIde *sessao_vazia = sef_sessao_ide_criar(&erro);
    verificar(gravar_arquivo("teste-primeira-aba.lisp", "(define first-file 1)\n") &&
                  sessao_vazia != NULL &&
                  sef_sessao_ide_abrir(sessao_vazia, "teste-primeira-aba.lisp", &erro) &&
                  sef_sessao_ide_quantidade_documentos(sessao_vazia) == 1 &&
                  strcmp(sef_sessao_ide_caminho(sessao_vazia), "teste-primeira-aba.lisp") == 0,
              "opening the first file reused the empty untitled editor tab");
    sef_sessao_ide_destruir(sessao_vazia);
    remove("teste-primeira-aba.lisp");
    if (falhas == 0)
        puts("ide: all tests passed");
    return falhas == 0 ? 0 : 1;
}
