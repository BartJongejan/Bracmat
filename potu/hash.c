#include "hash.h"
#include "hashtypes.h"
#include "encoding.h"
#include "memory.h"
#include "copy.h"
#include "wipecopy.h"
#include "globals.h"
#include "input.h"
#include "eval.h"
#include <string.h>
#include <assert.h>

static LONG casesensitivehash(const char* cp)
    {
    LONG hash_temp = 0;
    while (*cp != '\0')
        {
        if (hash_temp < 0)
            {
            hash_temp = (hash_temp << 1) + 1;
            }
        else
            {
            hash_temp = hash_temp << 1;
            }
        hash_temp ^= *(const signed char*)cp;
        ++cp;
        }
    return hash_temp;
    }

static LONG caseinsensitivehash(const char* cp)
    {
    LONG hash_temp = 0;
    int isutf = 1;
    while (*cp != '\0')
        {
        if (hash_temp < 0)
            {
            hash_temp = (hash_temp << 1) + 1;
            }
        else
            {
            hash_temp = hash_temp << 1;
            }
        hash_temp ^= toLowerUnicode(getCodePoint2((const char**)&cp, &isutf));
        }
    return hash_temp;
    }

#if CODEPAGE850
static LONG caseinsensitivehashDOS(const char* cp)
    {
    LONG hash_temp = 0;
    while (*cp != '\0')
        {
        if (hash_temp < 0)
            hash_temp = (hash_temp << 1) + 1;
        else
            hash_temp = hash_temp << 1;
        hash_temp ^= ISO8859toCodePage850(lowerEquivalent[CodePage850toISO8859(*cp)]);
        ++cp;
        }
    return hash_temp;
    }
#endif

static psk removeFromHash(Hash* temp, psk Arg)
    {
    const char* key = (const char*)POBJ(Arg);
    LONG i;
    LONG hash_temp;
    pskRecord** pr;
    hash_temp = (*temp->hashfunc)(key);
    assert(temp->hash_size);
    i = ((unsigned int)hash_temp) % temp->hash_size;
    pr = temp->hash_table + i;
    if (*pr)
        {
        while (*pr)
            {
            if (Op((*pr)->entry) == WHITE)
                {
                if (!(*temp->cmpfunc)(key, (const char*)POBJ((*pr)->entry->LEFT->LEFT)))
                    break;
                }
            else if (!(*temp->cmpfunc)(key, (const char*)POBJ((*pr)->entry->LEFT)))
                break;
            pr = &(*pr)->next;
            }
        if (*pr)
            {
            pskRecord* next = (*pr)->next;
            psk ret = (*pr)->entry;
            bfree(*pr);
            *pr = next;
            --temp->record_count;
            return ret;
            }
        }
    return NULL;
    }

static psk inserthash(Hash* temp, psk Arg)
    {
    const char* key = (const char*)POBJ(Arg->LEFT);
    LONG i;
    LONG hash_temp;
    pskRecord* r;
    hash_temp = (*temp->hashfunc)(key);
    assert(temp->hash_size);
    i = ((unsigned int)hash_temp) % temp->hash_size;
    r = temp->hash_table[i];
    if (!r)
        --temp->unoccupied;
    else
        while (r)
            {
            if (Op(r->entry) == WHITE)
                {
                if (!(*temp->cmpfunc)(key, (const char*)POBJ(r->entry->LEFT->LEFT)))
                    {
                    break;
                    }
                }
            else
                {
                if (!(*temp->cmpfunc)(key, (const char*)POBJ(r->entry->LEFT)))
                    {
                    break;
                    }
                }
            r = r->next;
            }
    if (r)
        {
        psk goal = (psk)bmalloc(__LINE__, sizeof(knode));
        goal->v.fl = WHITE | SUCCESS;
        goal->v.fl &= ~ALL_REFCOUNT_BITS_SET;
        goal->LEFT = same_as_w(Arg);
        goal->RIGHT = r->entry;
        r->entry = goal;
        }
    else
        {
        r = (pskRecord*)bmalloc(__LINE__, sizeof(pskRecord));
        r->entry = same_as_w(Arg);
        r->next = temp->hash_table[i];
        temp->hash_table[i] = r;
        ++temp->record_count;
        }
    ++temp->elements;
    return r->entry;
    }

static psk findhash(Hash* temp, psk Arg)
    {
    const char* key = (const char*)POBJ(Arg);
    LONG i;
    LONG hash_temp;
    pskRecord* r;
    hash_temp = (*temp->hashfunc)(key);
    assert(temp->hash_size);
    i = ((unsigned int)hash_temp) % temp->hash_size;
    r = temp->hash_table[i];
    if (r)
        {
        while (r)
            {
            if (Op(r->entry) == WHITE)
                {
                if (!(*temp->cmpfunc)(key, (const char*)POBJ(r->entry->LEFT->LEFT)))
                    break;
                }
            else if (!(*temp->cmpfunc)(key, (const char*)POBJ(r->entry->LEFT)))
                break;
            r = r->next;
            }
        if (r)
            return r->entry;
        }
    return NULL;
    }

static void freehash(Hash* temp)
    {
    if (temp)
        {
        if (temp->hash_table)
            {
            ULONG i;
            for (i = temp->hash_size; i > 0;)
                {
                pskRecord* r = temp->hash_table[--i];
                pskRecord* next;
                while (r)
                    {
                    wipe(r->entry);
                    next = r->next;
                    bfree(r);
                    r = next;
                    }
                }
            bfree(temp->hash_table);
            }
        bfree(temp);
        }
    }

static Hash* newhash(ULONG size)
    {
    ULONG i;
    Hash* temp = (Hash*)bmalloc(__LINE__, sizeof(Hash));
    assert(size > 0);
    temp->hash_size = size;
    temp->record_count = (unsigned int)0;
    temp->hash_table = (pskRecord**)bmalloc(__LINE__, sizeof(pskRecord*) * temp->hash_size);
#ifdef __VMS
    temp->cmpfunc = (int(*)())strcmp;
#else
    temp->cmpfunc = strcmp;
#endif
    temp->hashfunc = casesensitivehash;
    temp->elements = 0L;     /* elements >= record_count */
    temp->record_count = 0L; /* record_count >= size - unoccupied */
    temp->unoccupied = size;
    for (i = temp->hash_size; i > 0;)
        temp->hash_table[--i] = NULL;
    return temp;
    }

static ULONG nextprime(ULONG g)
    {
    /* For primality test, only try divisors that are 2, 3 or 5 or greater
    numbers that are not multiples of 2, 3 or 5. Candidates below 100 are:
     2  3  5  7 11 13 17 19 23 29 31 37 41 43
    47 49 53 59 61 67 71 73 77 79 83 89 91 97
    Of these 28 candidates, three are not prime:
    49 (7*7), 77 (7*11) and 91 (7*13) */
    int i;
    ULONG smalldivisor;
    static int bijt[12] =
        { 1,  2,  2,  4,    2,    4,    2,    4,    6,    2,  6 };
    /*2-3,3-5,5-7,7-11,11-13,13-17,17-19,19-23,23-29,29-1,1-7*/
    ULONG bigdivisor;
    if (!(g & 1))
        {
        if (g <= 2)
            return 2;
        ++g;
        }
    smalldivisor = 2;
    i = 0;
    while ((bigdivisor = g / smalldivisor) >= smalldivisor)
        {
        if (bigdivisor * smalldivisor == g)
            {
            g += 2;
            smalldivisor = 2;
            i = 0;
            }
        else
            {
            smalldivisor += bijt[i];
            if (++i > 10)
                i = 3;
            }
        }
    return g;
    }

static void rehash(Hash** ptemp, int loadFactor/*1-100*/)
    {
    Hash* temp = *ptemp;
    if (temp)
        {
        ULONG newsize;
        Hash* newtable;
        newsize = nextprime((100 * temp->record_count) / loadFactor);
        if (!newsize)
            newsize = 1;
        newtable = newhash(newsize);
        newtable->cmpfunc = temp->cmpfunc;
        newtable->hashfunc = temp->hashfunc;
        if (temp->hash_table)
            {
            ULONG i;
            for (i = temp->hash_size; i > 0;)
                {
                pskRecord* r = temp->hash_table[--i];
                while (r)
                    {
                    psk Pnode = r->entry;
                    while (is_op(Pnode) && Op(Pnode) == WHITE)
                        {
                        inserthash(newtable, Pnode->LEFT);
                        Pnode = Pnode->RIGHT;
                        }
                    inserthash(newtable, Pnode);
                    r = r->next;
                    }
                }
            }
        freehash(temp);
        *ptemp = newtable;
        }
    }

static int loadfactor(Hash* temp)
    {
    assert(temp->hash_size);
    if (temp->record_count < 10000000L)
        return (int)((100 * temp->record_count) / temp->hash_size);
    else
        return (int)(temp->record_count / (temp->hash_size / 100));
    }

static Boolean hashinsert(struct typedObjectnode* This, ppsk arg)
    {
    psk Arg = (*arg)->RIGHT;
    if (is_op(Arg) && !is_op(Arg->LEFT))
        {
        psk ret;
        int lf = loadfactor(HASH(This));
        if (lf > 100)
            rehash((Hash**)(&(This->voiddata)), 60);
        ret = inserthash(HASH(This), Arg);
        wipe(*arg);
        *arg = same_as_w(ret);
        return TRUE;
        }
    return FALSE;
    }

static Boolean hashfind(struct typedObjectnode* This, ppsk arg)
    {
    psk Arg = (*arg)->RIGHT;
    if (!is_op(Arg))
        {
        psk ret = findhash(HASH(This), Arg);
        if (ret)
            {
            wipe(*arg);
            *arg = same_as_w(ret);
            return TRUE;
            }
        }
    return FALSE;
    }

static Boolean hashremove(struct typedObjectnode* This, ppsk arg)
    {
    psk Arg = (*arg)->RIGHT;
    if (!is_op(Arg))
        {
        Hash* temp = HASH(This);
        psk ret = removeFromHash(temp, Arg);
        if (ret)
            {
            if (loadfactor(temp) < 50 && temp->hash_size > 97)
                rehash(PHASH(This), 90);
            wipe(*arg);
            *arg = ret;
            return TRUE;
            }
        }
    return FALSE;
    }

static Boolean hashnew(struct typedObjectnode* This, ppsk arg)
    {
    /*    UNREFERENCED_PARAMETER(arg);*/
    unsigned long Nprime = 97;
    if (INTEGER_POS_COMP((*arg)->RIGHT))
        {
        Nprime = strtoul((char*)POBJ((*arg)->RIGHT), NULL, 10);
        if (Nprime == 0 || Nprime == ULONG_MAX)
            Nprime = 97;
        }
    VOID(This) = (void*)newhash(Nprime);
    return TRUE;
    }

static Boolean hashdie(struct typedObjectnode* This, ppsk arg)
    {
    UNREFERENCED_PARAMETER(arg);
    freehash(HASH(This));
    return TRUE;
    }

#if CODEPAGE850
static Boolean hashDOS(struct typedObjectnode* This, ppsk arg)
    {
    UNREFERENCED_PARAMETER(arg);
    (HASH(This))->hashfunc = caseinsensitivehashDOS;
    (HASH(This))->cmpfunc = strcasecmpDOS;
    rehash(PHASH(This), 100);
    return TRUE;
    }
#endif

static Boolean hashISO(struct typedObjectnode* This, ppsk arg)
    {
    UNREFERENCED_PARAMETER(arg);
    (HASH(This))->hashfunc = caseinsensitivehash;
    (HASH(This))->cmpfunc = strcasecomp;
    rehash(PHASH(This), 100);
    return TRUE;
    }

static Boolean hashcasesensitive(struct typedObjectnode* This, ppsk arg)
    {
    UNREFERENCED_PARAMETER(arg);
    (HASH(This))->hashfunc = casesensitivehash;
#ifdef __VMS
    (HASH(This))->cmpfunc = (int(*)())strcmp;
#else
    (HASH(This))->cmpfunc = strcmp;
#endif
    rehash(PHASH(This), 100);
    return TRUE;
    }


static Boolean hashforall(struct typedObjectnode* This, ppsk arg)
    {
    ULONG i;
    int ret = TRUE;
    This = (typedObjectnode*)same_as_w((psk)This);
    for (i = 0
        ;    ret && HASH(This)
        && i < (HASH(This))->hash_size
        ;
        )
        {
        pskRecord* r = (HASH(This))->hash_table[i];
        int j = 0;
        while (r)
            {
            int m;
            psk Pnode = NULL;
            addr[2] = (*arg)->RIGHT; /* each time! addr[n] may be overwritten by evaluate (below)*/
            addr[3] = r->entry;
            Pnode = build_up(Pnode, "(\2'\3)", NULL);
            Pnode = eval(Pnode);
            ret = isSUCCESSorFENCE(Pnode);
            wipe(Pnode);
            if (!ret
                || !HASH(This)
                || i >= (HASH(This))->hash_size
                || !(HASH(This))->hash_table[i]
                )
                break;
            ++j;
            for (m = 0, r = (HASH(This))->hash_table[i]
                ; r && m < j
                ; ++m
                )
                r = r->next;
            }
        ++i;
        }
    wipe((psk)This);
    return TRUE;
    }

method hash[] = {
    {"find",hashfind},
    {"insert",hashinsert},
    {"remove",hashremove},
    {"New",hashnew},
    {"Die",hashdie},
#if CODEPAGE850
    {"DOS",hashDOS},
#endif
    {"ISO",hashISO},
    {"casesensitive",hashcasesensitive},
    {"forall",hashforall},
    {NULL,NULL} };
/*
Standard methods are 'New' and 'Die'.
A user defined 'die' can be added after creation of the object and will be invoked just before 'Die'.

Example:

new$hash:?h;

     (=(Insert=.out$Insert & lst$its & lst$Its & (Its..insert)$!arg)
      (die = .out$"Oh dear")
    ):(=?(h.));

    (h..Insert)$(X.x);

    :?h;



    (=(Insert=.out$Insert & lst$its & lst$Its & (Its..insert)$!arg)
      (die = .out$"The end.")
    ):(=?(new$hash:?k));

    (k..Insert)$(Y.y);

    :?k;

A little problem is that in the last example, the '?' ends up as a flag on the '=' node.

Bart 20010222

*/

