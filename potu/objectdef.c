#include "objectdef.h"
#include "hashtypes.h"
#include "hash.h"
#include "nodedefs.h"
#include "memory.h"
#include "copy.h"
#include "objectnode.h"
#include "globals.h"
#include "head.h"
#include "binding.h"
#include "wipecopy.h"
#include <string.h>

static classdef classes[] = { {"hash",hash},{NULL,NULL} };

static int hasSubObject(psk src)
    {
    while (is_op(src))
        {
        if (Op(src) == EQUALS)
            return TRUE;
        else
            {
            if (hasSubObject(src->LEFT))
                return TRUE;
            src = src->RIGHT;
            }
        }
    return FALSE;
    }

static psk objectcopysub(psk src);

static psk objectcopysub2(psk src) /* src is NOT an object */
    {
    psk goal;
    if (is_op(src) && hasSubObject(src))
        {
        goal = (psk)bmalloc(__LINE__, sizeof(knode));
        goal->v.fl = src->v.fl & COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
        goal->LEFT = objectcopysub(src->LEFT);
        goal->RIGHT = objectcopysub(src->RIGHT);
        return goal;
        }
    else
        return same_as_w(src);
    }

static psk objectcopysub(psk src)
    {
    psk goal;
    if (is_object(src))
        {
        if (ISBUILTIN((objectnode*)src))
            {
            return same_as_w(src);
            }
        else
            {
            goal = (psk)bmalloc(__LINE__, sizeof(objectnode));
#if WORD32
            ((typedObjectnode*)goal)->u.Int = 0;
#else
            ((typedObjectnode*)goal)->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
#endif
            }
        goal->v.fl = src->v.fl & COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
        goal->LEFT = same_as_w(src->LEFT);
        goal->RIGHT = same_as_w(src->RIGHT);
        return goal;
        }
    else
        return objectcopysub2(src);
    }

static psk objectcopy(psk src)
    {
    psk goal;
    if (is_object(src))                              /* Make a copy of this '=' node ... */
        {
        if (ISBUILTIN((objectnode*)src))
            {
            goal = (psk)bmalloc(__LINE__, sizeof(typedObjectnode));
#if WORD32
            ((typedObjectnode*)goal)->u.Int = BUILTIN;
#else
            ((typedObjectnode*)goal)->v.fl |= BUILT_IN;
#endif
            ((typedObjectnode*)goal)->vtab = ((typedObjectnode*)src)->vtab;
            ((typedObjectnode*)goal)->voiddata = NULL;
            }
        else
            {
            goal = (psk)bmalloc(__LINE__, sizeof(objectnode));
#if WORD32
            ((typedObjectnode*)goal)->u.Int = 0;
#else
            ((typedObjectnode*)goal)->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
#endif
            }
        goal->v.fl = src->v.fl & COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
        goal->LEFT = same_as_w(src->LEFT);
        /*?? This adds an extra level of copying, but ONLY for objects that have a '=' node as the lhs of the main '=' node*/
        /* What is it good for? Bart 20010220 */
        goal->RIGHT = objectcopysub(src->RIGHT); /* and of all '=' child nodes (but not of grandchildren!) */
        return goal;
        }
    else
        return objectcopysub2(src);
    }


psk getObjectDef(psk src)
    {
    psk def;
    typedObjectnode* dest;
    if (!is_op(src))
        {
        classdef* df = classes;
        for (; df->name && strcmp(df->name, (char*)POBJ(src)); ++df)
            ;
        if (df->vtab)
            {
            dest = (typedObjectnode*)bmalloc(__LINE__, sizeof(typedObjectnode));
            dest->v.fl = EQUALS | SUCCESS;
            dest->left = same_as_w(&nilNode);
            dest->right = same_as_w(src);
#if WORD32
            dest->u.Int = BUILTIN;
#else
            dest->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
            dest->v.fl |= BUILT_IN;
#endif
            VOID(dest) = NULL;
            dest->vtab = df->vtab;
            return (psk)dest;
            }
        }
    else if (Op(src) == EQUALS)
        {
        src->RIGHT = Head(src->RIGHT);
        return objectcopy(src);
        }



    if ((def = SymbolBinding_w(src, src->v.fl & DOUBLY_INDIRECT)) != NULL)
        {
        dest = (typedObjectnode*)bmalloc(__LINE__, sizeof(typedObjectnode));
        dest->v.fl = EQUALS | SUCCESS;
        dest->left = same_as_w(&nilNode);
        dest->right = objectcopy(def); /* TODO Head(&def) ? */
        wipe(def);
#if WORD32
        dest->u.Int = 0;
#else
        dest->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
#endif
        VOID(dest) = NULL;
        dest->vtab = NULL;
        return (psk)dest;
        }
    return NULL;
    }
