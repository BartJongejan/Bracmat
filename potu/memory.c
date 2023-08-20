#include "memory.h"

#include "parse.h"
#include "globals.h"
#include "objectnode.h"
#include "filewrite.h"
#include "typedobjectnode.h"
#include "nodedefs.h"
#include "nodestruct.h"
#include "eval.h"
#include "wipecopy.h"
#include <stddef.h>
#include <assert.h>
#include <string.h>

/*
After running valid.bra  on 32 bit platform
1     8           32
2 16384       131072
3 32696       392352
4  1024        16384
5  256          5120
6 2048         49152
              ------+
total bytes = 594112
*/
#if WORD32
#define MEM1SIZE 8
#define MEM2SIZE 16384
#define MEM3SIZE 32696
#define MEM4SIZE 1024
#define MEM5SIZE 256
#define MEM6SIZE 2048
#else
/*
After running valid.bra  on 64 bit platform
1     8            64
2 16384        262144
3 32768        786432
4  1024         32768
5  2048         81920
6    64          3072
              -------+
total bytes = 1166400
*/
#define MEM1SIZE 8
#define MEM2SIZE 16384
#define MEM3SIZE 32768
#define MEM4SIZE 1024
#define MEM5SIZE 2048
#define MEM6SIZE 64
#endif


#if SHOWMAXALLOCATED
static size_t globalloc = 0, maxgloballoc = 0;
#endif

#if SHOWCURRENTLYALLOCATED
static size_t cnts[256], alloc_cnt = 0, totcnt = 0;
#endif

struct memblock
    {
    struct memoryElement* lowestAddress;
    struct memoryElement* highestAddress;
    struct memoryElement* firstFreeElementBetweenAddresses; /* if NULL : no more free elements */
    struct memblock* previousOfSameLength; /* address of older ands smaller block with same sized elements */
    size_t sizeOfElement;
#if SHOWMAXALLOCATED
    size_t numberOfFreeElementsBetweenAddresses; /* optional field. */
    size_t numberOfElementsBetweenAddresses; /* optional field. */
    size_t minimumNumberOfFreeElementsBetweenAddresses;
#endif
    };

struct allocation
    {
    size_t elementSize;
    int numberOfElements;
    struct memblock* memoryBlock;
    };

static struct allocation* global_allocations;
static int global_nallocations = 0;


struct memoryElement
    {
    struct memoryElement* next;
    };
/*
struct pointerStruct
    {
    struct pointerStruct * lp;
    } *global_p, *global_ep;
*/
struct memblock** pMemBlocks = 0; /* list of memblock, sorted
                                      according to memory address */
static int NumberOfMemBlocks = 0;
#if SHOWMAXALLOCATED
static int malloced = 0;
#endif

#if DOSUMCHECK

static int LineNo;
static int globN;


static int getchecksum(void)
    {
    int i;
    int sum = 0;
    for(i = 0; i < NumberOfMemBlocks; ++i)
        {
        struct memoryElement* me;
        struct memblock* mb = pMemBlocks[i];
        me = mb->firstFreeElementBetweenAddresses;
        while(me)
            {
            sum += (LONG)me;
            me = me->next;
            }
        }
    return sum;
    }

static int Checksum = 0;

static void setChecksum(int lineno, int n)
    {
    if(lineno)
        {
        LineNo = lineno;
        globN = n;
        }
    Checksum = getchecksum();
    }

static void checksum(int line)
    {
    static int nChecksum = 0;
    nChecksum = getchecksum();
    if(Checksum && Checksum != nChecksum)
        {
        Printf("Line %d: Illegal write after bmalloc(%d) on line %d", line, globN, LineNo);
        getchar();
        exit(1);
        }
    }
#else
#define setChecksum(a,b)
#define checksum(a)
#endif

static struct memblock* initializeMemBlock(size_t elementSize, size_t numberOfElements)
    {
    size_t nlongpointers;
    size_t stepSize;
    struct memblock* mb;
    struct memoryElement* mEa, * mEz;
    mb = (struct memblock*)malloc(sizeof(struct memblock));
    if(mb)
        {
        mb->sizeOfElement = elementSize;
        mb->previousOfSameLength = 0;
        stepSize = elementSize / sizeof(struct memoryElement);
        nlongpointers = stepSize * numberOfElements;
        mb->firstFreeElementBetweenAddresses = mb->lowestAddress = (struct memoryElement*)malloc(elementSize * numberOfElements);
        if(mb->lowestAddress == 0)
            {
#if _BRACMATEMBEDDED
            return 0;
#else
            exit(-1);
#endif
            }
        else
            {
            mEa = mb->lowestAddress;
            mb->highestAddress = mEa + nlongpointers;
#if SHOWMAXALLOCATED
            mb->numberOfFreeElementsBetweenAddresses = numberOfElements;
            mb->numberOfElementsBetweenAddresses = numberOfElements;
            mb->minimumNumberOfFreeElementsBetweenAddresses = numberOfElements;
#endif
            mEz = mb->highestAddress - stepSize;
            for(; mEa < mEz; )
                {
                mEa->next = mEa + stepSize;
                assert(((LONG)(mEa->next) & 1) == 0);
                mEa = mEa->next;
                }
            assert(mEa == mEz);
            mEa->next = 0;
            return mb;
            }
        }
    else
#if _BRACMATEMBEDDED
        return 0;
#else
        exit(-1);
#endif
    }

/* The newMemBlocks function is introduced because the same code,
if in-line in bmalloc, and if compiled with -O3, doesn't run. */
static struct memblock* newMemBlocks(size_t n)
    {
    struct memblock* mb;
    int i, j = 0;
    struct memblock** npMemBlocks;
    mb = initializeMemBlock(global_allocations[n].elementSize, global_allocations[n].numberOfElements);
    if(!mb)
        return 0;
    global_allocations[n].numberOfElements *= 2;
    mb->previousOfSameLength = global_allocations[n].memoryBlock;
    global_allocations[n].memoryBlock = mb;

    ++NumberOfMemBlocks;
    npMemBlocks = (struct memblock**)malloc((NumberOfMemBlocks) * sizeof(struct memblock*));
    if(npMemBlocks)
        {
        for(i = 0; i < NumberOfMemBlocks - 1; ++i)
            {
            if(mb < pMemBlocks[i])
                {
                npMemBlocks[j++] = mb;
                for(; i < NumberOfMemBlocks - 1; ++i)
                    {
                    npMemBlocks[j++] = pMemBlocks[i];
                    }
                free(pMemBlocks);
                pMemBlocks = npMemBlocks;
#if SHOWMEMBLOCKS
                showMemBlocks();
#endif
                return mb;
                }
            npMemBlocks[j++] = pMemBlocks[i];
            }
        npMemBlocks[j] = mb;
        free(pMemBlocks);
        pMemBlocks = npMemBlocks;
#if SHOWMEMBLOCKS
        showMemBlocks();
#endif
        return mb;
        }
    else
        {
#if _BRACMATEMBEDDED
        return 0;
#else
        exit(-1);
#endif
        }

    }

#if SHOWCURRENTLYALLOCATED
void bezetting(void)
    {
    struct memblock* mb = 0;
    size_t words = 0;
    int i;
    Printf("\nfree\n");
    for(i = 0; i < NumberOfMemBlocks; ++i)
        {
        mb = pMemBlocks[i];
#if WORD32 || defined __VMS
        Printf("%zd words per node : %lu of %lu\n", mb->sizeOfElement / sizeof(struct memoryElement), mb->numberOfFreeElementsBetweenAddresses, mb->numberOfElementsBetweenAddresses);
#else                                                                                                                                         
        Printf("%zd words per node : %zu of %lu\n", mb->sizeOfElement / sizeof(struct memoryElement), mb->numberOfFreeElementsBetweenAddresses, mb->numberOfElementsBetweenAddresses);
#endif
        }
    Printf("\nmin free\n");
    for(i = 0; i < NumberOfMemBlocks; ++i)
        {
        mb = pMemBlocks[i];
#if WORD32 || defined __VMS
        Printf("%zd words per node : %lu of %lu\n", mb->sizeOfElement / sizeof(struct memoryElement), mb->minimumNumberOfFreeElementsBetweenAddresses, mb->numberOfElementsBetweenAddresses);
#else                                                                                                                                                
        Printf("%zd words per node : %zu of %lu\n", mb->sizeOfElement / sizeof(struct memoryElement), mb->minimumNumberOfFreeElementsBetweenAddresses, mb->numberOfElementsBetweenAddresses);
#endif
        }
    Printf("more than %zd words per node : %u\n", words, malloced);
    }
#endif

void* bmalloc(int lineno, size_t n)
    {
    void* ret;
#if DOSUMCHECK
    size_t nn = n;
#endif
#if SHOWCURRENTLYALLOCATED
    int tel;
    alloc_cnt++;
    if(n < 256)
        cnts[n]++;
    totcnt += n;
#endif
#if SHOWMAXALLOCATED
    globalloc++;
    if(maxgloballoc < globalloc)
        maxgloballoc = globalloc;
#endif
#if CHECKALLOCBOUNDS
    n += 3 * sizeof(LONG);
#endif
    checksum(__LINE__);
    n = (n - 1) / sizeof(struct memoryElement);
    if(n <
#if _5_6
       6
#elif _4
       4
#else
       3
#endif
       )
        {
        struct memblock* mb = global_allocations[n].memoryBlock;
        ret = mb->firstFreeElementBetweenAddresses;
        while(ret == 0)
            {
            mb = mb->previousOfSameLength;
            if(!mb)
                mb = newMemBlocks(n);
            if(!mb)
                break;
            ret = mb->firstFreeElementBetweenAddresses;
            }
        if(ret != 0)
            {
#if SHOWMAXALLOCATED
            --(mb->numberOfFreeElementsBetweenAddresses);
            if(mb->numberOfFreeElementsBetweenAddresses < mb->minimumNumberOfFreeElementsBetweenAddresses)
                mb->minimumNumberOfFreeElementsBetweenAddresses = mb->numberOfFreeElementsBetweenAddresses;
#endif
            mb->firstFreeElementBetweenAddresses = ((struct memoryElement*)mb->firstFreeElementBetweenAddresses)->next;
            /** /
            memset(ret,0,(n+1) * sizeof(struct pointerStruct));
            / **/
            ((LONG*)ret)[n] = 0;
            ((LONG*)ret)[0] = 0;
            setChecksum(lineno, nn);
#if CHECKALLOCBOUNDS
            ((LONG*)ret)[n - 1] = 0;
            ((LONG*)ret)[2] = 0;
            ((LONG*)ret)[1] = n;
            ((LONG*)ret)[n] = ('t' << 24) + ('e' << 16) + ('n' << 8) + ('d');
            ((LONG*)ret)[0] = ('s' << 24) + ('t' << 16) + ('a' << 8) + ('r');
            return (void*)(((LONG*)ret) + 2);
#else
            return ret;
#endif
            }
        }
    ret = malloc((n + 1) * sizeof(struct memoryElement));

    if(!ret)
        {
#if SHOWCURRENTLYALLOCATED
        errorprintf(
            "MEMORY FULL AFTER %lu ALLOCATIONS WITH MEAN LENGTH %lu\n",
            globalloc, totcnt / alloc_cnt);
        for(tel = 0; tel < 16; tel++)
            {
            int tel1;
            for(tel1 = 0; tel1 < 256; tel1 += 16)
                errorprintf("%lu ", (cnts[tel + tel1] * 1000UL + 500UL) / alloc_cnt);
            errorprintf("\n");
            }
        bezetting();
#endif
        errorprintf(
            "memory full (requested block of %d bytes could not be allocated)",
            (n << 2) + 4);

        exit(1);
        }

#if SHOWMAXALLOCATED
    ++malloced;
#endif
    ((LONG*)ret)[n] = 0;
    ((LONG*)ret)[0] = 0;
    setChecksum(lineno, n);
#if CHECKALLOCBOUNDS
    ((LONG*)ret)[n - 1] = 0;
    ((LONG*)ret)[2] = 0;
    ((LONG*)ret)[1] = n;
    ((LONG*)ret)[n] = ('t' << 24) + ('e' << 16) + ('n' << 8) + ('d');
    ((LONG*)ret)[0] = ('s' << 24) + ('t' << 16) + ('a' << 8) + ('r');
    return (void*)(((LONG*)ret) + 2);
#else
    return ret;
#endif
    }


#if CHECKALLOCBOUNDS
static int isFree(void* p)
    {
    LONG* q;
    int i;
    struct memoryElement* I;
    q = (LONG*)p - 2;
    I = (struct memoryElement*)q;
    for(i = 0; i < NumberOfMemBlocks; ++i)
        {
        struct memoryElement* me;
        struct memblock* mb = pMemBlocks[i];
        me = (struct memoryElement*)mb->firstFreeElementBetweenAddresses;
        while(me)
            {
            if(I == me)
                return 1;
            me = (struct memoryElement*)me->next;
            }
        }
    return 0;
    }

void result(psk Root);
static int rfree(psk p)
    {
    int r = 0;
    if(isFree(p))
        {
        printf(" [");
        result(p);
        printf("] ");
        r = 1;
        }
    if(is_op(p))
        {
        r |= rfree(p->LEFT);
        r |= rfree(p->RIGHT);
        }
    return r;
    }

static int areFree(char* t, psk p)
    {
    if(rfree(p))
        {
        POINT = 1;
        printf("%s:areFree(", t);
        result(p);
        POINT = 0;
        printf("\n");
        return 1;
        }
    return 0;
    }

static void checkMem(void* p)
    {
    LONG* q;
    q = (LONG*)p - 2;
    if(q[0] == ('s' << 24) + ('t' << 16) + ('a' << 8) + ('r')
       && q[q[1]] == ('t' << 24) + ('e' << 16) + ('n' << 8) + ('d')
       )
        {
        ;
        }
    else
        {
        char* s = (char*)q;
        printf("s:[");
        for(; s < (char*)(q + q[1] + 1); ++s)
            {
            if((((LONG)s) % 4) == 0)
                printf("|");
            if(' ' <= *s && *s <= 127)
                printf(" %c", *s);
            else
                printf("%.2x", (int)((unsigned char)*s));
            }
        printf("] %p\n", p);
        }
    assert(q[0] == ('s' << 24) + ('t' << 16) + ('a' << 8) + ('r'));
    assert(q[q[1]] == ('t' << 24) + ('e' << 16) + ('n' << 8) + ('d'));
    }


static void checkBounds(void* p)
    {
    struct memblock** q;
    LONG* lp = (LONG*)p;
    assert(p != 0);
    checkMem(p);
    lp = lp - 2;
    p = lp;
    assert(lp[0] == ('s' << 24) + ('t' << 16) + ('a' << 8) + ('r'));
    assert(lp[lp[1]] == ('t' << 24) + ('e' << 16) + ('n' << 8) + ('d'));
    for(q = pMemBlocks + NumberOfMemBlocks; --q >= pMemBlocks;)
        {
#ifndef NDEBUG
        size_t stepSize = (*q)->sizeOfElement / sizeof(struct memoryElement);
#endif // !NDEBUG
        if((*q)->lowestAddress <= (struct memoryElement*)p && (struct memoryElement*)p < (*q)->highestAddress)
            {
            assert(lp[stepSize - 1] == ('t' << 24) + ('e' << 16) + ('n' << 8) + ('d'));
            return;
            }
        }
    }

static void checkAllBounds(void)
    {
    struct memblock** q;
    for(q = pMemBlocks + NumberOfMemBlocks; --q >= pMemBlocks;)
        {
        size_t stepSize = (*q)->sizeOfElement / sizeof(struct memoryElement);

        struct memoryElement* p = (struct memoryElement*)(*q)->lowestAddress;
        struct memoryElement* e = (struct memoryElement*)(*q)->highestAddress;
        size_t L = (*q)->sizeOfElement - 1;
        struct memoryElement* x;
        for(x = p; x < e; x += stepSize)
            {
            struct memoryElement* a = ((struct memoryElement*)x)->next;
            if(a == 0 || (p <= a && a < e))
                ;
            else
                {
                if((((LONG*)x)[0] == ('s' << 24) + ('t' << 16) + ('a' << 8) + ('r'))
                   && (((LONG*)x)[stepSize - 1] == ('t' << 24) + ('e' << 16) + ('n' << 8) + ('d')))
                    ;
                else
                    {
                    char* s = (char*)x;
                    printf("s:[");
                    for(; s <= (char*)x + L; ++s)
                        if(' ' <= *s && *s <= 127)
                            printf("%c", *s);
                        else if(*s == 0)
                            printf("NIL");
                        else
                            printf("-%c", *s);
                    printf("] %p\n", x);
                    }
                assert(((LONG*)x)[0] == ('s' << 24) + ('t' << 16) + ('a' << 8) + ('r'));
                assert(((LONG*)x)[stepSize - 1] == ('t' << 24) + ('e' << 16) + ('n' << 8) + ('d'));
                }
            }
        }
    }
#endif

void bfree(void* p)
    {
    assert(p != (void*)&zeroNode);
    assert(p != (void*)&oneNode);
    assert(p != (void*)&minusOneNode);
    assert(p != (void*)&nilNode);
    assert(p != (void*)&nilNodeNotNeutral);
    assert(p != (void*)&zeroNodeNotNeutral);
    assert(p != (void*)&oneNodeNotNeutral);
    assert(p != (void*)&argNode);
    assert(p != (void*)&selfNode);
    assert(p != (void*)&SelfNode);
    assert(p != (void*)&twoNode);
    assert(p != (void*)&fourNode);
    assert(p != (void*)&sjtNode);
    struct memblock** q;
#if CHECKALLOCBOUNDS
    LONG* lp = (LONG*)p;
#endif
    assert(p != 0);
    checksum(__LINE__);
#if CHECKALLOCBOUNDS
    checkBounds(p);
    lp = lp - 2;
    p = lp;
#endif
#if SHOWMAXALLOCATED
    globalloc--;
#endif
    for(q = pMemBlocks + NumberOfMemBlocks; --q >= pMemBlocks;)
        {
        if((*q)->lowestAddress <= (struct memoryElement*)p && (struct memoryElement*)p < (*q)->highestAddress)
            {
#if SHOWMAXALLOCATED
            ++((*q)->numberOfFreeElementsBetweenAddresses);
#endif
            ((struct memoryElement*)p)->next = (*q)->firstFreeElementBetweenAddresses;
            (*q)->firstFreeElementBetweenAddresses = (struct memoryElement*)p;
            setChecksum(LineNo, globN);
            return;
            }
        }
    free(p);
#if SHOWMAXALLOCATED
    --malloced;
#endif
    setChecksum(LineNo, globN);
    }



#if SHOWMEMBLOCKS
static void showMemBlocks()
    {
    int totalbytes;
    int i;
    for(i = 0; i < NumberOfMemBlocks; ++i)
        {
#if defined __VMS
        printf("%p %d %p <= %p <= %p [%p] %lu\n"
#else
        printf("%p %d %p <= %p <= %p [%p] %zu\n"
#endif
               , pMemBlocks[i]
               , i
               , pMemBlocks[i]->lowestAddress
               , pMemBlocks[i]->firstFreeElementBetweenAddresses
               , pMemBlocks[i]->highestAddress
               , pMemBlocks[i]->previousOfSameLength
               , pMemBlocks[i]->sizeOfElement
        );
        }
    totalbytes = 0;
    for(i = 0; i < global_nallocations; ++i)
        {
#if defined __VMS
        printf("%d %d %lu\n"
#else
        printf("%d %d %zu\n"
#endif
               , i + 1
               , global_allocations[i].numberOfElements
               , global_allocations[i].numberOfElements * (i + 1) * sizeof(struct memoryElement)
        );
        totalbytes += global_allocations[i].numberOfElements * (i + 1) * sizeof(struct memoryElement);
        }
    printf("total bytes = %d\n", totalbytes);
    }
#endif

int addAllocation(size_t size, int number, int nallocations, struct allocation* allocations)
    {
    int i;
    for(i = 0; i < nallocations; ++i)
        {
        if(allocations[i].elementSize == size)
            {
            allocations[i].numberOfElements += number;
            return nallocations;
            }
        }
    allocations[nallocations].elementSize = size;
    allocations[nallocations].numberOfElements = number;
    return nallocations + 1;
    }

int memblocksort(const void* a, const void* b)
    {
    struct memblock* A = *(struct memblock**)a;
    struct memblock* B = *(struct memblock**)b;
    if(A->lowestAddress < B->lowestAddress)
        return -1;
    return 1;
    }

int init_memoryspace(void)
    {
    int i;
    global_allocations = (struct allocation*)malloc(sizeof(struct allocation)
                                                    *
#if _5_6
                                                    6
#elif _4
                                                    4
#else
                                                    3
#endif
    );
    global_nallocations = addAllocation(1 * sizeof(struct memoryElement), MEM1SIZE, 0, global_allocations);
    global_nallocations = addAllocation(2 * sizeof(struct memoryElement), MEM2SIZE, global_nallocations, global_allocations);
    global_nallocations = addAllocation(3 * sizeof(struct memoryElement), MEM3SIZE, global_nallocations, global_allocations);
#if _4
    global_nallocations = addAllocation(4 * sizeof(struct memoryElement), MEM4SIZE, global_nallocations, global_allocations);
#endif
#if _5_6
    global_nallocations = addAllocation(5 * sizeof(struct memoryElement), MEM5SIZE, global_nallocations, global_allocations);
    global_nallocations = addAllocation(6 * sizeof(struct memoryElement), MEM6SIZE, global_nallocations, global_allocations);
#endif
    NumberOfMemBlocks = global_nallocations;
    pMemBlocks = (struct memblock**)malloc(NumberOfMemBlocks * sizeof(struct memblock*));

    if(pMemBlocks)
        {
        for(i = 0; i < NumberOfMemBlocks; ++i)
            {
            pMemBlocks[i] = global_allocations[i].memoryBlock = initializeMemBlock(global_allocations[i].elementSize, global_allocations[i].numberOfElements);
            }
        qsort(pMemBlocks, NumberOfMemBlocks, sizeof(struct memblock*), memblocksort);
        /*
        for(i = 0;i < NumberOfMemBlocks;++i)
            {
            printf  ("%p %d %p %p %p %p %lu\n"
                    ,pMemBlocks[i]
                    ,i
                    ,pMemBlocks[i]->lowestAddress
                    ,pMemBlocks[i]->firstFreeElementBetweenAddresses
                    ,pMemBlocks[i]->highestAddress
                    ,pMemBlocks[i]->previousOfSameLength
                    ,pMemBlocks[i]->sizeOfElement
                    );
            }
        */
        return 1;
        }
    else
        return 0;
    }

void pskfree(psk p)
    {
    bfree(p);
    }

int all_refcount_bits_set(psk pnode)
    {
    return (shared(pnode) == ALL_REFCOUNT_BITS_SET) && !is_object(pnode);
    }

void dec_refcount(psk pnode)
    {
    assert(pnode->v.fl & ALL_REFCOUNT_BITS_SET);
    pnode->v.fl -= ONEREF;
#if WORD32
    if((pnode->v.fl & (OPERATOR | ALL_REFCOUNT_BITS_SET)) == EQUALS)
        {
        if(REFCOUNTNONZERO((objectnode*)pnode))
            {
            DECREFCOUNT(pnode);
            }
        }
#endif
    }

#if SHOWMAXALLOCATED
#if SHOWCURRENTLYALLOCATED
static void initcnts(void)
    {
    for(ULONG tel = 0; tel < sizeof(cnts) / sizeof(cnts[0]); ++tel)
        cnts[tel] = 0;
    }
#endif

void Bez(char draft[22])
    {
#if MAXSTACK
#if defined _WIN32 || defined __VMS
    sprintf(draft, "%lu.%lu.%d", (unsigned long)globalloc, (unsigned long)maxgloballoc, maxstack);
#else
    sprintf(draft, "%zu.%zu.%d", globalloc, maxgloballoc, maxstack);
#endif
#else
#if defined _WIN32 || defined __VMS
    sprintf(draft, "%lu.%lu", (unsigned long)globalloc, (unsigned long)maxgloballoc);
#else
    sprintf(draft, "%zu.%zu", globalloc, maxgloballoc);
#endif
#endif
    }
#endif
