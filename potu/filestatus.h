#ifndef FILESTATUS_H
#define FILESTATUS_H

#include "nodestruct.h"
#include "nonnodetypes.h"

int fil(ppsk PPnode);
int output(ppsk PPnode, void(*how)(psk k));

#if !defined NO_FOPEN
psk fileget(psk rlnode,int intval_i, psk Pnode, int * err, Boolean * GoOn);
#endif

#endif
