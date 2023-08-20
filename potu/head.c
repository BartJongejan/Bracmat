#include "head.h"
#include "nodedefs.h"
#include "copy.h"
#include "memory.h"
#include <assert.h>
#include <string.h>

void wipe(psk top);

void copyToCutoff(psk* ppnode, psk pnode, psk cutoff)
    {
    for(;;)
        {
        if(is_op(pnode))
            {
            if(pnode->RIGHT == cutoff)
                {
                *ppnode = same_as_w(pnode->LEFT);
                break;
                }
            else
                {
                psk p = new_operator_like(pnode);
                p->v.fl = pnode->v.fl & COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                p->LEFT = same_as_w(pnode->LEFT);
                *ppnode = p;
                ppnode = &(p->RIGHT);
                pnode = pnode->RIGHT;
                }
            }
        else
            {
            *ppnode = iCopyOf(pnode);
            break;
            }
        }
    }

psk Head(psk pnode)
    {
    if(pnode->v.fl & LATEBIND)
        {
        assert(!shared(pnode));
        if(is_op(pnode))
            {
            psk root = pnode;
            copyToCutoff(&pnode, root->LEFT, root->RIGHT);
            wipe(root);
            }
        else
            {
            stringrefnode* ps = (stringrefnode*)pnode;
            pnode = (psk)bmalloc(__LINE__, sizeof(ULONG) + 1 + ps->length);
            pnode->v.fl = (ps->v.fl & COPYFILTER /*~ALL_REFCOUNT_BITS_SET*/ & ~LATEBIND);
            strncpy((char*)(pnode)+sizeof(ULONG), (char*)ps->str, ps->length);
            wipe(ps->pnode);
            bfree(ps);
            }
        }
    return pnode;
    }
