#include "ide/ide.h"

#include "apoio.h"

#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEF_LIMITE_ARQUIVO_IDE (64L * 1024L * 1024L)

typedef struct TextoIde {
    char *dados;
    size_t tamanho;
    size_t capacidade;
} TextoIde;

struct SefSessaoIde {
    SefRuntime *runtime;
    TextoIde editor;
    TextoIde ouvinte;
    TextoIde transcricao;
    TextoIde inspetor;
    TextoIde navegador;
    TextoIde estado;
    TextoIde caminho;
    TextoIde caminho_imagem;
    size_t cursor_editor;
    SefHistoricoTextoIde *historico_ouvinte;
    SefHistoricoEditorIde *historico_editor;
    SefRaiz **objetos_inspecao;
    size_t quantidade_objetos_inspecao;
    size_t objeto_selecionado;
    uint64_t *formas_executadas;
    size_t quantidade_formas_executadas;
    size_t capacidade_formas_executadas;
};

static bool texto_reservar(TextoIde *texto, size_t necessario, SefErro *erro) {
    if (necessario <= texto->capacidade)
        return true;
    size_t capacidade = texto->capacidade == 0 ? 128 : texto->capacidade;
    while (capacidade < necessario) {
        if (capacidade > SIZE_MAX / 2u) {
            capacidade = necessario;
            break;
        }
        capacidade *= 2u;
    }
    char *dados = realloc(texto->dados, capacidade);
    if (dados == NULL) {
        sef_erro_definir(erro, 0, 0, "memoria insuficiente na sessao da IDE");
        return false;
    }
    texto->dados = dados;
    texto->capacidade = capacidade;
    return true;
}

static bool texto_definir_n(TextoIde *texto, const char *dados, size_t tamanho, SefErro *erro) {
    if (tamanho > SIZE_MAX - 1 || !texto_reservar(texto, tamanho + 1, erro))
        return false;
    if (tamanho > 0)
        memcpy(texto->dados, dados, tamanho);
    texto->dados[tamanho] = '\0';
    texto->tamanho = tamanho;
    return true;
}

static bool texto_definir(TextoIde *texto, const char *dados, SefErro *erro) {
    return texto_definir_n(texto, dados, strlen(dados), erro);
}

static bool texto_acrescentar_n(TextoIde *texto, const char *dados, size_t tamanho, SefErro *erro) {
    if (tamanho > SIZE_MAX - texto->tamanho - 1 ||
        !texto_reservar(texto, texto->tamanho + tamanho + 1, erro))
        return false;
    if (tamanho > 0)
        memcpy(texto->dados + texto->tamanho, dados, tamanho);
    texto->tamanho += tamanho;
    texto->dados[texto->tamanho] = '\0';
    return true;
}

static bool texto_acrescentar(TextoIde *texto, const char *dados, SefErro *erro) {
    return texto_acrescentar_n(texto, dados, strlen(dados), erro);
}

static bool texto_inserir_n(TextoIde *texto, size_t posicao, const char *dados, size_t tamanho,
                            SefErro *erro) {
    if (posicao > texto->tamanho) {
        sef_erro_definir(erro, 0, 0, "cursor fora do texto da IDE");
        return false;
    }
    if (tamanho > SIZE_MAX - texto->tamanho - 1 ||
        !texto_reservar(texto, texto->tamanho + tamanho + 1, erro))
        return false;
    memmove(texto->dados + posicao + tamanho, texto->dados + posicao, texto->tamanho - posicao + 1);
    memcpy(texto->dados + posicao, dados, tamanho);
    texto->tamanho += tamanho;
    return true;
}

static bool texto_formatar(TextoIde *texto, SefErro *erro, const char *formato, ...) {
    va_list argumentos;
    va_start(argumentos, formato);
    va_list copia;
    va_copy(copia, argumentos);
    int tamanho = vsnprintf(NULL, 0, formato, copia);
    va_end(copia);
    if (tamanho < 0 || !texto_reservar(texto, (size_t)tamanho + 1, erro)) {
        va_end(argumentos);
        return false;
    }
    vsnprintf(texto->dados, texto->capacidade, formato, argumentos);
    va_end(argumentos);
    texto->tamanho = (size_t)tamanho;
    return true;
}

static size_t utf8_anterior(const TextoIde *texto, size_t posicao) {
    if (posicao == 0)
        return 0;
    posicao--;
    while (posicao > 0 && ((unsigned char)texto->dados[posicao] & 0xc0u) == 0x80u)
        posicao--;
    return posicao;
}

static size_t utf8_proximo(const TextoIde *texto, size_t posicao) {
    if (posicao >= texto->tamanho)
        return texto->tamanho;
    posicao++;
    while (posicao < texto->tamanho && ((unsigned char)texto->dados[posicao] & 0xc0u) == 0x80u)
        posicao++;
    return posicao;
}

static void texto_apagar_utf8(TextoIde *texto) {
    size_t inicio = utf8_anterior(texto, texto->tamanho);
    texto->tamanho = inicio;
    texto->dados[inicio] = '\0';
}

static void texto_liberar(TextoIde *texto) {
    free(texto->dados);
    memset(texto, 0, sizeof(*texto));
}

static bool atualizar_caminho_imagem(SefSessaoIde *sessao, SefErro *erro) {
    const char *caminho = sessao->caminho.dados;
    size_t tamanho = sessao->caminho.tamanho;
    size_t inicio_nome = 0;
    size_t extensao = tamanho;
    for (size_t i = 0; i < tamanho; i++) {
        if (caminho[i] == '/' || caminho[i] == '\\') {
            inicio_nome = i + 1;
            extensao = tamanho;
        } else if (caminho[i] == '.' && i >= inicio_nome) {
            extensao = i;
        }
    }
    return texto_definir_n(&sessao->caminho_imagem, caminho, extensao, erro) &&
           texto_acrescentar(&sessao->caminho_imagem, ".imagem", erro);
}

static bool codigo_vazio(const char *codigo) {
    while (*codigo != '\0') {
        if (*codigo != ' ' && *codigo != '\t' && *codigo != '\r' && *codigo != '\n')
            return false;
        codigo++;
    }
    return true;
}

static bool registrar_erro(SefSessaoIde *sessao, const char *origem, const SefErro *causa,
                           SefErro *erro) {
    if (!texto_formatar(&sessao->estado, erro, "%s: %s", origem, causa->mensagem))
        return false;
    char cabecalho[96];
    int tamanho = snprintf(cabecalho, sizeof(cabecalho), "\n[%s]\nERRO: ", origem);
    return tamanho > 0 && (size_t)tamanho < sizeof(cabecalho) &&
           texto_acrescentar(&sessao->transcricao, cabecalho, erro) &&
           texto_acrescentar(&sessao->transcricao, causa->mensagem, erro) &&
           texto_acrescentar(&sessao->transcricao, "\n", erro);
}

static void liberar_objetos_inspecao(SefSessaoIde *sessao) {
    for (size_t i = 0; i < sessao->quantidade_objetos_inspecao; i++)
        sef_raiz_liberar(sessao->objetos_inspecao[i]);
    free(sessao->objetos_inspecao);
    sessao->objetos_inspecao = NULL;
    sessao->quantidade_objetos_inspecao = 0;
    sessao->objeto_selecionado = 0;
}

static bool capturar_objetos_inspecao(SefSessaoIde *sessao, SefErro *erro) {
    size_t quantidade = sef_runtime_quantidade_valores(sessao->runtime);
    SefRaiz **objetos = quantidade == 0 ? NULL : calloc(quantidade, sizeof(*objetos));
    if (quantidade > 0 && objetos == NULL) {
        sef_erro_definir(erro, 0, 0, "memoria insuficiente para a prateleira de objetos");
        return false;
    }
    for (size_t i = 0; i < quantidade; i++) {
        objetos[i] = sef_raiz_criar(sessao->runtime, sef_runtime_valor(sessao->runtime, i), erro);
        if (objetos[i] == NULL) {
            for (size_t j = 0; j < i; j++)
                sef_raiz_liberar(objetos[j]);
            free(objetos);
            return false;
        }
    }
    liberar_objetos_inspecao(sessao);
    sessao->objetos_inspecao = objetos;
    sessao->quantidade_objetos_inspecao = quantidade;
    return true;
}

static bool atualizar_inspetor(SefSessaoIde *sessao, SefErro *erro) {
    size_t quantidade = sessao->quantidade_objetos_inspecao;
    if (quantidade == 0)
        return texto_formatar(&sessao->inspetor, erro,
                              "OBJETOS: 0\nSELECIONADO: (AUSENTE)\nEVENTOS DO OUVINTE: %zu",
                              sef_historico_texto_quantidade(sessao->historico_ouvinte));
    if (sessao->objeto_selecionado >= quantidade)
        sessao->objeto_selecionado = 0;
    SefValor selecionado = sef_raiz_valor(sessao->objetos_inspecao[sessao->objeto_selecionado]);
    char *representacao = sef_valor_para_texto(sessao->runtime, selecionado, true, erro);
    if (representacao == NULL)
        return false;
    bool atualizou = texto_formatar(
        &sessao->inspetor, erro,
        "OBJETOS: %zu\nSELECIONADO: %zu/%zu\nTIPO: %s\nVALOR: %s\n\nPRATELEIRA VIVA:", quantidade,
        sessao->objeto_selecionado + 1, quantidade, sef_valor_nome_tipo(selecionado),
        representacao);
    sef_texto_liberar(representacao);
    for (size_t i = 0; atualizou && i < quantidade; i++) {
        char *texto = sef_valor_para_texto(sessao->runtime,
                                           sef_raiz_valor(sessao->objetos_inspecao[i]), true, erro);
        if (texto == NULL)
            return false;
        char prefixo[48];
        int tamanho = snprintf(prefixo, sizeof(prefixo),
                               "\n%c %zu: ", i == sessao->objeto_selecionado ? '>' : ' ', i + 1);
        atualizou = tamanho > 0 && (size_t)tamanho < sizeof(prefixo) &&
                    texto_acrescentar(&sessao->inspetor, prefixo, erro) &&
                    texto_acrescentar(&sessao->inspetor, texto, erro);
        sef_texto_liberar(texto);
    }
    if (atualizou) {
        char rodape[96];
        int tamanho = snprintf(rodape, sizeof(rodape), "\n\nEVENTOS DO OUVINTE: %zu",
                               sef_historico_texto_quantidade(sessao->historico_ouvinte));
        atualizou = tamanho > 0 && (size_t)tamanho < sizeof(rodape) &&
                    texto_acrescentar(&sessao->inspetor, rodape, erro);
    }
    return atualizou;
}

static bool atualizar_navegador(SefSessaoIde *sessao, const SefFormaEstruturalIde *formas,
                                size_t quantidade, size_t selecionada, SefErro *erro) {
    size_t definicoes = 0;
    for (size_t i = 0; i < quantidade; i++)
        if (formas[i].definicao)
            definicoes++;
    if (!texto_formatar(&sessao->navegador, erro,
                        "DEFINICOES: %zu\nFORMAS DE TOPO: %zu\nMUNDO: %s\n",
                        definicoes, quantidade, sessao->caminho_imagem.dados))
        return false;
    if (definicoes == 0)
        return texto_acrescentar(&sessao->navegador, "\n(nenhuma definicao nomeada)", erro);
    for (size_t i = 0; i < quantidade; i++) {
        if (!formas[i].definicao)
            continue;
        char linha[180];
        int tamanho = snprintf(linha, sizeof(linha), "\n%c L%zu  %-10s %s",
                               i == selecionada ? '>' : ' ', formas[i].linha,
                               formas[i].categoria, formas[i].nome);
        if (tamanho <= 0 || (size_t)tamanho >= sizeof(linha) ||
            !texto_acrescentar(&sessao->navegador, linha, erro))
            return false;
    }
    return true;
}

static size_t quantidade_da_assinatura(const SefSessaoIde *sessao, uint64_t assinatura) {
    size_t quantidade = 0;
    for (size_t i = 0; i < sessao->quantidade_formas_executadas; i++)
        if (sessao->formas_executadas[i] == assinatura)
            quantidade++;
    return quantidade;
}

static bool registrar_assinatura_executada(SefSessaoIde *sessao, uint64_t assinatura,
                                           SefErro *erro) {
    if (sessao->quantidade_formas_executadas == sessao->capacidade_formas_executadas) {
        size_t capacidade = sessao->capacidade_formas_executadas == 0
                                ? 32
                                : sessao->capacidade_formas_executadas * 2;
        if (capacidade < sessao->capacidade_formas_executadas ||
            capacidade > SIZE_MAX / sizeof(*sessao->formas_executadas)) {
            sef_erro_definir(erro, 0, 0, "historico incremental da IDE excedeu o limite");
            return false;
        }
        uint64_t *assinaturas =
            realloc(sessao->formas_executadas, capacidade * sizeof(*assinaturas));
        if (assinaturas == NULL) {
            sef_erro_definir(erro, 0, 0, "memoria insuficiente para historico incremental");
            return false;
        }
        sessao->formas_executadas = assinaturas;
        sessao->capacidade_formas_executadas = capacidade;
    }
    sessao->formas_executadas[sessao->quantidade_formas_executadas++] = assinatura;
    return true;
}

static bool registrar_catalogo_executado(SefSessaoIde *sessao, SefErro *erro) {
    SefFormaEstruturalIde *formas = NULL;
    size_t quantidade = 0;
    if (!sef_ide_catalogar_formas(sessao->editor.dados, &formas, &quantidade, erro))
        return false;
    sessao->quantidade_formas_executadas = 0;
    bool registrou = true;
    for (size_t i = 0; registrou && i < quantidade; i++)
        registrou = registrar_assinatura_executada(sessao, formas[i].assinatura, erro);
    if (registrou)
        registrou = atualizar_navegador(sessao, formas, quantidade, SIZE_MAX, erro);
    sef_ide_catalogo_liberar(formas);
    return registrou;
}

static bool imprimir_valores(SefSessaoIde *sessao, SefErro *erro) {
    size_t quantidade = sef_runtime_quantidade_valores(sessao->runtime);
    if (quantidade == 0)
        return texto_acrescentar(&sessao->transcricao, "; nenhum valor\n", erro);
    for (size_t i = 0; i < quantidade; i++) {
        char *texto = sef_valor_para_texto(sessao->runtime, sef_runtime_valor(sessao->runtime, i),
                                           true, erro);
        if (texto == NULL)
            return false;
        bool acrescentou = texto_acrescentar(&sessao->transcricao, texto, erro) &&
                           texto_acrescentar(&sessao->transcricao, "\n", erro);
        sef_texto_liberar(texto);
        if (!acrescentou)
            return false;
    }
    return atualizar_inspetor(sessao, erro);
}

static bool executar_codigo(SefSessaoIde *sessao, const char *codigo, const char *origem,
                            bool mostrar_codigo, SefErro *erro) {
    sef_erro_limpar(erro);
    SefErro sintaxe;
    SefEstadoCodigo estado = sef_runtime_estado_codigo(codigo, &sintaxe);
    if (estado != SEF_CODIGO_COMPLETO) {
        if (estado == SEF_CODIGO_INCOMPLETO)
            sef_erro_definir(&sintaxe, 0, 0, "codigo incompleto");
        registrar_erro(sessao, origem, &sintaxe, erro);
        if (!erro->ocorreu)
            *erro = sintaxe;
        return false;
    }
    if (codigo_vazio(codigo)) {
        texto_definir(&sessao->estado, "Nenhuma forma para executar", erro);
        return !erro->ocorreu;
    }

    char cabecalho[96];
    int tamanho = snprintf(cabecalho, sizeof(cabecalho), "\n[%s]\n", origem);
    if (tamanho <= 0 || (size_t)tamanho >= sizeof(cabecalho) ||
        !texto_acrescentar(&sessao->transcricao, cabecalho, erro) ||
        (mostrar_codigo &&
         (!texto_acrescentar(&sessao->transcricao, codigo, erro) ||
          !texto_acrescentar(&sessao->transcricao, codigo[strlen(codigo) - 1] == '\n' ? "" : "\n",
                             erro))))
        return false;

    SefErro avaliacao;
    SefValor valor = sef_runtime_avaliar_texto(sessao->runtime, codigo, &avaliacao);
    if (valor == NULL) {
        if (!texto_acrescentar(&sessao->transcricao, "ERRO: ", erro) ||
            !texto_acrescentar(&sessao->transcricao, avaliacao.mensagem, erro) ||
            !texto_acrescentar(&sessao->transcricao, "\n", erro))
            return false;
        texto_formatar(&sessao->estado, erro, "%s: %s", origem, avaliacao.mensagem);
        if (!erro->ocorreu)
            *erro = avaliacao;
        return false;
    }
    if (!capturar_objetos_inspecao(sessao, erro) || !imprimir_valores(sessao, erro) ||
        !texto_formatar(&sessao->estado, erro, "%s concluido", origem))
        return false;
    return true;
}

SefSessaoIde *sef_sessao_ide_criar(SefErro *erro) {
    sef_erro_limpar(erro);
    SefSessaoIde *sessao = calloc(1, sizeof(*sessao));
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "memoria insuficiente ao criar sessao da IDE");
        return NULL;
    }
    sessao->runtime = sef_runtime_criar(erro);
    if (sessao->runtime == NULL || !texto_definir(&sessao->editor, "", erro) ||
        !texto_definir(&sessao->ouvinte, "", erro) ||
        !texto_definir(&sessao->transcricao, "", erro) ||
        !texto_definir(&sessao->inspetor, "OBJETOS: 0\nSELECIONADO: (AUSENTE)", erro) ||
        !texto_definir(&sessao->navegador,
                       "DEFINICOES: 0\nFORMAS DE TOPO: 0\n\nF8 navega pelo codigo", erro) ||
        !texto_definir(&sessao->estado, "Novo arquivo", erro) ||
        !texto_definir(&sessao->caminho, "programa.lisp", erro) ||
        !atualizar_caminho_imagem(sessao, erro)) {
        sef_sessao_ide_destruir(sessao);
        return NULL;
    }
    sessao->historico_ouvinte = sef_historico_texto_criar(erro);
    sessao->historico_editor = sef_historico_editor_criar("", 0, erro);
    if (sessao->historico_ouvinte == NULL || sessao->historico_editor == NULL) {
        sef_sessao_ide_destruir(sessao);
        return NULL;
    }
    return sessao;
}

void sef_sessao_ide_destruir(SefSessaoIde *sessao) {
    if (sessao == NULL)
        return;
    liberar_objetos_inspecao(sessao);
    sef_historico_texto_destruir(sessao->historico_ouvinte);
    sef_historico_editor_destruir(sessao->historico_editor);
    sef_runtime_destruir(sessao->runtime);
    texto_liberar(&sessao->editor);
    texto_liberar(&sessao->ouvinte);
    texto_liberar(&sessao->transcricao);
    texto_liberar(&sessao->inspetor);
    texto_liberar(&sessao->navegador);
    texto_liberar(&sessao->estado);
    texto_liberar(&sessao->caminho);
    texto_liberar(&sessao->caminho_imagem);
    free(sessao->formas_executadas);
    free(sessao);
}

const char *sef_sessao_ide_editor(const SefSessaoIde *sessao) { return sessao->editor.dados; }
const char *sef_sessao_ide_ouvinte(const SefSessaoIde *sessao) { return sessao->ouvinte.dados; }
const char *sef_sessao_ide_transcricao(const SefSessaoIde *sessao) {
    return sessao->transcricao.dados;
}
const char *sef_sessao_ide_inspetor(const SefSessaoIde *sessao) { return sessao->inspetor.dados; }
const char *sef_sessao_ide_navegador(const SefSessaoIde *sessao) {
    return sessao->navegador.dados;
}
const char *sef_sessao_ide_estado(const SefSessaoIde *sessao) { return sessao->estado.dados; }
const char *sef_sessao_ide_caminho(const SefSessaoIde *sessao) { return sessao->caminho.dados; }
size_t sef_sessao_ide_cursor_editor(const SefSessaoIde *sessao) {
    return sessao == NULL ? 0 : sessao->cursor_editor;
}

bool sef_sessao_ide_editor_definir(SefSessaoIde *sessao, const char *codigo, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL || codigo == NULL) {
        sef_erro_definir(erro, 0, 0, "sessao ou codigo ausente no editor");
        return false;
    }
    if (!texto_definir(&sessao->editor, codigo, erro))
        return false;
    sessao->cursor_editor = sessao->editor.tamanho;
    return sef_historico_editor_registrar(sessao->historico_editor, sessao->editor.dados,
                                          sessao->cursor_editor, erro);
}

bool sef_sessao_ide_editor_inserir(SefSessaoIde *sessao, const char *texto, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL || texto == NULL) {
        sef_erro_definir(erro, 0, 0, "sessao ou texto ausente no editor");
        return false;
    }
    size_t tamanho = strlen(texto);
    if (!texto_inserir_n(&sessao->editor, sessao->cursor_editor, texto, tamanho, erro))
        return false;
    sessao->cursor_editor += tamanho;
    return sef_historico_editor_registrar(sessao->historico_editor, sessao->editor.dados,
                                          sessao->cursor_editor, erro);
}

void sef_sessao_ide_editor_apagar(SefSessaoIde *sessao) {
    if (sessao == NULL || sessao->cursor_editor == 0)
        return;
    size_t inicio = utf8_anterior(&sessao->editor, sessao->cursor_editor);
    memmove(sessao->editor.dados + inicio, sessao->editor.dados + sessao->cursor_editor,
            sessao->editor.tamanho - sessao->cursor_editor + 1);
    sessao->editor.tamanho -= sessao->cursor_editor - inicio;
    sessao->cursor_editor = inicio;
    SefErro descarte;
    sef_erro_limpar(&descarte);
    sef_historico_editor_registrar(sessao->historico_editor, sessao->editor.dados,
                                   sessao->cursor_editor, &descarte);
}

static size_t inicio_linha(const TextoIde *texto, size_t posicao) {
    while (posicao > 0 && texto->dados[posicao - 1] != '\n')
        posicao--;
    return posicao;
}

static size_t fim_linha(const TextoIde *texto, size_t posicao) {
    while (posicao < texto->tamanho && texto->dados[posicao] != '\n')
        posicao++;
    return posicao;
}

static size_t coluna_utf8(const TextoIde *texto, size_t inicio, size_t posicao) {
    size_t coluna = 0;
    while (inicio < posicao) {
        inicio = utf8_proximo(texto, inicio);
        coluna++;
    }
    return coluna;
}

static size_t posicao_na_coluna(const TextoIde *texto, size_t inicio, size_t fim, size_t coluna) {
    size_t posicao = inicio;
    while (posicao < fim && coluna > 0) {
        posicao = utf8_proximo(texto, posicao);
        coluna--;
    }
    return posicao;
}

void sef_sessao_ide_editor_mover_cursor(SefSessaoIde *sessao, SefMovimentoCursorIde movimento) {
    if (sessao == NULL)
        return;
    TextoIde *editor = &sessao->editor;
    size_t inicio = inicio_linha(editor, sessao->cursor_editor);
    size_t fim = fim_linha(editor, sessao->cursor_editor);
    size_t coluna = coluna_utf8(editor, inicio, sessao->cursor_editor);

    switch (movimento) {
    case SEF_CURSOR_ESQUERDA:
        sessao->cursor_editor = utf8_anterior(editor, sessao->cursor_editor);
        break;
    case SEF_CURSOR_DIREITA:
        sessao->cursor_editor = utf8_proximo(editor, sessao->cursor_editor);
        break;
    case SEF_CURSOR_INICIO_LINHA:
        sessao->cursor_editor = inicio;
        break;
    case SEF_CURSOR_FIM_LINHA:
        sessao->cursor_editor = fim;
        break;
    case SEF_CURSOR_CIMA:
        if (inicio > 0) {
            size_t fim_anterior = inicio - 1;
            size_t inicio_anterior = inicio_linha(editor, fim_anterior);
            sessao->cursor_editor =
                posicao_na_coluna(editor, inicio_anterior, fim_anterior, coluna);
        }
        break;
    case SEF_CURSOR_BAIXO:
        if (fim < editor->tamanho) {
            size_t inicio_seguinte = fim + 1;
            size_t fim_seguinte = fim_linha(editor, inicio_seguinte);
            sessao->cursor_editor =
                posicao_na_coluna(editor, inicio_seguinte, fim_seguinte, coluna);
        }
        break;
    }
}

bool sef_sessao_ide_editor_nova_linha(SefSessaoIde *sessao, SefErro *erro) {
    return sef_sessao_ide_editor_inserir(sessao, "\n", erro);
}

static bool restaurar_editor(SefSessaoIde *sessao, bool desfazer, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "sessao da IDE ausente");
        return false;
    }
    const char *texto = NULL;
    size_t cursor = 0;
    bool encontrou = desfazer
                         ? sef_historico_editor_desfazer(sessao->historico_editor, &texto, &cursor)
                         : sef_historico_editor_refazer(sessao->historico_editor, &texto, &cursor);
    if (!encontrou)
        return texto_definir(&sessao->estado, desfazer ? "Nada para desfazer" : "Nada para refazer",
                             erro);
    if (!texto_definir(&sessao->editor, texto, erro))
        return false;
    sessao->cursor_editor = cursor;
    return texto_definir(&sessao->estado, desfazer ? "Edicao desfeita" : "Edicao refeita", erro);
}

bool sef_sessao_ide_editor_desfazer(SefSessaoIde *sessao, SefErro *erro) {
    return restaurar_editor(sessao, true, erro);
}

bool sef_sessao_ide_editor_refazer(SefSessaoIde *sessao, SefErro *erro) {
    return restaurar_editor(sessao, false, erro);
}

bool sef_sessao_ide_ouvinte_inserir(SefSessaoIde *sessao, const char *texto, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL || texto == NULL) {
        sef_erro_definir(erro, 0, 0, "sessao ou texto ausente no ouvinte");
        return false;
    }
    sef_historico_texto_ir_ao_fim(sessao->historico_ouvinte);
    return texto_acrescentar(&sessao->ouvinte, texto, erro);
}

void sef_sessao_ide_ouvinte_apagar(SefSessaoIde *sessao) {
    if (sessao != NULL) {
        sef_historico_texto_ir_ao_fim(sessao->historico_ouvinte);
        texto_apagar_utf8(&sessao->ouvinte);
    }
}

bool sef_sessao_ide_ouvinte_enviar(SefSessaoIde *sessao, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL)
        return false;
    if (!texto_acrescentar(&sessao->ouvinte, "\n", erro))
        return false;
    SefEstadoCodigo estado = sef_runtime_estado_codigo(sessao->ouvinte.dados, erro);
    if (estado == SEF_CODIGO_INCOMPLETO) {
        return texto_definir(&sessao->estado, "Ouvinte aguardando continuacao", erro);
    }
    size_t tamanho_evento = sessao->ouvinte.tamanho;
    while (tamanho_evento > 0 && (sessao->ouvinte.dados[tamanho_evento - 1] == '\n' ||
                                  sessao->ouvinte.dados[tamanho_evento - 1] == '\r'))
        tamanho_evento--;
    if (!sef_historico_texto_registrar(sessao->historico_ouvinte, sessao->ouvinte.dados,
                                       tamanho_evento, erro))
        return false;
    bool executou = executar_codigo(sessao, sessao->ouvinte.dados, "OUVINTE", true, erro);
    SefErro descarte;
    sef_erro_limpar(&descarte);
    texto_definir(&sessao->ouvinte, "", &descarte);
    return executou && !erro->ocorreu;
}

bool sef_sessao_ide_ouvinte_mover_historico(SefSessaoIde *sessao,
                                            SefMovimentoHistoricoIde movimento, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "sessao da IDE ausente");
        return false;
    }
    const char *texto = movimento == SEF_HISTORICO_ANTERIOR
                            ? sef_historico_texto_anterior(sessao->historico_ouvinte)
                            : sef_historico_texto_proximo(sessao->historico_ouvinte);
    if (texto == NULL)
        return true;
    return texto_definir(&sessao->ouvinte, texto, erro) && atualizar_inspetor(sessao, erro);
}

bool sef_sessao_ide_executar_editor(SefSessaoIde *sessao, SefErro *erro) {
    if (sessao == NULL) {
        sef_erro_limpar(erro);
        sef_erro_definir(erro, 0, 0, "sessao da IDE ausente");
        return false;
    }
    if (!executar_codigo(sessao, sessao->editor.dados, "EDITOR", false, erro))
        return false;
    return registrar_catalogo_executado(sessao, erro);
}

bool sef_sessao_ide_executar_forma_no_cursor(SefSessaoIde *sessao, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "sessao da IDE ausente");
        return false;
    }
    size_t inicio = 0;
    size_t fim = 0;
    if (!sef_ide_forma_no_cursor(sessao->editor.dados, sessao->cursor_editor, &inicio, &fim)) {
        sef_erro_definir(erro, 0, 0, "nenhuma forma Lisp completa no cursor");
        return false;
    }
    char *forma = malloc(fim - inicio + 1);
    if (forma == NULL) {
        sef_erro_definir(erro, 0, 0, "memoria insuficiente ao copiar forma do editor");
        return false;
    }
    memcpy(forma, sessao->editor.dados + inicio, fim - inicio);
    forma[fim - inicio] = '\0';
    bool executou = executar_codigo(sessao, forma, "FORMA NO CURSOR", true, erro);
    free(forma);
    if (executou) {
        SefFormaEstruturalIde *formas = NULL;
        size_t quantidade = 0;
        if (!sef_ide_catalogar_formas(sessao->editor.dados, &formas, &quantidade, erro))
            return false;
        for (size_t i = 0; i < quantidade; i++)
            if (formas[i].inicio == inicio && formas[i].fim == fim &&
                quantidade_da_assinatura(sessao, formas[i].assinatura) == 0)
                executou = registrar_assinatura_executada(sessao, formas[i].assinatura, erro);
        sef_ide_catalogo_liberar(formas);
    }
    return executou;
}

bool sef_sessao_ide_executar_alteracoes(SefSessaoIde *sessao, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "sessao da IDE ausente");
        return false;
    }
    SefFormaEstruturalIde *formas = NULL;
    size_t quantidade = 0;
    if (!sef_ide_catalogar_formas(sessao->editor.dados, &formas, &quantidade, erro))
        return false;
    size_t executadas = 0;
    for (size_t i = 0; i < quantidade; i++) {
        size_t ocorrencia = 1;
        for (size_t anterior = 0; anterior < i; anterior++)
            if (formas[anterior].assinatura == formas[i].assinatura)
                ocorrencia++;
        if (ocorrencia <= quantidade_da_assinatura(sessao, formas[i].assinatura))
            continue;
        size_t tamanho = formas[i].fim - formas[i].inicio;
        char *codigo = malloc(tamanho + 1);
        if (codigo == NULL) {
            sef_erro_definir(erro, 0, 0, "memoria insuficiente ao executar alteracao");
            sef_ide_catalogo_liberar(formas);
            return false;
        }
        memcpy(codigo, sessao->editor.dados + formas[i].inicio, tamanho);
        codigo[tamanho] = '\0';
        bool executou = executar_codigo(sessao, codigo, "ALTERACAO", true, erro);
        free(codigo);
        if (!executou ||
            !registrar_assinatura_executada(sessao, formas[i].assinatura, erro)) {
            sef_ide_catalogo_liberar(formas);
            return false;
        }
        executadas++;
    }
    sessao->quantidade_formas_executadas = 0;
    bool atualizou = true;
    for (size_t i = 0; atualizou && i < quantidade; i++)
        atualizou = registrar_assinatura_executada(sessao, formas[i].assinatura, erro);
    if (atualizou)
        atualizou = atualizar_navegador(sessao, formas, quantidade, SIZE_MAX, erro);
    sef_ide_catalogo_liberar(formas);
    if (!atualizou)
        return false;
    if (executadas == 0)
        return texto_definir(&sessao->estado, "Nenhuma forma alterada", erro);
    return texto_formatar(&sessao->estado, erro, "%zu forma(s) alterada(s) executada(s)",
                          executadas);
}

bool sef_sessao_ide_navegar_definicao(SefSessaoIde *sessao,
                                      SefMovimentoDefinicaoIde movimento, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "sessao da IDE ausente");
        return false;
    }
    SefFormaEstruturalIde *formas = NULL;
    size_t quantidade = 0;
    if (!sef_ide_catalogar_formas(sessao->editor.dados, &formas, &quantidade, erro))
        return false;
    size_t primeira = SIZE_MAX;
    size_t ultima = SIZE_MAX;
    size_t escolhida = SIZE_MAX;
    for (size_t i = 0; i < quantidade; i++) {
        if (!formas[i].definicao)
            continue;
        if (primeira == SIZE_MAX)
            primeira = i;
        ultima = i;
        if (movimento == SEF_DEFINICAO_PROXIMA && escolhida == SIZE_MAX &&
            formas[i].inicio_nome > sessao->cursor_editor)
            escolhida = i;
        if (movimento == SEF_DEFINICAO_ANTERIOR &&
            formas[i].inicio_nome < sessao->cursor_editor)
            escolhida = i;
    }
    if (escolhida == SIZE_MAX)
        escolhida = movimento == SEF_DEFINICAO_PROXIMA ? primeira : ultima;
    if (escolhida == SIZE_MAX) {
        bool atualizou = atualizar_navegador(sessao, formas, quantidade, SIZE_MAX, erro);
        sef_ide_catalogo_liberar(formas);
        if (!atualizou)
            return false;
        return texto_definir(&sessao->estado, "Nenhuma definicao no buffer", erro);
    }
    sessao->cursor_editor = formas[escolhida].inicio_nome;
    bool atualizou = atualizar_navegador(sessao, formas, quantidade, escolhida, erro) &&
                     texto_formatar(&sessao->estado, erro, "Definicao: %s (%s, linha %zu)",
                                    formas[escolhida].nome, formas[escolhida].categoria,
                                    formas[escolhida].linha);
    sef_ide_catalogo_liberar(formas);
    return atualizou;
}

bool sef_sessao_ide_inspetor_mover(SefSessaoIde *sessao, SefMovimentoInspetorIde movimento,
                                   SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL || sessao->quantidade_objetos_inspecao == 0)
        return true;
    if (movimento == SEF_INSPETOR_ANTERIOR) {
        sessao->objeto_selecionado = sessao->objeto_selecionado == 0
                                         ? sessao->quantidade_objetos_inspecao - 1
                                         : sessao->objeto_selecionado - 1;
    } else {
        sessao->objeto_selecionado =
            (sessao->objeto_selecionado + 1) % sessao->quantidade_objetos_inspecao;
    }
    return atualizar_inspetor(sessao, erro);
}

bool sef_sessao_ide_salvar(SefSessaoIde *sessao, const char *caminho, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL || caminho == NULL || caminho[0] == '\0') {
        sef_erro_definir(erro, 0, 0, "caminho ausente ao salvar");
        return false;
    }
    FILE *arquivo = fopen(caminho, "wb");
    if (arquivo == NULL) {
        sef_erro_definir(erro, 0, 0, "nao foi possivel salvar '%s': %s", caminho, strerror(errno));
        return false;
    }
    bool gravou =
        fwrite(sessao->editor.dados, 1, sessao->editor.tamanho, arquivo) == sessao->editor.tamanho;
    if (fclose(arquivo) != 0)
        gravou = false;
    if (!gravou) {
        sef_erro_definir(erro, 0, 0, "falha ao gravar '%s'", caminho);
        return false;
    }
    return texto_definir(&sessao->caminho, caminho, erro) &&
           atualizar_caminho_imagem(sessao, erro) &&
           texto_formatar(&sessao->estado, erro, "Salvo: %s", caminho);
}

bool sef_sessao_ide_abrir(SefSessaoIde *sessao, const char *caminho, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL || caminho == NULL || caminho[0] == '\0') {
        sef_erro_definir(erro, 0, 0, "caminho ausente ao abrir");
        return false;
    }
    FILE *arquivo = fopen(caminho, "rb");
    if (arquivo == NULL) {
        sef_erro_definir(erro, 0, 0, "nao foi possivel abrir '%s': %s", caminho, strerror(errno));
        return false;
    }
    if (fseek(arquivo, 0, SEEK_END) != 0) {
        fclose(arquivo);
        sef_erro_definir(erro, 0, 0, "nao foi possivel medir '%s'", caminho);
        return false;
    }
    long tamanho = ftell(arquivo);
    if (tamanho < 0 || fseek(arquivo, 0, SEEK_SET) != 0) {
        fclose(arquivo);
        sef_erro_definir(erro, 0, 0, "arquivo '%s' nao e pesquisavel", caminho);
        return false;
    }
    if (tamanho > SEF_LIMITE_ARQUIVO_IDE) {
        fclose(arquivo);
        sef_erro_definir(erro, 0, 0, "arquivo '%s' excede o limite de 64 MiB da IDE", caminho);
        return false;
    }
    char *dados = malloc((size_t)tamanho + 1);
    if (dados == NULL) {
        fclose(arquivo);
        sef_erro_definir(erro, 0, 0, "memoria insuficiente ao abrir '%s'", caminho);
        return false;
    }
    size_t lidos = fread(dados, 1, (size_t)tamanho, arquivo);
    fclose(arquivo);
    dados[lidos] = '\0';
    if (lidos != (size_t)tamanho) {
        free(dados);
        sef_erro_definir(erro, 0, 0, "leitura incompleta de '%s'", caminho);
        return false;
    }
    bool abriu = texto_definir_n(&sessao->editor, dados, lidos, erro) &&
                 texto_definir(&sessao->caminho, caminho, erro) &&
                 atualizar_caminho_imagem(sessao, erro) &&
                 texto_formatar(&sessao->estado, erro, "Aberto: %s", caminho);
    if (abriu)
        sessao->cursor_editor = lidos;
    if (abriu) {
        sessao->quantidade_formas_executadas = 0;
        abriu = sef_historico_editor_registrar(sessao->historico_editor, sessao->editor.dados,
                                               sessao->cursor_editor, erro);
    }
    free(dados);
    return abriu;
}

bool sef_sessao_ide_imagem_salvar(SefSessaoIde *sessao, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "sessao da IDE ausente");
        return false;
    }
    if (!sef_runtime_imagem_salvar(sessao->runtime, sessao->caminho_imagem.dados, erro)) {
        SefErro causa = *erro;
        registrar_erro(sessao, "IMAGEM", &causa, erro);
        if (!erro->ocorreu)
            *erro = causa;
        return false;
    }
    return texto_formatar(&sessao->estado, erro, "Mundo salvo: %s",
                          sessao->caminho_imagem.dados);
}

bool sef_sessao_ide_imagem_restaurar(SefSessaoIde *sessao, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "sessao da IDE ausente");
        return false;
    }
    SefRuntime *restaurado = sef_runtime_imagem_abrir(sessao->caminho_imagem.dados, erro);
    if (restaurado == NULL) {
        SefErro causa = *erro;
        registrar_erro(sessao, "RESTAURAR IMAGEM", &causa, erro);
        if (!erro->ocorreu)
            *erro = causa;
        return false;
    }
    liberar_objetos_inspecao(sessao);
    sef_runtime_destruir(sessao->runtime);
    sessao->runtime = restaurado;
    sessao->quantidade_formas_executadas = 0;
    if (!texto_definir(&sessao->inspetor, "OBJETOS: 0\nMUNDO RESTAURADO", erro) ||
        !texto_formatar(&sessao->estado, erro, "Mundo restaurado: %s",
                        sessao->caminho_imagem.dados))
        return false;
    return true;
}
