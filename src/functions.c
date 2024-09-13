#include "functions.h"
#include "unichartypes.h"
#include "nodedefs.h"
#include "copy.h"
#include "typedobjectnode.h"
#include "globals.h"
#include "branch.h"
#include "variables.h"
//#include "input.h"
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
#include "encoding.h"
#include "filestatus.h"
#include "rational.h"
#include "opt.h"
#include "simil.h"
#include "objectdef.h"
#include "objectnode.h"
#include "macro.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <time.h>

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
static psk swi(psk Pnode, psk rlnode, psk rrnode)
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
    rrnode = Pnode;
    i = 0;
    do
        {
        rrnode = rrnode->RIGHT;
        rlnode = is_op(rrnode) ? rrnode->LEFT : rrnode;
        if(is_op(rlnode) || !INTEGER_NOT_NEG(rlnode))
            return functionFail(Pnode);
        u.i[i++] = (unsigned int)
            strtoul((char*)POBJ(rlnode), (char**)NULL, 10);
        }
    while(is_op(rrnode) && i < 10);
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

function_return_type execFnc(psk Pnode)
    {
    psk lnode;
    objectStuff Object = { 0,0,0 };

    lnode = Pnode->LEFT;
    if(is_op(lnode))
        {
        switch(Op(lnode))
            {
                case EQUALS: /* Anonymous function: (=.out$!arg)$HELLO -> lnode == (=.out$!arg) */
                    lnode->RIGHT = Head(lnode->RIGHT);
                    lnode = same_as_w(lnode->RIGHT);
                    if(lnode) /* lnode is null if either the function wasn't found or it is a built-in member function of an object. */
                        {
                        if(Op(lnode) == DOT) /* The dot separating local variables from the function body. */
                            {
                            psh(&argNode, Pnode->RIGHT, NULL);
                            Pnode = dopb(Pnode, lnode);
                            wipe(lnode);
                            if(Op(Pnode) == DOT)
                                {
                                psh(Pnode->LEFT, &zeroNode, NULL);
                                Pnode = eval(Pnode);
                                /**** Evaluate anonymous function.

                                         (=.!arg)$XYZ
                                ****/
                                pop(Pnode->LEFT);
                                Pnode = dopb(Pnode, Pnode->RIGHT);
                                }
                            deleteNode(&argNode);
                            return functionOk(Pnode);
                            }
                        else
                            {
#if defined NO_EXIT_ON_NON_SEVERE_ERRORS
                            return functionFail(Pnode);
#else
                            errorprintf("(Syntax error) The following is not a function:\n\n  ");
                            writeError(Pnode->LEFT);
                            exit(116);
#endif
                            }
                        }
                    break;
                case DOT: /* Method. Dot separating object name (or anonymous definition) from method name */
                    lnode = findMethod(lnode, &Object);
                    if(lnode)
                        {
                        if(Op(lnode) == DOT) /* The dot separating local variables from the function body. */
                            {
                            psh(&argNode, Pnode->RIGHT, NULL);

                            if(Object.self)
                                {
                                psh(&selfNode, Object.self, NULL); /* its */
                                Pnode = dopb(Pnode, lnode);
                                wipe(lnode);
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
                                    psh(&SelfNode, Object.object, NULL); /* Its */
                                    if(Op(Pnode) == DOT)
                                        {
                                        psh(Pnode->LEFT, &zeroNode, NULL);
                                        Pnode = eval(Pnode);
                                        /**** Evaluate member function of built-in
                                              object from within an enveloping
                                              object. -----------------
                                                                       |
                                        ( new$hash:?myhash             |
                                        &   (                          |
                                            = ( myInsert               |
                                              = . (Its..insert)$!arg <-
                                              )
                                            )
                                          : (=?(myhash.))
                                        & (myhash..myInsert)$(X.12)
                                        )
                                        ****/
                                        pop(Pnode->LEFT);
                                        Pnode = dopb(Pnode, Pnode->RIGHT);
                                        }
                                    deleteNode(&SelfNode);
                                    }
                                else
                                    {
                                    if(Op(Pnode) == DOT)
                                        {
                                        psh(Pnode->LEFT, &zeroNode, NULL);
                                        Pnode = eval(Pnode);
                                        /**** Evaluate member function from
                                              within an other member function
                                            ( ( Object                    |
                                              =   (do=.out$!arg)          |
                                                  ( A                     |
                                                  =                       |
                                                    . (its.do)$!arg <-----
                                                  )
                                              )
                                            & (Object.A)$XYZ
                                            )
                                        ****/
                                        pop(Pnode->LEFT);
                                        Pnode = dopb(Pnode, Pnode->RIGHT);
                                        }
                                    }
                                deleteNode(&selfNode);
                                deleteNode(&argNode);
                                return functionOk(Pnode);
                                }
                            else
                                { /* Unreachable? */
                                deleteNode(&argNode);
                                }
                            }
                        else if(Object.theMethod)
                            {
                            if(Object.theMethod((struct typedObjectnode*)Object.object, &Pnode))
                                {
                                /**** Evaluate a built-in method of an anonymous object.

                                        (new$hash.insert)$(a.2)
                                ****/
                                wipe(lnode); /* This is the built-in object, which got increased refcount to evade untimely wiping. */
                                return functionOk(Pnode);
                                }
                            else
                                { /* method failed. E.g. (new$hash.insert)$(a) */
                                wipe(lnode);
                                }
                            }
                        else
                            {
#if defined NO_EXIT_ON_NON_SEVERE_ERRORS
                            return functionFail(Pnode);
#else
                            errorprintf("(Syntax error) The following is not a function:\n\n  ");
                            writeError(Pnode->LEFT);
                            exit(116);
#endif
                            }
                        }
                    else
                        {
                        if(Object.theMethod)
                            {
                            if(Object.theMethod((struct typedObjectnode*)Object.object, &Pnode))
                                {
                                /**** Evaluate a built-in method of a named object.
                                                                |
                                          new$hash:?H           |
                                        & (H..insert)$(XYZ.2) <-
                                ****/
                                return functionOk(Pnode);
                                }
                            }
                        }
                    break;
                default:
                    {
                    /* /('(x.$x^2)) when evaluating /('(x.$x^2))$3 */
                    if((Op(lnode) == FUU)
                       && (lnode->v.fl & FRACTION)
                       && (Op(lnode->RIGHT) == DOT)
                       && (!is_op(lnode->RIGHT->LEFT))
                       )
                        {
                        lnode = lambda(lnode->RIGHT->RIGHT, lnode->RIGHT->LEFT, Pnode->RIGHT);
                        /**** Evaluate a lambda expression

                                /('(x.$x^2))$3
                        ****/
                        if(lnode)
                            {
                            /*
                                  /(
                                   ' ( g
                                     .   /('(x.$g'($x'$x)))
                                       $ /('(x.$g'($x'$x)))
                                     )
                                   )
                                $ /(
                                   ' ( r
                                     . /(
                                        ' ( n
                                          .   $n:~>0&1
                                            | $n*($r)$($n+-1)
                                          )
                                        )
                                     )
                                   )
                            */
                            wipe(Pnode);
                            Pnode = lnode;
                            }
                        else
                            {
                            /*
                                /('(x./('(x.$x ()$x))$aap))$noot
                            */
                            lnode = subtreecopy(Pnode->LEFT->RIGHT->RIGHT);
                            wipe(Pnode);
                            Pnode = lnode;
                            if(!is_op(Pnode) && !(Pnode->v.fl & INDIRECT))
                                Pnode->v.fl |= READY;  /*  /('(x.u))$7  */
                            }
                        return functionOk(Pnode);
                        }
                    return functionFail(Pnode);/* completely wrong expression, e.g. (+(x.$x^2))$3 */
                    }
            }
        }
    else
        {
#if 1
        lnode = getValueByVariableName(lnode);
#else
        /* UNSAFE */
        /* The next two variables are used to cache the address of the most recently called user function.
        These values must be reset each time stringEval() is called.
        (When Bracmat is embedded in a Java program as a JNI, data addresses are not stable, it seems.) */
        static psk oldlnode = 0;
        static psk lastEvaluatedFunction = 0;
        /* Unsafe, esp. in JNI */
        if(oldlnode == lnode)
            lnode = lastEvaluatedFunction;  /* Speeding up! Esp. in map, vap, mop. */
        else
            {
            oldlnode = lnode;
            lnode = getValueByVariableName(lnode);
            lastEvaluatedFunction = lnode;
            }
#endif

        if(lnode) /* lnode is null if either the function wasn't found or it is a built-in member function of an object. */
            {
            if(Op(lnode) == DOT) /* The dot separating local variables from the function body. */
                {
                psh(&argNode, Pnode->RIGHT, NULL);
                Pnode = dopb(Pnode, lnode);
                if(Op(Pnode) == DOT)
                    {
                    psh(Pnode->LEFT, &zeroNode, NULL);
                    Pnode = eval(Pnode);
                    /**** Evaluate a named function
                                             |
                          (  f=.2*!arg       |
                          & f$6        <-----
                          )
                    ****/
                    pop(Pnode->LEFT);
                    Pnode = dopb(Pnode, Pnode->RIGHT);
                    }
                deleteNode(&argNode);
                return functionOk(Pnode);
                }
            else
                {
#if defined NO_EXIT_ON_NON_SEVERE_ERRORS
                return functionFail(Pnode);
#else
                errorprintf("(Syntax error) The following is not a function:\n\n  ");
                writeError(Pnode->LEFT);
                exit(116);
#endif
                }
            }
        }
    DBGSRC(errorprintf("Function not found"); writeError(Pnode); Printf("\n");)
        return functionFail(Pnode);
    }

static psk applyFncToElem_w(psk fnc, psk elm, ULONG fl)
    {
    if(!fnc)
        return eval(same_as_w(elm)); /* combine or sort elm */
    else if(IS_NIL(fnc))
        return same_as_w(elm); /* Do absolutely nothing */
    else
        {
        psk nnode;
        psk rlnode;
        nnode = (psk)bmalloc(sizeof(knode));
        nnode->v.fl = fl;
        nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
        nnode->LEFT = same_as_w(fnc);
        nnode->RIGHT = same_as_w(elm);
        rlnode = setIndex(nnode);
        if(rlnode)
            {
            nnode = rlnode;
            }
        else
            {
            if(not_built_in(nnode->LEFT)) /* Do not use ternary operator! That eats stack! */
                {
                nnode = execFnc(nnode);
                }
            else
                {
                rlnode = functions(nnode);
                if(rlnode)
                    nnode = rlnode;
                else
                    nnode = execFnc(nnode);
                }
            }
        return nnode;
        }
    }

static psk applyFncToString(psk fnc, psk elm, ULONG fl)
    {
    if(IS_NIL(fnc))
        {
        return elm;
        }
    else
        {
        psk nnode;
        nnode = (psk)bmalloc(sizeof(knode));
        nnode->v.fl = fl;
        nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
        nnode->LEFT = same_as_w(fnc);
        nnode->RIGHT = elm;// charcopy(oldsubject, subject);
        psk rlnode = setIndex(nnode);
        if(rlnode)
            nnode = rlnode;
        else
            {
            if(not_built_in(nnode->LEFT)) /* Do not use ternary operator! That eats stack! */
                nnode = execFnc(nnode);
            else
                {
                rlnode = functions(nnode);
                if(rlnode)
                    nnode = rlnode;
                else
                    nnode = execFnc(nnode);
                }
            }
        return nnode;
        }
    }

static void SortMOP(psk fun, psk datanode, ULONG inop, psk outopnode, ULONG fl, ppsk pPnode)
    {
    ULONG outop;
    if(outopnode)
        outop = Op(outopnode->RIGHT) | SUCCESS; // specific out-operator
    else
        outop = WHITE | SUCCESS; // default out-operator
    fun = same_as_w(fun);
    datanode = same_as_w(datanode);
    wipe(*pPnode);
    *pPnode = datanode;
    ULONG outopsh;
    outopsh = outop >> OPSH;
    psk hasnil = knil[outopsh];
    LONG theNil;
    if(hasnil)
        theNil = hasnil->u.lobj;
    else
        theNil = 0;
    if((outop & OPERATOR) == PLUS || (outop & OPERATOR) == TIMES)
        {
        psk resultnode = datanode;
        psk fnc = fun;
        int repeat = 0;   // Merge sort: make every second op an inop instead of '+' or '*', then rearrange, then repeat
        int inops = 0;
        while(Op(datanode) == inop)
            {
            ++inops;
            datanode = datanode->RIGHT;
            }

        do
            {
            psk evaluatedNode;
            ppsk presultnode = &resultnode;
            datanode = resultnode;
            repeat = 0;
            while(repeat < inops)
                {
                assert(Op(datanode) == inop);
                ++repeat;
                evaluatedNode = applyFncToElem_w(fnc, datanode->LEFT, fl);
                psk nxt = datanode->RIGHT;
                if(!is_op(evaluatedNode) && hasnil && evaluatedNode->u.lobj == theNil)
                    {
                    wipe(evaluatedNode);
                    }
                else
                    {
                    psk newnode;
                    if(fnc) /* Only true in first iteration! */
                        newnode = (psk)bmalloc(sizeof(knode));
                    else
                        { /* Reuse cell that was allocated in first iteration. */
                        wipe(datanode->LEFT); /* Do not wipe the rhs. We do need that one. (Except if it is the last one.) */
                        newnode = datanode;
                        }

                    newnode->LEFT = evaluatedNode;
                    newnode->RIGHT = nxt;
                    if(repeat % 2) // ODD
                        newnode->v.fl = outop;
                    else
                        { // EVEN
                        newnode->v.fl = inop;
                        }
                    *presultnode = newnode;
                    presultnode = &(newnode->RIGHT);
                    }
                datanode = nxt;
                }

            *presultnode = applyFncToElem_w(fnc, datanode, fl); /* If the evaluation of the last (or only) datanode results
                                     in a neutral element, we still have to put that at the end of the list. What else?*/

            if(fnc) /* if fnc == 0 then all previously allocated operators can be reused. So we do not wipe them all. */
                {
                wipe(*pPnode);
                }
            else
                {
                wipe(datanode); /* Only the last element in the data list must still be deleted. */
                }

            fnc = 0;

            ppsk A;
            psk B;

            A = &resultnode;
            repeat = 0;
            inops /= 2;
            for(; ++repeat <= inops;)
                {
                assert(Op(*A) == (outop & OPERATOR));
                B = (*A)->RIGHT;
                assert(is_op(B));
                if(Op(B) == inop)
                    {
                    (*A)->RIGHT = B->LEFT;
                    B->LEFT = *A;
                    *A = B;
                    A = &(B->RIGHT);
                    }
                else
                    A = &((*A)->RIGHT);
                }
            *pPnode = resultnode;
            }
        while(inops);
        }
    else
        {
        psk resultnode;
        ppsk presultnode = &resultnode;
        while(Op(datanode) == inop)
            {
            psk evaluatedNode = applyFncToElem_w(fun, datanode->LEFT, fl);
            if(!is_op(evaluatedNode) && hasnil && evaluatedNode->u.lobj == theNil)
                {
                wipe(evaluatedNode);
                }
            else
                {
                psk newnode = (psk)bmalloc(sizeof(knode));
                newnode->LEFT = evaluatedNode;
                newnode->v.fl = outop;
                *presultnode = newnode;
                presultnode = &(newnode->RIGHT);
                }
            datanode = datanode->RIGHT;
            }
        if(!is_op(datanode) && knil[outopsh] && datanode->u.lobj == theNil)
            {
            *presultnode = same_as_w(datanode);
            }
        else
            {
            *presultnode = applyFncToElem_w(fun, datanode, fl);
            }
        datanode = resultnode;
        wipe(*pPnode);
        *pPnode = resultnode;
        }
    wipe(fun);
    }

function_return_type functions(psk Pnode)
    {
    static char draft[112];
    psk lnode, rnode, rlnode, rrnode;
    union
        {
        int i;
        ULONG ul;
        } intVal;
    lnode = Pnode->LEFT;
    rnode = Pnode->RIGHT;
    {
    SWITCH(PLOBJ(lnode))
        {
        FIRSTCASE(STR) /* str$(arg arg arg .. ..) */
            {
            beNice = FALSE;
            hum = 0;
            telling = 1;
            process = tstr;
            result(rnode);
            rlnode = (psk)bmalloc(sizeof(ULONG) + telling);
            process = pstr;
            source = POBJ(rlnode);
            result(rnode);
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
            Pnode = swi(Pnode, rlnode, rrnode);
            return functionOk(Pnode);
            }
#endif
#if _BRACMATEMBEDDED
#if defined PYTHONINTERFACE
        CASE(NI) /* Ni$"Statements to be executed by Python" */
            {
            if(Ni && !is_op(rnode) && !HAS_VISIBLE_FLAGS_OR_MINUS(rnode))
                {
                Ni((const char*)POBJ(rnode));
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
        CASE(NII) /* Ni!$"Expression to be evaluated by Python" */
            {
            if(Nii && !is_op(rnode) && !HAS_VISIBLE_FLAGS_OR_MINUS(rnode))
                {
                const char* val;
                val = Nii((const char*)POBJ(rnode));
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
        CASE(ERR) /* err $ <file name to direct errorStream to> */
            {
            if(!is_op(rnode))
                {
                if(redirectError((char*)POBJ(rnode)))
                    return functionOk(Pnode);
                }
            return functionFail(Pnode);
            }
#endif
#if !defined NO_C_INTERFACE
        CASE(ALC)  /* alc $ <number of bytes> */
            {
            void* p;
            if(is_op(rnode)
               || !INTEGER_POS(rnode)
               || (p = bmalloc((int)strtoul((char*)POBJ(rnode), (char**)NULL, 10)))
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
            if(is_op(rnode) || !INTEGER_POS(rnode))
                return functionFail(Pnode);
            p = strToPointer((char*)POBJ(rnode));
            pskfree((psk)p);
            return functionOk(Pnode);
            }
        CASE(PEE) /* pee $ (<pointer>[,<number of bytes>]) (1,2,4,8)*/
            {
            void* p;
            intVal.i = 1;
            if(is_op(rnode))
                {
                rlnode = rnode->LEFT;
                rrnode = rnode->RIGHT;
                if(!is_op(rrnode))
                    {
                    switch(rrnode->u.obj)
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
                rlnode = rnode;
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
            if(!is_op(rnode))
                return functionFail(Pnode);
            rlnode = rnode->LEFT;
            rrnode = rnode->RIGHT;
            if(is_op(rrnode))
                {
                psk rrrnode;
                rrrnode = rrnode->RIGHT;
                rrlnode = rrnode->LEFT;
                if(!is_op(rrrnode))
                    {
                    switch(rrrnode->u.obj)
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
                rrlnode = rrnode;
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
            if(sizeof(int(*)(void*)) != sizeof(void*) || !is_op(rnode))
                return functionFail(Pnode);
            u.vp = strToPointer((char*)POBJ(rnode->LEFT));
            if(!u.pfnc)
                return functionFail(Pnode);
            argStruct = strToPointer((char*)POBJ(rnode->RIGHT));
            return u.pfnc(argStruct) ? functionOk(Pnode) : functionFail(Pnode);
            }
#endif
        CASE(X2D) /* x2d $ hexnumber */
            {
            char* endptr;
            uint64_t val;
            if(is_op(rnode)
               || HAS_VISIBLE_FLAGS_OR_MINUS(rnode)
               )
                return functionFail(Pnode);
            errno = 0;
            val = STRTOUL((char*)POBJ(rnode), &endptr, 16);
            if(errno == ERANGE || (endptr && *endptr))
                return functionFail(Pnode); /*not all characters scanned*/
            sprintf(draft, "%" PRIu64, val);
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(D2X) /* d2x $ decimalnumber */
            {
            char* endptr;
            int64_t val;
            if(is_op(rnode) || !INTEGER_NOT_NEG(rnode))
                return functionFail(Pnode);
#ifdef __BORLANDC__
            if(strlen((char*)POBJ(rnode)) > 10
               || strlen((char*)POBJ(rnode)) == 10
               && strcmp((char*)POBJ(rnode), "4294967295") > 0
               )
                return functionFail(Pnode); /*not all characters scanned*/
#endif
            errno = 0;
            val = STRTOUL((char*)POBJ(rnode), &endptr, 10);
            if(errno == ERANGE
               || (endptr && *endptr)
               )
                return functionFail(Pnode); /*not all characters scanned*/
            sprintf(draft, "%" PRIX64, val);
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(Chr) /* chr $ number */
            {
            if(is_op(rnode) || !INTEGER_POS(rnode))
                return functionFail(Pnode);
            intVal.ul = strtoul((char*)POBJ(rnode), (char**)NULL, 10);
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
            unsigned long val;
            if(is_op(rnode) || !INTEGER_POS(rnode))
                return functionFail(Pnode);
            val = strtoul((char*)POBJ(rnode), (char**)NULL, 10);
            if(putCodePoint(val, (unsigned char*)draft) == NULL)
                return functionFail(Pnode);
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(ASC) /* asc $ character */
            {
            if(is_op(rnode))
                return functionFail(Pnode);
            sprintf(draft, "%d", (int)rnode->u.obj);
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(UGC)
            {
            if(is_op(rnode))
                {
                Pnode->v.fl |= FENCE;
                return functionFail(Pnode);
                }
            else
                {
                const char* s = (const char*)POBJ(rnode);
                if(*s)
                    {
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
                else
                    return functionFail(Pnode);
                }
            }
        CASE(UTF)
            {
            /*
            @(abcædef:? (%@>"~" ?:?a & utf$!a) ?)
            @(str$(abc chu$200 def):? (%@>"~" ?:?a & utf$!a) ?)
            */
            if(is_op(rnode))
                {
                Pnode->v.fl |= FENCE;
                return functionFail(Pnode);
                }
            else
                {
                const char* s = (const char*)POBJ(rnode);
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
            if(is_object(rnode) && !(rnode->LEFT->v.fl & VISIBLE_FLAGS))
                rnode = rnode->RIGHT;
            intVal.ul = rnode->v.fl;
            addr[3] = same_as_w(rnode);
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
            if(is_object(rnode)
               && Op(rnode->RIGHT) == DOT
               )
                {
                intVal.ul = rnode->RIGHT->LEFT->v.fl & VISIBLE_FLAGS;
                if(intVal.ul && (rnode->RIGHT->RIGHT->v.fl & intVal.ul))
                    return functionFail(Pnode);
                addr[3] = same_as_w(rnode->RIGHT->RIGHT);
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
#if SHOWMAXALLOCATED
        CASE(BEZ) /* bez $  */
            {
            Bez(draft);
            Pnode = build_up(Pnode, draft, NULL);
#if SHOWCURRENTLYALLOCATED
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
            if(RATIONAL_COMP(rlnode = rnode->LEFT) &&
               RATIONAL_COMP_NOT_NUL(rrnode = rnode->RIGHT))
                {
                psk pnode;
                pnode = qModulo(rlnode, rrnode);
                wipe(Pnode);
                Pnode = pnode;
                return functionOk(Pnode);
                }
            return functionFail(Pnode);
            }
        CASE(REV)
            {
            if(!is_op(rnode))
                {
                size_t len = strlen((char*)POBJ(rnode));
                psk pnode;
                pnode = same_as_w(rnode);
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
            if(!is_op(rnode))
                pnode = changeCase(rnode
#if CODEPAGE850
                                   , FALSE
#endif
                                   , TRUE);
            else if(!is_op(rlnode = rnode->LEFT))
                pnode = changeCase(rlnode
#if CODEPAGE850
                                   , search_opt(rnode->RIGHT, DOS)
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
            if(!is_op(rnode))
                pnode = changeCase(rnode
#if CODEPAGE850
                                   , FALSE
#endif
                                   , FALSE);
            else if(!is_op(rlnode = rnode->LEFT))
                pnode = changeCase(rlnode
#if CODEPAGE850
                                   , search_opt(rnode->RIGHT, DOS)
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
            if(is_op(rnode)
               && RATIONAL_COMP(rlnode = rnode->LEFT)
               && RATIONAL_COMP_NOT_NUL(rrnode = rnode->RIGHT)
               )
                {
                psk pnode;
                pnode = qIntegerDivision(rlnode, rrnode);
                wipe(Pnode);
                Pnode = pnode;
                return functionOk(Pnode);
                }
            return functionFail(Pnode);
            }
        CASE(DEN)
            {
            if(RATIONAL_COMP(rnode))
                {
                Pnode = qDenominator(Pnode);
                }
            return functionOk(Pnode);
            }
        CASE(LST)
            {
            return (output(&Pnode, lst) && GoodLST) ? functionOk(Pnode) : functionFail(Pnode);
            }
        CASE(MAP) /* map $ (<function>.<list>) */
            {
            if(is_op(rnode))
                {/*XXX*/
                psk nnode;
                psk nPnode;
                ppsk ppnode = &nPnode;
                rrnode = rnode->RIGHT;
                while(Op(rrnode) == WHITE)
                    {
#if 1
                    nnode = applyFncToElem_w(rnode->LEFT, rrnode->LEFT, Pnode->v.fl);
#else
                    nnode = (psk)bmalloc(sizeof(knode));
                    nnode->v.fl = Pnode->v.fl;
                    nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                    nnode->LEFT = same_as_w(rnode->LEFT);
                    nnode->RIGHT = same_as_w(rrnode->LEFT);
                    rlnode = setIndex(nnode);
                    if(rlnode)
                        nnode = rlnode;
                    else
                        {
                        if(not_built_in(nnode->LEFT)) /* Do not use ternary operator! That eats stack! */
                            nnode = execFnc(nnode);
                        else
                            {
                            rlnode = functions(nnode);
                            if(rlnode)
                                {
                                /*
                                if(!isSUCCESS(rlnode))
                                    {
                                    wipe(nnode);
                                    return functionFail(Pnode);
                                    }*/
                                nnode = rlnode;
                                }
                            else
                                nnode = execFnc(nnode);
                            }
                        }
#endif
                    if(!is_op(nnode) && IS_NIL(nnode))
                        {
                        wipe(nnode);
                        }
                    else
                        {
                        rlnode = (psk)bmalloc(sizeof(knode));
                        rlnode->v.fl = WHITE | SUCCESS;
                        *ppnode = rlnode;
                        ppnode = &(rlnode->RIGHT);
                        rlnode->LEFT = nnode;
                        }

                    rrnode = rrnode->RIGHT;
                    }
                if(is_op(rrnode) || !IS_NIL(rrnode))
                    {
#if 1
                    nnode = applyFncToElem_w(rnode->LEFT, rrnode, Pnode->v.fl);
#else
                    nnode = (psk)bmalloc(sizeof(knode));
                    nnode->v.fl = Pnode->v.fl;
                    nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                    nnode->LEFT = same_as_w(rnode->LEFT);
                    nnode->RIGHT = same_as_w(rrnode);
                    rlnode = setIndex(nnode);
                    if(rlnode)
                        nnode = rlnode;
                    else
                        {
                        if(not_built_in(nnode->LEFT)) /* Do not use ternary operator! That eats stack! */
                            nnode = execFnc(nnode);
                        else
                            {
                            rlnode = functions(nnode);
                            if(rlnode)
                                nnode = rlnode;
                            else
                                nnode = execFnc(nnode);
                            }
                        }
#endif
                    * ppnode = nnode;
                    }
                else
                    {
                    *ppnode = same_as_w(rrnode);
                    }
                wipe(Pnode);
                Pnode = nPnode;
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
        CASE(MOP)/*     mop $ (<function>.<tree1>.(=<tree2>))
                    mop regards tree1 as a right descending 'list' with a backbone
                    operator that is the same as the heading operator of tree 2.
                    For example,
                        mop$((=.!arg).2*a+e\Lb+c^2.(=+))
                    generates the list
                        2*a e\Lb c^2
                    */
            /*
            * (a.b)+(c.d)+(e.f):?s & mop$((=.!arg:(?.?arg)&!arg).!s.(=+).(=*))
            */
            {
            if(is_op(rnode))
                {/*XXX*/
                psk nnode;
                //psk fun = rnode->LEFT; // mop$(fun. ...)
                rrnode = rnode->RIGHT;
                ULONG fl = Pnode->v.fl;
                if(Op(rnode) == DOT)
                    {
                    if(Op(rrnode) == DOT)
                        {
                        rlnode = rrnode->LEFT; // mop$(fun.rlnode.rrnode ...)
                        rrnode = rrnode->RIGHT;
                        if(Op(rrnode) == DOT)
                            {
                            nnode = rrnode->RIGHT; // mop$(fun.rlnode.rrnode.nnode) : rlnode is data, rrnode is in-operator, nnode is out-operator 
                            rrnode = rrnode->LEFT;
                            }
                        else
                            {
                            nnode = 0;
                            }
                        if(Op(rrnode) == EQUALS)
                            {
                            intVal.ul = Op(rrnode->RIGHT); // in-operator
                            if(intVal.ul)
                                {
                                if(nnode)
                                    {
                                    if(Op(nnode) == EQUALS)
                                        {
                                        if(Op(nnode->RIGHT))
                                            {
                                            SortMOP(rnode->LEFT, rlnode, intVal.ul, nnode, fl, &Pnode);
                                            }
                                        else
                                            /* No operator specified */
                                            return functionFail(Pnode);
                                        }
                                    else
                                        /* The last argument must have heading = operator */
                                        return functionFail(Pnode);
                                    }
                                else
                                    SortMOP(rnode->LEFT, rlnode, intVal.ul, 0, fl, &Pnode);
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
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
        CASE(Vap) /*    vap $ (<function>.<string>.<char>)
                        or vap $ (<function>.<string>)
                        Second form splits in characters. */
            {
            if(is_op(rnode))
                {/*XXX*/
                rrnode = rnode->RIGHT;
                if(is_op(rrnode))
                    { /* first form */
                    psk sepnode = rrnode->RIGHT;
                    rrnode = rrnode->LEFT;
                    if(is_op(sepnode) || is_op(rrnode))
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
                            char* subject = &rrnode->u.sobj;
                            psk nPnode;
                            ppsk ppnode = &nPnode;
                            char* oldsubject = subject;
                            while(subject)
                                {
                                psk elm;
                                subject = strstr(oldsubject, separator);
                                if(subject)
                                    {
                                    *subject = '\0';
                                    elm = scopy(oldsubject);
                                    *subject = separator[0];
                                    oldsubject = subject + strlen(separator);
                                    }
                                else
                                    {
                                    elm = scopy(oldsubject);
                                    }
                                psk nnode = applyFncToString(rnode->LEFT, elm, Pnode->v.fl);
                                if(subject)
                                    { /* strstr did find a separator. So this is not the last element yet. */
                                    rlnode = (psk)bmalloc(sizeof(knode));
                                    rlnode->v.fl = WHITE | SUCCESS;
                                    *ppnode = rlnode;
                                    ppnode = &(rlnode->RIGHT);
                                    rlnode->LEFT = nnode;
                                    }
                                else
                                    { /* This is the last (or only) element */
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
                    const char* subject = &rrnode->u.sobj;
                    psk nPnode = 0;
                    ppsk ppnode = &nPnode;
                    const char* oldsubject = subject;
                    int k;
                    if(hasUTF8MultiByteCharacters(subject))
                        {
                        for(; (k = getCodePoint(&subject)) > 0; oldsubject = subject)
                            {
                            psk nnode = applyFncToString(rnode->LEFT, charcopy(oldsubject, subject), Pnode->v.fl);
                            if(*subject)
                                { /* This is not the last character in the subject. */
                                rlnode = (psk)bmalloc(sizeof(knode));
                                rlnode->v.fl = WHITE | SUCCESS;
                                *ppnode = rlnode;
                                ppnode = &(rlnode->RIGHT);
                                rlnode->LEFT = nnode;
                                }
                            else
                                { /* This was the last character in the subject */
                                *ppnode = nnode;
                                }
                            }
                        }
                    else
                        {
                        for(; (k = *subject++) != 0; oldsubject = subject)
                            {
                            psk nnode = applyFncToString(rnode->LEFT, charcopy(oldsubject, subject), Pnode->v.fl);
                            if(*subject)
                                {
                                rlnode = (psk)bmalloc(sizeof(knode));
                                rlnode->v.fl = WHITE | SUCCESS;
                                *ppnode = rlnode;
                                ppnode = &(rlnode->RIGHT);
                                rlnode->LEFT = nnode;
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
            if(is_op(rnode)
               && !is_op(rlnode = rnode->LEFT)
               && !is_op(rrnode = rnode->RIGHT)
               )
                {
                intVal.i = rename((const char*)POBJ(rlnode), (const char*)POBJ(rrnode));
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
            if(!is_op(rnode))
                {
#if defined __SYMBIAN32__
                intVal.i = unlink((const char*)POBJ(rnode));
#else
                intVal.i = remove((const char*)POBJ(rnode));
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
            if(is_op(rnode))
                return functionFail(Pnode);
            if(PLOBJ(rnode) != '\0')
                {
                unsigned long val;
                if(!INTEGER_NOT_NEG(rnode))
                    return functionFail(Pnode);
                val = strtoul((char*)POBJ(rnode), (char**)NULL, 10);
                if(val >= (unsigned long)ARGC)
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
            if(is_op(rnode))
                {
                if(is_op(rlnode = rnode->LEFT))
                    return functionFail(Pnode);
                rrnode = rnode->RIGHT;
                intVal.i = (search_opt(rrnode, ECH) << SHIFT_ECH)
                    + (search_opt(rrnode, MEM) << SHIFT_MEM)
                    + (search_opt(rrnode, VAP) << SHIFT_VAP)
                    + (search_opt(rrnode, STG) << SHIFT_STR)
                    + (search_opt(rrnode, ML) << SHIFT_ML)
                    + (search_opt(rrnode, TRM) << SHIFT_TRM)
                    + (search_opt(rrnode, HT) << SHIFT_HT)
                    + (search_opt(rrnode, X) << SHIFT_X)
                    + (search_opt(rrnode, JSN) << SHIFT_JSN)
                    + (search_opt(rrnode, TXT) << SHIFT_TXT)
                    + (search_opt(rrnode, BIN) << SHIFT_BIN);
                }
            else
                {
                intVal.i = 0;
                rlnode = rnode;
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
            if(is_op(rnode) || (PLOBJ(rnode) == '\0'))
                return functionFail(Pnode);
            else
                {
                intVal.i = system((const char*)POBJ(rnode));
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
            if(is_op(rnode))
                return psh(rnode->LEFT, &zeroNode, rnode->RIGHT) ? functionOk(Pnode) : functionFail(Pnode);
            else
                return functionFail(Pnode);
            }
#if 0
        /*
        The same effect is obtained by <expr>:?!(=)
        */
        CASE(PRV) /* "?"$<expr> */
            {
            if((rnode->v.fl & SUCCESS)
               && (is_op(rnode) || rnode->u.obj || HAS_UNOPS(rnode)))
                insert(&nilNode, rnode);
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
            if(is_op(rnode)
               && !is_op(rlnode = rnode->LEFT)
               && !is_op(rrnode = rnode->RIGHT))
                {
                Sim(draft, (char*)POBJ(rlnode), (char*)POBJ(rrnode));
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
        CASE(TME)
            {
            time_t seconds;
            struct tm* f;
            time(&seconds);
            if(search_opt(Pnode->RIGHT, GMT))
                {
                f = gmtime(&seconds);
                }
            else /* local time */
                {
                f = localtime(&seconds);
                }
#if 0
            struct tm
                {
                int tm_sec;         /* seconds,  range 0 to 59          */
                int tm_min;         /* minutes, range 0 to 59           */
                int tm_hour;        /* hours, range 0 to 23             */
                int tm_mday;        /* day of the month, range 1 to 31  */
                int tm_mon;         /* month, range 0 to 11             */
                int tm_year;        /* The number of years since 1900   */
                int tm_wday;        /* day of the week, range 0 to 6    */
                int tm_yday;        /* day in the year, range 0 to 365  */
                int tm_isdst;       /* daylight saving time             */
                };
#endif
            sprintf(draft, "%d.%d.%d.%d.%d.%d.%d.%d.%d",
                    f->tm_sec, f->tm_min, f->tm_hour, f->tm_mday, 1 + f->tm_mon,
                    1900 + f->tm_year, 1 + f->tm_wday, 1 + f->tm_yday, f->tm_isdst);
            Pnode = build_up(Pnode, draft, NULL);
            return functionOk(Pnode);
            }
        CASE(WHL)
            {
            while(isSUCCESSorFENCE(rnode = eval(same_as_w(Pnode->RIGHT))))
                {
                wipe(rnode);
                }
            wipe(rnode);
            return functionOk(Pnode);
            }
        CASE(New) /* new$<object>*/
            {
            if(Op(rnode) == COMMA)
                {
                addr[2] = getObjectDef(rnode->LEFT);
                if(!addr[2])
                    return functionFail(Pnode);
                addr[3] = rnode->RIGHT;
                if(ISBUILTIN((objectnode*)addr[2]))
                    Pnode = build_up(Pnode, "((\2.New)'\3)&\2", NULL);
                /* We might be able to call 'new' if 'New' had attached the argument
                    (containing the definition of a 'new' method) to the rhs of the '='.
                    This cannot be done in a general way without introducing new syntax rules for the new$ function.
                */
                else
                    Pnode = build_up(Pnode, "(((\2.new)'\3)|)&\2", NULL);
                }
            else
                {
                addr[2] = getObjectDef(rnode);
                if(!addr[2])
                    return functionFail(Pnode);
                if(ISBUILTIN((objectnode*)addr[2]))
                    Pnode = build_up(Pnode, "((\2.New)')&\2", NULL);
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
                        rnode = evalmacro(Pnode->RIGHT);
                        rrnode = (psk)bmalloc(sizeof(objectnode));
#if WORD32
                        ((typedObjectnode*)rrnode)->u.Int = 0;
#else
                        ((typedObjectnode*)rrnode)->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
#endif
                        rrnode->v.fl = EQUALS | SUCCESS;
                        rrnode->LEFT = same_as_w(&nilNode);
                        if(rnode)
                            {
                            rrnode->RIGHT = rnode;
                            }
                        else
                            {
                            rrnode->RIGHT = same_as_w(Pnode->RIGHT);
                            }
                        wipe(Pnode);
                        Pnode = rrnode;
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
            return 0;
            }
        }
    }
    /*return functionOk(Pnode); 20 Dec 1995, unreachable code in Borland C */
    }
