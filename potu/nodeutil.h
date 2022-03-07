#ifndef NODEUTIL_H
#define NODEUTIL_H
#include "nodestruct.h"
int atomtest(psk pnode);
LONG toLong(psk pnode);
int number_degree(psk pnode);
int is_constant(psk pnode);
psk * backbone(psk arg, psk Pnode, psk * pfirst);
void cleanOncePattern(psk pat);
psk rightoperand(psk Pnode);
psk rightoperand_and_tail(psk Pnode, ppsk head, ppsk tail);
psk leftoperand_and_tail(psk Pnode, ppsk head, ppsk tail);
void privatized(psk Pnode, psk plkn);


#endif
