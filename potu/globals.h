#ifndef GLOBALS_H
#define GLOBALS_H

#include "platformdependentdefs.h"
#include "nodestruct.h"
#include "nonnodetypes.h"
#include <stdio.h>
#include <stdarg.h>

extern int dummy_op;
extern psk addr[7];
extern sk zeroNode, oneNode, minusOneNode,
    nilNode, nilNodeNotNeutral,
    zeroNodeNotNeutral,
    oneNodeNotNeutral,
    argNode, selfNode, SelfNode, minusTwoNode, twoNode, minusFourNode, fourNode, sjtNode;

extern psk global_anchor;

extern FILE * global_fpi;
extern FILE * global_fpo;
extern size_t telling;

#if GLOBALARGPTR
extern va_list argptr;
#endif

/*#if !_BRACMATEMBEDDED*/
#if !defined NO_FOPEN
extern char * errorFileName;
#endif
extern FILE * errorStream;
/*#endif*/
extern int optab[256];

extern int hum;
extern int listWithName;
extern int beNice;

extern unsigned char *source;
extern const psk knil[16];

#if !defined NO_FOPEN
extern char* targetPath; /* Path that can be prepended to filenames. */
#endif

extern int ARGC;
extern char** ARGV;

extern psk m0, m1, f0, f1, f4, f5;

#endif