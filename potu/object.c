#include "object.h"
#include "objectnode.h"
#include "equal.h"
#include "variables.h"
#include "builtinmethod.h"

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

