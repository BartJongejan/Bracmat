#include "globals.h"

#include "flags.h"
#include <stdio.h>
#include <stdarg.h>

int dummy_op = WHITE;
psk addr[7];
sk zeroNode, oneNode, minusOneNode,
nilNode, nilNodeNotNeutral,
zeroNodeNotNeutral,
oneNodeNotNeutral,
argNode, selfNode, SelfNode, minusTwoNode, twoNode, minusFourNode, fourNode, sjtNode;

FILE * global_fpi;
FILE * global_fpo;
int optab[256];

#if GLOBALARGPTR
va_list argptr;
#endif

/*#if !_BRACMATEMBEDDED*/
#if !defined NO_FOPEN
char * errorFileName = NULL;
#endif
FILE * errorStream = NULL;
/*#endif*/

int hum = 1;
int listWithName = 1;
int beNice = TRUE;
psk global_anchor;
size_t telling = 0;
unsigned char *source;

const psk knil[16] =
    { NULL,NULL,NULL,NULL,NULL,NULL,&nilNode,&zeroNode,
    &oneNode,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

#if !defined NO_FOPEN
char* targetPath = NULL; /* Path that can be prepended to filenames. */

int ARGC = 0;
char** ARGV = NULL;

psk m0 = NULL, m1 = NULL, f0 = NULL, f1 = NULL, f4 = NULL, f5 = NULL;


#endif
