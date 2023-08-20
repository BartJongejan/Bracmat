#include "position.h"
#include "nodedefs.h"
#include "matchstate.h"
#include "globals.h"
#include "nodeutil.h"
#include "copy.h"
#include "eval.h"
#include "wipecopy.h"
#include "binding.h"
#include "variables.h"

static LONG expressionLength(psk Pnode, unsigned int op)
    {
    if(!is_op(Pnode) && Pnode->u.lobj == knil[op >> OPSH]->u.lobj)
        return 0;
    else
        {
        LONG len = 1;
        while(Op(Pnode) == op)
            {
            ++len;
            Pnode = Pnode->RIGHT;
            }
        return len;
        }
    }

char doPosition(matchstate s, psk pat, LONG pposition, size_t stringLength, psk expr
#if CUTOFFSUGGEST
                , char** mayMoveStartOfSubject
#endif
                , unsigned int op
)
    {
    ULONG Flgs;
    psk name;
    LONG pos;
    Flgs = pat->v.fl;
#if CUTOFFSUGGEST
    if(((Flgs & (SUCCESS | VISIBLE_FLAGS_POS0 | IS_OPERATOR)) == (SUCCESS | QNUMBER))
       && mayMoveStartOfSubject
       && *mayMoveStartOfSubject != 0
       )
        {
        pos = toLong(pat); /* [20 */
        if(pos < 0)
            pos += (expr == NULL ? (LONG)stringLength : expr ? expressionLength(expr, op) : 0) + 1; /* [(20+-1*(!len+1)) -> `-7 */

        if(pposition < pos
           && (MORE_EQUAL(pat) || EQUAL(pat) || NOTLESSORMORE(pat))
           )
            {
            if((long)stringLength > pos)
                *mayMoveStartOfSubject += pos - pposition;
            s.c.rmr = FALSE; /* [20 */
            return s.c.rmr;
            }
        else if(pposition <= pos
                && MORE(pat)
                )
            {
            if((long)stringLength > pos)
                *mayMoveStartOfSubject += pos - pposition + 1;
            s.c.rmr = FALSE; /* [>5 */
            return s.c.rmr;
            }
        }
#endif
    Flgs = pat->v.fl & (UNIFY | INDIRECT | DOUBLY_INDIRECT);

    name = subtreecopy(pat);
    name->v.fl |= SUCCESS;
    if((Flgs & UNIFY) && (is_op(pat) || (Flgs & INDIRECT)))
        {
        name->v.fl &= ~VISIBLE_FLAGS;
        if(!is_op(name))
            name->v.fl |= READY;
        s.c.rmr = (char)evaluate(name) & TRUE;

        if(!(s.c.rmr))
            {
            wipe(name);
            return FALSE;
            }
        }
    else
        {
        s.c.rmr = (char)evaluate(name) & TRUE;

        if(!(s.c.rmr))
            {
            wipe(name);
            return FALSE;
            }

        Flgs = pat->v.fl & UNIFY;
        Flgs |= name->v.fl;
        }
    pat = name;
    if(Flgs & UNIFY)
        {
        if(is_op(pat)
           || pat->u.obj
           )
            {
            if(Flgs & INDIRECT)        /* ?! of ?!! */
                {
                psk loc;
                if((loc = SymbolBinding_w(pat, Flgs & DOUBLY_INDIRECT)) != NULL)
                    {
                    if(is_object(loc))
                        s.c.rmr = (char)icopy_insert(loc, pposition);
                    else
                        {
                        s.c.rmr = (char)evaluate(loc) & (TRUE | FENCE);
                        if(!icopy_insert(loc, pposition))
                            s.c.rmr = FALSE;
                        }
                    wipe(loc);
                    }
                else
                    s.c.rmr = FALSE;
                }
            else
                {
                s.c.rmr = (char)icopy_insert(pat, pposition); /* [?a */
                }
            }
        else
            s.c.rmr = TRUE;

        if(name)
            wipe(name); /* [?a */
        /*
          (   ( CharacterLength
              =   length c p q
                .     0:?length:?p
                    & @( !arg
                       :   ?
                           ( [!p %?c [?q
                           & utf$!c:?k
                           & !q:?p
                           & 1+!length:?length
                           & ~
                           )
                           ?
                       )
                  | !length
              )
            & CharacterLength$str$(a chu$1000 b chu$100000 c):5
          )
        */
        return (char)(ONCE | POSITION_ONCE | s.c.rmr);
        }

    if(((pat->v.fl & (SUCCESS | VISIBLE_FLAGS_POS0 | IS_OPERATOR)) == (SUCCESS | QNUMBER)))
        {
        pos = toLong(pat); /* [20 */
        if(pos < 0)
            pos += (expr == NULL ? (LONG)stringLength : expressionLength(expr, op)) + 1; /* [(20+-1*(!len+1)) -> `-7 */
        if(LESS(pat))
            { /* [<18 */
            if(pposition < pos)
                {
                s.c.rmr = TRUE;/* [<18 */
                }
            else
                {
                s.c.rmr = FALSE | POSITION_MAX_REACHED;
                }
            }
        else if(LESS_EQUAL(pat))
            {
            if(pposition < pos)
                {
                s.c.rmr = TRUE;
                }
            else if(pposition == pos)
                {
                s.c.rmr = TRUE | POSITION_MAX_REACHED;
                }
            else
                {
                s.c.rmr = FALSE | POSITION_MAX_REACHED;
                }
            }
        else if(MORE_EQUAL(pat))
            { /* [~<13 */
            if(pposition >= pos)
                {
                s.c.rmr = TRUE; /* [~<13 */
                }
            else
                {
                s.c.rmr = FALSE; /* [~<13 */
                }
            }
        else if(MORE(pat))
            { /* [>5 */
            if(pposition > pos)
                {
                s.c.rmr = TRUE; /* [>5 */
                }
            else
                {
                s.c.rmr = FALSE; /* [>5 */
                }
            }
        else if(UNEQUAL(pat) || LESSORMORE(pat))
            { /* [~13 */
            if(pposition != pos)
                {
                s.c.rmr = TRUE; /* [~13 */
                }
            else
                {
                s.c.rmr = FALSE; /* [~13 */
                }
            }
        else if(EQUAL(pat) || NOTLESSORMORE(pat))
            {
            if(pposition == pos)
                {
                s.c.rmr = TRUE | POSITION_MAX_REACHED; /* [20 */
                }
            else if(pposition > pos)
                {
                s.c.rmr = FALSE | POSITION_MAX_REACHED;
                }
            else
                s.c.rmr = FALSE; /* [20 */
            }
        }
    else
        {
        s.c.rmr = FALSE;
        }
    wipe(pat); /* [20 */
    s.c.rmr |= ONCE | POSITION_ONCE;
    return s.c.rmr;
    }

