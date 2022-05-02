#include "equal.h"

#include "flags.h"
#include "nodedefs.h"
#include "rational.h"
#include "globals.h"
#include "input.h"
#include "eval.h"
#include "wipecopy.h"
#include "encoding.h"
#include "numbercheck.h"
#include "input.h"
#include <string.h>

int equal(psk kn1, psk kn2)
    {
    while (kn1 != kn2)
        {
        int r;
        if (is_op(kn1))
            {
            if (is_op(kn2))
                {
                r = (int)Op(kn2) - (int)Op(kn1);
                if (r)
                    {
                    return r;
                    }
                r = equal(kn1->LEFT, kn2->LEFT);
                if (r)
                    return r;
                kn1 = kn1->RIGHT;
                kn2 = kn2->RIGHT;
                }
            else
                return 1;
            }
        else if (is_op(kn2))
            return -1;
        else if (RATIONAL_COMP(kn1))
            {
            if (RATIONAL_COMP(kn2))
                {
                switch (qCompare(kn1, kn2))
                    {
                        case MINUS: return -1;
                        case QNUL:
                            {
                            return 0;
                            }
                        default: return 1;
                    }
                }
            else
                return -1;
            }
        else if (RATIONAL_COMP(kn2))
            {
            return 1;
            }
        else
            return (r = HAS_MINUS_SIGN(kn1) - HAS_MINUS_SIGN(kn2)) == 0
            ? strcmp((char *)POBJ(kn1), (char *)POBJ(kn2))
            : r;
        }
    return 0;
    }

int cmp(psk kn1, psk kn2);

static int cmpsub(psk kn1, psk kn2)
    {
    int ret;
    psk pnode = NULL;
    addr[1] = kn1;
    pnode = build_up(pnode, "1+\1+-1)", NULL);
    pnode = eval(pnode);
    ret = cmp(pnode, kn2);
    wipe(pnode);
    return ret;
    }

int cmp(psk kn1, psk kn2)
    {
    while (kn1 != kn2)
        {
        int r;
        if (is_op(kn1))
            {
            if (is_op(kn2))
                {
                r = (int)Op(kn2) - (int)Op(kn1);
                if (r)
                    {
                    /*{?} x^(y*(a+b))+-1*x^(a*y+b*y) => 0 */ /*{!} 0 */
                    if (Op(kn1) == TIMES
                        && Op(kn2) == PLUS
                        && is_op(kn1->RIGHT)
                        && Op(kn1->RIGHT) == PLUS
                        )
                        {
                        return cmpsub(kn1, kn2);
                        }
                    else if (Op(kn2) == TIMES
                        && Op(kn1) == PLUS
                        && is_op(kn2->RIGHT)
                        && Op(kn2->RIGHT) == PLUS
                        )
                        {
                        /*{?} a^(b*d+c*d) * (a^(b*f+c*f)+a^(e*b+e*c)) + -1*(a^((b+c)*(d+e))+a^((b+c)*(d+f))) => 0 */
                        return -cmpsub(kn2, kn1);
                        }
                    return r;
                    }
                switch (Op(kn1))
                    {
                        case PLUS:
                        case EXP:
                            r = cmp(kn1->LEFT, kn2->LEFT);
                            if (r)
                                return r;
                            kn1 = kn1->RIGHT;
                            kn2 = kn2->RIGHT;
                            break;
                        default:
                            r = equal(kn1->LEFT, kn2->LEFT);
                            if (r)
                                return r;
                            else
                                return equal(kn1->RIGHT, kn2->RIGHT);
                    }
                }
            else
                return 1;
            }
        else if (is_op(kn2))
            return -1;
        else if (RATIONAL_COMP(kn1))
            {
            if (RATIONAL_COMP(kn2))
                {
                switch (qCompare(kn1, kn2))
                    {
                        case MINUS: return -1;
                        case QNUL:
                            {
                            return 0;
                            }
                        default: return 1;
                    }
                }
            else
                return -1;
            }
        else if (RATIONAL_COMP(kn2))
            {
            return 1;
            }
        else
            return (r = HAS_MINUS_SIGN(kn1) - HAS_MINUS_SIGN(kn2)) == 0
            ? strcmp((char*)POBJ(kn1), (char*)POBJ(kn2))
            : r;
        }
    return 0;
    }

#define PNOT ((p->v.fl & NOT) && (p->v.fl & FLGS) < NUMBER)
#define PGRT (p->v.fl & GREATER_THAN)
#define PSML (p->v.fl & SMALLER_THAN)
#define PUNQ  (PGRT && PSML)
#define EPGRT (PGRT && !PSML)
#define EPSML (PSML && !PGRT)

int compare(psk s, psk p)
    {
    int Sign;
    if (RATIONAL_COMP(s) && RATIONAL_WEAK(p))
        Sign = qCompare(s, p);
    else
        {
        if (is_op(s))
            return NOTHING(p);
        if (PLOBJ(s) == IM && PLOBJ(p) == IM)
            {
            int TMP = ((s->v.fl & MINUS) ^ (p->v.fl & MINUS));
            int Njet = (p->v.fl & FLGS) < NUMBER && ((p->v.fl & NOT) && 1);
            int ul = (p->v.fl & (GREATER_THAN | SMALLER_THAN)) == (GREATER_THAN | SMALLER_THAN);
            int e1 = ((p->v.fl & GREATER_THAN) && 1);
            int e2 = ((p->v.fl & SMALLER_THAN) && 1);
            int ee = (e1 ^ e2) && 1;
            int R = !ee && (Njet ^ ul ^ !TMP);
            return R;
            }
        if ((p->v.fl & (NOT | FRACTION | NUMBER | GREATER_THAN | SMALLER_THAN)) == (NOT | GREATER_THAN | SMALLER_THAN))
            { /* Case insensitive match: ~<> means "not different" */
            Sign = strcasecomp((char*)POBJ(s), (char*)POBJ(p));
            }
        else
            {
            Sign = strcmp((char*)POBJ(s), (char*)POBJ(p));
            }
        if (Sign > 0)
            Sign = 0;
        else if (Sign < 0)
            Sign = MINUS;
        else
            Sign = QNUL;
        }
    switch (Sign)
        {
            case 0:
                {
                return PNOT ^ (PGRT && 1);
                }
            case QNUL:
                {
                return !PNOT ^ (PGRT || PSML);
                }
            default:
                {
                return PNOT ^ (PSML && 1);
                }
        }
    }


/*
With the % flag on an otherwise numeric pattern, the pattern is treated
    as a string, not a number.
*/
int scompare(char* s, char* cutoff, psk p
#if CUTOFFSUGGEST
    , char** suggestedCutOff
    , char** mayMoveStartOfSubject
#endif
)
    {
    int Sign = 0;
    char* P;
#if CUTOFFSUGGEST
    char* S = s;
#endif
    char sav;
    int smallerIfMoreDigitsAdded; /* -1/22 smaller than -1/2 */ /* 1/22 smaller than 1/2 */
    int samesign;
    int less;
    ULONG Flgs = p->v.fl;
    enum { NoIndication, AnInteger, NotAFraction, NotANumber, AFraction, ANumber };
    int status = NoIndication;

    if (NEGATION(Flgs, NONIDENT))
        {
        Flgs &= ~(NOT | NONIDENT);
        }
    if (NEGATION(Flgs, FRACTION))
        {
        Flgs &= ~(NOT | FRACTION);
        if (Flgs & NUMBER)
            status = AnInteger;
        else
            status = NotAFraction;
        }
    else if (NEGATION(Flgs, NUMBER))
        {
        Flgs &= ~(NOT | NUMBER);
        status = NotANumber;
        }
    else if (Flgs & FRACTION)
        status = AFraction;
    else if (Flgs & NUMBER)
        status = ANumber;


    if (!(Flgs & NONIDENT) /* % as flag on number forces comparison as string, not as number */
        && RATIONAL_WEAK(p)
        && (status != NotANumber)
        && ((((Flgs & (QFRACTION | IS_OPERATOR)) == QFRACTION)
        && (status != NotAFraction)
        )
        || (((Flgs & (QFRACTION | QNUMBER | IS_OPERATOR)) == QNUMBER)
        && (status != AFraction)
        )
        )
        )
        {
        int check = sfullnumbercheck(s, cutoff);
        if (check & QNUMBER)
            {
            int anythingGoes = 0;
            psk n = NULL;
            sav = *cutoff;
            *cutoff = '\0';
            n = build_up(n, s, NULL);
            *cutoff = sav;

            if (RAT_RAT(n))
                {
                smallerIfMoreDigitsAdded = 1;
                }
            else
                {
                smallerIfMoreDigitsAdded = 0;
                /* check whether there is a slash followed by non-zero decimal digit coming */
                if (!RAT_RAT(n))
                    {
                    char* t = cutoff;
                    for (; *t; ++t)
                        {
                        if (*t == '/')
                            {
                            if (('0' < *++t) && (*t <= '9'))
                                {
                                anythingGoes = 1;
                                }
                            break;
                            }
                        if ('0' > *t || *t > '9')
                            break;
                        }
                    }
                }
            if (RAT_NEG_COMP(n))
                {
                less = !smallerIfMoreDigitsAdded;
                }
            else
                {
                less = smallerIfMoreDigitsAdded;
                }
            Sign = qCompare(n, p);
            samesign = ((n->v.fl & MINUS) == (p->v.fl & MINUS));
            wipe(n);
            switch (Flgs & (NOT | GREATER_THAN | SMALLER_THAN))
                {
                    case NOT | GREATER_THAN | SMALLER_THAN:    /* n:~<>p */
                    case 0:                                /* n:p */
                        {
                        /*
                                        n:p        n == p
                                        n:~<>p  same as n:p
                                            [n == p]
                                                TRUE | ONCE
                                            [n > p]
                                                if n < 0 && p < 0
                                                    FALSE
                                                else
                                                    ONCE
                                            [n < p]
                                                if n < 0
                                                    ONCE
                                                else
                                                    FALSE
                        */
                        switch (Sign)
                            {
                                case QNUL:    /* n == p */
                                    if (anythingGoes)
                                        return TRUE;
                                    else
                                        return TRUE | ONCE;
                                case 0:        /* n > p */
                                    if (samesign && (anythingGoes || less))
                                        return FALSE;
                                    else
                                        return ONCE;
                                default:    /* n < p */
                                    if (samesign && (anythingGoes || !less))
                                        return FALSE;
                                    else
                                        return ONCE;
                            }
                        }
                    case SMALLER_THAN:    /* n:<p */
                        {
                        /*
                                        n:<p    n < p
                                            [n == p]
                                                if n < 0
                                                    FALSE
                                                else
                                                    ONCE
                                            [n > p]
                                                if n < 0
                                                    FALSE
                                                else
                                                    ONCE
                                            [n < p]
                                                TRUE
                        */
                        switch (Sign)
                            {
                                case QNUL:    /* n == p */
                                    if (anythingGoes || less)
                                        return FALSE;
                                    else
                                        return ONCE;
                                case 0:        /* n > p */
                                    if (samesign && (anythingGoes || less))
                                        return FALSE;
                                    else
                                        return ONCE;
                                default:    /* n < p */
                                    return TRUE;
                            }
                        }
                    case GREATER_THAN:    /* n:>p */
                        {
                        /*
                                        n:>p    n > p
                                            [n == p]
                                                if n < 0
                                                    ONCE
                                                else
                                                    FALSE
                                            [n > p]
                                                TRUE
                                            [n < p]
                                                if n < 0
                                                    ONCE
                                                else
                                                    FALSE
                        */
                        switch (Sign)
                            {
                                case 0:        /* n > p */
                                    return TRUE;
                                case QNUL:/* n == p */
                                    if (anythingGoes || !less)
                                        return FALSE;
                                    else
                                        return ONCE;
                                default:    /* n < p */
                                    if (samesign && (anythingGoes || !less))
                                        return FALSE;
                                    else
                                        return ONCE;
                            }
                        }
                    case GREATER_THAN | SMALLER_THAN:    /* n:<>p */
                    case NOT:                        /* n:~p */
                        {
                        /*
                                        n:<>p   n != p
                                        n:~p    same as n:<>p
                                            [n == p]
                                                FALSE
                                            [n > p]
                                                TRUE
                                            [n < p]
                                                TRUE

                        */
                        switch (Sign)
                            {
                                case QNUL:    /* n == p */
                                    return FALSE;
                                default:    /* n < p, n > p */
                                    return TRUE;
                            }
                        }
                    case NOT | SMALLER_THAN:    /* n:~<p */
                        {
                        /*
                                        n:~<p   n >= p
                                            [n == p]
                                                if n < 0
                                                    TRUE | ONCE
                                                else
                                                    TRUE
                                            [n > p]
                                                TRUE
                                            [n < p]
                                                if n < 0
                                                    ONCE
                                                else
                                                    FALSE
                        */
                        switch (Sign)
                            {
                                case QNUL:    /* n == p */
                                    if (anythingGoes || !less)
                                        return TRUE;
                                    else
                                        return TRUE | ONCE;
                                case 0:        /* n > p */
                                    return TRUE;
                                default:    /* n < p */
                                    if (samesign && (anythingGoes || !less))
                                        return FALSE;
                                    else
                                        return ONCE;
                            }
                        }
                        /*case NOT|GREATER_THAN:*/    /* n:~>p */
                    default:
                        {
                        /*
                                        n:~>p   n <= p
                                            [n == p]
                                                if n < p
                                                    TRUE
                                                else
                                                    TRUE | ONCE
                                            [n > p]
                                                if n < p
                                                    FALSE
                                                else
                                                    ONCE
                                            [n < p]
                                                TRUE
                        */
                        switch (Sign)
                            {
                                case QNUL:    /* n == p */
                                    if (anythingGoes || less)
                                        return TRUE;
                                    else
                                        return TRUE | ONCE;
                                case 0:        /* n > p */
                                    if (samesign && (anythingGoes || less))
                                        return FALSE;
                                    else
                                        return ONCE;
                                default:    /* n < p */
                                    return TRUE;
                            }
                        }
                }
            /* End (check & QNUMBER) == TRUE. */
            /*printf("Not expected here!");getchar();*/
            }
        else if (((s == cutoff) && (Flgs & (NUMBER | FRACTION)))
            || ((s < cutoff)
            && (((*s == '-') && (cutoff < s + 2))
            || cutoff[-1] == '/'
            )
            )
            )
            {
            /* We can not yet discredit the subject as a number.
            With additional characters it may become a number. */
            return FALSE;
            }
        /* Subject is definitely not a number. */
        }

    P = (char*)SPOBJ(p);

    if ((Flgs & (NOT | FRACTION | NUMBER | GREATER_THAN | SMALLER_THAN)) == (NOT | GREATER_THAN | SMALLER_THAN))
        { /* Case insensitive match: ~<> means "not different" */
        Sign = strcasecompu(&s, &P, cutoff); /* Additional argument cutoff */
        }
    else
        {
#if CUTOFFSUGGEST
        if (suggestedCutOff)
            {
            switch (Flgs & (NOT | GREATER_THAN | SMALLER_THAN))
                {
                    case NOT | GREATER_THAN:    /* n:~>p */
                    case SMALLER_THAN:    /* n:<p */
                    case GREATER_THAN | SMALLER_THAN:    /* n:<>p */
                    case NOT:                        /* n:~p */
                        {
                        while (((Sign = (s < cutoff ? (int)(unsigned char)*s : 0) - (int)(unsigned char)*P) == 0) && *P) /* Additional argument cutoff */
                            {
                            ++s;
                            ++P;
                            }
                        if (Sign > 0)
                            {
                            switch (Flgs & (NOT | GREATER_THAN | SMALLER_THAN))
                                {
                                    case NOT | GREATER_THAN:
                                    case SMALLER_THAN:
                                        return ONCE;
                                    default:
                                        return TRUE;
                                }
                            }
                        else if (Sign == 0)
                            {
                            switch (Flgs & (NOT | GREATER_THAN | SMALLER_THAN))
                                {
                                    case NOT | GREATER_THAN:
                                        return TRUE | ONCE;
                                    case SMALLER_THAN:
                                        return ONCE;
                                    default:
                                        return FALSE;
                                }
                            }
                        return TRUE;
                        }
                    case GREATER_THAN:    /* n:>p */
                        {
                        while (((Sign = (int)(unsigned char)*s - (int)(unsigned char)*P) == 0) && *s && *P)
                            {
                            ++s;
                            ++P;
                            }
                        if (s >= cutoff)
                            {
                            if (Sign > 0)
                                *suggestedCutOff = s + 1;
                            break;
                            }
                        return ONCE;
                        }
                    case NOT | GREATER_THAN | SMALLER_THAN:    /* n:~<>p */
                    case 0:                                /* n:p */
                        while (((Sign = (int)(unsigned char)*s - (int)(unsigned char)*P) == 0) && *s && *P)
                            {
                            ++s;
                            ++P;
                            }
                        if (s >= cutoff && *P == 0)
                            {
                            *suggestedCutOff = s;
                            /*Sign = 0;*/
                            if (mayMoveStartOfSubject)
                                *mayMoveStartOfSubject = 0;
                            return TRUE | ONCE;
                            }
                        if (mayMoveStartOfSubject && *mayMoveStartOfSubject != 0)
                            {
                            char* startpos;
                            /* #ifndef NDEBUG
                                                    char * pat = (char *)POBJ(p);
                            #endif */
                            startpos = strstr((char*)S, (char*)POBJ(p));
                            if (startpos != 0)
                                {
                                if (Flgs & MINUS)
                                    --startpos;
                                }
                            /*assert(  startpos == 0
                                  || (startpos+strlen((char *)POBJ(p)) < cutoff - 1)
                                  || (startpos+strlen((char *)POBJ(p)) >= cutoff)
                                  );*/
                            *mayMoveStartOfSubject = (char*)startpos;
                            return ONCE;
                            }

                        if (Sign > 0 || s < cutoff)
                            {
                            return ONCE;
                            }
                        return FALSE;/*subject too short*/
                    case NOT | SMALLER_THAN:    /* n:~<p */
                        while (((Sign = (int)(unsigned char)*s - (int)(unsigned char)*P) == 0) && *s && *P)
                            {
                            ++s;
                            ++P;
                            }
                        if (Sign >= 0)
                            {
                            if (s >= cutoff)
                                {
                                *suggestedCutOff = (*P) ? s + 1 : s;
                                }
                            return TRUE;
                            }
                        return ONCE;
                }
            }
        else
#endif
            {
#if CUTOFFSUGGEST
            switch (Flgs & (NOT | GREATER_THAN | SMALLER_THAN))
                {
#if 1
                    case NOT | GREATER_THAN | SMALLER_THAN:    /* n:~<>p */
                    case 0:                                /* n:p */
                        while (((Sign = (s < cutoff ? (int)(unsigned char)*s : 0) - (int)(unsigned char)*P) == 0) && *s && *P)
                            {
                            ++s;
                            ++P;
                            }
                        if (Sign != 0 && mayMoveStartOfSubject && *mayMoveStartOfSubject != 0)
                            {
                            char* startpos;
                            char* ep = SPOBJ(p) + strlen((char*)POBJ(p));
                            char* es = cutoff ? cutoff : S + strlen((char*)S);
                            while (ep > SPOBJ(p))
                                {
                                if (*--ep != *--es)
                                    {
                                    *mayMoveStartOfSubject = 0;
                                    return ONCE;
                                    }
                                }
                            startpos = es;
                            if (Flgs & MINUS)
                                --startpos;
                            if ((char*)startpos > *mayMoveStartOfSubject)
                                *mayMoveStartOfSubject = (char*)startpos;
                            return ONCE;
                            }

                        break;
#endif
                    default:
#else
                        {
#endif
                        while (((Sign = (s < cutoff ? (int)(unsigned char)*s : 0) - (int)(unsigned char)*P) == 0) && *P) /* Additional argument cutoff */
                            {
                            ++s;
                            ++P;
                            }
                        }
                }
            }


    switch (Flgs & (NOT | GREATER_THAN | SMALLER_THAN))
        {
            case NOT | GREATER_THAN | SMALLER_THAN:    /* n:~<>p */
            case 0:                                /* n:p */
                {
                /*
                            n:p        n == p
                            n:~<>p  same as n:p
                                [n == p]
                                    TRUE | ONCE
                                [n > p]
                                    ONCE
                                [n < p]
                                    FALSE
                */
                if (Sign == 0)
                    {
                    return TRUE | ONCE;
                    }
                else if (Sign < 0 && s >= cutoff)
                    {
                    return FALSE;
                    }
                return ONCE;
                }
            case SMALLER_THAN:    /* n:<p */
                {
                /*
                            n:<p    n < p
                                [n == p]
                                    ONCE
                                [n > p]
                                    ONCE
                                [n < p]
                                    TRUE
                */
                if (Sign >= 0)
                    {
                    return ONCE;
                    }
                return TRUE;
                }
            case GREATER_THAN:    /* n:>p */
                {
                /*
                            n:>p    n > p
                                [n == p]
                                    FALSE
                                [n > p]
                                    TRUE
                                [n < p]
                                    FALSE
                */
                if (Sign > 0)
                    {
                    return TRUE;
                    }
                else if (Sign < 0 && s < cutoff)
                    {
                    return ONCE;
                    }
                return FALSE;
                }
            case GREATER_THAN | SMALLER_THAN:    /* n:<>p */
            case NOT:                        /* n:~p */
                {
                /*
                            n:<>p   n != p
                            n:~p    same as n:<>p
                                [n == p]
                                    FALSE
                                [n > p]
                                    TRUE
                                [n < p]
                                    TRUE

                */
                if (Sign == 0)
                    {
                    return FALSE;
                    }
                return TRUE;
                }
            case NOT | SMALLER_THAN:    /* n:~<p */
                {
                /*
                            n:~<p   n >= p
                                [n == p]
                                    TRUE
                                [n > p]
                                    TRUE
                                [n < p]
                                    FALSE
                */
                if (Sign < 0)
                    {
                    if (s < cutoff)
                        {
                        return ONCE;
                        }
                    else
                        {
                        return FALSE;
                        }
                    }
                return TRUE;
                }
            case NOT | GREATER_THAN:    /* n:~>p */
            default:
                {
                /*
                            n:~>p   n <= p
                                [n == p]
                                    TRUE | ONCE
                                [n > p]
                                    ONCE
                                [n < p]
                                    TRUE
                */
                if (Sign > 0)
                    {
                    return ONCE;
                    }
                else if (Sign < 0)
                    {
                    return TRUE;
                    }
                return TRUE | ONCE;
                }
        }
        }

#if INTSCMP
        int intscmp(LONG* s1, LONG* s2) /* this routine produces different results
                                          depending on BIGENDIAN */
            {
            while (*((char*)s1 + 3))
                {
                if (*s1 != *s2)
                    {
                    if (*s1 < *s2)
                        return -1;
                    else
                        return 1;
                    }
                s1++;
                s2++;
                }
            if (*s1 != *s2)
                {
                if (*s1 < *s2)
                    return -1;
                else
                    return 1;
                }
            else
                return 0;
            }
#endif
