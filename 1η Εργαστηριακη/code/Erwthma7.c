/*
 * Erwthma 7 - y = sum_{k=1..10} (a_k * b_k)
 * Store y, a_k, b_k in consecutive memory locations
 */

#include <stdint.h>

#define BASE_ADDR 0x101F1000u

volatile uint32_t * const base = (volatile uint32_t *)BASE_ADDR;

void _start(void) {
    uint32_t k;

    // Example values (βάλε εδώ τις τιμές που θες/σου ζητάνε)
    uint32_t a[10] = {4,3,2,5,3,2,2,5,2,6};
    uint32_t b[10] = {5,2,6,2,3,2,1,4,2,3};

    // Αποθήκευση a_k σε base[1..10]
    for (k = 1; k <= 10; k++) {
        base[k] = a[k-1];
    }

    // Αποθήκευση b_k σε base[11..20]
    for (k = 1; k <= 10; k++) {
        base[10 + k] = b[k-1];
    }

    // Υπολογισμός y στο base[0]
    base[0] = 0;
    for (k = 1; k <= 10; k++) {
        base[0] += base[k] * base[10 + k];   // ΠΟΛΛΑΠΛΑΣΙΑΣΜΟΣ με *
    }

    while (1) { }
}
