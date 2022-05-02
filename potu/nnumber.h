#ifndef NNUMBER_H
#define NNUMBER_H

#include "platformdependentdefs.h"
/*#include <stdlib.h>*/

typedef struct nnumber
    {
    ptrdiff_t length;
    ptrdiff_t ilength;
    void * alloc;
    void * ialloc;
    LONG * inumber;
    char * number;
    size_t allocated;
    size_t iallocated;
    int sign; /* 0: positive, QNUL: zero, MINUS: negative number */
    } nnumber;

#endif
