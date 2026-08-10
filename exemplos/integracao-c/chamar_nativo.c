#include <stdint.h>
#include <stdio.h>

extern int64_t calcular_nativo(const int64_t *argumentos);

int main(void) {
    int64_t argumentos[2] = {10, 22};
    int64_t resultado = calcular_nativo(argumentos);
    printf("resultado nativo: %lld\n", (long long)resultado);
    return resultado == 42 ? 0 : 1;
}
