#include "sefirah/janela.h"

#include <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/runtime.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

typedef struct RetanguloNS {
    CGPoint origin;
    CGSize size;
} RetanguloNS;

typedef struct EstadoJanelaMac {
    SefSuperficie superficie;
    SefAoDesenhar ao_desenhar;
    SefAoEvento ao_evento;
    void *dados;
    id janela;
    id vista;
} EstadoJanelaMac;

enum {
    MODIFICADOR_SHIFT_MAC = 1ul << 17,
    MODIFICADOR_CONTROL_MAC = 1ul << 18,
    MODIFICADOR_COMMAND_MAC = 1ul << 20
};

#define ENVIAR0(tipo, alvo, seletor)                                                               \
    ((tipo(*)(id, SEL))(void *)objc_msgSend)((id)(alvo), sel_registerName(seletor))
#define ENVIAR1(tipo, alvo, seletor, t1, a1)                                                       \
    ((tipo(*)(id, SEL, t1))(void *)objc_msgSend)((id)(alvo), sel_registerName(seletor), (a1))

static EstadoJanelaMac *estado_da_vista(id vista) {
    void *estado = NULL;
    object_getInstanceVariable(vista, "estadoSefirah", &estado);
    return estado;
}

static id criar_imagem(const SefSuperficie *superficie) {
    CGColorSpaceRef cores = CGColorSpaceCreateDeviceRGB();
    if (cores == NULL)
        return NULL;
    size_t bytes = (size_t)superficie->passo * (size_t)superficie->altura * sizeof(SefCor);
    CGDataProviderRef provedor =
        CGDataProviderCreateWithData(NULL, superficie->pixels, bytes, NULL);
    if (provedor == NULL) {
        CGColorSpaceRelease(cores);
        return NULL;
    }
    CGImageRef imagem_cg = CGImageCreate((size_t)superficie->largura, (size_t)superficie->altura, 8,
                                         32, (size_t)superficie->passo * sizeof(SefCor), cores,
                                         kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
                                         provedor, NULL, false, kCGRenderingIntentDefault);
    CGDataProviderRelease(provedor);
    CGColorSpaceRelease(cores);
    if (imagem_cg == NULL)
        return NULL;

    id imagem_alocada = ENVIAR0(id, (id)objc_getClass("NSImage"), "alloc");
    typedef id (*CriarImagem)(id, SEL, CGImageRef, CGSize);
    CGSize tamanho = {(double)superficie->largura, (double)superficie->altura};
    id imagem = ((CriarImagem)(void *)objc_msgSend)(
        imagem_alocada, sel_registerName("initWithCGImage:size:"), imagem_cg, tamanho);
    CGImageRelease(imagem_cg);
    return imagem;
}

static bool redesenhar(EstadoJanelaMac *estado) {
    estado->ao_desenhar(&estado->superficie, estado->dados);
    id imagem = criar_imagem(&estado->superficie);
    if (imagem == NULL)
        return false;
    ENVIAR1(void, estado->vista, "setImage:", id, imagem);
    ENVIAR1(void, estado->vista, "setNeedsDisplay:", signed char, 1);
    ENVIAR0(void, imagem, "release");
    return true;
}

static void enviar_evento(EstadoJanelaMac *estado, SefEventoJanela *evento) {
    if (estado->ao_evento != NULL && estado->ao_evento(evento, estado->dados))
        redesenhar(estado);
}

static signed char vista_aceita_primeiro_responder(id self, SEL cmd) {
    (void)self;
    (void)cmd;
    return 1;
}

static signed char vista_e_invertida(id self, SEL cmd) {
    (void)self;
    (void)cmd;
    return 1;
}

static bool texto_igual_ascii(const char *texto, char esperado) {
    return texto != NULL && texto[0] != '\0' && texto[1] == '\0' &&
           (texto[0] == esperado || texto[0] == (char)(esperado - 'a' + 'A'));
}

static size_t tamanho_primeiro_utf8(const unsigned char *texto) {
    if (texto[0] < 0x80u)
        return 1;
    if ((texto[0] & 0xe0u) == 0xc0u)
        return 2;
    if ((texto[0] & 0xf0u) == 0xe0u)
        return 3;
    if ((texto[0] & 0xf8u) == 0xf0u)
        return 4;
    return 1;
}

static void enviar_texto_utf8(EstadoJanelaMac *estado, const char *texto) {
    const unsigned char *cursor = (const unsigned char *)texto;
    while (cursor != NULL && *cursor != '\0') {
        size_t tamanho = tamanho_primeiro_utf8(cursor);
        size_t disponivel = strlen((const char *)cursor);
        if (tamanho > disponivel)
            tamanho = 1;
        SefEventoJanela evento = {0};
        evento.tipo = SEF_EVENTO_TEXTO;
        memcpy(evento.texto_utf8, cursor, tamanho);
        enviar_evento(estado, &evento);
        cursor += tamanho;
    }
}

static void vista_tecla_pressionada(id self, SEL cmd, id evento_nativo) {
    (void)cmd;
    EstadoJanelaMac *estado = estado_da_vista(self);
    if (estado == NULL)
        return;
    unsigned short codigo = ENVIAR0(unsigned short, evento_nativo, "keyCode");
    unsigned long modificadores = ENVIAR0(unsigned long, evento_nativo, "modifierFlags");
    bool comando = (modificadores & (MODIFICADOR_COMMAND_MAC | MODIFICADOR_CONTROL_MAC)) != 0;
    SefEventoJanela evento = {0};
    bool reconhecido = true;

    if (codigo == 53) {
        ENVIAR1(void, estado->janela, "performClose:", id, NULL);
        return;
    }
    if (codigo == 96 || (comando && (codigo == 36 || codigo == 76))) {
        evento.tipo = SEF_EVENTO_EXECUTAR;
    } else if (codigo == 36 || codigo == 76) {
        evento.tipo = SEF_EVENTO_ENTER;
    } else if (codigo == 51) {
        evento.tipo = SEF_EVENTO_APAGAR;
    } else if (codigo == 48) {
        evento.tipo = SEF_EVENTO_TAB;
        evento.modificador_shift = (modificadores & MODIFICADOR_SHIFT_MAC) != 0;
    } else if (codigo == 123) {
        evento.tipo = SEF_EVENTO_CURSOR_ESQUERDA;
    } else if (codigo == 124) {
        evento.tipo = SEF_EVENTO_CURSOR_DIREITA;
    } else if (codigo == 126) {
        evento.tipo = SEF_EVENTO_CURSOR_CIMA;
    } else if (codigo == 125) {
        evento.tipo = SEF_EVENTO_CURSOR_BAIXO;
    } else if (codigo == 115) {
        evento.tipo = SEF_EVENTO_CURSOR_INICIO;
    } else if (codigo == 119) {
        evento.tipo = SEF_EVENTO_CURSOR_FIM;
    } else {
        reconhecido = false;
    }

    id caracteres_sem_modificador = ENVIAR0(id, evento_nativo, "charactersIgnoringModifiers");
    const char *atalho = caracteres_sem_modificador == NULL
                             ? NULL
                             : ENVIAR0(const char *, caracteres_sem_modificador, "UTF8String");
    if (comando && texto_igual_ascii(atalho, 's')) {
        evento.tipo = SEF_EVENTO_SALVAR;
        reconhecido = true;
    } else if (comando && texto_igual_ascii(atalho, 'o')) {
        evento.tipo = SEF_EVENTO_ABRIR;
        reconhecido = true;
    }

    if (reconhecido) {
        enviar_evento(estado, &evento);
    } else if (!comando) {
        id caracteres = ENVIAR0(id, evento_nativo, "characters");
        const char *texto =
            caracteres == NULL ? NULL : ENVIAR0(const char *, caracteres, "UTF8String");
        if (texto != NULL)
            enviar_texto_utf8(estado, texto);
    }
}

static CGPoint ponto_local(id vista, id evento_nativo) {
    CGPoint ponto_janela = ENVIAR0(CGPoint, evento_nativo, "locationInWindow");
    typedef CGPoint (*ConverterPonto)(id, SEL, CGPoint, id);
    return ((ConverterPonto)(void *)objc_msgSend)(vista, sel_registerName("convertPoint:fromView:"),
                                                  ponto_janela, NULL);
}

static void enviar_ponteiro(id self, id evento_nativo, SefTipoEventoJanela tipo) {
    EstadoJanelaMac *estado = estado_da_vista(self);
    if (estado == NULL)
        return;
    CGPoint ponto = ponto_local(self, evento_nativo);
    SefEventoJanela evento = {0};
    evento.tipo = tipo;
    evento.x = (int)ponto.x;
    evento.y = (int)ponto.y;
    enviar_evento(estado, &evento);
}

static void vista_mouse_pressionado(id self, SEL cmd, id evento_nativo) {
    (void)cmd;
    enviar_ponteiro(self, evento_nativo, SEF_EVENTO_PONTEIRO_PRESSIONAR);
}

static void vista_mouse_solto(id self, SEL cmd, id evento_nativo) {
    (void)cmd;
    enviar_ponteiro(self, evento_nativo, SEF_EVENTO_PONTEIRO_SOLTAR);
}

static void vista_mouse_movido(id self, SEL cmd, id evento_nativo) {
    (void)cmd;
    enviar_ponteiro(self, evento_nativo, SEF_EVENTO_PONTEIRO_MOVER);
}

static Class classe_vista_sefirah(void) {
    Class classe = objc_getClass("SefirahVistaRaster");
    if (classe != Nil)
        return classe;
    classe = objc_allocateClassPair(objc_getClass("NSImageView"), "SefirahVistaRaster", 0);
    if (classe == Nil)
        return Nil;
    unsigned char alinhamento_ponteiro = sizeof(void *) == 8 ? 3 : 2;
    if (!class_addIvar(classe, "estadoSefirah", sizeof(void *), alinhamento_ponteiro, "^v") ||
        !class_addMethod(classe, sel_registerName("acceptsFirstResponder"),
                         (IMP)vista_aceita_primeiro_responder, "c@:") ||
        !class_addMethod(classe, sel_registerName("isFlipped"), (IMP)vista_e_invertida, "c@:") ||
        !class_addMethod(classe, sel_registerName("keyDown:"), (IMP)vista_tecla_pressionada,
                         "v@:@") ||
        !class_addMethod(classe, sel_registerName("mouseDown:"), (IMP)vista_mouse_pressionado,
                         "v@:@") ||
        !class_addMethod(classe, sel_registerName("mouseUp:"), (IMP)vista_mouse_solto, "v@:@") ||
        !class_addMethod(classe, sel_registerName("mouseMoved:"), (IMP)vista_mouse_movido,
                         "v@:@") ||
        !class_addMethod(classe, sel_registerName("mouseDragged:"), (IMP)vista_mouse_movido,
                         "v@:@")) {
        objc_disposeClassPair(classe);
        return Nil;
    }
    objc_registerClassPair(classe);
    return classe;
}

static signed char encerrar_apos_ultima_janela(id self, SEL cmd, id aplicacao) {
    (void)self;
    (void)cmd;
    (void)aplicacao;
    return 1;
}

static Class classe_delegada_sefirah(void) {
    Class classe = objc_getClass("SefirahDelegadaAplicacao");
    if (classe != Nil)
        return classe;
    classe = objc_allocateClassPair(objc_getClass("NSObject"), "SefirahDelegadaAplicacao", 0);
    if (classe == Nil ||
        !class_addMethod(classe,
                         sel_registerName("applicationShouldTerminateAfterLastWindowClosed:"),
                         (IMP)encerrar_apos_ultima_janela, "c@:@")) {
        if (classe != Nil)
            objc_disposeClassPair(classe);
        return Nil;
    }
    objc_registerClassPair(classe);
    return classe;
}

int sef_janela_executar(const SefConfigJanela *configuracao, SefAoDesenhar ao_desenhar,
                        SefAoEvento ao_evento, void *dados, char *mensagem_erro,
                        int capacidade_erro) {
    if (configuracao == NULL || ao_desenhar == NULL) {
        snprintf(mensagem_erro, (size_t)capacidade_erro, "configuracao de janela invalida");
        return 1;
    }
    id pool = ENVIAR0(id, (id)objc_getClass("NSAutoreleasePool"), "new");
    id aplicacao = ENVIAR0(id, (id)objc_getClass("NSApplication"), "sharedApplication");
    ENVIAR1(void, aplicacao, "setActivationPolicy:", long, 0);

    Class classe_delegada = classe_delegada_sefirah();
    Class classe_vista = classe_vista_sefirah();
    if (classe_delegada == Nil || classe_vista == Nil) {
        snprintf(mensagem_erro, (size_t)capacidade_erro,
                 "runtime Objective-C recusou as classes da janela");
        ENVIAR0(void, pool, "drain");
        return 1;
    }
    id delegada = ENVIAR0(id, (id)classe_delegada, "new");
    ENVIAR1(void, aplicacao, "setDelegate:", id, delegada);

    EstadoJanelaMac estado = {0};
    estado.ao_desenhar = ao_desenhar;
    estado.ao_evento = ao_evento;
    estado.dados = dados;
    if (!sef_superficie_criar(&estado.superficie, configuracao->largura, configuracao->altura)) {
        snprintf(mensagem_erro, (size_t)capacidade_erro, "memoria insuficiente para superficie");
        ENVIAR0(void, delegada, "release");
        ENVIAR0(void, pool, "drain");
        return 1;
    }

    RetanguloNS quadro = {{0, 0}, {(double)configuracao->largura, (double)configuracao->altura}};
    id janela_alocada = ENVIAR0(id, (id)objc_getClass("NSWindow"), "alloc");
    typedef id (*CriarJanela)(id, SEL, RetanguloNS, unsigned long, unsigned long, signed char);
    estado.janela = ((CriarJanela)(void *)objc_msgSend)(
        janela_alocada, sel_registerName("initWithContentRect:styleMask:backing:defer:"), quadro,
        1ul | 2ul | 4ul, 2ul, 0);
    if (estado.janela == NULL) {
        snprintf(mensagem_erro, (size_t)capacidade_erro, "AppKit recusou a janela");
        sef_superficie_destruir(&estado.superficie);
        ENVIAR0(void, delegada, "release");
        ENVIAR0(void, pool, "drain");
        return 1;
    }

    id titulo = ENVIAR1(id, (id)objc_getClass("NSString"), "stringWithUTF8String:", const char *,
                        configuracao->titulo);
    ENVIAR1(void, estado.janela, "setTitle:", id, titulo);
    ENVIAR1(void, estado.janela, "setAcceptsMouseMovedEvents:", signed char, 1);

    id vista_alocada = ENVIAR0(id, (id)classe_vista, "alloc");
    estado.vista = ENVIAR1(id, vista_alocada, "initWithFrame:", RetanguloNS, quadro);
    object_setInstanceVariable(estado.vista, "estadoSefirah", &estado);
    ENVIAR1(void, estado.vista, "setImageScaling:", long, 1);
    ENVIAR1(void, estado.janela, "setContentView:", id, estado.vista);
    if (!redesenhar(&estado)) {
        snprintf(mensagem_erro, (size_t)capacidade_erro, "CoreGraphics recusou a imagem da janela");
        object_setInstanceVariable(estado.vista, "estadoSefirah", NULL);
        ENVIAR0(void, estado.vista, "release");
        ENVIAR0(void, estado.janela, "release");
        sef_superficie_destruir(&estado.superficie);
        ENVIAR0(void, delegada, "release");
        ENVIAR0(void, pool, "drain");
        return 1;
    }

    ENVIAR1(void, estado.janela, "makeKeyAndOrderFront:", id, NULL);
    ENVIAR1(signed char, estado.janela, "makeFirstResponder:", id, estado.vista);
    ENVIAR1(void, aplicacao, "activateIgnoringOtherApps:", signed char, 1);
    ENVIAR0(void, aplicacao, "run");

    object_setInstanceVariable(estado.vista, "estadoSefirah", NULL);
    ENVIAR0(void, estado.vista, "release");
    ENVIAR0(void, estado.janela, "release");
    ENVIAR0(void, delegada, "release");
    sef_superficie_destruir(&estado.superficie);
    ENVIAR0(void, pool, "drain");
    return mensagem_erro[0] == '\0' ? 0 : 1;
}
