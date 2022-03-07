#include "rational.h"

#include "nonnodetypes.h"
#include "globals.h"
#include "memory.h"
#include "copy.h"
#include "wipecopy.h"
#include <assert.h>
#include <string.h>

/*
multiply number with 204586 digits with itself
New algorithm:   17,13 s
Old algorithm: 1520,32 s

{?} get$longint&!x*!x:?y&lst$(y,verylongint,NEW)&ok
*/
#ifndef NDEBUG
#include <stdio.h>
static void pbint(LONG * high, LONG * low)
    {
    for (; high <= low; ++high)
        {
        if (*high)
            {
            printf(LONGD " ", *high);
            break;
            }
        else
            printf("NUL ");
        }
    for (; ++high <= low;)
        {
        printf(LONG0nD " ", (int)TEN_LOG_RADIX, *high);
        }
    }

static void fpbint(FILE * fp, LONG * high, LONG * low)
    {
    for (; high <= low; ++high)
        {
        if (*high)
            {
            fprintf(fp, LONGD " ", *high);
            break;
            }
        else
            fprintf(fp, "NUL ");
        }
    for (; ++high <= low;)
        {
        fprintf(fp, LONG0nD " ", (int)TEN_LOG_RADIX, *high);
        }
    }

static void fpbin(FILE * fp, nnumber * res)
    {
    fpbint(fp, res->inumber, res->inumber + res->ilength - 1);
    }

static void validt(LONG * high, LONG * low)
    {
    for (; high <= low; ++high)
        {
        assert(0 <= *high);
        assert(*high < RADIX);
        }
    }

static void valid(nnumber * res)
    {
    validt(res->inumber, res->inumber + res->ilength - 1);
    }
#endif


static ptrdiff_t numlength(nnumber * n)
    {
    ptrdiff_t len;
    LONG H;
    assert(n->ilength >= 1);
    len = TEN_LOG_RADIX * n->ilength;
    H = n->inumber[0];
    if (H < 10)
        len -= TEN_LOG_RADIX - 1;
    else if (H < 100)
        len -= TEN_LOG_RADIX - 2;
    else if (H < 1000)
        len -= TEN_LOG_RADIX - 3;
#if !WORD32
    else if (H < 10000)
        len -= TEN_LOG_RADIX - 4;
    else if (H < 100000)
        len -= TEN_LOG_RADIX - 5;
    else if (H < 1000000)
        len -= TEN_LOG_RADIX - 6;
    else if (H < 10000000)
        len -= TEN_LOG_RADIX - 7;
    else if (H < 100000000)
        len -= TEN_LOG_RADIX - 8;
#endif
    return len;
    }


static LONG iAddSubtractFinal(LONG *highRemainder, LONG * lowRemainder, LONG carry)
    {
    while (highRemainder <= lowRemainder)
        {
        carry += *lowRemainder;
        if (carry >= RADIX)
            {
            /* 99999999999999999/10000000001+1 */
            /* 99999999999999999999999999999999/100000000000000000000001+1 */
            *lowRemainder = carry - RADIX;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = 1;
            }
        else if (carry < 0)
            {
            *lowRemainder = carry + RADIX;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = -1;
            }
        else
            {
            *lowRemainder = carry;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = 0;
            }
        --lowRemainder;
        }
    return carry;
    }


static LONG iAdd(LONG *highRemainder, LONG *lowRemainder, LONG *highDivisor, LONG *lowDivisor)
    {
    LONG carry = 0;
    assert(*highRemainder == 0);
    do
        {
        assert(lowRemainder >= highRemainder);
        carry += (*lowRemainder + *lowDivisor);
        if (carry >= RADIX)
            {
            *lowRemainder = carry - RADIX;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = 1;
            }
        else
            {
            *lowRemainder = carry;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = 0;
            }
        --lowRemainder;
        --lowDivisor;
        } while (highDivisor <= lowDivisor);
        assert(*highRemainder == 0);
        return iAddSubtractFinal(highRemainder, lowRemainder, carry);
    }

static LONG iSubtract(LONG *highRemainder
                      , LONG *lowRemainder
                      , LONG *highDivisor
                      , LONG *lowDivisor
                      , LONG factor
)
    {
    LONG carry = 0;
    do
        {
        assert(lowRemainder >= highRemainder);
        assert(*lowDivisor >= 0);
        carry += (*lowRemainder - factor * (*lowDivisor));
        if (carry < 0)
            {
            LONG f = 1 + ((-carry - 1) / RADIX);
            *lowRemainder = carry + f * RADIX;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = -f;
            }
        else
            {
            *lowRemainder = carry;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = 0;
            }
        --lowRemainder;
        --lowDivisor;
        } while (highDivisor <= lowDivisor);
        return iAddSubtractFinal(highRemainder, lowRemainder, carry);
    }

static LONG iSubtract2(LONG *highRemainder
                       , LONG *lowRemainder
                       , LONG *highDivisor
                       , LONG *lowDivisor
                       , LONG factor
)
    {
    int allzero = TRUE;
    LONG carry = 0;
    LONG *slowRemainder = lowRemainder;
    do
        {
        assert(highRemainder <= lowRemainder);
        assert(*lowDivisor >= 0);
        carry += (*lowRemainder - factor * (*lowDivisor));
        if (carry < 0)
            {
            LONG f = 1 + ((-carry - 1) / RADIX);
            *lowRemainder = carry + f * RADIX;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = -f;
            }
        else
            {
            *lowRemainder = carry;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = 0;
            }
        if (*lowRemainder)
            allzero = FALSE;
        --lowRemainder;
        --lowDivisor;
        } while (highDivisor <= lowDivisor);
        assert(highRemainder > lowRemainder);
        if (carry == 0)
            {
            assert(carry == 0);
            assert(0 <= *highRemainder);
            assert(*highRemainder < RADIX);
            return *highRemainder == 0 ? (allzero ? 0 : 1) : 1;
            }
        ++lowRemainder;
        assert(carry != 0);
        /* negative result */
        assert(carry < 0);
        {
        assert(lowRemainder == highRemainder);
        *lowRemainder += carry * RADIX;
        assert(*lowRemainder < 0);
        assert(-((HEADROOM + 1)*RADIX) < HEADROOM * *lowRemainder && HEADROOM * *lowRemainder < ((HEADROOM + 1)*RADIX));
        lowRemainder = slowRemainder;
        carry = 0;
        while (lowRemainder >= highRemainder)
            {
            *lowRemainder = carry - *lowRemainder;
            if (*lowRemainder < 0)
                {
                *lowRemainder += RADIX;
                carry = -1;
                }
            else
                carry = 0;
            assert(0 <= *lowRemainder && (*lowRemainder < RADIX || (lowRemainder == highRemainder && HEADROOM * *lowRemainder < ((HEADROOM + 1)*RADIX))));
            if (*lowRemainder)
                allzero = FALSE;
            --lowRemainder;
            }
        assert(carry == 0);

        assert(0 <= *++lowRemainder && HEADROOM * *lowRemainder < ((HEADROOM + 1)*RADIX));
        if (*lowRemainder == 0)
            return allzero ? 0 : -1;
        else
            return -1;
        }
    }

static LONG nnDivide(nnumber * dividend, nnumber * divisor, nnumber * quotient, nnumber * remainder)
    {
    LONG *low, *quot, *head, *oldhead;
    /* Remainder starts out as copy of dividend. As the division progresses,
       the leading element of the remainder moves to the right.
       The movement is interupted if the divisor is greater than the
       leading elements of the dividend.
    */
    LONG divRADIX, divRADIX2; /* leading one or two elements of divisor */
    assert(!(divisor->sign & QNUL));

    assert(remainder->length == 0);
    assert(remainder->ilength == 0);
    assert(remainder->alloc == 0);
    assert(remainder->ialloc == 0);
    assert(quotient->length == 0);
    assert(quotient->ilength == 0);
    assert(quotient->alloc == 0);
    assert(quotient->ialloc == 0);

    remainder->ilength = remainder->iallocated = dividend->sign & QNUL ? 1 : dividend->ilength;
    assert(remainder->iallocated > 0);
    assert((LONG)TEN_LOG_RADIX * remainder->ilength >= dividend->length);
    remainder->inumber = remainder->ialloc = (LONG *)bmalloc(__LINE__, sizeof(LONG) * remainder->iallocated);
    *(remainder->inumber) = 0;
    assert(dividend->ilength != 0);/*if(dividend->ilength == 0)
        remainder->inumber[0] = 0;
    else*/
    memcpy(remainder->inumber, dividend->inumber, dividend->ilength * sizeof(LONG));

    if (dividend->ilength >= divisor->ilength)
        quotient->ilength = quotient->iallocated = 1 + dividend->ilength - divisor->ilength;
    else
        quotient->ilength = quotient->iallocated = 1;

    quotient->inumber = quotient->ialloc = (LONG *)bmalloc(__LINE__, (size_t)(quotient->iallocated) * sizeof(LONG));
    memset(quotient->inumber, 0, (size_t)(quotient->iallocated) * sizeof(LONG));
    quot = quotient->inumber;

    divRADIX = divisor->inumber[0];
    if (divisor->ilength > 1)
        {
        divRADIX2 = RADIX * divisor->inumber[0] + divisor->inumber[1];
        }
    else
        {
        divRADIX2 = 0;
        }
    assert(divRADIX > 0);
    for (low = remainder->inumber + (size_t)divisor->ilength - 1
         , head = oldhead = remainder->inumber
         ; low < remainder->inumber + dividend->ilength
         ; ++low, ++quot, ++head
         )
        {
        LONG sign = 1;
        LONG factor;
        *quot = 0;
        if (head > oldhead)
            {
            *head += RADIX * *oldhead;
            *oldhead = 0;
            oldhead = head;
            }
        do
            {
            assert(low < remainder->inumber + remainder->ilength);
            assert(quot < quotient->inumber + quotient->ilength);
            assert(sign != 0);

            assert(*head >= 0);
            factor = *head / divRADIX;
            if (sign == -1 && factor == 0)
                ++factor;

            if (factor == 0)
                {
                break;
                }
            else
                {
                LONG nsign;
                assert(factor > 0);
                if (divRADIX2)
                    {
                    assert(head + 1 <= low);
                    if (head[0] < HEADROOM * RADIX)
                        {
                        factor = (RADIX * head[0] + head[1]) / divRADIX2;
                        if (sign == -1 && factor == 0)
                            ++factor;
                        }
                    }
                /*
                div$(20999900000000,2099990001) -> factor 10499
                    (RADIX 4 HEADROOM 20)
                div$(2999000000,2999001)        -> factor 1499
                    (RADIX 3 HEADROOM 2)
                */
                assert(factor * HEADROOM < RADIX * (HEADROOM + 1));
                *quot += sign * factor;
                assert(0 <= *quot);
                /*assert(*quot < RADIX);*/
                nsign = iSubtract2(head
                                   , low
                                   , divisor->inumber
                                   , divisor->inumber + divisor->ilength - 1
                                   , factor
                );
                assert(*head >= 0);
                assert(sign != 0);
                if (nsign < 0)
                    sign = -sign;
                else if (nsign == 0)
                    sign = 0;
                }
            } while (sign < 0);
            /*
            checkBounds(remainder->ialloc);
            checkBounds(quotient->ialloc);
            checkBounds(remainder->ialloc);
            checkBounds(quotient->ialloc);
            */
            assert(*quot < RADIX);
        }
    /*
    checkBounds(remainder->ialloc);
    checkBounds(quotient->ialloc);
    */
    for (low = remainder->inumber; remainder->ilength > 1 && *low == 0; ++low)
        {
        --(remainder->ilength);
        ++(remainder->inumber);
        }

    assert(remainder->ilength >= 1);
    assert(remainder->inumber >= (LONG*)remainder->ialloc);
    assert(remainder->inumber < (LONG*)remainder->ialloc + remainder->iallocated);
    assert(remainder->inumber + remainder->ilength <= (LONG*)remainder->ialloc + remainder->iallocated);

    for (low = quotient->inumber; quotient->ilength > 1 && *low == 0; ++low)
        {
        --(quotient->ilength);
        ++(quotient->inumber);
        }

    assert(quotient->ilength >= 1);
    assert(quotient->inumber >= (LONG*)quotient->ialloc);
    assert(quotient->inumber < (LONG*)quotient->ialloc + quotient->iallocated);
    assert(quotient->inumber + quotient->ilength <= (LONG*)quotient->ialloc + quotient->iallocated);
    remainder->sign = remainder->inumber[0] ? ((dividend->sign ^ divisor->sign) & MINUS) : QNUL;
    quotient->sign = quotient->inumber[0] ? ((dividend->sign ^ divisor->sign) & MINUS) : QNUL;
    /*
    checkBounds(remainder->ialloc);
    checkBounds(quotient->ialloc);
    */
    return TRUE;
    }

/* Create a node from a number, allocating memory for the node.
The numbers' memory isn't deallocated. */

static char * iconvert2decimal(nnumber * res, char * g)
    {
    LONG * ipointer;
    g[0] = '0';
    g[1] = 0;
    for (ipointer = res->inumber; ipointer < res->inumber + res->ilength; ++ipointer)
        {
        assert(*ipointer >= 0);
        assert(*ipointer < RADIX);
        if (*ipointer)
            {
            g += sprintf(g, LONGD, *ipointer);
            for (; ++ipointer < res->inumber + res->ilength;)
                {
                assert(*ipointer >= 0);
                assert(*ipointer < RADIX);
                g += sprintf(g,/*"%0*ld"*/LONG0nD, (int)TEN_LOG_RADIX, *ipointer);
                }
            break;
            }
        }
    return g;
    }

static psk inumberNode(nnumber * g)
    {
    psk res;
    size_t len;
    len = offsetof(sk, u.obj) + numlength(g);
    res = (psk)bmalloc(__LINE__, len + 1);
    if (g->sign & QNUL)
        res->u.obj = '0';
    else
        {
        iconvert2decimal(g, (char *)POBJ(res));
        }

    res->v.fl = READY | SUCCESS | QNUMBER BITWISE_OR_SELFMATCHING;
    res->v.fl |= g->sign;
    return res;
    }


static Qnumber nn2q(nnumber * num, nnumber * den)
    {
    Qnumber res;
    assert(!(num->sign & QNUL));
    assert(!(den->sign & QNUL));
    /*    if(num->sign & QNUL)
            return copyof(&zeroNode);
        else if(den->sign & QNUL)
            return not_a_number();*/

    num->sign ^= (den->sign & MINUS);

    if (den->ilength == 1 && den->inumber[0] == 1)
        {
        res = inumberNode(num);
        }
    else
        {
        char * endp;
        size_t len = offsetof(sk, u.obj) + 2 + numlength(num) + numlength(den);
        res = (psk)bmalloc(__LINE__, len);
        endp = iconvert2decimal(num, (char *)POBJ(res));
        *endp++ = '/';
        endp = iconvert2decimal(den, endp);
        assert((size_t)(endp - (char *)res) <= len);
        res->v.fl = READY | SUCCESS | QNUMBER | QFRACTION BITWISE_OR_SELFMATCHING;
        res->v.fl |= num->sign;
        }
    return res;
    }

static Qnumber not_a_number(void)
    {
    Qnumber res;
    res = copyof(&zeroNode);
    res->v.fl ^= SUCCESS;
    return res;
    }



Qnumber qnDivide(nnumber * x, nnumber * y)
    {
    Qnumber res;
    nnumber gcd = { 0 }, hrem = { 0 };
    nnumber quotientx = { 0 }, remainderx = { 0 };
    nnumber quotienty = { 0 }, remaindery = { 0 };

#ifndef NDEBUG
    valid(x);
    valid(y);
#endif
    if (x->sign & QNUL)
        return copyof(&zeroNode);
    else if (y->sign & QNUL)
        return not_a_number();

    gcd = *x;
    gcd.alloc = NULL;
    gcd.ialloc = NULL;
    hrem = *y;
    hrem.alloc = NULL;
    hrem.ialloc = NULL;
    do
        {
        nnDivide(&gcd, &hrem, &quotientx, &remainderx);
#ifndef NDEBUG
        valid(&gcd);
        valid(&hrem);
        valid(&quotientx);
        valid(&remainderx);
#endif
        if (gcd.ialloc)
            {
            bfree(gcd.ialloc);
            }
        gcd = hrem;
        hrem = remainderx;
        remainderx.alloc = 0;
        remainderx.length = 0;
        bfree(quotientx.ialloc);
        remainderx.ialloc = 0;
        remainderx.ilength = 0;
        quotientx.ialloc = 0;
        quotientx.alloc = 0;
        quotientx.ilength = 0;
        quotientx.length = 0;
        } while (!(hrem.sign & QNUL));

        if (hrem.ialloc)
            bfree(hrem.ialloc);

        nnDivide(x, &gcd, &quotientx, &remainderx);
#ifndef NDEBUG
        valid(&gcd);
        valid(x);
        valid(&quotientx);
        valid(&remainderx);
#endif
        bfree(remainderx.ialloc);
        nnDivide(y, &gcd, &quotienty, &remaindery);
#ifndef NDEBUG
        valid(&gcd);
        valid(y);
        valid(&quotienty);
        valid(&remaindery);
#endif
        bfree(remaindery.ialloc);

        if (gcd.ialloc)
            bfree(gcd.ialloc);
        res = nn2q(&quotientx, &quotienty);
        bfree(quotientx.ialloc);
        bfree(quotienty.ialloc);
        return res;
    }


static void nTimes(nnumber * x, nnumber * y, nnumber * product)
    {
    LONG *I1, *I2;
    LONG *ipointer, *itussen;

    assert(product->length == 0);
    assert(product->ilength == 0);
    assert(product->alloc == 0);
    assert(product->ialloc == 0);
    product->ilength = product->iallocated = x->ilength + y->ilength;
    assert(product->iallocated > 0);
    product->inumber = product->ialloc = (LONG *)bmalloc(__LINE__, sizeof(LONG) * product->iallocated);

    for (ipointer = product->inumber; ipointer < product->inumber + product->ilength; *ipointer++ = 0)
        ;

    for (I1 = x->inumber + x->ilength - 1; I1 >= x->inumber; I1--)
        {
        itussen = --ipointer; /* pointer to result, starting from LSW. */
        assert(itussen >= product->inumber);
        for (I2 = y->inumber + y->ilength - 1; I2 >= y->inumber; I2--)
            {
            LONG prod;
            LONG *itussen2;
            prod = (*I1)*(*I2);
            *itussen += prod;
            itussen2 = itussen--;
            while (*itussen2 >= HEADROOM * RADIX2)
                {
                LONG karry;
                karry = *itussen2 / RADIX;
                *itussen2 %= RADIX;
                --itussen2;
                assert(itussen2 >= product->inumber);
                *itussen2 += karry;
                }
            assert(itussen2 >= product->inumber);
            }
        if (*ipointer >= RADIX)
            {
            LONG karry = *ipointer / RADIX;
            *ipointer %= RADIX;
            itussen = ipointer - 1;
            assert(itussen >= product->inumber);
            *itussen += karry;
            while (*itussen >= HEADROOM * RADIX2/* 2000000000 */)
                {
                karry = *itussen / RADIX;
                *itussen %= RADIX;
                --itussen;
                assert(itussen >= product->inumber);
                *itussen += karry;
                }
            assert(itussen >= product->inumber);
            }
        }
    while (ipointer >= product->inumber)
        {
        if (*ipointer >= RADIX)
            {
            LONG karry = *ipointer / RADIX;
            *ipointer %= RADIX;
            --ipointer;
            assert(ipointer >= product->inumber);
            *ipointer += karry;
            }
        else
            --ipointer;
        }

    for (ipointer = product->inumber; product->ilength > 1 && *ipointer == 0; ++ipointer)
        {
        --(product->ilength);
        ++(product->inumber);
        }

    assert(product->ilength >= 1);
    assert(product->inumber >= (LONG*)product->ialloc);
    assert(product->inumber < (LONG*)product->ialloc + product->iallocated);
    assert(product->inumber + product->ilength <= (LONG*)product->ialloc + product->iallocated);

    product->sign = product->inumber[0] ? ((x->sign ^ y->sign) & MINUS) : QNUL;
    }

char * split(Qnumber _qget, nnumber * ptel, nnumber * pnoem)
    {
    ptel->sign = _qget->v.fl & (MINUS | QNUL);
    pnoem->sign = 0;
    pnoem->alloc = ptel->alloc = NULL;
    ptel->number = (char *)POBJ(_qget);

    if (_qget->v.fl & QFRACTION)
        {
        char * on = strchr(ptel->number, '/');
        assert(on);
        ptel->length = on - ptel->number;
        pnoem->number = on + 1;
        pnoem->length = strlen(on + 1);
        return on;
        }
    else
        {
        assert(!(_qget->v.fl & QFRACTION));
        ptel->length = strlen(ptel->number);
        pnoem->number = "1";
        pnoem->length = 1;
        return NULL;
        }
    }


/* Create a node from a number, only allocating memory for the node if the
number hasn't the right size (== too large). If new memory is allocated,
the number's memory is deallocated. */

static psk numberNode2(nnumber * g)
    {
    psk res;
    size_t neededlen;
    size_t availablelen = g->allocated;
    neededlen = offsetof(sk, u.obj) + 1 + g->length;
    if ((neededlen - 1) / sizeof(LONG) == (availablelen - 1) / sizeof(LONG))
        {
        res = (psk)g->alloc;
        if (g->sign & QNUL)
            {
            res->u.lobj = 0;
            res->u.obj = '0';
            }
        else
            {
            char * end = (char *)POBJ(res) + g->length;
            char * limit = (char *)g->alloc + (1 + (availablelen - 1) / sizeof(LONG)) * sizeof(LONG);
            memmove((void*)POBJ(res), g->number, g->length);
            while (end < limit)
                *end++ = '\0';
            }
        }
    else
        {
        res = (psk)bmalloc(__LINE__, neededlen);
        if (g->sign & QNUL)
            res->u.obj = '0';
        else
            {
            memcpy((void*)POBJ(res), g->number, g->length);
            /*(char *)POBJ(res) + g.length = '\0'; not necessary, is done in bmalloc */
            }
        bfree(g->alloc);
        }
    res->v.fl = READY | SUCCESS | QNUMBER BITWISE_OR_SELFMATCHING;
    res->v.fl |= g->sign;
    return res;
    }


#if 1
/*

{?} 0:?n&whl'(1+!n:<1000:?n&5726597846592437657823456/67678346259784659237457297*5364523564325982435082458045728395/543895704328725085743289570+1:?T)&!T
{!} 62694910002908910202610946081623305253537463604246972909/75122371034222274084834066940872899586533484041821
    S   0,98 sec  (16500.31292.1022)


70
1,71*10E0 s Loop implemented as circular datastructure.
1,79*10E0 s Loop implemented with variable name evaluation.
1,85*10E0 s Loop implemented with member name evaluation
1,49*10E0 s Loop implemented with whl built-in function
80
210
Done. No errors. See valid.txt
ok
{!} ok
    S   7,29 sec  (16497.31239.1022)
*/

static Qnumber qDivideMultiply(nnumber * x1, nnumber * x2, nnumber * y1, nnumber * y2)
    {
    nnumber pa = { 0 }, pb = { 0 };
    Qnumber res;

    nTimes(x1, y1, &pa);
    nTimes(x2, y2, &pb);
    res = qnDivide(&pa, &pb);
    bfree(pa.ialloc);
    bfree(pb.ialloc);
    return(res);
    }
#else
/*
{?} 0:?n&whl'(1+!n:<1000:?n&707957265978333334659243765782345642321377768009873333/676799876699834625978465923745729787673565217476876234*536452356432598243508245804572839536543879878347777777/113555480476987659789060958543895703333333333333333333+1:?T)&!T
{!} 62694910002908910202610946081623305253537463604246972909/75122371034222274084834066940872899586533484041821
    S   1,03 sec  (1437.1463.2)


70
1,69*10E0 s Loop implemented as circular datastructure.
1,76*10E0 s Loop implemented with variable name evaluation.
1,87*10E0 s Loop implemented with member name evaluation
1,45*10E0 s Loop implemented with whl built-in function
80
210
Done. No errors. See valid.txt
ok
{!} ok
    S   7,32 sec  (16497.31239.1022)

70
1,82*10E0 s Loop implemented as circular datastructure.
2,06*10E0 s Loop implemented with variable name evaluation.
1,89*10E0 s Loop implemented with member name evaluation
1,45*10E0 s Loop implemented with whl built-in function
80
210
Done. No errors. See valid.txt
ok
{!} ok
    S   7,89 sec  (16497.31292.1022)

70
1,68*10E0 s Loop implemented as circular datastructure.
1,79*10E0 s Loop implemented with variable name evaluation.
1,90*10E0 s Loop implemented with member name evaluation
1,45*10E0 s Loop implemented with whl built-in function
80
210
Done. No errors. See valid.txt
ok
{!} ok
    S   7,26 sec  (16497.31292.1022)
*/

static Qnumber qDivideMultiply2(nnumber * x1, nnumber * x2, nnumber * y1, nnumber * y2)
    {
    nnumber pa = { 0 }, pb = { 0 };
    Qnumber res;
    nTimes(x1, y1, &pa);
    nTimes(x2, y2, &pb);
    res = nn2q(&pa, &pb);
    bfree(pa.alloc);
    bfree(pb.alloc);
    return(res);
    }

static Qnumber qTimes2(Qnumber _qx, Qnumber _qy)
    {
    nnumber xt = { 0 }, xn = { 0 }, yt = { 0 }, yn = { 0 };
    char *xb, *yb;
    xb = split(_qx, &xt, &xn);
    yb = split(_qy, &yt, &yn);
    if (!xb && !yb)
        {
        nnumber g = { 0 };
        nTimes(&xt, &yt, &g);
        psk res = numberNode2(&g);
        return res;
        }
    else
        {
        Qnumber res;
        res = qDivideMultiply2(&xt, &xn, &yt, &yn);
        return res;
        }
    }

static int nnDivide(nnumber * x, nnumber * y)
    {
    division resx, resy;
    nnumber gcd = { 0 }, hrem = { 0 };

    if (x->sign & QNUL)
        return 0; /* zero */
    else if (y->sign & QNUL)
        return -1; /* not a number */

    gcd = *x;
    gcd.alloc = NULL;
    hrem = *y;
    hrem.alloc = NULL;
    do
        {
        nnDivide(&gcd, &hrem, &resx);
        if (gcd.alloc)
            bfree(gcd.alloc);
        gcd = hrem;
        hrem = resx.remainder;
        bfree(resx.quotient.alloc);
        } while (!(hrem.sign & QNUL));

        if (hrem.alloc)
            bfree(hrem.alloc);

        nnDivide(x, &gcd, &resx);
        bfree(resx.remainder.alloc);

        nnDivide(y, &gcd, &resy);
        bfree(resy.remainder.alloc);
        if (gcd.alloc)
            bfree(gcd.alloc);
        if (x->alloc)
            bfree(x->alloc);
        if (y->alloc)
            bfree(y->alloc);
        *x = resx.quotient;
        *y = resy.quotient;
        return 2;
    }

/*Meant to be a faster multiplication of rational numbers
Assuming that the two numbers are reduced, we only need to
reduce the numerators with the denominators of the other
number. Numerators and denominators may get smaller, speeding
up the following multiplication.
*/
static Qnumber qDivideMultiply(nnumber * x1, nnumber * x2, nnumber * y1, nnumber * y2)
    {
    Qnumber res;
    nnumber z1 = { 0 }, z2 = { 0 };
    nnDivide(x1, y2);
    nnDivide(y1, x2);
    nnDivide(x1, x2);
    nnDivide(y1, y2);
    nTimes(x1, y1, &z1);
    nTimes(x2, y2, &z2);
    if (x1->alloc) bfree(x1->alloc);
    if (x2->alloc) bfree(x2->alloc);
    if (y1->alloc) bfree(y1->alloc);
    if (y2->alloc) bfree(y2->alloc);
    res = nn2q(&z1, &z2);
    if (z1.alloc) bfree(z1.alloc);
    if (z2.alloc) bfree(z2.alloc);
    return res;
    }
#endif

static void convert2binary(nnumber * x)
    {
    LONG * ipointer;
    char * charpointer;
    ptrdiff_t n;

    x->ilength = x->iallocated = ((x->sign & QNUL ? 1 : x->length) + TEN_LOG_RADIX - 1) / TEN_LOG_RADIX;
    x->inumber = x->ialloc = (LONG *)bmalloc(__LINE__, sizeof(LONG) * x->iallocated);

    for (ipointer = x->inumber
         , charpointer = x->number
         , n = x->length
         ; ipointer < x->inumber + x->ilength
         ; ++ipointer
         )
        {
        *ipointer = 0;
        do
            {
            *ipointer = 10 * (*ipointer) + *charpointer++ - '0';
            } while (--n % TEN_LOG_RADIX != 0);
        }
    assert((LONG)TEN_LOG_RADIX * x->ilength >= charpointer - x->number);
    }



static char * isplit(Qnumber _qget, nnumber * ptel, nnumber * pnoem)
    {
    ptel->sign = _qget->v.fl & (MINUS | QNUL);
    pnoem->sign = 0;
    pnoem->alloc = ptel->alloc = NULL;
    ptel->number = (char *)POBJ(_qget);
    if (_qget->v.fl & QFRACTION)
        {
        char * on = strchr(ptel->number, '/');
        assert(on);
        ptel->length = on - ptel->number;
        pnoem->number = on + 1;
        pnoem->length = strlen(on + 1);
        convert2binary(ptel);
        convert2binary(pnoem);
        return on;
        }
    else
        {
        assert(!(_qget->v.fl & QFRACTION));
        ptel->length = strlen(ptel->number);
        pnoem->number = "1";
        pnoem->length = 1;
        convert2binary(ptel);
        convert2binary(pnoem);
        return NULL;
        }
    }


Qnumber qTimes(Qnumber _qx, Qnumber _qy)
    {
    Qnumber res;
    nnumber xt = { 0 }, xn = { 0 }, yt = { 0 }, yn = { 0 };
    char *xb, *yb;

    xb = isplit(_qx, &xt, &xn);
    yb = isplit(_qy, &yt, &yn);
    if (!xb && !yb)
        {
        nnumber g = { 0 };
        nTimes(&xt, &yt, &g);
        bfree(xt.ialloc);
        bfree(xn.ialloc);
        bfree(yt.ialloc);
        bfree(yn.ialloc);
        res = inumberNode(&g);
        bfree(g.ialloc);
        return res;
        }
    else
        {
        res = qDivideMultiply(&xt, &xn, &yt, &yn);
        bfree(xt.ialloc);
        bfree(xn.ialloc);
        bfree(yt.ialloc);
        bfree(yn.ialloc);
        return res;
        }
    }

Qnumber qqDivide(Qnumber _qx, Qnumber _qy)
    {
    nnumber xt = { 0 }, xn = { 0 }, yt = { 0 }, yn = { 0 };
    char *xb, *yb;

    xb = isplit(_qx, &xt, &xn);
    yb = isplit(_qy, &yt, &yn);
    if (!xb && !yb)
        {
        Qnumber res = qnDivide(&xt, &yt);
        bfree(xt.ialloc);
        bfree(xn.ialloc);
        bfree(yt.ialloc);
        bfree(yn.ialloc);
        return res;
        }
    else
        {
        Qnumber res;
        res = qDivideMultiply(&xt, &xn, &yn, &yt);
        bfree(xt.ialloc);
        bfree(xn.ialloc);
        bfree(yt.ialloc);
        bfree(yn.ialloc);
        return res;
        }
    }


static void skipnullen(nnumber * nget, int Sign)
    {
    for (
        ; nget->length > 0 && *(nget->number) == '0'
        ; nget->number++, nget->length--
        )
        ;
    nget->sign = nget->length ? (Sign & MINUS) : QNUL;
    }



static int addSubtractFinal(char * i1, char * i2, char tmp, char ** pres, char *bx)
    {
    for (; i2 >= bx;)
        {
        tmp += (*i2);
        if (tmp > '9')
            {
            *i1-- = tmp - 10;
            tmp = 1;
            }
        else if (tmp < '0')
            {
            *i1-- = tmp + 10;
            tmp = -1;
            }
        else
            {
            *i1-- = tmp;
            tmp = 0;
            }
        i2--;
        }
    *pres = i1 + 1;
    return tmp;
    }

static int increase(char **pres, char *bx, char *ex, char *by, char *ey)
    {
    char *i1 = *pres, *i2 = ex, *ypointer = ey;
    char tmp = 0;
    do
        {
        tmp += (*i2 + *ypointer - '0');
        if (tmp > '9')
            {
            *i1-- = tmp - 10;
            tmp = 1;
            }
        else
            {
            *i1-- = tmp;
            tmp = 0;
            }
        --i2;
        --ypointer;
        } while (ypointer >= by);
        return addSubtractFinal(i1, i2, tmp, pres, bx);
    }

static int decrease(char **pres, char *bx, char *ex, char *by, char *ey)
    {
    char *i1 = *pres, *i2 = ex, *ypointer = ey;
    char tmp = 0;
    do
        {
        tmp += (*i2 - *ypointer + '0');
        if (tmp < '0')
            {
            *i1-- = tmp + 10;
            tmp = -1;
            }
        else
            {
            *i1-- = tmp;
            tmp = 0;
            }
        --i2;
        --ypointer;
        } while (ypointer >= by);
        return addSubtractFinal(i1, i2, tmp, pres, bx);
    }


nnumber nPlus(nnumber * x, nnumber * y)
    {
    nnumber res = { 0 };
    char *hres;
    ptrdiff_t xGreaterThany;
    res.length = 1 + (x->length > y->length ? x->length : y->length);
    res.allocated = (size_t)res.length + offsetof(sk, u.obj);
    res.alloc = res.number = (char *)bmalloc(__LINE__, res.allocated);
    *res.number = '0';
    hres = res.number + (size_t)res.length - 1;
    if (x->length == y->length)
        xGreaterThany = strncmp(x->number, y->number, (size_t)res.length);
    else
        xGreaterThany = x->length - y->length;
    if (xGreaterThany < 0)
        {
        nnumber * hget = x;
        x = y;
        y = hget;
        }
    if (x->sign == y->sign)
        {
        if (increase(&hres, x->number, x->number + x->length - 1, y->number, y->number + y->length - 1))
            *--hres = '1';
        }
    else
        decrease(&hres, x->number, x->number + x->length - 1, y->number, y->number + y->length - 1);
    skipnullen(&res, x->sign);
    return res;
    }


void nnSPlus(nnumber * x, nnumber * y, nnumber * som)
    {
    LONG * hres, *px, *py, *ex;
    ptrdiff_t xGreaterThany;

    som->ilength = 1 + (x->ilength > y->ilength ? x->ilength : y->ilength);
    som->iallocated = som->ilength;
    som->ialloc = som->inumber = (LONG *)bmalloc(__LINE__, sizeof(LONG)*som->iallocated);
    *som->inumber = 0;
    hres = som->inumber + som->ilength;

    if (x->ilength == y->ilength)
        {
        px = x->inumber;
        ex = px + x->ilength;
        py = y->inumber;
        do
            {
            xGreaterThany = *px++ - *py++;
            } while (!xGreaterThany && px < ex);
        }
    else
        xGreaterThany = x->ilength - y->ilength;
    if (xGreaterThany < 0)
        {
        nnumber * hget = x;
        x = y;
        y = hget;
        }

    for (px = x->inumber + x->ilength; px > x->inumber;)
        *--hres = *--px;
    if (x->sign == y->sign)
        {
#ifndef NDEBUG
        LONG carry =
#endif
            iAdd(som->inumber, som->inumber + som->ilength - 1, y->inumber, y->inumber + y->ilength - 1);
        assert(carry == 0);
        }
    else
        {
        iSubtract(som->inumber, som->inumber + som->ilength - 1, y->inumber, y->inumber + y->ilength - 1, 1);
        }
    for (hres = som->inumber; som->ilength > 1 && *hres == 0; ++hres)
        {
        --(som->ilength);
        ++(som->inumber);
        }

    som->sign = som->inumber[0] ? (x->sign & MINUS) : QNUL;
    }


Qnumber qPlus(Qnumber _qx, Qnumber _qy, int minus)
    {
    nnumber xt = { 0 }, xn = { 0 }, yt = { 0 }, yn = { 0 };

    char *xb, *yb;
    xb = split(_qx, &xt, &xn);
    yb = split(_qy, &yt, &yn);
    yt.sign ^= minus;

    if (!xb && !yb)
        {
        nnumber g = nPlus(&xt, &yt);
        psk res = numberNode2(&g);
        return res;
        }
    else
        {
        nnumber pa = { 0 }, pb = { 0 }, som = { 0 };
        Qnumber res;
        convert2binary(&xt);
        convert2binary(&xn);
        convert2binary(&yt);
        convert2binary(&yn);
        nTimes(&xt, &yn, &pa);
        nTimes(&yt, &xn, &pb);
        nnSPlus(&pa, &pb, &som);
        bfree(pa.ialloc);
        bfree(pb.ialloc);
        pa.ilength = 0;
        pa.ialloc = 0;
        pa.length = 0;
        pa.alloc = 0;
        nTimes(&xn, &yn, &pa);
        res = qnDivide(&som, &pa);
        if (som.ialloc)
            bfree(som.ialloc);
        bfree(pa.ialloc);
        bfree(xt.ialloc);
        bfree(xn.ialloc);
        bfree(yt.ialloc);
        bfree(yn.ialloc);
        return res;
        }
    }

Qnumber qIntegerDivision(Qnumber _qx, Qnumber _qy)
    {
    Qnumber res, aqnumber;
    nnumber xt = { 0 }, xn = { 0 }, yt = { 0 }, yn = { 0 }, p1 = { 0 }, p2 = { 0 }, quotient = { 0 }, remainder = { 0 };

    isplit(_qx, &xt, &xn);
    isplit(_qy, &yt, &yn);

    nTimes(&xt, &yn, &p1);
    nTimes(&xn, &yt, &p2);
    nnDivide(&p1, &p2, &quotient, &remainder);
    bfree(p1.ialloc);
    bfree(p2.ialloc);
    res = qPlus
    ((remainder.sign & QNUL)
     || !(_qx->v.fl & MINUS)
     ? &zeroNode
     : (_qy->v.fl & MINUS)
     ? &oneNode
     : &minusOneNode
     , (aqnumber = inumberNode(&quotient))
     , 0
    );
    pskfree(aqnumber);
    bfree(quotient.ialloc);
    bfree(remainder.ialloc);
    bfree(xt.ialloc);
    bfree(xn.ialloc);
    bfree(yt.ialloc);
    bfree(yn.ialloc);
    return res;
    }

Qnumber qModulo(Qnumber _qx, Qnumber _qy)
    {
    Qnumber res, _q2, _q3;

    _q2 = qIntegerDivision(_qx, _qy);
    _q3 = qTimes(_qy, _q2);
    pskfree(_q2);
    res = qPlus(_qx, _q3, MINUS);
    pskfree(_q3);
    return res;
    }

psk qDenominator(psk Pnode)
    {
    Qnumber _qx = Pnode->RIGHT;
    psk res;
    size_t len;
    nnumber xt = { 0 }, xn = { 0 };
    split(_qx, &xt, &xn);
    len = offsetof(sk, u.obj) + 1 + xn.length;
    res = (psk)bmalloc(__LINE__, len);
    assert(!(xn.sign & QNUL)); /*Because RATIONAL_COMP(_qx)*/
    memcpy((void*)POBJ(res), xn.number, xn.length);
    res->v.fl = READY | SUCCESS | QNUMBER BITWISE_OR_SELFMATCHING;
    res->v.fl |= xn.sign;
    wipe(Pnode);
    return res;
    }

int qCompare(Qnumber _qx, Qnumber _qy)
    {
    Qnumber som;
    int res;
    som = qPlus(_qx, _qy, MINUS);
    res = som->v.fl & (MINUS | QNUL);
    pskfree(som);
    return res;
    }


Qnumber qTimesMinusOne(Qnumber _qx)
    {
    Qnumber res;
    size_t len;
    len = offsetof(sk, u.obj) + 1 + strlen((char*)POBJ(_qx));
    res = (Qnumber)bmalloc(__LINE__, len);
    memcpy(res, _qx, len);
    res->v.fl ^= MINUS;
    res->v.fl &= ~ALL_REFCOUNT_BITS_SET;
    return res;
    }

int subroot(nnumber* ag, char* conc[], int* pind)
    {
    int macht, i;
    ULONG g, smalldivisor;
    ULONG ores;
    static int bijt[12] =
        { 1,  2,  2,  4,    2,    4,    2,    4,    6,    2,  6 };
    /* 2-3,3-5,5-7,7-11,11-13,13-17,17-19,19-23,23-29,29-1,1-7*/
    ULONG bigdivisor;

#ifdef ERANGE   /* ANSI C : strtoul() out of range */
    errno = 0;
    g = STRTOUL(ag->number, NULL, 10);
    if (errno == ERANGE)
        return FALSE; /*{?} 45237183544316235476^1/2 => 45237183544316235476^1/2 */
#else  /* TURBOC, vcc */
    if (ag->length > 10 || (ag->length == 10 && (strcmp(ag->number, "4294967295") > 0)))
        return FALSE;
    g = STRTOUL(ag->number, NULL, 10);
#endif
    ores = 1;
    macht = 1;
    smalldivisor = 2;
    i = 0;
    while ((bigdivisor = g / smalldivisor) >= smalldivisor)
        {
        if (bigdivisor * smalldivisor == g)
            {
            g = bigdivisor;
            if (smalldivisor != ores)
                {
                if (ores != 1)
                    {
                    if (ores < 1000)
                        {
                        conc[(*pind)] = (char*)bmalloc(__LINE__, 12);/*{?} 327365274^1/2 => 2^1/2*3^1/2*2477^1/2*22027^1/2 */
                        }
                    else
                        {
                        conc[*pind] = (char*)bmalloc(__LINE__, 20);
                        }
                    sprintf(conc[(*pind)++], LONGU "^(%d*\1)*", ores, macht);
                    }
                macht = 1;
                ores = smalldivisor;
                }
            else
                {
                macht++; /*{?} 80956863^1/2 => 3*13^1/2*541^1/2*1279^1/2 */
                }
            }
        else
            {
            smalldivisor += bijt[i];
            if (++i > 10)
                i = 3;
            }
        }
    if (ores == 1 && macht == 1)
        return FALSE;
    conc[*pind] = (char*)bmalloc(__LINE__, 32);
    if ((ores == g && ++macht) || ores == 1)
        sprintf(conc[(*pind)++], LONGU "^(%d*\1)", g, macht); /*{?} 32^1/2 => 2^5/2 */
    else
        sprintf(conc[(*pind)++], LONGU "^(%d*\1)*" LONGU "^\1", ores, macht, g);
    return TRUE;
    }
