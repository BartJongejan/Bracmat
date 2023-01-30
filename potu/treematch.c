#include "treematch.h"
#include "eval.h"
#include "nodedefs.h"
#include "variables.h"
#include "globals.h"
#include "copy.h"
#include "wipecopy.h"
#include "matchstate.h"
#include "position.h"
#include "numbercheck.h"
#include "binding.h"
#include "nodeutil.h"
#include "equal.h"
#include "objectnode.h"
#include "filewrite.h"
#include "stringmatch.h"
#include "result.h"
#include <assert.h>
#include <string.h>

/*#define SUBJECTNOTNIL(sub,pat) (is_op(sub) || HAS_UNOPS(sub) || (PIOBJ(sub) != PIOBJ(nil(pat))))*/
#define SUBJECTNOTNIL(sub,pat) (is_op(sub) || HAS_UNOPS(sub) || (PLOBJ(sub) != PLOBJ(nil(pat))))


/*
    ( Dogs and Cats are friends: ? [%(out$(!sjt SJT)&~) (|))&
    ( Dogs and Cats are friends: ? [%(out$(!sjt)&~) (|))&
*/
static char doEval(psk sub, psk cutoff, psk pat)
    {
    char ret;
    psk loc;
    psh(&sjtNode, &nilNode, NULL);
    copy_insert(&sjtNode, sub, cutoff);
    loc = subtreecopy(pat);
    loc->v.fl &= ~(POSITION | NONIDENT | IMPLIEDFENCE | ONCE);
    loc = eval(loc);
    deleteNode(&sjtNode);
    if (isSUCCESS(loc))
        {
        ret = (loc->v.fl & FENCE) ? (TRUE | ONCE) : TRUE;
        }
    else
        {
        ret = (loc->v.fl & FENCE) ? ONCE : FALSE;
        }
    wipe(loc);

    return ret;
    }

static int oncePattern(psk pat)
    {
    /*
    This function has a side effect: it sets a flag IMPLIEDFENCE in all
    pattern nodes that can be matched by at most one non-trivial list element
    (a nonzero term in a sum, a factor in a product that is not 1, or a
    nonempty word in a sentence. Because the function depends on ATOMFILTERS,
    the algorithm should be slightly different for normal matches and for
    string matches. Ideally, two flags should be reserved.
    */
    if (pat->v.fl & IMPLIEDFENCE)
        {
        return TRUE;
        }
    if ((pat->v.fl & ATOM) && NEGATION(pat->v.fl, ATOM))
        {
        return FALSE;
        }
    else if (pat->v.fl & ATOMFILTERS)
        {
        pat->v.fl |= IMPLIEDFENCE;
        return TRUE;
        }
    else if (IS_VARIABLE(pat)
        || NOTHING(pat)
        || (pat->v.fl & NONIDENT) /*{?} a b c:% c => a b c */
        )
        return FALSE;
    else if (!is_op(pat))
        {
        pat->v.fl |= IMPLIEDFENCE;
        return TRUE;
        }
    else
        switch (Op(pat))
            {
                case DOT:
                case COMMA:
                case EQUALS:
                case LOG:
                case DIF:
                    pat->v.fl |= IMPLIEDFENCE;
                    return TRUE;
                case OR:
                    if (oncePattern(pat->LEFT) && oncePattern(pat->RIGHT))
                        {
                        pat->v.fl |= IMPLIEDFENCE;
                        return TRUE;
                        }
                    break;
                case MATCH:
                    if (oncePattern(pat->LEFT) || oncePattern(pat->RIGHT))
                        {
                        pat->v.fl |= IMPLIEDFENCE;
                        return TRUE;
                        }
                    break;
                case AND:
                    if (oncePattern(pat->LEFT))
                        {
                        pat->v.fl |= IMPLIEDFENCE;
                        return TRUE;
                        }
                    break;
                default:
                    break;
            }
    return FALSE;
    }

char match(int ind, psk sub, psk pat, psk cutoff, LONG pposition, psk expr, unsigned int op)
    {
    /*
    s.c.lmr or s.c.rmr have three independent flags: TRUE/FALSE, ONCE and FENCE.

    TRUE/FALSE The success or failure of the match.

    ONCE       Unwillingness of the pattern to match longer substrings from the
               subject. Example:

    {?} a b c d:?x @?y d
    {!} a b c d
    {?} !y
    {!} c
    {?} !x
    {!} a b
               In fact, the pattern @?y first matches the empty string and then,
               after backtracking from the failing match of the last subpattern d,
               a single element from the string. Thereafter, when again
               backtracking, the subpattern @?y denies to even try to match a
               substring that is one element longer (two elements, in this example)
               and the subpattern preceding @?y is offered an enlarged substring
               from the subject, while @?y itself starts with the empty element.

               This flag is of importance for patterns with the space, + or
               * operator.
               The flag is turned on in patterns by the `@#/ flags and by operators
               other than space + * _ & : | = $ '
               The flag is turned off "after consumption", i.e. it does not
               percolate upwards through patterns with space + or * operators.

    (once=
      (p=?`Y)
    &   a b c d
      : ?X !p (d|?&(p=`?Z&foo:?Y)&~)
    & out$(X !X Y !Y Z !Z));


    (once=a b c d:?X (?|?) d & out$(X !X))
    (once=a b c d:?X (@|@) d & out$(X !X))
    (once=a b c d:?X (?|@) d & out$(X !X))
    (once=a b c d:?X (@|?) d & out$(X !X))
    (once=a b c d:?X (@|`) d & out$(X !X))
    (once=a b c d:?X (`|?) d & out$(X !X))
    (once=a b c d:?X (`c|?) d & out$(X !X))

    FENCE      Unwillingness of the subject to be matched by alternative patterns.
               Of importance for the | and : operators in a pattern.
               Is turned on by the ` prefix (whether or not in a pattern).
               Is turned off in pattern with space + * | or : operator.
               (With | and : operators this is only the case for the left operand,
               with the other operators this is the case for all except the last
               operand in a list.)
               */
    matchstate s;
    psk loc;
    ULONG Flgs;
    psk name = NULL;
    DBGSRC(Printf("%d%*smatch(", ind, ind, ""); results(sub, cutoff); Printf(":"); \
        result(pat); Printf(")"); Printf("\n");)
        if (is_op(sub))
            {
            if (Op(sub) == EQUALS)
                sub->RIGHT = Head(sub->RIGHT);

            if (sub->RIGHT == cutoff)
                return match(ind + 1, sub->LEFT, pat, NULL, pposition, expr, op);
            }
    s.i = (PRISTINE << SHIFT_LMR) + (PRISTINE << SHIFT_RMR);
    Flgs = pat->v.fl;
    if (Flgs & POSITION)
        {
        if (Flgs & NONIDENT)
            return doEval(sub, cutoff, pat);
        else if (cutoff || !(sub->v.fl & IDENT))
            return FALSE | ONCE | POSITION_ONCE;
        else
            return doPosition(s, pat, pposition, 0, expr
#if CUTOFFSUGGEST
            , 0
#endif
            , op
            );
        }
    if (!(((Flgs & NONIDENT) && (((sub->v.fl & IDENT) && 1) ^ NEGATION(Flgs, NONIDENT)))
        || ((Flgs & ATOM) && ((is_op(sub) && 1) ^ NEGATION(Flgs, ATOM)))
        || ((Flgs & FRACTION) && (!RAT_RAT(sub) ^ NEGATION(Flgs, FRACTION)))
        || ((Flgs & NUMBER) && (!RATIONAL_COMP(sub) ^ NEGATION(Flgs, NUMBER)))
        )
        )
        {
        if (IS_VARIABLE(pat))
            {
            int ok = TRUE;
            if (is_op(pat))
                {
                ULONG saveflgs = Flgs & VISIBLE_FLAGS;
                name = subtreecopy(pat);
                name->v.fl &= ~VISIBLE_FLAGS;
                name->v.fl |= SUCCESS;
                if ((s.c.rmr = (char)evaluate(name)) != TRUE)
                    ok = FALSE;
                if (Op(name) != EQUALS)
                    {
                    name = isolated(name);/*Is this needed? 20220913*/
                    /* Yes, it is. Otherwise a '?' flag is permanently attached to name. 20230130*/
                    name->v.fl |= saveflgs;
                    }
                pat = name;
                }
            if (ok)
                {
                if (Flgs & UNIFY)        /* ?  */
                    {
                    if (!NOTHING(pat) || is_op(sub) || (sub->u.obj))
                        {
                        if (is_op(pat)
                            || pat->u.obj
                            )
                            if (Flgs & INDIRECT)        /* ?! of ?!! */
                                {
                                if ((loc = SymbolBinding_w(pat, Flgs & DOUBLY_INDIRECT)) != NULL)
                                    {
                                    if (is_object(loc))
                                        s.c.rmr = (char)copy_insert(loc, sub, cutoff);
                                    else
                                        {
                                        s.c.rmr = (char)evaluate(loc);
                                        if (!copy_insert(loc, sub, cutoff))
                                            s.c.rmr = FALSE;
                                        /* Previously, s.c.rmr was not influenced by failure of copy_insert */

                                        }
                                    wipe(loc);
                                    }
                                else
                                    s.c.rmr = (char)NOTHING(pat);
                                }
                            else
                                {
                                s.c.rmr = (char)copy_insert(pat, sub, cutoff);
                                /* Previously, s.c.rmr was unconditionally set to TRUE */
                                }

                        else
                            s.c.rmr = TRUE;
                        }
                    /*
                     * else NOTHING(pat) && !is_op(sub) && !sub->u.obj
                     * which means   ~?[`][!][!]
                     */
                    }
                else if (Flgs & INDIRECT)        /* ! or !! */
                    {
                    if ((loc = SymbolBinding_w(pat, Flgs & DOUBLY_INDIRECT)) != NULL)
                        {
                        cleanOncePattern(loc);
                        s.c.rmr = (char)(match(ind + 1, sub, loc, cutoff, pposition, expr, op) ^ NOTHING(pat));
                        wipe(loc);
                        }
                    else
                        s.c.rmr = (char)NOTHING(pat);
                    }
                }
            }
        else
            {
#if DATAMATCHESITSELF
            if (pat == sub && (pat->v.fl & SELFMATCHING))
                {
                return TRUE | ONCE;
                }
#endif
            switch (Op(pat))
                {
                    case WHITE:
                    case PLUS:
                    case TIMES:
                        {
                        LONG locpos = pposition;
                        if (Op(pat) == WHITE)
                            {
                            if (sub == &zeroNode /*&& Op(pat) != PLUS*/)
                                {
                                sub = &zeroNodeNotNeutral;
                                locpos = 0;
                                }
                            else if (sub == &oneNode)
                                {
                                sub = &oneNodeNotNeutral;
                                locpos = 0;
                                }
                            }
                        else if (sub == &nilNode)
                            {
                            sub = &nilNodeNotNeutral;
                            locpos = 0;
                            }
                        else if (sub == &oneNode && Op(pat) == PLUS)
                            {
                            sub = &oneNodeNotNeutral;
                            locpos = 0;
                            }
                        /* Optimal sructure for this code:
                                    A0 (B A)* B0
                        S:P ::=
                        A       divisionPoint=S
                        B       leftResult=0(P):car(P)
                        C       while divisionPoint
                        D           if leftResult.success
                        E               rightResult=SR:cdr(P)
                        F           if(done)
                        G               return
                        H           SL,SR=shift_right divisionPoint
                        I           leftResult=SL:car(P)
                        J       if leftResult.success
                        K           rightResult=0(P):cdr(pat)
                        L       return

                        0(P)=nil(pat): nil(WHITE)="", nil(+)=0,nil(*)=1
                        In stringmatch, there is no need for L0; the empty string ""
                        is part of the string.
                        */
                        /* A    divisionPoint=S */
                        /* B    leftResult=0(P):car(P) */
                        /* C    while divisionPoint */
                        /* D        if leftResult.success */
                        /* E            rightResult=SR:cdr(P) */
                        /* F        if(done) */
                            /* done =  (1) full success */
                            /*      or (2) may not be shifted.
                               ad (2): In the first pass, a position
                               flag on car(P) counts as criterion for being done. */
                               /* In all but the first pass, the left and right
                                  results can indicate that the loop is done. */
                                  /* In all passes a position_max_reached on the
                                     rightResult indicates that the loop is done. */
                                     /* G            return */
                                         /* Return true if full success.
                                            Also return whether lhs experienced max position
                                            being reached. */
                                            /* Also return whether the pattern as a whole doesn't
                                               want longer subjects, which can be found out by
                                               looking at the pattern */
                                               /* or by looking at whether both lhs and rhs results
                                                  indicated this, in which case both sides must be
                                                  non-zero size subjects. */
                                                  /* POSITION_ONCE, on the other hand, requires zero size
                                                     subjects. */
                                                     /* Also return the fence flag, if present in rmr.
                                                        (This flag in lmr has no influence.)
                                                     */
                                                     /* H        SL,SR=shift_right divisionPoint */
                                                         /* SL = lhs divisionPoint S, SR = rhs divisionPoint S
                                                         */
                                                         /* I        leftResult=SL:car(P) */
                                                         /* J    if leftResult.success */
                                                         /* K        rightResult=0(P):cdr(pat) */
                                                         /* L    return */
                                                             /* Return true if full success.

                                                                Also return whether lhs experienced max position
                                                                being reached. */
                                                                /* Also return whether the pattern as a whole doesn't
                                                                   want longer subjects, which can be found out by
                                                                   looking at the pattern or by looking at whether */
                                                                   /* both lhs and rhs results indicated this.
                                                                      These come in two sorts: POSITION_ONCE requires */
                                                                      /* zero size subjects, ONCE requires non-zero size
                                                                         subjects. */
                                                                         /* Also return the fence flag, which can be found on
                                                                            the pattern or in the result of the lhs or the rhs.
                                                                            (Not necessary that both have this flag.)
                                                                         */
                                                                         /* end */
                        if (SUBJECTNOTNIL(sub, pat))                      /* A    divisionPoint=S */
                            loc = sub;
                        else
                            loc = NULL;
                        /* B    leftResult=0(P):car(P) */
                        s.c.lmr = (char)match(ind + 1, nil(pat), pat->LEFT  /* a*b+c*d:?+[1+?*[1*%@?q*?+?            (q = c) */
                            , NULL, pposition, expr       /* a b c d:? [1 (? [1 %@?q ?) ?          (q = b) */
                            , Op(pat));                /* a b c d:? [1  ? [1 %@?q ?  ?          (q = b) */
                        s.c.lmr &= ~ONCE;
                        while (loc)                                      /* C    while divisionPoint */
                            {
                            if (s.c.lmr & TRUE)                          /* D        if leftResult.success */
                                {                                       /* E            rightResult=SR:cdr(P) */
                                s.c.rmr = match(ind + 1, loc, pat->RIGHT, cutoff, locpos, expr, op);
                                if (!(s.c.lmr & ONCE))
                                    s.c.rmr &= ~ONCE;
                                }
                            if ((s.c.rmr & TRUE)                       /* F        if(1) full success */
                                || (s.c.lmr & (POSITION_ONCE                  /*     or (2) may not be shifted. In the first pass, a position flag on car(P) counts as criterion for being done. */
                                | ONCE
                                )
                                )                                          /* In all but the first pass, the left and right */
                                || (s.c.rmr & (ONCE                           /* results can indicate that the loop is done.   */
                                | POSITION_MAX_REACHED           /* In all passes a position_max_reached on the   */
                                )                               /* rightResult indicates that the loop is done.  */
                                )
                                )
                                {                                       /* G            return */
                                if (loc != sub)
                                    s.c.rmr &= ~POSITION_MAX_REACHED;           /* This flag is reason to stop increasing the position of the division any further, but it must not be signalled  back to the caller if the lhs is not nil ... */
                                s.c.rmr |= (char)(s.c.lmr & POSITION_MAX_REACHED);  /* ... unless it is the lhs that signals it. */
                                if (oncePattern(pat))                        /* Also return whether the pattern as a whole doesn't want longer subjects, which can be found out by looking at the pattern */
                                    {                                       /* For example,                            */
                                    s.c.rmr |= ONCE;                        /*     a b c d:`(?x ?y) (?z:c) ?           */
                                    s.c.rmr |= (char)(pat->v.fl & FENCE);   /* must fail and set x==nil, y==a and z==b */
                                    }
                                else if (!(s.c.lmr & ONCE))
                                    s.c.rmr &= ~ONCE;
                                DBGSRC(Printf("%d%*smatch(", ind, ind, ""); \
                                    results(sub, cutoff); Printf(":"); result(pat);)
#ifndef NDEBUG
                                    DBGSRC(printMatchState("EXIT-MID", s, pposition, 0);)
#endif
                                    DBGSRC(if (pat->v.fl & FRACTION) Printf("FRACTION "); \
                                    if (pat->v.fl & NUMBER) Printf("NUMBER "); \
                                        if (pat->v.fl & SMALLER_THAN)\
                                            Printf("SMALLER_THAN "); \
                                            if (pat->v.fl & GREATER_THAN)\
                                                Printf("GREATER_THAN "); \
                                                if (pat->v.fl & ATOM) Printf("ATOM "); \
                                                    if (pat->v.fl & FENCE) Printf("FENCE "); \
                                                        if (pat->v.fl & IDENT) Printf("IDENT"); \
                                                            Printf("\n");)
                                    return s.c.rmr ^ (char)NOTHING(pat);
                                }
                            /* H        SL,SR=shift_right divisionPoint */
                            if (Op(loc) == Op(pat)
                                && loc->RIGHT != cutoff
                                )
                                loc = loc->RIGHT;
                            else
                                loc = NULL;
                            /* SL = lhs divisionPoint S, SR = rhs divisionPoint S
                            */
                            ++locpos;
                            /* I        leftResult=SL:car(P) */
                            s.c.lmr = match(ind + 1, sub, pat->LEFT, loc, pposition, sub, Op(pat));
                            }
                        /* J    if leftResult.success */
                        if (s.c.lmr & TRUE)
                            /* K        rightResult=0(P):cdr(pat) */
                            {
                            s.c.rmr = match(ind + 1, nil(pat), pat->RIGHT, NULL, locpos, expr, Op(pat));
                            s.c.rmr &= ~ONCE;
                            }
                        /* L    return */
                            /* Return true if full success.

                               Also return whether lhs experienced max position
                               being reached. */
                        if (!(s.c.rmr & POSITION_MAX_REACHED))
                            s.c.rmr &= ~POSITION_ONCE;
                        /* Also return whether the pattern as a whole doesn't
                           want longer subjects, which can be found out by
                           looking at the pattern. */
                        if (/*cutoff &&*/ oncePattern(pat))
                            /* The test cutoff != NULL merely avoids that
                            oncePattern is called when it is useless. */
                            { /* Test:
                              a b c d e:`(a ?x) (?z:d) ?
                              x=
                              z=b
                              */
                            s.c.rmr |= ONCE;
                            s.c.rmr |= (char)(pat->v.fl & FENCE);
                            }
                        /* POSITION_ONCE requires zero size subjects. */
                        /* Also return the fence flag, which can be found on
                           the pattern or in the result of the lhs or the rhs.
                           (Not necessary that both have this flag.)
                        */
                        s.c.rmr ^= (char)NOTHING(pat);
                        return s.c.rmr;
                        /* end */
                        }
                    case EXP:
                        if (Op(sub) == EXP)
                            {
                            if ((s.c.lmr = match(ind + 1, sub->LEFT, pat->LEFT, NULL, 0, sub->LEFT, 12345)) & TRUE)
                                s.c.rmr = match(ind + 1, sub->RIGHT, pat->RIGHT, NULL, 0, sub->RIGHT, 12345);
#ifndef NDEBUG
                            DBGSRC(printMatchState("EXP:EXIT-MID", s, pposition, 0);)
#endif
                                s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE)); /* a*b^2*c:?x*?y^(~1:?t)*?z */
                            }
                        if (!(s.c.rmr & TRUE)
                            && ((s.c.lmr = match(ind + 1, sub, pat->LEFT, cutoff, pposition, expr, op)) & TRUE)
                            && ((s.c.rmr = match(ind + 1, &oneNode, pat->RIGHT, NULL, 0, &oneNode, 1234567)) & TRUE)
                            )
                            { /* a^2*b*c*d^3 : ?x^(?s:~1)*?y^?t*?z^(>2:?u) */
                            s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                            }
                        s.c.rmr &= ~POSITION_MAX_REACHED;
                        break;
                    case UNDERSCORE:
                        if (is_op(sub))
                            {
                            if (Op(sub) == EQUALS)
                                {
                                if (ISBUILTIN((objectnode*)sub))
                                    {
                                    errorprintf("You cannot match an object '=' with '_' if the object is built-in\n");
                                    s.c.rmr = ONCE;
                                    }
                                else
                                    {
                                    if ((s.c.lmr = match(ind + 1, sub->LEFT, pat->LEFT, NULL, 0, sub->LEFT, 12345)) & TRUE)
                                        {
                                        loc = same_as_w(sub->RIGHT); /* Object might change as a side effect!*/
                                        if ((s.c.rmr = match(ind + 1, loc, pat->RIGHT, cutoff, 0, loc, 123)) & TRUE)
                                            {
                                            dummy_op = Op(sub);
                                            }
                                        wipe(loc);
                                        }
                                    }
                                }
                            else if (((s.c.lmr = match(ind + 1, sub->LEFT, pat->LEFT, NULL, 0, sub->LEFT, 12345)) & TRUE)
                                && ((s.c.rmr = match(ind + 1, sub->RIGHT, pat->RIGHT, cutoff, 0, sub->RIGHT, 123)) & TRUE)
                                )
                                {
                                dummy_op = Op(sub);
                                }
#ifndef NDEBUG
                            DBGSRC(printMatchState("UNDERSCORE:EXIT-MID", s, pposition, 0);)
#endif
                                switch (Op(sub))
                                    {
                                        case WHITE:
                                        case PLUS:
                                        case TIMES:
                                            break;
                                        default:
                                            s.c.rmr &= ~POSITION_MAX_REACHED;
                                    }
#ifndef NDEBUG
                            DBGSRC(printMatchState("streep:EXIT-MID", s, pposition, 0);)
#endif
                            }
                        if (s.c.lmr != PRISTINE)
                            s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                        break;
                    case AND:
                        if ((s.c.lmr = match(ind + 1, sub, pat->LEFT, cutoff, pposition, expr, op)) & TRUE)
                            {
                            loc = same_as_w(pat->RIGHT);
                            loc = eval(loc);
                            if (loc->v.fl & SUCCESS)
                                {
                                s.c.rmr = TRUE;
                                if (loc->v.fl & FENCE)
                                    s.c.rmr |= ONCE;
                                }
                            else
                                {
                                s.c.rmr = FALSE;
                                if (loc->v.fl & FENCE)
                                    s.c.rmr |= (FENCE | ONCE);
                                }
                            wipe(loc);
                            }
                        s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                        break;
                    case MATCH:
                        if ((s.c.lmr = match(ind + 1, sub, pat->LEFT, cutoff, pposition, expr, op)) & TRUE)
                            {
                            if ((pat->v.fl & ATOM)
#if !STRINGMATCH_CAN_BE_NEGATED
                                && !NEGATION(pat->v.fl, ATOM)
#endif
                                )
#if CUTOFFSUGGEST
                                s.c.rmr = (char)(stringmatch(ind + 1, "U", SPOBJ(sub), NULL, pat->RIGHT, sub, 0, strlen((char*)SPOBJ(sub)), NULL, 0) & TRUE);
#else
                                s.c.rmr = (char)(stringmatch(ind + 1, "U", POBJ(sub), NULL, pat->RIGHT, sub, 0, strlen((char*)POBJ(sub))) & TRUE);
#endif
                            else
                                s.c.rmr = (char)(match(ind + 1, sub, pat->RIGHT, cutoff, pposition, expr, op) & TRUE);
                            }
                        else
                            s.c.rmr = FALSE;
                        s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE | POSITION_MAX_REACHED));
                        /*
                       dbg'(x y z f t m a i n l:? ((m a i n|f t):?X) ?W) & out$(X !X W !W)
                       correct:X f t W m a i n l

                       @(jfhljkhlhfgjkhfas:? ((lh|jk):?W) ?) & !W
                       wrong: jf
                       correct: jk
                       */
                        break;
                    case OR:
                        if ((s.c.lmr = (char)match(ind + 1, sub, pat->LEFT, cutoff, pposition, expr, op))
                            & (TRUE | FENCE)
                            )
                            {
                            if ((s.c.lmr & ONCE) && !oncePattern(pat->RIGHT))
                                {
                                s.c.rmr = (char)(s.c.lmr & TRUE);
                                }
                            else
                                {
                                s.c.rmr = (char)(s.c.lmr & (TRUE | ONCE));
                                }
                            }
                        else
                            {
                            s.c.rmr = match(ind + 1, sub, pat->RIGHT, cutoff, pposition, expr, op);
                            if ((s.c.rmr & ONCE)
                                && !(s.c.lmr & ONCE)
                                )
                                {
                                s.c.rmr &= ~(ONCE | POSITION_ONCE);
                                }
                            if ((s.c.rmr & POSITION_MAX_REACHED)
                                && !(s.c.lmr & POSITION_MAX_REACHED)
                                )
                                {
                                s.c.rmr &= ~(POSITION_MAX_REACHED | POSITION_ONCE);
                                }
                            }
                        DBGSRC(Printf("%d%*s", ind, ind, ""); \
                            Printf("OR s.c.lmr %d s.c.rmr %d\n", s.c.lmr, s.c.rmr);)
                            /*
                            :?W:?X:?Y:?Z & dbg'(a b c d:?X (((a ?:?W) & ~`|?Y)|?Z) d) & out$(X !X W !W Y !Y Z !Z)
                            erroneous: X a W a b c d Y b c Z
                            expected: X W a b c Y Z a b c
                            */
                            break;
                        /*
                        This is now much quicker than previously, because the whole expression
                        (|bc|x) is ONCE if the start of the subject does not match the start of any of
                        the alternations:
                        dbg'(h h h h h h h h h b b b b b b b b b b b b b b b b b b b b b b b b b b b b
                        b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b
                        b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b
                        b b h h h a b c d:?X (|b c|x) d)
                        */
                    case FUN:
                        /* Test whether $ is escape */
                        if (atomtest(pat->LEFT) == 0)
                            {
                            if (((sub->v.fl & (UNIFY | FLGS | NOT)) & (pat->RIGHT->v.fl & (UNIFY | FLGS | NOT)))
                                == (pat->RIGHT->v.fl & (UNIFY | FLGS | NOT))
                                )
                                {
                                if (is_op(pat->RIGHT))
                                    {
                                    if (Op(sub) == Op(pat->RIGHT))
                                        {
                                        if ((s.c.lmr = match(ind + 1, sub->LEFT, pat->RIGHT->LEFT, NULL, 0, sub->LEFT, 9999)) & TRUE)
                                            s.c.rmr = match(ind + 1, sub->RIGHT, pat->RIGHT->RIGHT, NULL, 0, sub->RIGHT, 8888);
                                        s.c.rmr |= (char)(s.c.lmr & ONCE); /*{?} (=!(a.b)):(=$!(a.b)) => =!(a.b)*/
                                        }
                                    else
                                        s.c.rmr = (char)ONCE; /*{?} (=!(a.b)):(=$!(a,b)) => F */
                                    }
                                else
                                    {
                                    if (!(pat->RIGHT->v.fl & MINUS)
                                        || (!is_op(sub)
                                        && (sub->v.fl & MINUS)
                                        )
                                        )
                                        {
                                        if (!pat->RIGHT->u.obj)
                                            {
                                            if (pat->RIGHT->v.fl & UNOPS)
                                                s.c.rmr = (char)(TRUE | ONCE); /*{?} (=!):(=$!) => =! */
                                                                             /*{?} (=!(a.b)):(=$!) => =!(a.b) */
                                                                             /*{?} (=-a):(=$-) => =-a */
                                            else if (!(sub->v.fl & (UNIFY | FLGS | NOT))
                                                && (is_op(sub) || !(sub->v.fl & MINUS))
                                                )
                                                {
                                                s.c.rmr = (char)(TRUE | ONCE); /*{?} (=):(=$) => = */
                                                                             /*{?} (=(a.b)):(=$) => =a.b */
                                                }
                                            else
                                                s.c.rmr = (char)ONCE;/*{?} (=-a):(=$) => F */
                                                                             /*{?} (=-):(=$) => F */
                                                                             /*{?} (=#):(=$) => F */
                                            }
                                        else if (!is_op(sub)
                                            && pat->RIGHT->u.obj == sub->u.obj
                                            )
                                            s.c.rmr = (char)(TRUE | ONCE); /*{?} (=-!a):(=$-!a) => =!-a */
                                        else
                                            s.c.rmr = (char)ONCE;/*{?} (=-!a):(=$-!b) => F */
                                        }
                                    else
                                        s.c.rmr = (char)ONCE; /*{?} (=):(=$-) => F */
                                                                      /*{?} (=(a.b)):(=$-) => F */
                                                                      /*{?} (=a):(=$-) => F */
                                    }
                                }
                            else
                                s.c.rmr = (char)ONCE; /*{?} (=!):(=$!!) => F */
                            s.c.rmr &= ~POSITION_MAX_REACHED;
                            break;
                            }
                        /* fall through */
                    case FUU:
                        psh(&sjtNode, &nilNode, NULL);
                        copy_insert(&sjtNode, sub, cutoff);
                        if (NOTHING(pat))
                            {
                            loc = _copyop(pat);
                            loc->v.fl &= ~NOT;
                            loc->v.fl |= SUCCESS;
                            }
                        else
                            loc = same_as_w(pat);
                        loc = eval(loc);
                        deleteNode(&sjtNode);
                        if (isSUCCESS(loc))
                            {
                            if (((loc->v.fl & (UNIFY | FILTERS)) == UNIFY) && !is_op(loc) && !loc->u.obj)
                                {
                                s.c.rmr = (char)TRUE;
                                wipe(loc);
                                break;
                                }
                            else
                                {
                                loc = setflgs(loc, pat->v.fl);
                                if (equal(pat, loc))
                                    {
                                    s.c.rmr = (char)(match(ind + 1, sub, loc, cutoff, pposition, expr, op) ^ NOTHING(loc));
                                    wipe(loc);
                                    break;
                                    }
                                }
                            }
                        else /*"cat" as return value is used as pattern, ~"cat" however is not, because the function failed. */
                            {
                            if (loc->v.fl & FENCE)
                                s.c.rmr = ONCE; /* '~ as return value from function stops stretching subject */
                            }
                        wipe(loc);
                        /* fall through */
                    default:
                        if (is_op(pat))
                            {
                            if (Op(sub) == Op(pat))
                                {
                                if ((s.c.lmr = match(ind + 1, sub->LEFT, pat->LEFT, NULL, 0, sub->LEFT, 4432)) & TRUE)
                                    {
                                    if (Op(sub) == EQUALS)
                                        {
                                        loc = same_as_w(sub->RIGHT); /* Object might change as a side effect!*/
                                        s.c.rmr = match(ind + 1, loc, pat->RIGHT, NULL, 0, loc, 2234);
                                        wipe(loc);
                                        }
                                    else
                                        s.c.rmr = match(ind + 1, sub->RIGHT, pat->RIGHT, NULL, 0, sub->RIGHT, 2234);
                                    }
                                s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                                s.c.rmr &= ~POSITION_MAX_REACHED;
                                }
#ifndef NDEBUG
                            DBGSRC(printMatchState("DEFAULT:EXIT-MID", s, pposition, 0);)
#endif
                            }
                        else
                            {
                            if (pat->u.obj
                                || !(Flgs & (FRACTION | NUMBER | NONIDENT | ATOM | IDENT))
                                )
                                {

                                s.c.rmr = (char)(ONCE | compare(sub, pat));
                                }
                            else         /* e.g.    a b c : % */
                                {
                                s.c.rmr = TRUE;
                                }
                            }
                }
            }
        }
    if (oncePattern(pat) || /* (a b X k:(|? b|`) X ?id) must fail*/ (s.c.rmr & (TRUE | FENCE | ONCE)) == FENCE)
        {
        s.c.rmr |= (char)(pat->v.fl & FENCE);
        s.c.rmr |= ONCE;
        DBGSRC(Printf("%d%*smatch(", ind, ind, ""); results(sub, cutoff); \
            Printf(":"); result(pat); Printf(") (B)");)
#ifndef NDEBUG
            DBGSRC(Printf(" rmr t %d o %d p %d m %d f %d ", \
            s.b.brmr_true, s.b.brmr_once, s.b.brmr_position_once, s.b.brmr_position_max_reached, s.b.brmr_fence);)
#endif
            DBGSRC(if (pat->v.fl & POSITION) Printf("POSITION "); \
            if (pat->v.fl & FRACTION) Printf("FRACTION "); \
                if (pat->v.fl & NUMBER) Printf("NUMBER "); \
                    if (pat->v.fl & SMALLER_THAN) Printf("SMALLER_THAN "); \
                        if (pat->v.fl & GREATER_THAN) Printf("GREATER_THAN "); \
                            if (pat->v.fl & ATOM) Printf("ATOM "); \
                                if (pat->v.fl & FENCE) Printf("FENCE "); \
                                    if (pat->v.fl & IDENT) Printf("IDENT"); \
                                        Printf("\n");)
        }
    if (is_op(pat))
        s.c.rmr ^= (char)NOTHING(pat);
    if (name)
        wipe(name);
    return s.c.rmr;
    }
