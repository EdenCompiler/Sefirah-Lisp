#include <stdint.h>

#ifdef _WIN32
#define SEF_EXPORTAR __declspec(dllexport)
#else
#define SEF_EXPORTAR __attribute__((visibility("default")))
#endif

SEF_EXPORTAR int64_t dobrar_i64(int64_t valor) { return (int64_t)((uint64_t)valor * 2u); }

SEF_EXPORTAR int64_t combinar_i64(int64_t a, int64_t b) {
    return (int64_t)((uint64_t)a * 10u + (uint64_t)b);
}
