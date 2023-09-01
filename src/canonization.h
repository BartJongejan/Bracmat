#ifndef CANONIZATION_H
#define CANONIZATION_H

#include "nodestruct.h"
#include "defines01.h"


psk evalvar(psk Pnode);
psk handleComma(psk Pnode);
psk handleWhitespace(psk Pnode);
psk merge
(psk Pnode
    , int(*comp)(psk, psk)
    , psk(*combine)(psk)
#if EXPAND
    , psk(*expand)(psk, int*)
#endif
);
int cmpplus(psk kn1, psk kn2);
psk mergeOrSortTerms(psk Pnode);
int cmptimes(psk kn1, psk kn2);
psk substtimes(psk Pnode);
psk handleExponents(psk Pnode);
psk substlog(psk Pnode);
psk substdiff(psk Pnode);






#endif
