#ifndef VARIABLES_H
#define VARIABLES_H
#include "nodestruct.h"
#include "platformdependentdefs.h"
#include "typedobjectnode.h"

void lst(psk pnode);
psk Head(psk pnode);
int insert(psk name, psk pnode);
psk findsub(psk namenode);
psk find2(psk namenode, int* newval);
psk findMethod(psk namenode, objectStuff* Object);
int psh(psk name, psk pnode, psk dim);
int deleteNode(psk name);
void pop(psk pnode);
int copy_insert(psk name, psk pnode, psk cutoff);
void mmf(ppsk PPnode);
int update(psk name, psk pnode); /* name = tree with DOT in root */
function_return_type setIndex(psk Pnode);
void initVariables(void);
int icopy_insert(psk name, LONG number);
int string_copy_insert(psk name, psk pnode, char* str, char* cutoff);

#endif
