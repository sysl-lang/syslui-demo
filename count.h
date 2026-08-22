/* libc's heap with a counter in front of it, so a rebuild can be priced. */
#ifndef SYSL_UI_COUNT_H
#define SYSL_UI_COUNT_H

#include <stddef.h>

void *ui_alloc(size_t n);
void  ui_free(void *p);

unsigned long long ui_allocs(void);
unsigned long long ui_frees(void);

#endif
