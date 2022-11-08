#include "calculation.h"
#include "nodedefs.h"
#include "memory.h"
#include "wipecopy.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "result.h" /* For debugging. Remove when done. */

#define COMPACT 1
#if COMPACT
#define INC 1
#else
#define INC 2
#endif

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


static double Nan;

typedef enum {
    TheEnd
    , ResolveAndPush
    , ResolveAndGet
    , Push
    , Afunction
    /*
    , Cfunction1
    , Cfunction2
    */
    , Abranch
    , CondBranch
    , UncondBranch
    , Acos
    , Acosh
    , Asin
    , Asinh
    , Atan
    , Atanh
    , Cbrt
    , Ceil
    , Cos
    , Cosh
    , Exp
    , Fabs
    , Floor
    , Log
    , Log10
    , Sin
    , Sinh
    , Sqrt
    , Tan
    , Tanh
    , Atan2
    , Fdim
    , Fmax
    , Fmin
    , Fmod
    , Hypot
    , Pow



    , NoOp
    /*
    , fand
    , fand2
    , fOr
    , fOr2
    , fOr
    , fwhl */
    } actionType;

static char* ActionAsWord[] =
    { "TheEnd      "
    , "Rslv&Psh    "
    , "Rslv&Get    "
    , "Push        "
    , "Func        "
    , "Branch      "
    , "CondBranch  "
    , "UncondBranch"
    , "NoOp        "
    , "fand        "
    , "fand        "
    , "fOr         "
    , "fOr2        "
    , "fOr         "
    , "fwhl        "
    };

struct forthMemory;

typedef void (*funct)(struct forthMemory* This);
typedef double (*Cfunct1)(double x);
typedef double (*Cfunct2)(double x, double y);

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
    actionType action : 8;
    unsigned int offset : 24;
    union
        {
        double floating; LONG integer; funct funcp; Cfunct1 Cfunc1p; Cfunct2 Cfunc2p; forthvalue* valp; forthvalue val;
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

typedef struct
    {
    char* name;
    funct Cfun;
    }Cpair;

typedef struct
    {
    char* name;
    actionType action;
    }Epair;

typedef struct
    {
    char* name;
    Cfunct1 Cfun;
    }Cpair1;

typedef struct
    {
    char* name;
    Cfunct2 Cfun;
    }Cpair2;

typedef struct
    {
    funct Afun;
    funct Bfun; /* negation of Afun */
    }neg;

static char* getVarName(forthvariable* varp, forthvalue* u)
    {
    for(; varp; varp = varp->next)
        {
        if(&(varp->u.floating) == &(u->floating))
            return varp->name;
        }
    return "UNK variable";
    }

static forthvalue* getVariablePointer(forthvariable** varp, char* name)
    {
    forthvariable* curvarp = *varp;
    while(curvarp != 0 && strcmp(curvarp->name, name))
        {
        curvarp = curvarp->next;
        }
    if(curvarp == 0)
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
    if(is_op(args))
        {
        nr = setArgs(varp, args->LEFT, nr);
        if(nr > 0)
            return setArgs(varp, args->RIGHT, nr);
        }
    else
        {
        static char name[24];/*Enough for 64 bit number in decimal.*/
        forthvalue* val;
        sprintf(name, "a%d", nr);
        val = getVariablePointer(varp, name);
        if(args->v.fl & QDOUBLE)
            {
            val->floating = strtod(&(args->u.sobj), 0);
            return 1 + nr;
            }
        else if(INTEGER(args))
            {
            val->integer = strtol(&(args->u.sobj), 0, 10);
            if(HAS_MINUS_SIGN(args))
                val->integer = -val->integer;
            return 1 + nr;
            }
        }
    printf("setArgs fails\n");
    return -1;
    }

static void justpush(forthMemory* This, forthvalue val)
    {
    assert(This->sp < &(This->stack[0]) + sizeof(This->stack) / sizeof(This->stack[0]));
    (This->sp)->val = val;
    ++(This->sp);
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

static void fless(forthMemory* This)
    {
    double b = cpop(This).floating;
    double a = cpop(This).floating;
    if(a >= b)
        This->wordp = This->word + This->wordp->offset;
    else
        This->wordp += INC;
    }
static void fless_equal(forthMemory* This)
    {
    double b = cpop(This).floating;
    double a = cpop(This).floating;
    if(a > b)
        This->wordp = This->word + This->wordp->offset;
    else
        This->wordp += INC;
    }
static void fmore_equal(forthMemory* This)
    {
    double b = cpop(This).floating;
    double a = cpop(This).floating;
    if(a < b)
        This->wordp = This->word + This->wordp->offset;
    else
        This->wordp += INC;
    }
static void fmore(forthMemory* This)
    {
    double b = cpop(This).floating;
    double a = cpop(This).floating;
    if(a <= b)
        This->wordp = This->word + This->wordp->offset;
    else
        This->wordp += INC;
    }
static void funequal(forthMemory* This)
    {
    double b = cpop(This).floating;
    double a = cpop(This).floating;
    if(a == b)
        This->wordp = This->word + This->wordp->offset;
    else
        This->wordp += INC;
    }
static void flessormore(forthMemory* This)
    {
    double b = cpop(This).floating;
    double a = cpop(This).floating;
    if(a == b)
        This->wordp = This->word + This->wordp->offset;
    else
        This->wordp += INC;
    }
static void fequal(forthMemory* This)
    {
    double b = cpop(This).floating;
    double a = cpop(This).floating;
    if(a != b)
        This->wordp = This->word + This->wordp->offset;
    else
        This->wordp += INC;
    }
static void fnotlessormore(forthMemory* This)
    {
    double b = cpop(This).floating;
    double a = cpop(This).floating;
    if(a != b)
        This->wordp = This->word + This->wordp->offset;
    else
        This->wordp += INC;
    }

static neg negations[] =
    {
        {fless         ,fmore_equal   },
        {fless_equal   ,fmore         },
        {fmore_equal   ,fless         },
        {fmore         ,fless_equal   },
        {funequal      ,fequal        },
        {flessormore   ,fnotlessormore},
        {fequal        ,funequal      },
        {fnotlessormore,flessormore   },
        {0             ,0             }
    };

static funct negated(funct fun)
    {
    int i;
    for(i = 0; negations[i].Afun != 0; ++i)
        if(negations[i].Afun == fun)
            return negations[i].Bfun;
    return 0;
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

static int dumb = 0; /* force compiler to create different functions. Remember to refer to 'dumb' somewhere. */
static void fand(forthMemory* This)
    {
    dumb = 1;
    }

static void fand2(forthMemory* This)
    {
    dumb = 2;
    }

static void fOr(forthMemory* This)
    {
    dumb = 3;
    }

static void fOr2(forthMemory* This)
    {
    dumb = 4;
    }

static void cpush(forthMemory* This, forthvalue val)
    {
    dumb = 5;
    }

static void fwhl(forthMemory* This)
    {
    dumb = 6;
    }

static Cpair pairs[] =
    {
        {"_plus"         ,fplus         },
        {"_times"        ,ftimes        },
        {"_exp"          ,fexp          },
        {"_log"          ,flog          },
        {"_less"         ,fless         },
        {"_less_equal"   ,fless_equal   },
        {"_more_equal"   ,fmore_equal   },
        {"_more"         ,fmore         },
        {"_unequal"      ,funequal      },
        {"_lessormore"   ,flessormore   },
        {"_equal"        ,fequal        },
        {"_notlessormore",fnotlessormore},
        {"_and"          ,fand          },
        {"_and2"         ,fand2         },
        {"_Or"           ,fOr           },
        {"_Or2"          ,fOr2          },
        {"_Or"           ,fOr           },
        {"_whl"          ,fwhl          },
        {0,0}
    };

static Epair epairs[] =
    {
        {"acos",  Acos},
        {"acosh", Acosh},
        {"asin",  Asin},
        {"asinh", Asinh},
        {"atan",  Atan},
        {"atanh", Atanh},
        {"cbrt",  Cbrt},
        {"ceil",  Ceil},
        {"cos",   Cos},
        {"cosh",  Cosh},
        {"exp",   Exp},
        {"fabs",  Fabs},
        {"floor", Floor},
        {"log",   Log},
        {"log10", Log10},
        {"sin",   Sin},
        {"sinh",  Sinh},
        {"sqrt",  Sqrt},
        {"tan",   Tan},
        {"tanh",  Tanh},
        {"atan2", Atan2},
        {"fdim",  Fdim},
        {"fmax",  Fmax},
        {"fmin",  Fmin},
        {"fmod",  Fmod},
        {"hypot", Hypot},
        {"pow",   Pow},
        {0,0}
    };

/*
static Cpair1 Cpairs1[] =
    {
        {"acos",  acos},
        {"acosh", acosh},
        {"asin",  asin},
        {"asinh", asinh},
        {"atan",  atan},
        {"atanh", atanh},
        {"cbrt",  cbrt},
        {"ceil",  ceil},
        {"cos",   cos},
        {"cosh",  cosh},
        {"exp",   exp},
        {"fabs",  fabs},
        {"floor", floor},
        {"log",   log},
        {"log10", log10},
        {"sin",   sin},
        {"sinh",  sinh},
        {"sqrt",  sqrt},
        {"tan",   tan},
        {"tanh",  tanh},
        {0,0}
    };
    */
/*
static Cpair2 Cpairs2[] =
    {
        {"atan2", atan2},
        {"fdim",  fdim},
        {"fmax",  fmax},
        {"fmin",  fmin},
        {"fmod",  fmod},
        {"hypot", hypot},
        {"pow",   pow},
        {0,0}
    };
*/
char* getFuncName(funct funcp)
    {
    Cpair* cpair;
    char* naam = "UNK function";
    static char buffer[64];
    for(cpair = pairs; cpair->name; ++cpair)
        {
        if(cpair->Cfun == funcp)
            {
            naam = cpair->name;
            return naam;
            }
        }
    sprintf(buffer, "UNKNOWN[%p] %d", funcp, dumb);
    return buffer;
    }

/*
char* getCFunc1Name(Cfunct1 funcp)
    {
    Cpair1* cpair;
    char* naam = "UNK function";
    static char buffer[64];
    for(cpair = Cpairs1; cpair->name; ++cpair)
        {
        if(cpair->Cfun == funcp)
            {
            naam = cpair->name;
            return naam;
            }
        }
    sprintf(buffer, "UNKNOWN[%p] %d", funcp, dumb);
    return buffer;
    }

char* getCFunc2Name(Cfunct2 funcp)
    {
    Cpair2* cpair;
    char* naam = "UNK function";
    static char buffer[64];
    for(cpair = Cpairs2; cpair->name; ++cpair)
        {
        if(cpair->Cfun == funcp)
            {
            naam = cpair->name;
            return naam;
            }
        }
    sprintf(buffer, "UNKNOWN[%p] %d", funcp, dumb);
    return buffer;
    }
*/
static Boolean calculate(struct typedObjectnode* This, ppsk arg)
    {
    psk Arg = (*arg)->RIGHT;
    forthMemory* mem = (forthMemory*)(This->voiddata);
    if(setArgs(&(mem->var), Arg, 0) > 0)
        {
        for(mem->wordp = mem->word;
            mem->wordp->action != TheEnd;
            )
            {
            switch(mem->wordp->action)
                {
                case ResolveAndPush:
                    {
                    forthvalue val = *(mem->wordp->u.valp);
                    justpush(mem, val);
                    ++(mem->wordp);
                    break;
                    }
                case ResolveAndGet:
                    {
                    forthvalue* valp = mem->wordp->u.valp;
                    stackvalue* sv = mem->sp;
                    assert(sv > mem->stack);
                    *valp = ((sv - 1)->val);
                    ++(mem->wordp);
                    break;
                    }
                case Push:
                    {
                    forthvalue val = mem->wordp->u.val;
                    justpush(mem, val);
                    ++(mem->wordp);
                    break;
                    }
                case Afunction:
                    {
                    mem->wordp->u.funcp(mem);
                    ++(mem->wordp);
                    break;
                    }
                    /*
                case Cfunction1:
                    {
                    fpush(mem, mem->wordp->u.Cfunc1p(cpop(mem).floating));
                    ++(mem->wordp);
                    break;
                    }
                case Cfunction2:
                    {
                    double a = cpop(mem).floating;
                    double b = cpop(mem).floating;
                    fpush(mem, mem->wordp->u.Cfunc2p(a, b));
                    ++(mem->wordp);
                    break;
                    }*/
                case Abranch:
                    {
                    cpop(mem);
                    ++(mem->wordp);
                    break;
                    }
                case CondBranch:
                    {
                    mem->wordp->u.funcp(mem);
                    break;
                    }
                case UncondBranch:
                    {
                    cpop(mem);
                    mem->wordp = mem->word + mem->wordp->offset;
                    break;
                    }
                    /*case NoOp:
                        {
                        ++(mem->wordp);
                        break;
                        }*/
                case Acos  :  fpush(mem, acos (cpop(mem).floating));++(mem->wordp);break; 
                case Acosh :  fpush(mem, acosh(cpop(mem).floating));++(mem->wordp);break; 
                case Asin  :  fpush(mem, asin (cpop(mem).floating));++(mem->wordp);break; 
                case Asinh :  fpush(mem, asinh(cpop(mem).floating));++(mem->wordp);break; 
                case Atan  :  fpush(mem, atan (cpop(mem).floating));++(mem->wordp);break; 
                case Atanh :  fpush(mem, atanh(cpop(mem).floating));++(mem->wordp);break; 
                case Cbrt  :  fpush(mem, cbrt (cpop(mem).floating));++(mem->wordp);break; 
                case Ceil  :  fpush(mem, ceil (cpop(mem).floating));++(mem->wordp);break; 
                case Cos   :  fpush(mem, cos  (cpop(mem).floating));++(mem->wordp);break; 
                case Cosh  :  fpush(mem, cosh (cpop(mem).floating));++(mem->wordp);break; 
                case Exp   :  fpush(mem, exp  (cpop(mem).floating));++(mem->wordp);break; 
                case Fabs  :  fpush(mem, fabs (cpop(mem).floating));++(mem->wordp);break; 
                case Floor :  fpush(mem, floor(cpop(mem).floating));++(mem->wordp);break; 
                case Log   :  fpush(mem, log  (cpop(mem).floating));++(mem->wordp);break; 
                case Log10 :  fpush(mem, log10(cpop(mem).floating));++(mem->wordp);break; 
                case Sin   :  fpush(mem, sin  (cpop(mem).floating));++(mem->wordp);break; 
                case Sinh  :  fpush(mem, sinh (cpop(mem).floating));++(mem->wordp);break; 
                case Sqrt  :  fpush(mem, sqrt (cpop(mem).floating));++(mem->wordp);break; 
                case Tan   :  fpush(mem, tan  (cpop(mem).floating));++(mem->wordp);break; 
                case Tanh  :  fpush(mem, tanh (cpop(mem).floating));++(mem->wordp);break; 
                case Fdim  :  {double a = cpop(mem).floating;double b = cpop(mem).floating;fpush(mem,fdim (a,b));++(mem->wordp);break; }
                case Fmax  :  {double a = cpop(mem).floating;double b = cpop(mem).floating;fpush(mem,fmax (a,b));++(mem->wordp);break; }
                case Atan2 :  {double a = cpop(mem).floating;double b = cpop(mem).floating;fpush(mem,atan2(a,b));++(mem->wordp);break; }
                case Fmin  :  {double a = cpop(mem).floating;double b = cpop(mem).floating;fpush(mem,fmin (a,b));++(mem->wordp);break; }
                case Fmod  :  {double a = cpop(mem).floating;double b = cpop(mem).floating;fpush(mem,fmod (a,b));++(mem->wordp);break; }
                case Hypot :  {
                    double a = cpop(mem).floating;double b = cpop(mem).floating;
                    fpush(mem,hypot(a,b));
                    ++(mem->wordp);
                    break; }
                case Pow   :  {double a = cpop(mem).floating;double b = cpop(mem).floating;fpush(mem,pow  (a,b));++(mem->wordp);break; }
                case NoOp:
                case TheEnd:
                default:
                    break;
                }
            }
        for(; mem->sp > mem->stack;)
            {
            psk res;
            size_t len;
            char buf[64]; /* 64 bytes is even enough for quad https://people.eecs.berkeley.edu/~wkahan/ieee754status/IEEE754.PDF*/
            double sv = (--(mem->sp))->val.floating;
            int flags;
            if(isnan(sv))
                {
                strcpy(buf, "NAN");
                flags = READY BITWISE_OR_SELFMATCHING;
                }
            else if(isinf(sv))
                {
                if(isinf(sv < 0))
                    strcpy(buf, "-INF");
                else
                    strcpy(buf, "INF");
                flags = READY BITWISE_OR_SELFMATCHING;
                }
            else
                {
                sprintf(buf, "%.16E", sv);
                flags = READY | SUCCESS | QNUMBER | QDOUBLE BITWISE_OR_SELFMATCHING;
                }
            len = offsetof(sk, u.obj) + strlen(buf);
            res = (psk)bmalloc(__LINE__, len + 1);
            strcpy((char*)POBJ(res), buf);
            if(res)
                {
                wipe(*arg);
                *arg = same_as_w(res);
                res->v.fl = flags;
                return TRUE;
                }
            }
        }
    return FALSE;
    }

static Boolean trc(struct typedObjectnode* This, ppsk arg)
    {
    char* naam;
    psk Arg = (*arg)->RIGHT;
    forthMemory* mem = (forthMemory*)(This->voiddata);
    if(setArgs(&(mem->var), Arg, 0) > 0)
        {
        for(mem->wordp = mem->word;
            mem->wordp->action != TheEnd;
            /*++(mem->wordp)*/
            )
            {
            forthvariable* v;
            stackvalue* svp;
            printf("%s %d,%d ", ActionAsWord[mem->wordp->action], (int)(mem->wordp - mem->word), (int)(mem->sp - mem->stack));
            for(v = mem->var; v; v = v->next)
                {
                printf("%s=%.2f ", v->name, v->u.floating);
                };
            for(svp = mem->sp - 1; svp >= mem->stack; --svp)
                {
                if(svp->valp == (forthvalue*)0xCDCDCDCDCDCDCDCD)
                    printf("<undef>");
                else printf("<%.2f>", svp->val.floating/*, svp->valp*/);
                }
            printf("\t");
            switch(mem->wordp->action)
                {
                case ResolveAndPush:
                    {
                    forthvalue val = *(mem->wordp->u.valp);
                    printf("%s %.2f --> stack", getVarName(mem->var, (mem->wordp->u.valp)), val.floating);
                    justpush(mem, val);
                    ++(mem->wordp);
                    break;
                    }
                case ResolveAndGet:
                    {
                    forthvalue* valp = mem->wordp->u.valp;
                    stackvalue* sv = mem->sp;
                    assert(sv > mem->stack);
                    printf("%s %.2f <-- stack", getVarName(mem->var, valp), ((sv - 1)->val).floating);
                    *valp = ((sv - 1)->val);
                    ++(mem->wordp);
                    break;
                    }
                case Push:
                    {
                    forthvalue val = mem->wordp->u.val;
                    printf("%.2f", val.floating);
                    justpush(mem, val);
                    ++(mem->wordp);
                    break;
                    }
                case Afunction:
                    {
                    naam = getFuncName(mem->wordp->u.funcp);
                    printf(" %s", naam);
                    mem->wordp->u.funcp(mem);
                    ++(mem->wordp);
                    break;
                    }
                    /*
                case Cfunction1:
                    {
                    naam = getCFunc1Name(mem->wordp->u.Cfunc1p);
                    printf(" %s", naam);
                    fpush(mem, mem->wordp->u.Cfunc1p(cpop(mem).floating));
                    ++(mem->wordp);
                    break;
                    }
                case Cfunction2:
                    {
                    double a = cpop(mem).floating;
                    double b = cpop(mem).floating;
                    naam = getCFunc2Name(mem->wordp->u.Cfunc2p);
                    printf(" %s", naam);
                    fpush(mem, mem->wordp->u.Cfunc2p(a, b));
                    ++(mem->wordp);
                    break;
                    }*/
                case Abranch:
                    {
                    naam = getFuncName(mem->wordp->u.funcp);
                    printf(" %s", naam);
                    printf(" conditional jump to %u", mem->wordp->offset);
                    cpop(mem);
                    ++(mem->wordp);
                    break;
                    }
                case CondBranch:
                    {
                    printf("CONDBRANCH\n");
                    naam = getFuncName(mem->wordp->u.funcp);
                    printf(" %s", naam);
                    printf(" test and jump on failure to %u", mem->wordp->offset);
                    mem->wordp->u.funcp(mem);
                    break;
                    }
                case UncondBranch:
                    {
                    naam = getFuncName(mem->wordp->u.funcp);
                    printf(" %s", naam);
                    printf(" unconditional jump to %u", mem->wordp->offset);
                    cpop(mem);
                    mem->wordp = mem->word + mem->wordp->offset;
                    break;
                    }
                case Acos  :  printf("acos  ");fpush(mem, acos (cpop(mem).floating));++(mem->wordp);break; 
                case Acosh :  printf("acosh ");fpush(mem, acosh(cpop(mem).floating));++(mem->wordp);break; 
                case Asin  :  printf("asin  ");fpush(mem, asin (cpop(mem).floating));++(mem->wordp);break; 
                case Asinh :  printf("asinh ");fpush(mem, asinh(cpop(mem).floating));++(mem->wordp);break; 
                case Atan  :  printf("atan  ");fpush(mem, atan (cpop(mem).floating));++(mem->wordp);break; 
                case Atanh :  printf("atanh ");fpush(mem, atanh(cpop(mem).floating));++(mem->wordp);break; 
                case Cbrt  :  printf("cbrt  ");fpush(mem, cbrt (cpop(mem).floating));++(mem->wordp);break; 
                case Ceil  :  printf("ceil  ");fpush(mem, ceil (cpop(mem).floating));++(mem->wordp);break; 
                case Cos   :  printf("cos   ");fpush(mem, cos  (cpop(mem).floating));++(mem->wordp);break; 
                case Cosh  :  printf("cosh  ");fpush(mem, cosh (cpop(mem).floating));++(mem->wordp);break; 
                case Exp   :  printf("exp   ");fpush(mem, exp  (cpop(mem).floating));++(mem->wordp);break; 
                case Fabs  :  printf("fabs  ");fpush(mem, fabs (cpop(mem).floating));++(mem->wordp);break; 
                case Floor :  printf("floor ");fpush(mem, floor(cpop(mem).floating));++(mem->wordp);break; 
                case Log   :  printf("log   ");fpush(mem, log  (cpop(mem).floating));++(mem->wordp);break; 
                case Log10 :  printf("log10 ");fpush(mem, log10(cpop(mem).floating));++(mem->wordp);break; 
                case Sin   :  printf("sin   ");fpush(mem, sin  (cpop(mem).floating));++(mem->wordp);break; 
                case Sinh  :  printf("sinh  ");fpush(mem, sinh (cpop(mem).floating));++(mem->wordp);break; 
                case Sqrt  :  printf("sqrt  ");fpush(mem, sqrt (cpop(mem).floating));++(mem->wordp);break; 
                case Tan   :  printf("tan   ");fpush(mem, tan  (cpop(mem).floating));++(mem->wordp);break; 
                case Tanh  :  printf("tanh  ");fpush(mem, tanh (cpop(mem).floating));++(mem->wordp);break; 
                case Fdim  : {double a = cpop(mem).floating;double b = cpop(mem).floating;printf("fdim  ");fpush(mem,fdim (a,b));++(mem->wordp);break; }
                case Fmax  : {double a = cpop(mem).floating;double b = cpop(mem).floating;printf("fmax  ");fpush(mem,fmax (a,b));++(mem->wordp);break; }
                case Atan2 : {double a = cpop(mem).floating;double b = cpop(mem).floating;printf("atan2 ");fpush(mem,atan2(a,b));++(mem->wordp);break; }
                case Fmin  : {double a = cpop(mem).floating;double b = cpop(mem).floating;printf("fmin  ");fpush(mem,fmin (a,b));++(mem->wordp);break; }
                case Fmod  : {double a = cpop(mem).floating;double b = cpop(mem).floating;printf("fmod  ");fpush(mem,fmod (a,b));++(mem->wordp);break; }
                case Hypot : {double a = cpop(mem).floating;double b = cpop(mem).floating;printf("hypot ");fpush(mem,hypot(a,b));++(mem->wordp);break; }
                case Pow   : {double a = cpop(mem).floating;double b = cpop(mem).floating;printf("pow   ");fpush(mem,pow  (a,b));++(mem->wordp);break; }
                    /*
                case NoOp:
                    {
                    naam = getFuncName(mem->wordp->u.funcp);
                    printf(" %s", naam);
                    printf(" NoOp");
                    ++(mem->wordp);
                    break;
                    }*/
                case NoOp:
                case TheEnd:
                default:
                    ;
                }
#if 0
            if(getchar() == 'q')
                break;
#else
            printf("\n");
#endif
            }
        printf("calculation DONE. On Stack %d\n", (int)(mem->sp - mem->stack));
        for(; mem->sp > mem->stack;)
            {
            psk res;
            size_t len;
            char buf[64]; /* 64 bytes is even enough for quad https://people.eecs.berkeley.edu/~wkahan/ieee754status/IEEE754.PDF*/
            double sv = (--(mem->sp))->val.floating;
            int flags;
            if(isnan(sv))
                {
                strcpy(buf, "NAN");
                flags = READY BITWISE_OR_SELFMATCHING;
                }
            else if(isinf(sv))
                {
                if(isinf(sv < 0))
                    strcpy(buf, "-INF");
                else
                    strcpy(buf, "INF");
                flags = READY BITWISE_OR_SELFMATCHING;
                }
            else
                {
                sprintf(buf, "%.16E", sv);
                flags = READY | SUCCESS | QNUMBER | QDOUBLE BITWISE_OR_SELFMATCHING;
                }
            len = offsetof(sk, u.obj) + strlen(buf);
            res = (psk)bmalloc(__LINE__, len + 1);
            strcpy((char*)POBJ(res), buf);
            printf("value on stack %s\n", buf);
            if(res)
                {
                wipe(*arg);
                *arg = same_as_w(res);
                res->v.fl = flags;
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
    /*printf("\npolish1{"); result(code); printf("}\n");*/
    switch(Op(code))
        {
        case PLUS:
        case TIMES:
        case EXP:
        case LOG:
            {
            R = polish1(code->LEFT);
            if(R == -1)
                return -1;
            C = polish1(code->RIGHT);
            if(C == -1)
                return -1;
            return 1 + R + C;
            }
        case AND:
            {
            R = polish1(code->LEFT);
            if(R == -1)
                return -1;
            C = polish1(code->RIGHT);
            if(C == -1)
                return -1;
            return 1 + R + C; /* one for AND (containing address to 'if false' branch). */
            }
        case OR:
            {
            R = polish1(code->LEFT);
            if(R == -1)
                return -1;
            C = polish1(code->RIGHT);
            if(C == -1)
                return -1;
            return 1 + R + C; /* one for OR, containing address to 'if true' branch */
            }
        case MATCH:
            {
            R = polish1(code->LEFT);
            if(R == -1)
                return -1;
            C = polish1(code->RIGHT);
            if(C == -1)
                return -1;
            if(Op(code->RIGHT) == MATCH || code->RIGHT->v.fl & UNIFY)
                return R + C;
            else
                return 1 + R + C;
            }
        case FUN:
        case FUU:
            if(is_op(code->LEFT))
                {
                printf("lhs of $ or ' is operator\n");
                return -1;
                }
            C = polish1(code->RIGHT);
            if(C == -1)
                return -1;
            return 1 + C;
        default:
            if(is_op(code))
                {
                R = polish1(code->LEFT);
                if(R == -1)
                    return -1;
                C = polish1(code->RIGHT);
                if(C == -1)
                    return -1;
                return R + C; /* Do not reserve room for operator! */
                }
            else
                {
                if(code->v.fl & QDOUBLE || INTEGER(code))
                    return 1;
                else if(code->v.fl & (UNIFY | INDIRECT))
                    return 1; /* variable */
                else
                    {
                    printf("Not parsed: [%s]\n", &(code->u.sobj));
                    return -1;
                    }
                }
        }
    }

static void ipush(forthMemory* This, LONG val)
    {
    assert(This->sp < &(This->stack[0]) + sizeof(This->stack) / sizeof(This->stack[0]));
    (This->sp)->val.integer = val;
    ++(This->sp);
    }

static forthvalue* pcpop(forthMemory* This)
    {
    assert(This->sp > This->stack);
    --(This->sp);
    return (This->sp)->valp;
    }

static void pcpush(forthMemory* This, forthvalue* valp)
    {
    assert(This->sp < &(This->stack[0]) + sizeof(This->stack) / sizeof(This->stack[0]));
    (This->sp)->valp = valp;
    ++(This->sp);
    }

static Boolean print(struct typedObjectnode* This, ppsk arg)
    {
    char* naam;
    forthMemory* mem = (forthMemory*)(This->voiddata);
    forthword* wordp = mem->word;
    printf("print\n");
    for(;
        wordp->action != TheEnd;
        ++wordp
        )
        {
        switch(wordp->action)
            {
            case ResolveAndPush:
                {
                forthvalue val = *(wordp->u.valp);
                printf(LONGD " ResolveAndPush  %s %f\n", wordp - mem->word, getVarName(mem->var, (wordp->u.valp)), val.floating);
                break;
                }
            case ResolveAndGet:
                {
                forthvalue val = *(wordp->u.valp);
                printf(LONGD " ResolveAndGet   %s %f\n", wordp - mem->word, getVarName(mem->var, (wordp->u.valp)), val.floating);
                break;
                }
            case Push:
                {
                forthvalue val = wordp->u.val;
                printf(LONGD " Push              %f\n", wordp - mem->word, val.floating);
                break;
                }
            case Afunction:
                {
                naam = getFuncName(wordp->u.funcp);
                printf(LONGD " Afunction       %s %u\n", wordp - mem->word, naam, wordp->offset);
                break;
                }
            case Abranch:
                {
                naam = getFuncName(wordp->u.funcp);
                printf(LONGD " Pop             %s %u\n", wordp - mem->word, naam, wordp->offset);
                break;
                }
            case CondBranch:
                {
                naam = getFuncName(wordp->u.funcp);
                printf(LONGD " Pop2 CondBranch %s %u\n", wordp - mem->word, naam, wordp->offset);
                break;
                }
            case UncondBranch:
                {
                naam = getFuncName(wordp->u.funcp);
                printf(LONGD " Pop UnconBranch %s %u\n", wordp - mem->word, naam, wordp->offset);
                break;
                }
            case Acos  :  printf(LONGD " acos               %u\n", wordp - mem->word, wordp->offset);break; 
            case Acosh :  printf(LONGD " acosh              %u\n", wordp - mem->word, wordp->offset);break; 
            case Asin  :  printf(LONGD " asin               %u\n", wordp - mem->word, wordp->offset);break; 
            case Asinh :  printf(LONGD " asinh              %u\n", wordp - mem->word, wordp->offset);break; 
            case Atan  :  printf(LONGD " atan               %u\n", wordp - mem->word, wordp->offset);break; 
            case Atanh :  printf(LONGD " atanh              %u\n", wordp - mem->word, wordp->offset);break; 
            case Cbrt  :  printf(LONGD " cbrt               %u\n", wordp - mem->word, wordp->offset);break; 
            case Ceil  :  printf(LONGD " ceil               %u\n", wordp - mem->word, wordp->offset);break; 
            case Cos   :  printf(LONGD " cos                %u\n", wordp - mem->word, wordp->offset);break; 
            case Cosh  :  printf(LONGD " cosh               %u\n", wordp - mem->word, wordp->offset);break; 
            case Exp   :  printf(LONGD " exp                %u\n", wordp - mem->word, wordp->offset);break; 
            case Fabs  :  printf(LONGD " fabs               %u\n", wordp - mem->word, wordp->offset);break; 
            case Floor :  printf(LONGD " floor              %u\n", wordp - mem->word, wordp->offset);break; 
            case Log   :  printf(LONGD " log                %u\n", wordp - mem->word, wordp->offset);break; 
            case Log10 :  printf(LONGD " log10              %u\n", wordp - mem->word, wordp->offset);break; 
            case Sin   :  printf(LONGD " sin                %u\n", wordp - mem->word, wordp->offset);break; 
            case Sinh  :  printf(LONGD " sinh               %u\n", wordp - mem->word, wordp->offset);break; 
            case Sqrt  :  printf(LONGD " sqrt               %u\n", wordp - mem->word, wordp->offset);break; 
            case Tan   :  printf(LONGD " tan                %u\n", wordp - mem->word, wordp->offset);break; 
            case Tanh  :  printf(LONGD " tanh               %u\n", wordp - mem->word, wordp->offset);break; 
            case Fdim  :  printf(LONGD " fdim               %u\n", wordp - mem->word, wordp->offset);break;
            case Fmax  :  printf(LONGD " fmax               %u\n", wordp - mem->word, wordp->offset);break;
            case Atan2 :  printf(LONGD " atan2              %u\n", wordp - mem->word, wordp->offset);break;
            case Fmin  :  printf(LONGD " fmin               %u\n", wordp - mem->word, wordp->offset);break;
            case Fmod  :  printf(LONGD " fmod               %u\n", wordp - mem->word, wordp->offset);break;
            case Hypot :  printf(LONGD " hypot              %u\n", wordp - mem->word, wordp->offset);break;
            case Pow   :  printf(LONGD " pow                %u\n", wordp - mem->word, wordp->offset);break;
            case NoOp:
                {
                naam = getFuncName(wordp->u.funcp);
                printf(LONGD " NoOp       %s\n", wordp - mem->word, naam);
                break;
                }
            case TheEnd:
            default:
                printf(LONGD " default         %d\n", wordp - mem->word, wordp->action);
                ;
            }
        }
    printf(LONGD " TheEnd          \n", wordp - mem->word);
    return TRUE;
    }

static void optimizeJumps(forthMemory* mem)
    {
    forthword* wordp = mem->word;

    for(;
        wordp->action != TheEnd;
        ++wordp
        )
        {
        switch(wordp->action)
            {
            case ResolveAndPush:
            case ResolveAndGet:
            case Push:
                wordp->offset = 0;
                break;
            case Afunction:
                if(wordp->u.funcp == fless
                   || wordp->u.funcp == fless_equal
                   || wordp->u.funcp == fmore_equal
                   || wordp->u.funcp == fmore
                   || wordp->u.funcp == funequal
                   || wordp->u.funcp == flessormore
                   || wordp->u.funcp == fequal
                   || wordp->u.funcp == fnotlessormore
                   )
                    {
                    ;
                    }
                break;
            case Abranch:
                {
                if(wordp->u.funcp == fand)
                    {
                    forthword* label;
                    while(1)
                        {
                        label = mem->word + wordp->offset; /* Address to jump to if previous failed. */
                        if(label->u.funcp == fand)  /* (((FAIL&BLA)&K)&L) */
                            wordp->offset = label->offset;
                        else
                            break;
                        }
                    label = mem->word + wordp->offset; /* Address to jump to if previous failed. */
                    if(label->u.funcp == fOr)    /* (FAIL&BLA)|X             If FAIL jump to X */
                        {
                        ++(wordp->offset);       /* offset now points to X */
                        wordp->u.funcp = fand2;  /* removes top from stack */
                        }
                    else if(label->u.funcp == fwhl)  /* whl'(FAIL&BLA) */
                        {
                        wordp->u.funcp = fand2; /* removes top from stack */
                        label = mem->word + wordp->offset + 1; /* label == The address after the loop. */
                        if(label->u.funcp == fand)             /* whl'(FAIL&BLA)&X            Jump to X for leaving the loop.                             */
                            (wordp->offset) += 2;              /* 1 for the end of the loop, 1 for the AND */
                        else if(label->u.funcp == fOr        /* whl'(FAIL&BLA)|X            From FAIL jump to the offset of the OR for leaving the loop. X is unreachable! */
                                || label->u.funcp == fwhl      /* whl'(ABC&whl'(FAIL&BLB))    From FAIL jump to the offset of the WHL, i.e. the start of the outer loop.                      */
                                )
                            {
                            wordp->offset = label->offset;
                            }
                        }
                    }
                else if(wordp->u.funcp == fOr)
                    {
                    forthword* label;
                    while(1)
                        {
                        label = mem->word + wordp->offset; /* Address to jump to if previous was OK. */
                        if(label->u.funcp == fOr)  /* (((OK|BLA)|K)|L) */
                            wordp->offset = label->offset;
                        else
                            break;
                        }
                    label = mem->word + wordp->offset; /* Address to jump to if previous failed. */
                    if(label->u.funcp == fand)    /* (OK|BLA)&X             If OK jump to X */
                        {
                        ++(wordp->offset);       /* skip AND, offset now points to X */
                        wordp->u.funcp = fOr2;   /* always removes top from stack */
                        }
                    else if(label->u.funcp == fwhl)  /* whl'(OK|BLA) */
                        {
                        wordp->u.funcp = fOr2; /* removes top from stack */
                        wordp->offset = label->offset;
                        }
                    }
                break;
                }
            case CondBranch:
            case UncondBranch:
            case Acos  : 
            case Acosh : 
            case Asin  : 
            case Asinh : 
            case Atan  : 
            case Atanh : 
            case Cbrt  : 
            case Ceil  : 
            case Cos   : 
            case Cosh  : 
            case Exp   : 
            case Fabs  : 
            case Floor : 
            case Log   : 
            case Log10 : 
            case Sin   : 
            case Sinh  : 
            case Sqrt  : 
            case Tan   : 
            case Tanh  : 
            case Fdim  :
            case Fmax  :
            case Atan2 :
            case Fmin  :
            case Fmod  :
            case Hypot :
            case Pow   :
            case NoOp:
                {
                break;
                }
            default:
                ;
            }
        }
    }

static void combineTestsAndJumps(forthMemory* mem)
    {
    forthword* wordp;
    forthword* label;

    for(wordp = mem->word; wordp->action != TheEnd; ++wordp)
        {
        if(wordp->action == Abranch)
            {
            if(wordp->u.funcp == fwhl)
                wordp->action = UncondBranch;
            }
        else if(wordp->action == Afunction)
            {
            if(wordp->u.funcp == fless
               || wordp->u.funcp == fless_equal
               || wordp->u.funcp == fmore_equal
               || wordp->u.funcp == fmore
               || wordp->u.funcp == funequal
               || wordp->u.funcp == flessormore
               || wordp->u.funcp == fequal
               || wordp->u.funcp == fnotlessormore
               )
                {
                label = wordp + 1;
                if(label->action == Abranch)
                    {
                    wordp->action = CondBranch;
                    wordp->offset = label->offset;
                    label->action = NoOp;
                    if(label->u.funcp == fOr
                       || label->u.funcp == fOr2
                       || label->u.funcp == fwhl
                       )
                        {
                        wordp->u.funcp = negated(wordp->u.funcp);
                        }
                    }
                }
            }
        }
    }

#if COMPACT
static void compaction(forthMemory* mem)
    {
    forthword* wordp;
    forthword* newword;
    forthword* newwordp;
    int cells = 0;
    int* arr;
    int skipping = 0;
    for(wordp = mem->word; wordp->action != TheEnd; ++wordp)
        {
        ++cells;
        }
    ++cells;
    arr = (int*)bmalloc(__LINE__, sizeof(int) * cells + 10);
    cells = 0;
    for(wordp = mem->word; wordp->action != TheEnd; ++wordp)
        {
        arr[cells] = skipping;
        if(wordp->action != NoOp)
            ++skipping;
        ++cells;
        }
    arr[cells] = skipping;
    if(skipping == 0)
        return;
    newword = bmalloc(__LINE__, skipping * sizeof(forthword) + 10);

    cells = 0;
    for(wordp = mem->word, newwordp = newword; wordp->action != TheEnd; ++wordp)
        {
        if(wordp->action != NoOp)
            {
            *newwordp = *wordp;
            /*if(wordp->offset >= 0) always true*/
                newwordp->offset = arr[wordp->offset];
            /*else
                newwordp->offset = 0;*/
            ++newwordp;
            }
        ++cells;
        }
    *newwordp = *wordp;
    /*if(wordp->offset >= 0) always true */
        newwordp->offset = arr[wordp->offset];
    bfree(mem->word);
    mem->word = newword;
    }
#endif

static forthword* polish2(forthvariable** varp, psk code, forthword* wordp, forthword* word)
    {
    switch(Op(code))
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
            saveword->action = Abranch;
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
            saveword->action = Abranch;
            saveword->u.funcp = fOr;
            wordp = polish2(varp, code->RIGHT, ++wordp, word);
            saveword->offset = (unsigned int)(ULONG)(wordp - word); /* the address of the word after the 'if false' branch.*/
            return wordp;
            }
        case MATCH:
            {
            wordp = polish2(varp, code->LEFT, wordp, word);
            wordp = polish2(varp, code->RIGHT, wordp, word);
            if(!(code->RIGHT->v.fl & UNIFY))
                {
                if(FLESS(code->RIGHT))
                    {
                    wordp->action = Afunction;
                    wordp->offset = 0;
                    wordp->u.funcp = fless;
                    return ++wordp;
                    }
                else if(FLESS_EQUAL(code->RIGHT))
                    {
                    wordp->action = Afunction;
                    wordp->offset = 0;
                    wordp->u.funcp = fless_equal;
                    return ++wordp;
                    }
                else if(FMORE_EQUAL(code->RIGHT))
                    {
                    wordp->action = Afunction;
                    wordp->offset = 0;
                    wordp->u.funcp = fmore_equal;
                    return ++wordp;
                    }
                else if(FMORE(code->RIGHT))
                    {
                    wordp->action = Afunction;
                    wordp->offset = 0;
                    wordp->u.funcp = fmore;
                    return ++wordp;
                    }
                else if(FUNEQUAL(code->RIGHT))
                    {
                    wordp->action = Afunction;
                    wordp->offset = 0;
                    wordp->u.funcp = funequal;
                    return ++wordp;
                    }
                else if(FLESSORMORE(code->RIGHT))
                    {
                    wordp->action = Afunction;
                    wordp->offset = 0;
                    wordp->u.funcp = flessormore;
                    return ++wordp;
                    }
                else if(FEQUAL(code->RIGHT))
                    {
                    wordp->action = Afunction;
                    wordp->offset = 0;
                    wordp->u.funcp = fequal;
                    return ++wordp;
                    }
                else if(FNOTLESSORMORE(code->RIGHT))
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
            if(strcmp(name, "whl"))
                return 0;
            wordp = polish2(varp, code->RIGHT, wordp, word);
            wordp->action = Abranch;
            wordp->u.funcp = fwhl;
            wordp->offset = here; /* If all good, jump back to start of loop */
            return ++wordp;;
            }
        case FUN:
            {
            Epair* ep = epairs;
            Cpair* p = pairs;
            char* name = &code->LEFT->u.sobj;
            wordp = polish2(varp, code->RIGHT, wordp, word);
            wordp->offset = 0;
            for(; ep->name != 0; ++ep)
                {
                if(!strcmp(ep->name, name))
                    {
                    wordp->action = ep->action;
                    return ++wordp;
                    }
                }

            wordp->action = Afunction;
            for(; p->name != 0; ++p)
                {
                if(!strcmp(p->name, name))
                    {
                    wordp->u.funcp = p->Cfun;
                    return ++wordp;
                    }
                }
            return 0;
            }
        default:
            if(is_op(code))
                {
                wordp = polish2(varp, code->LEFT, wordp, word);
                wordp = polish2(varp, code->RIGHT, wordp, word);
                return wordp;
                }
            else
                {
                if(INTEGER(code))
                    {
                    wordp->u.integer = (int)STRTOL(&(code->u.sobj), 0, 10);
                    if(HAS_MINUS_SIGN(code))
                        {
                        wordp->u.integer = -(wordp->u.integer);
                        }
                    wordp->action = Push;
                    /*When executing, push number onto the data stack*/
                    }
                else if(code->v.fl & QDOUBLE)
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
    Nan = log(0.0);
    /*    printf("\ncalculationnew{"); result(code); printf("}\n");*/
    if(is_object(code))
        code = code->RIGHT;
    length = polish1(code);
    if(length < 0)
        return FALSE;
    This->voiddata = bmalloc(__LINE__, sizeof(forthMemory));
    forthstuff = (forthMemory*)(This->voiddata);
    forthstuff->var = 0;
    forthstuff->word = bmalloc(__LINE__, length * sizeof(forthword) + 1);
    forthstuff->wordp = forthstuff->word;
    forthstuff->sp = forthstuff->stack;
    lastword = polish2(&(forthstuff->var), code, forthstuff->wordp, forthstuff->word);
    lastword->action = TheEnd;
    optimizeJumps(forthstuff);
    combineTestsAndJumps(forthstuff);
#if COMPACT
    compaction(forthstuff);
#endif
    /*printf("allocated for %d words\nActual number of words %d\n", length + 1, (int)(lastword - forthstuff->word) + 1);*/
    return TRUE;
    }

static Boolean calculationdie(struct typedObjectnode* This, ppsk arg)
    {
    forthvariable* curvarp = ((forthMemory*)(This->voiddata))->var;
    while(curvarp)
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
    {"trc",trc},
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

