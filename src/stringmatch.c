#include "stringmatch.h"
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
#include "result.h"
#include "filewrite.h"
#include <assert.h>

static int stringOncePattern(psk pat)
    {
    /*
    This function has a side effect: it sets a flag in all pattern nodes that
    can be matched by at most one non-trivial list element (a nonzero term in
    a sum, a factor in a product that is not 1, or a nonempty word in a
    sentence. Because the function depends on ATOMFILTERS, the algorithm
    should be slightly different for normal matches and for string matches.
    Ideally, two flags should be reserved.
    */
    if(pat->v.fl & IMPLIEDFENCE)
        {
        return TRUE;
        }
    if(pat->v.fl & SATOMFILTERS)
        {
        pat->v.fl |= IMPLIEDFENCE;
        return TRUE;
        }
    else if(pat->v.fl & ATOMFILTERS)
        {
        return FALSE;
        }
    else if(IS_VARIABLE(pat)
            || NOTHING(pat)
            || (pat->v.fl & NONIDENT) /* @(abc:% c) */
            )
        {
        return FALSE;
        }
    else if(!is_op(pat))
        {
        if(!pat->u.obj)
            {
            pat->v.fl |= IMPLIEDFENCE;
            return TRUE;
            }
        else
            {
            return FALSE;
            }
        }
    else
        {
        switch(Op(pat))
            {
            case DOT:
            case COMMA:
            case EQUALS:
            case EXP:
            case LOG:
            case DIF:
                pat->v.fl |= IMPLIEDFENCE;
                return TRUE;
            case OR:
                if(stringOncePattern(pat->LEFT) && stringOncePattern(pat->RIGHT))
                    {
                    pat->v.fl |= IMPLIEDFENCE;
                    return TRUE;
                    }
                break;
            case MATCH:
                if(stringOncePattern(pat->LEFT) || stringOncePattern(pat->RIGHT))
                    {
                    pat->v.fl |= IMPLIEDFENCE;
                    return TRUE;
                    }
                break;
            case AND:
                if(stringOncePattern(pat->LEFT))
                    {
                    pat->v.fl |= IMPLIEDFENCE;
                    return TRUE;
                    }
                break;
            default:
                break;
            }
        }
    return FALSE;
    }


static char sdoEval(char* sub, char* cutoff, psk pat, psk subkn)
    {
    char ret;
    psk loc;
    psh(&sjtNode, &nilNode, NULL);
    string_copy_insert(&sjtNode, subkn, sub, cutoff);
    loc = subtreecopy(pat);
    loc->v.fl &= ~(POSITION | NONIDENT | IMPLIEDFENCE | ONCE);
    loc = eval(loc);
    deleteNode(&sjtNode);
    if(isSUCCESS(loc))
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


#if CUTOFFSUGGEST
char stringmatch
(int ind
 , char* wh
 , char* sub
 , char* cutoff
 , psk pat
 , psk subkn
 , LONG pposition
 , size_t stringLength
 , char** suggestedCutOff
 , char** mayMoveStartOfSubject
)
#else
char stringmatch
(int ind
 , char* wh
 , unsigned char* sub
 , unsigned char* cutoff
 , psk pat
 , psk subkn
 , LONG pposition
 , size_t stringLength
)
#endif
    {
    /*
    s.c.lmr and s.c.rmr have 3 independent bit fields : TRUE/FALSE, ONCE en FENCE.
    TRUE/FALSE Whether the match succeeds or fails.
    ONCE       Unwillingness of the pattern to match subjects that start in
               the same position, but end further "to the right".
               Of importance for pattern with space, + or * operator.
               Is turned on in pattern by the `@#/ prefixes and by the operators
               other than space + * _ & : | = $ '.
               Is turned of in pattern with space + * or | operator.
    FENCE      Unwillingness of the subject to be matched by alternative patterns.
               Of importance for the | and : operators in a pattern.
               Is turned on by the ` prefix (whether or not in a pattern).
               Is turned off in pattern with space + * | or : operator.
               (With | and : operators this is only the case for the left operand,
               with the other operators this is the case for all except the last
               operand in a list.)
    */
    psk loc;
    char* sloc;
    ULONG Flgs;
    matchstate s;
    int ci;
    psk name = NULL;
    assert(sizeof(s) == 4);
#if SHOWWHETHERNEVERVISITED
    if(is_op(pat))
        pat->v.fl |= VISITED;
#endif
    if(!cutoff)
        cutoff = sub + stringLength;
#if CUTOFFSUGGEST
    if((pat->v.fl & ATOM)
       || (NOTHING(pat)
           && (is_op(pat)
               || !pat->u.obj)
           )
       )
        {
        suggestedCutOff = NULL;
        }
#endif
    DBGSRC(Boolean saveNice; int redhum; saveNice = beNice; redhum = hum; beNice = FALSE; \
           hum = FALSE; Printf("%d  %.*s|%s", ind, (int)(cutoff - sub), sub, cutoff); \
           Printf(":"); result(pat); \
           Printf(",pos=" LONGD ",sLen=%ld,sugCut=%s,mayMoveStart=%s)"\
                  , pposition\
                  , (long int)stringLength\
                  , suggestedCutOff ? *suggestedCutOff ? *suggestedCutOff : (char*)"(0)" : (char*)"0"\
                  , mayMoveStartOfSubject ? *mayMoveStartOfSubject ? *mayMoveStartOfSubject : (char*)"(0)" : (char*)"0"\
           ); \
           Printf("\n"); beNice = saveNice; hum = redhum;)
        s.i = (PRISTINE << SHIFT_LMR) + (PRISTINE << SHIFT_RMR);

    Flgs = pat->v.fl;
    if(Flgs & POSITION)
        {
        if(Flgs & NONIDENT)
            return sdoEval(sub, cutoff, pat, subkn);
        else if(cutoff > sub)
            {
#if CUTOFFSUGGEST
            if(mayMoveStartOfSubject && *mayMoveStartOfSubject)
                {
                *mayMoveStartOfSubject = cutoff;
                }
#endif
            return FALSE | ONCE | POSITION_ONCE;
            }
        else
            return doPosition(s, pat, pposition, stringLength, NULL
#if CUTOFFSUGGEST
                              , mayMoveStartOfSubject
#endif
                              , 12345
            );
        }
    if(!(((Flgs & NONIDENT)
          && (NEGATION(Flgs, NONIDENT)
              ? ((s.c.once = ONCE), cutoff > sub)
              : cutoff == sub
              )
          )
         || ((Flgs & ATOM)
             && (NEGATION(Flgs, ATOM)
                 ? (cutoff < sub + 2) /*!(sub[0] && sub[1])*/
                 : cutoff > sub
                 && ((s.c.once = ONCE), cutoff > sub + 1 /*sub[1]*/)
                 )
             )
         || ((Flgs & (FRACTION | NUMBER))
             && ((ci = sfullnumbercheck(sub, cutoff))
                 , (((Flgs & FRACTION)
                     && ((ci != (QFRACTION | QNUMBER)) ^ NEGATION(Flgs, FRACTION))
                     )
                    || ((Flgs & NUMBER)
                        && (((ci & QNUMBER) == 0) ^ NEGATION(Flgs, NUMBER))
                        )
                    )
                 )
             && (s.c.rmr = (ci == DEFINITELYNONUMBER) ? ONCE : FALSE
                 , (s.c.lmr = PRISTINE) /* Single = is correct! */
                 )
             )
         )
       )
        {
        if(IS_VARIABLE(pat))
            {
            int ok = TRUE;
            if(is_op(pat))
                {
                ULONG saveflgs = Flgs & VISIBLE_FLAGS;
                name = subtreecopy(pat);
                name->v.fl &= ~VISIBLE_FLAGS;
                name->v.fl |= SUCCESS;
                if((s.c.rmr = (char)evaluate(name)) != TRUE)
                    ok = FALSE;
                if(Op(name) != EQUALS)
                    {
                    name = isolated(name);
                    name->v.fl |= saveflgs;
                    }
                pat = name;
                }
            if(ok)
                {
                if(Flgs & UNIFY)        /* ?  */
                    {
                    if(!NOTHING(pat) || cutoff > sub)
                        {
                        if(is_op(pat)
                           || pat->u.obj
                           )
                            {
                            if(Flgs & INDIRECT)        /* ?! of ?!! */
                                {
                                if((loc = SymbolBinding_w(pat, Flgs & DOUBLY_INDIRECT)) != NULL)
                                    {
                                    if(is_object(loc))
                                        s.c.rmr = (char)string_copy_insert(loc, subkn, sub, cutoff);
                                    else
                                        {
                                        s.c.rmr = (char)evaluate(loc);
                                        if(!string_copy_insert(loc, subkn, sub, cutoff))
                                            s.c.rmr = FALSE;
                                        }
                                    wipe(loc);
                                    }
                                else
                                    s.c.rmr = (char)NOTHING(pat);
                                }
                            else
                                {
                                s.c.rmr = (char)string_copy_insert(pat, subkn, sub, cutoff);
                                }
                            }
                        else
                            s.c.rmr = TRUE;
                        }
                    }
                else if(Flgs & INDIRECT)        /* ! or !! */
                    {
                    if((loc = SymbolBinding_w(pat, Flgs & DOUBLY_INDIRECT)) != NULL)
                        {
                        cleanOncePattern(loc);
#if CUTOFFSUGGEST
                        if(mayMoveStartOfSubject)
                            {
                            *mayMoveStartOfSubject = 0;
                            }
                        s.c.rmr = (char)(stringmatch(ind + 1, "A", sub, cutoff, loc, subkn, pposition, stringLength
                                                     , suggestedCutOff
                                                     , 0
                        ) ^ NOTHING(pat));
#else
                        s.c.rmr = (char)(stringmatch(ind + 1, "A", sub, cutoff, loc, subkn, pposition, stringLength
                        ) ^ NOTHING(pat));
#endif
                        wipe(loc);
                        }
                    else
                        s.c.rmr = (char)NOTHING(pat);
                    }
                }
            }
        else
            {
            switch(Op(pat))
                {
                case PLUS:
                case TIMES:
                    break;
                case WHITE:
                    {
                    LONG locpos = pposition;
#if CUTOFFSUGGEST
                    char* suggested_Cut_Off = sub;
                    char* may_Move_Start_Of_Subject;
                    may_Move_Start_Of_Subject = sub;
#endif
                    /* This code mirrors that of match(). (see below)*/

                    sloc = sub;                                     /* A    divisionPoint=S */

#if CUTOFFSUGGEST
                    s.c.lmr = stringmatch(ind + 1, "I", sub, sloc       /* B    leftResult=0(P):car(P) */
                                          , pat->LEFT, subkn, pposition
                                          , stringLength, &suggested_Cut_Off
                                          , mayMoveStartOfSubject);
                    if((s.c.lmr & ONCE) && mayMoveStartOfSubject && *mayMoveStartOfSubject > sub)
                        {
                        return ONCE;
                        }
#else
                    s.c.lmr = stringmatch(ind + 1, "I", sub, sloc, pat->LEFT, subkn, pposition, stringLength);
#endif

#if CUTOFFSUGGEST
                    if(suggested_Cut_Off > sloc)
                        {
                        if(cutoff && suggested_Cut_Off > cutoff)
                            {
                            if(suggestedCutOff)
                                {
                                locpos += suggested_Cut_Off - sloc;
                                cutoff = sloc = *suggestedCutOff = suggested_Cut_Off;
                                }
                            else
                                {
                                locpos += cutoff - sloc;
                                sloc = cutoff;
                                s.c.lmr &= ~TRUE;
                                }
                            }
                        else
                            {
                            assert(suggested_Cut_Off > sloc);
                            locpos += suggested_Cut_Off - sloc;
                            sloc = suggested_Cut_Off;
                            }
                        }
                    else
#endif
                        s.c.lmr &= ~ONCE;
                    while(sloc < cutoff)                            /* C    while divisionPoint */
                        {
                        if(s.c.lmr & TRUE)                          /* D        if leftResult.success */
                            {
#if CUTOFFSUGGEST
                            if(s.c.lmr & ONCE)
                                may_Move_Start_Of_Subject = 0;
                            else if(may_Move_Start_Of_Subject != 0)
                                may_Move_Start_Of_Subject = sloc;
                            s.c.rmr = stringmatch(ind + 1, "J", sloc    /* E            rightResult=SR:cdr(P) */
                                                  , cutoff, pat->RIGHT, subkn
                                                  , locpos, stringLength, suggestedCutOff
                                                  , &may_Move_Start_Of_Subject);
                            if(may_Move_Start_Of_Subject != sloc && may_Move_Start_Of_Subject != 0)
                                {
                                assert(may_Move_Start_Of_Subject > sloc);
                                locpos += may_Move_Start_Of_Subject - sloc;
                                sloc = may_Move_Start_Of_Subject;
                                }
                            else
                                {
                                ++sloc;
                                ++locpos;
                                }
#else
                            s.c.rmr = stringmatch(ind + 1, "J", sloc, cutoff, pat->RIGHT, subkn, locpos, stringLength);
                            ++sloc;
                            ++locpos;
#endif
                            if(!(s.c.lmr & ONCE))
                                s.c.rmr &= ~ONCE;
                            }
                        else
                            {
                            ++sloc;
                            ++locpos;
                            }
                        if((s.c.rmr & TRUE)                       /* F        if(1) full success */
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
                            if(sloc > sub + 1)                          /* Also return whether sub has reached max position.*/
                                s.c.rmr &= ~POSITION_MAX_REACHED;       /* This flag is reason to stop increasing the position of the division any further, but it must not be signalled back to the caller if the lhs is not nil ... */
                            s.c.rmr |= (char)(s.c.lmr & POSITION_MAX_REACHED); /* ... unless it is the lhs that signals it. */
                            if(stringOncePattern(pat))                  /* Also return whether the pattern as a whole */
                                {                                       /* doesn't want longer subjects, which can be */
                                s.c.rmr |= ONCE;                        /* found out by looking at the pattern        */
                                s.c.rmr |= (char)(pat->v.fl & FENCE);
                                }                                       /* or by looking at whether both lhs and rhs  */
                            else if(!(s.c.lmr & ONCE))                  /* results indicated this, in which case both */
                                s.c.rmr &= ~ONCE;                       /* sides must be non-zero size subjects.      */
                            return s.c.rmr ^ (char)NOTHING(pat);
                            }
                        /* H        SL,SR=shift_right divisionPoint */
                        /* SL = lhs divisionPoint S, SR = rhs divisionPoint S */
                        /* I        leftResult=SL:car(P) */
#if CUTOFFSUGGEST
                        suggested_Cut_Off = sub;
                        s.c.lmr = stringmatch(ind + 1, "I", sub, sloc, pat->LEFT, subkn,/* 0 ? */pposition,/* strlen(sub) ? */ stringLength, &suggested_Cut_Off, mayMoveStartOfSubject);
                        if(suggested_Cut_Off > sloc)
                            {
                            if(!(cutoff && suggested_Cut_Off > cutoff))
                                {
                                assert(suggested_Cut_Off > sloc);
                                locpos += suggested_Cut_Off - sloc;
                                sloc = suggested_Cut_Off;
                                }
                            }
#else
                        s.c.lmr = stringmatch(ind + 1, "I", sub, sloc, pat->LEFT, subkn,/* 0 ? */pposition,/* strlen(sub) ? */ stringLength);
#endif
                        }

                    if(s.c.lmr & TRUE)                              /* J    if leftResult.success */
                        {
#if CUTOFFSUGGEST
                        s.c.rmr = stringmatch(ind + 1, "J", sloc, cutoff /* K        rightResult=0(P):cdr(pat) */
                                              , pat->RIGHT, subkn, locpos, stringLength
                                              , suggestedCutOff, mayMoveStartOfSubject);
#else
                        s.c.rmr = stringmatch(ind + 1, "J", sloc, cutoff, pat->RIGHT, subkn, locpos, stringLength);
#endif
                        s.c.rmr &= ~ONCE;
                        }
                    /* L    return */
                    if(!(s.c.rmr & POSITION_MAX_REACHED))
                        s.c.rmr &= ~POSITION_ONCE;
                    if(/*(cutoff > sub) &&*/ stringOncePattern(pat))    /* The test cutoff > sub merely avoids that stringOncePattern is called when it is useless. */
                        {/* Test:
                         @(abcde:`(a ?x) (?z:d) ? )
                         z=b
                         */
                        s.c.rmr |= ONCE;
                        s.c.rmr |= (char)(pat->v.fl & FENCE);
                        }
                    return s.c.rmr ^ (char)NOTHING(pat);               /* end */
                    }
                case UNDERSCORE:
                    if(cutoff > sub + 1)
                        {
#if CUTOFFSUGGEST
                        s.c.lmr = stringmatch(ind + 1, "M", sub, sub + 1, pat->LEFT, subkn, pposition, stringLength, NULL, mayMoveStartOfSubject);
#else
                        s.c.lmr = stringmatch(ind + 1, "M", sub, sub + 1, pat->LEFT, subkn, pposition, stringLength);
#endif
                        if((s.c.lmr & TRUE)
#if CUTOFFSUGGEST
                           && ((s.c.rmr = stringmatch(ind + 1, "N", sub + 1, cutoff, pat->RIGHT, subkn, pposition, stringLength, suggestedCutOff, mayMoveStartOfSubject)) & TRUE)
#else
                           && ((s.c.rmr = stringmatch(ind + 1, "N", sub + 1, cutoff, pat->RIGHT, subkn, pposition, stringLength)) & TRUE)
#endif
                           )
                            {
                            dummy_op = WHITE;
                            }
                        s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                        }
                    break;
                case AND:
#if CUTOFFSUGGEST
                    if((s.c.lmr = stringmatch(ind + 1, "O", sub, cutoff, pat->LEFT, subkn, pposition, stringLength, suggestedCutOff, mayMoveStartOfSubject)) & TRUE)
#else
                    if((s.c.lmr = stringmatch(ind + 1, "O", sub, cutoff, pat->LEFT, subkn, pposition, stringLength)) & TRUE)
#endif
                        {
                        loc = same_as_w(pat->RIGHT);
                        loc = eval(loc);
                        if(loc->v.fl & SUCCESS)
                            {
                            s.c.rmr = TRUE;
                            if(loc->v.fl & FENCE)
                                s.c.rmr |= ONCE;
                            }
                        else
                            {
                            s.c.rmr = FALSE;
                            if(loc->v.fl & FENCE)
                                s.c.rmr |= (FENCE | ONCE);
                            if(loc->v.fl & IMPLIEDFENCE) /* (for function utf$) */
                                s.c.rmr |= ONCE;
                            }
                        wipe(loc);
                        }
                    s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                    break;
                case MATCH:
#if CUTOFFSUGGEST
                    if((s.c.lmr = stringmatch(ind + 1, "P", sub, cutoff, pat->LEFT, subkn, pposition, stringLength, suggestedCutOff, mayMoveStartOfSubject)) & TRUE)
#else
                    if((s.c.lmr = stringmatch(ind + 1, "P", sub, cutoff, pat->LEFT, subkn, pposition, stringLength)) & TRUE)
#endif
                        {
#if CUTOFFSUGGEST
                        if(suggestedCutOff && *suggestedCutOff > cutoff)
                            {
                            cutoff = *suggestedCutOff;
                            }

                        s.c.rmr = (char)(stringmatch(ind + 1, "Q", sub, cutoff, pat->RIGHT, subkn, pposition, stringLength, 0, 0));
#else
                        s.c.rmr = (char)(stringmatch(ind + 1, "Q", sub, cutoff, pat->RIGHT, subkn, pposition, stringLength));
#endif
                        }
                    else
                        s.c.rmr = FALSE;
                    s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE | POSITION_MAX_REACHED));
                    break;
                case OR:
#if CUTOFFSUGGEST
                    if(mayMoveStartOfSubject)
                        *mayMoveStartOfSubject = 0;
                    if((s.c.lmr = (char)(stringmatch(ind + 1, "R", sub, cutoff, pat->LEFT, subkn, pposition, stringLength, NULL, 0)))
#else
                    if((s.c.lmr = (char)(stringmatch(ind + 1, "R", sub, cutoff, pat->LEFT, subkn, pposition, stringLength)))
#endif
                       & (TRUE | FENCE)
                       )
                        {
                        if((s.c.lmr & ONCE) && !stringOncePattern(pat->RIGHT))
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
#if CUTOFFSUGGEST
                        s.c.rmr = stringmatch(ind + 1, "S", sub, cutoff, pat->RIGHT, subkn, pposition, stringLength, NULL, 0);
#else
                        s.c.rmr = stringmatch(ind + 1, "S", sub, cutoff, pat->RIGHT, subkn, pposition, stringLength);
#endif
                        if((s.c.rmr & ONCE)
                           && !(s.c.lmr & ONCE)
                           )
                            {
                            s.c.rmr &= ~(ONCE | POSITION_ONCE);
                            }
                        if((s.c.rmr & POSITION_MAX_REACHED)
                           && !(s.c.lmr & POSITION_MAX_REACHED)
                           )
                            {
                            s.c.rmr &= ~(POSITION_MAX_REACHED | POSITION_ONCE);
                            }
                        }
                    break;
                    /*
                    This is now much quicker than previously, because the whole expression
                    (|bc|x) is ONCE if the start of the subject does not match the start of any of
                    the alternations:
                    dbg'@(hhhhhhhhhbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbhhhabcd:?X (|bc|x) d)
                    */
                case FUN:
                case FUU:
                    psh(&sjtNode, &nilNode, NULL);
                    string_copy_insert(&sjtNode, subkn, sub, cutoff);
                    if(NOTHING(pat))
                        {
                        loc = _copyop(pat);
                        loc->v.fl &= ~NOT;
                        loc->v.fl |= SUCCESS;
                        }
                    else
                        loc = same_as_w(pat);
                    loc = eval(loc);
                    deleteNode(&sjtNode);
#if CUTOFFSUGGEST
                    if(mayMoveStartOfSubject)
                        *mayMoveStartOfSubject = 0;
#endif
                    if(isSUCCESS(loc))
                        {
                        if(((loc->v.fl & (UNIFY | FILTERS)) == UNIFY) && !is_op(loc) && !loc->u.obj)
                            {
                            s.c.rmr = (char)TRUE;
                            wipe(loc);
                            break;
                            }
                        else
                            {
                            loc = setflgs(loc, pat->v.fl);
                            if(equal(pat, loc))
                                {
#if CUTOFFSUGGEST
                                s.c.rmr = (char)(stringmatch(ind + 1, "T", sub, cutoff, loc, subkn, pposition, stringLength, NULL, 0) ^ NOTHING(loc));
#else
                                s.c.rmr = (char)(stringmatch(ind + 1, "T", sub, cutoff, loc, subkn, pposition, stringLength) ^ NOTHING(loc));
#endif
                                wipe(loc);
                                break;
                                }
                            }
                        }
                    else
                        {
                        if(loc->v.fl & (FENCE | IMPLIEDFENCE))
                            s.c.rmr = ONCE; /* '~ as return value from function stops stretching subject */
                        }
                    wipe(loc);
                    break;
                default:
                    if(!is_op(pat))
                        {
                        if(!pat->u.obj
                           && (Flgs & (FRACTION | NUMBER | NONIDENT | ATOM | IDENT))
                           )
                            {         /* e.g.    a b c : % */
                            s.c.rmr = TRUE;
                            }
                        else
                            {
#if CUTOFFSUGGEST
                            s.c.rmr = (char)(scompare((char*)sub, cutoff, pat
                                                      , ((!(Flgs & ATOM)
                                                          || NEGATION(Flgs, ATOM)
                                                          )
                                                         ? suggestedCutOff
                                                         : NULL
                                                         )
                                                      , mayMoveStartOfSubject
                            )
                                             );
#else
                            s.c.rmr = (char)(scompare((unsigned char*)sub, cutoff, pat));
#endif
                            DBGSRC(Printf("%s %d%*sscompare(%.*s,", wh, ind, ind, "", (int)(cutoff - sub), sub); result(pat); Printf(") "); \
                                   if(s.c.rmr & ONCE) Printf("ONCE|"); \
                                       if(s.c.rmr & TRUE) Printf("TRUE"); else Printf("FALSE"); \
                                           Printf("\n");)
                            }
                        }
                }
            }
        }
    DBGSRC(if(s.c.rmr & (FENCE | ONCE))\
        {Printf("%s %d%*s+", wh, ind, ind, ""); if(s.c.rmr & FENCE)\
           Printf(" FENCE "); if(s.c.rmr & ONCE)Printf(" ONCE "); Printf("\n"); })
        s.c.rmr |= (char)(pat->v.fl & FENCE);
    if(stringOncePattern(pat) || /* @("abXk":(|? b|`) X ?id) must fail*/ (s.c.rmr & (TRUE | FENCE | ONCE)) == FENCE)
        {
        s.c.rmr |= ONCE;
        DBGSRC(Boolean saveNice; int redhum; saveNice = beNice; redhum = hum; \
               beNice = FALSE; hum = FALSE; \
               Printf("%d%*sstringmatch(%.*s", ind, ind, "", (int)(cutoff - sub), sub); \
               Printf(":"); result(pat); \
               beNice = saveNice; hum = redhum; \
               Printf(") s.c.rmr %d (B)", s.c.rmr); \
               if(pat->v.fl & POSITION) Printf("POSITION "); \
                   if(pat->v.fl & FRACTION)Printf("FRACTION "); \
                       if(pat->v.fl & NUMBER)Printf("NUMBER "); \
                           if(pat->v.fl & SMALLER_THAN)Printf("SMALLER_THAN "); \
                               if(pat->v.fl & GREATER_THAN) Printf("GREATER_THAN "); \
                                   if(pat->v.fl & ATOM)  Printf("ATOM "); \
                                       if(pat->v.fl & FENCE) Printf("FENCE "); \
                                           if(pat->v.fl & IDENT) Printf("IDENT"); \
                                               Printf("\n");)
        }
    if(is_op(pat))
        s.c.rmr ^= (char)NOTHING(pat);
    if(name)
        wipe(name);
    return (char)(s.c.once | s.c.rmr);
    }
