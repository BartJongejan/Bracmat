#include "wipecopy.h"
#include "typedobjectnode.h"
#include "globals.h"
#include "nodedefs.h"
#include "objectnode.h"
#include "input.h"
#include "eval.h"
#include "memory.h"
#include "object.h"
#include "builtinmethod.h"
#include "copy.h"

void wipe(psk top)
    {
    while (!shared(top)) /* tail recursion optimisation; delete deep structures*/
        {
        psk pnode = NULL;
        if (is_object(top) && ISCREATEDWITHNEW((objectnode*)top))
            {
            addr[1] = top->RIGHT;
            pnode = build_up(pnode, "(((=\1).die)')", NULL);
            pnode = eval(pnode);
            wipe(pnode);
            if (ISBUILTIN((objectnode*)top))
                {
                method_pnt theMethod = findBuiltInMethodByName((typedObjectnode*)top, "Die");
                if (theMethod)
                    {
                    theMethod((struct typedObjectnode *)top, NULL);
                    }
                }
            }
        if (is_op(top))
            {
            wipe(top->LEFT);
            pnode = top;
            top = top->RIGHT;
            pskfree(pnode);
            }
        else
            {
            if (top->v.fl & LATEBIND)
                {
                wipe(((stringrefnode*)top)->pnode);
                }
            pskfree(top);
            return;
            }
        }
    dec_refcount(top);
    }



