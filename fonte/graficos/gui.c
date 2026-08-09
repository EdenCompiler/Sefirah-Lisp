#include "sefirah/gui.h"

#include <stdlib.h>
#include <string.h>

SefTemaGui sef_tema_gui_classico(void) {
    SefTemaGui tema = {SEF_COR(207, 198, 164),
                       SEF_COR(244, 238, 211),
                       SEF_COR(177, 196, 154),
                       SEF_COR(43, 54, 45),
                       SEF_COR(43, 54, 45),
                       SEF_COR(107, 39, 55),
                       8,
                       6};
    return tema;
}

void sef_componente_iniciar(SefComponente *componente, SefTipoComponente tipo, const char *texto) {
    if (componente == NULL)
        return;
    memset(componente, 0, sizeof(*componente));
    componente->tipo = tipo;
    componente->texto = texto == NULL ? "" : texto;
    componente->peso = tipo == SEF_COMPONENTE_PAINEL ? 1 : 0;
    componente->altura_preferida = tipo == SEF_COMPONENTE_ROTULO ? 18 : 28;
    componente->direcao = SEF_LAYOUT_COLUNA;
    componente->visivel = true;
    componente->habilitado = true;
    componente->aceita_foco = tipo == SEF_COMPONENTE_BOTAO || tipo == SEF_COMPONENTE_CAMPO_TEXTO;
}

bool sef_componente_adicionar(SefComponente *pai, SefComponente *filho) {
    if (pai == NULL || filho == NULL || pai == filho || filho->pai != NULL)
        return false;
    if (pai->quantidade_filhos == pai->capacidade_filhos) {
        size_t capacidade = pai->capacidade_filhos == 0 ? 4 : pai->capacidade_filhos * 2;
        SefComponente **filhos = realloc(pai->filhos, capacidade * sizeof(*filhos));
        if (filhos == NULL)
            return false;
        pai->filhos = filhos;
        pai->capacidade_filhos = capacidade;
    }
    pai->filhos[pai->quantidade_filhos++] = filho;
    filho->pai = pai;
    return true;
}

void sef_componente_liberar(SefComponente *componente) {
    if (componente == NULL)
        return;
    for (size_t i = 0; i < componente->quantidade_filhos; i++)
        sef_componente_liberar(componente->filhos[i]);
    free(componente->filhos);
    componente->filhos = NULL;
    componente->quantidade_filhos = 0;
    componente->capacidade_filhos = 0;
    componente->pai = NULL;
}

static int preferencia_no_eixo(const SefComponente *componente, bool horizontal) {
    return horizontal ? componente->largura_preferida : componente->altura_preferida;
}

static void organizar_filhos(SefComponente *pai, const SefTemaGui *tema) {
    bool horizontal = pai->direcao == SEF_LAYOUT_LINHA;
    int margem = pai->margem > 0 ? pai->margem : tema->margem;
    int espacamento = pai->espacamento > 0 ? pai->espacamento : tema->espacamento;
    int comprimento = (horizontal ? pai->limites.largura : pai->limites.altura) - margem * 2;
    int transversal = (horizontal ? pai->limites.altura : pai->limites.largura) - margem * 2;
    size_t visiveis = 0;
    int fixo = 0;
    int peso_total = 0;
    for (size_t i = 0; i < pai->quantidade_filhos; i++) {
        SefComponente *filho = pai->filhos[i];
        if (!filho->visivel)
            continue;
        visiveis++;
        if (filho->peso > 0)
            peso_total += filho->peso;
        else
            fixo += preferencia_no_eixo(filho, horizontal);
    }
    if (visiveis > 1)
        comprimento -= (int)(visiveis - 1) * espacamento;
    int flexivel = comprimento - fixo;
    if (flexivel < 0)
        flexivel = 0;
    int cursor = (horizontal ? pai->limites.x : pai->limites.y) + margem;
    for (size_t i = 0; i < pai->quantidade_filhos; i++) {
        SefComponente *filho = pai->filhos[i];
        if (!filho->visivel)
            continue;
        int tamanho = filho->peso > 0 && peso_total > 0 ? flexivel * filho->peso / peso_total
                                                        : preferencia_no_eixo(filho, horizontal);
        if (horizontal)
            filho->limites = (SefRetangulo){cursor, pai->limites.y + margem, tamanho, transversal};
        else
            filho->limites = (SefRetangulo){pai->limites.x + margem, cursor, transversal, tamanho};
        cursor += tamanho + espacamento;
        organizar_filhos(filho, tema);
    }
}

void sef_componente_organizar(SefComponente *raiz, SefRetangulo limites, const SefTemaGui *tema) {
    if (raiz == NULL || tema == NULL)
        return;
    raiz->limites = limites;
    organizar_filhos(raiz, tema);
}

static void desenhar_um(const SefComponente *componente, SefSuperficie *superficie,
                        const SefTemaGui *tema) {
    if (!componente->visivel)
        return;
    SefRetangulo r = componente->limites;
    if (componente->tipo == SEF_COMPONENTE_PAINEL) {
        sef_superficie_retangulo(superficie, r.x, r.y, r.largura, r.altura, tema->superficie);
    } else if (componente->tipo == SEF_COMPONENTE_ROTULO) {
        sef_superficie_texto(superficie, r.x, r.y + 4, componente->texto, 1, tema->texto);
    } else {
        SefCor fundo =
            componente->tipo == SEF_COMPONENTE_BOTAO ? tema->superficie_destaque : tema->superficie;
        sef_superficie_retangulo(superficie, r.x, r.y, r.largura, r.altura, fundo);
        sef_superficie_contorno(superficie, r.x, r.y, r.largura, r.altura,
                                componente->tem_foco ? 3 : 1,
                                componente->tem_foco ? tema->foco : tema->borda);
        sef_superficie_texto(superficie, r.x + 7, r.y + 8, componente->texto, 1, tema->texto);
    }
    for (size_t i = 0; i < componente->quantidade_filhos; i++)
        desenhar_um(componente->filhos[i], superficie, tema);
}

void sef_componente_desenhar(const SefComponente *raiz, SefSuperficie *superficie,
                             const SefTemaGui *tema) {
    if (raiz == NULL || superficie == NULL || tema == NULL)
        return;
    desenhar_um(raiz, superficie, tema);
}

static bool contem(SefRetangulo r, int x, int y) {
    return x >= r.x && y >= r.y && x < r.x + r.largura && y < r.y + r.altura;
}

SefComponente *sef_componente_em(SefComponente *raiz, int x, int y) {
    if (raiz == NULL || !raiz->visivel || !contem(raiz->limites, x, y))
        return NULL;
    for (size_t i = raiz->quantidade_filhos; i > 0; i--) {
        SefComponente *encontrado = sef_componente_em(raiz->filhos[i - 1], x, y);
        if (encontrado != NULL)
            return encontrado;
    }
    return raiz;
}

static void coletar_focaveis(SefComponente *raiz, SefComponente ***itens, size_t *quantidade,
                             size_t *capacidade) {
    if (raiz == NULL || !raiz->visivel || !raiz->habilitado)
        return;
    if (raiz->aceita_foco) {
        if (*quantidade == *capacidade) {
            size_t nova_capacidade = *capacidade == 0 ? 8 : *capacidade * 2;
            SefComponente **novos = realloc(*itens, nova_capacidade * sizeof(*novos));
            if (novos == NULL)
                return;
            *itens = novos;
            *capacidade = nova_capacidade;
        }
        (*itens)[(*quantidade)++] = raiz;
    }
    for (size_t i = 0; i < raiz->quantidade_filhos; i++)
        coletar_focaveis(raiz->filhos[i], itens, quantidade, capacidade);
}

SefComponente *sef_componente_focar_proximo(SefComponente *raiz, SefComponente *atual,
                                            bool retroceder) {
    SefComponente **itens = NULL;
    size_t quantidade = 0, capacidade = 0;
    coletar_focaveis(raiz, &itens, &quantidade, &capacidade);
    if (quantidade == 0) {
        free(itens);
        return NULL;
    }
    size_t indice = retroceder ? quantidade - 1 : 0;
    for (size_t i = 0; i < quantidade; i++) {
        itens[i]->tem_foco = false;
        if (itens[i] == atual)
            indice = retroceder ? (i + quantidade - 1) % quantidade : (i + 1) % quantidade;
    }
    itens[indice]->tem_foco = true;
    SefComponente *resultado = itens[indice];
    free(itens);
    return resultado;
}

bool sef_componente_acionar(SefComponente *componente) {
    if (componente == NULL || !componente->visivel || !componente->habilitado ||
        componente->ao_acionar == NULL)
        return false;
    componente->ao_acionar(componente->dados_acao);
    return true;
}

static void remover_foco(SefComponente *componente) {
    componente->tem_foco = false;
    for (size_t i = 0; i < componente->quantidade_filhos; i++)
        remover_foco(componente->filhos[i]);
}

bool sef_gui_tratar_evento(SefComponente *raiz, SefInteracaoGui *interacao,
                           const SefEventoJanela *evento) {
    if (raiz == NULL || interacao == NULL || evento == NULL)
        return false;
    if (evento->tipo == SEF_EVENTO_TAB) {
        interacao->foco =
            sef_componente_focar_proximo(raiz, interacao->foco, evento->modificador_shift);
        return interacao->foco != NULL;
    }
    if (evento->tipo == SEF_EVENTO_PONTEIRO_PRESSIONAR) {
        SefComponente *alvo = sef_componente_em(raiz, evento->x, evento->y);
        interacao->pressionado = alvo != NULL && alvo->habilitado ? alvo : NULL;
        if (alvo != NULL && alvo->aceita_foco && alvo->habilitado) {
            remover_foco(raiz);
            alvo->tem_foco = true;
            interacao->foco = alvo;
        }
        return alvo != NULL;
    }
    if (evento->tipo == SEF_EVENTO_PONTEIRO_SOLTAR) {
        SefComponente *alvo = sef_componente_em(raiz, evento->x, evento->y);
        bool acionou = alvo == interacao->pressionado && sef_componente_acionar(alvo);
        interacao->pressionado = NULL;
        return acionou;
    }
    if (evento->tipo == SEF_EVENTO_ATIVAR || evento->tipo == SEF_EVENTO_ENTER)
        return sef_componente_acionar(interacao->foco);
    return false;
}
