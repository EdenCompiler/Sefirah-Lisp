#include "ide/ide.h"

#include <stdio.h>
#include <string.h>

static int falhas = 0;

static void verificar(bool condicao, const char *mensagem) {
    if (!condicao) {
        fprintf(stderr, "FALHOU: %s\n", mensagem);
        falhas++;
    }
}

int main(void) {
    SefErro erro;
    SefSessaoIde *sessao = sef_sessao_ide_criar(&erro);
    verificar(sessao != NULL, "sessao da IDE foi criada");
    if (sessao == NULL)
        return 1;

    verificar(sef_sessao_ide_editor_definir(
                  sessao, "(defun resposta (x)\n  (+ x 2))\n(resposta 40)\n", &erro),
              "editor recebeu programa multilinha");
    verificar(sef_sessao_ide_executar_editor(sessao, &erro), "IDE executou o editor");
    verificar(strstr(sef_sessao_ide_transcricao(sessao), "42\n") != NULL,
              "transcricao recebeu o resultado do editor");
    verificar(strstr(sef_sessao_ide_inspetor(sessao), "PRIMARIO: 42") != NULL,
              "inspetor acompanhou o valor avaliado");

    verificar(sef_sessao_ide_ouvinte_inserir(sessao, "(let ((x 40))", &erro) &&
                  sef_sessao_ide_ouvinte_enviar(sessao, &erro),
              "ouvinte reteve primeira linha incompleta");
    verificar(strstr(sef_sessao_ide_estado(sessao), "continuacao") != NULL,
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
    verificar(strstr(sef_sessao_ide_inspetor(sessao), "VALORES: 3") != NULL,
              "inspetor contou valores multiplos");

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
    remove("teste-ide.lisp");

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

    sef_sessao_ide_destruir(sessao);
    if (falhas == 0)
        puts("ide: todos os testes passaram");
    return falhas == 0 ? 0 : 1;
}
