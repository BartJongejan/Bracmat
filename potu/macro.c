#include "macro.h"
#include "eval.h"
#include "variables.h"
#include "nodedefs.h"
#include "nodeutil.h"
#include "wipecopy.h"
#include "copy.h"
#include "globals.h"
#include "memory.h"
#include "objectnode.h"
#include "writeerr.h"
#include "filewrite.h"
#include <assert.h>
psk evalmacro(psk Pnode)
    {
    if (!is_op(Pnode))
        {
        return NULL;
        }
    else
        {
        psk arg = Pnode;
        while (!(Pnode->v.fl & READY))
            {
            if (atomtest(Pnode->LEFT) != 0)
                {
                psk left = evalmacro(Pnode->LEFT);
                if (left != NULL)
                    {
                    /* copy backbone from evalmacro's argument to current Pnode
                       release lhs of copy of current and replace with 'left'
                       assign copy to 'Pnode'
                       evalmacro current, if not null, replace current
                       return current
                    */
                    psk ret;
                    psk first = NULL;
                    psk* last = backbone(arg, Pnode, &first);
                    wipe((*last)->LEFT);
                    (*last)->LEFT = left;
                    if (atomtest((*last)->LEFT) == 0 && Op((*last)) == FUN)
                        {
                        ret = evalmacro(*last);
                        if (ret)
                            {
                            wipe(*last);
                            *last = ret;
                            }
                        }
                    else
                        {
                        psk right = evalmacro((*last)->RIGHT);
                        if (right)
                            {
                            wipe((*last)->RIGHT);
                            (*last)->RIGHT = right;
                            }
                        }
                    return first;
                    }
                }
            else if (Op(Pnode) == FUN)
                {
                if (Op(Pnode->RIGHT) == UNDERSCORE)
                    {
                    int Flgs;
                    psk h;
                    psk hh;
                    psk first = NULL;
                    psk* last;
                    Flgs = Pnode->v.fl & (UNOPS | SUCCESS);
                    h = subtreecopy(Pnode->RIGHT);
                    if (dummy_op == EQUALS)
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
                    hh = evalmacro(h->LEFT);
                    if (hh)
                        {
                        wipe(h->LEFT);
                        h->LEFT = hh;
                        }
                    hh = evalmacro(h->RIGHT);
                    if (hh)
                        {
                        wipe(h->RIGHT);
                        h->RIGHT = hh;
                        }
                    last = backbone(arg, Pnode, &first);
                    wipe(*last);
                    *last = h;
                    return first;
                    }
                else if (Op(Pnode->RIGHT) == FUN
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
                    /* hh = evalmacro(h->LEFT);
                    if(hh)
                        {
                        wipe(h->LEFT);
                        h->LEFT = hh;
                        }*/
                    hh = evalmacro(h->RIGHT);
                    if (hh)
                        {
                        wipe(h->RIGHT);
                        h->RIGHT = hh;
                        }
                    last = backbone(arg, Pnode, &first);
                    wipe(*last);
                    *last = h;
                    return first;
                    }
                else
                    {
                    int newval = 0;
                    psk tmp = same_as_w(Pnode->RIGHT);
                    psk h;
                    tmp = eval(tmp);

                    if ((h = find2(tmp, &newval)) != NULL)
                        {
                        int Flgs;
                        psk first = NULL;
                        psk* last;
                        if ((Op(h) == EQUALS) && ISBUILTIN((objectnode*)h))
                            {
                            if (!newval)
                                h = same_as_w(h);
                            }
                        else
                            {
                            Flgs = Pnode->v.fl & (UNOPS);
                            if (!newval)
                                {
                                h = subtreecopy(h);
                                }
                            if (Flgs)
                                {
                                h->v.fl |= Flgs;
                                if (h->v.fl & INDIRECT)
                                    h->v.fl &= ~READY;
                                }
                            else if (h->v.fl & INDIRECT)
                                {
                                h->v.fl &= ~READY;
                                }
                            else if (Op(h) == EQUALS)
                                {
                                h->v.fl &= ~READY;
                                }
                            }

                        wipe(tmp);
                        last = backbone(arg, Pnode, &first);
                        wipe(*last);
                        *last = h;
                        return first;
                        }
                    else
                        {
                        errorprintf("\nmacro evaluation fails because rhs of $ operator is not bound to a value: "); writeError(Pnode); errorprintf("\n");
                        wipe(tmp);
                        return NULL;
                        }
                    }
                }
            Pnode = Pnode->RIGHT;
            if (!is_op(Pnode))
                {
                break;
                }
            }
        }
    return NULL;
    }


