#include "binding.h"
#include "variables.h"
#include "copy.h"
#include "eval.h"
#include "nonnodetypes.h"
#include "nodedefs.h"
#include "wipecopy.h"
#include <assert.h>

psk SymbolBinding_w(psk variabele, int twolevelsofindirection);

static psk SymbolBinding(psk variabele, int* newval, int twolevelsofindirection)
    {
    psk pbinding;
    if ((pbinding = find2(variabele, newval)) != NULL)
        {
        if (twolevelsofindirection)
            {
            psk peval;

            if (pbinding->v.fl & INDIRECT)
                {
                peval = subtreecopy(pbinding);
                peval = eval(peval);
                if (!isSUCCESS(peval)
                    || (is_op(peval)
                    && Op(peval) != EQUALS
                    && Op(peval) != DOT
                    )
                    )
                    {
                    wipe(peval);
                    return 0;
                    }
                if (*newval)
                    wipe(pbinding);
                *newval = TRUE;
                pbinding = peval;
                }
            if (is_op(pbinding))
                {
                if (is_object(pbinding))
                    {
                    peval = same_as_w(pbinding);
                    }
                else
                    {
                    peval = subtreecopy(pbinding);
                    /*
                    a=b=(c.d)
                    c=(d=e)
                    f:?!!(a.b)
                    !e
                    f
                    */
                    peval = eval(peval);
                    if (!isSUCCESS(peval)
                        || (is_op(peval)
                        && Op(peval) != EQUALS
                        && Op(peval) != DOT
                        )
                        )
                        {
                        wipe(peval);
                        if (*newval)
                            {
                            *newval = FALSE;
                            wipe(pbinding);
                            }
                        return NULL;
                        }
                    }
                assert(pbinding);
                if (*newval)
                    {
                    *newval = FALSE;
                    wipe(pbinding);
                    }
                pbinding = SymbolBinding(peval, newval, (peval->v.fl & DOUBLY_INDIRECT));
                wipe(peval);
                }
            else
                {
                int newv = *newval;
                psk binding;
                *newval = FALSE;
                binding = SymbolBinding(pbinding, newval, pbinding->v.fl & DOUBLY_INDIRECT);
                if (newv)
                    {
                    wipe(pbinding);
                    }
                pbinding = binding;
                }
            }
        }
    return pbinding;
    }

psk SymbolBinding_w(psk variabele, int twolevelsofindirection)
/* twolevelsofindirection because the variable not always can have the
bangs. Example:
  (A==B)  &  a b c:? [?!!A
first finds (=B), which is an object that should not obtain the flags !! as in
!!(=B), because that would have a side effect on A as A=!!(=B)
*/
    {
    psk pbinding;
    int newval;
    newval = FALSE;
    if ((pbinding = SymbolBinding(variabele, &newval, twolevelsofindirection)) != NULL)
        {
        ULONG nameflags, valueflags;
        nameflags = (variabele->v.fl & (BEQUEST | SUCCESS));
        if (ANYNEGATION(variabele->v.fl))
            nameflags |= NOT;

        valueflags = (pbinding)->v.fl;
        valueflags |= (nameflags & (BEQUEST | NOT));
        valueflags ^= ((nameflags & SUCCESS) ^ SUCCESS);

        assert(pbinding != NULL);

        if (Op(pbinding) == EQUALS)
            {
            if (!newval)
                {
                pbinding->RIGHT = Head(pbinding->RIGHT);
                pbinding = same_as_w(pbinding);
                }
            }
        else if ((pbinding)->v.fl == valueflags)
            {
            if (!newval)
                {
                pbinding = same_as_w(pbinding);
                }
            }
        else
            {
            assert(Op(pbinding) != EQUALS);
            if (newval)
                {
                pbinding = isolated(pbinding);
                }
            else
                {
                pbinding = subtreecopy(pbinding);
                }
            (pbinding)->v.fl = valueflags & COPYFILTER; /* ~ALL_REFCOUNT_BITS_SET;*/
            }
        }
    return pbinding;
    }
