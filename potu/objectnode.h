#ifndef OBJECTNODE_H
#define OBJECTNODE_H

#include "flags.h"
#include "nodestruct.h"

#if WORD32
#define BUILTIN (1 << 30)
typedef struct objectnode /* createdWithNew == 0 */
    {
    tFlags v;
    psk left, right;
    union
        {
        struct
            {
            unsigned int refcount : 30;
            unsigned int built_in : 1;
            unsigned int createdWithNew : 1;
            } s;
        int Int : 32;
        } u;
    } objectnode;
#else
typedef struct objectnode /* createdWithNew == 0 */
    {
    tFlags v;
    psk left, right;
    } objectnode;
#endif

#if WORD32
#define INCREFCOUNT(a) { ((objectnode*)a)->u.s.refcount++;(a)->v.fl &= ((~ALL_REFCOUNT_BITS_SET)|ONEREF); }
#define DECREFCOUNT(a) { ((objectnode*)a)->u.s.refcount--;(a)->v.fl |= ALL_REFCOUNT_BITS_SET; }
#define REFCOUNTNONZERO(a) ((a)->u.s.refcount)
#define ISBUILTIN(a) ((a)->u.s.built_in)
#define ISCREATEDWITHNEW(a) ((a)->u.s.createdWithNew)
#define SETCREATEDWITHNEW(a) (a)->u.s.createdWithNew = 1
#else
#define ISBUILTIN(a) ((a)->v.fl & BUILT_IN)
#define ISCREATEDWITHNEW(a) ((a)->v.fl & CREATEDWITHNEW)
#define SETCREATEDWITHNEW(a) (a)->v.fl |= CREATEDWITHNEW
#endif

#endif