#include "variables.h"
#include "branch.h"
#include "nodedefs.h"
#include "equal.h"
#include "nonnodetypes.h"
#include "globals.h"
#include "memory.h"
#include "copy.h"
#include "objectnode.h"
#include "typedobjectnode.h"
#include "opt.h"
#include "object.h"
#include "input.h"
#include "quote.h"
#include "filewrite.h"
#include "result.h"
#include "wipecopy.h"
#include "nodeutil.h"
#include "builtinmethod.h"
#include "head.h"
#include "numbercheck.h"
#include <assert.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

typedef struct varia
    {
    struct varia* prev; /* variableValue[-1] */
    psk variableValue[1];       /* variableValue[0], arraysize is adjusted by psh */
    } varia;

typedef struct vars /* sizeof(vars) = n * 4 bytes */
    {
#if PVNAME
    unsigned char* vname;
#define VARNAME(x) x->vname
#endif
    struct vars* next;
    int n;
    int selector;
    varia* pvaria; /* Can also contain entry[0]   (if n == 0) */
#if !PVNAME
    union
        {
        LONG Lobj;
        unsigned char Obj;
        } u;
#define VARNAME(x) &x->u.Obj
#endif
    } vars;

static vars* variables[256];

static int searchname(psk name,
                      vars** pprevvar,
                      vars** pnxtvar)
    {
    unsigned char* strng;
    vars* nxtvar, * prevvar;
    strng = POBJ(name);
    for(prevvar = NULL, nxtvar = variables[*strng]
        ;  nxtvar && (STRCMP(VARNAME(nxtvar), strng) < 0)
        ; prevvar = nxtvar, nxtvar = nxtvar->next
        )
        ;
    /* prevvar < strng <= nxtvar */
    *pprevvar = prevvar;
    *pnxtvar = nxtvar;
    return nxtvar && !STRCMP(VARNAME(nxtvar), strng);
    }

/*
name must be atom or <atom>.<atom>.<atom>...
*/
static int setmember(psk name, psk tree, psk newValue)
    {
    while(is_op(tree))
        {
        if(Op(tree) == EQUALS)
            {
            psk nname;
            if(Op(name) == DOT)
                nname = name->LEFT;
            else
                nname = name;
            if(equal(tree->LEFT, nname))
                {
                return FALSE;
                }
            else if(nname == name)
                {
                wipe(tree->RIGHT);
                tree->RIGHT = same_as_w(newValue);
                return TRUE;
                }
            else /* Found partial match for member name,
                    recurse in both name and member */
                {
                name = name->RIGHT;
                }
            }
        else if(setmember(name, tree->LEFT, newValue))
            {
            return TRUE;
            }
        tree = tree->RIGHT;
        }
    return FALSE;
    }

static int ilog2(int n)
/* returns MSB of n */
    {
    int m;
    for(m = 1
        ; n
        ; n >>= 1, m <<= 1
        )
        ;
    return m >> 1;
    }

/* Given the number of layers of a stack, an index and a pointer to the stack,
   return the entry pointed at by the index.
   Each level is twice the size of the level below it.
   The lowest level has size 2.
   The  role of the level with size 1 is played by the pointer to the stack.
   The caller can mutate the returned entry.*/
static ppsk Entry(int n, int index, varia** pv)
    {
    if(n == 0)
        {
        return (ppsk)pv;  /* no varia records are needed for 1 entry */
        }
    else
        {
#if defined POWER2
        varia* hv;
        int MSB = ilog2(n);
        for(hv = *pv /* begin with longest varia record */
            ; MSB > 1 && index < MSB
            ; MSB >>= 1
            )
            hv = hv->prev;
        index -= MSB;   /* if index == 0, then index becomes -1 */
#else
        /* This code does not make Bracmat noticeably faster*/
        int MSB;
        varia* hv = *pv;
        for(MSB = 1; MSB <= index; MSB <<= 1)
            ;

        if(MSB > 1)
            index %= (MSB >> 1);
        else
            {
            index = -1;
            MSB <<= 1;
            }

        for(; MSB <= n; MSB <<= 1)
            hv = hv->prev;
#endif
        return &hv->variableValue[index];  /* variableValue[-1] == (psk)*prev */
        }
    }

/* Given the number of layers of a stack, an index and a pointer to the stack,
   return the entry pointed at by the index.
   Each level is twice the size of the level below it.
   The lowest level has size 2.
   The  role of the level with size 1 is played by the pointer to the stack.
   The caller cannot mutate the returned entry.*/
static psk Entry2(int n, int index, varia* pv)
    {
    if(n == 0)
        {
        return (psk)pv;  /* no varia records needed for 1 entry */
        }
    else
        {
        varia* hv;
#if defined POWER2
        int MSB = ilog2(n);
        for(hv = pv /* begin with longest varia record */
            ; MSB > 1 && index < MSB
            ; MSB >>= 1
            )
            hv = hv->prev;
        index -= MSB;   /* if index == 0, then index becomes -1 */
#else
        /* This code does not make Bracmat noticeably faster*/
        int MSB;
        hv = pv;
        for(MSB = 1; MSB <= index; MSB <<= 1)
            ;

        if(MSB > 1)
            index %= (MSB >> 1);
        else
            {
            index = -1;
            MSB <<= 1;
            }

        for(; MSB <= n; MSB <<= 1)
            hv = hv->prev;
#endif
        return hv->variableValue[index];  /* variableValue[-1] == (psk)*prev */
        }
    }

int update(psk name, psk pnode) /* name = tree with DOT in root */
/*
    x:?(k.b)
    x:?((=(a=) (b=)).b)
*/
    {
    vars* nxtvar;
    vars* prevvar;
    if(is_op(name->LEFT))
        {
        if(Op(name->LEFT) == EQUALS)
            /*{?} x:?((=(a=) (b=)).b) => x */
            /*          ^              */
            return setmember(name->RIGHT, name->LEFT->RIGHT, pnode);
        else
            {
            /*{?} b:?((x.y).z) => x */
            return FALSE;
            }
        }
    if(Op(name) == EQUALS) /* {?} (=a+b)=5 ==> =5 */
        {
        wipe(name->RIGHT);
        name->RIGHT = same_as_w(pnode);
        return TRUE;
        }
    else if(searchname(name->LEFT,
                       &prevvar,
                       &nxtvar))
        {
        assert(nxtvar->pvaria);
        return setmember(name->RIGHT, Entry2(nxtvar->n, nxtvar->selector, nxtvar->pvaria), pnode);
        }
    else
        {
        return FALSE; /*{?} 66:?(someUnidentifiableObject.x) */
        }
    }


int insert(psk name, psk pnode)
    {
    vars* nxtvar, * prevvar, * newvar;

    if(is_op(name))
        {
        if(Op(name) == EQUALS)
            {
            wipe(name->RIGHT);
            name->RIGHT = same_as_w(pnode);  /*{?} monk2:?(=monk1) => monk2 */
            return TRUE;
            }
        else
            { /* This allows, in fact, other operators than DOT, e.g. b:?(x y) */
            return update(name, pnode); /*{?} (borfo=klot=)&bk:?(borfo klot)&!(borfo.klot):bk => bk */
            }
        }
    if(searchname(name,
                  &prevvar,
                  &nxtvar))
        {
        ppsk PPnode;
        wipe(*(PPnode = Entry(nxtvar->n, nxtvar->selector, &nxtvar->pvaria)));
        *PPnode = same_as_w(pnode);
        }
    else
        {
        size_t len;
        unsigned char* strng;
        strng = POBJ(name);
        len = strlen((char*)strng);
#if PVNAME
        newvar = (vars*)bmalloc(sizeof(vars));
        if(*strng)
            {
#if ICPY
            MEMCPY(newvar->vname = (unsigned char*)
                   bmalloc(len + 1), strng, (len >> LOGWORDLENGTH) + 1);
#else
            MEMCPY(newvar->vname = (unsigned char*)
                   bmalloc(len + 1), strng, ((len / sizeof(LONG)) + 1) * sizeof(LONG));
#endif
            }
#else
        if(len < 4)
            newvar = (vars*)bmalloc(sizeof(vars));
        else
            newvar = (vars*)bmalloc(sizeof(vars) - 3 + len);
        if(*strng)
            {
#if ICPY
            MEMCPY(&newvar->u.Obj, strng, (len / sizeof(LONG)) + 1);
#else
            MEMCPY(&newvar->u.Obj, strng, ((len / sizeof(LONG)) + 1) * sizeof(LONG));
#endif
            }
#endif
        else
            {
#if PVNAME
            newvar->vname = OBJ(nilNode);
#else
            newvar->u.Lobj = LOBJ(nilNode);
#endif
            }
        newvar->next = nxtvar;
        if(prevvar == NULL)
            variables[*strng] = newvar;
        else
            prevvar->next = newvar;
        newvar->n = 0;
        newvar->selector = 0;
        newvar->pvaria = (varia*)same_as_w(pnode);
        }
    return TRUE;
    }

int psh(psk name, psk pnode, psk dim)
    {
    /* string must fulfill requirements of icpy */
    vars* nxtvar, * prevvar;
    varia* nvaria;
    psk cnode;
    int oldn, n, m2, m22;
    while(is_op(name))
        {
        /* return psh(name->LEFT,pnode,dim) && psh(name->RIGHT,pnode,dim); */
        if(!psh(name->LEFT, pnode, dim))
            return FALSE;
        name = name->RIGHT;
        }
    if(dim && !INTEGER(dim))
        return FALSE;
    if(!searchname(name,
                   &prevvar,
                   &nxtvar))
        {
        insert(name, pnode);
        if(dim)
            {
            searchname(name,
                       &prevvar,
                       &nxtvar);
            }
        else
            {
            return TRUE;
            }
        }
    n = oldn = nxtvar->n;
    if(dim)
        {
        int newn;
        errno = 0;
        newn = (int)STRTOUL((char*)POBJ(dim), (char**)NULL, 10);
        if(errno == ERANGE)
            return FALSE;
        if(RAT_NEG(dim))
            newn = oldn - newn + 1;
        if(newn < 0)
            return FALSE;
        nxtvar->n = newn;
        if(oldn >= nxtvar->n)
            {
            assert(nxtvar->pvaria);
            for(; oldn >= nxtvar->n;)
                wipe(Entry2(n, oldn--, nxtvar->pvaria));
            }
        nxtvar->n--;
        if(nxtvar->selector > nxtvar->n)
            nxtvar->selector = nxtvar->n;
        }
    else
        {
        nxtvar->n++;
        nxtvar->selector = nxtvar->n;
        }
    m2 = ilog2(n);
    if(m2 == 0)
        m22 = 1;
    else
        m22 = m2 << 1;
    if(nxtvar->n >= m22)
        /* allocate */
        {
        for(; nxtvar->n >= m22; m22 <<= 1)
            {
            nvaria = (varia*)bmalloc(sizeof(varia) + (m22 - 1) * sizeof(psk));
            nvaria->prev = nxtvar->pvaria;
            nxtvar->pvaria = nvaria;
            }
        }
    else if(nxtvar->n < m2)
        /* deallocate */
        {
        for(; m2 && nxtvar->n < m2; m2 >>= 1)
            {
            nvaria = nxtvar->pvaria;
            nxtvar->pvaria = nvaria->prev;
            bfree(nvaria);
            }
        if(nxtvar->n < 0)
            {
            if(prevvar)
                prevvar->next = nxtvar->next;
            else
                variables[*POBJ(name)] = nxtvar->next;
#if PVNAME
            if(nxtvar->vname != OBJ(nilNode))
                bfree(nxtvar->vname);
#endif
            bfree(nxtvar);
            return TRUE;
            }
        }
    assert(nxtvar->pvaria);
    for(cnode = pnode
        ; ++oldn <= nxtvar->n
        ; cnode = *Entry(nxtvar->n, oldn, &nxtvar->pvaria) = same_as_w(cnode)
        )
        ;
    return TRUE;
    }

int deleteNode(psk name)
    {
    vars* nxtvar, * prevvar;
    varia* hv;
    if(searchname(name,
                  &prevvar,
                  &nxtvar))
        {
        psk tmp;
        assert(nxtvar->pvaria);
        tmp = Entry2(nxtvar->n, nxtvar->n, nxtvar->pvaria);
        wipe(tmp);
        if(nxtvar->n)
            {
            if((nxtvar->n) - 1 < ilog2(nxtvar->n))
                {
                hv = nxtvar->pvaria;
                nxtvar->pvaria = hv->prev;
                bfree(hv);
                }
            nxtvar->n--;
            if(nxtvar->n < nxtvar->selector)
                nxtvar->selector = nxtvar->n;
            }
        else
            {
            if(prevvar)
                prevvar->next = nxtvar->next;
            else
                variables[*POBJ(name)] = nxtvar->next;
#if PVNAME
            if(nxtvar->vname != OBJ(nilNode))
                bfree(nxtvar->vname);
#endif
            bfree(nxtvar);
            }
        return TRUE;
        }
    else
        return FALSE;
    }

void pop(psk pnode)
    {
    while(is_op(pnode))
        {
        pop(pnode->LEFT);
        pnode = pnode->RIGHT;
        }
    deleteNode(pnode);
    }

psk getValueByVariableName(psk namenode)
    {
    vars* nxtvar;
    assert(!is_op(namenode));
    for(nxtvar = variables[namenode->u.obj];
        nxtvar && (STRCMP(VARNAME(nxtvar), POBJ(namenode)) < 0);
        nxtvar = nxtvar->next)
        ;
    if(nxtvar && !STRCMP(VARNAME(nxtvar), POBJ(namenode))
       && nxtvar->selector <= nxtvar->n
       )
        {
        ppsk self;
        assert(nxtvar->pvaria);
        self = Entry(nxtvar->n, nxtvar->selector, &nxtvar->pvaria);
        *self = Head(*self);
        return *self;
        }
    else
        {
        return NULL;
        }
    }

psk getValue(psk namenode, int* newval)
    {
    if(is_op(namenode))
        {
        switch(Op(namenode))
            {
            case EQUALS:
                {
                /* namenode is /!(=b) when evaluating (a=b)&/!('$a):b */
                *newval = TRUE;
                namenode->RIGHT = Head(namenode->RIGHT);
                return same_as_w(namenode->RIGHT);
                }
            case DOT: /*
                      e.g.

                            x  =  (a=2) (b=3)

                            !(x.a)
                      and
                            !((=  (a=2) (b=3)).a)

                      must give same result.
                      */
                {
                psk tmp;
                if(is_op(namenode->LEFT))
                    {
                    if(Op(namenode->LEFT) == EQUALS) /* namenode->LEFT == (=  (a=2) (b=3))   */
                        {
                        tmp = namenode->LEFT->RIGHT; /* tmp == ((a=2) (b=3))   */
                        }
                    else
                        {
                        return NULL; /* !(+a.a) */
                        }
                    }
                else                                   /* x */
                    {
                    if((tmp = getValueByVariableName(namenode->LEFT)) == NULL)
                        {
                        return NULL; /* !(xua.gjh) if xua isn't defined */
                        }
                    /*
                    tmp == ((a=2) (b=3))
                    tmp == (=)
                    */
                    }
                /* The number of '=' to reach the method name in 'tmp' must be one greater
                   than the number of '.' that precedes the method name in 'namenode->RIGHT'

                   e.g (= (say=.out$!arg)) and (.say) match

                   For built-in methods (definitions of which are not visible) an invisible '=' has to be assumed.

                   The function getmember resolves this.
                */
                tmp = getmember2(namenode->RIGHT, tmp);

                if(tmp)
                    {
                    *newval = TRUE;
                    return same_as_w(tmp);
                    }
                else
                    {
                    return NULL; /* !(a.c) if a=(b=4) */
                    }
                }
            default:
                {
                *newval = FALSE;
                return NULL; /* !(a,b) because of comma instead of dot */
                }
            }
        }
    else
        {
        return getValueByVariableName(namenode);
        }
    }

psk findMethod(psk namenode, objectStuff* Object)
/* Only for finding 'function' definitions. (LHS of ' or $)*/
/*
'namenode' is the expression that has to lead to a binding.
Conceptually, expression (or its complement) and binding are separated by a '=' operator.
E.g.

  say            =         (.out$!arg)
  ---            -         -----------
  expression   '='-operator     binding

    say$HELLO
    and
    (= (.out$!arg))$HELLO

      must be equivalent. Therefore, both of 'say' and its complement '(= .out$!arg)' have the binding '(.out$!arg)'.

        'find' returns returns a binding or NULL.
        If the function returns NULL, 'theMethod' may still be bound.
        The parameter 'self' is the rhs of the root '=' of an object. It is used for non-built-ins
        The parameter 'object' is the root of an object, possibly having built-in methods.


Built in methods:
if
    new$hash:?x
then
    ((=).insert)$   (=) being the hash node with invisible built in method 'insert'
and
    (!x.insert)$
and
    (x..insert)$
must be equivalent
*/
    {
    psk tmp;
    assert(is_op(namenode));
    assert(Op(namenode) == DOT);
    /*
    e.g.

    new$hash:?y

    (y..insert)$
    (!y.insert)$
    */
    /* (=hash).New when evaluating new$hash:?y
        y..insert when evaluating (y..insert)$
    */
    if(is_op(namenode->LEFT))
        {
        if(Op(namenode->LEFT) == EQUALS)
            {
            if(ISBUILTIN((objectnode*)(namenode->LEFT)))
                {
                Object->theMethod = findBuiltInMethod((typedObjectnode*)(namenode->LEFT), namenode->RIGHT);
                /* findBuiltInMethod((=),(insert)) */
                Object->object = namenode->LEFT;  /* object == (=) */
                if(Object->theMethod)
                    {
                    namenode->LEFT = same_as_w(namenode->LEFT);
                    return namenode->LEFT; /* (=hash).New when evaluating (new$hash..insert)$(a.b) */
                    }
                }
            tmp = namenode->LEFT->RIGHT;
            /* e.g. tmp == hash
            Or  tmp ==
                  (name=(first=John),(last=Bull))
                , (age=20)
                , ( new
                  =
                    .   new$(its.name):(=?(its.name))
                      &   !arg
                        : ( ?(its.name.first)
                          . ?(its.name.last)
                          . ?(its.age)
                          )
                  )
            */
            }
        else
            {
            return NULL;  /* ((.(did=.!arg+2)).did)$3 */
            }
        }
    else                                   /* x */
        {
        if((tmp = getValueByVariableName(namenode->LEFT)) == NULL)
            {
            return NULL;   /* (y.did)$3  when y is not defined at all */
            }
        /*
        tmp == ((a=2) (b=3))
        tmp == (=)
        */
        }
    Object->self = tmp; /* self == ((a=2) (b=3))   */
    /* The number of '=' to reach the method name in 'tmp' must be one greater
        than the number of '.' that precedes the method name in 'namenode->RIGHT'

        e.g (= (say=.out$!arg)) and (.say) match

        For built-in methods (definitions of which are not visible) an invisible '=' has to be assumed.

        The function getmember resolves this.
    */
    tmp = getmember(namenode->RIGHT, tmp, Object);

    if(tmp)
        {
        return same_as_w(tmp);
        }
    else
        { /* You get here if a built-in method is called. */
        return NULL; /* (=hash)..insert when evaluating (new$hash..insert)$(a.b) */
        }
    }

int copy_insert(psk name, psk pnode, psk cutoff)
    {
    psk PNODE;
    int ret;
    assert((pnode->RIGHT == 0 && cutoff == 0) || pnode->RIGHT != cutoff);
    if((pnode->v.fl & INDIRECT)
       && (pnode->v.fl & READY)
       /*
       {?} !dagj a:?dagj a
       {!} !dagj
       The test (pnode->v.fl & READY) does not solve stackoverflow
       in the following examples:

       {?} (=!y):(=?y)
       {?} !y

       {?} (=!y):(=?x)
       {?} (=!x):(=?y)
       {?} !x
       */
       )
        {
        return FALSE;
        }
    else if(pnode->v.fl & IDENT)
        {
        PNODE = copyof(pnode);
        }
    else if(cutoff == NULL)
        {
        return insert(name, pnode);
        }
    else
        {
        assert(!is_object(pnode));
        if((shared(pnode) != ALL_REFCOUNT_BITS_SET) && !all_refcount_bits_set(cutoff))
            {/* cutoff: either node with headroom in the small refcounter
                or object */
            PNODE = new_operator_like(pnode);
            PNODE->v.fl = (pnode->v.fl & COPYFILTER/*~ALL_REFCOUNT_BITS_SET*/) | LATEBIND;
            pnode->v.fl += ONEREF;
#if WORD32
            if(shared(cutoff) == ALL_REFCOUNT_BITS_SET)
                {
                /*
                (T=
                1100:?I
                & tbl$(AA,!I)
                & (OBJ==(=a) (=b) (=c))
                &   !OBJ
                : (
                =   %
                %
                (?m:?n:?o:?p:?q:?r:?s) { increase refcount of (=c) to 7}
                )
                &   whl
                ' ( !I+-1:~<0:?I
                & !OBJ:(=(% %:?(!I$?AA)) ?)
                )
                & !I);
                {due to late binding, the refcount of (=c) has just come above 1023, and then
                late binding stops for the last 80 or so iterations. The left hand side of the
                late bound node is not an object, and therefore can only count to 1024.
                Thereafter copies must be made.}
                */
                INCREFCOUNT(cutoff);
                }
            else
#endif
                cutoff->v.fl += ONEREF;

            PNODE->LEFT = pnode;
            PNODE->RIGHT = cutoff;
            }
        else
            {
            copyToCutoff(&PNODE, pnode, cutoff); /*{?} a b c:(?z:?x:?y:?a:?b) c => a b c */
            /*{?} 0:?n&a b c:?abc&whl'(!n+1:?n:<2000&str$(v !n):?v&!abc:?!v c) =>   whl
            ' ( !n+1:?n:<2000
            & str$(v !n):?v
            & !abc:?!v c
            ) */
            }
        }

    ret = insert(name, PNODE);
    wipe(PNODE);
    return ret;
    }

void mmf(ppsk PPnode)
    {
    psk goal;
    ppsk pgoal;
    vars* nxtvar;
    int alphabet, ext;
    char dim[22];
    ext = search_opt(*PPnode, EXT);
    wipe(*PPnode);
    pgoal = PPnode;
    for(alphabet = 0; alphabet < 256/*0x80*/; alphabet++)
        {
        for(nxtvar = variables[alphabet];
            nxtvar;
            nxtvar = nxtvar->next)
            {
            goal = *pgoal = (psk)bmalloc(sizeof(knode));
            goal->v.fl = WHITE | SUCCESS;
            if(ext && nxtvar->n > 0)
                {
                goal = goal->LEFT = (psk)bmalloc(sizeof(knode));
                goal->v.fl = DOT | SUCCESS;
                sprintf(dim, "%d.%d", nxtvar->n, nxtvar->selector);
                goal->RIGHT = NULL;
                goal->RIGHT = build_up(goal->RIGHT, dim, NULL);
                }
            goal = goal->LEFT =
                (psk)bmalloc(sizeof(ULONG) + 1 + strlen((char*)VARNAME(nxtvar)));
            goal->v.fl = (READY | SUCCESS);
            strcpy((char*)(goal)+sizeof(ULONG), (char*)VARNAME(nxtvar));
            pgoal = &(*pgoal)->RIGHT;
            }
        }
    *pgoal = same_as_w(&nilNode);
    }

static Boolean lstsub(psk pnode)
    {
    vars* nxtvar;
    unsigned char* name;
    int alphabet, n;
    Boolean found = FALSE;
    beNice = FALSE;
    name = POBJ(pnode);
    for(alphabet = 0; alphabet < 256; alphabet++)
        {
        for(nxtvar = variables[alphabet];
            nxtvar;
            nxtvar = nxtvar->next)
            {
            if((pnode->u.obj == 0 && alphabet < 0x80) || !STRCMP(VARNAME(nxtvar), name))
                {
                found = TRUE;
                for(n = nxtvar->n; n >= 0; n--)
                    {
                    ppsk tmp;
                    if(listWithName)
                        {
                        if(global_fpo == stdout)
                            {
                            if(nxtvar->n > 0)
                                Printf("%c%d (", n == nxtvar->selector ? '>' : ' ', n);
                            else
                                Printf("(");
                            }
                        if(quote(VARNAME(nxtvar)))
                            myprintf("\"", (char*)VARNAME(nxtvar), "\"=", NULL);
                        else
                            myprintf((char*)VARNAME(nxtvar), "=", NULL);
                        if(hum)
                            myprintf("\n", NULL);
                        }
                    assert(nxtvar->pvaria);
                    tmp = Entry(nxtvar->n, n, &nxtvar->pvaria);
                    result(*tmp = Head(*tmp));
                    if(listWithName)
                        {
                        if(global_fpo == stdout)
                            Printf("\n)");
                        myprintf(";\n", NULL);
                        }
                    else
                        break; /*Only list variable on top of stack if RAW*/
                    }
                }
            }
        }
    beNice = TRUE;
    return found;
    }

Boolean GoodLST;

void lst(psk pnode)
    {
    GoodLST = TRUE;
    while(GoodLST && is_op(pnode))
        {
        if(Op(pnode) == EQUALS)
            {
            beNice = FALSE;
            myprintf("(", NULL);
            if(hum)
                myprintf("\n", NULL);
            if(listWithName)
                {
                result(pnode);
                }
            else
                {
                result(pnode->RIGHT);
                }
            if(hum)
                myprintf("\n", NULL);
            myprintf(")\n", NULL);
            beNice = TRUE;
            return;
            }
        else
            {
            lst(pnode->LEFT);
            pnode = pnode->RIGHT;
            }
        }
    if(GoodLST)
        GoodLST &= lstsub(pnode);
    }

function_return_type setIndex(psk Pnode)
    {
    psk lnode = Pnode->LEFT;
    psk rightnode = Pnode->RIGHT;
    if(INTEGER(lnode))
        {
        vars* nxtvar;
        if(is_op(rightnode))
            return functionFail(Pnode);
        for(nxtvar = variables[rightnode->u.obj];
            nxtvar && (STRCMP(VARNAME(nxtvar), POBJ(rightnode)) < 0);
            nxtvar = nxtvar->next);
        /* find first name in a row of equal names */
        if(nxtvar && !STRCMP(VARNAME(nxtvar), POBJ(rightnode)))
            {
            nxtvar->selector =
                (int)toLong(lnode)
                % (nxtvar->n + 1);
            if(nxtvar->selector < 0)
                nxtvar->selector += (nxtvar->n + 1);
            Pnode = rightbranch(Pnode);
            return functionOk(Pnode);
            }
        else
            return functionFail(Pnode);
        }
    return 0;
    /*
    if(!(rightnode->v.fl & SUCCESS))
        return functionFail(Pnode);
    addr[1] = NULL;
    return execFnc(Pnode);
    */
    }

void initVariables(void)
    {
    int tel;
    for(tel = 0; tel < 256; variables[tel++] = NULL)
        ;
    }

static int scopy_insert(psk name, const char* str)
    {
    int ret;
    psk pnode;
    pnode = scopy(str);
    ret = insert(name, pnode);
    wipe(pnode);
    return ret;
    }

int icopy_insert(psk name, LONG number)
    {
    char buf[22];
    sprintf((char*)buf, LONGD, number);
    return scopy_insert(name, buf);
    }

int string_copy_insert(psk name, psk pnode, char* str, char* cutoff)
    {
    char sav = *cutoff;
    int ret;
    *cutoff = '\0';
    if((pnode->v.fl & IDENT) || all_refcount_bits_set(pnode))
        {
        ret = scopy_insert(name, str);
        }
    else
        {
        stringrefnode* psnode;
        int nr;
        nr = fullnumbercheck(str) & ~DEFINITELYNONUMBER;
        if((nr & MINUS) && !(name->v.fl & NUMBER))
            nr = 0; /* "-1" is only converted to -1 if the # flag is present on the pattern */
        psnode = (stringrefnode*)bmalloc(sizeof(stringrefnode));
        psnode->v.fl = /*(pnode->v.fl & ~(ALL_REFCOUNT_BITS_SET|VISIBLE_FLAGS)) substring doesn't inherit flags like */
            READY | SUCCESS | LATEBIND | nr;
        /*psnode->v.fl |= SUCCESS;*/ /*{?} @(~`ab:%?x %?y)&!x => a */ /*{!} a */
        psnode->pnode = same_as_w(pnode);
        if(nr & MINUS)
            {
            psnode->str = str + 1;
            psnode->length = cutoff - str - 1;
            }
        else
            {
            psnode->str = str;
            psnode->length = cutoff - str;
            }
        DBGSRC(Boolean saveNice; int redhum; saveNice = beNice; \
               redhum = hum; beNice = FALSE; hum = FALSE; \
               Printf("str [%s] length " LONGU "\n", psnode->str, (ULONG int)psnode->length); \
               beNice = saveNice; hum = redhum;)
            ret = insert(name, (psk)psnode);
        if(ret)
            dec_refcount((psk)psnode);
        else
            {
            wipe(pnode);
            bfree((void*)psnode);
            }
        }
    *cutoff = sav;
    return ret;
    }
