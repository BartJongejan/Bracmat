#include "functions.h"
#include "unichartypes.h"
#include "nodedefs.h"
#include "copy.h"
#include "typedobjectnode.h"
#include "globals.h"
#include "variables.h"
#include "input.h"
#include "wipecopy.h"
#include "eval.h"
#include "filewrite.h"
#include "input.h"
#include "writeerr.h"
#include "lambda.h"
#include "result.h"
#include "memory.h"
#include "numbercheck.h"
#include "nodeutil.h"
#include "real.h"
#include "encoding.h"
#include "filestatus.h"
#include "rational.h"
#include "opt.h"
#include "simil.h"
#include "objectdef.h"
#include "objectnode.h"
#include "macro.h"
#include "branch.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

/*MOVETO FUNCTIONS*/
#define LONGCASE

#ifdef LONGCASE
#define SWITCH(v) switch(v)
#define FIRSTCASE(a) case a :
#define CASE(a) case a :
#define DEFAULT default :
#else
#define SWITCH(v) LONG lob;lob = v;
#define FIRSTCASE(a) if(lob == a)
#define CASE(a) else if(lob == a)
#define DEFAULT else
#endif

#ifdef DELAY_DUE_TO_INPUT
static clock_t delayDueToInput = 0;
#endif

#define functionFail(x) ((x)->v.fl ^= SUCCESS,(x))
#define functionOk(x) (x)

#if !defined NO_C_INTERFACE
static void* strToPointer(const char* str)
    {
    size_t res = 0;
    while(*str)
        res = 10 * res + (*str++ - '0');
    return (void*)res;
    }

static void pointerToStr(char* pc, void* p)
    {
    size_t P = (size_t)p;
    char* PC = pc;
    while(P)
        {
        *pc++ = (char)((P % 10) + '0');
        P /= 10;
        }
    *pc-- = '\0';
    while(PC < pc)
        {
        char sav = *PC;
        *PC = *pc;
        *pc = sav;
        ++PC;
        --pc;
        }
    }
#endif

#if O_S
static psk swi(psk Pnode, psk rlnode, psk rrightnode)
    {
    int i;
    union
        {
        unsigned int i[sizeof(os_regset) + 1];
        struct
            {
            int swicode;
            os_regset regs;
            } s;
        } u;
    char pc[121];
    for(i = 0; i < sizeof(os_regset) / sizeof(int); i++)
        u.s.regs.r[i] = 0;
    rrightnode = Pnode;
    i = 0;
    do
        {
        rrightnode = rrightnode->RIGHT;
        rlnode = is_op(rrightnode) ? rrightnode->LEFT : rrightnode;
        if(is_op(rlnode) || !INTEGER_NOT_NEG(rlnode))
            return functionFail(Pnode);
        u.i[i++] = (unsigned int)
            strtoul((char*)POBJ(rlnode), (char**)NULL, 10);
        } while(is_op(rrightnode) && i < 10);
#ifdef __TURBOC__
        intr(u.s.swicode, (struct REGPACK*)&u.s.regs);
        sprintf(pc, "0.%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
                u.i[1], u.i[2], u.i[3], u.i[4], u.i[5],
                u.i[6], u.i[7], u.i[8], u.i[9], u.i[10]);
#else
#if defined ARM
        i = (int)os_swix(u.s.swicode, &u.s.regs);
        sprintf(pc, "%u.%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
                i,
                u.i[1], u.i[2], u.i[3], u.i[4], u.i[5],
                u.i[6], u.i[7], u.i[8], u.i[9], u.i[10]);
#endif
#endif
        return build_up(Pnode, pc, NULL);
    }
#endif

static void stringreverse(char* a, size_t len)
    {
    char* b;
    b = a + len;
    while(a < --b)
        {
        char c = *a;
        *a = *b;
        *b = c;
        ++a;
        }
    }

static void print_clock(char* pjotter)
    {
    clock_t time = clock();
#ifdef DELAY_DUE_TO_INPUT
    time -= delayDueToInput;
#endif
    if(time == (clock_t)-1)
        sprintf(pjotter, "-1");
    else
#if defined __TURBOC__ && !defined __BORLANDC__
        sprintf(pjotter, "%0lu/%lu", (ULONG)time, (ULONG)(10.0 * CLOCKS_PER_SEC));/* CLOCKS_PER_SEC == 18.2 */
#else
        sprintf(pjotter, LONG0D "/" LONGD, (LONG)time, (LONG)CLOCKS_PER_SEC);
#endif
    }

static void combiflags(psk pnode)
    {
    int lflgs;
    if((lflgs = pnode->LEFT->v.fl & UNOPS) != 0)
        {
        pnode->RIGHT = isolated(pnode->RIGHT);
        if(NOTHINGF(lflgs))
            {
            pnode->RIGHT->v.fl |= lflgs & ~NOT;
            pnode->RIGHT->v.fl ^= NOT | SUCCESS;
            }
        else
            pnode->RIGHT->v.fl |= lflgs;
        }
    }


static function_return_type find_func(psk Pnode)
    {
    psk lnode = Pnode->LEFT;
    objectStuff Object = { 0,0,0 };
    int isNewRef = FALSE;
    addr[1] = NULL;
    addr[1] = find(lnode, &isNewRef, &Object);
    if(addr[1])
        {
        if(is_op(addr[1])
           && Op(addr[1]) == DOT
           )
            {
            psh(&argNode, Pnode->RIGHT, NULL);
            if(Object.self)
                {
                psh(&selfNode, Object.self, NULL);
                }
            Pnode = dopb(Pnode, addr[1]);
            if(isNewRef)
                wipe(addr[1]);
            if(Object.self)
                {
                /*
                psh(&selfNode,self,NULL); Must precede dopb(...).
                Example where this is relevant:

                {?} ((==.lst$its).)'
                (its=
                =.lst$its);
                {!} its

                */
                if(Object.object)
                    {
                    psh(&SelfNode, Object.object, NULL);
                    if(Op(Pnode) == DOT)
                        {
                        psh(Pnode->LEFT, &zeroNode, NULL);
                        Pnode = eval(Pnode);
                        pop(Pnode->LEFT);
                        Pnode = dopb(Pnode, Pnode->RIGHT);
                        }
                    deleteNode(&argNode);
                    deleteNode(&selfNode);
                    deleteNode(&SelfNode);
                    return functionOk(Pnode);
                    }
                else
                    {
                    if(Op(Pnode) == DOT)
                        {
                        psh(Pnode->LEFT, &zeroNode, NULL);
                        Pnode = eval(Pnode);
                        pop(Pnode->LEFT);
                        Pnode = dopb(Pnode, Pnode->RIGHT);
                        }
                    deleteNode(&argNode);
                    deleteNode(&selfNode);
                    return functionOk(Pnode);
                    }
                }
            else
                {
                if(Op(Pnode) == DOT)
                    {
                    psh(Pnode->LEFT, &zeroNode, NULL);
                    Pnode = eval(Pnode);
                    pop(Pnode->LEFT);
                    Pnode = dopb(Pnode, Pnode->RIGHT);
                    }
                deleteNode(&argNode);
                return functionOk(Pnode);
                }
            }
        else
            {
#if defined NO_EXIT_ON_NON_SEVERE_ERRORS
            return functionFail(Pnode);
#else
            errorprintf("(Syntax error) The following is not a function:\n\n  ");
            writeError(lnode);
            exit(116);
#endif
            }
        }
    else if(Object.theMethod)
        {
        if(Object.theMethod((struct typedObjectnode*)Object.object, &Pnode))
            {
            return functionOk(Pnode);
            }
        }
    else if((Op(Pnode->LEFT) == FUU)
            && (Pnode->LEFT->v.fl & FRACTION)
            && (Op(Pnode->LEFT->RIGHT) == DOT)
            && (!is_op(Pnode->LEFT->RIGHT->LEFT))
            )
        {
        psk rightnode;
        rightnode = lambda(Pnode->LEFT->RIGHT->RIGHT, Pnode->LEFT->RIGHT->LEFT, Pnode->RIGHT);
        if(rightnode)
            {
            wipe(Pnode);
            Pnode = rightnode;
            }
        else
            {
            psk npkn = subtreecopy(Pnode->LEFT->RIGHT->RIGHT);
            wipe(Pnode);
            Pnode = npkn;
            if(!is_op(Pnode) && !(Pnode->v.fl & INDIRECT))
                Pnode->v.fl |= READY;
            }
        return functionOk(Pnode);
        }
    else
        {
        DBGSRC(errorprintf("Function not found"); writeError(Pnode); \
               Printf("\n");)
        }
    return functionFail(Pnode);
    }

function_return_type functions(psk Pnode)
    {
    static char draft[22];
    psk lnode, rightnode, rrightnode, rlnode;
    union {
        int i;
        ULONG ul;
        } intVal;
    if(is_op(lnode = Pnode->LEFT))
        {
        if(!is_op(rlnode = lnode->LEFT) && !strcmp((char*)POBJ(rlnode), "math"))
            {
            if(is_op(Pnode->RIGHT))
                {
                if(REAL_COMP(Pnode->RIGHT->LEFT))
                    {
                    if(is_op(Pnode->RIGHT->RIGHT))
                        {
                        ;
                        }
                    else
                        {
                        if(REAL_COMP(Pnode->RIGHT->RIGHT))
                            {
                            psk restl = Cmath2(lnode->RIGHT, Pnode->RIGHT->LEFT, Pnode->RIGHT->RIGHT);
                            if(restl)
                                {
                                wipe(Pnode);
                                Pnode = restl;
                                return functionOk(Pnode);
                                }
                            else
                                return functionFail(Pnode);
                            }
                        }
                    }
                }
            else
                {
                if(REAL_COMP(Pnode->RIGHT))
                    {
                    psk restl = Cmath(lnode->RIGHT, Pnode->RIGHT);
                    if(restl)
                        {
                        wipe(Pnode);
                        Pnode = restl;
                        return functionOk(Pnode);
                        }
                    else
                        return functionFail(Pnode);
                    }
                }
            }

        return find_func(Pnode);
        }
    rightnode = Pnode->RIGHT;
    {
    SWITCH(PLOBJ(lnode))
        {
        FIRSTCASE(STR) /* str$(arg arg arg .. ..) */
            {
            beNice = FALSE;
            hum = 0;
            telling = 1;
            process = tstr;
            result(rightnode);
            rlnode = (psk)bmalloc(__LINE__, sizeof(ULONG) + telling);
            process = pstr;
            source = POBJ(rlnode);
            result(rightnode);
            rlnode->v.fl = (READY | SUCCESS) | (numbercheck(SPOBJ(rlnode)) & ~DEFINITELYNONUMBER);
            beNice = TRUE;
            hum = 1;
            process = myputc;
            wipe(Pnode);
            Pnode = rlnode;
            return functionOk(Pnode);
            }
#if O_S
        CASE(SWI) /* swi$(<interrupt number>.(input regs)) */
            {
            Pnode = swi(Pnode, rlnode, rrightnode);
            return functionOk(Pnode);
            }
#endif
#if _BRACMATEMBEDDED
#if defined PYTHONINTERFACE
        CASE(NI) /* Ni$"Statements to be executed by Python" */
            {
            if(Ni && !is_op(rightnode) && !HAS_VISIBLE_FLAGS_OR_MINUS(rightnode))
                {
                Ni((const char*)POBJ(rightnode));
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
        CASE(NII) /* Ni!$"Expression to be evaluated by Python" */
            {
            if(Nii && !is_op(rightnode) && !HAS_VISIBLE_FLAGS_OR_MINUS(rightnode))
                {
                const char* val;
                val = Nii((const char*)POBJ(rightnode));
                wipe(Pnode);
                Pnode = scopy((const char*)val);
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
#endif
#endif

#ifdef ERR
        /*#if !_BRACMATEMBEDDED*/
        CASE(ERR) /* err $ <file name to direct errorStream to> */
            {
            if(!is_op(rightnode))
                {
                if(redirectError((char*)POBJ(rightnode)))
                    return functionOk(Pnode);
                }
            return functionFail(Pnode);
            }
        /*#endif*/
#endif
#if !defined NO_C_INTERFACE
        CASE(ALC)  /* alc $ <number of bytes> */
            {
            void* p;
            if(is_op(rightnode)
               || !INTEGER_POS(rightnode)
               || (p = bmalloc(__LINE__, (int)strtoul((char*)POBJ(rightnode), (char**)NULL, 10)))
               == NULL)
                return functionFail(Pnode);
            pointerToStr(draft, p);
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(FRE) /* fre $ <pointer> */
            {
            void* p;
            if(is_op(rightnode) || !INTEGER_POS(rightnode))
                return functionFail(Pnode);
            p = strToPointer((char*)POBJ(rightnode));
            pskfree((psk)p);
            return functionOk(Pnode);
            }
        CASE(PEE) /* pee $ (<pointer>[,<number of bytes>]) (1,2,4,8)*/
            {
            void* p;
            intVal.i = 1;
            if(is_op(rightnode))
                {
                rlnode = rightnode->LEFT;
                rrightnode = rightnode->RIGHT;
                if(!is_op(rrightnode))
                    {
                    switch(rrightnode->u.obj)
                        {
                        case '2':
                            intVal.i = 2;
                            break;
                        case '4':
                            intVal.i = 4;
                            break;
                        }
                    }
                }
            else
                rlnode = rightnode;
            if(is_op(rlnode) || !INTEGER_POS(rlnode))
                return functionFail(Pnode);
            p = strToPointer((char*)POBJ(rlnode));
            p = (void*)((char*)p - (ptrdiff_t)((size_t)p % intVal.i));
            switch(intVal.i)
                {
                case 2:
                    sprintf(draft, "%hu", *(short unsigned int*)p);
                    break;
                case 4:
                    sprintf(draft, "%lu", (unsigned long)*(UINT32_T*)p);
                    break;
#ifndef __BORLANDC__
#if (!defined ARM || defined __SYMBIAN32__)
                case 8:
                    sprintf(draft, "%llu", *(unsigned long long*)p);
                    break;
#endif
#endif                    
                case 1:
                default:
                    sprintf(draft, "%u", (unsigned int)*(unsigned char*)p);
                    break;
                }
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(POK) /* pok $ (<pointer>,<number>[,<number of bytes>]) */
            {
            psk rrlnode;
            void* p;
            LONG val;
            intVal.i = 1;
            if(!is_op(rightnode))
                return functionFail(Pnode);
            rlnode = rightnode->LEFT;
            rrightnode = rightnode->RIGHT;
            if(is_op(rrightnode))
                {
                psk rrrightnode;
                rrrightnode = rrightnode->RIGHT;
                rrlnode = rrightnode->LEFT;
                if(!is_op(rrrightnode))
                    {
                    switch(rrrightnode->u.obj)
                        {
                        case '2':
                            intVal.i = 2;
                            break;
                        case '4':
                            intVal.i = 4;
                            break;
                        case '8':
                            intVal.i = 8;
                            break;
                        default:
                            ;
                        }
                    }
                }
            else
                rrlnode = rrightnode;
            if(is_op(rlnode) || !INTEGER_POS(rlnode)
               || is_op(rrlnode) || !INTEGER(rrlnode))
                return functionFail(Pnode);
            p = strToPointer((char*)POBJ(rlnode));
            p = (void*)((char*)p - (ptrdiff_t)((size_t)p % intVal.i));
            val = toLong(rrlnode);
            switch(intVal.i)
                {
                case 2:
                    *(unsigned short int*)p = (unsigned short int)val;
                    break;
                case 4:
                    *(UINT32_T*)p = (UINT32_T)val;
                    break;
#ifndef __BORLANDC__
                case 8:
                    *(ULONG*)p = (ULONG)val;
                    break;
#endif
                case 1:
                default:
                    *(unsigned char*)p = (unsigned char)val;
                    break;
                }
            return functionOk(Pnode);
            }
        CASE(FNC) /* fnc $ (<function pointer>.<struct pointer>) */
            {
            typedef Boolean(*fncTp)(void*);
            union
                {
                fncTp pfnc; /* Hoping this works. */
                void* vp;  /* Pointers to data and pointers to functions may
                               have different sizes. */
                } u;
            void* argStruct;
            if(sizeof(int(*)(void*)) != sizeof(void*) || !is_op(rightnode))
                return functionFail(Pnode);
            u.vp = strToPointer((char*)POBJ(rightnode->LEFT));
            if(!u.pfnc)
                return functionFail(Pnode);
            argStruct = strToPointer((char*)POBJ(rightnode->RIGHT));
            return u.pfnc(argStruct) ? functionOk(Pnode) : functionFail(Pnode);
            }
#endif
        CASE(X2D) /* x2d $ hexnumber */
            {
            char* endptr;
            ULONG val;
            if(is_op(rightnode)
               || HAS_VISIBLE_FLAGS_OR_MINUS(rightnode)
               )
                return functionFail(Pnode);
            errno = 0;
            val = STRTOUL((char*)POBJ(rightnode), &endptr, 16);
            if(errno == ERANGE || (endptr && *endptr))
                return functionFail(Pnode); /*not all characters scanned*/
            sprintf(draft, LONGU, val);
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(D2X) /* d2x $ decimalnumber */
            {
            char* endptr;
            ULONG val;
            if(is_op(rightnode) || !INTEGER_NOT_NEG(rightnode))
                return functionFail(Pnode);
#ifdef __BORLANDC__
            if(strlen((char*)POBJ(rightnode)) > 10
               || strlen((char*)POBJ(rightnode)) == 10
               && strcmp((char*)POBJ(rightnode), "4294967295") > 0
               )
                return functionFail(Pnode); /*not all characters scanned*/
#endif
            errno = 0;
            val = STRTOUL((char*)POBJ(rightnode), &endptr, 10);
            if(errno == ERANGE
               || (endptr && *endptr)
               )
                return functionFail(Pnode); /*not all characters scanned*/
            sprintf(draft, LONGX, val);
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(Chr) /* chr $ number */
            {
            if(is_op(rightnode) || !INTEGER_POS(rightnode))
                return functionFail(Pnode);
            intVal.ul = strtoul((char*)POBJ(rightnode), (char**)NULL, 10);
            if(intVal.ul > 255)
                return functionFail(Pnode);
            draft[0] = (char)intVal.ul;
            draft[1] = 0;
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(Chu) /* chu $ number */
            {
            ULONG val;
            if(is_op(rightnode) || !INTEGER_POS(rightnode))
                return functionFail(Pnode);
            val = STRTOUL((char*)POBJ(rightnode), (char**)NULL, 10);
            if(putCodePoint(val, (unsigned char*)draft) == NULL)
                return functionFail(Pnode);
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(ASC) /* asc $ character */
            {
            if(is_op(rightnode))
                return functionFail(Pnode);
            sprintf(draft, "%d", (int)rightnode->u.obj);
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(UGC)
            {
            if(is_op(rightnode))
                {
                Pnode->v.fl |= FENCE;
                return functionFail(Pnode);
                }
            else
                {
                const char* s = (const char*)POBJ(rightnode);
                const char* cat;
                intVal.i = getCodePoint(&s);
                if(intVal.i < 0 || *s)
                    {
                    if(intVal.i != -2)
                        {
                        Pnode->v.fl |= IMPLIEDFENCE;
                        }
                    return functionFail(Pnode);
                    }
                sprintf(draft, "%s", gencat(intVal.i));
                wipe(Pnode);
                Pnode = scopy((const char*)draft);
                return functionOk(Pnode);
                }
            }
        CASE(UTF)
            {
            /*
            @(abcædef:? (%@>"~" ?:?a & utf$!a) ?)
            @(str$(abc chu$200 def):? (%@>"~" ?:?a & utf$!a) ?)
            */
            if(is_op(rightnode))
                {
                Pnode->v.fl |= FENCE;
                return functionFail(Pnode);
                }
            else
                {
                const char* s = (const char*)POBJ(rightnode);
                intVal.i = getCodePoint(&s);
                if(intVal.i < 0 || *s)
                    {
                    if(intVal.i != -2)
                        {
                        Pnode->v.fl |= IMPLIEDFENCE;
                        }
                    return functionFail(Pnode);
                    }
                sprintf(draft, "%d", intVal.i);
                wipe(Pnode);
                Pnode = scopy((const char*)draft);
                return functionOk(Pnode);
                }
            }
#if !defined NO_LOW_LEVEL_FILE_HANDLING
#if !defined NO_FOPEN
        CASE(FIL) /* fil $ (<name>,[<offset>,[set|cur|end]]) */
            {
            return fil(&Pnode) ? functionOk(Pnode) : functionFail(Pnode);
            }
#endif
#endif
        CASE(FLG) /* flg $ <expr>  or flg$(=<expr>) */
            {
            if(is_object(rightnode) && !(rightnode->LEFT->v.fl & VISIBLE_FLAGS))
                rightnode = rightnode->RIGHT;
            intVal.ul = rightnode->v.fl;
            addr[3] = same_as_w(rightnode);
            addr[3] = isolated(addr[3]);
            addr[3]->v.fl = addr[3]->v.fl & ~VISIBLE_FLAGS;
            addr[2] = same_as_w(&nilNode);
            addr[2] = isolated(addr[2]);
            addr[2]->v.fl &= ~VISIBLE_FLAGS;
            addr[2]->v.fl |= VISIBLE_FLAGS & intVal.ul;
            if(addr[2]->v.fl & INDIRECT)
                {
                addr[2]->v.fl &= ~READY; /* {?} flg$(=!a):(=?X.?)&lst$X */
                addr[3]->v.fl |= READY;  /* {?} flg$(=!a):(=?.?Y)&!Y */
                }
            if(NOTHINGF(intVal.ul))
                {
                addr[2]->v.fl ^= SUCCESS;
                addr[3]->v.fl ^= SUCCESS;
                }
            sprintf(draft, "=\2.\3");
            Pnode = build_up(Pnode, draft, NULL);
            wipe(addr[2]);
            wipe(addr[3]);
            return functionOk(Pnode);
            }
        CASE(GLF) /* glf $ (=<flags>.<exp>) : (=?a)  a=<flags><exp> */
            {
            if(is_object(rightnode)
               && Op(rightnode->RIGHT) == DOT
               )
                {
                intVal.ul = rightnode->RIGHT->LEFT->v.fl & VISIBLE_FLAGS;
                if(intVal.ul && (rightnode->RIGHT->RIGHT->v.fl & intVal.ul))
                    return functionFail(Pnode);
                addr[3] = same_as_w(rightnode->RIGHT->RIGHT);
                addr[3] = isolated(addr[3]);
                addr[3]->v.fl |= intVal.ul;
                if(NOTHINGF(intVal.ul))
                    {
                    addr[3]->v.fl ^= SUCCESS;
                    }
                if(intVal.ul & INDIRECT)
                    {
                    addr[3]->v.fl &= ~READY;/* ^= --> &= ~ */
                    }
                sprintf(draft, "=\3");
                Pnode = build_up(Pnode, draft, NULL);
                wipe(addr[3]);
                return functionOk(Pnode);
                }
            return functionFail(Pnode);
            }
#if TELMAX
        CASE(BEZ) /* bez $  */
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
            Pnode = build_up(Pnode, draft, NULL);
#if TELLING
            bezetting();
#endif
            return functionOk(Pnode);
            }
#endif
        CASE(MMF) /* mem $ [EXT] */
            {
            mmf(&Pnode);
            return functionOk(Pnode);
            }
        CASE(MOD)
            {
            if(RATIONAL_COMP(rlnode = rightnode->LEFT) &&
               RATIONAL_COMP_NOT_NUL(rrightnode = rightnode->RIGHT))
                {
                psk pnode;
                pnode = qModulo(rlnode, rrightnode);
                wipe(Pnode);
                Pnode = pnode;
                return functionOk(Pnode);
                }
            return functionFail(Pnode);
            }
        CASE(REV)
            {
            if(!is_op(rightnode))
                {
                size_t len = strlen((char*)POBJ(rightnode));
                psk pnode;
                pnode = same_as_w(rightnode);
                if(len > 1)
                    {
                    pnode = isolated(pnode);
                    stringreverse((char*)POBJ(pnode), len);
                    }
                wipe(Pnode);
                Pnode = pnode;
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
        CASE(LOW)
            {
            psk pnode;
            if(!is_op(rightnode))
                pnode = changeCase(rightnode
#if CODEPAGE850
                                   , FALSE
#endif
                                   , TRUE);
            else if(!is_op(rlnode = rightnode->LEFT))
                pnode = changeCase(rlnode
#if CODEPAGE850
                                   , search_opt(rightnode->RIGHT, DOS)
#endif
                                   , TRUE);
            else
                return functionFail(Pnode);
            wipe(Pnode);
            Pnode = pnode;
            return functionOk(Pnode);
            }
        CASE(UPP)
            {
            psk pnode;
            if(!is_op(rightnode))
                pnode = changeCase(rightnode
#if CODEPAGE850
                                   , FALSE
#endif
                                   , FALSE);
            else if(!is_op(rlnode = rightnode->LEFT))
                pnode = changeCase(rlnode
#if CODEPAGE850
                                   , search_opt(rightnode->RIGHT, DOS)
#endif
                                   , FALSE);
            else
                return functionFail(Pnode);
            wipe(Pnode);
            Pnode = pnode;
            return functionOk(Pnode);
            }
        CASE(DIV)
            {
            if(is_op(rightnode)
               && RATIONAL_COMP(rlnode = rightnode->LEFT)
               && RATIONAL_COMP_NOT_NUL(rrightnode = rightnode->RIGHT)
               )
                {
                psk pnode;
                pnode = qIntegerDivision(rlnode, rrightnode);
                wipe(Pnode);
                Pnode = pnode;
                return functionOk(Pnode);
                }
            return functionFail(Pnode);
            }
        CASE(DEN)
            {
            if(RATIONAL_COMP(rightnode))
                {
                Pnode = qDenominator(Pnode);
                }
            return functionOk(Pnode);
            }
        CASE(LST)
            {
            return output(&Pnode, lst) ? functionOk(Pnode) : functionFail(Pnode);
            }
        CASE(MAP) /* map $ (<function>.<list>) */
            {
            if(is_op(rightnode))
                {/*XXX*/
                psk nnode;
                psk nPnode;
                ppsk ppnode = &nPnode;
                rrightnode = rightnode->RIGHT;
                while(is_op(rrightnode) && Op(rrightnode) == WHITE)
                    {
                    nnode = (psk)bmalloc(__LINE__, sizeof(knode));
                    nnode->v.fl = Pnode->v.fl;
                    nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                    nnode->LEFT = same_as_w(rightnode->LEFT);
                    nnode->RIGHT = same_as_w(rrightnode->LEFT);
                    nnode = functions(nnode);
                    if(!is_op(nnode) && IS_NIL(nnode))
                        {
                        wipe(nnode);
                        }
                    else
                        {
                        psk wnode = (psk)bmalloc(__LINE__, sizeof(knode));
                        wnode->v.fl = WHITE | SUCCESS;
                        *ppnode = wnode;
                        ppnode = &(wnode->RIGHT);
                        wnode->LEFT = nnode;
                        }
                    rrightnode = rrightnode->RIGHT;
                    }
                if(is_op(rrightnode) || !IS_NIL(rrightnode))
                    {
                    nnode = (psk)bmalloc(__LINE__, sizeof(knode));
                    nnode->v.fl = Pnode->v.fl;
                    nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                    nnode->LEFT = same_as_w(rightnode->LEFT);
                    nnode->RIGHT = same_as_w(rrightnode);
                    nnode = functions(nnode);
                    *ppnode = nnode;
                    }
                else
                    {
                    *ppnode = same_as_w(rrightnode);
                    }
                wipe(Pnode);
                Pnode = nPnode;
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
        CASE(MOP) /*     mop $ (<function>.<tree1>.(=<tree2>))
                mop regards tree1 as a right descending 'list' with a backbone
                operator that is the same as the heading operator of tree 2.
                For example,
          mop$((=.!arg).2*a+e\Lb+c^2.(=+))
                generates the list
                        2*a e\Lb c^2
                  */
            {
            if(is_op(rightnode))
                {/*XXX*/
                psk nnode;
                psk nPnode;
                ppsk ppnode = &nPnode;
                rrightnode = rightnode->RIGHT;
                if(Op(rightnode) == DOT)
                    {
                    if(Op(rrightnode) == DOT)
                        {
                        lnode = rrightnode->RIGHT;
                        rrightnode = rrightnode->LEFT;
                        if(Op(lnode) == EQUALS)
                            {
                            intVal.ul = Op(lnode->RIGHT);
                            if(intVal.ul)
                                {
                                while(is_op(rrightnode) && Op(rrightnode) == intVal.ul)
                                    {
                                    nnode = (psk)bmalloc(__LINE__, sizeof(knode));
                                    nnode->v.fl = Pnode->v.fl;
                                    nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                                    nnode->LEFT = same_as_w(rightnode->LEFT);
                                    nnode->RIGHT = same_as_w(rrightnode->LEFT);
                                    nnode = functions(nnode);
                                    if(!is_op(nnode) && IS_NIL(nnode))
                                        {
                                        wipe(nnode);
                                        }
                                    else
                                        {
                                        psk wnode = (psk)bmalloc(__LINE__, sizeof(knode));
                                        wnode->v.fl = WHITE | SUCCESS;
                                        *ppnode = wnode;
                                        ppnode = &(wnode->RIGHT);
                                        wnode->LEFT = nnode;
                                        }
                                    rrightnode = rrightnode->RIGHT;
                                    }
                                }
                            else
                                {
                                /* No operator specified */
                                return functionFail(Pnode);
                                }
                            }
                        else
                            {
                            /* The last argument must have heading = operator */
                            return functionFail(Pnode);
                            }
                        }
                    else
                        {
                        /* Expecting a dot */
                        return functionFail(Pnode);
                        }
                    }
                else
                    {
                    /* Expecting a dot */
                    return functionFail(Pnode);
                    }
                if(is_op(rrightnode) || !IS_NIL(rrightnode))
                    {
                    nnode = (psk)bmalloc(__LINE__, sizeof(knode));
                    nnode->v.fl = Pnode->v.fl;
                    nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                    nnode->LEFT = same_as_w(rightnode->LEFT);
                    nnode->RIGHT = same_as_w(rrightnode);
                    nnode = functions(nnode);
                    *ppnode = nnode;
                    }
                else
                    {
                    *ppnode = same_as_w(rrightnode);
                    }
                wipe(Pnode);
                Pnode = nPnode;
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
        CASE(Vap) /*    vap $ (<function>.<string>.<char>)
                 or vap $ (<function>.<string>)
                 Second form splits in characters. */
            {
            if(is_op(rightnode))
                {/*XXX*/
                rrightnode = rightnode->RIGHT;
                if(is_op(rrightnode))
                    { /* first form */
                    psk sepnode = rrightnode->RIGHT;
                    rrightnode = rrightnode->LEFT;
                    if(is_op(sepnode) || is_op(rrightnode))
                        {
                        return functionFail(Pnode);
                        }
                    else
                        {
                        const char* separator = &sepnode->u.sobj;
                        if(!*separator)
                            {
                            return functionFail(Pnode);
                            }
                        else
                            {
                            char* subject = &rrightnode->u.sobj;
                            psk nPnode;
                            ppsk ppnode = &nPnode;
                            char* oldsubject = subject;
                            while(subject)
                                {
                                psk nnode;
                                nnode = (psk)bmalloc(__LINE__, sizeof(knode));
                                nnode->v.fl = Pnode->v.fl;
                                nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                                nnode->LEFT = same_as_w(rightnode->LEFT);
                                subject = strstr(oldsubject, separator);
                                if(subject)
                                    {
                                    *subject = '\0';
                                    nnode->RIGHT = scopy(oldsubject);
                                    *subject = separator[0];
                                    oldsubject = subject + strlen(separator);
                                    }
                                else
                                    {
                                    nnode->RIGHT = scopy(oldsubject);
                                    }
                                nnode = functions(nnode);
                                if(subject)
                                    {
                                    psk wnode = (psk)bmalloc(__LINE__, sizeof(knode));
                                    wnode->v.fl = WHITE | SUCCESS;
                                    *ppnode = wnode;
                                    ppnode = &(wnode->RIGHT);
                                    wnode->LEFT = nnode;
                                    }
                                else
                                    {
                                    *ppnode = nnode;
                                    }
                                }
                            wipe(Pnode);
                            Pnode = nPnode;
                            return functionOk(Pnode);
                            }
                        }
                    }
                else
                    {
                    /* second form */
                    const char* subject = &rrightnode->u.sobj;
                    psk nPnode = 0;
                    ppsk ppnode = &nPnode;
                    const char* oldsubject = subject;
                    int k;
                    if(hasUTF8MultiByteCharacters(subject))
                        {
                        for(; (k = getCodePoint(&subject)) > 0; oldsubject = subject)
                            {
                            psk nnode;
                            nnode = (psk)bmalloc(__LINE__, sizeof(knode));
                            nnode->v.fl = Pnode->v.fl;
                            nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                            nnode->LEFT = same_as_w(rightnode->LEFT);
                            nnode->RIGHT = charcopy(oldsubject, subject);
                            nnode = functions(nnode);
                            if(*subject)
                                {
                                psk wnode = (psk)bmalloc(__LINE__, sizeof(knode));
                                wnode->v.fl = WHITE | SUCCESS;
                                *ppnode = wnode;
                                ppnode = &(wnode->RIGHT);
                                wnode->LEFT = nnode;
                                }
                            else
                                {
                                *ppnode = nnode;
                                }
                            }
                        }
                    else
                        {
                        for(; (k = *subject++) != 0; oldsubject = subject)
                            {
                            psk nnode;
                            nnode = (psk)bmalloc(__LINE__, sizeof(knode));
                            nnode->v.fl = Pnode->v.fl;
                            nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                            nnode->LEFT = same_as_w(rightnode->LEFT);
                            nnode->RIGHT = charcopy(oldsubject, subject);
                            nnode = functions(nnode);
                            if(*subject)
                                {
                                psk wnode = (psk)bmalloc(__LINE__, sizeof(knode));
                                wnode->v.fl = WHITE | SUCCESS;
                                *ppnode = wnode;
                                ppnode = &(wnode->RIGHT);
                                wnode->LEFT = nnode;
                                }
                            else
                                {
                                *ppnode = nnode;
                                }
                            }
                        }
                    wipe(Pnode);
                    Pnode = nPnode ? nPnode : same_as_w(&nilNode);
                    return functionOk(Pnode);
                    }
                }
            else
                return functionFail(Pnode);
            }
#if !defined NO_FILE_RENAME
        CASE(REN)
            {
            if(is_op(rightnode)
               && !is_op(rlnode = rightnode->LEFT)
               && !is_op(rrightnode = rightnode->RIGHT)
               )
                {
                intVal.i = rename((const char*)POBJ(rlnode), (const char*)POBJ(rrightnode));
                if(intVal.i)
                    {
#ifndef EACCES
                    sprintf(draft, "%d", intVal.i);
#else
                    switch(errno)
                        {
                        case EACCES:
                            /*
                            File or directory specified by newname already exists or
                            could not be created (invalid path); or oldname is a directory
                            and newname specifies a different path.
                            */
                            strcpy(draft, "EACCES");
                            break;
                        case ENOENT:
                            /*
                            File or path specified by oldname not found.
                            */
                            strcpy(draft, "ENOENT");
                            break;
                        case EINVAL:
                            /*
                            Name contains invalid characters.
                            */
                            strcpy(draft, "EINVAL");
                            break;
                        default:
                            sprintf(draft, "%d", errno);
                            break;
                        }
#endif
                    }
                else
                    strcpy(draft, "0");
                wipe(Pnode);
                Pnode = scopy((const char*)draft);
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
#endif
#if !defined NO_FILE_REMOVE
        CASE(RMV)
            {
            if(!is_op(rightnode))
                {
#if defined __SYMBIAN32__
                intVal.i = unlink((const char*)POBJ(rightnode));
#else
                intVal.i = remove((const char*)POBJ(rightnode));
#endif
                if(intVal.i)
                    {
#ifndef EACCES
                    sprintf(draft, "%d", intVal.i);
#else
                    switch(errno)
                        {
                        case EACCES:
                            /*
                            File or directory specified by newname already exists or
                            could not be created (invalid path); or oldname is a directory
                            and newname specifies a different path.
                            */
                            strcpy(draft, "EACCES");
                            break;
                        case ENOENT:
                            /*
                            File or path specified by oldname not found.
                            */
                            strcpy(draft, "ENOENT");
                            break;
                        default:
                            {
#ifdef __VMS
                            if(!strcmp("file currently locked by another user", (const char*)strerror(errno)))
                                {  /* OpenVMS */
                                strcpy(draft, "EACCES");
                                break;
                                }
                            else
#endif
                                {
                                wipe(Pnode);
                                Pnode = scopy((const char*)strerror(errno));
                                return functionOk(Pnode);
                                }
                            /* sprintf(draft,"%d",errno);
                             break;*/
                            }
                        }
#endif
                    }
                else
                    strcpy(draft, "0");
                wipe(Pnode);
                Pnode = scopy((const char*)draft);
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
#endif
        CASE(ARG) /* arg$ or arg$N  (N == 0,1,... and N < argc) */
            {
            static int argno = 0;
            if(is_op(rightnode))
                return functionFail(Pnode);
            if(PLOBJ(rightnode) != '\0')
                {
                LONG val;
                if(!INTEGER_NOT_NEG(rightnode))
                    return functionFail(Pnode);
                val = STRTOUL((char*)POBJ(rightnode), (char**)NULL, 10);
                if(val >= ARGC)
                    return functionFail(Pnode);
                wipe(Pnode);
                Pnode = scopy((const char*)ARGV[val]);
                return functionOk(Pnode);
                }
            if(argno < ARGC)
                {
                wipe(Pnode);
                Pnode = scopy((const char*)ARGV[argno++]);
                return functionOk(Pnode);
                }
            else
                {
                return functionFail(Pnode);
                }
            }
        CASE(GET) /* get$file */
            {
            Boolean GoOn;
            int err = 0;
            if(is_op(rightnode))
                {
                if(is_op(rlnode = rightnode->LEFT))
                    return functionFail(Pnode);
                rrightnode = rightnode->RIGHT;
                intVal.i = (search_opt(rrightnode, ECH) << SHIFT_ECH)
                    + (search_opt(rrightnode, MEM) << SHIFT_MEM)
                    + (search_opt(rrightnode, VAP) << SHIFT_VAP)
                    + (search_opt(rrightnode, STG) << SHIFT_STR)
                    + (search_opt(rrightnode, ML) << SHIFT_ML)
                    + (search_opt(rrightnode, TRM) << SHIFT_TRM)
                    + (search_opt(rrightnode, HT) << SHIFT_HT)
                    + (search_opt(rrightnode, X) << SHIFT_X)
                    + (search_opt(rrightnode, JSN) << SHIFT_JSN)
                    + (search_opt(rrightnode, TXT) << SHIFT_TXT)
                    + (search_opt(rrightnode, BIN) << SHIFT_BIN);
                }
            else
                {
                intVal.i = 0;
                rlnode = rightnode;
                }
            if(intVal.i & OPT_MEM)
                {
                addr[1] = same_as_w(rlnode);
                source = POBJ(addr[1]);
                for(;;)
                    {
                    Pnode = input(NULL, Pnode, intVal.i, &err, &GoOn);
                    if(!GoOn || err)
                        break;
                    Pnode = eval(Pnode);
                    }
                wipe(addr[1]);
                }
            else
                {
                if(rlnode->u.obj && strcmp((char*)POBJ(rlnode), "stdin"))
                    {
#if defined NO_FOPEN
                    return functionFail(Pnode);
#else
                    psk pnode = fileget(rlnode, intVal.i, Pnode, &err, &GoOn);
                    if(pnode)
                        Pnode = pnode;
                    else
                        return functionFail(Pnode);
#endif
                    }
                else
                    {
                    intVal.i |= OPT_ECH;
#ifdef DELAY_DUE_TO_INPUT
                    for(;;)
                        {
                        clock_t time0;
                        time0 = clock();
                        Pnode = input(stdin, Pnode, intVal.i, &err, &GoOn);
                        delayDueToInput += clock() - time0;
                        if(!GoOn || err)
                            break;
                        Pnode = eval(Pnode);
                        }
#else
                    for(;;)
                        {
                        Pnode = input(stdin, Pnode, intVal.i, &err, &GoOn);
                        if(!GoOn || err)
                            break;
                        Pnode = eval(Pnode);
                        }
#endif
                    }
                }
            return err ? functionFail(Pnode) : functionOk(Pnode);
            }
        CASE(PUT) /* put$(file,mode,node) of put$node */
            {
            return output(&Pnode, result) ? functionOk(Pnode) : functionFail(Pnode);
            }
#if !defined __SYMBIAN32__
#if !defined NO_SYSTEM_CALL
        CASE(SYS)
            {
            if(is_op(rightnode) || (PLOBJ(rightnode) == '\0'))
                return functionFail(Pnode);
            else
                {
                intVal.i = system((const char*)POBJ(rightnode));
                if(intVal.i)
                    {
#ifndef E2BIG
                    sprintf(draft, "%d", intVal.i);
#else
                    switch(errno)
                        {
                        case E2BIG:
                            /*
                            Argument list (which is system-dependent) is too big.
                            */
                            strcpy(draft, "E2BIG");
                            break;
                        case ENOENT:
                            /*
                            Command interpreter cannot be found.
                            */
                            strcpy(draft, "ENOENT");
                            break;
                        case ENOEXEC:
                            /*
                            Command-interpreter file has invalid format and is not executable.
                            */
                            strcpy(draft, "ENOEXEC");
                            break;
                        case ENOMEM:
                            /*
                            Not enough memory is available to execute command; or available
                            memory has been corrupted; or invalid block exists, indicating
                            that process making call was not allocated properly.
                            */
                            strcpy(draft, "ENOMEM");
                            break;
                        default:
                            sprintf(draft, "%d", errno);
                            break;
                        }
#endif
                    }
                else
                    strcpy(draft, "0");
                wipe(Pnode);
                Pnode = scopy((const char*)draft);
                return functionOk(Pnode);
                }
            }
#endif
#endif
        CASE(TBL) /* tbl$(varname,length) */
            {
            if(is_op(rightnode))
                return psh(rightnode->LEFT, &zeroNode, rightnode->RIGHT) ? functionOk(Pnode) : functionFail(Pnode);
            else
                return functionFail(Pnode);
            }
#if 0
        /*
        The same effect is obtained by <expr>:?!(=)
        */
        CASE(PRV) /* "?"$<expr> */
            {
            if((rightnode->v.fl & SUCCESS)
               && (is_op(rightnode) || rightnode->u.obj || HAS_UNOPS(rightnode)))
                insert(&nilNode, rightnode);
            Pnode = rightbranch(Pnode);
            return functionOk(Pnode);
            }
#endif
        CASE(CLK) /* clk' */
            {
            print_clock(draft);
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }

        CASE(SIM) /* sim$(<atom>,<atom>) , fuzzy compare (percentage) */
            {
            if(is_op(rightnode)
               && !is_op(rlnode = rightnode->LEFT)
               && !is_op(rrightnode = rightnode->RIGHT))
                {
                Sim(draft, (char*)POBJ(rlnode), (char*)POBJ(rrightnode));
                wipe(Pnode);
                Pnode = scopy((const char*)draft);
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
#if DEBUGBRACMAT
        CASE(DBG) /* dbg$<expr> */
            {
            ++debug;
            if(Op(Pnode) != FUU)
                {
                errorprintf("Use dbg'(expression), not dbg$(expression)!\n");
                writeError(Pnode);
                }
            Pnode = rightbranch(Pnode);
            Pnode = eval(Pnode);
            --debug;
            return functionOk(Pnode);
            }
#endif
        CASE(WHL)
            {
            while(isSUCCESSorFENCE(rightnode = eval(same_as_w(Pnode->RIGHT))))
                {
                wipe(rightnode);
                }
            wipe(rightnode);
            return functionOk(Pnode);
            }
        CASE(New) /* new$<object>*/
            {
            if(Op(rightnode) == COMMA)
                {
                addr[2] = getObjectDef(rightnode->LEFT);
                if(!addr[2])
                    return functionFail(Pnode);
                addr[3] = rightnode->RIGHT;
                if(ISBUILTIN((objectnode*)addr[2]))
                    Pnode = build_up(Pnode, "(((\2.New)'\3)|)&\2", NULL);
                /* We might be able to call 'new' if 'New' had attached the argument
                    (containing the definition of a 'new' method) to the rhs of the '='.
                   This cannot be done in a general way without introducing new syntax rules for the new$ function.
                */
                else
                    Pnode = build_up(Pnode, "(((\2.new)'\3)|)&\2", NULL);
                }
            else
                {
                addr[2] = getObjectDef(rightnode);
                if(!addr[2])
                    return functionFail(Pnode);
                if(ISBUILTIN((objectnode*)addr[2]))
                    Pnode = build_up(Pnode, "(((\2.New)')|)&\2", NULL);
                /* There cannot be a user-defined 'new' method on a built-in object if there is no way to supply it*/
                /* 'die' CAN be user-supplied. The built-in function is 'Die' */
                else
                    Pnode = build_up(Pnode, "(((\2.new)')|)&\2", NULL);
                }
            SETCREATEDWITHNEW((objectnode*)addr[2]);
            wipe(addr[2]);
            return functionOk(Pnode);
            }
        CASE(0) /* $<expr>  '<expr> */
            {
            if(Op(Pnode) == FUU)
                {
                if(!HAS_UNOPS(Pnode->LEFT))
                    {
                    intVal.ul = Pnode->v.fl & UNOPS;
                    if(intVal.ul == FRACTION
                       && is_op(Pnode->RIGHT)
                       && Op(Pnode->RIGHT) == DOT
                       && !is_op(Pnode->RIGHT->LEFT)
                       )
                        { /* /('(a.a+2))*/
                        return functionOk(Pnode);
                        }
                    else
                        {
                        rightnode = evalmacro(Pnode->RIGHT);
                        rrightnode = (psk)bmalloc(__LINE__, sizeof(objectnode));
#if WORD32
                        ((typedObjectnode*)rrightnode)->u.Int = 0;
#else
                        ((typedObjectnode*)rrightnode)->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
#endif
                        rrightnode->v.fl = EQUALS | SUCCESS;
                        rrightnode->LEFT = same_as_w(&nilNode);
                        if(rightnode)
                            {
                            rrightnode->RIGHT = rightnode;
                            }
                        else
                            {
                            rrightnode->RIGHT = same_as_w(Pnode->RIGHT);
                            }
                        wipe(Pnode);
                        Pnode = rrightnode;
                        Pnode->v.fl |= intVal.ul; /* (a=b)&!('$a)*/
                        }
                    }
                else
                    {
                    combiflags(Pnode);
                    Pnode = rightbranch(Pnode);
                    }
                return functionOk(Pnode);
                }
            else
                {
                return functionFail(Pnode);
                }
            }
        DEFAULT
            {
            if(INTEGER(lnode))
                {
                if(is_op(rightnode))
                    return functionFail(Pnode);
                else
                    {
                    if(setIndex(rightnode,lnode))
                        {
                        Pnode = rightbranch(Pnode);
                        return functionOk(Pnode);
                        }
                    }
                }
            if(!(rightnode->v.fl & SUCCESS))
                return functionFail(Pnode);
            addr[1] = NULL;
            return find_func(Pnode);
            }
        }
    }
    /*return functionOk(Pnode); 20 Dec 1995, unreachable code in Borland C */
    }
