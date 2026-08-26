#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "sefirah/janela.h"

#include <stdio.h>

typedef struct EstadoJanela {
    SefSuperficie superficie;
    SefAoDesenhar ao_desenhar;
    SefAoEvento ao_evento;
    void *dados;
    HWND janela;
    bool executando;
} EstadoJanela;

static EstadoJanela *janela_ativa = NULL;

static LRESULT CALLBACK procedimento(HWND janela, UINT mensagem, WPARAM wparam, LPARAM lparam) {
    EstadoJanela *estado = (EstadoJanela *)GetWindowLongPtrW(janela, GWLP_USERDATA);
    if (mensagem == WM_NCCREATE) {
        CREATESTRUCTW *criacao = (CREATESTRUCTW *)lparam;
        estado = (EstadoJanela *)criacao->lpCreateParams;
        estado->janela = janela;
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
    } else if (mensagem == WM_KEYDOWN && wparam == VK_ESCAPE && estado != NULL &&
               estado->ao_evento != NULL) {
        SefEventoJanela evento = {0};
        evento.tipo = SEF_EVENTO_CANCELAR;
        if (estado->ao_evento(&evento, estado->dados))
            InvalidateRect(janela, NULL, FALSE);
        return 0;
    } else if (mensagem == WM_KEYDOWN && wparam == VK_ESCAPE) {
        DestroyWindow(janela);
        return 0;
    } else if (mensagem == WM_KEYDOWN && estado != NULL && estado->ao_evento != NULL &&
               (wparam == VK_DELETE || wparam == VK_F5 || wparam == VK_F6 || wparam == VK_F7 ||
                wparam == VK_F8 || wparam == VK_F9 || wparam == VK_F10 || wparam == VK_F11 ||
                wparam == VK_F12 ||
                ((GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
                 (wparam == VK_RETURN || wparam == 'S' || wparam == 'O' || wparam == 'P' ||
                  wparam == 'T' || wparam == 'F' || wparam == 'G' || wparam == 'N' ||
                  wparam == 'W' || wparam == 'A' || wparam == 'Z' || wparam == 'Y')))) {
        SefEventoJanela evento = {0};
        evento.modificador_shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (wparam == VK_DELETE)
            evento.tipo = SEF_EVENTO_APAGAR_ADIANTE;
        else if (wparam == 'S')
            evento.tipo = SEF_EVENTO_SALVAR;
        else if (wparam == 'O')
            evento.tipo = SEF_EVENTO_ABRIR;
        else if (wparam == 'P')
            evento.tipo =
                evento.modificador_shift ? SEF_EVENTO_PALETA_COMANDOS : SEF_EVENTO_ABRIR_RAPIDO;
        else if (wparam == 'T')
            evento.tipo = SEF_EVENTO_BUSCAR_SIMBOLOS;
        else if (wparam == 'F')
            evento.tipo = SEF_EVENTO_BUSCAR_EDITOR;
        else if (wparam == 'G')
            evento.tipo = SEF_EVENTO_IR_PARA_LINHA;
        else if (wparam == 'N')
            evento.tipo = SEF_EVENTO_NOVO_DOCUMENTO;
        else if (wparam == 'W')
            evento.tipo = SEF_EVENTO_FECHAR_DOCUMENTO;
        else if (wparam == 'A')
            evento.tipo = SEF_EVENTO_SELECIONAR_TUDO;
        else if (wparam == 'Z')
            evento.tipo = SEF_EVENTO_DESFAZER;
        else if (wparam == 'Y')
            evento.tipo = SEF_EVENTO_REFAZER;
        else if (wparam == VK_F6)
            evento.tipo = SEF_EVENTO_EXECUTAR_FORMA;
        else if (wparam == VK_F7)
            evento.tipo = SEF_EVENTO_EXECUTAR_ALTERACOES;
        else if (wparam == VK_F8)
            evento.tipo = SEF_EVENTO_NAVEGAR_DEFINICAO;
        else if (wparam == VK_F9)
            evento.tipo = SEF_EVENTO_SALVAR_IMAGEM;
        else if (wparam == VK_F10)
            evento.tipo = SEF_EVENTO_RESTAURAR_IMAGEM;
        else if (wparam == VK_F11)
            evento.tipo = SEF_EVENTO_IR_PARA_DEFINICAO;
        else if (wparam == VK_F12)
            evento.tipo = SEF_EVENTO_NAVEGAR_REFERENCIA;
        else
            evento.tipo = SEF_EVENTO_EXECUTAR;
        if (estado->ao_evento(&evento, estado->dados))
            InvalidateRect(janela, NULL, FALSE);
        return 0;
    } else if (mensagem == WM_KEYDOWN && estado != NULL && estado->ao_evento != NULL &&
               (wparam == VK_LEFT || wparam == VK_RIGHT || wparam == VK_UP || wparam == VK_DOWN ||
                wparam == VK_HOME || wparam == VK_END)) {
        SefEventoJanela evento = {0};
        evento.modificador_shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        evento.modificador_controle = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (wparam == VK_LEFT)
            evento.tipo = SEF_EVENTO_CURSOR_ESQUERDA;
        else if (wparam == VK_RIGHT)
            evento.tipo = SEF_EVENTO_CURSOR_DIREITA;
        else if (wparam == VK_UP)
            evento.tipo = SEF_EVENTO_CURSOR_CIMA;
        else if (wparam == VK_DOWN)
            evento.tipo = SEF_EVENTO_CURSOR_BAIXO;
        else if (wparam == VK_HOME)
            evento.tipo = SEF_EVENTO_CURSOR_INICIO;
        else
            evento.tipo = SEF_EVENTO_CURSOR_FIM;
        if (estado->ao_evento(&evento, estado->dados))
            InvalidateRect(janela, NULL, FALSE);
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
        if (mensagem == WM_LBUTTONDOWN)
            SetCapture(janela);
        else if (mensagem == WM_LBUTTONUP && GetCapture() == janela)
            ReleaseCapture();
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
        evento.modificador_shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (wparam == '\r')
            evento.tipo = SEF_EVENTO_ENTER;
        else if (wparam == '\b')
            evento.tipo = SEF_EVENTO_APAGAR;
        else if (wparam >= 32) {
            wchar_t caractere[2] = {(wchar_t)wparam, 0};
            evento.tipo = SEF_EVENTO_TEXTO;
            WideCharToMultiByte(CP_UTF8, 0, caractere, 1, evento.texto_utf8,
                                (int)sizeof(evento.texto_utf8) - 1, NULL, NULL);
        } else
            return 0;
        if (estado->ao_evento(&evento, estado->dados)) {
            InvalidateRect(janela, NULL, FALSE);
        }
        return 0;
    } else if (mensagem == WM_DESTROY) {
        if (estado != NULL)
            estado->executando = false;
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(janela, mensagem, wparam, lparam);
}

bool sef_janela_processar_modal(SefEnquantoModal enquanto_modal, void *dados) {
    EstadoJanela *estado = janela_ativa;
    if (estado == NULL || enquanto_modal == NULL)
        return false;
    InvalidateRect(estado->janela, NULL, FALSE);
    MSG mensagem;
    while (estado->executando && enquanto_modal(dados)) {
        BOOL resultado = GetMessageW(&mensagem, NULL, 0, 0);
        if (resultado <= 0) {
            estado->executando = false;
            return false;
        }
        TranslateMessage(&mensagem);
        DispatchMessageW(&mensagem);
    }
    return estado->executando;
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
        snprintf(mensagem_erro, (size_t)capacidade_erro, "failed to register Win32 window");
        return 1;
    }

    EstadoJanela estado = {0};
    estado.ao_desenhar = ao_desenhar;
    estado.ao_evento = ao_evento;
    estado.dados = dados;
    estado.executando = true;
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
        snprintf(mensagem_erro, (size_t)capacidade_erro, "failed to create Win32 window");
        return 1;
    }
    ShowWindow(janela, SW_SHOW);
    janela_ativa = &estado;
    MSG mensagem;
    while (estado.executando && GetMessageW(&mensagem, NULL, 0, 0) > 0) {
        TranslateMessage(&mensagem);
        DispatchMessageW(&mensagem);
    }
    janela_ativa = NULL;
    sef_superficie_destruir(&estado.superficie);
    return 0;
}
