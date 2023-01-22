#include "numbercheck.h"
#include "globals.h"
#include "nonnodetypes.h"

static int onlydigits(const char* begin)
    {
    int op_or_0;
    while(optab[op_or_0 = *begin++] != -1)
        {
        if(op_or_0 < '0' || op_or_0 > '9')
            {
            return 0;
            }
        }
    return 1;
    }

static const char* seeexponent(const char* begin)
    {
    int op_or_0;
    if(optab[op_or_0 = *begin++] != -1)
        {
        if(op_or_0 == '+' || op_or_0 == '-')
            {
            if(optab[op_or_0 = *begin++] != -1)
                {
                if((op_or_0 >= '0' && op_or_0 <= '9'))
                    {
                    if(!onlydigits(begin))
                        {
                        return 0;
                        }
                    }
                else
                    {
                    return 0;
                    }
                }
            else
                {
                return 0;
                }
            }
        else if(op_or_0 >= '0' && op_or_0 <= '9')
            {
            if(!onlydigits(begin))
                {
                return 0;
                }
            }
        else
            {
            return 0;
            }
        }
    else
        {
        return 0;
        }
    return begin;
    }

int numbercheck(const char* begin)
    {
    int op_or_0, check;
    int needNonZeroDigit = FALSE;
    int signseen = FALSE;
    if(!*begin)
        return 0;
    check = QNUMBER | QDOUBLE;
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
            if(*(begin + 1) == 'x' || *(begin + 1) == 'X')
                {
                /* hexadecimal number, must be in domain of strtod */
                char* endptr;
                double testdouble = strtod(begin, &endptr);
                if(*endptr)
                    return 0; /* format error */
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
            if(op_or_0 == '.' /* || op_or_0 == ','*/)
                {
                check &= ~QFRACTION;
                if(optab[op_or_0 = *++begin] != -1)
                    {
                    if(op_or_0 < '0' || op_or_0 > '9')
                        {
                        check = DEFINITELYNONUMBER;
                        }
                    else
                        {
                        while((check != DEFINITELYNONUMBER) && (optab[op_or_0 = *begin++] != -1))
                            {
                            if(op_or_0 == 'e' || op_or_0 == 'E')
                                {
                                begin = seeexponent(begin);
                                if(!begin)
                                    {
                                    check = DEFINITELYNONUMBER;
                                    break;
                                    }
                                }
                            else if((check & QNUL) && (op_or_0 != '0'))
                                {
                                check = DEFINITELYNONUMBER;
                                break;
                                }
                            else if(op_or_0 < '0' || op_or_0 > '9')
                                {
                                check = DEFINITELYNONUMBER;
                                break;
                                }
                            }
                        }
                    }
                }
            else if(op_or_0 == 'E' || op_or_0 == 'e')
                {
                check &= ~QFRACTION;
                begin = seeexponent(++begin);
                if(!begin)
                    {
                    check = DEFINITELYNONUMBER;
                    }
                }
            else if(signseen)
                check = DEFINITELYNONUMBER;
            else
                {
                check &= ~QDOUBLE;
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
                            check = DEFINITELYNONUMBER;
                            break;
                            }
                        else if(op_or_0 != '0')
                            {
                            needNonZeroDigit = FALSE;
                            /*check &= ~QNUL;*/
                            }
                        else if(needNonZeroDigit) /* '/' followed by '0' */
                            {
                            check = DEFINITELYNONUMBER;
                            break;
                            }
                        }
                    }
                }
            }
        else
            {
            check &= ~QDOUBLE;
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
/*  if(check & (QNUMBER | QNUL | QFRACTION | QDOUBLE))
        printf("%s",save);

    if(check & QNUMBER)
        printf("QNUMBER ");
    if(check & QNUL)
        printf("QNUL ");
    if(check & QFRACTION)
        printf("QFRACTION ");
    if(check & QDOUBLE)
        printf("QDOUBLE ");
    printf("\n");*/
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
