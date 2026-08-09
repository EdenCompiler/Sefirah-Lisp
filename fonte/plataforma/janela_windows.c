#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "sefirah/janela.h"

#include <stdio.h>

typedef struct EstadoJanela {
    SefSuperficie superficie;
    SefAoDesenhar ao_desenhar;
    SefAoEvento ao_evento;
    void *dados;
} EstadoJanela;

static LRESULT CALLBACK procedimento(HWND janela, UINT mensagem, WPARAM wparam, LPARAM lparam) {
    EstadoJanela *estado = (EstadoJanela *)GetWindowLongPtrW(janela, GWLP_USERDATA);
    if (mensagem == WM_NCCREATE) {
        CREATESTRUCTW *criacao = (CREATESTRUCTW *)lparam;
        estado = (EstadoJanela *)criacao->lpCreateParams;
        SetWindowLongPtrW(janela, GWLP_USERDATA, (LONG_PTR)estado);
    } else if (mensagem == WM_SIZE && estado != NULL) {
        int largura = LOWORD(lparam);
        int altura = HIWORD(lparam);
        if (largura > 0 && altura > 0) {
            sef_superficie_redimensionar(&estado->superficie, largura, altura);
            InvalidateRect(janela, NULL, FALSE);
        }
    } else if (mensagem == WM_PAINT && estado != NULL) {
        PAINTSTRUCT pintura;
        HDC dc = BeginPaint(janela, &pintura);
        estado->ao_desenhar(&estado->superficie, estado->dados);
        BITMAPINFO informacao = {0};
        informacao.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        informacao.bmiHeader.biWidth = estado->superficie.largura;
        informacao.bmiHeader.biHeight = -estado->superficie.altura;
        informacao.bmiHeader.biPlanes = 1;
        informacao.bmiHeader.biBitCount = 32;
        informacao.bmiHeader.biCompression = BI_RGB;
        StretchDIBits(dc, 0, 0, estado->superficie.largura, estado->superficie.altura, 0, 0,
                      estado->superficie.largura, estado->superficie.altura,
                      estado->superficie.pixels, &informacao, DIB_RGB_COLORS, SRCCOPY);
        EndPaint(janela, &pintura);
        return 0;
    } else if (mensagem == WM_KEYDOWN && wparam == VK_ESCAPE) {
        DestroyWindow(janela);
        return 0;
    } else if (mensagem == WM_KEYDOWN && estado != NULL && estado->ao_evento != NULL &&
               wparam == VK_TAB) {
        SefEventoJanela evento = {0};
        evento.tipo = SEF_EVENTO_TAB;
        evento.modificador_shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (estado->ao_evento(&evento, estado->dados))
            InvalidateRect(janela, NULL, FALSE);
        return 0;
    } else if ((mensagem == WM_MOUSEMOVE || mensagem == WM_LBUTTONDOWN ||
                mensagem == WM_LBUTTONUP) &&
               estado != NULL && estado->ao_evento != NULL) {
        SefEventoJanela evento = {0};
        evento.tipo = mensagem == WM_MOUSEMOVE     ? SEF_EVENTO_PONTEIRO_MOVER
                      : mensagem == WM_LBUTTONDOWN ? SEF_EVENTO_PONTEIRO_PRESSIONAR
                                                   : SEF_EVENTO_PONTEIRO_SOLTAR;
        evento.x = (int)(short)(lparam & 0xffff);
        evento.y = (int)(short)((lparam >> 16) & 0xffff);
        if (estado->ao_evento(&evento, estado->dados))
            InvalidateRect(janela, NULL, FALSE);
        return 0;
    } else if (mensagem == WM_CHAR && estado != NULL && estado->ao_evento != NULL) {
        SefEventoJanela evento = {0};
        if (wparam == '\r')
            evento.tipo = SEF_EVENTO_ENTER;
        else if (wparam == '\b')
            evento.tipo = SEF_EVENTO_APAGAR;
        else {
            wchar_t caractere[2] = {(wchar_t)wparam, 0};
            evento.tipo = SEF_EVENTO_TEXTO;
            WideCharToMultiByte(CP_UTF8, 0, caractere, 1, evento.texto_utf8,
                                (int)sizeof(evento.texto_utf8) - 1, NULL, NULL);
        }
        if (estado->ao_evento(&evento, estado->dados)) {
            InvalidateRect(janela, NULL, FALSE);
        }
        return 0;
    } else if (mensagem == WM_DESTROY) {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(janela, mensagem, wparam, lparam);
}

int sef_janela_executar(const SefConfigJanela *configuracao, SefAoDesenhar ao_desenhar,
                        SefAoEvento ao_evento, void *dados, char *mensagem_erro,
                        int capacidade_erro) {
    HINSTANCE instancia = GetModuleHandleW(NULL);
    const wchar_t *classe = L"SefirahJanelaRaster";
    WNDCLASSW descricao = {0};
    descricao.lpfnWndProc = procedimento;
    descricao.hInstance = instancia;
    descricao.lpszClassName = classe;
    descricao.hCursor = LoadCursorW(NULL, MAKEINTRESOURCEW(32512));
    if (!RegisterClassW(&descricao) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        snprintf(mensagem_erro, (size_t)capacidade_erro, "falha ao registrar janela Win32");
        return 1;
    }

    EstadoJanela estado = {0};
    estado.ao_desenhar = ao_desenhar;
    estado.ao_evento = ao_evento;
    estado.dados = dados;
    if (!sef_superficie_criar(&estado.superficie, configuracao->largura, configuracao->altura))
        return 1;

    wchar_t titulo[256];
    MultiByteToWideChar(CP_UTF8, 0, configuracao->titulo, -1, titulo,
                        (int)(sizeof(titulo) / sizeof(titulo[0])));
    HWND janela = CreateWindowExW(0, classe, titulo, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                  CW_USEDEFAULT, configuracao->largura, configuracao->altura, NULL,
                                  NULL, instancia, &estado);
    if (janela == NULL) {
        sef_superficie_destruir(&estado.superficie);
        snprintf(mensagem_erro, (size_t)capacidade_erro, "falha ao criar janela Win32");
        return 1;
    }
    ShowWindow(janela, SW_SHOW);
    MSG mensagem;
    while (GetMessageW(&mensagem, NULL, 0, 0) > 0) {
        TranslateMessage(&mensagem);
        DispatchMessageW(&mensagem);
    }
    sef_superficie_destruir(&estado.superficie);
    return 0;
}
