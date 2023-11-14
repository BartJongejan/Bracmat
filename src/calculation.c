#include "calculation.h"
#include "variables.h"
#include "nodedefs.h"
#include "memory.h"
#include "wipecopy.h"
#include "input.h"
#include "globals.h" /*nilNode*/
#include "writeerr.h"
#include "filewrite.h"
#include "eval.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <float.h>

#include "result.h" /* For debugging. Remove when done. */

#define TOINT(a) ((size_t)ceil(fabs(a)))

typedef enum { epop, enopop } popping;
static popping mustpop = enopop;

typedef enum
    {
    TheEnd
    , varPush
    , var2stack // copy variable to stack w/o incrementing stack
    , var2stackBranch // same, then jump
    , stack2var // copy stack to variable w/o decrementing stack
    , stack2varBranch // same, then jump
    , ArrElmValPush // push value of array alement
    , stack2ArrElm
    , val2stack // copy value to stack w/o incrementing stack
    , valPush
    , Afunction
    , Pop // decrement stack
    , Branch
    , PopBranch
    , valPushBranch
    , val2stackBranch
    , Fless
    , Fless_equal
    , Fmore_equal
    , Fmore
    , Funequal
    , Fequal
    , FlessP
    , Fless_equalP
    , Fmore_equalP
    , FmoreP
    , FunequalP
    , FequalP
    , Plus
    , varPlus
    , valPlus
    , Times
    , varTimes
    , valTimes
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
    , Sign
    , Sin
    , Sinh
    , Cube
    , Sqr
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
    , varSubtract
    , valSubtract
    , Divide
    , varDivide
    , valDivide
    , Drand
    , Tbl
    , Out
    , Outln
    , Idx   /*   idx$(<array name>,<index>,...)  */
    , QIdx  /* ?(idx$(<array name>,<index>,...)) */
    , EIdx  /* !(idx$(<array name>,<index>,...)) */
    , Extent /* extent$(<array name>,d)) 0 <= d < rank, returns 0 if d < 0 or d >= array rank */
    , Rank  /* rank$(<array name>)) */
    , Eval /* callback, evaluate normal Bracmat code */
    , NoOp
    } actionType;

static char* ActionAsWord[] =
    { "TheEnd"
    ,"varPush"
    ,"var2stack"
    ,"var2stackBranch"
    ,"stack2var"
    ,"stack2varBranch"
    ,"ArrElmValPush"
    ,"stack2ArrElm"
    ,"val2stack"
    ,"valPush"
    ,"Afunction"
    ,"Pop"
    ,"Branch"
    ,"PopBranch"
    ,"valPushBranch"
    ,"val2stackBranch"
    ,"F<"
    ,"F<="
    ,"F>="
    ,"F>"
    ,"F!="
    ,"F=="
    ,"F<P"
    ,"F<=P"
    ,"F>=P"
    ,"F>P"
    ,"F!=P"
    ,"F==P"
    ,"Plus"
    ,"varPlus"
    ,"valPlus"
    ,"Times"
    ,"varTimes"
    ,"valTimes"
    ,"Acos"
    ,"Acosh"
    ,"Asin"
    ,"Asinh"
    ,"Atan"
    ,"Atanh"
    ,"Cbrt"
    ,"Ceil"
    ,"Cos"
    ,"Cosh"
    ,"Exp"
    ,"Fabs"
    ,"Floor"
    ,"Log"
    ,"Log10"
    ,"Sign"
    ,"Sin"
    ,"Sinh"
    ,"Cube"
    ,"Sqr"
    ,"Sqrt"
    ,"Tan"
    ,"Tanh"
    ,"Atan2"
    ,"Fdim"
    ,"Fmax"
    ,"Fmin"
    ,"Fmod"
    ,"Hypot"
    ,"Pow"
    ,"Subtract"
    ,"varSubtract"
    ,"valSubtract"
    ,"Divide"
    ,"varDivide"
    ,"valDivide"
    ,"Drand"
    ,"tbl"
    ,"out"
    ,"outln"
    ,"idx"
    ,"Qidx"
    ,"Eidx"
    ,"extent"
    ,"rank"
    ,"eval"
    ,"NoOp"
    };

struct forthMemory;

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
    size_t* extent;
    size_t* stride; // Product of extents
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

typedef enum { Scalar, Array, Neither } scalarORarray;

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
    fother = 0, fand, fand2, fOr, fOr2, cpush, fwhl
    } dumbl;

static char* logics[] =
    { "fother"
    , "fand "
    , "fand2"
    , "fOr  "
    , "fOr2 "
    , "cpush"
    , "fwhl "
    };

struct forthMemory;

#define CALCULATION_PROFILING 0
typedef struct forthword
    {
#if CALCULATION_PROFILING
    actionType action : 8;
    unsigned int count : 24;
    unsigned int offset : 32; /* Interpreted as arity if action is Afunction */
#else
    actionType action;
    unsigned int offset; /* Interpreted as arity if action is Afunction */
#endif
    union
        {
        fortharray* arrp; forthvalue* valp; forthvalue val; dumbl logic; struct forthMemory* that; psk Pnode /*callback*/;
        } u;
    } forthword;

#if CALCULATION_PROFILING
#define INDNT " %8u ",wordp->count
#else
#define INDNT " "
#endif

typedef enum { estart, epopS, eS, epopF, eF, esentinel } ejump;

typedef struct jumpblock
    {
    forthword j[esentinel]; /* estart ... eF */
    } jumpblock;

typedef struct forthMemory
    {
    char* name;
    struct forthMemory* functions; /* points to first of child functions. */
    struct forthMemory* nextFnc; /* points to sibling function */
    struct forthMemory* parent; /* points to parent, or is null */
    forthword* word; /* fixed once calculation is compiled */
    forthword* wordp; /* runs through words when calculating */
    forthvariable* var;
    fortharray* arr;
    stackvalue* sp;
    parameter* parameters;
    size_t nparameters;
    stackvalue stack[64];
    } forthMemory;

typedef struct Etriple
    {
    char* name;
    actionType action;
    unsigned int arity;
    }Etriple;

typedef struct
    {
    actionType Afun;
    actionType Bfun; /* negation of Afun */
    }actionPair;

static void showProblematicNode(char* msg, psk node)
    {
    errorprintf("%s", msg);
    writeError(node);
    errorprintf("\n");
    }

static double drand(double extent)
    {
    return extent * (double)rand() / ((double)RAND_MAX + 1.0);
    }

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
    forthvariable* newvarp = (forthvariable*)bmalloc(sizeof(forthvariable));
    if(newvarp)
        {
        newvarp->name = bmalloc(strlen(name) + 1);
        if(newvarp->name)
            {
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
    //assert(curarrp->pval == 0);
    assert(curarrp->pval == 0 || curarrp->size == size);
    if(curarrp->pval == 0)
        curarrp->pval = (forthvalue*)bmalloc(size * sizeof(forthvalue));
    if(curarrp->pval)
        {
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
        errorprintf("Array name is empty string.\n");
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
        *arrp = (fortharray*)bmalloc(sizeof(fortharray));
        if(*arrp)
            {
            memset(*arrp, 0, sizeof(fortharray));
            (*arrp)->next = curarrp;
            curarrp = *arrp;
            assert(curarrp->name == 0);
            curarrp->name = bmalloc(strlen(name) + 1);
            if(curarrp->name)
                {
                strcpy(curarrp->name, name);
                /* already done by memset()
                curarrp->rank = 0;
                curarrp->extent = 0;
                curarrp->index = 0;
                curarrp->size = 0;
                curarrp->pval = 0;
                */
                }
            else
                {
                errorprintf("Cannot allocate memory for array name \"%s\".\n", name);
                return 0;
                }
            }
        else
            {
            errorprintf("Cannot allocate memory for array struct.\n");
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
        if(*arrp == 0)
            {
            *arrp = (fortharray*)bmalloc(sizeof(fortharray));
            if(*arrp)
                {
                curarrp = *arrp;
                memset(curarrp, 0, sizeof(fortharray));
                curarrp->name = bmalloc(strlen(name) + 1);
                if(curarrp->name)
                    {
                    strcpy(curarrp->name, name);
                    /*
                    curarrp->next = 0;
                    curarrp->pval = 0;
                    curarrp->size = 0;
                    curarrp->index = 0;
                    curarrp->rank = 0;
                    curarrp->extent = 0;
                    curarrp = *arrp;
                    */
                    }
                }
            }
        else
            {
            showProblematicNode("Are you sure this is an array? Or did you forget a \"!\" ?", 0);
            showProblematicNode(name, 0);
            return 0;
            }
        }
    if(!*arrp)
        *arrp = curarrp;
    return curarrp;
    }

static Boolean setFloat(double* destination, psk args)
    {
    if(args->v.fl & QDOUBLE)
        {
        *destination = strtod(&(args->u.sobj), 0);
        if(HAS_MINUS_SIGN(args))
            {
            *destination = -(*destination);
            }
        return TRUE;
        }
    else if(INTEGER_COMP(args))
        {
        *destination = strtod(&(args->u.sobj), 0);
        if(isinf(*destination))
            {
            /*'((s.value).!value):?code & new$(UFP,!code):?test &  (test..go)$555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555555 */
            errorprintf("'%s' cannot be represented as a double ('infinite' value)\n", &(args->u.sobj));
            return FALSE;
            }
        if(HAS_MINUS_SIGN(args))
            *destination = -*destination;
        return TRUE;
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
            return TRUE;
            }
        }
    else if(args->u.sobj == '\0')
        {
        errorprintf("Numerical value missing.\n");
        return FALSE; /* Something wrong happened. */
        }
    char* endptr;
    double temp = strtod(&(args->u.sobj), &endptr);
    if(*endptr == '\0')
        {
        *destination = temp;
        return TRUE;
        }
    /*'((s.value).!value):?code & new$(UFP,!code):?test &  (test..go)$abc*/
    errorprintf("'%s' is not a numerical value.\n", &(args->u.sobj));
    return FALSE;
    }

static stackvalue* fsetArgs(stackvalue* sp, int arity, forthMemory* thatmem)
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
            arrp->extent = arr->extent;
            arrp->stride = arr->stride;
            }
        --sp;
        }

    return sp;
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

static void set_extent(size_t* extent, psk Node)
    {
    assert(Op(Node) == COMMA);
    size_t lextent = 1;
    psk el;
    for(el = Node->RIGHT; is_op(el) && Op(el) == WHITE; el = el->RIGHT)
        {
        ++lextent;
        if(is_op(el->LEFT) && Op(el->LEFT) == COMMA)
            {
            set_extent(extent - 1, el->LEFT);
            }
        }
    if(is_op(el) && Op(el) == COMMA)
        {
        set_extent(extent - 1, el);
        }
    if(lextent > *extent)
        {
        *extent = lextent;
        }
    }

static Boolean set_vals(forthvalue* pval, size_t rank, size_t* extent, psk Node)
    {
    assert(Op(Node) == COMMA);
    psk el;
    size_t stride = 1;
    size_t N;
    size_t index;

    for(size_t k = 0; k + 1 < rank; ++k)
        stride *= extent[k];
    N = stride * extent[rank - 1];
    for(index = 0, el = Node->RIGHT; index < N && is_op(el) && Op(el) == WHITE; index += stride, el = el->RIGHT)
        {
        if(is_op(el->LEFT) && Op(el->LEFT) == COMMA)
            {
            if(!set_vals(pval + index, rank - 1, extent, el->LEFT))
                return FALSE;
            }
        else
            {
            for(size_t t = 0; t < stride; ++t)
                if(!setFloat(&(pval[index + t].floating), el->LEFT))
                    return FALSE;
            }
        }
    if(is_op(el) && Op(el) == COMMA)
        {
        return set_vals(pval + index, rank - 1, extent, el);
        }
    else
        {
        for(size_t t = 0; t < stride; ++t)
            if(!setFloat(&(pval[index + t].floating), el))
                return FALSE;
        }
    return TRUE;
    }

static long setArgs(forthMemory* mem, size_t Nparm, psk args)
    {
    parameter* curparm = mem->parameters;
    long ParmIndex = (long)Nparm;
    if(is_op(args))
        {
        assert(Op(args) == DOT || Op(args) == COMMA || Op(args) == WHITE);
        if(Op(args) == COMMA && !is_op(args->LEFT) && args->LEFT->u.sobj == '\0')
            { /* args is an array */
            /* find rank and extents */
            size_t rank = find_rank(args);
            size_t* extent = (size_t*)bmalloc(2 * rank * sizeof(size_t)); /* twice: both extents and strides */
            if(extent != 0)
                {
                memset(extent, 0, 2 * rank * sizeof(size_t));/* twice: both extents and strides */
                set_extent(extent + rank - 1, args);
                size_t totsize = 1;
                for(size_t k = 0; k < rank; ++k)
                    {
                    totsize *= extent[k];
                    }
                fortharray* a = 0;
                if(curparm)
                    {
                    a = curparm[ParmIndex].u.a;
                    if(a->size != totsize)
                        {
                        errorprintf("Declared size of array \"%s\" is %zu, actual size is %zu.\n", a->name, a->size, totsize);
                        bfree(extent);
                        return -1;
                        }
                    for(size_t k = 0; k < a->rank; ++k)
                        if(a->extent[k] != extent[k])
                            {
                            errorprintf("Declared extent of array \"%s\" is %zu, actual extent is %zu.\n", a->name, a->extent[k], extent[k]);
                            bfree(extent);
                            return -1;
                            }
                    initialise(a, totsize);
                    /*
                    assert(a->extent == 0);
                    a->extent = extent;
                    a->rank = rank;
                    */
                    if(!set_vals(a->pval, rank, extent, args))
                        {
                        errorprintf("Value is not numeric.\n");
                        bfree(extent);
                        return -1;
                        }
                    ++ParmIndex;
                    }
                bfree(extent);
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
            if(!setFloat(&(var->val.floating), args))
                return -1;
            ++ParmIndex;
            }
        }
    return ParmIndex;
    }

static actionPair extraPop[] =
    {
        {Fless       ,FlessP      },
        {Fless_equal ,Fless_equalP},
        {Fmore_equal ,Fmore_equalP},
        {Fmore       ,FmoreP      },
        {Funequal    ,FunequalP   },
        {Fequal      ,FequalP     },
        {TheEnd      ,TheEnd      }
    };

static actionPair negations[] =
    {
        {Fless       ,Fmore_equalP},
        {Fless_equal ,FmoreP      },
        {Fmore_equal ,FlessP      },
        {Fmore       ,Fless_equalP},
        {Funequal    ,FequalP     },
        {Fequal      ,FunequalP   },
        {TheEnd      ,TheEnd      }
    };

static actionType extraPopped(actionType fun, actionPair* pair)
    {
    int i;
    for(i = 0; pair[i].Afun != TheEnd; ++i)
        if(pair[i].Afun == fun)
            return pair[i].Bfun;
    return TheEnd;
    }

static Etriple etriples[] =
    {
        {"_less"         ,Fless         ,2},
        {"_less_equal"   ,Fless_equal   ,2},
        {"_more_equal"   ,Fmore_equal   ,2},
        {"_more"         ,Fmore         ,2},
        {"_unequal"      ,Funequal      ,2},
        {"_equal"        ,Fequal        ,2},
        {"_lessP"        ,FlessP        ,2},
        {"_less_equalP"  ,Fless_equalP  ,2},
        {"_more_equalP"  ,Fmore_equalP  ,2},
        {"_moreP"        ,FmoreP        ,2},
        {"_unequalP"     ,FunequalP     ,2},
        {"_equalP"       ,FequalP       ,2},
        {"plus",  Plus,1},
        {"varPlus", varPlus,0},
        {"valPlus", valPlus,0},
        {"times", Times,1},
        {"varTimes",varTimes,0},
        {"valTimes",valTimes},
        {"acos",  Acos,1},
        {"acosh", Acosh,1},
        {"asin",  Asin,1},
        {"asinh", Asinh,1},
        {"atan",  Atan,1},
        {"atanh", Atanh,1},
        {"cbrt",  Cbrt,1},
        {"ceil",  Ceil,1},
        {"cos",   Cos,1},
        {"cosh",  Cosh,1},
        {"exp",   Exp,1},
        {"fabs",  Fabs,1},
        {"floor", Floor,1},
        {"log",   Log,1},
        {"log10", Log10,1},
        {"sign",  Sign,1},
        {"sin",   Sin,1},
        {"sinh",  Sinh,1},
        {"cube",  Cube,1},
        {"sqr",   Sqr,1},
        {"sqrt",  Sqrt,1},
        {"tan",   Tan,1},
        {"tanh",  Tanh,1},
        {"atan2", Atan2,2},
        {"fdim",  Fdim,2},
        {"fmax",  Fmax,2},
        {"fmin",  Fmin,2},
        {"fmod",  Fmod,2},
        {"hypot", Hypot,2},
        {"pow",   Pow,2},
        {"subtract",    Subtract,2},
        {"varsubtract",    varSubtract,1},
        {"valsubtract",    valSubtract,1},
        {"divide",Divide,2},
        {"varDivide",varDivide,1},
        {"valDivide",valDivide,1},
        {"rand",  Drand,1},
        {"tbl",   Tbl,0},
        {"out",   Out,1},
        {"outln", Outln,1},
        {"idx",   Idx,0},
        {"Qidx",  QIdx,0},
        {"Eidx",  EIdx,0},
        {"extent", Extent,0},
        {"rank",  Rank,0},
        {"eval",  Eval,0},
        {"NoOp",  NoOp,0},
        {0,0,0}
    };

static char* getLogiName(dumbl logi)
    {
    return logics[logi];
    }

static stackvalue* setArray(stackvalue* sp, forthword* wordp)
/* 'runtime' function, creation of array with extents that were unknown during compilation */
    {
    size_t rank = wordp->offset - 1;
    fortharray* arrp = (sp - rank)->arrp;
    size_t i;
    i = (size_t)((sp--)->val).floating;
    if(arrp->stride != 0)
        {
        for(size_t extent_index = arrp->rank - rank; extent_index < rank - 1; ++extent_index)
            {
            i += (size_t)((sp--)->val).floating * arrp->stride[extent_index];
            }
        }
    /* else linear array passed as argument to calculation, a0, a1, ... */
    arrp->index = i;
    return sp;
    }

static stackvalue* getArrayIndex(stackvalue* sp, forthword* wordp)
/* Called from 'Idx' function at runtime. */
    {
    size_t rank = wordp->offset - 1;
    fortharray* arrp = (sp - rank)->arrp;
    size_t i;
    i = (size_t)((sp--)->val).floating;
    if(arrp->stride != 0)
        {
        for(size_t extent_index = arrp->rank - rank; extent_index < rank - 1; ++extent_index)
            {
            i += (size_t)((sp--)->val).floating * arrp->stride[extent_index];
            }
        }
    /* else linear array passed as argument to calculation, a0, a1, ... */
    if(i >= arrp->size)
        {
        errorprintf("%s: index %d is out of array bounds. (0 <= index < %zu)\n", arrp->name, (signed int)i, arrp->size);
        return 0;
        }
    arrp->index = i;
    return sp;
    }

static stackvalue* getArrayExtent(stackvalue* sp)
    {
    fortharray* arrp = (sp - 1)->arrp;
    size_t rank = arrp->rank;
    LONG arg = (LONG)sp->val.floating;
    (--sp)->val.floating = (double)((0 <= arg && arg < (LONG)rank) ? (arrp->extent[rank - arg - 1]) : 0);
    return sp;
    }

static stackvalue* getArrayRank(stackvalue* sp)
    {
    sp->val.floating = (double)(sp->arrp->rank);
    return sp;
    }

static stackvalue* doTbl(stackvalue* sp, forthword* wordp, fortharray** parr)
    {
    stackvalue* sph = setArray(sp, wordp);
    if(sph == 0)
        return 0;
    fortharray* arr = sph->arrp;

    if(parr)
        *parr = arr;

    if(arr == 0)
        {
        sp = sph - 1;
        return 0;
        }
    size_t rank = arr->rank;
    size_t size = 1;

    size_t* extent = arr->extent;
    if(extent == 0)
        {
        extent = (size_t*)bmalloc(2 * rank * sizeof(size_t));/* twice: both extents and strides */
        arr->extent = extent;
        }
    if(extent != 0)
        {
        arr->stride = extent + rank;
        for(size_t j = 0; j < rank; ++j)
            {
            if(extent[j] == 0)
                extent[j] = (size_t)(sp->val).floating; /* extent[0] = extent of last index*/
            else if(extent[j] != (size_t)(sp->val).floating)
                {
                errorprintf("tbl: attempting to change fixed extent from %zu to %zu.\n", extent[j], (size_t)(sp->val).floating);
                --sp;
                --sp;
                return 0;
                }
            --sp;
            size *= extent[j];
            }

        arr->stride[0] = arr->extent[0];
        for(size_t extent_index = 1; extent_index < rank;)
            {
            arr->stride[extent_index] = arr->stride[extent_index - 1] * arr->extent[extent_index];
            ++extent_index;
            }

        arr = sp->arrp;
        arr->size = size;
        arr->index = 0;
        assert(arr->pval == 0);
        arr->pval = (forthvalue*)bmalloc(size * sizeof(forthvalue));
        if(arr->pval)
            {
            memset(arr->pval, 0, size * sizeof(forthvalue));
            }
        arr->rank = rank;
        }
    return sp;
    }

static stackvalue* fcalculate(stackvalue* sp, forthword* wordp, double* ret);

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
        assert(sp >= mem->sp - 1);
#if CALCULATION_PROFILING
        ++wordp->count;
#endif
        switch(wordp->action)
            {
                case varPush:
                    {
                    (++sp)->val = *(wordp++->u.valp);
                    break;
                    }
                case var2stack:
                    {
                    sp->val = *(wordp++->u.valp);
                    break;
                    }
                case var2stackBranch:
                    {
                    sp->val = *(wordp->u.valp);
                    wordp = word + wordp->offset;
                    break;
                    }
                case stack2var:
                    {
                    assert(sp >= mem->stack);
                    *(wordp++->u.valp) = sp->val;
                    break;
                    }
                case stack2varBranch:
                    {
                    assert(sp >= mem->stack);
                    *(wordp->u.valp) = sp->val;
                    wordp = word + wordp->offset;
                    break;
                    }
                case ArrElmValPush:
                    {
                    (++sp)->val = (wordp->u.arrp->pval)[wordp->u.arrp->index];
                    ++wordp;
                    break;
                    }
                case stack2ArrElm:
                    {
                    assert(sp >= mem->stack);
                    (wordp->u.arrp->pval)[wordp->u.arrp->index] = sp->val;
                    ++wordp;
                    break;
                    }
                case val2stack:
                    {
                    sp->val = wordp++->u.val;
                    break;
                    }
                case valPush:
                    {
                    (++sp)->val = wordp++->u.val;
                    break;
                    }
                case Afunction:
                    {
                    double ret = 0;
                    stackvalue* res = fcalculate(sp, wordp, &ret);
                    if(!res)
                        return 0;
                    sp = res;
                    (++sp)->val.floating = ret;
                    ++wordp;
                    break;
                    }
                case Pop:
                    {
                    --sp;
                    ++wordp;
                    break;
                    }
                case Branch:
                    wordp = word + wordp->offset;
                    break;
                case PopBranch:
                    {
                    --sp;
                    wordp = word + wordp->offset;
                    break;
                    }
                case valPushBranch:
                    {
                    (++sp)->val = wordp->u.val;
                    wordp = word + wordp->offset;
                    break;
                    }
                case val2stackBranch:
                    {
                    sp->val = wordp->u.val;
                    wordp = word + wordp->offset;
                    break;
                    }
                case Fless:
                    b = ((sp--)->val).floating; if((sp->val).floating >= b) wordp = word + wordp->offset; else ++wordp; break;
                case Fless_equal:
                    b = ((sp--)->val).floating; if((sp->val).floating > b) wordp = word + wordp->offset; else ++wordp; break;
                case Fmore_equal:
                    b = ((sp--)->val).floating; if((sp->val).floating < b) wordp = word + wordp->offset; else ++wordp; break;
                case Fmore:
                    b = ((sp--)->val).floating; if((sp->val).floating <= b) wordp = word + wordp->offset; else ++wordp; break;
                case Funequal:
                    b = ((sp--)->val).floating; if((sp->val).floating == b) wordp = word + wordp->offset; else ++wordp; break;
                case Fequal:
                    b = ((sp--)->val).floating; if((sp->val).floating != b) wordp = word + wordp->offset; else ++wordp; break;
                case FlessP:
                    b = ((sp--)->val).floating; if(((sp--)->val).floating >= b) wordp = word + wordp->offset; else ++wordp; break;
                case Fless_equalP:
                    b = ((sp--)->val).floating; if(((sp--)->val).floating > b) wordp = word + wordp->offset; else ++wordp; break;
                case Fmore_equalP:
                    b = ((sp--)->val).floating; if(((sp--)->val).floating < b) wordp = word + wordp->offset; else ++wordp; break;
                case FmoreP:
                    b = ((sp--)->val).floating; if(((sp--)->val).floating <= b) wordp = word + wordp->offset; else ++wordp; break;
                case FunequalP:
                    b = ((sp--)->val).floating; if(((sp--)->val).floating == b) wordp = word + wordp->offset; else ++wordp; break;
                case FequalP:
                    b = ((sp--)->val).floating; if(((sp--)->val).floating != b) wordp = word + wordp->offset; else ++wordp; break;
                case Plus:
                    a = ((sp--)->val).floating; sp->val.floating += a; ++wordp; break;
                case varPlus:
                    sp->val.floating += wordp++->u.valp->floating; break;
                case valPlus:
                    sp->val.floating += wordp++->u.val.floating; break;
                case Times:
                    a = ((sp--)->val).floating; sp->val.floating *= a; ++wordp; break;
                case varTimes:
                    sp->val.floating *= wordp++->u.valp->floating; break;
                case valTimes:
                    sp->val.floating *= wordp++->u.val.floating; break;
                case Acos:
                    sp->val.floating = acos((sp->val).floating); ++wordp; break;
                case Acosh:
                    sp->val.floating = acosh((sp->val).floating); ++wordp; break;
                case Asin:
                    sp->val.floating = asin((sp->val).floating); ++wordp; break;
                case Asinh:
                    sp->val.floating = asinh((sp->val).floating); ++wordp; break;
                case Atan:
                    sp->val.floating = atan((sp->val).floating); ++wordp; break;
                case Atanh:
                    sp->val.floating = atanh((sp->val).floating); ++wordp; break;
                case Cbrt:
                    sp->val.floating = cbrt((sp->val).floating); ++wordp; break;
                case Ceil:
                    sp->val.floating = ceil((sp->val).floating); ++wordp; break;
                case Cos:
                    sp->val.floating = cos((sp->val).floating); ++wordp; break;
                case Cosh:
                    sp->val.floating = cosh((sp->val).floating); ++wordp; break;
                case Exp:
                    sp->val.floating = exp((sp->val).floating); ++wordp; break;
                case Fabs:
                    sp->val.floating = fabs((sp->val).floating); ++wordp; break;
                case Floor:
                    sp->val.floating = floor((sp->val).floating); ++wordp; break;
                case Log:
                    sp->val.floating = log((sp->val).floating); ++wordp; break;
                case Log10:
                    sp->val.floating = log10((sp->val).floating); ++wordp; break;
                case Sign:
                    b = (sp->val).floating; if(b != 0.0) { if(b > 0) sp->val.floating = 1.0; else if(b < 0) sp->val.floating = -1.0; } ++wordp; break;
                case Sin:
                    sp->val.floating = sin((sp->val).floating); ++wordp; break;
                case Sinh:
                    sp->val.floating = sinh((sp->val).floating); ++wordp; break;
                case Cube:
                    sp->val.floating *= sp->val.floating * sp->val.floating; ++wordp; break;
                case Sqr:
                    sp->val.floating *= sp->val.floating; ++wordp; break;
                case Sqrt:
                    sp->val.floating = sqrt((sp->val).floating); ++wordp; break;
                case Tan:
                    sp->val.floating = tan((sp->val).floating); ++wordp; break;
                case Tanh:
                    sp->val.floating = tanh((sp->val).floating); ++wordp; break;
                case Atan2:
                    a = ((sp--)->val).floating; sp->val.floating = atan2(a, (sp->val).floating); ++wordp; break;
                case Fdim:
                    a = ((sp--)->val).floating; sp->val.floating = fdim(a, (sp->val).floating); ++wordp; break;
                case Fmax:
                    a = ((sp--)->val).floating; sp->val.floating = fmax(a, (sp->val).floating); ++wordp; break;
                case Fmin:
                    a = ((sp--)->val).floating; sp->val.floating = fmin(a, (sp->val).floating); ++wordp; break;
                case Fmod:
                    a = ((sp--)->val).floating; sp->val.floating = fmod(a, (sp->val).floating); ++wordp; break;
                case Hypot:
                    a = ((sp--)->val).floating; sp->val.floating = hypot(a, (sp->val).floating); ++wordp; break;
                case Pow:
                    a = ((sp--)->val).floating; sp->val.floating = pow(a, (sp->val).floating); ++wordp; break;
                case Subtract:
                    a = ((sp--)->val).floating; sp->val.floating = (sp->val).floating - a; ++wordp; break;
                case varSubtract:
                    sp->val.floating -= wordp++->u.valp->floating; break;
                case valSubtract:
                    sp->val.floating -= wordp++->u.val.floating; break;
                case Divide:
                    a = ((sp--)->val).floating; sp->val.floating = (sp->val).floating / a; ++wordp; break;
                case varDivide:
                    sp->val.floating /= wordp++->u.valp->floating; break;
                case valDivide:
                    sp->val.floating /= wordp++->u.val.floating; break;
                case Drand:
                    sp->val.floating = drand((sp->val).floating); ++wordp; break;
                case Tbl:
                    {
                    sp = doTbl(sp, wordp, 0);
                    if(!sp)
                        return 0;
                    ++wordp;
                    break;
                    }
                case Out:
                    printf("%f ", (sp--)->val.floating); ++wordp; break;
                    break;
                case Outln:
                    printf("%f\n", (sp--)->val.floating); ++wordp; break;
                    break;
                case Idx:
                    {
                    if((sp = getArrayIndex(sp, wordp)) == 0)
                        return 0;
                    ++wordp;
                    --sp;
                    break;
                    }
                case QIdx:
                    {
                    if((sp = getArrayIndex(sp, wordp)) == 0)
                        return 0;
                    i = sp->arrp->index;

                    forthvalue* val = sp->arrp->pval + i;

                    assert(sp >= mem->stack);
                    assert(sp >= mem->stack);
                    *val = (--sp)->val;
                    ++wordp;
                    break;
                    }
                case EIdx:
                    {
                    if((sp = getArrayIndex(sp, wordp)) == 0)
                        return 0;
                    i = sp->arrp->index;

                    assert(sp >= mem->stack);
                    sp->arrp->index = i;
                    sp->val = (sp->arrp->pval)[i];
                    ++wordp;
                    break;
                    }
                case Extent:
                    {
                    sp = getArrayExtent(sp);
                    ++wordp;
                    break;
                    }
                case Rank:
                    {
                    sp = getArrayRank(sp);
                    ++wordp;
                    break;
                    }
                case Eval:
                    {
                    psk pnode;
                    pnode = eval(same_as_w(wordp->u.Pnode));
                    wipe(pnode);
                    ++wordp;
                    break;
                    }
                case NoOp:
                    ++wordp;
                    break;
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
    if(mem)
        {
        parameter* curparm = mem->parameters;
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
                    res = (psk)bmalloc(len + 1);

                    if(res)
                        {
                        strcpy((char*)(res)+offsetof(sk, u.sobj), buf);
                        wipe(*arg);
                        *arg = res;
                        res->v.fl = flags;
                        return TRUE;
                        }
                    }
                }
            else
                return FALSE;
            }
        else
            return FALSE;
        }
    return FALSE;
    }

static stackvalue* fcalculate(stackvalue* sp, forthword* wordp, double* ret)
    {
    forthMemory* thatmem = wordp->u.that;
    if(sp && thatmem)
        {
        stackvalue* sp2;
        sp = fsetArgs(sp, wordp->offset, thatmem);
        sp2 = calculateBody(thatmem);
        if(sp2 && sp2 >= thatmem->stack)
            {
            *ret = thatmem->stack->val.floating;
            }
        }
    return sp;
    }

static stackvalue* ftrc(stackvalue* sp, forthword* wordp, double* ret);

static stackvalue* trcBody(forthMemory* mem)
    {
    forthword* word = mem->word;
    forthword* wordp = mem->wordp;
    stackvalue* sp = mem->sp - 1;
    printf("ACTION            WORD STACK  variables <stack elements> other\n");
    for(wordp = word; wordp->action != TheEnd;)
        {
        double a;
        double b;
        size_t i;
        char* naam;
        assert(sp + 1 >= mem->stack);
        printf("%-16s %5d %5d: ", ActionAsWord[wordp->action], (int)(wordp - word), (int)(sp - mem->stack));
        printf("\t");
        switch(wordp->action)
            {
                case varPush:
                    {
                    printf("%s %.2f --> stack", getVarName(mem, (wordp->u.valp)), (*(wordp->u.valp)).floating);
                    (++sp)->val = *(wordp++->u.valp);
                    break;
                    }
                case var2stack:
                    {
                    printf("%s %.2f --> stack", getVarName(mem, (wordp->u.valp)), (*(wordp->u.valp)).floating);
                    sp->val = *(wordp++->u.valp);
                    break;
                    }
                case var2stackBranch:
                    {
                    printf("%s %.2f --> stack unconditional jump to %u", getVarName(mem, (wordp->u.valp)), (*(wordp->u.valp)).floating, wordp->offset);
                    sp->val = *(wordp->u.valp);
                    wordp = word + wordp->offset;
                    break;
                    }
                case stack2var:
                    {
                    assert(sp >= mem->stack);
                    *(wordp->u.valp) = sp->val;
                    printf("%s %.2f <-- stack", getVarName(mem, wordp->u.valp), sp->val.floating);
                    ++wordp;
                    break;
                    }
                case stack2varBranch:
                    {
                    assert(sp >= mem->stack);
                    printf("%s %.2f <-- stack unconditional jump to %u", getVarName(mem, (wordp->u.valp)), (*(wordp->u.valp)).floating, wordp->offset);
                    *(wordp->u.valp) = sp->val;
                    wordp = word + wordp->offset;
                    break;
                    }
                case ArrElmValPush:
                    {
                    printf("%s %.2f --> stack", wordp->u.arrp->name, (wordp->u.arrp->pval)[wordp->u.arrp->index].floating);
                    (++sp)->val = (wordp->u.arrp->pval)[wordp->u.arrp->index];
                    ++wordp;
                    break;
                    }
                case stack2ArrElm:
                    {
                    assert(sp >= mem->stack);
                    (wordp->u.arrp->pval)[wordp->u.arrp->index] = sp->val;
                    printf("%s %.2f <-- stack", wordp->u.arrp->name, (wordp->u.arrp->pval)[wordp->u.arrp->index].floating);
                    ++wordp;
                    break;
                    }
                case val2stack:
                    {
                    printf("%.2f %p ", wordp->u.val.floating, (void*)wordp->u.arrp);
                    sp->val = wordp++->u.val;
                    break;
                    }
                case valPush:
                    {
                    printf("%.2f %p ", wordp->u.val.floating, (void*)wordp->u.arrp);
                    (++sp)->val = wordp++->u.val;
                    break;
                    }
                case Afunction:
                    {
                    double ret = 0;
                    naam = wordp->u.that->name;
                    printf("%s\n", naam);
                    stackvalue* res = ftrc(sp, wordp, &ret); // May fail!
                    printf("%s DONE\n", naam);
                    if(!res)
                        return 0;
                    sp = res;
                    (++sp)->val.floating = ret;
                    ++wordp;
                    break;
                    }
                case Pop:
                    {
                    naam = getLogiName(wordp->u.logic);
                    printf(" %s", naam);
                    printf(" conditional jump to %u", wordp->offset);
                    assert(sp >= mem->stack);
                    --sp;
                    ++wordp;
                    break;
                    }
                case Branch:
                    printf("unconditional jump to %u", wordp->offset);
                    wordp = word + wordp->offset;
                    break;
                case PopBranch:
                    {
                    naam = getLogiName(wordp->u.logic);
                    printf(" %s", naam);
                    printf(" unconditional jump to %u", wordp->offset);
                    assert(sp >= mem->stack);
                    --sp;
                    wordp = word + wordp->offset;
                    break;
                    }
                case valPushBranch:
                    {
                    printf("%.2f %p ", wordp->u.val.floating, (void*)wordp->u.arrp);
                    printf(" unconditional jump to %u", wordp->offset);
                    (++sp)->val = wordp->u.val;
                    wordp = word + wordp->offset;
                    break;
                    }
                case val2stackBranch:
                    {
                    printf("%.2f %p ", wordp->u.val.floating, (void*)wordp->u.arrp);
                    printf(" unconditional jump to %u", wordp->offset);
                    sp->val = wordp->u.val;
                    wordp = word + wordp->offset;
                    break;
                    }
                case Fless:
                    printf("PopB < ");
                    b = ((sp--)->val).floating; if((sp->val).floating >= b) wordp = word + wordp->offset; else ++wordp; break;
                case Fless_equal:
                    printf("PopB <=");
                    b = ((sp--)->val).floating; if((sp->val).floating > b) wordp = word + wordp->offset; else ++wordp; break;
                case Fmore_equal:
                    printf("PopB >=");
                    b = ((sp--)->val).floating; if((sp->val).floating < b) wordp = word + wordp->offset; else ++wordp; break;
                case Fmore:
                    printf("PopB > ");
                    b = ((sp--)->val).floating; if((sp->val).floating <= b) wordp = word + wordp->offset; else ++wordp; break;
                case Funequal:
                    printf("PopB !=");
                    b = ((sp--)->val).floating; if((sp->val).floating == b) wordp = word + wordp->offset; else ++wordp; break;
                case Fequal:
                    printf("PopB ==");
                    b = ((sp--)->val).floating; if((sp->val).floating != b) wordp = word + wordp->offset; else ++wordp; break;
                case FlessP:
                    printf("PopPopB < ");
                    b = ((sp--)->val).floating; if(((sp--)->val).floating >= b) wordp = word + wordp->offset; else ++wordp; break;
                case Fless_equalP:
                    printf("PopPopB <=");
                    b = ((sp--)->val).floating; if(((sp--)->val).floating > b) wordp = word + wordp->offset; else ++wordp; break;
                case Fmore_equalP:
                    printf("PopPopB >=");
                    b = ((sp--)->val).floating; if(((sp--)->val).floating < b) wordp = word + wordp->offset; else ++wordp; break;
                case FmoreP:
                    printf("PopPopB > ");
                    b = ((sp--)->val).floating; if(((sp--)->val).floating <= b) wordp = word + wordp->offset; else ++wordp; break;
                case FunequalP:
                    printf("PopPopB !=");
                    b = ((sp--)->val).floating; if(((sp--)->val).floating == b) wordp = word + wordp->offset; else ++wordp; break;
                case FequalP:
                    printf("PopPopB ==");
                    b = ((sp--)->val).floating; if(((sp--)->val).floating != b) wordp = word + wordp->offset; else ++wordp; break;
                case Plus:
                    printf("Pop plus  ");
                    a = ((sp--)->val).floating; sp->val.floating += a; ++wordp; break;
                case varPlus:
                    printf("varPlus  ");
                    sp->val.floating += wordp++->u.valp->floating; break;
                case valPlus:
                    printf("valPlus  ");
                    sp->val.floating += wordp++->u.val.floating; break;
                case Times:
                    printf("Pop times ");
                    a = ((sp--)->val).floating; sp->val.floating *= a; ++wordp; break;
                case varTimes:
                    printf("varTimes ");
                    sp->val.floating *= wordp++->u.valp->floating; break;
                case valTimes:
                    printf("valTimes ");
                    sp->val.floating *= wordp++->u.val.floating; break;
                case Acos:
                    printf("acos  ");
                    sp->val.floating = acos((sp->val).floating); ++wordp; break;
                case Acosh:
                    printf("acosh ");
                    sp->val.floating = acosh((sp->val).floating); ++wordp; break;
                case Asin:
                    printf("asin  ");
                    sp->val.floating = asin((sp->val).floating); ++wordp; break;
                case Asinh:
                    printf("asinh ");
                    sp->val.floating = asinh((sp->val).floating); ++wordp; break;
                case Atan:
                    printf("atan  ");
                    sp->val.floating = atan((sp->val).floating); ++wordp; break;
                case Atanh:
                    printf("atanh ");
                    sp->val.floating = atanh((sp->val).floating); ++wordp; break;
                case Cbrt:
                    printf("cbrt  ");
                    sp->val.floating = cbrt((sp->val).floating); ++wordp; break;
                case Ceil:
                    printf("ceil  ");
                    sp->val.floating = ceil((sp->val).floating); ++wordp; break;
                case Cos:
                    printf("cos   ");
                    sp->val.floating = cos((sp->val).floating); ++wordp; break;
                case Cosh:
                    printf("cosh  ");
                    sp->val.floating = cosh((sp->val).floating); ++wordp; break;
                case Exp:
                    printf("exp   ");
                    sp->val.floating = exp((sp->val).floating); ++wordp; break;
                case Fabs:
                    printf("fabs  ");
                    sp->val.floating = fabs((sp->val).floating); ++wordp; break;
                case Floor:
                    printf("floor ");
                    sp->val.floating = floor((sp->val).floating); ++wordp; break;
                case Log:
                    printf("log   ");
                    sp->val.floating = log((sp->val).floating); ++wordp; break;
                case Log10:
                    printf("log10 ");
                    sp->val.floating = log10((sp->val).floating); ++wordp; break;
                case Sign:
                    printf("sign  ");
                    b = (sp->val).floating; if(b != 0.0) { if(b > 0) sp->val.floating = 1.0; else if(b < 0) sp->val.floating = -1.0; } ++wordp; break;
                case Sin:
                    printf("sin   ");
                    sp->val.floating = sin((sp->val).floating); ++wordp; break;
                case Sinh:
                    printf("sinh  ");
                    sp->val.floating = sinh((sp->val).floating); ++wordp; break;
                case Cube:
                    printf("cube  ");
                    sp->val.floating *= sp->val.floating * sp->val.floating; ++wordp; break;
                case Sqr:
                    printf("sqr   ");
                    sp->val.floating *= sp->val.floating; ++wordp; break;
                case Sqrt:
                    printf("sqrt  ");
                    sp->val.floating = sqrt((sp->val).floating); ++wordp; break;
                case Tan:
                    printf("tan   ");
                    sp->val.floating = tan((sp->val).floating); ++wordp; break;
                case Tanh:
                    printf("tanh  ");
                    sp->val.floating = tanh((sp->val).floating); ++wordp; break;
                case Atan2:
                    printf("Pop fmax  ");
                    a = ((sp--)->val).floating; sp->val.floating = atan2(a, (sp->val).floating); ++wordp; break;
                case Fdim:
                    printf("Pop fmin  ");
                    a = ((sp--)->val).floating; sp->val.floating = fdim(a, (sp->val).floating); ++wordp; break;
                case Fmax:
                    printf("Pop fdim  ");
                    a = ((sp--)->val).floating; sp->val.floating = fmax(a, (sp->val).floating); ++wordp; break;
                case Fmin:
                    printf("Pop atan2 ");
                    a = ((sp--)->val).floating; sp->val.floating = fmin(a, (sp->val).floating); ++wordp; break;
                case Fmod:
                    printf("Pop fmod  ");
                    a = ((sp--)->val).floating; sp->val.floating = fmod(a, (sp->val).floating); ++wordp; break;
                case Hypot:
                    printf("Pop hypot ");
                    a = ((sp--)->val).floating; sp->val.floating = hypot(a, (sp->val).floating); ++wordp; break;
                case Pow:
                    printf("Pop pow   ");
                    a = ((sp--)->val).floating; sp->val.floating = pow(a, (sp->val).floating); ++wordp; break;
                case Subtract:
                    printf("Pop subtract");
                    a = ((sp--)->val).floating; sp->val.floating = (sp->val).floating - a; ++wordp; break;
                case varSubtract:
                    printf("varSubtract");
                    sp->val.floating -= wordp++->u.valp->floating; break;
                case valSubtract:
                    printf("valSubtract");
                    sp->val.floating -= wordp++->u.val.floating; break;
                case Divide:
                    printf("Pop divide");
                    a = ((sp--)->val).floating; sp->val.floating = (sp->val).floating / a; ++wordp; break;
                case varDivide:
                    printf("varDivide");
                    sp->val.floating /= wordp++->u.valp->floating; break;
                case valDivide:
                    printf("valDivide");
                    sp->val.floating /= wordp++->u.val.floating; break;
                case Drand:
                    printf("Pop rand");
                    sp->val.floating = drand((sp->val).floating); ++wordp; break;
                case Tbl:
                    {
                    fortharray* arr = 0;
                    sp = doTbl(sp, wordp, &arr);
                    if(!sp)
                        return 0;
                    printf("Pop tbl   %p size %zu index %zu", (void*)arr, arr->size, arr->index);
                    ++wordp;
                    break;
                    }
                case Out:
                    printf("%f ", (sp--)->val.floating); ++wordp; break;
                    break;
                case Outln:
                    printf("%f\n", (sp--)->val.floating); ++wordp; break;
                    break;
                case Idx:
                    {
                    if((sp = getArrayIndex(sp, wordp)) == 0)
                        return 0;
                    printf("Pop index   ");
                    ++wordp;
                    --sp;
                    break;
                    }
                case QIdx:
                    {
                    if((sp = getArrayIndex(sp, wordp)) == 0)
                        return 0;
                    i = sp->arrp->index;

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
                    if((sp = getArrayIndex(sp, wordp)) == 0)
                        return 0;
                    i = sp->arrp->index;

                    printf("Pop !index  ");
                    assert(sp >= mem->stack);
                    sp->arrp->index = i;
                    sp->val = (sp->arrp->pval)[i];
                    ++wordp;
                    break;
                    }
                case Extent:
                    {
                    printf("Extent      ");
                    assert(sp >= mem->stack);
                    sp = getArrayExtent(sp);
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
                case Eval:
                    {
                    printf("Eval        ");
                    psk pnode;
                    pnode = eval(same_as_w(wordp->u.Pnode));
                    wipe(pnode);
                    ++wordp;
                    break;
                    }
                case NoOp:
                    printf("NoOp        ");
                    ++wordp;
                    break;
                case TheEnd:
                    printf("TheEnd      ");
                    break;
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
    if(mem)
        {
        parameter* curparm = mem->parameters;
        if(!curparm || setArgs(mem, 0, Arg) > 0)
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
                    res = (psk)bmalloc(len + 1);

                    if(res)
                        {
                        strcpy((char*)(res)+offsetof(sk, u.sobj), buf);
                        printf("value on stack %s\n", buf);
                        wipe(*arg);
                        *arg = res;
                        res->v.fl = flags;
                        return TRUE;
                        }
                    }
                }
            else
                return FALSE;
            }
        else
            return FALSE;
        }
    return FALSE;
    }

static stackvalue* ftrc(stackvalue* sp, forthword* wordp, double* ret)
    {
    forthMemory* thatmem = wordp->u.that;
    if(sp && thatmem)
        {
        stackvalue* sp2;
        sp = fsetArgs(sp, wordp->offset, thatmem);
        sp2 = trcBody(thatmem);
        if(sp2 && sp2 >= thatmem->stack)
            {
            *ret = thatmem->stack->val.floating;
            }
        }
    return sp;
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

static Boolean StaticArray(psk declaration)
    {
    if(is_op(declaration))
        {
        for(psk extents = declaration->RIGHT;; extents = extents->RIGHT)
            {
            if(is_op(extents))
                {
                if(!INTEGER_POS(extents->LEFT))
                    return FALSE;
                }
            else
                {
                if(INTEGER_POS(extents))
                    return TRUE;
                else
                    return FALSE;
                }
            }
        return FALSE;
        }
    return FALSE;
    }

static int polish1(psk code, Boolean commentsAllowed)
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
                R = polish1(code->LEFT, FALSE);
                if(R == -1)
                    return -1;
                C = polish1(code->RIGHT, FALSE);
                if(C == -1)
                    return -1;
                return 1 + R + C;
                }
            case AND:
                {
                R = polish1(code->LEFT, TRUE);
                if(R == -1)
                    return -1;
                C = polish1(code->RIGHT, TRUE);
                if(C == -1)
                    return -1;
                if(C == 0) // This results in a NoOp
                    return 7 + R + C;
                if(R == 0 || C == 0)
                    return R + C; /* Function definition on the left and/or right side. */
                return 7 + R + C;
                /* 0: jump +5 1: pop 2 : jump to success branch 3: pop 4: jump to fail branch.
                Two more for jumps after lhs and rhs*/
                }
            case OR:
                {
                R = polish1(code->LEFT, TRUE);
                if(R == -1)
                    return -1;
                if(R == 0)
                    return 0; /* (|...) means: ignore ..., do nothing. */
                C = polish1(code->RIGHT, TRUE);
                if(C == -1)
                    return -1;
                return 7 + R + C;
                /* 0: jump +5 1: pop 2 : jump to success branch 3: pop 4: jump to fail branch.
                Two more for jumps after lhs and rhs*/
                }
            case MATCH:
                {
                R = polish1(code->LEFT, FALSE);
                if(R == -1)
                    return -1;
                C = polish1(code->RIGHT, FALSE);
                if(C == -1)
                    return -1;
                if(Op(code->RIGHT) == MATCH || code->RIGHT->v.fl & UNIFY)
                    return R + C;
                else
                    return 1 + R + C;
                }
            case FUN:
                if(is_op(code->LEFT))
                    {
                    errorprintf("calculation: lhs of $ is operator\n");
                    return -1;
                    }
                else if(!strcmp(&(code->LEFT->u.sobj), "tbl") && StaticArray(code->RIGHT))
                    {
                    return 0;
                    }
                else
                    {
                    C = polish1(code->RIGHT, FALSE);
                    if(C == 1 && code->RIGHT->u.sobj == '\0') /* No parameters at all. */
                        C = 0;
                    if(C == -1)
                        return -1;
                    return 1 + C;
                    }
            case FUU:
                if(is_op(code->LEFT))
                    {
                    errorprintf("calculation: lhs of ' is operator\n");
                    return -1;
                    }
                else if(!strcmp(&(code->LEFT->u.sobj), "eval"))
                    {
                    return 1;
                    }
                else
                    {
                    C = polish1(code->RIGHT, FALSE);
                    if(C == -1)
                        return -1;
                    return 6 + C;
                    /* 0: jump +5 1: pop 2 : jump to wstart of loop 3: pop 4: jump out of the loop.
                    One more for jump after loop*/
                    }
            default:
                if(is_op(code))
                    {
                    R = polish1(code->LEFT, FALSE);
                    if(R == -1)
                        return -1;
                    C = polish1(code->RIGHT, FALSE);
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
                    else if(/*code->u.sobj == '\0' ||*/ commentsAllowed)
                        return 0;
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
    if(mem == 0)
        return FALSE;
    char* naam;
    int In = 0;
    forthword* wordp = mem->word;
    printf("print %s\n", mem->name == 0 ? "(main)" : mem->name);
    for(; wordp->action != TheEnd; ++wordp)
        {
        actionType act = wordp->action;
        char* Act = ActionAsWord[act];
        switch(act)
            {
                case varPush:
                    printf(INDNT); printf("%*td" " %-32s %s\n", 5, wordp - mem->word, Act, getVarName(mem, (wordp->u.valp)));
                    ++In;
                    break;
                case var2stack:
                    printf(INDNT); printf("%*td" " %-32s %s\n", 5, wordp - mem->word, Act, getVarName(mem, (wordp->u.valp)));
                    ++In;
                    break;
                case var2stackBranch:
                    printf(INDNT); printf("%*td" " %-24s " LONGnD "   %s\n", 5, wordp - mem->word, Act, 5, (LONG)(wordp->offset), getVarName(mem, (wordp->u.valp)));
                    ++In;
                    break;
                case stack2var:
                    printf(INDNT); printf("%*td" " %-32s %s\n", 5, wordp - mem->word, Act, getVarName(mem, (wordp->u.valp)));
                    break;
                case stack2varBranch:
                    printf(INDNT); printf("%*td" " %-24s " LONGnD "   %s\n", 5, wordp - mem->word, Act, 5, (LONG)(wordp->offset), getVarName(mem, (wordp->u.valp)));
                    ++In;
                    break;
                case ArrElmValPush:
                    printf(INDNT); printf("%*td" " %-32s %s\n", 5, wordp - mem->word, Act, wordp->u.arrp->name);
                    break;
                case stack2ArrElm:
                    printf(INDNT); printf("%*td" " %-32s %s\n", 5, wordp - mem->word, Act, wordp->u.arrp->name);
                    break;
                case val2stack:
                    {
                    forthvalue val = wordp->u.val;
                    printf(INDNT); printf("%*td" " %-32s %f\n", 5, wordp - mem->word, Act, val.floating);
                    ++In;
                    break;
                    }
                case valPush:
                    {
                    forthvalue val = wordp->u.val;
                    printf(INDNT); printf("%*td" " %-32s %f\n", 5, wordp - mem->word, Act, val.floating);
                    ++In;
                    break;
                    }
                case Afunction:
                    naam = wordp->u.that->name;
                    printf(INDNT); printf("%*td" " %s \"%-12s\" " LONGnD "\n", 5, wordp - mem->word, Act, naam, 5, (LONG)(wordp->offset));
                    break;
                case Pop:
                    naam = getLogiName(wordp->u.logic);
                    printf(INDNT); printf("%*td" " %-32s %s\n", 5, wordp - mem->word, Act, naam);
                    --In;
                    break;
                case Branch:
                    naam = getLogiName(wordp->u.logic);
                    printf(INDNT); printf("%*td" " %-24s " LONGnD "   %s\n", 5, wordp - mem->word, Act, 5, (LONG)(wordp->offset), naam);
                    break;
                case PopBranch:
                    naam = getLogiName(wordp->u.logic);
                    printf(INDNT); printf("%*td" " %-24s " LONGnD "   %s\n", 5, wordp - mem->word, Act, 5, (LONG)(wordp->offset), naam);
                    --In;
                    break;
                case valPushBranch:
                    {
                    forthvalue val = wordp->u.val;
                    printf(INDNT); printf("%*td" " %-24s " LONGnD "   %f\n", 5, wordp - mem->word, Act, 5, (LONG)(wordp->offset), val.floating);
                    ++In;
                    break;
                    }
                case val2stackBranch:
                    {
                    forthvalue val = wordp->u.val;
                    printf(INDNT);
                    printf("%*td" " %-24s " LONGnD "   %f\n", 5, wordp - mem->word, Act, 5, (LONG)(wordp->offset), val.floating);
                    ++In;
                    break;
                    }
                case Fless: printf(INDNT);
                    printf("%*td" " %-24s " LONGnD "\n", 5, wordp - mem->word, "PopB <", 5, (LONG)(wordp->offset));
                    --In; --In; break;
                case Fless_equal: printf(INDNT); printf("%*td" " %-24s " LONGnD "\n", 5, wordp - mem->word, "PopB <=", 5, (LONG)(wordp->offset)); --In; --In; break;
                case Fmore_equal: printf(INDNT); printf("%*td" " %-24s " LONGnD "\n", 5, wordp - mem->word, "PopB >=", 5, (LONG)(wordp->offset)); --In; --In; break;
                case Fmore: printf(INDNT); printf("%*td" " %-24s " LONGnD "\n", 5, wordp - mem->word, "PopB >", 5, (LONG)(wordp->offset)); --In; --In; break;
                case Funequal: printf(INDNT); printf("%*td" " %-24s " LONGnD "\n", 5, wordp - mem->word, "PopB !=", 5, (LONG)(wordp->offset)); --In; --In; break;
                case Fequal: printf(INDNT); printf("%*td" " %-24s " LONGnD "\n", 5, wordp - mem->word, "PopB ==", 5, (LONG)(wordp->offset)); --In; --In; break;

                case FlessP: printf(INDNT); printf("%*td" " %-24s " LONGnD "\n", 5, wordp - mem->word, "PopPopB <", 5, (LONG)(wordp->offset)); --In; --In; break;
                case Fless_equalP: printf(INDNT); printf("%*td" " %-24s " LONGnD "\n", 5, wordp - mem->word, "PopPopB <=", 5, (LONG)(wordp->offset)); --In; --In; break;
                case Fmore_equalP: printf(INDNT); printf("%*td" " %-24s " LONGnD "\n", 5, wordp - mem->word, "PopPopB >=", 5, (LONG)(wordp->offset)); --In; --In; break;
                case FmoreP: printf(INDNT); printf("%*td" " %-24s " LONGnD "\n", 5, wordp - mem->word, "PopPopB >", 5, (LONG)(wordp->offset)); --In; --In; break;
                case FunequalP: printf(INDNT); printf("%*td" " %-24s " LONGnD "\n", 5, wordp - mem->word, "PopPopB !=", 5, (LONG)(wordp->offset)); --In; --In; break;
                case FequalP: printf(INDNT); printf("%*td" " %-24s " LONGnD "\n", 5, wordp - mem->word, "PopPop ==", 5, (LONG)(wordp->offset)); --In; --In; break;

                case Plus:  printf(INDNT); printf("%*td"     " Pop plus\n", 5, wordp - mem->word); --In; break;
                case varPlus:  printf(INDNT); printf("%*td" " %-32s %s\n", 5, wordp - mem->word, "varPlus", getVarName(mem, (wordp->u.valp))); break;
                case valPlus:  printf(INDNT); printf("%*td" " %-32s %f\n", 5, wordp - mem->word, "valPlus", wordp->u.val.floating); break;
                case Times:  printf(INDNT); printf("%*td"    " Pop times\n", 5, wordp - mem->word); --In; break;
                case varTimes:  printf(INDNT); printf("%*td" " %-32s %s\n", 5, wordp - mem->word, "varTimes", getVarName(mem, (wordp->u.valp))); break;
                case valTimes:  printf(INDNT); printf("%*td" " %-32s %f\n", 5, wordp - mem->word, "valTimes", wordp->u.val.floating); break;
                case Acos:  printf(INDNT); printf("%*td"     " acos\n", 5, wordp - mem->word); break;
                case Acosh:  printf(INDNT); printf("%*td"    " acosh\n", 5, wordp - mem->word); break;
                case Asin:  printf(INDNT); printf("%*td"     " asin\n", 5, wordp - mem->word); break;
                case Asinh:  printf(INDNT); printf("%*td"    " asinh\n", 5, wordp - mem->word); break;
                case Atan:  printf(INDNT); printf("%*td"     " atan\n", 5, wordp - mem->word); break;
                case Atanh:  printf(INDNT); printf("%*td"    " atanh\n", 5, wordp - mem->word); break;
                case Cbrt:  printf(INDNT); printf("%*td"     " cbrt\n", 5, wordp - mem->word); break;
                case Ceil:  printf(INDNT); printf("%*td"     " ceil\n", 5, wordp - mem->word); break;
                case Cos:
                    printf(INDNT);
                    printf("%*td"      " cos\n", 5, wordp - mem->word);
                    break;
                case Cosh:  printf(INDNT); printf("%*td"     " cosh\n", 5, wordp - mem->word); break;
                case Exp:  printf(INDNT); printf("%*td"      " exp\n", 5, wordp - mem->word); break;
                case Fabs:  printf(INDNT); printf("%*td"     " fabs\n", 5, wordp - mem->word); break;
                case Floor:  printf(INDNT); printf("%*td"    " floor\n", 5, wordp - mem->word); break;
                case Log:  printf(INDNT); printf("%*td"      " log\n", 5, wordp - mem->word); break;
                case Log10:  printf(INDNT); printf("%*td"    " log10\n", 5, wordp - mem->word); break;
                case Sign:   printf(INDNT); printf("%*td"    " sign\n", 5, wordp - mem->word); break;
                case Sin:  printf(INDNT); printf("%*td"      " sin\n", 5, wordp - mem->word); break;
                case Sinh:  printf(INDNT); printf("%*td"     " sinh\n", 5, wordp - mem->word); break;
                case Cube:  printf(INDNT); printf("%*td"     " cube\n", 5, wordp - mem->word); break;
                case Sqr:  printf(INDNT); printf("%*td"     " sqr\n", 5, wordp - mem->word); break;
                case Sqrt:  printf(INDNT); printf("%*td"     " sqrt\n", 5, wordp - mem->word); break;
                case Tan:  printf(INDNT); printf("%*td"      " tan\n", 5, wordp - mem->word); break;
                case Tanh:  printf(INDNT); printf("%*td"     " tanh\n", 5, wordp - mem->word); break;
                case Atan2:  printf(INDNT); printf("%*td"    " Pop atan2\n", 5, wordp - mem->word); --In; break;
                case Fdim:  printf(INDNT); printf("%*td"     " Pop fdim\n", 5, wordp - mem->word); --In; break;
                case Fmax:  printf(INDNT); printf("%*td"     " Pop fmax\n", 5, wordp - mem->word); --In; break;
                case Fmin:  printf(INDNT); printf("%*td"     " Pop fmin\n", 5, wordp - mem->word); --In; break;
                case Fmod:  printf(INDNT); printf("%*td"     " Pop fmod\n", 5, wordp - mem->word); --In; break;
                case Hypot:  printf(INDNT); printf("%*td"    " Pop hypot\n", 5, wordp - mem->word); --In; break;
                case Pow:  printf(INDNT); printf("%*td"      " Pop pow\n", 5, wordp - mem->word); --In; break;
                case Subtract:  printf(INDNT); printf("%*td" " Pop subtract\n", 5, wordp - mem->word); --In; break;
                case varSubtract:  printf(INDNT); printf("%*td" " %-32s %s\n", 5, wordp - mem->word, "varSubtract", getVarName(mem, (wordp->u.valp))); break;
                case valSubtract:  printf(INDNT); printf("%*td" " %-32s %f\n", 5, wordp - mem->word, "valSubtract", wordp->u.val.floating); break;
                case Divide:  printf(INDNT); printf("%*td"   " Pop divide\n", 5, wordp - mem->word); --In; break;
                case varDivide:  printf(INDNT); printf("%*td" " %-32s %s\n", 5, wordp - mem->word, "varDivide", getVarName(mem, (wordp->u.valp))); break;
                case valDivide:  printf(INDNT); printf("%*td" " %-32s %f\n", 5, wordp - mem->word, "valDivide", wordp->u.val.floating); break;
                case Drand:  printf(INDNT); printf("%*td"    " Pop rand\n", 5, wordp - mem->word); --In; break;
                case Tbl:  printf(INDNT); printf("%*td"      " Pop tbl\n", 5, wordp - mem->word); --In; break;
                case Out:  printf(INDNT); printf("%*td"      " Pop out\n", 5, wordp - mem->word); --In; break;
                case Outln:  printf(INDNT); printf("%*td"    " Pop outln\n", 5, wordp - mem->word); --In; break;
                case Idx:  printf(INDNT); printf("%*td"      " Pop idx\n", 5, wordp - mem->word); --In; break;
                case QIdx:  printf(INDNT); printf("%*td"     " PopPop Qidx\n", 5, wordp - mem->word); --In; --In; break;
                case EIdx:  printf(INDNT); printf("%*td"     " Pop Eidx\n", 5, wordp - mem->word); --In; break;
                case Extent: printf(INDNT); printf("%*td"     " Pop extent\n", 5, wordp - mem->word); --In; break;
                case Rank: printf(INDNT); printf("%*td"      " Pop rank\n", 5, wordp - mem->word); --In; break;
                case Eval: printf(INDNT); printf("%*td"      " eval\n", 5, wordp - mem->word); --In; break;
                case NoOp:
                    naam = "";
                    printf(INDNT); printf("%*td" " %-32s %s\n", 5, wordp - mem->word, "NoOp", naam);
                    break;
                case TheEnd:
                default:
                    printf(INDNT); printf("%*td" " %-32s %d\n", 5, wordp - mem->word, "default", wordp->action);
                    ;
            }
        }
    printf(INDNT); printf("%*td" " TheEnd\n", 5, wordp - mem->word);
    if(mem->functions)
        printmem(mem->functions);
    if(mem->nextFnc)
        printmem(mem->nextFnc);
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
    psk operatorNode = (psk)bmalloc(sizeof(knode));
    if(operatorNode)
        {
        operatorNode->v.fl = (operator | SUCCESS | READY) & COPYFILTER;
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
    psk res = (psk)bmalloc(bytes);
    if(res)
        {
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
    psk res = (psk)bmalloc(bytes);
    if(res)
        {
        strcpy((char*)(res)+offsetof(sk, u.sobj), jotter);
        res->v.fl = READY | SUCCESS BITWISE_OR_SELFMATCHING;
        res->v.fl &= COPYFILTER;
        }
    return res;
    }

static psk IntegerNode(double val)
    {
    char jotter[512];
    if(val <= (double)INT64_MIN || val > (double)INT64_MAX)
        {
#if defined __EMSCRIPTEN__
//    long long long1 = (long long)1;
        int64_t long1 = (int64_t)1;
#else
        int64_t long1 = (int64_t)1;
#endif
        double fcac = (double)(long1 << 52);
        int exponent;
        ULONG flg = 0;
        if(val < 0)
            {
            flg = MINUS;
            val = -val;
            }
        double mantissa = frexp(val, &exponent);
        /*
        The IEEE-754 double-precision float uses 11 bits for the exponent.
        The exponent field is an unsigned integer from 0 to 2047, in biased form:
        an exponent value of 1023 represents the actual zero.
        Exponents range from −1022 to +1023 because exponents of −1023 (all 0s)
        and +1024 (all 1s) are reserved for special numbers.
        */
        int64_t Mantissa = (int64_t)(fcac * mantissa);
        int shft;
        assert(Mantissa < (long1 << 52));
        assert(Mantissa != 0);
        for(shft = 52 - exponent; (Mantissa & long1) == 0; --shft, Mantissa >>= 1)
            ;

        if(flg & MINUS)
            sprintf(jotter, "-%" PRId64 "*2^%d", Mantissa, (-shft));
        else
            sprintf(jotter, "%" PRId64 "*2^%d", Mantissa, (-shft));
        return build_up(NULL, jotter, NULL);
        }
    else
        {
        size_t bytes = offsetof(sk, u.obj) + 1;
        if(val < 0)
            bytes += sprintf(jotter, "%" PRId64, -(int64_t)val);
        else
            bytes += sprintf(jotter, "%" PRId64, (int64_t)val);
        psk res = (psk)bmalloc(bytes);
        if(res)
            {
            strcpy((char*)(res)+offsetof(sk, u.sobj), jotter);
            res->v.fl = READY | SUCCESS | QNUMBER BITWISE_OR_SELFMATCHING;
            res->v.fl &= COPYFILTER;
            if((int64_t)val < 0)
                res->v.fl |= MINUS;
            else if((int64_t)val == 0)
                res->v.fl |= QNUL;
            res->v.fl &= ~DEFINITELYNONUMBER;
            }
        return res;
        }
    }

static psk FractionNode(double val)
    {
    char jotter[512];
    size_t bytes = offsetof(sk, u.obj) + 1;
#if defined __EMSCRIPTEN__
    uint64_t long1 = (uint64_t)1;
#else
    uint64_t long1 = (uint64_t)1;
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
    /*
    The IEEE-754 double-precision float uses 11 bits for the exponent.
    The exponent field is an unsigned integer from 0 to 2047, in biased form:
    an exponent value of 1023 represents the actual zero.
    Exponents range from −1022 to +1023 because exponents of −1023 (all 0s)
    and +1024 (all 1s) are reserved for special numbers.
    */
    int64_t Mantissa = (int64_t)(fcac * mantissa);
    psk res;
    assert(Mantissa < (long1 << 52));
    if(Mantissa)
        {
        int shft;
        for(shft = 52 - exponent; (Mantissa & long1) == 0; --shft, Mantissa >>= 1)
            ;

        if(shft == 0)
            bytes += sprintf(jotter, "%" PRId64, Mantissa);
        else if(shft < 0)
            { /* DANGER: multiplication can easily overflow! */
            if(shft > -12)
                bytes += sprintf(jotter, "%" PRId64, Mantissa << (-shft));
            else
                {
                if(flg & MINUS)
                    sprintf(jotter, "-%" PRId64 "*2^%d", Mantissa, (-shft));
                else
                    sprintf(jotter, "%" PRId64 "*2^%d", Mantissa, (-shft));
                return build_up(NULL, jotter, NULL);
                }
            }
        else
            { /* DANGER: multiplication can easily overflow! */
            if(shft < 64)
                {
                bytes += sprintf(jotter, "%" PRId64 "/" "%" PRIu64, Mantissa, (uint64_t)(long1 << shft));
                flg |= QFRACTION;
                }
            else
                {
                if(flg & MINUS)
                    sprintf(jotter, "-%" PRId64 "*2^%d", Mantissa, (-shft));
                else
                    sprintf(jotter, "%" PRId64 "*2^%d", Mantissa, (-shft));
                return build_up(NULL, jotter, NULL);
                }
            }
        }
    else
        bytes += sprintf(jotter, "0");

    res = (psk)bmalloc(bytes);
    if(res)
        {
        strcpy((char*)(res)+offsetof(sk, u.sobj), jotter);
        res->v.fl = flg;
        res->v.fl &= COPYFILTER;
        }
    return res;
    }

static psk eksportArray(forthvalue* val, size_t rank, size_t* extent)
    {
    psk res;
    psk head = createOperatorNode(COMMA);
    res = head;
    head->LEFT = same_as_w(&nilNode);
    if(rank > 1)
        { /* Go deeper */
        size_t stride = 1;
        for(size_t k = 0; k < rank - 1; ++k)
            stride *= extent[k];
        for(size_t r = extent[rank - 1]; --r > 0; val += stride)
            {
            head->RIGHT = createOperatorNode(WHITE);
            head = head->RIGHT;
            head->LEFT = eksportArray(val, rank - 1, extent);
            }
        head->RIGHT = eksportArray(val, rank - 1, extent);
        }
    else
        { /* Export list of values. */
        for(size_t r = *extent; --r > 0; ++val)
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
                        return TRUE;
                        }
                    }
                else
                    {
                    fortharray* a = getArrayPointer(arrp, name);
                    if(a)
                        {
                        psk res = 0;
                        res = eksportArray(a->pval, a->rank, a->extent);
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

static Boolean shortcutJumpChains(forthword* wordp)
    {
    forthword* wstart = wordp;
    Boolean res = FALSE;
    for(; wordp->action != TheEnd; ++wordp)
        {
        if(wordp->action == Branch)
            {
            forthword* label = wstart + wordp->offset;
            while(label->action == Branch)
                {
                label = wstart + label->offset;
                res = TRUE;
                }
            wordp->offset = (unsigned int)(label - wstart);
            }
            }
        //#define SHOWOPTIMIZATIONS
#ifdef SHOWOPTIMIZATIONS
    if(res) printf("shortcutJumpChains\n");
#endif
    return res;
        }

static Boolean combinePopBranch(forthword* wordp)
    {
    Boolean res = FALSE;
    for(; wordp->action != TheEnd; ++wordp)
        {
        if(wordp->action == Pop)
            {
            if(wordp[1].action == Branch)
                {
                wordp->action = PopBranch;
                wordp->offset = wordp[1].offset;
                res = TRUE;
                }
                }
            }
#ifdef SHOWOPTIMIZATIONS
    if(res) printf("combinePopBranch\n");
#endif
    return res;
        }

static Boolean combineBranchPopBranch(forthword* wstart)
    {
    Boolean res = FALSE;
    forthword* wordp = wstart;
    for(; wordp->action != TheEnd; ++wordp)
        {
        if(wordp->action == Branch)
            {
            forthword* label = wstart + wordp->offset;
            if(label->action == PopBranch)
                {
                wordp->action = PopBranch;
                wordp->offset = label->offset;
                wordp->u.logic = label->u.logic;
                res = TRUE;
                }
                }
            }
#ifdef SHOWOPTIMIZATIONS
    if(res) printf("combineBranchPopBranch\n");
#endif
    return res;
        }

static void markReachable(forthword* wordp, forthword* wstart, char* marks)
    {
    if(marks[wordp - wstart] == 1)
        return; /* Already visited! */

    for(; wordp->action != TheEnd; ++wordp)
        {
        marks[wordp - wstart] = 1;
        switch(wordp->action)
            {
                case var2stackBranch:
                case stack2varBranch:
                case Branch:
                case PopBranch:
                case valPushBranch:
                case val2stackBranch:
                    markReachable(wstart + wordp->offset, wstart, marks);
                    return;
                case Fless:
                case Fless_equal:
                case Fmore_equal:
                case Fmore:
                case Funequal:
                case Fequal:
                case FlessP:
                case Fless_equalP:
                case Fmore_equalP:
                case FmoreP:
                case FunequalP:
                case FequalP:
                    markReachable(wstart + wordp->offset, wstart, marks);
                    break;
                default:
                    ;
            }
        }
    }

static Boolean markUnReachable(forthword* wstart, char* marks)
    {
    Boolean res = FALSE;
    markReachable(wstart, wstart, marks);
    for(forthword* wordp = wstart; wordp->action != TheEnd; ++wordp)
        {
        if(wordp->action != NoOp && marks[wordp - wstart] != 1)
            {
            wordp->action = NoOp;
            res = TRUE;
            }
        }
#ifdef SHOWOPTIMIZATIONS
    if(res) printf("markUnReachable\n");
#endif
    return res;
    }

static Boolean dissolveNextWordBranches(forthword* wstart)
    {
    Boolean res = FALSE;
    for(forthword* wordp = wstart; wordp->action != TheEnd; ++wordp)
        {
        switch(wordp->action)
            {
                case var2stackBranch:
                    if(wstart + wordp->offset == wordp + 1)
                        {
                        wordp->action = var2stack;
                        res = TRUE;
                        }
                    break;
                case stack2varBranch:
                    if(wstart + wordp->offset == wordp + 1)
                        {
                        wordp->action = stack2var;
                        res = TRUE;
                        }
                    break;
                case Branch:
                    if(wstart + wordp->offset == wordp + 1)
                        {
                        wordp->action = NoOp;
                        res = TRUE;
                        }
                    break;
                case PopBranch:
                    {
                    if(wstart + wordp->offset == wordp + 1)
                        {
                        wordp->action = Pop;
                        res = TRUE;
                        }
                    break;
                    }
                case valPushBranch:
                    {
                    if(wstart + wordp->offset == wordp + 1)
                        {
                        wordp->action = valPush;
                        res = TRUE;
                        }
                    break;
                    }
                case val2stackBranch:
                    {
                    if(wstart + wordp->offset == wordp + 1)
                        {
                        wordp->action = val2stack;
                        res = TRUE;
                        }
                    break;
                    }
                default:
                    ;
            }
        }
#ifdef SHOWOPTIMIZATIONS
    if(res) printf("dissolveNextWordBranches\n");
#endif
    return res;
    }

static Boolean combineUnconditionalBranchTovalPush(forthword* wstart)
    {
    Boolean res = FALSE;
    for(forthword* wordp = wstart; wordp->action != TheEnd; ++wordp)
        {
        switch(wordp->action)
            {
                case Branch:
                    {
                    forthword* label = wstart + wordp->offset;
                    if(label->action == valPush)
                        {
                        wordp->action = valPushBranch;
                        wordp->u.val = label->u.val;
                        wordp->offset += 1;
                        res = TRUE;
                        }
                    break;
                    }
                case PopBranch:
                    {
                    forthword* label = wstart + wordp->offset;
                    if(label->action == valPush)
                        {
                        wordp->action = val2stackBranch;
                        wordp->u.val = label->u.val;
                        wordp->offset += 1;
                        res = TRUE;
                        }
                    break;
                    }
                default:
                    ;
            }
        }
#ifdef SHOWOPTIMIZATIONS
    if(res) printf("combineUnconditionalBranchTovalPush\n");
#endif
    return res;
    }

static Boolean stack2var_var2stack(forthword* wstart)
    {
    Boolean res = FALSE;
    for(forthword* wordp = wstart; wordp->action != TheEnd; ++wordp)
        {
        switch(wordp->action)
            {
                case stack2var:
                    {
                    switch(wordp[1].action)
                        {
                            case var2stackBranch:
                                {
                                if(wordp->u.valp == wordp[1].u.valp)
                                    {
                                    *wordp = wordp[1];
                                    wordp->action = stack2varBranch;
                                    res = TRUE;
                                    }
                                break;
                                }
                            default:
                                ;
                        }
                    break;
                    }
                default:
                    ;
            }
        }
#ifdef SHOWOPTIMIZATIONS
    if(res) printf("stack2var_var2stack\n");
#endif
    return res;
    }

static Boolean removeIdempotentActions(forthword* wstart)
    {
    Boolean res = FALSE;
    for(forthword* wordp = wstart; wordp->action != TheEnd; ++wordp)
        {
        switch(wordp->action)
            {
                case stack2var:
                    {
                    switch(wordp[1].action)
                        {
                            case stack2var:
                            case stack2varBranch:
                                if(wordp->u.valp == wordp[1].u.valp)
                                    {
                                    wordp->action = NoOp;
                                    res = TRUE;
                                    }
                                break;
                            default:
                                ;
                        }
                    break;
                    }
                case val2stack:
                    {
                    switch(wordp[1].action)
                        {
                            case val2stack:
                            case val2stackBranch:
                                if(wordp->u.val.floating == wordp[1].u.val.floating)
                                    {
                                    wordp->action = NoOp;
                                    res = TRUE;
                                    }
                                break;
                            default:
                                ;
                        }
                    break;
                    }
                case var2stack:
                    {
                    switch(wordp[1].action)
                        {
                            case var2stack:
                            case var2stackBranch:
                                if(wordp->u.valp == wordp[1].u.valp)
                                    {
                                    wordp->action = NoOp;
                                    res = TRUE;
                                    }
                                break;
                            default:
                                ;
                        }
                    break;
                    }
                default:
                    ;
            }
        }
#ifdef SHOWOPTIMIZATIONS
    if(res) printf("removeIdempotentActions\n");
#endif
    return res;
    }

static Boolean combinePushAndOperation(forthword* wstart)
    {
    Boolean res = FALSE;
    for(forthword* wordp = wstart; wordp->action != TheEnd; ++wordp)
        {
        switch(wordp->action)
            {
                case varPush:
                    {
                    switch(wordp[1].action)
                        {
                            case Plus:
                                wordp->action = varPlus;
                                wordp[1].action = NoOp;
                                res = TRUE;
                                break;
                            case Times:
                                wordp->action = varTimes;
                                wordp[1].action = NoOp;
                                res = TRUE;
                                break;
                            case Subtract:
                                wordp->action = varSubtract;
                                wordp[1].action = NoOp;
                                res = TRUE;
                                break;
                            case Divide:
                                wordp->action = varDivide;
                                wordp[1].action = NoOp;
                                res = TRUE;
                                break;
                            default:
                                ;
                        }
                    break;
                    }
                case valPush:
                    {
                    switch(wordp[1].action)
                        {
                            case Plus:
                                wordp->action = valPlus;
                                wordp[1].action = NoOp;
                                res = TRUE;
                                break;
                            case Times:
                                wordp->action = valTimes;
                                wordp[1].action = NoOp;
                                res = TRUE;
                                break;
                            case Subtract:
                                wordp->action = valSubtract;
                                wordp[1].action = NoOp;
                                res = TRUE;
                                break;
                            case Divide:
                                wordp->action = valDivide;
                                wordp[1].action = NoOp;
                                res = TRUE;
                                break;
                            default:
                                ;
                        }
                    break;
                    }
                default:
                    ;
            }
        }
#ifdef SHOWOPTIMIZATIONS
    if(res) printf("combinePushAndPlus\n");
#endif
    return res;
    }

static void markLabels(forthword* wstart, char* marks)
    {
    for(forthword* wordp = wstart; wordp->action != TheEnd; ++wordp)
        {
        switch(wordp->action)
            {
                case var2stackBranch:
                case stack2varBranch:
                case Branch:
                case PopBranch:
                case valPushBranch:
                case val2stackBranch:
                case Fless:
                case Fless_equal:
                case Fmore_equal:
                case Fmore:
                case Funequal:
                case Fequal:
                case FlessP:
                case Fless_equalP:
                case Fmore_equalP:
                case FmoreP:
                case FunequalP:
                case FequalP:
                    marks[wordp->offset] = 1;
                    break;
                default:
                    ;
            }
        }
    }

static Boolean combineval2stack(forthword* wstart, char* marks)
    {
    Boolean res = FALSE;
    markLabels(wstart, marks);
    for(forthword* wordp = wstart; wordp->action != TheEnd; ++wordp)
        {
        switch(wordp->action)
            {
                case Pop:
                    {
                    if(marks[1 + (wordp - wstart)] != 1)
                        {
                        switch(wordp[1].action)
                            {
                                case valPush:
                                    {
                                    wordp->action = NoOp;
                                    wordp[1].action = val2stack;
                                    res = TRUE;
                                    break;
                                    }
                                case varPush:
                                    {
                                    wordp->action = NoOp;
                                    wordp[1].action = var2stack;
                                    res = TRUE;
                                    break;
                                    }
                                default:
                                    ;
                            }
                        }
                    else
                        {
                        switch(wordp[1].action)
                            {
                                case varPush:
                                    {
                                    *wordp = wordp[1];
                                    wordp->action = var2stackBranch;
                                    wordp->offset = (unsigned int)((wordp + 2) - wstart);
                                    res = TRUE;
                                    break;
                                    }
                                default:
                                    ;
                            }
                        }
                    break;
                    }
                case PopBranch:
                    {
                    forthword* label = wstart + wordp->offset;
                    switch(label->action)
                        {
                            case varPush:
                                {
                                *wordp = *label;
                                wordp->action = var2stackBranch;
                                wordp->offset = (unsigned int)((label + 1) - wstart);
                                res = TRUE;
                                break;
                                }
                            default:
                                ;
                        }
                    }
                default:
                    ;
            }
        }
#ifdef SHOWOPTIMIZATIONS
    if(res) printf("combineval2stack\n");
#endif
    return res;
    }

static Boolean UnconditionalBranch(actionType calcaction)
    {
    switch(calcaction)
        {
            case var2stackBranch:
            case stack2varBranch:
            case Branch:
            case PopBranch:
            case valPushBranch:
            case val2stackBranch:
                return TRUE;
            default:
                return FALSE;
        }
    }
static Boolean IdemPotent(forthword* maybebranch, forthword* notbranch)
    {
    switch(notbranch->action)
        {
            case stack2var:
                {
                switch(maybebranch->action)
                    {
                        case stack2var:
                        case stack2varBranch:
                            if(notbranch->u.valp == maybebranch->u.valp)
                                {
                                return TRUE;
                                }
                            break;
                        default:
                            ;
                    }
                break;
                }
            case val2stack:
                {
                switch(maybebranch->action)
                    {
                        case val2stack:
                        case val2stackBranch:
                            if(notbranch->u.val.floating == maybebranch->u.val.floating)
                                {
                                return TRUE;
                                }
                            break;
                        default:
                            ;
                    }
                break;
                }
            case var2stack:
                {
                switch(maybebranch->action)
                    {
                        case var2stack:
                        case var2stackBranch:
                            if(notbranch->u.valp == maybebranch->u.valp)
                                {
                                return TRUE;
                                }
                            break;
                        default:
                            ;
                    }
                break;
                }
            default:
                ;
        }
    return FALSE;
    }



static Boolean eliminateBranch(forthword* wstart)
/*
    118 Branch                     120   fand
    119 val2stackBranch            129   0.000000
    120 varPush                          J
    121 valPush                          10.000000
    122 PopB < 119
    123 var2stack                        J
    124 valPush                          25.600000
    125 Pop times
    126 floor
    127 stack2ArrElm                     T
    128 var2stackBranch            131   j           // This must be some unconditional branch
    129 stack2ArrElm                     T

    118 NoOp                             fand        // Branch -> NoOp
    119 varPush                          J
    120 valPush                          10.000000
    121 PopB < 128                                   // 119 -> 129 -> 128
    122 var2stack                        J
    123 valPush                          25.600000
    124 Pop times
    125 floor
    126 stack2ArrElm                     T
    127 var2stackBranch            131   j
    128 val2stack                        0.000000    //val2stackBranch -> val2stack
    129 stack2ArrElm                     T

Label 119 replaced by (label of 119)
Labels [119 - 129) decremented by 1
*/
    {
    Boolean res = FALSE;
    for(forthword* wordp = wstart; wordp->action != TheEnd; ++wordp)
        {
        if(UnconditionalBranch(wordp->action))
            {
            if(wordp->offset + wstart == wordp + 2)
                {
                unsigned int lo = (unsigned int)((wordp + 1) - wstart);
                forthword tmp = wordp[1];
                if(UnconditionalBranch(tmp.action))
                    {
                    unsigned int hi = tmp.offset - 1;
                    if(hi > lo)
                        {
                        forthword* high = wstart + hi;
                        if(UnconditionalBranch(high->action) || IdemPotent(wordp + 1, high))
                            {
                            switch(tmp.action)
                                {
                                    case var2stackBranch:
                                        {
                                        tmp.action = var2stack;
                                        break;
                                        }
                                    case stack2varBranch:
                                        {
                                        tmp.action = stack2var;
                                        break;
                                        }
                                    case Branch:
                                        {
                                        tmp.action = NoOp;
                                        break;
                                        }
                                    case PopBranch:
                                        {
                                        tmp.action = Pop;
                                        break;
                                        }
                                    case valPushBranch:
                                        {
                                        tmp.action = valPush;
                                        break;
                                        }
                                    case val2stackBranch:
                                        {
                                        tmp.action = val2stack;
                                        break;
                                        }
                                    default:
                                        ;
                                }

                            forthword* w;
                            for(w = wordp + 1; w < high; ++w)
                                {
                                *w = w[1];
                                }
                            *w = tmp;
                            switch(wordp->action)
                                {
                                    case var2stackBranch:
                                        wordp->action = var2stack;
                                        break;
                                    case stack2varBranch:
                                        wordp->action = stack2var;
                                        break;
                                    case Branch:
                                        wordp->action = NoOp;
                                        break;
                                    case PopBranch:
                                        wordp->action = Pop;
                                        break;
                                    case valPushBranch:
                                        wordp->action = valPush;
                                        break;
                                    case val2stackBranch:
                                        wordp->action = val2stack;
                                        break;
                                    default:
                                        ;
                                }
                            for(w = wstart; w->action != TheEnd; ++w)
                                {
                                switch(w->action)
                                    {
                                        case var2stackBranch:
                                        case stack2varBranch:
                                        case Branch:
                                        case PopBranch:
                                        case valPushBranch:
                                        case val2stackBranch:
                                        case Fless:
                                        case Fless_equal:
                                        case Fmore_equal:
                                        case Fmore:
                                        case Funequal:
                                        case Fequal:
                                        case FlessP:
                                        case Fless_equalP:
                                        case Fmore_equalP:
                                        case FmoreP:
                                        case FunequalP:
                                        case FequalP:
                                            if(w->offset == lo)
                                                w->offset = hi;
                                            else if(w->offset > lo && w->offset <= hi)
                                                --(w->offset);
                                            break;
                                        default:
                                            ;
                                    }
                                }
                            res = TRUE;
                            }
                        }
                    }
                }
                    }
                }
#ifdef SHOWOPTIMIZATIONS
    if(res) printf("eliminateBranch\n");
#endif
    return res;
            }


static int removeNoOp(forthMemory* mem, int length)
    {
    forthword* wstart = mem->word;
    unsigned int* deltaoffset = calloc(length, sizeof(unsigned int));
    int newlength = 0;
    if(deltaoffset)
        {
        memset(deltaoffset, 0, length * sizeof(char));
        unsigned int delta = 0;
        forthword* wordp;
        for(wordp = wstart; wordp->action != TheEnd; ++wordp)
            {
            deltaoffset[wordp - wstart] = delta;
            if(wordp->action == NoOp)
                ++delta;
            }
        deltaoffset[wordp - wstart] = delta;
        for(wordp = wstart; wordp->action != TheEnd; ++wordp)
            {
            switch(wordp->action)
                {
                    case var2stackBranch:
                    case stack2varBranch:
                    case Branch:
                    case PopBranch:
                    case valPushBranch:
                    case val2stackBranch:
                    case Fless:
                    case Fless_equal:
                    case Fmore_equal:
                    case Fmore:
                    case Funequal:
                    case Fequal:
                    case FlessP:
                    case Fless_equalP:
                    case Fmore_equalP:
                    case FmoreP:
                    case FunequalP:
                    case FequalP:
                        wordp->offset -= deltaoffset[wordp->offset];
                        break;
                    default:
                        ;
                }
            }
        newlength = length - delta;
        forthword* newword = bmalloc(newlength * sizeof(forthword));
        if(newword)
            {
            mem->word = newword;
            for(wordp = wstart; wordp->action != TheEnd; ++wordp)
                {
                if(wordp->action != NoOp)
                    {
                    *newword++ = *wordp;
                    }
                }
            *newword = *wordp;
            bfree(wstart);
            }
        free(deltaoffset);
        }
    return newlength;
    }

static Boolean combinePopThenPop(forthword* wstart, char* marks)
    {
    Boolean res = FALSE;
    markLabels(wstart, marks);
    for(forthword* wordp = wstart; wordp->action != TheEnd; ++wordp)
        {
        forthword* label;
        if(marks[(wordp + 1) - wstart] != 1) // Nobody is jumping to the next word
            {
            switch(wordp->action)
                {
                    case Fless:
                    case Fless_equal:
                    case Fmore_equal:
                    case Fmore:
                    case Funequal:
                    case Fequal:
                        label = wstart + wordp->offset;
                        switch(wordp[1].action)
                            {
                                case Pop:
                                    {
                                    if(label->action == PopBranch)
                                        {
                                        wordp->action = extraPopped(wordp->action, extraPop);
                                        wordp->offset = label->offset;
                                        wordp[1].action = NoOp;
                                        res = TRUE;
                                        }
                                    else if(label->action == val2stackBranch)
                                        {
                                        wordp->action = extraPopped(wordp->action, extraPop);
                                        label->action = valPushBranch;
                                        wordp[1].action = NoOp;
                                        res = TRUE;
                                        }
                                    break;
                                    }
                                case val2stack:
                                    {
                                    if(label->action == PopBranch)
                                        {
                                        wordp->action = extraPopped(wordp->action, extraPop);
                                        wordp->offset = label->offset;
                                        wordp[1].action = valPush;
                                        res = TRUE;
                                        }
                                    break;
                                    }
                                case PopBranch:
                                    {
                                    if(label->action == PopBranch)
                                        { /* see if backwards branch can be converted to branch to wordp+2 */
                                        if(label->offset == (wordp + 2) - wstart)
                                            {
                                            wordp->action = extraPopped(wordp->action, negations);
                                            wordp->offset = wordp[1].offset;
                                            wordp[1].action = NoOp;
                                            res = TRUE;
                                            }
                                        else /* We can still eliminate the backward branch */
                                            {
                                            wordp->action = extraPopped(wordp->action, extraPop);
                                            wordp->offset = label->offset;
                                            wordp[1].action = Branch;
                                            res = TRUE;
                                            }
                                        }
                                    else if(label->action == val2stackBranch)
                                        {
                                        wordp->offset = wordp[1].offset;
                                        wordp->action = extraPopped(wordp->action, negations);
                                        wordp[1] = *label;
                                        wordp[1].action = valPushBranch;
                                        res = TRUE;
                                        }
                                    break;
                                    }
                                case val2stackBranch:
                                    {
                                    if(label->action == PopBranch)
                                        {
                                        wordp->action = extraPopped(wordp->action, extraPop);
                                        wordp->offset = label->offset;
                                        wordp[1].action = valPushBranch;
                                        res = TRUE;
                                        }
                                    break;
                                    }
                                default:
                                    ;
                            }
                    default:
                        ;
                }
                }
            }
#ifdef SHOWOPTIMIZATIONS
    if(res) printf("combinePopThenPop\n");
#endif
    return res;
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

static forthMemory* calcnew(psk arg, forthMemory* parent, Boolean in_function);

static Boolean setArity(forthword* wordp, psk code, unsigned int expectedArity)
    {
    unsigned int arity = 1;
    psk tmp = code->RIGHT;
    for(; is_op(tmp) && Op(tmp) == COMMA; tmp = tmp->RIGHT)
        ++arity;
    if(expectedArity > 0 && arity != expectedArity)
        {
        /*TODO: check whether function does not take a single argument!*/
        errorprintf("The arity is %u, but %u arguments found.\n", expectedArity, arity);
        return FALSE;
        }
    wordp->offset = arity;
    return TRUE;
    }

static fortharray* haveArray(forthMemory* forthstuff, psk declaration, Boolean in_function)
    {
    psk pname = 0;
    psk extents = 0;
    if(is_op(declaration))
        {
        if(is_op(declaration->LEFT))
            {
            errorprintf("Array parameter declaration requires name.\n");
            return 0;
            }
        pname = declaration->LEFT;
        extents = declaration->RIGHT;

        if(in_function)
            {
            errorprintf("In functions, array parameter (here: [%s]) declarations do not take extent specs.\n", &(pname->u.sobj));
            }
        }
    else
        {
        pname = declaration;
        }

    if(HAS_VISIBLE_FLAGS_OR_MINUS(pname))
        {
        errorprintf("Array name may not have any prefixes.\n");
        return 0;
        }

    fortharray* a = getOrCreateArrayPointer(&(forthstuff->arr), &(pname->u.sobj), 0);
    if(!in_function && a && extents)
        {
        size_t rank = 1;
        psk kn;
        for(kn = extents; is_op(kn); kn = kn->RIGHT)
            {
            ++rank;
            }

        a->rank = rank;
        assert(a->extent == 0);
        a->extent = (size_t*)bmalloc(2 * rank * sizeof(size_t));/* twice: both extents and strides */
        if(a->extent)
            {
            a->stride = a->extent + rank;
            size_t* pextent;
            size_t totsize = 1;
            for(kn = extents, pextent = a->extent + rank - 1;; )
                {
                psk H;

                if(is_op(kn))
                    H = kn->LEFT;
                else
                    H = kn;

                if(INTEGER_POS(H))
                    *pextent = strtol(&(H->u.sobj), 0, 10);
                else if((H->v.fl & VISIBLE_FLAGS) == INDIRECT)
                    *pextent = 0; /* extent still unknown. */
                else
                    {
                    errorprintf("Extent specification \"%s\" of array \"%s\" invalid. It must be a positive number or a variable name prefixed with '!'\n", (is_op(H) ? "(non-leaf)" : &(H->u.sobj)), a->name);
                    return 0;
                    }

                totsize *= *pextent;
                if(!is_op(kn))
                    break;
                kn = kn->RIGHT;
                --pextent;
                }
            a->stride[0] = a->extent[0];
            for(size_t extent_index = 1; extent_index < rank;)
                {
                a->stride[extent_index] = a->stride[extent_index - 1] * a->extent[extent_index];
                ++extent_index;
                }

            if(totsize > 0)
                {
                if(!initialise(a, totsize))
                    {
                    bfree(a->extent);
                    a->extent = 0;
                    a->stride = 0;
                    return 0;
                    }
                }
            else
                a->size = 0;
            }
        }
    return a;
    }

Boolean okatomicarg(psk code)
    {
    if((code->v.fl & QNUMBER)
       || (code->v.fl & QDOUBLE)
       || (code->v.fl & (INDIRECT | UNIFY))
       )
        return TRUE;
    return FALSE;
    }

static int argcount(psk rhs)
    {
    if(is_op(rhs))
        {
        if(Op(rhs) == FUN)
            return 1;
        else if(is_op(rhs->LEFT))
            {
            switch(Op(rhs->LEFT))
                {
                    case FUN:
                    case PLUS:
                    case TIMES:
                    case EXP:
                    case LOG:
                        return 1;
                }
            showProblematicNode("Function argument must be atomic. (A number or a variable prefixed with \"!\")\n", rhs);
            return -1;
            }
        else if((rhs->LEFT->u.sobj) == 0)
            {
            errorprintf("Function argument is empty string. (It must be a number or a variable prefixed with \"!\")\n");
            return -1;
            }
        else if(!okatomicarg(rhs->LEFT))
            {
            errorprintf("Function argument is not valid. (It must be a number or a variable prefixed with \"!\")\n");
            return -1;
            }
        else
            {
            static int deep;
            deep = argcount(rhs->RIGHT);
            if(deep <= 0)
                {
                return deep;
                }
            return 1 + deep;
            }
        }
    else if((rhs->u.sobj) == 0)
        {
        errorprintf("Last (or only) function argument is empty string. (It must be a number or a variable prefixed with \"!\")\n");
        return -1;
        }
    else if(!okatomicarg(rhs))
        {
        errorprintf("Last function argument is not valid. (It must be a number or a variable prefixed with \"!\")\n");
        return -1;
        }
    return 1;
    }

static fortharray* namedArray(char* name, forthMemory* mem, psk node)
    {
    if(!is_op(node))
        {
        if((node->v.fl & VISIBLE_FLAGS) == 0)
            {
            char* arrname = &node->u.sobj;
            fortharray* arr;
            for(arr = mem->arr; arr; arr = arr->next)
                {
                if(!strcmp(arr->name, arrname))
                    {
                    return arr;
                    }
                }
            return 0;

            }
        else
            {
            errorprintf("First argument of \"%s\" must be without any prefixes.\n", name);
            return 0;
            }
        }
    else
        {
        errorprintf("First argument of \"%s\" must be an atom.\n", name);
        return 0;
        }
    }

static forthword* polish2(forthMemory* mem, jumpblock* jumps, psk code, forthword* wordp, Boolean commentsAllowed)
/* jumps points to 5 words as explained for AND and OR */
    {
    if(wordp == 0)
        return 0; /* Something wrong happened. */
    forthvariable** varp = &(mem->var);
    fortharray** arrp = &(mem->arr);
    switch(Op(code))
        {
            case EQUALS:
                {
                if(!calcnew(code, mem, TRUE))
                    return 0;
                mustpop = enopop;
                return wordp;
                }
            case PLUS:
                wordp = polish2(mem, jumps, code->LEFT, wordp, FALSE);
                wordp = polish2(mem, jumps, code->RIGHT, wordp, FALSE);
                if(wordp == 0)
                    return 0; /* Something wrong happened. */
                wordp->action = Plus;
                wordp->offset = 0;
                mustpop = epop;
                return ++wordp;
            case TIMES:
                wordp = polish2(mem, jumps, code->LEFT, wordp, FALSE);
                wordp = polish2(mem, jumps, code->RIGHT, wordp, FALSE);
                if(wordp == 0)
                    return 0; /* Something wrong happened. */
                wordp->action = Times;
                wordp->offset = 0;
                mustpop = epop;
                return ++wordp;
            case EXP:
                wordp = polish2(mem, jumps, code->RIGHT, wordp, FALSE); /* SIC! */
                wordp = polish2(mem, jumps, code->LEFT, wordp, FALSE);
                if(wordp == 0)
                    return 0; /* Something wrong happened. */
                wordp->action = Pow;
                wordp->offset = 0;
                mustpop = epop;
                return ++wordp;
            case LOG:
                wordp = polish2(mem, jumps, code->LEFT, wordp, FALSE);
                wordp = polish2(mem, jumps, code->RIGHT, wordp, FALSE);
                if(wordp == 0)
                    return 0; /* Something wrong happened. */
                wordp->action = Log;
                wordp->offset = 0;
                mustpop = epop;
                return ++wordp;
            case AND:
                {
                /*
                            0 or 1      Result of comparison between to doubles
                        AND @1          address to jump to 'if false' == end of RIGHT
                            RIGHT       wstart of 'if true' branch
                            ....
                            ....        end of 'if true' branch (RIGHT)
                @1:         ....
                            ....

                */
                forthword* saveword;
                saveword = wordp;
                forthword* lhs = wordp + sizeof(jumpblock) / sizeof(forthword);
                jumpblock* j5 = (jumpblock*)wordp;
                wordp = polish2(mem, j5, code->LEFT, lhs, TRUE);
                if(wordp == 0)
                    {
                    mustpop = enopop;
                    return 0; /* Something wrong happened. */
                    }
                else if(wordp == lhs) /* LHS is function definition or another empty statement. Ignore this & operator. */
                    {
                    wordp = polish2(mem, jumps, code->RIGHT, saveword, TRUE);
                    return wordp;
                    }
                else
                    {
                    wordp->action = Branch;
                    wordp->u.logic = fand;
                    wordp->offset = (unsigned int)((j5->j + ((mustpop == epop) ? epopS : eS)) - mem->word);
                    ++wordp;
                    j5->j[estart].offset = (unsigned int)(((j5->j + estart) + sizeof(jumpblock) / sizeof(forthword)) - mem->word);
                    j5->j[estart].action = Branch;
                    j5->j[estart].u.logic = fand;
                    j5->j[epopS].offset = 1;
                    j5->j[epopS].action = Pop;
                    j5->j[epopS].u.logic = fand;
                    j5->j[eS].offset = (unsigned int)(wordp - mem->word);
                    j5->j[eS].action = Branch;
                    j5->j[eS].u.logic = fand;
                    j5->j[epopF].offset = 1;
                    j5->j[epopF].action = Pop;
                    j5->j[epopF].u.logic = fand;
                    j5->j[eF].offset = (unsigned int)((jumps->j + eF) - mem->word);
                    j5->j[eF].action = Branch;
                    j5->j[eF].u.logic = fand;
                    saveword = wordp;
                    wordp = polish2(mem, jumps, code->RIGHT, wordp, TRUE);
                    if(!wordp)
                        {
                        //showProblematicNode("wordp==0", code->RIGHT);
                        return 0;
                        }
                    if(wordp == saveword)
                        {
                        wordp->action = NoOp;
                        wordp->u.logic = fand;
                        }
                    else
                        {
                        wordp->action = Branch;
                        wordp->u.logic = fand;
                        wordp->offset = (unsigned int)((jumps->j + ((mustpop == epop) ? epopS : eS)) - mem->word);
                        }
                    ++wordp;
                    }
                mustpop = enopop;
                return wordp;
                }
            case OR: /* jump if true, continue if false */
                {
                /*
                            0 or 1      Result of comparison between to doubles
                        OR  @1          address to jump to 'if true' == end of RIGHT
                            RIGHT       wstart of 'if false' branch
                            ....
                            ....        end of 'if false' branch (RIGHT)
                @1:         ....
                            ....

                */
                forthword* saveword = wordp;
                forthword* lhs = wordp + sizeof(jumpblock) / sizeof(forthword);
                jumpblock* j5 = (jumpblock*)wordp;
                wordp = polish2(mem, j5, code->LEFT, lhs, TRUE);
                if(wordp == 0)
                    {
                    mustpop = enopop;
                    return 0; /* Something wrong happened. */
                    }
                else if(wordp == lhs) /* LHS is empty. Ignore this | operator. */
                    {
                    return saveword;
                    }
                else
                    {
                    wordp->action = Branch;
                    wordp->u.logic = fOr;
                    wordp->offset = (unsigned int)((j5->j + ((mustpop == epop) ? epopS : eS)) - mem->word);
                    ++wordp;
                    j5->j[estart].offset = (unsigned int)(((j5->j + estart) + sizeof(jumpblock) / sizeof(forthword)) - mem->word);
                    j5->j[estart].action = Branch;
                    j5->j[estart].u.logic = fOr;
                    j5->j[epopS].offset = 1;
                    j5->j[epopS].action = Pop;
                    j5->j[epopS].u.logic = fOr;
                    j5->j[eS].offset = (unsigned int)(&(jumps->j[eS]) - mem->word);
                    j5->j[eS].action = Branch;
                    j5->j[eS].u.logic = fOr;
                    j5->j[epopF].offset = 1;
                    j5->j[epopF].action = Pop;
                    j5->j[epopF].u.logic = fOr;
                    j5->j[eF].offset = (unsigned int)(wordp - mem->word);
                    j5->j[eF].action = Branch;
                    j5->j[eF].u.logic = fOr;
                    saveword = wordp;
                    wordp = polish2(mem, jumps, code->RIGHT, wordp, TRUE);
                    if(wordp == 0)
                        {
                        return 0;
                        }
                    wordp->action = Branch;
                    wordp->u.logic = fOr;
                    wordp->offset = (unsigned int)((jumps->j + ((mustpop == epop) ? epopS : eS)) - mem->word);
                    ++wordp;
                    }
                mustpop = enopop;
                return wordp;
                }
            case MATCH:
                {
                wordp = polish2(mem, jumps, code->LEFT, wordp, FALSE);
                wordp = polish2(mem, jumps, code->RIGHT, wordp, FALSE);
                if(wordp == 0)
                    {
                    mustpop = enopop;
                    return 0; /* Something wrong happened. */
                    }
                if(!(code->RIGHT->v.fl & UNIFY) & !is_op(code->RIGHT))
                    {
                    if(FLESS(code->RIGHT))
                        {
                        wordp->action = Fless;
                        }
                    else if(FLESS_EQUAL(code->RIGHT))
                        {
                        wordp->action = Fless_equal;
                        }
                    else if(FMORE_EQUAL(code->RIGHT))
                        {
                        wordp->action = Fmore_equal;
                        }
                    else if(FMORE(code->RIGHT))
                        {
                        wordp->action = Fmore;
                        }
                    else if(FUNEQUAL(code->RIGHT))
                        {
                        wordp->action = Funequal;
                        }
                    else if(FLESSORMORE(code->RIGHT))
                        {
                        wordp->action = Funequal;
                        }
                    else if(FEQUAL(code->RIGHT))
                        {
                        wordp->action = Fequal;
                        }
                    else if(FNOTLESSORMORE(code->RIGHT))
                        {
                        wordp->action = Fequal;
                        }
                    else if(VLESS(code->RIGHT))
                        {
                        wordp->action = Fless;
                        }
                    else if(VLESS_EQUAL(code->RIGHT))
                        {
                        wordp->action = Fless_equal;
                        }
                    else if(VMORE_EQUAL(code->RIGHT))
                        {
                        wordp->action = Fmore_equal;
                        }
                    else if(VMORE(code->RIGHT))
                        {
                        wordp->action = Fmore;
                        }
                    else if(VUNEQUAL(code->RIGHT))
                        {
                        wordp->action = Funequal;
                        }
                    else if(VLESSORMORE(code->RIGHT))
                        {
                        wordp->action = Funequal;
                        }
                    else if(VEQUAL(code->RIGHT))
                        {
                        wordp->action = Fequal;
                        }
                    else if(VNOTLESSORMORE(code->RIGHT))
                        {
                        wordp->action = Fequal;
                        }
                    else if(ILESS(code->RIGHT))
                        {
                        wordp->action = Fless;
                        }
                    else if(ILESS_EQUAL(code->RIGHT))
                        {
                        wordp->action = Fless_equal;
                        }
                    else if(IMORE_EQUAL(code->RIGHT))
                        {
                        wordp->action = Fmore_equal;
                        }
                    else if(IMORE(code->RIGHT))
                        {
                        wordp->action = Fmore;
                        }
                    else if(IUNEQUAL(code->RIGHT))
                        {
                        wordp->action = Funequal;
                        }
                    else if(ILESSORMORE(code->RIGHT))
                        {
                        wordp->action = Funequal;
                        }
                    else if(IEQUAL(code->RIGHT))
                        {
                        wordp->action = Fequal;
                        }
                    else if(INOTLESSORMORE(code->RIGHT))
                        {
                        wordp->action = Fequal;
                        }
                    else
                        return wordp;
                    wordp->offset = (unsigned int)(&(jumps->j[epopF]) - mem->word);
                    mustpop = epop;
                    return ++wordp;
                    }
                return wordp;
                }
            case FUU: 
                {
                if(!strcmp(&code->LEFT->u.sobj, "eval")) /* eval'(normal Bracmat code) */
                    {
                    wordp->action = Eval;
                    wordp->u.Pnode = same_as_w(code->RIGHT);
                    mustpop = enopop;
                    return ++wordp;
                    }
                else /* whl'(blbla) */
                    {
                    forthword* saveword = wordp;
                    forthword* loop = wordp + sizeof(jumpblock) / sizeof(forthword);
                    jumpblock* j5 = (jumpblock*)wordp;
                    wordp = polish2(mem, j5, code->RIGHT, loop, FALSE);
                    if(wordp == 0)
                        {
                        mustpop = enopop;
                        return 0; /* Something wrong happened. */
                        }
                    else if(wordp == loop) /* Loop body is function definition. Ignore this ' operator. */
                        {
                        mustpop = enopop;
                        return saveword;
                        }
                    else
                        {
                        wordp->action = Branch;
                        wordp->u.logic = fwhl;
                        wordp->offset = (unsigned int)((j5->j + ((mustpop == epop) ? epopS : eS)) - mem->word); /* If all good, jump back to wstart of loop */

                        ++wordp;

                        j5->j[estart].offset = (unsigned int)(((j5->j + estart) + sizeof(jumpblock) / sizeof(forthword)) - mem->word);
                        j5->j[estart].action = Branch;
                        j5->j[estart].u.logic = fwhl;
                        j5->j[epopS].offset = 1;
                        j5->j[epopS].action = Pop;
                        j5->j[epopS].u.logic = fwhl;
                        j5->j[eS].offset = j5->j[estart].offset;
                        j5->j[eS].action = Branch;
                        j5->j[eS].u.logic = fwhl;
                        j5->j[epopF].offset = 1;
                        j5->j[epopF].action = Pop;
                        j5->j[epopF].u.logic = fwhl;
                        j5->j[eF].offset = (unsigned int)(&(jumps->j[eS]) - mem->word); /* whl loop terminates when one of the steps in the loop failed */
                        j5->j[eF].action = Branch;
                        j5->j[eF].u.logic = fwhl;
                        mustpop = enopop;
                        return wordp;
                        }
                    }
                }
            case FUN:
                {
                Etriple* ep = etriples;
                char* name = &code->LEFT->u.sobj;
                psk rhs = code->RIGHT;
                if(!strcmp(name, "tbl"))
                    { /* Check that name is array name and that arity is correct. */
                    if(is_op(rhs))
                        {
                        fortharray* arr = namedArray("tbl", mem, rhs->LEFT);
                        if(arr == 0)
                            {
                            arr = haveArray(mem, rhs, FALSE);
                            if(arr != 0 && arr->size != 0)
                                return wordp; /* Arguments are fixed. Memory is allocated foNo nr array cells. No need to reevaluate. */
                            }

                        if(arr == 0)
                            {
                            errorprintf("tbl:Array \"%s\"is not declared\n", &(rhs->LEFT->u.sobj));
                            return 0;
                            }
                        }
                    else
                        {
                        errorprintf("Right hand side of \"tbl$\" must be at least two arguments: an array name and one or more extents.\n");
                        return 0;
                        }
                    }
                else if(!strcmp(name, "idx"))
                    { /* Check that name is array name and that arity is correct. */
                    if(is_op(rhs))
                        {
                        fortharray* arr = namedArray("idx", mem, rhs->LEFT);
                        if(arr == 0)
                            {
                            showProblematicNode("idx:Array is not declared: ", code);
                            return 0;
                            }

                        size_t h = 1;
                        psk R;
                        for(R = rhs; Op(R->RIGHT) == COMMA; R = R->RIGHT)
                            ++h;
                        size_t rank = arr->rank;
                        if(rank == 0)
                            {
                            //                                    errorprintf( "idx: Array \"%s\" has unknown rank and extent(s). Assuming %zu, based on idx.\n", arrname, h);
                            rank = arr->rank = h;
                            //                                    return 0;
                            }
                        if(h != rank)
                            {
                            errorprintf("idx: Array \"%s\" expects %zu arguments. %zu have been found\n", arr->name, rank, h);
                            return 0;
                            }
                        if(arr->extent != 0)
                            {
                            size_t* rng = arr->extent + rank;
                            for(R = rhs->RIGHT; ; R = R->RIGHT)
                                {
                                psk Arg;
                                --rng;
                                if(rng < arr->extent)
                                    break; /* Why this can happen: array with rank 1, and Arg is a non atomic expression that eventually evaluates to a single integer.*/
                                if(*rng != 0) /* If 0, extent is still unknown. Has to wait until running.  */
                                    {
                                    if(is_op(R))
                                        Arg = R->LEFT;
                                    else
                                        Arg = R;
                                    if(INTEGER_NOT_NEG(Arg))
                                        {
                                        long index = strtol(&(Arg->u.sobj), 0, 10);
                                        if(index < 0 || index >= (long)(*rng))
                                            {
                                            errorprintf("idx: Array \"%s\": index %zu is out of bounds 0 <= index < %zu, found %ld\n", arr->name, rank - (rng - arr->extent), *rng, index);
                                            return 0;
                                            }
                                        }
                                    }
                                if(!is_op(R))
                                    break;
                                }
                            }
                        }
                    else
                        {
                        errorprintf("Right hand side of \"idx$\" must be at least two arguments: an array name and one or more extents.\n");
                        return 0;
                        }
                    }
                else if(!strcmp(name, "rank"))
                    {
                    fortharray* arr = namedArray("rank", mem, rhs);
                    if(arr == 0)
                        {
                        errorprintf("\"extent\" takes one argument: the name of an array.\n");
                        return 0;
                        }
                    }
                else if(!strcmp(name, "extent"))
                    {
                    if(!is_op(rhs) || namedArray("extent", mem, rhs->LEFT) == 0 || argcount(rhs->RIGHT) != 1)
                        {
                        errorprintf("\"extent\" takes two arguments: the name of an array and the extent index.\n");
                        showProblematicNode("Here", rhs);
                        return 0;
                        }
                    }
                else
                    {
                    /* Check whether built in or user defined. */

                    forthMemory* func = 0, * childMem = 0;
                    for(forthMemory* currentMem = mem; currentMem && !func; childMem = currentMem, currentMem = currentMem->parent)
                        {
                        for(func = currentMem->functions; func; func = func->nextFnc)
                            {
                            if(func != childMem // No recursion! (Has to be tested during compilation)
                               && func->name
                               && !strcmp(func->name, name)
                               )
                                break;
                            }
                        }

                    if(func)
                        {
                        parameter* parms;
                        if(func->nparameters > 0)
                            {
                            for(parms = func->parameters + func->nparameters; --parms >= func->parameters;)
                                {
                                if(!is_op(rhs) && parms > func->parameters)
                                    {
                                    errorprintf("Too few parameters when calling \"%s\".\n",name);
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
                                        errorprintf("Array name expected in call to \"%s\".\n",name);
                                        return 0;
                                        }
                                    else
                                        {
                                        if(HAS_VISIBLE_FLAGS_OR_MINUS(parm))
                                            {
                                            errorprintf("Parameter \"%s\", while calling \"%s\": You seem to pass a scalar where an array is expected.\n",&(parm->u.sobj),name);
                                            return 0;
                                            }
                                        }
                                    }
                                if(is_op(rhs))
                                    rhs = rhs->RIGHT;
                                else
                                    {
                                    assert(parms == func->parameters);
                                    }
                                }
                            }
                        }
                    else
                        {
                        int ArgCount = argcount(rhs);
                        if(ArgCount < 0)
                            {
                            errorprintf("Function \"%s\" has an invalid argument.\n", name);
                            return 0;
                            }
                        else if(ArgCount == 0)
                            {
                            errorprintf("Function \"%s\" used without argument(s).\n", name);
                            return 0;
                            }
                        }
                    }
                wordp = polish2(mem, jumps, code->RIGHT, wordp, FALSE);
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
                                if(!setArity(wordp, code, ep->arity))
                                    return 0;
                            mustpop = epop;
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
                                if(!setArity(wordp, code, ep->arity))
                                    return 0;
                            mustpop = epop;
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
                            if(!setArity(wordp, code, ep->arity))
                                {
                                errorprintf("Argument error in call to \"%s\".\n", name);
                                return 0;
                                }
                            mustpop = enopop;
                            return ++wordp;
                            }
                        }
                    }

                wordp->action = Afunction;
                forthMemory* func = 0, * childMem = 0;
                for(forthMemory* currentMem = mem; currentMem && !func; childMem = currentMem, currentMem = currentMem->parent)
                    {
                    for(func = currentMem->functions; func; func = func->nextFnc)
                        {
                        if(func != childMem // No recursion! (Has to be tested during compilation)
                           && func->name
                           && !strcmp(func->name, name)
                           )
                            {
                            setArity(wordp, code, (unsigned int)func->nparameters);
                            wordp->action = Afunction;
                            wordp->u.that = func;
                            mustpop = epop;
                            return ++wordp;
                            }
                        }
                    }


                errorprintf("Function named \"%s\" not found.\n", name);
                return 0; /* Something wrong happened. */
                }
            default:
                {
                if(is_op(code)) /* e.g. COMMA */
                    {
                    wordp = polish2(mem, jumps, code->LEFT, wordp, FALSE);
                    wordp = polish2(mem, jumps, code->RIGHT, wordp, FALSE);
                    mustpop = enopop;
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
                        wordp->action = valPush;
                        /*When executing, push number onto the data stack*/
                        }
                    else if(code->v.fl & QDOUBLE)
                        {
                        wordp->u.val.floating = strtod(&(code->u.sobj), 0);
                        if(HAS_MINUS_SIGN(code))
                            {
                            wordp->u.val.floating = -(wordp->u.val.floating);
                            }
                        wordp->action = valPush;
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
                            wordp->action = valPush;
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
                                    wordp->action = (code->v.fl & INDIRECT) ? ArrElmValPush : stack2ArrElm;
                                    }
                                else
                                    {
                                    fortharray* a = getArrayPointer(arrp, &(code->u.sobj));
                                    if(a)
                                        {
                                        wordp->u.arrp = a;
                                        wordp->action = (code->v.fl & INDIRECT) ? ArrElmValPush : stack2ArrElm;
                                        }
                                    else
                                        {
                                        forthvariable* var = createVariablePointer(varp, &(code->u.sobj));
                                        if(var)
                                            wordp->u.valp = &(var->val);
                                        wordp->action = (code->v.fl & INDIRECT) ? varPush : stack2var;
                                        }
                                    }
                                }
                            else
                                {
                                wordp->u.valp = &(v->val);
                                wordp->action = (code->v.fl & INDIRECT) ? varPush : stack2var;
                                /* When executing,
                                * varPush: follow the pointer wordp->u.valp and push the pointed-at value onto the stack.
                                * stack2var:  follow the pointer wordp->u.valp and assign to that address the value that is on top of the stack. Do not change the stack.
                                */
                                }
                            }
                        else if(/*code->u.sobj == '\0' || */ commentsAllowed)
                            {
                            return wordp;
                            }
                        else
                            {
                            /*array*/
                            fortharray* a = getOrCreateArrayPointerButNoArray(arrp, &(code->u.sobj));
                            if(a != 0 || argumentArrayNumber(code) >= 0)
                                {
                                wordp->u.arrp = a;
                                wordp->action = valPush;
                                /* When executing,
                                * valPush:           valPush the value wordp->u.arrp onto the stack.
                                */
                                }
                            else
                                {
                                mustpop = enopop;
                                if(a == 0)
                                    return 0;
                                return wordp;
                                }
                            }
                        wordp->offset = 0;
                        }
                    ++wordp;
                    mustpop = epop;
                    return wordp;
                    }
                }
        }
    }

static Boolean setparm(size_t Ndecl, forthMemory* forthstuff, psk declaration, Boolean in_function)
    {
    parameter* npar;
    if(is_op(declaration->LEFT))
        {
        errorprintf("Parameter declaration requires 's' or 'a'.\n");
        return FALSE;
        }

    if(declaration->LEFT->u.sobj == 's') // scalar
        {
        if(is_op(declaration->RIGHT))
            {
            errorprintf("Scalar parameter declaration doesn't take extent.\n");
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
        fortharray* a = haveArray(forthstuff, declaration->RIGHT, in_function);
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

static void wipepnodes(forthword* word)
    {
    for(; word && word->action != TheEnd; ++word)
        {
        if(word->action == Eval)
            wipe(word->u.Pnode);
        }
    }

static Boolean calcdie(forthMemory* mem);

static Boolean functiondie(forthMemory* mem)
    {
    if(!mem)
        {
        errorprintf("calculation.functiondie does not deallocate, member \"mem\" is zero.\n");
        return FALSE;
        }
    forthvariable* curvarp = mem->var;
    fortharray* curarr = mem->arr;
    forthMemory* functions = mem->functions;
    parameter* parameters = mem->parameters;
    while(curvarp)
        {
        forthvariable* nextvarp = curvarp->next;
        if(curvarp->name)
            {
            bfree(curvarp->name);
            curvarp->name = 0;
            }
        bfree(curvarp);
        curvarp = nextvarp;
        }
    while(curarr)
        {
        fortharray* nextarrp = curarr->next;
        if(curarr->name)
            {
            /* extent and pval are not allocated when function is called.
               Instead, pointers are set to caller's pointers. */
            parameter* pars = 0;
            for(pars = parameters; pars < parameters + mem->nparameters; ++pars)
                {
                if(pars->scalar_or_array == Array)
                    {
                    if(!strcmp(pars->u.a->name, curarr->name))
                        {
                        pars->scalar_or_array = Neither;
                        break;
                        }
                    }
                }

            if(pars == parameters + mem->nparameters)
                { /* Reached end of loop without matching a parameter name.
                     So this array is not a function parameter.
                     So the function owns the data and should delete them.
                  */
                if(curarr->extent)
                    {
                    bfree(curarr->extent);
                    curarr->extent = 0;
                    curarr->stride = 0;
                    }
                if(curarr->pval)
                    {
                    bfree(curarr->pval);
                    curarr->pval = 0;
                    }
                }
            bfree(curarr->name);
            curarr->name = 0;
            }

        bfree(curarr);
        curarr = nextarrp;
        }
    mem->arr = 0;
    if(mem->parameters)
        {
        bfree(mem->parameters);
        mem->parameters = 0;
        }
    for(; functions;)
        {
        forthMemory* nextfunc = functions->nextFnc;
        functiondie(functions);
        functions = nextfunc;
        }
    mem->functions = 0;
    if(mem->name)
        {
        bfree(mem->name);
        mem->name = 0;
        }
    if(mem->word)
        {
        wipepnodes(mem->word);
        bfree(mem->word);
        mem->word = 0;
        }
    bfree(mem);
    return TRUE;
    }

static forthMemory* calcnew(psk arg, forthMemory* parent, Boolean in_function)
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
        length = polish1(code, FALSE) + sizeof(jumpblock) / sizeof(forthword) + 1; /* 1 for TheEnd */
        if(length < 1)
            {
            errorprintf("polish1 returns length < 0 [%d]\n", length);
            return 0; /* Something wrong happened. */
            }
        forthstuff = (forthMemory*)bmalloc(sizeof(forthMemory));
        if(forthstuff)
            {
            memset(forthstuff, 0, sizeof(forthMemory));
            if(parent)
                {
                forthstuff->parent = parent;
                forthMemory* funcs = parent->functions;
                parent->functions = forthstuff;
                forthstuff->nextFnc = funcs;
                }
            forthstuff->name = 0;
            if(is_op(arg) && !is_op(arg->LEFT))
                {
                name = &(arg->LEFT->u.sobj);
                if(*name)
                    {
                    assert(forthstuff->name == 0);
                    forthstuff->name = (char*)bmalloc(strlen(name) + 1);
                    if(forthstuff->name)
                        {
                        strcpy(forthstuff->name, name);
                        }
                    }
                }
            /*
            forthstuff->functions = 0;
            forthstuff->nextFnc = 0;
            forthstuff->var = 0;
            forthstuff->arr = 0;
            */
            assert(forthstuff->word == 0);
            forthstuff->word = bmalloc(length * sizeof(forthword));
            if(forthstuff->word)
                {
                memset(forthstuff->word, 0, length * sizeof(forthword));
                forthstuff->wordp = forthstuff->word;
                forthstuff->sp = forthstuff->stack;
                forthstuff->parameters = 0;
                if(declarations)
                    {
                    psk decl;
                    size_t Ndecl = 0;
                    for(decl = declarations; decl && is_op(decl); decl = decl->RIGHT)
                        {
                        ++Ndecl;
                        if(Op(decl) == DOT) /*Just one parameter that is an array*/
                            break;
                        }

                    forthstuff->nparameters = Ndecl;
                    if(Ndecl > 0)
                        {
                        forthstuff->parameters = bmalloc(Ndecl * sizeof(parameter));
                        if(forthstuff->parameters != 0)
                            {
                            for(decl = declarations; Ndecl-- > 0 && decl && is_op(decl); decl = decl->RIGHT)
                                {
                                if(Op(decl) == DOT)
                                    {
                                    if(!setparm(Ndecl, forthstuff, decl, in_function))
                                        {
                                        errorprintf("Error in parameter declaration.\n");
                                        return 0; /* Something wrong happened. */
                                        }
                                    break;
                                    }
                                else
                                    {
                                    if(is_op(decl->LEFT) && Op(decl->LEFT) == DOT)
                                        if(!setparm(Ndecl, forthstuff, decl->LEFT, in_function))
                                            {
                                            errorprintf("Error in parameter declaration.\n");
                                            return 0; /* Something wrong happened. */
                                            }
                                    }
                                }
                            }
                        }
                    }
                jumpblock* j5 = (jumpblock*)(forthstuff->word);
                j5->j[estart].offset = (unsigned int)((&(j5->j[0]) + sizeof(jumpblock) / sizeof(forthword)) - forthstuff->word);
                j5->j[estart].action = Branch;
                //j5->j[epopS].offset = 1;
                //j5->j[epopS].action = Pop;
                //j5->j[eS].offset = 0;
                //j5->j[eS].action = TheEnd;
                j5->j[epopF].offset = 1;
                j5->j[epopF].action = Pop;
                //j5->j[eF].offset = 0;
                //j5->j[eF].action = TheEnd;

                mustpop = enopop;

                lastword = polish2(forthstuff, j5, code, forthstuff->word + sizeof(jumpblock) / sizeof(forthword), FALSE);
                if(lastword != 0)
                    {
                    unsigned int theend = (unsigned int)(lastword - forthstuff->word);
                    if(theend + 1 == (unsigned int)length)
                        {
                        j5->j[epopS].offset = theend;
                        j5->j[epopS].action = Branch; /* Leave last value on the stack. (If there is one.) */
                        j5->j[eS].offset = theend;
                        j5->j[eS].action = Branch;
                        j5->j[eF].offset = theend;
                        j5->j[eF].action = Branch;

                        lastword->action = TheEnd;
                        if(newval)
                            wipe(fullcode);
                        char* marks = calloc(length, sizeof(char));
                        if(marks)
                            {
#ifdef SHOWOPTIMIZATIONS
                            int loop = 0;
#endif
                            for(;;)
                                {
#ifdef SHOWOPTIMIZATIONS
                                printf("\nOptimization loop %d\n", ++loop);
#endif
                                Boolean somethingdone = FALSE;
                                
                                somethingdone |= shortcutJumpChains(forthstuff->word);
                                somethingdone |= combinePopBranch(forthstuff->word);
                                somethingdone |= combineBranchPopBranch(forthstuff->word);
                                memset(marks, 0, length * sizeof(char));
                                somethingdone |= markUnReachable(forthstuff->word, marks);
                                
                                if(somethingdone)
                                    {
                                    length = removeNoOp(forthstuff, length);
                                    continue;
                                    }
                                somethingdone |= dissolveNextWordBranches(forthstuff->word);
                                somethingdone |= combineUnconditionalBranchTovalPush(forthstuff->word);
                                memset(marks, 0, length * sizeof(char));
                                somethingdone |= combineval2stack(forthstuff->word, marks); // FAULTY!
                                memset(marks, 0, length * sizeof(char));
                                somethingdone |= combinePopThenPop(forthstuff->word, marks);
                                somethingdone |= eliminateBranch(forthstuff->word);
                                somethingdone |= stack2var_var2stack(forthstuff->word);
                                somethingdone |= removeIdempotentActions(forthstuff->word);
                                somethingdone |= combinePushAndOperation(forthstuff->word);
                                
                                if(somethingdone)
                                    {
                                    length = removeNoOp(forthstuff, length);
                                    continue;
                                    }
                                break;
                                }
                            free(marks);
                            }
                        return forthstuff;
                        }
                    }
                }
            if(!in_function)
                {
                calcdie(forthstuff);
                }
            else
                {
                showProblematicNode("In function:", 0);
                showProblematicNode(forthstuff->name, 0);
                }
            }
        }
    wipe(fullcode);
    return 0; /* Something wrong happened. */
    }

static Boolean calculationnew(struct typedObjectnode* This, ppsk arg)
    {
    if(is_op(*arg))
        {
        forthMemory* forthstuff = calcnew((*arg)->RIGHT, 0, FALSE);
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
        if(mem->word)
            {
            wipepnodes(mem->word);
            bfree(mem->word);
            mem->word = 0;
            }
        forthvariable* curvarp = mem->var;
        fortharray* curarr = mem->arr;
        forthMemory* functions = mem->functions;
        while(curvarp)
            {
            forthvariable* nextvarp = curvarp->next;
            bfree(curvarp->name);
            curvarp->name = 0;
            bfree(curvarp);
            curvarp = nextvarp;
            }
        while(curarr)
            {
            fortharray* nextarrp = curarr->next;
            bfree(curarr->name);
            curarr->name = 0;
            if(curarr->extent)
                {
                bfree(curarr->extent);
                curarr->extent = 0;
                curarr->stride = 0;
                }
            if(curarr->pval)
                {
                bfree(curarr->pval);
                curarr->pval = 0;
                }
            bfree(curarr);
            curarr = nextarrp;
            }
        mem->arr = 0;
        if(mem->parameters)
            {
            bfree(mem->parameters);
            mem->parameters = 0;
            }
        for(; functions;)
            {
            forthMemory* nextfunc = functions->nextFnc;
            functiondie(functions);
            functions = nextfunc;
            }
        mem->functions = 0;
        if(mem->name)
            {
            bfree(mem->name);
            mem->name = 0;
            }
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
    {"run",calculate}, /*Alternative to `calculate' and `go'*/
    {"go",calculate}, /*Alternative to calculate and `run'*/
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

