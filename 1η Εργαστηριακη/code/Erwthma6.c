/*
 * Erwthma 6 - y = sum_{k=1..10} k
 * Store y and k in consecutive memory locations
 * Also compute y2 with a different for-loop
 */

#include <stdint.h>

#define BASE_ADDR 0x101F1000u

volatile uint32_t * const base = (volatile uint32_t *)BASE_ADDR;

void _start(void) {
    uint32_t k;

    // Αποθήκευση k σε διαδοχικές θέσεις: base[1]..base[10]
    for (k = 1; k <= 10; k++) {
        base[k] = k;
    }

    // y1 στο base[0] (ανοδικός for)
    base[0] = 0;
    for (k = 1; k <= 10; k++) {
        base[0] += base[k];
    }

    // y2 στο base[11] (διαφορετικός for: καθοδικός)
    base[11] = 0;
    for (k = 10; k >= 1; k--) {
        base[11] += base[k];
    }

    while (1) { }
}
