#include "branch.h"
#include "nodedefs.h"
#include "copy.h"
#include "wipecopy.h"
#include "memory.h"

psk _leftbranch(psk Pnode)
    {
    psk lnode;
    lnode = Pnode->LEFT;
    if (!(Pnode->v.fl & SUCCESS))
        {
        lnode = isolated(lnode);
        lnode->v.fl ^= SUCCESS;
        }
    if ((Pnode->v.fl & FENCE) && !(lnode->v.fl & FENCE))
        {
        lnode = isolated(lnode);
        lnode->v.fl |= FENCE;
        }
    wipe(Pnode->RIGHT);
    return lnode;
    }

psk leftbranch(psk Pnode)
    {
    psk lnode = _leftbranch(Pnode);
    pskfree(Pnode);
    return lnode;
    }

psk _fleftbranch(psk Pnode)
    {
    psk lnode;
    lnode = Pnode->LEFT;
    if (Pnode->v.fl & SUCCESS)
        {
        lnode = isolated(lnode);
        lnode->v.fl ^= SUCCESS;
        }
    if ((Pnode->v.fl & FENCE) && !(lnode->v.fl & FENCE))
        {
        lnode = isolated(lnode);
        lnode->v.fl |= FENCE;
        }
    wipe(Pnode->RIGHT);
    return lnode;
    }

psk fleftbranch(psk Pnode)
    {
    psk lnode = _fleftbranch(Pnode);
    pskfree(Pnode);
    return lnode;
    }

psk _fenceleftbranch(psk Pnode)
    {
    psk lnode;
    lnode = Pnode->LEFT;
    if (!(Pnode->v.fl & SUCCESS))
        {
        lnode = isolated(lnode);
        lnode->v.fl ^= SUCCESS;
        }
    if (Pnode->v.fl & FENCE)
        {
        if (!(lnode->v.fl & FENCE))
            {
            lnode = isolated(lnode);
            lnode->v.fl |= FENCE;
            }
        }
    else if (lnode->v.fl & FENCE)
        {
        lnode = isolated(lnode);
        lnode->v.fl &= ~FENCE;
        }
    wipe(Pnode->RIGHT);
    return lnode;
    }

psk _rightbranch(psk Pnode)
    {
    psk rightnode;
    rightnode = Pnode->RIGHT;
    if (!(Pnode->v.fl & SUCCESS))
        {
        rightnode = isolated(rightnode);
        rightnode->v.fl ^= SUCCESS;
        }
    if ((Pnode->v.fl & FENCE) && !(rightnode->v.fl & FENCE))
        {
        rightnode = isolated(rightnode);
        rightnode->v.fl |= FENCE;
        }
    wipe(Pnode->LEFT);
    return rightnode;
    }

psk rightbranch(psk Pnode)
    {
    psk rightnode = _rightbranch(Pnode);
    pskfree(Pnode);
    return rightnode;
    }

psk __rightbranch(psk Pnode)
    {
    psk ret;
    int success = Pnode->v.fl & SUCCESS;
    if (shared(Pnode))
        {
        ret = same_as_w(Pnode->RIGHT);
        dec_refcount(Pnode);
        }
    else
        {
        ret = Pnode->RIGHT;
        wipe(Pnode->LEFT);
        pskfree(Pnode);
        }
    if (!success)
        {
        ret = isolated(ret);
        ret->v.fl ^= SUCCESS;
        }
    return ret;
    }