#include "encoding.h"
#include "memory.h"
#include "copy.h"
#include "wipecopy.h"
#include <assert.h>
#include <string.h>

int convertLetter(int a, struct ccaseconv * T)
    {
    int i;
    if (a > 0x10FFFF)
        return a;
    for (i = 0;; ++i)
        {
        if ((unsigned int)a < T[i].L)
            {
            if (i == 0)
                return a;
            --i;
            if ((unsigned int)a <= (unsigned int)(T[i].L + T[i].range)
                && (T[i].inc < 2
                    || !((a - T[i].L) & 1)
                    )
                )
                {
                return a + T[i].dif;
                }
            else
                {
                break;
                }
            }
        }
    return a;
    }

/* extern, is called from xml.c json.c */
unsigned char * putCodePoint(ULONG val, unsigned char * s)
    {
    /* Converts Unicode character w to 1,2,3 or 4 bytes of UTF8 in s. */
    if (val < 0x80)
        {
        *s++ = (char)val;
        }
    else
        {
        if (val < 0x0800) /* 7FF = 1 1111 111111 */
            {
            *s++ = (char)(0xc0 | (val >> 6));
            }
        else
            {
            if (val < 0x10000) /* FFFF = 1111 111111 111111 */
                {
                *s++ = (char)(0xe0 | (val >> 12));
                }
            else
                {
                if (val < 0x200000)
                    { /* 10000 = 010000 000000 000000, 10ffff = 100 001111 111111 111111 */
                    *s++ = (char)(0xf0 | (val >> 18));
                    }
                else
                    {
                    if (val < 0x4000000)
                        {
                        *s++ = (char)(0xf8 | (val >> 24));
                        }
                    else
                        {
                        if (val < 0x80000000)
                            {
                            *s++ = (char)(0xfc | (val >> 30));
                            *s++ = (char)(0x80 | ((val >> 24) & 0x3f));
                            }
                        else
                            return NULL;
                        }
                    *s++ = (char)(0x80 | ((val >> 18) & 0x3f));
                    }
                *s++ = (char)(0x80 | ((val >> 12) & 0x3f));
                }
            *s++ = (char)(0x80 | ((val >> 6) & 0x3f));
            }
        *s++ = (char)(0x80 | (val & 0x3f));
        }
    *s = (char)0;
    return s;
    }

static int utf8bytes(ULONG val)
    {
    if (val < 0x80)
        {
        return 1;
        }
    else if (val < 0x0800) /* 7FF = 1 1111 111111 */
        {
        return 2;
        }
    else if (val < 0x10000) /* FFFF = 1111 111111 111111 */
        {
        return 3;
        }
    else if (val < 0x200000)
        { /* 10000 = 010000 000000 000000, 10ffff = 100 001111 111111 111111 */
        return 4;
        }
    else if (val < 0x4000000)
        {
        return 5;
        }
    else
        {
        return 6;
        }
    }

int getCodePoint(const char ** ps)
    {
    /*
    return values:
    > 0:    code point
    -1:     no UTF-8
    -2:     too short for being UTF-8
    */
    int K;
    const char * s = *ps;
    if ((K = (const unsigned char)*s++) != 0)
        {
        if ((K & 0xc0) == 0xc0) /* 11bbbbbb */
            {
            int k[6];
            int i;
            int I;
            if ((K & 0xfe) == 0xfe) /* 11111110 */
                {
                return -1;
                }
            /* Start of multibyte */

            k[0] = K;
            for (i = 1; (K << i) & 0x80; ++i)
                {
                k[i] = (const unsigned char)*s++;
                if ((k[i] & 0xc0) != 0x80) /* 10bbbbbb */
                    {
                    if (k[i])
                        {
                        return -1;
                        }
                    return -2;
                    }
                }
            K = ((k[0] << i) & 0xff) << (5 * i - 6);
            I = --i;
            while (i > 0)
                {
                K |= (k[i] & 0x3f) << ((I - i) * 6);
                --i;
                }
            if (K <= 0x7F) /* ASCII, must be a single byte */
                {
                return -1;
                }
            }
        else if ((K & 0xc0) == 0x80) /* 10bbbbbb, wrong first byte */
            {
            return -1;
            }
        }
    *ps = s; /* next character */
    return K;
    }

int getCodePoint2(const char ** ps, int * isutf)
    {
    int ks = *isutf ? getCodePoint(ps) : (const unsigned char)*(*ps)++;
    if (ks < 0)
        {
        *isutf = 0;
        ks = (const unsigned char)*(*ps)++;
        }
    assert(ks >= 0);
    return ks;
    }

psk changeCase(psk Pnode
#if CODEPAGE850
                      , int dos
#endif
                      , int low)
    {
#if !CODEPAGE850
    const
#endif
        char * s;
    psk pnode;
    size_t len;
    pnode = same_as_w(Pnode);
    s = SPOBJ(Pnode);
    len = strlen((const char *)s);
    if (len > 0)
        {
        char * d;
        char * dwarn;
        char * buf = NULL;
        char * obuf;
        pnode = isolated(pnode);
        d = SPOBJ(pnode);
        obuf = d;
        dwarn = obuf + strlen((const char*)obuf) - 6;
#if CODEPAGE850
        if (dos)
            {
            if (low)
                {
                for (; *s; ++s)
                    {
                    *s = ISO8859toCodePage850(lowerEquivalent[(int)(const unsigned char)*s]);
                    }
                }
            else
                {
                for (; *s; ++s)
                    {
                    *s = ISO8859toCodePage850(upperEquivalent[(int)(const unsigned char)*s]);
                    }
                }
            }
        else
#endif
            {
            int isutf = 1;
            struct ccaseconv * t = low ? u2l : l2u;
            for (; *s;)
                {
                int S = getCodePoint2(&s, &isutf);
                int D = convertLetter(S, t);
                if (isutf)
                    {
                    if (d >= dwarn)
                        {
                        int nb = utf8bytes(D);
                        if (d + nb >= dwarn + 6)
                            {
                            /* overrun */
                            buf = (char *)bmalloc(__LINE__, 2 * ((dwarn + 6) - obuf));
                            dwarn = buf + 2 * ((dwarn + 6) - obuf) - 6;
                            memcpy(buf, obuf, d - obuf);
                            d = buf + (d - obuf);
                            if (obuf != SPOBJ(pnode))
                                bfree(obuf);
                            obuf = buf;
                            }
                        }
                    d = (char *)putCodePoint(D, (unsigned char *)d);
                    }
                else
                    *d++ = (unsigned char)D;
                }
            *d = 0;
            if (buf)
                {
                wipe(pnode);
                pnode = scopy(buf);
                bfree(buf);
                }
            }
        }
    return pnode;
    }

int toLowerUnicode(int a)
    {
    return convertLetter(a, u2l);
    }

int hasUTF8MultiByteCharacters(const char * s)
    { /* returns 0 if s is not valid UTF-8 or if s is pure 7-bit ASCII */
    int ret;
    int multiByteCharSeen = 0;
    for (; (ret = getCodePoint(&s)) > 0;)
        if (ret > 0x7F)
            ++multiByteCharSeen;
    return ret == 0 ? multiByteCharSeen : 0;
    }

int strcasecompu(char ** S, char ** P, char * cutoff)
/* Additional argument cutoff */
    {
    int sutf = 1;
    int putf = 1;
    char * s = *S;
    char * p = *P;
    while (s < cutoff && *s && *p)
        {
        int diff;
        char * ns = s;
        char * np = p;
        int ks = getCodePoint2((const char **)&ns, &sutf);
        int kp = getCodePoint2((const char **)&np, &putf);
        assert(ks >= 0 && kp >= 0);
        diff = toLowerUnicode(ks) - toLowerUnicode(kp);
        if (diff)
            {
            *S = s;
            *P = p;
            return diff;
            }
        s = ns;
        p = np;
        }
    *S = s;
    *P = p;
    return (s < cutoff ? (int)(unsigned char)*s : 0) - (int)(unsigned char)*p;
    }

int strcasecomp(const char *s, const char *p)
    {
    int sutf = 1; /* assume UTF-8, becomes 0 if it is not */
    int putf = 1;
    while (*s && *p)
        {
        int ks = getCodePoint2((const char **)&s, &sutf);
        int kp = getCodePoint2((const char **)&p, &putf);
        int diff = toLowerUnicode(ks) - toLowerUnicode(kp);
        if (diff)
            {
            return diff;
            }
        }
    return (int)(const unsigned char)*s - (int)(const unsigned char)*p;
    }

#if CODEPAGE850
int strcasecmpDOS(const char *s, const char *p)
    {
    while (*s && *p)
        {
        int diff = (int)ISO8859toCodePage850(lowerEquivalent[CodePage850toISO8859((unsigned char)*s)]) - (int)ISO8859toCodePage850(lowerEquivalent[CodePage850toISO8859((unsigned char)*p)]);
        if (diff)
            return diff;
        ++s;
        ++p;
        }
    return (int)*s - (int)*p;
    }
#endif
