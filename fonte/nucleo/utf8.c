#include "sefirah/interno.h"

bool sef_utf8_decodificar(const char *dados, size_t tamanho, size_t *consumidos, uint32_t *codigo) {
    if (dados == NULL || tamanho == 0 || consumidos == NULL || codigo == NULL)
        return false;
    const unsigned char *bytes = (const unsigned char *)dados;
    uint32_t resultado;
    size_t quantidade;
    if (bytes[0] < 0x80u) {
        resultado = bytes[0];
        quantidade = 1;
    } else if (bytes[0] >= 0xc2u && bytes[0] <= 0xdfu) {
        resultado = bytes[0] & 0x1fu;
        quantidade = 2;
    } else if (bytes[0] >= 0xe0u && bytes[0] <= 0xefu) {
        resultado = bytes[0] & 0x0fu;
        quantidade = 3;
    } else if (bytes[0] >= 0xf0u && bytes[0] <= 0xf4u) {
        resultado = bytes[0] & 0x07u;
        quantidade = 4;
    } else {
        return false;
    }
    if (quantidade > tamanho)
        return false;
    for (size_t i = 1; i < quantidade; i++) {
        if ((bytes[i] & 0xc0u) != 0x80u)
            return false;
        resultado = (resultado << 6u) | (bytes[i] & 0x3fu);
    }
    if ((quantidade == 3 && resultado < 0x800u) || (quantidade == 4 && resultado < 0x10000u) ||
        (resultado >= 0xd800u && resultado <= 0xdfffu) || resultado > 0x10ffffu)
        return false;
    *consumidos = quantidade;
    *codigo = resultado;
    return true;
}

size_t sef_utf8_codificar(uint32_t codigo, char saida[4]) {
    if (saida == NULL || (codigo >= 0xd800u && codigo <= 0xdfffu) || codigo > 0x10ffffu)
        return 0;
    if (codigo < 0x80u) {
        saida[0] = (char)codigo;
        return 1;
    }
    if (codigo < 0x800u) {
        saida[0] = (char)(0xc0u | (codigo >> 6u));
        saida[1] = (char)(0x80u | (codigo & 0x3fu));
        return 2;
    }
    if (codigo < 0x10000u) {
        saida[0] = (char)(0xe0u | (codigo >> 12u));
        saida[1] = (char)(0x80u | ((codigo >> 6u) & 0x3fu));
        saida[2] = (char)(0x80u | (codigo & 0x3fu));
        return 3;
    }
    saida[0] = (char)(0xf0u | (codigo >> 18u));
    saida[1] = (char)(0x80u | ((codigo >> 12u) & 0x3fu));
    saida[2] = (char)(0x80u | ((codigo >> 6u) & 0x3fu));
    saida[3] = (char)(0x80u | (codigo & 0x3fu));
    return 4;
}

size_t sef_utf8_quantidade(const char *dados, size_t tamanho, bool *valido) {
    size_t quantidade = 0;
    size_t deslocamento = 0;
    while (deslocamento < tamanho) {
        size_t consumidos;
        uint32_t codigo;
        if (!sef_utf8_decodificar(dados + deslocamento, tamanho - deslocamento, &consumidos,
                                  &codigo)) {
            if (valido != NULL)
                *valido = false;
            return quantidade;
        }
        deslocamento += consumidos;
        quantidade++;
    }
    if (valido != NULL)
        *valido = true;
    return quantidade;
}

bool sef_utf8_localizar(const char *dados, size_t tamanho, size_t indice, size_t *inicio,
                        size_t *comprimento, uint32_t *codigo) {
    size_t deslocamento = 0;
    for (size_t atual = 0; deslocamento < tamanho; atual++) {
        size_t consumidos;
        uint32_t encontrado;
        if (!sef_utf8_decodificar(dados + deslocamento, tamanho - deslocamento, &consumidos,
                                  &encontrado))
            return false;
        if (atual == indice) {
            if (inicio != NULL)
                *inicio = deslocamento;
            if (comprimento != NULL)
                *comprimento = consumidos;
            if (codigo != NULL)
                *codigo = encontrado;
            return true;
        }
        deslocamento += consumidos;
    }
    return false;
}
