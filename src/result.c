#include "result.h"
#include "nonnodetypes.h"
#include "nodedefs.h"
#include "globals.h"
#include "filewrite.h"
#include "quote.h"
#include "head.h"
#include <string.h>

static const char opchar[16] =
    { '=','.',',','|','&',':',' ','+','*','^','\016','\017','\'','$','_','?' };

static void parenthesised_result(psk Root, int level, int ind, int space);
#if DEBUGBRACMAT
static void hreslts(psk Root, int level, int ind, int space, psk cutoff);
#endif


#define COMPLEX_MAX 80
int LineLength = NARROWLINELENGTH;

static size_t complexity(psk Root, size_t max)
    {
    static int Parent, Child;
    while(is_op(Root))
        {
        max += 2; /* Each time reslt is called, level is incremented by 1.
                     indent() calls complexity with twice that increment.
                     So to predict what complexity says at each level, we have
                     to add 2 in each iteration while descending the tree. */
        switch(Op(Root))
            {
            case OR:
            case AND:
                max += COMPLEX_MAX / 5;
                break;
            case EQUALS:
            case MATCH:
                max += COMPLEX_MAX / 10;
                break;
            case DOT:
            case COMMA:
            case WHITE:
                switch(Op(Root->LEFT))
                    {
                    case DOT:
                    case COMMA:
                    case WHITE:
                        max += COMPLEX_MAX / 10;
                        break;
                    default:
                        max += COMPLEX_MAX / LineLength;
                    }
                break;
            default:
                max += COMPLEX_MAX / LineLength;
            }
        Parent = Op(Root);
        Child = is_op(Root->LEFT) ? Op(Root->LEFT) : 0;
        if(HAS__UNOPS(Root->LEFT) || Parent >= Child)
            max += (2 * COMPLEX_MAX) / LineLength; /* 2 parentheses */

        Child = is_op(Root->RIGHT) ? Op(Root->RIGHT) : 0;
        if(HAS__UNOPS(Root->RIGHT) || Parent > Child || (Parent == Child && Parent > TIMES))
            max += (2 * COMPLEX_MAX) / LineLength; /* 2 parentheses */

        if(max > COMPLEX_MAX)
            return max;
        max = complexity(Root->LEFT, max);
        Root = Root->RIGHT;
        }
    if(!is_op(Root))
        max += (COMPLEX_MAX * strlen((char*)POBJ(Root))) / LineLength;
    return max;
    }

static int indtel = 0, extraSpc = 0, number_of_flags_on_node = 0;

static int indent(psk Root, int level, int ind)
    {
    if(hum)
        {
        if(ind > 0 || (ind == 0 && complexity(Root, 2 * level) > COMPLEX_MAX))
            {  /*    blanks that start a line    */
            int p;
            (*process)('\n');
            for(p = 2 * level + number_of_flags_on_node; p; p--)
                (*process)(' ');
            ind = TRUE;
            }
        else
            {  /* blanks after an operator or parenthesis */
            for(indtel = extraSpc + 2 * indtel; indtel; indtel--)
                (*process)(' ');
            ind = FALSE;
            }
        extraSpc = 0;
        }
    return ind;
    }

static int needIndent(psk Root, int ind, int level)
    {
    return hum && !ind && complexity(Root, 2 * level) > COMPLEX_MAX;
    }

static void do_something(int c)
    {
    if(c == 016 || c == 017)
        {
        (*process)('\\');
        (*process)(c == 016 ? 'L' : 'D');
        }
    else
        (*process)(c);
    }


static int printflags(psk Root)
    {
    int count = 0;
    ULONG Flgs = Root->v.fl;
    if(Flgs & FENCE)
        {
        (*process)('`');
        ++count;
        }
    if(Flgs & POSITION)
        {
        (*process)('[');
        ++count;
        }
    if(Flgs & NOT)
        {
        (*process)('~');
        ++count;
        }
    if(Flgs & FRACTION)
        {
        (*process)('/');
        ++count;
        }
    if(Flgs & NUMBER)
        {
        (*process)('#');
        ++count;
        }
    if(Flgs & SMALLER_THAN)
        {
        (*process)('<');
        ++count;
        }
    if(Flgs & GREATER_THAN)
        {
        (*process)('>');
        ++count;
        }
    if(Flgs & NONIDENT)
        {
        (*process)('%');
        ++count;
        }
    if(Flgs & ATOM)
        {
        (*process)('@');
        ++count;
        }
    if(Flgs & UNIFY)
        {
        (*process)('?');
        ++count;
        }
    if(Flgs & INDIRECT)
        {
        (*process)('!');
        ++count;
        }
    if(Flgs & DOUBLY_INDIRECT)
        {
        (*process)('!');
        ++count;
        }
    return count;
    }

#define LHS 1
#define RHS 2

#if SHOWWHETHERNEVERVISISTED
Boolean vis = FALSE;
#define SM(Root) if(vis && !(Root->v.fl & VISITED)) { (*process)('{'); (*process)('}'); }
#else
#define SM(Root)
#endif

static void endnode(psk Root, int space)
    {
    unsigned char* pstring;
    int q, ikar;
#if CHECKALLOCBOUNDS
    if(POINT)
        printf("\n[%p %lld]", Root, (Root->v.fl & ALL_REFCOUNT_BITS_SET) / ONEREF);
#endif
    if(!Root->u.obj
       && !HAS_UNOPS(Root)
       && space)
        {
        (*process)('(');
        (*process)(')');
        return;
        }
    printflags(Root);
    if(Root->v.fl & MINUS)
        (*process)('-');
    if(beNice)
        {
        for(pstring = POBJ(Root); *pstring; pstring++)
            do_something(*pstring);
        }
    else
        {
        Boolean longline = FALSE;
        if((q = quote(POBJ(Root))) == TRUE)
            (*process)('"');
        for(pstring = POBJ(Root); (ikar = *pstring) != 0; pstring++)
            {
            switch(ikar)
                {
                case '\n':
                    if(longline || lineTooLong(POBJ(Root)))
                        /* We need to call this, even though quote returned TRUE,
                        because quote may have returned before reaching this character.
                        */
                        {
                        longline = TRUE;
                        (*process)('\n');
                        continue;
                        }
                    ikar = 'n';
                    break;
                case '\f':
                    ikar = 'f';
                    break;
                case '\r':
                    ikar = 'r';
                    break;
                case '\b':
                    ikar = 'b';
                    break;
                case ALERT:
                    ikar = 'a';
                    break;
                case '\v':
                    ikar = 'v';
                    break;
                case '\t':
                    if(longline || lineTooLong(POBJ(Root)))
                        /* We need to call this, even though quote returned TRUE,
                        because quote may have returned before reaching this character.
                        */
                        {
                        longline = TRUE;
                        (*process)('\t');
                        continue;
                        }
                    ikar = 't';
                    break;
                case '"':
                case '\\':
                    break;
                case 016:
                    ikar = 'L';
                    break;
                case 017:
                    ikar = 'D';
                    break;
                default:
                    (*process)(ikar);
                    continue;
                }
            (*process)('\\');
            (*process)(ikar);
            }
        if(q)
            (*process)('"');
        }
    }



#define RSP (Parent == WHITE ? RHS : 0)
#define LSP (Parent == WHITE ? LHS : 0)

#ifndef reslt
static void reslt(psk Root, int level, int ind, int space)
    {
    static int Parent, Child, newind;
    while(is_op(Root))
        {
        if(Op(Root) == EQUALS)
            Root->RIGHT = Head(Root->RIGHT);
        Parent = Op(Root);
        Child = is_op(Root->LEFT) ? Op(Root->LEFT) : 0;
        if(needIndent(Root, ind, level))
            indtel++;
        if(HAS__UNOPS(Root->LEFT) || Parent >= Child)
            parenthesised_result(Root->LEFT, level + 1, FALSE, (space & LHS) | RSP);
        else
            reslt(Root->LEFT, level + 1, FALSE, (space & LHS) | RSP);
        newind = indent(Root, level, ind);
        if(newind)
            extraSpc = 1;
#if CHECKALLOCBOUNDS
        if(POINT)
            printf("\n[%p %lld]", Root, (Root->v.fl & ALL_REFCOUNT_BITS_SET) / ONEREF);
#endif
        do_something(opchar[klopcode(Root)]);

        SM(Root)
            Parent = Op(Root);
        Child = is_op(Root->RIGHT) ? Op(Root->RIGHT) : 0;
        if(HAS__UNOPS(Root->RIGHT) || Parent > Child || (Parent == Child && Parent > TIMES))
            {
            parenthesised_result(Root->RIGHT, level + 1, FALSE, LSP | (space & RHS));
            return;
            }
        else if(Parent < Child)
            {
            reslt(Root->RIGHT, level + 1, FALSE, LSP | (space & RHS));
            return;
            }
        else if(newind != ind || ((LSP | (space & RHS)) != space))
            {
            reslt(Root->RIGHT, level, newind, LSP | (space & RHS));
            return;
            }
        Root = Root->RIGHT;
        }
    indent(Root, level, -1);
    endnode(Root, space);
    }

#if DEBUGBRACMAT

static void reslts(psk Root, int level, int ind, int space, psk cutoff)
    {
    static int Parent, Child, newind;
    if(is_op(Root))
        {
        if(Op(Root) == EQUALS)
            Root->RIGHT = Head(Root->RIGHT);

        do
            {
            if(cutoff && Root->RIGHT == cutoff)
                {
                reslt(Root->LEFT, level, ind, space);
                return;
                }
            Parent = Op(Root);
            Child = is_op(Root->LEFT) ? Op(Root->LEFT) : 0;
            if(needIndent(Root, ind, level))
                indtel++;
            if(HAS__UNOPS(Root->LEFT) || Parent >= Child)
                parenthesised_result(Root->LEFT, level + 1, FALSE, (space & LHS) | RSP);
            else
                reslt(Root->LEFT, level + 1, FALSE, (space & LHS) | RSP);
            newind = indent(Root, level, ind);
            if(newind)
                extraSpc = 1;
            SM(Root)
                do_something(opchar[klopcode(Root)]);
            Parent = Op(Root);
            Child = is_op(Root->RIGHT) ? Op(Root->RIGHT) : 0;
            if(HAS__UNOPS(Root->RIGHT) || Parent > Child || (Parent == Child && Parent > TIMES))
                hreslts(Root->RIGHT, level + 1, FALSE, LSP | (space & RHS), cutoff);
            else if(Parent < Child)
                {
                reslts(Root->RIGHT, level + 1, FALSE, LSP | (space & RHS), cutoff);
                return;
                }
            else if(newind != ind || ((LSP | (space & RHS)) != space))
                {
                reslts(Root->RIGHT, level, newind, LSP | (space & RHS), cutoff);
                return;
                }
            Root = Root->RIGHT;
            }
        while(is_op(Root));
        }
    else
        {
        indent(Root, level, -1);
        endnode(Root, space);
        }
    }
#endif /* DEBUGBRACMAT */
#endif

static void parenthesised_result(psk Root, int level, int ind, int space)
    {
    static int Parent, Child;
    if(is_op(Root))
        {
        int number_of_flags;
        if(Op(Root) == EQUALS)
            Root->RIGHT = Head(Root->RIGHT);
        indent(Root, level, -1);
        number_of_flags = printflags(Root);
        number_of_flags_on_node += number_of_flags;
        (*process)('(');
        indtel = 0;
        if(needIndent(Root, ind, level))
            extraSpc = 1;
        Parent = Op(Root);
        Child = is_op(Root->LEFT) ? Op(Root->LEFT) : 0;
        if(HAS__UNOPS(Root->LEFT) || Parent >= Child)
            parenthesised_result(Root->LEFT, level + 1, FALSE, RSP);
        else
            reslt(Root->LEFT, level + 1, FALSE, RSP);
        ind = indent(Root, level, ind);
        if(ind)
            extraSpc = 1;
#if CHECKALLOCBOUNDS
        if(POINT)
            printf("\n[%p %lld]", Root, (Root->v.fl & ALL_REFCOUNT_BITS_SET) / ONEREF);
#endif
        do_something(opchar[klopcode(Root)]);
        SM(Root)
            Parent = Op(Root);

        Child = is_op(Root->RIGHT) ? Op(Root->RIGHT) : 0;
        if(HAS__UNOPS(Root->RIGHT) || Parent > Child || (Parent == Child && Parent > TIMES))
            parenthesised_result(Root->RIGHT, level + 1, FALSE, LSP);
        else if(Parent < Child)
            reslt(Root->RIGHT, level + 1, FALSE, LSP);
        else
            reslt(Root->RIGHT, level, ind, LSP);
        indent(Root, level, FALSE);
        (*process)(')');
        number_of_flags_on_node -= number_of_flags;
        }
    else
        {
        indent(Root, level, -1);
        endnode(Root, space);
        }
    }

#if DEBUGBRACMAT
static void hreslts(psk Root, int level, int ind, int space, psk cutoff)
    {
    static int Parent, Child;
    if(is_op(Root))
        {
        int number_of_flags;
        if(Op(Root) == EQUALS)
            Root->RIGHT = Head(Root->RIGHT);
        if(cutoff && Root->RIGHT == cutoff)
            {
            parenthesised_result(Root->LEFT, level, ind, space);
            return;
            }
        indent(Root, level, -1);
        number_of_flags = printflags(Root);
        number_of_flags_on_node += number_of_flags;
        (*process)('(');
        indtel = 0;
        if(needIndent(Root, ind, level))
            extraSpc = 1;
        Parent = Op(Root);
        Child = is_op(Root->LEFT) ? Op(Root->LEFT) : 0;
        if(HAS__UNOPS(Root->LEFT) || Parent >= Child)
            parenthesised_result(Root->LEFT, level + 1, FALSE, RSP);
        else
            reslt(Root->LEFT, level + 1, FALSE, RSP);
        ind = indent(Root, level, ind);
        if(ind)
            extraSpc = 1;
        SM(Root)
            do_something(opchar[klopcode(Root)]);
        Parent = Op(Root);
        Child = is_op(Root->RIGHT) ? Op(Root->RIGHT) : 0;
        if(HAS__UNOPS(Root->RIGHT) || Parent > Child || (Parent == Child && Parent > TIMES))
            hreslts(Root->RIGHT, level + 1, FALSE, LSP, cutoff);
        else if(Parent < Child)
            reslts(Root->RIGHT, level + 1, FALSE, LSP, cutoff);
        else
            reslts(Root->RIGHT, level, ind, LSP, cutoff);
        indent(Root, level, FALSE);
        (*process)(')');
        number_of_flags_on_node -= number_of_flags;
        }
    else
        {
        indent(Root, level, -1);
        endnode(Root, space);
        }
    }

void results(psk Root, psk cutoff)
    {
    if(HAS__UNOPS(Root))
        {
        hreslts(Root, 0, FALSE, 0, cutoff);
        }
    else
        reslts(Root, 0, FALSE, 0, cutoff);
    }
#endif


void result(psk Root)
    {
    if(Root)
        {
        if(HAS__UNOPS(Root))
            {
            parenthesised_result(Root, 0, FALSE, 0);
            }
        else
            reslt(Root, 0, FALSE, 0);
        }
    }
