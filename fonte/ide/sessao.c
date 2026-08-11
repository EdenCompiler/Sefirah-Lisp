#include "ide/ide.h"

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
    TextoIde estado;
    TextoIde caminho;
    size_t cursor_editor;
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

static bool atualizar_inspetor(SefSessaoIde *sessao, SefErro *erro) {
    size_t quantidade = sef_runtime_quantidade_valores(sessao->runtime);
    if (quantidade == 0)
        return texto_definir(&sessao->inspetor, "VALORES: 0\nPRIMARIO: (AUSENTE)", erro);
    char *primario =
        sef_valor_para_texto(sessao->runtime, sef_runtime_valor(sessao->runtime, 0), true, erro);
    if (primario == NULL)
        return false;
    bool atualizou =
        texto_formatar(&sessao->inspetor, erro, "VALORES: %zu\nPRIMARIO: %s", quantidade, primario);
    sef_texto_liberar(primario);
    return atualizou;
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
    if (!imprimir_valores(sessao, erro) ||
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
        !texto_definir(&sessao->inspetor, "VALORES: 0\nPRIMARIO: (AUSENTE)", erro) ||
        !texto_definir(&sessao->estado, "Novo arquivo", erro) ||
        !texto_definir(&sessao->caminho, "programa.lisp", erro)) {
        sef_sessao_ide_destruir(sessao);
        return NULL;
    }
    return sessao;
}

void sef_sessao_ide_destruir(SefSessaoIde *sessao) {
    if (sessao == NULL)
        return;
    sef_runtime_destruir(sessao->runtime);
    texto_liberar(&sessao->editor);
    texto_liberar(&sessao->ouvinte);
    texto_liberar(&sessao->transcricao);
    texto_liberar(&sessao->inspetor);
    texto_liberar(&sessao->estado);
    texto_liberar(&sessao->caminho);
    free(sessao);
}

const char *sef_sessao_ide_editor(const SefSessaoIde *sessao) { return sessao->editor.dados; }
const char *sef_sessao_ide_ouvinte(const SefSessaoIde *sessao) { return sessao->ouvinte.dados; }
const char *sef_sessao_ide_transcricao(const SefSessaoIde *sessao) {
    return sessao->transcricao.dados;
}
const char *sef_sessao_ide_inspetor(const SefSessaoIde *sessao) { return sessao->inspetor.dados; }
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
    return true;
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
    return true;
}

void sef_sessao_ide_editor_apagar(SefSessaoIde *sessao) {
    if (sessao == NULL || sessao->cursor_editor == 0)
        return;
    size_t inicio = utf8_anterior(&sessao->editor, sessao->cursor_editor);
    memmove(sessao->editor.dados + inicio, sessao->editor.dados + sessao->cursor_editor,
            sessao->editor.tamanho - sessao->cursor_editor + 1);
    sessao->editor.tamanho -= sessao->cursor_editor - inicio;
    sessao->cursor_editor = inicio;
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

bool sef_sessao_ide_ouvinte_inserir(SefSessaoIde *sessao, const char *texto, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL || texto == NULL) {
        sef_erro_definir(erro, 0, 0, "sessao ou texto ausente no ouvinte");
        return false;
    }
    return texto_acrescentar(&sessao->ouvinte, texto, erro);
}

void sef_sessao_ide_ouvinte_apagar(SefSessaoIde *sessao) {
    if (sessao != NULL)
        texto_apagar_utf8(&sessao->ouvinte);
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
    bool executou = executar_codigo(sessao, sessao->ouvinte.dados, "OUVINTE", true, erro);
    SefErro descarte;
    sef_erro_limpar(&descarte);
    texto_definir(&sessao->ouvinte, "", &descarte);
    return executou && !erro->ocorreu;
}

bool sef_sessao_ide_executar_editor(SefSessaoIde *sessao, SefErro *erro) {
    if (sessao == NULL) {
        sef_erro_limpar(erro);
        sef_erro_definir(erro, 0, 0, "sessao da IDE ausente");
        return false;
    }
    return executar_codigo(sessao, sessao->editor.dados, "EDITOR", false, erro);
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
                 texto_formatar(&sessao->estado, erro, "Aberto: %s", caminho);
    if (abriu)
        sessao->cursor_editor = lidos;
    free(dados);
    return abriu;
}
