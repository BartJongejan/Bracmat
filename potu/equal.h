#ifndef EQUAL_H
#define EQUAL_H

#include "nodestruct.h"
#include "defines01.h"

#if INTSCMP
int intscmp(LONG* s1, LONG* s2);
#endif

int equal(psk kn1, psk kn2);
int cmp(psk kn1, psk kn2);
int compare(psk s, psk p);
int scompare(char* s, char* cutoff, psk p
#if CUTOFFSUGGEST
    , char** suggestedCutOff
    , char** mayMoveStartOfSubject
#endif
);

#endif