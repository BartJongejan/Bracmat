#ifndef NODEDEFS_H
#define NODEDEFS_H

#include "flags.h"
#include "nodestruct.h"

#define RIGHT u.p.right
#define LEFT  u.p.left
#define OBJ(p) &((p).u.obj)
#define LOBJ(p) ((p).u.lobj)
#define POBJ(p) &((p)->u.obj)
#define SPOBJ(p) &((p)->u.sobj)
/*#define PIOBJ(p) ((p)->u.iobj)*/ /* Added. Bart 20031110 */
#define PLOBJ(p) ((p)->u.lobj)

#define Qnumber psk

#define QOBJ(p) &(p)
#define QPOBJ(p) p

#define FILTERS     (FRACTION | NUMBER | SMALLER_THAN | GREATER_THAN | ATOM | NONIDENT)
#define ATOMFILTERS (FRACTION | NUMBER | SMALLER_THAN | GREATER_THAN | ATOM | FENCE | IDENT)
#define SATOMFILTERS (/*ATOM | */FENCE | IDENT)

#define FLGS (FILTERS | FENCE | DOUBLY_INDIRECT | INDIRECT | POSITION)

#define NEGATION(Flgs,flag)  ((Flgs & NOT ) && \
                             (Flgs & FILTERS) >= (flag) && \
                             (Flgs & FILTERS) < ((flag) << 1))
#define ANYNEGATION(Flgs)  ((Flgs & NOT ) && (Flgs & FILTERS))
#define ASSERTIVE(Flgs,flag) ((Flgs & flag) && !NEGATION(Flgs,flag))
#define FAIL (pat->v.fl & NOT)
#define NOTHING(p) (((p)->v.fl & NOT) && !((p)->v.fl & FILTERS))
#define NOTHINGF(Flgs) ((Flgs & NOT) && !(Flgs & FILTERS))
#define BEQUEST (FILTERS | FENCE | UNIFY)
#define UNOPS (UNIFY | FLGS | NOT | MINUS)
#define HAS_UNOPS(a) ((a)->v.fl & UNOPS)
#define HAS__UNOPS(a) (is_op(a) && (a)->v.fl & (UNIFY | FLGS | NOT))
#define IS_VARIABLE(a) ((a)->v.fl & (UNIFY | INDIRECT | DOUBLY_INDIRECT))
#define IS_BANG_VARIABLE(a) ((a)->v.fl & (INDIRECT | DOUBLY_INDIRECT))

#define OPERATOR ((0xF<<OPSH) + IS_OPERATOR)

#define Op(pn) ((pn)->v.fl & OPERATOR)
#define kopo(pn) ((pn).v.fl & OPERATOR)
#define is_op(pn) ((pn)->v.fl & IS_OPERATOR)
#define is_object(pn) (((pn)->v.fl & OPERATOR) == EQUALS)
#define klopcode(pn) (Op(pn) >> OPSH)

#define nil(p) knil[klopcode(p)]



#define shared(pn) ((pn)->v.fl & ALL_REFCOUNT_BITS_SET)
#define currRefCount(pn) (((pn)->v.fl & ALL_REFCOUNT_BITS_SET) >> NON_REF_COUNT_BITS)


#endif