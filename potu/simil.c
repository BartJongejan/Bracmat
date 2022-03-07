#include "simil.h"
#include "platformdependentdefs.h"
#include "encoding.h"
#include <stdio.h>
#include <string.h>

static LONG simil
(const char* s1
    , const char* s1end
    , const char* s2
    , const char* s2end
    , int* putf1
    , int* putf2
    , LONG* plen1
    , LONG* plen2
)
    {
    const char* ls1;
    const char* s1l = NULL;
    const char* s1r = NULL;
    const char* s2l = NULL;
    const char* s2r = NULL;
    LONG max;
    LONG len1;
    LONG len2 = 0;
    /* regard each character in s1 as possible starting point for match */
    for (max = 0, ls1 = s1, len1 = 0
        ; ls1 < s1end
        ; getCodePoint2(&ls1, putf1), ++len1)
        {
        const char* ls2;
        /* compare with s2 */
        for (ls2 = s2, len2 = 0; ls2 < s2end; getCodePoint2(&ls2, putf2), ++len2)
            {
            const char* lls1 = ls1;
            const char* lls2 = ls2;
            /* determine lenght of equal parts */
            LONG len12 = 0;
            for (;;)
                {
                if (lls1 < s1end && lls2 < s2end)
                    {
                    const char* ns1 = lls1, * ns2 = lls2;
                    int K1 = getCodePoint2(&ns1, putf1);
                    int K2 = getCodePoint2(&ns2, putf2);
                    if (convertLetter(K1, u2l) == convertLetter(K2, u2l))
                        {
                        ++len12;
                        lls1 = ns1;
                        lls2 = ns2;
                        }
                    else
                        break;
                    }
                else
                    break;
                }
            /* adapt score if needed */
            if (len12 > max)
                {
                max = len12;
                /* remember end points of left strings and start points of
                   right strings */
                s1l = ls1;
                s1r = lls1;
                s2l = ls2;
                s2r = lls2;
                }
            }
        }
    if (max)
        {
        max += simil(s1, s1l, s2, s2l, putf1, putf2, NULL, NULL) + simil(s1r, s1end, s2r, s2end, putf1, putf2, NULL, NULL);
        }
    if (plen1)
        {
        *plen1 = len1;
        }
    if (plen2)
        {
        if (len1 == 0)
            {
            for (len2 = 0; *s2; getCodePoint2(&s2, putf2), ++len2)
                ;
            }
        *plen2 = len2;
        }
    return max;
    }

void Sim(char* draft, char* str1, char* str2)
    {
    int utf1 = 1;
    int utf2 = 1;
    LONG len1 = 0;
    LONG len2 = 0;
    LONG sim = simil(str1, str1 + strlen((char*)str1), str2, str2 + strlen((char*)str2), &utf1, &utf2, &len1, &len2);
    sprintf(draft, LONGD "/" LONGD, (2L * (LONG)sim), len1 + len2);
    }
