#ifndef INPUT_H
#define INPUT_H

#include "nodestruct.h"
#include "nonnodetypes.h"
#include <stdio.h>

void tel(int c);
void glue(int c);
void tstr(int c);
void pstr(int c);
void init_opcode(void);
psk input(FILE * fpi, psk Pnode, int echmemvapstrmltrm, Boolean * err, Boolean * GoOn);
psk vbuildup(psk Pnode, const char *conc[]);
psk dopb(psk Pnode, psk src);
psk starttree_w(psk Pnode, ...);
psk build_up(psk Pnode, ...);
/*
void writeError(psk Pnode);
int redirectError(char* name);
*/
void stringEval(const char *s, const char ** out, int * err);

#endif