#ifndef ENCODING_H
#define ENCODING_H

#include "unicaseconv.h"
#include "nodedefs.h"

#if CODEPAGE850
unsigned char ISO8859toCodePage850(unsigned char kar);
unsigned char CodePage850toISO8859(unsigned char kar);
int strcasecmpDOS(const char* s, const char* p);
#endif

psk changeCase(psk Pnode
#if CODEPAGE850
               , int dos
#endif
               , int low);
unsigned char* putCodePoint(ULONG val, unsigned char* s);
int getCodePoint(const char** ps);
int hasUTF8MultiByteCharacters(const char* s);
int getCodePoint2(const char** ps, int* isutf);
int strcasecompu(const char* S, const char* P, const char* cutoff);
int strcasecomp(const char* s, const char* p);

#endif
