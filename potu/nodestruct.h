#ifndef NODESTRUCT_H
#define NODESTRUCT_H

#include "defines01.h"
#include "platformdependentdefs.h"
#include "flags.h"

typedef union
    {
#ifndef NDEBUG
    struct
        {
        ULONG Not : 1; /* ~ */
        ULONG success : 1;
        ULONG ready : 1;
        ULONG position : 1; /* [ */

        ULONG indirect : 1; /* ! */
        ULONG doubly_indirect : 1; /* !! */
        ULONG fence : 1; /* `   (within same byte as ATOM and NOT) */
        ULONG atom : 1; /* @ */

        ULONG nonident : 1; /* % */
        ULONG greater_than : 1; /* > */
        ULONG smaller_than : 1; /* < */
        ULONG number : 1; /* # */

        ULONG breuk : 1; /* / */
        ULONG unify : 1; /* ? */
        ULONG ident : 1;
        ULONG impliedfence : 1;

        ULONG is_operator : 1;
        ULONG binop : 4; /* only if operator node*/
        /* EQUALS DOT COMMA OR AND MATCH WHITE PLUS TIMES EXP LOG DIF FUU FUN UNDERSCORE */
        ULONG latebind : 1;
#if WORD32
#else
        ULONG built_in : 1; /* only used for objects (operator =) */
        ULONG createdWithNew : 1; /* only used for objects (operator =) */
#endif
        ULONG        refcount : REF_COUNT_BITS;
        } node;
    struct
        {
        ULONG Not : 1; /* ~ */
        ULONG success : 1;
        ULONG ready : 1;
        ULONG position : 1; /* [ */

        ULONG indirect : 1; /* ! */
        ULONG doubly_indirect : 1; /* !! */
        ULONG fence : 1; /* `   (within same byte as ATOM and NOT) */
        ULONG atom : 1; /* @ */

        ULONG nonident : 1; /* % */
        ULONG greater_than : 1; /* > */
        ULONG smaller_than : 1; /* < */
        ULONG number : 1; /* # */

        ULONG breuk : 1; /* / */
        ULONG unify : 1; /* ? */
        ULONG ident : 1;
        ULONG impliedfence : 1;

        ULONG is_operator : 1;
        ULONG qgetal : 1; /* only if leaf */
        ULONG minus : 1; /* only if leaf */
        ULONG qnul : 1; /* only if leaf */
        ULONG qbreuk : 1; /* only if leaf */

        ULONG latebind : 1;
#if WORD32
#else
        ULONG built_in : 1; /* only used for objects (operator =) */
        ULONG createdWithNew : 1; /* only used for objects (operator =) */
#endif
        ULONG        refcount : REF_COUNT_BITS;
        } leaf;
#endif
    ULONG fl;
    } tFlags;


typedef struct sk
    {
    tFlags v;

    union
        {
        struct
            {
            struct sk *left, *right;
            } p;
        LONG lobj; /* This part of the structure can be used for comparisons
                      with short strings that fit into one word in one machine
                      operation, like "\0\0\0\0" or "1\0\0\0" */
        unsigned char obj;
        char sobj;
        } u;
    } sk;

typedef  sk * psk;
typedef psk * ppsk;

typedef struct knode
    {
    tFlags v;
    psk left, right;
    } knode;


typedef struct stringrefnode
    {
    tFlags v;
    psk pnode;
    char * str;
    size_t length;
    } stringrefnode;

typedef psk function_return_type;


#endif