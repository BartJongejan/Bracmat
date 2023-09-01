#include "opt.h"
#include "nonnodetypes.h"

int search_opt(psk pnode, LONG opt)
    {
    while(is_op(pnode))
        {
        if(search_opt(pnode->LEFT, opt))
            return TRUE;
        pnode = pnode->RIGHT;
        }
    return PLOBJ(pnode) == opt;
    }
