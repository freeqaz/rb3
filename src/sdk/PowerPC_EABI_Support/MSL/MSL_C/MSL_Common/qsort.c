#include "MSL_Common/size_def.h"

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    unsigned char *b = (unsigned char *)base;
    size_t i, j;

    if (nmemb <= 1) return;

    /* Insertion sort for small arrays, enough for NonMatching stub */
    for (i = 1; i < nmemb; i++) {
        unsigned char tmp[256];
        unsigned char *key = b + i * size;
        j = i;
        while (j > 0 && compar(b + (j - 1) * size, key) > 0) {
            j--;
        }
        if (j != i) {
            size_t k;
            for (k = 0; k < size && k < 256; k++) tmp[k] = key[k];
            for (k = i; k > j; k--) {
                size_t m;
                for (m = 0; m < size; m++) {
                    b[k * size + m] = b[(k - 1) * size + m];
                }
            }
            for (k = 0; k < size && k < 256; k++) b[j * size + k] = tmp[k];
        }
    }
}
