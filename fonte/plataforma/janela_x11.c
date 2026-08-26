#include "sefirah/janela.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdio.h>
#include <string.h>

typedef struct EstadoJanelaX11 {
    Display *exibidor;
    int tela;
    Window janela;
    Atom fechar;
    GC contexto;
    SefSuperficie superficie;
    SefAoDesenhar ao_desenhar;
    SefAoEvento ao_evento;
    void *dados;
    char *mensagem_erro;
    int capacidade_erro;
    bool executando;
    bool redesenhar;
    bool janela_destruida;
} EstadoJanelaX11;

static EstadoJanelaX11 *janela_ativa = NULL;

static bool traduzir_tecla(const XKeyEvent *evento, SefEventoJanela *traduzido) {
    char texto[8] = {0};
    KeySym tecla = NoSymbol;
    int tamanho = XLookupString((XKeyEvent *)evento, texto, (int)sizeof(texto) - 1, &tecla, NULL);
    bool controle = (evento->state & ControlMask) != 0;
    traduzido->modificador_shift = (evento->state & ShiftMask) != 0;
    traduzido->modificador_controle = controle;
    if (tecla == XK_Escape)
        traduzido->tipo = SEF_EVENTO_CANCELAR;
    else if (controle && (tecla == XK_p || tecla == XK_P))
        traduzido->tipo = traduzido->modificador_shift ? SEF_EVENTO_PALETA_COMANDOS
                                                        : SEF_EVENTO_ABRIR_RAPIDO;
    else if (controle && (tecla == XK_t || tecla == XK_T))
        traduzido->tipo = SEF_EVENTO_BUSCAR_SIMBOLOS;
    else if (controle && (tecla == XK_f || tecla == XK_F))
        traduzido->tipo = SEF_EVENTO_BUSCAR_EDITOR;
    else if (controle && (tecla == XK_g || tecla == XK_G))
        traduzido->tipo = SEF_EVENTO_IR_PARA_LINHA;
    else if (controle && (tecla == XK_n || tecla == XK_N))
        traduzido->tipo = SEF_EVENTO_NOVO_DOCUMENTO;
    else if (controle && (tecla == XK_w || tecla == XK_W))
        traduzido->tipo = SEF_EVENTO_FECHAR_DOCUMENTO;
    else if (controle && (tecla == XK_a || tecla == XK_A))
        traduzido->tipo = SEF_EVENTO_SELECIONAR_TUDO;
    else if (tecla == XK_F5 || (controle && (tecla == XK_Return || tecla == XK_KP_Enter)))
        traduzido->tipo = SEF_EVENTO_EXECUTAR;
    else if (tecla == XK_F6)
        traduzido->tipo = SEF_EVENTO_EXECUTAR_FORMA;
    else if (tecla == XK_F7)
        traduzido->tipo = SEF_EVENTO_EXECUTAR_ALTERACOES;
    else if (tecla == XK_F8)
        traduzido->tipo = SEF_EVENTO_NAVEGAR_DEFINICAO;
    else if (tecla == XK_F9)
        traduzido->tipo = SEF_EVENTO_SALVAR_IMAGEM;
    else if (tecla == XK_F10)
        traduzido->tipo = SEF_EVENTO_RESTAURAR_IMAGEM;
    else if (tecla == XK_F11)
        traduzido->tipo = SEF_EVENTO_IR_PARA_DEFINICAO;
    else if (tecla == XK_F12)
        traduzido->tipo = SEF_EVENTO_NAVEGAR_REFERENCIA;
    else if (controle && (tecla == XK_z || tecla == XK_Z))
        traduzido->tipo = SEF_EVENTO_DESFAZER;
    else if (controle && (tecla == XK_y || tecla == XK_Y))
        traduzido->tipo = SEF_EVENTO_REFAZER;
    else if (controle && (tecla == XK_s || tecla == XK_S))
        traduzido->tipo = SEF_EVENTO_SALVAR;
    else if (controle && (tecla == XK_o || tecla == XK_O))
        traduzido->tipo = SEF_EVENTO_ABRIR;
    else if (tecla == XK_Left)
        traduzido->tipo = SEF_EVENTO_CURSOR_ESQUERDA;
    else if (tecla == XK_Right)
        traduzido->tipo = SEF_EVENTO_CURSOR_DIREITA;
    else if (tecla == XK_Up)
        traduzido->tipo = SEF_EVENTO_CURSOR_CIMA;
    else if (tecla == XK_Down)
        traduzido->tipo = SEF_EVENTO_CURSOR_BAIXO;
    else if (tecla == XK_Home)
        traduzido->tipo = SEF_EVENTO_CURSOR_INICIO;
    else if (tecla == XK_End)
        traduzido->tipo = SEF_EVENTO_CURSOR_FIM;
    else if (tecla == XK_Return || tecla == XK_KP_Enter)
        traduzido->tipo = SEF_EVENTO_ENTER;
    else if (tecla == XK_BackSpace)
        traduzido->tipo = SEF_EVENTO_APAGAR;
    else if (tecla == XK_Delete)
        traduzido->tipo = SEF_EVENTO_APAGAR_ADIANTE;
    else if (tecla == XK_Tab || tecla == XK_ISO_Left_Tab)
        traduzido->tipo = SEF_EVENTO_TAB;
    else if (tamanho > 0) {
        traduzido->tipo = SEF_EVENTO_TEXTO;
        memcpy(traduzido->texto_utf8, texto, (size_t)tamanho);
    } else {
        return false;
    }
    return true;
}

static void processar_evento(EstadoJanelaX11 *estado, XEvent *evento) {
    if (evento->type == DestroyNotify) {
        estado->janela_destruida = true;
        estado->executando = false;
        return;
    }
    if (evento->type == ClientMessage && (Atom)evento->xclient.data.l[0] == estado->fechar) {
        estado->executando = false;
        return;
    }
    if (evento->type == KeyPress) {
        SefEventoJanela traduzido = {0};
        if (estado->ao_evento == NULL) {
            KeySym tecla = XLookupKeysym(&evento->xkey, 0);
            if (tecla == XK_Escape)
                estado->executando = false;
            return;
        }
        if (traduzir_tecla(&evento->xkey, &traduzido) &&
            estado->ao_evento(&traduzido, estado->dados))
            estado->redesenhar = true;
        return;
    }
    if ((evento->type == ButtonPress || evento->type == ButtonRelease ||
         evento->type == MotionNotify) &&
        estado->ao_evento != NULL) {
        SefEventoJanela traduzido = {0};
        traduzido.tipo = evento->type == ButtonPress     ? SEF_EVENTO_PONTEIRO_PRESSIONAR
                         : evento->type == ButtonRelease ? SEF_EVENTO_PONTEIRO_SOLTAR
                                                        : SEF_EVENTO_PONTEIRO_MOVER;
        traduzido.x = evento->type == MotionNotify ? evento->xmotion.x : evento->xbutton.x;
        traduzido.y = evento->type == MotionNotify ? evento->xmotion.y : evento->xbutton.y;
        if (estado->ao_evento(&traduzido, estado->dados))
            estado->redesenhar = true;
        return;
    }
    if (evento->type == ConfigureNotify) {
        int largura = evento->xconfigure.width;
        int altura = evento->xconfigure.height;
        if (!sef_superficie_redimensionar(&estado->superficie, largura, altura)) {
            snprintf(estado->mensagem_erro, (size_t)estado->capacidade_erro,
                     "not enough memory to resize the window");
            estado->executando = false;
        }
        estado->redesenhar = true;
    } else if (evento->type == Expose) {
        estado->redesenhar = true;
    }
}

static void apresentar(EstadoJanelaX11 *estado) {
    if (!estado->executando || !estado->redesenhar)
        return;
    estado->ao_desenhar(&estado->superficie, estado->dados);
    XImage *imagem = XCreateImage(
        estado->exibidor, DefaultVisual(estado->exibidor, estado->tela),
        (unsigned int)DefaultDepth(estado->exibidor, estado->tela), ZPixmap, 0,
        (char *)estado->superficie.pixels, (unsigned int)estado->superficie.largura,
        (unsigned int)estado->superficie.altura, 32, estado->superficie.passo * 4);
    if (imagem == NULL) {
        snprintf(estado->mensagem_erro, (size_t)estado->capacidade_erro,
                 "X11 refused the surface image");
        estado->executando = false;
        return;
    }
    XPutImage(estado->exibidor, estado->janela, estado->contexto, imagem, 0, 0, 0, 0,
              (unsigned int)estado->superficie.largura,
              (unsigned int)estado->superficie.altura);
    imagem->data = NULL;
    XDestroyImage(imagem);
    XFlush(estado->exibidor);
    estado->redesenhar = false;
}

static void processar_proximo_evento(EstadoJanelaX11 *estado) {
    XEvent evento;
    XNextEvent(estado->exibidor, &evento);
    processar_evento(estado, &evento);
}

bool sef_janela_processar_modal(SefEnquantoModal enquanto_modal, void *dados) {
    EstadoJanelaX11 *estado = janela_ativa;
    if (estado == NULL || enquanto_modal == NULL)
        return false;
    estado->redesenhar = true;
    apresentar(estado);
    while (estado->executando && enquanto_modal(dados)) {
        processar_proximo_evento(estado);
        apresentar(estado);
    }
    return estado->executando;
}

int sef_janela_executar(const SefConfigJanela *configuracao, SefAoDesenhar ao_desenhar,
                        SefAoEvento ao_evento, void *dados, char *mensagem_erro,
                        int capacidade_erro) {
    Display *exibidor = XOpenDisplay(NULL);
    if (exibidor == NULL) {
        snprintf(mensagem_erro, (size_t)capacidade_erro,
                 "could not connect to the X11 server; check DISPLAY");
        return 1;
    }

    EstadoJanelaX11 estado = {0};
    estado.exibidor = exibidor;
    estado.tela = DefaultScreen(exibidor);
    estado.ao_desenhar = ao_desenhar;
    estado.ao_evento = ao_evento;
    estado.dados = dados;
    estado.mensagem_erro = mensagem_erro;
    estado.capacidade_erro = capacidade_erro;
    estado.executando = true;
    estado.redesenhar = true;
    estado.janela = XCreateSimpleWindow(
        exibidor, RootWindow(exibidor, estado.tela), 80, 80,
        (unsigned int)configuracao->largura, (unsigned int)configuracao->altura, 0,
        BlackPixel(exibidor, estado.tela), BlackPixel(exibidor, estado.tela));
    XStoreName(exibidor, estado.janela, configuracao->titulo);
    XSelectInput(exibidor, estado.janela,
                 ExposureMask | StructureNotifyMask | KeyPressMask | ButtonPressMask |
                     ButtonReleaseMask | PointerMotionMask);
    estado.fechar = XInternAtom(exibidor, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(exibidor, estado.janela, &estado.fechar, 1);
    XMapWindow(exibidor, estado.janela);

    if (!sef_superficie_criar(&estado.superficie, configuracao->largura, configuracao->altura)) {
        snprintf(mensagem_erro, (size_t)capacidade_erro,
                 "not enough memory for the window surface");
        XDestroyWindow(exibidor, estado.janela);
        XCloseDisplay(exibidor);
        return 1;
    }

    estado.contexto = XCreateGC(exibidor, estado.janela, 0, NULL);
    janela_ativa = &estado;
    while (estado.executando) {
        if (!estado.redesenhar || XPending(exibidor) > 0)
            processar_proximo_evento(&estado);
        apresentar(&estado);
    }
    janela_ativa = NULL;

    XFreeGC(exibidor, estado.contexto);
    sef_superficie_destruir(&estado.superficie);
    if (!estado.janela_destruida)
        XDestroyWindow(exibidor, estado.janela);
    XCloseDisplay(exibidor);
    return mensagem_erro[0] == '\0' ? 0 : 1;
}
