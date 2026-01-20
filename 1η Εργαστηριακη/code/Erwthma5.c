 /*
 * Erwthma 5 - A = B + C, store A, B, C in memory
 */

#include <stdint.h>

#define BASE_ADDR 0x00300000u

volatile uint32_t * const base = (volatile uint32_t *)BASE_ADDR;

void _start(void) {
    base[1] = 23;                  // B
    base[2] = 14;                  // C
    base[0] = base[1] + base[2];   // A (st? base[0])

    while (1) { }
}