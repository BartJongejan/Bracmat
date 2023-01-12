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

#define TOINT(a) ((size_t)ceil(fabs(a)))

typedef enum
    {
    TheEnd
    , ResolveAndPush
    , ResolveAndGet
    , RslvPshArrElm
    , RslvGetArrElm
    , Push
    , Afunction
    , Pop
    , UncondBranch
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
    , Tbl
    , Ind
    , QInd /* ?(ind$(<array name>,<index>)) */
    , EInd /* !(ind$(<array name>,<index>)) */
    , NoOp
    /*
    , Cfunction1
    , Cfunction2
    */
    } actionType;

static char* ActionAsWord[] =
    { "TheEnd                     "
    , "ResolveAndPush             "
    , "ResolveAndGet              "
    , "RslvPshArrElm              "
    , "RslvGetArrElm              "
    , "Push                       "
    , "Afunction                  "
    , "Pop                        "
    , "UncondBranch               "
    , "PopUncondBranch            "
    , "Fless                      "
    , "Fless_equal                "
    , "Fmore_equal                "
    , "Fmore                      "
    , "Funequal                   "
    , "Fequal                     "
    , "Plus                       "
    , "Times                      "
    , "Acos                       "
    , "Acosh                      "
    , "Asin                       "
    , "Asinh                      "
    , "Atan                       "
    , "Atanh                      "
    , "Cbrt                       "
    , "Ceil                       "
    , "Cos                        "
    , "Cosh                       "
    , "Exp                        "
    , "Fabs                       "
    , "Floor                      "
    , "Log                        "
    , "Log10                      "
    , "Sin                        "
    , "Sinh                       "
    , "Sqrt                       "
    , "Tan                        "
    , "Tanh                       "
    , "Atan2                      "
    , "Fdim                       "
    , "Fmax                       "
    , "Fmin                       "
    , "Fmod                       "
    , "Hypot                      "
    , "Pow                        "
    , "tbl                        "
    , "ind                        "
    , "Qind                        "
    , "Eind                        "
    , "NoOp                       "
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
    // LONG integer;
    } forthvalue;

typedef struct fortharray
    {
    forthvalue* pval;
    char* name;
    struct fortharray* next;
    size_t size;
    size_t index;
    } fortharray;

typedef union stackvalue
    {
    forthvalue val;
    forthvalue* valp; /*pointer to value held by a variable*/
    forthvalue** valpp; /*pointer to array of values held by a variable*/
    fortharray* arrp;
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
        funct funcp; fortharray* arrp; forthvalue* valp; forthvalue val; dumbl logic;
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
    fortharray* arr;
    stackvalue* sp;
    stackvalue stack[64];
    } forthMemory;

typedef struct Cpair
    {
    char* name;
    funct Cfun;
    }Cpair;

typedef struct Epair
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

static char* getVarName(forthMemory* mem, forthvalue* val)
    {
    if(mem)
        {
        forthvariable* varp = mem->var;
        fortharray* arrp = mem->arr;
        static char buffer[32];
        for(; varp; varp = varp->next)
            {
            if(&(varp->val.floating) == &(val->floating))
                return varp->name;
            }
        for(; arrp; arrp = arrp->next)
            {
            for(size_t i = 0; i < arrp->size; ++i)
                if(&((arrp->pval + i)->floating) == &(val->floating))
                    {
                    sprintf(buffer, "%s[%zu]", arrp->name, i);
                    return buffer;
                    }
            }
        }
    return "UNK variable";
    }

static forthvariable* getVariablePointer(forthvariable** varp, char* name)
    {
    forthvariable* curvarp = *varp;
    while(curvarp != 0 && strcmp(curvarp->name, name))
        {
        curvarp = curvarp->next;
        }
    return curvarp;
    }

static forthvalue* createVariablePointer(forthvariable** varp, char* name)
    {
    forthvariable* curvarp = *varp;
    *varp = (forthvariable*)bmalloc(__LINE__, sizeof(forthvariable));
    (*varp)->name = bmalloc(__LINE__, strlen(name) + 1);
    strcpy((*varp)->name, name);
    (*varp)->next = curvarp;
    curvarp = *varp;
    return &(curvarp->val);
    }

static fortharray* getArrayPointer(fortharray** arrp, char* name)
    {
    fortharray* curarrp = *arrp;
    while(curarrp != 0 && strcmp(curarrp->name, name))
        {
        curarrp = curarrp->next;
        }
    return curarrp;
    }

static fortharray* getOrCreateArrayPointer(fortharray** arrp, char* name, size_t size)
    {
    fortharray* curarrp = *arrp;
    while(curarrp != 0 && strcmp(curarrp->name, name))
        {
        curarrp = curarrp->next;
        }
    if(curarrp == 0)
        {
        curarrp = *arrp;
        *arrp = (fortharray*)bmalloc(__LINE__, sizeof(fortharray));
        (*arrp)->name = bmalloc(__LINE__, strlen(name) + 1);
        strcpy((*arrp)->name, name);
        (*arrp)->next = curarrp;
        curarrp = *arrp;
        }
    if(curarrp->pval == 0)
        {
        curarrp->pval = (forthvalue*)bmalloc(__LINE__, size * sizeof(forthvalue));
        memset(curarrp->pval, 0, size * sizeof(forthvalue));
        curarrp->size = size;
        curarrp->index = 0;
        }
    if(!*arrp)
        *arrp = curarrp;
    return curarrp;
    }

static fortharray* getOrCreateArrayPointerButNoArray(fortharray** arrp, char* name)
    {
    fortharray* curarrp = *arrp;
    while(curarrp != 0 && strcmp(curarrp->name, name))
        {
        curarrp = curarrp->next;
        }
    if(curarrp == 0)
        {
        curarrp = *arrp;
        *arrp = (fortharray*)bmalloc(__LINE__, sizeof(fortharray));
        (*arrp)->name = bmalloc(__LINE__, strlen(name) + 1);
        strcpy((*arrp)->name, name);
        (*arrp)->next = curarrp;
        (*arrp)->pval = 0;
        (*arrp)->size = 0;
        (*arrp)->index = 0;
        curarrp = *arrp;
        }
    if(!*arrp)
        *arrp = curarrp;
    return curarrp;
    }

static int setFloat(forthvalue* destination, psk args)
    {
    if(args->v.fl & QDOUBLE)
        {
        destination->floating = strtod(&(args->u.sobj), 0);
        return 1;
        }
    else if(INTEGER_COMP(args))
        {
        destination->floating = strtod(&(args->u.sobj), 0);
        if(HAS_MINUS_SIGN(args))
            destination->floating = -destination->floating;
        return 1;
        }
    else if(RAT_RAT_COMP(args))
        {
        char* slash = strchr(&(args->u.sobj), '/');
        if(slash)
            {
            double numerator;
            double denominator;
            *slash = '\0';
            numerator = strtod(&(args->u.sobj), 0);
            denominator = strtod(slash + 1, 0);
            *slash = '/';
            destination->floating = numerator / denominator;
            if(HAS_MINUS_SIGN(args))
                destination->floating = -destination->floating;
            return 1;
            }
        }
    else if(args->u.sobj == '\0')
        {
        //printf("setArgsArr fails, numerical value missing\n");
        return 0;
        }
    return 1;
    }

static int setArgs(forthvariable** varp, fortharray** arrp, psk args, int nr)
    {
    if(is_op(args))
        {
        if(is_op(args->LEFT))
            {
            size_t size = 0;
            psk x = args->LEFT;
            static char name[24];/*Enough for 64 bit number in decimal.*/
            fortharray* a;
            forthvalue** val;
            size_t index;
            while(1)
                {
                ++size;
                x = x->RIGHT;
                if(!is_op(x))
                    break;
                }
            sprintf(name, "a%d", nr);
            a = getOrCreateArrayPointer(arrp, name, size);

            for(index = 0, x = args->LEFT; index < size; ++index, x = x->RIGHT)
                {
                setFloat(a->pval + index, x->LEFT);
                }
            setFloat(a->pval + index, x);
            return setArgs(varp, arrp, args->RIGHT, 1 + nr);
            }
        else
            {
            nr = setArgs(varp, arrp, args->LEFT, nr);
            if(nr > 0)
                return setArgs(varp, arrp, args->RIGHT, nr);
            }
        }
    else if(args->u.sobj != '\0')
        {
        static char name[24];/*Enough for 64 bit number in decimal.*/
        forthvalue* val;
        sprintf(name, "v%d", nr);
        val = &(getVariablePointer(varp, name)->val);
        if(setFloat(val, args))
            return 1 + nr;
        }
    else
        return nr;
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
        {"tbl",   Tbl},
        {"ind",   Ind},
        {"Qind",  QInd},
        {"Eind",  EInd},
        {"NoOp",  NoOp},
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
    if(setArgs(&(mem->var), &(mem->arr), Arg, 0) >= 0)
        {
        for(wordp = word; wordp->action != TheEnd;)
            {
            double a;
            double b;
            switch(wordp->action)
                {
                case ResolveAndPush:
                    (++sp)->val = *(wordp++->u.valp);
                    break;
                case ResolveAndGet:
                    assert(sp >= mem->stack);
                    *(wordp++->u.valp) = sp->val;
                    break;
                case RslvPshArrElm:
                    (++sp)->val = (wordp->u.arrp->pval)[wordp->u.arrp->index];
                    ++wordp;
                    break;
                case RslvGetArrElm:
                    assert(sp >= mem->stack);
                    (wordp->u.arrp->pval)[wordp->u.arrp->index] = sp->val;
                    ++wordp;
                    break;
                case Push:
                    (++sp)->val = wordp++->u.val;
                    break;
                case Afunction:
                    mem->wordp = wordp;
                    mem->sp = sp;
                    wordp->u.funcp(mem);
                    wordp = mem->wordp + 1;
                    sp = mem->sp;
                    break;
#if CFUNCS
                case Cfunction1:
                    sp->val.floating = wordp->u.Cfunc1p((sp->val).floating);
                    ++wordp;
                    break;
                case Cfunction2:
                    a = ((sp--)->val).floating;
                    b = (sp->val).floating;
                    sp->val.floating = wordp->u.Cfunc2p(a, b);
                    ++wordp;
                    break;
#endif
                case Pop:
                    --sp;
                    ++wordp;
                    break;
                case UncondBranch:
                    wordp = word + wordp->offset;
                    break;
                case PopUncondBranch:
                    --sp;
                    wordp = word + wordp->offset;
                    break;

                case Fless: b = ((sp--)->val).floating; if(((sp--)->val).floating >= b) wordp = word + wordp->offset; else ++wordp; break;
                case Fless_equal: b = ((sp--)->val).floating; if(((sp--)->val).floating > b) wordp = word + wordp->offset; else ++wordp; break;
                case Fmore_equal: b = ((sp--)->val).floating; if(((sp--)->val).floating < b) wordp = word + wordp->offset; else ++wordp; break;
                case Fmore: b = ((sp--)->val).floating; if(((sp--)->val).floating <= b) wordp = word + wordp->offset; else ++wordp; break;
                case Funequal: b = ((sp--)->val).floating; if(((sp--)->val).floating == b) wordp = word + wordp->offset; else ++wordp; break;
                case Fequal: b = ((sp--)->val).floating; if(((sp--)->val).floating != b) wordp = word + wordp->offset; else ++wordp; break;

                case Plus:  a = ((sp--)->val).floating; sp->val.floating += a; ++wordp; break;
                case Times:  a = ((sp--)->val).floating; sp->val.floating *= a; ++wordp; break;

                case Acos:  sp->val.floating = acos((sp->val).floating); ++wordp; break;
                case Acosh:  sp->val.floating = acosh((sp->val).floating); ++wordp; break;
                case Asin:  sp->val.floating = asin((sp->val).floating); ++wordp; break;
                case Asinh:  sp->val.floating = asinh((sp->val).floating); ++wordp; break;
                case Atan:  sp->val.floating = atan((sp->val).floating); ++wordp; break;
                case Atanh:  sp->val.floating = atanh((sp->val).floating); ++wordp; break;
                case Cbrt:  sp->val.floating = cbrt((sp->val).floating); ++wordp; break;
                case Ceil:  sp->val.floating = ceil((sp->val).floating); ++wordp; break;
                case Cos:  sp->val.floating = cos((sp->val).floating); ++wordp; break;
                case Cosh:  sp->val.floating = cosh((sp->val).floating); ++wordp; break;
                case Exp:  sp->val.floating = exp((sp->val).floating); ++wordp; break;
                case Fabs:  sp->val.floating = fabs((sp->val).floating); ++wordp; break;
                case Floor:  sp->val.floating = floor((sp->val).floating); ++wordp; break;
                case Log:  sp->val.floating = log((sp->val).floating); ++wordp; break;
                case Log10:  sp->val.floating = log10((sp->val).floating); ++wordp; break;
                case Sin:  sp->val.floating = sin((sp->val).floating); ++wordp; break;
                case Sinh:  sp->val.floating = sinh((sp->val).floating); ++wordp; break;
                case Sqrt:  sp->val.floating = sqrt((sp->val).floating); ++wordp; break;
                case Tan:  sp->val.floating = tan((sp->val).floating); ++wordp; break;
                case Tanh:  sp->val.floating = tanh((sp->val).floating); ++wordp; break;
                case Fmax:  a = ((sp--)->val).floating; sp->val.floating = fmax(a, (sp->val).floating); ++wordp; break;
                case Atan2:  a = ((sp--)->val).floating; sp->val.floating = atan2(a, (sp->val).floating); ++wordp; break;
                case Fmin:  a = ((sp--)->val).floating; sp->val.floating = fmin(a, (sp->val).floating); ++wordp; break;
                case Fdim:  a = ((sp--)->val).floating; sp->val.floating = fdim(a, (sp->val).floating); ++wordp; break;
                case Fmod:  a = ((sp--)->val).floating; sp->val.floating = fmod(a, (sp->val).floating); ++wordp; break;
                case Hypot:  a = ((sp--)->val).floating; sp->val.floating = hypot(a, (sp->val).floating); ++wordp; break;
                case Pow:  a = ((sp--)->val).floating; sp->val.floating = pow(a, (sp->val).floating); ++wordp; break;
                case Tbl:
                    {
                    size_t size = (size_t)((sp--)->val).floating;
                    sp->arrp->size = size;
                    sp->arrp->index = 0;
                    sp->arrp->pval = (forthvalue*)bmalloc(__LINE__, size * sizeof(forthvalue));
                    memset(sp->arrp->pval, 0, size * sizeof(forthvalue));
                    ++wordp;
                    break;
                    }
                case Ind:
                    {
                    int i = (int)((sp--)->val).floating;
                    sp->arrp->index = i;
                    ++wordp;
                    break;
                    }
                case QInd:
                    {
                    int i = (int)(sp->val).floating;
                    forthvalue* v = (--sp)->arrp->pval + i;
                    *v = (--sp)->val;
                    ++wordp;
                    break;
                    }
                case EInd:
                    {
                    int i = (int)((sp--)->val).floating;
                    sp->val = (sp->arrp->pval)[i];
                    ++wordp;
                    break;
                    }
/*
                case Ind:
                    {
                    int i = (int)((sp--)->val).floating;
                    sp->arrp->index = i;
                    ++wordp;
                    break;
                    }
                case QInd:
                    {
                    int i = (int)((sp--)->val).floating;
                    (sp->arrp->pval)[i] = sp->val;
                    ++wordp;
                    break;
                    }
                case EInd:
                    {
                    int i = (int)((sp--)->val).floating;
                    sp->val = (sp->arrp->pval)[i];
                    ++wordp;
                    break;
                    }
                    */
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
    stackvalue* sp = mem->sp - 1;
    if(setArgs(&(mem->var), &(mem->arr), Arg, 0) >= 0)
        {
        for(wordp = word; wordp->action != TheEnd; )
            {
            double a;
            double b;
            forthvariable* v;
            fortharray* arr;
            stackvalue* svp;
            printf("%s wordp %d, sp %d: ", ActionAsWord[wordp->action], (int)(wordp - word), (int)(sp - mem->stack));
            for(v = mem->var; v; v = v->next)
                {
                printf("%s=%.2f ", v->name, v->val.floating);
                };
            for(arr = mem->arr; arr; arr = arr->next)
                {
                size_t i;
                printf("%s=%zu index=%zu ", arr->name, arr->size, arr->index);
                for(i = 0; i < arr->size; ++i)
                    printf("%s[%zu]=%f ", arr->name, i, arr->pval[i].floating);
                };
            for(svp = sp; svp >= mem->stack; --svp)
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
                    printf("%s %.2f --> stack", getVarName(mem, (wordp->u.valp)), (*(wordp->u.valp)).floating);
                    (++sp)->val = *(wordp++->u.valp);
                    break;
                    }
                case ResolveAndGet:
                    {
                    assert(sp >= mem->stack);
                    *(wordp->u.valp) = sp->val;
                    printf("%s %.2f <-- stack", getVarName(mem, wordp->u.valp), sp->val.floating);
                    ++wordp;
                    break;
                    }
                case RslvPshArrElm:
                    printf("%s %.2f --> stack", wordp->u.arrp->name, (wordp->u.arrp->pval)[wordp->u.arrp->index].floating);
                    (++sp)->val = (wordp->u.arrp->pval)[wordp->u.arrp->index];
                    ++wordp;
                    break;
                case RslvGetArrElm:
                    assert(sp >= mem->stack);
                    (wordp->u.arrp->pval)[wordp->u.arrp->index] = sp->val;
                    printf("%s %.2f <-- stack", wordp->u.arrp->name, (wordp->u.arrp->pval)[wordp->u.arrp->index].floating);
                    ++wordp;
                    break;
                case Push:
                    {
                    printf("%.2f %p ", wordp->u.val.floating, wordp->u.arrp);
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
                    if(wordp->u.logic == fOr)
                        wordp = word + wordp->offset;
                    else
                        {
                        --sp;
                        ++wordp;
                        }
                    break;
                    }
                case UncondBranch:
                    printf("unconditional jump to %u", wordp->offset);
                    wordp = word + wordp->offset;
                    break;
                case PopUncondBranch:
                    {
                    naam = getLogiName(wordp->u.logic);
                    printf(" %s", naam);
                    printf(" unconditional jump to %u", wordp->offset);
                    --sp;
                    wordp = word + wordp->offset;
                    break;
                    }

                case Fless: printf("< "); b = ((sp--)->val).floating; if(((sp--)->val).floating >= b) wordp = word + wordp->offset; else ++wordp; break;
                case Fless_equal: printf("<="); b = ((sp--)->val).floating; if(((sp--)->val).floating > b) wordp = word + wordp->offset; else ++wordp; break;
                case Fmore_equal: printf(">="); b = ((sp--)->val).floating; if(((sp--)->val).floating < b) wordp = word + wordp->offset; else ++wordp; break;
                case Fmore: printf("> "); b = ((sp--)->val).floating; if(((sp--)->val).floating <= b) wordp = word + wordp->offset; else ++wordp; break;
                case Funequal: printf("!="); b = ((sp--)->val).floating; if(((sp--)->val).floating == b) wordp = word + wordp->offset; else ++wordp; break;
                case Fequal: printf("=="); b = ((sp--)->val).floating; if(((sp--)->val).floating != b) wordp = word + wordp->offset; else ++wordp; break;

                case Plus:  printf("plus  "); a = ((sp--)->val).floating; sp->val.floating += a; ++wordp; break;
                case Times:  printf("times "); a = ((sp--)->val).floating; sp->val.floating *= a; ++wordp; break;

                case Acos:  printf("acos  "); sp->val.floating = acos((sp->val).floating); ++wordp; break;
                case Acosh:  printf("acosh "); sp->val.floating = acosh((sp->val).floating); ++wordp; break;
                case Asin:  printf("asin  "); sp->val.floating = asin((sp->val).floating); ++wordp; break;
                case Asinh:  printf("asinh "); sp->val.floating = asinh((sp->val).floating); ++wordp; break;
                case Atan:  printf("atan  "); sp->val.floating = atan((sp->val).floating); ++wordp; break;
                case Atanh:  printf("atanh "); sp->val.floating = atanh((sp->val).floating); ++wordp; break;
                case Cbrt:  printf("cbrt  "); sp->val.floating = cbrt((sp->val).floating); ++wordp; break;
                case Ceil:  printf("ceil  "); sp->val.floating = ceil((sp->val).floating); ++wordp; break;
                case Cos:  printf("cos   "); sp->val.floating = cos((sp->val).floating); ++wordp; break;
                case Cosh:  printf("cosh  "); sp->val.floating = cosh((sp->val).floating); ++wordp; break;
                case Exp:  printf("exp   "); sp->val.floating = exp((sp->val).floating); ++wordp; break;
                case Fabs:  printf("fabs  "); sp->val.floating = fabs((sp->val).floating); ++wordp; break;
                case Floor:  printf("floor "); sp->val.floating = floor((sp->val).floating); ++wordp; break;
                case Log:  printf("log   "); sp->val.floating = log((sp->val).floating); ++wordp; break;
                case Log10:  printf("log10 "); sp->val.floating = log10((sp->val).floating); ++wordp; break;
                case Sin:  printf("sin   "); sp->val.floating = sin((sp->val).floating); ++wordp; break;
                case Sinh:  printf("sinh  "); sp->val.floating = sinh((sp->val).floating); ++wordp; break;
                case Sqrt:  printf("sqrt  "); sp->val.floating = sqrt((sp->val).floating); ++wordp; break;
                case Tan:  printf("tan   "); sp->val.floating = tan((sp->val).floating); ++wordp; break;
                case Tanh:  printf("tanh  "); sp->val.floating = tanh((sp->val).floating); ++wordp; break;
                case Fmax:  printf("fdim  "); a = ((sp--)->val).floating; sp->val.floating = fmax(a, (sp->val).floating); ++wordp; break;
                case Atan2:  printf("fmax  "); a = ((sp--)->val).floating; sp->val.floating = atan2(a, (sp->val).floating); ++wordp; break;
                case Fmin:  printf("atan2 "); a = ((sp--)->val).floating; sp->val.floating = fmin(a, (sp->val).floating); ++wordp; break;
                case Fdim:  printf("fmin  "); a = ((sp--)->val).floating; sp->val.floating = fdim(a, (sp->val).floating); ++wordp; break;
                case Fmod:  printf("fmod  "); a = ((sp--)->val).floating; sp->val.floating = fmod(a, (sp->val).floating); ++wordp; break;
                case Hypot:  printf("hypot "); a = ((sp--)->val).floating; sp->val.floating = hypot(a, (sp->val).floating); ++wordp; break;
                case Pow:  printf("pow   "); a = ((sp--)->val).floating; sp->val.floating = pow(a, (sp->val).floating); ++wordp; break;
                case Tbl:
                    {
                    size_t size = (size_t)((sp--)->val).floating;
                    printf("tbl   ");
                    sp->arrp->size = size;
                    sp->arrp->index = 0;
                    sp->arrp->pval = (forthvalue*)bmalloc(__LINE__, size * sizeof(forthvalue));
                    memset(sp->arrp->pval, 0, size * sizeof(forthvalue));
                    printf("%p size %zu index %zu", sp->arrp, sp->arrp->size, sp->arrp->index);
                    ++wordp;
                    break;
                    }
                case Ind:
                    {
                    int i = (int)((sp--)->val).floating;
                    printf("index   ");
                    sp->arrp->index = i;
                    ++wordp;
                    break;
                    }
                case QInd:
                    {
                    int i = (int)(sp->val).floating;
                    printf("?index  ");
                    forthvalue * v = (--sp)->arrp->pval+i;
                    *v = (--sp)->val;
                    ++wordp;
                    break;
                    }
                case EInd:
                    {
                    int i = (int)((sp--)->val).floating;
                    printf("!index  ");
                    sp->val = (sp->arrp->pval)[i];
                    ++wordp;
                    break;
                    }
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

static long argumentArrayNumber(psk code)
    {
    if((code->u.sobj) == 'a' && (&(code->u.sobj))[1])
        {
        const char* str = &(code->u.sobj) + 1;
        char* endptr;
        long nr;
        nr = strtol(str, &endptr, 10);
        if(!*endptr)
            return nr;
        }
    return -1L;
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
                if((code->v.fl & QDOUBLE) || INTEGER_COMP(code) || RAT_RAT_COMP(code))
                    return 1;
                else if(code->v.fl & (UNIFY | INDIRECT))
                    return 1; /* variable */
                else
                    {
                    if(argumentArrayNumber(code) >= 0)
                        return 1; /* aN  (N >= 0): name of implicit array passed as argument */
                    return 1; /* Variables & arrays explicitly declared in the code. */
                    }
                }
        }
    }

static Boolean printmem(forthMemory* mem)
    {
    char* naam;
    forthword* wordp = mem->word;
    printf("print\n");
    for(; wordp->action != TheEnd; ++wordp)
        {
        switch(wordp->action)
            {
            case ResolveAndPush:
                printf(LONGD " ResolveAndPush     %s\n", wordp - mem->word, getVarName(mem, (wordp->u.valp)));
                break;
            case ResolveAndGet:
                printf(LONGD " ResolveAndGet      %s\n", wordp - mem->word, getVarName(mem, (wordp->u.valp)));
                break;
            case RslvPshArrElm:
                printf(LONGD " RslvPshArrElm      %s\n", wordp - mem->word, wordp->u.arrp->name);
                break;
            case RslvGetArrElm:
                printf(LONGD " RslvGetArrElm      %s\n", wordp - mem->word, wordp->u.arrp->name);
                break;
            case Push:
                {
                forthvalue val = wordp->u.val;
                printf(LONGD " Push               %f or @%p\n", wordp - mem->word, val.floating, wordp->u.arrp);
                break;
                }
            case Afunction:
                naam = getFuncName(wordp->u.funcp);
                printf(LONGD " Afunction       %u %s\n", wordp - mem->word, wordp->offset, naam);
                break;
            case Pop:
                naam = getLogiName(wordp->u.logic);
                printf(LONGD " Pop             %u %s\n", wordp - mem->word, wordp->offset, naam);
                break;
            case UncondBranch:
                printf(LONGD " UncondBranch    %u\n", wordp - mem->word, wordp->offset);
                break;
            case PopUncondBranch:
                naam = getLogiName(wordp->u.logic);
                printf(LONGD " PopUncondBranch %u %s\n", wordp - mem->word, wordp->offset, naam);
                break;

            case Fless: printf(LONGD " <               %u\n", wordp - mem->word, wordp->offset); break;
            case Fless_equal: printf(LONGD " <=              %u\n", wordp - mem->word, wordp->offset); break;
            case Fmore_equal: printf(LONGD " >=              %u\n", wordp - mem->word, wordp->offset); break;
            case Fmore: printf(LONGD " >               %u\n", wordp - mem->word, wordp->offset); break;
            case Funequal: printf(LONGD " !=              %u\n", wordp - mem->word, wordp->offset); break;
            case Fequal: printf(LONGD " ==              %u\n", wordp - mem->word, wordp->offset); break;

            case Plus:  printf(LONGD " plus   \n", wordp - mem->word); break;
            case Times:  printf(LONGD " times  \n", wordp - mem->word); break;

            case Acos:  printf(LONGD " acos   \n", wordp - mem->word); break;
            case Acosh:  printf(LONGD " acosh  \n", wordp - mem->word); break;
            case Asin:  printf(LONGD " asin   \n", wordp - mem->word); break;
            case Asinh:  printf(LONGD " asinh  \n", wordp - mem->word); break;
            case Atan:  printf(LONGD " atan   \n", wordp - mem->word); break;
            case Atanh:  printf(LONGD " atanh  \n", wordp - mem->word); break;
            case Cbrt:  printf(LONGD " cbrt   \n", wordp - mem->word); break;
            case Ceil:  printf(LONGD " ceil   \n", wordp - mem->word); break;
            case Cos:  printf(LONGD " cos    \n", wordp - mem->word); break;
            case Cosh:  printf(LONGD " cosh   \n", wordp - mem->word); break;
            case Exp:  printf(LONGD " exp    \n", wordp - mem->word); break;
            case Fabs:  printf(LONGD " fabs   \n", wordp - mem->word); break;
            case Floor:  printf(LONGD " floor  \n", wordp - mem->word); break;
            case Log:  printf(LONGD " log    \n", wordp - mem->word); break;
            case Log10:  printf(LONGD " log10  \n", wordp - mem->word); break;
            case Sin:  printf(LONGD " sin    \n", wordp - mem->word); break;
            case Sinh:  printf(LONGD " sinh   \n", wordp - mem->word); break;
            case Sqrt:  printf(LONGD " sqrt   \n", wordp - mem->word); break;
            case Tan:  printf(LONGD " tan    \n", wordp - mem->word); break;
            case Tanh:  printf(LONGD " tanh   \n", wordp - mem->word); break;
            case Fdim:  printf(LONGD " fdim   \n", wordp - mem->word); break;
            case Fmax:  printf(LONGD " fmax   \n", wordp - mem->word); break;
            case Atan2:  printf(LONGD " atan2  \n", wordp - mem->word); break;
            case Fmin:  printf(LONGD " fmin   \n", wordp - mem->word); break;
            case Fmod:  printf(LONGD " fmod   \n", wordp - mem->word); break;
            case Hypot:  printf(LONGD " hypot  \n", wordp - mem->word); break;
            case Pow:  printf(LONGD " pow    \n", wordp - mem->word); break;
            case Tbl:  printf(LONGD " tbl    \n", wordp - mem->word); break;
            case Ind:  printf(LONGD " ind    \n", wordp - mem->word); break;
            case QInd:  printf(LONGD " Qind    \n", wordp - mem->word); break;
            case EInd:  printf(LONGD " Eind    \n", wordp - mem->word); break;
            case NoOp:
                naam = getFuncName(wordp->u.funcp);
                printf(LONGD " NoOp       %s\n", wordp - mem->word, naam);
                break;
            case TheEnd:
            default:
                printf(LONGD " default         %d\n", wordp - mem->word, wordp->action);
                ;
            }
        }
    printf(LONGD " TheEnd          \n", wordp - mem->word);
    return TRUE;
    }

static Boolean print(struct typedObjectnode* This, ppsk arg)
    {
    forthMemory* mem = (forthMemory*)(This->voiddata);
    return printmem(mem);
    }

static void optimizeJumps(forthMemory* mem)
    {
    forthword* wordp = mem->word;

    for(; wordp->action != TheEnd; ++wordp)
        {
        switch(wordp->action)
            {
            case TheEnd: break;
            case ResolveAndPush:
            case ResolveAndGet:
            case RslvPshArrElm:
            case RslvGetArrElm:
            case Push:
                wordp->offset = 0;
                break;
            case Afunction: break;
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
                    else
                        wordp->action = UncondBranch;
                    }
                break;
                }
            case UncondBranch:
            case PopUncondBranch:
            case Fless:
            case Fless_equal:
            case Fmore_equal:
            case Fmore:
            case Funequal:
            case Fequal:
            case Plus:
            case Times:
            case Acos:
            case Acosh:
            case Asin:
            case Asinh:
            case Atan:
            case Atanh:
            case Cbrt:
            case Ceil:
            case Cos:
            case Cosh:
            case Exp:
            case Fabs:
            case Floor:
            case Log:
            case Log10:
            case Sin:
            case Sinh:
            case Sqrt:
            case Tan:
            case Tanh:
            case Fdim:
            case Fmax:
            case Atan2:
            case Fmin:
            case Fmod:
            case Hypot:
            case Pow:
            case Tbl:
            case Ind:
            case QInd:
            case EInd:
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
            case RslvPshArrElm:
            case RslvGetArrElm:
            case Push:
            case Afunction:
                break;
            case Pop:
                {
                if(wordp->u.logic == fwhl)
                    wordp->action = PopUncondBranch;
                break;
                }
            case UncondBranch:
            case PopUncondBranch:
                break;
            case Fless:
            case Fless_equal:
            case Fmore_equal:
            case Fmore:
            case Funequal:
            case Fequal:
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

            case Plus:
            case Times:
            case Acos:
            case Acosh:
            case Asin:
            case Asinh:
            case Atan:
            case Atanh:
            case Cbrt:
            case Ceil:
            case Cos:
            case Cosh:
            case Exp:
            case Fabs:
            case Floor:
            case Log:
            case Log10:
            case Sin:
            case Sinh:
            case Sqrt:
            case Tan:
            case Tanh:
            case Atan2:
            case Fdim:
            case Fmax:
            case Fmin:
            case Fmod:
            case Hypot:
            case Pow:
            case Tbl:
            case Ind:
            case QInd:
            case EInd:
            case NoOp:
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

static forthword* polish2(forthMemory* mem, psk code, forthword* wordp)
    {
    forthvariable** varp = &(mem->var);
    fortharray** arrp = &(mem->arr);
    switch(Op(code))
        {
        case PLUS:
            wordp = polish2(mem, code->LEFT, wordp);
            wordp = polish2(mem, code->RIGHT, wordp);
            wordp->action = Plus;
            wordp->offset = 0;
            return ++wordp;
        case TIMES:
            wordp = polish2(mem, code->LEFT, wordp);
            wordp = polish2(mem, code->RIGHT, wordp);
            wordp->action = Times;
            wordp->offset = 0;
            return ++wordp;
        case EXP:
            wordp = polish2(mem, code->LEFT, wordp);
            wordp = polish2(mem, code->RIGHT, wordp);
            wordp->action = Exp;
            wordp->offset = 0;
            return ++wordp;
        case LOG:
            wordp = polish2(mem, code->LEFT, wordp);
            wordp = polish2(mem, code->RIGHT, wordp);
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
            wordp = polish2(mem, code->LEFT, wordp);
            saveword = wordp;
            saveword->action = Pop;
            saveword->u.logic = fand;
            wordp = polish2(mem, code->RIGHT, ++wordp);
            saveword->offset = (unsigned int)(wordp - mem->word);
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
            wordp = polish2(mem, code->LEFT, wordp);
            saveword = wordp;
            saveword->action = Pop;
            saveword->u.logic = fOr;
            wordp = polish2(mem, code->RIGHT, ++wordp);
            saveword->offset = (unsigned int)(ULONG)(wordp - mem->word); /* the address of the word after the 'if false' branch.*/
            return wordp;
            }
        case MATCH:
            {
            wordp = polish2(mem, code->LEFT, wordp);
            wordp = polish2(mem, code->RIGHT, wordp);
            if(!(code->RIGHT->v.fl & UNIFY) & !is_op(code->RIGHT))
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
                else if(ILESS(code->RIGHT))
                    {
                    wordp->action = Fless;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(ILESS_EQUAL(code->RIGHT))
                    {
                    wordp->action = Fless_equal;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(IMORE_EQUAL(code->RIGHT))
                    {
                    wordp->action = Fmore_equal;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(IMORE(code->RIGHT))
                    {
                    wordp->action = Fmore;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(IUNEQUAL(code->RIGHT))
                    {
                    wordp->action = Funequal;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(ILESSORMORE(code->RIGHT))
                    {
                    wordp->action = Funequal;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(IEQUAL(code->RIGHT))
                    {
                    wordp->action = Fequal;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(INOTLESSORMORE(code->RIGHT))
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
            unsigned int here = (unsigned int)(ULONG)(wordp - mem->word); /* start of loop */
            char* name = &code->LEFT->u.sobj;
            if(strcmp(name, "whl"))
                return 0;
            wordp = polish2(mem, code->RIGHT, wordp);
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
            wordp = polish2(mem, code->RIGHT, wordp);
            wordp->offset = 0;
            if(code->v.fl & INDIRECT)
                {
                for(; ep->name != 0; ++ep)
                    {
                    if(!strcmp(ep->name+1, name) && ep->name[0] == 'E') /* ! -> E(xclamation) */
                        {
                        wordp->action = ep->action;
                        return ++wordp;
                        }
                    }
                }
            else if(code->v.fl & UNIFY)
                {
                for(; ep->name != 0; ++ep)
                    {
                    if(!strcmp(ep->name + 1, name) && ep->name[0] == 'Q') /* ? -> Q(uestion)*/
                        {
                        wordp->action = ep->action;
                        return ++wordp;
                        }
                    }
                }
            else
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
                wordp = polish2(mem, code->LEFT, wordp);
                wordp = polish2(mem, code->RIGHT, wordp);
                return wordp;
                }
            else
                {
                if(INTEGER_COMP(code))
                    {
                    wordp->u.val.floating = strtod(&(code->u.sobj), 0);
                    if(HAS_MINUS_SIGN(code))
                        {
                        wordp->u.val.floating = -(wordp->u.val.floating);
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
                else if(RAT_RAT_COMP(code))
                    {
                    char* slash = strchr(&(code->u.sobj), '/');
                    if(slash)
                        {
                        double numerator;
                        double denominator;
                        *slash = '\0';
                        numerator = strtod(&(code->u.sobj), 0);
                        denominator = strtod(slash + 1, 0);
                        *slash = '/';
                        wordp->u.val.floating = numerator / denominator;
                        if(HAS_MINUS_SIGN(code))
                            wordp->u.val.floating = -wordp->u.val.floating;
                        wordp->action = Push;
                        }
                    }

                else
                    {
                    if(code->v.fl & (INDIRECT | UNIFY))
                        {
                        /*variable*/
                        forthvariable* v = getVariablePointer(varp, &(code->u.sobj));
                        if(v == 0)
                            {
                            if(argumentArrayNumber(code) >= 0)
                                {
                                fortharray* a = getOrCreateArrayPointerButNoArray(arrp, &(code->u.sobj));
                                wordp->u.arrp = a;
                                wordp->action = (code->v.fl & INDIRECT) ? RslvPshArrElm : RslvGetArrElm;
                                }
                            else
                                {
                                fortharray* a = getArrayPointer(arrp, &(code->u.sobj));
                                if(a)
                                    {
                                    wordp->u.arrp = a;
                                    wordp->action = (code->v.fl & INDIRECT) ? RslvPshArrElm : RslvGetArrElm;
                                    }
                                else
                                    {
                                    wordp->u.valp = createVariablePointer(varp, &(code->u.sobj));
                                    wordp->action = (code->v.fl & INDIRECT) ? ResolveAndPush : ResolveAndGet;
                                    }
                                }
                            }
                        else
                            {
                            wordp->u.valp = &(v->val);
                            wordp->action = (code->v.fl & INDIRECT) ? ResolveAndPush : ResolveAndGet;
                            /* When executing,
                            * ResolveAndPush: follow the pointer wordp->u.valp and push the pointed-at value onto the stack.
                            * ResolveAndGet:  follow the pointer wordp->u.valp and assign to that address the value that is on top of the stack. Do not change the stack.
                            */
                            }
                        }
                    else
                        {
                        /*array*/
                        fortharray* a = getOrCreateArrayPointerButNoArray(arrp, &(code->u.sobj));
                        if(a != 0 || argumentArrayNumber(code) >= 0)
                            {
                            wordp->u.arrp = a;
                            wordp->action = Push;
                            /* When executing,
                            * Push:           Push the value wordp->u.arrp onto the stack.
                            */
                            }
                        else
                            {
                            printf("Unknown array: [%s]\n", &(code->u.sobj));
                            }
                        }
                    wordp->offset = 0;
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
    forthstuff->arr = 0;
    forthstuff->word = bmalloc(__LINE__, length * sizeof(forthword) + 1);
    forthstuff->wordp = forthstuff->word;
    forthstuff->sp = forthstuff->stack;
    lastword = polish2(forthstuff, code, forthstuff->wordp);
    lastword->action = TheEnd;
    /*
        printf("Not optimized:\n");
        printmem(forthstuff);
    */
    optimizeJumps(forthstuff);
    /*
        printf("Optimized jumps:\n");
        printmem(forthstuff);
    */
    combineTestsAndJumps(forthstuff);
    /*
        printf("Combined testst and jumps:\n");
        printmem(forthstuff);
    */
    compaction(forthstuff);
    /*
        printf("Compacted:\n");
        printmem(forthstuff);
    */
    return TRUE;
    }

static Boolean calculationdie(struct typedObjectnode* This, ppsk arg)
    {
    forthvariable* curvarp = ((forthMemory*)(This->voiddata))->var;
    fortharray* curarr = ((forthMemory*)(This->voiddata))->arr;
    while(curvarp)
        {
        forthvariable* nextvarp = curvarp->next;
        bfree(curvarp->name);
        bfree(curvarp);
        curvarp = nextvarp;
        }
    while(curarr)
        {
        fortharray* nextarrp = curarr->next;
        bfree(curarr->name);
        if(curarr->pval)
            bfree(curarr->pval);
        bfree(curarr);
        curarr = nextarrp;
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

