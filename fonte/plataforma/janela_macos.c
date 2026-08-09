#include "sefirah/janela.h"

#include <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/runtime.h>

#include <stdbool.h>
#include <stdio.h>

typedef struct {
    CGPoint origin;
    CGSize size;
} RetanguloNS;

#define ENVIAR0(tipo, alvo, seletor)                                                               \
    ((tipo(*)(id, SEL))(void *)objc_msgSend)((id)(alvo), sel_registerName(seletor))
#define ENVIAR1(tipo, alvo, seletor, t1, a1)                                                       \
    ((tipo(*)(id, SEL, t1))(void *)objc_msgSend)((id)(alvo), sel_registerName(seletor), (a1))

static signed char encerrar_apos_ultima_janela(id self, SEL cmd, id aplicacao) {
    (void)self;
    (void)cmd;
    (void)aplicacao;
    return 1;
}

int sef_janela_executar(const SefConfigJanela *configuracao, SefAoDesenhar ao_desenhar,
                        SefAoEvento ao_evento, void *dados, char *mensagem_erro,
                        int capacidade_erro) {
    id pool = ENVIAR0(id, (id)objc_getClass("NSAutoreleasePool"), "new");
    (void)ao_evento; /* A ponte de eventos de teclado entra no proximo marco macOS. */
    id aplicacao = ENVIAR0(id, (id)objc_getClass("NSApplication"), "sharedApplication");
    ENVIAR1(void, aplicacao, "setActivationPolicy:", long, 0);

    Class classe_delegada = objc_getClass("SefirahDelegadaAplicacao");
    if (classe_delegada == Nil) {
        classe_delegada =
            objc_allocateClassPair(objc_getClass("NSObject"), "SefirahDelegadaAplicacao", 0);
        class_addMethod(classe_delegada,
                        sel_registerName("applicationShouldTerminateAfterLastWindowClosed:"),
                        (IMP)encerrar_apos_ultima_janela, "c@:@");
        objc_registerClassPair(classe_delegada);
    }
    id delegada = ENVIAR0(id, (id)classe_delegada, "new");
    ENVIAR1(void, aplicacao, "setDelegate:", id, delegada);

    SefSuperficie superficie = {0};
    if (!sef_superficie_criar(&superficie, configuracao->largura, configuracao->altura)) {
        snprintf(mensagem_erro, (size_t)capacidade_erro, "memoria insuficiente para superficie");
        return 1;
    }
    ao_desenhar(&superficie, dados);

    CGColorSpaceRef cores = CGColorSpaceCreateDeviceRGB();
    CGDataProviderRef provedor = CGDataProviderCreateWithData(
        NULL, superficie.pixels, (size_t)superficie.passo * (size_t)superficie.altura * 4u, NULL);
    CGImageRef imagem_cg = CGImageCreate((size_t)superficie.largura, (size_t)superficie.altura, 8,
                                         32, (size_t)superficie.passo * 4u, cores,
                                         kCGBitmapByteOrder32Little | kCGImageAlphaNoneSkipFirst,
                                         provedor, NULL, false, kCGRenderingIntentDefault);

    RetanguloNS quadro = {{0, 0}, {(double)configuracao->largura, (double)configuracao->altura}};
    id janela_alocada = ENVIAR0(id, (id)objc_getClass("NSWindow"), "alloc");
    typedef id (*CriarJanela)(id, SEL, RetanguloNS, unsigned long, unsigned long, signed char);
    id janela = ((CriarJanela)(void *)objc_msgSend)(
        janela_alocada, sel_registerName("initWithContentRect:styleMask:backing:defer:"), quadro,
        1ul | 2ul | 4ul, 2ul, 0);

    id titulo = ENVIAR1(id, (id)objc_getClass("NSString"), "stringWithUTF8String:", const char *,
                        configuracao->titulo);
    ENVIAR1(void, janela, "setTitle:", id, titulo);

    id imagem_alocada = ENVIAR0(id, (id)objc_getClass("NSImage"), "alloc");
    typedef id (*CriarImagem)(id, SEL, CGImageRef, CGSize);
    id imagem = ((CriarImagem)(void *)objc_msgSend)(
        imagem_alocada, sel_registerName("initWithCGImage:size:"), imagem_cg, quadro.size);
    id vista_alocada = ENVIAR0(id, (id)objc_getClass("NSImageView"), "alloc");
    id vista = ENVIAR1(id, vista_alocada, "initWithFrame:", RetanguloNS, quadro);
    ENVIAR1(void, vista, "setImage:", id, imagem);
    ENVIAR1(void, janela, "setContentView:", id, vista);
    ENVIAR1(void, janela, "makeKeyAndOrderFront:", id, NULL);
    ENVIAR1(void, aplicacao, "activateIgnoringOtherApps:", signed char, 1);

    CGImageRelease(imagem_cg);
    CGDataProviderRelease(provedor);
    CGColorSpaceRelease(cores);
    ENVIAR0(void, aplicacao, "run");

    ENVIAR0(void, vista, "release");
    ENVIAR0(void, imagem, "release");
    ENVIAR0(void, janela, "release");
    ENVIAR0(void, delegada, "release");
    sef_superficie_destruir(&superficie);
    ENVIAR0(void, pool, "drain");
    return 0;
}
