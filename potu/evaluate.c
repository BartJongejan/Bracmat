#include "evaluate.h"
#include "nodedefs.h"
#include "nodeutil.h"
#include "stringmatch.h"
#include "filewrite.h"
#include "writeerr.h"
#include "treematch.h"
#include "canonization.h"
#include "objectnode.h"
#include "copy.h"
#include "branch.h"
#include "variables.h"
#include "functions.h"
#include "globals.h"
#include "memory.h"
#include "wipecopy.h"
#include "platformdependentdefs.h"
#include "result.h"
#include <string.h>

#if MAXSTACK
static int maxstack = 0;
static int theStack = 0;
#define ASTACK {++theStack;if(theStack > maxstack) maxstack = theStack;}{
#define ZSTACK }{--theStack;}
#else
#define ASTACK
#define ZSTACK
#endif
psk eval(psk Pnode)
    {
    ASTACK
        /*
        Notice that there are only few local variables on the stack. This ensures
        maximal utilisation of stack-depth for recursion.
        */
        DBGSRC(Printf("evaluate :"); result(Pnode); Printf("\n");)
        while(!(Pnode->v.fl & READY))
            {
            if(is_op(Pnode))
                {
                sk lkn = *Pnode;
                psk auxkn;
                /* The operators MATCH, AND and OR are treated in another way than
                the other operators. These three operators are the only 'volatile'
                operators: they cannot occur in a fully evaluated tree. For that reason
                there is no need to allocate space for an evaluated version of such
                operators on the stack. Instead the local variable lkn is used.
                */
                switch(Op(Pnode))
                    {
                    case MATCH:
                        {
                        privatized(Pnode, &lkn);
                        lkn.LEFT = eval(lkn.LEFT);
                        if(isSUCCESSorFENCE(lkn.LEFT))
                            /*
                            `~a:?b will assign `~a to b
                            */
                            {
#if STRINGMATCH_CAN_BE_NEGATED
                            if(lkn.v.fl & ATOM) /* should other flags be
                                                excluded, including ~ ?*/
#else
                            if((lkn.v.fl & ATOM) && !NEGATION(lkn.v.fl, ATOM))
#endif
                                {
#if CUTOFFSUGGEST
                                if(!is_op(lkn.LEFT) && stringmatch(0, "V", SPOBJ(lkn.LEFT), NULL, lkn.RIGHT, lkn.LEFT, 0, strlen((char*)POBJ(lkn.LEFT)), NULL, 0) & TRUE)
#else
                                if(!is_op(lkn.LEFT) && stringmatch(0, "V", POBJ(lkn.LEFT), NULL, lkn.RIGHT, lkn.LEFT, 0, strlen((char*)POBJ(lkn.LEFT))) & TRUE)
#endif
                                    Pnode = _leftbranch(&lkn); /* ~@(a:a) is now treated like ~(a:a)*/
                                else
                                    {
                                    if(is_op(lkn.LEFT))
                                        {
#if !defined NO_EXIT_ON_NON_SEVERE_ERRORS
                                        errorprintf("Error in stringmatch: left operand is not atomic "); writeError(&lkn);
                                        exit(117);
#endif
                                        }
                                    Pnode = _fleftbranch(&lkn);/* ~@(a:b) is now treated like ~(a:b)*/
                                    }
                                }
                            else
                                {
                                if(match(0, lkn.LEFT, lkn.RIGHT, NULL, 0, lkn.LEFT, 5555) & TRUE)
                                    Pnode = _leftbranch(&lkn);
                                else
                                    Pnode = _fleftbranch(&lkn);
                                }
                            }
                        else
                            {
                            Pnode = _leftbranch(&lkn);
                            }
                        DBGSRC(Printf("after match:"); result(Pnode); Printf("\n"); \
                               if(Pnode->v.fl & SUCCESS) Printf(" SUCCESS\n"); \
                               else Printf(" FENCE\n");)
                            break;
                        }
                        /* The operators AND and OR are tail-recursion optimised. */
                    case AND:
                        {
                        privatized(Pnode, &lkn);
                        lkn.LEFT = eval(lkn.LEFT);
                        if(isSUCCESSorFENCE(lkn.LEFT))
                            {
                            Pnode = _rightbranch(&lkn);/* TRUE or FENCE */
                            if(lkn.v.fl & INDIRECT)
                                {
                                lkn.RIGHT = eval(Pnode);
                                if(isSUCCESS(lkn.RIGHT))
                                    {
                                    Pnode = evalvar(lkn.RIGHT);
                                    }
                                else
                                    Pnode->v.fl ^= SUCCESS;
                                }
                            }
                        else
                            Pnode = _leftbranch(&lkn);/* FAIL */
                        break;
                        }
                    case OR:
                        {
                        privatized(Pnode, &lkn);
                        lkn.LEFT = eval(lkn.LEFT);
                        if(isSUCCESSorFENCE(lkn.LEFT))
                            {
                            Pnode = _fenceleftbranch(&lkn);/* FENCE or TRUE */
                            if((lkn.v.fl & INDIRECT) && isSUCCESS(Pnode))
                                {
                                lkn.RIGHT = eval(Pnode);
                                if(isSUCCESS(lkn.RIGHT))
                                    Pnode = evalvar(lkn.RIGHT);
                                else
                                    Pnode->v.fl ^= SUCCESS;
                                }
                            }
                        else
                            {
                            Pnode = _rightbranch(&lkn);/* FAIL */
                            if((lkn.v.fl & INDIRECT) && isSUCCESS(Pnode))
                                {
                                lkn.RIGHT = eval(Pnode);
                                if(isSUCCESS(lkn.RIGHT))
                                    Pnode = evalvar(lkn.RIGHT);
                                else
                                    Pnode->v.fl ^= SUCCESS;
                                }
                            }
                        break;
                        }
                        /* Operators that can occur in evaluated expressions: */
                    case EQUALS:
                        if(ISBUILTIN((objectnode*)Pnode))
                            {
                            Pnode->v.fl |= READY;
                            break;
                            }

                        if(!is_op(Pnode->LEFT)
                           && !Pnode->LEFT->u.obj
                           && (Pnode->v.fl & INDIRECT)
                           && !(Pnode->v.fl & DOUBLY_INDIRECT)
                           )
                            {
                            static int fl;
                            fl = Pnode->v.fl & (UNOPS & ~INDIRECT);
                            Pnode = __rightbranch(Pnode);
                            if(fl)
                                {
                                Pnode = isolated(Pnode);
                                Pnode->v.fl |= fl; /* {?} <>#@`/%?!(=b) => /#<>%@?`b */
                                }
                            break;

                            /*                    Pnode = Pnode->RIGHT;*/
                            }
                        else
                            {
                            if(shared(Pnode))
                                {
                                Pnode = copyop(Pnode);
                                }
                            Pnode->v.fl |= READY;
                            Pnode->LEFT = eval(Pnode->LEFT);
                            if(is_op(Pnode->LEFT))
                                {
                                if(update(Pnode->LEFT, Pnode->RIGHT))
                                    Pnode = leftbranch(Pnode);
                                else
                                    Pnode = fleftbranch(Pnode);
                                }
                            else if(Pnode->LEFT->u.obj)
                                {
                                insert(Pnode->LEFT, Pnode->RIGHT);
                                Pnode = leftbranch(Pnode);
                                }
                            else if(Pnode->v.fl & INDIRECT)
                                /* !(=a) -> a */
                                {
                                Pnode = evalvar(Pnode);
                                }
                            }
                        break;
                    case DOT:
                        {
                        if(shared(Pnode))
                            {
                            Pnode = copyop(Pnode);
                            }
                        Pnode->v.fl |= READY;
                        Pnode->LEFT = eval(Pnode->LEFT);
                        Pnode->RIGHT = eval(Pnode->RIGHT);
                        if(Pnode->v.fl & INDIRECT)
                            {
                            Pnode = evalvar(Pnode);
                            }
                        break;
                        }
                    case COMMA:
                        if(shared(Pnode))
                            {
                            Pnode = copyop(Pnode);
                            }
                        Pnode->v.fl |= READY;
                        Pnode = handleComma(Pnode);/* do not recurse, iterate! */
                        if(lkn.v.fl & INDIRECT)
                            {
                            Pnode = evalvar(Pnode);
                            }
                        break;
                    case WHITE:
                        if(shared(Pnode))
                            {
                            Pnode = copyop(Pnode);
                            }
                        Pnode->v.fl |= READY;
                        Pnode = handleWhitespace(Pnode);/* do not recurse, iterate! */
                        if(lkn.v.fl & INDIRECT)
                            {
                            Pnode = evalvar(Pnode);
                            }
                        break;
                    case PLUS:
                        if(shared(Pnode))
                            {
                            Pnode = copyop(Pnode);
                            }
                        Pnode = merge(Pnode, cmpplus, mergeOrSortTerms
#if EXPAND
                                      , expandProduct
#endif
                        );
                        if(lkn.v.fl & INDIRECT)
                            {
                            Pnode = evalvar(Pnode);
                            }
                        break;
                    case TIMES:
                        if(shared(Pnode))
                            {
                            Pnode = copyop(Pnode);
                            }
                        Pnode->v.fl |= READY;
                        {
                        Pnode = merge(Pnode, cmptimes, substtimes
#if EXPAND
                                      , expandDummy
#endif
                        );
                        }
                        if(lkn.v.fl & INDIRECT)
                            {
                            Pnode = evalvar(Pnode);                             /* {?} a=7 & !(a*1) */
                            }
                        break;
                    case EXP:
                        if(shared(Pnode))
                            {
                            Pnode = copyop(Pnode);
                            }
                        Pnode->v.fl |= READY;
                        if(evaluate((Pnode->LEFT)) == TRUE
                           && evaluate((Pnode->RIGHT)) == TRUE
                           )
                            {
                            Pnode = handleExponents(Pnode);
                            }
                        else
                            Pnode->v.fl ^= SUCCESS;
                        if(lkn.v.fl & INDIRECT)
                            {
                            Pnode = evalvar(Pnode);
                            }
                        break;
                    case LOG:
                        if(shared(Pnode))
                            {
                            Pnode = copyop(Pnode);
                            }
                        Pnode->v.fl |= READY;
                        if(evaluate((Pnode->LEFT)) == TRUE
                           && evaluate((Pnode->RIGHT)) == TRUE
                           )
                            {
                            Pnode = substlog(Pnode);
                            }
                        else
                            {
                            Pnode->v.fl ^= SUCCESS;
                            }
                        if(lkn.v.fl & INDIRECT)
                            {
                            Pnode = evalvar(Pnode);
                            }
                        break;
                    case DIF:
                        if(shared(Pnode))
                            {
                            Pnode = copyop(Pnode);
                            }
                        Pnode->v.fl |= READY;
                        if(!(lkn.v.fl & INDIRECT)
                           && evaluate((Pnode->LEFT)) == TRUE
                           && evaluate((Pnode->RIGHT)) == TRUE
                           )
                            {
                            Pnode = substdiff(Pnode);
                            break;
                            }
                        Pnode->v.fl ^= SUCCESS;
                        break;
                    case FUN:
                    case FUU:
                        if(shared(Pnode))
                            {
                            Pnode = copyop(Pnode);
                            }
                        Pnode->v.fl |= READY;
                        Pnode->LEFT = eval(Pnode->LEFT);
                        if(Op(Pnode) == FUN)
                            {
                            Pnode->RIGHT = eval(Pnode->RIGHT);
                            }

                        auxkn = setIndex(Pnode);
                        if(auxkn)
                            Pnode = auxkn;
                        else
                            {
                            if(not_built_in(Pnode->LEFT)) /* Do not use ternary operator! That eats stack! */
                                Pnode = execFnc(Pnode);
                            else
                                {
                                auxkn = functions(Pnode);
                                if(auxkn)
                                    Pnode = auxkn;
                                else
                                    Pnode = execFnc(Pnode);
                                }
                            }

                        if(lkn.v.fl & INDIRECT)
                            {
                            Pnode = evalvar(Pnode);
                            }
                        break;
                    case UNDERSCORE:
                        if(shared(Pnode))
                            {
                            Pnode = copyop(Pnode);
                            }
                        Pnode->v.fl |= READY;
                        if(dummy_op == EQUALS)
                            {
                            auxkn = Pnode;
                            Pnode = (psk)bmalloc(__LINE__, sizeof(objectnode));
#if WORD32
                            ((typedObjectnode*)(Pnode))->u.Int = 0;
#else
                            ((typedObjectnode*)(Pnode))->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
#endif
                            Pnode->LEFT = subtreecopy(auxkn->LEFT);
                            auxkn->RIGHT = Head(auxkn->RIGHT);
                            Pnode->RIGHT = subtreecopy(auxkn->RIGHT);
                            wipe(auxkn);
                            }
                        Pnode->v.fl &= (~OPERATOR & ~READY);
                        Pnode->v.fl |= dummy_op;
                        Pnode->v.fl |= SUCCESS;
                        if(dummy_op == UNDERSCORE)
                            Pnode->v.fl |= READY; /* stop iterating */
                        if(lkn.v.fl & INDIRECT)
                            {/* (a=b=127)&(.):(_)&!(a_b) */
                            Pnode = evalvar(Pnode);
                            }
                        break;
                    }
                }
            else
                {
                /* An unevaluated leaf can only be an atom with ! or !!,
                so we don't need to test for this condition.*/
                Pnode = evalvar(Pnode);
                /* After evaluation of a variable, the loop continues.
                Together with how & and | (AND and OR) are treated, this ensures that
                a loop can run indefinitely, without using stack space. */
                }
            }
#if JMP
    PeekMsg();
#endif
    ZSTACK
        return Pnode;
    }
