#ifndef POSITION_H
#define POSITION_H

#include "defines01.h"
#include "matchstate.h"
#include "nonnodetypes.h"
#include "nodestruct.h"

char doPosition(matchstate s, psk pat, LONG pposition, size_t stringLength, psk expr
#if CUTOFFSUGGEST
    , char** mayMoveStartOfSubject
#endif
    , unsigned int op
);
#endif
