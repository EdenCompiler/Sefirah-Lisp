#include <stdint.h>

extern int64_t chamar_dobro(const int64_t *argumentos);

int64_t dobrar_i64(int64_t valor) { return (int64_t)((uint64_t)valor * 2u); }

int main(void) {
    int64_t argumento = 21;
    return chamar_dobro(&argumento) == 42 ? 0 : 1;
}
