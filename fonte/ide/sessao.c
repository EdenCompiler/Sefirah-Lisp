#include "ide/ide.h"

#include "apoio.h"
#include "espaco_trabalho.h"

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SEF_LIMITE_ARQUIVO_IDE (64L * 1024L * 1024L)
#define SEF_LIMITE_CONDICOES_IDE 32u
#define SEF_LIMITE_SIMBOLOS_ESPACO_TRABALHO 4096u

typedef struct TextoIde {
    char *dados;
    size_t tamanho;
    size_t capacidade;
} TextoIde;

typedef struct DocumentoIde {
    TextoIde editor;
    TextoIde caminho;
    TextoIde caminho_imagem;
    size_t cursor_editor;
    size_t ancora_selecao_editor;
    bool selecao_editor_ativa;
    bool modificado;
    SefHistoricoEditorIde *historico_editor;
    uint64_t *formas_executadas;
    size_t quantidade_formas_executadas;
    size_t capacidade_formas_executadas;
} DocumentoIde;

typedef struct PassoInspecaoIde {
    SefRaiz *raiz;
    char rotulo[64];
} PassoInspecaoIde;

typedef struct DiagnosticoIde {
    char origem[64];
    char mensagem[512];
    size_t linha;
    size_t coluna;
    SefRaiz *condicao;
} DiagnosticoIde;

typedef struct ResultadoSimboloEspacoTrabalhoIde {
    size_t indice_arquivo;
    size_t linha;
    size_t inicio_nome;
    char categoria[20];
    char nome[96];
    char *rotulo;
} ResultadoSimboloEspacoTrabalhoIde;

struct SefSessaoIde {
    SefRuntime *runtime;
    TextoIde editor;
    TextoIde ouvinte;
    TextoIde transcricao;
    TextoIde inspetor;
    TextoIde navegador;
    TextoIde depurador;
    TextoIde estado;
    TextoIde caminho;
    TextoIde caminho_imagem;
    TextoIde abas;
    TextoIde explorador;
    size_t cursor_editor;
    size_t ancora_selecao_editor;
    bool selecao_editor_ativa;
    SefHistoricoTextoIde *historico_ouvinte;
    SefHistoricoEditorIde *historico_editor;
    SefRaiz **objetos_inspecao;
    size_t quantidade_objetos_inspecao;
    size_t objeto_selecionado;
    PassoInspecaoIde *caminho_inspecao;
    size_t profundidade_inspecao;
    size_t capacidade_caminho_inspecao;
    size_t componente_inspecao;
    uint64_t *formas_executadas;
    size_t quantidade_formas_executadas;
    size_t capacidade_formas_executadas;
    bool documento_modificado;
    DocumentoIde *documentos;
    size_t quantidade_documentos;
    size_t capacidade_documentos;
    size_t documento_ativo;
    SefEspacoTrabalhoIde *espaco_trabalho;
    size_t arquivo_espaco_trabalho_selecionado;
    ResultadoSimboloEspacoTrabalhoIde *simbolos_espaco_trabalho;
    size_t quantidade_simbolos_espaco_trabalho;
    size_t capacidade_simbolos_espaco_trabalho;
    DiagnosticoIde diagnosticos[SEF_LIMITE_CONDICOES_IDE];
    size_t quantidade_diagnosticos;
    size_t diagnostico_selecionado;
};

static void selecao_editor_limpar(SefSessaoIde *sessao);

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
        sef_erro_definir(erro, 0, 0, "not enough memory in IDE session");
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

static bool texto_substituir_n(TextoIde *texto, size_t inicio, size_t fim, const char *dados,
                               size_t tamanho, SefErro *erro) {
    if (inicio > fim || fim > texto->tamanho) {
        sef_erro_definir(erro, 0, 0, "invalid range in IDE editor");
        return false;
    }
    size_t removido = fim - inicio;
    if (tamanho > SIZE_MAX - (texto->tamanho - removido) - 1 ||
        !texto_reservar(texto, texto->tamanho - removido + tamanho + 1, erro))
        return false;
    memmove(texto->dados + inicio + tamanho, texto->dados + fim, texto->tamanho - fim + 1);
    if (tamanho > 0)
        memcpy(texto->dados + inicio, dados, tamanho);
    texto->tamanho = texto->tamanho - removido + tamanho;
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

static void documento_liberar(DocumentoIde *documento) {
    texto_liberar(&documento->editor);
    texto_liberar(&documento->caminho);
    texto_liberar(&documento->caminho_imagem);
    sef_historico_editor_destruir(documento->historico_editor);
    free(documento->formas_executadas);
    memset(documento, 0, sizeof(*documento));
}

static void simbolos_espaco_trabalho_limpar(SefSessaoIde *sessao) {
    for (size_t i = 0; i < sessao->quantidade_simbolos_espaco_trabalho; i++)
        free(sessao->simbolos_espaco_trabalho[i].rotulo);
    sessao->quantidade_simbolos_espaco_trabalho = 0;
}

static const char *nome_base(const char *caminho) {
    const char *nome = caminho;
    for (const char *cursor = caminho; *cursor != '\0'; cursor++) {
        if (*cursor == '/' || *cursor == '\\')
            nome = cursor + 1;
    }
    return nome;
}

static const char *documento_caminho(const SefSessaoIde *sessao, size_t indice) {
    if (indice >= sessao->quantidade_documentos)
        return NULL;
    return indice == sessao->documento_ativo ? sessao->caminho.dados
                                             : sessao->documentos[indice].caminho.dados;
}

static bool documento_modificado(const SefSessaoIde *sessao, size_t indice) {
    return indice == sessao->documento_ativo ? sessao->documento_modificado
                                             : sessao->documentos[indice].modificado;
}

static bool atualizar_abas(SefSessaoIde *sessao, SefErro *erro) {
    if (!texto_definir(&sessao->abas, "", erro))
        return false;
    for (size_t i = 0; i < sessao->quantidade_documentos; i++) {
        const char *caminho = documento_caminho(sessao, i);
        char aba[320];
        int tamanho = snprintf(aba, sizeof(aba), "%s%s[%zu] %s%s", i == 0 ? "" : "  ",
                               i == sessao->documento_ativo ? ">" : "", i + 1,
                               nome_base(caminho == NULL ? "untitled.lisp" : caminho),
                               documento_modificado(sessao, i) ? " *" : "");
        if (tamanho < 0 || (size_t)tamanho >= sizeof(aba) ||
            !texto_acrescentar_n(&sessao->abas, aba, (size_t)tamanho, erro))
            return false;
    }
    return true;
}

static bool atualizar_explorador(SefSessaoIde *sessao, SefErro *erro) {
    const char *raiz = sef_espaco_trabalho_ide_raiz(sessao->espaco_trabalho);
    size_t quantidade = sef_espaco_trabalho_ide_quantidade(sessao->espaco_trabalho);
    if (raiz[0] == '\0')
        return texto_definir(&sessao->explorador,
                             "NO FOLDER OPEN\n\nPass a directory to sefirah_ide.", erro);
    if (!texto_formatar(&sessao->explorador, erro, "ROOT: %s\nLISP FILES: %zu\n", nome_base(raiz),
                        quantidade))
        return false;
    if (quantidade == 0)
        return texto_acrescentar(&sessao->explorador, "\n(no .lisp files)", erro);
    for (size_t i = 0; i < quantidade; i++) {
        const char *arquivo = sef_espaco_trabalho_ide_arquivo_relativo(sessao->espaco_trabalho, i);
        char prefixo[8];
        int tamanho = snprintf(prefixo, sizeof(prefixo), "\n%c ",
                               i == sessao->arquivo_espaco_trabalho_selecionado ? '>' : ' ');
        if (tamanho <= 0 || (size_t)tamanho >= sizeof(prefixo) ||
            !texto_acrescentar_n(&sessao->explorador, prefixo, (size_t)tamanho, erro) ||
            !texto_acrescentar(&sessao->explorador, arquivo, erro))
            return false;
    }
    return true;
}

static bool reservar_documentos(SefSessaoIde *sessao, size_t quantidade, SefErro *erro) {
    if (quantidade <= sessao->capacidade_documentos)
        return true;
    size_t capacidade = sessao->capacidade_documentos == 0 ? 4 : sessao->capacidade_documentos * 2;
    while (capacidade < quantidade)
        capacidade *= 2;
    DocumentoIde *documentos = realloc(sessao->documentos, capacidade * sizeof(*documentos));
    if (documentos == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for editor tabs");
        return false;
    }
    memset(documentos + sessao->capacidade_documentos, 0,
           (capacidade - sessao->capacidade_documentos) * sizeof(*documentos));
    sessao->documentos = documentos;
    sessao->capacidade_documentos = capacidade;
    return true;
}

static void guardar_documento_ativo(SefSessaoIde *sessao) {
    DocumentoIde *documento = &sessao->documentos[sessao->documento_ativo];
    documento->editor = sessao->editor;
    documento->caminho = sessao->caminho;
    documento->caminho_imagem = sessao->caminho_imagem;
    documento->cursor_editor = sessao->cursor_editor;
    documento->ancora_selecao_editor = sessao->ancora_selecao_editor;
    documento->selecao_editor_ativa = sessao->selecao_editor_ativa;
    documento->modificado = sessao->documento_modificado;
    documento->historico_editor = sessao->historico_editor;
    documento->formas_executadas = sessao->formas_executadas;
    documento->quantidade_formas_executadas = sessao->quantidade_formas_executadas;
    documento->capacidade_formas_executadas = sessao->capacidade_formas_executadas;
    memset(&sessao->editor, 0, sizeof(sessao->editor));
    memset(&sessao->caminho, 0, sizeof(sessao->caminho));
    memset(&sessao->caminho_imagem, 0, sizeof(sessao->caminho_imagem));
    sessao->historico_editor = NULL;
    sessao->formas_executadas = NULL;
    sessao->quantidade_formas_executadas = 0;
    sessao->capacidade_formas_executadas = 0;
}

static void carregar_documento(SefSessaoIde *sessao, size_t indice) {
    DocumentoIde *documento = &sessao->documentos[indice];
    sessao->editor = documento->editor;
    sessao->caminho = documento->caminho;
    sessao->caminho_imagem = documento->caminho_imagem;
    sessao->cursor_editor = documento->cursor_editor;
    sessao->ancora_selecao_editor = documento->ancora_selecao_editor;
    sessao->selecao_editor_ativa = documento->selecao_editor_ativa;
    sessao->documento_modificado = documento->modificado;
    sessao->historico_editor = documento->historico_editor;
    sessao->formas_executadas = documento->formas_executadas;
    sessao->quantidade_formas_executadas = documento->quantidade_formas_executadas;
    sessao->capacidade_formas_executadas = documento->capacidade_formas_executadas;
    memset(documento, 0, sizeof(*documento));
    sessao->documento_ativo = indice;
}

static bool definir_caminho_imagem(TextoIde *imagem, const TextoIde *fonte, SefErro *erro) {
    const char *caminho = fonte->dados;
    size_t tamanho = fonte->tamanho;
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
    return texto_definir_n(imagem, caminho, extensao, erro) &&
           texto_acrescentar(imagem, ".imagem", erro);
}

static bool atualizar_caminho_imagem(SefSessaoIde *sessao, SefErro *erro) {
    return definir_caminho_imagem(&sessao->caminho_imagem, &sessao->caminho, erro);
}

static bool codigo_vazio(const char *codigo) {
    while (*codigo != '\0') {
        if (*codigo != ' ' && *codigo != '\t' && *codigo != '\r' && *codigo != '\n')
            return false;
        codigo++;
    }
    return true;
}

static bool atualizar_depurador(SefSessaoIde *sessao, SefErro *erro) {
    if (sessao->quantidade_diagnosticos == 0)
        return texto_definir(
            &sessao->depurador,
            "CONDITIONS: 0\n\nNo unhandled conditions.\nShift+F9/F10 navigates history.", erro);
    if (sessao->diagnostico_selecionado >= sessao->quantidade_diagnosticos)
        sessao->diagnostico_selecionado = sessao->quantidade_diagnosticos - 1;
    DiagnosticoIde *diagnostico = &sessao->diagnosticos[sessao->diagnostico_selecionado];
    char tipo[96] = "NO CONDITION OBJECT";
    if (diagnostico->condicao != NULL) {
        SefValor classe = NULL;
        char rotulo[32];
        if (sef_valor_componente(sessao->runtime, sef_raiz_valor(diagnostico->condicao), 0, &classe,
                                 rotulo, sizeof(rotulo))) {
            char *nome_classe = sef_valor_para_texto(sessao->runtime, classe, true, erro);
            if (nome_classe == NULL)
                return false;
            snprintf(tipo, sizeof(tipo), "%s", nome_classe);
            sef_texto_liberar(nome_classe);
        } else {
            snprintf(tipo, sizeof(tipo), "%s",
                     sef_valor_nome_tipo(sef_raiz_valor(diagnostico->condicao)));
        }
    }
    if (!texto_formatar(&sessao->depurador, erro,
                        "CONDITIONS: %zu\nSELECTED: %zu/%zu\nSOURCE: %s\nTYPE: %s\n"
                        "POSITION: %zu:%zu\nMESSAGE: %s",
                        sessao->quantidade_diagnosticos, sessao->diagnostico_selecionado + 1,
                        sessao->quantidade_diagnosticos, diagnostico->origem, tipo,
                        diagnostico->linha, diagnostico->coluna, diagnostico->mensagem))
        return false;
    if (diagnostico->condicao != NULL)
        return texto_acrescentar(
            &sessao->depurador,
            "\n\nENTER inspects the condition.\nActive restarts ended with evaluation.", erro);
    return texto_acrescentar(&sessao->depurador,
                             "\n\nThis diagnostic has no inspectable Lisp object.", erro);
}

static bool registrar_diagnostico(SefSessaoIde *sessao, const char *origem, const SefErro *causa,
                                  SefValor condicao, SefErro *erro) {
    if (sessao->quantidade_diagnosticos == SEF_LIMITE_CONDICOES_IDE) {
        sef_raiz_liberar(sessao->diagnosticos[0].condicao);
        memmove(sessao->diagnosticos, sessao->diagnosticos + 1,
                (SEF_LIMITE_CONDICOES_IDE - 1) * sizeof(sessao->diagnosticos[0]));
        sessao->quantidade_diagnosticos--;
    }
    DiagnosticoIde diagnostico = {0};
    snprintf(diagnostico.origem, sizeof(diagnostico.origem), "%s", origem);
    snprintf(diagnostico.mensagem, sizeof(diagnostico.mensagem), "%s", causa->mensagem);
    diagnostico.linha = causa->linha;
    diagnostico.coluna = causa->coluna;
    if (condicao != NULL) {
        SefErro erro_raiz;
        diagnostico.condicao = sef_raiz_criar(sessao->runtime, condicao, &erro_raiz);
        if (diagnostico.condicao == NULL) {
            sef_erro_definir(erro, 0, 0, "%s", erro_raiz.mensagem);
            return false;
        }
    }
    sessao->diagnosticos[sessao->quantidade_diagnosticos++] = diagnostico;
    sessao->diagnostico_selecionado = sessao->quantidade_diagnosticos - 1;
    return atualizar_depurador(sessao, erro);
}

static void liberar_diagnosticos(SefSessaoIde *sessao) {
    for (size_t i = 0; i < sessao->quantidade_diagnosticos; i++)
        sef_raiz_liberar(sessao->diagnosticos[i].condicao);
    memset(sessao->diagnosticos, 0, sizeof(sessao->diagnosticos));
    sessao->quantidade_diagnosticos = 0;
    sessao->diagnostico_selecionado = 0;
}

static bool registrar_erro(SefSessaoIde *sessao, const char *origem, const SefErro *causa,
                           SefErro *erro) {
    if (!texto_formatar(&sessao->estado, erro, "%s: %s", origem, causa->mensagem))
        return false;
    char cabecalho[96];
    int tamanho = snprintf(cabecalho, sizeof(cabecalho), "\n[%s]\nERROR: ", origem);
    return tamanho > 0 && (size_t)tamanho < sizeof(cabecalho) &&
           texto_acrescentar(&sessao->transcricao, cabecalho, erro) &&
           texto_acrescentar(&sessao->transcricao, causa->mensagem, erro) &&
           texto_acrescentar(&sessao->transcricao, "\n", erro) &&
           registrar_diagnostico(sessao, origem, causa, NULL, erro);
}

static void liberar_caminho_inspecao(SefSessaoIde *sessao) {
    for (size_t i = 0; i < sessao->profundidade_inspecao; i++)
        sef_raiz_liberar(sessao->caminho_inspecao[i].raiz);
    free(sessao->caminho_inspecao);
    sessao->caminho_inspecao = NULL;
    sessao->profundidade_inspecao = 0;
    sessao->capacidade_caminho_inspecao = 0;
    sessao->componente_inspecao = 0;
}

static void liberar_objetos_inspecao(SefSessaoIde *sessao) {
    liberar_caminho_inspecao(sessao);
    for (size_t i = 0; i < sessao->quantidade_objetos_inspecao; i++)
        sef_raiz_liberar(sessao->objetos_inspecao[i]);
    free(sessao->objetos_inspecao);
    sessao->objetos_inspecao = NULL;
    sessao->quantidade_objetos_inspecao = 0;
    sessao->objeto_selecionado = 0;
}

static SefValor objeto_inspecionado(const SefSessaoIde *sessao) {
    if (sessao->profundidade_inspecao > 0)
        return sef_raiz_valor(sessao->caminho_inspecao[sessao->profundidade_inspecao - 1].raiz);
    if (sessao->quantidade_objetos_inspecao == 0)
        return NULL;
    return sef_raiz_valor(sessao->objetos_inspecao[sessao->objeto_selecionado]);
}

static bool capturar_objetos_inspecao(SefSessaoIde *sessao, SefErro *erro) {
    size_t quantidade = sef_runtime_quantidade_valores(sessao->runtime);
    SefRaiz **objetos = quantidade == 0 ? NULL : calloc(quantidade, sizeof(*objetos));
    if (quantidade > 0 && objetos == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for object shelf");
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
                              "OBJECTS: 0\nSELECTED: (NONE)\nREPL EVENTS: %zu",
                              sef_historico_texto_quantidade(sessao->historico_ouvinte));
    if (sessao->objeto_selecionado >= quantidade)
        sessao->objeto_selecionado = 0;
    SefValor selecionado = objeto_inspecionado(sessao);
    char *representacao = sef_valor_para_texto(sessao->runtime, selecionado, true, erro);
    if (representacao == NULL)
        return false;
    size_t quantidade_componentes = sef_valor_quantidade_componentes(sessao->runtime, selecionado);
    if (quantidade_componentes == 0)
        sessao->componente_inspecao = 0;
    else if (sessao->componente_inspecao >= quantidade_componentes)
        sessao->componente_inspecao = quantidade_componentes - 1;
    bool atualizou = texto_formatar(
        &sessao->inspetor, erro,
        "OBJECTS: %zu\nROOT: %zu/%zu\nDEPTH: %zu\nTYPE: %s\nVALUE: %s\nCOMPONENTS: %zu", quantidade,
        sessao->objeto_selecionado + 1, quantidade, sessao->profundidade_inspecao,
        sef_valor_nome_tipo(selecionado), representacao, quantidade_componentes);
    sef_texto_liberar(representacao);
    if (atualizou && sessao->profundidade_inspecao > 0) {
        atualizou = texto_acrescentar(&sessao->inspetor, "\nPATH: ROOT", erro);
        for (size_t i = 0; atualizou && i < sessao->profundidade_inspecao; i++) {
            atualizou =
                texto_acrescentar(&sessao->inspetor, " > ", erro) &&
                texto_acrescentar(&sessao->inspetor, sessao->caminho_inspecao[i].rotulo, erro);
        }
    }
    for (size_t i = 0; atualizou && i < quantidade_componentes; i++) {
        SefValor componente = NULL;
        char rotulo[64];
        if (!sef_valor_componente(sessao->runtime, selecionado, i, &componente, rotulo,
                                  sizeof(rotulo))) {
            sef_erro_definir(erro, 0, 0, "invalid component during inspection");
            return false;
        }
        char *texto = sef_valor_para_texto(sessao->runtime, componente, true, erro);
        if (texto == NULL)
            return false;
        char prefixo[96];
        int tamanho = snprintf(prefixo, sizeof(prefixo),
                               "\n%c %s: ", i == sessao->componente_inspecao ? '>' : ' ', rotulo);
        atualizou = tamanho > 0 && (size_t)tamanho < sizeof(prefixo) &&
                    texto_acrescentar(&sessao->inspetor, prefixo, erro) &&
                    texto_acrescentar(&sessao->inspetor, texto, erro);
        sef_texto_liberar(texto);
    }
    if (atualizou)
        atualizou = texto_acrescentar(&sessao->inspetor, "\n\nLIVE SHELF:", erro);
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
        int tamanho = snprintf(rodape, sizeof(rodape), "\n\nREPL EVENTS: %zu",
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
                        "DEFINITIONS: %zu\nTOP-LEVEL FORMS: %zu\nWORLD: %s\n", definicoes,
                        quantidade, sessao->caminho_imagem.dados))
        return false;
    if (definicoes == 0)
        return texto_acrescentar(&sessao->navegador, "\n(no named definitions)", erro);
    for (size_t i = 0; i < quantidade; i++) {
        if (!formas[i].definicao)
            continue;
        char linha[180];
        int tamanho =
            snprintf(linha, sizeof(linha), "\n%c L%zu  %-10s %s", i == selecionada ? '>' : ' ',
                     formas[i].linha, formas[i].categoria, formas[i].nome);
        if (tamanho <= 0 || (size_t)tamanho >= sizeof(linha) ||
            !texto_acrescentar(&sessao->navegador, linha, erro))
            return false;
    }
    return true;
}

static bool atualizar_navegador_atual(SefSessaoIde *sessao, SefErro *erro) {
    SefFormaEstruturalIde *formas = NULL;
    size_t quantidade = 0;
    if (!sef_ide_catalogar_formas(sessao->editor.dados, &formas, &quantidade, erro))
        return false;
    bool atualizou = atualizar_navegador(sessao, formas, quantidade, SIZE_MAX, erro);
    sef_ide_catalogo_liberar(formas);
    return atualizou;
}

static bool atualizar_navegador_referencias(SefSessaoIde *sessao,
                                            const SefFormaEstruturalIde *formas,
                                            size_t quantidade_formas,
                                            const SefReferenciaEstruturalIde *referencias,
                                            size_t quantidade_referencias, size_t selecionada,
                                            size_t nome_inicio, size_t nome_fim, SefErro *erro) {
    size_t tamanho_nome = nome_fim - nome_inicio;
    if (!texto_formatar(&sessao->navegador, erro, "REFERENCES: %zu\nSYMBOL: %.*s\nWORLD: %s\n",
                        quantidade_referencias, (int)tamanho_nome,
                        sessao->editor.dados + nome_inicio, sessao->caminho_imagem.dados))
        return false;
    if (quantidade_referencias == 0)
        return texto_acrescentar(&sessao->navegador, "\n(no references in buffer)", erro);
    for (size_t i = 0; i < quantidade_referencias; i++) {
        const SefReferenciaEstruturalIde *referencia = &referencias[i];
        const char *categoria = "TOP LEVEL";
        const char *nome = "anonymous form";
        if (referencia->indice_forma < quantidade_formas &&
            formas[referencia->indice_forma].definicao) {
            categoria = formas[referencia->indice_forma].categoria;
            nome = formas[referencia->indice_forma].nome;
        }
        char linha[220];
        int tamanho = snprintf(linha, sizeof(linha), "\n%c L%zu  %-10s %s",
                               i == selecionada ? '>' : ' ', referencia->linha, categoria, nome);
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
            sef_erro_definir(erro, 0, 0, "incremental IDE history exceeded the limit");
            return false;
        }
        uint64_t *assinaturas =
            realloc(sessao->formas_executadas, capacidade * sizeof(*assinaturas));
        if (assinaturas == NULL) {
            sef_erro_definir(erro, 0, 0, "not enough memory for incremental history");
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
        return texto_acrescentar(&sessao->transcricao, "; no values\n", erro);
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
            sef_erro_definir(&sintaxe, 0, 0, "incomplete source code");
        registrar_erro(sessao, origem, &sintaxe, erro);
        if (!erro->ocorreu)
            *erro = sintaxe;
        return false;
    }
    if (codigo_vazio(codigo)) {
        texto_definir(&sessao->estado, "No forms to run", erro);
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
        if (!texto_acrescentar(&sessao->transcricao, "ERROR: ", erro) ||
            !texto_acrescentar(&sessao->transcricao, avaliacao.mensagem, erro) ||
            !texto_acrescentar(&sessao->transcricao, "\n", erro) ||
            !registrar_diagnostico(sessao, origem, &avaliacao,
                                   sef_runtime_ultima_condicao(sessao->runtime), erro))
            return false;
        texto_formatar(&sessao->estado, erro, "%s: %s", origem, avaliacao.mensagem);
        if (!erro->ocorreu)
            *erro = avaliacao;
        return false;
    }
    if (!capturar_objetos_inspecao(sessao, erro) || !imprimir_valores(sessao, erro) ||
        !texto_formatar(&sessao->estado, erro, "%s completed", origem))
        return false;
    return true;
}

SefSessaoIde *sef_sessao_ide_criar(SefErro *erro) {
    sef_erro_limpar(erro);
    SefSessaoIde *sessao = calloc(1, sizeof(*sessao));
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory to create IDE session");
        return NULL;
    }
    sessao->runtime = sef_runtime_criar(erro);
    if (sessao->runtime == NULL) {
        sef_sessao_ide_destruir(sessao);
        return NULL;
    }
    sessao->espaco_trabalho = sef_espaco_trabalho_ide_criar(erro);
    if (sessao->espaco_trabalho == NULL) {
        sef_sessao_ide_destruir(sessao);
        return NULL;
    }
    if (!texto_definir(&sessao->editor, "", erro) || !texto_definir(&sessao->ouvinte, "", erro) ||
        !texto_definir(&sessao->transcricao, "", erro) ||
        !texto_definir(&sessao->inspetor, "OBJECTS: 0\nSELECTED: (NONE)", erro) ||
        !texto_definir(&sessao->navegador,
                       "DEFINITIONS: 0\nTOP-LEVEL FORMS: 0\n\nF8 browses source", erro) ||
        !texto_definir(&sessao->depurador,
                       "CONDITIONS: 0\n\nNo unhandled conditions.\n"
                       "Shift+F9/F10 navigates history.",
                       erro) ||
        !texto_definir(&sessao->estado, "New file", erro) ||
        !texto_definir(&sessao->caminho, "untitled.lisp", erro) ||
        !texto_definir(&sessao->abas, "", erro) || !texto_definir(&sessao->explorador, "", erro) ||
        !atualizar_caminho_imagem(sessao, erro)) {
        sef_sessao_ide_destruir(sessao);
        return NULL;
    }
    sessao->historico_ouvinte = sef_historico_texto_criar(erro);
    sessao->historico_editor = sef_historico_editor_criar("", 0, erro);
    if (sessao->historico_ouvinte == NULL || sessao->historico_editor == NULL ||
        !reservar_documentos(sessao, 1, erro)) {
        sef_sessao_ide_destruir(sessao);
        return NULL;
    }
    sessao->quantidade_documentos = 1;
    if (!atualizar_abas(sessao, erro) || !atualizar_explorador(sessao, erro)) {
        sef_sessao_ide_destruir(sessao);
        return NULL;
    }
    return sessao;
}

void sef_sessao_ide_destruir(SefSessaoIde *sessao) {
    if (sessao == NULL)
        return;
    liberar_objetos_inspecao(sessao);
    liberar_diagnosticos(sessao);
    sef_historico_texto_destruir(sessao->historico_ouvinte);
    sef_historico_editor_destruir(sessao->historico_editor);
    sef_espaco_trabalho_ide_destruir(sessao->espaco_trabalho);
    sef_runtime_destruir(sessao->runtime);
    texto_liberar(&sessao->editor);
    texto_liberar(&sessao->ouvinte);
    texto_liberar(&sessao->transcricao);
    texto_liberar(&sessao->inspetor);
    texto_liberar(&sessao->navegador);
    texto_liberar(&sessao->depurador);
    texto_liberar(&sessao->estado);
    texto_liberar(&sessao->caminho);
    texto_liberar(&sessao->caminho_imagem);
    texto_liberar(&sessao->abas);
    texto_liberar(&sessao->explorador);
    simbolos_espaco_trabalho_limpar(sessao);
    free(sessao->simbolos_espaco_trabalho);
    for (size_t i = 0; i < sessao->quantidade_documentos; i++)
        documento_liberar(&sessao->documentos[i]);
    free(sessao->documentos);
    free(sessao->formas_executadas);
    free(sessao);
}

const char *sef_sessao_ide_editor(const SefSessaoIde *sessao) { return sessao->editor.dados; }
const char *sef_sessao_ide_ouvinte(const SefSessaoIde *sessao) { return sessao->ouvinte.dados; }
const char *sef_sessao_ide_transcricao(const SefSessaoIde *sessao) {
    return sessao->transcricao.dados;
}
const char *sef_sessao_ide_inspetor(const SefSessaoIde *sessao) { return sessao->inspetor.dados; }
const char *sef_sessao_ide_navegador(const SefSessaoIde *sessao) { return sessao->navegador.dados; }
const char *sef_sessao_ide_depurador(const SefSessaoIde *sessao) { return sessao->depurador.dados; }
const char *sef_sessao_ide_estado(const SefSessaoIde *sessao) { return sessao->estado.dados; }
const char *sef_sessao_ide_caminho(const SefSessaoIde *sessao) { return sessao->caminho.dados; }
const char *sef_sessao_ide_abas(const SefSessaoIde *sessao) {
    return sessao == NULL ? "" : sessao->abas.dados;
}
const char *sef_sessao_ide_explorador(const SefSessaoIde *sessao) {
    return sessao == NULL ? "" : sessao->explorador.dados;
}
const char *sef_sessao_ide_espaco_trabalho_raiz(const SefSessaoIde *sessao) {
    return sessao == NULL ? "" : sef_espaco_trabalho_ide_raiz(sessao->espaco_trabalho);
}
size_t sef_sessao_ide_espaco_trabalho_quantidade(const SefSessaoIde *sessao) {
    return sessao == NULL ? 0 : sef_espaco_trabalho_ide_quantidade(sessao->espaco_trabalho);
}
size_t sef_sessao_ide_espaco_trabalho_selecionado(const SefSessaoIde *sessao) {
    return sessao == NULL ? 0 : sessao->arquivo_espaco_trabalho_selecionado;
}
const char *sef_sessao_ide_espaco_trabalho_arquivo(const SefSessaoIde *sessao, size_t indice) {
    return sessao == NULL
               ? NULL
               : sef_espaco_trabalho_ide_arquivo_relativo(sessao->espaco_trabalho, indice);
}
size_t sef_sessao_ide_simbolos_espaco_trabalho_quantidade(const SefSessaoIde *sessao) {
    return sessao == NULL ? 0 : sessao->quantidade_simbolos_espaco_trabalho;
}
const char *sef_sessao_ide_simbolo_espaco_trabalho(const SefSessaoIde *sessao, size_t indice) {
    return sessao == NULL || indice >= sessao->quantidade_simbolos_espaco_trabalho
               ? NULL
               : sessao->simbolos_espaco_trabalho[indice].rotulo;
}
size_t sef_sessao_ide_quantidade_documentos(const SefSessaoIde *sessao) {
    return sessao == NULL ? 0 : sessao->quantidade_documentos;
}
size_t sef_sessao_ide_documento_ativo(const SefSessaoIde *sessao) {
    return sessao == NULL ? 0 : sessao->documento_ativo;
}
const char *sef_sessao_ide_documento_caminho(const SefSessaoIde *sessao, size_t indice) {
    return sessao == NULL ? NULL : documento_caminho(sessao, indice);
}
bool sef_sessao_ide_documento_modificado(const SefSessaoIde *sessao, size_t indice) {
    return sessao != NULL && indice < sessao->quantidade_documentos &&
           documento_modificado(sessao, indice);
}

bool sef_sessao_ide_documento_ativar(SefSessaoIde *sessao, size_t indice, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL || indice >= sessao->quantidade_documentos) {
        sef_erro_definir(erro, 0, 0, "invalid editor tab");
        return false;
    }
    if (indice == sessao->documento_ativo)
        return true;
    guardar_documento_ativo(sessao);
    carregar_documento(sessao, indice);
    if (!atualizar_abas(sessao, erro) || !atualizar_navegador_atual(sessao, erro))
        return false;
    return texto_formatar(&sessao->estado, erro, "Active tab: %s",
                          nome_base(sessao->caminho.dados));
}
size_t sef_sessao_ide_cursor_editor(const SefSessaoIde *sessao) {
    return sessao == NULL ? 0 : sessao->cursor_editor;
}

bool sef_sessao_ide_selecao_editor(const SefSessaoIde *sessao, size_t *inicio, size_t *fim) {
    if (sessao == NULL || inicio == NULL || fim == NULL || !sessao->selecao_editor_ativa ||
        sessao->ancora_selecao_editor == sessao->cursor_editor)
        return false;
    *inicio = sessao->ancora_selecao_editor < sessao->cursor_editor ? sessao->ancora_selecao_editor
                                                                    : sessao->cursor_editor;
    *fim = sessao->ancora_selecao_editor > sessao->cursor_editor ? sessao->ancora_selecao_editor
                                                                 : sessao->cursor_editor;
    return true;
}

bool sef_sessao_ide_espaco_trabalho_abrir(SefSessaoIde *sessao, const char *caminho,
                                          SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session while opening workspace");
        return false;
    }
    if (!sef_espaco_trabalho_ide_abrir(sessao->espaco_trabalho, caminho, erro))
        return false;
    simbolos_espaco_trabalho_limpar(sessao);
    sessao->arquivo_espaco_trabalho_selecionado = 0;
    return atualizar_explorador(sessao, erro) &&
           texto_formatar(&sessao->estado, erro, "Workspace opened: %s (%zu Lisp file(s))", caminho,
                          sef_espaco_trabalho_ide_quantidade(sessao->espaco_trabalho));
}

bool sef_sessao_ide_espaco_trabalho_atualizar(SefSessaoIde *sessao, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL || sef_espaco_trabalho_ide_raiz(sessao->espaco_trabalho)[0] == '\0') {
        sef_erro_definir(erro, 0, 0, "no workspace is open to refresh");
        return false;
    }
    TextoIde raiz = {0};
    TextoIde selecionado = {0};
    const char *arquivo_atual = sef_espaco_trabalho_ide_arquivo_relativo(
        sessao->espaco_trabalho, sessao->arquivo_espaco_trabalho_selecionado);
    bool preparou =
        texto_definir(&raiz, sef_espaco_trabalho_ide_raiz(sessao->espaco_trabalho), erro) &&
        texto_definir(&selecionado, arquivo_atual == NULL ? "" : arquivo_atual, erro);
    if (!preparou) {
        texto_liberar(&raiz);
        texto_liberar(&selecionado);
        return false;
    }
    bool atualizou = sef_espaco_trabalho_ide_abrir(sessao->espaco_trabalho, raiz.dados, erro);
    if (atualizou) {
        simbolos_espaco_trabalho_limpar(sessao);
        sessao->arquivo_espaco_trabalho_selecionado = 0;
        for (size_t i = 0; i < sef_espaco_trabalho_ide_quantidade(sessao->espaco_trabalho); i++) {
            if (strcmp(sef_espaco_trabalho_ide_arquivo_relativo(sessao->espaco_trabalho, i),
                       selecionado.dados) == 0) {
                sessao->arquivo_espaco_trabalho_selecionado = i;
                break;
            }
        }
        atualizou = atualizar_explorador(sessao, erro) &&
                    texto_formatar(&sessao->estado, erro, "Explorer refreshed: %zu Lisp file(s)",
                                   sef_espaco_trabalho_ide_quantidade(sessao->espaco_trabalho));
    }
    texto_liberar(&raiz);
    texto_liberar(&selecionado);
    return atualizou;
}

bool sef_sessao_ide_espaco_trabalho_mover(SefSessaoIde *sessao, SefMovimentoArquivoIde movimento,
                                          SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session while navigating workspace");
        return false;
    }
    size_t quantidade = sef_espaco_trabalho_ide_quantidade(sessao->espaco_trabalho);
    if (quantidade == 0)
        return texto_definir(&sessao->estado, "Workspace has no Lisp files", erro);
    if (movimento == SEF_ARQUIVO_ANTERIOR)
        sessao->arquivo_espaco_trabalho_selecionado =
            sessao->arquivo_espaco_trabalho_selecionado == 0
                ? quantidade - 1
                : sessao->arquivo_espaco_trabalho_selecionado - 1;
    else
        sessao->arquivo_espaco_trabalho_selecionado =
            (sessao->arquivo_espaco_trabalho_selecionado + 1) % quantidade;
    return atualizar_explorador(sessao, erro) &&
           texto_formatar(
               &sessao->estado, erro, "Selected: %s",
               sef_espaco_trabalho_ide_arquivo_relativo(
                   sessao->espaco_trabalho, sessao->arquivo_espaco_trabalho_selecionado));
}

bool sef_sessao_ide_espaco_trabalho_selecionar(SefSessaoIde *sessao, size_t indice, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL || indice >= sef_espaco_trabalho_ide_quantidade(sessao->espaco_trabalho)) {
        sef_erro_definir(erro, 0, 0, "invalid workspace file selection");
        return false;
    }
    sessao->arquivo_espaco_trabalho_selecionado = indice;
    return atualizar_explorador(sessao, erro) &&
           texto_formatar(
               &sessao->estado, erro, "Selected: %s",
               sef_espaco_trabalho_ide_arquivo_relativo(sessao->espaco_trabalho, indice));
}

bool sef_sessao_ide_espaco_trabalho_abrir_selecionado(SefSessaoIde *sessao, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session while opening workspace file");
        return false;
    }
    const char *caminho = sef_espaco_trabalho_ide_arquivo_absoluto(
        sessao->espaco_trabalho, sessao->arquivo_espaco_trabalho_selecionado);
    if (caminho == NULL) {
        sef_erro_definir(erro, 0, 0, "workspace has no selected Lisp file");
        return false;
    }
    return sef_sessao_ide_abrir(sessao, caminho, erro);
}

static bool texto_contem_sem_diferenciar_caixa(const char *texto, const char *consulta) {
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

static const char *fonte_aberta_no_editor(const SefSessaoIde *sessao, const char *caminho) {
    for (size_t i = 0; i < sessao->quantidade_documentos; i++) {
        const char *caminho_documento = documento_caminho(sessao, i);
        if (caminho_documento == NULL || strcmp(caminho_documento, caminho) != 0)
            continue;
        return i == sessao->documento_ativo ? sessao->editor.dados
                                             : sessao->documentos[i].editor.dados;
    }
    return NULL;
}

static char *arquivo_ler_para_indice(const char *caminho, SefErro *erro) {
    FILE *arquivo = fopen(caminho, "rb");
    if (arquivo == NULL) {
        sef_erro_definir(erro, 0, 0, "could not index '%s': %s", caminho, strerror(errno));
        return NULL;
    }
    if (fseek(arquivo, 0, SEEK_END) != 0) {
        fclose(arquivo);
        sef_erro_definir(erro, 0, 0, "could not determine the size of '%s'", caminho);
        return NULL;
    }
    long tamanho = ftell(arquivo);
    if (tamanho < 0 || fseek(arquivo, 0, SEEK_SET) != 0 || tamanho > SEF_LIMITE_ARQUIVO_IDE) {
        fclose(arquivo);
        sef_erro_definir(erro, 0, 0, "could not index '%s' within the IDE file limit", caminho);
        return NULL;
    }
    char *dados = malloc((size_t)tamanho + 1);
    if (dados == NULL) {
        fclose(arquivo);
        sef_erro_definir(erro, 0, 0, "not enough memory to index '%s'", caminho);
        return NULL;
    }
    size_t lidos = fread(dados, 1, (size_t)tamanho, arquivo);
    fclose(arquivo);
    if (lidos != (size_t)tamanho) {
        free(dados);
        sef_erro_definir(erro, 0, 0, "incomplete read while indexing '%s'", caminho);
        return NULL;
    }
    dados[lidos] = '\0';
    return dados;
}

static bool erro_e_falta_memoria(const SefErro *erro) {
    return erro->ocorreu && strstr(erro->mensagem, "not enough memory") != NULL;
}

static bool simbolos_espaco_trabalho_reservar(SefSessaoIde *sessao, size_t quantidade,
                                              SefErro *erro) {
    if (quantidade <= sessao->capacidade_simbolos_espaco_trabalho)
        return true;
    size_t capacidade = sessao->capacidade_simbolos_espaco_trabalho == 0
                            ? 64
                            : sessao->capacidade_simbolos_espaco_trabalho * 2;
    while (capacidade < quantidade)
        capacidade *= 2;
    ResultadoSimboloEspacoTrabalhoIde *resultados =
        realloc(sessao->simbolos_espaco_trabalho, capacidade * sizeof(*resultados));
    if (resultados == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for workspace symbol results");
        return false;
    }
    sessao->simbolos_espaco_trabalho = resultados;
    sessao->capacidade_simbolos_espaco_trabalho = capacidade;
    return true;
}

static bool simbolo_espaco_trabalho_adicionar(SefSessaoIde *sessao, size_t indice_arquivo,
                                              const char *arquivo_relativo,
                                              const SefFormaEstruturalIde *forma,
                                              SefErro *erro) {
    if (sessao->quantidade_simbolos_espaco_trabalho >=
        SEF_LIMITE_SIMBOLOS_ESPACO_TRABALHO)
        return true;
    if (!simbolos_espaco_trabalho_reservar(
            sessao, sessao->quantidade_simbolos_espaco_trabalho + 1, erro))
        return false;
    SefEstadoVinculosSimbolo estado_runtime = {0};
    SefErro erro_runtime;
    bool consultou = sef_runtime_consultar_vinculos_simbolo(sessao->runtime, forma->nome,
                                                            &estado_runtime, &erro_runtime);
    const char *situacao = "RUNTIME UNKNOWN";
    if (consultou && !estado_runtime.encontrado)
        situacao = "SOURCE ONLY";
    else if (consultou && estado_runtime.possui_valor && estado_runtime.possui_funcao)
        situacao = "LIVE VALUE/FUNCTION";
    else if (consultou && estado_runtime.possui_funcao)
        situacao = "LIVE FUNCTION";
    else if (consultou && estado_runtime.possui_valor)
        situacao = "LIVE VALUE";
    else if (consultou)
        situacao = "INTERNED";
    int tamanho = snprintf(NULL, 0, "%s:%zu  %-10s %s  [%s]", arquivo_relativo, forma->linha,
                           forma->categoria, forma->nome, situacao);
    if (tamanho < 0) {
        sef_erro_definir(erro, 0, 0, "could not format a workspace symbol result");
        return false;
    }
    char *rotulo = malloc((size_t)tamanho + 1);
    if (rotulo == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory for workspace symbol results");
        return false;
    }
    snprintf(rotulo, (size_t)tamanho + 1, "%s:%zu  %-10s %s  [%s]", arquivo_relativo,
             forma->linha, forma->categoria, forma->nome, situacao);
    ResultadoSimboloEspacoTrabalhoIde *resultado =
        &sessao->simbolos_espaco_trabalho[sessao->quantidade_simbolos_espaco_trabalho++];
    memset(resultado, 0, sizeof(*resultado));
    resultado->indice_arquivo = indice_arquivo;
    resultado->linha = forma->linha;
    resultado->inicio_nome = forma->inicio_nome;
    snprintf(resultado->categoria, sizeof(resultado->categoria), "%s", forma->categoria);
    snprintf(resultado->nome, sizeof(resultado->nome), "%s", forma->nome);
    resultado->rotulo = rotulo;
    return true;
}

static bool navegador_mostrar_simbolos_espaco_trabalho(SefSessaoIde *sessao,
                                                       const char *consulta, size_t ignorados,
                                                       bool truncado, SefErro *erro) {
    if (!texto_formatar(&sessao->navegador, erro,
                        "WORKSPACE SYMBOLS: %zu\nQUERY: %s\nSKIPPED FILES: %zu\n",
                        sessao->quantidade_simbolos_espaco_trabalho,
                        consulta[0] == '\0' ? "(all definitions)" : consulta, ignorados))
        return false;
    for (size_t i = 0; i < sessao->quantidade_simbolos_espaco_trabalho; i++) {
        if (!texto_acrescentar(&sessao->navegador, "\n  ", erro) ||
            !texto_acrescentar(&sessao->navegador,
                               sessao->simbolos_espaco_trabalho[i].rotulo, erro))
            return false;
    }
    return !truncado ||
           texto_acrescentar(&sessao->navegador,
                             "\n\n(result limit reached; narrow the query)", erro);
}

bool sef_sessao_ide_simbolos_espaco_trabalho_buscar(SefSessaoIde *sessao, const char *consulta,
                                                    SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL || consulta == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session or workspace symbol query");
        return false;
    }
    size_t quantidade_arquivos =
        sessao == NULL ? 0 : sef_espaco_trabalho_ide_quantidade(sessao->espaco_trabalho);
    if (sef_espaco_trabalho_ide_raiz(sessao->espaco_trabalho)[0] == '\0') {
        sef_erro_definir(erro, 0, 0, "no workspace is open for symbol search");
        return false;
    }
    simbolos_espaco_trabalho_limpar(sessao);
    size_t ignorados = 0;
    size_t total_encontrado = 0;
    for (size_t i = 0; i < quantidade_arquivos; i++) {
        const char *absoluto =
            sef_espaco_trabalho_ide_arquivo_absoluto(sessao->espaco_trabalho, i);
        const char *relativo =
            sef_espaco_trabalho_ide_arquivo_relativo(sessao->espaco_trabalho, i);
        const char *codigo = fonte_aberta_no_editor(sessao, absoluto);
        char *codigo_alocado = NULL;
        if (codigo == NULL) {
            codigo_alocado = arquivo_ler_para_indice(absoluto, erro);
            codigo = codigo_alocado;
        }
        SefFormaEstruturalIde *formas = NULL;
        size_t quantidade_formas = 0;
        if (codigo == NULL ||
            !sef_ide_catalogar_formas(codigo, &formas, &quantidade_formas, erro)) {
            free(codigo_alocado);
            if (erro_e_falta_memoria(erro)) {
                simbolos_espaco_trabalho_limpar(sessao);
                return false;
            }
            ignorados++;
            sef_erro_limpar(erro);
            continue;
        }
        bool adicionou = true;
        for (size_t j = 0; adicionou && j < quantidade_formas; j++) {
            if (!formas[j].definicao ||
                !texto_contem_sem_diferenciar_caixa(formas[j].nome, consulta))
                continue;
            total_encontrado++;
            adicionou = simbolo_espaco_trabalho_adicionar(sessao, i, relativo, &formas[j], erro);
        }
        sef_ide_catalogo_liberar(formas);
        free(codigo_alocado);
        if (!adicionou) {
            simbolos_espaco_trabalho_limpar(sessao);
            return false;
        }
    }
    bool truncado = total_encontrado > sessao->quantidade_simbolos_espaco_trabalho;
    if (!navegador_mostrar_simbolos_espaco_trabalho(sessao, consulta, ignorados, truncado, erro))
        return false;
    return texto_formatar(&sessao->estado, erro, "Workspace symbols: %zu match(es)%s",
                          total_encontrado, truncado ? " (showing first 4096)" : "");
}

bool sef_sessao_ide_simbolo_espaco_trabalho_abrir(SefSessaoIde *sessao, size_t indice,
                                                  SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL || indice >= sessao->quantidade_simbolos_espaco_trabalho) {
        sef_erro_definir(erro, 0, 0, "invalid workspace symbol selection");
        return false;
    }
    ResultadoSimboloEspacoTrabalhoIde resultado = sessao->simbolos_espaco_trabalho[indice];
    const char *caminho = sef_espaco_trabalho_ide_arquivo_absoluto(
        sessao->espaco_trabalho, resultado.indice_arquivo);
    const char *relativo = sef_espaco_trabalho_ide_arquivo_relativo(
        sessao->espaco_trabalho, resultado.indice_arquivo);
    if (caminho == NULL || relativo == NULL) {
        sef_erro_definir(erro, 0, 0, "workspace symbol file is no longer available");
        return false;
    }
    bool ja_ativo = strcmp(sessao->caminho.dados, caminho) == 0;
    if (!sef_sessao_ide_espaco_trabalho_selecionar(sessao, resultado.indice_arquivo, erro) ||
        (!ja_ativo && !sef_sessao_ide_abrir(sessao, caminho, erro)))
        return false;
    if (resultado.inicio_nome > sessao->editor.tamanho) {
        sef_erro_definir(erro, 0, 0, "workspace symbol location is stale; search again");
        return false;
    }
    sessao->cursor_editor = resultado.inicio_nome;
    selecao_editor_limpar(sessao);
    SefFormaEstruturalIde *formas = NULL;
    size_t quantidade_formas = 0;
    if (!sef_ide_catalogar_formas(sessao->editor.dados, &formas, &quantidade_formas, erro))
        return false;
    size_t selecionada = SIZE_MAX;
    for (size_t i = 0; i < quantidade_formas; i++)
        if (formas[i].definicao && formas[i].inicio_nome == resultado.inicio_nome &&
            strcmp(formas[i].nome, resultado.nome) == 0) {
            selecionada = i;
            break;
        }
    if (selecionada == SIZE_MAX) {
        sef_ide_catalogo_liberar(formas);
        sef_erro_definir(erro, 0, 0, "workspace symbol location is stale; search again");
        return false;
    }
    bool abriu = atualizar_navegador(sessao, formas, quantidade_formas, selecionada, erro) &&
                 texto_formatar(&sessao->estado, erro,
                                "Workspace definition: %s (%s, %s:%zu)", resultado.nome,
                                resultado.categoria, relativo, resultado.linha);
    sef_ide_catalogo_liberar(formas);
    return abriu;
}

bool sef_sessao_ide_arquivo_criar(SefSessaoIde *sessao, const char *caminho, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session while creating file");
        return false;
    }
    if (!sef_ide_arquivo_criar(caminho, erro))
        return false;
    SefErro descarte;
    sef_erro_limpar(&descarte);
    sef_sessao_ide_espaco_trabalho_atualizar(sessao, &descarte);
    return sef_sessao_ide_abrir(sessao, caminho, erro);
}

bool sef_sessao_ide_diretorio_criar(SefSessaoIde *sessao, const char *caminho, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session while creating folder");
        return false;
    }
    if (!sef_ide_diretorio_criar(caminho, erro))
        return false;
    SefErro descarte;
    sef_erro_limpar(&descarte);
    sef_sessao_ide_espaco_trabalho_atualizar(sessao, &descarte);
    return texto_formatar(&sessao->estado, erro, "Folder created: %s", caminho);
}

static void selecao_editor_limpar(SefSessaoIde *sessao) {
    sessao->selecao_editor_ativa = false;
    sessao->ancora_selecao_editor = sessao->cursor_editor;
}

bool sef_sessao_ide_editor_definir(SefSessaoIde *sessao, const char *codigo, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL || codigo == NULL) {
        sef_erro_definir(erro, 0, 0, "missing session or source code in editor");
        return false;
    }
    if (!texto_definir(&sessao->editor, codigo, erro))
        return false;
    sessao->cursor_editor = sessao->editor.tamanho;
    selecao_editor_limpar(sessao);
    sessao->documento_modificado = true;
    return sef_historico_editor_registrar(sessao->historico_editor, sessao->editor.dados,
                                          sessao->cursor_editor, erro) &&
           atualizar_abas(sessao, erro);
}

bool sef_sessao_ide_editor_inserir(SefSessaoIde *sessao, const char *texto, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL || texto == NULL) {
        sef_erro_definir(erro, 0, 0, "missing session or text in editor");
        return false;
    }
    size_t tamanho = strlen(texto);
    size_t inicio = sessao->cursor_editor;
    size_t fim = sessao->cursor_editor;
    sef_sessao_ide_selecao_editor(sessao, &inicio, &fim);
    if (!texto_substituir_n(&sessao->editor, inicio, fim, texto, tamanho, erro))
        return false;
    sessao->cursor_editor = inicio + tamanho;
    selecao_editor_limpar(sessao);
    sessao->documento_modificado = true;
    return sef_historico_editor_registrar(sessao->historico_editor, sessao->editor.dados,
                                          sessao->cursor_editor, erro) &&
           atualizar_abas(sessao, erro);
}

void sef_sessao_ide_editor_apagar(SefSessaoIde *sessao) {
    if (sessao == NULL)
        return;
    size_t inicio = 0;
    size_t fim = 0;
    if (!sef_sessao_ide_selecao_editor(sessao, &inicio, &fim)) {
        if (sessao->cursor_editor == 0)
            return;
        inicio = utf8_anterior(&sessao->editor, sessao->cursor_editor);
        fim = sessao->cursor_editor;
    }
    memmove(sessao->editor.dados + inicio, sessao->editor.dados + fim,
            sessao->editor.tamanho - fim + 1);
    sessao->editor.tamanho -= fim - inicio;
    sessao->cursor_editor = inicio;
    selecao_editor_limpar(sessao);
    SefErro descarte;
    sef_erro_limpar(&descarte);
    sef_historico_editor_registrar(sessao->historico_editor, sessao->editor.dados,
                                   sessao->cursor_editor, &descarte);
    sessao->documento_modificado = true;
    atualizar_abas(sessao, &descarte);
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

static void mover_cursor_editor(SefSessaoIde *sessao, SefMovimentoCursorIde movimento,
                                bool selecionar) {
    if (sessao == NULL)
        return;
    size_t cursor_anterior = sessao->cursor_editor;
    if (selecionar && !sessao->selecao_editor_ativa)
        sessao->ancora_selecao_editor = cursor_anterior;
    else if (!selecionar)
        selecao_editor_limpar(sessao);
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
    if (selecionar)
        sessao->selecao_editor_ativa = sessao->ancora_selecao_editor != sessao->cursor_editor;
    if (!sessao->selecao_editor_ativa)
        sessao->ancora_selecao_editor = sessao->cursor_editor;
}

void sef_sessao_ide_editor_mover_cursor(SefSessaoIde *sessao, SefMovimentoCursorIde movimento) {
    mover_cursor_editor(sessao, movimento, false);
}

void sef_sessao_ide_editor_mover_cursor_selecionando(SefSessaoIde *sessao,
                                                     SefMovimentoCursorIde movimento) {
    mover_cursor_editor(sessao, movimento, true);
}

bool sef_sessao_ide_editor_selecionar_forma(SefSessaoIde *sessao, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session");
        return false;
    }
    size_t inicio = 0;
    size_t fim = 0;
    if (!sef_ide_forma_no_cursor(sessao->editor.dados, sessao->cursor_editor, &inicio, &fim)) {
        sef_erro_definir(erro, 0, 0, "no complete Lisp form at cursor");
        return false;
    }
    sessao->ancora_selecao_editor = inicio;
    sessao->cursor_editor = fim;
    sessao->selecao_editor_ativa = inicio != fim;
    return texto_formatar(&sessao->estado, erro, "Selected form: bytes %zu..%zu", inicio, fim);
}

bool sef_sessao_ide_editor_nova_linha(SefSessaoIde *sessao, SefErro *erro) {
    return sef_sessao_ide_editor_inserir(sessao, "\n", erro);
}

static bool restaurar_editor(SefSessaoIde *sessao, bool desfazer, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session");
        return false;
    }
    const char *texto = NULL;
    size_t cursor = 0;
    bool encontrou = desfazer
                         ? sef_historico_editor_desfazer(sessao->historico_editor, &texto, &cursor)
                         : sef_historico_editor_refazer(sessao->historico_editor, &texto, &cursor);
    if (!encontrou)
        return texto_definir(&sessao->estado, desfazer ? "Nothing to undo" : "Nothing to redo",
                             erro);
    if (!texto_definir(&sessao->editor, texto, erro))
        return false;
    sessao->cursor_editor = cursor;
    selecao_editor_limpar(sessao);
    sessao->documento_modificado = true;
    return atualizar_abas(sessao, erro) &&
           texto_definir(&sessao->estado, desfazer ? "Edit undone" : "Edit redone", erro);
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
        sef_erro_definir(erro, 0, 0, "missing session or text in REPL");
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
        return texto_definir(&sessao->estado, "REPL waiting for continuation", erro);
    }
    size_t tamanho_evento = sessao->ouvinte.tamanho;
    while (tamanho_evento > 0 && (sessao->ouvinte.dados[tamanho_evento - 1] == '\n' ||
                                  sessao->ouvinte.dados[tamanho_evento - 1] == '\r'))
        tamanho_evento--;
    if (!sef_historico_texto_registrar(sessao->historico_ouvinte, sessao->ouvinte.dados,
                                       tamanho_evento, erro))
        return false;
    bool executou = executar_codigo(sessao, sessao->ouvinte.dados, "REPL", true, erro);
    SefErro descarte;
    sef_erro_limpar(&descarte);
    texto_definir(&sessao->ouvinte, "", &descarte);
    return executou && !erro->ocorreu;
}

bool sef_sessao_ide_ouvinte_mover_historico(SefSessaoIde *sessao,
                                            SefMovimentoHistoricoIde movimento, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session");
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
        sef_erro_definir(erro, 0, 0, "missing IDE session");
        return false;
    }
    if (!executar_codigo(sessao, sessao->editor.dados, "EDITOR", false, erro))
        return false;
    return registrar_catalogo_executado(sessao, erro);
}

bool sef_sessao_ide_executar_forma_no_cursor(SefSessaoIde *sessao, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session");
        return false;
    }
    size_t inicio = 0;
    size_t fim = 0;
    if (!sef_ide_forma_no_cursor(sessao->editor.dados, sessao->cursor_editor, &inicio, &fim)) {
        sef_erro_definir(erro, 0, 0, "no complete Lisp form at cursor");
        return false;
    }
    char *forma = malloc(fim - inicio + 1);
    if (forma == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory to copy editor form");
        return false;
    }
    memcpy(forma, sessao->editor.dados + inicio, fim - inicio);
    forma[fim - inicio] = '\0';
    bool executou = executar_codigo(sessao, forma, "FORM AT CURSOR", true, erro);
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
        sef_erro_definir(erro, 0, 0, "missing IDE session");
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
            sef_erro_definir(erro, 0, 0, "not enough memory to run change");
            sef_ide_catalogo_liberar(formas);
            return false;
        }
        memcpy(codigo, sessao->editor.dados + formas[i].inicio, tamanho);
        codigo[tamanho] = '\0';
        bool executou = executar_codigo(sessao, codigo, "CHANGE", true, erro);
        free(codigo);
        if (!executou || !registrar_assinatura_executada(sessao, formas[i].assinatura, erro)) {
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
        return texto_definir(&sessao->estado, "No changed forms", erro);
    return texto_formatar(&sessao->estado, erro, "%zu changed form(s) executed", executadas);
}

bool sef_sessao_ide_navegar_definicao(SefSessaoIde *sessao, SefMovimentoDefinicaoIde movimento,
                                      SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session");
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
        if (movimento == SEF_DEFINICAO_ANTERIOR && formas[i].inicio_nome < sessao->cursor_editor)
            escolhida = i;
    }
    if (escolhida == SIZE_MAX)
        escolhida = movimento == SEF_DEFINICAO_PROXIMA ? primeira : ultima;
    if (escolhida == SIZE_MAX) {
        bool atualizou = atualizar_navegador(sessao, formas, quantidade, SIZE_MAX, erro);
        sef_ide_catalogo_liberar(formas);
        if (!atualizou)
            return false;
        return texto_definir(&sessao->estado, "No definitions in buffer", erro);
    }
    sessao->cursor_editor = formas[escolhida].inicio_nome;
    selecao_editor_limpar(sessao);
    bool atualizou = atualizar_navegador(sessao, formas, quantidade, escolhida, erro) &&
                     texto_formatar(&sessao->estado, erro, "Definition: %s (%s, line %zu)",
                                    formas[escolhida].nome, formas[escolhida].categoria,
                                    formas[escolhida].linha);
    sef_ide_catalogo_liberar(formas);
    return atualizou;
}

bool sef_sessao_ide_ir_para_definicao(SefSessaoIde *sessao, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session");
        return false;
    }
    size_t nome_inicio = 0;
    size_t nome_fim = 0;
    if (!sef_ide_atomo_no_cursor(sessao->editor.dados, sessao->cursor_editor, &nome_inicio,
                                 &nome_fim))
        return texto_definir(&sessao->estado, "No symbol at cursor", erro);
    SefFormaEstruturalIde *formas = NULL;
    size_t quantidade = 0;
    if (!sef_ide_catalogar_formas(sessao->editor.dados, &formas, &quantidade, erro))
        return false;
    size_t escolhida = SIZE_MAX;
    for (size_t i = 0; i < quantidade; i++) {
        if (formas[i].definicao &&
            sef_ide_atomos_iguais(sessao->editor.dados, nome_inicio, nome_fim,
                                  formas[i].inicio_nome, formas[i].fim_nome)) {
            escolhida = i;
            break;
        }
    }
    if (escolhida == SIZE_MAX) {
        bool atualizou = atualizar_navegador(sessao, formas, quantidade, SIZE_MAX, erro);
        sef_ide_catalogo_liberar(formas);
        if (!atualizou)
            return false;
        return texto_formatar(&sessao->estado, erro, "Definition not found: %.*s",
                              (int)(nome_fim - nome_inicio), sessao->editor.dados + nome_inicio);
    }
    sessao->cursor_editor = formas[escolhida].inicio_nome;
    selecao_editor_limpar(sessao);
    bool atualizou = atualizar_navegador(sessao, formas, quantidade, escolhida, erro) &&
                     texto_formatar(&sessao->estado, erro, "Definition found: %s (%s, line %zu)",
                                    formas[escolhida].nome, formas[escolhida].categoria,
                                    formas[escolhida].linha);
    sef_ide_catalogo_liberar(formas);
    return atualizou;
}

bool sef_sessao_ide_navegar_referencia(SefSessaoIde *sessao, SefMovimentoReferenciaIde movimento,
                                       SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session");
        return false;
    }
    size_t nome_inicio = 0;
    size_t nome_fim = 0;
    if (!sef_ide_atomo_no_cursor(sessao->editor.dados, sessao->cursor_editor, &nome_inicio,
                                 &nome_fim))
        return texto_definir(&sessao->estado, "No symbol at cursor", erro);
    SefFormaEstruturalIde *formas = NULL;
    size_t quantidade_formas = 0;
    if (!sef_ide_catalogar_formas(sessao->editor.dados, &formas, &quantidade_formas, erro))
        return false;
    SefReferenciaEstruturalIde *referencias = NULL;
    size_t quantidade_referencias = 0;
    if (!sef_ide_catalogar_referencias(sessao->editor.dados, nome_inicio, nome_fim, formas,
                                       quantidade_formas, &referencias, &quantidade_referencias,
                                       erro)) {
        sef_ide_catalogo_liberar(formas);
        return false;
    }
    size_t escolhida = SIZE_MAX;
    for (size_t i = 0; i < quantidade_referencias; i++) {
        if (movimento == SEF_REFERENCIA_PROXIMA && escolhida == SIZE_MAX &&
            referencias[i].inicio > sessao->cursor_editor)
            escolhida = i;
        if (movimento == SEF_REFERENCIA_ANTERIOR && referencias[i].inicio < sessao->cursor_editor)
            escolhida = i;
    }
    if (escolhida == SIZE_MAX && quantidade_referencias > 0)
        escolhida = movimento == SEF_REFERENCIA_PROXIMA ? 0 : quantidade_referencias - 1;
    bool atualizou = atualizar_navegador_referencias(sessao, formas, quantidade_formas, referencias,
                                                     quantidade_referencias, escolhida, nome_inicio,
                                                     nome_fim, erro);
    if (atualizou && escolhida == SIZE_MAX)
        atualizou =
            texto_formatar(&sessao->estado, erro, "No references for %.*s",
                           (int)(nome_fim - nome_inicio), sessao->editor.dados + nome_inicio);
    if (atualizou && escolhida != SIZE_MAX) {
        sessao->cursor_editor = referencias[escolhida].inicio;
        selecao_editor_limpar(sessao);
        atualizou =
            texto_formatar(&sessao->estado, erro, "Reference %zu/%zu: %.*s (line %zu)",
                           escolhida + 1, quantidade_referencias, (int)(nome_fim - nome_inicio),
                           sessao->editor.dados + nome_inicio, referencias[escolhida].linha);
    }
    sef_ide_referencias_liberar(referencias);
    sef_ide_catalogo_liberar(formas);
    return atualizou;
}

bool sef_sessao_ide_navegar_condicao(SefSessaoIde *sessao, SefMovimentoCondicaoIde movimento,
                                     SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session");
        return false;
    }
    if (sessao->quantidade_diagnosticos == 0)
        return texto_definir(&sessao->estado, "No conditions in history", erro);
    if (movimento == SEF_CONDICAO_ANTERIOR) {
        sessao->diagnostico_selecionado = sessao->diagnostico_selecionado == 0
                                              ? sessao->quantidade_diagnosticos - 1
                                              : sessao->diagnostico_selecionado - 1;
    } else {
        sessao->diagnostico_selecionado =
            (sessao->diagnostico_selecionado + 1) % sessao->quantidade_diagnosticos;
    }
    return atualizar_depurador(sessao, erro) &&
           texto_formatar(&sessao->estado, erro, "Condition %zu/%zu selected",
                          sessao->diagnostico_selecionado + 1, sessao->quantidade_diagnosticos);
}

bool sef_sessao_ide_inspecionar_condicao(SefSessaoIde *sessao, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session");
        return false;
    }
    if (sessao->quantidade_diagnosticos == 0)
        return texto_definir(&sessao->estado, "No condition to inspect", erro);
    DiagnosticoIde *diagnostico = &sessao->diagnosticos[sessao->diagnostico_selecionado];
    if (diagnostico->condicao == NULL)
        return texto_definir(&sessao->estado, "Selected diagnostic has no CONDITION object", erro);
    SefRaiz **objetos = calloc(1, sizeof(*objetos));
    if (objetos == NULL) {
        sef_erro_definir(erro, 0, 0, "not enough memory to inspect condition");
        return false;
    }
    objetos[0] = sef_raiz_criar(sessao->runtime, sef_raiz_valor(diagnostico->condicao), erro);
    if (objetos[0] == NULL) {
        free(objetos);
        return false;
    }
    liberar_objetos_inspecao(sessao);
    sessao->objetos_inspecao = objetos;
    sessao->quantidade_objetos_inspecao = 1;
    return atualizar_inspetor(sessao, erro) &&
           texto_formatar(&sessao->estado, erro, "Condition %zu opened in inspector",
                          sessao->diagnostico_selecionado + 1);
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
    liberar_caminho_inspecao(sessao);
    return atualizar_inspetor(sessao, erro);
}

bool sef_sessao_ide_inspetor_mover_componente(SefSessaoIde *sessao,
                                              SefMovimentoComponenteIde movimento, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session");
        return false;
    }
    if (sessao->quantidade_objetos_inspecao == 0)
        return true;
    size_t quantidade =
        sef_valor_quantidade_componentes(sessao->runtime, objeto_inspecionado(sessao));
    if (quantidade == 0)
        return texto_definir(&sessao->estado, "Object has no components", erro);
    if (movimento == SEF_COMPONENTE_INSPETOR_ANTERIOR)
        sessao->componente_inspecao =
            sessao->componente_inspecao == 0 ? quantidade - 1 : sessao->componente_inspecao - 1;
    else
        sessao->componente_inspecao = (sessao->componente_inspecao + 1) % quantidade;
    return atualizar_inspetor(sessao, erro);
}

bool sef_sessao_ide_inspetor_entrar(SefSessaoIde *sessao, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session");
        return false;
    }
    if (sessao->quantidade_objetos_inspecao == 0)
        return true;
    SefValor atual = objeto_inspecionado(sessao);
    size_t quantidade = sef_valor_quantidade_componentes(sessao->runtime, atual);
    if (quantidade == 0)
        return texto_definir(&sessao->estado, "Object has no components", erro);
    SefValor componente = NULL;
    char rotulo[64];
    if (!sef_valor_componente(sessao->runtime, atual, sessao->componente_inspecao, &componente,
                              rotulo, sizeof(rotulo))) {
        sef_erro_definir(erro, 0, 0, "could not open inspector component");
        return false;
    }
    SefRaiz *raiz = sef_raiz_criar(sessao->runtime, componente, erro);
    if (raiz == NULL)
        return false;
    if (sessao->profundidade_inspecao == sessao->capacidade_caminho_inspecao) {
        size_t capacidade =
            sessao->capacidade_caminho_inspecao == 0 ? 8 : sessao->capacidade_caminho_inspecao * 2;
        if (capacidade < sessao->capacidade_caminho_inspecao ||
            capacidade > SIZE_MAX / sizeof(*sessao->caminho_inspecao)) {
            sef_raiz_liberar(raiz);
            sef_erro_definir(erro, 0, 0, "inspector path exceeded the limit");
            return false;
        }
        PassoInspecaoIde *passos = realloc(sessao->caminho_inspecao, capacidade * sizeof(*passos));
        if (passos == NULL) {
            sef_raiz_liberar(raiz);
            sef_erro_definir(erro, 0, 0, "not enough memory for inspector path");
            return false;
        }
        sessao->caminho_inspecao = passos;
        sessao->capacidade_caminho_inspecao = capacidade;
    }
    PassoInspecaoIde *passo = &sessao->caminho_inspecao[sessao->profundidade_inspecao++];
    passo->raiz = raiz;
    snprintf(passo->rotulo, sizeof(passo->rotulo), "%s", rotulo);
    sessao->componente_inspecao = 0;
    return atualizar_inspetor(sessao, erro) &&
           texto_formatar(&sessao->estado, erro, "Inspector opened %s (depth %zu)", rotulo,
                          sessao->profundidade_inspecao);
}

bool sef_sessao_ide_inspetor_voltar(SefSessaoIde *sessao, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session");
        return false;
    }
    if (sessao->profundidade_inspecao == 0)
        return texto_definir(&sessao->estado, "Inspector is already at the root", erro);
    sef_raiz_liberar(sessao->caminho_inspecao[--sessao->profundidade_inspecao].raiz);
    sessao->componente_inspecao = 0;
    return atualizar_inspetor(sessao, erro) &&
           texto_formatar(&sessao->estado, erro, "Inspector returned to depth %zu",
                          sessao->profundidade_inspecao);
}

bool sef_sessao_ide_salvar(SefSessaoIde *sessao, const char *caminho, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL || caminho == NULL || caminho[0] == '\0') {
        sef_erro_definir(erro, 0, 0, "missing path while saving");
        return false;
    }
    FILE *arquivo = fopen(caminho, "wb");
    if (arquivo == NULL) {
        sef_erro_definir(erro, 0, 0, "could not save '%s': %s", caminho, strerror(errno));
        return false;
    }
    bool gravou =
        fwrite(sessao->editor.dados, 1, sessao->editor.tamanho, arquivo) == sessao->editor.tamanho;
    if (fclose(arquivo) != 0)
        gravou = false;
    if (!gravou) {
        sef_erro_definir(erro, 0, 0, "failed to write '%s'", caminho);
        return false;
    }
    if (!texto_definir(&sessao->caminho, caminho, erro) || !atualizar_caminho_imagem(sessao, erro))
        return false;
    sessao->documento_modificado = false;
    return atualizar_abas(sessao, erro) &&
           texto_formatar(&sessao->estado, erro, "Saved: %s", caminho);
}

static void tentar_abrir_espaco_trabalho_pai(SefSessaoIde *sessao, const char *caminho) {
    if (sef_espaco_trabalho_ide_raiz(sessao->espaco_trabalho)[0] != '\0')
        return;
    const char *separador = NULL;
    for (const char *cursor = caminho; *cursor != '\0'; cursor++)
        if (*cursor == '/' || *cursor == '\\')
            separador = cursor;
    TextoIde raiz = {0};
    SefErro descarte;
    sef_erro_limpar(&descarte);
    bool definiu = separador == NULL
                       ? texto_definir(&raiz, ".", &descarte)
                       : texto_definir_n(&raiz, caminho,
                                         separador == caminho ? 1u
                                         : separador == caminho + 2 && caminho[1] == ':'
                                             ? 3u
                                             : (size_t)(separador - caminho),
                                         &descarte);
    if (definiu && sef_espaco_trabalho_ide_abrir(sessao->espaco_trabalho, raiz.dados, &descarte)) {
        sessao->arquivo_espaco_trabalho_selecionado = 0;
        atualizar_explorador(sessao, &descarte);
    }
    texto_liberar(&raiz);
}

bool sef_sessao_ide_abrir(SefSessaoIde *sessao, const char *caminho, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL || caminho == NULL || caminho[0] == '\0') {
        sef_erro_definir(erro, 0, 0, "missing path while opening");
        return false;
    }
    for (size_t i = 0; i < sessao->quantidade_documentos; i++) {
        const char *aberto = documento_caminho(sessao, i);
        if (i != sessao->documento_ativo && aberto != NULL && strcmp(aberto, caminho) == 0)
            return sef_sessao_ide_documento_ativar(sessao, i, erro);
    }
    FILE *arquivo = fopen(caminho, "rb");
    if (arquivo == NULL) {
        sef_erro_definir(erro, 0, 0, "could not open '%s': %s", caminho, strerror(errno));
        return false;
    }
    if (fseek(arquivo, 0, SEEK_END) != 0) {
        fclose(arquivo);
        sef_erro_definir(erro, 0, 0, "could not determine the size of '%s'", caminho);
        return false;
    }
    long tamanho = ftell(arquivo);
    if (tamanho < 0 || fseek(arquivo, 0, SEEK_SET) != 0) {
        fclose(arquivo);
        sef_erro_definir(erro, 0, 0, "file '%s' is not seekable", caminho);
        return false;
    }
    if (tamanho > SEF_LIMITE_ARQUIVO_IDE) {
        fclose(arquivo);
        sef_erro_definir(erro, 0, 0, "file '%s' exceeds the IDE's 64 MiB limit", caminho);
        return false;
    }
    char *dados = malloc((size_t)tamanho + 1);
    if (dados == NULL) {
        fclose(arquivo);
        sef_erro_definir(erro, 0, 0, "not enough memory to open '%s'", caminho);
        return false;
    }
    size_t lidos = fread(dados, 1, (size_t)tamanho, arquivo);
    fclose(arquivo);
    dados[lidos] = '\0';
    if (lidos != (size_t)tamanho) {
        free(dados);
        sef_erro_definir(erro, 0, 0, "incomplete read from '%s'", caminho);
        return false;
    }
    bool reutilizar_aba_vazia = sessao->quantidade_documentos == 1 &&
                                !sessao->documento_modificado && sessao->editor.tamanho == 0 &&
                                strcmp(sessao->caminho.dados, "untitled.lisp") == 0;
    bool mesmo_documento = reutilizar_aba_vazia || strcmp(sessao->caminho.dados, caminho) == 0;
    bool abriu = false;
    if (mesmo_documento) {
        abriu = texto_definir_n(&sessao->editor, dados, lidos, erro) &&
                texto_definir(&sessao->caminho, caminho, erro) &&
                atualizar_caminho_imagem(sessao, erro);
        if (abriu) {
            sessao->cursor_editor = lidos;
            selecao_editor_limpar(sessao);
            sessao->documento_modificado = false;
            sessao->quantidade_formas_executadas = 0;
            abriu = sef_historico_editor_registrar(sessao->historico_editor, sessao->editor.dados,
                                                   sessao->cursor_editor, erro);
        }
    } else {
        DocumentoIde novo = {0};
        novo.cursor_editor = lidos;
        novo.historico_editor = sef_historico_editor_criar(dados, lidos, erro);
        abriu = novo.historico_editor != NULL &&
                texto_definir_n(&novo.editor, dados, lidos, erro) &&
                texto_definir(&novo.caminho, caminho, erro) &&
                definir_caminho_imagem(&novo.caminho_imagem, &novo.caminho, erro) &&
                reservar_documentos(sessao, sessao->quantidade_documentos + 1, erro);
        if (abriu) {
            size_t indice = sessao->quantidade_documentos++;
            guardar_documento_ativo(sessao);
            sessao->documentos[indice] = novo;
            memset(&novo, 0, sizeof(novo));
            carregar_documento(sessao, indice);
        }
        documento_liberar(&novo);
    }
    if (abriu) {
        tentar_abrir_espaco_trabalho_pai(sessao, caminho);
        abriu = atualizar_abas(sessao, erro) && atualizar_navegador_atual(sessao, erro) &&
                texto_formatar(&sessao->estado, erro, "Opened: %s", caminho);
    }
    free(dados);
    return abriu;
}

bool sef_sessao_ide_imagem_salvar(SefSessaoIde *sessao, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session");
        return false;
    }
    if (!sef_runtime_imagem_salvar(sessao->runtime, sessao->caminho_imagem.dados, erro)) {
        SefErro causa = *erro;
        registrar_erro(sessao, "IMAGE", &causa, erro);
        if (!erro->ocorreu)
            *erro = causa;
        return false;
    }
    return texto_formatar(&sessao->estado, erro, "World saved: %s", sessao->caminho_imagem.dados);
}

bool sef_sessao_ide_imagem_restaurar(SefSessaoIde *sessao, SefErro *erro) {
    sef_erro_limpar(erro);
    if (sessao == NULL) {
        sef_erro_definir(erro, 0, 0, "missing IDE session");
        return false;
    }
    SefRuntime *restaurado = sef_runtime_imagem_abrir(sessao->caminho_imagem.dados, erro);
    if (restaurado == NULL) {
        SefErro causa = *erro;
        registrar_erro(sessao, "RESTORE IMAGE", &causa, erro);
        if (!erro->ocorreu)
            *erro = causa;
        return false;
    }
    liberar_objetos_inspecao(sessao);
    liberar_diagnosticos(sessao);
    sef_runtime_destruir(sessao->runtime);
    sessao->runtime = restaurado;
    sessao->quantidade_formas_executadas = 0;
    if (!texto_definir(&sessao->inspetor, "OBJECTS: 0\nWORLD RESTORED", erro) ||
        !atualizar_depurador(sessao, erro) ||
        !texto_formatar(&sessao->estado, erro, "World restored: %s", sessao->caminho_imagem.dados))
        return false;
    return true;
}
