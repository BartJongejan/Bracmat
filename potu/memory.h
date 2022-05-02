#ifndef MEMORY_H
#define MEMORY_H
#include "defines01.h"
#include "nodestruct.h"
#include "nnumber.h"
#include <stddef.h>
#if DOSUMCHECK
#else
#define bmalloc(LINENO,N) bmalloc(N)
#endif



int init_memoryspace(void);

void * bmalloc(int lineno, size_t n);
/*void wipe(psk top);*/
void dec_refcount(psk pnode);
void bfree(void *p);
void pskfree(psk p);
int all_refcount_bits_set(psk pnode);

#endif