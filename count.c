/* The allocator `package.hocon` names, which is libc's with two counters.
 *
 * Nothing here is clever and that is the point: replacing the pair changes who
 * is asked for storage and changes nothing about where it comes from, so the
 * numbers this reports are the program's real allocation traffic rather than a
 * model of it.
 *
 * `ui_free(NULL)` is counted as nothing, because free(NULL) is defined and does
 * nothing -- counting it would make frees exceed allocations and say the program
 * had a double free. */

#include <stdlib.h>

#include "count.h"

static unsigned long long allocs = 0;
static unsigned long long frees  = 0;

void *ui_alloc(size_t n) {
    allocs++;
    return malloc(n);
}

void ui_free(void *p) {
    if (p == NULL) return;

    frees++;
    free(p);
}

unsigned long long ui_allocs(void) { return allocs; }
unsigned long long ui_frees(void)  { return frees; }
