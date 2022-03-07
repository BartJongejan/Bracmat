#include "numbercheck.h"
#include "globals.h"
#include "nonnodetypes.h"

int numbercheck(const char *begin)
    {
    int op_or_0, check;
    int needNonZeroDigit = FALSE;
    if (!*begin)
        return 0;
    check = QNUMBER;
    op_or_0 = *begin;

    if (op_or_0 >= '0' && op_or_0 <= '9')
        {
        if (op_or_0 == '0')
            check |= QNUL;
        while (optab[op_or_0 = *++begin] != -1)
            {
            if (op_or_0 == '/')
                {
                /* check &= ~QNUL;*/
                if (check & QFRACTION)
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
            else if (op_or_0 < '0' || op_or_0 > '9')
                {
                check = DEFINITELYNONUMBER;
                break;
                }
            else
                {
                /* initial zero followed by
                                 0 <= k <= 9 makes no number */
                if ((check & (QNUL | QFRACTION)) == QNUL)
                    {
                    check = DEFINITELYNONUMBER;
                    break;
                    }
                else if (op_or_0 != '0')
                    {
                    needNonZeroDigit = FALSE;
                    /*check &= ~QNUL;*/
                    }
                else if (needNonZeroDigit) /* '/' followed by '0' */
                    {
                    check = DEFINITELYNONUMBER;
                    break;
                    }
                }
            }
        /* Trailing closing parentheses were accepted on equal footing with '\0' bytes. */
        if (op_or_0 == ')') /* "2)"+3       @("-23/4)))))":-23/4)  */
            {
            check = DEFINITELYNONUMBER;
            }
        }
    else
        {
        check = DEFINITELYNONUMBER;
        }
    if (check && needNonZeroDigit)
        {
        check = 0;
        }
    return check;
    }

int fullnumbercheck(const char *begin)
    {
    if (*begin == '-')
        {
        int ret = numbercheck(begin + 1);
        if (ret & ~DEFINITELYNONUMBER)
            return ret | MINUS;
        else
            return ret;
        }
    else
        return numbercheck(begin);
    }

int sfullnumbercheck(char *begin, char * cutoff)
    {
    unsigned char sav = *cutoff;
    int ret;
    *cutoff = '\0';
    ret = fullnumbercheck(begin);
    *cutoff = sav;
    return ret;
    }
