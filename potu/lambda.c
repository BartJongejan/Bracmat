#include "lambda.h"

#include "memory.h"
#include "nonnodetypes.h"
#include "nodedefs.h"
#include "typedobjectnode.h"
#include "nodestruct.h"
#include "objectnode.h"
#include "flags.h"
#include "globals.h"
#include "equal.h"
#include "nodeutil.h"
#include "wipecopy.h"
#include "copy.h"
#include <assert.h>


psk lambda(psk Pnode, psk name, psk Arg)
    {
    if(!is_op(Pnode))
        {
        return NULL;
        }
    else
        {
        psk arg = Pnode;
        while(!(Pnode->v.fl & READY))
            {
            if(atomtest(Pnode->LEFT) != 0)
                {
                psk left = lambda(Pnode->LEFT, name, Arg);
                if(left != NULL)
                    {
                    /* copy backbone from lambda's argument to current Pnode
                       release lhs of copy of current and replace with 'left'
                       assign copy to 'Pnode'
                       lambda current, if not null, replace current
                       return current
                    */
                    psk ret;
                    psk first = NULL;
                    psk* last = backbone(arg, Pnode, &first);
                    wipe((*last)->LEFT);
                    (*last)->LEFT = left;
                    if(atomtest((*last)->LEFT) == 0 && Op((*last)) == FUN)
                        {
                        ret = lambda(*last, name, Arg);
                        if(ret)
                            {
                            wipe(*last);
                            *last = ret;
                            }
                        }
                    else
                        {
                        psk right = lambda((*last)->RIGHT, name, Arg);
                        if(right)
                            {
                            wipe((*last)->RIGHT);
                            (*last)->RIGHT = right;
                            }
                        }
                    return first;
                    }
                }
            else if(Op(Pnode) == FUN)
                {
                if(Op(Pnode->RIGHT) == UNDERSCORE)
                    {
                    int Flgs;
                    psk h;
                    psk hh;
                    psk first = NULL;
                    psk* last;
                    Flgs = Pnode->v.fl & (UNOPS | SUCCESS);
                    h = subtreecopy(Pnode->RIGHT);
                    if(dummy_op == EQUALS)
                        {
                        psk becomes = (psk)bmalloc(__LINE__, sizeof(objectnode));
#if WORD32
                        ((typedObjectnode*)becomes)->u.Int = 0;
#else
                        ((typedObjectnode*)becomes)->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
#endif
                        becomes->LEFT = same_as_w(h->LEFT);
                        becomes->RIGHT = same_as_w(h->RIGHT);
                        wipe(h);
                        h = becomes;
                        }
                    h->v.fl = dummy_op | Flgs;
                    hh = lambda(h->LEFT, name, Arg);
                    if(hh)
                        {
                        wipe(h->LEFT);
                        h->LEFT = hh;
                        }
                    hh = lambda(h->RIGHT, name, Arg);
                    if(hh)
                        {
                        wipe(h->RIGHT);
                        h->RIGHT = hh;
                        }
                    last = backbone(arg, Pnode, &first);
                    wipe(*last);
                    *last = h;
                    return first;
                    }
                else if(Op(Pnode->RIGHT) == FUN
                        && atomtest(Pnode->RIGHT->LEFT) == 0
                        )
                    {
                    int Flgs;
                    psk h;
                    psk hh;
                    psk first = NULL;
                    psk* last;
                    Flgs = Pnode->v.fl & UNOPS;
                    h = subtreecopy(Pnode->RIGHT);
                    h->v.fl |= Flgs;
                    assert(atomtest(h->LEFT) == 0);
                    hh = lambda(h->RIGHT, name, Arg);
                    if(hh)
                        {
                        wipe(h->RIGHT);
                        h->RIGHT = hh;
                        }
                    last = backbone(arg, Pnode, &first);
                    wipe(*last);
                    *last = h;
                    return first;
                    }
                else if(!equal(name, Pnode->RIGHT))
                    {
                    psk h;
                    psk first;
                    psk* last;
                    h = subtreecopy(Arg);
                    if(h->v.fl & INDIRECT)
                        {
                        h->v.fl &= ~READY;
                        }
                    else if(is_op(h) && Op(h) == EQUALS)
                        {
                        h->v.fl &= ~READY;
                        }

                    last = backbone(arg, Pnode, &first);
                    wipe(*last);
                    *last = h;
                    return first;
                    }
                }
            else if(Op(Pnode) == FUU
                    && (Pnode->v.fl & FRACTION)
                    && Op(Pnode->RIGHT) == DOT
                    && !equal(name, Pnode->RIGHT->LEFT)
                    )
                {
                return NULL;
                }
            Pnode = Pnode->RIGHT;
            if(!is_op(Pnode))
                {
                break;
                }
            }
        }
    return NULL;
    }
