#ifndef STRINGMATCH_H
#define STRINGMATCH_H

#include "defines01.h"
#include "platformdependentdefs.h"
#include "nodestruct.h"
#include <stddef.h>

#if CUTOFFSUGGEST
char stringmatch
(int ind
    , char* wh
    , char* sub
    , char* cutoff
    , psk pat
    , psk subkn
    , LONG pposition
    , size_t stringLength
    , char** suggestedCutOff
    , char** mayMoveStartOfSubject
);
#else
char stringmatch
(int ind
    , char* wh
    , unsigned char* sub
    , unsigned char* cutoff
    , psk pat
    , psk subkn
    , LONG pposition
    , size_t stringLength
);
#endif

#endif