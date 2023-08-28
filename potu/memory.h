#ifndef MEMORY_H
#define MEMORY_H
#include "defines01.h"
#include "nodestruct.h"
#include "nnumber.h"
#include <stddef.h>

int init_memoryspace(void);
#if DOSUMCHECK
void* Bmalloc(const char* file, int lineno, size_t n);
#else
void* Bmalloc(size_t n);
#endif
void dec_refcount(psk pnode);
void bfree(void* p);
void pskfree(psk p);
int all_refcount_bits_set(psk pnode);
#if SHOWMAXALLOCATED
#if SHOWCURRENTLYALLOCATED
void bezetting(void);
#endif
void Bez(char draft[22]);
#endif
#endif
