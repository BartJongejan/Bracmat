#ifndef EVAL_H
#define EVAL_H

#include "nodestruct.h"
#include "nonnodetypes.h"

psk eval(psk Pnode);
#define evaluate(x) \
(    ( ((x) = eval(x))->v.fl \
     & SUCCESS\
     ) \
   ? TRUE\
   : ((x)->v.fl & FENCE)\
)

#endif