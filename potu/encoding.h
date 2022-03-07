#ifndef ENCODING_H
#define ENCODING_H

#include "u2l-l2u.h"
#include "nodedefs.h"

int convertLetter(int a, struct ccaseconv * T);
psk changeCase(psk Pnode
#if CODEPAGE850
               , int dos
#endif
               , int low);
int toLowerUnicode(int a);
unsigned char * putCodePoint(ULONG val, unsigned char * s);
int getCodePoint(const char ** ps);
int hasUTF8MultiByteCharacters(const char * s);
int getCodePoint2(const char ** ps, int * isutf);
int strcasecompu(char ** S, char ** P, char * cutoff);
int strcasecomp(const char *s, const char *p);

#endif
