#include "calculation.h"
#include "variables.h"
#include "nodedefs.h"
#include "memory.h"
#include "wipecopy.h"
#include "input.h"
#include "globals.h" /*nilNode*/
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <float.h>

/*#include "result.h"*/ /* For debugging. Remove when done. */


#define CFUNCS 0
#define CFUNC 0

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
    , Subtract
    , Divide
    , Tbl
    , Out
    , Idx   /*   idx$(<array name>,<index>,...)  */
    , QIdx  /* ?(idx$(<array name>,<index>,...)) */
    , EIdx  /* !(idx$(<array name>,<index>,...)) */
    , Range /* range$(<array name>,d)) 0 <= d < rank, returns 0 if d < 0 or d >= array rank */
    , Rank  /* rank$(<array name>)) */
    , NoOp
#if CFUNCS
    , Cfunction1
    , Cfunction2
#endif
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
    , "Subtract                   "
    , "Divide                     "
    , "tbl                        "
    , "idx                        "
    , "Qidx                       "
    , "Eidx                       "
    , "NoOp                       "
#if CFUNCS
    , "Cfunction1                 "
    , "Cfunction2                 "
#endif
    };

struct forthMemory;

#if CFUNC
typedef void (*funct)(struct forthMemory* This);
#endif
#if CFUNCS
typedef double (*Cfunct1)(double x);
typedef double (*Cfunct2)(double x, double y);
#endif

typedef psk(*exportfunct)(double x);
exportfunct xprtfnc;

typedef union forthvalue /* a number. either integer or 'real' */
    {
    double floating;
    /* LONG integer;*/
    } forthvalue;

typedef struct fortharray
    {
    char* name;
    struct fortharray* next;
    forthvalue* pval;
    size_t size;
    size_t index;
    size_t rank;
    size_t* range;
    } fortharray;

typedef union stackvalue
    {
    forthvalue val;
    forthvalue* valp; /*pointer to value held by a variable*/
    fortharray* arrp;
    } stackvalue;

typedef struct forthvariable
    {
    char* name;
    struct forthvariable* next;
    forthvalue val;
    } forthvariable;

typedef enum { Scalar, Array } scalarORarray;

typedef struct parameter
    {
    scalarORarray scalar_or_array;
    union
        {
        forthvariable* v;
        fortharray* a;
        } u;
    } parameter;

typedef enum
    {
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

struct forthMemory;

typedef struct forthword
    {
    actionType action : 8;
    unsigned int offset : 24; /* Interpreted as arity if action is Afunction */
    union
        {
#if CFUNC
        funct funcp;
#endif
        fortharray* arrp; forthvalue* valp; forthvalue val; dumbl logic; struct forthMemory* that;
#if CFUNCS
        Cfunct1 Cfunc1p; Cfunct2 Cfunc2p;
#endif        
        } u;
    } forthword;

typedef struct forthMemory
    {
    char* name;
    struct forthMemory* nextFnc;
    forthword* word; /* fixed once calculation is compiled */
    forthword* wordp; /* runs through words when calculating */
    forthvariable* var;
    fortharray* arr;
    stackvalue* sp;
    parameter* parameters;
    size_t nparameters;
    stackvalue stack[64];
    } forthMemory;

#if CFUNC
typedef struct Cpair
    {
    char* name;
    funct Cfun;
    }Cpair;
#endif

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

static forthvariable* getVariablePointer(forthvariable* varp, char* name)
    {
    forthvariable* curvarp = varp;
    while(curvarp != 0 && (curvarp->name == 0 || strcmp(curvarp->name, name)))
        {
        curvarp = curvarp->next;
        }
    return curvarp;
    }

static forthvariable* createVariablePointer(forthvariable** varp, char* name)
    {
    forthvariable* newvarp = (forthvariable*)bmalloc(__LINE__, sizeof(forthvariable));
    if(newvarp)
        {
        //printf("bmalloc:%s\n", "forthvariable");
        newvarp->name = bmalloc(__LINE__, strlen(name) + 1);
        if(newvarp->name)
            {
            //printf("bmalloc:%s\n", "char[]");
            strcpy(newvarp->name, name);
            }
        newvarp->next = *varp;
        *varp = newvarp;
        }
    return *varp;
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

Boolean initialise(fortharray* curarrp, size_t size)
    {
    curarrp->index = 0;
    assert(curarrp->pval == 0);
    curarrp->pval = (forthvalue*)bmalloc(__LINE__, size * sizeof(forthvalue));
    if(curarrp->pval)
        {
        //printf("bmalloc:%s\n", "forthvalue[]");
        memset(curarrp->pval, 0, size * sizeof(forthvalue));
        curarrp->size = size;
        }
    else
        {
        curarrp->size = 0;
        return FALSE;
        }
    return TRUE;
    }

static fortharray* getOrCreateArrayPointer(fortharray** arrp, char* name, size_t size)
    {
    fortharray* curarrp = *arrp;
    if(name[0] == '\0')
        {
        fprintf(stderr, "Array name is empty string.\n");
        return 0; /* Something wrong happened. */
        }

    while(curarrp != 0 && strcmp(curarrp->name, name))
        {
        curarrp = curarrp->next;
        }

    if(curarrp == 0)
        {
        curarrp = *arrp;
        //assert(*arrp == 0);
        *arrp = (fortharray*)bmalloc(__LINE__, sizeof(fortharray));
        if(*arrp)
            {
            //printf("bmalloc:%s\n", "fortharray");
            (*arrp)->next = curarrp;
            curarrp = *arrp;
            assert(curarrp->name == 0);
            curarrp->name = bmalloc(__LINE__, strlen(name) + 1);
            if(curarrp->name)
                {
                //printf("bmalloc:%s\n", "char[]");
                strcpy(curarrp->name, name);
                curarrp->rank = 0;
                curarrp->range = 0;
                curarrp->index = 0;
                curarrp->size = 0;
                curarrp->pval = 0;
                }
            else
                {
                fprintf(stderr, "Cannot allocate memory for array name \"%s\".\n", name);
                return 0;
                }
            }
        else
            {
            fprintf(stderr, "Cannot allocate memory for array struct.\n");
            return 0;
            }
        }
    else if(curarrp->pval != 0)
        {
        if(curarrp->size != size)
            {
            bfree(curarrp->pval);
            curarrp->pval = 0;
            }
        else if(size > 0)
            {
            memset(curarrp->pval, 0, size * sizeof(forthvalue));
            curarrp->index = 0;
            }
        }

    if((size > 0) && (curarrp->pval == 0))
        {
        initialise(curarrp, size);
        }

    if(!*arrp)
        *arrp = curarrp;
    return curarrp;
    }

static fortharray* getOrCreateArrayPointerButNoArray(fortharray** arrp, char* name)
    {
    fortharray* curarrp = *arrp;
    if(name[0] == '\0')
        {
        return 0; /* This is OK. */
        }

    while(curarrp != 0 && strcmp(curarrp->name, name))
        {
        curarrp = curarrp->next;
        }
    if(curarrp == 0)
        {
        curarrp = *arrp;
        assert(*arrp == 0);
        *arrp = (fortharray*)bmalloc(__LINE__, sizeof(fortharray));
        if(*arrp)
            {
            //printf("bmalloc:%s\n", "fortharray");
            (*arrp)->name = bmalloc(__LINE__, strlen(name) + 1);
            if((*arrp)->name)
                {
                //printf("bmalloc:%s\n", "char[]");
                strcpy((*arrp)->name, name);
                (*arrp)->next = curarrp;
                (*arrp)->pval = 0;
                (*arrp)->size = 0;
                (*arrp)->index = 0;
                (*arrp)->rank = 0;
                (*arrp)->range = 0;
                curarrp = *arrp;
                }
            }
        }
    if(!*arrp)
        *arrp = curarrp;
    return curarrp;
    }

static int setFloat(double* destination, psk args)
    {
    if(args->v.fl & QDOUBLE)
        {
        *destination = strtod(&(args->u.sobj), 0);
        if(HAS_MINUS_SIGN(args))
            {
            *destination = -(*destination);
            }
        return 1;
        }
    else if(INTEGER_COMP(args))
        {
        *destination = strtod(&(args->u.sobj), 0);
        if(HAS_MINUS_SIGN(args))
            *destination = -*destination;
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
            *destination = numerator / denominator;
            if(HAS_MINUS_SIGN(args))
                *destination = -*destination;
            return 1;
            }
        }
    else if(args->u.sobj == '\0')
        {
        /*printf("setArgsArr fails, numerical value missing\n");*/
        fprintf(stderr, "Numerical value missing.\n");
        return 0; /* Something wrong happened. */
        }
    return 1;
    }

static void fsetArgs(stackvalue* sp, int arity, forthMemory* thatmem)
    {
    parameter* parms = thatmem->parameters;
    size_t Ndecl = 0;
    for(; --arity >= 0 && Ndecl < thatmem->nparameters; ++Ndecl)
        {
        if(parms[Ndecl].scalar_or_array == Scalar)
            {
            parms[Ndecl].u.v->val.floating = sp->val.floating;
            }
        else
            {
            fortharray* arrp = parms[Ndecl].u.a;
            fortharray* arr = sp->arrp;
            arrp->pval = arr->pval;
            arrp->size = arr->size;
            arrp->index = arr->index;
            arrp->rank = arr->rank;
            arrp->range = arr->range;
            }
        --sp;
        }
    }

static size_t find_rank(psk Node)
    {
    assert(Op(Node) == COMMA);
    size_t subrank;
    size_t msubrank = 0;
    psk el;
    for(el = Node->RIGHT; is_op(el) && Op(el) == WHITE; el = el->RIGHT)
        if(is_op(el->LEFT) && Op(el->LEFT) == COMMA)
            {
            subrank = find_rank(el->LEFT);
            if(subrank > msubrank)
                msubrank = subrank;
            }
    if(is_op(el) && Op(el) == COMMA)
        {
        subrank = find_rank(el);
        if(subrank > msubrank)
            msubrank = subrank;
        }
    return 1 + msubrank;
    }

static void set_range(size_t* range, psk Node)
    {
    assert(Op(Node) == COMMA);
    size_t lrange = 1;
    psk el;
    for(el = Node->RIGHT; is_op(el) && Op(el) == WHITE; el = el->RIGHT)
        {
        ++lrange;
        if(is_op(el->LEFT) && Op(el->LEFT) == COMMA)
            {
            set_range(range - 1, el->LEFT);
            }
        }
    if(is_op(el) && Op(el) == COMMA)
        {
        set_range(range - 1, el);
        }
    if(lrange > *range)
        {
        *range = lrange;
        }
    }

static void set_vals(forthvalue* pval, size_t rank, size_t* range, psk Node)
    {
    assert(Op(Node) == COMMA);
    psk el;
    size_t stride = 1;
    size_t N;
    size_t index;

    for(size_t k = 0; k + 1 < rank; ++k)
        stride *= range[k];
    N = stride * range[rank - 1];
    for(index = 0, el = Node->RIGHT; index < N && is_op(el) && Op(el) == WHITE; index += stride, el = el->RIGHT)
        {
        if(is_op(el->LEFT) && Op(el->LEFT) == COMMA)
            {
            set_vals(pval + index, rank - 1, range, el->LEFT);
            }
        else
            {
            for(size_t t = 0; t < stride; ++t)
                setFloat(&(pval[index + t].floating), el->LEFT);
            }
        }
    if(is_op(el) && Op(el) == COMMA)
        {
        set_vals(pval + index, rank - 1, range, el);
        }
    else
        {
        for(size_t t = 0; t < stride; ++t)
            setFloat(&(pval[index + t].floating), el);
        }
    }

static long setArgs(forthMemory * mem, size_t Nparm, psk args)
    {
    parameter* curparm = mem->parameters;
    long ParmIndex = (long)Nparm;
    if(is_op(args))
        {
        assert(Op(args) == DOT || Op(args) == COMMA || Op(args) == WHITE);
        if(Op(args) == COMMA && !is_op(args->LEFT) && args->LEFT->u.sobj == '\0')
            { /* args is an array */
            /* find rank and ranges */
            size_t rank = find_rank(args);
            size_t* range = (size_t*)bmalloc(__LINE__, rank * sizeof(size_t));
            if(range != 0)
                {
                //printf("bmalloc:%s\n", "size_t[]");
                memset(range, 0, rank * sizeof(size_t));
                set_range(range + rank - 1, args);
                size_t totsize = 1;
                for(size_t k = 0; k < rank; ++k)
                    {
                    totsize *= range[k];
                    }
                fortharray* a = 0;
                if(curparm)
                    {
                    a = curparm[ParmIndex].u.a;
                    if(a->size != totsize)
                        {
                        fprintf(stderr, "Declared size of array \"%s\" is %zu, actual size is %zu.\n", a->name, a->size, totsize);
                        bfree(range);
                        return -1;
                        }
                    for(size_t k = 0; k < a->rank; ++k)
                        if(a->range[k] != range[k])
                            {
                            fprintf(stderr, "Declared range of array \"%s\" is %zu, actual range is %zu.\n", a->name, a->range[k], range[k]);
                            bfree(range);
                            return -1;
                            }
                    initialise(a, totsize);
                    /*
                    assert(a->range == 0);
                    a->range = range;
                    a->rank = rank;
                    */
                    set_vals(a->pval, rank, range, args);
                    ++ParmIndex;
                    }
                bfree(range);
                }
            }
        else
            {
            ParmIndex = setArgs(mem, ParmIndex, args->RIGHT);
            if(ParmIndex >= 0)
                ParmIndex = setArgs(mem, (size_t)ParmIndex, args->LEFT);
            }
        }
    else if(args->u.sobj != '\0')
        {
        forthvariable* var = 0;
        if(curparm)
            {
            var = curparm[ParmIndex].u.v;
            setFloat(&(var->val.floating), args);
            ++ParmIndex;
            }
        }
    return ParmIndex;
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


#if CFUNC
static Cpair pairs[] =
    {
        {0,0}
    };
#endif

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
        {"subtract",    Subtract},
        {"divide",Divide},
        {"tbl",   Tbl},
        {"out",   Out},
        {"idx",   Idx},
        {"Qidx",  QIdx},
        {"Eidx",  EIdx},
        {"range", Range},
        {"rank",  Rank},
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

#if CFUNC
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
#endif

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

static stackvalue* getArrayIndex(stackvalue* sp, forthword* wordp)
    {
    /*size_t rank = (size_t)((sp--)->val).floating;*/
    size_t rank = wordp->offset - 1;
    fortharray* arrp = (sp - rank)->arrp;
    size_t i;
    if(arrp->range == 0) /* linear array passed as argument to calculation, a0, a1, ... */
        {
        i = (size_t)((sp--)->val).floating;
        }
    else
        {
        i = (size_t)((sp--)->val).floating;
        size_t arank = arrp->rank;
        size_t stride = arrp->range[0];
        size_t range_index = 1;
        for(; arank > rank; --arank)
            {
            stride *= arrp->range[range_index++];
            }

        for(; range_index < rank;)
            {
            size_t k = (size_t)((sp--)->val).floating;
            i += k * stride;
            stride *= arrp->range[range_index++];
            }
        }
    sp->arrp->index = i;
    return sp;
    }

static stackvalue* getArrayRange(stackvalue* sp)
    {
    fortharray* arrp = (sp - 1)->arrp;
    size_t rank = arrp->rank;
    LONG arg = (LONG)sp->val.floating;
    sp->val.floating = (double)((0 <= arg && arg < (LONG)rank) ? (arrp->range[rank - arg - 1]) : 0);
    return sp;
    }

static stackvalue* getArrayRank(stackvalue* sp)
    {
    sp->val.floating = (double)(sp->arrp->rank);
    return sp;
    }

static Boolean fcalculate(stackvalue* sp, forthword* wordp, double* ret);

static stackvalue* calculateBody(forthMemory* mem)
    {
    forthword* word = mem->word;
    forthword* wordp = mem->wordp;
    stackvalue* sp = mem->sp - 1;
    for(wordp = word; wordp->action != TheEnd;)
        {
        double a;
        double b;
        size_t i;
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
                {
                double ret = 0;
                fcalculate(sp, wordp, &ret); /* May fail! */
                (++sp)->val.floating = ret;
                ++wordp;
                break;
                }
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
            case Subtract: a = ((sp--)->val).floating; sp->val.floating = (sp->val).floating - a; ++wordp; break;
            case Divide: a = ((sp--)->val).floating; sp->val.floating = (sp->val).floating / a; ++wordp; break;

            case Tbl:
                {
                //size_t rank = (size_t)((sp--)->val).floating;
                fortharray* arr = getArrayIndex(sp, wordp)->arrp;
                size_t rank = arr->rank;
                size_t size = 1;
                size_t* range = arr->range;
                if(range == 0)
                    {
                    range = (size_t*)bmalloc(__LINE__, rank * sizeof(size_t));
                    arr->range = range;
                    }
                if(range != 0)
                    {
                    //printf("bmalloc:%s\n", "size_t[]");
                    for(size_t j = 0; j < rank; ++j)
                        {
                        if(range[j] == 0)
                            range[j] = (size_t)((sp--)->val).floating; /* range[0] = range of last index*/
                        else if(range[j] != (size_t)((sp--)->val).floating)
                            {
                            fprintf(stderr, "tbl: attempting to change fixed range from %zu to %zu.\n", range[j], (size_t)((sp--)->val).floating);
                            return FALSE;
                            }
                        size *= range[j];
                        }
                    fortharray* arr = sp->arrp;
                    arr->size = size;
                    arr->index = 0;
                    assert(arr->pval == 0);
                    arr->pval = (forthvalue*)bmalloc(__LINE__, size * sizeof(forthvalue));
                    if(arr->pval)
                        {
                        //printf("bmalloc:%s\n", "forthvalue[]");
                        memset(arr->pval, 0, size * sizeof(forthvalue));
                        }
                    arr->rank = rank;
                    arr->range = range;
                    }
                ++wordp;
                break;
                }
            case Out:
                printf("%f\n", sp->val.floating); ++wordp; break;
                break;
            case Idx:
                {
                sp = getArrayIndex(sp, wordp);
                i = sp->arrp->index;
                if(i >= sp->arrp->size)
                    {
                    return FALSE;
                    }
                sp->arrp->index = i;
                ++wordp;
                break;
                }
            case QIdx:
                {
                sp = getArrayIndex(sp, wordp);
                i = sp->arrp->index;
                if(i >= sp->arrp->size)
                    {
                    fprintf(stderr, "Index past upper bound.\n");
                    return 0; /* Something wrong happened. */
                    }

                forthvalue* val = sp->arrp->pval + i;
                *val = (--sp)->val;
                ++wordp;
                break;
                }
            case EIdx:
                {
                sp = getArrayIndex(sp, wordp);
                i = sp->arrp->index;
                if(i >= sp->arrp->size)
                    {
                    return FALSE;
                    }
                sp->arrp->index = i;
                sp->val = (sp->arrp->pval)[i];
                ++wordp;
                break;
                }
            case Range:
                {
                sp = getArrayRange(sp);
                ++wordp;
                break;
                }
            case Rank:
                {
                sp = getArrayRank(sp);
                ++wordp;
                break;
                }
            case NoOp:
            case TheEnd:
            default:
                break;
            }
        }
    return sp;
    }

static Boolean calculate(struct typedObjectnode* This, ppsk arg)
    {
    psk Arg = (*arg)->RIGHT;
    forthMemory* mem = (forthMemory*)(This->voiddata);
    parameter* curparm = mem->parameters;
    if(mem)
        {
        if(!curparm || setArgs(mem, 0, Arg) > 0)
            {
            stackvalue* sp = calculateBody(mem);
            if(sp)
                {
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
                        if(sv > DBL_MAX)
                            strcpy(buf, "INF");
                        else
                            strcpy(buf, "-INF");
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
                        //                    printf("bmalloc:%s\n", "char[]");
                        strcpy((char*)(res)+offsetof(sk, u.sobj), buf);
                        wipe(*arg);
                        *arg = /*same_as_w*/(res);
                        res->v.fl = flags;
                        return TRUE;
                        }
                    }
                }
            }
        else
            return FALSE;
        }
    return FALSE;
    }

static Boolean fcalculate(stackvalue* sp, forthword* wordp, double* ret)
    {
    forthMemory* thatmem = wordp->u.that;
    if(sp && thatmem)
        {
        stackvalue* sp2;
        fsetArgs(sp, wordp->offset, thatmem);
        sp2 = calculateBody(thatmem);
        if(sp2 && sp2 >= thatmem->stack)
            {
            *ret = thatmem->stack->val.floating;
            return TRUE;
            }
        else
            return FALSE;
        }
    return FALSE;
    }

static Boolean ftrc(stackvalue* sp, forthword* wordp, double* ret);

static stackvalue* trcBody(forthMemory* mem)
    {
    forthword* word = mem->word;
    forthword* wordp = mem->wordp;
    stackvalue* sp = mem->sp - 1;
    for(wordp = word; wordp->action != TheEnd;)
        {
        double a;
        double b;
        size_t i;
        forthvariable* v;
        fortharray* arr;
        stackvalue* svp;
        char* naam;
        assert(sp + 1 >= mem->stack);
        printf("%s wordp %d, sp %d: ", ActionAsWord[wordp->action], (int)(wordp - word), (int)(sp - mem->stack));
        for(v = mem->var; v; v = v->next)
            {
            printf("%s=%.2f ", v->name, v->val.floating);
            };

        for(arr = mem->arr; arr; arr = arr->next)
            {
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
                printf("%.2f %p ", wordp->u.val.floating, (void*)wordp->u.arrp);
                (++sp)->val = wordp++->u.val;
                break;
                }
            case Afunction:
                {
                double ret = 0;
                naam = wordp->u.that->name;
                printf(" %s", naam);
                ftrc(sp, wordp, &ret); // May fail!
                (++sp)->val.floating = ret;
                ++wordp;
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
                /* if(wordp->u.logic == fOr)
                     wordp = word + wordp->offset;
                 else*/
                {
                assert(sp >= mem->stack);
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
                assert(sp >= mem->stack);
                --sp;
                wordp = word + wordp->offset;
                break;
                }

            case Fless: printf("PopPop < "); b = ((sp--)->val).floating; if(((sp--)->val).floating >= b) wordp = word + wordp->offset; else ++wordp; break;
            case Fless_equal: printf("PopPop <="); b = ((sp--)->val).floating; if(((sp--)->val).floating > b) wordp = word + wordp->offset; else ++wordp; break;
            case Fmore_equal: printf("PopPop >="); b = ((sp--)->val).floating; if(((sp--)->val).floating < b) wordp = word + wordp->offset; else ++wordp; break;
            case Fmore: printf("PopPop > "); b = ((sp--)->val).floating; if(((sp--)->val).floating <= b) wordp = word + wordp->offset; else ++wordp; break;
            case Funequal: printf("PopPop !="); b = ((sp--)->val).floating; if(((sp--)->val).floating == b) wordp = word + wordp->offset; else ++wordp; break;
            case Fequal: printf("PopPop =="); b = ((sp--)->val).floating; if(((sp--)->val).floating != b) wordp = word + wordp->offset; else ++wordp; break;

            case Plus:  printf("Pop plus  "); a = ((sp--)->val).floating; sp->val.floating += a; ++wordp; break;
            case Times:  printf("Pop times "); a = ((sp--)->val).floating; sp->val.floating *= a; ++wordp; break;

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
            case Fmax:  printf("Pop fdim  "); a = ((sp--)->val).floating; sp->val.floating = fmax(a, (sp->val).floating); ++wordp; break;
            case Atan2:  printf("Pop fmax  "); a = ((sp--)->val).floating; sp->val.floating = atan2(a, (sp->val).floating); ++wordp; break;
            case Fmin:  printf("Pop atan2 "); a = ((sp--)->val).floating; sp->val.floating = fmin(a, (sp->val).floating); ++wordp; break;
            case Fdim:  printf("Pop fmin  "); a = ((sp--)->val).floating; sp->val.floating = fdim(a, (sp->val).floating); ++wordp; break;
            case Fmod:  printf("Pop fmod  "); a = ((sp--)->val).floating; sp->val.floating = fmod(a, (sp->val).floating); ++wordp; break;
            case Hypot:  printf("Pop hypot "); a = ((sp--)->val).floating; sp->val.floating = hypot(a, (sp->val).floating); ++wordp; break;
            case Pow:    printf("Pop pow   "); a = ((sp--)->val).floating; sp->val.floating = pow(a, (sp->val).floating); ++wordp; break;
            case Subtract: printf("Pop subtract"); a = ((sp--)->val).floating; sp->val.floating = (sp->val).floating - a; ++wordp; break;
            case Divide: printf("Pop divide"); a = ((sp--)->val).floating; sp->val.floating = (sp->val).floating / a; ++wordp; break;
            case Tbl:
                {
                size_t rank = (size_t)((sp--)->val).floating;
                size_t size = 1;
                size_t* range = (size_t*)bmalloc(__LINE__, rank * sizeof(size_t));
                printf("Pop tbl   ");
                if(range != 0)
                    {
                    //printf("bmalloc:%s\n", "size_t[]");
                    for(size_t j = 0; j < rank; ++j)
                        {
                        range[j] = (size_t)((sp--)->val).floating; /* range[0] = range of last index*/
                        size *= range[j];
                        }
                    fortharray* arr = sp->arrp;
                    arr->size = size;
                    arr->index = 0;
                    assert(arr->pval == 0);
                    arr->pval = (forthvalue*)bmalloc(__LINE__, size * sizeof(forthvalue));
                    if(arr->pval)
                        {
                        //printf("bmalloc:%s\n", "forthvalue[]");
                        memset(arr->pval, 0, size * sizeof(forthvalue));
                        }
                    arr->rank = rank;
                    arr->range = range;
                    printf("%p size %zu index %zu", (void*)arr, arr->size, arr->index);
                    }
                ++wordp;
                break;
                }
            case Out:
                printf("%f\n", sp->val.floating); ++wordp; break;
                break;
            case Idx:
                {
                sp = getArrayIndex(sp, wordp);
                i = sp->arrp->index;
                if(i >= sp->arrp->size)
                    {
                    return FALSE;
                    }
                printf("Pop index   ");
                sp->arrp->index = i;
                ++wordp;
                break;
                }
            case QIdx:
                {
                sp = getArrayIndex(sp, wordp);
                i = sp->arrp->index;
                if(i >= sp->arrp->size)
                    {
                    fprintf(stderr, "Index past upper bound.\n");
                    return 0; /* Something wrong happened. */
                    }

                forthvalue* val = sp->arrp->pval + i;

                printf("PopPop ?index  ");
                assert(sp >= mem->stack);
                assert(sp >= mem->stack);
                *val = (--sp)->val;
                ++wordp;
                break;
                }
            case EIdx:
                {
                sp = getArrayIndex(sp, wordp);
                i = sp->arrp->index;
                if(i >= sp->arrp->size)
                    {
                    return FALSE;
                    }

                printf("Pop !index  ");
                assert(sp >= mem->stack);
                sp->arrp->index = i;
                sp->val = (sp->arrp->pval)[i];
                ++wordp;
                break;
                }
            case Range:
                {
                printf("Range       ");
                assert(sp >= mem->stack);
                sp = getArrayRange(sp);
                ++wordp;
                break;
                }
            case Rank:
                {
                printf("Rank        ");
                assert(sp >= mem->stack);
                sp = getArrayRank(sp);
                ++wordp;
                break;
                }
            case NoOp:
            case TheEnd:
            default:
                break;
            }
        assert(sp + 1 >= mem->stack);
        printf("\n");
        }
    return sp;
    }

static Boolean trc(struct typedObjectnode* This, ppsk arg)
    {
    psk Arg = (*arg)->RIGHT;
    forthMemory* mem = (forthMemory*)(This->voiddata);
    parameter* curparm = mem->parameters;
    if(mem)
        {
        if(curparm && setArgs(mem, 0, Arg) > 0)
            {
            stackvalue* sp = trcBody(mem);
            if(sp)
                {
                printf("calculation DONE. On Stack %d\n", (int)(sp - mem->stack));
                assert(sp + 1 >= mem->stack);
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
                        if(sv > DBL_MAX)
                            strcpy(buf, "INF");
                        else
                            strcpy(buf, "-INF");
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
                        //printf("bmalloc:%s\n", "char[]");
                        strcpy((char*)(res)+offsetof(sk, u.sobj), buf);
                        //strcpy((char*)SPOBJ(res), buf); /* gcc: generates buffer overflow. Why? (Addresses are the same!) */
                        printf("value on stack %s\n", buf);
                        wipe(*arg);
                        *arg = /*same_as_w*/(res);
                        res->v.fl = flags;
                        return TRUE;
                        }
                    /*
                    2.6700000000000000E+02
                    */
                    }
                }
            }
        else
            return FALSE;

        }
    return FALSE;
    }

static Boolean ftrc(stackvalue* sp, forthword* wordp, double* ret)
    {
    forthMemory* thatmem = wordp->u.that;
    if(sp && thatmem)
        {
        stackvalue* sp2;
        fsetArgs(sp, wordp->offset, thatmem);
        sp2 = trcBody(thatmem);
        if(sp2 && sp2 >= thatmem->stack)
            {
            *ret = thatmem->stack->val.floating;
            return TRUE;
            }
        else
            return FALSE;
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
        case EQUALS:
            return 0; /* Function definition disappears in final compiled code. */
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
            if(R == 0 || C == 0)
                return R + C; /* Function definition on the left and/or right side. */
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
                fprintf(stderr, "calculation: lhs of $ or ' is operator\n");
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


/*#define INDNT "%*s",4*In,""*/
#define INDNT " "

static Boolean printmem(forthMemory* mem)
    {
    if(mem == 0)
        return FALSE;
    char* naam;
    int In = 0;
    forthword* wordp = mem->word;
    printf("print\n");
    for(; wordp->action != TheEnd; ++wordp)
        {
        switch(wordp->action)
            {
            case ResolveAndPush:
                printf(INDNT); printf(LONGD " ResolveAndPush     %s\n", wordp - mem->word, getVarName(mem, (wordp->u.valp)));
                ++In;
                break;
            case ResolveAndGet:
                printf(INDNT); printf(LONGD " ResolveAndGet      %s\n", wordp - mem->word, getVarName(mem, (wordp->u.valp)));
                break;
            case RslvPshArrElm:
                printf(INDNT); printf(LONGD " RslvPshArrElm      %s\n", wordp - mem->word, wordp->u.arrp->name);
                break;
            case RslvGetArrElm:
                printf(INDNT); printf(LONGD " RslvGetArrElm      %s\n", wordp - mem->word, wordp->u.arrp->name);
                break;
            case Push:
                {
                forthvalue val = wordp->u.val;
                printf(INDNT); printf(LONGD " Push               %f or @%p\n", wordp - mem->word, val.floating, (void*)wordp->u.arrp);
                ++In;
                break;
                }
            case Afunction:
#if CFUNC
                naam = getFuncName(wordp->u.funcp);
                printf(INDNT); printf(LONGD " Afunction       %u %s\n", wordp - mem->word, wordp->offset, naam);
#endif
                break;
            case Pop:
                naam = getLogiName(wordp->u.logic);
                printf(INDNT); printf(LONGD " Pop             %u %s\n", wordp - mem->word, wordp->offset, naam);
                --In;
                break;
            case UncondBranch:
                printf(INDNT); printf(LONGD " UncondBranch    %u\n", wordp - mem->word, wordp->offset);
                break;
            case PopUncondBranch:
                naam = getLogiName(wordp->u.logic);
                printf(INDNT); printf(LONGD " PopUncondBranch %u %s\n", wordp - mem->word, wordp->offset, naam);
                --In;
                break;

            case Fless: printf(INDNT); printf(LONGD " PopPop <               %u\n", wordp - mem->word, wordp->offset); --In; --In; break;
            case Fless_equal: printf(INDNT); printf(LONGD " PopPop <=              %u\n", wordp - mem->word, wordp->offset); --In; --In; break;
            case Fmore_equal: printf(INDNT); printf(LONGD " PopPop >=              %u\n", wordp - mem->word, wordp->offset); --In; --In; break;
            case Fmore: printf(INDNT); printf(LONGD " PopPop >               %u\n", wordp - mem->word, wordp->offset); --In; --In; break;
            case Funequal: printf(INDNT); printf(LONGD " PopPop !=              %u\n", wordp - mem->word, wordp->offset); --In; --In; break;
            case Fequal: printf(INDNT); printf(LONGD " PopPop ==              %u\n", wordp - mem->word, wordp->offset); --In; --In; break;

            case Plus:  printf(INDNT); printf(LONGD " Pop plus   \n", wordp - mem->word); --In; break;
            case Times:  printf(INDNT); printf(LONGD " Pop times  \n", wordp - mem->word); --In; break;

            case Acos:  printf(INDNT); printf(LONGD " acos   \n", wordp - mem->word); break;
            case Acosh:  printf(INDNT); printf(LONGD " acosh  \n", wordp - mem->word); break;
            case Asin:  printf(INDNT); printf(LONGD " asin   \n", wordp - mem->word); break;
            case Asinh:  printf(INDNT); printf(LONGD " asinh  \n", wordp - mem->word); break;
            case Atan:  printf(INDNT); printf(LONGD " atan   \n", wordp - mem->word); break;
            case Atanh:  printf(INDNT); printf(LONGD " atanh  \n", wordp - mem->word); break;
            case Cbrt:  printf(INDNT); printf(LONGD " cbrt   \n", wordp - mem->word); break;
            case Ceil:  printf(INDNT); printf(LONGD " ceil   \n", wordp - mem->word); break;
            case Cos:  printf(INDNT); printf(LONGD " cos    \n", wordp - mem->word); break;
            case Cosh:  printf(INDNT); printf(LONGD " cosh   \n", wordp - mem->word); break;
            case Exp:  printf(INDNT); printf(LONGD " exp    \n", wordp - mem->word); break;
            case Fabs:  printf(INDNT); printf(LONGD " fabs   \n", wordp - mem->word); break;
            case Floor:  printf(INDNT); printf(LONGD " floor  \n", wordp - mem->word); break;
            case Log:  printf(INDNT); printf(LONGD " log    \n", wordp - mem->word); break;
            case Log10:  printf(INDNT); printf(LONGD " log10  \n", wordp - mem->word); break;
            case Sin:  printf(INDNT); printf(LONGD " sin    \n", wordp - mem->word); break;
            case Sinh:  printf(INDNT); printf(LONGD " sinh   \n", wordp - mem->word); break;
            case Sqrt:  printf(INDNT); printf(LONGD " sqrt   \n", wordp - mem->word); break;
            case Tan:  printf(INDNT); printf(LONGD " tan    \n", wordp - mem->word); break;
            case Tanh:  printf(INDNT); printf(LONGD " tanh   \n", wordp - mem->word); break;
            case Fdim:  printf(INDNT); printf(LONGD " Pop fdim   \n", wordp - mem->word); --In; break;
            case Fmax:  printf(INDNT); printf(LONGD " Pop fmax   \n", wordp - mem->word); --In; break;
            case Atan2:  printf(INDNT); printf(LONGD " Pop atan2  \n", wordp - mem->word); --In; break;
            case Fmin:  printf(INDNT); printf(LONGD " Pop fmin   \n", wordp - mem->word); --In; break;
            case Fmod:  printf(INDNT); printf(LONGD " Pop fmod   \n", wordp - mem->word); --In; break;
            case Hypot:  printf(INDNT); printf(LONGD " Pop hypot  \n", wordp - mem->word); --In; break;
            case Pow:  printf(INDNT); printf(LONGD      " Pop pow    \n", wordp - mem->word); --In; break;
            case Subtract:  printf(INDNT); printf(LONGD " Pop subtract\n", wordp - mem->word); --In; break;
            case Divide:  printf(INDNT); printf(LONGD   " Pop divide \n", wordp - mem->word); --In; break;
            case Tbl:  printf(INDNT); printf(LONGD " Pop tbl    \n", wordp - mem->word); --In; break;
            case Out:  printf(INDNT); printf(LONGD " Pop out    \n", wordp - mem->word); --In; break;
            case Idx:  printf(INDNT); printf(LONGD " Pop idx    \n", wordp - mem->word); --In; break;
            case QIdx:  printf(INDNT); printf(LONGD " PopPop Qidx    \n", wordp - mem->word); --In; --In; break;
            case EIdx:  printf(INDNT); printf(LONGD " Pop Eidx    \n", wordp - mem->word); --In; break;
            case Range: printf(INDNT); printf(LONGD " Pop range  \n", wordp - mem->word); --In; break;
            case Rank: printf(INDNT); printf(LONGD " Pop rank   \n", wordp - mem->word); --In; break;
            case NoOp:
#if CFUNC
                naam = getFuncName(wordp->u.funcp);
                printf(INDNT); printf(LONGD " NoOp       %s\n", wordp - mem->word, naam);
#endif
                break;
            case TheEnd:
            default:
                printf(INDNT); printf(LONGD " default         %d\n", wordp - mem->word, wordp->action);
                ;
            }
        }
    printf(INDNT); printf(LONGD " TheEnd          \n", wordp - mem->word);
    return TRUE;
    }

static Boolean print(struct typedObjectnode* This, ppsk arg)
    {
    forthMemory* mem = (forthMemory*)(This->voiddata);
    return printmem(mem);
    }

enum formt { floating, integer, fraction, hexadecimal };

static enum formt getFormat(char* psobj)
    {
    if(!strcmp(psobj, "R"))
        return floating;
    else if(!strcmp(psobj, "N"))
        return integer;
    else if(!strcmp(psobj, "Q"))
        return fraction;
    else if(!strcmp(psobj, "%a"))
        return hexadecimal;
    return floating;
    }

static psk createOperatorNode(int operator)
    {
    assert(operator == DOT || operator == COMMA || operator == WHITE);
    psk operatorNode = (psk)bmalloc(__LINE__, sizeof(knode));
    if(operatorNode)
        {
        //        printf("bmalloc:%s\n", "knode");
        operatorNode->v.fl = (operator | SUCCESS | READY) & COPYFILTER;
        //operatorNode->v.fl &= COPYFILTER;
        operatorNode->LEFT = 0;
        operatorNode->RIGHT = 0;
        }
    return operatorNode;
    }

static psk FloatNode(double val)
    {
    char jotter[500];
    size_t bytes = offsetof(sk, u.obj) + 1;
    bytes += sprintf(jotter, "%e", val);
    psk res = (psk)bmalloc(__LINE__, bytes);
    if(res)
        {
        //printf("bmalloc:%s\n", "psk");
        strcpy((char*)(res)+offsetof(sk, u.sobj), jotter);
        res->v.fl = READY | SUCCESS BITWISE_OR_SELFMATCHING;
        res->v.fl &= COPYFILTER;
        }
    return res;
    }

static psk HexNode(double val)
    {
    char jotter[500];
    size_t bytes = offsetof(sk, u.obj) + 1;
    bytes += sprintf(jotter, "%a", val);
    psk res = (psk)bmalloc(__LINE__, bytes);
    if(res)
        {
        //printf("bmalloc:%s\n", "psk");
        strcpy((char*)(res)+offsetof(sk, u.sobj), jotter);
        res->v.fl = READY | SUCCESS BITWISE_OR_SELFMATCHING;
        res->v.fl &= COPYFILTER;
        }
    return res;
    }

static psk IntegerNode(double val)
    {
    char jotter[500];
    size_t bytes = offsetof(sk, u.obj) + 1;
    if(val < 0)
        bytes += sprintf(jotter, "%d", (int)-val);
    else
        bytes += sprintf(jotter, "%d", (int)val);
    psk res = (psk)bmalloc(__LINE__, bytes);
    if(res)
        {
        //printf("bmalloc:%s\n", "psk");
        strcpy((char*)(res)+offsetof(sk, u.sobj), jotter);
        res->v.fl = READY | SUCCESS | QNUMBER BITWISE_OR_SELFMATCHING;
        res->v.fl &= COPYFILTER;
        if(val < 0)
            res->v.fl |= MINUS;
        else if(val == 0)
            res->v.fl |= QNUL;
        }
    return res;
    }

static psk FractionNode(double val)
    {
    char jotter[500];
    size_t bytes = offsetof(sk, u.obj) + 1;
#if defined __EMSCRIPTEN__ 
    long long long1 = (long long)1;
#else
    LONG long1 = (LONG)1;
#endif
    double fcac = (double)(long1 << 52);
    int exponent;
    ULONG flg = READY | SUCCESS | QNUMBER BITWISE_OR_SELFMATCHING;
    if(val < 0)
        {
        flg |= MINUS;
        val = -val;
        }
    else if(val == 0)
        flg |= QNUL;
    double mantissa = frexp(val, &exponent);
    LONG Mantissa = (LONG)(fcac * mantissa);

    if(Mantissa)
        {
        int shft;
        for(shft = 52 - exponent; (Mantissa & long1) == 0; --shft, Mantissa >>= 1)
            ;

        if(shft == 0)
            bytes += sprintf(jotter, LONGD "\n", Mantissa);
        else
            {
            bytes += sprintf(jotter, LONGD "/" LONGD "\n", Mantissa, (LONG)(long1 << shft));
            flg |= QFRACTION;
            }
        }
    else
        bytes += sprintf(jotter, "0 ");

    psk res = (psk)bmalloc(__LINE__, bytes);
    if(res)
        {
        //printf("bmalloc:%s\n", "psk");
        strcpy((char*)(res)+offsetof(sk, u.sobj), jotter);
        res->v.fl = flg;
        res->v.fl &= COPYFILTER;
        }
    return res;
    }

static psk eksportArray(forthvalue* val, size_t rank, size_t* range)
    {
    psk res;
    psk head = createOperatorNode(COMMA);
    res = head;
    head->LEFT = same_as_w(&nilNode);
    if(rank > 1)
        { /* Go deeper */
        /*
        printf("range\n");
        for(int j = 0; j < rank; ++j)
            printf("%d ", range[j]);
        printf("\n");
        */
        size_t stride = 1;
        for(size_t k = 0; k < rank - 1; ++k)
            stride *= range[k];
        for(size_t r = range[rank - 1]; --r > 0; val += stride)
            {
            head->RIGHT = createOperatorNode(WHITE);
            head = head->RIGHT;
            head->LEFT = eksportArray(val, rank - 1, range);
            }
        head->RIGHT = eksportArray(val, rank - 1, range);
        }
    else
        { /* Export list of values. */
        for(size_t r = *range; --r > 0; ++val)
            {
            head->RIGHT = createOperatorNode(WHITE);
            head = head->RIGHT;
            head->LEFT = xprtfnc(val->floating);
            }
        head->RIGHT = xprtfnc(val->floating);
        }
    return res;
    }

static Boolean eksport(struct typedObjectnode* This, ppsk arg)
    {
    forthMemory* mem = (forthMemory*)(This->voiddata);
    if(mem)
        {
        fortharray** arrp = &(mem->arr);
        forthvariable* varp = (mem->var);
        psk Arg = (*arg)->RIGHT;
        char* name = "";
        if(is_op(Arg))
            {
            psk lhs = Arg->LEFT;
            psk rhs = Arg->RIGHT;
            enum formt format = floating;
            if(!is_op(lhs))
                {
                format = getFormat(&lhs->u.sobj);
                }
            switch(format)
                {
                case floating:
                    xprtfnc = FloatNode;
                    break;
                case integer:
                    xprtfnc = IntegerNode;
                    break;
                case fraction:
                    xprtfnc = FractionNode;
                    break;
                case hexadecimal:
                    xprtfnc = HexNode;
                    break;
                }
            if(!is_op(rhs))
                {
                name = &rhs->u.sobj;
                forthvariable* v = getVariablePointer(varp, name);

                if(v)
                    {
                    psk res = 0;
                    res = xprtfnc(v->val.floating);
                    if(res)
                        {
                        wipe(*arg);
                        *arg = res;
                        //*arg = same_as_w(res);
                        res->v.fl = READY | SUCCESS;
                        return TRUE;
                        }
                    }
                else
                    {
                    fortharray* a = getArrayPointer(arrp, name);
                    if(a)
                        {
                        psk res = 0;
                        res = eksportArray(a->pval, a->rank, a->range);
                        if(res)
                            {
                            wipe(*arg);
                            *arg = res;
                            return TRUE;
                            }
                        }
                    }
                }
            }
        }
    return TRUE;
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
                forthword* label;
                if(wordp->u.logic == fand)
                    {
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
                        if(label->action == TheEnd)
                            (wordp->offset) += 1;
                        else if(label->u.logic == fand)             /* whl'(FAIL&BLA)&X            Jump to X for leaving the loop.                             */
                            (wordp->offset) += 2;              /* 1 for the end of the loop, 1 for the AND */
                        else if(label->u.logic == fOr        /* whl'(FAIL&BLA)|X            From FAIL jump to the offset of the OR for leaving the loop. X is unreachable! */
                                || label->u.logic == fwhl      /* whl'(ABC&whl'(FAIL&BLB))    From FAIL jump to the offset of the WHL, i.e. the start of the outer loop.                      */
                                )
                            {
                            do {
                                wordp->offset = label->offset;
                                label = mem->word + wordp->offset;
                                } while(label->action == Pop);
                            }
                        }
                    }
                else if(wordp->u.logic == fOr)
                    {
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
            case Subtract:
            case Divide:
            case Tbl:
            case Out:
            case Idx:
            case QIdx:
            case EIdx:
            case Range:
            case Rank:
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
            case Subtract:
            case Divide:
            case Tbl:
            case Out:
            case Idx:
            case QIdx:
            case EIdx:
            case Range:
            case Rank:
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
    int fwhlseen = 0;
    for(wordp = mem->word; wordp->action != TheEnd; ++wordp)
        {
        ++cells;
        }
    ++cells;
    arr = (int*)bmalloc(__LINE__, sizeof(int) * cells + 10);
    if(arr)
        {
        //printf("bmalloc:%s\n", "int[]");
        cells = 0;

        for(wordp = mem->word; wordp->action != TheEnd; ++wordp)
            {
            arr[cells] = skipping;
            if(fwhlseen)
                {
                fwhlseen = 0;
                }
            else if(wordp->action != NoOp)
                {
                ++skipping;
                if(wordp->action == PopUncondBranch)
                    fwhlseen = 1;
                }
            ++cells;
            }
        arr[cells] = skipping;
        }
    if(skipping == 0)
        return;
    newword = bmalloc(__LINE__, skipping * sizeof(forthword) + 10);
    if(newword)
        {
        //printf("bmalloc:%s\n", "forthword[]");
        cells = 0;
        fwhlseen = 0;
        for(wordp = mem->word, newwordp = newword; wordp->action != TheEnd; ++wordp)
            {
            if(fwhlseen)
                {
                fwhlseen = 0;
                }
            else if(wordp->action != NoOp)
                {
                *newwordp = *wordp;
                newwordp->offset = arr[wordp->offset];
                ++newwordp;
                if(wordp->action == PopUncondBranch)
                    fwhlseen = 1;
                }
            ++cells;
            }
        *newwordp = *wordp;
        assert(wordp->action == TheEnd);
        newwordp->offset = 0;
        bfree(mem->word);
        bfree(arr);
        mem->word = newword;
        }
    }


#define VARCOMP (NOT|GREATER_THAN|SMALLER_THAN|INDIRECT)

#define          VLESS(psk)             (((psk)->v.fl & (VARCOMP)) == (INDIRECT|SMALLER_THAN))
#define    VLESS_EQUAL(psk)             (((psk)->v.fl & (VARCOMP)) == (INDIRECT|NOT|GREATER_THAN))
#define    VMORE_EQUAL(psk)             (((psk)->v.fl & (VARCOMP)) == (INDIRECT|NOT|SMALLER_THAN))
#define          VMORE(psk)             (((psk)->v.fl & (VARCOMP)) == (INDIRECT|GREATER_THAN))
#define       VUNEQUAL(psk)             (((psk)->v.fl & (VARCOMP)) == (INDIRECT|NOT))
#define    VLESSORMORE(psk)             (((psk)->v.fl & (VARCOMP)) == (INDIRECT|SMALLER_THAN|GREATER_THAN))
#define         VEQUAL(psk)             (((psk)->v.fl & (VARCOMP)) == (INDIRECT))
#define VNOTLESSORMORE(psk)             (((psk)->v.fl & (VARCOMP)) == (INDIRECT|NOT|SMALLER_THAN|GREATER_THAN))

static forthMemory* calcnew(psk arg);

static void setArity(forthword* wordp, psk code)
    {
    int arity = 1;
    psk tmp = code->RIGHT;
    for(; is_op(tmp) && Op(tmp) == COMMA; tmp = tmp->RIGHT)
        ++arity;
    if(arity == 1)
        {
        /*TODO: check whether function does not take a single argument!*/
        }
    wordp->offset = arity;
    }

static fortharray* haveArray(forthMemory* forthstuff, psk declaration)
    {
    psk pname = 0;
    psk ranges = 0;
    printf("haveArray::decl: ");
    result(declaration);
    printf("\n");

    if(is_op(declaration))
        {
        if(is_op(declaration->LEFT))
            {
            fprintf(stderr, "Array parameter declaration requires name.\n");
            return FALSE;
            }
        pname = declaration->LEFT;
        ranges = declaration->RIGHT;
        }
    else
        {
        pname = declaration;
        //        fprintf(stderr, "Array parameter declaration takes range spec.\n");
        //        return FALSE;
        }

    if(HAS_VISIBLE_FLAGS_OR_MINUS(pname))
        {
        fprintf(stderr, "Array name may not have any prefixes.\n");
        return FALSE;
        }

    fortharray* a = getOrCreateArrayPointer(&(forthstuff->arr), &(pname->u.sobj), 0);
    if(a && ranges)
        {
        size_t rank = 0;
        psk kn;
        for(kn = ranges; is_op(kn); kn = kn->RIGHT)
            {
            ++rank;
            }
        if(INTEGER_POS(kn))
            ++rank;
        a->rank = rank;
        assert(a->range == 0);
        a->range = (size_t*)bmalloc(__LINE__, rank * sizeof(size_t));
        size_t* prange;
        size_t totsize = 1;
        for(kn = ranges, prange = a->range + rank - 1;; )
            {
            psk H;
            if(is_op(kn))
                H = kn->LEFT;
            else
                H = kn;
            if(INTEGER_POS(H))
                *prange = strtod(&(H->u.sobj), 0);
            else if((H->v.fl & VISIBLE_FLAGS) == INDIRECT)
                *prange = 0; /* range still unknown. */
            else
                {
                fprintf(stderr, "Range specification of array \"%s\" invalid. It must be a positive number or a variable name prefixed with '!'\n", a->name);
                //bfree(a->range);
                //a->range = 0;
                return 0;
                }
            totsize *= *prange;
            if(!is_op(kn))
                break;
            kn = kn->RIGHT;
            --prange;
            }
        a->size = totsize;
        }
    return a;
    }

static forthword* polish2(forthMemory* mem, psk code, forthword* wordp)
    {
    if(wordp == 0)
        return 0; /* Something wrong happened. */
    forthvariable** varp = &(mem->var);
    fortharray** arrp = &(mem->arr);
    switch(Op(code))
        {
        case EQUALS:
            {
            forthMemory* acalc = calcnew(code);
            if(acalc)
                {
                acalc->nextFnc = mem->nextFnc;
                mem->nextFnc = acalc;
                }
            return wordp;
            }
        case PLUS:
            wordp = polish2(mem, code->LEFT, wordp);
            wordp = polish2(mem, code->RIGHT, wordp);
            if(wordp == 0)
                return 0; /* Something wrong happened. */
            wordp->action = Plus;
            wordp->offset = 0;
            return ++wordp;
        case TIMES:
            wordp = polish2(mem, code->LEFT, wordp);
            wordp = polish2(mem, code->RIGHT, wordp);
            if(wordp == 0)
                return 0; /* Something wrong happened. */
            wordp->action = Times;
            wordp->offset = 0;
            return ++wordp;
        case EXP:
            wordp = polish2(mem, code->RIGHT, wordp); /* SIC! */
            wordp = polish2(mem, code->LEFT, wordp);
            if(wordp == 0)
                return 0; /* Something wrong happened. */
            wordp->action = Pow;
            wordp->offset = 0;
            return ++wordp;
        case LOG:
            wordp = polish2(mem, code->LEFT, wordp);
            wordp = polish2(mem, code->RIGHT, wordp);
            if(wordp == 0)
                return 0; /* Something wrong happened. */
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
            saveword = wordp;
            wordp = polish2(mem, code->LEFT, wordp);
            if(wordp == 0)
                return 0; /* Something wrong happened. */
            if(wordp == saveword)
                {
                wordp = polish2(mem, code->RIGHT, wordp);
                }
            else
                {
                saveword = wordp;
                saveword->action = Pop;
                saveword->u.logic = fand;
                wordp = polish2(mem, code->RIGHT, ++wordp);
                if(wordp == 0)
                    return 0; /* Something wrong happened. */
                saveword->offset = (unsigned int)(wordp - mem->word);
                }
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
            if(wordp == 0)
                return 0; /* Something wrong happened. */
            saveword = wordp;
            saveword->action = Pop;
            saveword->u.logic = fOr;
            wordp = polish2(mem, code->RIGHT, ++wordp);
            if(wordp == 0)
                return 0; /* Something wrong happened. */
            saveword->offset = (unsigned int)(ULONG)(wordp - mem->word); /* the address of the word after the 'if false' branch.*/
            return wordp;
            }
        case MATCH:
            {
            wordp = polish2(mem, code->LEFT, wordp);
            wordp = polish2(mem, code->RIGHT, wordp);
            if(wordp == 0)
                return 0; /* Something wrong happened. */
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
                else if(VLESS(code->RIGHT))
                    {
                    wordp->action = Fless;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(VLESS_EQUAL(code->RIGHT))
                    {
                    wordp->action = Fless_equal;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(VMORE_EQUAL(code->RIGHT))
                    {
                    wordp->action = Fmore_equal;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(VMORE(code->RIGHT))
                    {
                    wordp->action = Fmore;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(VUNEQUAL(code->RIGHT))
                    {
                    wordp->action = Funequal;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(VLESSORMORE(code->RIGHT))
                    {
                    wordp->action = Funequal;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(VEQUAL(code->RIGHT))
                    {
                    wordp->action = Fequal;
                    wordp->offset = 0;
                    return ++wordp;
                    }
                else if(VNOTLESSORMORE(code->RIGHT))
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
                {
                fprintf(stderr, "\"whl\" expected as left hand side of ' operator (apostrophe). Found \"%s\"\n", name);
                return 0; /* Something wrong happened. */
                }
            wordp = polish2(mem, code->RIGHT, wordp);
            if(wordp == 0)
                return 0; /* Something wrong happened. */
            wordp->action = Pop;
            wordp->u.logic = fwhl;
            wordp->offset = here; /* If all good, jump back to start of loop */
            return ++wordp;;
            }
        case FUN:
            {
            Epair* ep = epairs;
#if CFUNC
            Cpair* p = pairs;
#endif
            char* name = &code->LEFT->u.sobj;
            psk rhs = code->RIGHT;
            printf("FUN rhs:"); result(rhs); printf("\n");
            if(!strcmp(name, "idx") || !strcmp(name, "tbl"))
                { /* Check that name is array name and that arity is correct. */
                if(is_op(rhs))
                    {
                    if(!is_op(rhs->LEFT))
                        {
                        if((rhs->LEFT->v.fl & VISIBLE_FLAGS) == 0)
                            {
                            char* arrname = &rhs->LEFT->u.sobj;
                            fortharray* arr;
                            for(arr = mem->arr; arr; arr = arr->next)
                                {
                                if(!strcmp(arr->name, arrname))
                                    {
                                    break;
                                    }
                                }


                            if(arr == 0 && !strcmp(name, "tbl"))
                                {
                                arr = haveArray(mem, rhs);
                                }

                            if(arr == 0)
                                {
                                fprintf(stderr, "%s:Array \"%s\" is not declared\n", name, arrname);
                                return 0;
                                }

                            if(!strcmp(name, "idx"))
                                {
                                size_t h = 1;
                                psk R;
                                for(R = rhs; Op(R->RIGHT) == COMMA; R = R->RIGHT)
                                    ++h;
                                size_t rank = arr->rank;
                                if(rank == 0)
                                    {
//                                    fprintf(stderr, "%s: Array \"%s\" has unknown rank and range(s). Assuming %zu, based on idx.\n", name, arrname, h);
                                    rank = arr->rank = h;
//                                    return 0;
                                    }
                                if(h != rank)
                                    {
                                    fprintf(stderr, "%s: Array \"%s\" expects %zu arguments. %zu have been found\n", name, arrname, rank, h);
                                    return 0;
                                    }
                                if(arr->range != 0)
                                    {
                                    size_t* rng = arr->range + rank;
                                    for(R = rhs->RIGHT; ; R = R->RIGHT)
                                        {
                                        psk Arg;
                                        --rng;
                                        if(*rng != 0) /* If 0, range is still unknown. Has to wait until running.  */
                                            {
                                            if(is_op(R))
                                                Arg = R->LEFT;
                                            else
                                                Arg = R;
                                            if(INTEGER_NOT_NEG(Arg))
                                                {
                                                long index = strtod(&(Arg->u.sobj), 0);
                                                if(index < 0 || index >= (long)(*rng))
                                                    {
                                                    fprintf(stderr, "%s: Array \"%s\": index %zu is out of bounds 0 <= index < %zu, found %ld\n", name, arrname, rank - (rng - arr->range), *rng, index);
                                                    return 0;
                                                    }
                                                }
                                            }
                                        if(!is_op(R))
                                            break;
                                        }
                                    }
                                }
                            }
                        else
                            {
                            fprintf(stderr, "First argument of \"%s\" must be without any prefixes.\n", name);
                            return 0;
                            }
                        }
                    else
                        {
                        fprintf(stderr, "First argument of \"%s\" must be an atom.\n", name);
                        return 0;
                        }
                    }
                }
            else
                {
                /* Check whether built in or user defined. */

                forthMemory* funcs;
                for(funcs = mem->nextFnc; funcs; funcs = funcs->nextFnc)
                    {
                    if(!strcmp(funcs->name, name))
                        {
                        parameter* parms;
                        printf("%s: nparameters %zu\n", name, funcs->nparameters);
                        for(parms = funcs->parameters + funcs->nparameters; --parms >= funcs->parameters;)
                            {
                            printf("%s rhs:", name); result(rhs); printf("\n");

                            if(!is_op(rhs) && parms > funcs->parameters)
                                {
                                fprintf(stderr, "Too few parameters.\n");
                            //    return 0;
                                }
                            psk parm;
                            if(Op(rhs) == COMMA)
                                parm = rhs->LEFT;
                            else
                                parm = rhs;
                            if(parms->scalar_or_array == Array)
                                {
                                if(is_op(parm))
                                    {
                                    fprintf(stderr, "Array name expected.\n");
                                    return 0;
                                    }
                                else
                                    {
                                    if(HAS_VISIBLE_FLAGS_OR_MINUS(parm))
                                        {
                                        printf("%s rhs:", name); result(parm); printf("\n");
                                        fprintf(stderr, "When passing an array to a function, the array name must be free of any prefixes.\n");
                                        return 0;
                                        }
                                    }
                                }
                            if(is_op(rhs))
                                rhs = rhs->RIGHT;
                            else
                                {
                                assert(parms == funcs->parameters);
                                }
                            }
                        }
                    }

                }
            wordp = polish2(mem, code->RIGHT, wordp);
            if(wordp == 0)
                return 0; /* Something wrong happened. */
            wordp->offset = 0;
            if(code->v.fl & INDIRECT)
                {
                for(; ep->name != 0; ++ep)
                    {
                    if(!strcmp(ep->name + 1, name) && ep->name[0] == 'E') /* ! -> E(xclamation) */
                        {
                        wordp->action = ep->action;
                        if(wordp->action == EIdx)
                            setArity(wordp, code);
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
                        if(wordp->action == QIdx)
                            setArity(wordp, code);
                        return ++wordp;
                        }
                    }
                }
            else
                {
                for(; ep->name != 0; ++ep)
                    {
                    if(!strcmp(ep->name, name))
                        {
                        wordp->action = ep->action;
                        if(wordp->action == Idx || wordp->action == Tbl)
                            setArity(wordp, code);
                        return ++wordp;
                        }
                    }
                }

            wordp->action = Afunction;
#if CFUNC
            for(; p->name != 0; ++p)
                {
                if(!strcmp(p->name, name))
                    {
                    wordp->u.funcp = p->Cfun;
                    return ++wordp;
                    }
                }
#endif
            for(forthMemory* fm = mem->nextFnc; fm; fm = fm->nextFnc)
                {
                if(!strcmp(fm->name, name))
                    {
                    setArity(wordp, code);
                    wordp->action = Afunction;
                    wordp->u.that = fm;
                    return ++wordp;
                    }
                }
            fprintf(stderr, "Function named \"%s\" not found.\n", name);
            return 0; /* Something wrong happened. */
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
                    if(HAS_MINUS_SIGN(code))
                        {
                        wordp->u.val.floating = -(wordp->u.val.floating);
                        }
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
                        forthvariable* v = getVariablePointer(*varp, &(code->u.sobj));
                        if(v == 0)
                            {
                            if(argumentArrayNumber(code) >= 0)
                                {
                                fortharray* a = getOrCreateArrayPointerButNoArray(arrp, &(code->u.sobj));
                                if(a == 0)
                                    return 0; /* Something wrong happened. */
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
                                    forthvariable* var = createVariablePointer(varp, &(code->u.sobj));
                                    if(var)
                                        wordp->u.valp = &(var->val);
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
                            wordp->action = Push;
                            /* We can get here if an expression is empty, e.g.
                            in a case like
                                whl'(!n+1:<10:?n&(!n:>3&dosomething$|))*/
                            }
                        }
                    wordp->offset = 0;
                    }
                ++wordp;
                return wordp;
                };
        }
    }

static Boolean setparm(size_t Ndecl, forthMemory* forthstuff, psk declaration)
    {
    parameter* opar;
    parameter* npar;
    printf("setparm::decl[%zu]: ", Ndecl);
    result(declaration);
    printf("\n");
    if(is_op(declaration->LEFT))
        {
        fprintf(stderr, "Parameter declaration requires 's' or 'a'.\n");
        return FALSE;
        }

    if(declaration->LEFT->u.sobj == 's') // scalar
        {
        if(is_op(declaration->RIGHT))
            {
            fprintf(stderr, "Scalar parameter declaration doesn't take range.\n");
            return FALSE;
            }
        forthvariable* var = getVariablePointer(forthstuff->var, &(declaration->RIGHT->u.sobj));

        if(!var)
            {
            var = createVariablePointer(&(forthstuff->var), &(declaration->RIGHT->u.sobj));
            }

        if(var)
            {
            npar = forthstuff->parameters + Ndecl;
            npar->scalar_or_array = Scalar;
            npar->u.v = var;
            }
        }
    else // array
        {
        fortharray* a = haveArray(forthstuff, declaration->RIGHT);
        if(a)
            {
            npar = forthstuff->parameters + Ndecl;
            if(npar)
                {
                npar->scalar_or_array = Array;
                npar->u.a = a;
                }
            }
        else
            return FALSE;
        }
    return TRUE;
    }

static Boolean calcdie(forthMemory* mem);

static Boolean functiondie(forthMemory* mem)
    {
    //printf("functiondie %p\n", mem);
    if(!mem)
        {
        fprintf(stderr, "calculation.functiondie does not deallocate, member \"mem\" is zero.\n");
        return FALSE;
        }
    forthvariable* curvarp = mem->var;
    fortharray* curarr = mem->arr;
    forthMemory* functions = mem->nextFnc;
    parameter* parameters = mem->parameters;
    while(curvarp)
        {
        forthvariable* nextvarp = curvarp->next;
        if(curvarp->name)
            {
            //printf("bfree:%s\n", "char[]");
            bfree(curvarp->name);
            }
        //printf("bfree:%s\n", "forthvariable");
        bfree(curvarp);
        curvarp = nextvarp;
        }
    while(curarr)
        {
        fortharray* nextarrp = curarr->next;
        if(curarr->name)
            {
            /* range and pval are not allocated when function is called.
               Instead, pointers are set to caller's pointers. */
            parameter* pars = 0;
            for(pars = parameters; pars < parameters + mem->nparameters; ++pars)
                {
                if(pars->scalar_or_array == Array)
                    {
                    if(!strcmp(pars->u.a->name, curarr->name))
                        break;
                    }
                }

            if(pars == parameters + mem->nparameters)
                { /* This array is not a function parameter. So the function owns the data and should delete them. */
                if(curarr->range)
                    bfree(curarr->range);
                if(curarr->pval)
                    bfree(curarr->pval);
                }
            printf("bfree:char[%s]\n", curarr->name);
            bfree(curarr->name);
            }

        //printf("bfree:%s\n", "fortharray");
        bfree(curarr);
        curarr = nextarrp;
        }
    if(mem->parameters)    
        bfree(mem->parameters);
    for(; functions;)
        {
        forthMemory* nextfunc = functions->nextFnc;
        calcdie(functions);
        functions = nextfunc;
        }
    if(mem->name)
        {
        //printf("bfree:%s\n", "char[]");
        bfree(mem->name);
        }
    if(mem->word)
        {
        //printf("bfree:%s\n", "forthword");
        bfree(mem->word);
        }
    //printf("bfree:%s\n", "forthMemory");
    bfree(mem);
    return TRUE;
    }

static forthMemory* calcnew(psk arg)
    {
    int newval = 0;
    psk code, fullcode;
    psk declarations = 0;
    code = getValue(arg, &newval);
    fullcode = code;
    if(is_op(code) && Op(code) == DOT)
        {
        declarations = code->LEFT;
        code = code->RIGHT;
        }
    if(code)
        {
        char* name;
        forthword* lastword;
        forthMemory* forthstuff;
        int length;
        length = polish1(code);
        if(length < 0)
            {
            fprintf(stderr, "polish1 returns length < 0 [%d]\n", length);
            return 0; /* Something wrong happened. */
            }
        forthstuff = (forthMemory*)bmalloc(__LINE__, sizeof(forthMemory));
        if(forthstuff)
            {
            //printf("bmalloc:%s\n", "forthMemory");
            memset(forthstuff, 0, sizeof(forthMemory));
            forthstuff->name = 0;
            if(!is_op(arg->LEFT))
                {
                name = &(arg->LEFT->u.sobj);
                if(*name)
                    {
                    assert(forthstuff->name == 0);
                    forthstuff->name = (char*)bmalloc(__LINE__, strlen(name) + 1);
                    if(forthstuff->name)
                        {
                        //printf("bmalloc:%s\n", "char[]");
                        strcpy(forthstuff->name, name);
                        }
                    }
                forthstuff->nextFnc = 0;
                forthstuff->var = 0;
                forthstuff->arr = 0;
                assert(forthstuff->word == 0);
                forthstuff->word = bmalloc(__LINE__, length * sizeof(forthword) + 1);
                if(forthstuff->word)
                    {
                    //printf("bmalloc:%s\n", "forthword[]");
                    forthstuff->wordp = forthstuff->word;
                    forthstuff->sp = forthstuff->stack;
                    forthstuff->parameters = 0;
                    if(declarations)
                        {
                        psk decl;
                        size_t Ndecl = 0;
                        printf("Determine Ndecl"); result(declarations); printf("\n");
                        for(decl = declarations; decl && is_op(decl); decl = decl->RIGHT)
                            {
                            ++Ndecl;
                            if(Op(decl) == DOT) /*Just one parameter that is an array*/
                                break;
                            }

                        forthstuff->nparameters = Ndecl;
                        if(Ndecl > 0)
                            {
                            forthstuff->parameters = bmalloc(__LINE__, Ndecl * sizeof(parameter));
                            if(forthstuff->parameters != 0)
                                {
                                for(decl = declarations; Ndecl-- > 0 && decl && is_op(decl); decl = decl->RIGHT)
                                    {
                                    if(Op(decl) == DOT)
                                        {
                                        if(!setparm(Ndecl, forthstuff, decl))
                                            {
                                            fprintf(stderr, "Error in parameter declaration.\n");
                                            return 0; /* Something wrong happened. */
                                            }
                                        break;
                                        }
                                    else
                                        {
                                        if(is_op(decl->LEFT) && Op(decl->LEFT) == DOT)
                                            if(!setparm(Ndecl, forthstuff, decl->LEFT))
                                                {
                                                fprintf(stderr, "Error in parameter declaration.\n");
                                                return 0; /* Something wrong happened. */
                                                }
                                        }
                                    }
                                }
                            }
                        }
                    lastword = polish2(forthstuff, code, forthstuff->wordp);
                    if(lastword != 0)
                        {
                        lastword->action = TheEnd;
                        if(newval)
                            wipe(fullcode);
                        /*
                            printf("Not optimized:\n");
                            printmem(forthstuff);
                        //*/
                        optimizeJumps(forthstuff);
                        /*
                            printf("Optimized jumps:\n");
                            printmem(forthstuff);
                        //*/
                        combineTestsAndJumps(forthstuff);
                        /*
                            printf("Combined testst and jumps:\n");
                            printmem(forthstuff);
                        //*/
                        compaction(forthstuff);
                        /*
                            printf("Compacted:\n");
                            printmem(forthstuff);
                        //*/
                        return forthstuff;
                        }
                    }
                }
            }
        calcdie(forthstuff);
        }
    wipe(fullcode);
    fprintf(stderr, "Compilation of calculation object failed.\n");
    return 0; /* Something wrong happened. */
    }

static Boolean calculationnew(struct typedObjectnode* This, ppsk arg)
    {
    if(is_op(*arg))
        {
        forthMemory* forthstuff = calcnew((*arg)->RIGHT);
        if(forthstuff)
            {
            This->voiddata = forthstuff;
            return TRUE;
            }
        }
    return FALSE;
    }

static Boolean calcdie(forthMemory* mem)
    {
    if(mem != 0)
        {
        forthvariable* curvarp = mem->var;
        fortharray* curarr = mem->arr;
        forthMemory* functions = mem->nextFnc;
        parameter* parameters = mem->parameters;
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
            if(curarr->range)
                bfree(curarr->range);
            if(curarr->pval)
                bfree(curarr->pval);
            bfree(curarr);
            curarr = nextarrp;
            }
        if(mem->parameters)
            bfree(mem->parameters);
        for(; functions;)
            {
            forthMemory* nextfunc = functions->nextFnc;
            functiondie(functions);
            functions = nextfunc;
            }
        if(mem->name)
            bfree(mem->name);
        bfree(mem->word);
        bfree(mem);
        }
    return TRUE;
    }

static Boolean calculationdie(struct typedObjectnode* This, ppsk arg)
    {
    return calcdie((forthMemory*)(This->voiddata));
    }

method calculation[] = {
    {"calculate",calculate},
    {"trc",trc},
    {"print",print},
    {"export",eksport},
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

