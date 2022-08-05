#include "nodeutil.h"

#include "nodedefs.h"
#include "nonnodetypes.h"
#include "copy.h"
#include "wipecopy.h"
#include "memory.h"
#include <assert.h>

int atomtest(psk pnode)
    {
    return (!is_op(pnode) && !HAS_UNOPS(pnode)) ? (int)pnode->u.obj : -1;
    }

LONG toLong(psk pnode)
    {
    LONG res;
    res = (LONG)STRTOUL((char *)POBJ(pnode), (char **)NULL, 10);
    if (pnode->v.fl & MINUS)
        res = -res;
    return res;
    }

/* "1.2E300" * i * pi * e * pa * 1/7 -> 1/7*i*"1.2E300"*pi*e*pa */
/*                                      5   4  3        2  1 0  */

int number_degree(psk pnode)
    {
    if (REAL_COMP(pnode))
        return 3;
    if (RATIONAL_COMP(pnode))
        return 5;
    switch (PLOBJ(pnode))
        {
            case IM: return 4;
            case PI: return 2;
            case XX: return 1;
            default: return 0;
        }
    }

int is_constant(psk pnode)
    {
    while (is_op(pnode))
        {
        if (!is_constant(pnode->LEFT))
            return FALSE;
        pnode = pnode->RIGHT;
        }
    return number_degree(pnode);
    }

psk * backbone(psk arg, psk Pnode, psk * pfirst)
    {
    psk first = *pfirst = subtreecopy(arg);
    psk * plast = pfirst;
    while (arg != Pnode)
        {
        psk R = subtreecopy((*plast)->RIGHT);
        wipe((*plast)->RIGHT);
        (*plast)->RIGHT = R;
        plast = &((*plast)->RIGHT);
        arg = arg->RIGHT;
        }
    *pfirst = first;
    return plast;
    }

void cleanOncePattern(psk pat)
    {
    pat->v.fl &= ~IMPLIEDFENCE;
    if (is_op(pat))
        {
        cleanOncePattern(pat->LEFT);
        cleanOncePattern(pat->RIGHT);
        }
    }

psk rightoperand(psk Pnode)
    {
    psk temp;
    ULONG Sign;
    temp = (Pnode->RIGHT);
    return((Sign = Op(Pnode)) == Op(temp) &&
        (Sign == PLUS || Sign == TIMES || Sign == WHITE) ?
        temp->LEFT : temp);
    }

psk rightoperand_and_tail(psk Pnode, ppsk head, ppsk tail)
    {
    psk temp;
    assert(is_op(Pnode));
    temp = Pnode->RIGHT;
    if (Op(Pnode) == Op(temp))
        {
        *head = temp->LEFT;
        *tail = temp->RIGHT;
        }
    else
        {
        *head = temp;
        *tail = NULL;
        }
    return temp;
    }

psk leftoperand_and_tail(psk Pnode, ppsk head, ppsk tail)
    {
    psk temp;
    assert(is_op(Pnode));
    temp = Pnode->LEFT;
    if (Op(Pnode) == Op(temp))
        {
        *head = temp->LEFT;
        *tail = temp->RIGHT;
        }
    else
        {
        *head = temp;
        *tail = NULL;
        }
    return temp;
    }

void privatized(psk Pnode, psk plkn)
    {
    *plkn = *Pnode;
    if (shared(plkn))
        {
        dec_refcount(Pnode);
        plkn->LEFT = same_as_w(plkn->LEFT);
        plkn->RIGHT = same_as_w(plkn->RIGHT);
        }
    else
        pskfree(Pnode);
    }

