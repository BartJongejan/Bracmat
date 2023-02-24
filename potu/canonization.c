#include "canonization.h"
#include "globals.h"
#include "branch.h"
#include "variables.h"
#include "copy.h"
#include "nodedefs.h"
#include "eval.h"
#include "wipecopy.h"
#include "input.h"
#include "rational.h"
#include "real.h"
#include "memory.h"
#include "treematch.h"
#include "nodeutil.h"
#include "equal.h"
#include "binding.h"
#include <assert.h>
#include <string.h>

#define MPI 3.14159265358979323846 
#define ME  2.718281828459045090795598298427648842334747314453125

static const char
hash5[] = "\5",
hash6[] = "\6";

static psk tryq(psk Pnode, psk fun, Boolean* ok)
    {
    psk anchor;
    psh(&argNode, Pnode, NULL);
    Pnode->v.fl |= READY;

    anchor = subtreecopy(fun->RIGHT);

    psh(fun->LEFT, &zeroNode, NULL);
    anchor = eval(anchor);
    pop(fun->LEFT);
    if(anchor->v.fl & SUCCESS)
        {
        *ok = TRUE;
        wipe(Pnode);
        Pnode = anchor;
        }
    else
        {
        *ok = FALSE;
        wipe(anchor);
        }
    deleteNode(&argNode);
    return Pnode;
    }

static int absone(psk pnode)
    {
    char* pstring;
    pstring = SPOBJ(pnode);
    return(*pstring == '1' && *++pstring == 0);
    }

psk handleExponents(psk Pnode)
    {
    psk lnode;
    Boolean done = FALSE;
    for(; ((lnode = Pnode->LEFT)->v.fl & READY) && Op(lnode) == EXP;)
        {
        done = TRUE;
        Pnode->LEFT = lnode = isolated(lnode);
        lnode->v.fl &= ~READY & ~OPERATOR;/* turn off READY flag */
        lnode->v.fl |= TIMES;
        addr[1] = lnode->LEFT;
        addr[2] = lnode->RIGHT;
        addr[3] = Pnode->RIGHT;
        Pnode = build_up(Pnode, "(\1^(\2*\3))", NULL);
        }
    if(done)
        {
        return Pnode;
        }
    else
        {
        static const char* conc_arr[] = { NULL,NULL,NULL,NULL,NULL,NULL };

        Qnumber iexponent, hiexponent;

        psk rightnode;
        if(!is_op(rightnode = Pnode->RIGHT))
            {
            if(RAT_NUL(rightnode))
                {
                wipe(Pnode);
                return copyof(&oneNode);
                }
            if(IS_ONE(rightnode))
                {
                return leftbranch(Pnode);
                }
            }
        lnode = Pnode->LEFT;
        if(!is_op(lnode))
            {
            if((RAT_NUL(lnode) && !RAT_NEG_COMP(rightnode)) || IS_ONE(lnode))
                {
                return leftbranch(Pnode);
                }

            if(!is_op(rightnode)) 
                {
                if(REAL_COMP(rightnode) && REAL_COMP(lnode))
                    {
                    /*"2.0E0" ^ "5.0E-1"*/
                    conc_arr[1] = NULL;
                    conc_arr[2] = hash6;
                    conc_arr[3] = NULL;
                    addr[6] = fExp(lnode, rightnode);
                    assert(lnode == Pnode->LEFT);
                    Pnode = vbuildup(Pnode, conc_arr + 2);
                    wipe(addr[6]);
                    return Pnode;
                    }
                else if(RATIONAL_COMP(rightnode))
                    {
                    if(RATIONAL_COMP(lnode))
                        {
                        if(RAT_NEG_COMP(rightnode) && absone(rightnode))
                            {
                            conc_arr[1] = NULL;
                            conc_arr[2] = hash6;
                            conc_arr[3] = NULL;
                            addr[6] = qqDivide(&oneNode, lnode);
                            assert(lnode == Pnode->LEFT);
                            Pnode = vbuildup(Pnode, conc_arr + 2);
                            wipe(addr[6]);
                            return Pnode;
                            }
                        else if(RAT_NEG_COMP(lnode) && RAT_RAT_COMP(rightnode))
                            {
                            return Pnode; /*{?} -3^2/3 => -3^2/3 */
                            }
                        /* Missing here is n^m, with m > 2.
                           That case is handled in casemacht. */
                        }
                    else if(PLOBJ(lnode) == IM)
                        {
                        if(qCompare(rightnode, &zeroNode) & MINUS)
                            { /* i^-n -> -i^n */ /*{?} i^-7 => i */
                              /* -i^-n -> i^n */ /*{?} -i^-7 => -i */
                            conc_arr[0] = "(\2^\3)";
                            addr[2] = qTimesMinusOne(lnode);
                            addr[3] = qTimesMinusOne(rightnode);
                            conc_arr[1] = NULL;
                            Pnode = vbuildup(Pnode, conc_arr);
                            wipe(addr[2]);
                            wipe(addr[3]);
                            return Pnode;
                            }
                        else if(qCompare(&twoNode, rightnode) & (QNUL | MINUS))
                            {
                            iexponent = qModulo(rightnode, &fourNode);
                            if(iexponent->v.fl & QNUL)
                                {
                                wipe(Pnode); /*{?} i^4 => 1 */
                                Pnode = copyof(&oneNode);
                                }
                            else
                                {
                                int Sign;
                                Sign = qCompare(iexponent, &twoNode);
                                if(Sign & QNUL)
                                    {
                                    wipe(Pnode);
                                    Pnode = copyof(&minusOneNode);
                                    }
                                else
                                    {
                                    if(!(Sign & MINUS))
                                        {
                                        hiexponent = iexponent;
                                        iexponent = qPlus(&fourNode, hiexponent, MINUS);
                                        wipe(hiexponent);
                                        }
                                    addr[2] = lnode;
                                    addr[6] = iexponent;
                                    conc_arr[0] = "(-1*\2)^";
                                    conc_arr[1] = "(\6)";
                                    conc_arr[2] = NULL;
                                    Pnode = vbuildup(Pnode, conc_arr);
                                    }
                                }
                            wipe(iexponent);
                            return Pnode;
                            }
                        }
                    }
                }
            }

        if(Op(lnode) == TIMES)
            {
            addr[1] = lnode->LEFT;
            addr[2] = lnode->RIGHT;
            addr[3] = Pnode->RIGHT;
            return build_up(Pnode, "\1^\3*\2^\3", NULL);
            }

        if(RATIONAL_COMP(lnode))
            {
            static const char parenonepow[] = "(\1^";
            if(INTEGER_NOT_NUL_COMP(rightnode) && !absone(rightnode))
                {
                addr[1] = lnode;
                if(INTEGER_POS_COMP(rightnode))
                    {
                    if(qCompare(&twoNode, rightnode) & MINUS)
                        {
                        /* m^n = (m^(n\2))^2*m^(n mod 2) */ /*{?} 9^7 => 4782969 */
                        conc_arr[0] = parenonepow;
                        conc_arr[1] = hash5;
                        conc_arr[3] = hash6;
                        conc_arr[4] = NULL;
                        addr[5] = qIntegerDivision(rightnode, &twoNode);
                        conc_arr[2] = ")^2*\1^";
                        addr[6] = qModulo(rightnode, &twoNode);
                        Pnode = vbuildup(Pnode, conc_arr);
                        wipe(addr[5]);
                        wipe(addr[6]);
                        }
                    else
                        {
                        /* m^2 = m*m */
                        Pnode = build_up(Pnode, "(\1*\1)", NULL);
                        }
                    }
                else
                    {
                    /*{?} 7^-13 => 1/96889010407 */
                    conc_arr[0] = parenonepow;
                    conc_arr[1] = hash6;
                    addr[6] = qTimesMinusOne(rightnode);
                    conc_arr[2] = ")^-1";
                    conc_arr[3] = 0;
                    Pnode = vbuildup(Pnode, conc_arr);
                    wipe(addr[6]);
                    }
                return Pnode;
                }
            else if(RAT_RAT(rightnode))
                {
                char** conc, slash = 0;
                int wipe[20], ind;
                nnumber numerator = { 0 }, denominator = { 0 };
                for(ind = 0; ind < 20; wipe[ind++] = TRUE);
                ind = 0;
                conc = (char**)bmalloc(__LINE__, 20 * sizeof(char**));
                /* 20 is safe value for ULONGs */
                addr[1] = Pnode->RIGHT;
                if(RAT_RAT_COMP(Pnode->LEFT))
                    {
                    split(Pnode->LEFT, &numerator, &denominator);
                    if(!subroot(&numerator, conc, &ind))
                        {
                        wipe[ind] = FALSE;
                        conc[ind++] = numerator.number;
                        slash = numerator.number[numerator.length];
                        numerator.number[numerator.length] = 0;

                        wipe[ind] = FALSE;
                        conc[ind++] = "^\1";
                        }
                    wipe[ind] = FALSE;
                    conc[ind++] = "*(";
                    if(!subroot(&denominator, conc, &ind))
                        {
                        wipe[ind] = FALSE;
                        conc[ind++] = denominator.number;
                        wipe[ind] = FALSE;
                        conc[ind++] = "^\1";
                        }
                    wipe[ind] = FALSE;
                    conc[ind++] = ")^-1";
                    }
                else
                    {
                    numerator.number = (char*)POBJ(Pnode->LEFT);
                    numerator.alloc = NULL;
                    numerator.length = strlen(numerator.number);
                    if(!subroot(&numerator, conc, &ind))
                        {
                        bfree(conc);
                        return Pnode;
                        }
                    }
                conc[ind--] = NULL;
                Pnode = vbuildup(Pnode, (const char**)conc);
                if(slash)
                    numerator.number[numerator.length] = slash;
                for(; ind >= 0; ind--)
                    if(wipe[ind])
                        bfree(conc[ind]);
                bfree(conc);
                return Pnode;
                }
            }
        }
    if(is_op(Pnode->RIGHT))
        {
        int ok;
        Pnode = tryq(Pnode, f4, &ok);
        }
    return Pnode;
    }

/*
Improvement that DOES evaluate b+(i*c+i*d)+-i*c
It also allows much deeper structures, because the right place for insertion
is found iteratively, not recursively. This also causes some operations to
be tremendously faster. e.g. (1+a+b+c)^30+1&ready evaluates in about
4,5 seconds now, previously in 330 seconds! (AST Bravo MS 5233M 233 MHz MMX Pentium)
*/
static void splitProduct_number_im_rest(psk pnode, ppsk Nm, ppsk I, ppsk NNNI)
    {
    psk temp;
    if(Op(pnode) == TIMES)
        {
        if(RATIONAL_COMP(pnode->LEFT))
            {/* 17*x */
            *Nm = pnode->LEFT;
            temp = pnode->RIGHT;
            }/* Nm*temp */
        else
            {
            *Nm = NULL;
            temp = pnode;
            }/* temp */
        if(Op(temp) == TIMES)
            {
            if(!is_op(temp->LEFT) && PLOBJ(temp->LEFT) == IM)
                {/* Nm*i*x */
                *I = temp->LEFT;
                *NNNI = temp->RIGHT;
                }/* Nm*I*NNNI */
            else
                {
                *I = NULL;
                *NNNI = temp;
                }/* Nm*NNNI */
            }
        else
            {
            if(!is_op(temp) && PLOBJ(temp) == IM)
                {/* Nm*i */
                *I = temp;
                *NNNI = NULL;
                }/* Nm*I */
            else
                {
                *I = NULL;
                *NNNI = temp;
                }/* Nm*NNNI */
            }
        }
    else if(!is_op(pnode) && PLOBJ(pnode) == IM)
        {/* i */
        *Nm = NULL;
        *I = pnode;
        *NNNI = NULL;
        }/* I */
    else
        {/* x */
        *Nm = NULL;
        *I = NULL;
        *NNNI = pnode;
        }/* NNNI */
    }

#if EXPAND
static psk expandDummy(psk Pnode, int* ok)
    {
    *ok = FALSE;
    return Pnode;
    }
#endif

static psk expandProduct(psk Pnode, int* ok)
    {
    switch(Op(Pnode))
        {
        case TIMES:
        case EXP:
            {
            if(((match(0, Pnode, m0, NULL, 0, Pnode, 3333) & TRUE)
                && ((Pnode = tryq(Pnode, f0, ok)), *ok)
                )
               || ((match(0, Pnode, m1, NULL, 0, Pnode, 4444) & TRUE)
                   && ((Pnode = tryq(Pnode, f1, ok)), *ok)
                   )
               )
                {
                if(is_op(Pnode)) /*{?} (1+i)*(1+-i)+Z => 2+Z */
                    Pnode->v.fl &= ~READY;
                return Pnode;
                }
            break;
            }
        }
    *ok = FALSE;
    return Pnode;
    }

psk mergeOrSortTerms(psk Pnode)
    {
    /*
    Split Pnode in left L and right R argument

    If L is zero,
        return R

    If R is zero,
        return L

    If L is a product containing a sum,
        expand it

    If R is a product containing a sum,
        expand it

    Find the proper place split of R into Rhead , RtermS,  RtermGE and Rtail for L to insert into:
        L + R -> Rhead + RtermS + L + RtermGE + Rtail:
    Start with Rhead = NIL, RtermS = NIL  RtermGE is first term of R and Rtail is remainder of R
    Split L into Lterm and Ltail
    If Lterm is a number
        if RtermGE is a number
            return sum(Lterm,RtermGE) + Ltail + Rtail
        else
            return Lterm + Ltail + R
    Else if Rterm is a number
        return Rterm + L + Rtail
    Else
        get the non-numerical factor LtermNN of Lterm
        if LtermNN is imaginary
            get the nonimaginary factors of LtermNN (these may also include 'e' and 'pi') LtermNNNI
            find Rhead,  RtermS, RtermGE and Rtail
                such that Rhead does contain all non-imaginary terms
                and such that RtermGE and Rtail
                    either are NIL
                    or RtermGE is imaginary
                        and (RtermS is NIL or RtermSNNNI <  LtermNNNI) and LtermNNNI <= RtermGENNNI
            if RtermGE is NIL
                return R + L
            else
                if LtermNNNI < RtermGENNNI
                    return Rhead + RtermS + L + RtermGE + Rtail
                else
                    return Rhead + RtermS + sum(L,RtermGE) + Rtail
        else
            find Rhead,  RtermS, RtermGE and Rtail
                such that RtermGE and Rtail
                    either are NIL
                    or (RtermS is NIL or RtermSNN <  LtermNN) and LtermNN <= RtermGENN
            if RtermGE is NIL
                return R + L
            else
                if LtermNN < RtermGENN
                    return Rhead + RtermS + L + RtermGE + Rtail
                else
                    return Rhead + RtermS + sum(L,RtermGE) + Rtail


    */
    static const char* conc[] = { NULL,NULL,NULL,NULL };
    int res = FALSE;
    psk top = Pnode;

    psk L = top->LEFT;
    psk Lterm, Ltail;
    psk LtermN, LtermI, LtermNNNI;

    psk R;
    psk Rterm, Rtail;
    psk RtermN, RtermI, RtermNNNI;

    int ok;
    if(!is_op(L) && RAT_NUL_COMP(L))
        {
        /* 0+x -> x */
        return rightbranch(top);
        }

    R = top->RIGHT;
    if(!is_op(R) && RAT_NUL_COMP(R))
        {
        /*{?} x+0 => x */
        return leftbranch(top);
        }

    if(is_op(L)
       && ((top->LEFT = expandProduct(top->LEFT, &ok)), ok)
       )
        {
        res = TRUE;
        }
    if(is_op(R)
       && ((top->RIGHT = expandProduct(top->RIGHT, &ok)), ok)
       )
        { /*
          {?} a*b+u*(x+y) => a*b+u*x+u*y
          */
        res = TRUE;
        }
    if(res)
        {
        Pnode->v.fl &= ~READY;
        return Pnode;
        }
    rightoperand_and_tail(top, &Rterm, &Rtail);
    leftoperand_and_tail(top, &Lterm, &Ltail);
    assert(Ltail == NULL);

    if(REAL_COMP(Lterm))
        {
        switch(PLOBJ(Rterm))
            {
            case PI:
            case XX:
                conc[0] = hash6;
                /* "4.0E0"+"7.0E0" -> "1.1E1" */
                addr[6] = dfPlus(Lterm, PLOBJ(Rterm) == PI ? MPI : ME);
                conc[1] = NULL;
                conc[2] = NULL;
                if(Rtail != NULL)
                    {
                    addr[4] = Rtail;
                    conc[1] = "+\4";
                    }
                Pnode = vbuildup(top, conc);
                wipe(addr[6]);
                return Pnode;
            default:
                if(REAL_COMP(Rterm))
                    {
                    conc[0] = hash6;
                    /* "4.0E0"+"7.0E0" -> "1.1E1" */
                    addr[6] = fPlus(Lterm, Rterm);
                    conc[1] = NULL;
                    conc[2] = NULL;
                    if(Rtail != NULL)
                        {
                        addr[4] = Rtail;
                        conc[1] = "+\4";
                        }
                    Pnode = vbuildup(top, conc);
                    wipe(addr[6]);
                    }
                return Pnode;
            }
        }
    else if(RATIONAL_COMP(Lterm))
        {
        if(RATIONAL_COMP(Rterm))
            {
            conc[0] = hash6;
            if(Lterm == Rterm)
                {
                /* 7+7 -> 2*7 */ /*{?} 7+7 => 14 */
                addr[6] = qTimes(&twoNode, Rterm);
                }
            else
                {
                /* 4+7 -> 11 */ /*{?} 4+7 => 11 */
                addr[6] = qPlus(Lterm, Rterm, 0);
                }
            conc[1] = NULL;
            conc[2] = NULL;
            if(Rtail != NULL)
                {
                addr[4] = Rtail;
                conc[1] = "+\4";
                }
            Pnode = vbuildup(top, conc);
            wipe(addr[6]);
            }
        return Pnode;
        }
    else if(RATIONAL_COMP(Rterm))
        {
        addr[1] = Rterm;
        addr[2] = L;
        if(Rtail)
            {
            /* How to get here?
                   (1+a)*(1+b)+c+(1+d)*(1+f)
            The lhs (1+a)*(1+b) is not expanded before the merge starts
            Comparing (1+a)*(1+b) with 1, c, d, f and d*f, this product lands
            after d*f. Thereafter (1+a)*(1+b) is expanded, giving a
            numeral 1 in the middle of the expression:
                   1+c+d+f+d*f+(1+a)*(1+b)
                   1+c+d+f+d*f+1+a+b+a*b
            Rterm is 1+a+b+a*b
            Rtail is a+b+a*b
            */
            addr[3] = Rtail;
            return build_up(top, "\1+\2+\3", NULL);
            }
        else
            {
            /* 4*(x+7)+p+-4*x */
            return build_up(top, "\1+\2", NULL);
            }
        }

    if(Op(Lterm) == LOG
       && Op(Rterm) == LOG
       && !equal(Lterm->LEFT, Rterm->LEFT)
       )
        {
        addr[1] = Lterm->LEFT;
        addr[2] = Lterm->RIGHT;
        addr[3] = Rterm->RIGHT;
        if(Rtail == NULL)
            return build_up(top, "\1\016(\2*\3)", NULL); /*{?} 2\L3+2\L9 => 4+2\L27/16 */
        else
            {
            addr[4] = Rtail;
            return build_up(top, "\1\016(\2*\3)+\4", NULL); /*{?} 2\L3+2\L9+3\L123 => 8+2\L27/16+3\L41/27 */
            }
        }

    splitProduct_number_im_rest(Lterm, &LtermN, &LtermI, &LtermNNNI);

    if(LtermI)
        {
        ppsk runner = &Pnode;
        splitProduct_number_im_rest(Rterm, &RtermN, &RtermI, &RtermNNNI);
        while(RtermI == NULL
              && Op((*runner)->RIGHT) == PLUS
              )
            {
            runner = &(*runner)->RIGHT;
            *runner = isolated(*runner);
            rightoperand_and_tail((*runner), &Rterm, &Rtail);
            splitProduct_number_im_rest(Rterm, &RtermN, &RtermI, &RtermNNNI);/*{?} i*x+-i*x+a => a */
            }
        if(RtermI != NULL)
            {                        /*{?} i*x+-i*x => 0 */
            int indx;
            int dif;
            if(LtermNNNI == NULL)
                {
                dif = RtermNNNI == NULL ? 0 : -1;/*{?} i+-i*x => i+-i*x */
                                                 /*{?} i+-i => 0 */
                }
            else
                {
                assert(RtermNNNI != NULL);
                dif = equal(LtermNNNI, RtermNNNI);
                }
            if(dif == 0)
                {                        /*{?} i*x+-i*x => 0 */
                if(RtermN)
                    {
                    addr[2] = RtermN; /*{?} i*x+3*i*x => 4*i*x */
                    if(LtermN == NULL)
                        {
                        /* a+n*a */ /*{?} i*x+3*i*x => 4*i*x */
                        if(HAS_MINUS_SIGN(LtermI))
                            {  /*{?} -i*x+3*i*x => 2*i*x */
                            if(HAS_MINUS_SIGN(RtermI))
                                {
                                conc[0] = "(1+\2)*-i";/*{?} -i*x+3*-i*x => 4*-i*x */
                                }
                            else
                                {
                                conc[0] = "(-1+\2)*i";/*{?} -i*x+3*i*x => 2*i*x */
                                }
                            }
                        else if(HAS_MINUS_SIGN(RtermI))
                            {
                            conc[0] = "(-1+\2)*-i"; /*{?} i*x+3*-i*x => 2*-i*x */
                            }
                        else
                            {
                            conc[0] = "(1+\2)*i";/*{?} i*x+3*i*x => 4*i*x */
                            }
                        }
                    /* (1+n)*a */
                    else
                        {
                        /* n*a+m*a */ /*{?} 3*-i*x+-3*i*x => 6*-i*x */
                        addr[3] = LtermN;
                        if(HAS_MINUS_SIGN(LtermI))
                            {
                            if(HAS_MINUS_SIGN(RtermI))
                                {
                                conc[0] = "(\3+\2)*-i";/*{?} 3*-i*x+-3*i*x => 6*-i*x */
                                }
                            else
                                {
                                conc[0] = "(-1*\3+\2)*i";/*{?} 3*-i*x+-3*-i*x => 0 */
                                }
                            }
                        else if(HAS_MINUS_SIGN(RtermI))
                            {
                            conc[0] = "(\3+-1*\2)*i"; /*{?} 3*i*x+-3*i*x => 0 */
                            }
                        else
                            {
                            conc[0] = "(\3+\2)*i"; /*{?} 3*i*x+4*i*x => 7*i*x */
                            }
                        /* (n+m)*a */
                        }
                    }
                else
                    {                        /*{?} i*x+-i*x => 0 */
                    addr[1] = LtermNNNI;
                    if(LtermN != NULL)
                        {
                        /* m*a+a */
                        addr[2] = LtermN; /*{?} 3*i*x+i*x => 4*i*x */
                        if(HAS_MINUS_SIGN(LtermI))

                            if(HAS_MINUS_SIGN(RtermI))
                                {
                                conc[0] = "(1+\2)*-i";/*{?} 3*-i*x+-i*x => 4*-i*x */
                                }
                            else
                                {
                                conc[0] = "(-1+\2)*-i";/*{?} 3*-i*x+i*x => 2*-i*x */
                                }

                        else if(HAS_MINUS_SIGN(RtermI))
                            {
                            conc[0] = "(-1+\2)*i"; /*{?} 3*i*x+-i*x => 2*i*x */
                            }
                        else
                            {
                            conc[0] = "(1+\2)*i"; /*{?} 3*i*x+i*x => 4*i*x */
                            }
                        /* (1+m)*a */
                        }
                    else
                        {
                        /* a+a */                        /*{?} i*x+-i*x => 0 */
                        if(HAS_MINUS_SIGN(LtermI))

                            if(HAS_MINUS_SIGN(RtermI))
                                {
                                conc[0] = "2*-i"; /*{?} -i+-i => 2*-i */
                                }
                            else
                                {
                                conc[0] = "0"; /*{?} -i+i => 0 */
                                }

                        else if(HAS_MINUS_SIGN(RtermI))
                            {
                            conc[0] = "0";                        /*{?} i*x+-i*x => 0 */
                            }
                        else
                            {
                            conc[0] = "2*i"; /*{?} i+i => 2*i */
                            }
                        }
                    /* 2*a */
                    }
                if(LtermNNNI != NULL)
                    {                        /*{?} i*x+-i*x => 0 */
                    addr[1] = RtermNNNI;
                    conc[1] = "*\1";
                    indx = 2;
                    }
                else
                    indx = 1; /*{?} i+-i => 0 */
                if(Rtail != NULL)
                    {
                    addr[4] = Rtail; /*{?} -i+-i+i*y => 2*-i+i*y */
                    conc[indx++] = "+\4";
                    }
                conc[indx] = NULL;                        /*{?} i*x+-i*x => 0 */
                (*runner)->RIGHT = vbuildup((*runner)->RIGHT, conc);
                if(runner != &Pnode)
                    {
                    (*runner)->v.fl &= ~READY;/*{?} i*x+-i*x+a => a */
                    *runner = eval(*runner);
                    }
                return rightbranch(top);                        /*{?} i*x+-i*x => 0 */
                }
            assert(Ltail == NULL);
            }
        else  /* LtermI != NULL && RtermI == NULL */
            {
            addr[1] = Rterm; /*{?} i*x+-i*x+a => a */
            addr[2] = L;
            (*runner)->RIGHT = build_up((*runner)->RIGHT, "\1+\2", NULL);
            (*runner)->RIGHT->v.fl |= READY;
            return rightbranch(top);
            }
        }
    else /* LtermI == NULL */
        {
        ppsk runner = &Pnode;
        int dif = 1;
        assert(LtermNNNI != NULL);
        splitProduct_number_im_rest(Rterm, &RtermN, &RtermI, &RtermNNNI);
        while(RtermNNNI != NULL
              && RtermI == NULL
              && (dif = cmp(LtermNNNI, RtermNNNI)) > 0
              && Op((*runner)->RIGHT) == PLUS
              )
            {
            /*
            x^(y*(a+b))+z^(y*(a+b))+-1*x^(a*y+b*y) => z^(y*(a+b))
            cos$(a+b)+-1*(cos$a*cos$b+-1*sin$a*sin$b) => 0
            x^(y*(a+b))+x^(a*y+b*y) => 2*x^(y*(a+b))
            x^(y*(a+b))+-1*x^(a*y+b*y) => 0
            fct$(a^((b+c)*(d+e))+a^((b+c)*(d+f))) => a^(b*d+c*d) * (a^(b*f+c*f)+a^(e*b+e*c))
            (a^((b+c)*(d+e))+a^((b+c)*(d+f))) + -1 * a^(b*d+c*d) * (a^(b*f+c*f)+a^(e*b+e*c)) => 0
            -1*(a^((b+c)*(d+e))+a^((b+c)*(d+f)))  +   a^(b*d+c*d) * (a^(b*f+c*f)+a^(e*b+e*c)) => 0
            a^(b*d+c*d) * (a^(b*f+c*f)+a^(e*b+e*c)) + -1*(a^((b+c)*(d+e))+a^((b+c)*(d+f))) => 0
            -1 * a^(b*d+c*d) * (a^(b*f+c*f)+a^(e*b+e*c)) + a^((b+c)*(d+e)) + a^((b+c)*(d+f)) => 0
            */
            runner = &(*runner)->RIGHT; /*{?} (b^3+c^3)+b^3+c+b^3 => c+3*b^3+c^3 */
            *runner = isolated(*runner);
            rightoperand_and_tail((*runner), &Rterm, &Rtail);
            splitProduct_number_im_rest(Rterm, &RtermN, &RtermI, &RtermNNNI);
            }
        if(RtermI != NULL)
            dif = -1; /*{?} (-i+a)+-i => a+2*-i */
        if(dif == 0)
            {
            if(RtermN)
                {
                addr[1] = RtermNNNI;
                addr[2] = RtermN;
                if(LtermN == NULL)
                    /*{?} a+n*a => a+a*n */
                    conc[0] = "(1+\2)*\1";
                /* (1+n)*a */
                else
                    {
                    /*{?} n*a+m*a => a*m+a*n */ /*{?} 7*a+9*a => 16*a */
                    addr[3] = LtermN;
                    conc[0] = "(\3+\2)*\1";
                    /* (n+m)*a */
                    }
                }
            else
                {
                addr[1] = LtermNNNI;
                if(LtermN != NULL)
                    {
                    /* m*a+a */
                    addr[2] = LtermN;
                    conc[0] = "(1+\2)*\1"; /*{?} 3*a+a => 4*a */
                    /* (1+m)*a */
                    }
                else
                    {
                    /*{?} a+a => 2*a */
                    conc[0] = "2*\1";
                    }
                /* 2*a */
                }
            assert(Ltail == NULL);
            conc[1] = NULL;
            conc[2] = NULL;
            if(Rtail != NULL)
                {
                addr[4] = Rtail;
                conc[1] = "+\4";
                }
            (*runner)->RIGHT = vbuildup((*runner)->RIGHT, conc);
            if(runner != &Pnode)
                {
                (*runner)->v.fl &= ~READY;
                /*{?} (b^3+c^3)+b^3+c+b^3 => c+3*b^3+c^3 */
                /* This would evaluate to c+(1+2)*b^3+c^3 if READY flag wasn't turned off */
                /* Correct evaluation: c+3*b^3+c^3 */
                *runner = eval(*runner);
                }
            return rightbranch(top);
            }
        else if(dif > 0)  /*{?} b+a => a+b */
            {
            addr[1] = Rterm;
            addr[2] = L;
            (*runner)->RIGHT = build_up((*runner)->RIGHT, "\1+\2", NULL);
            (*runner)->RIGHT->v.fl |= READY;
            return rightbranch(top);
            }
        else if((*runner) != top) /* b + a + c */
            {
            addr[1] = L;
            addr[2] = (*runner)->RIGHT;
            (*runner)->RIGHT = build_up((*runner)->RIGHT, "\1+\2", NULL);
            (*runner)->RIGHT = eval((*runner)->RIGHT);
            return rightbranch(top);          /* (1+a+b+c)^30+1 */
            }
        assert(Ltail == NULL);
        }
    return Pnode;
    }


psk substtimes(psk Pnode)
    {
    static const char* conc[] = { NULL,NULL,NULL,NULL };
    psk rkn, lkn;
    psk rvar, lvar;
    psk temp, llnode, rlnode;
    int nodedifference;
    rkn = rightoperand(Pnode);

    if(is_op(rkn))
        rvar = NULL; /* (f.e)*(y.s) */
    else
        {
        if(IS_ONE(rkn))
            {
            return leftbranch(Pnode); /*{?} (a=7)&!(a*1) => 7 */
            }
        else if(RAT_NUL(rkn))/*{?} -1*140/1000 => -7/50 */
            {
            wipe(Pnode); /*{?} x*0 => 0 */
            return copyof(&zeroNode);
            }
        rvar = rkn; /*{?} a*a => a^2 */
        }

    lkn = Pnode->LEFT;
    if(!is_op(lkn))
        {
        if(RAT_NUL(lkn)) /*{?} -1*140/1000 => -7/50 */
            {
            wipe(Pnode); /*{?} 0*x => 0 */
            return copyof(&zeroNode);
            }
        lvar = lkn;

        if(IS_ONE(lkn))
            {
            return rightbranch(Pnode); /*{?} 1*-1 => -1 */
            }
        else if(RATIONAL_COMP(lkn) && rvar)
            {
            if(RATIONAL_COMP(rkn))
                {
                if(rkn == lkn)
                    lvar = (Pnode->LEFT = isolated(lkn)); /*{?} 1/10*1/10 => 1/100 */
                conc[0] = hash6;
                addr[6] = qTimes(rvar, lvar);
                if(rkn == Pnode->RIGHT)
                    conc[1] = NULL; /*{?} -1*140/1000 => -7/50 */
                else
                    {
                    addr[1] = Pnode->RIGHT->RIGHT; /*{?} -1*1/4*e^(2*i*x) => -1/4*e^(2*i*x) */
                    conc[1] = "*\1";
                    }
                Pnode = vbuildup(Pnode, conc);
                wipe(addr[6]);
                return Pnode;
                }
            else
                {
                if(PLOBJ(rkn) == IM && RAT_NEG_COMP(lkn))
                    {
                    conc[0] = "(\2*\3)";
                    addr[2] = qTimesMinusOne(lkn);
                    addr[3] = qTimesMinusOne(rkn);
                    if(rkn == Pnode->RIGHT)
                        conc[1] = NULL; /*{?} -1*i => -i */
                    else
                        {
                        addr[1] = Pnode->RIGHT->RIGHT; /*{?} -3*i*x => 3*-i*x */
                        conc[1] = "*\1";
                        }
                    Pnode = vbuildup(Pnode, conc);
                    wipe(addr[2]);
                    wipe(addr[3]);
                    return Pnode;
                    }
                }
            }
        else if(REAL_COMP(lkn) && rvar)
            {
            switch(PLOBJ(rkn))
                {
                case PI:
                case XX:
                    conc[0] = hash6;
                    addr[6] = dfTimes(PLOBJ(rkn) == PI ? MPI : ME, lvar);
                    if(rkn == Pnode->RIGHT)
                        conc[1] = NULL; /*{?} -1*140/1000 => -7/50 */
                    else
                        {
                        addr[1] = Pnode->RIGHT->RIGHT; /*{?} -1*1/4*e^(2*i*x) => -1/4*e^(2*i*x) */
                        conc[1] = "*\1";
                        }
                    Pnode = vbuildup(Pnode, conc);
                    wipe(addr[6]);
                    return Pnode;
                default:
                    if(REAL_COMP(rkn))
                        {
                        if(rkn == lkn)
                            lvar = (Pnode->LEFT = isolated(lkn)); /*{?} 1/10*1/10 => 1/100 */
                        conc[0] = hash6;
                        addr[6] = fTimes(rvar, lvar);
                        if(rkn == Pnode->RIGHT)
                            conc[1] = NULL; /*{?} -1*140/1000 => -7/50 */
                        else
                            {
                            addr[1] = Pnode->RIGHT->RIGHT; /*{?} -1*1/4*e^(2*i*x) => -1/4*e^(2*i*x) */
                            conc[1] = "*\1";
                            }
                        Pnode = vbuildup(Pnode, conc);
                        wipe(addr[6]);
                        return Pnode;
                        }
                }
            }
        }

    rlnode = Op(rkn) == EXP ? rkn->LEFT : rkn; /*{?} (f.e)*(y.s) => (f.e)*(y.s) */
    llnode = Op(lkn) == EXP ? lkn->LEFT : lkn;
    if((nodedifference = equal(llnode, rlnode)) == 0)
        {
        /* a^n*a^m */
        if(rlnode != rkn)
            {
            addr[1] = rlnode; /*{?} e^(i*x)*e^(-i*x) => 1 */
            addr[2] = rkn->RIGHT;
            if(llnode == lkn)
                {
                conc[0] = "\1^(1+\2)"; /*{?} a*a^n => a^(1+n) */
                }/* a^(1+n) */
            else
                {
                /* a^n*a^m */
                addr[3] = lkn->RIGHT; /*{?} e^(i*x)*e^(-i*x) => 1 */
                conc[0] = "\1^(\3+\2)";
                /* a^(n+m) */
                }
            }
        else
            {
            if(llnode != lkn)
                {
                addr[1] = llnode;     /*{?} a^m*a => a^(1+m) */
                addr[2] = lkn->RIGHT;
                conc[0] = "\1^(1+\2)";
                /* a^(m+1) */
                }
            else
                {
                /*{?} a*a => a^2 */
                addr[1] = llnode; /*{?} i*i*(-1/2*e^(i*a)+1/2*e^(-i*a))*(-1/2*e^(i*b)+1/2*e^(-i*b)) => -1*(-1/2*e^(i*a)+1/2*e^(-i*a))*(-1/2*e^(i*b)+1/2*e^(-i*b)) */
                conc[0] = "\1^2";
                /* a^2 */
                }
            }
        if(rkn != (temp = Pnode->RIGHT))
            {
            addr[4] = temp->RIGHT; /*{?} i*i*(-1/2*e^(i*a)+1/2*e^(-i*a))*(-1/2*e^(i*b)+1/2*e^(-i*b)) => -1*(-1/2*e^(i*a)+1/2*e^(-i*a))*(-1/2*e^(i*b)+1/2*e^(-i*b)) */
            conc[1] = "*\4";
            }
        else
            conc[1] = NULL; /*{?} e^(i*x)*e^(-i*x) => 1 */
        return vbuildup(Pnode, (const char**)conc);
        }
    else
        {
        int degree;
        degree = number_degree(rlnode) - number_degree(llnode); /*{?} (f.e)*(y.s) => (f.e)*(y.s) */
        if(degree > 0
           || (degree == 0 && (nodedifference > 0)))
            {
            /* b^n*a^m */
            /* l^n*a^m */
            if((temp = Pnode->RIGHT) == rkn)
                {
                Pnode->RIGHT = lkn; /*{?} x*2 => 2*x */
                Pnode->LEFT = rkn;
                Pnode->v.fl &= ~READY;
                }
            else
                {
                addr[1] = lkn; /*{?} i*2*x => 2*i*x */
                addr[2] = temp->LEFT;
                addr[3] = temp->RIGHT;
                Pnode = build_up(Pnode, "\2*\1*\3", NULL);
                }
            return Pnode;
            /* a^m*b^n */
            /* a^m*l^n */
            }
        else if(PLOBJ(rlnode) == IM)
            {
            if(PLOBJ(llnode) == IM) /*{?} -1*i^1/3 => -i^5/3 */
                {
                /*{?} i^n*-i^m => i^(-1*m+n) */
                if(rlnode != rkn)
                    {
                    addr[1] = llnode;
                    addr[2] = rkn->RIGHT;
                    if(llnode == lkn)
                        /*{?} i*-i^n => i^(1+-1*n) */
                        conc[0] = "\1^(1+-1*\2)";
                    /* i^(1-n) */
                    else
                        {
                        addr[3] = lkn->RIGHT; /*{?} i^n*-i^m => i^(-1*m+n) */
                        conc[0] = "\1^(\3+-1*\2)";
                        /* i^(n-m) */
                        }
                    }
                else
                    {
                    if(llnode != lkn)
                        {
                        /*{?} i^m*-i => i^(-1+m) */
                        addr[1] = llnode;
                        addr[2] = lkn->RIGHT;
                        conc[0] = "\1^(-1+\2)";
                        /* i^(m-1) */
                        }
                    else
                        {
                        /*{?} i*-i => 1 */
                        conc[0] = "1";
                        /* 1 */
                        }
                    }
                }
            else if(RAT_NEG_COMP(llnode)
                    /* -n*i^m -> n*-i^(2+m) */
                    /*{?} -7*i^9 => 7*i */ /*{!} 7*-i^11 */
                    /* -n*-i^m -> n*i^(2+m) */
                    /*{?} -7*-i^9 => 7*-i */ /*{!}-> 7*i^11 */
                    && rlnode != rkn
                    && llnode == lkn
                    )
                { /*{?} -1*i^1/3 => -i^5/3 */
                addr[1] = llnode;
                addr[2] = rkn->LEFT;
                addr[3] = rkn->RIGHT;
                addr[4] = &twoNode;
                conc[0] = "(-1*\1)*\2^(\3+\4)";
                }
            else
                return Pnode; /*{?} 2*i*x => 2*i*x */
            if(rkn != (temp = Pnode->RIGHT))
                {
                addr[4] = temp->RIGHT; /*{?} i^n*-i^m*z => i^(-1*m+n)*z */
                conc[1] = "*\4";
                }
            else
                conc[1] = NULL; /*{?} -1*i^1/3 => -i^5/3 */ /*{?} i^n*-i^m => i^(-1*m+n) */
            return vbuildup(Pnode, (const char**)conc);
            }
        else
            return Pnode; /*{?} (f.e)*(y.s) => (f.e)*(y.s) */
        }
    }

static int bringright(psk pnode)
    {
    /* (a*b*c*d)*a*b*c*d -> a*b*c*d*a*b*c*d */
    psk lnode;
    int done;
    done = FALSE;
    for(; Op(lnode = pnode->LEFT) == Op(pnode);)
        {
        lnode = isolated(lnode);
        lnode->v.fl &= ~READY;
        pnode->LEFT = lnode->LEFT;
        lnode->LEFT = lnode->RIGHT;
        lnode->RIGHT = pnode->RIGHT;
        pnode->v.fl &= ~READY;
        pnode->RIGHT = lnode;
        pnode = lnode;
        done = TRUE;
        }
    return done;
    }
/*
       1*
       / \
      /   \
     /     \
   2*      3*
   / \     / \
  a   x    b  c

lhead = a
ltail = x
rhead = b
rtail = c

rennur = NIL
runner = (a * x) * b * c
lloper = a * x

  2*
  / \
 a
 rennur = a*NIL


             1*
             / \
            x  3*
               / \
              b   c
*/
int cmpplus(psk kn1, psk kn2)
    {
    if(RATIONAL_COMP(kn2))
        {
        if(RATIONAL_COMP(kn1))
            return 0;
        return 1; /* switch places */
        }
    else if(RATIONAL_COMP(kn1))
        return -1;
    else
        {
        psk N1;
        psk I1;
        psk NNNI1;
        psk N2;
        psk I2;
        psk NNNI2;
        splitProduct_number_im_rest(kn1, &N1, &I1, &NNNI1);
        splitProduct_number_im_rest(kn2, &N2, &I2, &NNNI2);
        if(I1 != NULL && I2 == NULL)
            return 1; /* switch places: imaginary terms after real terms */
        if(NNNI2 == NULL)
            {
            if(NNNI1 != NULL)
                return 1;
            return 0;
            }
        else if(NNNI1 == NULL)
            return -1;
        else
            {
            int diff;
            diff = equal(NNNI1, NNNI2);
            if(diff > 0)
                {
                return 1; /* switch places */
                }
            else if(diff < 0)
                {
                return -1;
                }
            else
                {
                return 0;
                }
            }
        }
    }

int cmptimes(psk kn1, psk kn2)
    {
    int diff;
    if(Op(kn1) == EXP)
        kn1 = kn1->LEFT;
    if(Op(kn2) == EXP)
        kn2 = kn2->LEFT;

    diff = number_degree(kn2);
    if(diff != 5)
        diff -= number_degree(kn1);
    else
        {
        diff = -number_degree(kn1);
        if(!diff)
            return 0; /* two numbers */
        }
    if(diff > 0)
        return 1; /* switch places */
    else if(diff == 0)
        {
        diff = equal(kn1, kn2);
        if(diff > 0)
            {
            return 1; /* switch places */
            }
        else if(diff < 0)
            {
            return -1;
            }
        else
            {
            return 0;
            }
        }
    else
        {
        return -1;
        }
    }

psk merge
(psk Pnode
 , int(*comp)(psk, psk)
 , psk(*combine)(psk)
#if EXPAND
 , psk(*expand)(psk, int*)
#endif
)
    {
    psk lhead, ltail, rhead, rtail;
    psk Rennur = &nilNode; /* Will contain all evaluated nodes in inverse order.*/
    psk tmp;
    for(;;)
        {/* traverse from left to right to evaluate left side branches */
#if EXPAND
        Boolean ok;
#endif
        Pnode = isolated(Pnode);
        assert(!shared(Pnode));
        Pnode->v.fl |= READY;
#if EXPAND
        do
            {
            Pnode->LEFT = eval(Pnode->LEFT);
            Pnode->LEFT = expand(Pnode->LEFT, &ok);
            } while(ok);
#else
        Pnode->LEFT = eval(Pnode->LEFT);
#endif
        tmp = Pnode->RIGHT;
        if(tmp->v.fl & READY)
            {
            break;
            }
        if(!is_op(tmp)
           || Op(Pnode) != Op(tmp)
           )
            {
#if EXPAND
            do
                {
                tmp = eval(tmp);
                tmp = expand(tmp, &ok);
                } while(ok);
                Pnode->RIGHT = tmp;
#else
            Pnode->RIGHT = eval(tmp);
#endif
            break;
            }
        Pnode->RIGHT = Rennur;
        Rennur = Pnode;
        Pnode = tmp;
        }
    for(;;)
        { /* From right to left, prepend sorted elements to result */
        psk rennur = &nilNode; /*Will contain branches in inverse sorted order*/
        psk L = leftoperand_and_tail(Pnode, &lhead, &ltail);
        psk R = rightoperand_and_tail(Pnode, &rhead, &rtail);
        for(;;)
            { /* From right to left, prepend smallest of lhs and rhs
                 to rennur
              */
            assert(rhead->v.fl & READY);
            assert((L == lhead && ltail == NULL) || L->RIGHT == ltail);
            assert((L == lhead && ltail == NULL) || L->LEFT == lhead);
            assert(Pnode->LEFT == L);
            assert((R == rhead && rtail == NULL) || R->RIGHT == rtail);
            assert((R == rhead && rtail == NULL) || R->LEFT == rhead);
            assert(Pnode->RIGHT == R);
            assert(L->v.fl & READY);
            assert(Pnode->RIGHT->v.fl & READY);
            if(comp(lhead, rhead) <= 0) /* a * b */
                {
                if(ltail == NULL)   /* a * (b*c) */
                    {
                    assert(Pnode->RIGHT->v.fl & READY);
                    break;
                    }
                else                /* (a*d) * (b*c) */
                    {
                    L = isolated(L);
                    assert(!shared(L));
                    if(ltail != L->RIGHT)
                        {
                        wipe(L->RIGHT); /* rare, set REFCOUNTSTRESSTEST 1 */
                        ltail = same_as_w(ltail);
                        }
                    L->RIGHT = rennur;
                    rennur = L;
                    assert(!shared(Pnode));
                    Pnode = isolated(Pnode);
                    assert(!shared(Pnode));
                    Pnode->LEFT = ltail;
                    L = leftoperand_and_tail(Pnode, &lhead, &ltail);
                    }               /* rennur := a*rennur */
                /* d * (b*c) */
                }
            else /* Wrong order */
                {
                Pnode = isolated(Pnode);
                assert(!shared(Pnode));
                assert(L->v.fl & READY);
                if(rtail == NULL) /* (b*c) * a */
                    {
                    Pnode->LEFT = R;
                    assert(L->v.fl & READY);
                    Pnode->RIGHT = L;
                    break;
                    }             /* a * (b*c) */
                else          /* (b*c) * (a*d)         c * (b*a) */
                    {
                    R = isolated(R);
                    assert(!shared(R));
                    if(R->RIGHT != rtail)
                        {
                        wipe(R->RIGHT); /* rare, set REFCOUNTSTRESSTEST 1 */
                        rtail = same_as_w(rtail);
                        }
                    R->RIGHT = rennur;
                    rennur = R;
                    assert(!shared(Pnode));
                    Pnode->RIGHT = rtail;
                    R = rightoperand_and_tail(Pnode, &rhead, &rtail);
                    }         /* rennur :=  a*rennur    rennur := b*rennur */
                /* (b*c) * d */
                }
            }
        for(;;)
            { /*Combine combinable elements and prepend to result*/
            Pnode->v.fl |= READY;

            Pnode = combine(Pnode);
            if(!(Pnode->v.fl & READY))
                { /*This may results in recursive call to merge
                    if the result of the evaluation is not in same
                    sorting position as unevaluated expression. */
                assert(!shared(Pnode));
                Pnode = eval(Pnode);
                }
#if DATAMATCHESITSELF
            if(is_op(Pnode))
                {
                if((Pnode->LEFT->v.fl & SELFMATCHING) && (Pnode->RIGHT->v.fl & SELFMATCHING))
                    Pnode->v.fl |= SELFMATCHING;
                else
                    Pnode->v.fl &= ~SELFMATCHING;
                }
#endif
            if(rennur != &nilNode)
                {
                psk n = rennur->RIGHT;
                assert(!shared(rennur));
                rennur->RIGHT = Pnode;
                Pnode = rennur;
                rennur = n;
                }
            else
                break;
            }
        if(Rennur == &nilNode)
            break;
        tmp = Rennur->RIGHT;
        assert(!shared(Rennur));
        Rennur->RIGHT = Pnode;
        Pnode = Rennur;
        assert(!shared(Pnode));
        Pnode->v.fl |= READY;
#if DATAMATCHESITSELF
        if(is_op(Pnode))
            {
            if((Pnode->LEFT->v.fl & SELFMATCHING) && (Pnode->RIGHT->v.fl & SELFMATCHING))
                Pnode->v.fl |= SELFMATCHING;
            else
                Pnode->v.fl &= ~SELFMATCHING;
            }
#endif
        Rennur = tmp;
        }
    return Pnode;
    }

psk substlog(psk Pnode)
    {
    static const char* conc[] = { NULL,NULL,NULL,NULL };
    psk lnode = Pnode->LEFT, rightnode = Pnode->RIGHT;
    if(!equal(lnode, rightnode))
        {
        wipe(Pnode);
        return copyof(&oneNode);
        }
    else if(is_op(rightnode))  /*{?} x\L(2+y) => x\L(2+y) */
        {
        int ok;
        return tryq(Pnode, f5, &ok); /*{?} x\L(a*x^n*z) => n+x\L(a*z) */
        }
    else if(IS_ONE(rightnode))  /*{?} x\L1 => 0 */ /*{!} 0 */
        {
        wipe(Pnode);
        return copyof(&zeroNode);
        }
    else if(RAT_NUL(rightnode)) /*{?} z\L0 => z\L0 */
        return Pnode;
    else if(is_op(lnode)) /*{?} (x+y)\Lz => (x+y)\Lz */
        return Pnode;
    else if(RAT_NUL(lnode))   /*{?} 0\Lx => 0\Lx */
        return Pnode;
    else if(IS_ONE(lnode))   /*{?} 1\Lx => 1\Lx */
        return Pnode;
    else if(RAT_NEG(lnode))  /*{?} -7\Lx => -7\Lx */
        return Pnode;
    else if(RATIONAL_COMP(rightnode))  /*{?} x\L7 => x\L7 */
        {
        if(qCompare(rightnode, &zeroNode) & MINUS)
            {
            /* (nL-m = i*pi/eLn+nLm)  */ /*{?} 7\L-9 => 1+7\L9/7+i*pi*e\L7^-1 */ /*{!} i*pi/e\L7+7\L9)  */
            addr[1] = lnode;
            addr[2] = rightnode;
            return build_up(Pnode, "(i*pi*e\016\1^-1+\1\016(-1*\2))", NULL);
            }
        else if(RATIONAL_COMP(lnode)) /* m\Ln */ /*{?} 7\L9 => 1+7\L9/7 */
            {
            if(qCompare(lnode, &oneNode) & MINUS)
                {
                /* (1/n)Lm = -1*nLm */ /*{?} 1/7\L9 => -1*(1+7\L9/7) */ /*{!} -1*nLm */
                addr[1] = rightnode;
                conc[0] = "(-1*";
                conc[1] = hash6;
                addr[6] = qqDivide(&oneNode, lnode);
                conc[2] = "\016\1)";
                Pnode = vbuildup(Pnode, conc);
                wipe(addr[6]);
                return Pnode;
                }
            else if(qCompare(lnode, rightnode) & MINUS)
                {
                /* nL(n+m) = 1+nL((n+m)/n) */ /*{?} 7\L(7+9) => 1+7\L16/7 */ /*{!} 1+nL((n+m)/n) */
                conc[0] = "(1+\1\016";
                assert(lnode != rightnode);
                addr[1] = lnode;
                conc[1] = hash6;
                addr[6] = qqDivide(rightnode, lnode);
                conc[2] = ")";
                Pnode = vbuildup(Pnode, conc);
                wipe(addr[6]);
                return Pnode;
                }
            else if(qCompare(rightnode, &oneNode) & MINUS)
                {
                /* nL(1/m) = -1+nL(n/m) */ /*{?} 7\L1/9 => -2+7\L49/9 */ /*{!} -1+nL(n/m) */
                conc[0] = "(-1+\1\016";
                assert(lnode != rightnode);
                addr[1] = lnode;
                conc[1] = hash6;
                addr[6] = qTimes(rightnode, lnode);
                conc[2] = ")";
                Pnode = vbuildup(Pnode, conc);
                wipe(addr[6]);
                return Pnode;
                }
            }
        }
    return Pnode;
    }

static int is_dependent_of(psk el, psk input_buffer)
    {
    int ret;
    psk pnode;
    assert(!is_op(input_buffer));
    pnode = NULL;
    addr[1] = input_buffer;
    addr[2] = el;
    pnode = build_up(pnode, "(!dep:(? (\1.? \2 ?) ?)", NULL);
    pnode = eval(pnode);
    ret = isSUCCESS(pnode);
    wipe(pnode);
    return ret;
    }

psk substdiff(psk Pnode)
    {
    psk lnode, rightnode;
    lnode = Pnode->LEFT;
    rightnode = Pnode->RIGHT;
    if(is_constant(lnode) || is_constant(rightnode))
        {
        wipe(Pnode);
        Pnode = copyof(&zeroNode);
        }
    else if(!equal(lnode, rightnode))
        {
        wipe(Pnode);
        Pnode = copyof(&oneNode);
        }
    else if(!is_op(rightnode)
            && is_dependent_of(lnode, rightnode)
            )
        {
        ;
        }
    else if(!is_op(rightnode))
        {
        wipe(Pnode);
        Pnode = copyof(&zeroNode);
        }
    else if(Op(rightnode) == PLUS)
        {
        addr[1] = lnode;
        addr[2] = rightnode->LEFT;
        addr[3] = rightnode->RIGHT;
        Pnode = build_up(Pnode, "((\1\017\2)+(\1\017\3))", NULL);
        }
    else if(is_op(rightnode))
        {
        addr[2] = rightnode->LEFT;
        addr[1] = lnode;
        addr[3] = rightnode->RIGHT;
        switch(Op(rightnode))
            {
            case TIMES:
                Pnode = build_up(Pnode, "(\001\017\2*\3+\2*\001\017\3)", NULL);
                break;
            case EXP:
                Pnode = build_up(Pnode,
                                 "(\2^(-1+\3)*\3*\001\017\2+\2^\3*e\016\2*\001\017\3)", NULL);
                break;
            case LOG:
                Pnode = build_up(Pnode,
                                 "(\2^-1*e\016\2^-2*e\016\3*\001\017\2+\3^-1*e\016\2^-1*\001\017\3)", NULL);
                break;
            }
        }
    return Pnode;
    }


#if JMP /* Often no need for polling in multithreaded apps.*/
#include <windows.h>
#include <dde.h>
static void PeekMsg(void)
    {
    static MSG msg;
    while(PeekMessage(&msg, NULL, WM_PAINT, WM_DDE_LAST, PM_REMOVE))
        {
        if(msg.message == WM_QUIT)
            {
            PostThreadMessage(GetCurrentThreadId(), WM_QUIT, 0, 0L);
            longjmp(jumper, 1);
            }
        TranslateMessage(&msg);        /* Translates virtual key codes */
        DispatchMessage(&msg);        /* Dispatches message to window*/
        }
    }
#endif

/*
Iterative handling of WHITE operator in evaluate.
Can now handle very deep structures without stack overflow
*/

psk handleWhitespace(psk Pnode)
    { /* assumption: (Op(*Pnode) == WHITE) && !((*Pnode)->v.fl & READY) */
    static psk apnode;
    psk whitespacenode;
    psk next;
    ppsk pwhitespacenode = &Pnode;
    ppsk prevpwhitespacenode = NULL;
    for(;;)
        {
        whitespacenode = *pwhitespacenode;
        whitespacenode->LEFT = eval(whitespacenode->LEFT);
        if(!is_op(apnode = whitespacenode->LEFT)
           && !(apnode->u.obj)
           && !HAS_UNOPS(apnode)
           )
            {
            *pwhitespacenode = rightbranch(whitespacenode);
            }
        else
            {
            prevpwhitespacenode = pwhitespacenode;
            pwhitespacenode = &(whitespacenode->RIGHT);
            }
        if(Op(whitespacenode = *pwhitespacenode) == WHITE && !(whitespacenode->v.fl & READY))
            {
            if(shared(*pwhitespacenode))
                *pwhitespacenode = copyop(*pwhitespacenode);
            }
        else
            {
            *pwhitespacenode = eval(*pwhitespacenode);
            if(prevpwhitespacenode
               && !is_op(whitespacenode = *pwhitespacenode)
               && !((whitespacenode)->u.obj)
               && !HAS_UNOPS(whitespacenode)
               )
                *prevpwhitespacenode = leftbranch(*prevpwhitespacenode);
            break;
            }
        }

    whitespacenode = Pnode;
    while(Op(whitespacenode) == WHITE)
        {
        next = whitespacenode->RIGHT;
        bringright(whitespacenode);
        if(next->v.fl & READY)
            break;
        whitespacenode = next;
        whitespacenode->v.fl |= READY;
        }
    return Pnode;
    }
/*
Iterative handling of COMMA operator in evaluate.
Can now handle very deep structures without stack overflow
*/
psk handleComma(psk Pnode)
    { /* assumption: (Op(*Pnode) == COMMA) && !((*Pnode)->v.fl & READY) */
    psk commanode = Pnode;
    psk next;
    ppsk pcommanode;
    while(Op(commanode->RIGHT) == COMMA && !(commanode->RIGHT->v.fl & READY))
        {
        commanode->LEFT = eval(commanode->LEFT);
        pcommanode = &(commanode->RIGHT);
        commanode = commanode->RIGHT;
        if(shared(commanode))
            {
            *pcommanode = commanode = copyop(commanode);
            }
        }
    commanode->LEFT = eval(commanode->LEFT);
    commanode->RIGHT = eval(commanode->RIGHT);
    commanode = Pnode;
    while(Op(commanode) == COMMA)
        {
        next = commanode->RIGHT;
        bringright(commanode);
        if(next->v.fl & READY)
            break;
        commanode = next;
        commanode->v.fl |= READY;
        }
    return Pnode;
    }

psk evalvar(psk Pnode)
    {
    psk loc_adr = SymbolBinding_w(Pnode, Pnode->v.fl & DOUBLY_INDIRECT);
    if(loc_adr != NULL)
        {
        wipe(Pnode);
        Pnode = loc_adr;
        }
    else
        {
        if(shared(Pnode))
            {
            /*You can get here if a !variable is unitialized*/
            dec_refcount(Pnode);
            Pnode = iCopyOf(Pnode);
            }
        assert(!shared(Pnode));
        Pnode->v.fl |= READY;
        Pnode->v.fl ^= SUCCESS;
        }
    return Pnode;
    }
