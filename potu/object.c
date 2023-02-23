#include "object.h"
#include "objectnode.h"
#include "equal.h"
#include "variables.h"
#include "builtinmethod.h"


psk getmember2(psk name, psk tree)
    {
    while(is_op(tree))
        {
        if(Op(tree) == EQUALS)
            {
            psk nname;

            if(Op(name) == DOT)
                nname = name->LEFT;
            else
                nname = name;

            if(equal(tree->LEFT, nname))
                return NULL;
            else if(nname == name)
                {
                return tree->RIGHT = Head(tree->RIGHT);
                }
            else
                {
                name = name->RIGHT;
                }
            }
        else
            {
            psk tmp;
            if((tmp = getmember2(name, tree->LEFT)) != NULL)
                {
                return tmp;
                }
            }
        tree = tree->RIGHT;
        }
    return NULL;
    }

#if 1

psk getmember(psk name, psk tree, objectStuff* Object)
    {
    /* Returns NULL if either the method is not found or if the method is
       built-in. In the latter case, Object->theMethod is set. */
    while(is_op(tree))
        {
        if(Op(tree) == EQUALS)
            {
            psk nname;
            if(ISBUILTIN((objectnode*)tree)
               && Op(name) == DOT
               )
                {
                Object->object = tree;  /* object == (=) */
                Object->theMethod = findBuiltInMethod((typedObjectnode*)tree, name->RIGHT);
                /* findBuiltInMethod((=),(insert)) */
                if(Object->theMethod)
                    {
                    return NULL;
                    }
                }

            if(Op(name) == DOT)
                nname = name->LEFT;
            else
                nname = name;
            if(equal(tree->LEFT, nname))
                return NULL;
            else if(nname == name)
                {
                return tree->RIGHT = Head(tree->RIGHT);
                }
            else
                {
                Object->self = tree->RIGHT;
                name = name->RIGHT;
                }
            }
        else
            {
            psk tmp;
            if((tmp = getmember(name, tree->LEFT, Object)) != NULL)
                {
                return tmp;
                }
            }
        tree = tree->RIGHT;
        }
    return NULL;
    }

#else
psk getmember(psk name, psk tree, objectStuff * Object)
    {
    while (is_op(tree))
        {
        if (Op(tree) == EQUALS)
            {
            psk nname;
            if (Object
                && ISBUILTIN((objectnode*)tree)
                && Op(name) == DOT
                )
                {
                Object->object = tree;  /* object == (=) */
                Object->theMethod = findBuiltInMethod((typedObjectnode *)tree, name->RIGHT);
                /* findBuiltInMethod((=),(insert)) */
                if (Object->theMethod)
                    {
                    return NULL;
                    }
                }

            if (Op(name) == DOT)
                nname = name->LEFT;
            else
                nname = name;
            if (equal(tree->LEFT, nname))
                return NULL;
            else if (nname == name)
                {
                return tree->RIGHT = Head(tree->RIGHT);
                }
            else
                {
                if (Object)
                    Object->self = tree->RIGHT;
                name = name->RIGHT;
                }
            }
        else
            {
            psk tmp;
            if ((tmp = getmember(name, tree->LEFT, Object)) != NULL)
                {
                return tmp;
                }
            }
        tree = tree->RIGHT;
        }
    return NULL;
    }
#endif
