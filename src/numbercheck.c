#include "numbercheck.h"
#include "globals.h"
#include "nonnodetypes.h"
#include <string.h> /*strspn()*/

int numbercheck(const char* begin)
    {
    size_t op_or_0;
    int check;
    int needNonZeroDigit = FALSE;
    int signseen = FALSE;
    char* endptr;
    const char* savebegin = begin;
    if(!*begin)
        return 0;
    op_or_0 = strspn(begin, "0123456789.-+/aAbBcCdDeEfFpPxX");
    if(begin[op_or_0])
        return DEFINITELYNONUMBER;
    check = QNUMBER;
    op_or_0 = *begin;

    if(op_or_0 == '-' || op_or_0 == '+')
        {
        op_or_0 = *++begin;
        signseen = TRUE;
        }
    if(op_or_0 >= '0' && op_or_0 <= '9')
        {
        if(op_or_0 == '0')
            {
            if(*(begin + 1) == 'x' || *(begin + 1) == 'X' || *(begin + 1) == '.')
                {
                /* hexadecimal number, must be in domain of strtod */
                double testdouble = strtod(begin, &endptr);
                if(*endptr)
                    return DEFINITELYNONUMBER; /* format error */
                if(testdouble == 0.0)
                    check |= QNUL;
                /* Since doubles can be serialised to hex strings without
                   losing bits, hex numbers are treated like doubles, not like
                   integer or fractional numbers.
                   Format as the printf() "%a" format specifier. The 'p'
                   (exponent) specification is optional. */
                return check;
                }
            else
                check |= QNUL;
            }
        if(optab[op_or_0 = *++begin] != -1)
            {
            if(signseen)
                check = DEFINITELYNONUMBER; /*sign must not be inside quotes*/
            else
                {
                while((check != DEFINITELYNONUMBER) && (optab[op_or_0 = *begin++] != -1))
                    {
                    if(op_or_0 == '/')
                        {
                        /* check &= ~QNUL;*/
                        if(check & QFRACTION)
                            {
                            check = DEFINITELYNONUMBER;
                            break;
                            }
                        else
                            {
                            needNonZeroDigit = TRUE;
                            check |= QFRACTION;
                            }
                        }
                    else if(op_or_0 < '0' || op_or_0 > '9')
                        {
                        check = DEFINITELYNONUMBER;
                        break;
                        }
                    else
                        {
                        /* initial zero followed by
                                         0 <= k <= 9 makes no number */
                        if((check & (QNUL | QFRACTION)) == QNUL)
                            {
                            return DEFINITELYNONUMBER;
                            }
                        else if(op_or_0 != '0')
                            {
                            needNonZeroDigit = FALSE;
                            /*check &= ~QNUL;*/
                            }
                        else if(needNonZeroDigit) /* '/' followed by '0' */
                            {
                            return DEFINITELYNONUMBER;
                            }
                        }
                    }
                }
            }
        if(check == DEFINITELYNONUMBER)
            {
            double testdouble = strtod(savebegin, &endptr);
            if(*endptr)
                return check; /* format error */
            check = QNUMBER|QDOUBLE;
            if(testdouble == 0.0)
                check |= QNUL;
            /* Since doubles can be serialised to hex strings without
               losing bits, hex numbers are treated like doubles, not like
               integer or fractional numbers.
               Format as the printf() "%a" format specifier. The 'p'
               (exponent) specification is optional. */
            return check;
            }
        /* Trailing closing parentheses were accepted on equal footing with '\0' bytes. */
        if(op_or_0 == ')') /* "2)"+3       @("-23/4)))))":-23/4)  */
            {
            check = DEFINITELYNONUMBER;
            }
        }
    else
        {
        check = DEFINITELYNONUMBER;
        }
    if(check && needNonZeroDigit)
        {
        check = 0;
        }
    return check;
    }

int fullnumbercheck(const char* begin)
    {
    if(*begin == '-')
        {
        int ret = numbercheck(begin + 1);
        if(ret & ~DEFINITELYNONUMBER)
            return ret | MINUS;
        else
            return ret;
        }
    else
        return numbercheck(begin);
    }

int sfullnumbercheck(char* begin, char* cutoff)
    {
    unsigned char sav = *cutoff;
    int ret;
    *cutoff = '\0';
    ret = fullnumbercheck(begin);
    *cutoff = sav;
    return ret;
    }
