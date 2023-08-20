#include "copy.h"
#include "nodedefs.h"
#include "memory.h"
#include "objectnode.h"
#include "numbercheck.h"
#include <string.h>
#include <assert.h>


psk iCopyOf(psk pnode)
    {
    /* REQUIREMENTS : After the string delimiting 0 all remaining bytes in the
    current computer word must be 0 as well.
    Argument must start on a word boundary. */
    psk ret;
    size_t len;
    len = sizeof(ULONG) + strlen((char*)POBJ(pnode));
    ret = (psk)bmalloc(__LINE__, len + 1);
#if ICPY
    MEMCPY(ret, pnode, (len >> LOGWORDLENGTH) + 1);
#else
    MEMCPY(ret, pnode, ((len / sizeof(LONG)) + 1) * sizeof(LONG));
#endif
    ret->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
    return ret;
    }

psk copyof(psk pnode)
    {
    psk res;
    res = iCopyOf(pnode);
    res->v.fl &= ~IDENT;
    return res;
    }

psk new_operator_like(psk pnode)
    {
    if(Op(pnode) == EQUALS)
        {
        objectnode* goal;
        assert(!ISBUILTIN((objectnode*)pnode));
        goal = (objectnode*)bmalloc(__LINE__, sizeof(objectnode));
#if WORD32
        goal->u.Int = 0;
#else
        goal->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
#endif
        return (psk)goal;
        }
    else
        return (psk)bmalloc(__LINE__, sizeof(knode));
    }

psk scopy(const char* str)
    {
    int nr = fullnumbercheck(str) & ~DEFINITELYNONUMBER;
    psk pnode;
    if(nr & MINUS)
        { /* bracmat out$arg$() -123 */
        pnode = (psk)bmalloc(__LINE__, sizeof(ULONG) + strlen((const char*)str));
        strcpy((char*)(pnode)+sizeof(ULONG), str + 1);
        }
    else
        {
        pnode = (psk)bmalloc(__LINE__, sizeof(ULONG) + 1 + strlen((const char*)str));
        strcpy((char*)(pnode)+sizeof(ULONG), str);
        }
    pnode->v.fl = READY | SUCCESS | nr;
    return pnode;
    }

psk subtreecopy(psk src);

psk same_as_w(psk pnode)
    {
    if(shared(pnode) != ALL_REFCOUNT_BITS_SET)
        {
        (pnode)->v.fl += ONEREF;
        return pnode;
        }
#if WORD32
    else if(is_object(pnode))
        {
        INCREFCOUNT(pnode);
        return pnode;
        }
#endif
    else
        {
        return subtreecopy(pnode);
        }
    }

static psk same_as_w_2(ppsk PPnode)
    {
    psk pnode = *PPnode;
    if(shared(pnode) != ALL_REFCOUNT_BITS_SET)
        {
        pnode->v.fl += ONEREF;
        return pnode;
        }
#if WORD32
    else if(is_object(pnode))
        {
        INCREFCOUNT(pnode);
        return pnode;
        }
#endif
    else
        {
        /*
        0:?n&:?L&whl'(!n+1:?n:<10000&out$!n&XXX !L:?L)
        0:?n&:?L&whl'(!n+1:?n:<10000&out$!n&!L XXX:?L) This is not improved!
        */
        *PPnode = subtreecopy(pnode);
        return pnode;
        }
    }


psk _copyop(psk Pnode)
    {
    psk apnode;
    apnode = new_operator_like(Pnode);
    apnode->v.fl = Pnode->v.fl & COPYFILTER;/* (ALL_REFCOUNT_BITS_SET | CREATEDWITHNEW);*/
    apnode->LEFT = same_as_w_2(&Pnode->LEFT);
    apnode->RIGHT = same_as_w(Pnode->RIGHT);
    return apnode;
    }

psk subtreecopy(psk src)
    {
    if(is_op(src))
        return _copyop(src);
    else
        return iCopyOf(src);
    }

psk isolated(psk Pnode)
    {
    if(shared(Pnode))
        {
        dec_refcount(Pnode);
        return subtreecopy(Pnode);
        }
    return Pnode;
    }

psk setflgs(psk pokn, ULONG Flgs)
    {
    if((Flgs & BEQUEST) || !(Flgs & SUCCESS))
        {
        pokn = isolated(pokn);
        pokn->v.fl ^= ((Flgs & SUCCESS) ^ SUCCESS);
        pokn->v.fl |= (Flgs & BEQUEST);
        if(ANYNEGATION(Flgs))
            pokn->v.fl |= NOT;
        }
    return pokn;
    }

#if ICPY
void icpy(LONG* d, LONG* b, int words)
    {
    while(words--)
        *d++ = *b++;
    }
#endif

psk copyop(psk Pnode)
    {
    dec_refcount(Pnode);
    return _copyop(Pnode);
    }

psk charcopy(const char* strt, const char* until)
    {
    int  nr = 0;
    psk pnode;
    if('0' <= *strt && *strt <= '9')
        {
        nr = QNUMBER BITWISE_OR_SELFMATCHING;
        if(*strt == '0')
            nr |= QNUL;
        }
    pnode = (psk)bmalloc(__LINE__, sizeof(ULONG) + 1 + (until - strt));
    strncpy((char*)(pnode)+sizeof(ULONG), strt, until - strt);
    pnode->v.fl = READY | SUCCESS | nr;
    return pnode;
    }

