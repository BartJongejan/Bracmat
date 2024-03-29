#include "encoding.h"
#include "memory.h"
#include "copy.h"
#include "wipecopy.h"
#include "globals.h"
#include <assert.h>
#include <string.h>

#if CODEPAGE850
unsigned char ISO8859toCodePage850(unsigned char kar)

    {
    static unsigned char translationTable[] =
        {
        0xBA,0xCD,0xC9,0xBB,0xC8,0xBC,0xCC,0xB9,0xCB,0xCA,0xCE,0xDF,0xDC,0xDB,0xFE,0xF2,
        0xB3,0xC4,0xDA,0xBF,0xC0,0xD9,0xC3,0xB4,0xC2,0xC1,0xC5,0xB0,0xB1,0xB2,0xD5,0x9F,
        0xFF,0xAD,0xBD,0x9C,0xCF,0xBE,0xDD,0xF5,0xF9,0xB8,0xA6,0xAE,0xAA,0xF0,0xA9,0xEE,
        0xF8,0xF1,0xFD,0xFC,0xEF,0xE6,0xF4,0xFA,0xF7,0xFB,0xA7,0xAF,0xAC,0xAB,0xF3,0xA8,
        0xB7,0xB5,0xB6,0xC7,0x8E,0x8F,0x92,0x80,0xD4,0x90,0xD2,0xD3,0xDE,0xD6,0xD7,0xD8,
        0xD1,0xA5,0xE3,0xE0,0xE2,0xE5,0x99,0x9E,0x9D,0xEB,0xE9,0xEA,0x9A,0xED,0xE8,0xE1,
        0x85,0xA0,0x83,0xC6,0x84,0x86,0x91,0x87,0x8A,0x82,0x88,0x89,0x8D,0xA1,0x8C,0x8B,
        0xD0,0xA4,0x95,0xA2,0x93,0xE4,0x94,0xF6,0x9B,0x97,0xA3,0x96,0x81,0xEC,0xE7,0x98
        };

    if(kar & 0x80)
        return translationTable[kar & 0x7F];
    else
        return kar;
    /*    return kar & 0x80 ? (unsigned char)translationTable[kar & 0x7F] : kar;*/
    }

unsigned char CodePage850toISO8859(unsigned char kar)
    {
    static unsigned char translationTable[] =
        {
        0xC7,0xFC,0xE9,0xE2,0xE4,0xE0,0xE5,0xE7,0xEA,0xEB,0xE8,0xEF,0xEE,0xEC,0xC4,0xC5,
        0xC9,0xE6,0xC6,0xF4,0xF6,0xF2,0xFB,0xF9,0xFF,0xD6,0xDC,0xF8,0xA3,0xD8,0xD7,0x9F,
        0xE1,0xED,0xF3,0xFA,0xF1,0xD1,0xAA,0xBA,0xBF,0xAE,0xAC,0xBD,0xBC,0xA1,0xAB,0xBB,
        0x9B,0x9C,0x9D,0x90,0x97,0xC1,0xC2,0xC0,0xA9,0x87,0x80,0x83,0x85,0xA2,0xA5,0x93,
        0x94,0x99,0x98,0x96,0x91,0x9A,0xE3,0xC3,0x84,0x82,0x89,0x88,0x86,0x81,0x8A,0xA4,
        0xF0,0xD0,0xCA,0xCB,0xC8,0x9E,0xCD,0xCE,0xCF,0x95,0x92,0x8D,0x8C,0xA6,0xCC,0x8B,
        0xD3,0xDF,0xD4,0xD2,0xF5,0xD5,0xB5,0xFE,0xDE,0xDA,0xDB,0xD9,0xFD,0xDD,0xAF,0xB4,
        0xAD,0xB1,0x8F,0xBE,0xB6,0xA7,0xF7,0xB8,0xB0,0xA8,0xB7,0xB9,0xB3,0xB2,0x8E,0xA0,
        };

    /* 0x7F = 127, 0xFF = 255 */
    /* delete bit-7 before search in tabel (0-6 is unchanged) */
    /* delete bit 15-8 */

    if(kar & 0x80)
        return translationTable[kar & 0x7F];
    else
        return kar;
    /*    return kar & 0x80 ? (unsigned char)translationTable[kar & 0x7F] : kar;*/
    }
#endif

/* extern, is called from xml.c json.c */
unsigned char* putCodePoint(ULONG val, unsigned char* s)
    {
    /* Converts Unicode character w to 1,2,3 or 4 bytes of UTF8 in s. */
    if(val < 0x80)
        {
        *s++ = (char)val;
        }
    else
        {
        if(val < 0x0800) /* 7FF = 1 1111 111111 */
            {
            *s++ = (char)(0xc0 | (val >> 6));
            }
        else
            {
            if(val < 0x10000) /* FFFF = 1111 111111 111111 */
                {
                *s++ = (char)(0xe0 | (val >> 12));
                }
            else
                {
                if(val < 0x200000)
                    { /* 10000 = 010000 000000 000000, 10ffff = 100 001111 111111 111111 */
                    *s++ = (char)(0xf0 | (val >> 18));
                    }
                else
                    {
                    if(val < 0x4000000)
                        {
                        *s++ = (char)(0xf8 | (val >> 24));
                        }
                    else
                        {
                        if(val < 0x80000000)
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
    if(val < 0x80)
        {
        return 1;
        }
    else if(val < 0x0800) /* 7FF = 1 1111 111111 */
        {
        return 2;
        }
    else if(val < 0x10000) /* FFFF = 1111 111111 111111 */
        {
        return 3;
        }
    else if(val < 0x200000)
        { /* 10000 = 010000 000000 000000, 10ffff = 100 001111 111111 111111 */
        return 4;
        }
    else if(val < 0x4000000)
        {
        return 5;
        }
    else
        {
        return 6;
        }
    }

int getCodePoint(const char** ps)
    {
    /*
    return values:
    > 0:    code point
    -1:     no UTF-8
    -2:     too short for being UTF-8
    */
    int K;
    const char* s = *ps;
    if((K = (const unsigned char)*s++) != 0)
        {
        if((K & 0xc0) == 0xc0) /* 11bbbbbb */
            {
            int k[6];
            int i;
            int I;
            if((K & 0xfe) == 0xfe) /* 11111110 */
                {
                return -1;
                }
            /* Start of multibyte */

            k[0] = K;
            for(i = 1; (K << i) & 0x80; ++i)
                {
                k[i] = (const unsigned char)*s++;
                if((k[i] & 0xc0) != 0x80) /* 10bbbbbb */
                    {
                    if(k[i])
                        {
                        return -1;
                        }
                    return -2;
                    }
                }
            K = ((k[0] << i) & 0xff) << (5 * i - 6);
            I = --i;
            while(i > 0)
                {
                K |= (k[i] & 0x3f) << ((I - i) * 6);
                --i;
                }
            if(K <= 0x7F) /* ASCII, must be a single byte */
                {
                return -1;
                }
            }
        else if((K & 0xc0) == 0x80) /* 10bbbbbb, wrong first byte */
            {
            return -1;
            }
        }
    *ps = s; /* next character */
    return K;
    }

int getCodePoint2(const char** ps, int* isutf)
    {
    int ks = *isutf ? getCodePoint(ps) : (const unsigned char)*(*ps)++;
    if(ks < 0)
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
        char* s;
    psk pnode;
    size_t len;
    pnode = same_as_w(Pnode);
    s = SPOBJ(Pnode);
    len = strlen((const char*)s);
    if(len > 0)
        {
        char* d;
        char* dwarn;
        char* buf = NULL;
        char* obuf;
        pnode = isolated(pnode);
        d = SPOBJ(pnode);
        obuf = d;
        dwarn = obuf + strlen((const char*)obuf) - 6;
#if CODEPAGE850
        if(dos)
            {
            if(low)
                {
                for(; *s; ++s)
                    {
                    *s = ISO8859toCodePage850(lowerEquivalent[(int)(const unsigned char)*s]);
                    }
                }
            else
                {
                for(; *s; ++s)
                    {
                    *s = ISO8859toCodePage850(upperEquivalent[(int)(const unsigned char)*s]);
                    }
                }
            }
        else
#endif
            {
            int isutf = 1;
            struct ccaseconv* t = low ? u2l : l2u;
            for(; *s;)
                {
                int S = getCodePoint2(&s, &isutf);
                int D = convertLetter(S, t);
                if(isutf)
                    {
                    if(d >= dwarn)
                        {
                        int nb = utf8bytes(D);
                        if(d + nb >= dwarn + 6)
                            {
                            /* overrun */
                            buf = (char*)bmalloc(2 * ((dwarn + 6) - obuf));
                            dwarn = buf + 2 * ((dwarn + 6) - obuf) - 6;
                            memcpy(buf, obuf, d - obuf);
                            d = buf + (d - obuf);
                            if(obuf != SPOBJ(pnode))
                                bfree(obuf);
                            obuf = buf;
                            }
                        }
                    d = (char*)putCodePoint(D, (unsigned char*)d);
                    }
                else
                    *d++ = (unsigned char)D;
                }
            *d = 0;
            if(buf)
                {
                wipe(pnode);
                pnode = scopy(buf);
                bfree(buf);
                }
            }
        }
    return pnode;
    }

int hasUTF8MultiByteCharacters(const char* s)
    { /* returns 0 if s is not valid UTF-8 or if s is pure 7-bit ASCII */
    int ret;
    int multiByteCharSeen = 0;
    for(; (ret = getCodePoint(&s)) > 0;)
        if(ret > 0x7F)
            ++multiByteCharSeen;
    return ret == 0 ? multiByteCharSeen : 0;
    }

int strcasecompu(char** S, char** P, char* cutoff)
/* Additional argument cutoff */
    {
    int sutf = 1;
    int putf = 1;
    char* s = *S;
    char* p = *P;
    while(s < cutoff && *s && *p)
        {
        int diff;
        char* ns = s;
        char* np = p;
        int ks = getCodePoint2((const char**)&ns, &sutf);
        int kp = getCodePoint2((const char**)&np, &putf);
        assert(ks >= 0 && kp >= 0);
        diff = toLowerUnicode(ks) - toLowerUnicode(kp);
        if(diff)
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

int strcasecomp(const char* s, const char* p)
    {
    int sutf = 1; /* assume UTF-8, becomes 0 if it is not */
    int putf = 1;
    while(*s && *p)
        {
        int ks = getCodePoint2((const char**)&s, &sutf);
        int kp = getCodePoint2((const char**)&p, &putf);
        int diff = toLowerUnicode(ks) - toLowerUnicode(kp);
        if(diff)
            {
            return diff;
            }
        }
    return (int)(const unsigned char)*s - (int)(const unsigned char)*p;
    }

#if CODEPAGE850
int strcasecmpDOS(const char* s, const char* p)
    {
    while(*s && *p)
        {
        int diff = (int)ISO8859toCodePage850(lowerEquivalent[CodePage850toISO8859((unsigned char)*s)]) - (int)ISO8859toCodePage850(lowerEquivalent[CodePage850toISO8859((unsigned char)*p)]);
        if(diff)
            return diff;
        ++s;
        ++p;
        }
    return (int)*s - (int)*p;
    }
#endif
