#include "calculation.h"
#include "nodedefs.h"
#include "memory.h"
#include "wipecopy.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "result.h" /* For debugging. Remove when done. */


#define CFUNCS 0

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

typedef enum 
    { TheEnd
    , ResolveAndPush
    , ResolveAndGet
    , Push
    , Afunction
    , Pop
    , PopUncondBranch
    , Fless
    , Fless_equal
    , Fmore_equal
    , Fmore
    , Funequal
    , Fequal
    , Plus
    , Times
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
    , Cfunction1
    , Cfunction2
    */
    } actionType;

static char* ActionAsWord[] =
    { "TheEnd          "
    , "ResolveAndPush  "
    , "ResolveAndGet   "
    , "Push            "
    , "Afunction       "
    , "Pop             "
    , "PopUncondBranch "
    , "Fless           "
    , "Fless_equal     "
    , "Fmore_equal     "
    , "Fmore           "
    , "Funequal        "
    , "Fequal          "
    , "Plus            "
    , "Times           "
    , "Acos            "
    , "Acosh           "
    , "Asin            "
    , "Asinh           "
    , "Atan            "
    , "Atanh           "
    , "Cbrt            "
    , "Ceil            "
    , "Cos             "
    , "Cosh            "
    , "Exp             "
    , "Fabs            "
    , "Floor           "
    , "Log             "
    , "Log10           "
    , "Sin             "
    , "Sinh            "
    , "Sqrt            "
    , "Tan             "
    , "Tanh            "
    , "Atan2           "
    , "Fdim            "
    , "Fmax            "
    , "Fmin            "
    , "Fmod            "
    , "Hypot           "
    , "Pow             "
    , "NoOp            "
    /*
    , Cfunction1
    , Cfunction2
    */
    };

struct forthMemory;

typedef void (*funct)(struct forthMemory* This);
#if CFUNCS
typedef double (*Cfunct1)(double x);
typedef double (*Cfunct2)(double x, double y);
#endif

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
    forthvalue val;
    char* name;
    struct forthvariable* next;
    } forthvariable;

typedef enum {
    fand, fand2, fOr, fOr2, cpush, fwhl
    } dumbl;

static char* logics[] =
    { "fand "
    , "fand2"
    , "fOr  "
    , "fOr2 "
    , "cpush"
    , "fwhl "
    };

typedef struct forthword
    {
    actionType action : 8;
    unsigned int offset : 24;
    union
        {
        funct funcp; forthvalue* valp; forthvalue val; dumbl logic;
#if CFUNCS
        Cfunct1 Cfunc1p; Cfunct2 Cfunc2p;
#endif        
        } u;
    } forthword;

typedef struct forthMemory
    {
    forthword* word; /* fixed once calculation is compiled */
    forthword* wordp; /* runs through words when calculating */
    forthvariable* var;
    stackvalue* sp;
    stackvalue stack[64];
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

#if CFUNCS
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
#endif

typedef struct
    {
    actionType Afun;
    actionType Bfun; /* negation of Afun */
    }neg;

static char* getVarName(forthvariable* varp, forthvalue* val)
    {
    for(; varp; varp = varp->next)
        {
        if(&(varp->val.floating) == &(val->floating))
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
    return &(curvarp->val);
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

static neg negations[] =
    {
        {Fless         ,Fmore_equal   },
        {Fless_equal   ,Fmore         },
        {Fmore_equal   ,Fless         },
        {Fmore         ,Fless_equal   },
        {Funequal      ,Fequal        },
        {Fequal        ,Funequal      },
        {TheEnd        ,TheEnd        }
    };

static actionType negated(actionType fun)
    {
    int i;
    for(i = 0; negations[i].Afun != TheEnd; ++i)
        if(negations[i].Afun == fun)
            return negations[i].Bfun;
    return TheEnd;
    }



static Cpair pairs[] =
    {
        {0,0}
    };

static Epair epairs[] =
    {
        {"_less"         ,Fless         },
        {"_less_equal"   ,Fless_equal   },
        {"_more_equal"   ,Fmore_equal   },
        {"_more"         ,Fmore         },
        {"_unequal"      ,Funequal      },
        {"_equal"        ,Fequal        },
        {"plus",  Plus},
        {"times", Times},
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

#if CFUNCS
static Cpair1 Cpairs1[] =
    {
        {0,0}
    };
    
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
#endif

static char* getLogiName(dumbl logi)
    {
    return logics[logi];
    }

static char* getFuncName(funct funcp)
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
    sprintf(buffer, "UNKNOWN");
    return buffer;
    }

#if CFUNCS
static char* getCFunc1Name(Cfunct1 funcp)
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

static char* getCFunc2Name(Cfunct2 funcp)
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
#endif

static Boolean calculate(struct typedObjectnode* This, ppsk arg)
    {
    psk Arg = (*arg)->RIGHT;
    forthMemory* mem = (forthMemory*)(This->voiddata);
    forthword* word = mem->word;
    forthword* wordp = mem->wordp;
    stackvalue* sp = mem->sp - 1;
    if(setArgs(&(mem->var), Arg, 0) > 0)
        {
        for(wordp = word;
            wordp->action != TheEnd;
            )
            {
            double a;
            double b;
            switch(wordp->action)
                {
                case ResolveAndPush:
                    {
                    (++sp)->val = *(wordp++->u.valp);
                    break;
                    }
                case ResolveAndGet:
                    {
                    assert(sp >= mem->stack);
                    *(wordp++->u.valp) = sp->val;
                    break;
                    }
                case Push:
                    {
                    (++sp)->val = wordp++->u.val;
                    break;
                    }
                case Afunction:
                    {
                    mem->wordp = wordp;
                    mem->sp = sp;
                    wordp->u.funcp(mem);
                    wordp = mem->wordp + 1;
                    sp = mem->sp;
                    break;
                    }
#if CFUNCS
                case Cfunction1:
                    {
                    sp->val.floating = wordp->u.Cfunc1p((sp->val).floating);
                    ++wordp;
                    break;
                    }
                case Cfunction2:
                    {
                    double a = ((sp--)->val).floating;
                    double b = (sp->val).floating;
                    sp->val.floating = wordp->u.Cfunc2p(a, b);
                    ++wordp;
                    break;
                    }
#endif
                case Pop:
                    {
                    --sp;
                    ++wordp;
                    break;
                    }
                case PopUncondBranch:
                    {
                    --sp;
                    wordp = word + wordp->offset;
                    break;
                    }

                case Fless          : b = ((sp--)->val).floating; if( ((sp--)->val).floating >= b) wordp = word + wordp->offset; else ++wordp; break;
                case Fless_equal    : b = ((sp--)->val).floating; if( ((sp--)->val).floating >  b) wordp = word + wordp->offset; else ++wordp; break;
                case Fmore_equal    : b = ((sp--)->val).floating; if( ((sp--)->val).floating <  b) wordp = word + wordp->offset; else ++wordp; break;
                case Fmore          : b = ((sp--)->val).floating; if( ((sp--)->val).floating <= b) wordp = word + wordp->offset; else ++wordp; break;
                case Funequal       : b = ((sp--)->val).floating; if( ((sp--)->val).floating == b) wordp = word + wordp->offset; else ++wordp; break;
                case Fequal         : b = ((sp--)->val).floating; if( ((sp--)->val).floating != b) wordp = word + wordp->offset; else ++wordp; break;

                case Plus  :  a = ((sp--)->val).floating; sp->val.floating += a; ++wordp; break;
                case Times :  a = ((sp--)->val).floating; sp->val.floating *= a; ++wordp; break;

                case Acos  :  sp->val.floating = acos ((sp->val).floating);++wordp;break; 
                case Acosh :  sp->val.floating = acosh((sp->val).floating);++wordp;break; 
                case Asin  :  sp->val.floating = asin ((sp->val).floating);++wordp;break; 
                case Asinh :  sp->val.floating = asinh((sp->val).floating);++wordp;break; 
                case Atan  :  sp->val.floating = atan ((sp->val).floating);++wordp;break; 
                case Atanh :  sp->val.floating = atanh((sp->val).floating);++wordp;break; 
                case Cbrt  :  sp->val.floating = cbrt ((sp->val).floating);++wordp;break; 
                case Ceil  :  sp->val.floating = ceil ((sp->val).floating);++wordp;break; 
                case Cos   :  sp->val.floating = cos  ((sp->val).floating);++wordp;break; 
                case Cosh  :  sp->val.floating = cosh ((sp->val).floating);++wordp;break; 
                case Exp   :  sp->val.floating = exp  ((sp->val).floating);++wordp;break; 
                case Fabs  :  sp->val.floating = fabs ((sp->val).floating);++wordp;break; 
                case Floor :  sp->val.floating = floor((sp->val).floating);++wordp;break; 
                case Log   :  sp->val.floating = log  ((sp->val).floating);++wordp;break; 
                case Log10 :  sp->val.floating = log10((sp->val).floating);++wordp;break; 
                case Sin   :  sp->val.floating = sin  ((sp->val).floating);++wordp;break; 
                case Sinh  :  sp->val.floating = sinh ((sp->val).floating);++wordp;break; 
                case Sqrt  :  sp->val.floating = sqrt ((sp->val).floating);++wordp;break; 
                case Tan   :  sp->val.floating = tan  ((sp->val).floating);++wordp;break; 
                case Tanh  :  sp->val.floating = tanh ((sp->val).floating);++wordp;break; 
                case Fmax  :  a = ((sp--)->val).floating; sp->val.floating = fmax (a,(sp->val).floating);++wordp;break;
                case Atan2 :  a = ((sp--)->val).floating; sp->val.floating = atan2(a,(sp->val).floating);++wordp;break;
                case Fmin  :  a = ((sp--)->val).floating; sp->val.floating = fmin (a,(sp->val).floating);++wordp;break;
                case Fdim  :  a = ((sp--)->val).floating; sp->val.floating = fdim (a,(sp->val).floating);++wordp;break;
                case Fmod  :  a = ((sp--)->val).floating; sp->val.floating = fmod (a,(sp->val).floating);++wordp;break;
                case Hypot :  a = ((sp--)->val).floating; sp->val.floating = hypot(a,(sp->val).floating);++wordp;break;
                case Pow   :  a = ((sp--)->val).floating; sp->val.floating = pow  (a,(sp->val).floating);++wordp;break;
                case NoOp:
                case TheEnd:
                default:
                    break;
                }
            }
        for(; sp >= mem->stack;)
            {
            psk res;
            size_t len;
            char buf[64]; /* 64 bytes is even enough for quad https://people.eecs.berkeley.edu/~wkahan/ieee754status/IEEE754.PDF*/
            double sv = (sp--)->val.floating;
            int flags;
            if(isnan(sv))
                {
                strcpy(buf, "NAN");
                flags = READY BITWISE_OR_SELFMATCHING;
                }
            else if(isinf(sv))
                {
                if(isinf(sv) < 0)
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
            /*strcpy((char*)POBJ(res), buf);*/
            if(res)
                {
                strcpy((char*)(res)+offsetof(sk, u.sobj), buf);
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
    forthword* word = mem->word;
    forthword* wordp = mem->wordp;
    stackvalue* sp = mem->sp;
    if(setArgs(&(mem->var), Arg, 0) > 0)
        {
        for(wordp = word;
            wordp->action != TheEnd;
            )
            {
            double a;
            double b;
            forthvariable* v;
            stackvalue* svp;
            printf("%s %d,%d ", ActionAsWord[wordp->action], (int)(wordp - word), (int)(sp - mem->stack));
            for(v = mem->var; v; v = v->next)
                {
                printf("%s=%.2f ", v->name, v->val.floating);
                };
            for(svp = sp - 1; svp >= mem->stack; --svp)
                {
                if(svp->valp == (forthvalue*)0xCDCDCDCDCDCDCDCD)
                    printf("<undef>");
                else printf("<%.2f>", svp->val.floating);
                }
            printf("\t");
            switch(wordp->action)
                {
                case ResolveAndPush:
                    {
                    printf("%s %.2f --> stack", getVarName(mem->var, (wordp->u.valp)), (*(wordp->u.valp)).floating);
                    (++sp)->val = *(wordp++->u.valp);
                    break;
                    }
                case ResolveAndGet:
                    {
                    assert(sp >= mem->stack);
                    printf("%s %.2f <-- stack", getVarName(mem->var, wordp->u.valp), ((sp - 1)->val).floating);
                    *(wordp++->u.valp) = sp->val;
                    break;
                    }
                case Push:
                    {
                    printf("%.2f", wordp->u.val.floating);
                    (++sp)->val = wordp++->u.val;
                    break;
                    }
                case Afunction:
                    {
                    naam = getFuncName(wordp->u.funcp);
                    printf(" %s", naam);
                    mem->wordp = wordp;
                    mem->sp = sp;
                    wordp->u.funcp(mem);
                    wordp = mem->wordp + 1;
                    sp = mem->sp;
                    break;
                    }
#if CFUNCS
                case Cfunction1:
                    {
                    printf("%.2f", wordp->u.val.floating);
                    sp->val.floating = wordp->u.Cfunc1p((sp->val).floating);
                    ++wordp;
                    break;
                    }
                case Cfunction2:
                    {
                    naam = getCFunc2Name(wordp->u.Cfunc2p);
                    printf(" %s", naam);
                    a = ((sp--)->val).floating;
                    b = (sp->val).floating;
                    sp->val.floating = wordp->u.Cfunc2p(a, b);
                    ++wordp;
                    break;
                    }
#endif
                case Pop:
                    {
                    naam = getLogiName(wordp->u.logic);
                    printf(" %s", naam);
                    printf(" conditional jump to %u", wordp->offset);
                    --sp;
                    ++wordp;
                    break;
                    }
                case PopUncondBranch:
                    {
                    naam = getLogiName(wordp->u.logic);
                    printf(" %s", naam);
                    printf(" unconditional jump to %u", wordp->offset);
                    --sp;
                    wordp = word + wordp->offset;
                    break;
                    }

                case Fless          : printf("< "); b = ((sp--)->val).floating; if( ((sp--)->val).floating >= b) wordp = word + wordp->offset; else ++wordp; break;
                case Fless_equal    : printf("<="); b = ((sp--)->val).floating; if( ((sp--)->val).floating >  b) wordp = word + wordp->offset; else ++wordp; break;
                case Fmore_equal    : printf(">="); b = ((sp--)->val).floating; if( ((sp--)->val).floating <  b) wordp = word + wordp->offset; else ++wordp; break;
                case Fmore          : printf("> "); b = ((sp--)->val).floating; if( ((sp--)->val).floating <= b) wordp = word + wordp->offset; else ++wordp; break;
                case Funequal       : printf("!="); b = ((sp--)->val).floating; if( ((sp--)->val).floating == b) wordp = word + wordp->offset; else ++wordp; break;
                case Fequal         : printf("=="); b = ((sp--)->val).floating; if( ((sp--)->val).floating != b) wordp = word + wordp->offset; else ++wordp; break;

                case Plus  :  printf("plus  "); a = ((sp--)->val).floating; sp->val.floating += a; ++wordp; break;
                case Times :  printf("times "); a = ((sp--)->val).floating; sp->val.floating *= a; ++wordp; break;

                case Acos  :  printf("acos  ");sp->val.floating = acos ((sp->val).floating);++wordp;break; 
                case Acosh :  printf("acosh ");sp->val.floating = acosh((sp->val).floating);++wordp;break; 
                case Asin  :  printf("asin  ");sp->val.floating = asin ((sp->val).floating);++wordp;break; 
                case Asinh :  printf("asinh ");sp->val.floating = asinh((sp->val).floating);++wordp;break; 
                case Atan  :  printf("atan  ");sp->val.floating = atan ((sp->val).floating);++wordp;break; 
                case Atanh :  printf("atanh ");sp->val.floating = atanh((sp->val).floating);++wordp;break; 
                case Cbrt  :  printf("cbrt  ");sp->val.floating = cbrt ((sp->val).floating);++wordp;break; 
                case Ceil  :  printf("ceil  ");sp->val.floating = ceil ((sp->val).floating);++wordp;break; 
                case Cos   :  printf("cos   ");sp->val.floating = cos  ((sp->val).floating);++wordp;break; 
                case Cosh  :  printf("cosh  ");sp->val.floating = cosh ((sp->val).floating);++wordp;break; 
                case Exp   :  printf("exp   ");sp->val.floating = exp  ((sp->val).floating);++wordp;break; 
                case Fabs  :  printf("fabs  ");sp->val.floating = fabs ((sp->val).floating);++wordp;break; 
                case Floor :  printf("floor ");sp->val.floating = floor((sp->val).floating);++wordp;break; 
                case Log   :  printf("log   ");sp->val.floating = log  ((sp->val).floating);++wordp;break; 
                case Log10 :  printf("log10 ");sp->val.floating = log10((sp->val).floating);++wordp;break; 
                case Sin   :  printf("sin   ");sp->val.floating = sin  ((sp->val).floating);++wordp;break; 
                case Sinh  :  printf("sinh  ");sp->val.floating = sinh ((sp->val).floating);++wordp;break; 
                case Sqrt  :  printf("sqrt  ");sp->val.floating = sqrt ((sp->val).floating);++wordp;break; 
                case Tan   :  printf("tan   ");sp->val.floating = tan  ((sp->val).floating);++wordp;break; 
                case Tanh  :  printf("tanh  ");sp->val.floating = tanh ((sp->val).floating);++wordp;break; 
                case Fmax  :  printf("fdim  ");a = ((sp--)->val).floating; sp->val.floating = fmax (a,(sp->val).floating);++wordp;break;
                case Atan2 :  printf("fmax  ");a = ((sp--)->val).floating; sp->val.floating = atan2(a,(sp->val).floating);++wordp;break;
                case Fmin  :  printf("atan2 ");a = ((sp--)->val).floating; sp->val.floating = fmin (a,(sp->val).floating);++wordp;break;
                case Fdim  :  printf("fmin  ");a = ((sp--)->val).floating; sp->val.floating = fdim (a,(sp->val).floating);++wordp;break;
                case Fmod  :  printf("fmod  ");a = ((sp--)->val).floating; sp->val.floating = fmod (a,(sp->val).floating);++wordp;break;
                case Hypot :  printf("hypot ");a = ((sp--)->val).floating; sp->val.floating = hypot(a,(sp->val).floating);++wordp;break;
                case Pow   :  printf("pow   ");a = ((sp--)->val).floating; sp->val.floating = pow  (a,(sp->val).floating);++wordp;break;
                case NoOp:
                case TheEnd:
                default:
                    break;
                }
            printf("\n");
            }
        printf("calculation DONE. On Stack %d\n", (int)(sp - mem->stack));
        for(; sp >= mem->stack;)
            {
            psk res;
            size_t len;
            char buf[64]; /* 64 bytes is even enough for quad https://people.eecs.berkeley.edu/~wkahan/ieee754status/IEEE754.PDF*/
            double sv = (--sp)->val.floating;
            int flags;
            if(isnan(sv))
                {
                strcpy(buf, "NAN");
                flags = READY BITWISE_OR_SELFMATCHING;
                }
            else if(isinf(sv))
                {
                if(isinf(sv) < 0)
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

            if(res)
                {
                strcpy((char*)(res)+offsetof(sk, u.sobj), buf);
                //strcpy((char*)SPOBJ(res), buf); /* gcc: generates buffer overflow. Why? (Addresses are the same!) */
                printf("value on stack %s\n", buf);
                wipe(*arg);
                *arg = same_as_w(res);
                res->v.fl = flags;
                return TRUE;
                }
/*
2.6700000000000000E+02
*/
            }
        }
    return FALSE;
    }

static int polish1(psk code)
    {
    int C;
    int R;
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
            case Pop:
                {
                naam = getLogiName(wordp->u.logic);
                printf(LONGD " Pop             %s %u\n", wordp - mem->word, naam, wordp->offset);
                break;
                }
            case PopUncondBranch:
                {
                naam = getLogiName(wordp->u.logic);
                printf(LONGD " PopUncondBranch %s %u\n", wordp - mem->word, naam, wordp->offset);
                break;
                }

            case Fless          : printf(LONGD " <                  %u\n", wordp - mem->word, wordp->offset);break;
            case Fless_equal    : printf(LONGD " <=                 %u\n", wordp - mem->word, wordp->offset);break;
            case Fmore_equal    : printf(LONGD " >=                 %u\n", wordp - mem->word, wordp->offset);break;
            case Fmore          : printf(LONGD " >                  %u\n", wordp - mem->word, wordp->offset);break;
            case Funequal       : printf(LONGD " !=                 %u\n", wordp - mem->word, wordp->offset);break;
            case Fequal         : printf(LONGD " ==                 %u\n", wordp - mem->word, wordp->offset);break;

            case Plus  :  printf(LONGD " plus               %u\n", wordp - mem->word, wordp->offset); break;
            case Times :  printf(LONGD " times              %u\n", wordp - mem->word, wordp->offset); break;

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
            case Pop:
                {
                if(wordp->u.logic == fand)
                    {
                    forthword* label;
                    while(1)
                        {
                        label = mem->word + wordp->offset; /* Address to jump to if previous failed. */
                        if(label->u.logic == fand)  /* (((FAIL&BLA)&K)&L) */
                            wordp->offset = label->offset;
                        else
                            break;
                        }
                    label = mem->word + wordp->offset; /* Address to jump to if previous failed. */
                    if(label->u.logic == fOr)    /* (FAIL&BLA)|X             If FAIL jump to X */
                        {
                        ++(wordp->offset);       /* offset now points to X */
                        wordp->u.logic = fand2;  /* removes top from stack */
                        }
                    else if(label->u.logic == fwhl)  /* whl'(FAIL&BLA) */
                        {
                        wordp->u.logic = fand2; /* removes top from stack */
                        label = mem->word + wordp->offset + 1; /* label == The address after the loop. */
                        if(label->u.logic == fand)             /* whl'(FAIL&BLA)&X            Jump to X for leaving the loop.                             */
                            (wordp->offset) += 2;              /* 1 for the end of the loop, 1 for the AND */
                        else if(label->u.logic == fOr        /* whl'(FAIL&BLA)|X            From FAIL jump to the offset of the OR for leaving the loop. X is unreachable! */
                                || label->u.logic == fwhl      /* whl'(ABC&whl'(FAIL&BLB))    From FAIL jump to the offset of the WHL, i.e. the start of the outer loop.                      */
                                )
                            {
                            wordp->offset = label->offset;
                            }
                        }
                    }
                else if(wordp->u.logic == fOr)
                    {
                    forthword* label;
                    while(1)
                        {
                        label = mem->word + wordp->offset; /* Address to jump to if previous was OK. */
                        if(label->u.logic == fOr)  /* (((OK|BLA)|K)|L) */
                            wordp->offset = label->offset;
                        else
                            break;
                        }
                    label = mem->word + wordp->offset; /* Address to jump to if previous failed. */
                    if(label->u.logic == fand)    /* (OK|BLA)&X             If OK jump to X */
                        {
                        ++(wordp->offset);       /* skip AND, offset now points to X */
                        wordp->u.logic = fOr2;   /* always removes top from stack */
                        }
                    else if(label->u.logic == fwhl)  /* whl'(OK|BLA) */
                        {
                        wordp->u.logic = fOr2; /* removes top from stack */
                        wordp->offset = label->offset;
                        }
                    }
                break;
                }
            case PopUncondBranch:
            case Fless          :
            case Fless_equal    :
            case Fmore_equal    :
            case Fmore          :
            case Funequal       :
            case Fequal         :
            case Plus:
            case Times:
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
        switch(wordp->action)
            {
            case TheEnd:
            case ResolveAndPush:
            case ResolveAndGet:
            case Push:
            case Afunction:
                break;
            case Pop:
                {
                if(wordp->u.logic == fwhl)
                    wordp->action = PopUncondBranch;
                break;
                }
            case PopUncondBranch:
                break;
            case Fless          :
            case Fless_equal    :
            case Fmore_equal    :
            case Fmore          :
            case Funequal       :
            case Fequal         :
                {
                label = wordp + 1;
                if(label->action == Pop)
                    {
                    wordp->offset = label->offset;
                    label->action = NoOp;
                    if(label->u.logic == fOr
                        || label->u.logic == fOr2
                        || label->u.logic == fwhl
                        )
                        {
                        wordp->action = negated(wordp->action);
                        }
                    }
                break;
                }

            case Plus  :
            case Times :
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
            case Atan2 :
            case Fdim  :
            case Fmax  :
            case Fmin  :
            case Fmod  :
            case Hypot :
            case Pow   :
            case NoOp  :
                break;
            }
        }
    }

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
            newwordp->offset = arr[wordp->offset];
            ++newwordp;
            }
        ++cells;
        }
    *newwordp = *wordp;
    newwordp->offset = arr[wordp->offset];
    bfree(mem->word);
    mem->word = newword;
    }

static forthword* polish2(forthvariable** varp, psk code, forthword* wordp, forthword* word)
    {
    switch(Op(code))
        {
        case PLUS:
            wordp = polish2(varp, code->LEFT, wordp, word);
            wordp = polish2(varp, code->RIGHT, wordp, word);
            wordp->action = Plus;
            wordp->offset = 0;
            return ++wordp;
        case TIMES:
            wordp = polish2(varp, code->LEFT, wordp, word);
            wordp = polish2(varp, code->RIGHT, wordp, word);
            wordp->action = Times;
            wordp->offset = 0;
            return ++wordp;
        case EXP:
            wordp = polish2(varp, code->LEFT, wordp, word);
            wordp = polish2(varp, code->RIGHT, wordp, word);
            wordp->action = Exp;
            wordp->offset = 0;
            return ++wordp;
        case LOG:
            wordp = polish2(varp, code->LEFT, wordp, word);
            wordp = polish2(varp, code->RIGHT, wordp, word);
            wordp->action = Log;
            wordp->offset = 0;
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
            saveword->action = Pop;
            saveword->u.logic = fand;
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
            saveword->action = Pop;
            saveword->u.logic = fOr;
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
                    wordp->action = Fless;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(FLESS_EQUAL(code->RIGHT))
                    {
                    wordp->action = Fless_equal;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(FMORE_EQUAL(code->RIGHT))
                    {
                    wordp->action = Fmore_equal;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(FMORE(code->RIGHT))
                    {
                    wordp->action = Fmore;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(FUNEQUAL(code->RIGHT))
                    {
                    wordp->action = Funequal;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(FLESSORMORE(code->RIGHT))
                    {
                    wordp->action = Funequal;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(FEQUAL(code->RIGHT))
                    {
                    wordp->action = Fequal;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(FNOTLESSORMORE(code->RIGHT))
                    {
                    wordp->action = Fequal;
                    wordp->offset = 0;
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
            wordp->action = Pop;
            wordp->u.logic = fwhl;
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
                    wordp->u.val.integer = (int)STRTOL(&(code->u.sobj), 0, 10);
                    if(HAS_MINUS_SIGN(code))
                        {
                        wordp->u.val.integer = -(wordp->u.val.integer);
                        }
                    wordp->action = Push;
                    /*When executing, push number onto the data stack*/
                    }
                else if(code->v.fl & QDOUBLE)
                    {
                    wordp->u.val.floating = strtod(&(code->u.sobj), 0);
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
    compaction(forthstuff);
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

