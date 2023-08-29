#ifndef TYPEDOBJECTNODE_H
#define TYPEDOBJECTNODE_H

#include "nonnodetypes.h"
#include "nodestruct.h"

struct typedObjectnode;
typedef Boolean(*method_pnt)(struct typedObjectnode* This, ppsk arg);

typedef struct method
    {
    char* name;
    method_pnt func;
    } method;

#if WORD32
typedef struct typedObjectnode /* createdWithNew == 1 */
    {
    tFlags v;
    psk left, right; /* left == nil, right == data (if vtab == NULL)
            or name of object type, e.g. [set], [hash], [file], [float] (if vtab != NULL)*/
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
    void* voiddata;
    method* vtab; /* The last element n of the array must have vtab[n].name == NULL */
    } typedObjectnode;
#else
typedef struct typedObjectnode /* createdWithNew == 1 */
    {
    tFlags v;
    psk left, right; /* left == nil, right == data (if vtab == NULL)
            or name of object type, e.g. [set], [hash], [file], [float] (if vtab != NULL)*/
    void* voiddata;
    method* vtab; /* The last element n of the array must have vtab[n].name == NULL */
    } typedObjectnode;
#endif

typedef struct
    {
    psk self;
    psk object;
    method_pnt theMethod;
    } objectStuff;

#endif
