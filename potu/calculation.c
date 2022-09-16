#include "calculation.h"
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
#include <math.h>

#include "result.h" /* For debugging. Remove when done. */


/*
(     new$(calculation,(=sin$!a0)):?calc
    & (calc..calculate)$"1.0E-01"
)

(      new$(calculation,(=hypot$(!a0.!a1))):?calc
    & (calc..calculate)$("1.0E-01"."2.0E0")
)
    
(    new$(calculation,(=!a0*!a1:?p&!p:>"1.0E1"&"1.0E0"|"0.0E0")):?calc&(calc..print)$
   & (calc..calculate)$("3.4E0","4.0E0")
)    
    
    */



typedef enum {
    TheEnd
    , ResolveAndPush
    , ResolveAndGet
    , Push
    , Afunction
    , Jump
    } actionType;

struct forthMemory;

typedef void (*funct)(struct forthMemory* This);

struct forthvariable;

typedef union forthvalue /* a number. either integer or 'real' */
    {
    double floating;
    LONG integer;
    } forthvalue;

typedef union stackvalue
    {
    forthvalue val;
    forthvalue* valp; /*pointer to value held by a variable*/
    } stackvalue;

typedef struct forthvariable
    {
    forthvalue u;
    char* name;
    struct forthvariable* next;
    } forthvariable;

typedef struct forthword
    {
    actionType action : 4;
    unsigned int offset : 28;
    union
        {
        double floating; LONG integer; funct funcp; forthvalue* valp; forthvalue val;
        } u;
    } forthword;

typedef struct forthMemory
    {
    forthword* word; /* fixed once calculation is compiled */
    forthword* wordp; /* runs through words when calculating */
    stackvalue stack[64];
    stackvalue* sp;
    forthvariable* var;
    } forthMemory;

static char* getVarName(forthvariable* varp, forthvalue *u)
    {
    for (;varp;++varp)
        {
        if (&(varp->u.floating) == &(u->floating))
            return varp->name;
        }
    return "UNKN";
    }

static forthvalue* getVariablePointer(forthvariable** varp, char* name)
    {
    forthvariable* curvarp = *varp;
    while (curvarp != 0 && strcmp(curvarp->name, name))
        {
        curvarp = curvarp->next;
        }
    if (curvarp == 0)
        {
        curvarp = *varp;
        *varp = (forthvariable*)bmalloc(__LINE__, sizeof(forthvariable));
        (*varp)->name = bmalloc(__LINE__, strlen(name) + 1);
        strcpy((*varp)->name, name);
        (*varp)->next = curvarp;
        curvarp = *varp;
        }
    return &(curvarp->u);
    }

static int setArgs(forthvariable** varp, psk args, int nr)
    {
    if (is_op(args))
        {
        nr = setArgs(varp, args->LEFT, nr);
        if (nr > 0)
            return setArgs(varp, args->RIGHT, nr);
        }
    else
        {
        static char name[24];/*Enough for 64 bit number in decimal.*/
        forthvalue* val;
        sprintf(name, "a%d", nr);
        val = getVariablePointer(varp, name);
        if (args->v.fl & QDOUBLE)
            {
            val->floating = strtod(&(args->u.sobj), 0);
            return 1 + nr;
            }
        else if (INTEGER(args))
            {
            val->integer = strtol(&(args->u.sobj), 0, 10);
            if (HAS_MINUS_SIGN(args))
                val->integer = -val->integer;
            return 1 + nr;
            }
        }
    return -1;
    }

static void justpush(forthMemory* This, forthvalue val)
    {
    assert(This->sp < &(This->stack[0]) + sizeof(This->stack) / sizeof(This->stack[0]));
    (This->sp)->val = val;
    ++(This->sp);
    }

static Boolean calculate(struct typedObjectnode* This, ppsk arg)
    {
    psk Arg = (*arg)->RIGHT;
    forthMemory* mem = (forthMemory*)(This->voiddata);
    printf("\n[");
    result(Arg);
    printf("]\n");
    if (setArgs(&(mem->var), Arg, 0) > 0)
        {
        for (mem->wordp = mem->word;
             mem->wordp->action != TheEnd;
             ++(mem->wordp)
             )
            {
            switch (mem->wordp->action)
                {
                case ResolveAndPush:
                    {
                    forthvalue val = *(mem->wordp->u.valp);
                    justpush(mem, val);
                    break;
                    }
                case ResolveAndGet:
                    {
                    forthvalue* valp = mem->wordp->u.valp;
                    stackvalue* sv = mem->sp;
                    assert(sv > mem->stack);
                    *valp = ((sv - 1)->val);
                    break;
                    }
                case Push:
                    {
                    forthvalue val = mem->wordp->u.val;
                    justpush(mem, val);
                    break;
                    }
                case Afunction:
                    mem->wordp->u.funcp(mem);
                    break;
                case Jump:
                    mem->wordp = mem->word + mem->wordp->offset;
                    break;
                default:
                    ;
                }
            }
        for (; mem->sp > mem->stack;)
            {
            psk res;
            size_t len;
            char buf[64]; /* 64 bytes is even enough for quad https://people.eecs.berkeley.edu/~wkahan/ieee754status/IEEE754.PDF*/
            stackvalue* sv = --(mem->sp);
            sprintf(buf, "%.16E", sv->val.floating);
            len = offsetof(sk, u.obj) + strlen(buf);
            res = (psk)bmalloc(__LINE__, len + 1);
            strcpy((char*)POBJ(res), buf);
            if (strcmp(buf, "INF") && strcmp(buf, "NAN"))
                res->v.fl = READY | SUCCESS | QNUMBER | QDOUBLE BITWISE_OR_SELFMATCHING;
            else
                res->v.fl = READY BITWISE_OR_SELFMATCHING;
            /*    res->v.fl |= g->sign;*/
            if (res)
                {
                wipe(*arg);
                *arg = same_as_w(res);
                return TRUE;
                }
            }
        }
    return FALSE;
    }


static int polish1(psk code)
    {
    int C;
    int R;
    printf("\npolish1{"); result(code); printf("}\n");
    switch (Op(code))
        {
        case PLUS:
        case TIMES:
        case EXP:
        case LOG:
            {
            R = polish1(code->LEFT);
            if (R == -1)
                return -1;
            C = polish1(code->RIGHT);
            if (C == -1)
                return -1;
            return 1 + R + C;
            }
        case AND:
            {
            R = polish1(code->LEFT);
            if (R == -1)
                return -1;
            C = polish1(code->RIGHT);
            if (C == -1)
                return -1;
            return 1 + R + C; /* one for AND (containing address to 'if false' branch). */
            }
        case OR:
            {
            R = polish1(code->LEFT);
            if (R == -1)
                return -1;
            C = polish1(code->RIGHT);
            if (C == -1)
                return -1;
            return 1 + R + C; /* one for OR, containing address to 'if true' branch */
            }
        case MATCH:
            {
            R = polish1(code->LEFT);
            if (R == -1)
                return -1;
            C = polish1(code->RIGHT);
            if (C == -1)
                return -1;
            return R + C;
            }
        case FUN:
        case FUU:
            if (is_op(code->LEFT))
                return -1;
            C = polish1(code->RIGHT);
            if (C == -1)
                return -1;
            return 1 + C;
        default:
            if (is_op(code))
                {
                R = polish1(code->LEFT);
                if (R == -1)
                    return -1;
                C = polish1(code->RIGHT);
                if (C == -1)
                    return -1;
                return R + C; /* Do not reserve room for operator! */
                }
            else
                {
                if (code->v.fl & QDOUBLE || INTEGER(code))
                    return 1;
                else if (code->v.fl & (UNIFY | INDIRECT))
                    return 1; /* variable */
                else
                    return -1;
                }
        }
    }
static forthvalue cpop(forthMemory* This)
    {
    assert(This->sp > This->stack);
    --(This->sp);
    return (This->sp)->val;
    }

static void fpush(forthMemory* This, double val)
    {
    assert(This->sp < &(This->stack[0]) + sizeof(This->stack) / sizeof(This->stack[0]));
    (This->sp)->val.floating = val;
    ++(This->sp);
    }

static void ipush(forthMemory* This, LONG val)
    {
    assert(This->sp < &(This->stack[0]) + sizeof(This->stack) / sizeof(This->stack[0]));
    (This->sp)->val.integer = val;
    ++(This->sp);
    }

static void fplus(forthMemory* This)
    {
    double a = cpop(This).floating;
    double b = cpop(This).floating;
    fpush(This, a + b);
    }
static void ftimes(forthMemory* This)
    {
    double a = cpop(This).floating;
    double b = cpop(This).floating;
    fpush(This, a * b);
    }
static void fexp(forthMemory* This)
    {
    double a = cpop(This).floating;
    double b = cpop(This).floating;
    fpush(This, pow(a, b));
    }
static void flog(forthMemory* This)
    {
    double a = cpop(This).floating;
    double b = cpop(This).floating;
    fpush(This, log(a) / log(b));
    }

static void fand(forthMemory* This) /* Consumes top of stack if no problem found. */
    {
    forthvalue v = cpop(This);
    if (isnan(v.floating) || isinf(v.floating) || v.integer == 0)
        {
        ++This->sp; /* Keep 0 on the stack, in case an OR wants to test. */
        This->wordp = This->word + This->wordp->offset; /* skip RIGHT side */
        }
    }

static void fOr(forthMemory* This) /* Consumes top of stack ! */
    {
    forthvalue v = cpop(This);
    if (!isnan(v.floating) && !isinf(v.floating) && v.integer != 0)
        { /* No problem found. Continue after 'if false' branch. */
        ++This->sp; /* Keep value on the stack, in case this is the result wanted */
        This->wordp = This->word + This->wordp->offset;
        }
    }

static forthvalue* pcpop(forthMemory* This)
    {
    assert(This->sp > This->stack);
    --(This->sp);
    return (This->sp)->valp;
    }

static void cpush(forthMemory* This, forthvalue val)
    {
    assert(This->sp < &(This->stack[0]) + sizeof(This->stack) / sizeof(This->stack[0]));
    (This->sp)->val = val;
    ++(This->sp);
    }

static void pcpush(forthMemory* This, forthvalue* valp)
    {
    assert(This->sp < &(This->stack[0]) + sizeof(This->stack) / sizeof(This->stack[0]));
    (This->sp)->valp = valp;
    ++(This->sp);
    }

static void Cacos(forthMemory* This) { double a = cpop(This).floating; fpush(This, acos(a)); }
static void Cacosh(forthMemory* This) { double a = cpop(This).floating; fpush(This, acosh(a)); }
static void Casin(forthMemory* This) { double a = cpop(This).floating; fpush(This, asin(a)); }
static void Casinh(forthMemory* This) { double a = cpop(This).floating; fpush(This, asinh(a)); }
static void Catan(forthMemory* This) { double a = cpop(This).floating; fpush(This, atan(a)); }
static void Catanh(forthMemory* This) { double a = cpop(This).floating; fpush(This, atanh(a)); }
static void Ccbrt(forthMemory* This) { double a = cpop(This).floating; fpush(This, cbrt(a)); }
static void Cceil(forthMemory* This) { double a = cpop(This).floating; fpush(This, ceil(a)); }
static void Ccos(forthMemory* This) { double a = cpop(This).floating; fpush(This, cos(a)); }
static void Ccosh(forthMemory* This) { double a = cpop(This).floating; fpush(This, cosh(a)); }
static void Cexp(forthMemory* This) { double a = cpop(This).floating; fpush(This, exp(a)); }
static void Cfabs(forthMemory* This) { double a = cpop(This).floating; fpush(This, fabs(a)); }
static void Cfloor(forthMemory* This) { double a = cpop(This).floating; fpush(This, floor(a)); }
static void Clog(forthMemory* This) { double a = cpop(This).floating; fpush(This, log(a)); }
static void Clog10(forthMemory* This) { double a = cpop(This).floating; fpush(This, log10(a)); }
static void Csin(forthMemory* This) { double a = cpop(This).floating; fpush(This, sin(a)); }
static void Csinh(forthMemory* This) { double a = cpop(This).floating; fpush(This, sinh(a)); }
static void Csqrt(forthMemory* This) { double a = cpop(This).floating; fpush(This, sqrt(a)); }
static void Ctan(forthMemory* This) { double a = cpop(This).floating; fpush(This, tan(a)); }
static void Ctanh(forthMemory* This) { double a = cpop(This).floating; fpush(This, tanh(a)); }

static void Catan2(forthMemory* This) { double b = cpop(This).floating; double a = cpop(This).floating; fpush(This, atan2(a, b)); }
static void Cfdim(forthMemory* This) { double b = cpop(This).floating; double a = cpop(This).floating; fpush(This, fdim(a, b)); }
static void Cfmax(forthMemory* This) { double b = cpop(This).floating; double a = cpop(This).floating; fpush(This, fmax(a, b)); }
static void Cfmin(forthMemory* This) { double b = cpop(This).floating; double a = cpop(This).floating; fpush(This, fmin(a, b)); }
static void Cfmod(forthMemory* This) { double b = cpop(This).floating; double a = cpop(This).floating; fpush(This, fmod(a, b)); }
static void Chypot(forthMemory* This) { double b = cpop(This).floating; double a = cpop(This).floating; fpush(This, hypot(a, b)); }
static void Cpow(forthMemory* This) { double b = cpop(This).floating; double a = cpop(This).floating; fpush(This, pow(a, b)); }


static void fless(forthMemory* This) { double b = cpop(This).floating; double a = cpop(This).floating; ipush(This, (a < b)); }
static void fless_equal(forthMemory* This) { double b = cpop(This).floating; double a = cpop(This).floating; ipush(This, (a <= b)); }
static void fmore_equal(forthMemory* This) { double b = cpop(This).floating; double a = cpop(This).floating; ipush(This, (a >= b)); }
static void fmore(forthMemory* This) { double b = cpop(This).floating; double a = cpop(This).floating; ipush(This, (a > b)); }
static void funequal(forthMemory* This) { double b = cpop(This).floating; double a = cpop(This).floating; ipush(This, (a != b)); }
static void flessormore(forthMemory* This) { double b = cpop(This).floating; double a = cpop(This).floating; ipush(This, (a != b)); }
static void fequal(forthMemory* This) { double b = cpop(This).floating; double a = cpop(This).floating; ipush(This, (a == b)); }
static void fnotlessormore(forthMemory* This) { double b = cpop(This).floating; double a = cpop(This).floating; ipush(This, (a == b)); }

typedef struct
    {
    char* name;
    void(*Cfun)(forthMemory*);
    }Cpair;

static Cpair pairs[] =
    {
        {"acos",  Cacos},
        {"acosh", Cacosh},
        {"asin",  Casin},
        {"asinh", Casinh},
        {"atan",  Catan},
        {"atanh", Catanh},
        {"cbrt",  Ccbrt},
        {"ceil",  Cceil},
        {"cos",   Ccos},
        {"cosh",  Ccosh},
        {"exp",   Cexp},
        {"fabs",  Cfabs},
        {"floor", Cfloor},
        {"log",   Clog},
        {"log10", Clog10},
        {"sin",   Csin},
        {"sinh",  Csinh},
        {"sqrt",  Csqrt},
        {"tan",   Ctan},
        {"tanh",  Ctanh},
        {"atan2", Catan2},
        {"fdim",  Cfdim},
        {"fmax",  Cfmax},
        {"fmin",  Cfmin},
        {"fmod",  Cfmod},
        {"hypot", Chypot},
        {"pow",   Cpow},

        {"fplus"         ,fplus         },
        {"ftimes"        ,ftimes        },
        {"fexp"          ,fexp          },
        {"flog"          ,flog          },
        {"fand"          ,fand          },
        {"fOr"           ,fOr           },
        {"fless"         ,fless         },
        {"fless_equal"   ,fless_equal   },
        {"fmore_equal"   ,fmore_equal   },
        {"fmore"         ,fmore         },
        {"funequal"      ,funequal      },
        {"flessormore"   ,flessormore   },
        {"fequal"        ,fequal        },
        {"fnotlessormore",fnotlessormore},
        {"fOr"           ,fOr           },

        {0,0}
    };


static Boolean print(struct typedObjectnode* This, ppsk arg)
    {
    forthMemory* mem = (forthMemory*)(This->voiddata);
    for (mem->wordp = mem->word;
         mem->wordp->action != TheEnd;
         ++(mem->wordp)
         )
        {
        switch (mem->wordp->action)
            {
            case ResolveAndPush:
                {
                forthvalue val = *(mem->wordp->u.valp);
                printf(LONGD " ResolveAndPush %s %f\n", mem->wordp - mem->word, getVarName(mem->var, (mem->wordp->u.valp)),val.floating);
                break;
                }
            case ResolveAndGet:
                {
                forthvalue val = *(mem->wordp->u.valp);
                printf(LONGD " ResolveAndGet  %s %f\n", mem->wordp - mem->word, getVarName(mem->var, (mem->wordp->u.valp)), val.floating);
                break;
                }
            case Push:
                {
                forthvalue val = mem->wordp->u.val;
                printf(LONGD " Push              %f\n", mem->wordp - mem->word, val.floating);
                break;
                }
            case Afunction:
                {
                Cpair* cpair;
                char* naam = "UNK";
                for (cpair = pairs; cpair->name; ++cpair)
                    {
                    if (cpair->Cfun == mem->wordp->u.funcp)
                        {
                        naam = cpair->name;
                        break;
                        }
                    }
                printf(LONGD " Afunction     %s %d\n", mem->wordp - mem->word, naam, mem->wordp->offset);
                break;
                }
            case Jump:
                printf(LONGD " Jump          \n", mem->wordp - mem->word);
                break;
            default:
                printf(LONGD " default       %d\n", mem->wordp - mem->word, mem->wordp->action);
                ;
            }
        }
    printf(LONGD " TheEnd          \n", mem->wordp - mem->word);
    return TRUE;
    }


static forthword* polish2(forthvariable** varp, psk code, forthword* wordp, forthword* word)
    {
    switch (Op(code))
        {
        case PLUS:
            wordp = polish2(varp, code->LEFT, wordp, word);
            wordp = polish2(varp, code->RIGHT, wordp, word);
            wordp->action = Afunction;
            wordp->offset = 0;
            wordp->u.funcp = fplus;
            return ++wordp;
        case TIMES:
            wordp = polish2(varp, code->LEFT, wordp, word);
            wordp = polish2(varp, code->RIGHT, wordp, word);
            wordp->action = Afunction;
            wordp->offset = 0;
            wordp->u.funcp = ftimes;
            return ++wordp;
        case EXP:
            wordp = polish2(varp, code->LEFT, wordp, word);
            wordp = polish2(varp, code->RIGHT, wordp, word);
            wordp->action = Afunction;
            wordp->offset = 0;
            wordp->u.funcp = fexp;
            return ++wordp;
        case LOG:
            wordp = polish2(varp, code->LEFT, wordp, word);
            wordp = polish2(varp, code->RIGHT, wordp, word);
            wordp->action = Afunction;
            wordp->offset = 0;
            wordp->u.funcp = flog;
            return ++wordp;
        case AND:
            {
            /*
                        0 or 1      Result of comparison between to doubles
                    AND @1          address to jump to 'if false' == end of RIGHT
                        RIGHT       start of 'if true' branch
                        ....
                        ....        end of 'if true' branch (RIGHT)
            @1:         ....
                        ....

            */
            forthword* saveword;
            wordp = polish2(varp, code->LEFT, wordp, word);
            saveword = wordp;
            saveword->action = Afunction;
            saveword->u.funcp = fand;
            wordp = polish2(varp, code->RIGHT, ++wordp, word);
            saveword->offset = (unsigned int)(wordp - word);
            return wordp;
            }
        case OR: /* jump if true, continue if false */
            {
            /*
                        0 or 1      Result of comparison between to doubles
                    OR  @1          address to jump to 'if true' == end of RIGHT
                        RIGHT       start of 'if false' branch
                        ....
                        ....        end of 'if false' branch (RIGHT)
            @1:         ....
                        ....

            */
            forthword* saveword;
            wordp = polish2(varp, code->LEFT, wordp, word);
            saveword = wordp;
            saveword->action = Afunction;
            saveword->u.funcp = fOr;
            wordp = polish2(varp, code->RIGHT, ++wordp, word);
            saveword->offset = (unsigned int)(ULONG)(wordp - word); /* the address of the word after the 'if false' branch.*/
            return wordp;
            }
        case MATCH:
            {
            wordp = polish2(varp, code->LEFT, wordp, word);
            wordp = polish2(varp, code->RIGHT, wordp, word);
            if (!(code->RIGHT->v.fl & UNIFY))
                {
                if (FLESS(code->RIGHT))
                    {
                    wordp->action = Afunction;
                    wordp->offset = 0;
                    wordp->u.funcp = fless;
                    return ++wordp;
                    }
                else if (FLESS_EQUAL(code->RIGHT))
                    {
                    wordp->action = Afunction;
                    wordp->offset = 0;
                    wordp->u.funcp = fless_equal;
                    return ++wordp;
                    }
                else if (FMORE_EQUAL(code->RIGHT))
                    {
                    wordp->action = Afunction;
                    wordp->offset = 0;
                    wordp->u.funcp = fmore_equal;
                    return ++wordp;
                    }
                else if (FMORE(code->RIGHT))
                    {
                    wordp->action = Afunction;
                    wordp->offset = 0;
                    wordp->u.funcp = fmore;
                    return ++wordp;
                    }
                else if (FUNEQUAL(code->RIGHT))
                    {
                    wordp->action = Afunction;
                    wordp->offset = 0;
                    wordp->u.funcp = funequal;
                    return ++wordp;
                    }
                else if (FLESSORMORE(code->RIGHT))
                    {
                    wordp->action = Afunction;
                    wordp->offset = 0;
                    wordp->u.funcp = flessormore;
                    return ++wordp;
                    }
                else if (FEQUAL(code->RIGHT))
                    {
                    wordp->action = Afunction;
                    wordp->offset = 0;
                    wordp->u.funcp = fequal;
                    return ++wordp;
                    }
                else if (FNOTLESSORMORE(code->RIGHT))
                    {
                    wordp->action = Afunction;
                    wordp->offset = 0;
                    wordp->u.funcp = fnotlessormore;
                    return ++wordp;
                    }
                }
            return wordp;
            }
        case FUU: /* whl'(blbla) */
            {
            unsigned int here = (unsigned int)(ULONG)(wordp - word); /* start of loop */
            char* name = &code->LEFT->u.sobj;
            if (strcmp(name, "whl"))
                return 0;
            wordp = polish2(varp, code->RIGHT, wordp, word);
            wordp->action = Afunction;
            wordp->u.funcp = fOr;
            wordp->offset = here; /* If all good, jump back to start of loop */
            return ++wordp;;
            }
        case FUN:
            {
            Cpair* p = pairs;
            char* name = &code->LEFT->u.sobj;
            wordp = polish2(varp, code->RIGHT, wordp, word);
            wordp->action = Afunction;
            wordp->offset = 0;
            for (; p->name != 0; ++p)
                {
                if (!strcmp(p->name, name))
                    {
                    wordp->u.funcp = p->Cfun;
                    return ++wordp;
                    }
                }
            return 0;
            }
        default:
            if (is_op(code))
                {
                wordp = polish2(varp, code->LEFT, wordp, word);
                wordp = polish2(varp, code->RIGHT, wordp, word);
                return wordp;
                }
            else
                {
                if (INTEGER(code))
                    {
                    wordp->u.integer = (int)STRTOL(&(code->u.sobj), 0, 10);
                    if (HAS_MINUS_SIGN(code))
                        {
                        wordp->u.integer = -(wordp->u.integer);
                        }
                    wordp->action = Push;
                    /*When executing, push number onto the data stack*/
                    }
                else if (code->v.fl & QDOUBLE)
                    {
                    wordp->u.floating = strtod(&(code->u.sobj), 0);
                    wordp->action = Push;
                    /*When executing, push number onto the data stack*/
                    }
                else
                    {
                    /*variable*/
                    wordp->u.valp = getVariablePointer(varp, &(code->u.sobj));
                    wordp->action = (code->v.fl & INDIRECT) ? ResolveAndPush : (code->v.fl & UNIFY) ? ResolveAndGet : Push;
                    /* When executing,
                    * ResolveAndPush: follow the pointer wordp->u.valp and push the pointed-at value onto the stack.
                    * ResolveAndGet:  follow the pointer wordp->u.valp and assign to that address the value that is on top of the stack. Do not change the stack.
                    * Push:           Push the value wordp->u.valp onto the stack. (This should not occur. It is an address!)
                    */
                    }
                ++wordp;
                return wordp;
                };
        }
    }

static Boolean calculationnew(struct typedObjectnode* This, ppsk arg)
    {
    psk code = (*arg)->RIGHT;
    forthword* lastword;
    forthMemory* forthstuff;
    int length;
    printf("\ncalculationnew{"); result(code); printf("}\n");
    if (is_object(code))
        code = code->RIGHT;
    length = polish1(code);
    if (length < 0)
        return FALSE;
    This->voiddata = bmalloc(__LINE__, sizeof(forthMemory));
    forthstuff = (forthMemory*)(This->voiddata);
    forthstuff->var = 0;
    forthstuff->word = bmalloc(__LINE__, length * sizeof(forthword) + 1);
    forthstuff->wordp = forthstuff->word;
    forthstuff->sp = forthstuff->stack;
    lastword = polish2(&(forthstuff->var), code, forthstuff->wordp, forthstuff->word);
    lastword->action = TheEnd;
    return TRUE;
    }

static Boolean calculationdie(struct typedObjectnode* This, ppsk arg)
    {
    forthvariable* curvarp = ((forthMemory*)(This->voiddata))->var;
    while (curvarp)
        {
        forthvariable* nextvarp = curvarp->next;
        bfree(curvarp->name);
        bfree(curvarp);
        curvarp = nextvarp;
        }
    bfree(((forthMemory*)(This->voiddata))->word);
    bfree(This->voiddata);
    return TRUE;
    }

method calculation[] = {
    {"calculate",calculate},
    {"print",print},
    {"New",calculationnew},
    {"Die",calculationdie},
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

