#include "sefirah/janela.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <stdio.h>
#include <string.h>

int sef_janela_executar(const SefConfigJanela *configuracao, SefAoDesenhar ao_desenhar,
                        SefAoEvento ao_evento, void *dados, char *mensagem_erro,
                        int capacidade_erro) {
    Display *exibidor = XOpenDisplay(NULL);
    if (exibidor == NULL) {
        snprintf(mensagem_erro, (size_t)capacidade_erro,
                 "nao foi possivel conectar ao servidor X11; verifique DISPLAY");
        return 1;
    }

    int tela = DefaultScreen(exibidor);
    Window janela =
        XCreateSimpleWindow(exibidor, RootWindow(exibidor, tela), 80, 80,
                            (unsigned int)configuracao->largura, (unsigned int)configuracao->altura,
                            0, BlackPixel(exibidor, tela), BlackPixel(exibidor, tela));
    XStoreName(exibidor, janela, configuracao->titulo);
    XSelectInput(exibidor, janela,
                 ExposureMask | StructureNotifyMask | KeyPressMask | ButtonPressMask |
                     ButtonReleaseMask | PointerMotionMask);

    Atom fechar = XInternAtom(exibidor, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(exibidor, janela, &fechar, 1);
    XMapWindow(exibidor, janela);

    SefSuperficie superficie = {0};
    if (!sef_superficie_criar(&superficie, configuracao->largura, configuracao->altura)) {
        snprintf(mensagem_erro, (size_t)capacidade_erro,
                 "memoria insuficiente para a superficie da janela");
        XDestroyWindow(exibidor, janela);
        XCloseDisplay(exibidor);
        return 1;
    }

    GC contexto = XCreateGC(exibidor, janela, 0, NULL);
    bool executando = true;
    bool redesenhar = true;
    while (executando) {
        XEvent evento;
        if (!redesenhar || XPending(exibidor) > 0) {
            XNextEvent(exibidor, &evento);
            if (evento.type == ClientMessage && (Atom)evento.xclient.data.l[0] == fechar) {
                executando = false;
            } else if (evento.type == KeyPress) {
                char texto[8] = {0};
                KeySym tecla = NoSymbol;
                int tamanho =
                    XLookupString(&evento.xkey, texto, (int)sizeof(texto) - 1, &tecla, NULL);
                if (tecla == XK_Escape)
                    executando = false;
                else if (ao_evento != NULL) {
                    SefEventoJanela evento_sefirah = {0};
                    bool controle = (evento.xkey.state & ControlMask) != 0;
                    evento_sefirah.modificador_shift = (evento.xkey.state & ShiftMask) != 0;
                    if (tecla == XK_F5 ||
                        (controle && (tecla == XK_Return || tecla == XK_KP_Enter))) {
                        evento_sefirah.tipo = SEF_EVENTO_EXECUTAR;
                    } else if (tecla == XK_F6) {
                        evento_sefirah.tipo = SEF_EVENTO_EXECUTAR_FORMA;
                    } else if (tecla == XK_F7) {
                        evento_sefirah.tipo = SEF_EVENTO_EXECUTAR_ALTERACOES;
                    } else if (tecla == XK_F8) {
                        evento_sefirah.tipo = SEF_EVENTO_NAVEGAR_DEFINICAO;
                    } else if (tecla == XK_F9) {
                        evento_sefirah.tipo = SEF_EVENTO_SALVAR_IMAGEM;
                    } else if (tecla == XK_F10) {
                        evento_sefirah.tipo = SEF_EVENTO_RESTAURAR_IMAGEM;
                    } else if (tecla == XK_F11) {
                        evento_sefirah.tipo = SEF_EVENTO_IR_PARA_DEFINICAO;
                    } else if (tecla == XK_F12) {
                        evento_sefirah.tipo = SEF_EVENTO_NAVEGAR_REFERENCIA;
                    } else if (controle && (tecla == XK_z || tecla == XK_Z)) {
                        evento_sefirah.tipo = SEF_EVENTO_DESFAZER;
                    } else if (controle && (tecla == XK_y || tecla == XK_Y)) {
                        evento_sefirah.tipo = SEF_EVENTO_REFAZER;
                    } else if (controle && (tecla == XK_s || tecla == XK_S)) {
                        evento_sefirah.tipo = SEF_EVENTO_SALVAR;
                    } else if (controle && (tecla == XK_o || tecla == XK_O)) {
                        evento_sefirah.tipo = SEF_EVENTO_ABRIR;
                    } else if (tecla == XK_Left) {
                        evento_sefirah.tipo = SEF_EVENTO_CURSOR_ESQUERDA;
                    } else if (tecla == XK_Right) {
                        evento_sefirah.tipo = SEF_EVENTO_CURSOR_DIREITA;
                    } else if (tecla == XK_Up) {
                        evento_sefirah.tipo = SEF_EVENTO_CURSOR_CIMA;
                    } else if (tecla == XK_Down) {
                        evento_sefirah.tipo = SEF_EVENTO_CURSOR_BAIXO;
                    } else if (tecla == XK_Home) {
                        evento_sefirah.tipo = SEF_EVENTO_CURSOR_INICIO;
                    } else if (tecla == XK_End) {
                        evento_sefirah.tipo = SEF_EVENTO_CURSOR_FIM;
                    } else if (tecla == XK_Return || tecla == XK_KP_Enter) {
                        evento_sefirah.tipo = SEF_EVENTO_ENTER;
                    } else if (tecla == XK_BackSpace) {
                        evento_sefirah.tipo = SEF_EVENTO_APAGAR;
                    } else if (tecla == XK_Tab || tecla == XK_ISO_Left_Tab) {
                        evento_sefirah.tipo = SEF_EVENTO_TAB;
                    } else if (tamanho > 0) {
                        evento_sefirah.tipo = SEF_EVENTO_TEXTO;
                        memcpy(evento_sefirah.texto_utf8, texto, (size_t)tamanho);
                    } else {
                        continue;
                    }
                    if (ao_evento(&evento_sefirah, dados))
                        redesenhar = true;
                }
            } else if ((evento.type == ButtonPress || evento.type == ButtonRelease ||
                        evento.type == MotionNotify) &&
                       ao_evento != NULL) {
                SefEventoJanela evento_sefirah = {0};
                if (evento.type == ButtonPress)
                    evento_sefirah.tipo = SEF_EVENTO_PONTEIRO_PRESSIONAR;
                else if (evento.type == ButtonRelease)
                    evento_sefirah.tipo = SEF_EVENTO_PONTEIRO_SOLTAR;
                else
                    evento_sefirah.tipo = SEF_EVENTO_PONTEIRO_MOVER;
                evento_sefirah.x =
                    evento.type == MotionNotify ? evento.xmotion.x : evento.xbutton.x;
                evento_sefirah.y =
                    evento.type == MotionNotify ? evento.xmotion.y : evento.xbutton.y;
                if (ao_evento(&evento_sefirah, dados))
                    redesenhar = true;
            } else if (evento.type == ConfigureNotify) {
                int largura = evento.xconfigure.width;
                int altura = evento.xconfigure.height;
                if (!sef_superficie_redimensionar(&superficie, largura, altura)) {
                    snprintf(mensagem_erro, (size_t)capacidade_erro,
                             "memoria insuficiente ao redimensionar janela");
                    executando = false;
                }
                redesenhar = true;
            } else if (evento.type == Expose) {
                redesenhar = true;
            }
        }

        if (executando && redesenhar) {
            ao_desenhar(&superficie, dados);
            XImage *imagem = XCreateImage(
                exibidor, DefaultVisual(exibidor, tela), (unsigned int)DefaultDepth(exibidor, tela),
                ZPixmap, 0, (char *)superficie.pixels, (unsigned int)superficie.largura,
                (unsigned int)superficie.altura, 32, superficie.passo * 4);
            if (imagem == NULL) {
                snprintf(mensagem_erro, (size_t)capacidade_erro,
                         "X11 recusou a imagem da superficie");
                executando = false;
            } else {
                XPutImage(exibidor, janela, contexto, imagem, 0, 0, 0, 0,
                          (unsigned int)superficie.largura, (unsigned int)superficie.altura);
                imagem->data = NULL;
                XDestroyImage(imagem);
                XFlush(exibidor);
                redesenhar = false;
            }
        }
    }

    XFreeGC(exibidor, contexto);
    sef_superficie_destruir(&superficie);
    XDestroyWindow(exibidor, janela);
    XCloseDisplay(exibidor);
    return mensagem_erro[0] == '\0' ? 0 : 1;
}
