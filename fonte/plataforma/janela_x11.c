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
                    if (tecla == XK_Return || tecla == XK_KP_Enter) {
                        evento_sefirah.tipo = SEF_EVENTO_ENTER;
                    } else if (tecla == XK_BackSpace) {
                        evento_sefirah.tipo = SEF_EVENTO_APAGAR;
                    } else if (tecla == XK_Tab || tecla == XK_ISO_Left_Tab) {
                        evento_sefirah.tipo = SEF_EVENTO_TAB;
                        evento_sefirah.modificador_shift = (evento.xkey.state & ShiftMask) != 0;
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
