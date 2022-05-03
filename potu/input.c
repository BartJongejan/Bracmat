#include "input.h"
#include "platformdependentdefs.h"
#include "globals.h"
#include "nodedefs.h"
#include "nonnodetypes.h"
#include "memory.h"
#include "numbercheck.h"
#include "copy.h"
#include "objectnode.h"
#include "filewrite.h"
#include "wipecopy.h"
#include "result.h"
#include "eval.h"
#include "xml.h"
#include "json.h"
#include "nodeutil.h"
#include "writeerr.h"
#include <stdarg.h>
#include <string.h>
#include <assert.h>

#ifdef __SYMBIAN32__
/* #define DEFAULT_INPUT_BUFFER_SIZE 0x100*/ /* If too high you get __chkstk error. Stack = 8K only! */
/* #define DEFAULT_INPUT_BUFFER_SIZE 0x7F00*/
#define DEFAULT_INPUT_BUFFER_SIZE 0x2000
#else
#ifdef _MSC_VER
#define DEFAULT_INPUT_BUFFER_SIZE 0x7F00 /* Microsoft C allows 32k automatic data */

#else
#ifdef __BORLANDC__
#if __BORLANDC__ >= 0x500
#define DEFAULT_INPUT_BUFFER_SIZE 0x7000
#else
#define DEFAULT_INPUT_BUFFER_SIZE 0x7FFC
#endif
#else
#define DEFAULT_INPUT_BUFFER_SIZE 0x7FFC
#endif
#endif
#endif

#if READMARKUPFAMILY
#define OPT_ML  (1 << SHIFT_ML)
#define OPT_TRM (1 << SHIFT_TRM)
#define OPT_HT  (1 << SHIFT_HT)
#define OPT_X   (1 << SHIFT_X)
#endif

#if READJSON
#define OPT_JSON (1 << SHIFT_JSN)
#endif


static unsigned char *startPos;
static unsigned char *start;
static unsigned char **pstart;
static unsigned char * inputBufferPointer;
static unsigned char * maxInputBufferPointer; /* inputBufferPointer <= maxInputBufferPointer,
                            if inputBufferPointer == maxInputBufferPointer, don't assign to *inputBufferPointer */

static const char
unbalanced[] = "unbalanced";

typedef struct inputBuffer
    {
    unsigned char * buffer;
    unsigned int cutoff : 1;    /* Set to true if very long string does not fit in buffer of size DEFAULT_INPUT_BUFFER_SIZE */
    unsigned int mallocallocated : 1; /* True if allocated with malloc. Otherwise on stack (except EPOC). */
    } inputBuffer;

static inputBuffer * InputArray;
static inputBuffer * InputElement; /* Points to member of InputArray */


static unsigned char *shift_nw(VOIDORARGPTR)
/* Used from starttree_w and build_up */
    {
    if (startPos)
        {
#if GLOBALARGPTR
        startPos = va_arg(argptr, unsigned char *);
#else
        startPos = va_arg(*pargptr, unsigned char *);
#endif
        if (startPos)
            start = startPos;
        }
    return start;
    }

static void combineInputBuffers(void)
/*
Only to be called if the current input buffer is too small to contain
a complete string (atom) and the content continues in one or more of
the next buffers. These buffers are combined into one big buffer.
*/
    {
    inputBuffer * nextInputElement = InputElement + 1;
    inputBuffer * next2;
    unsigned char * bigBuffer;
    size_t len;
    while (nextInputElement->cutoff)
        ++nextInputElement;

    len = (nextInputElement - InputElement) * (DEFAULT_INPUT_BUFFER_SIZE - 1) + 1;

    if (nextInputElement->buffer)
        {
        len += strlen((const char *)nextInputElement->buffer);
        }

    bigBuffer = (unsigned char *)bmalloc(__LINE__, len);

    nextInputElement = InputElement;

    while (nextInputElement->cutoff)
        {
        strncpy((char *)bigBuffer + (nextInputElement - InputElement)*(DEFAULT_INPUT_BUFFER_SIZE - 1), (char *)nextInputElement->buffer, DEFAULT_INPUT_BUFFER_SIZE - 1);
        bfree(nextInputElement->buffer);
        ++nextInputElement;
        }

    if (nextInputElement->buffer)
        {
        strcpy((char *)bigBuffer + (nextInputElement - InputElement)*(DEFAULT_INPUT_BUFFER_SIZE - 1), (char *)nextInputElement->buffer);
        if (nextInputElement->mallocallocated)
            {
            bfree(nextInputElement->buffer);
            }
        ++nextInputElement;
        }
    else
        bigBuffer[(nextInputElement - InputElement)*(DEFAULT_INPUT_BUFFER_SIZE - 1)] = '\0';

    InputElement->buffer = bigBuffer;
    InputElement->cutoff = FALSE;
    InputElement->mallocallocated = TRUE;

    for (next2 = InputElement + 1; nextInputElement->buffer; ++next2, ++nextInputElement)
        {
        next2->buffer = nextInputElement->buffer;
        next2->cutoff = nextInputElement->cutoff;
        next2->mallocallocated = nextInputElement->mallocallocated;
        }

    next2->buffer = NULL;
    next2->cutoff = FALSE;
    next2->mallocallocated = FALSE;
    }

static unsigned char * vshift_w(VOIDORARGPTR)
/* used from buildtree_w, which receives a list of bmalloc-allocated string
   pointers. The last string pointer must not be deallocated here */
    {
    if (InputElement->buffer && (++InputElement)->buffer)
        {
        if (InputElement->cutoff)
            {
            combineInputBuffers();
            }
        assert(InputElement[-1].mallocallocated);
        bfree(InputElement[-1].buffer);
        InputElement[-1].mallocallocated = FALSE;
        start = InputElement->buffer;
        }
    return start;
    }

static unsigned char *vshift_nw(VOIDORARGPTR)
/* Used from vbuildup */
    {
    if (*pstart && *++pstart)
        start = *pstart;
    return start;
    }

static unsigned char *(*shift)(VOIDORARGPTR) = shift_nw;

void tel(int c)
    {
    UNREFERENCED_PARAMETER(c);
    telling++;
    }

void tstr(int c)
    {
    static int esc = FALSE, str = FALSE;
    if (esc)
        {
        esc = FALSE;
        telling++;
        }
    else if (c == '\\')
        esc = TRUE;
    else if (str)
        {
        if (c == '"')
            str = FALSE;
        else
            telling++;
        }
    else if (c == '"')
        str = TRUE;
    else if (c != ' ')
        telling++;
    }

void pstr(int c)
    {
    static int esc = FALSE, str = FALSE;
    if (esc)
        {
        esc = FALSE;
        switch (c)
            {
                case 'n':
                    c = '\n';
                    break;
                case 'f':
                    c = '\f';
                    break;
                case 'r':
                    c = '\r';
                    break;
                case 'b':
                    c = '\b';
                    break;
                case 'a':
                    c = ALERT;
                    break;
                case 'v':
                    c = '\v';
                    break;
                case 't':
                    c = '\t';
                    break;
                case 'L':
                    c = 016;
                    break;
                case 'D':
                    c = 017;
                    break;
            }
        *source++ = (char)c;
        }
    else if (c == '\\')
        esc = TRUE;
    else if (str)
        {
        if (c == '"')
            str = FALSE;
        else
            *source++ = (char)c;
        }
    else if (c == '"')
        str = TRUE;
    else if (c != ' ')
        *source++ = (char)c;
    }

void glue(int c)
    {
    *source++ = (char)c;
    }


static int flags(void)
    {
    int Flgs = 0;

    for (;; start++)
        {
        switch (*start)
            {
                case '!':
                    if (Flgs & INDIRECT)
                        Flgs |= DOUBLY_INDIRECT;
                    else
                        Flgs |= INDIRECT;
                    continue;
                case '[':
                    Flgs |= POSITION;
                    continue;
                case '?':
                    Flgs |= UNIFY;
                    continue;
                case '#':
                    Flgs |= NUMBER;
                    continue;
                case '/':
                    Flgs |= FRACTION;
                    continue;
                case '@':
                    Flgs |= ATOM;
                    continue;
                case '`':
                    Flgs |= FENCE;
                    continue;
                case '%':
                    Flgs |= NONIDENT;
                    continue;
                case '~':
                    Flgs ^= NOT;
                    continue;
                case '<':
                    Flgs |= SMALLER_THAN;
                    continue;
                case '>':
                    Flgs |= GREATER_THAN;
                    continue;
                case '-':
                    Flgs ^= MINUS;
                    continue;
            }
        break;
        }

    if ((Flgs & NOT) && (Flgs < ATOM))
        Flgs ^= SUCCESS;
    return Flgs;
    }

static psk Atom(int Flgs)
    {
    unsigned char *begin, *eind;
    size_t af = 0;
    psk Pnode;
    begin = start;

    while (optab[*start] == NOOP)
        if (*start++ == 0x7F)
            af++;

    eind = start;
    Pnode = (psk)bmalloc(__LINE__, sizeof(ULONG) + 1 + (size_t)(eind - begin) - af);
    start = begin;
    begin = POBJ(Pnode);
    while (start < eind)
        {
        if (*start == 0x7F)
            {
            ++start;
            *begin++ = (unsigned char)(*start++ | 0x80);
            }
        else
            {
            *begin++ = (unsigned char)(*start++ & 0x7F);
            }
        }
    if (Flgs & INDIRECT)
        {
        Pnode->v.fl = Flgs ^ SUCCESS;
        }
    else
        {
        if (NEGATION(Flgs, NUMBER))
            Pnode->v.fl = (Flgs ^ (READY | SUCCESS));
        else
            Pnode->v.fl = (Flgs ^ (READY | SUCCESS)) | (numbercheck(SPOBJ(Pnode)) & ~DEFINITELYNONUMBER);
        }
#if DATAMATCHESITSELF
    if (!(Pnode->v.fl & VISIBLE_FLAGS))
        {
        Pnode->v.fl |= SELFMATCHING;
        }
#endif
    return Pnode;
    }

#if DATAMATCHESITSELF
static psk leftDescend_rightDescend(psk top)
    {
    /* (a.b).((c.d).(e.f)) -> (((a.b).(c.d)).e).f*/
    psk Lhs, child;
    Lhs = top->LEFT;
    while (top->v.fl & IMPLIEDFENCE)
        {
        child = top->RIGHT;
        if (Op(child) != Op(top))
            break;
        /* Something to do. */
        top->RIGHT = child->LEFT;
        child->LEFT = top;
        top = child;
        }
    for (;;)
        {
        top->v.fl &= ~IMPLIEDFENCE;
        child = top->LEFT;
        if (RATIONAL(child))
            child->v.fl |= SELFMATCHING;
        if (RATIONAL(top->RIGHT))
            top->RIGHT->v.fl |= SELFMATCHING;
        if (child != Lhs)
            top->LEFT = child->RIGHT;
        if (((top->RIGHT->v.fl) & SELFMATCHING)
            && ((top->LEFT->v.fl) & SELFMATCHING)
            )
            {
            /* Something to do. */
            top->v.fl |= SELFMATCHING;
            }
        else
            top->v.fl &= ~SELFMATCHING;

        if (child == Lhs)
            break;
        child->RIGHT = top;
        top = child;
        }
    return top;
    }
#endif

#if GLOBALARGPTR
static psk lex(int * nxt, int priority, int Flags)
#else
static psk lex(int * nxt, int priority, int Flags, va_list * pargptr)
#endif
/* *nxt (if nxt != 0) is set to the character following the expression. */
    {
    int op_or_0;
    psk Pnode;
    if (*start > 0 && *start <= '\6')
        Pnode = same_as_w(addr[*start++]);
    else
        {
        int Flgs;
        Flgs = flags();
        if (*start == '(')
            {
            if (*++start == 0)
#if GLOBALARGPTR
            (*shift)();
            Pnode = lex(NULL, 0, Flgs);
#else
                (*shift)(pargptr);
            Pnode = lex(NULL, 0, Flgs, pargptr);
#endif
            }
        else
            Pnode = Atom(Flgs);
        }
    /* Check whether Pnode is followed by an operator, in which case Pnode
       would become the LHS of that operator. */
#if GLOBALARGPTR
    if ((*start == 0) && (!*(*shift)()))
#else
    if ((*start == 0) && (!*(*shift)(pargptr)))
#endif
        {
        /* No, no operator following. Return what we already have got. */
        return Pnode;
        }
    else
        {
        op_or_0 = *start;

        if (*++start == 0)
#if GLOBALARGPTR
        (*shift)();
#else
            (*shift)(pargptr);
#endif

        if (optab[op_or_0] == NOOP) /* 20080910 Otherwise problem with the k in ()k */
            /* We expected an operator, but got a NO OPerator*/
            errorprintf("malformed input\n");
        else
            {
            Flags &= ~MINUS;/* 20110831 Bitwise, operators cannot have the - flag. */
            do
                {
                /* op_or_0 == an operator */
                psk operatorNode;
                int child_op_or_0;
                if (optab[op_or_0] < priority) /* 'op_or_0' has too low priority */
                    {
                    /* Our Pnode is followed by an operator, but it turns out
                       that Pnode isn't the LHS of that operator. Instead,
                       Pnode is the RHS of some other operator and the LHS of
                       the coming operator is a bigger subtree that contains
                       Pnode as the rightmost leaf.
                    */
#if STRINGMATCH_CAN_BE_NEGATED
                    if ((Flags & (NOT | FILTERS)) == (NOT | ATOM)
                        && Op(Pnode) == MATCH
                        ) /* 20071229 Undo setting of
                              success == FALSE
                             if ~@ flags are attached to : operator
                             Notice that op_or_0 is ')'
                             This is a special case. In ~@(a:b) the ~ operator must
                             not negate the @ but the result of the string match.
                          */
                        {
                        Flags ^= SUCCESS;
                        }
#endif
                    Pnode->v.fl ^= Flags; /*19970821*/
                    if (nxt)
                        *nxt = op_or_0; /* Tell the ancestors of Pnode about
                                           the coming operator. */
                    return Pnode;
                    }
                else
                    {
                    /* The coming operator has the same or higher priority. */
#if WORD32
                    if (optab[op_or_0] == EQUALS)
                        operatorNode = (psk)bmalloc(__LINE__, sizeof(objectnode));
                    else
#endif
                        /* on 64 bit platform, sizeof(objectnode) == sizeof(knode) 20210803*/
                        operatorNode = (psk)bmalloc(__LINE__, sizeof(knode));
                    assert(optab[op_or_0] != NOOP);
                    assert(optab[op_or_0] >= 0);
                    operatorNode->v.fl = optab[op_or_0] | SUCCESS;
                    operatorNode->LEFT = Pnode;

                    if (optab[op_or_0] == priority) /* 'op_or_0' has same priority */
                        {
                        /* We are in some kind of list: the same operator is
                           found on the RHS of the current operator.
                        */
                        operatorNode->v.fl ^= Flags; /*19970821*/
                        /* Resist temptation to call lex() recursively here.
                           Just set the RHS to 0. The caller will collect all
                           list members iteratively.
                        */
                        operatorNode->RIGHT = NULL;
                        if (nxt)
                            *nxt = op_or_0; /* To let the caller know to
                                               iterate.
                                            */
                        return operatorNode;
                        }
                    else
                        {
#if DATAMATCHESITSELF
                        switch (optab[op_or_0])
                            {
                                case DOT:
                                case COMMA:
                                case WHITE:
                                case PLUS:
                                case TIMES:
                                case EXP:
                                case LOG:
                                    {
                                    /* Must be turned off in leftDescend_rightDescend() */
                                    Pnode = operatorNode;/* 'op_or_0' has sufficient priority */
                                    for (;;)
                                        {
                                        operatorNode->v.fl |= IMPLIEDFENCE;
                                        child_op_or_0 = 0;
                                        assert(optab[op_or_0] >= 0);
#if GLOBALARGPTR
                                        operatorNode->RIGHT = lex(&child_op_or_0, optab[op_or_0], 0);
#else
                                        operatorNode->RIGHT = lex(&child_op_or_0, optab[op_or_0], 0, pargptr);
#endif
                                        if (child_op_or_0 != op_or_0)
                                            break;
                                        operatorNode = operatorNode->RIGHT;
                                        }
                                    op_or_0 = child_op_or_0;
                                    Pnode = leftDescend_rightDescend(Pnode);
                                    break;
                                    }
                                default:
                                    {
#endif
                                    Pnode = operatorNode;/* 'op_or_0' has sufficient priority */
                                    for (;;)
                                        {
                                        child_op_or_0 = 0;
                                        assert(optab[op_or_0] >= 0);
#if GLOBALARGPTR
                                        operatorNode->RIGHT = lex(&child_op_or_0, optab[op_or_0], 0);
#else
                                        operatorNode->RIGHT = lex(&child_op_or_0, optab[op_or_0], 0, pargptr);
#endif
                                        if (child_op_or_0 != op_or_0)
                                            break;
                                        operatorNode = operatorNode->RIGHT;
                                        }
#if DATAMATCHESITSELF
                                    }
                            }
#endif
                        op_or_0 = child_op_or_0;
                        }
                    }
                } while (op_or_0 != 0);
            }
        Pnode->v.fl ^= Flags; /*19970821*/
#if DATAMATCHESITSELF
        Pnode = leftDescend_rightDescend(Pnode);
#endif
        return Pnode;
        }
    }

static psk buildtree_w(psk Pnode)
    {
    if (Pnode)
        wipe(Pnode);
    InputElement = InputArray;
    if (InputElement->cutoff)
        {
        combineInputBuffers();
        }
    start = InputElement->buffer;
    shift = vshift_w;
#if GLOBALARGPTR
    Pnode = lex(NULL, 0, 0);
#else
    Pnode = lex(NULL, 0, 0, 0);
#endif
    shift = shift_nw;
    if ((--InputElement)->mallocallocated)
        {
        bfree(InputElement->buffer);
        }
    bfree(InputArray);
    return Pnode;
    }

static void lput(int c)
    {
    if (inputBufferPointer >= maxInputBufferPointer)
        {
        inputBuffer * newInputArray;
        unsigned char * input_buffer;
        unsigned char * dest;
        int len;
        size_t L;

        for (len = 0; InputArray[++len].buffer;)
            ;
        /* len = index of last element in InputArray array */

        input_buffer = InputArray[len - 1].buffer;
        /* The last string (probably on the stack, not on the heap) */

        while (inputBufferPointer > input_buffer && optab[*--inputBufferPointer] == NOOP)
            ;
        /* inputBufferPointer points at last operator (where string can be split) or at
           the start of the string. */

        newInputArray = (inputBuffer *)bmalloc(__LINE__, (2 + len) * sizeof(inputBuffer));
        /* allocate new array one element bigger than the previous. */

        newInputArray[len + 1].buffer = NULL;
        newInputArray[len + 1].cutoff = FALSE;
        newInputArray[len + 1].mallocallocated = FALSE;
        newInputArray[len].buffer = input_buffer;
        /*The buffer pointers with lower index are copied further down.*/

            /*Printf("input_buffer %p\n",input_buffer);*/

        newInputArray[len].cutoff = FALSE;
        newInputArray[len].mallocallocated = FALSE;
        /*The active buffer is still the one declared in input(),
          so on the stack (except under EPOC).*/
        --len; /* point at the second last element, the one that got filled up. */
        if (inputBufferPointer == input_buffer)
            {
            /* copy the full content of input_buffer to the second last element */
            dest = newInputArray[len].buffer = (unsigned char *)bmalloc(__LINE__, DEFAULT_INPUT_BUFFER_SIZE);
            strncpy((char *)dest, (char *)input_buffer, DEFAULT_INPUT_BUFFER_SIZE - 1);
            dest[DEFAULT_INPUT_BUFFER_SIZE - 1] = '\0';
            /* Make a notice that the element's string is cut-off */
            newInputArray[len].cutoff = TRUE;
            newInputArray[len].mallocallocated = TRUE;
            }
        else
            {
            ++inputBufferPointer; /* inputBufferPointer points at first character after the operator */
            /* maxInputBufferPointer - inputBufferPointer >= 0 */
            L = (size_t)(inputBufferPointer - input_buffer);
            dest = newInputArray[len].buffer = (unsigned char *)bmalloc(__LINE__, L + 1);
            strncpy((char *)dest, (char *)input_buffer, L);
            dest[L] = '\0';
            newInputArray[len].cutoff = FALSE;
            newInputArray[len].mallocallocated = TRUE;

            /* Now remove the substring up to inputBufferPointer from input_buffer */
            L = (size_t)(maxInputBufferPointer - inputBufferPointer);
            strncpy((char *)input_buffer, (char *)inputBufferPointer, L);
            input_buffer[L] = '\0';
            inputBufferPointer = input_buffer + L;
            }

        /* Copy previous element's fields */
        while (len)
            {
            --len;
            newInputArray[len].buffer = InputArray[len].buffer;
            newInputArray[len].cutoff = InputArray[len].cutoff;
            newInputArray[len].mallocallocated = InputArray[len].mallocallocated;
            }
        bfree(InputArray);
        InputArray = newInputArray;
        }
    assert(inputBufferPointer <= maxInputBufferPointer);
    *inputBufferPointer++ = (unsigned char)c;
    }

/* referenced from xml.c json.c */
void putOperatorChar(int c)
/* c == parenthesis, operator of flag */
    {
    lput(c);
    }

/* referenced from xml.c json.c */
void putLeafChar(int c)
/* c == any character that should end as part of an atom (string) */
    {
    if (c & 0x80)
        lput(0x7F);
    lput(c | 0x80);
    }


static void politelyWriteError(psk Pnode)
    {
    unsigned char name[256] = "";
#if !_BRACMATEMBEDDED
#if !defined NO_FOPEN
    if (errorFileName)
        {
        ;
        }
    else
        {
        int i, ikar;
        Printf("\nType name of file to write erroneous expression to and then press <return>\n(Type nothing: screen, type dot '.': skip): ");
        for (i = 0; i < 255 && (ikar = mygetc(stdin)) != '\n';)
            {
            name[i++] = (unsigned char)ikar;
            }
        name[i] = '\0';
        if (name[0] && name[0] != '.')
            redirectError((char *)name);
        }
#endif
#endif
    if (name[0] != '.')
        writeError(Pnode);
    }

psk input(FILE * fpi, psk Pnode, int echmemvapstrmltrm, Boolean * err, Boolean * GoOn)
    {
    static int stdinEOF = FALSE;
    int braces, ikar, hasop, whiteSpaceSeen, escape, backslashesAreEscaped,
        inString, parentheses, error;
#ifdef __SYMBIAN32__
    unsigned char * input_buffer;
    input_buffer = bmalloc(__LINE__, DEFAULT_INPUT_BUFFER_SIZE);
#else
    unsigned char input_buffer[DEFAULT_INPUT_BUFFER_SIZE];
#endif
    if ((fpi == stdin) && (stdinEOF == TRUE))
        exit(0);
    maxInputBufferPointer = input_buffer + (DEFAULT_INPUT_BUFFER_SIZE - 1);/* there must be room  for terminating 0 */
    /* Array of pointers to inputbuffers. Initially 2 elements,
       large enough for small inputs (< DEFAULT_INPUT_BUFFER_SIZE)*/
    InputArray = (inputBuffer *)bmalloc(__LINE__, 2 * sizeof(inputBuffer));
    InputArray[0].buffer = input_buffer;
    InputArray[0].cutoff = FALSE;
    InputArray[0].mallocallocated = FALSE;
    InputArray[1].buffer = NULL;
    InputArray[1].cutoff = FALSE;
    InputArray[1].mallocallocated = FALSE;
    error = FALSE;
    braces = 0;
    parentheses = 0;
    hasop = TRUE;
    whiteSpaceSeen = FALSE;
    escape = FALSE;
    backslashesAreEscaped = TRUE; /* but false in @"C:\dir1\bracmat" */
    inString = FALSE;

#if READMARKUPFAMILY
    if (echmemvapstrmltrm & OPT_ML)
        {
        inputBufferPointer = input_buffer;
        XMLtext(fpi, source, (echmemvapstrmltrm & OPT_TRM), (echmemvapstrmltrm & OPT_HT), (echmemvapstrmltrm & OPT_X));
        *inputBufferPointer = 0;
        Pnode = buildtree_w(Pnode);
        if (err) *err = error;
#ifdef __SYMBIAN32__
        bfree(input_buffer);
#endif
        if (GoOn)
            *GoOn = FALSE;
        return Pnode;
        }
    else
#endif
#if READJSON
        if (echmemvapstrmltrm & OPT_JSON)
            {
            inputBufferPointer = input_buffer;
            error = JSONtext(fpi, (char*)source);
            *inputBufferPointer = 0;
            Pnode = buildtree_w(Pnode);
            if (err) *err = error;
#ifdef __SYMBIAN32__
            bfree(input_buffer);
#endif
            if (GoOn)
                *GoOn = FALSE;
            return Pnode;
            }
        else
#endif
            if (echmemvapstrmltrm & (OPT_VAP | OPT_STR))
                {
                for (inputBufferPointer = input_buffer;;)
                    {
                    if (fpi)
                        {
                        ikar = mygetc(fpi);
                        if (ikar == EOF)
                            {
                            if (fpi == stdin)
                                stdinEOF = TRUE;
                            break;
                            }
                        if ((fpi == stdin)
                            && (ikar == '\n')
                            )
                            break;
                        }
                    else
                        if ((ikar = *source++) == 0)
                            break;
                    if (ikar & 0x80)
                        lput(0x7F);
                    lput(ikar | 0x80);
                    if (echmemvapstrmltrm & OPT_VAP)
                        {
                        if (echmemvapstrmltrm & OPT_STR)
                            lput(' ' | 0x80);
                        else
                            lput(' ');
                        }
                    }
                *inputBufferPointer = 0;
                Pnode = buildtree_w(Pnode);
                if (err) *err = error;
#ifdef __SYMBIAN32__
                bfree(input_buffer);
#endif
                if (GoOn)
                    *GoOn = FALSE;
                return Pnode;
                }
    for (inputBufferPointer = input_buffer
         ;
#if _BRACMATEMBEDDED
         !error
         &&
#endif
         (ikar = fpi ? mygetc(fpi) : *source++) != EOF
         && ikar
         && parentheses >= 0
         ;
         )
        {
        if (echmemvapstrmltrm & OPT_ECH)
            {
            if (fpi != stdin)
                Printf("%c", ikar);
            if (ikar == '\n')
                {
                if (braces)
                    Printf("{com} ");
                else if (inString)
                    Printf("{str} ");
                else if (parentheses > 0 || fpi != stdin)
                    {
                    int tel;
                    Printf("{%d} ", parentheses);
                    if (fpi == stdin)
                        for (tel = parentheses; tel; tel--)
                            Printf("  ");
                    }
                }
            }
        if (braces)
            {
            if (ikar == '{')
                braces++;
            else if (ikar == '}')
                braces--;
            }
        else if (ikar & 0x80)
            {
            if (whiteSpaceSeen && !hasop)
                lput(' ');
            whiteSpaceSeen = FALSE;
            lput(0x7F);
            lput(ikar);
            escape = FALSE;
            hasop = FALSE;
            }
        else
            {
            if (escape)
                {
                escape = FALSE;
                if (0 <= ikar && ikar < ' ')
                    break; /* this is unsyntactical */
                switch (ikar)
                    {
                        case 'n':
                            ikar = '\n' | 0x80;
                            break;
                        case 'f':
                            ikar = '\f' | 0x80;
                            break;
                        case 'r':
                            ikar = '\r' | 0x80;
                            break;
                        case 'b':
                            ikar = '\b' | 0x80;
                            break;
                        case 'a':
                            ikar = ALERT | 0x80;
                            break;
                        case 'v':
                            ikar = '\v' | 0x80;
                            break;
                        case 't':
                            ikar = '\t' | 0x80;
                            break;
                        case '"':
                            ikar = '"' | 0x80;
                            break;
                        case 'L':
                            ikar = 016;
                            break;
                        case 'D':
                            ikar = 017;
                            break;
                        default:
                            ikar = ikar | 0x80;
                    }
                }
            else if (ikar == '\\'
                     && (backslashesAreEscaped
                         || !inString /* %\L @\L */
                         )
                     )
                {
                escape = TRUE;
                continue;
                }
            if (inString)
                {
                if (ikar == '"')
                    {
                    inString = FALSE;
                    backslashesAreEscaped = TRUE;
                    }
                else
                    {
                    lput(ikar | 0x80);
                    }
                }
            else
                {
                switch (ikar)
                    {
                        case '{':
                            braces = 1;
                            break;
                        case '}':
                            *inputBufferPointer = 0;
                            errorprintf(
                                "\n%s brace }",
                                unbalanced);
                            error = TRUE;
                            break;
                        default:
                            {
                            if (optab[ikar] == WHITE
                                && (ikar != '\n'
                                    || fpi != stdin
                                    || parentheses
                                    )
                                )
                                {
                                whiteSpaceSeen = TRUE;
                                backslashesAreEscaped = TRUE;
                                }
                            else
                                {
                                switch (ikar)
                                    {
                                        case ';':
                                            if (parentheses)
                                                {
                                                *inputBufferPointer = 0;
                                                errorprintf("\n%d %s \"(\"", parentheses, unbalanced);
                                                error = TRUE;
                                                }
                                            if (echmemvapstrmltrm & OPT_ECH)
                                                Printf("\n");
                                            /* fall through */
                                        case '\n':
                                            /* You get here only directly if fpi==stdin */
                                            *inputBufferPointer = 0;
                                            Pnode = buildtree_w(Pnode);
                                            if (error)
                                                politelyWriteError(Pnode);
                                            if (err) *err = error;
#ifdef __SYMBIAN32__
                                            bfree(input_buffer);
#endif
                                            if (GoOn)
                                                *GoOn = ikar == ';' && !error;
                                            return Pnode;
                                        default:
                                            switch (ikar)
                                                {
                                                    case '"':
                                                        inString = TRUE;
                                                        break;
                                                    case '@':
                                                    case '%': /* These flags are removed if the string
                                                                 is non-empty, so using them to
                                                                 indicate "do not use escape sequences"
                                                                 does no harm.
                                                             */
                                                        backslashesAreEscaped = FALSE;
                                                        break;
                                                    case '(':
                                                        parentheses++;
                                                        break;
                                                    case ')':
                                                        backslashesAreEscaped = TRUE;
                                                        parentheses--;
                                                        break;
                                                }

                                            if (whiteSpaceSeen
                                                && !hasop
                                                && optab[ikar] == NOOP
                                                )
                                                lput(' ');

                                            whiteSpaceSeen = FALSE;
                                            hasop =
                                                ((ikar == '(')
                                                 || ((optab[ikar] < NOOP)
                                                     && ikar != ')'
                                                     )
                                                 );

                                            if (!inString)
                                                {
                                                lput(ikar);
                                                if (hasop)
                                                    backslashesAreEscaped = TRUE;
                                                }
                                    }
                                }
                            }
                    }
                }
            }
        }
    if ((fpi == stdin) && (ikar == EOF))
        {
        stdinEOF = TRUE;
        }
    *inputBufferPointer = 0;
#if _BRACMATEMBEDDED
    if (!error)
#endif
        {
        if (inString)
            {
            errorprintf("\n%s \"", unbalanced);
            error = TRUE;
            /*exit(1);*/
            }
        if (braces)
            {
            errorprintf("\n%d %s \"{\"", braces, unbalanced);
            error = TRUE;
            /*exit(1);*/
            }
        if (parentheses > 0)
            {
            errorprintf("\n%d %s \"(\"", parentheses, unbalanced);
            error = TRUE;
            /*exit(1);*/
            }
        if (parentheses < 0)
            {
#if !defined NO_EXIT_ON_NON_SEVERE_ERRORS
            if (ikar == 'j' || ikar == 'J' || ikar == 'y' || ikar == 'Y')
                {
                exit(0);
                }
            else if (!fpi || fpi == stdin)
                {
                Printf("\nend session? (y/n)");
                while ((ikar = mygetc(stdin)) != 'n')
                    {
                    if (ikar == 'j' || ikar == 'J' || ikar == 'y' || ikar == 'Y')
                        {
                        exit(0);
                        }
                    }
                while (ikar != '\n')
                    {
                    ikar = mygetc(stdin);
                    }
                }
            else
#endif
                {
                errorprintf("\n%d %s \")\"", -parentheses, unbalanced);
                error = TRUE;
                /*exit(1);*/
                }
            }
        if (echmemvapstrmltrm & OPT_ECH)
            Printf("\n");
        /*if(*InputArray[0].buffer)*/
        {
        Pnode = buildtree_w(Pnode);
        if (error)
            {
            politelyWriteError(Pnode);
            }
        }
        /*else
            {
            bfree(InputArray);
            }*/
        }
#if _BRACMATEMBEDDED
    else
        {
        bfree(InputArray);
        }
#endif
    if (err)
        *err = error;
#ifdef __SYMBIAN32__
    bfree(input_buffer);
#endif
    if (GoOn)
        *GoOn = FALSE;
    return Pnode;
    }

#if JMP
#include <setjmp.h>
static jmp_buf jumper;
#endif

void stringEval(const char *s, const char ** out, int * err)
    {
#if _BRACMATEMBEDDED
    char * buf = (char *)malloc(strlen(s) + 11);
    sprintf(buf, "put$(%s,MEM)", s);
#else
    char * buf = (char *)malloc(strlen(s) + 7);
    if (buf)
        {
        sprintf(buf, "str$(%s)", s);
#endif
        source = (unsigned char*)buf;
        global_anchor = input(NULL, global_anchor, OPT_MEM, err, NULL); /* 4 -> OPT_MEM*/
        if (err && *err)
            return;
#if JMP
        if (setjmp(jumper) != 0)
            {
            free(buf);
            return -1;
            }
#endif
        global_anchor = eval(global_anchor);
        if (out != NULL)
            *out = is_op(global_anchor) ? (const char*)"" : (const char*)POBJ(global_anchor);
        free(buf);
        }
    return;
    }


void init_opcode(void)
    {
    int tel;
    for (tel = 0; tel < 256; tel++)
        {
#if TELLING
        cnts[tel] = 0;
#endif
        switch (tel)
            {

                case 0:
                case ')': optab[tel] = -1; break;
                case '=': optab[tel] = EQUALS; break;
                case '.': optab[tel] = DOT; break;
                case ',': optab[tel] = COMMA; break;
                case '|': optab[tel] = OR; break;
                case '&': optab[tel] = AND; break;
                case ':': optab[tel] = MATCH; break;
                case '+': optab[tel] = PLUS; break;
                case '*': optab[tel] = TIMES; break;
                case '^': optab[tel] = EXP; break;
                case 016: optab[tel] = LOG; break;
                case 017: optab[tel] = DIF; break;
                case '$': optab[tel] = FUN; break;
                case '\'': optab[tel] = FUU; break;
                case '_': optab[tel] = UNDERSCORE; break;
                default: optab[tel] = (tel <= ' ') ? WHITE : NOOP;
            }
        }
    }

psk starttree_w(psk Pnode, ...)
    {
#if !GLOBALARGPTR
    va_list argptr;
#endif
    if (Pnode)
        wipe(Pnode);
    va_start(argptr, Pnode);
    start = startPos = va_arg(argptr, unsigned char *);
#if GLOBALARGPTR
    Pnode = lex(NULL, 0, 0);
#else
    Pnode = lex(NULL, 0, 0, &argptr);
#endif
    va_end(argptr);
    return Pnode;
    }

static psk vbuildupnowipe(psk Pnode, const char *conc[])
    {
    psk okn;
    assert(Pnode != NULL);
    pstart = (unsigned char **)conc;
    start = (unsigned char *)conc[0];
    shift = vshift_nw;
#if GLOBALARGPTR
    okn = lex(NULL, 0, 0);
#else
    okn = lex(NULL, 0, 0, 0);
#endif
    shift = shift_nw;
    okn = setflgs(okn, Pnode->v.fl);
    return okn;
    }

psk vbuildup(psk Pnode, const char *conc[])
    {
    psk okn = vbuildupnowipe(Pnode, conc);
    wipe(Pnode);
    return okn;
    }

psk build_up(psk Pnode, ...)
    {
    psk okn;
#if !GLOBALARGPTR
    va_list argptr;
#endif
    va_start(argptr, Pnode);
    start = startPos = va_arg(argptr, unsigned char *);
#if GLOBALARGPTR
    okn = lex(NULL, 0, 0);
#else
    okn = lex(NULL, 0, 0, &argptr);
#endif
    va_end(argptr);
    if (Pnode)
        {
        okn = setflgs(okn, Pnode->v.fl);
        wipe(Pnode);
        }
    return okn;
    }

psk dopb(psk Pnode, psk src)
    {
    psk okn;
    okn = same_as_w(src);
    okn = setflgs(okn, Pnode->v.fl);
    wipe(Pnode);
    return okn;
    }

