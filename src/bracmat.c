/*
    Bracmat. Programming language with pattern matching on tree structures.
    Copyright (C) 2002  Bart Jongejan

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/*
email: bartj@hum.ku.dk
*/

#define DATUM "5 March 2023"
#define VERSION "6.12.8"
#define BUILD "270"
/*
COMPILATION
-----------
It is assumed that the hardware is litte-endian, (which has become the most
common these days).

If you can compile successfully but cannot run the program succesfully,
try putting -DBIGENDIAN on the command line of the compiler.
This works of course only if the machine's hardware is big-endian.

The program only utilizes standard libraries.

Until 2012, the source code consisted of a single file, bracmat.c.
Because XML is becoming more popular, a separate source file, xml.c
implements reading XML files. Another separate source file, json.c,
implements reading JSON files.

On *N?X, just compile with

    gcc -std=c99 -pedantic -Wall -O3 -static -DNDEBUG -o bracmat bracmat.c xml.c json.c


The options are not necessary to successfully compile the program just

    gcc *.c

also works and produces the executable a.out.

Profiling:

    gcc -Wall -c -pg -DNDEBUG bracmat.c xml.c json.c
    gcc -Wall -pg bracmat.o xml.o json.o
    ./a.out 'get$"valid.bra";!r'
    gprof a.out

Test coverage:
(see http://gcc.gnu.org/onlinedocs/gcc/Invoking-Gcov.html#Invoking-Gcov)

    gcc -fprofile-arcs -ftest-coverage -DNDEBUG bracmat.c xml.c json.c
    ./a.out
    gcov bracmat.c
    gcov xml.c
    gcov json.c

*/

/*
NOTES
=====

2002.12.15
----------
This is the open source version of Bracmat.

Originally, Bracmat was written in Basic for the Amstrad computer. Its sole
purpose was to derive a simple expression for the curvature of a model
space-time, starting from the components of the metric. The only operations
the program was capable to perform were multiplications, exponentiations,
logarithms, and partial derivatives. The expressions were a mix of numbers
and symbolic variables, like "a+2*b". Thus, "a+a" was simplified to "2*a".

The second version of Bracmat, about 1988, was developed in Ansi C and run
on an Acorn Archimedes. Great care was taken to make the program portable to
other hardware, such as Atari, VAX and even the PC. The PC architecture, with
its limited memory, its 16 bit architecture and its limitation on the size of
the program stack, posed some severe limitations on the program. Therefore
great care was taken to limit the use of the stack to a minimum. This has
lead to some rather large functions (to save stack-consuming function calls).

The C version became a true programnming language, with program variables,
logical operators and function calls. The syntax, however, was not changed.
For example, instead of using "if...then...else...", I decided to write
something like "...&...|...", using lazy evaluation of the infix binary
logical operators "&" (AND) and "|" (OR) for program control.

The logical operators, of course, should test for operands being "true" or
"false". The main provider of interesting truth values became the pattern
matching operator ":", which tests whether the pattern (the rhs-operand)
matches the subject (the lhs-operand). The single outstanding feature of
Bracmat is, in fact, its pattern recognition, which works on tree structures,
not on strings of characters.

Bracmat was also extended with a few operators that makes this programming
language interesting in the field of computational linguistics, namely the
blank " ", the comma "," and the full stop ".". (The interpunction symbols
"?", "!" and ";" were already 'taken' to serve other purposes.)
*/

/*
TODO list:
20010103: Make > and < work on non-atomic stuff
20010904: Issue warning if 'arg' is declared as a local variable
*/
/* 20010309 evaluate on WHITE and COMMA strings is now iterative on the
   right descending (deep) branch, not recursive.
   (It is now possible to read long lists, e.g. dictionairies, without causing
   stack overflow when evaluating the list.)
   20010821 a () () was evaluated to a (). Removed last ().
   20010903 (a.) () was evaluated to a
*/
#include "unicaseconv.h"
#include "unichartypes.h"
#define DEBUGBRACMAT 1 /* Implement dbg'(expression), see DBGSRC macro */
#define REFCOUNTSTRESSTEST 0
#define DOSUMCHECK 0
#define CHECKALLOCBOUNDS 0 /* only if NDEBUG is false */
#define PVNAME 0 /* Allocate strings of variable names separately from variable
                    structure. Does not give better results. */
#define STRINGMATCH_CAN_BE_NEGATED 0 /* ~@(a:a) */
#define CODEPAGE850 0
#define MAXSTACK 0 /* 1: Show max stack depth (eval function only)*/
#define CUTOFFSUGGEST 1
#define READMARKUPFAMILY 1 /* Read SGML, HTML and XML files. 
                             (include xml.c in your project!) */
#define READJSON 1 /* Read JSON files. (Include json.c in your project!) */
#define SHOWMEMBLOCKS 0
#define DATAMATCHESITSELF 0 /* An experiment from August 2021.
The idea is to make matching a data structure with itself faster by just
checking whether they have the same address. Only structures with no prefixes
anywhere and only using the operators DOT COMMA WHITE PLUS TIMES EXP LOG can be
compared safely this way. For example,

  (a=x+y+x) & ('$a:'$a)

and

  (a=(x.y),z) & !a:!a

succeed, whereas

  (b=x ~%) & ('$b:'$b)

fails.

To make this work, an extra bit has to be reserved in each node. The overhead
of setting the bit turns out to outweigh the advantage: programs become a
little bit slower, not faster.
*/

/*
About reference counting.
Most nodes can be shared by no more than 1024 referers. Copies must be made as needed.
Objects (nodes with = ('EQUALS') are objects and can be shared by almost 2^40 referers.

small refcounter       large refcounter              comment
(9 bits, all nodes)   (30 bits, only objects)
0                      0                             not shared, no test of large refcounter needed.
1                      0                             shared with one, so totalling two
2                      0                             shared with two
..                     ..
1022                   0
1023                   0                             totalling 1024, *** max for normal nodes ***
1                      1                             totalling 1025
2                      1
..                     ..
m                      1
..                     ..                            totalling 1024+(1-1)*1023+m
1023                   1                             totalling 1024+1*1023
..                     ..
m                      n                             totalling 1024+(n-1)*1023+m or 1+n*1023+m
1023                   2^30-1                        totalling 1024+(2^30-2)*1023+1023=1 098 437 885 953
                                                         (or   1+2^30*1023)
*/

#if DEBUGBRACMAT
#define DBGSRC(code)    if(debug){code}
#else
#define DBGSRC(code)
#endif

#if MAXSTACK
static int maxstack = 0;
static int stack = 0;
#define ASTACK {++stack;if(stack > maxstack) maxstack = stack;}{
#define ZSTACK }{--stack;}
#else
#define ASTACK
#define ZSTACK
#endif

#include <assert.h>

/*#define ARM */ /* assume it isn't an Acorn */


#if (defined __TURBOC__ && !defined __WIN32__) || (defined ARM && !defined __SYMBIAN32__)
#define O_S 1 /* 1 = with operating system interface swi$ (RISC_OS or TURBO-C), 0 = without  */
#else
#define O_S 0
#endif

/* How to compile
   (with a ANSI-C compiler or newer)

Archimedes ANSI-C release 3:
*up
*del. :0.$.c.clog
*spool :0.$.c.clog
*cc bracmat
*spool

With RISC_OS functions:

cc bracmat

file cc (in directory c):

| >cc
up
delete $.c.clog
spool $.c.clog
cc -c %0 -IRAM:$.RISC_OSLib
if Sys$ReturnCode = 0 then run c.li %0 else spool

file li (in directory c):

| >li
link -o %0 o.%0 RAM:$.RISC_OSLib.o.RISC_OSLib RAM:$.Clib.o.Stubs
||G
spool
if Sys$ReturnCode = 0 then squeeze %0 else echo |G

Microsoft QUICKC (MS-DOS) (compact and large model both possible)
qcl /Ox /AC /F D000 bracmat.c
Microsoft optimizing compiler V5.1
cl /Ox /AC /F D000 bracmat.c

Borland TURBOC (MS-DOS) V2.0
tcc -w -f- -r- -mc -K- bracmat

Atari : define -DATARI because of BIGENDIAN and extern int _stksize = -1;
               and -DW32   but only if (int)==(long)
*/

/* Optional #defines for debugging and adaptation to machine */

#define TELMAX  1 /* Show the maximum number of allocated nodes. */
#define TELLING 0 /* Same, plus current number of allocated nodes, in groups of
                       4,8,12 and >12 bytes */
#if TELLING
#if TELMAX == 0
#undef TELMAX
#define TELMAX 1
#endif
#ifndef TELMAX
#define TELMAX 1
#endif
#endif

                       /*#define reslt parenthesised_result */ /* to show ALL parentheses (one pair for each operator)*/
#define INTSCMP 0   /* dangerous way of comparing strings */
#define ICPY 0      /* copy word by word instead of byte by byte */

/* There are no optional #defines below this line. */
#if defined __BYTE_ORDER && defined __LITTLE_ENDIAN /* gcc on linux defines these */
#if __BYTE_ORDER == __LITTLE_ENDIAN
#define BIGENDIAN 0
#else
#define BIGENDIAN 1
#endif
#endif

#ifndef BIGENDIAN

#if defined mc68000 || defined MC68000 || defined mc68010 || defined mc68020 || defined mc68030 || defined ATARI || defined sparc || defined __hpux || defined __hpux__
#define BIGENDIAN 1
#endif

#endif

#ifndef BIGENDIAN
#define BIGENDIAN     0
#endif

#if defined BRACMATEMBEDDED
#define _BRACMATEMBEDDED 1
#else
#define _BRACMATEMBEDDED 0
#endif

#if (defined _Windows || defined _MT /*multithreaded, VC6.0*/)&& (!defined _CONSOLE && !defined __CONSOLE__ || defined NOTCONSOLE || _BRACMATEMBEDDED)
/* _CONSOLE defined by Visual C++ and __CONSOLE__ seems always to be defined in C++Builder */
#define MICROSOFT_WINDOWS_API 1
#else
#define MICROSOFT_WINDOWS_API 0
#endif

#if MICROSOFT_WINDOWS_API && defined POLL /* Often no need for polling in multithreaded apps.*/
#define JMP 1 /* 1: listen to WM_QUIT message. 0: Do not listen to WM_QUIT message. */
#else
#define JMP 0
#endif

#if _BRACMATEMBEDDED
#include "bracmat.h"
#endif


#if defined sun && !defined __GNUC__
#include <unistd.h> /* SEEK_SET, SEEK_CUR */
#define ALERT 7
#define strtoul(a,b,c) strtol(a,b,c)
#else
#include <stddef.h>
#endif

#include <stdlib.h>

#ifndef ALERT
#define ALERT '\a'
#endif

#include <limits.h>

#if defined _WIN64 || defined _WIN32
#ifdef __BORLANDC__
typedef unsigned int UINT32_T;
typedef   signed int  INT32_T;
#else
typedef unsigned __int32 UINT32_T; /* pre VS2010 has no int32_t */
typedef   signed __int32  INT32_T; /* pre VS2010 has no int32_t */
#endif
#endif

#if defined _WIN64
/*Microsoft*/
#define WORD32  0
#define _4      1
#define _5_6    1
#define LONG long long
#define ULONG unsigned long long
#define LONGU "%llu"
#define LONGD "%lld"
#define LONG0D "%0lld"
#define LONG0nD "%0*lld"
#define LONGX "%llX"
#define STRTOUL _strtoui64
#define STRTOL _strtoi64
#define FSEEK _fseeki64
#define FTELL _ftelli64
#else
#if !defined NO_C_INTERFACE && !defined _WIN32
#if UINT_MAX == 4294967295ul
typedef unsigned int UINT32_T;
#elif ULONG_MAX == 4294967295ul
typedef unsigned long UINT32_T;
#endif
#endif
#if !defined NO_LOW_LEVEL_FILE_HANDLING
#if UINT_MAX == 4294967295ul
typedef   signed int  INT32_T;
#elif ULONG_MAX == 4294967295ul
typedef   signed long  INT32_T;
#endif
#endif
#if LONG_MAX <= 2147483647L
#define WORD32  1
#define _4      1
#define _5_6    1
#else
#define WORD32  0
#define _4      1
#define _5_6    1
#endif
#define LONG long
#define ULONG unsigned long
#define LONGU "%lu"
#define LONGD "%ld"
#define LONG0D "%0ld"
#define LONG0nD "%0*ld"
#define LONGX "%lX"
#define STRTOUL strtoul
#define STRTOL strtol
#define FSEEK fseek
#define FTELL ftell
#endif


#if defined __TURBOC__ || defined __MSDOS__ || defined _WIN32 || defined __GNUC__
#define DELAY_DUE_TO_INPUT
#endif

#include <time.h>
#ifndef CLOCKS_PER_SEC
#ifdef CLK_TCK  /* pre-ANSI-C */
#define CLOCKS_PER_SEC CLK_TCK
#else
#if defined sun
#define CLOCKS_PER_SEC 1000000 /* ??? */
#endif
#endif
#endif

#define READBIN   "rb" /* BIN is default for get$, overrule with option TXT */
#define READTXT   "r"
#define WRITEBIN  "wb" /* BIN is default for lst$, overrule with option TXT */
#define APPENDBIN "ab" 
#define WRITETXT  "w"  /* TXT is default for put$, overrule with option BIN */
#define APPENDTXT "a"

#define LOGWORDLENGTH 2
/* flags (prefixes) in node */
#define NOT              1      /* ~ */
     /* Keep definition of NOT like this because of mixing of logical and bit
        operators && || | ^ & */
#define SUCCESS         (1<< 1)
#define READY           (1<< 2)
#define POSITION        (1<< 3) /* [ */
#define INDIRECT        (1<< 4) /* ! */
#define DOUBLY_INDIRECT (1<< 5) /* !! */
#define FENCE           (1<< 6) /* `   (within same byte as ATOM and NOT) */
#define ATOM            (1<< 7) /* @ */
#define NONIDENT        (1<< 8) /* % */
#define GREATER_THAN    (1<< 9) /* > */
#define SMALLER_THAN    (1<<10) /* < */
#define NUMBER          (1<<11) /* # */
#define FRACTION        (1<<12) /* / */
#define UNIFY           (1<<13) /* ? */
#define IDENT           (1<<14)
#define IMPLIEDFENCE    (1<<15) /* 20070222 */
        /* 1<<16 test whether operator
           1<<17 operator
           1<<18 operator
           1<<19 operator
           1<<20 operator
           1<<21 latebind
        */
#if DATAMATCHESITSELF
#define SELFMATCHING    (1<<22) /* 20210801 */
#define BITWISE_OR_SELFMATCHING |SELFMATCHING
#else
#define BITWISE_OR_SELFMATCHING 
#endif

#if WORD32
#else
#if DATAMATCHESITSELF
#define BUILT_IN        (1<<23) /* 20210801 only used for objects (operator =) */
#define CREATEDWITHNEW  (1<<24) /* 20210801 only used for objects (operator =) */
#else
#define BUILT_IN        (1<<22) /* 20210801 only used for objects (operator =) */
#define CREATEDWITHNEW  (1<<23) /* 20210801 only used for objects (operator =) */
#endif
#endif

#define VISIBLE_FLAGS_WEAK      (INDIRECT|DOUBLY_INDIRECT|FENCE|UNIFY)
#define VISIBLE_FLAGS_NON_COMP  (INDIRECT|DOUBLY_INDIRECT|ATOM|NONIDENT|NUMBER|FRACTION|UNIFY) /* allows < > ~< and ~> as flags on numbers */
#define VISIBLE_FLAGS_POS0      (INDIRECT|DOUBLY_INDIRECT|NONIDENT|QFRACTION|UNIFY|QNUMBER)
#define VISIBLE_FLAGS_POS       (INDIRECT|DOUBLY_INDIRECT|NONIDENT|QFRACTION|UNIFY|QNUMBER|NOT|GREATER_THAN|SMALLER_THAN)
#define VISIBLE_FLAGS           (INDIRECT|DOUBLY_INDIRECT|ATOM|NONIDENT|NUMBER|FRACTION|UNIFY|NOT|GREATER_THAN|SMALLER_THAN|FENCE|POSITION)

#define HAS_VISIBLE_FLAGS_OR_MINUS(psk) ((psk)->v.fl & (VISIBLE_FLAGS|MINUS))
#define RATIONAL(psk)      (((psk)->v.fl & (QNUMBER|IS_OPERATOR|VISIBLE_FLAGS)) == QNUMBER)
#define RATIONAL_COMP(psk) (((psk)->v.fl & (QNUMBER|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP)) == QNUMBER)
#define RATIONAL_COMP_NOT_NUL(psk) (((psk)->v.fl & (QNUMBER|QNUL|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP)) == QNUMBER)
#define RATIONAL_WEAK(psk) (((psk)->v.fl & (QNUMBER|IS_OPERATOR|INDIRECT|DOUBLY_INDIRECT|FENCE|UNIFY)) == QNUMBER)/* allows < > ~< and ~> as flags on numbers */
#define       LESS(psk)    (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|SMALLER_THAN))
#define LESS_EQUAL(psk)    (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|NOT|GREATER_THAN))
#define MORE_EQUAL(psk)    (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|NOT|SMALLER_THAN))
#define       MORE(psk)    (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|GREATER_THAN))
#define    UNEQUAL(psk)    (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|NOT))
#define LESSORMORE(psk)    (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|SMALLER_THAN|GREATER_THAN))
#define      EQUAL(psk)    (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == QNUMBER)
#define NOTLESSORMORE(psk) (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|NOT|SMALLER_THAN|GREATER_THAN))

#define INTEGER(pn)               (((pn)->v.fl & (QNUMBER|QFRACTION|IS_OPERATOR|VISIBLE_FLAGS))                      == QNUMBER)
#define INTEGER_COMP(pn)          (((pn)->v.fl & (QNUMBER|QFRACTION|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP))             == QNUMBER)

#define INTEGER_NOT_NEG(pn)       (((pn)->v.fl & (QNUMBER|MINUS|QFRACTION|IS_OPERATOR|VISIBLE_FLAGS))                == QNUMBER)
#define INTEGER_NOT_NEG_COMP(pn)  (((pn)->v.fl & (QNUMBER|MINUS|QFRACTION|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP))       == QNUMBER)

#define INTEGER_POS(pn)           (((pn)->v.fl & (QNUMBER|MINUS|QNUL|QFRACTION|IS_OPERATOR|VISIBLE_FLAGS))           == QNUMBER)
#define INTEGER_POS_COMP(pn)      (((pn)->v.fl & (QNUMBER|MINUS|QNUL|QFRACTION|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP))  == QNUMBER)

#define INTEGER_NOT_NUL_COMP(pn) (((pn)->v.fl & (QNUMBER|QNUL|QFRACTION|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP))        == QNUMBER)
#define HAS_MINUS_SIGN(pn)         (((pn)->v.fl & (MINUS|IS_OPERATOR)) == MINUS)

#define RAT_NUL(pn) (((pn)->v.fl & (QNUL|IS_OPERATOR|VISIBLE_FLAGS)) == QNUL)
#define RAT_NUL_COMP(pn) (((pn)->v.fl & (QNUL|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP)) == QNUL)
#define RAT_NEG(pn) (((pn)->v.fl & (QNUMBER|MINUS|IS_OPERATOR|VISIBLE_FLAGS)) \
                                == (QNUMBER|MINUS))
#define RAT_NEG_COMP(pn) (((pn)->v.fl & (QNUMBER|MINUS|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP)) \
                                == (QNUMBER|MINUS))

#define RAT_RAT(pn) (((pn)->v.fl & (QNUMBER|QFRACTION|IS_OPERATOR|VISIBLE_FLAGS))\
                                == (QNUMBER|QFRACTION))

#define RAT_RAT_COMP(pn) (((pn)->v.fl & (QNUMBER|QFRACTION|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP))\
                                == (QNUMBER|QFRACTION))
#define IS_ONE(pn) ((pn)->u.lobj == ONE && !((pn)->v.fl & (MINUS | VISIBLE_FLAGS)))
#define IS_NIL(pn) ((pn)->u.lobj == 0   && !((pn)->v.fl & (MINUS | VISIBLE_FLAGS)))


#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#if INTSCMP
#define STRCMP(a,b) intscmp((LONG*)(a),(LONG*)(b))
#else
#define STRCMP(a,b) strcmp((char *)(a),(char *)(b))
#endif
#if ICPY
#define MEMCPY(a,b,n) icpy((LONG*)(a),(LONG*)(b),n)
#else
#define MEMCPY(a,b,n) memcpy((char *)(a),(char *)(b),n)
#endif

#ifdef ATARI
extern int _stksize = -1;
#endif

#if defined ARM
#if O_S
#if defined __GNUC__ && !defined sparc
#include "sys/os.h"
#else
#include "os.h"
#endif
#endif
#endif

#ifdef __TURBOC__
#if O_S
typedef struct
    {
    int r[10];
    } os_regset;
#endif
#endif

#define RIGHT u.p.right
#define LEFT  u.p.left
#define TRUE 1
#define FALSE 0
#define PRISTINE (1<<2) /* Used to initialise matchstate variables. */
#define ONCE (1<<3)
#define POSITION_ONCE (1<<4)
#define POSITION_MAX_REACHED (1<<5)
#define OBJ(p) &((p).u.obj)
#define LOBJ(p) ((p).u.lobj)
#define POBJ(p) &((p)->u.obj)
#define SPOBJ(p) &((p)->u.sobj)
/*#define PIOBJ(p) ((p)->u.iobj)*/ /* Added. Bart 20031110 */
#define PLOBJ(p) ((p)->u.lobj)

#define QOBJ(p) &(p)
#define QPOBJ(p) p

#if BIGENDIAN
#define iobj lobj
#define O(a,b,c) (a*0x1000000L+b*0x10000L+c*0x100)
#define notO 0xFF
#else
#if !WORD32
#define iobj lobj
#endif
#define O(a,b,c) (a+b*0x100+c*0x10000L)
#define notO 0xFF000000
#endif

#define not_built_in(pn) (((pn)->v.fl & IS_OPERATOR) || (((pn)->u.lobj) & (notO)))

#if TELMAX
#define BEZ O('b','e','z')
#endif

#if O_S
#define SWI O('s','w','i')
#endif

#if !defined NO_C_INTERFACE
#define ALC O('a','l','c')
#define FRE O('f','r','e')
#define PEE O('p','e','e')
#define POK O('p','o','k')
#define FNC O('f','n','c')
#endif

#if !defined NO_FILE_RENAME
#define REN O('r','e','n') /* rename a file */
#endif

#if !defined NO_FILE_REMOVE
#define RMV O('r','m','v') /* remove a file */
#endif

#if !defined NO_SYSTEM_CALL
#define SYS O('s','y','s')
#endif

#if !defined NO_LOW_LEVEL_FILE_HANDLING
#define FIL O('f','i','l')
#define CHR O('C','H','R')
#define DEC O('D','E','C')
#define CUR O('C','U','R')
#define END O('E','N','D')/* SEEK_END: no guarantee that this works !!*/
#define SET O('S','E','T')
#define STRt O('S','T','R')
#define TEL O('T','E','L')
#endif

#define ARG O('a','r','g') /* arg$ returns next program argument*/
#define APP O('A','P','P')
#define ASC O('a','s','c')
#define BIN O('B','I','N')
#define CLK O('c','l','k')
#define CON O('C','O','N')
#if DEBUGBRACMAT
#define DBG O('d','b','g')
#endif
#define DEN O('d','e','n')
#define DIV O('d','i','v')
#if CODEPAGE850
#define DOS O('D','O','S')
#endif
#if _BRACMATEMBEDDED
#if defined PYTHONINTERFACE
#define NI  O('N','i', 0 )
#define NII O('N','i','!')
#endif
#endif

#define Chr O('c','h','r')
#define Chu O('c','h','u')
#define D2X O('d','2','x') /* dec -> hex */
#define ECH O('E','C','H')
/* err$foo redirects error messages to foo */
#define ERR O('e','r','r')
#define EXT O('E','X','T')
#define FLG O('f','l','g')
#define GET O('g','e','t')
#define GLF O('g','l','f') /* The opposite of flg */
/*#define HUM O('H','U','M')*/
#define HT  O('H','T', 0 )
#define IM  O('i', 0 , 0 )
#define JSN O('J','S','N')
#define LIN O('L','I','N')
#define LOW O('l','o','w')
#define LST O('l','s','t')
#define MAP O('m','a','p')
#define MEM O('M','E','M')
#define MINONE O('-','1',0)
#define ML  O('M','L',0)
#define MMF O('m','e','m')
#define MOD O('m','o','d')
#define MOP O('m','o','p')
#define NEW O('N','E','W')
#define New O('n','e','w')
#define ONE O('1', 0 , 0 )
#define PI  O('p','i', 0 )
#define PUT O('p','u','t')
#define PRV O('?', 0 , 0 )
#define RAW O('R','A','W')
#define REV O('r','e','v') /* strrev */
#define SIM O('s','i','m')
#define STG O('S','T','R')
#define STR O('s','t','r')
#define TBL O('t','b','l')
#define TRM O('T','R','M')
#define TWO O('2', 0 , 0 )
#define TXT O('T','X','T')
#define UPP O('u','p','p')
#define UGC O('u','g','c') /* Unicode General Category, a two-letter string */
#define UTF O('u','t','f')
#define VAP O('V','A','P')
#define Vap O('v','a','p') /* map for string instead of list */
#define WHL O('w','h','l')
#define WYD O('W','Y','D') /* lst option for wider lines */
#define X   O('X', 0 , 0 )
#define X2D O('x','2','d') /* hex -> dec */
#define XX  O('e', 0 , 0 )


#define SHIFT_STR 0
#define SHIFT_VAP 1
#define SHIFT_MEM 2
#define SHIFT_ECH 3
#define SHIFT_ML  4
#define SHIFT_TRM 5
#define SHIFT_HT  6
#define SHIFT_X   7
#define SHIFT_JSN 8
#define SHIFT_TXT 9  /* "r" "w" "a" */
#define SHIFT_BIN 10 /* "rb" "wb" "ab" */

#define OPT_STR (1 << SHIFT_STR)
#define OPT_VAP (1 << SHIFT_VAP)
#define OPT_MEM (1 << SHIFT_MEM)
#define OPT_ECH (1 << SHIFT_ECH)
#define OPT_TXT (1 << SHIFT_TXT)
#define OPT_BIN (1 << SHIFT_BIN)

#if READMARKUPFAMILY
#define OPT_ML  (1 << SHIFT_ML)
#define OPT_TRM (1 << SHIFT_TRM)
#define OPT_HT  (1 << SHIFT_HT)
#define OPT_X   (1 << SHIFT_X)
extern void XMLtext(FILE* fpi, char* source, int trim, int html, int xml);
#endif

#if READJSON
#define OPT_JSON (1 << SHIFT_JSN)
extern int JSONtext(FILE* fpi, char* source);
#endif

#if DATAMATCHESITSELF
#if WORD32
#define NON_REF_COUNT_BITS 23 /* prefixes, hidden flags, operator bits */
#else
#define NON_REF_COUNT_BITS 25 /* prefixes, hidden flags, operator bits */
#endif
#else
#if WORD32
#define NON_REF_COUNT_BITS 22 /* prefixes, hidden flags, operator bits */
#else
#define NON_REF_COUNT_BITS 24 /* prefixes, hidden flags, operator bits */
#endif
#endif

#if REFCOUNTSTRESSTEST
#define REF_COUNT_BITS 1
#endif

#if !defined REF_COUNT_BITS
#if WORD32
#define REF_COUNT_BITS (32 - NON_REF_COUNT_BITS)
#else
#define REF_COUNT_BITS (64 - NON_REF_COUNT_BITS)
#endif
#endif

#define FILTERS     (FRACTION | NUMBER | SMALLER_THAN | GREATER_THAN | ATOM | NONIDENT)
#define ATOMFILTERS (FRACTION | NUMBER | SMALLER_THAN | GREATER_THAN | ATOM | FENCE | IDENT)
#define SATOMFILTERS (/*ATOM | */FENCE | IDENT)

#define FLGS (FILTERS | FENCE | DOUBLY_INDIRECT | INDIRECT | POSITION)

#define NEGATION(Flgs,flag)  ((Flgs & NOT ) && \
                             (Flgs & FILTERS) >= (flag) && \
                             (Flgs & FILTERS) < ((flag) << 1))
#define ANYNEGATION(Flgs)  ((Flgs & NOT ) && (Flgs & FILTERS))
#define ASSERTIVE(Flgs,flag) ((Flgs & flag) && !NEGATION(Flgs,flag))
#define FAIL (pat->v.fl & NOT)
#define NOTHING(p) (((p)->v.fl & NOT) && !((p)->v.fl & FILTERS))
#define NOTHINGF(Flgs) ((Flgs & NOT) && !(Flgs & FILTERS))
#define BEQUEST (FILTERS | FENCE | UNIFY)
#define UNOPS (UNIFY | FLGS | NOT | MINUS)
#define HAS_UNOPS(a) ((a)->v.fl & UNOPS)
#define HAS__UNOPS(a) (is_op(a) && (a)->v.fl & (UNIFY | FLGS | NOT))
#define IS_VARIABLE(a) ((a)->v.fl & (UNIFY | INDIRECT | DOUBLY_INDIRECT))
#define IS_BANG_VARIABLE(a) ((a)->v.fl & (INDIRECT | DOUBLY_INDIRECT))

/*#define SUBJECTNOTNIL(sub,pat) (is_op(sub) || HAS_UNOPS(sub) || (PIOBJ(sub) != PIOBJ(nil(pat))))*/
#define SUBJECTNOTNIL(sub,pat) (is_op(sub) || HAS_UNOPS(sub) || (PLOBJ(sub) != PLOBJ(nil(pat))))


typedef int Boolean;

typedef union
    {
#ifndef NDEBUG
    struct
        {
        ULONG Not : 1; /* ~ */
        ULONG success : 1;
        ULONG ready : 1;
        ULONG position : 1; /* [ */

        ULONG indirect : 1; /* ! */
        ULONG doubly_indirect : 1; /* !! */
        ULONG fence : 1; /* `   (within same byte as ATOM and NOT) */
        ULONG atom : 1; /* @ */

        ULONG nonident : 1; /* % */
        ULONG greater_than : 1; /* > */
        ULONG smaller_than : 1; /* < */
        ULONG number : 1; /* # */

        ULONG breuk : 1; /* / */
        ULONG unify : 1; /* ? */
        ULONG ident : 1;
        ULONG impliedfence : 1;

        ULONG is_operator : 1;
        ULONG binop : 4; /* only if operator node*/
        /* EQUALS DOT COMMA OR AND MATCH WHITE PLUS TIMES EXP LOG DIF FUU FUN UNDERSCORE */
        ULONG latebind : 1;
#if WORD32
#else
        ULONG built_in : 1; /* only used for objects (operator =) */
        ULONG createdWithNew : 1; /* only used for objects (operator =) */
#endif
        ULONG        refcount : REF_COUNT_BITS;
        } node;
    struct
        {
        ULONG Not : 1; /* ~ */
        ULONG success : 1;
        ULONG ready : 1;
        ULONG position : 1; /* [ */

        ULONG indirect : 1; /* ! */
        ULONG doubly_indirect : 1; /* !! */
        ULONG fence : 1; /* `   (within same byte as ATOM and NOT) */
        ULONG atom : 1; /* @ */

        ULONG nonident : 1; /* % */
        ULONG greater_than : 1; /* > */
        ULONG smaller_than : 1; /* < */
        ULONG number : 1; /* # */

        ULONG breuk : 1; /* / */
        ULONG unify : 1; /* ? */
        ULONG ident : 1;
        ULONG impliedfence : 1;

        ULONG is_operator : 1;
        ULONG qgetal : 1; /* only if leaf */
        ULONG minus : 1; /* only if leaf */
        ULONG qnul : 1; /* only if leaf */
        ULONG qbreuk : 1; /* only if leaf */

        ULONG latebind : 1;
#if WORD32
#else
        ULONG built_in : 1; /* only used for objects (operator =) */
        ULONG createdWithNew : 1; /* only used for objects (operator =) */
#endif
        ULONG        refcount : REF_COUNT_BITS;
        } leaf;
#endif
    ULONG fl;
    } tFlags;


typedef struct sk
    {
    tFlags v;

    union
        {
        struct
            {
            struct sk* left, * right;
            } p;
        LONG lobj; /* This part of the structure can be used for comparisons
                      with short strings that fit into one word in one machine
                      operation, like "\0\0\0\0" or "1\0\0\0" */
        unsigned char obj;
        char sobj;
        } u;
    } sk;

static sk nilNode, nilNodeNotNeutral,
zeroNode, zeroNodeNotNeutral,
oneNode, oneNodeNotNeutral,
argNode, selfNode, SelfNode, minusTwoNode, minusOneNode, twoNode, minusFourNode, fourNode, sjtNode;

#if !defined NO_FOPEN
static char* targetPath = NULL; /* Path that can be prepended to filenames. */
#endif

typedef  sk* psk;
typedef psk* ppsk;

static psk addr[7], m0 = NULL, m1 = NULL, f0 = NULL, f1 = NULL, f4 = NULL, f5 = NULL;
#if 0 
/* UNSAFE */
/* The next two variables are used to cache the address of the most recently called user function. 
These values must be reset each time stringEval() is called.
(When Bracmat is embedded in a Java program as a JNI, data addresses are not stable, it seems.) */
static psk oldlnode = 0;
static psk lastEvaluatedFunction = 0;
#endif
/*
0:?n&whl'(1+!n:<100000:?n&57265978465924376578234566767834625978465923745729775787627876873875436743934786450097*53645235643259824350824580457283955438957043287250857432895703498700987123454567897656:?T)&!T
NEWMULT
{!} 3072046909146355923036506564192345471346475055611123765430367260576556764424411699428134904701221896786418686608674094452972067252677279867454597742488128986716908647272632
    S   3,41 sec  (1437.1457.2)
!NEWMULT
{!} 3072046909146355923036506564192345471346475055611123765430367260576556764424411699428134904701221896786418686608674094452972067252677279867454597742488128986716908647272632
    S   26,60 sec  (1437.1453.2)

0:?n&whl'(1+!n:<1000000:?n&57265978465627876873875436743934786450097*53645235643259824350824987123454567897656:?T)&!T
NEWMULT
{!} 3072046909130450126528027450054726559442406960607487886384944156986716908647272632
    S   18,53 sec  (1437.1457.2)!NEWMULT
{!} 3072046909130450126528027450054726559442406960607487886384944156986716908647272632
    S   67,78 sec  (1437.1456.2)

0:?n&whl'(1+!n:<1000000:?n&75436743934786450097*53645235643259824350:?T)&!T
NEWMULT
{!} 4046821904541870443295997539156260461950
    S   10,52 sec  (1437.1457.2)
!NEWMULT
{!} 4046821904541870443295997539156260461950
    S   17,06 sec  (1437.1453.2)

0:?n&whl'(1+!n:<1000000:?n&4350*2073384975284367439375369802:?T)&!T
NEWMULT
{!} 9019224642486998361282858638700
    S   8,93 sec  (1437.1456.2)
!NEWMULT
{!} 9019224642486998361282858638700
    S   5,59 sec  (1437.1456.2)

0:?n&whl'(1+!n:<1000000:?n&43500273*384975284367439375369802:?T)&!T
NEWMULT
{!} 16746529968236245139535862955946
    S   9,51 sec  (1437.1456.2)
!NEWMULT
{!} 16746529968236245139535862955946
    S   8,53 sec  (1437.1456.2)

0:?n&whl'(1+!n:<1000000:?n&384975284367439375369802*43500273:?T)&!T
NEWMULT
{!} 16746529968236245139535862955946
    S   9,12 sec  (1437.1456.2)
!NEWMULT
{!} 16746529968236245139535862955946
    S   8,10 sec  (1437.1456.2)

0:?n&whl'(1+!n:<1000000:?n&38497528436743937536*980243500273:?T)&!T
NEWMULT
{!} 37736952026693231197301110947328
    S   9,38 sec  (1437.1456.2)
!NEWMULT
{!} 37736952026693231197301110947328
    S   8,80 sec  (1437.1454.2)
    S   9,95 sec  (1437.1456.2)
    S   9,58 sec  (1437.1453.2)

0:?n&whl'(1+!n:<1000000:?n&3849752843674393*7536980243500273:?T)&!T
NEWMULT
{!} 29015511125132894970381018609289
    S   9,60 sec  (1437.1454.2)
!NEWMULT
{!} 29015511125132894970381018609289
    S   10,86 sec  (1437.1453.2)

0:?n&whl'(1+!n:<10000000:?n&752843674393*753698024350:?T)&!T
NEWMULT
{!} 567416790034398785469550
    S   71,22 sec  (1437.1457.2)
!NEWMULT
{!} 567416790034398785469550
    S   66,63 sec  (1437.1453.2)

0:?n&whl'(1+!n:<10000000:?n&7543674393*5369824350:?T)&!T
NEWMULT
{!} 40508206444002869550
    S   62,38 sec  (1437.1456.2)
!NEWMULT
{!} 40508206444002869550
    S   51,77 sec  (1437.1463.2)

*/
#if WORD32
#define RADIX 10000L
#define TEN_LOG_RADIX 4L
#define HEADROOM 20L
#else
#if defined _WIN64
#define RADIX 100000000LL
#define HEADROOM 800LL
#else
#define RADIX 100000000L
#define HEADROOM 800L
#endif
#define TEN_LOG_RADIX 8L
#endif

#define RADIX2 (RADIX * RADIX)

typedef struct nnumber
    {
    ptrdiff_t length;
    ptrdiff_t ilength;
    void* alloc;
    void* ialloc;
    LONG* inumber;
    char* number;
    size_t allocated;
    size_t iallocated;
    int sign; /* 0: positive, QNUL: zero, MINUS: negative number */
    } nnumber;

#define NNUMBERIS1(x) ((x)->sign == 0 && (x)->length == 1 && ((char*)((x)->number))[0] == '1')

#define Qnumber psk

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

static int ARGC = 0;
static char** ARGV = NULL;

typedef struct knode
    {
    tFlags v;
    psk left, right;
    } knode;

#if WORD32
#define BUILTIN (1 << 30)
typedef struct objectnode /* createdWithNew == 0 */
    {
    tFlags v;
    psk left, right;
    union
        {
        struct
            {
            unsigned int refcount : 30;
            unsigned int built_in : 1;
            unsigned int createdWithNew : 1;
            } s;
        int Int : 32;
        } u;
    } objectnode;
#else
typedef struct objectnode /* createdWithNew == 0 */
    {
    tFlags v;
    psk left, right;
    } objectnode;
#endif

typedef struct stringrefnode
    {
    tFlags v;
    psk pnode;
    char* str;
    size_t length;
    } stringrefnode;

typedef psk function_return_type;

#define functionFail(x) ((x)->v.fl ^= SUCCESS,(x))
#define functionOk(x) (x)

struct typedObjectnode;
typedef Boolean(*method_pnt)(struct typedObjectnode* This, ppsk arg);

typedef struct method
    {
    char* name;
    method_pnt func;
    } method;

struct Hash;

#if WORD32
typedef struct typedObjectnode /* createdWithNew == 1 */
    {
    tFlags v;
    psk left, right; /* left == nil, right == data (if vtab == NULL)
            or name of object type, e.g. [set], [hash], [file], [float] (if vtab != NULL)*/
    union
        {
        struct
            {
            unsigned int refcount : 30;
            unsigned int built_in : 1;
            unsigned int createdWithNew : 1;
            } s;
        int Int : 32;
        } u;
    struct Hash* voiddata;
#define HASH(x) (Hash*)x->voiddata
#define VOID(x) x->voiddata
#define PHASH(x) (Hash**)&(x->voiddata)
    method* vtab; /* The last element n of the array must have vtab[n].name == NULL */
    } typedObjectnode;
#else
typedef struct typedObjectnode /* createdWithNew == 1 */
    {
    tFlags v;
    psk left, right; /* left == nil, right == data (if vtab == NULL)
            or name of object type, e.g. [set], [hash], [file], [float] (if vtab != NULL)*/
    struct Hash* voiddata;
#define HASH(x) (Hash*)x->voiddata
#define VOID(x) x->voiddata
#define PHASH(x) (Hash**)&(x->voiddata)
    method* vtab; /* The last element n of the array must have vtab[n].name == NULL */
    } typedObjectnode;
#endif

#if WORD32
#define INCREFCOUNT(a) { ((objectnode*)a)->u.s.refcount++;(a)->v.fl &= ((~ALL_REFCOUNT_BITS_SET)|ONEREF); }
#define DECREFCOUNT(a) { ((objectnode*)a)->u.s.refcount--;(a)->v.fl |= ALL_REFCOUNT_BITS_SET; }
#define REFCOUNTNONZERO(a) ((a)->u.s.refcount)
#define ISBUILTIN(a) ((a)->u.s.built_in)
#define ISCREATEDWITHNEW(a) ((a)->u.s.createdWithNew)
#define SETCREATEDWITHNEW(a) (a)->u.s.createdWithNew = 1
#else
#define ISBUILTIN(a) ((a)->v.fl & BUILT_IN)
#define ISCREATEDWITHNEW(a) ((a)->v.fl & CREATEDWITHNEW)
#define SETCREATEDWITHNEW(a) (a)->v.fl |= CREATEDWITHNEW
#endif

/*#if !_BRACMATEMBEDDED*/
#if !defined NO_FOPEN
static char* errorFileName = NULL;
#endif
static FILE* errorStream = NULL;
/*#endif*/

#if !defined NO_FOPEN
enum { NoPending, Writing, Reading };
typedef struct fileStatus
    {
    char* fname;
    FILE* fp;
    struct fileStatus* next;
#if !defined NO_LOW_LEVEL_FILE_HANDLING
    Boolean dontcloseme;
    LONG filepos; /* Normally -1. If >= 0, then the file is closed.
                When reopening, filepos is used to find the position
                before the file was closed. */
    LONG mode;
    LONG type;
    LONG size;
    LONG number;
    LONG time;
    int rwstatus;
    char* stop; /* contains characters to stop reading at, default NULL */
#endif
    } fileStatus;

static fileStatus* fs0 = NULL;
#endif

typedef LONG refCountType;

typedef struct indexType
    {
    LONG offset;
    refCountType refCnt;
    size_t size;
    } indexType;

typedef struct handleType
    {
    LONG number;
    char* fileName;
    } handleType;

typedef struct objectType
    {
    handleType handle;
    refCountType refCnt;
    LONG offset;
    psk obj;
    LONG size;
    } objectType;

#define NOT_STORED -1L

typedef struct freeStoreType
    {
    LONG nextFree;
    LONG size;
    } freeStoreType;

/*          operator              leaf                optab       comment
Flgs 0                   NOT
     1                  SUCCESS
     2                  READY
     3                  POSITION
     4                 INDIRECT
     5              DOUBLY_INDIRECT
     6                  FENCE
     7                  ATOM
     8                 NONIDENT
     9                GREATER_THAN
    10                SMALLER_THAN
    11                  NUMBER
    12                  FRACTION
    13                  UNIFY
    14                  IDENT
    15               IMPLIEDFENCE
    16               IS_OPERATOR                                  SHL
    17      (operators 0-14)      QNUMBER
    18          "                 MINUS
    19          "                 QNUL
    20          "                 QFRACTION
    21                LATEBIND                        NOOP
    22               SELFMATCHING                                Toggles with DATAMATCHESITSELF
    23                 BUILT_IN                                  ONLY for 64 bit platform
    24              CREATEDWITHNEW                               ONLY for 64 bit platform
    25             (reference count)                             NON_REF_COUNT_BITS 25 or 23
    26                    "
    27                    "
    28                    "
    29                    "
    30                    "
    31                    "

Reference count starts with 0, not 1
*/

#define SHL 16
#define REF 23
#define OPSH (SHL+1)
#define IS_OPERATOR (1 << SHL)
#define EQUALS     (( 0<<OPSH) + IS_OPERATOR)
#define DOT        (( 1<<OPSH) + IS_OPERATOR)
#define COMMA      (( 2<<OPSH) + IS_OPERATOR)
#define OR         (( 3<<OPSH) + IS_OPERATOR)
#define AND        (( 4<<OPSH) + IS_OPERATOR)
#define MATCH      (( 5<<OPSH) + IS_OPERATOR)
#define WHITE      (( 6<<OPSH) + IS_OPERATOR)
#define PLUS       (( 7<<OPSH) + IS_OPERATOR)
#define TIMES      (( 8<<OPSH) + IS_OPERATOR)
#define EXP        (( 9<<OPSH) + IS_OPERATOR)
#define LOG        ((10<<OPSH) + IS_OPERATOR)
#define DIF        ((11<<OPSH) + IS_OPERATOR)
#define FUU        ((12<<OPSH) + IS_OPERATOR)
#define FUN        ((13<<OPSH) + IS_OPERATOR)
#define UNDERSCORE ((14<<OPSH) + IS_OPERATOR) /* dummy */

static const psk knil[16] =
    { NULL,NULL,NULL,NULL,NULL,NULL,&nilNode,&zeroNode,
    &oneNode,NULL,NULL,NULL,NULL,NULL,NULL,NULL };

static const char opchar[16] =
    { '=','.',',','|','&',':',' ','+','*','^','\016','\017','\'','$','_','?' };

#define OPERATOR ((0xF<<OPSH) + IS_OPERATOR)

#define Op(pn) ((pn)->v.fl & OPERATOR)
#define kopo(pn) ((pn).v.fl & OPERATOR)
#define is_op(pn) ((pn)->v.fl & IS_OPERATOR)
#define is_object(pn) (((pn)->v.fl & OPERATOR) == EQUALS)
#define klopcode(pn) (Op(pn) >> OPSH)

#define nil(p) knil[klopcode(p)]


#define NOOP                (OPERATOR+1)
#define QNUMBER             (1 << (SHL+1))
#define MINUS               (1 << (SHL+2))
#define QNUL                (1 << (SHL+3))
#define QFRACTION           (1 << (SHL+4))
#define LATEBIND            (1 << (SHL+5))
#define DEFINITELYNONUMBER  (1 << (SHL+6)) /* this is not stored in a node! */
#define ONEREF   (ULONG)(1 << NON_REF_COUNT_BITS)

#define ALL_REFCOUNT_BITS_SET \
       ((((ULONG)(~0)) >> NON_REF_COUNT_BITS) << NON_REF_COUNT_BITS)
#if WORD32
#define COPYFILTER ~ALL_REFCOUNT_BITS_SET
#else
#define COPYFILTER ~(ALL_REFCOUNT_BITS_SET | BUILT_IN | CREATEDWITHNEW)
#endif

#define shared(pn) ((pn)->v.fl & ALL_REFCOUNT_BITS_SET)
#define currRefCount(pn) (((pn)->v.fl & ALL_REFCOUNT_BITS_SET) >> NON_REF_COUNT_BITS)

static int all_refcount_bits_set(psk pnode)
    {
    return (shared(pnode) == ALL_REFCOUNT_BITS_SET) && !is_object(pnode);
    }

static void dec_refcount(psk pnode)
    {
    assert(pnode->v.fl & ALL_REFCOUNT_BITS_SET);
    pnode->v.fl -= ONEREF;
#if WORD32
    if((pnode->v.fl & (OPERATOR | ALL_REFCOUNT_BITS_SET)) == EQUALS)
        {
        if(REFCOUNTNONZERO((objectnode*)pnode))
            {
            DECREFCOUNT(pnode);
            }
        }
#endif
    }

#if TELMAX
#if TELLING
void initcnts(void)
    {
    for(int tel = 0; tel < sizeof(cnts) / sizeof(cnts[0]); ++tel)
        cnts[tel] = 0;
    }
#endif

static size_t globalloc = 0, maxgloballoc = 0;

void Bez(char draft[22])
    {
#if MAXSTACK
#if defined _WIN32 || defined __VMS
    sprintf(draft, "%lu.%lu.%d", (unsigned long)globalloc, (unsigned long)maxgloballoc, maxstack);
#else
    sprintf(draft, "%zu.%zu.%d", globalloc, maxgloballoc, maxstack);
#endif
#else
#if defined _WIN32 || defined __VMS
    sprintf(draft, "%lu.%lu", (unsigned long)globalloc, (unsigned long)maxgloballoc);
#else
    sprintf(draft, "%zu.%zu", globalloc, maxgloballoc);
#endif
#endif
    }
#endif


#define STRING    1
#define VAPORIZED 2
#define MEMORY    4
#define ECHO      8

#include <stdarg.h>

#if defined __EMSCRIPTEN__ /* This is set if compiling with __EMSCRIPTEN__. */
#define EMSCRIPTEN_HTML 1 /* set to 1 if using __EMSCRIPTEN__ to convert this file to HTML*/
#define GLOBALARGPTR 0

#define NO_C_INTERFACE
#define NO_FILE_RENAME
#define NO_FILE_REMOVE
#define NO_SYSTEM_CALL
#define NO_LOW_LEVEL_FILE_HANDLING
#define NO_FOPEN
#define NO_EXIT_ON_NON_SEVERE_ERRORS

#else
#define EMSCRIPTEN_HTML 0 /* Normally this should be 0 */
#define GLOBALARGPTR 1 /* Normally this should be 1 */
#endif

#if EMSCRIPTEN_HTML
/* must be 0: compiling with emcc (__EMSCRIPTEN__ C to JavaScript compiler) */
#else
#define GLOBALARGPTR 1 /* 0 if compiling with emcc (__EMSCRIPTEN__ C to JavaScript compiler) */
#endif

#if GLOBALARGPTR
static va_list argptr;
#define VOIDORARGPTR void
#else
#define VOIDORARGPTR va_list * pargptr
#endif
static unsigned char* startPos;

static const char
hash5[] = "\5",
hash6[] = "\6",
unbalanced[] =
"unbalanced",

fct[] = "(fct=f G T P C V I B W H J O.(T=m Z a p r R Q.!arg:(?m.?Z)&0:?R:?Q&"
"whl'(!Z:?a+?p*!m*?r+?Z&!R+!a:?R&!Q+!p*!r:?Q)&(!Q.!R+!Z))&(P=M E.!arg:(?M.?E)&"
"whl'(!E:?*(!M|!M^((#%:~<1)+?))*?+?E)&!E:0)&(G=f e r a.!arg:(?e.?f)&0:?r&whl'("
"!e:%?a+?e&!a*!f:?a&!a+!r:?r)&!r)&(C=f r A Z M.!arg:%+%:(?+?*(?M^((#<%0:?f)+?r"
")&!M^!f:?M)*?+?|?+?*(?M^(#>%1+?)&P$(!M.!arg))*?+?|%?f+?r&!f:?A*~#%?`M*(?Z&P$("
"!M.!r)))&!M*C$(G$(!arg.!M^-1))|!arg)&(W=n A Z M s.C$!arg:?arg:?A*((~-1:#%?n)*"
"?+?:?M)*?Z&(!n:<0&-1|1):?s&!s*!n*!A*(1+!s*!n^-1*!M+-1)*!Z|!arg)&(V=n A Z M.C$"
"!arg:?arg:?A*(#%?n*?+?:?M)*?Z&!n*!A*(1+!n^-1*!M+-1)*!Z|!arg)&(I=f v l r.!arg:"
"(?f.?v)&!v:?l_?r&I$(!f.!l)&I$(!f.!r)|!v:#|!v\017!f:0)&(O=a f e.!arg:?a*?f^(%*"
"%:?e)*?arg&!a*!f^J$(1+!e+-1)*O$!arg|!arg)&(J=t.!arg:%?t+%?arg&O$!t+(!arg:%+%&"
"J|O)$!arg|!arg)&(f=L R A Z a m z S Q r q t F h N D.(D=R Q S t x r X ax zx M."
"!arg:(?R.?Q)&!Q:?+%`(?*(~#%?`M&T$(!M.!Q):(?x.?r)&I$(!r.!M))*?)+?&N$!x:?x&sub$"
"(!R.!M.(VAR+-1*!r)*!x^-1):?S&(!x:?ax*(%+%:?X)*?zx&T$(!X^-1.!S):(?t.?S)&1+!ax*"
"!zx*(!S*!X+!t)+-1:?S|1:?x)&T$(VAR.!S):(?t.0)&N$!t*!x^-1)&!arg:(?arg.(=?N))&N$"
"!arg:?arg&(!arg:%?L*%?R&f$(!L.'$N)*f$(!R.'$N)|!arg:%?L^%?R&f$(!L.!R:~/#&'$V|'"
"$W)^J$(1+!R+-1)|J$!arg:?arg:#?+~#%?A+%?Z&!A:?a*~#%?`m*?z&T$(!m.!Z):(~0:?Q.?)&"
"!a*!z+!Q:?t:?r&!arg:?S&1:?Q&1:?F&whl'(!r:%?q*?r&N$!q:?h*(%+%:?q)&D$(!S.!q):?S"
"&!h*!F:?F&!Q*!q:?Q)&!Q+-1*!F^-1*!t:0&f$(!Q.'$N)*f$(!S.'$N)|!arg))&(B=A E M Z "
"a b e m n y z.!arg:?A*%?`M^?E*(?Z&!A*!Z:?a*%?`m^?e*(?z&!a*!z:?b*?n^!e*?y&!M+"
"!m:0))&B$(!b*(1+-1*!n+-1)^!e*!y*!M^(!E+!e))|!arg)&(H=A Z a b e z w x n m o."
"!arg:?A*(%?b+%?z:?m)*(?Z&!b:?*?a^%?e*(?&!z:?w&whl'(!w:?*!a^?*?+?w)&!w:0&!e:?+"
"#?n*(~#%@*?:?x)+(?&!z:?w&whl'(!w:?*!a^(?+#?o*(!x&(!o:<!n:?n|))+?)*?+?w)&!w:0)"
"))&fct$(!A*!a^(!n*!x)*(1+!a^(-1*!n*!x)*!m+-1)*!Z)|!arg)&H$(B$(f$(!arg.'$V))))";

static size_t telling = 0;

#if TELLING
static size_t cnts[256], alloc_cnt = 0, totcnt = 0;
#endif

/*
After running valid.bra  on 32 bit platform
1     8           32
2 16384       131072
3 32696       392352
4  1024        16384
5  256          5120
6 2048         49152
              ------+
total bytes = 594112
*/
#if WORD32
#define MEM1SIZE 8
#define MEM2SIZE 16384
#define MEM3SIZE 32696
#define MEM4SIZE 1024
#define MEM5SIZE 256
#define MEM6SIZE 2048
#else
/*
After running valid.bra  on 64 bit platform
1     8            64
2 16384        262144
3 32768        786432
4  1024         32768
5  2048         81920
6    64          3072
              -------+
total bytes = 1166400
*/
#define MEM1SIZE 8
#define MEM2SIZE 16384
#define MEM3SIZE 32768
#define MEM4SIZE 1024
#define MEM5SIZE 2048
#define MEM6SIZE 64
#endif

static int hum = 1;
static int listWithName = 1;
static int beNice = TRUE;
static int optab[256];
static int dummy_op = WHITE;
#if DEBUGBRACMAT
static int debug = 0;
#endif

static FILE* global_fpi;
static FILE* global_fpo;

static const char needsquotes[256] = {
    /*
       1 : needsquotes if first character;
       3 : needsquotes always
       4 : needsquotes if \t and \n must be expanded
    */
    0,0,0,0,0,0,0,0,0,4,4,0,0,0,3,3, /* \L \D */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    3,1,0,1,3,1,3,3,3,3,3,3,3,1,3,1, /* SP ! # $ % & ' ( ) * + , - . / */
    0,0,0,0,0,0,0,0,0,0,3,3,1,3,1,1, /* : < = > ? */
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* @ */
    0,0,0,0,0,0,0,0,0,0,0,1,3,1,3,3, /* [ \ ] ^ _ */
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ` */
    0,0,0,0,0,0,0,0,0,0,0,3,3,3,1,0 };/* { | } ~ */


/* ISO8859 */ /* NOT DOS compatible! */
#if 0
static const unsigned char lowerEquivalent[256] =
    {
          0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,
         16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
        ' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
        '@', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
        'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '[', '\\', ']', '^', '_',
        '`', 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n', 'o',
        'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', '{', '|', '}', '~', 127,
        128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
        144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
        160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
        176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
        224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
        240, 241, 242, 243, 244, 245, 246, 215, 248, 249, 250, 251, 252, 253, 254, 223 /*ringel s*/,
        224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
        240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254, 255
    };

static const unsigned char upperEquivalent[256] =
    {
          0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,
         16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,
        ' ', '!', '"', '#', '$', '%', '&', '\'', '(', ')', '*', '+', ',', '-', '.', '/',
        '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>', '?',
        '@', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
        'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '[', '\\', ']', '^', '_',
        '`', 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O',
        'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', '{', '|', '}', '~', 127,
        128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
        144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159,
        160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175,
        176, 177, 178, 179, 180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191,
        192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
        208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223,
        192, 193, 194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207,
        208, 209, 210, 211, 212, 213, 214, 247, 216, 217, 218, 219, 220, 221, 222, 255 /* ij */
    };
#endif
#if CODEPAGE850
static unsigned char ISO8859toCodePage850(unsigned char kar)

    {
    static unsigned char translationTable[] =
        {
        0xBA,0xCD,0xC9,0xBB,0xC8,0xBC,0xCC,0xB9,0xCB,0xCA,0xCE,0xDF,0xDC,0xDB,0xFE,0xF2,
        0xB3,0xC4,0xDA,0xBF,0xC0,0xD9,0xC3,0xB4,0xC2,0xC1,0xC5,0xB0,0xB1,0xB2,0xD5,0x9F,
        0xFF,0xAD,0xBD,0x9C,0xCF,0xBE,0xDD,0xF5,0xF9,0xB8,0xA6,0xAE,0xAA,0xF0,0xA9,0xEE,
        0xF8,0xF1,0xFD,0xFC,0xEF,0xE6,0xF4,0xFA,0xF7,0xFB,0xA7,0xAF,0xAC,0xAB,0xF3,0xA8,
        0xB7,0xB5,0xB6,0xC7,0x8E,0x8F,0x92,0x80,0xD4,0x90,0xD2,0xD3,0xDE,0xD6,0xD7,0xD8,
        0xD1,0xA5,0xE3,0xE0,0xE2,0xE5,0x99,0x9E,0x9D,0xEB,0xE9,0xEA,0x9A,0xED,0xE8,0xE1,
        0x85,0xA0,0x83,0xC6,0x84,0x86,0x91,0x87,0x8A,0x82,0x88,0x89,0x8D,0xA1,0x8C,0x8B,
        0xD0,0xA4,0x95,0xA2,0x93,0xE4,0x94,0xF6,0x9B,0x97,0xA3,0x96,0x81,0xEC,0xE7,0x98
        };

    if(kar & 0x80)
        return translationTable[kar & 0x7F];
    else
        return kar;
    /*    return kar & 0x80 ? (unsigned char)translationTable[kar & 0x7F] : kar;*/
    }

static unsigned char CodePage850toISO8859(unsigned char kar)
    {
    static unsigned char translationTable[] =
        {
        0xC7,0xFC,0xE9,0xE2,0xE4,0xE0,0xE5,0xE7,0xEA,0xEB,0xE8,0xEF,0xEE,0xEC,0xC4,0xC5,
        0xC9,0xE6,0xC6,0xF4,0xF6,0xF2,0xFB,0xF9,0xFF,0xD6,0xDC,0xF8,0xA3,0xD8,0xD7,0x9F,
        0xE1,0xED,0xF3,0xFA,0xF1,0xD1,0xAA,0xBA,0xBF,0xAE,0xAC,0xBD,0xBC,0xA1,0xAB,0xBB,
        0x9B,0x9C,0x9D,0x90,0x97,0xC1,0xC2,0xC0,0xA9,0x87,0x80,0x83,0x85,0xA2,0xA5,0x93,
        0x94,0x99,0x98,0x96,0x91,0x9A,0xE3,0xC3,0x84,0x82,0x89,0x88,0x86,0x81,0x8A,0xA4,
        0xF0,0xD0,0xCA,0xCB,0xC8,0x9E,0xCD,0xCE,0xCF,0x95,0x92,0x8D,0x8C,0xA6,0xCC,0x8B,
        0xD3,0xDF,0xD4,0xD2,0xF5,0xD5,0xB5,0xFE,0xDE,0xDA,0xDB,0xD9,0xFD,0xDD,0xAF,0xB4,
        0xAD,0xB1,0x8F,0xBE,0xB6,0xA7,0xF7,0xB8,0xB0,0xA8,0xB7,0xB9,0xB3,0xB2,0x8E,0xA0,
        };

    /* 0x7F = 127, 0xFF = 255 */
    /* delete bit-7 before search in tabel (0-6 is unchanged) */
    /* delete bit 15-8 */

    if(kar & 0x80)
        return translationTable[kar & 0x7F];
    else
        return kar;
    /*    return kar & 0x80 ? (unsigned char)translationTable[kar & 0x7F] : kar;*/
    }
#endif



#ifdef DELAY_DUE_TO_INPUT
static clock_t delayDueToInput = 0;
#endif

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

#ifndef UNREFERENCED_PARAMETER
#if defined _MSC_VER
#define UNREFERENCED_PARAMETER(P) (P)
#else
#define UNREFERENCED_PARAMETER(P)
#endif
#endif

static psk global_anchor;

typedef struct inputBuffer
    {
    unsigned char* buffer;
    unsigned int cutoff : 1;    /* Set to true if very long string does not fit in buffer of size DEFAULT_INPUT_BUFFER_SIZE */
    unsigned int mallocallocated : 1; /* True if allocated with malloc. Otherwise on stack (except EPOC). */
    } inputBuffer;

static inputBuffer* InputArray;
static inputBuffer* InputElement; /* Points to member of InputArray */
static unsigned char* start, ** pstart, * source;
static unsigned char* inputBufferPointer;
static unsigned char* maxInputBufferPointer; /* inputBufferPointer <= maxInputBufferPointer,
                            if inputBufferPointer == maxInputBufferPointer, don't assign to *inputBufferPointer */


                            /* FUNCTIONS */

static void parenthesised_result(psk Root, int level, int ind, int space);
#if DEBUGBRACMAT
static void hreslts(psk Root, int level, int ind, int space, psk cutoff);
#endif

static psk eval(psk Pnode);
#define evaluate(x) \
(    ( ((x) = eval(x))->v.fl \
     & SUCCESS\
     ) \
   ? TRUE\
   : ((x)->v.fl & FENCE)\
)

#define isSUCCESS(x) ((x)->v.fl & SUCCESS)
#define isFENCE(x) (((x)->v.fl & (SUCCESS|FENCE)) == FENCE)
#define isSUCCESSorFENCE(x) ((x)->v.fl & (SUCCESS|FENCE))
#define isFailed(x) (((x)->v.fl & (SUCCESS|FENCE)) == 0)

static psk subtreecopy(psk src);

#if _BRACMATEMBEDDED

static int(*WinIn)(void) = NULL;
static void(*WinOut)(int c) = NULL;
static void(*WinFlush)(void) = NULL;
#if defined PYTHONINTERFACE
static void(*Ni)(const char*) = NULL;
static const char* (*Nii)(const char*) = NULL;
#endif
static int mygetc(FILE* fpi)
    {
    if(WinIn && fpi == stdin)
        {
        return WinIn();
        }
    else
        return fgetc(fpi);
    }


static void myputc(int c)
    {
    if(WinOut && (global_fpo == stdout || global_fpo == stderr))
        {
        WinOut(c);
        }
    else
        fputc(c, global_fpo);
    }
#else
static void myputc(int c)
    {
    fputc(c, global_fpo);
    }

static int mygetc(FILE* fpi)
    {
#ifdef __SYMBIAN32__
    if(fpi == stdin)
        {
        static unsigned char inputbuffer[256] = { 0 };
        static unsigned char* out = inputbuffer;
        if(!*out)
            {
            static unsigned char* in = inputbuffer;
            static int kar;
            while(in < inputbuffer + sizeof(inputbuffer) - 2
                  && (kar = fgetc(fpi)) != '\n'
                  )
                {
                switch(kar)
                    {
                    case '\r':
                        break;
                    case 8:
                        if(in > inputbuffer)
                            {
                            --in;
                            putchar(' ');
                            putchar(8);
                            }
                        break;
                    default:
                        *in++ = kar;
                    }
                }
            *in = kar;
            *++in = '\0';
            out = in = inputbuffer;
            }
        return *out++;
        }
#endif
    return fgetc(fpi);
    }
#endif

static void(*process)(int c) = myputc;

static void myprintf(char* strng, ...)
    {
    char* i, * j;
    va_list ap;
    va_start(ap, strng);
    i = strng;
    while(i)
        {
        for(j = i; *j; j++)
            (*process)(*j);
        i = va_arg(ap, char*);
        }
    va_end(ap);
    }


#if _BRACMATEMBEDDED && !defined _MT
static int Printf(const char* fmt, ...)
    {
    char buffer[1000];
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = vsprintf(buffer, fmt, ap);
    myprintf(buffer, NULL);
    va_end(ap);
    return ret;
    }
#else
#define Printf printf
#endif

static int errorprintf(const char* fmt, ...)
    {
    char buffer[1000];
    int ret;
    FILE* save = global_fpo;
    va_list ap;
    va_start(ap, fmt);
    ret = vsprintf(buffer, fmt, ap);
    global_fpo = errorStream;
#if !defined NO_FOPEN
    if(global_fpo == NULL && errorFileName != NULL)
        global_fpo = fopen(errorFileName, APPENDTXT);
#endif
    /*#endif*/
    if(global_fpo)
        myprintf(buffer, NULL);
    else
        ret = 0;
    /*#if !_BRACMATEMBEDDED*/
#if !defined NO_FOPEN
    if(errorStream == NULL && global_fpo != NULL)
        fclose(global_fpo);
#endif
    /*#endif*/
    global_fpo = save;
    va_end(ap);
    return ret;
    }

struct memblock
    {
    struct memoryElement* lowestAddress;
    struct memoryElement* highestAddress;
    struct memoryElement* firstFreeElementBetweenAddresses; /* if NULL : no more free elements */
    struct memblock* previousOfSameLength; /* address of older ands smaller block with same sized elements */
    size_t sizeOfElement;
#if TELMAX
    size_t numberOfFreeElementsBetweenAddresses; /* optional field. */
    size_t numberOfElementsBetweenAddresses; /* optional field. */
    size_t minimumNumberOfFreeElementsBetweenAddresses;
#endif
    };

struct allocation
    {
    size_t elementSize;
    int numberOfElements;
    struct memblock* memoryBlock;
    };

static struct allocation* global_allocations;
static int global_nallocations = 0;


struct memoryElement
    {
    struct memoryElement* next;
    };
/*
struct pointerStruct
    {
    struct pointerStruct * lp;
    } *global_p, *global_ep;
*/
struct memblock** pMemBlocks = 0; /* list of memblock, sorted
                                      according to memory address */
static int NumberOfMemBlocks = 0;
#if TELMAX
static int malloced = 0;
#endif

#if DOSUMCHECK

static int LineNo;
static int globN;

static int getchecksum(void)
    {
    int i;
    int sum = 0;
    for(i = 0; i < NumberOfMemBlocks; ++i)
        {
        struct memoryElement* me;
        struct memblock* mb = pMemBlocks[i];
        me = mb->firstFreeElementBetweenAddresses;
        while(me)
            {
            sum += (LONG)me;
            me = me->next;
            }
        }
    return sum;
    }

static int Checksum = 0;

static void setChecksum(int lineno, int n)
    {
    if(lineno)
        {
        LineNo = lineno;
        globN = n;
        }
    Checksum = getchecksum();
    }

static void checksum(int line)
    {
    static int nChecksum = 0;
    nChecksum = getchecksum();
    if(Checksum && Checksum != nChecksum)
        {
        Printf("Line %d: Illegal write after bmalloc(%d) on line %d", line, globN, LineNo);
        getchar();
        exit(1);
        }
    }
#else
#define setChecksum(a,b)
#define bmalloc(LINENO,N) bmalloc(N)
#define checksum(a)
#endif

#if CHECKALLOCBOUNDS
static int isFree(void* p)
    {
    LONG* q;
    int i;
    struct memoryElement* I;
    q = (LONG*)p - 2;
    I = (struct memoryElement*)q;
    for(i = 0; i < NumberOfMemBlocks; ++i)
        {
        struct memoryElement* me;
        struct memblock* mb = pMemBlocks[i];
        me = (struct memoryElement*)mb->firstFreeElementBetweenAddresses;
        while(me)
            {
            if(I == me)
                return 1;
            me = (struct memoryElement*)me->next;
            }
        }
    return 0;
    }

static void result(psk Root);
static int rfree(psk p)
    {
    int r = 0;
    if(isFree(p))
        {
        printf(" [");
        result(p);
        printf("] ");
        r = 1;
        }
    if(is_op(p))
        {
        r |= rfree(p->LEFT);
        r |= rfree(p->RIGHT);
        }
    return r;
    }

static int POINT = 0;

static int areFree(char* t, psk p)
    {
    if(rfree(p))
        {
        POINT = 1;
        printf("%s:areFree(", t);
        result(p);
        POINT = 0;
        printf("\n");
        return 1;
        }
    return 0;
    }

static void checkMem(void* p)
    {
    LONG* q;
    q = (LONG*)p - 2;
    if(q[0] == ('s' << 24) + ('t' << 16) + ('a' << 8) + ('r')
       && q[q[1]] == ('t' << 24) + ('e' << 16) + ('n' << 8) + ('d')
       )
        {
        ;
        }
    else
        {
        char* s = (char*)q;
        printf("s:[");
        for(; s < (char*)(q + q[1] + 1); ++s)
            {
            if((((int)s) % 4) == 0)
                printf("|");
            if(' ' <= *s && *s <= 127)
                printf(" %c", *s);
            else
                printf("%.2x", (int)((unsigned char)*s));
            }
        printf("] %p\n", p);
        }
    assert(q[0] == ('s' << 24) + ('t' << 16) + ('a' << 8) + ('r'));
    assert(q[q[1]] == ('t' << 24) + ('e' << 16) + ('n' << 8) + ('d'));
    }


static void checkBounds(void* p)
    {
    struct memblock** q;
    LONG* lp = (LONG*)p;
    assert(p != 0);
    checkMem(p);
    lp = lp - 2;
    p = lp;
    assert(lp[0] == ('s' << 24) + ('t' << 16) + ('a' << 8) + ('r'));
    assert(lp[lp[1]] == ('t' << 24) + ('e' << 16) + ('n' << 8) + ('d'));
    for(q = pMemBlocks + NumberOfMemBlocks; --q >= pMemBlocks;)
        {
        size_t stepSize = (*q)->sizeOfElement / sizeof(struct memoryElement);
        if((*q)->lowestAddress <= (struct memoryElement*)p && (struct memoryElement*)p < (*q)->highestAddress)
            {
            assert(lp[stepSize - 1] == ('t' << 24) + ('e' << 16) + ('n' << 8) + ('d'));
            return;
            }
        }
    }

static void checkAllBounds()
    {
    struct memblock** q;
    for(q = pMemBlocks + NumberOfMemBlocks; --q >= pMemBlocks;)
        {
        size_t stepSize = (*q)->sizeOfElement / sizeof(struct memoryElement);

        struct memoryElement* p = (struct memoryElement*)(*q)->lowestAddress;
        struct memoryElement* e = (struct memoryElement*)(*q)->highestAddress;
        size_t L = (*q)->sizeOfElement - 1;
        struct memoryElement* x;
        for(x = p; x < e; x += stepSize)
            {
            struct memoryElement* a = ((struct memoryElement*)x)->next;
            if(a == 0 || (p <= a && a < e))
                ;
            else
                {
                if((((LONG*)x)[0] == ('s' << 24) + ('t' << 16) + ('a' << 8) + ('r'))
                   && (((LONG*)x)[stepSize - 1] == ('t' << 24) + ('e' << 16) + ('n' << 8) + ('d')))
                    ;
                else
                    {
                    char* s = (char*)x;
                    printf("s:[");
                    for(; s <= (char*)x + L; ++s)
                        if(' ' <= *s && *s <= 127)
                            printf("%c", *s);
                        else if(*s == 0)
                            printf("NIL");
                        else
                            printf("-%c", *s);
                    printf("] %p\n", x);
                    }
                assert(((LONG*)x)[0] == ('s' << 24) + ('t' << 16) + ('a' << 8) + ('r'));
                assert(((LONG*)x)[stepSize - 1] == ('t' << 24) + ('e' << 16) + ('n' << 8) + ('d'));
                }
            }
        }
    }
#endif

static void bfree(void* p)
    {
    struct memblock** q;
#if CHECKALLOCBOUNDS
    LONG* lp = (LONG*)p;
#endif
    assert(p != 0);
    checksum(__LINE__);
#if CHECKALLOCBOUNDS
    checkBounds(p);
    lp = lp - 2;
    p = lp;
#endif
#if TELMAX
    globalloc--;
#endif
    for(q = pMemBlocks + NumberOfMemBlocks; --q >= pMemBlocks;)
        {
        if((*q)->lowestAddress <= (struct memoryElement*)p && (struct memoryElement*)p < (*q)->highestAddress)
            {
#if TELMAX
            ++((*q)->numberOfFreeElementsBetweenAddresses);
#endif
            ((struct memoryElement*)p)->next = (*q)->firstFreeElementBetweenAddresses;
            (*q)->firstFreeElementBetweenAddresses = (struct memoryElement*)p;
            setChecksum(LineNo, globN);
            return;
            }
        }
    free(p);
#if TELMAX
    --malloced;
#endif
    setChecksum(LineNo, globN);
    }

#if TELLING
static void bezetting(void)
    {
    struct memblock* mb = 0;
    size_t words = 0;
    int i;
    Printf("\noccupied (promilles)\n");
    for(i = 0; i < NumberOfMemBlocks; ++i)
        {
        mb = pMemBlocks[i];
#if WORD32 || defined __VMS
        Printf("%zd word : %lu\n", mb->sizeOfElement / sizeof(struct memoryElement), 1000UL - (1000UL * mb->numberOfFreeElementsBetweenAddresses) / mb->numberOfElementsBetweenAddresses);
#else
        Printf("%zd word : %zu\n", mb->sizeOfElement / sizeof(struct memoryElement), 1000UL - (1000UL * mb->numberOfFreeElementsBetweenAddresses) / mb->numberOfElementsBetweenAddresses);
#endif
        }
    Printf("\nmax occupied (promilles)\n");
    for(i = 0; i < NumberOfMemBlocks; ++i)
        {
        mb = pMemBlocks[i];
#if WORD32 || defined __VMS
        Printf("%zd word : %lu\n", mb->sizeOfElement / sizeof(struct memoryElement), 1000UL - (1000UL * mb->minimumNumberOfFreeElementsBetweenAddresses) / mb->numberOfElementsBetweenAddresses);
#else
        Printf("%zd word : %zu\n", mb->sizeOfElement / sizeof(struct memoryElement), 1000UL - (1000UL * mb->minimumNumberOfFreeElementsBetweenAddresses) / mb->numberOfElementsBetweenAddresses);
#endif
        }
    Printf("\noccupied (absolute)\n");
    for(i = 0; i < NumberOfMemBlocks; ++i)
        {
        mb = pMemBlocks[i];
        words = mb->sizeOfElement / sizeof(struct memoryElement);
        Printf("%zd word : %zu\n", words, (mb->numberOfElementsBetweenAddresses - mb->numberOfFreeElementsBetweenAddresses));
        }
    Printf("more than %zd words : %u\n", words, malloced);
    }
#endif

static struct memblock* initializeMemBlock(size_t elementSize, size_t numberOfElements)
    {
    size_t nlongpointers;
    size_t stepSize;
    struct memblock* mb;
    struct memoryElement* mEa, * mEz;
    mb = (struct memblock*)malloc(sizeof(struct memblock));
    if(mb)
        {
        mb->sizeOfElement = elementSize;
        mb->previousOfSameLength = 0;
        stepSize = elementSize / sizeof(struct memoryElement);
        nlongpointers = stepSize * numberOfElements;
        mb->firstFreeElementBetweenAddresses = mb->lowestAddress = malloc(elementSize * numberOfElements);
        if(mb->lowestAddress == 0)
            {
#if _BRACMATEMBEDDED
            return 0;
#else
            exit(-1);
#endif
            }
        else
            {
#if TELMAX
            mb->numberOfElementsBetweenAddresses = numberOfElements;
#endif
            mEa = mb->lowestAddress;
            mb->highestAddress = mEa + nlongpointers;
#if TELMAX
            mb->numberOfFreeElementsBetweenAddresses = numberOfElements;
#endif
            mEz = mb->highestAddress - stepSize;
            for(; mEa < mEz; )
                {
                mEa->next = mEa + stepSize;
                assert(((LONG)(mEa->next) & 1) == 0);
                mEa = mEa->next;
                }
            assert(mEa == mEz);
            mEa->next = 0;
            return mb;
            }
        }
    else
#if _BRACMATEMBEDDED
        return 0;
#else
        exit(-1);
#endif
    }

#if SHOWMEMBLOCKS
static void showMemBlocks()
    {
    int totalbytes;
    int i;
    for(i = 0; i < NumberOfMemBlocks; ++i)
        {
#if defined __VMS
        printf("%p %d %p <= %p <= %p [%p] %lu\n"
#else
        printf("%p %d %p <= %p <= %p [%p] %zu\n"
#endif
               , pMemBlocks[i]
               , i
               , pMemBlocks[i]->lowestAddress
               , pMemBlocks[i]->firstFreeElementBetweenAddresses
               , pMemBlocks[i]->highestAddress
               , pMemBlocks[i]->previousOfSameLength
               , pMemBlocks[i]->sizeOfElement
        );
        }
    totalbytes = 0;
    for(i = 0; i < global_nallocations; ++i)
        {
#if defined __VMS
        printf("%d %d %lu\n"
#else
        printf("%d %d %zu\n"
#endif
               , i + 1
               , global_allocations[i].numberOfElements
               , global_allocations[i].numberOfElements * (i + 1) * sizeof(struct memoryElement)
        );
        totalbytes += global_allocations[i].numberOfElements * (i + 1) * sizeof(struct memoryElement);
        }
    printf("total bytes = %d\n", totalbytes);
    }
#endif

/* The newMemBlocks function is introduced because the same code,
if in-line in bmalloc, and if compiled with -O3, doesn't run. */
static struct memblock* newMemBlocks(size_t n)
    {
    struct memblock* mb;
    int i, j = 0;
    struct memblock** npMemBlocks;
    mb = initializeMemBlock(global_allocations[n].elementSize, global_allocations[n].numberOfElements);
    if(!mb)
        return 0;
    global_allocations[n].numberOfElements *= 2;
    mb->previousOfSameLength = global_allocations[n].memoryBlock;
    global_allocations[n].memoryBlock = mb;

    ++NumberOfMemBlocks;
    npMemBlocks = (struct memblock**)malloc((NumberOfMemBlocks) * sizeof(struct memblock*));
    if(npMemBlocks)
        {
        for(i = 0; i < NumberOfMemBlocks - 1; ++i)
            {
            if(mb < pMemBlocks[i])
                {
                npMemBlocks[j++] = mb;
                for(; i < NumberOfMemBlocks - 1; ++i)
                    {
                    npMemBlocks[j++] = pMemBlocks[i];
                    }
                free(pMemBlocks);
                pMemBlocks = npMemBlocks;
#if SHOWMEMBLOCKS
                showMemBlocks();
#endif
                return mb;
                }
            npMemBlocks[j++] = pMemBlocks[i];
            }
        npMemBlocks[j] = mb;
        free(pMemBlocks);
        pMemBlocks = npMemBlocks;
#if SHOWMEMBLOCKS
        showMemBlocks();
#endif
        return mb;
        }
    else
        {
#if _BRACMATEMBEDDED
        return 0;
#else
        exit(-1);
#endif
        }

    }


static void* bmalloc(int lineno, size_t n)
    {
    void* ret;
#if DOSUMCHECK
    size_t nn = n;
#endif
#if TELLING
    int tel;
    alloc_cnt++;
    if(n < 256)
        cnts[n]++;
    totcnt += n;
#endif
#if TELMAX
    globalloc++;
    if(maxgloballoc < globalloc)
        maxgloballoc = globalloc;
#endif
#if CHECKALLOCBOUNDS
    n += 3 * sizeof(LONG);
#endif
    checksum(__LINE__);
    n = (n - 1) / sizeof(struct memoryElement);
    if(n <
#if _5_6
       6
#elif _4
       4
#else
       3
#endif
       )
        {
        struct memblock* mb = global_allocations[n].memoryBlock;
        ret = mb->firstFreeElementBetweenAddresses;
        while(ret == 0)
            {
            mb = mb->previousOfSameLength;
            if(!mb)
                mb = newMemBlocks(n);
            if(!mb)
                break;
            ret = mb->firstFreeElementBetweenAddresses;
            }
        if(ret != 0)
            {
#if TELMAX
            --(mb->numberOfFreeElementsBetweenAddresses);
            if(mb->numberOfFreeElementsBetweenAddresses < mb->minimumNumberOfFreeElementsBetweenAddresses)
                mb->minimumNumberOfFreeElementsBetweenAddresses = mb->numberOfFreeElementsBetweenAddresses;
#endif
            mb->firstFreeElementBetweenAddresses = ((struct memoryElement*)mb->firstFreeElementBetweenAddresses)->next;
            /** /
            memset(ret,0,(n+1) * sizeof(struct pointerStruct));
            / **/
            ((LONG*)ret)[n] = 0;
            ((LONG*)ret)[0] = 0;
            setChecksum(lineno, nn);
#if CHECKALLOCBOUNDS
            ((LONG*)ret)[n - 1] = 0;
            ((LONG*)ret)[2] = 0;
            ((LONG*)ret)[1] = n;
            ((LONG*)ret)[n] = ('t' << 24) + ('e' << 16) + ('n' << 8) + ('d');
            ((LONG*)ret)[0] = ('s' << 24) + ('t' << 16) + ('a' << 8) + ('r');
            return (void*)(((LONG*)ret) + 2);
#else
            return ret;
#endif
            }
        }
    ret = malloc((n + 1) * sizeof(struct memoryElement));

    if(!ret)
        {
#if TELLING
        errorprintf(
            "MEMORY FULL AFTER %lu ALLOCATIONS WITH MEAN LENGTH %lu\n",
            globalloc, totcnt / alloc_cnt);
        for(tel = 0; tel < 16; tel++)
            {
            int tel1;
            for(tel1 = 0; tel1 < 256; tel1 += 16)
                errorprintf("%lu ", (cnts[tel + tel1] * 1000UL + 500UL) / alloc_cnt);
            errorprintf("\n");
            }
        bezetting();
#endif
        errorprintf(
            "memory full (requested block of %d bytes could not be allocated)",
            (n << 2) + 4);

        exit(1);
        }

#if TELMAX
    ++malloced;
#endif
    ((LONG*)ret)[n] = 0;
    ((LONG*)ret)[0] = 0;
    setChecksum(lineno, n);
#if CHECKALLOCBOUNDS
    ((LONG*)ret)[n - 1] = 0;
    ((LONG*)ret)[2] = 0;
    ((LONG*)ret)[1] = n;
    ((LONG*)ret)[n] = ('t' << 24) + ('e' << 16) + ('n' << 8) + ('d');
    ((LONG*)ret)[0] = ('s' << 24) + ('t' << 16) + ('a' << 8) + ('r');
    return (void*)(((LONG*)ret) + 2);
#else
    return ret;
#endif
    }

int addAllocation(size_t size, int number, int nallocations, struct allocation* allocations)
    {
    int i;
    for(i = 0; i < nallocations; ++i)
        {
        if(allocations[i].elementSize == size)
            {
            allocations[i].numberOfElements += number;
            return nallocations;
            }
        }
    allocations[nallocations].elementSize = size;
    allocations[nallocations].numberOfElements = number;
    return nallocations + 1;
    }

int memblocksort(const void* a, const void* b)
    {
    struct memblock* A = *(struct memblock**)a;
    struct memblock* B = *(struct memblock**)b;
    if(A->lowestAddress < B->lowestAddress)
        return -1;
    return 1;
    }

static int init_memoryspace(void)
    {
    int i;
    global_allocations = (struct allocation*)malloc(sizeof(struct allocation)
                                                    *
#if _5_6
                                                    6
#elif _4
                                                    4
#else
                                                    3
#endif
    );
    global_nallocations = addAllocation(1 * sizeof(struct memoryElement), MEM1SIZE, 0, global_allocations);
    global_nallocations = addAllocation(2 * sizeof(struct memoryElement), MEM2SIZE, global_nallocations, global_allocations);
    global_nallocations = addAllocation(3 * sizeof(struct memoryElement), MEM3SIZE, global_nallocations, global_allocations);
#if _4
    global_nallocations = addAllocation(4 * sizeof(struct memoryElement), MEM4SIZE, global_nallocations, global_allocations);
#endif
#if _5_6
    global_nallocations = addAllocation(5 * sizeof(struct memoryElement), MEM5SIZE, global_nallocations, global_allocations);
    global_nallocations = addAllocation(6 * sizeof(struct memoryElement), MEM6SIZE, global_nallocations, global_allocations);
#endif
    NumberOfMemBlocks = global_nallocations;
    pMemBlocks = (struct memblock**)malloc(NumberOfMemBlocks * sizeof(struct memblock*));

    if(pMemBlocks)
        {
        for(i = 0; i < NumberOfMemBlocks; ++i)
            {
            pMemBlocks[i] = global_allocations[i].memoryBlock = initializeMemBlock(global_allocations[i].elementSize, global_allocations[i].numberOfElements);
            }
        qsort(pMemBlocks, NumberOfMemBlocks, sizeof(struct memblock*), memblocksort);
        /*
        for(i = 0;i < NumberOfMemBlocks;++i)
            {
            printf  ("%p %d %p %p %p %p %lu\n"
                    ,pMemBlocks[i]
                    ,i
                    ,pMemBlocks[i]->lowestAddress
                    ,pMemBlocks[i]->firstFreeElementBetweenAddresses
                    ,pMemBlocks[i]->highestAddress
                    ,pMemBlocks[i]->previousOfSameLength
                    ,pMemBlocks[i]->sizeOfElement
                    );
            }
        */
        return 1;
        }
    else
        return 0;
    }

static void pskfree(psk p)
    {
    bfree(p);
    }

#if defined DEBUGBRACMAT
static void result(psk Root);
#endif

static psk new_operator_like(psk pnode)
    {
    if(Op(pnode) == EQUALS)
        {
        objectnode* goal;
        assert(!ISBUILTIN((objectnode*)pnode));
        goal = (objectnode*)bmalloc(__LINE__, sizeof(objectnode));
#if WORD32
        goal->u.Int = 0;
#else
        goal->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
#endif
        return (psk)goal;
        }
    else
        return (psk)bmalloc(__LINE__, sizeof(knode));
    }

static unsigned char* shift_nw(VOIDORARGPTR)
/* Used from starttree_w and build_up */
    {
    if(startPos)
        {
#if GLOBALARGPTR
        startPos = va_arg(argptr, unsigned char*);
#else
        startPos = va_arg(*pargptr, unsigned char*);
#endif
        if(startPos)
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
    inputBuffer* nextInputElement = InputElement + 1;
    inputBuffer* next2;
    unsigned char* bigBuffer;
    size_t len;
    while(nextInputElement->cutoff)
        ++nextInputElement;

    len = (nextInputElement - InputElement) * (DEFAULT_INPUT_BUFFER_SIZE - 1) + 1;

    if(nextInputElement->buffer)
        {
        len += strlen((const char*)nextInputElement->buffer);
        }

    bigBuffer = (unsigned char*)bmalloc(__LINE__, len);

    nextInputElement = InputElement;

    while(nextInputElement->cutoff)
        {
        strncpy((char*)bigBuffer + (nextInputElement - InputElement) * (DEFAULT_INPUT_BUFFER_SIZE - 1), (char*)nextInputElement->buffer, DEFAULT_INPUT_BUFFER_SIZE - 1);
        bfree(nextInputElement->buffer);
        ++nextInputElement;
        }

    if(nextInputElement->buffer)
        {
        strcpy((char*)bigBuffer + (nextInputElement - InputElement) * (DEFAULT_INPUT_BUFFER_SIZE - 1), (char*)nextInputElement->buffer);
        if(nextInputElement->mallocallocated)
            {
            bfree(nextInputElement->buffer);
            }
        ++nextInputElement;
        }
    else
        bigBuffer[(nextInputElement - InputElement) * (DEFAULT_INPUT_BUFFER_SIZE - 1)] = '\0';

    InputElement->buffer = bigBuffer;
    InputElement->cutoff = FALSE;
    InputElement->mallocallocated = TRUE;

    for(next2 = InputElement + 1; nextInputElement->buffer; ++next2, ++nextInputElement)
        {
        next2->buffer = nextInputElement->buffer;
        next2->cutoff = nextInputElement->cutoff;
        next2->mallocallocated = nextInputElement->mallocallocated;
        }

    next2->buffer = NULL;
    next2->cutoff = FALSE;
    next2->mallocallocated = FALSE;
    }

static unsigned char* vshift_w(VOIDORARGPTR)
/* used from buildtree_w, which receives a list of bmalloc-allocated string
   pointers. The last string pointer must not be deallocated here */
    {
    if(InputElement->buffer && (++InputElement)->buffer)
        {
        if(InputElement->cutoff)
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

static unsigned char* vshift_nw(VOIDORARGPTR)
/* Used from vbuildup */
    {
    if(*pstart && *++pstart)
        start = *pstart;
    return start;
    }

static unsigned char* (*shift)(VOIDORARGPTR) = shift_nw;

static void tel(int c)
    {
    UNREFERENCED_PARAMETER(c);
    telling++;
    }

static void tstr(int c)
    {
    static int esc = FALSE, str = FALSE;
    if(esc)
        {
        esc = FALSE;
        telling++;
        }
    else if(c == '\\')
        esc = TRUE;
    else if(str)
        {
        if(c == '"')
            str = FALSE;
        else
            telling++;
        }
    else if(c == '"')
        str = TRUE;
    else if(c != ' ')
        telling++;
    }

static void pstr(int c)
    {
    static int esc = FALSE, str = FALSE;
    if(esc)
        {
        esc = FALSE;
        switch(c)
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
    else if(c == '\\')
        esc = TRUE;
    else if(str)
        {
        if(c == '"')
            str = FALSE;
        else
            *source++ = (char)c;
        }
    else if(c == '"')
        str = TRUE;
    else if(c != ' ')
        *source++ = (char)c;
    }

static void glue(int c)
    {
    *source++ = (char)c;
    }

#define NARROWLINELENGTH 80
#define WIDELINELENGTH 120

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
        Child = Op(Root->LEFT);
        if(HAS__UNOPS(Root->LEFT) || Parent >= Child)
            max += (2 * COMPLEX_MAX) / LineLength; /* 2 parentheses */

        Child = Op(Root->RIGHT);
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

static int lineToLong(unsigned char* strng)
    {
    if(hum
       && strlen((const char*)strng) > 10 /*LineLength*/
       /* very short strings are allowed to keep \n and \t */
       )
        return TRUE;
    return FALSE;
    }

static int quote(unsigned char* strng)
    {
    unsigned char* pstring;
    if(needsquotes[*strng] & 1)
        return TRUE;
    for(pstring = strng; *pstring; pstring++)
        if(needsquotes[*pstring] & 2)
            return TRUE;
        else if(needsquotes[*pstring] & 4
                && lineToLong(strng)
                )
            return TRUE;
    return FALSE;
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

#if DATAMATCHESITSELF
//#define SM(Root) {if((Root)->v.fl & SELFMATCHING) (*process)(';');}
#define SM(Root)
#else
#define SM(Root)
#endif

static void endnode(psk Root, int space)
    {
    unsigned char* pstring;
    int q, ikar;
#if CHECKALLOCBOUNDS
    if(POINT)
        printf("\n[%p %d]", Root, (Root->v.fl & ALL_REFCOUNT_BITS_SET) / ONEREF);
#endif
    SM(Root)

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
                    if(longline || lineToLong(POBJ(Root)))
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
                    if(longline || lineToLong(POBJ(Root)))
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

static psk same_as_w(psk pnode)
    {
    if(shared(pnode) != ALL_REFCOUNT_BITS_SET)
        {
        (pnode)->v.fl += ONEREF;
        return pnode;
        }
#if WORD32
    else if(is_object(pnode))
        {
        INCREFCOUNT(pnode);
        return pnode;
        }
#endif
    else
        {
        return subtreecopy(pnode);
        }
    }

static psk same_as_w_2(ppsk PPnode)
    {
    psk pnode = *PPnode;
    if(shared(pnode) != ALL_REFCOUNT_BITS_SET)
        {
        pnode->v.fl += ONEREF;
        return pnode;
        }
#if WORD32
    else if(is_object(pnode))
        {
        INCREFCOUNT(pnode);
        return pnode;
        }
#endif
    else
        {
        /*
        0:?n&:?L&whl'(!n+1:?n:<10000&out$!n&XXX !L:?L)
        0:?n&:?L&whl'(!n+1:?n:<10000&out$!n&!L XXX:?L) This is not improved!
        */
        *PPnode = subtreecopy(pnode);
        return pnode;
        }
    }

#if ICPY
static void icpy(LONG* d, LONG* b, int words)
    {
    while(words--)
        *d++ = *b++;
    }
#endif

static psk iCopyOf(psk pnode)
    {
    /* REQUIREMENTS : After the string delimiting 0 all remaining bytes in the
    current computer word must be 0 as well.
    Argument must start on a word boundary. */
    psk ret;
    size_t len;
    len = sizeof(ULONG) + strlen((char*)POBJ(pnode));
    ret = (psk)bmalloc(__LINE__, len + 1);
#if ICPY
    MEMCPY(ret, pnode, (len >> LOGWORDLENGTH) + 1);
#else
    MEMCPY(ret, pnode, ((len / sizeof(LONG)) + 1) * sizeof(LONG));
#endif
    ret->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
    return ret;
    }

static void wipe(psk top);

static void copyToCutoff(psk* ppnode, psk pnode, psk cutoff)
    {
    for(;;)
        {
        if(is_op(pnode))
            {
            if(pnode->RIGHT == cutoff)
                {
                *ppnode = same_as_w(pnode->LEFT);
                break;
                }
            else
                {
                psk p = new_operator_like(pnode);
                p->v.fl = pnode->v.fl & COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                p->LEFT = same_as_w(pnode->LEFT);
                *ppnode = p;
                ppnode = &(p->RIGHT);
                pnode = pnode->RIGHT;
                }
            }
        else
            {
            *ppnode = iCopyOf(pnode);
            break;
            }
        }
    }

static psk Head(psk pnode)
    {
    if(pnode->v.fl & LATEBIND)
        {
        assert(!shared(pnode));
        if(is_op(pnode))
            {
            psk root = pnode;
            copyToCutoff(&pnode, root->LEFT, root->RIGHT);
            wipe(root);
            }
        else
            {
            stringrefnode* ps = (stringrefnode*)pnode;
            pnode = (psk)bmalloc(__LINE__, sizeof(ULONG) + 1 + ps->length);
            pnode->v.fl = (ps->v.fl & COPYFILTER /*~ALL_REFCOUNT_BITS_SET*/ & ~LATEBIND);
            strncpy((char*)(pnode)+sizeof(ULONG), (char*)ps->str, ps->length);
            wipe(ps->pnode);
            bfree(ps);
            }
        }
    return pnode;
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
        Child = Op(Root->LEFT);
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
            printf("\n[%p %d]", Root, (Root->v.fl & ALL_REFCOUNT_BITS_SET) / ONEREF);
#endif
        SM(Root)
            do_something(opchar[klopcode(Root)]);
        Parent = Op(Root);
        Child = Op(Root->RIGHT);
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
            Child = Op(Root->LEFT);
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
            Child = Op(Root->RIGHT);
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
            } while(is_op(Root));
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
        Child = Op(Root->LEFT);
        if(HAS__UNOPS(Root->LEFT) || Parent >= Child)
            parenthesised_result(Root->LEFT, level + 1, FALSE, RSP);
        else
            reslt(Root->LEFT, level + 1, FALSE, RSP);
        ind = indent(Root, level, ind);
        if(ind)
            extraSpc = 1;
#if CHECKALLOCBOUNDS
        if(POINT)
            printf("\n[%p %d]", Root, (Root->v.fl & ALL_REFCOUNT_BITS_SET) / ONEREF);
#endif
        SM(Root)
            do_something(opchar[klopcode(Root)]);
        Parent = Op(Root);

        Child = Op(Root->RIGHT);
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

static void result(psk Root)
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

#if 1
#define testMul(a,b,c,d) 
#else
#if CHECKALLOCBOUNDS
static int testMul(char* txt, psk variabele, psk pbinding, int doit)
    {
    if(doit
       || is_op(pbinding) && Op(pbinding) == TIMES && pbinding->LEFT->u.obj == 'a'
       )
        {
        POINT = 1;
        printf("%s ", txt);
        if(variabele)
            {
            result(variabele);
            }
        printf(":");
        result(pbinding);
        printf("\n");
        POINT = 0;
        return 1;
        }
    return 0;
    }
#else
#define testMul(a,b,c,d) 
#endif
#endif

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
        Child = Op(Root->LEFT);
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
        Child = Op(Root->RIGHT);
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

static void results(psk Root, psk cutoff)
    {
    if(HAS__UNOPS(Root))
        {
        hreslts(Root, 0, FALSE, 0, cutoff);
        }
    else
        reslts(Root, 0, FALSE, 0, cutoff);
    }
#endif

static LONG toLong(psk pnode)
    {
    LONG res;
    res = (LONG)STRTOUL((char*)POBJ(pnode), (char**)NULL, 10);
    if(pnode->v.fl & MINUS)
        res = -res;
    return res;
    }

static int numbercheck(const char* begin)
    {
    int op_or_0, check;
    int needNonZeroDigit = FALSE;
    if(!*begin)
        return 0;
    check = QNUMBER;
    op_or_0 = *begin;

    if(op_or_0 >= '0' && op_or_0 <= '9')
        {
        if(op_or_0 == '0')
            check |= QNUL;
        while(optab[op_or_0 = *++begin] != -1)
            {
            if(op_or_0 == '/')
                {
                /* check &= ~QNUL;*/
                if(check & QFRACTION)
                    {
                    check = DEFINITELYNONUMBER;
                    break;
                    }
                else
                    {
                    needNonZeroDigit = TRUE;
                    check |= QFRACTION;
                    }
                }
            else if(op_or_0 < '0' || op_or_0 > '9')
                {
                check = DEFINITELYNONUMBER;
                break;
                }
            else
                {
                /* initial zero followed by
                                 0 <= k <= 9 makes no number */
                if((check & (QNUL | QFRACTION)) == QNUL)
                    {
                    check = DEFINITELYNONUMBER;
                    break;
                    }
                else if(op_or_0 != '0')
                    {
                    needNonZeroDigit = FALSE;
                    /*check &= ~QNUL;*/
                    }
                else if(needNonZeroDigit) /* '/' followed by '0' */
                    {
                    check = DEFINITELYNONUMBER;
                    break;
                    }
                }
            }
        /* Trailing closing parentheses were accepted on equal footing with '\0' bytes. */
        if(op_or_0 == ')') /* "2)"+3       @("-23/4)))))":-23/4)  */
            {
            check = DEFINITELYNONUMBER;
            }
        }
    else
        {
        check = DEFINITELYNONUMBER;
        }
    if(check && needNonZeroDigit)
        {
        check = 0;
        }
    return check;
    }

static int fullnumbercheck(const char* begin)
    {
    if(*begin == '-')
        {
        int ret = numbercheck(begin + 1);
        if(ret & ~DEFINITELYNONUMBER)
            return ret | MINUS;
        else
            return ret;
        }
    else
        return numbercheck(begin);
    }

static int sfullnumbercheck(char* begin, char* cutoff)
    {
    unsigned char sav = *cutoff;
    int ret;
    *cutoff = '\0';
    ret = fullnumbercheck(begin);
    *cutoff = sav;
    return ret;
    }

static int flags(void)
    {
    int Flgs = 0;

    for(;; start++)
        {
        switch(*start)
            {
            case '!':
                if(Flgs & INDIRECT)
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

    if((Flgs & NOT) && (Flgs < ATOM))
        Flgs ^= SUCCESS;
    return Flgs;
    }

static psk Atom(int Flgs)
    {
    unsigned char* begin, * eind;
    size_t af = 0;
    psk Pnode;
    begin = start;

    while(optab[*start] == NOOP)
        if(*start++ == 0x7F)
            af++;

    eind = start;
    Pnode = (psk)bmalloc(__LINE__, sizeof(ULONG) + 1 + (size_t)(eind - begin) - af);
    start = begin;
    begin = POBJ(Pnode);
    while(start < eind)
        {
        if(*start == 0x7F)
            {
            ++start;
            *begin++ = (unsigned char)(*start++ | 0x80);
            }
        else
            {
            *begin++ = (unsigned char)(*start++ & 0x7F);
            }
        }
    if(Flgs & INDIRECT)
        {
        Pnode->v.fl = Flgs ^ SUCCESS;
        }
    else
        {
        if(NEGATION(Flgs, NUMBER))
            Pnode->v.fl = (Flgs ^ (READY | SUCCESS));
        else
            Pnode->v.fl = (Flgs ^ (READY | SUCCESS)) | (numbercheck(SPOBJ(Pnode)) & ~DEFINITELYNONUMBER);
        }
#if DATAMATCHESITSELF
    if(!(Pnode->v.fl & VISIBLE_FLAGS))
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
    while(top->v.fl & IMPLIEDFENCE)
        {
        child = top->RIGHT;
        if(Op(child) != Op(top))
            break;
        /* Something to do. */
        top->RIGHT = child->LEFT;
        child->LEFT = top;
        top = child;
        }
    for(;;)
        {
        top->v.fl &= ~IMPLIEDFENCE;
        child = top->LEFT;
        if(RATIONAL(child))
            child->v.fl |= SELFMATCHING;
        if(RATIONAL(top->RIGHT))
            top->RIGHT->v.fl |= SELFMATCHING;
        if(child != Lhs)
            top->LEFT = child->RIGHT;
        if(((top->RIGHT->v.fl) & SELFMATCHING)
           && ((top->LEFT->v.fl) & SELFMATCHING)
           )
            {
            /* Something to do. */
            top->v.fl |= SELFMATCHING;
            }
        else
            top->v.fl &= ~SELFMATCHING;

        if(child == Lhs)
            break;
        child->RIGHT = top;
        top = child;
        }
    return top;
    }
#endif

#if GLOBALARGPTR
static psk lex(int* nxt, int priority, int Flags)
#else
static psk lex(int* nxt, int priority, int Flags, va_list* pargptr)
#endif
/* *nxt (if nxt != 0) is set to the character following the expression. */
    {
    int op_or_0;
    psk Pnode;
    if(*start > 0 && *start <= '\6')
        Pnode = same_as_w(addr[*start++]);
    else
        {
        int Flgs;
        Flgs = flags();
        if(*start == '(')
            {
            if(*++start == 0)
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
    if((*start == 0) && (!*(*shift)()))
#else
    if((*start == 0) && (!*(*shift)(pargptr)))
#endif
        {
        /* No, no operator following. Return what we already have got. */
        return Pnode;
        }
    else
        {
        op_or_0 = *start;

        if(*++start == 0)
#if GLOBALARGPTR
        (*shift)();
#else
            (*shift)(pargptr);
#endif

        if(optab[op_or_0] == NOOP) /* 20080910 Otherwise problem with the k in ()k */
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
                if(optab[op_or_0] < priority) /* 'op_or_0' has too low priority */
                    {
                    /* Our Pnode is followed by an operator, but it turns out
                       that Pnode isn't the LHS of that operator. Instead,
                       Pnode is the RHS of some other operator and the LHS of
                       the coming operator is a bigger subtree that contains
                       Pnode as the rightmost leaf.
                    */
#if STRINGMATCH_CAN_BE_NEGATED
                    if((Flags & (NOT | FILTERS)) == (NOT | ATOM)
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
                    if(nxt)
                        *nxt = op_or_0; /* Tell the ancestors of Pnode about
                                           the coming operator. */
                    return Pnode;
                    }
                else
                    {
                    /* The coming operator has the same or higher priority. */
#if WORD32
                    if(optab[op_or_0] == EQUALS)
                        operatorNode = (psk)bmalloc(__LINE__, sizeof(objectnode));
                    else
#endif
                        /* on 64 bit platform, sizeof(objectnode) == sizeof(knode) 20210803*/
                        operatorNode = (psk)bmalloc(__LINE__, sizeof(knode));
                    assert(optab[op_or_0] != NOOP);
                    assert(optab[op_or_0] >= 0);
                    operatorNode->v.fl = optab[op_or_0] | SUCCESS;
                    operatorNode->LEFT = Pnode;

                    if(optab[op_or_0] == priority) /* 'op_or_0' has same priority */
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
                        if(nxt)
                            *nxt = op_or_0; /* To let the caller know to
                                               iterate.
                                            */
                        return operatorNode;
                        }
                    else
                        {
#if DATAMATCHESITSELF
                        switch(optab[op_or_0])
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
                                for(;;)
                                    {
                                    operatorNode->v.fl |= IMPLIEDFENCE;
                                    child_op_or_0 = 0;
                                    assert(optab[op_or_0] >= 0);
#if GLOBALARGPTR
                                    operatorNode->RIGHT = lex(&child_op_or_0, optab[op_or_0], 0);
#else
                                    operatorNode->RIGHT = lex(&child_op_or_0, optab[op_or_0], 0, pargptr);
#endif
                                    if(child_op_or_0 != op_or_0)
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
                                for(;;)
                                    {
                                    child_op_or_0 = 0;
                                    assert(optab[op_or_0] >= 0);
#if GLOBALARGPTR
                                    operatorNode->RIGHT = lex(&child_op_or_0, optab[op_or_0], 0);
#else
                                    operatorNode->RIGHT = lex(&child_op_or_0, optab[op_or_0], 0, pargptr);
#endif
                                    if(child_op_or_0 != op_or_0)
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
                } while(op_or_0 != 0);
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
    if(Pnode)
        wipe(Pnode);
    InputElement = InputArray;
    if(InputElement->cutoff)
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
    if((--InputElement)->mallocallocated)
        {
        bfree(InputElement->buffer);
        }
    bfree(InputArray);
    return Pnode;
    }

static void lput(int c)
    {
    if(inputBufferPointer >= maxInputBufferPointer)
        {
        inputBuffer* newInputArray;
        unsigned char* input_buffer;
        unsigned char* dest;
        int len;
        size_t L;

        for(len = 0; InputArray[++len].buffer;)
            ;
        /* len = index of last element in InputArray array */

        input_buffer = InputArray[len - 1].buffer;
        /* The last string (probably on the stack, not on the heap) */

        while(inputBufferPointer > input_buffer && optab[*--inputBufferPointer] == NOOP)
            ;
        /* inputBufferPointer points at last operator (where string can be split) or at
           the start of the string. */

        newInputArray = (inputBuffer*)bmalloc(__LINE__, (2 + len) * sizeof(inputBuffer));
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
        if(inputBufferPointer == input_buffer)
            {
            /* copy the full content of input_buffer to the second last element */
            dest = newInputArray[len].buffer = (unsigned char*)bmalloc(__LINE__, DEFAULT_INPUT_BUFFER_SIZE);
            strncpy((char*)dest, (char*)input_buffer, DEFAULT_INPUT_BUFFER_SIZE - 1);
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
            dest = newInputArray[len].buffer = (unsigned char*)bmalloc(__LINE__, L + 1);
            strncpy((char*)dest, (char*)input_buffer, L);
            dest[L] = '\0';
            newInputArray[len].cutoff = FALSE;
            newInputArray[len].mallocallocated = TRUE;

            /* Now remove the substring up to inputBufferPointer from input_buffer */
            L = (size_t)(maxInputBufferPointer - inputBufferPointer);
            strncpy((char*)input_buffer, (char*)inputBufferPointer, L);
            input_buffer[L] = '\0';
            inputBufferPointer = input_buffer + L;
            }

        /* Copy previous element's fields */
        while(len)
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
    if(c & 0x80)
        lput(0x7F);
    lput(c | 0x80);
    }

void writeError(psk Pnode)
    {
    FILE* saveFpo;
    int saveNice;
    saveNice = beNice;
    beNice = FALSE;
    saveFpo = global_fpo;
    global_fpo = errorStream;
#if !defined NO_FOPEN
    if(global_fpo == NULL && errorFileName != NULL)
        global_fpo = fopen(errorFileName, APPENDBIN);
#endif
    if(global_fpo)
        {
        result(Pnode);
        myputc('\n');
        /*#if !_BRACMATEMBEDDED*/
#if !defined NO_FOPEN
        if(errorStream == NULL && global_fpo != stderr && global_fpo != stdout)
            {
            fclose(global_fpo);
            }
#endif
        }
    /*#endif*/
    global_fpo = saveFpo;
    beNice = saveNice;
    }

/*#if !_BRACMATEMBEDDED*/
static int redirectError(char* name)
    {
#if !defined NO_FOPEN
    if(errorFileName)
        {
        free(errorFileName);
        errorFileName = NULL;
        }
#endif
    if(!strcmp(name, "stdout"))
        {
        errorStream = stdout;
        return TRUE;
        }
    else if(!strcmp(name, "stderr"))
        {
        errorStream = stderr;
        return TRUE;
        }
    else
        {
#if !defined NO_FOPEN
        errorStream = fopen(name, APPENDTXT);
        if(errorStream)
            {
            fclose(errorStream);
            errorStream = NULL;
            errorFileName = (char*)malloc(strlen(name) + 1);
            if(errorFileName)
                {
                strcpy(errorFileName, name);
                return TRUE;
                }
            else
                return FALSE;
            }
#endif
        errorStream = stderr;
        }
    return FALSE;
    }
/*#endif*/

static void politelyWriteError(psk Pnode)
    {
    unsigned char name[256] = "";
#if !_BRACMATEMBEDDED
#if !defined NO_FOPEN
    if(errorFileName)
        {
        ;
        }
    else
        {
        int i, ikar;
        Printf("\nType name of file to write erroneous expression to and then press <return>\n(Type nothing: screen, type dot '.': skip): ");
        for(i = 0; i < 255 && (ikar = mygetc(stdin)) != '\n';)
            {
            name[i++] = (unsigned char)ikar;
            }
        name[i] = '\0';
        if(name[0] && name[0] != '.')
            redirectError((char*)name);
        }
#endif
#endif
    if(name[0] != '.')
        writeError(Pnode);
    }

static psk input(FILE* fpi, psk Pnode, int echmemvapstrmltrm, Boolean* err, Boolean* GoOn)
    {
    static int stdinEOF = FALSE;
    int braces, ikar, hasop, whiteSpaceSeen, escape, backslashesAreEscaped,
        inString, parentheses, error;
#ifdef __SYMBIAN32__
    unsigned char* input_buffer;
    input_buffer = bmalloc(__LINE__, DEFAULT_INPUT_BUFFER_SIZE);
#else
    unsigned char input_buffer[DEFAULT_INPUT_BUFFER_SIZE];
#endif
    if((fpi == stdin) && (stdinEOF == TRUE))
        exit(0);
    maxInputBufferPointer = input_buffer + (DEFAULT_INPUT_BUFFER_SIZE - 1);/* there must be room  for terminating 0 */
    /* Array of pointers to inputbuffers. Initially 2 elements,
       large enough for small inputs (< DEFAULT_INPUT_BUFFER_SIZE)*/
    InputArray = (inputBuffer*)bmalloc(__LINE__, 2 * sizeof(inputBuffer));
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
    if(echmemvapstrmltrm & OPT_ML)
        {
        inputBufferPointer = input_buffer;
        XMLtext(fpi, (char*)source, (echmemvapstrmltrm & OPT_TRM), (echmemvapstrmltrm & OPT_HT), (echmemvapstrmltrm & OPT_X));
        *inputBufferPointer = 0;
        Pnode = buildtree_w(Pnode);
        if(err) *err = error;
#ifdef __SYMBIAN32__
        bfree(input_buffer);
#endif
        if(GoOn)
            *GoOn = FALSE;
        return Pnode;
        }
    else
#endif
#if READJSON
        if(echmemvapstrmltrm & OPT_JSON)
            {
            inputBufferPointer = input_buffer;
            error = JSONtext(fpi, (char*)source);
            *inputBufferPointer = 0;
            Pnode = buildtree_w(Pnode);
            if(err) *err = error;
#ifdef __SYMBIAN32__
            bfree(input_buffer);
#endif
            if(GoOn)
                *GoOn = FALSE;
            return Pnode;
            }
        else
#endif
            if(echmemvapstrmltrm & (OPT_VAP | OPT_STR))
                {
                for(inputBufferPointer = input_buffer;;)
                    {
                    if(fpi)
                        {
                        ikar = mygetc(fpi);
                        if(ikar == EOF)
                            {
                            if(fpi == stdin)
                                stdinEOF = TRUE;
                            break;
                            }
                        if((fpi == stdin)
                           && (ikar == '\n')
                           )
                            break;
                        }
                    else
                        if((ikar = *source++) == 0)
                            break;
                    if(ikar & 0x80)
                        lput(0x7F);
                    lput(ikar | 0x80);
                    if(echmemvapstrmltrm & OPT_VAP)
                        {
                        if(echmemvapstrmltrm & OPT_STR)
                            lput(' ' | 0x80);
                        else
                            lput(' ');
                        }
                    }
                *inputBufferPointer = 0;
                Pnode = buildtree_w(Pnode);
                if(err) *err = error;
#ifdef __SYMBIAN32__
                bfree(input_buffer);
#endif
                if(GoOn)
                    *GoOn = FALSE;
                return Pnode;
                }
    for(inputBufferPointer = input_buffer
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
        if(echmemvapstrmltrm & OPT_ECH)
            {
            if(fpi != stdin)
                Printf("%c", ikar);
            if(ikar == '\n')
                {
                if(braces)
                    Printf("{com} ");
                else if(inString)
                    Printf("{str} ");
                else if(parentheses > 0 || fpi != stdin)
                    {
                    int tel;
                    Printf("{%d} ", parentheses);
                    if(fpi == stdin)
                        for(tel = parentheses; tel; tel--)
                            Printf("  ");
                    }
                }
            }
        if(braces)
            {
            if(ikar == '{')
                braces++;
            else if(ikar == '}')
                braces--;
            }
        else if(ikar & 0x80)
            {
            if(whiteSpaceSeen && !hasop)
                lput(' ');
            whiteSpaceSeen = FALSE;
            lput(0x7F);
            lput(ikar);
            escape = FALSE;
            hasop = FALSE;
            }
        else
            {
            if(escape)
                {
                escape = FALSE;
                if(0 <= ikar && ikar < ' ')
                    break; /* this is unsyntactical */
                switch(ikar)
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
            else if(ikar == '\\'
                    && (backslashesAreEscaped
                        || !inString /* %\L @\L */
                        )
                    )
                {
                escape = TRUE;
                continue;
                }
            if(inString)
                {
                if(ikar == '"')
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
                switch(ikar)
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
                        if(optab[ikar] == WHITE
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
                            switch(ikar)
                                {
                                case ';':
                                    if(parentheses)
                                        {
                                        *inputBufferPointer = 0;
                                        errorprintf("\n%d %s \"(\"", parentheses, unbalanced);
                                        error = TRUE;
                                        }
                                    if(echmemvapstrmltrm & OPT_ECH)
                                        Printf("\n");
                                    /* fall through */
                                case '\n':
                                    /* You get here only directly if fpi==stdin */
                                    *inputBufferPointer = 0;
                                    Pnode = buildtree_w(Pnode);
                                    if(error)
                                        politelyWriteError(Pnode);
                                    if(err) *err = error;
#ifdef __SYMBIAN32__
                                    bfree(input_buffer);
#endif
                                    if(GoOn)
                                        *GoOn = ikar == ';' && !error;
                                    return Pnode;
                                default:
                                    switch(ikar)
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

                                    if(whiteSpaceSeen
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

                                    if(!inString)
                                        {
                                        lput(ikar);
                                        if(hasop)
                                            backslashesAreEscaped = TRUE;
                                        }
                                }
                            }
                        }
                    }
                }
            }
        }
    if((fpi == stdin) && (ikar == EOF))
        {
        stdinEOF = TRUE;
        }
    *inputBufferPointer = 0;
#if _BRACMATEMBEDDED
    if(!error)
#endif
        {
        if(inString)
            {
            errorprintf("\n%s \"", unbalanced);
            error = TRUE;
            /*exit(1);*/
            }
        if(braces)
            {
            errorprintf("\n%d %s \"{\"", braces, unbalanced);
            error = TRUE;
            /*exit(1);*/
            }
        if(parentheses > 0)
            {
            errorprintf("\n%d %s \"(\"", parentheses, unbalanced);
            error = TRUE;
            /*exit(1);*/
            }
        if(parentheses < 0)
            {
#if !defined NO_EXIT_ON_NON_SEVERE_ERRORS
            if(ikar == 'j' || ikar == 'J' || ikar == 'y' || ikar == 'Y')
                {
                exit(0);
                }
            else if(!fpi || fpi == stdin)
                {
                Printf("\nend session? (y/n)");
                while((ikar = mygetc(stdin)) != 'n')
                    {
                    if(ikar == 'j' || ikar == 'J' || ikar == 'y' || ikar == 'Y')
                        {
                        exit(0);
                        }
                    }
                while(ikar != '\n')
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
        if(echmemvapstrmltrm & OPT_ECH)
            Printf("\n");
        /*if(*InputArray[0].buffer)*/
        {
        Pnode = buildtree_w(Pnode);
        if(error)
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
    if(err)
        *err = error;
#ifdef __SYMBIAN32__
    bfree(input_buffer);
#endif
    if(GoOn)
        *GoOn = FALSE;
    return Pnode;
    }

#if JMP
#include <setjmp.h>
static jmp_buf jumper;
#endif

void stringEval(const char* s, const char** out, int* err)
    {
#if _BRACMATEMBEDDED
    char* buf = (char*)malloc(strlen(s) + 11);
    if(buf)
        {
        sprintf(buf, "put$(%s,MEM)", s);
#else
    char* buf = (char*)malloc(strlen(s) + 7);
    if(buf)
        {
        sprintf(buf, "str$(%s)", s);
#endif
        source = (unsigned char*)buf;
#if 0
        oldlnode = 0;
        lastEvaluatedFunction = 0;
#endif
        global_anchor = input(NULL, global_anchor, OPT_MEM, err, NULL); /* 4 -> OPT_MEM*/
        if(err && *err)
            return;
#if JMP
        if(setjmp(jumper) != 0)
            {
            free(buf);
            return -1;
            }
#endif
        global_anchor = eval(global_anchor);
        if(out != NULL)
            *out = is_op(global_anchor) ? (const char*)"" : (const char*)POBJ(global_anchor);
        free(buf);
        }
    return;
    }

static psk copyof(psk pnode)
    {
    psk res;
    res = iCopyOf(pnode);
    res->v.fl &= ~IDENT;
    return res;
    }

static psk _copyop(psk Pnode)
    {
    psk apnode;
    apnode = new_operator_like(Pnode);
    apnode->v.fl = Pnode->v.fl & COPYFILTER;/* (ALL_REFCOUNT_BITS_SET | CREATEDWITHNEW);*/
    apnode->LEFT = same_as_w_2(&Pnode->LEFT);
    apnode->RIGHT = same_as_w(Pnode->RIGHT);
    return apnode;
    }

static psk copyop(psk Pnode)
    {
    dec_refcount(Pnode);
    return _copyop(Pnode);
    }

static psk subtreecopy(psk src)
    {
    if(is_op(src))
        return _copyop(src);
    else
        return iCopyOf(src);
    }

static int number_degree(psk pnode)
    {
    if(RATIONAL_COMP(pnode))
        return 4;
    switch(PLOBJ(pnode))
        {
        case IM: return 3;
        case PI: return 2;
        case XX: return 1;
        default: return 0;
        }
    }

static int is_constant(psk pnode)
    {
    while(is_op(pnode))
        {
        if(!is_constant(pnode->LEFT))
            return FALSE;
        pnode = pnode->RIGHT;
        }
    return number_degree(pnode);
    }

static void init_opcode(void)
    {
    int tel;
    for(tel = 0; tel < 256; tel++)
        {
#if TELLING
        cnts[tel] = 0;
#endif
        switch(tel)
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

static psk isolated(psk Pnode)
    {
    if(shared(Pnode))
        {
        dec_refcount(Pnode);
        return subtreecopy(Pnode);
        }
    return Pnode;
    }

static psk setflgs(psk pokn, ULONG Flgs)
    {
    if((Flgs & BEQUEST) || !(Flgs & SUCCESS))
        {
        pokn = isolated(pokn);
        pokn->v.fl ^= ((Flgs & SUCCESS) ^ SUCCESS);
        pokn->v.fl |= (Flgs & BEQUEST);
        if(ANYNEGATION(Flgs))
            pokn->v.fl |= NOT;
        }
    return pokn;
    }

static psk starttree_w(psk Pnode, ...)
    {
#if !GLOBALARGPTR
    va_list argptr;
#endif
    if(Pnode)
        wipe(Pnode);
    va_start(argptr, Pnode);
    start = startPos = va_arg(argptr, unsigned char*);
#if GLOBALARGPTR
    Pnode = lex(NULL, 0, 0);
#else
    Pnode = lex(NULL, 0, 0, &argptr);
#endif
    va_end(argptr);
    return Pnode;
    }

static psk vbuildupnowipe(psk Pnode, const char* conc[])
    {
    psk okn;
    assert(Pnode != NULL);
    pstart = (unsigned char**)conc;
    start = (unsigned char*)conc[0];
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

static psk vbuildup(psk Pnode, const char* conc[])
    {
    psk okn = vbuildupnowipe(Pnode, conc);
    wipe(Pnode);
    return okn;
    }

static psk build_up(psk Pnode, ...)
    {
    psk okn;
#if !GLOBALARGPTR
    va_list argptr;
#endif
    va_start(argptr, Pnode);
    start = startPos = va_arg(argptr, unsigned char*);
#if GLOBALARGPTR
    okn = lex(NULL, 0, 0);
#else
    okn = lex(NULL, 0, 0, &argptr);
#endif
    va_end(argptr);
    if(Pnode)
        {
        okn = setflgs(okn, Pnode->v.fl);
        wipe(Pnode);
        }
    return okn;
    }

static psk dopb(psk Pnode, psk src)
    {
    psk okn;
    okn = same_as_w(src);
    okn = setflgs(okn, Pnode->v.fl);
    wipe(Pnode);
    return okn;
    }

static method_pnt findBuiltInMethodByName(typedObjectnode* object, const char* name)
    {
    method* methods = object->vtab;
    if(methods)
        {
        for(; methods->name && strcmp(methods->name, name); ++methods)
            ;
        return methods->func;
        }
    return NULL;
    }

static void wipe(psk top)
    {
    while(!shared(top)) /* tail recursion optimisation; delete deep structures*/
        {
        psk pnode = NULL;
        if(is_object(top) && ISCREATEDWITHNEW((objectnode*)top))
            {
            addr[1] = top->RIGHT;
            pnode = build_up(pnode, "(((=\1).die)')", NULL);
            pnode = eval(pnode);
            wipe(pnode);
            if(ISBUILTIN((objectnode*)top))
                {
                method_pnt theMethod = findBuiltInMethodByName((typedObjectnode*)top, "Die");
                if(theMethod)
                    {
                    theMethod((struct typedObjectnode*)top, NULL);
                    }
                }
            }
        if(is_op(top))
            {
            wipe(top->LEFT);
            pnode = top;
            top = top->RIGHT;
            pskfree(pnode);
            }
        else
            {
            if(top->v.fl & LATEBIND)
                {
                wipe(((stringrefnode*)top)->pnode);
                }
            pskfree(top);
            return;
            }
        }
    dec_refcount(top);
    }
#define POWER2
static int power2(int n)
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
        int MSB = power2(n);
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
        int MSB = power2(n);
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

#if INTSCMP
static int intscmp(LONG* s1, LONG* s2) /* this routine produces different results
                                  depending on BIGENDIAN */
    {
    while(*((char*)s1 + 3))
        {
        if(*s1 != *s2)
            {
            if(*s1 < *s2)
                return -1;
            else
                return 1;
            }
        s1++;
        s2++;
        }
    if(*s1 != *s2)
        {
        if(*s1 < *s2)
            return -1;
        else
            return 1;
        }
    else
        return 0;
    }
#endif


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

static Qnumber qTimesMinusOne(Qnumber _qx)
    {
    Qnumber res;
    size_t len;
    len = offsetof(sk, u.obj) + 1 + strlen((char*)POBJ(_qx));
    res = (Qnumber)bmalloc(__LINE__, len);
    memcpy(res, _qx, len);
    res->v.fl ^= MINUS;
    res->v.fl &= ~ALL_REFCOUNT_BITS_SET;
    return res;
    }

/* Create a node from a number, allocating memory for the node.
The numbers' memory isn't deallocated. */

static char* iconvert2decimal(nnumber* res, char* g)
    {
    LONG* ipointer;
    g[0] = '0';
    g[1] = 0;
    for(ipointer = res->inumber; ipointer < res->inumber + res->ilength; ++ipointer)
        {
        assert(*ipointer >= 0);
        assert(*ipointer < RADIX);
        if(*ipointer)
            {
            g += sprintf(g, LONGD, *ipointer);
            for(; ++ipointer < res->inumber + res->ilength;)
                {
                assert(*ipointer >= 0);
                assert(*ipointer < RADIX);
                g += sprintf(g,/*"%0*ld"*/LONG0nD, (int)TEN_LOG_RADIX, *ipointer);
                }
            break;
            }
        }
    return g;
    }

static ptrdiff_t numlength(nnumber* n)
    {
    ptrdiff_t len;
    LONG H;
    assert(n->ilength >= 1);
    len = TEN_LOG_RADIX * n->ilength;
    H = n->inumber[0];
    if(H < 10)
        len -= TEN_LOG_RADIX - 1;
    else if(H < 100)
        len -= TEN_LOG_RADIX - 2;
    else if(H < 1000)
        len -= TEN_LOG_RADIX - 3;
#if !WORD32
    else if(H < 10000)
        len -= TEN_LOG_RADIX - 4;
    else if(H < 100000)
        len -= TEN_LOG_RADIX - 5;
    else if(H < 1000000)
        len -= TEN_LOG_RADIX - 6;
    else if(H < 10000000)
        len -= TEN_LOG_RADIX - 7;
    else if(H < 100000000)
        len -= TEN_LOG_RADIX - 8;
#endif
    return len;
    }

static psk inumberNode(nnumber* g)
    {
    psk res;
    size_t len;
    len = offsetof(sk, u.obj) + numlength(g);
    res = (psk)bmalloc(__LINE__, len + 1);
    if(g->sign & QNUL)
        res->u.obj = '0';
    else
        {
        iconvert2decimal(g, (char*)POBJ(res));
        }

    res->v.fl = READY | SUCCESS | QNUMBER BITWISE_OR_SELFMATCHING;
    res->v.fl |= g->sign;
    return res;
    }

/* Create a node from a number, only allocating memory for the node if the
number hasn't the right size (== too large). If new memory is allocated,
the number's memory is deallocated. */
static psk numberNode2(nnumber* g)
    {
    psk res;
    size_t neededlen;
    size_t availablelen = g->allocated;
    neededlen = offsetof(sk, u.obj) + 1 + g->length;
    if((neededlen - 1) / sizeof(LONG) == (availablelen - 1) / sizeof(LONG))
        {
        res = (psk)g->alloc;
        if(g->sign & QNUL)
            {
            res->u.lobj = 0;
            res->u.obj = '0';
            }
        else
            {
            char* end = (char*)POBJ(res) + g->length;
            char* limit = (char*)g->alloc + (1 + (availablelen - 1) / sizeof(LONG)) * sizeof(LONG);
            memmove((void*)POBJ(res), g->number, g->length);
            while(end < limit)
                *end++ = '\0';
            }
        }
    else
        {
        res = (psk)bmalloc(__LINE__, neededlen);
        if(g->sign & QNUL)
            res->u.obj = '0';
        else
            {
            memcpy((void*)POBJ(res), g->number, g->length);
            /*(char *)POBJ(res) + g.length = '\0'; not necessary, is done in bmalloc */
            }
        bfree(g->alloc);
        }
    res->v.fl = READY | SUCCESS | QNUMBER BITWISE_OR_SELFMATCHING;
    res->v.fl |= g->sign;
    return res;
    }

static Qnumber not_a_number(void)
    {
    Qnumber res;
    res = copyof(&zeroNode);
    res->v.fl ^= SUCCESS;
    return res;
    }

static void convert2binary(nnumber* x)
    {
    LONG* ipointer;
    char* charpointer;
    ptrdiff_t n;

    x->ilength = x->iallocated = ((x->sign & QNUL ? 1 : x->length) + TEN_LOG_RADIX - 1) / TEN_LOG_RADIX;
    x->inumber = x->ialloc = (LONG*)bmalloc(__LINE__, sizeof(LONG) * x->iallocated);

    for(ipointer = x->inumber
        , charpointer = x->number
        , n = x->length
        ; ipointer < x->inumber + x->ilength
        ; ++ipointer
        )
        {
        *ipointer = 0;
        do
            {
            *ipointer = 10 * (*ipointer) + *charpointer++ - '0';
            } while(--n % TEN_LOG_RADIX != 0);
        }
    assert((LONG)TEN_LOG_RADIX * x->ilength >= charpointer - x->number);
    }

static char* isplit(Qnumber _qget, nnumber* ptel, nnumber* pnoem)
    {
    ptel->sign = _qget->v.fl & (MINUS | QNUL);
    pnoem->sign = 0;
    pnoem->alloc = ptel->alloc = NULL;
    ptel->number = (char*)POBJ(_qget);
    if(_qget->v.fl & QFRACTION)
        {
        char* on = strchr(ptel->number, '/');
        assert(on);
        ptel->length = on - ptel->number;
        pnoem->number = on + 1;
        pnoem->length = strlen(on + 1);
        convert2binary(ptel);
        convert2binary(pnoem);
        return on;
        }
    else
        {
        assert(!(_qget->v.fl & QFRACTION));
        ptel->length = strlen(ptel->number);
        pnoem->number = "1";
        pnoem->length = 1;
        convert2binary(ptel);
        convert2binary(pnoem);
        return NULL;
        }
    }

static char* split(Qnumber _qget, nnumber* ptel, nnumber* pnoem)
    {
    ptel->sign = _qget->v.fl & (MINUS | QNUL);
    pnoem->sign = 0;
    pnoem->alloc = ptel->alloc = NULL;
    ptel->number = (char*)POBJ(_qget);

    if(_qget->v.fl & QFRACTION)
        {
        char* on = strchr(ptel->number, '/');
        assert(on);
        ptel->length = on - ptel->number;
        pnoem->number = on + 1;
        pnoem->length = strlen(on + 1);
        return on;
        }
    else
        {
        assert(!(_qget->v.fl & QFRACTION));
        ptel->length = strlen(ptel->number);
        pnoem->number = "1";
        pnoem->length = 1;
        return NULL;
        }
    }


static int addSubtractFinal(char* i1, char* i2, char tmp, char** pres, char* bx)
    {
    for(; i2 >= bx;)
        {
        tmp += (*i2);
        if(tmp > '9')
            {
            *i1-- = tmp - 10;
            tmp = 1;
            }
        else if(tmp < '0')
            {
            *i1-- = tmp + 10;
            tmp = -1;
            }
        else
            {
            *i1-- = tmp;
            tmp = 0;
            }
        i2--;
        }
    *pres = i1 + 1;
    return tmp;
    }


static int increase(char** pres, char* bx, char* ex, char* by, char* ey)
    {
    char* i1 = *pres, * i2 = ex, * ypointer = ey;
    char tmp = 0;
    do
        {
        tmp += (*i2 + *ypointer - '0');
        if(tmp > '9')
            {
            *i1-- = tmp - 10;
            tmp = 1;
            }
        else
            {
            *i1-- = tmp;
            tmp = 0;
            }
        --i2;
        --ypointer;
        } while(ypointer >= by);
        return addSubtractFinal(i1, i2, tmp, pres, bx);
    }

static int decrease(char** pres, char* bx, char* ex, char* by, char* ey)
    {
    char* i1 = *pres, * i2 = ex, * ypointer = ey;
    char tmp = 0;
    do
        {
        tmp += (*i2 - *ypointer + '0');
        if(tmp < '0')
            {
            *i1-- = tmp + 10;
            tmp = -1;
            }
        else
            {
            *i1-- = tmp;
            tmp = 0;
            }
        --i2;
        --ypointer;
        } while(ypointer >= by);
        return addSubtractFinal(i1, i2, tmp, pres, bx);
    }



static void skipnullen(nnumber* nget, int Sign)
    {
    for(
        ; nget->length > 0 && *(nget->number) == '0'
        ; nget->number++, nget->length--
        )
        ;
    nget->sign = nget->length ? (Sign & MINUS) : QNUL;
    }
/*
multiply number with 204586 digits with itself
New algorithm:   17,13 s
Old algorithm: 1520,32 s

{?} get$longint&!x*!x:?y&lst$(y,verylongint,NEW)&ok
*/
#ifndef NDEBUG
static void pbint(LONG* high, LONG* low)
    {
    for(; high <= low; ++high)
        {
        if(*high)
            {
            printf(LONGD " ", *high);
            break;
            }
        else
            printf("NUL ");
        }
    for(; ++high <= low;)
        {
        printf(LONG0nD " ", (int)TEN_LOG_RADIX, *high);
        }
    }

static void fpbint(FILE* fp, LONG* high, LONG* low)
    {
    for(; high <= low; ++high)
        {
        if(*high)
            {
            fprintf(fp, LONGD " ", *high);
            break;
            }
        else
            fprintf(fp, "NUL ");
        }
    for(; ++high <= low;)
        {
        fprintf(fp, LONG0nD " ", (int)TEN_LOG_RADIX, *high);
        }
    }

static void validt(LONG* high, LONG* low)
    {
    for(; high <= low; ++high)
        {
        assert(0 <= *high);
        assert(*high < RADIX);
        }
    }

static void valid(nnumber* res)
    {
    validt(res->inumber, res->inumber + res->ilength - 1);
    }
#endif

static void nTimes(nnumber* x, nnumber* y, nnumber* product)
    {
    LONG* I1, * I2;
    LONG* ipointer, * itussen;

    assert(product->length == 0);
    assert(product->ilength == 0);
    assert(product->alloc == 0);
    assert(product->ialloc == 0);
    product->ilength = product->iallocated = x->ilength + y->ilength;
    assert(product->iallocated > 0);
    product->inumber = product->ialloc = (LONG*)bmalloc(__LINE__, sizeof(LONG) * product->iallocated);

    for(ipointer = product->inumber; ipointer < product->inumber + product->ilength; *ipointer++ = 0)
        ;

    for(I1 = x->inumber + x->ilength - 1; I1 >= x->inumber; I1--)
        {
        itussen = --ipointer; /* pointer to result, starting from LSW. */
        assert(itussen >= product->inumber);
        for(I2 = y->inumber + y->ilength - 1; I2 >= y->inumber; I2--)
            {
            LONG prod;
            LONG* itussen2;
            prod = (*I1) * (*I2);
            *itussen += prod;
            itussen2 = itussen--;
            while(*itussen2 >= HEADROOM * RADIX2)
                {
                LONG karry;
                karry = *itussen2 / RADIX;
                *itussen2 %= RADIX;
                --itussen2;
                assert(itussen2 >= product->inumber);
                *itussen2 += karry;
                }
            assert(itussen2 >= product->inumber);
            }
        if(*ipointer >= RADIX)
            {
            LONG karry = *ipointer / RADIX;
            *ipointer %= RADIX;
            itussen = ipointer - 1;
            assert(itussen >= product->inumber);
            *itussen += karry;
            while(*itussen >= HEADROOM * RADIX2/* 2000000000 */)
                {
                karry = *itussen / RADIX;
                *itussen %= RADIX;
                --itussen;
                assert(itussen >= product->inumber);
                *itussen += karry;
                }
            assert(itussen >= product->inumber);
            }
        }
    while(ipointer >= product->inumber)
        {
        if(*ipointer >= RADIX)
            {
            LONG karry = *ipointer / RADIX;
            *ipointer %= RADIX;
            --ipointer;
            assert(ipointer >= product->inumber);
            *ipointer += karry;
            }
        else
            --ipointer;
        }

    for(ipointer = product->inumber; product->ilength > 1 && *ipointer == 0; ++ipointer)
        {
        --(product->ilength);
        ++(product->inumber);
        }

    assert(product->ilength >= 1);
    assert(product->inumber >= (LONG*)product->ialloc);
    assert(product->inumber < (LONG*)product->ialloc + product->iallocated);
    assert(product->inumber + product->ilength <= (LONG*)product->ialloc + product->iallocated);

    product->sign = product->inumber[0] ? ((x->sign ^ y->sign) & MINUS) : QNUL;
    }

static LONG iAddSubtractFinal(LONG* highRemainder, LONG* lowRemainder, LONG carry)
    {
    while(highRemainder <= lowRemainder)
        {
        carry += *lowRemainder;
        if(carry >= RADIX)
            {
            /* 99999999999999999/10000000001+1 */
            /* 99999999999999999999999999999999/100000000000000000000001+1 */
            *lowRemainder = carry - RADIX;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = 1;
            }
        else if(carry < 0)
            {
            *lowRemainder = carry + RADIX;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = -1;
            }
        else
            {
            *lowRemainder = carry;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = 0;
            }
        --lowRemainder;
        }
    return carry;
    }


static LONG iAdd(LONG* highRemainder, LONG* lowRemainder, LONG* highDivisor, LONG* lowDivisor)
    {
    LONG carry = 0;
    assert(*highRemainder == 0);
    do
        {
        assert(lowRemainder >= highRemainder);
        carry += (*lowRemainder + *lowDivisor);
        if(carry >= RADIX)
            {
            *lowRemainder = carry - RADIX;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = 1;
            }
        else
            {
            *lowRemainder = carry;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = 0;
            }
        --lowRemainder;
        --lowDivisor;
        } while(highDivisor <= lowDivisor);
        assert(*highRemainder == 0);
        return iAddSubtractFinal(highRemainder, lowRemainder, carry);
    }

static LONG iSubtract(LONG* highRemainder
                      , LONG* lowRemainder
                      , LONG* highDivisor
                      , LONG* lowDivisor
                      , LONG factor
)
    {
    LONG carry = 0;
    do
        {
        assert(lowRemainder >= highRemainder);
        assert(*lowDivisor >= 0);
        carry += (*lowRemainder - factor * (*lowDivisor));
        if(carry < 0)
            {
            LONG f = 1 + ((-carry - 1) / RADIX);
            *lowRemainder = carry + f * RADIX;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = -f;
            }
        else
            {
            *lowRemainder = carry;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = 0;
            }
        --lowRemainder;
        --lowDivisor;
        } while(highDivisor <= lowDivisor);
        return iAddSubtractFinal(highRemainder, lowRemainder, carry);
    }

static LONG iSubtract2(LONG* highRemainder
                       , LONG* lowRemainder
                       , LONG* highDivisor
                       , LONG* lowDivisor
                       , LONG factor
)
    {
    int allzero = TRUE;
    LONG carry = 0;
    LONG* slowRemainder = lowRemainder;
    do
        {
        assert(highRemainder <= lowRemainder);
        assert(*lowDivisor >= 0);
        carry += (*lowRemainder - factor * (*lowDivisor));
        if(carry < 0)
            {
            LONG f = 1 + ((-carry - 1) / RADIX);
            *lowRemainder = carry + f * RADIX;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = -f;
            }
        else
            {
            *lowRemainder = carry;
            assert(*lowRemainder >= 0);
            assert(*lowRemainder < RADIX);
            carry = 0;
            }
        if(*lowRemainder)
            allzero = FALSE;
        --lowRemainder;
        --lowDivisor;
        } while(highDivisor <= lowDivisor);
        assert(highRemainder > lowRemainder);
        if(carry == 0)
            {
            assert(carry == 0);
            assert(0 <= *highRemainder);
            assert(*highRemainder < RADIX);
            return *highRemainder == 0 ? (allzero ? 0 : 1) : 1;
            }
        ++lowRemainder;
        assert(carry != 0);
        /* negative result */
        assert(carry < 0);
        {
        assert(lowRemainder == highRemainder);
        *lowRemainder += carry * RADIX;
        assert(*lowRemainder < 0);
        assert(-((HEADROOM + 1) * RADIX) < HEADROOM * *lowRemainder && HEADROOM * *lowRemainder < ((HEADROOM + 1)* RADIX));
        lowRemainder = slowRemainder;
        carry = 0;
        while(lowRemainder >= highRemainder)
            {
            *lowRemainder = carry - *lowRemainder;
            if(*lowRemainder < 0)
                {
                *lowRemainder += RADIX;
                carry = -1;
                }
            else
                carry = 0;
            assert(0 <= *lowRemainder && (*lowRemainder < RADIX || (lowRemainder == highRemainder && HEADROOM * *lowRemainder < ((HEADROOM + 1)* RADIX))));
            if(*lowRemainder)
                allzero = FALSE;
            --lowRemainder;
            }
        assert(carry == 0);

        assert(0 <= *++lowRemainder && HEADROOM * *lowRemainder < ((HEADROOM + 1)* RADIX));
        if(*lowRemainder == 0)
            return allzero ? 0 : -1;
        else
            return -1;
        }
    }

static LONG nnDivide(nnumber* dividend, nnumber* divisor, nnumber* quotient, nnumber* remainder)
    {
    LONG* low, * quot, * head, * oldhead;
    /* Remainder starts out as copy of dividend. As the division progresses,
       the leading element of the remainder moves to the right.
       The movement is interupted if the divisor is greater than the
       leading elements of the dividend.
    */
    LONG divRADIX, divRADIX2; /* leading one or two elements of divisor */
    assert(!(divisor->sign & QNUL));

    assert(remainder->length == 0);
    assert(remainder->ilength == 0);
    assert(remainder->alloc == 0);
    assert(remainder->ialloc == 0);
    assert(quotient->length == 0);
    assert(quotient->ilength == 0);
    assert(quotient->alloc == 0);
    assert(quotient->ialloc == 0);

    remainder->ilength = remainder->iallocated = dividend->sign & QNUL ? 1 : dividend->ilength;
    assert(remainder->iallocated > 0);
    assert((LONG)TEN_LOG_RADIX * remainder->ilength >= dividend->length);
    remainder->inumber = remainder->ialloc = (LONG*)bmalloc(__LINE__, sizeof(LONG) * remainder->iallocated);
    *(remainder->inumber) = 0;
    assert(dividend->ilength != 0);/*if(dividend->ilength == 0)
        remainder->inumber[0] = 0;
    else*/
    memcpy(remainder->inumber, dividend->inumber, dividend->ilength * sizeof(LONG));

    if(dividend->ilength >= divisor->ilength)
        quotient->ilength = quotient->iallocated = 1 + dividend->ilength - divisor->ilength;
    else
        quotient->ilength = quotient->iallocated = 1;

    quotient->inumber = quotient->ialloc = (LONG*)bmalloc(__LINE__, (size_t)(quotient->iallocated) * sizeof(LONG));
    memset(quotient->inumber, 0, (size_t)(quotient->iallocated) * sizeof(LONG));
    quot = quotient->inumber;

    divRADIX = divisor->inumber[0];
    if(divisor->ilength > 1)
        {
        divRADIX2 = RADIX * divisor->inumber[0] + divisor->inumber[1];
        }
    else
        {
        divRADIX2 = 0;
        }
    assert(divRADIX > 0);
    for(low = remainder->inumber + (size_t)divisor->ilength - 1
        , head = oldhead = remainder->inumber
        ; low < remainder->inumber + dividend->ilength
        ; ++low, ++quot, ++head
        )
        {
        LONG sign = 1;
        LONG factor;
        *quot = 0;
        if(head > oldhead)
            {
            *head += RADIX * *oldhead;
            *oldhead = 0;
            oldhead = head;
            }
        do
            {
            assert(low < remainder->inumber + remainder->ilength);
            assert(quot < quotient->inumber + quotient->ilength);
            assert(sign != 0);

            assert(*head >= 0);
            factor = *head / divRADIX;
            if(sign == -1 && factor == 0)
                ++factor;

            if(factor == 0)
                {
                break;
                }
            else
                {
                LONG nsign;
                assert(factor > 0);
                if(divRADIX2)
                    {
                    assert(head + 1 <= low);
                    if(head[0] < HEADROOM * RADIX)
                        {
                        factor = (RADIX * head[0] + head[1]) / divRADIX2;
                        if(sign == -1 && factor == 0)
                            ++factor;
                        }
                    }
                /*
                div$(20999900000000,2099990001) -> factor 10499
                    (RADIX 4 HEADROOM 20)
                div$(2999000000,2999001)        -> factor 1499
                    (RADIX 3 HEADROOM 2)
                */
                assert(factor * HEADROOM < RADIX* (HEADROOM + 1));
                *quot += sign * factor;
                assert(0 <= *quot);
                /*assert(*quot < RADIX);*/
                nsign = iSubtract2(head
                                   , low
                                   , divisor->inumber
                                   , divisor->inumber + divisor->ilength - 1
                                   , factor
                );
                assert(*head >= 0);
                assert(sign != 0);
                if(nsign < 0)
                    sign = -sign;
                else if(nsign == 0)
                    sign = 0;
                }
            } while(sign < 0);
            /*
            checkBounds(remainder->ialloc);
            checkBounds(quotient->ialloc);
            checkBounds(remainder->ialloc);
            checkBounds(quotient->ialloc);
            */
            assert(*quot < RADIX);
        }
    /*
    checkBounds(remainder->ialloc);
    checkBounds(quotient->ialloc);
    */
    for(low = remainder->inumber; remainder->ilength > 1 && *low == 0; ++low)
        {
        --(remainder->ilength);
        ++(remainder->inumber);
        }

    assert(remainder->ilength >= 1);
    assert(remainder->inumber >= (LONG*)remainder->ialloc);
    assert(remainder->inumber < (LONG*)remainder->ialloc + remainder->iallocated);
    assert(remainder->inumber + remainder->ilength <= (LONG*)remainder->ialloc + remainder->iallocated);

    for(low = quotient->inumber; quotient->ilength > 1 && *low == 0; ++low)
        {
        --(quotient->ilength);
        ++(quotient->inumber);
        }

    assert(quotient->ilength >= 1);
    assert(quotient->inumber >= (LONG*)quotient->ialloc);
    assert(quotient->inumber < (LONG*)quotient->ialloc + quotient->iallocated);
    assert(quotient->inumber + quotient->ilength <= (LONG*)quotient->ialloc + quotient->iallocated);
    remainder->sign = remainder->inumber[0] ? ((dividend->sign ^ divisor->sign) & MINUS) : QNUL;
    quotient->sign = quotient->inumber[0] ? ((dividend->sign ^ divisor->sign) & MINUS) : QNUL;
    /*
    checkBounds(remainder->ialloc);
    checkBounds(quotient->ialloc);
    */
    return TRUE;
    }


static Qnumber nn2q(nnumber* num, nnumber* den)
    {
    Qnumber res;
    assert(!(num->sign & QNUL));
    assert(!(den->sign & QNUL));
    /*    if(num->sign & QNUL)
            return copyof(&zeroNode);
        else if(den->sign & QNUL)
            return not_a_number();*/

    num->sign ^= (den->sign & MINUS);

    if(den->ilength == 1 && den->inumber[0] == 1)
        {
        res = inumberNode(num);
        }
    else
        {
        char* endp;
        size_t len = offsetof(sk, u.obj) + 2 + numlength(num) + numlength(den);
        res = (psk)bmalloc(__LINE__, len);
        endp = iconvert2decimal(num, (char*)POBJ(res));
        *endp++ = '/';
        endp = iconvert2decimal(den, endp);
        assert((size_t)(endp - (char*)res) <= len);
        res->v.fl = READY | SUCCESS | QNUMBER | QFRACTION BITWISE_OR_SELFMATCHING;
        res->v.fl |= num->sign;
        }
    return res;
    }

static Qnumber qnDivide(nnumber* x, nnumber* y)
    {
    Qnumber res;
    nnumber gcd = { 0 }, hrem = { 0 };
    nnumber quotientx = { 0 }, remainderx = { 0 };
    nnumber quotienty = { 0 }, remaindery = { 0 };

#ifndef NDEBUG
    valid(x);
    valid(y);
#endif
    if(x->sign & QNUL)
        return copyof(&zeroNode);
    else if(y->sign & QNUL)
        return not_a_number();

    gcd = *x;
    gcd.alloc = NULL;
    gcd.ialloc = NULL;
    hrem = *y;
    hrem.alloc = NULL;
    hrem.ialloc = NULL;
    do
        {
        nnDivide(&gcd, &hrem, &quotientx, &remainderx);
#ifndef NDEBUG
        valid(&gcd);
        valid(&hrem);
        valid(&quotientx);
        valid(&remainderx);
#endif
        if(gcd.ialloc)
            {
            bfree(gcd.ialloc);
            }
        gcd = hrem;
        hrem = remainderx;
        remainderx.alloc = 0;
        remainderx.length = 0;
        bfree(quotientx.ialloc);
        remainderx.ialloc = 0;
        remainderx.ilength = 0;
        quotientx.ialloc = 0;
        quotientx.alloc = 0;
        quotientx.ilength = 0;
        quotientx.length = 0;
        } while(!(hrem.sign & QNUL));

        if(hrem.ialloc)
            bfree(hrem.ialloc);

        nnDivide(x, &gcd, &quotientx, &remainderx);
#ifndef NDEBUG
        valid(&gcd);
        valid(x);
        valid(&quotientx);
        valid(&remainderx);
#endif
        bfree(remainderx.ialloc);
        nnDivide(y, &gcd, &quotienty, &remaindery);
#ifndef NDEBUG
        valid(&gcd);
        valid(y);
        valid(&quotienty);
        valid(&remaindery);
#endif
        bfree(remaindery.ialloc);

        if(gcd.ialloc)
            bfree(gcd.ialloc);
        res = nn2q(&quotientx, &quotienty);
        bfree(quotientx.ialloc);
        bfree(quotienty.ialloc);
        return res;
    }


static nnumber nPlus(nnumber* x, nnumber* y)
    {
    nnumber res = { 0 };
    char* hres;
    ptrdiff_t xGreaterThany;
    res.length = 1 + (x->length > y->length ? x->length : y->length);
    res.allocated = (size_t)res.length + offsetof(sk, u.obj);
    res.alloc = res.number = (char*)bmalloc(__LINE__, res.allocated);
    *res.number = '0';
    hres = res.number + (size_t)res.length - 1;
    if(x->length == y->length)
        xGreaterThany = strncmp(x->number, y->number, (size_t)res.length);
    else
        xGreaterThany = x->length - y->length;
    if(xGreaterThany < 0)
        {
        nnumber* hget = x;
        x = y;
        y = hget;
        }
    if(x->sign == y->sign)
        {
        if(increase(&hres, x->number, x->number + x->length - 1, y->number, y->number + y->length - 1))
            *--hres = '1';
        }
    else
        decrease(&hres, x->number, x->number + x->length - 1, y->number, y->number + y->length - 1);
    skipnullen(&res, x->sign);
    return res;
    }

static void nnSPlus(nnumber* x, nnumber* y, nnumber* som)
    {
    LONG* hres, * px, * py, * ex;
    ptrdiff_t xGreaterThany;

    som->ilength = 1 + (x->ilength > y->ilength ? x->ilength : y->ilength);
    som->iallocated = som->ilength;
    som->ialloc = som->inumber = (LONG*)bmalloc(__LINE__, sizeof(LONG) * som->iallocated);
    *som->inumber = 0;
    hres = som->inumber + som->ilength;

    if(x->ilength == y->ilength)
        {
        px = x->inumber;
        ex = px + x->ilength;
        py = y->inumber;
        do
            {
            xGreaterThany = *px++ - *py++;
            } while(!xGreaterThany && px < ex);
        }
    else
        xGreaterThany = x->ilength - y->ilength;
    if(xGreaterThany < 0)
        {
        nnumber* hget = x;
        x = y;
        y = hget;
        }

    for(px = x->inumber + x->ilength; px > x->inumber;)
        *--hres = *--px;
    if(x->sign == y->sign)
        {
#ifndef NDEBUG
        LONG carry =
#endif
            iAdd(som->inumber, som->inumber + som->ilength - 1, y->inumber, y->inumber + y->ilength - 1);
        assert(carry == 0);
        }
    else
        {
        iSubtract(som->inumber, som->inumber + som->ilength - 1, y->inumber, y->inumber + y->ilength - 1, 1);
        }
    for(hres = som->inumber; som->ilength > 1 && *hres == 0; ++hres)
        {
        --(som->ilength);
        ++(som->inumber);
        }

    som->sign = som->inumber[0] ? (x->sign & MINUS) : QNUL;
    }

static Qnumber qPlus(Qnumber _qx, Qnumber _qy, int minus)
    {
    nnumber xt = { 0 }, xn = { 0 }, yt = { 0 }, yn = { 0 };

    char* xb, * yb;
    xb = split(_qx, &xt, &xn);
    yb = split(_qy, &yt, &yn);
    yt.sign ^= minus;

    if(!xb && !yb)
        {
        nnumber g = nPlus(&xt, &yt);
        psk res = numberNode2(&g);
        return res;
        }
    else
        {
        nnumber pa = { 0 }, pb = { 0 }, som = { 0 };
        Qnumber res;
        convert2binary(&xt);
        convert2binary(&xn);
        convert2binary(&yt);
        convert2binary(&yn);
        nTimes(&xt, &yn, &pa);
        nTimes(&yt, &xn, &pb);
        nnSPlus(&pa, &pb, &som);
        bfree(pa.ialloc);
        bfree(pb.ialloc);
        pa.ilength = 0;
        pa.ialloc = 0;
        pa.length = 0;
        pa.alloc = 0;
        nTimes(&xn, &yn, &pa);
        res = qnDivide(&som, &pa);
        if(som.ialloc)
            bfree(som.ialloc);
        bfree(pa.ialloc);
        bfree(xt.ialloc);
        bfree(xn.ialloc);
        bfree(yt.ialloc);
        bfree(yn.ialloc);
        return res;
        }
    }

#if 1
/*

{?} 0:?n&whl'(1+!n:<1000:?n&5726597846592437657823456/67678346259784659237457297*5364523564325982435082458045728395/543895704328725085743289570+1:?T)&!T
{!} 62694910002908910202610946081623305253537463604246972909/75122371034222274084834066940872899586533484041821
    S   0,98 sec  (16500.31292.1022)


70
1,71*10E0 s Loop implemented as circular datastructure.
1,79*10E0 s Loop implemented with variable name evaluation.
1,85*10E0 s Loop implemented with member name evaluation
1,49*10E0 s Loop implemented with whl built-in function
80
210
Done. No errors. See valid.txt
ok
{!} ok
    S   7,29 sec  (16497.31239.1022)
*/

static Qnumber qDivideMultiply(nnumber* x1, nnumber* x2, nnumber* y1, nnumber* y2)
    {
    nnumber pa = { 0 }, pb = { 0 };
    Qnumber res;

    nTimes(x1, y1, &pa);
    nTimes(x2, y2, &pb);
    res = qnDivide(&pa, &pb);
    bfree(pa.ialloc);
    bfree(pb.ialloc);
    return(res);
    }
#else
/*
{?} 0:?n&whl'(1+!n:<1000:?n&707957265978333334659243765782345642321377768009873333/676799876699834625978465923745729787673565217476876234*536452356432598243508245804572839536543879878347777777/113555480476987659789060958543895703333333333333333333+1:?T)&!T
{!} 62694910002908910202610946081623305253537463604246972909/75122371034222274084834066940872899586533484041821
    S   1,03 sec  (1437.1463.2)


70
1,69*10E0 s Loop implemented as circular datastructure.
1,76*10E0 s Loop implemented with variable name evaluation.
1,87*10E0 s Loop implemented with member name evaluation
1,45*10E0 s Loop implemented with whl built-in function
80
210
Done. No errors. See valid.txt
ok
{!} ok
    S   7,32 sec  (16497.31239.1022)

70
1,82*10E0 s Loop implemented as circular datastructure.
2,06*10E0 s Loop implemented with variable name evaluation.
1,89*10E0 s Loop implemented with member name evaluation
1,45*10E0 s Loop implemented with whl built-in function
80
210
Done. No errors. See valid.txt
ok
{!} ok
    S   7,89 sec  (16497.31292.1022)

70
1,68*10E0 s Loop implemented as circular datastructure.
1,79*10E0 s Loop implemented with variable name evaluation.
1,90*10E0 s Loop implemented with member name evaluation
1,45*10E0 s Loop implemented with whl built-in function
80
210
Done. No errors. See valid.txt
ok
{!} ok
    S   7,26 sec  (16497.31292.1022)
*/

static Qnumber qDivideMultiply2(nnumber* x1, nnumber* x2, nnumber* y1, nnumber* y2)
    {
    nnumber pa = { 0 }, pb = { 0 };
    Qnumber res;
    nTimes(x1, y1, &pa);
    nTimes(x2, y2, &pb);
    res = nn2q(&pa, &pb);
    bfree(pa.alloc);
    bfree(pb.alloc);
    return(res);
    }

static Qnumber qTimes2(Qnumber _qx, Qnumber _qy)
    {
    nnumber xt = { 0 }, xn = { 0 }, yt = { 0 }, yn = { 0 };
    char* xb, * yb;
    xb = split(_qx, &xt, &xn);
    yb = split(_qy, &yt, &yn);
    if(!xb && !yb)
        {
        nnumber g = { 0 };
        nTimes(&xt, &yt, &g);
        psk res = numberNode2(&g);
        return res;
        }
    else
        {
        Qnumber res;
        res = qDivideMultiply2(&xt, &xn, &yt, &yn);
        return res;
        }
    }

static int nnDivide(nnumber* x, nnumber* y)
    {
    division resx, resy;
    nnumber gcd = { 0 }, hrem = { 0 };

    if(x->sign & QNUL)
        return 0; /* zero */
    else if(y->sign & QNUL)
        return -1; /* not a number */

    gcd = *x;
    gcd.alloc = NULL;
    hrem = *y;
    hrem.alloc = NULL;
    do
        {
        nnDivide(&gcd, &hrem, &resx);
        if(gcd.alloc)
            bfree(gcd.alloc);
        gcd = hrem;
        hrem = resx.remainder;
        bfree(resx.quotient.alloc);
        } while(!(hrem.sign & QNUL));

        if(hrem.alloc)
            bfree(hrem.alloc);

        nnDivide(x, &gcd, &resx);
        bfree(resx.remainder.alloc);

        nnDivide(y, &gcd, &resy);
        bfree(resy.remainder.alloc);
        if(gcd.alloc)
            bfree(gcd.alloc);
        if(x->alloc)
            bfree(x->alloc);
        if(y->alloc)
            bfree(y->alloc);
        *x = resx.quotient;
        *y = resy.quotient;
        return 2;
    }

/*Meant to be a faster multiplication of rational numbers
Assuming that the two numbers are reduced, we only need to
reduce the numerators with the denominators of the other
number. Numerators and denominators may get smaller, speeding
up the following multiplication.
*/
static Qnumber qDivideMultiply(nnumber* x1, nnumber* x2, nnumber* y1, nnumber* y2)
    {
    Qnumber res;
    nnumber z1 = { 0 }, z2 = { 0 };
    nnDivide(x1, y2);
    nnDivide(y1, x2);
    nnDivide(x1, x2);
    nnDivide(y1, y2);
    nTimes(x1, y1, &z1);
    nTimes(x2, y2, &z2);
    if(x1->alloc) bfree(x1->alloc);
    if(x2->alloc) bfree(x2->alloc);
    if(y1->alloc) bfree(y1->alloc);
    if(y2->alloc) bfree(y2->alloc);
    res = nn2q(&z1, &z2);
    if(z1.alloc) bfree(z1.alloc);
    if(z2.alloc) bfree(z2.alloc);
    return res;
    }
#endif

static Qnumber qTimes(Qnumber _qx, Qnumber _qy)
    {
    Qnumber res;
    nnumber xt = { 0 }, xn = { 0 }, yt = { 0 }, yn = { 0 };
    char* xb, * yb;

    xb = isplit(_qx, &xt, &xn);
    yb = isplit(_qy, &yt, &yn);
    if(!xb && !yb)
        {
        nnumber g = { 0 };
        nTimes(&xt, &yt, &g);
        bfree(xt.ialloc);
        bfree(xn.ialloc);
        bfree(yt.ialloc);
        bfree(yn.ialloc);
        res = inumberNode(&g);
        bfree(g.ialloc);
        return res;
        }
    else
        {
        res = qDivideMultiply(&xt, &xn, &yt, &yn);
        bfree(xt.ialloc);
        bfree(xn.ialloc);
        bfree(yt.ialloc);
        bfree(yn.ialloc);
        return res;
        }
    }

static Qnumber qqDivide(Qnumber _qx, Qnumber _qy)
    {
    nnumber xt = { 0 }, xn = { 0 }, yt = { 0 }, yn = { 0 };
    char* xb, * yb;

    xb = isplit(_qx, &xt, &xn);
    yb = isplit(_qy, &yt, &yn);
    if(!xb && !yb)
        {
        Qnumber res = qnDivide(&xt, &yt);
        bfree(xt.ialloc);
        bfree(xn.ialloc);
        bfree(yt.ialloc);
        bfree(yn.ialloc);
        return res;
        }
    else
        {
        Qnumber res;
        res = qDivideMultiply(&xt, &xn, &yn, &yt);
        bfree(xt.ialloc);
        bfree(xn.ialloc);
        bfree(yt.ialloc);
        bfree(yn.ialloc);
        return res;
        }
    }


static Qnumber qIntegerDivision(Qnumber _qx, Qnumber _qy)
    {
    Qnumber res, aqnumber;
    nnumber xt = { 0 }, xn = { 0 }, yt = { 0 }, yn = { 0 }, p1 = { 0 }, p2 = { 0 }, quotient = { 0 }, remainder = { 0 };

    isplit(_qx, &xt, &xn);
    isplit(_qy, &yt, &yn);

    nTimes(&xt, &yn, &p1);
    nTimes(&xn, &yt, &p2);
    nnDivide(&p1, &p2, &quotient, &remainder);
    bfree(p1.ialloc);
    bfree(p2.ialloc);
    res = qPlus
    ((remainder.sign & QNUL)
     || !(_qx->v.fl & MINUS)
     ? &zeroNode
     : (_qy->v.fl & MINUS)
     ? &oneNode
     : &minusOneNode
     , (aqnumber = inumberNode(&quotient))
     , 0
    );
    pskfree(aqnumber);
    bfree(quotient.ialloc);
    bfree(remainder.ialloc);
    bfree(xt.ialloc);
    bfree(xn.ialloc);
    bfree(yt.ialloc);
    bfree(yn.ialloc);
    return res;
    }

static Qnumber qModulo(Qnumber _qx, Qnumber _qy)
    {
    Qnumber res, _q2, _q3;

    _q2 = qIntegerDivision(_qx, _qy);
    _q3 = qTimes(_qy, _q2);
    pskfree(_q2);
    res = qPlus(_qx, _q3, MINUS);
    pskfree(_q3);
    return res;
    }

static psk qDenominator(psk Pnode)
    {
    Qnumber _qx = Pnode->RIGHT;
    psk res;
    size_t len;
    nnumber xt = { 0 }, xn = { 0 };
    split(_qx, &xt, &xn);
    len = offsetof(sk, u.obj) + 1 + xn.length;
    res = (psk)bmalloc(__LINE__, len);
    assert(!(xn.sign & QNUL)); /*Because RATIONAL_COMP(_qx)*/
    memcpy((void*)POBJ(res), xn.number, xn.length);
    res->v.fl = READY | SUCCESS | QNUMBER BITWISE_OR_SELFMATCHING;
    res->v.fl |= xn.sign;
    wipe(Pnode);
    return res;
    }

static int qCompare(Qnumber _qx, Qnumber _qy)
    {
    Qnumber som;
    int res;
    som = qPlus(_qx, _qy, MINUS);
    res = som->v.fl & (MINUS | QNUL);
    pskfree(som);
    return res;
    }


static int equal(psk kn1, psk kn2)
    {
    while(kn1 != kn2)
        {
        int r;
        if(is_op(kn1))
            {
            if(is_op(kn2))
                {
                r = (int)Op(kn2) - (int)Op(kn1);
                if(r)
                    {
                    return r;
                    }
                r = equal(kn1->LEFT, kn2->LEFT);
                if(r)
                    return r;
                kn1 = kn1->RIGHT;
                kn2 = kn2->RIGHT;
                }
            else
                return 1;
            }
        else if(is_op(kn2))
            return -1;
        else if(RATIONAL_COMP(kn1))
            {
            if(RATIONAL_COMP(kn2))
                {
                switch(qCompare(kn1, kn2))
                    {
                    case MINUS: return -1;
                    case QNUL:
                        {
                        return 0;
                        }
                    default: return 1;
                    }
                }
            else
                return -1;
            }
        else if(RATIONAL_COMP(kn2))
            {
            return 1;
            }
        else
            return (r = HAS_MINUS_SIGN(kn1) - HAS_MINUS_SIGN(kn2)) == 0
            ? strcmp((char*)POBJ(kn1), (char*)POBJ(kn2))
            : r;
        }
    return 0;
    }

static int cmp(psk kn1, psk kn2);

static int cmpsub(psk kn1, psk kn2)
    {
    int ret;
    psk pnode = NULL;
    addr[1] = kn1;
    pnode = build_up(pnode, "1+\1+-1)", NULL);
    pnode = eval(pnode);
    ret = cmp(pnode, kn2);
    wipe(pnode);
    return ret;
    }

static int cmp(psk kn1, psk kn2)
    {
    while(kn1 != kn2)
        {
        int r;
        if(is_op(kn1))
            {
            if(is_op(kn2))
                {
                r = (int)Op(kn2) - (int)Op(kn1);
                if(r)
                    {
                    /*{?} x^(y*(a+b))+-1*x^(a*y+b*y) => 0 */ /*{!} 0 */
                    if(Op(kn1) == TIMES
                       && Op(kn2) == PLUS
                       && is_op(kn1->RIGHT)
                       && Op(kn1->RIGHT) == PLUS
                       )
                        {
                        return cmpsub(kn1, kn2);
                        }
                    else if(Op(kn2) == TIMES
                            && Op(kn1) == PLUS
                            && is_op(kn2->RIGHT)
                            && Op(kn2->RIGHT) == PLUS
                            )
                        {
                        /*{?} a^(b*d+c*d) * (a^(b*f+c*f)+a^(e*b+e*c)) + -1*(a^((b+c)*(d+e))+a^((b+c)*(d+f))) => 0 */
                        return -cmpsub(kn2, kn1);
                        }
                    return r;
                    }
                switch(Op(kn1))
                    {
                    case PLUS:
                    case EXP:
                        r = cmp(kn1->LEFT, kn2->LEFT);
                        if(r)
                            return r;
                        kn1 = kn1->RIGHT;
                        kn2 = kn2->RIGHT;
                        break;
                    default:
                        r = equal(kn1->LEFT, kn2->LEFT);
                        if(r)
                            return r;
                        else
                            return equal(kn1->RIGHT, kn2->RIGHT);
                    }
                }
            else
                return 1;
            }
        else if(is_op(kn2))
            return -1;
        else if(RATIONAL_COMP(kn1))
            {
            if(RATIONAL_COMP(kn2))
                {
                switch(qCompare(kn1, kn2))
                    {
                    case MINUS: return -1;
                    case QNUL:
                        {
                        return 0;
                        }
                    default: return 1;
                    }
                }
            else
                return -1;
            }
        else if(RATIONAL_COMP(kn2))
            {
            return 1;
            }
        else
            return (r = HAS_MINUS_SIGN(kn1) - HAS_MINUS_SIGN(kn2)) == 0
            ? strcmp((char*)POBJ(kn1), (char*)POBJ(kn2))
            : r;
        }
    return 0;
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

static int update(psk name, psk pnode) /* name = tree with DOT in root */
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


static int insert(psk name, psk pnode)
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
        newvar = (vars*)bmalloc(__LINE__, sizeof(vars));
        if(*strng)
            {
#if ICPY
            MEMCPY(newvar->vname = (unsigned char*)
                   bmalloc(__LINE__, len + 1), strng, (len >> LOGWORDLENGTH) + 1);
#else
            MEMCPY(newvar->vname = (unsigned char*)
                   bmalloc(__LINE__, len + 1), strng, ((len / sizeof(LONG)) + 1) * sizeof(LONG));
#endif
            }
#else
        if(len < 4)
            newvar = (vars*)bmalloc(__LINE__, sizeof(vars));
        else
            newvar = (vars*)bmalloc(__LINE__, sizeof(vars) - 3 + len);
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

static int copy_insert(psk name, psk pnode, psk cutoff)
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

static psk scopy(const char* str)
    {
    int nr = fullnumbercheck(str) & ~DEFINITELYNONUMBER;
    psk pnode;
    if(nr & MINUS)
        { /* bracmat out$arg$() -123 */
        pnode = (psk)bmalloc(__LINE__, sizeof(ULONG) + strlen((const char*)str));
        strcpy((char*)(pnode)+sizeof(ULONG), str + 1);
        }
    else
        {
        pnode = (psk)bmalloc(__LINE__, sizeof(ULONG) + 1 + strlen((const char*)str));
        strcpy((char*)(pnode)+sizeof(ULONG), str);
        }
    pnode->v.fl = READY | SUCCESS | nr;
    return pnode;
    }

static psk charcopy(const char* strt, const char* until)
    {
    int  nr = 0;
    psk pnode;
    if('0' <= *strt && *strt <= '9')
        {
        nr = QNUMBER BITWISE_OR_SELFMATCHING;
        if(*strt == '0')
            nr |= QNUL;
        }
    pnode = (psk)bmalloc(__LINE__, sizeof(ULONG) + 1 + (until - strt));
    strncpy((char*)(pnode)+sizeof(ULONG), strt, until - strt);
    pnode->v.fl = READY | SUCCESS | nr;
    return pnode;
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

static int icopy_insert(psk name, LONG number)
    {
    char buf[22];
    sprintf((char*)buf, LONGD, number);
    return scopy_insert(name, buf);
    }

static int string_copy_insert(psk name, psk pnode, char* str, char* cutoff)
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
        psnode = (stringrefnode*)bmalloc(__LINE__, sizeof(stringrefnode));
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
        DBGSRC(int saveNice; int redhum; saveNice = beNice; \
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

static int getCodePoint(const char** ps)
    {
    /*
    return values:
    > 0:    code point
    -1:     no UTF-8
    -2:     too short for being UTF-8
    */
    int K;
    const char* s = *ps;
    if((K = (const unsigned char)*s++) != 0)
        {
        if((K & 0xc0) == 0xc0) /* 11bbbbbb */
            {
            int k[6];
            int i;
            int I;
            if((K & 0xfe) == 0xfe) /* 11111110 */
                {
                return -1;
                }
            /* Start of multibyte */

            k[0] = K;
            for(i = 1; (K << i) & 0x80; ++i)
                {
                k[i] = (const unsigned char)*s++;
                if((k[i] & 0xc0) != 0x80) /* 10bbbbbb */
                    {
                    if(k[i])
                        {
                        return -1;
                        }
                    return -2;
                    }
                }
            K = ((k[0] << i) & 0xff) << (5 * i - 6);
            I = --i;
            while(i > 0)
                {
                K |= (k[i] & 0x3f) << ((I - i) * 6);
                --i;
                }
            if(K <= 0x7F) /* ASCII, must be a single byte */
                {
                return -1;
                }
            }
        else if((K & 0xc0) == 0x80) /* 10bbbbbb, wrong first byte */
            {
            return -1;
            }
        }
    *ps = s; /* next character */
    return K;
    }

static int hasUTF8MultiByteCharacters(const char* s)
    { /* returns 0 if s is not valid UTF-8 or if s is pure 7-bit ASCII */
    int ret;
    int multiByteCharSeen = 0;
    for(; (ret = getCodePoint(&s)) > 0;)
        if(ret > 0x7F)
            ++multiByteCharSeen;
    return ret == 0 ? multiByteCharSeen : 0;
    }

static int getCodePoint2(const char** ps, int* isutf)
    {
    int ks = *isutf ? getCodePoint(ps) : (const unsigned char)*(*ps)++;
    if(ks < 0)
        {
        *isutf = 0;
        ks = (const unsigned char)*(*ps)++;
        }
    assert(ks >= 0);
    return ks;
    }

static int utf8bytes(ULONG val)
    {
    if(val < 0x80)
        {
        return 1;
        }
    else if(val < 0x0800) /* 7FF = 1 1111 111111 */
        {
        return 2;
        }
    else if(val < 0x10000) /* FFFF = 1111 111111 111111 */
        {
        return 3;
        }
    else if(val < 0x200000)
        { /* 10000 = 010000 000000 000000, 10ffff = 100 001111 111111 111111 */
        return 4;
        }
    else if(val < 0x4000000)
        {
        return 5;
        }
    else
        {
        return 6;
        }
    }

/* extern, is called from xml.c json.c */
char* putCodePoint(ULONG val, char* s)
    {
    /* Converts Unicode character w to 1,2,3 or 4 bytes of UTF8 in s. */
    if(val < 0x80)
        {
        *s++ = (char)val;
        }
    else
        {
        if(val < 0x0800) /* 7FF = 1 1111 111111 */
            {
            *s++ = (char)(0xc0 | (val >> 6));
            }
        else
            {
            if(val < 0x10000) /* FFFF = 1111 111111 111111 */
                {
                *s++ = (char)(0xe0 | (val >> 12));
                }
            else
                {
                if(val < 0x200000)
                    { /* 10000 = 010000 000000 000000, 10ffff = 100 001111 111111 111111 */
                    *s++ = (char)(0xf0 | (val >> 18));
                    }
                else
                    {
                    if(val < 0x4000000)
                        {
                        *s++ = (char)(0xf8 | (val >> 24));
                        }
                    else
                        {
                        if(val < 0x80000000)
                            {
                            *s++ = (char)(0xfc | (val >> 30));
                            *s++ = (char)(0x80 | ((val >> 24) & 0x3f));
                            }
                        else
                            return NULL;
                        }
                    *s++ = (char)(0x80 | ((val >> 18) & 0x3f));
                    }
                *s++ = (char)(0x80 | ((val >> 12) & 0x3f));
                }
            *s++ = (char)(0x80 | ((val >> 6) & 0x3f));
            }
        *s++ = (char)(0x80 | (val & 0x3f));
        }
    *s = (char)0;
    return s;
    }

static int strcasecompu(char** S, char** P, char* cutoff)
/* Additional argument cutoff */
    {
    int sutf = 1;
    int putf = 1;
    char* s = *S;
    char* p = *P;
    while(s < cutoff && *s && *p)
        {
        int diff;
        char* ns = s;
        char* np = p;
        int ks = getCodePoint2((const char**)&ns, &sutf);
        int kp = getCodePoint2((const char**)&np, &putf);
        assert(ks >= 0 && kp >= 0);
        diff = toLowerUnicode(ks) - toLowerUnicode(kp);
        if(diff)
            {
            *S = s;
            *P = p;
            return diff;
            }
        s = ns;
        p = np;
        }
    *S = s;
    *P = p;
    return (s < cutoff ? (int)(unsigned char)*s : 0) - (int)(unsigned char)*p;
    }


static int strcasecomp(const char* s, const char* p)
    {
    int sutf = 1; /* assume UTF-8, becomes 0 if it is not */
    int putf = 1;
    while(*s && *p)
        {
        int ks = getCodePoint2((const char**)&s, &sutf);
        int kp = getCodePoint2((const char**)&p, &putf);
        int diff = toLowerUnicode(ks) - toLowerUnicode(kp);
        if(diff)
            {
            return diff;
            }
        }
    return (int)(const unsigned char)*s - (int)(const unsigned char)*p;
    }

#if CODEPAGE850
static int strcasecmpDOS(const char* s, const char* p)
    {
    while(*s && *p)
        {
        int diff = (int)ISO8859toCodePage850(lowerEquivalent[CodePage850toISO8859((unsigned char)*s)]) - (int)ISO8859toCodePage850(lowerEquivalent[CodePage850toISO8859((unsigned char)*p)]);
        if(diff)
            return diff;
        ++s;
        ++p;
        }
    return (int)*s - (int)*p;
    }
#endif

#define PNOT ((p->v.fl & NOT) && (p->v.fl & FLGS) < NUMBER)
#define PGRT (p->v.fl & GREATER_THAN)
#define PSML (p->v.fl & SMALLER_THAN)
#define PUNQ  (PGRT && PSML)
#define EPGRT (PGRT && !PSML)
#define EPSML (PSML && !PGRT)

static int compare(psk s, psk p)
    {
    int Sign;
    if(RATIONAL_COMP(s) && RATIONAL_WEAK(p))
        Sign = qCompare(s, p);
    else
        {
        if(is_op(s))
            return NOTHING(p);
        if(PLOBJ(s) == IM && PLOBJ(p) == IM)
            {
            int TMP = ((s->v.fl & MINUS) ^ (p->v.fl & MINUS));
            int Njet = (p->v.fl & FLGS) < NUMBER && ((p->v.fl & NOT) && 1);
            int ul = (p->v.fl & (GREATER_THAN | SMALLER_THAN)) == (GREATER_THAN | SMALLER_THAN);
            int e1 = ((p->v.fl & GREATER_THAN) && 1);
            int e2 = ((p->v.fl & SMALLER_THAN) && 1);
            int ee = (e1 ^ e2) && 1;
            int R = !ee && (Njet ^ ul ^ !TMP);
            return R;
            }
        if((p->v.fl & (NOT | FRACTION | NUMBER | GREATER_THAN | SMALLER_THAN)) == (NOT | GREATER_THAN | SMALLER_THAN))
            { /* Case insensitive match: ~<> means "not different" */
            Sign = strcasecomp((char*)POBJ(s), (char*)POBJ(p));
            }
        else
            {
            Sign = strcmp((char*)POBJ(s), (char*)POBJ(p));
            }
        if(Sign > 0)
            Sign = 0;
        else if(Sign < 0)
            Sign = MINUS;
        else
            Sign = QNUL;
        }
    switch(Sign)
        {
        case 0:
            {
            return PNOT ^ (PGRT && 1);
            }
        case QNUL:
            {
            return !PNOT ^ (PGRT || PSML);
            }
        default:
            {
            return PNOT ^ (PSML && 1);
            }
        }
    }



#if CUTOFFSUGGEST
#define scompare(wh,s,c,p,suggestedCutOff,mayMoveStartOfSubject) scompare(s,c,p,suggestedCutOff,mayMoveStartOfSubject)
#else
#define scompare(wh,s,c,p) scompare(s,c,p)
#endif


/*
With the % flag on an otherwise numeric pattern, the pattern is treated
    as a string, not a number.
*/
#if CUTOFFSUGGEST
static int scompare(char* wh, char* s, char* cutoff, psk p, char** suggestedCutOff, char** mayMoveStartOfSubject)
#else
static int scompare(char* wh, unsigned char* s, unsigned char* cutoff, psk p)
#endif
    {
    int Sign = 0;
    char* P;
#if CUTOFFSUGGEST
    char* S = s;
#endif
    char sav;
    int smallerIfMoreDigitsAdded; /* -1/22 smaller than -1/2 */ /* 1/22 smaller than 1/2 */
    int samesign;
    int less;
    ULONG Flgs = p->v.fl;
    enum { NoIndication, AnInteger, NotAFraction, NotANumber, AFraction, ANumber };
    int status = NoIndication;

    if(NEGATION(Flgs, NONIDENT))
        {
        Flgs &= ~(NOT | NONIDENT);
        }
    if(NEGATION(Flgs, FRACTION))
        {
        Flgs &= ~(NOT | FRACTION);
        if(Flgs & NUMBER)
            status = AnInteger;
        else
            status = NotAFraction;
        }
    else if(NEGATION(Flgs, NUMBER))
        {
        Flgs &= ~(NOT | NUMBER);
        status = NotANumber;
        }
    else if(Flgs & FRACTION)
        status = AFraction;
    else if(Flgs & NUMBER)
        status = ANumber;


    if(!(Flgs & NONIDENT) /* % as flag on number forces comparison as string, not as number */
       && RATIONAL_WEAK(p)
       && (status != NotANumber)
       && ((((Flgs & (QFRACTION | IS_OPERATOR)) == QFRACTION)
            && (status != NotAFraction)
            )
           || (((Flgs & (QFRACTION | QNUMBER | IS_OPERATOR)) == QNUMBER)
               && (status != AFraction)
               )
           )
       )
        {
        int check = sfullnumbercheck(s, cutoff);
        if(check & QNUMBER)
            {
            int anythingGoes = 0;
            psk n = NULL;
            sav = *cutoff;
            *cutoff = '\0';
            n = build_up(n, s, NULL);
            *cutoff = sav;

            if(RAT_RAT(n))
                {
                smallerIfMoreDigitsAdded = 1;
                }
            else
                {
                smallerIfMoreDigitsAdded = 0;
                /* check whether there is a slash followed by non-zero decimal digit coming */
                if(!RAT_RAT(n))
                    {
                    char* t = cutoff;
                    for(; *t; ++t)
                        {
                        if(*t == '/')
                            {
                            if(('0' < *++t) && (*t <= '9'))
                                {
                                anythingGoes = 1;
                                }
                            break;
                            }
                        if('0' > *t || *t > '9')
                            break;
                        }
                    }
                }
            if(RAT_NEG_COMP(n))
                {
                less = !smallerIfMoreDigitsAdded;
                }
            else
                {
                less = smallerIfMoreDigitsAdded;
                }
            Sign = qCompare(n, p);
            samesign = ((n->v.fl & MINUS) == (p->v.fl & MINUS));
            wipe(n);
            switch(Flgs & (NOT | GREATER_THAN | SMALLER_THAN))
                {
                case NOT | GREATER_THAN | SMALLER_THAN:    /* n:~<>p */
                case 0:                                /* n:p */
                    {
                    /*
                                    n:p        n == p
                                    n:~<>p  same as n:p
                                        [n == p]
                                            TRUE | ONCE
                                        [n > p]
                                            if n < 0 && p < 0
                                                FALSE
                                            else
                                                ONCE
                                        [n < p]
                                            if n < 0
                                                ONCE
                                            else
                                                FALSE
                    */
                    switch(Sign)
                        {
                        case QNUL:    /* n == p */
                            if(anythingGoes)
                                return TRUE;
                            else
                                return TRUE | ONCE;
                        case 0:        /* n > p */
                            if(samesign && (anythingGoes || less))
                                return FALSE;
                            else
                                return ONCE;
                        default:    /* n < p */
                            if(samesign && (anythingGoes || !less))
                                return FALSE;
                            else
                                return ONCE;
                        }
                    }
                case SMALLER_THAN:    /* n:<p */
                    {
                    /*
                                    n:<p    n < p
                                        [n == p]
                                            if n < 0
                                                FALSE
                                            else
                                                ONCE
                                        [n > p]
                                            if n < 0
                                                FALSE
                                            else
                                                ONCE
                                        [n < p]
                                            TRUE
                    */
                    switch(Sign)
                        {
                        case QNUL:    /* n == p */
                            if(anythingGoes || less)
                                return FALSE;
                            else
                                return ONCE;
                        case 0:        /* n > p */
                            if(samesign && (anythingGoes || less))
                                return FALSE;
                            else
                                return ONCE;
                        default:    /* n < p */
                            return TRUE;
                        }
                    }
                case GREATER_THAN:    /* n:>p */
                    {
                    /*
                                    n:>p    n > p
                                        [n == p]
                                            if n < 0
                                                ONCE
                                            else
                                                FALSE
                                        [n > p]
                                            TRUE
                                        [n < p]
                                            if n < 0
                                                ONCE
                                            else
                                                FALSE
                    */
                    switch(Sign)
                        {
                        case 0:        /* n > p */
                            return TRUE;
                        case QNUL:/* n == p */
                            if(anythingGoes || !less)
                                return FALSE;
                            else
                                return ONCE;
                        default:    /* n < p */
                            if(samesign && (anythingGoes || !less))
                                return FALSE;
                            else
                                return ONCE;
                        }
                    }
                case GREATER_THAN | SMALLER_THAN:    /* n:<>p */
                case NOT:                        /* n:~p */
                    {
                    /*
                                    n:<>p   n != p
                                    n:~p    same as n:<>p
                                        [n == p]
                                            FALSE
                                        [n > p]
                                            TRUE
                                        [n < p]
                                            TRUE

                    */
                    switch(Sign)
                        {
                        case QNUL:    /* n == p */
                            return FALSE;
                        default:    /* n < p, n > p */
                            return TRUE;
                        }
                    }
                case NOT | SMALLER_THAN:    /* n:~<p */
                    {
                    /*
                                    n:~<p   n >= p
                                        [n == p]
                                            if n < 0
                                                TRUE | ONCE
                                            else
                                                TRUE
                                        [n > p]
                                            TRUE
                                        [n < p]
                                            if n < 0
                                                ONCE
                                            else
                                                FALSE
                    */
                    switch(Sign)
                        {
                        case QNUL:    /* n == p */
                            if(anythingGoes || !less)
                                return TRUE;
                            else
                                return TRUE | ONCE;
                        case 0:        /* n > p */
                            return TRUE;
                        default:    /* n < p */
                            if(samesign && (anythingGoes || !less))
                                return FALSE;
                            else
                                return ONCE;
                        }
                    }
                    /*case NOT|GREATER_THAN:*/    /* n:~>p */
                default:
                    {
                    /*
                                    n:~>p   n <= p
                                        [n == p]
                                            if n < p
                                                TRUE
                                            else
                                                TRUE | ONCE
                                        [n > p]
                                            if n < p
                                                FALSE
                                            else
                                                ONCE
                                        [n < p]
                                            TRUE
                    */
                    switch(Sign)
                        {
                        case QNUL:    /* n == p */
                            if(anythingGoes || less)
                                return TRUE;
                            else
                                return TRUE | ONCE;
                        case 0:        /* n > p */
                            if(samesign && (anythingGoes || less))
                                return FALSE;
                            else
                                return ONCE;
                        default:    /* n < p */
                            return TRUE;
                        }
                    }
                }
            /* End (check & QNUMBER) == TRUE. */
            /*printf("Not expected here!");getchar();*/
            }
        else if(((s == cutoff) && (Flgs & (NUMBER | FRACTION)))
                || ((s < cutoff)
                    && (((*s == '-') && (cutoff < s + 2))
                        || cutoff[-1] == '/'
                        )
                    )
                )
            {
            /* We can not yet discredit the subject as a number.
            With additional characters it may become a number. */
            return FALSE;
            }
        /* Subject is definitely not a number. */
        }

    P = (char*)SPOBJ(p);

    if((Flgs & (NOT | FRACTION | NUMBER | GREATER_THAN | SMALLER_THAN)) == (NOT | GREATER_THAN | SMALLER_THAN))
        { /* Case insensitive match: ~<> means "not different" */
        Sign = strcasecompu(&s, &P, cutoff); /* Additional argument cutoff */
        }
    else
        {
#if CUTOFFSUGGEST
        if(suggestedCutOff)
            {
            switch(Flgs & (NOT | GREATER_THAN | SMALLER_THAN))
                {
                case NOT | GREATER_THAN:    /* n:~>p */
                case SMALLER_THAN:    /* n:<p */
                case GREATER_THAN | SMALLER_THAN:    /* n:<>p */
                case NOT:                        /* n:~p */
                    {
                    while(((Sign = (s < cutoff ? (int)(unsigned char)*s : 0) - (int)(unsigned char)*P) == 0) && *P) /* Additional argument cutoff */
                        {
                        ++s;
                        ++P;
                        }
                    if(Sign > 0)
                        {
                        switch(Flgs & (NOT | GREATER_THAN | SMALLER_THAN))
                            {
                            case NOT | GREATER_THAN:
                            case SMALLER_THAN:
                                return ONCE;
                            default:
                                return TRUE;
                            }
                        }
                    else if(Sign == 0)
                        {
                        switch(Flgs & (NOT | GREATER_THAN | SMALLER_THAN))
                            {
                            case NOT | GREATER_THAN:
                                return TRUE | ONCE;
                            case SMALLER_THAN:
                                return ONCE;
                            default:
                                return FALSE;
                            }
                        }
                    return TRUE;
                    }
                case GREATER_THAN:    /* n:>p */
                    {
                    while(((Sign = (int)(unsigned char)*s - (int)(unsigned char)*P) == 0) && *s && *P)
                        {
                        ++s;
                        ++P;
                        }
                    if(s >= cutoff)
                        {
                        if(Sign > 0)
                            *suggestedCutOff = s + 1;
                        break;
                        }
                    return ONCE;
                    }
                case NOT | GREATER_THAN | SMALLER_THAN:    /* n:~<>p */
                case 0:                                /* n:p */
                    while(((Sign = (int)(unsigned char)*s - (int)(unsigned char)*P) == 0) && *s && *P)
                        {
                        ++s;
                        ++P;
                        }
                    if(s >= cutoff && *P == 0)
                        {
                        *suggestedCutOff = s;
                        /*Sign = 0;*/
                        if(mayMoveStartOfSubject)
                            *mayMoveStartOfSubject = 0;
                        return TRUE | ONCE;
                        }
                    if(mayMoveStartOfSubject && *mayMoveStartOfSubject != 0)
                        {
                        char* startpos;
                        /* #ifndef NDEBUG
                                                char * pat = (char *)POBJ(p);
                        #endif */
                        startpos = strstr((char*)S, (char*)POBJ(p));
                        if(startpos != 0)
                            {
                            if(Flgs & MINUS)
                                --startpos;
                            }
                        /*assert(  startpos == 0
                              || (startpos+strlen((char *)POBJ(p)) < cutoff - 1)
                              || (startpos+strlen((char *)POBJ(p)) >= cutoff)
                              );*/
                        *mayMoveStartOfSubject = (char*)startpos;
                        return ONCE;
                        }

                    if(Sign > 0 || s < cutoff)
                        {
                        return ONCE;
                        }
                    return FALSE;/*subject too short*/
                case NOT | SMALLER_THAN:    /* n:~<p */
                    while(((Sign = (int)(unsigned char)*s - (int)(unsigned char)*P) == 0) && *s && *P)
                        {
                        ++s;
                        ++P;
                        }
                    if(Sign >= 0)
                        {
                        if(s >= cutoff)
                            {
                            *suggestedCutOff = (*P) ? s + 1 : s;
                            }
                        return TRUE;
                        }
                    return ONCE;
                }
            }
        else
#endif
            {
#if CUTOFFSUGGEST
            switch(Flgs & (NOT | GREATER_THAN | SMALLER_THAN))
                {
#if 1
                case NOT | GREATER_THAN | SMALLER_THAN:    /* n:~<>p */
                case 0:                                /* n:p */
                    while(((Sign = (s < cutoff ? (int)(unsigned char)*s : 0) - (int)(unsigned char)*P) == 0) && *s && *P)
                        {
                        ++s;
                        ++P;
                        }
                    if(Sign != 0 && mayMoveStartOfSubject && *mayMoveStartOfSubject != 0)
                        {
                        char* startpos;
                        char* ep = SPOBJ(p) + strlen((char*)POBJ(p));
                        char* es = cutoff ? cutoff : S + strlen((char*)S);
                        while(ep > SPOBJ(p))
                            {
                            if(*--ep != *--es)
                                {
                                *mayMoveStartOfSubject = 0;
                                return ONCE;
                                }
                            }
                        startpos = es;
                        if(Flgs & MINUS)
                            --startpos;
                        if((char*)startpos > *mayMoveStartOfSubject)
                            *mayMoveStartOfSubject = (char*)startpos;
                        return ONCE;
                        }

                    break;
#endif
                default:
#else
                    {
#endif
                    while(((Sign = (s < cutoff ? (int)(unsigned char)*s : 0) - (int)(unsigned char)*P) == 0) && *P) /* Additional argument cutoff */
                        {
                        ++s;
                        ++P;
                        }
                }
            }
        }


    switch(Flgs & (NOT | GREATER_THAN | SMALLER_THAN))
        {
        case NOT | GREATER_THAN | SMALLER_THAN:    /* n:~<>p */
        case 0:                                /* n:p */
            {
            /*
                        n:p        n == p
                        n:~<>p  same as n:p
                            [n == p]
                                TRUE | ONCE
                            [n > p]
                                ONCE
                            [n < p]
                                FALSE
            */
            if(Sign == 0)
                {
                return TRUE | ONCE;
                }
            else if(Sign < 0 && s >= cutoff)
                {
                return FALSE;
                }
            return ONCE;
            }
        case SMALLER_THAN:    /* n:<p */
            {
            /*
                        n:<p    n < p
                            [n == p]
                                ONCE
                            [n > p]
                                ONCE
                            [n < p]
                                TRUE
            */
            if(Sign >= 0)
                {
                return ONCE;
                }
            return TRUE;
            }
        case GREATER_THAN:    /* n:>p */
            {
            /*
                        n:>p    n > p
                            [n == p]
                                FALSE
                            [n > p]
                                TRUE
                            [n < p]
                                FALSE
            */
            if(Sign > 0)
                {
                return TRUE;
                }
            else if(Sign < 0 && s < cutoff)
                {
                return ONCE;
                }
            return FALSE;
            }
        case GREATER_THAN | SMALLER_THAN:    /* n:<>p */
        case NOT:                        /* n:~p */
            {
            /*
                        n:<>p   n != p
                        n:~p    same as n:<>p
                            [n == p]
                                FALSE
                            [n > p]
                                TRUE
                            [n < p]
                                TRUE

            */
            if(Sign == 0)
                {
                return FALSE;
                }
            return TRUE;
            }
        case NOT | SMALLER_THAN:    /* n:~<p */
            {
            /*
                        n:~<p   n >= p
                            [n == p]
                                TRUE
                            [n > p]
                                TRUE
                            [n < p]
                                FALSE
            */
            if(Sign < 0)
                {
                if(s < cutoff)
                    {
                    return ONCE;
                    }
                else
                    {
                    return FALSE;
                    }
                }
            return TRUE;
            }
        case NOT | GREATER_THAN:    /* n:~>p */
        default:
            {
            /*
                        n:~>p   n <= p
                            [n == p]
                                TRUE | ONCE
                            [n > p]
                                ONCE
                            [n < p]
                                TRUE
            */
            if(Sign > 0)
                {
                return ONCE;
                }
            else if(Sign < 0)
                {
                return TRUE;
                }
            return TRUE | ONCE;
            }
        }
    }

static int psh(psk name, psk pnode, psk dim)
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
    m2 = power2(n);
    if(m2 == 0)
        m22 = 1;
    else
        m22 = m2 << 1;
    if(nxtvar->n >= m22)
        /* allocate */
        {
        for(; nxtvar->n >= m22; m22 <<= 1)
            {
            nvaria = (varia*)bmalloc(__LINE__, sizeof(varia) + (m22 - 1) * sizeof(psk));
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

typedef struct classdef
    {
    char* name;
    method* vtab;
    } classdef;

typedef struct pskRecord
    {
    psk entry;
    struct pskRecord* next;
    } pskRecord;

typedef int(*cmpfuncTp)(const char* s, const char* p);
typedef LONG(*hashfuncTp)(const char* s);

typedef struct Hash
    {
    pskRecord** hash_table;
    ULONG hash_size;
    ULONG elements;     /* elements >= record_count */
    ULONG record_count; /* record_count >= size - unoccupied */
    ULONG unoccupied;
    cmpfuncTp cmpfunc;
    hashfuncTp hashfunc;
    } Hash;

static LONG casesensitivehash(const char* cp)
    {
    LONG hash_temp = 0;
    while(*cp != '\0')
        {
        if(hash_temp < 0)
            {
            hash_temp = (hash_temp << 1) + 1;
            }
        else
            {
            hash_temp = hash_temp << 1;
            }
        hash_temp ^= *(const signed char*)cp;
        ++cp;
        }
    return hash_temp;
    }

static LONG caseinsensitivehash(const char* cp)
    {
    LONG hash_temp = 0;
    int isutf = 1;
    while(*cp != '\0')
        {
        if(hash_temp < 0)
            {
            hash_temp = (hash_temp << 1) + 1;
            }
        else
            {
            hash_temp = hash_temp << 1;
            }
        hash_temp ^= toLowerUnicode(getCodePoint2((const char**)&cp, &isutf));
        }
    return hash_temp;
    }

#if CODEPAGE850
static LONG caseinsensitivehashDOS(const char* cp)
    {
    LONG hash_temp = 0;
    while(*cp != '\0')
        {
        if(hash_temp < 0)
            hash_temp = (hash_temp << 1) + 1;
        else
            hash_temp = hash_temp << 1;
        hash_temp ^= ISO8859toCodePage850(lowerEquivalent[CodePage850toISO8859(*cp)]);
        ++cp;
        }
    return hash_temp;
    }
#endif

static psk removeFromHash(Hash * temp, psk Arg)
    {
    const char* key = (const char*)POBJ(Arg);
    LONG i;
    LONG hash_temp;
    pskRecord** pr;
    hash_temp = (*temp->hashfunc)(key);
    assert(temp->hash_size);
    i = ((unsigned int)hash_temp) % temp->hash_size;
    pr = temp->hash_table + i;
    if(*pr)
        {
        while(*pr)
            {
            if(Op((*pr)->entry) == WHITE)
                {
                if(!(*temp->cmpfunc)(key, (const char*)POBJ((*pr)->entry->LEFT->LEFT)))
                    break;
                }
            else if(!(*temp->cmpfunc)(key, (const char*)POBJ((*pr)->entry->LEFT)))
                break;
            pr = &(*pr)->next;
            }
        if(*pr)
            {
            pskRecord* next = (*pr)->next;
            psk ret = (*pr)->entry;
            bfree(*pr);
            *pr = next;
            --temp->record_count;
            return ret;
            }
        }
    return NULL;
    }

static psk inserthash(Hash * temp, psk Arg)
    {
    const char* key = (const char*)POBJ(Arg->LEFT);
    LONG i;
    LONG hash_temp;
    pskRecord* r;
    hash_temp = (*temp->hashfunc)(key);
    assert(temp->hash_size);
    i = ((unsigned int)hash_temp) % temp->hash_size;
    r = temp->hash_table[i];
    if(!r)
        --temp->unoccupied;
    else
        while(r)
            {
            if(Op(r->entry) == WHITE)
                {
                if(!(*temp->cmpfunc)(key, (const char*)POBJ(r->entry->LEFT->LEFT)))
                    {
                    break;
                    }
                }
            else
                {
                if(!(*temp->cmpfunc)(key, (const char*)POBJ(r->entry->LEFT)))
                    {
                    break;
                    }
                }
            r = r->next;
            }
    if(r)
        {
        psk goal = (psk)bmalloc(__LINE__, sizeof(knode));
        goal->v.fl = WHITE | SUCCESS;
        goal->v.fl &= ~ALL_REFCOUNT_BITS_SET;
        goal->LEFT = same_as_w(Arg);
        goal->RIGHT = r->entry;
        r->entry = goal;
        }
    else
        {
        r = (pskRecord*)bmalloc(__LINE__, sizeof(pskRecord));
        r->entry = same_as_w(Arg);
        r->next = temp->hash_table[i];
        temp->hash_table[i] = r;
        ++temp->record_count;
        }
    ++temp->elements;
    return r->entry;
    }

static psk findhash(Hash * temp, psk Arg)
    {
    const char* key = (const char*)POBJ(Arg);
    LONG i;
    LONG hash_temp;
    pskRecord* r;
    hash_temp = (*temp->hashfunc)(key);
    assert(temp->hash_size);
    i = ((unsigned int)hash_temp) % temp->hash_size;
    r = temp->hash_table[i];
    if(r)
        {
        while(r)
            {
            if(Op(r->entry) == WHITE)
                {
                if(!(*temp->cmpfunc)(key, (const char*)POBJ(r->entry->LEFT->LEFT)))
                    break;
                }
            else if(!(*temp->cmpfunc)(key, (const char*)POBJ(r->entry->LEFT)))
                break;
            r = r->next;
            }
        if(r)
            return r->entry;
        }
    return NULL;
    }

static void freehash(Hash * temp)
    {
    if(temp)
        {
        if(temp->hash_table)
            {
            ULONG i;
            for(i = temp->hash_size; i > 0;)
                {
                pskRecord* r = temp->hash_table[--i];
                pskRecord* next;
                while(r)
                    {
                    wipe(r->entry);
                    next = r->next;
                    bfree(r);
                    r = next;
                    }
                }
            bfree(temp->hash_table);
            }
        bfree(temp);
        }
    }

static Hash* newhash(ULONG size)
    {
    ULONG i;
    Hash* temp = (Hash*)bmalloc(__LINE__, sizeof(Hash));
    assert(size > 0);
    temp->hash_size = size;
    temp->record_count = (unsigned int)0;
    temp->hash_table = (pskRecord**)bmalloc(__LINE__, sizeof(pskRecord*) * temp->hash_size);
#ifdef __VMS
    temp->cmpfunc = (int(*)())strcmp;
#else
    temp->cmpfunc = strcmp;
#endif
    temp->hashfunc = casesensitivehash;
    temp->elements = 0L;     /* elements >= record_count */
    temp->record_count = 0L; /* record_count >= size - unoccupied */
    temp->unoccupied = size;
    for(i = temp->hash_size; i > 0;)
        temp->hash_table[--i] = NULL;
    return temp;
    }

static ULONG nextprime(ULONG g)
    {
    /* For primality test, only try divisors that are 2, 3 or 5 or greater
    numbers that are not multiples of 2, 3 or 5. Candidates below 100 are:
     2  3  5  7 11 13 17 19 23 29 31 37 41 43
    47 49 53 59 61 67 71 73 77 79 83 89 91 97
    Of these 28 candidates, three are not prime:
    49 (7*7), 77 (7*11) and 91 (7*13) */
    int i;
    ULONG smalldivisor;
    static int bijt[12] =
        { 1,  2,  2,  4,    2,    4,    2,    4,    6,    2,  6 };
    /*2-3,3-5,5-7,7-11,11-13,13-17,17-19,19-23,23-29,29-1,1-7*/
    ULONG bigdivisor;
    if(!(g & 1))
        {
        if(g <= 2)
            return 2;
        ++g;
        }
    smalldivisor = 2;
    i = 0;
    while((bigdivisor = g / smalldivisor) >= smalldivisor)
        {
        if(bigdivisor * smalldivisor == g)
            {
            g += 2;
            smalldivisor = 2;
            i = 0;
            }
        else
            {
            smalldivisor += bijt[i];
            if(++i > 10)
                i = 3;
            }
        }
    return g;
    }

static void rehash(Hash * *ptemp, int loadFactor/*1-100*/)
    {
    Hash* temp = *ptemp;
    if(temp)
        {
        ULONG newsize;
        Hash* newtable;
        newsize = nextprime((100 * temp->record_count) / loadFactor);
        if(!newsize)
            newsize = 1;
        newtable = newhash(newsize);
        newtable->cmpfunc = temp->cmpfunc;
        newtable->hashfunc = temp->hashfunc;
        if(temp->hash_table)
            {
            ULONG i;
            for(i = temp->hash_size; i > 0;)
                {
                pskRecord* r = temp->hash_table[--i];
                while(r)
                    {
                    psk Pnode = r->entry;
                    while(is_op(Pnode) && Op(Pnode) == WHITE)
                        {
                        inserthash(newtable, Pnode->LEFT);
                        Pnode = Pnode->RIGHT;
                        }
                    inserthash(newtable, Pnode);
                    r = r->next;
                    }
                }
            }
        freehash(temp);
        *ptemp = newtable;
        }
    }

static int loadfactor(Hash * temp)
    {
    assert(temp->hash_size);
    if(temp->record_count < 10000000L)
        return (int)((100 * temp->record_count) / temp->hash_size);
    else
        return (int)(temp->record_count / (temp->hash_size / 100));
    }

static Boolean hashinsert(struct typedObjectnode* This, ppsk arg)
    {
    psk Arg = (*arg)->RIGHT;
    if(is_op(Arg) && !is_op(Arg->LEFT))
        {
        psk ret;
        int lf = loadfactor(HASH(This));
        if(lf > 100)
            rehash((Hash**)(&(This->voiddata)), 60);
        ret = inserthash(HASH(This), Arg);
        wipe(*arg);
        *arg = same_as_w(ret);
        return TRUE;
        }
    return FALSE;
    }

static Boolean hashfind(struct typedObjectnode* This, ppsk arg)
    {
    psk Arg = (*arg)->RIGHT;
    if(!is_op(Arg))
        {
        psk ret = findhash(HASH(This), Arg);
        if(ret)
            {
            wipe(*arg);
            *arg = same_as_w(ret);
            return TRUE;
            }
        }
    return FALSE;
    }

static Boolean hashremove(struct typedObjectnode* This, ppsk arg)
    {
    psk Arg = (*arg)->RIGHT;
    if(!is_op(Arg))
        {
        Hash* temp = HASH(This);
        psk ret = removeFromHash(temp, Arg);
        if(ret)
            {
            if(loadfactor(temp) < 50 && temp->hash_size > 97)
                rehash(PHASH(This), 90);
            wipe(*arg);
            *arg = ret;
            return TRUE;
            }
        }
    return FALSE;
    }

static Boolean hashnew(struct typedObjectnode* This, ppsk arg)
    {
    /*    UNREFERENCED_PARAMETER(arg);*/
    unsigned long Nprime = 97;
    if(INTEGER_POS_COMP((*arg)->RIGHT))
        {
        Nprime = strtoul((char*)POBJ((*arg)->RIGHT), NULL, 10);
        if(Nprime == 0 || Nprime == ULONG_MAX)
            Nprime = 97;
        }
    VOID(This) = (void*)newhash(Nprime);
    return TRUE;
    }

static Boolean hashdie(struct typedObjectnode* This, ppsk arg)
    {
    UNREFERENCED_PARAMETER(arg);
    freehash(HASH(This));
    return TRUE;
    }

#if CODEPAGE850
static Boolean hashDOS(struct typedObjectnode* This, ppsk arg)
    {
    UNREFERENCED_PARAMETER(arg);
    (HASH(This))->hashfunc = caseinsensitivehashDOS;
    (HASH(This))->cmpfunc = strcasecmpDOS;
    rehash(PHASH(This), 100);
    return TRUE;
    }
#endif

static Boolean hashISO(struct typedObjectnode* This, ppsk arg)
    {
    UNREFERENCED_PARAMETER(arg);
    (HASH(This))->hashfunc = caseinsensitivehash;
    (HASH(This))->cmpfunc = strcasecomp;
    rehash(PHASH(This), 100);
    return TRUE;
    }

static Boolean hashcasesensitive(struct typedObjectnode* This, ppsk arg)
    {
    UNREFERENCED_PARAMETER(arg);
    (HASH(This))->hashfunc = casesensitivehash;
#ifdef __VMS
    (HASH(This))->cmpfunc = (int(*)())strcmp;
#else
    (HASH(This))->cmpfunc = strcmp;
#endif
    rehash(PHASH(This), 100);
    return TRUE;
    }


static Boolean hashforall(struct typedObjectnode* This, ppsk arg)
    {
    ULONG i;
    int ret = TRUE;
    This = (typedObjectnode*)same_as_w((psk)This);
    for(i = 0
        ;    ret && HASH(This)
        && i < (HASH(This))->hash_size
        ;
        )
        {
        pskRecord* r = (HASH(This))->hash_table[i];
        int j = 0;
        while(r)
            {
            int m;
            psk Pnode = NULL;
            addr[2] = (*arg)->RIGHT; /* each time! addr[n] may be overwritten by evaluate (below)*/
            addr[3] = r->entry;
            Pnode = build_up(Pnode, "(\2'\3)", NULL);
            Pnode = eval(Pnode);
            ret = isSUCCESSorFENCE(Pnode);
            wipe(Pnode);
            if(!ret
               || !HASH(This)
               || i >= (HASH(This))->hash_size
               || !(HASH(This))->hash_table[i]
               )
                break;
            ++j;
            for(m = 0, r = (HASH(This))->hash_table[i]
                ; r && m < j
                ; ++m
                )
                r = r->next;
            }
        ++i;
        }
    wipe((psk)This);
    return TRUE;
    }


method hash[] = {
    {"find",hashfind},
    {"insert",hashinsert},
    {"remove",hashremove},
    {"New",hashnew},
    {"Die",hashdie},
#if CODEPAGE850
    {"DOS",hashDOS},
#endif
    {"ISO",hashISO},
    {"casesensitive",hashcasesensitive},
    {"forall",hashforall},
    {NULL,NULL} };
/*
Standard methods are 'New' and 'Die'.
A user defined 'die' can be added after creation of the object and will be invoked just before 'Die'.

Examples:

( new$(=):?h
&   (
    =   ( List
        =
          .   out$List
            & lst$its
            & lst$Its
        )
        (die=.out$"Oh dear")
    )
  : (=?(h.))
& (h..List)$
& :?h
);

(   (
    =   ( List
        =
          .   out$List
            & lst$its
            & lst$Its
        )
        (die=.out$"The end.")
    )
  : (=?(new$(=):?k))
& (k..List)$
& :?k
);

( new$hash:?h
&   (
    =   ( List
        =
          .   out$List
            & lst$its
            & lst$Its
        )
        (die=.out$"Oh dear")
    )
  : (=?(h.))
& (h..List)$
& :?h
);

(   (
    =   ( List
        =
          .   out$List
            & lst$its
            & lst$Its
        )
        (die=.out$"The end.")
    )
  : (=?(new$hash:?k))
& (k..List)$
& lst$k
& :?k
);

*/

classdef classes[] = { {"hash",hash},{NULL,NULL} };

static method_pnt findBuiltInMethod(typedObjectnode * object, psk methodName)
    {
    if(!is_op(methodName))
        {
        return findBuiltInMethodByName(object, (const char*)POBJ(methodName));
        }
    return NULL;
    }

typedef struct
    {
    psk self;
    psk object;
    method_pnt theMethod;
    } objectStuff;


static psk getmember2(psk name, psk tree)
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
                return NULL;
            else if(nname == name)
                {
                return tree->RIGHT = Head(tree->RIGHT);
                }
            else
                {
                name = name->RIGHT;
                }
            }
        else
            {
            psk tmp;
            if((tmp = getmember2(name, tree->LEFT)) != NULL)
                {
                return tmp;
                }
            }
        tree = tree->RIGHT;
        }
    return NULL;
    }


static psk getmember(psk name, psk tree, objectStuff * Object)
    {
    /* Returns NULL if either the method is not found or if the method is
       built-in. In the latter case, Object->theMethod is set. */
    while(is_op(tree))
        {
        if(Op(tree) == EQUALS)
            {
            psk nname;
            if(ISBUILTIN((objectnode*)tree)
               && Op(name) == DOT
               )
                {
                Object->object = tree;  /* object == (=) */
                Object->theMethod = findBuiltInMethod((typedObjectnode*)tree, name->RIGHT);
                /* findBuiltInMethod((=),(insert)) */
                if(Object->theMethod)
                    {
                    return NULL;
                    }
                }

            if(Op(name) == DOT)
                nname = name->LEFT;
            else
                nname = name;
            if(equal(tree->LEFT, nname))
                return NULL;
            else if(nname == name)
                {
                return tree->RIGHT = Head(tree->RIGHT);
                }
            else
                {
                Object->self = tree->RIGHT;
                name = name->RIGHT;
                }
            }
        else
            {
            psk tmp;
            if((tmp = getmember(name, tree->LEFT, Object)) != NULL)
                {
                return tmp;
                }
            }
        tree = tree->RIGHT;
        }
    return NULL;
    }

static psk getValueByVariableName(psk namenode)
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

static psk getValue(psk namenode, int* newval)
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

static psk findMethod(psk namenode, objectStuff * Object)
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
            if(ISBUILTIN((objectnode*)(namenode->LEFT))
               )
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

static int deleteNode(psk name)
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
            if((nxtvar->n) - 1 < power2(nxtvar->n))
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

static psk SymbolBinding(psk variabele, int* newval, int twolevelsofindirection)
    {
    psk pbinding;
    *newval = 0;
    if((pbinding = getValue(variabele, newval)) != NULL)
        {
        if(twolevelsofindirection)
            {
            psk peval;

            if(pbinding->v.fl & INDIRECT)
                {
                peval = subtreecopy(pbinding);
                peval = eval(peval);
                if(!isSUCCESS(peval)
                   || (is_op(peval)
                       && Op(peval) != EQUALS
                       && Op(peval) != DOT
                       )
                   )
                    {
                    wipe(peval);
                    return 0;
                    }
                if(*newval)
                    wipe(pbinding);
                *newval = TRUE;
                pbinding = peval;
                }
            if(is_op(pbinding))
                {
                if(is_object(pbinding))
                    {
                    peval = same_as_w(pbinding);
                    }
                else
                    {
                    peval = subtreecopy(pbinding);
                    /*
                    a=b=(c.d)
                    c=(d=e)
                    f:?!!(a.b)
                    !e
                    f
                    */
                    peval = eval(peval);
                    if(!isSUCCESS(peval)
                       || (is_op(peval)
                           && Op(peval) != EQUALS
                           && Op(peval) != DOT
                           )
                       )
                        {
                        wipe(peval);
                        if(*newval)
                            {
                            *newval = FALSE;
                            wipe(pbinding);
                            }
                        return NULL;
                        }
                    }
                assert(pbinding);
                if(*newval)
                    {
                    *newval = FALSE;
                    wipe(pbinding);
                    }
                pbinding = SymbolBinding(peval, newval, (peval->v.fl & DOUBLY_INDIRECT));
                wipe(peval);
                }
            else
                {
                int newv = *newval;
                psk binding;
                *newval = FALSE;
                binding = SymbolBinding(pbinding, newval, pbinding->v.fl & DOUBLY_INDIRECT);
                if(newv)
                    {
                    wipe(pbinding);
                    }
                pbinding = binding;
                }
            }
        }
    return pbinding;
    }

static psk SymbolBinding_w(psk variabele, int twolevelsofindirection)
/* twolevelsofindirection because the variable not always can have the
bangs. Example:
  (A==B)  &  a b c:? [?!!A
first finds (=B), which is an object that should not obtain the flags !! as in
!!(=B), because that would have a side effect on A as A=!!(=B)
*/
    {
    psk pbinding;
    int newval;
    newval = FALSE;
    if((pbinding = SymbolBinding(variabele, &newval, twolevelsofindirection)) != NULL)
        {
        ULONG nameflags, valueflags;
        nameflags = (variabele->v.fl & (BEQUEST | SUCCESS));
        if(ANYNEGATION(variabele->v.fl))
            nameflags |= NOT;

        valueflags = (pbinding)->v.fl;
        valueflags |= (nameflags & (BEQUEST | NOT));
        valueflags ^= ((nameflags & SUCCESS) ^ SUCCESS);

        assert(pbinding != NULL);

        if(Op(pbinding) == EQUALS)
            {
            if(!newval)
                {
                pbinding->RIGHT = Head(pbinding->RIGHT);
                pbinding = same_as_w(pbinding);
                }
            }
        else if((pbinding)->v.fl == valueflags)
            {
            if(!newval)
                {
                pbinding = same_as_w(pbinding);
                }
            }
        else
            {
            assert(Op(pbinding) != EQUALS);
            if(newval)
                {
                pbinding = isolated(pbinding);
                }
            else
                {
                pbinding = subtreecopy(pbinding);
                }
            (pbinding)->v.fl = valueflags & COPYFILTER; /* ~ALL_REFCOUNT_BITS_SET;*/
            }
        }
    return pbinding;
    }

#if !DEBUGBRACMAT
#define match(IND,SUB,PAT,SNIJAF,POS,LENGTH,OP) match(SUB,PAT,SNIJAF,POS,LENGTH,OP)
#if CUTOFFSUGGEST
#define stringmatch(IND,WH,SUB,SNIJAF,PAT,PKN,POS,LENGTH,SUGGESTEDCUTOFF,MAYMOVESTARTOFSUBJECT) stringmatch(SUB,SNIJAF,PAT,PKN,POS,LENGTH,SUGGESTEDCUTOFF,MAYMOVESTARTOFSUBJECT)
#else
#define stringmatch(IND,WH,SUB,SNIJAF,PAT,PKN,POS,LENGTH) stringmatch(SUB,SNIJAF,PAT,PKN,POS,LENGTH)
#endif
#endif

static void cleanOncePattern(psk pat)
    {
    pat->v.fl &= ~IMPLIEDFENCE;
    if(is_op(pat))
        {
        cleanOncePattern(pat->LEFT);
        cleanOncePattern(pat->RIGHT);
        }
    }


static int stringOncePattern(psk pat)
    {
    /*
    This function has a side effect: it sets a flag in all pattern nodes that
    can be matched by at most one non-trivial list element (a nonzero term in
    a sum, a factor in a product that is not 1, or a nonempty word in a
    sentence. Because the function depends on ATOMFILTERS, the algorithm
    should be slightly different for normal matches and for string matches.
    Ideally, two flags should be reserved.
    */
    if(pat->v.fl & IMPLIEDFENCE)
        {
        return TRUE;
        }
    if(pat->v.fl & SATOMFILTERS)
        {
        pat->v.fl |= IMPLIEDFENCE;
        return TRUE;
        }
    else if(pat->v.fl & ATOMFILTERS)
        {
        return FALSE;
        }
    else if(IS_VARIABLE(pat)
            || NOTHING(pat)
            || (pat->v.fl & NONIDENT) /* @(abc:% c) */
            )
        {
        return FALSE;
        }
    else if(!is_op(pat))
        {
        if(!pat->u.obj)
            {
            pat->v.fl |= IMPLIEDFENCE;
            return TRUE;
            }
        else
            {
            return FALSE;
            }
        }
    else
        {
        switch(Op(pat))
            {
            case DOT:
            case COMMA:
            case EQUALS:
            case EXP:
            case LOG:
            case DIF:
                pat->v.fl |= IMPLIEDFENCE;
                return TRUE;
            case OR:
                if(stringOncePattern(pat->LEFT) && stringOncePattern(pat->RIGHT))
                    {
                    pat->v.fl |= IMPLIEDFENCE;
                    return TRUE;
                    }
                break;
            case MATCH:
                if(stringOncePattern(pat->LEFT) || stringOncePattern(pat->RIGHT))
                    {
                    pat->v.fl |= IMPLIEDFENCE;
                    return TRUE;
                    }
                break;
            case AND:
                if(stringOncePattern(pat->LEFT))
                    {
                    pat->v.fl |= IMPLIEDFENCE;
                    return TRUE;
                    }
                break;
            default:
                break;
            }
        }
    return FALSE;
    }


static int oncePattern(psk pat)
    {
    /*
    This function has a side effect: it sets a flag IMPLIEDFENCE in all
    pattern nodes that can be matched by at most one non-trivial list element
    (a nonzero term in a sum, a factor in a product that is not 1, or a
    nonempty word in a sentence. Because the function depends on ATOMFILTERS,
    the algorithm should be slightly different for normal matches and for
    string matches. Ideally, two flags should be reserved.
    */
    if(pat->v.fl & IMPLIEDFENCE)
        {
        return TRUE;
        }
    if((pat->v.fl & ATOM) && NEGATION(pat->v.fl, ATOM))
        {
        return FALSE;
        }
    else if(pat->v.fl & ATOMFILTERS)
        {
        pat->v.fl |= IMPLIEDFENCE;
        return TRUE;
        }
    else if(IS_VARIABLE(pat)
            || NOTHING(pat)
            || (pat->v.fl & NONIDENT) /*{?} a b c:% c => a b c */
            )
        return FALSE;
    else if(!is_op(pat))
        {
        pat->v.fl |= IMPLIEDFENCE;
        return TRUE;
        }
    else
        switch(Op(pat))
            {
            case DOT:
            case COMMA:
            case EQUALS:
            case LOG:
            case DIF:
                pat->v.fl |= IMPLIEDFENCE;
                return TRUE;
            case OR:
                if(oncePattern(pat->LEFT) && oncePattern(pat->RIGHT))
                    {
                    pat->v.fl |= IMPLIEDFENCE;
                    return TRUE;
                    }
                break;
            case MATCH:
                if(oncePattern(pat->LEFT) || oncePattern(pat->RIGHT))
                    {
                    pat->v.fl |= IMPLIEDFENCE;
                    return TRUE;
                    }
                break;
            case AND:
                if(oncePattern(pat->LEFT))
                    {
                    pat->v.fl |= IMPLIEDFENCE;
                    return TRUE;
                    }
                break;
            default:
                break;
            }
    return FALSE;
    }

#define SHIFT_SAV 0
#define SHIFT_LMR 8
#define SHIFT_RMR 16
#define SHIFT_ONCE 24

typedef union matchstate
    {
#ifndef NDEBUG
    struct
        {
        unsigned int bsave : 8;

        unsigned int blmr_true : 1;
        unsigned int blmr_success : 1; /* SUCCESS */
        unsigned int blmr_pristine : 1;
        unsigned int blmr_once : 1;
        unsigned int blmr_position_once : 1;
        unsigned int blmr_position_max_reached : 1;
        unsigned int blmr_fence : 1; /* FENCE */
        unsigned int blmr_unused_15 : 1;

        unsigned int brmr_true : 1;
        unsigned int brmr_success : 1; /* SUCCESS */
        unsigned int brmr_pristine : 1;
        unsigned int brmr_once : 1;
        unsigned int brmr_position_once : 1;
        unsigned int brmr_position_max_reached : 1;
        unsigned int brmr_fence : 1; /* FENCE */
        unsigned int brmr_unused_23 : 1;

        unsigned int unused_24_26 : 3;
        unsigned int bonce : 1;
        unsigned int unused_28_31 : 4;
        } b;
#endif
    struct
        {
        char sav;
        char lmr;
        char rmr;
        unsigned char once;
        } c;
    unsigned int i;
    } matchstate;

#if DEBUGBRACMAT
#ifndef NDEBUG
static void printMatchState(const char* msg, matchstate s, int pos, int len)
    {
    Printf("\n%s pos %d len %d once %d", msg, pos, len, s.b.bonce);
    Printf("\n     t o p m f i");
    Printf("\n lmr %d %d %d %d %d %d",
           s.b.blmr_true, s.b.blmr_once, s.b.blmr_position_once, s.b.blmr_position_max_reached, s.b.blmr_fence, s.b.blmr_pristine);
    Printf("\n rmr %d %d %d %d %d %d\n",
           s.b.brmr_true, s.b.brmr_once, s.b.brmr_position_once, s.b.brmr_position_max_reached, s.b.brmr_fence, s.b.brmr_pristine);
    }
#endif
#endif

static LONG expressionLength(psk Pnode, unsigned int op)
    {
    if(!is_op(Pnode) && Pnode->u.lobj == knil[op >> OPSH]->u.lobj)
        return 0;
    else
        {
        LONG len = 1;
        while(Op(Pnode) == op)
            {
            ++len;
            Pnode = Pnode->RIGHT;
            }
        return len;
        }
    }

static char doPosition(matchstate s, psk pat, LONG pposition, size_t stringLength, psk expr
#if CUTOFFSUGGEST
                       , char** mayMoveStartOfSubject
#endif
                       , unsigned int op
)
    {
    ULONG Flgs;
    psk name;
    LONG pos;
    Flgs = pat->v.fl;
#if CUTOFFSUGGEST
    if(((Flgs & (SUCCESS | VISIBLE_FLAGS_POS0 | IS_OPERATOR)) == (SUCCESS | QNUMBER))
       && mayMoveStartOfSubject
       && *mayMoveStartOfSubject != 0
       )
        {
        pos = toLong(pat); /* [20 */
        if(pos < 0)
            pos += (expr == NULL ? (LONG)stringLength : expr ? expressionLength(expr, op) : 0) + 1; /* [(20+-1*(!len+1)) -> `-7 */

        if(pposition < pos
           && (MORE_EQUAL(pat) || EQUAL(pat) || NOTLESSORMORE(pat))
           )
            {
            if((long)stringLength > pos)
                *mayMoveStartOfSubject += pos - pposition;
            s.c.rmr = FALSE; /* [20 */
            return s.c.rmr;
            }
        else if(pposition <= pos
                && MORE(pat)
                )
            {
            if((long)stringLength > pos)
                *mayMoveStartOfSubject += pos - pposition + 1;
            s.c.rmr = FALSE; /* [>5 */
            return s.c.rmr;
            }
        }
#endif
    Flgs = pat->v.fl & (UNIFY | INDIRECT | DOUBLY_INDIRECT);

    name = subtreecopy(pat);
    name->v.fl |= SUCCESS;
    if((Flgs & UNIFY) && (is_op(pat) || (Flgs & INDIRECT)))
        {
        name->v.fl &= ~VISIBLE_FLAGS;
        if(!is_op(name))
            name->v.fl |= READY;
        s.c.rmr = (char)evaluate(name) & TRUE;

        if(!(s.c.rmr))
            {
            wipe(name);
            return FALSE;
            }
        }
    else
        {
        s.c.rmr = (char)evaluate(name) & TRUE;

        if(!(s.c.rmr))
            {
            wipe(name);
            return FALSE;
            }

        Flgs = pat->v.fl & UNIFY;
        Flgs |= name->v.fl;
        }
    pat = name;
    if(Flgs & UNIFY)
        {
        if(is_op(pat)
           || pat->u.obj
           )
            {
            if(Flgs & INDIRECT)        /* ?! of ?!! */
                {
                psk loc;
                if((loc = SymbolBinding_w(pat, Flgs & DOUBLY_INDIRECT)) != NULL)
                    {
                    if(is_object(loc))
                        s.c.rmr = (char)icopy_insert(loc, pposition);
                    else
                        {
                        s.c.rmr = (char)evaluate(loc) & (TRUE | FENCE);
                        if(!icopy_insert(loc, pposition))
                            s.c.rmr = FALSE;
                        }
                    wipe(loc);
                    }
                else
                    s.c.rmr = FALSE;
                }
            else
                {
                s.c.rmr = (char)icopy_insert(pat, pposition); /* [?a */
                }
            }
        else
            s.c.rmr = TRUE;

        if(name)
            wipe(name); /* [?a */
        /*
          (   ( CharacterLength
              =   length c p q
                .     0:?length:?p
                    & @( !arg
                       :   ?
                           ( [!p %?c [?q
                           & utf$!c:?k
                           & !q:?p
                           & 1+!length:?length
                           & ~
                           )
                           ?
                       )
                  | !length
              )
            & CharacterLength$str$(a chu$1000 b chu$100000 c):5
          )
        */
        return (char)(ONCE | POSITION_ONCE | s.c.rmr);
        }

    if(((pat->v.fl & (SUCCESS | VISIBLE_FLAGS_POS0 | IS_OPERATOR)) == (SUCCESS | QNUMBER)))
        {
        pos = toLong(pat); /* [20 */
        if(pos < 0)
            pos += (expr == NULL ? (LONG)stringLength : expressionLength(expr, op)) + 1; /* [(20+-1*(!len+1)) -> `-7 */
        if(LESS(pat))
            { /* [<18 */
            if(pposition < pos)
                {
                s.c.rmr = TRUE;/* [<18 */
                }
            else
                {
                s.c.rmr = FALSE | POSITION_MAX_REACHED;
                }
            }
        else if(LESS_EQUAL(pat))
            {
            if(pposition < pos)
                {
                s.c.rmr = TRUE;
                }
            else if(pposition == pos)
                {
                s.c.rmr = TRUE | POSITION_MAX_REACHED;
                }
            else
                {
                s.c.rmr = FALSE | POSITION_MAX_REACHED;
                }
            }
        else if(MORE_EQUAL(pat))
            { /* [~<13 */
            if(pposition >= pos)
                {
                s.c.rmr = TRUE; /* [~<13 */
                }
            else
                {
                s.c.rmr = FALSE; /* [~<13 */
                }
            }
        else if(MORE(pat))
            { /* [>5 */
            if(pposition > pos)
                {
                s.c.rmr = TRUE; /* [>5 */
                }
            else
                {
                s.c.rmr = FALSE; /* [>5 */
                }
            }
        else if(UNEQUAL(pat) || LESSORMORE(pat))
            { /* [~13 */
            if(pposition != pos)
                {
                s.c.rmr = TRUE; /* [~13 */
                }
            else
                {
                s.c.rmr = FALSE; /* [~13 */
                }
            }
        else if(EQUAL(pat) || NOTLESSORMORE(pat))
            {
            if(pposition == pos)
                {
                s.c.rmr = TRUE | POSITION_MAX_REACHED; /* [20 */
                }
            else if(pposition > pos)
                {
                s.c.rmr = FALSE | POSITION_MAX_REACHED;
                }
            else
                s.c.rmr = FALSE; /* [20 */
            }
        }
    else
        {
        s.c.rmr = FALSE;
        }
    wipe(pat); /* [20 */
    s.c.rmr |= ONCE | POSITION_ONCE;
    return s.c.rmr;
    }

static int atomtest(psk pnode)
    {
    return (!is_op(pnode) && !HAS_UNOPS(pnode)) ? (int)pnode->u.obj : -1;
    }

static char sdoEval(char* sub, char* cutoff, psk pat, psk subkn)
    {
    char ret;
    psk loc;
    psh(&sjtNode, &nilNode, NULL);
    string_copy_insert(&sjtNode, subkn, sub, cutoff);
    loc = subtreecopy(pat);
    loc->v.fl &= ~(POSITION | NONIDENT | IMPLIEDFENCE | ONCE);
    loc = eval(loc);
    deleteNode(&sjtNode);
    if(isSUCCESS(loc))
        {
        ret = (loc->v.fl & FENCE) ? (TRUE | ONCE) : TRUE;
        }
    else
        {
        ret = (loc->v.fl & FENCE) ? ONCE : FALSE;
        }
    wipe(loc);
    return ret;
    }

/*
    ( Dogs and Cats are friends: ? [%(out$(!sjt SJT)&~) (|))&
    ( Dogs and Cats are friends: ? [%(out$(!sjt)&~) (|))&
*/
static char doEval(psk sub, psk cutoff, psk pat)
    {
    char ret;
    psk loc;
    psh(&sjtNode, &nilNode, NULL);
    copy_insert(&sjtNode, sub, cutoff);
    loc = subtreecopy(pat);
    loc->v.fl &= ~(POSITION | NONIDENT | IMPLIEDFENCE | ONCE);
    loc = eval(loc);
    deleteNode(&sjtNode);
    if(isSUCCESS(loc))
        {
        ret = (loc->v.fl & FENCE) ? (TRUE | ONCE) : TRUE;
        }
    else
        {
        ret = (loc->v.fl & FENCE) ? ONCE : FALSE;
        }
    wipe(loc);

    return ret;
    }


#if CUTOFFSUGGEST
static char stringmatch
(int ind
 , char* wh
 , char* sub
 , char* cutoff
 , psk pat
 , psk subkn
 , LONG pposition
 , size_t stringLength
 , char** suggestedCutOff
 , char** mayMoveStartOfSubject
)
#else
static char stringmatch
(int ind
 , char* wh
 , unsigned char* sub
 , unsigned char* cutoff
 , psk pat
 , psk subkn
 , LONG pposition
 , size_t stringLength
)
#endif
    {
    /*
    s.c.lmr and s.c.rmr have 3 independent bit fields : TRUE/FALSE, ONCE en FENCE.
    TRUE/FALSE Whether the match succeeds or fails.
    ONCE       Unwillingness of the pattern to match subjects that start in
               the same position, but end further "to the right".
               Of importance for pattern with space, + or * operator.
               Is turned on in pattern by the `@#/ prefixes and by the operators
               other than space + * _ & : | = $ '.
               Is turned of in pattern with space + * or | operator.
    FENCE      Unwillingness of the subject to be matched by alternative patterns.
               Of importance for the | and : operators in a pattern.
               Is turned on by the ` prefix (whether or not in a pattern).
               Is turned off in pattern with space + * | or : operator.
               (With | and : operators this is only the case for the left operand,
               with the other operators this is the case for all except the last
               operand in a list.)
    */
    psk loc;
    char* sloc;
    ULONG Flgs;
    matchstate s;
    int ci;
    psk name = NULL;
    assert(sizeof(s) == 4);
    if(!cutoff)
        cutoff = sub + stringLength;
#if CUTOFFSUGGEST
    if((pat->v.fl & ATOM)
       || (NOTHING(pat)
           && (is_op(pat)
               || !pat->u.obj)
           )
       )
        {
        suggestedCutOff = NULL;
        }
#endif
    DBGSRC(int saveNice; int redhum; saveNice = beNice; redhum = hum; beNice = FALSE; \
           hum = FALSE; Printf("%d  %.*s|%s", ind, (int)(cutoff - sub), sub, cutoff); \
           Printf(":"); result(pat); \
           Printf(",pos=" LONGD ",sLen=%ld,sugCut=%s,mayMoveStart=%s)"\
                  , pposition\
                  , (long int)stringLength\
                  , suggestedCutOff ? *suggestedCutOff ? *suggestedCutOff : (char*)"(0)" : (char*)"0"\
                  , mayMoveStartOfSubject ? *mayMoveStartOfSubject ? *mayMoveStartOfSubject : (char*)"(0)" : (char*)"0"\
           ); \
           Printf("\n"); beNice = saveNice; hum = redhum;)
        s.i = (PRISTINE << SHIFT_LMR) + (PRISTINE << SHIFT_RMR);

    Flgs = pat->v.fl;
    if(Flgs & POSITION)
        {
        if(Flgs & NONIDENT)
            return sdoEval(sub, cutoff, pat, subkn);
        else if(cutoff > sub)
            {
#if CUTOFFSUGGEST
            if(mayMoveStartOfSubject && *mayMoveStartOfSubject)
                {
                *mayMoveStartOfSubject = cutoff;
                }
#endif
            return FALSE | ONCE | POSITION_ONCE;
            }
        else
            return doPosition(s, pat, pposition, stringLength, NULL
#if CUTOFFSUGGEST
                              , mayMoveStartOfSubject
#endif
                              , 12345
            );
        }
    if(!(((Flgs & NONIDENT)
          && (NEGATION(Flgs, NONIDENT)
              ? ((s.c.once = ONCE)
                 , cutoff > sub
                 )
              : cutoff == sub
              )
          )
         || ((Flgs & ATOM)
             && (NEGATION(Flgs, ATOM)
                 ? (cutoff < sub + 2) /*!(sub[0] && sub[1])*/
                 : cutoff > sub
                 && ((s.c.once = ONCE)
                     , cutoff > sub + 1 /*sub[1]*/
                     )
                 )
             )
         || ((Flgs & (FRACTION | NUMBER))
             && ((ci = sfullnumbercheck(sub, cutoff))
                 , (((Flgs & FRACTION)
                     && ((ci != (QFRACTION | QNUMBER)) ^ NEGATION(Flgs, FRACTION))
                     )
                    || ((Flgs & NUMBER)
                        && (((ci & QNUMBER) == 0) ^ NEGATION(Flgs, NUMBER))
                        )
                    )
                 )
             && (s.c.rmr = (ci == DEFINITELYNONUMBER) ? ONCE : FALSE
                 , (s.c.lmr = PRISTINE)
                 )
             )
         )
       )
        {
        if(IS_VARIABLE(pat))
            {
            int ok = TRUE;
            if(is_op(pat))
                {
                ULONG saveflgs = Flgs & VISIBLE_FLAGS;
                name = subtreecopy(pat);
                name->v.fl &= ~VISIBLE_FLAGS;
                name->v.fl |= SUCCESS;
                if((s.c.rmr = (char)evaluate(name)) != TRUE)
                    ok = FALSE;
                if(Op(name) != EQUALS)
                    {
                    name = isolated(name);
                    name->v.fl |= saveflgs;
                    }
                pat = name;
                }
            if(ok)
                {
                if(Flgs & UNIFY)        /* ?  */
                    {
                    if(!NOTHING(pat) || cutoff > sub)
                        {
                        if(is_op(pat)
                           || pat->u.obj
                           )
                            {
                            if(Flgs & INDIRECT)        /* ?! of ?!! */
                                {
                                if((loc = SymbolBinding_w(pat, Flgs & DOUBLY_INDIRECT)) != NULL)
                                    {
                                    if(is_object(loc))
                                        s.c.rmr = (char)string_copy_insert(loc, subkn, sub, cutoff);
                                    else
                                        {
                                        s.c.rmr = (char)evaluate(loc);
                                        if(!string_copy_insert(loc, subkn, sub, cutoff))
                                            s.c.rmr = FALSE;
                                        }
                                    wipe(loc);
                                    }
                                else
                                    s.c.rmr = (char)NOTHING(pat);
                                }
                            else
                                {
                                s.c.rmr = (char)string_copy_insert(pat, subkn, sub, cutoff);
                                }
                            }
                        else
                            s.c.rmr = TRUE;
                        }
                    }
                else if(Flgs & INDIRECT)        /* ! or !! */
                    {
                    if((loc = SymbolBinding_w(pat, Flgs & DOUBLY_INDIRECT)) != NULL)
                        {
                        cleanOncePattern(loc);
#if CUTOFFSUGGEST
                        if(mayMoveStartOfSubject)
                            {
                            *mayMoveStartOfSubject = 0;
                            }
                        s.c.rmr = (char)(stringmatch(ind + 1, "A", sub, cutoff, loc, subkn, pposition, stringLength
                                                     , suggestedCutOff
                                                     , 0
                        ) ^ NOTHING(pat));
#else
                        s.c.rmr = (char)(stringmatch(ind + 1, "A", sub, cutoff, loc, subkn, pposition, stringLength
                        ) ^ NOTHING(pat));
#endif
                        wipe(loc);
                        }
                    else
                        s.c.rmr = (char)NOTHING(pat);
                    }
                }
            }
        else
            {
            switch(Op(pat))
                {
                case PLUS:
                case TIMES:
                    break;
                case WHITE:
                    {
                    LONG locpos = pposition;
#if CUTOFFSUGGEST
                    char* suggested_Cut_Off = sub;
                    char* may_Move_Start_Of_Subject;
                    may_Move_Start_Of_Subject = sub;
#endif
                    /* This code mirrors that of match(). (see below)*/

                    sloc = sub;                                     /* A    divisionPoint=S */

#if CUTOFFSUGGEST
                    s.c.lmr = stringmatch(ind + 1, "I", sub, sloc       /* B    leftResult=0(P):car(P) */
                                          , pat->LEFT, subkn, pposition
                                          , stringLength, &suggested_Cut_Off
                                          , mayMoveStartOfSubject);
                    if((s.c.lmr & ONCE) && mayMoveStartOfSubject && *mayMoveStartOfSubject > sub)
                        {
                        return ONCE;
                        }
#else
                    s.c.lmr = stringmatch(ind + 1, "I", sub, sloc, pat->LEFT, subkn, pposition, stringLength);
#endif

#if CUTOFFSUGGEST
                    if(suggested_Cut_Off > sloc)
                        {
                        if(cutoff && suggested_Cut_Off > cutoff)
                            {
                            if(suggestedCutOff)
                                {
                                locpos += suggested_Cut_Off - sloc;
                                cutoff = sloc = *suggestedCutOff = suggested_Cut_Off;
                                }
                            else
                                {
                                locpos += cutoff - sloc;
                                sloc = cutoff;
                                s.c.lmr &= ~TRUE;
                                }
                            }
                        else
                            {
                            assert(suggested_Cut_Off > sloc);
                            locpos += suggested_Cut_Off - sloc;
                            sloc = suggested_Cut_Off;
                            }
                        }
                    else
#endif
                        s.c.lmr &= ~ONCE;
                    while(sloc < cutoff)                            /* C    while divisionPoint */
                        {
                        if(s.c.lmr & TRUE)                          /* D        if leftResult.success */
                            {
#if CUTOFFSUGGEST
                            if(s.c.lmr & ONCE)
                                may_Move_Start_Of_Subject = 0;
                            else if(may_Move_Start_Of_Subject != 0)
                                may_Move_Start_Of_Subject = sloc;
                            s.c.rmr = stringmatch(ind + 1, "J", sloc    /* E            rightResult=SR:cdr(P) */
                                                  , cutoff, pat->RIGHT, subkn
                                                  , locpos, stringLength, suggestedCutOff
                                                  , &may_Move_Start_Of_Subject);
                            if(may_Move_Start_Of_Subject != sloc && may_Move_Start_Of_Subject != 0)
                                {
                                assert(may_Move_Start_Of_Subject > sloc);
                                locpos += may_Move_Start_Of_Subject - sloc;
                                sloc = may_Move_Start_Of_Subject;
                                }
                            else
                                {
                                ++sloc;
                                ++locpos;
                                }
#else
                            s.c.rmr = stringmatch(ind + 1, "J", sloc, cutoff, pat->RIGHT, subkn, locpos, stringLength);
                            ++sloc;
                            ++locpos;
#endif
                            if(!(s.c.lmr & ONCE))
                                s.c.rmr &= ~ONCE;
                            }
                        else
                            {
                            ++sloc;
                            ++locpos;
                            }
                        if((s.c.rmr & TRUE)                       /* F        if(1) full success */
                           || (s.c.lmr & (POSITION_ONCE                  /*     or (2) may not be shifted. In the first pass, a position flag on car(P) counts as criterion for being done. */
                                          | ONCE
                                          )
                               )                                          /* In all but the first pass, the left and right */
                           || (s.c.rmr & (ONCE                           /* results can indicate that the loop is done.   */
                                          | POSITION_MAX_REACHED           /* In all passes a position_max_reached on the   */
                                          )                               /* rightResult indicates that the loop is done.  */
                               )
                           )
                            {                                       /* G            return */
                            if(sloc > sub + 1)                          /* Also return whether sub has reached max position.*/
                                s.c.rmr &= ~POSITION_MAX_REACHED;       /* This flag is reason to stop increasing the position of the division any further, but it must not be signalled back to the caller if the lhs is not nil ... */
                            s.c.rmr |= (char)(s.c.lmr & POSITION_MAX_REACHED); /* ... unless it is the lhs that signals it. */
                            if(stringOncePattern(pat))                  /* Also return whether the pattern as a whole */
                                {                                       /* doesn't want longer subjects, which can be */
                                s.c.rmr |= ONCE;                        /* found out by looking at the pattern        */
                                s.c.rmr |= (char)(pat->v.fl & FENCE);
                                }                                       /* or by looking at whether both lhs and rhs  */
                            else if(!(s.c.lmr & ONCE))                  /* results indicated this, in which case both */
                                s.c.rmr &= ~ONCE;                       /* sides must be non-zero size subjects.      */
                            return s.c.rmr ^ (char)NOTHING(pat);
                            }
                        /* H        SL,SR=shift_right divisionPoint */
                        /* SL = lhs divisionPoint S, SR = rhs divisionPoint S */
                        /* I        leftResult=SL:car(P) */
#if CUTOFFSUGGEST
                        suggested_Cut_Off = sub;
                        s.c.lmr = stringmatch(ind + 1, "I", sub, sloc, pat->LEFT, subkn,/* 0 ? */pposition,/* strlen(sub) ? */ stringLength, &suggested_Cut_Off, mayMoveStartOfSubject);
                        if(suggested_Cut_Off > sloc)
                            {
                            if(!(cutoff && suggested_Cut_Off > cutoff))
                                {
                                assert(suggested_Cut_Off > sloc);
                                locpos += suggested_Cut_Off - sloc;
                                sloc = suggested_Cut_Off;
                                }
                            }
#else
                        s.c.lmr = stringmatch(ind + 1, "I", sub, sloc, pat->LEFT, subkn,/* 0 ? */pposition,/* strlen(sub) ? */ stringLength);
#endif
                        }

                    if(s.c.lmr & TRUE)                              /* J    if leftResult.success */
                        {
#if CUTOFFSUGGEST
                        s.c.rmr = stringmatch(ind + 1, "J", sloc, cutoff /* K        rightResult=0(P):cdr(pat) */
                                              , pat->RIGHT, subkn, locpos, stringLength
                                              , suggestedCutOff, mayMoveStartOfSubject);
#else
                        s.c.rmr = stringmatch(ind + 1, "J", sloc, cutoff, pat->RIGHT, subkn, locpos, stringLength);
#endif
                        s.c.rmr &= ~ONCE;
                        }
                    /* L    return */
                    if(!(s.c.rmr & POSITION_MAX_REACHED))
                        s.c.rmr &= ~POSITION_ONCE;
                    if(/*(cutoff > sub) &&*/ stringOncePattern(pat))    /* The test cutoff > sub merely avoids that stringOncePattern is called when it is useless. */
                        {/* Test:
                         @(abcde:`(a ?x) (?z:d) ? )
                         z=b
                         */
                        s.c.rmr |= ONCE;
                        s.c.rmr |= (char)(pat->v.fl & FENCE);
                        }
                    return s.c.rmr ^ (char)NOTHING(pat);               /* end */
                    }
                case UNDERSCORE:
                    if(cutoff > sub + 1)
                        {
#if CUTOFFSUGGEST
                        s.c.lmr = stringmatch(ind + 1, "M", sub, sub + 1, pat->LEFT, subkn, pposition, stringLength, NULL, mayMoveStartOfSubject);
#else
                        s.c.lmr = stringmatch(ind + 1, "M", sub, sub + 1, pat->LEFT, subkn, pposition, stringLength);
#endif
                        if((s.c.lmr & TRUE)
#if CUTOFFSUGGEST
                           && ((s.c.rmr = stringmatch(ind + 1, "N", sub + 1, cutoff, pat->RIGHT, subkn, pposition, stringLength, suggestedCutOff, mayMoveStartOfSubject)) & TRUE)
#else
                           && ((s.c.rmr = stringmatch(ind + 1, "N", sub + 1, cutoff, pat->RIGHT, subkn, pposition, stringLength)) & TRUE)
#endif
                           )
                            {
                            dummy_op = WHITE;
                            }
                        s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                        }
                    break;
                case AND:
#if CUTOFFSUGGEST
                    if((s.c.lmr = stringmatch(ind + 1, "O", sub, cutoff, pat->LEFT, subkn, pposition, stringLength, suggestedCutOff, mayMoveStartOfSubject)) & TRUE)
#else
                    if((s.c.lmr = stringmatch(ind + 1, "O", sub, cutoff, pat->LEFT, subkn, pposition, stringLength)) & TRUE)
#endif
                        {
                        loc = same_as_w(pat->RIGHT);
                        loc = eval(loc);
                        if(loc->v.fl & SUCCESS)
                            {
                            s.c.rmr = TRUE;
                            if(loc->v.fl & FENCE)
                                s.c.rmr |= ONCE;
                            }
                        else
                            {
                            s.c.rmr = FALSE;
                            if(loc->v.fl & FENCE)
                                s.c.rmr |= (FENCE | ONCE);
                            if(loc->v.fl & IMPLIEDFENCE) /* (for function utf$) */
                                s.c.rmr |= ONCE;
                            }
                        wipe(loc);
                        }
                    s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                    break;
                case MATCH:
#if CUTOFFSUGGEST
                    if((s.c.lmr = stringmatch(ind + 1, "P", sub, cutoff, pat->LEFT, subkn, pposition, stringLength, suggestedCutOff, mayMoveStartOfSubject)) & TRUE)
#else
                    if((s.c.lmr = stringmatch(ind + 1, "P", sub, cutoff, pat->LEFT, subkn, pposition, stringLength)) & TRUE)
#endif
                        {
#if CUTOFFSUGGEST
                        if(suggestedCutOff && *suggestedCutOff > cutoff)
                            {
                            cutoff = *suggestedCutOff;
                            }

                        s.c.rmr = (char)(stringmatch(ind + 1, "Q", sub, cutoff, pat->RIGHT, subkn, pposition, stringLength, 0, 0));
#else
                        s.c.rmr = (char)(stringmatch(ind + 1, "Q", sub, cutoff, pat->RIGHT, subkn, pposition, stringLength));
#endif
                        }
                    else
                        s.c.rmr = FALSE;
                    s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE | POSITION_MAX_REACHED));
                    break;
                case OR:
#if CUTOFFSUGGEST
                    if(mayMoveStartOfSubject)
                        *mayMoveStartOfSubject = 0;
                    if((s.c.lmr = (char)(stringmatch(ind + 1, "R", sub, cutoff, pat->LEFT, subkn, pposition, stringLength, NULL, 0)))
#else
                    if((s.c.lmr = (char)(stringmatch(ind + 1, "R", sub, cutoff, pat->LEFT, subkn, pposition, stringLength)))
#endif
                       & (TRUE | FENCE)
                       )
                        {
                        if((s.c.lmr & ONCE) && !stringOncePattern(pat->RIGHT))
                            {
                            s.c.rmr = (char)(s.c.lmr & TRUE);
                            }
                        else
                            {
                            s.c.rmr = (char)(s.c.lmr & (TRUE | ONCE));
                            }
                        }
                    else
                        {
#if CUTOFFSUGGEST
                        s.c.rmr = stringmatch(ind + 1, "S", sub, cutoff, pat->RIGHT, subkn, pposition, stringLength, NULL, 0);
#else
                        s.c.rmr = stringmatch(ind + 1, "S", sub, cutoff, pat->RIGHT, subkn, pposition, stringLength);
#endif
                        if((s.c.rmr & ONCE)
                           && !(s.c.lmr & ONCE)
                           )
                            {
                            s.c.rmr &= ~(ONCE | POSITION_ONCE);
                            }
                        if((s.c.rmr & POSITION_MAX_REACHED)
                           && !(s.c.lmr & POSITION_MAX_REACHED)
                           )
                            {
                            s.c.rmr &= ~(POSITION_MAX_REACHED | POSITION_ONCE);
                            }
                        }
                    break;
                    /*
                    This is now much quicker than previously, because the whole expression
                    (|bc|x) is ONCE if the start of the subject does not match the start of any of
                    the alternations:
                    dbg'@(hhhhhhhhhbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbhhhabcd:?X (|bc|x) d)
                    */
                case FUN:
                case FUU:
                    psh(&sjtNode, &nilNode, NULL);
                    string_copy_insert(&sjtNode, subkn, sub, cutoff);
                    if(NOTHING(pat))
                        {
                        loc = _copyop(pat);
                        loc->v.fl &= ~NOT;
                        loc->v.fl |= SUCCESS;
                        }
                    else
                        loc = same_as_w(pat);
                    loc = eval(loc);
                    deleteNode(&sjtNode);
#if CUTOFFSUGGEST
                    if(mayMoveStartOfSubject)
                        *mayMoveStartOfSubject = 0;
#endif
                    if(isSUCCESS(loc))
                        {
                        if(((loc->v.fl & (UNIFY | FILTERS)) == UNIFY) && !is_op(loc) && !loc->u.obj)
                            {
                            s.c.rmr = (char)TRUE;
                            wipe(loc);
                            break;
                            }
                        else
                            {
                            loc = setflgs(loc, pat->v.fl);
                            if(equal(pat, loc))
                                {
#if CUTOFFSUGGEST
                                s.c.rmr = (char)(stringmatch(ind + 1, "T", sub, cutoff, loc, subkn, pposition, stringLength, NULL, 0) ^ NOTHING(loc));
#else
                                s.c.rmr = (char)(stringmatch(ind + 1, "T", sub, cutoff, loc, subkn, pposition, stringLength) ^ NOTHING(loc));
#endif
                                wipe(loc);
                                break;
                                }
                            }
                        }
                    else
                        {
                        if(loc->v.fl & (FENCE | IMPLIEDFENCE))
                            s.c.rmr = ONCE; /* '~ as return value from function stops stretching subject */
                        }
                    wipe(loc);
                    break;
                default:
                    if(!is_op(pat))
                        {
                        if(!pat->u.obj
                           && (Flgs & (FRACTION | NUMBER | NONIDENT | ATOM | IDENT))
                           )
                            {         /* e.g.    a b c : % */
                            s.c.rmr = TRUE;
                            }
                        else
                            {
#if CUTOFFSUGGEST
                            s.c.rmr = (char)(scompare("b", (char*)sub, cutoff, pat
                                                      , ((!(Flgs & ATOM)
                                                          || NEGATION(Flgs, ATOM)
                                                          )
                                                         ? suggestedCutOff
                                                         : NULL
                                                         )
                                                      , mayMoveStartOfSubject
                            )
                                             );
#else
                            s.c.rmr = (char)(scompare("b", (unsigned char*)sub, cutoff, pat));
#endif
                            DBGSRC(Printf("%s %d%*sscompare(%.*s,", wh, ind, ind, "", (int)(cutoff - sub), sub); result(pat); Printf(") "); \
                                   if(s.c.rmr & ONCE) Printf("ONCE|"); \
                                       if(s.c.rmr & TRUE) Printf("TRUE"); else Printf("FALSE"); \
                                           Printf("\n");)
                            }
                        }
                }
            }
        }
    DBGSRC(if(s.c.rmr & (FENCE | ONCE))\
        {Printf("%s %d%*s+", wh, ind, ind, ""); if(s.c.rmr & FENCE)\
           Printf(" FENCE "); if(s.c.rmr & ONCE)Printf(" ONCE "); Printf("\n"); })
        s.c.rmr |= (char)(pat->v.fl & FENCE);
    if(stringOncePattern(pat) || /* @("abXk":(|? b|`) X ?id) must fail*/ (s.c.rmr & (TRUE | FENCE | ONCE)) == FENCE)
        {
        s.c.rmr |= ONCE;
        DBGSRC(int saveNice; int redhum; saveNice = beNice; redhum = hum; \
               beNice = FALSE; hum = FALSE; \
               Printf("%d%*sstringmatch(%.*s", ind, ind, "", (int)(cutoff - sub), sub); \
               Printf(":"); result(pat); \
               beNice = saveNice; hum = redhum; \
               Printf(") s.c.rmr %d (B)", s.c.rmr); \
               if(pat->v.fl & POSITION) Printf("POSITION "); \
                   if(pat->v.fl & FRACTION)Printf("FRACTION "); \
                       if(pat->v.fl & NUMBER)Printf("NUMBER "); \
                           if(pat->v.fl & SMALLER_THAN)Printf("SMALLER_THAN "); \
                               if(pat->v.fl & GREATER_THAN) Printf("GREATER_THAN "); \
                                   if(pat->v.fl & ATOM)  Printf("ATOM "); \
                                       if(pat->v.fl & FENCE) Printf("FENCE "); \
                                           if(pat->v.fl & IDENT) Printf("IDENT"); \
                                               Printf("\n");)
        }
    if(is_op(pat))
        s.c.rmr ^= (char)NOTHING(pat);
    if(name)
        wipe(name);
    return (char)(s.c.once | s.c.rmr);
    }



static char match(int ind, psk sub, psk pat, psk cutoff, LONG pposition, psk expr, unsigned int op)
    {
    /*
    s.c.lmr or s.c.rmr have three independent flags: TRUE/FALSE, ONCE and FENCE.

    TRUE/FALSE The success or failure of the match.

    ONCE       Unwillingness of the pattern to match longer substrings from the
               subject. Example:

    {?} a b c d:?x @?y d
    {!} a b c d
    {?} !y
    {!} c
    {?} !x
    {!} a b
               In fact, the pattern @?y first matches the empty string and then,
               after backtracking from the failing match of the last subpattern d,
               a single element from the string. Thereafter, when again
               backtracking, the subpattern @?y denies to even try to match a
               substring that is one element longer (two elements, in this example)
               and the subpattern preceding @?y is offered an enlarged substring
               from the subject, while @?y itself starts with the empty element.

               This flag is of importance for patterns with the space, + or
               * operator.
               The flag is turned on in patterns by the `@#/ flags and by operators
               other than space + * _ & : | = $ '
               The flag is turned off "after consumption", i.e. it does not
               percolate upwards through patterns with space + or * operators.

    (once=
      (p=?`Y)
    &   a b c d
      : ?X !p (d|?&(p=`?Z&foo:?Y)&~)
    & out$(X !X Y !Y Z !Z));


    (once=a b c d:?X (?|?) d & out$(X !X))
    (once=a b c d:?X (@|@) d & out$(X !X))
    (once=a b c d:?X (?|@) d & out$(X !X))
    (once=a b c d:?X (@|?) d & out$(X !X))
    (once=a b c d:?X (@|`) d & out$(X !X))
    (once=a b c d:?X (`|?) d & out$(X !X))
    (once=a b c d:?X (`c|?) d & out$(X !X))

    FENCE      Unwillingness of the subject to be matched by alternative patterns.
               Of importance for the | and : operators in a pattern.
               Is turned on by the ` prefix (whether or not in a pattern).
               Is turned off in pattern with space + * | or : operator.
               (With | and : operators this is only the case for the left operand,
               with the other operators this is the case for all except the last
               operand in a list.)
               */
    matchstate s;
    psk loc;
    ULONG Flgs;
    psk name = NULL;
    DBGSRC(Printf("%d%*smatch(", ind, ind, ""); results(sub, cutoff); Printf(":"); \
           result(pat); Printf(")"); Printf("\n");)
        if(is_op(sub))
            {
            if(Op(sub) == EQUALS)
                sub->RIGHT = Head(sub->RIGHT);

            if(sub->RIGHT == cutoff)
                return match(ind + 1, sub->LEFT, pat, NULL, pposition, expr, op);
            }
    s.i = (PRISTINE << SHIFT_LMR) + (PRISTINE << SHIFT_RMR);
    Flgs = pat->v.fl;
    if(Flgs & POSITION)
        {
        if(Flgs & NONIDENT)
            return doEval(sub, cutoff, pat);
        else if(cutoff || !(sub->v.fl & IDENT))
            return FALSE | ONCE | POSITION_ONCE;
        else
            return doPosition(s, pat, pposition, 0, expr
#if CUTOFFSUGGEST
                              , 0
#endif
                              , op
            );
        }
    if(!(((Flgs & NONIDENT) && (((sub->v.fl & IDENT) && 1) ^ NEGATION(Flgs, NONIDENT)))
         || ((Flgs & ATOM) && ((is_op(sub) && 1) ^ NEGATION(Flgs, ATOM)))
         || ((Flgs & FRACTION) && (!RAT_RAT(sub) ^ NEGATION(Flgs, FRACTION)))
         || ((Flgs & NUMBER) && (!RATIONAL_COMP(sub) ^ NEGATION(Flgs, NUMBER)))
         )
       )
        {
        if(IS_VARIABLE(pat))
            {
            int ok = TRUE;
            if(is_op(pat))
                {
                ULONG saveflgs = Flgs & VISIBLE_FLAGS;
                name = subtreecopy(pat);
                name->v.fl &= ~VISIBLE_FLAGS;
                name->v.fl |= SUCCESS;
                if((s.c.rmr = (char)evaluate(name)) != TRUE)
                    ok = FALSE;
                if(Op(name) != EQUALS)
                    {
                    name = isolated(name);/*Is this needed? 20220913*/
                    /* Yes, it is. Otherwise a '?' flag is permanently attached to name. 20230130*/
                    name->v.fl |= saveflgs;
                    }
                pat = name;
                /* name is wiped later in this function! */
                }
            if(ok)
                {
                if(Flgs & UNIFY)        /* ?  */
                    {
                    if(!NOTHING(pat) || is_op(sub) || (sub->u.obj))
                        {
                        if(is_op(pat)
                           || pat->u.obj
                           )
                            if(Flgs & INDIRECT)        /* ?! of ?!! */
                                {
                                if((loc = SymbolBinding_w(pat, Flgs & DOUBLY_INDIRECT)) != NULL)
                                    {
                                    if(is_object(loc))
                                        s.c.rmr = (char)copy_insert(loc, sub, cutoff);
                                    else
                                        {
                                        s.c.rmr = (char)evaluate(loc);
                                        if(!copy_insert(loc, sub, cutoff))
                                            s.c.rmr = FALSE;
                                        /* Previously, s.c.rmr was not influenced by failure of copy_insert */

                                        }
                                    wipe(loc);
                                    }
                                else
                                    s.c.rmr = (char)NOTHING(pat);
                                }
                            else
                                {
                                s.c.rmr = (char)copy_insert(pat, sub, cutoff);
                                /* Previously, s.c.rmr was unconditionally set to TRUE */
                                }

                        else
                            s.c.rmr = TRUE;
                        }
                    /*
                     * else NOTHING(pat) && !is_op(sub) && !sub->u.obj
                     * which means   ~?[`][!][!]
                     */
                    }
                else if(Flgs & INDIRECT)        /* ! or !! */
                    {
                    if((loc = SymbolBinding_w(pat, Flgs & DOUBLY_INDIRECT)) != NULL)
                        {
                        cleanOncePattern(loc);
                        s.c.rmr = (char)(match(ind + 1, sub, loc, cutoff, pposition, expr, op) ^ NOTHING(pat));
                        wipe(loc);
                        }
                    else
                        s.c.rmr = (char)NOTHING(pat);
                    }
                }
            }
        else
            {
#if DATAMATCHESITSELF
            if(pat == sub && (pat->v.fl & SELFMATCHING))
                {
                return TRUE | ONCE;
                }
#endif
            switch(Op(pat))
                {
                case WHITE:
                case PLUS:
                case TIMES:
                    {
                    LONG locpos = pposition;
                    if(Op(pat) == WHITE)
                        {
                        if(sub == &zeroNode /*&& Op(pat) != PLUS*/)
                            {
                            sub = &zeroNodeNotNeutral;
                            locpos = 0;
                            }
                        else if(sub == &oneNode)
                            {
                            sub = &oneNodeNotNeutral;
                            locpos = 0;
                            }
                        }
                    else if(sub == &nilNode)
                        {
                        sub = &nilNodeNotNeutral;
                        locpos = 0;
                        }
                    else if(sub == &oneNode && Op(pat) == PLUS)
                        {
                        sub = &oneNodeNotNeutral;
                        locpos = 0;
                        }
                    /* Optimal sructure for this code:
                                A0 (B A)* B0
                    S:P ::=
                    A       divisionPoint=S
                    B       leftResult=0(P):car(P)
                    C       while divisionPoint
                    D           if leftResult.success
                    E               rightResult=SR:cdr(P)
                    F           if(done)
                    G               return
                    H           SL,SR=shift_right divisionPoint
                    I           leftResult=SL:car(P)
                    J       if leftResult.success
                    K           rightResult=0(P):cdr(pat)
                    L       return

                    0(P)=nil(pat): nil(WHITE)="", nil(+)=0,nil(*)=1
                    In stringmatch, there is no need for L0; the empty string ""
                    is part of the string.
                    */
                    /* A    divisionPoint=S */
                    /* B    leftResult=0(P):car(P) */
                    /* C    while divisionPoint */
                    /* D        if leftResult.success */
                    /* E            rightResult=SR:cdr(P) */
                    /* F        if(done) */
                        /* done =  (1) full success */
                        /*      or (2) may not be shifted.
                           ad (2): In the first pass, a position
                           flag on car(P) counts as criterion for being done. */
                           /* In all but the first pass, the left and right
                              results can indicate that the loop is done. */
                              /* In all passes a position_max_reached on the
                                 rightResult indicates that the loop is done. */
                                 /* G            return */
                                     /* Return true if full success.
                                        Also return whether lhs experienced max position
                                        being reached. */
                                        /* Also return whether the pattern as a whole doesn't
                                           want longer subjects, which can be found out by
                                           looking at the pattern */
                                           /* or by looking at whether both lhs and rhs results
                                              indicated this, in which case both sides must be
                                              non-zero size subjects. */
                                              /* POSITION_ONCE, on the other hand, requires zero size
                                                 subjects. */
                                                 /* Also return the fence flag, if present in rmr.
                                                    (This flag in lmr has no influence.)
                                                 */
                                                 /* H        SL,SR=shift_right divisionPoint */
                                                     /* SL = lhs divisionPoint S, SR = rhs divisionPoint S
                                                     */
                                                     /* I        leftResult=SL:car(P) */
                                                     /* J    if leftResult.success */
                                                     /* K        rightResult=0(P):cdr(pat) */
                                                     /* L    return */
                                                         /* Return true if full success.

                                                            Also return whether lhs experienced max position
                                                            being reached. */
                                                            /* Also return whether the pattern as a whole doesn't
                                                               want longer subjects, which can be found out by
                                                               looking at the pattern or by looking at whether */
                                                               /* both lhs and rhs results indicated this.
                                                                  These come in two sorts: POSITION_ONCE requires */
                                                                  /* zero size subjects, ONCE requires non-zero size
                                                                     subjects. */
                                                                     /* Also return the fence flag, which can be found on
                                                                        the pattern or in the result of the lhs or the rhs.
                                                                        (Not necessary that both have this flag.)
                                                                     */
                                                                     /* end */
                    if(SUBJECTNOTNIL(sub, pat))                      /* A    divisionPoint=S */
                        loc = sub;
                    else
                        loc = NULL;
                    /* B    leftResult=0(P):car(P) */
                    s.c.lmr = (char)match(ind + 1, nil(pat), pat->LEFT  /* a*b+c*d:?+[1+?*[1*%@?q*?+?            (q = c) */
                                          , NULL, pposition, expr       /* a b c d:? [1 (? [1 %@?q ?) ?          (q = b) */
                                          , Op(pat));                /* a b c d:? [1  ? [1 %@?q ?  ?          (q = b) */
                    s.c.lmr &= ~ONCE;
                    while(loc)                                      /* C    while divisionPoint */
                        {
                        if(s.c.lmr & TRUE)                          /* D        if leftResult.success */
                            {                                       /* E            rightResult=SR:cdr(P) */
                            s.c.rmr = match(ind + 1, loc, pat->RIGHT, cutoff, locpos, expr, op);
                            if(!(s.c.lmr & ONCE))
                                s.c.rmr &= ~ONCE;
                            }
                        if((s.c.rmr & TRUE)                       /* F        if(1) full success */
                           || (s.c.lmr & (POSITION_ONCE                  /*     or (2) may not be shifted. In the first pass, a position flag on car(P) counts as criterion for being done. */
                                          | ONCE
                                          )
                               )                                          /* In all but the first pass, the left and right */
                           || (s.c.rmr & (ONCE                           /* results can indicate that the loop is done.   */
                                          | POSITION_MAX_REACHED           /* In all passes a position_max_reached on the   */
                                          )                               /* rightResult indicates that the loop is done.  */
                               )
                           )
                            {                                       /* G            return */
                            if(loc != sub)
                                s.c.rmr &= ~POSITION_MAX_REACHED;           /* This flag is reason to stop increasing the position of the division any further, but it must not be signalled  back to the caller if the lhs is not nil ... */
                            s.c.rmr |= (char)(s.c.lmr & POSITION_MAX_REACHED);  /* ... unless it is the lhs that signals it. */
                            if(oncePattern(pat))                        /* Also return whether the pattern as a whole doesn't want longer subjects, which can be found out by looking at the pattern */
                                {                                       /* For example,                            */
                                s.c.rmr |= ONCE;                        /*     a b c d:`(?x ?y) (?z:c) ?           */
                                s.c.rmr |= (char)(pat->v.fl & FENCE);   /* must fail and set x==nil, y==a and z==b */
                                }
                            else if(!(s.c.lmr & ONCE))
                                s.c.rmr &= ~ONCE;
                            DBGSRC(Printf("%d%*smatch(", ind, ind, ""); \
                                   results(sub, cutoff); Printf(":"); result(pat);)
#ifndef NDEBUG
                                DBGSRC(printMatchState("EXIT-MID", s, pposition, 0);)
#endif
                                DBGSRC(if(pat->v.fl & FRACTION) Printf("FRACTION "); \
                                       if(pat->v.fl & NUMBER) Printf("NUMBER "); \
                                           if(pat->v.fl & SMALLER_THAN)\
                                               Printf("SMALLER_THAN "); \
                                               if(pat->v.fl & GREATER_THAN)\
                                                   Printf("GREATER_THAN "); \
                                                   if(pat->v.fl & ATOM) Printf("ATOM "); \
                                                       if(pat->v.fl & FENCE) Printf("FENCE "); \
                                                           if(pat->v.fl & IDENT) Printf("IDENT"); \
                                                               Printf("\n");)
                                return s.c.rmr ^ (char)NOTHING(pat);
                            }
                        /* H        SL,SR=shift_right divisionPoint */
                        if(Op(loc) == Op(pat)
                           && loc->RIGHT != cutoff
                           )
                            loc = loc->RIGHT;
                        else
                            loc = NULL;
                        /* SL = lhs divisionPoint S, SR = rhs divisionPoint S
                        */
                        ++locpos;
                        /* I        leftResult=SL:car(P) */
                        s.c.lmr = match(ind + 1, sub, pat->LEFT, loc, pposition, sub, Op(pat));
                        }
                    /* J    if leftResult.success */
                    if(s.c.lmr & TRUE)
                        /* K        rightResult=0(P):cdr(pat) */
                        {
                        s.c.rmr = match(ind + 1, nil(pat), pat->RIGHT, NULL, locpos, expr, Op(pat));
                        s.c.rmr &= ~ONCE;
                        }
                    /* L    return */
                        /* Return true if full success.

                           Also return whether lhs experienced max position
                           being reached. */
                    if(!(s.c.rmr & POSITION_MAX_REACHED))
                        s.c.rmr &= ~POSITION_ONCE;
                    /* Also return whether the pattern as a whole doesn't
                       want longer subjects, which can be found out by
                       looking at the pattern. */
                    if(/*cutoff &&*/ oncePattern(pat))
                        /* The test cutoff != NULL merely avoids that
                        oncePattern is called when it is useless. */
                        { /* Test:
                          a b c d e:`(a ?x) (?z:d) ?
                          x=
                          z=b
                          */
                        s.c.rmr |= ONCE;
                        s.c.rmr |= (char)(pat->v.fl & FENCE);
                        }
                    /* POSITION_ONCE requires zero size subjects. */
                    /* Also return the fence flag, which can be found on
                       the pattern or in the result of the lhs or the rhs.
                       (Not necessary that both have this flag.)
                    */
                    s.c.rmr ^= (char)NOTHING(pat);
                    return s.c.rmr;
                    /* end */
                    }
                case EXP:
                    if(Op(sub) == EXP)
                        {
                        if((s.c.lmr = match(ind + 1, sub->LEFT, pat->LEFT, NULL, 0, sub->LEFT, 12345)) & TRUE)
                            s.c.rmr = match(ind + 1, sub->RIGHT, pat->RIGHT, NULL, 0, sub->RIGHT, 12345);
#ifndef NDEBUG
                        DBGSRC(printMatchState("EXP:EXIT-MID", s, pposition, 0);)
#endif
                            s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE)); /* a*b^2*c:?x*?y^(~1:?t)*?z */
                        }
                    if(!(s.c.rmr & TRUE)
                       && ((s.c.lmr = match(ind + 1, sub, pat->LEFT, cutoff, pposition, expr, op)) & TRUE)
                       && ((s.c.rmr = match(ind + 1, &oneNode, pat->RIGHT, NULL, 0, &oneNode, 1234567)) & TRUE)
                       )
                        { /* a^2*b*c*d^3 : ?x^(?s:~1)*?y^?t*?z^(>2:?u) */
                        s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                        }
                    s.c.rmr &= ~POSITION_MAX_REACHED;
                    break;
                case UNDERSCORE:
                    if(is_op(sub))
                        {
                        if(Op(sub) == EQUALS)
                            {
                            if(ISBUILTIN((objectnode*)sub))
                                {
                                errorprintf("You cannot match an object '=' with '_' if the object is built-in\n");
                                s.c.rmr = ONCE;
                                }
                            else
                                {
                                if((s.c.lmr = match(ind + 1, sub->LEFT, pat->LEFT, NULL, 0, sub->LEFT, 12345)) & TRUE)
                                    {
                                    loc = same_as_w(sub->RIGHT); /* Object might change as a side effect!*/
                                    if((s.c.rmr = match(ind + 1, loc, pat->RIGHT, cutoff, 0, loc, 123)) & TRUE)
                                        {
                                        dummy_op = Op(sub);
                                        }
                                    wipe(loc);
                                    }
                                }
                            }
                        else if(((s.c.lmr = match(ind + 1, sub->LEFT, pat->LEFT, NULL, 0, sub->LEFT, 12345)) & TRUE)
                                && ((s.c.rmr = match(ind + 1, sub->RIGHT, pat->RIGHT, cutoff, 0, sub->RIGHT, 123)) & TRUE)
                                )
                            {
                            dummy_op = Op(sub);
                            }
#ifndef NDEBUG
                        DBGSRC(printMatchState("UNDERSCORE:EXIT-MID", s, pposition, 0);)
#endif
                            switch(Op(sub))
                                {
                                case WHITE:
                                case PLUS:
                                case TIMES:
                                    break;
                                default:
                                    s.c.rmr &= ~POSITION_MAX_REACHED;
                                }
#ifndef NDEBUG
                        DBGSRC(printMatchState("streep:EXIT-MID", s, pposition, 0);)
#endif
                        }
                    if(s.c.lmr != PRISTINE)
                        s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                    break;
                case AND:
                    if((s.c.lmr = match(ind + 1, sub, pat->LEFT, cutoff, pposition, expr, op)) & TRUE)
                        {
                        loc = same_as_w(pat->RIGHT);
                        loc = eval(loc);
                        if(loc->v.fl & SUCCESS)
                            {
                            s.c.rmr = TRUE;
                            if(loc->v.fl & FENCE)
                                s.c.rmr |= ONCE;
                            }
                        else
                            {
                            s.c.rmr = FALSE;
                            if(loc->v.fl & FENCE)
                                s.c.rmr |= (FENCE | ONCE);
                            }
                        wipe(loc);
                        }
                    s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                    break;
                case MATCH:
                    if((s.c.lmr = match(ind + 1, sub, pat->LEFT, cutoff, pposition, expr, op)) & TRUE)
                        {
                        if((pat->v.fl & ATOM)
#if !STRINGMATCH_CAN_BE_NEGATED
                           && !NEGATION(pat->v.fl, ATOM)
#endif
                           )
#if CUTOFFSUGGEST
                            s.c.rmr = (char)(stringmatch(ind + 1, "U", SPOBJ(sub), NULL, pat->RIGHT, sub, 0, strlen((char*)SPOBJ(sub)), NULL, 0) & TRUE);
#else
                            s.c.rmr = (char)(stringmatch(ind + 1, "U", POBJ(sub), NULL, pat->RIGHT, sub, 0, strlen((char*)POBJ(sub))) & TRUE);
#endif
                        else
                            s.c.rmr = (char)(match(ind + 1, sub, pat->RIGHT, cutoff, pposition, expr, op) & TRUE);
                        }
                    else
                        s.c.rmr = FALSE;
                    s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE | POSITION_MAX_REACHED));
                    /*
                   dbg'(x y z f t m a i n l:? ((m a i n|f t):?X) ?W) & out$(X !X W !W)
                   correct:X f t W m a i n l

                   @(jfhljkhlhfgjkhfas:? ((lh|jk):?W) ?) & !W
                   wrong: jf
                   correct: jk
                   */
                    break;
                case OR:
                    if((s.c.lmr = (char)match(ind + 1, sub, pat->LEFT, cutoff, pposition, expr, op))
                       & (TRUE | FENCE)
                       )
                        {
                        if((s.c.lmr & ONCE) && !oncePattern(pat->RIGHT))
                            {
                            s.c.rmr = (char)(s.c.lmr & TRUE);
                            }
                        else
                            {
                            s.c.rmr = (char)(s.c.lmr & (TRUE | ONCE));
                            }
                        }
                    else
                        {
                        s.c.rmr = match(ind + 1, sub, pat->RIGHT, cutoff, pposition, expr, op);
                        if((s.c.rmr & ONCE)
                           && !(s.c.lmr & ONCE)
                           )
                            {
                            s.c.rmr &= ~(ONCE | POSITION_ONCE);
                            }
                        if((s.c.rmr & POSITION_MAX_REACHED)
                           && !(s.c.lmr & POSITION_MAX_REACHED)
                           )
                            {
                            s.c.rmr &= ~(POSITION_MAX_REACHED | POSITION_ONCE);
                            }
                        }
                    DBGSRC(Printf("%d%*s", ind, ind, ""); \
                           Printf("OR s.c.lmr %d s.c.rmr %d\n", s.c.lmr, s.c.rmr);)
                        /*
                        :?W:?X:?Y:?Z & dbg'(a b c d:?X (((a ?:?W) & ~`|?Y)|?Z) d) & out$(X !X W !W Y !Y Z !Z)
                        erroneous: X a W a b c d Y b c Z
                        expected: X W a b c Y Z a b c
                        */
                        break;
                    /*
                    This is now much quicker than previously, because the whole expression
                    (|bc|x) is ONCE if the start of the subject does not match the start of any of
                    the alternations:
                    dbg'(h h h h h h h h h b b b b b b b b b b b b b b b b b b b b b b b b b b b b
                    b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b
                    b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b b
                    b b h h h a b c d:?X (|b c|x) d)
                    */
                case FUN:
                    /* Test whether $ is escape */
                    if(atomtest(pat->LEFT) == 0)
                        {
                        if(((sub->v.fl & (UNIFY | FLGS | NOT)) & (pat->RIGHT->v.fl & (UNIFY | FLGS | NOT)))
                           == (pat->RIGHT->v.fl & (UNIFY | FLGS | NOT))
                           )
                            {
                            if(is_op(pat->RIGHT))
                                {
                                if(Op(sub) == Op(pat->RIGHT))
                                    {
                                    if((s.c.lmr = match(ind + 1, sub->LEFT, pat->RIGHT->LEFT, NULL, 0, sub->LEFT, 9999)) & TRUE)
                                        s.c.rmr = match(ind + 1, sub->RIGHT, pat->RIGHT->RIGHT, NULL, 0, sub->RIGHT, 8888);
                                    s.c.rmr |= (char)(s.c.lmr & ONCE); /*{?} (=!(a.b)):(=$!(a.b)) => =!(a.b)*/
                                    }
                                else
                                    s.c.rmr = (char)ONCE; /*{?} (=!(a.b)):(=$!(a,b)) => F */
                                }
                            else
                                {
                                if(!(pat->RIGHT->v.fl & MINUS)
                                   || (!is_op(sub)
                                       && (sub->v.fl & MINUS)
                                       )
                                   )
                                    {
                                    if(!pat->RIGHT->u.obj)
                                        {
                                        if(pat->RIGHT->v.fl & UNOPS)
                                            s.c.rmr = (char)(TRUE | ONCE); /*{?} (=!):(=$!) => =! */
                                        /*{?} (=!(a.b)):(=$!) => =!(a.b) */
                                        /*{?} (=-a):(=$-) => =-a */
                                        else if(!(sub->v.fl & (UNIFY | FLGS | NOT))
                                                && (is_op(sub) || !(sub->v.fl & MINUS))
                                                )
                                            {
                                            s.c.rmr = (char)(TRUE | ONCE); /*{?} (=):(=$) => = */
                                            /*{?} (=(a.b)):(=$) => =a.b */
                                            }
                                        else
                                            s.c.rmr = (char)ONCE;/*{?} (=-a):(=$) => F */
                                        /*{?} (=-):(=$) => F */
                                        /*{?} (=#):(=$) => F */
                                        }
                                    else if(!is_op(sub)
                                            && pat->RIGHT->u.obj == sub->u.obj
                                            )
                                        s.c.rmr = (char)(TRUE | ONCE); /*{?} (=-!a):(=$-!a) => =!-a */
                                    else
                                        s.c.rmr = (char)ONCE;/*{?} (=-!a):(=$-!b) => F */
                                    }
                                else
                                    s.c.rmr = (char)ONCE; /*{?} (=):(=$-) => F */
                                /*{?} (=(a.b)):(=$-) => F */
                                /*{?} (=a):(=$-) => F */
                                }
                            }
                        else
                            s.c.rmr = (char)ONCE; /*{?} (=!):(=$!!) => F */
                        s.c.rmr &= ~POSITION_MAX_REACHED;
                        break;
                        }
                    /* fall through */
                case FUU:
                    psh(&sjtNode, &nilNode, NULL);
                    copy_insert(&sjtNode, sub, cutoff);
                    if(NOTHING(pat))
                        {
                        loc = _copyop(pat);
                        loc->v.fl &= ~NOT;
                        loc->v.fl |= SUCCESS;
                        }
                    else
                        loc = same_as_w(pat);
                    loc = eval(loc);
                    deleteNode(&sjtNode);
                    if(isSUCCESS(loc))
                        {
                        if(((loc->v.fl & (UNIFY | FILTERS)) == UNIFY) && !is_op(loc) && !loc->u.obj)
                            {
                            s.c.rmr = (char)TRUE;
                            wipe(loc);
                            break;
                            }
                        else
                            {
                            loc = setflgs(loc, pat->v.fl);
                            if(equal(pat, loc))
                                {
                                s.c.rmr = (char)(match(ind + 1, sub, loc, cutoff, pposition, expr, op) ^ NOTHING(loc));
                                wipe(loc);
                                break;
                                }
                            }
                        }
                    else /*"cat" as return value is used as pattern, ~"cat" however is not, because the function failed. */
                        {
                        if(loc->v.fl & FENCE)
                            s.c.rmr = ONCE; /* '~ as return value from function stops stretching subject */
                        }
                    wipe(loc);
                    /* fall through */
                default:
                    if(is_op(pat))
                        {
                        if(Op(sub) == Op(pat))
                            {
                            if((s.c.lmr = match(ind + 1, sub->LEFT, pat->LEFT, NULL, 0, sub->LEFT, 4432)) & TRUE)
                                {
                                if(Op(sub) == EQUALS)
                                    {
                                    loc = same_as_w(sub->RIGHT); /* Object might change as a side effect!*/
                                    s.c.rmr = match(ind + 1, loc, pat->RIGHT, NULL, 0, loc, 2234);
                                    wipe(loc);
                                    }
                                else
                                    s.c.rmr = match(ind + 1, sub->RIGHT, pat->RIGHT, NULL, 0, sub->RIGHT, 2234);
                                }
                            s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                            s.c.rmr &= ~POSITION_MAX_REACHED;
                            }
#ifndef NDEBUG
                        DBGSRC(printMatchState("DEFAULT:EXIT-MID", s, pposition, 0);)
#endif
                        }
                    else
                        {
                        if(pat->u.obj
                           || !(Flgs & (FRACTION | NUMBER | NONIDENT | ATOM | IDENT))
                           )
                            {

                            s.c.rmr = (char)(ONCE | compare(sub, pat));
                            }
                        else         /* e.g.    a b c : % */
                            {
                            s.c.rmr = TRUE;
                            }
                        }
                }
            }
        }
    if(oncePattern(pat) || /* (a b X k:(|? b|`) X ?id) must fail*/ (s.c.rmr & (TRUE | FENCE | ONCE)) == FENCE)
        {
        s.c.rmr |= (char)(pat->v.fl & FENCE);
        s.c.rmr |= ONCE;
        DBGSRC(Printf("%d%*smatch(", ind, ind, ""); results(sub, cutoff); \
               Printf(":"); result(pat); Printf(") (B)");)
#ifndef NDEBUG
            DBGSRC(Printf(" rmr t %d o %d p %d m %d f %d ", \
                          s.b.brmr_true, s.b.brmr_once, s.b.brmr_position_once, s.b.brmr_position_max_reached, s.b.brmr_fence);)
#endif
            DBGSRC(if(pat->v.fl & POSITION) Printf("POSITION "); \
                   if(pat->v.fl & FRACTION) Printf("FRACTION "); \
                       if(pat->v.fl & NUMBER) Printf("NUMBER "); \
                           if(pat->v.fl & SMALLER_THAN) Printf("SMALLER_THAN "); \
                               if(pat->v.fl & GREATER_THAN) Printf("GREATER_THAN "); \
                                   if(pat->v.fl & ATOM) Printf("ATOM "); \
                                       if(pat->v.fl & FENCE) Printf("FENCE "); \
                                           if(pat->v.fl & IDENT) Printf("IDENT"); \
                                               Printf("\n");)
        }
    if(is_op(pat))
        s.c.rmr ^= (char)NOTHING(pat);
    if(name)
        wipe(name);
    return s.c.rmr;
    }

static int subroot(nnumber * ag, char* conc[], int* pind)
    {
    int macht, i;
    ULONG g, smalldivisor;
    ULONG ores;
    static int bijt[12] =
        { 1,  2,  2,  4,    2,    4,    2,    4,    6,    2,  6 };
    /* 2-3,3-5,5-7,7-11,11-13,13-17,17-19,19-23,23-29,29-1,1-7*/
    ULONG bigdivisor;

#ifdef ERANGE   /* ANSI C : strtoul() out of range */
    errno = 0;
    g = STRTOUL(ag->number, NULL, 10);
    if(errno == ERANGE)
        return FALSE; /*{?} 45237183544316235476^1/2 => 45237183544316235476^1/2 */
#else  /* TURBOC, vcc */
    if(ag.length > 10 || ag.length == 10 && strcmp(ag.number, "4294967295") > 0)
        return FALSE;
    g = STRTOUL(ag.number, NULL, 10);
#endif
    ores = 1;
    macht = 1;
    smalldivisor = 2;
    i = 0;
    while((bigdivisor = g / smalldivisor) >= smalldivisor)
        {
        if(bigdivisor * smalldivisor == g)
            {
            g = bigdivisor;
            if(smalldivisor != ores)
                {
                if(ores != 1)
                    {
                    if(ores < 1000)
                        {
                        conc[(*pind)] = (char*)bmalloc(__LINE__, 12);/*{?} 327365274^1/2 => 2^1/2*3^1/2*2477^1/2*22027^1/2 */
                        }
                    else
                        {
                        conc[*pind] = (char*)bmalloc(__LINE__, 20);
                        }
                    sprintf(conc[(*pind)++], LONGU "^(%d*\1)*", ores, macht);
                    }
                macht = 1;
                ores = smalldivisor;
                }
            else
                {
                macht++; /*{?} 80956863^1/2 => 3*13^1/2*541^1/2*1279^1/2 */
                }
            }
        else
            {
            smalldivisor += bijt[i];
            if(++i > 10)
                i = 3;
            }
        }
    if(ores == 1 && macht == 1)
        return FALSE;
    conc[*pind] = (char*)bmalloc(__LINE__, 32);
    if((ores == g && ++macht) || ores == 1)
        sprintf(conc[(*pind)++], LONGU "^(%d*\1)", g, macht); /*{?} 32^1/2 => 2^5/2 */
    else
        sprintf(conc[(*pind)++], LONGU "^(%d*\1)*" LONGU "^\1", ores, macht, g);
    return TRUE;
    }

static int absone(psk pnode)
    {
    char* pstring;
    pstring = SPOBJ(pnode);
    return(*pstring == '1' && *++pstring == 0);
    }

static psk _leftbranch(psk Pnode)
    {
    psk lnode;
    lnode = Pnode->LEFT;
    if(!(Pnode->v.fl & SUCCESS))
        {
        lnode = isolated(lnode);
        lnode->v.fl ^= SUCCESS;
        }
    if((Pnode->v.fl & FENCE) && !(lnode->v.fl & FENCE))
        {
        lnode = isolated(lnode);
        lnode->v.fl |= FENCE;
        }
    wipe(Pnode->RIGHT);
    return lnode;
    }

static psk leftbranch(psk Pnode)
    {
    psk lnode = _leftbranch(Pnode);
    pskfree(Pnode);
    return lnode;
    }

static psk _fleftbranch(psk Pnode)
    {
    psk lnode;
    lnode = Pnode->LEFT;
    if(Pnode->v.fl & SUCCESS)
        {
        lnode = isolated(lnode);
        lnode->v.fl ^= SUCCESS;
        }
    if((Pnode->v.fl & FENCE) && !(lnode->v.fl & FENCE))
        {
        lnode = isolated(lnode);
        lnode->v.fl |= FENCE;
        }
    wipe(Pnode->RIGHT);
    return lnode;
    }

static psk fleftbranch(psk Pnode)
    {
    psk lnode = _fleftbranch(Pnode);
    pskfree(Pnode);
    return lnode;
    }

static psk _fenceleftbranch(psk Pnode)
    {
    psk lnode;
    lnode = Pnode->LEFT;
    if(!(Pnode->v.fl & SUCCESS))
        {
        lnode = isolated(lnode);
        lnode->v.fl ^= SUCCESS;
        }
    if(Pnode->v.fl & FENCE)
        {
        if(!(lnode->v.fl & FENCE))
            {
            lnode = isolated(lnode);
            lnode->v.fl |= FENCE;
            }
        }
    else if(lnode->v.fl & FENCE)
        {
        lnode = isolated(lnode);
        lnode->v.fl &= ~FENCE;
        }
    wipe(Pnode->RIGHT);
    return lnode;
    }

static psk _rightbranch(psk Pnode)
    {
    psk rightnode;
    rightnode = Pnode->RIGHT;
    if(!(Pnode->v.fl & SUCCESS))
        {
        rightnode = isolated(rightnode);
        rightnode->v.fl ^= SUCCESS;
        }
    if((Pnode->v.fl & FENCE) && !(rightnode->v.fl & FENCE))
        {
        rightnode = isolated(rightnode);
        rightnode->v.fl |= FENCE;
        }
    wipe(Pnode->LEFT);
    return rightnode;
    }

static psk rightbranch(psk Pnode)
    {
    psk rightnode = _rightbranch(Pnode);
    pskfree(Pnode);
    return rightnode;
    }

static void pop(psk pnode)
    {
    while(is_op(pnode))
        {
        pop(pnode->LEFT);
        pnode = pnode->RIGHT;
        }
    deleteNode(pnode);
    }

static psk tryq(psk Pnode, psk fun, Boolean * ok)
    {
    psk anchor;
    psh(&argNode, Pnode, NULL);
    Pnode->v.fl |= READY;

    anchor = subtreecopy(fun->RIGHT);

    psh(fun->LEFT, &zeroNode, NULL);
    anchor = eval(anchor);
    pop(fun->LEFT);
    if(anchor->v.fl & SUCCESS)
        {
        *ok = TRUE;
        wipe(Pnode);
        Pnode = anchor;
        }
    else
        {
        *ok = FALSE;
        wipe(anchor);
        }
    deleteNode(&argNode);
    return Pnode;
    }

static psk* backbone(psk arg, psk Pnode, psk * pfirst)
    {
    psk first = *pfirst = subtreecopy(arg);
    psk* plast = pfirst;
    while(arg != Pnode)
        {
        psk R = subtreecopy((*plast)->RIGHT);
        wipe((*plast)->RIGHT);
        (*plast)->RIGHT = R;
        plast = &((*plast)->RIGHT);
        arg = arg->RIGHT;
        }
    *pfirst = first;
    return plast;
    }

static psk rightoperand(psk Pnode)
    {
    psk temp;
    ULONG Sign;
    temp = (Pnode->RIGHT);
    return((Sign = Op(Pnode)) == Op(temp) &&
           (Sign == PLUS || Sign == TIMES || Sign == WHITE) ?
           temp->LEFT : temp);
    }

static psk evalmacro(psk Pnode)
    {
    if(!is_op(Pnode))
        {
        return NULL;
        }
    else
        {
        psk arg = Pnode;
        while(!(Pnode->v.fl & READY))
            {
            if(atomtest(Pnode->LEFT) != 0)
                {
                psk left = evalmacro(Pnode->LEFT);
                if(left != NULL)
                    {
                    /* copy backbone from evalmacro's argument to current Pnode
                       release lhs of copy of current and replace with 'left'
                       assign copy to 'Pnode'
                       evalmacro current, if not null, replace current
                       return current
                    */
                    psk ret;
                    psk first = NULL;
                    psk* last = backbone(arg, Pnode, &first);
                    wipe((*last)->LEFT);
                    (*last)->LEFT = left;
                    if(atomtest((*last)->LEFT) == 0 && Op((*last)) == FUN)
                        {
                        ret = evalmacro(*last);
                        if(ret)
                            {
                            wipe(*last);
                            *last = ret;
                            }
                        }
                    else
                        {
                        psk right = evalmacro((*last)->RIGHT);
                        if(right)
                            {
                            wipe((*last)->RIGHT);
                            (*last)->RIGHT = right;
                            }
                        }
                    return first;
                    }
                }
            else if(Op(Pnode) == FUN)
                {
                if(Op(Pnode->RIGHT) == UNDERSCORE)
                    {
                    int Flgs;
                    psk h;
                    psk hh;
                    psk first = NULL;
                    psk* last;
                    Flgs = Pnode->v.fl & (UNOPS | SUCCESS);
                    h = subtreecopy(Pnode->RIGHT);
                    if(dummy_op == EQUALS)
                        {
                        psk becomes = (psk)bmalloc(__LINE__, sizeof(objectnode));
#if WORD32
                        ((typedObjectnode*)becomes)->u.Int = 0;
#else
                        ((typedObjectnode*)becomes)->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
#endif
                        becomes->LEFT = same_as_w(h->LEFT);
                        becomes->RIGHT = same_as_w(h->RIGHT);
                        wipe(h);
                        h = becomes;
                        }
                    h->v.fl = dummy_op | Flgs;
                    hh = evalmacro(h->LEFT);
                    if(hh)
                        {
                        wipe(h->LEFT);
                        h->LEFT = hh;
                        }
                    hh = evalmacro(h->RIGHT);
                    if(hh)
                        {
                        wipe(h->RIGHT);
                        h->RIGHT = hh;
                        }
                    last = backbone(arg, Pnode, &first);
                    wipe(*last);
                    *last = h;
                    return first;
                    }
                else if(Op(Pnode->RIGHT) == FUN
                        && atomtest(Pnode->RIGHT->LEFT) == 0
                        )
                    {
                    int Flgs;
                    psk h;
                    psk hh;
                    psk first = NULL;
                    psk* last;
                    Flgs = Pnode->v.fl & UNOPS;
                    h = subtreecopy(Pnode->RIGHT);
                    h->v.fl |= Flgs;
                    assert(atomtest(h->LEFT) == 0);
                    /* hh = evalmacro(h->LEFT);
                    if(hh)
                        {
                        wipe(h->LEFT);
                        h->LEFT = hh;
                        }*/
                    hh = evalmacro(h->RIGHT);
                    if(hh)
                        {
                        wipe(h->RIGHT);
                        h->RIGHT = hh;
                        }
                    last = backbone(arg, Pnode, &first);
                    wipe(*last);
                    *last = h;
                    return first;
                    }
                else
                    {
                    int newval = 0;
                    psk tmp = same_as_w(Pnode->RIGHT);
                    psk h;
                    tmp = eval(tmp);

                    if((h = getValue(tmp, &newval)) != NULL)
                        {
                        int Flgs;
                        psk first = NULL;
                        psk* last;
                        if((Op(h) == EQUALS) && ISBUILTIN((objectnode*)h))
                            {
                            if(!newval)
                                h = same_as_w(h);
                            }
                        else
                            {
                            Flgs = Pnode->v.fl & (UNOPS);
                            if(!newval)
                                {
                                h = subtreecopy(h);
                                }
                            if(Flgs)
                                {
                                h->v.fl |= Flgs;
                                if(h->v.fl & INDIRECT)
                                    h->v.fl &= ~READY;
                                }
                            else if(h->v.fl & INDIRECT)
                                {
                                h->v.fl &= ~READY;
                                }
                            else if(Op(h) == EQUALS)
                                {
                                h->v.fl &= ~READY;
                                }
                            }

                        wipe(tmp);
                        last = backbone(arg, Pnode, &first);
                        wipe(*last);
                        *last = h;
                        return first;
                        }
                    else
                        {
                        errorprintf("\nmacro evaluation fails because rhs of $ operator is not bound to a value: "); writeError(Pnode); errorprintf("\n");
                        wipe(tmp);
                        return NULL;
                        }
                    }
                }
            Pnode = Pnode->RIGHT;
            if(!is_op(Pnode))
                {
                break;
                }
            }
        }
    return NULL;
    }

static psk lambda(psk Pnode, psk name, psk Arg)
    {
    if(!is_op(Pnode))
        {
        return NULL;
        }
    else
        {
        psk arg = Pnode;
        while(!(Pnode->v.fl & READY))
            {
            if(atomtest(Pnode->LEFT) != 0)
                {
                psk left = lambda(Pnode->LEFT, name, Arg);
                if(left != NULL)
                    {
                    /* copy backbone from lambda's argument to current Pnode
                       release lhs of copy of current and replace with 'left'
                       assign copy to 'Pnode'
                       lambda current, if not null, replace current
                       return current
                    */
                    psk ret;
                    psk first = NULL;
                    psk* last = backbone(arg, Pnode, &first);
                    wipe((*last)->LEFT);
                    (*last)->LEFT = left;
                    if(atomtest((*last)->LEFT) == 0 && Op((*last)) == FUN)
                        {
                        ret = lambda(*last, name, Arg);
                        if(ret)
                            {
                            wipe(*last);
                            *last = ret;
                            }
                        }
                    else
                        {
                        psk right = lambda((*last)->RIGHT, name, Arg);
                        if(right)
                            {
                            wipe((*last)->RIGHT);
                            (*last)->RIGHT = right;
                            }
                        }
                    return first;
                    }
                }
            else if(Op(Pnode) == FUN)
                {
                if(Op(Pnode->RIGHT) == UNDERSCORE)
                    {
                    int Flgs;
                    psk h;
                    psk hh;
                    psk first = NULL;
                    psk* last;
                    Flgs = Pnode->v.fl & (UNOPS | SUCCESS);
                    h = subtreecopy(Pnode->RIGHT);
                    if(dummy_op == EQUALS)
                        {
                        psk becomes = (psk)bmalloc(__LINE__, sizeof(objectnode));
#if WORD32
                        ((typedObjectnode*)becomes)->u.Int = 0;
#else
                        ((typedObjectnode*)becomes)->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
#endif
                        becomes->LEFT = same_as_w(h->LEFT);
                        becomes->RIGHT = same_as_w(h->RIGHT);
                        wipe(h);
                        h = becomes;
                        }
                    h->v.fl = dummy_op | Flgs;
                    hh = lambda(h->LEFT, name, Arg);
                    if(hh)
                        {
                        wipe(h->LEFT);
                        h->LEFT = hh;
                        }
                    hh = lambda(h->RIGHT, name, Arg);
                    if(hh)
                        {
                        wipe(h->RIGHT);
                        h->RIGHT = hh;
                        }
                    last = backbone(arg, Pnode, &first);
                    wipe(*last);
                    *last = h;
                    return first;
                    }
                else if(Op(Pnode->RIGHT) == FUN
                        && atomtest(Pnode->RIGHT->LEFT) == 0
                        )
                    {
                    int Flgs;
                    psk h;
                    psk hh;
                    psk first = NULL;
                    psk* last;
                    Flgs = Pnode->v.fl & UNOPS;
                    h = subtreecopy(Pnode->RIGHT);
                    h->v.fl |= Flgs;
                    assert(atomtest(h->LEFT) == 0);
                    hh = lambda(h->RIGHT, name, Arg);
                    if(hh)
                        {
                        wipe(h->RIGHT);
                        h->RIGHT = hh;
                        }
                    last = backbone(arg, Pnode, &first);
                    wipe(*last);
                    *last = h;
                    return first;
                    }
                else if(!equal(name, Pnode->RIGHT))
                    {
                    psk h;
                    psk first;
                    psk* last;
                    h = subtreecopy(Arg);
                    if(h->v.fl & INDIRECT)
                        {
                        h->v.fl &= ~READY;
                        }
                    else if(is_op(h) && Op(h) == EQUALS)
                        {
                        h->v.fl &= ~READY;
                        }

                    last = backbone(arg, Pnode, &first);
                    wipe(*last);
                    *last = h;
                    return first;
                    }
                }
            else if(Op(Pnode) == FUU
                    && (Pnode->v.fl & FRACTION)
                    && Op(Pnode->RIGHT) == DOT
                    && !equal(name, Pnode->RIGHT->LEFT)
                    )
                {
                return NULL;
                }
            Pnode = Pnode->RIGHT;
            if(!is_op(Pnode))
                {
                break;
                }
            }
        }
    return NULL;
    }


static void combiflags(psk pnode)
    {
    int lflgs;
    if((lflgs = pnode->LEFT->v.fl & UNOPS) != 0)
        {
        pnode->RIGHT = isolated(pnode->RIGHT);
        if(NOTHINGF(lflgs))
            {
            pnode->RIGHT->v.fl |= lflgs & ~NOT;
            pnode->RIGHT->v.fl ^= NOT | SUCCESS;
            }
        else
            pnode->RIGHT->v.fl |= lflgs;
        }
    }


static int is_dependent_of(psk el, psk input_buffer)
    {
    int ret;
    psk pnode;
    assert(!is_op(input_buffer));
    pnode = NULL;
    addr[1] = input_buffer;
    addr[2] = el;
    pnode = build_up(pnode, "(!dep:(? (\1.? \2 ?) ?)", NULL);
    pnode = eval(pnode);
    ret = isSUCCESS(pnode);
    wipe(pnode);
    return ret;
    }

static int search_opt(psk pnode, LONG opt)
    {
    while(is_op(pnode))
        {
        if(search_opt(pnode->LEFT, opt))
            return TRUE;
        pnode = pnode->RIGHT;
        }
    return PLOBJ(pnode) == opt;
    }

static void mmf(ppsk PPnode)
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
            goal = *pgoal = (psk)bmalloc(__LINE__, sizeof(knode));
            goal->v.fl = WHITE | SUCCESS;
            if(ext && nxtvar->n > 0)
                {
                goal = goal->LEFT = (psk)bmalloc(__LINE__, sizeof(knode));
                goal->v.fl = DOT | SUCCESS;
                sprintf(dim, "%d.%d", nxtvar->n, nxtvar->selector);
                goal->RIGHT = NULL;
                goal->RIGHT = build_up(goal->RIGHT, dim, NULL);
                }
            goal = goal->LEFT =
                (psk)bmalloc(__LINE__, sizeof(ULONG) + 1 + strlen((char*)VARNAME(nxtvar)));
            goal->v.fl = (READY | SUCCESS);
            strcpy((char*)(goal)+sizeof(ULONG), (char*)VARNAME(nxtvar));
            pgoal = &(*pgoal)->RIGHT;
            }
        }
    *pgoal = same_as_w(&nilNode);
    }

static void lstsub(psk pnode)
    {
    vars* nxtvar;
    unsigned char* name;
    int alphabet, n;
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
    }

static void lst(psk pnode)
    {
    while(is_op(pnode))
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
    lstsub(pnode);
    }

#if !defined NO_FOPEN
static fileStatus* findFileStatusByName(const char* name)
    {
    fileStatus* fs;
    for(fs = fs0
        ; fs
        ; fs = fs->next
        )
        if(!strcmp(fs->fname, name))
            return fs;
    return NULL;
    }

static fileStatus* allocateFileStatus(const char* name, FILE * fp
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                                      , Boolean dontcloseme
#endif
)
    {
    fileStatus* fs = (fileStatus*)bmalloc(__LINE__, sizeof(fileStatus));
    fs->fname = (char*)bmalloc(__LINE__, strlen(name) + 1);
    strcpy(fs->fname, name);
    fs->fp = fp;
#if !defined NO_LOW_LEVEL_FILE_HANDLING
    fs->dontcloseme = dontcloseme;
#endif
    fs->next = fs0;
    fs0 = fs;
    return fs0;
    }

static void deallocateFileStatus(fileStatus * fs)
    {
    fileStatus* fsPrevious, * fsaux;
    for(fsPrevious = NULL, fsaux = fs0
        ; fsaux != fs
        ; fsPrevious = fsaux, fsaux = fsaux->next
        )
        ;
    if(fsPrevious)
        fsPrevious->next = fs->next;
    else
        fs0 = fs->next;
    if(fs->fp)
        fclose(fs->fp);
    bfree(fs->fname);
#if !defined NO_LOW_LEVEL_FILE_HANDLING
    if(fs->stop)
#ifdef BMALLLOC
        bfree(fs->stop);
#else
        free(fs->stop);
#endif
#endif
    bfree(fs);
    }

fileStatus* mygetFileStatus(const char* filename, const char* mode
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                            , Boolean dontcloseme
#endif
)
    {
    FILE* fp = fopen(filename, mode);
    if(fp)
        {
        fileStatus* fs = allocateFileStatus(filename, fp
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                                            , dontcloseme
#endif
        );
        return fs;
        }
    return NULL;
    }

fileStatus* myfopen(const char* filename, const char* mode
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                    , Boolean dontcloseme
#endif
)
    {
#if !defined NO_FOPEN
    if(!findFileStatusByName(filename))
        {
        fileStatus* fs = mygetFileStatus(filename, mode
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                                         , dontcloseme
#endif
        );
        if(!fs && targetPath && strchr(mode, 'r'))
            {
            const char* p = filename;
            char* q;
            size_t len;
            while(*p)
                {
                if(*p == '\\' || *p == '/')
                    {
                    if(p == filename)
                        return NULL;
                    break;
                    }
                else if((*p == ':') && (p == filename + 1))
                    return NULL;
                ++p;
                }
            q = (char*)malloc((len = strlen(targetPath)) + strlen(filename) + 1);
            if(q)
                {
                strcpy(q, targetPath);
                strcpy(q + len, filename);
                fs = mygetFileStatus(q, mode
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                                     , dontcloseme
#endif
                );
                free(q);
                }
            }
        return fs;
        }
#endif
    return NULL;
    }
#endif

#if !defined NO_LOW_LEVEL_FILE_HANDLING
static LONG someopt(psk pnode, LONG opt[])
    {
    int i;
    assert(!is_op(pnode));
    for(i = 0; opt[i]; i++)
        if(PLOBJ(pnode) == opt[i])
            return opt[i];
    return 0L;
    }

#if !defined NO_FOPEN

static LONG tijdnr = 0L;

static int closeAFile(void)
    {
    fileStatus* fs, * fsmin;
    if(fs0 == NULL)
        return FALSE;
    for(fs = fs0, fsmin = fs0;
        fs != NULL;
        fs = fs->next)
        {
        if(!fs->dontcloseme
           && fs->filepos == -1L
           && fs->time < fsmin->time
           )
            fsmin = fs;
        }
    if(fsmin == NULL || fsmin->dontcloseme)
        {
        return FALSE;
        }
    fsmin->filepos = FTELL(fsmin->fp);
    /* fs->filepos != -1 means that the file is closed */
    fclose(fsmin->fp);
    fsmin->fp = NULL;
    return TRUE;
    }
#if defined NDEBUG
#define preparefp(fs,name,mode) preparefp(fs,mode)
#endif

static fileStatus* preparefp(fileStatus * fs, char* name, LONG mode)
    {
    assert(fs != NULL);
    assert(!strcmp(fs->fname, name));
    if(mode != 0L
       && mode != fs->mode
       && fs->fp != NULL)
        {
        fs->mode = mode;
        if((fs->fp = freopen(fs->fname, (char*)&(fs->mode), fs->fp)) == NULL)
            return NULL;
        fs->rwstatus = NoPending;
        }
    else if(fs->filepos >= 0L)
        {
        if((fs->fp = fopen(fs->fname, (char*)&(fs->mode))) == NULL)
            {
            if(closeAFile())
                fs->fp = fopen(fs->fname, (char*)&(fs->mode));
            }
        if(fs->fp == NULL)
            return NULL;
        fs->rwstatus = NoPending;
        FSEEK(fs->fp, fs->filepos, SEEK_SET);
        }
    fs->filepos = -1L;
    fs->time = tijdnr++;
    return fs;
    }
/*
Find an existing or create a fresh file handle for a known file name
If the file mode differs from the current file mode,
    reopen the file with the new file mode.
If the file is known but has been closed (e.g. to save file handles),
    open the file with the memorized file mode and go to the memorized position
*/
static fileStatus* search_fp(char* name, LONG mode)
    {
    fileStatus* fs;
    for(fs = fs0; fs; fs = fs->next)
        if(!strcmp(name, fs->fname))
            return preparefp(fs, name, mode);
    return NULL;
    }

static void setStop(fileStatus * fs, char* stopstring)
    {
    if(fs->stop)
#ifdef BMALLLOC
        bfree(fs->stop);
    fs->stop = (char*)bmalloc(__LINE__, strlen(stopstring + 1);
#else
                              free(fs->stop);
    fs->stop = (char*)malloc(strlen(stopstring) + 1);
#endif
    strcpy(fs->stop, stopstring);
    }

static int fil(ppsk PPnode)
    {
    FILE* fp;
    psk kns[4];
    LONG ind;
    int sh;
    psk pnode;
    static fileStatus* fs = NULL;
    char* name;

    static LONG types[] = { CHR,DEC,STRt,0L };
    static LONG whences[] = { SET,CUR,END,0L };
    static LONG modes[] = {
    O('r', 0 , 0),/*open text file for reading                                  */
    O('w', 0 , 0),/*create text file for writing, or trucate to zero length     */
    O('a', 0 , 0),/*append; open text file or create for writing at eof         */
    O('r','b', 0),/*open binary file for reading                                */
    O('w','b', 0),/*create binary file for writing, or trucate to zero length   */
    O('a','b', 0),/*append; open binary file or create for writing at eof       */
    O('r','+', 0),/*open text file for update (reading and writing)             */
    O('w','+', 0),/*create text file for update, or trucate to zero length      */
    O('a','+', 0),/*append; open text file or create for update, writing at eof */
    O('r','+','b'),
    O('r','b','+'),/*open binary file for update (reading and writing)           */
    O('w','+','b'),
    O('w','b','+'),/*create binary file for update, or trucate to zero length    */
    O('a','+','b'),
    O('a','b','+'),/*append;open binary file or create for update, writing at eof*/
    0L };

    static LONG type, numericalvalue, whence;
    union
        {
        LONG l;
        char c[4];
        } mode;

    union
        {
        short s;
        INT32_T i;
        char c[4];
        } snum;

    /*
    Fail if there are more than four arguments or if an argument is non-atomic
    */
    for(ind = 0, pnode = (*PPnode)->RIGHT;
        is_op(pnode);
        pnode = pnode->RIGHT)
        {
        if(is_op(pnode->LEFT) || ind > 2)
            {
            return FALSE;
            }
        kns[ind++] = pnode->LEFT;
        }
    kns[ind++] = pnode;
    for(; ind < 4;)
        kns[ind++] = NULL;

    /*
      FIRST ARGUMENT: File name
      if the current file name is different from the argument,
            reset the current file name
      if the first argument is empty, the current file name must not be NULL
      fil$(name)
      fil$(name,...)
      name field is optional in all fil$ operations
    */
    if(kns[0]->u.obj)
        {
        name = (char*)POBJ(kns[0]);
        if(fs && strcmp(name, fs->fname))
            {
            fs = NULL;
            }
        }
    else
        {
        if(fs)
            name = fs->fname;
        else
            {
            return FALSE;
            }
        }

    /*
      SECOND ARGUMENT: mode, type, whence or TEL
            if the second argument is a mode string,
                    the file handel is found and adapted to the  mode
                    or a new file handel is made
            else
                    file handel is set to current name

            If the second argument is set, fil$ does never read or write!
    */
    if(kns[1] && kns[1]->u.obj)
        {
        /*
        SECOND ARGUMENT:FILE MODE
        fil$(,"r")
        fil$(,"b")
        fil$(,"a")
        etc.
        */
        if((mode.l = someopt(kns[1], modes)) != 0L)
            {
            if(fs)
                fs = preparefp(fs, name, mode.l);
            else
                fs = search_fp(name, mode.l);
            if(fs == NULL)
                {
                if((fs = myfopen(name, (char*)&mode, FALSE)) == NULL)
                    {
                    if(closeAFile())
                        fs = myfopen(name, (char*)&mode, FALSE);
                    }
                if(fs == NULL)
                    {
                    return FALSE;
                    }
                fs->filepos = -1L;
                fs->mode = mode.l;
                fs->type = CHR;
                fs->size = 1;
                fs->number = 1;
                fs->time = tijdnr++;
                fs->rwstatus = NoPending;
                fs->stop = NULL;
                assert(fs->fp != 0);
                }
            assert(fs->fp != 0);
            return TRUE;
            }
        else
            {
            /*
            We do not open a file now, so we should have a file handle in memory.
            */
            if(fs)
                {
                fs = preparefp(fs, name, 0L);
                }
            else
                {
                fs = search_fp(name, 0L);
                }


            if(!fs)
                {
                return FALSE;
                }
            assert(fs->fp != 0);

            /*
            SECOND ARGUMENT:TYPE
            fil$(,CHR)
            fil$(,DEC)
            fil$(,CHR,size)
            fil$(,DEC,size)
            fil$(,CHR,size,number)
            fil$(,DEC,size,number)
            fil$(,STR)        (stop == NULL)
            fil$(,STR,stop)
            */
            if((type = someopt(kns[1], types)) != 0L)
                {
                fs->type = type;
                if(type == STRt)
                    {
                    /*
                      THIRD ARGUMENT: primary stopping character (e.g. "\n")

                      An empty string "" sets stopping string to NULL,
                      (Changed behaviour! Previously default stop was newline!)
                    */
                    if(kns[2] && kns[2]->u.obj)
                        {
                        setStop(fs, (char*)&kns[2]->u.obj);
                        }
                    else
                        {
                        if(fs->stop)
#ifdef BMALLLOC
                            bfree(fs->stop);
                        fs->stop = NULL;
#else
                            free(fs->stop);
                        fs->stop = NULL;
#endif
                        }
                    }
                else
                    {
                    /*
                      THIRD ARGUMENT: a size of elements to read or write
                    */
                    if(kns[2] && kns[2]->u.obj)
                        {
                        if(!INTEGER(kns[2]))
                            {
                            return FALSE;
                            }
                        fs->size = toLong(kns[2]);
                        }
                    else
                        {
                        fs->size = 1;
                        fs->number = 1;
                        }
                    /*
                      FOURTH ARGUMENT: the number of elements to read or write
                    */
                    if(kns[3] && kns[3]->u.obj)
                        {
                        if(!INTEGER(kns[3]))
                            {
                            return FALSE;
                            }
                        fs->number = toLong(kns[3]);
                        }
                    else
                        fs->number = 1;
                    }
                return TRUE;
                }
            /*
            SECOND ARGUMENT:POSITIONING
            fil$(,SET)
            fil$(,END)
            fil$(,CUR)
            fil$(,SET,offset)
            fil$(,END,offset)
            fil$(,CUR,offset)
            */
            else if((whence = someopt(kns[1], whences)) != 0L)
                {
                LONG offset;
                assert(fs->fp != 0);
                fs->time = tijdnr++;
                /*
                  THIRD ARGUMENT: an offset
                */
                if(kns[2] && kns[2]->u.obj)
                    {
                    if(!INTEGER(kns[2]))
                        {
                        return FALSE;
                        }
                    offset = toLong(kns[2]);
                    }
                else
                    offset = 0L;

                if((offset < 0L && whence == SEEK_SET)
                   || (offset > 0L && whence == SEEK_END)
                   || FSEEK(fs->fp, offset, whence == SET ? SEEK_SET
                            : whence == END ? SEEK_END
                            : SEEK_CUR))
                    {
                    deallocateFileStatus(fs);
                    fs = NULL;
                    return FALSE;
                    }
                fs->rwstatus = NoPending;
                return TRUE;
                }
            /*
            SECOND ARGUMENT:TELL POSITION
            fil$(,TEL)
            */
            else if(PLOBJ(kns[1]) == TEL)
                {
                char pos[11];
                sprintf(pos, LONGD, FTELL(fs->fp));
                wipe(*PPnode);
                *PPnode = scopy((const char*)pos);
                return TRUE;
                }
            else
                {
                return FALSE;
                }
            }
        /*
        return FALSE if the second argument is not empty but could not be recognised
        */
        }
    else
        {
        if(fs)
            {
            fs = preparefp(fs, name, 0L);
            }
        else
            {
            fs = search_fp(name, 0L);
            }
        }

    if(!fs)
        {
        return FALSE;
        }
    /*
    READ OR WRITE
    Now we are either going to read or to write
    */

    type = fs->type;
    mode.l = fs->mode;
    fp = fs->fp;

    /*
    THIRD ARGUMENT: the number of elements to read or write
    OR stop characters, depending on type (20081113)
    */

    if(kns[2] && kns[2]->u.obj)
        {
        if(type == STRt)
            {
            setStop(fs, (char*)&kns[2]->u.obj);
            }
        else
            {
            if(!INTEGER(kns[2]))
                {
                return FALSE;
                }
            fs->number = toLong(kns[2]);
            }
        }

    /*
    We allow 1, 2 or 4 bytes to be read/written in one fil$ operation
    These can be distributed over decimal numbers.
    */

    if(type == DEC)
        {
        switch((int)fs->size)
            {
            case 1:
                if(fs->number > 4)
                    fs->number = 4;
                break;
            case 2:
                if(fs->number > 2)
                    fs->number = 2;
                break;
            default:
                fs->size = 4; /*Invalid size declaration adjusted*/
                fs->number = 1;
            }
        }
    fs->time = tijdnr++;
    /*
    FOURTH ARGUMENT:VALUE TO WRITE
    */
    if(kns[3])
        {
        if(mode.c[0] != 'r' || mode.c[1] == '+' || mode.c[2] == '+')
            /*
            WRITE
            */
            {
            if(fs->rwstatus == Reading)
                {
                LONG fpos = FTELL(fs->fp);
                FSEEK(fs->fp, fpos, SEEK_SET);
                }
            fs->rwstatus = Writing;
            if(type == DEC)
                {
                numericalvalue = toLong(kns[3]);
                for(ind = 0; ind < fs->number; ind++)
                    switch((int)fs->size)
                        {
                        case 1:
                            fputc((int)numericalvalue & 0xFF, fs->fp);
                            numericalvalue >>= 8;
                            break;
                        case 2:
                            snum.s = (short)(numericalvalue & 0xFFFF);
                            fwrite(snum.c, 1, 2, fs->fp);
                            numericalvalue >>= 16;
                            break;
                        case 4:
                            snum.i = (INT32_T)(numericalvalue & 0xFFFFFFFF);
                            fwrite(snum.c, 1, 4, fs->fp);
                            assert(fs->number == 1);
                            break;
                        default:
                            fwrite((char*)&numericalvalue, 1, 4, fs->fp);
                            break;
                        }
                }
            else if(type == CHR)
                {
                size_t len, len1, minl;
                len1 = (size_t)(fs->size * fs->number);
                len = strlen((char*)POBJ(kns[3]));
                minl = len1 < len ? (len1 > 0 ? len1 : len) : len;
                if(fwrite(POBJ(kns[3]), 1, minl, fs->fp) == minl)
                    for(; len < len1 && putc(' ', fs->fp) != EOF; len++);
                }
            else /*if(type == STRt)*/
                {
                if(fs->stop
                   && fs->stop[0]
                   )/* stop string also works when writing. */
                    {
                    char* s = (char*)POBJ(kns[3]);
                    while(!strchr(fs->stop, *s))
                        fputc(*s++, fs->fp);
                    }
                else
                    {
                    fputs((char*)POBJ(kns[3]), fs->fp);
                    }
                }
            }
        else
            {
            /*
            Fail if not in write mode
            */
            return FALSE;
            }
        }
    else
        {
        if(mode.c[0] == 'r' || mode.c[1] == '+' || mode.c[2] == '+')
            {
            /*
            READ
            */
#define INPUTBUFFERSIZE 256
            unsigned char buffer[INPUTBUFFERSIZE];
            unsigned char* bbuffer;/* = buffer;*/
            if(fs->rwstatus == Writing)
                {
                fflush(fs->fp);
                fs->rwstatus = NoPending;
                }
            if(feof(fp))
                {
                return FALSE;
                }
            fs->rwstatus = Reading;
            if(type == STRt)
                {
                psk lpkn = NULL;
                psk rpkn = NULL;
                char* conc[2];
                int count = 0;
                LONG pos = FTELL(fp);
                int kar = 0;
                while(count < (INPUTBUFFERSIZE - 1)
                      && (kar = fgetc(fp)) != EOF
                      && (!fs->stop
                          || !strchr(fs->stop, kar)
                          )
                      )
                    {
                    buffer[count++] = (char)kar;
                    }
                if(count < (INPUTBUFFERSIZE - 1))
                    {
                    buffer[count] = '\0';
                    bbuffer = buffer;
                    }
                else
                    {
                    buffer[(INPUTBUFFERSIZE - 1)] = '\0';
                    while((kar = fgetc(fp)) != EOF
                          && (!fs->stop
                              || !strchr(fs->stop, kar)
                              )
                          )
                        count++;
                    if(count >= INPUTBUFFERSIZE)
                        {
                        bbuffer = (unsigned char*)bmalloc(__LINE__, (size_t)count + 1);
                        strcpy((char*)bbuffer, (char*)buffer);
                        FSEEK(fp, pos + (INPUTBUFFERSIZE - 1), SEEK_SET);
                        if(fread((char*)bbuffer + (INPUTBUFFERSIZE - 1), 1, count - (INPUTBUFFERSIZE - 1), fs->fp) == 0)
                            {
                            bfree(bbuffer);
                            return FALSE;
                            }
                        if(ferror(fs->fp))
                            {
                            bfree(bbuffer);
                            perror("fread");
                            return FALSE;
                            }
                        if(kar != EOF)
                            fgetc(fp); /* skip stopping character (which is in 'kar') */
                        }
                    else
                        bbuffer = buffer;
                    }
                source = bbuffer;
                lpkn = input(NULL, lpkn, 1, NULL, NULL);
                if(kar == EOF)
                    bbuffer[0] = '\0';
                else
                    {
                    bbuffer[0] = (char)kar;
                    bbuffer[1] = '\0';
                    }
                source = bbuffer;
                rpkn = input(NULL, rpkn, 1, NULL, NULL);
                conc[0] = "(\1.\2)";
                addr[1] = lpkn;
                addr[2] = rpkn;
                conc[1] = NULL;
                *PPnode = vbuildup(*PPnode, (const char**)conc);
                wipe(addr[1]);
                wipe(addr[2]);
                }
            else
                {
                size_t readbytes = fs->size * fs->number;
                if(readbytes >= INPUTBUFFERSIZE)
                    bbuffer = (unsigned char*)bmalloc(__LINE__, readbytes + 1);
                else
                    bbuffer = buffer;
                if((readbytes = fread((char*)bbuffer, (size_t)fs->size, (size_t)fs->number, fs->fp)) == 0
                   && fs->size != 0
                   && fs->number != 0
                   )
                    {
                    return FALSE;
                    }
                if(ferror(fs->fp))
                    {
                    perror("fread");
                    return FALSE;
                    }
                *(bbuffer + (int)readbytes) = 0;
                if(type == DEC)
                    {
                    numericalvalue = 0L;
                    sh = 0;
                    for(ind = 0; ind < fs->number;)
                        {
                        switch((int)fs->size)
                            {
                            case 1:
                                numericalvalue += (LONG)bbuffer[ind++] << sh;
                                sh += 8;
                                continue;
                            case 2:
                                numericalvalue += (LONG)(*(short*)(bbuffer + ind)) << sh;
                                ind += 2;
                                sh += 16;
                                continue;
                            case 4:
                                numericalvalue += (LONG)(*(INT32_T*)(bbuffer + ind)) << sh;
                                ind += 4;
                                sh += 32;
                                continue;
                            default:
                                numericalvalue += *(LONG*)bbuffer;
                                break;
                            }
                        break;
                        }
                    sprintf((char*)bbuffer, LONGD, numericalvalue);
                    }
                source = bbuffer;
                *PPnode = input(NULL, *PPnode, 1, NULL, NULL);
                }
            if(bbuffer != (unsigned char*)&buffer[0])
                bfree(bbuffer);
            return TRUE;
            }
        else
            {
            return FALSE;
            }
        }
    return TRUE;
    }
#endif
#endif

static int allopts(psk pnode, LONG opt[])
    {
    int i;
    while(is_op(pnode))
        {
        if(!allopts(pnode->LEFT, opt))
            return FALSE;
        pnode = pnode->RIGHT;
        }
    for(i = 0; opt[i]; i++)
        if(PLOBJ(pnode) == opt[i])
            return TRUE;
    return FALSE;
    }

static int flush(void)
    {
#ifdef __GNUC__
    return fflush(global_fpo);
#else
#if _BRACMATEMBEDDED
    if(WinFlush)
        WinFlush();
    return 1;
#else
    return 1;
#endif
#endif
    }


static int output(ppsk PPnode, void(*how)(psk k))
    {
    FILE* saveFpo;
    psk rightnode, rlnode, rrightnode, rrrightnode;
    static LONG opts[] =
        { APP,BIN,CON,EXT,MEM,LIN,NEW,RAW,TXT,VAP,WYD,0L };
    if(Op(rightnode = (*PPnode)->RIGHT) == COMMA)
        {
        int wide;
        saveFpo = global_fpo;
        rlnode = rightnode->LEFT;
        rrightnode = rightnode->RIGHT;
        wide = search_opt(rrightnode, WYD);
        if(wide)
            LineLength = WIDELINELENGTH;
        hum = !search_opt(rrightnode, LIN);
        listWithName = !search_opt(rrightnode, RAW);
        if(allopts(rrightnode, opts))
            {
            if(search_opt(rrightnode, MEM))
                {
                psk ret;
                telling = 1;
                process = tel;
                global_fpo = NULL;
                (*how)(rlnode);
                ret = (psk)bmalloc(__LINE__, sizeof(ULONG) + telling);
                ret->v.fl = READY | SUCCESS;
                process = glue;
                source = POBJ(ret);
                (*how)(rlnode);
                hum = 1;
                process = myputc;
                wipe(*PPnode);
                *PPnode = ret;
                global_fpo = saveFpo;
                return TRUE;
                }
            else
                {
                (*how)(rlnode);
                flush();
                addr[2] = rlnode;
                }
            }
        else if(Op(rrightnode) == COMMA
                && !is_op(rrightnode->LEFT)
                && allopts((rrrightnode = rrightnode->RIGHT), opts))
            {
#if !defined NO_FOPEN
            int binmode = ((how == lst) && !search_opt(rrrightnode, TXT)) || search_opt(rrrightnode, BIN);
            fileStatus* fs =
                myfopen((char*)POBJ(rrightnode->LEFT),
                        search_opt(rrrightnode, NEW)
                        ? (binmode
                           ? WRITEBIN
                           : WRITETXT
                           )
                        : (binmode
                           ? APPENDBIN
                           : APPENDTXT
                           )
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                        , TRUE
#endif
                );
            if(fs == NULL)
                {
                errorprintf("cannot open %s\n", POBJ(rrightnode->LEFT));
                global_fpo = saveFpo;
                hum = 1;
                return FALSE;
                }
            else
                {
                global_fpo = fs->fp;
                (*how)(rlnode);
                deallocateFileStatus(fs);
                global_fpo = saveFpo;
                addr[2] = rlnode;
                }
#else
            hum = 1;
            return FALSE;
#endif
            }
        else
            {
            (*how)(rightnode);
            flush();
            addr[2] = rightnode;
            }
        *PPnode = dopb(*PPnode, addr[2]);
        if(wide)
            LineLength = NARROWLINELENGTH;
        }
    else
        {
        (*how)(rightnode);
        flush();
        *PPnode = rightbranch(*PPnode);
        }
    hum = 1;
    listWithName = 1;
    return TRUE;
    }

#if !defined NO_FOPEN
static psk fileget(psk rlnode, int intval_i, psk Pnode, int* err, Boolean * GoOn)
    {
    FILE* saveFp;
    fileStatus* fs;
    saveFp = global_fpi;
    fs = myfopen((char*)POBJ(rlnode), (intval_i & OPT_TXT) ? READTXT : READBIN
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                 , TRUE
#endif
    );
    if(fs == NULL)
        {
        global_fpi = saveFp;
        return 0L;
        }
    else
        global_fpi = fs->fp;
    for(;;)
        {
        Pnode = input(global_fpi, Pnode, intval_i, err, GoOn);
        if(!*GoOn || *err)
            break;
        Pnode = eval(Pnode);
        }
    deallocateFileStatus(fs);
    global_fpi = saveFp;
    return Pnode;
    }
#endif

static LONG simil
(const char* s1
 , const char* s1end
 , const char* s2
 , const char* s2end
 , int* putf1
 , int* putf2
 , LONG * plen1
 , LONG * plen2
)
    {
    const char* ls1;
    const char* s1l = NULL;
    const char* s1r = NULL;
    const char* s2l = NULL;
    const char* s2r = NULL;
    LONG max;
    LONG len1;
    LONG len2 = 0;
    /* regard each character in s1 as possible starting point for match */
    for(max = 0, ls1 = s1, len1 = 0
        ; ls1 < s1end
        ; getCodePoint2(&ls1, putf1), ++len1)
        {
        const char* ls2;
        /* compare with s2 */
        for(ls2 = s2, len2 = 0; ls2 < s2end; getCodePoint2(&ls2, putf2), ++len2)
            {
            const char* lls1 = ls1;
            const char* lls2 = ls2;
            /* determine lenght of equal parts */
            LONG len12 = 0;
            for(;;)
                {
                if(lls1 < s1end && lls2 < s2end)
                    {
                    const char* ns1 = lls1, * ns2 = lls2;
                    int K1 = getCodePoint2(&ns1, putf1);
                    int K2 = getCodePoint2(&ns2, putf2);
                    if(toLowerUnicode(K1) == toLowerUnicode(K2))
                        {
                        ++len12;
                        lls1 = ns1;
                        lls2 = ns2;
                        }
                    else
                        break;
                    }
                else
                    break;
                }
            /* adapt score if needed */
            if(len12 > max)
                {
                max = len12;
                /* remember end points of left strings and start points of
                   right strings */
                s1l = ls1;
                s1r = lls1;
                s2l = ls2;
                s2r = lls2;
                }
            }
        }
    if(max)
        {
        max += simil(s1, s1l, s2, s2l, putf1, putf2, NULL, NULL) + simil(s1r, s1end, s2r, s2end, putf1, putf2, NULL, NULL);
        }
    if(plen1)
        {
        *plen1 = len1;
        }
    if(plen2)
        {
        if(len1 == 0)
            {
            for(len2 = 0; *s2; getCodePoint2(&s2, putf2), ++len2)
                ;
            }
        *plen2 = len2;
        }
    return max;
    }

static void Sim(char* draft, char* str1, char* str2)
    {
    int utf1 = 1;
    int utf2 = 1;
    LONG len1 = 0;
    LONG len2 = 0;
    LONG sim = simil(str1, str1 + strlen((char*)str1), str2, str2 + strlen((char*)str2), &utf1, &utf2, &len1, &len2);
    sprintf(draft, LONGD "/" LONGD, (2L * (LONG)sim), len1 + len2);
    }

static function_return_type execFnc(psk Pnode)
    {
    psk lnode;
    objectStuff Object = { 0,0,0 };

    lnode = Pnode->LEFT;
    if(is_op(lnode))
        {
        switch(Op(lnode))
            {
            case EQUALS: /* Anonymous function: (=.out$!arg)$HELLO -> lnode == (=.out$!arg) */
                lnode->RIGHT = Head(lnode->RIGHT);
                lnode = same_as_w(lnode->RIGHT);
                if(lnode) /* lnode is null if either the function wasn't found or it is a built-in member function of an object. */
                    {
                    if(Op(lnode) == DOT) /* The dot separating local variables from the function body. */
                        {
                        psh(&argNode, Pnode->RIGHT, NULL);
                        Pnode = dopb(Pnode, lnode);
                        wipe(lnode);
                        if(Op(Pnode) == DOT)
                            {
                            psh(Pnode->LEFT, &zeroNode, NULL);
                            Pnode = eval(Pnode);
                            /**** Evaluate anonymous function.

                                     (=.!arg)$XYZ
                            ****/
                            pop(Pnode->LEFT);
                            Pnode = dopb(Pnode, Pnode->RIGHT);
                            }
                        deleteNode(&argNode);
                        return functionOk(Pnode);
                        }
                    else
                        {
#if defined NO_EXIT_ON_NON_SEVERE_ERRORS
                        return functionFail(Pnode);
#else
                        errorprintf("(Syntax error) The following is not a function:\n\n  ");
                        writeError(Pnode->LEFT);
                        exit(116);
#endif
                        }
                    }
                break;
            case DOT: /* Method. Dot separating object name (or anonymous definition) from method name */
                lnode = findMethod(lnode, &Object);
                if(lnode)
                    {
                    if(Op(lnode) == DOT) /* The dot separating local variables from the function body. */
                        {
                        psh(&argNode, Pnode->RIGHT, NULL);

                        if(Object.self)
                            {
                            psh(&selfNode, Object.self, NULL); /* its */
                            Pnode = dopb(Pnode, lnode);
                            wipe(lnode);
                            /*
                            psh(&selfNode,self,NULL); Must precede dopb(...).
                            Example where this is relevant:

                            {?} ((==.lst$its).)'
                            (its=
                            =.lst$its);
                            {!} its

                            */
                            if(Object.object)
                                {
                                psh(&SelfNode, Object.object, NULL); /* Its */
                                if(Op(Pnode) == DOT)
                                    {
                                    psh(Pnode->LEFT, &zeroNode, NULL);
                                    Pnode = eval(Pnode);
                                    /**** Evaluate member function of built-in
                                          object from within an enveloping
                                          object. -----------------
                                                                   |
                                    ( new$hash:?myhash             |
                                    &   (                          |
                                        = ( myInsert               |
                                          = . (Its..insert)$!arg <-
                                          )
                                        )
                                      : (=?(myhash.))
                                    & (myhash..myInsert)$(X.12)
                                    )
                                    ****/
                                    pop(Pnode->LEFT);
                                    Pnode = dopb(Pnode, Pnode->RIGHT);
                                    }
                                deleteNode(&SelfNode);
                                }
                            else
                                {
                                if(Op(Pnode) == DOT)
                                    {
                                    psh(Pnode->LEFT, &zeroNode, NULL);
                                    Pnode = eval(Pnode);
                                    /**** Evaluate member function from
                                          within an other member function
                                        ( ( Object                    |
                                          =   (do=.out$!arg)          |
                                              ( A                     |
                                              =                       |
                                                . (its.do)$!arg <-----
                                              )
                                          )
                                        & (Object.A)$XYZ
                                        )
                                    ****/
                                    pop(Pnode->LEFT);
                                    Pnode = dopb(Pnode, Pnode->RIGHT);
                                    }
                                }
                            deleteNode(&selfNode);
                            deleteNode(&argNode);
                            return functionOk(Pnode);
                            }
                        else
                            { /* Unreachable? */
                            deleteNode(&argNode);
                            }
                        }
                    else if(Object.theMethod)
                        {
                        if(Object.theMethod((struct typedObjectnode*)Object.object, &Pnode))
                            {
                            /**** Evaluate a built-in method of an anonymous object.

                                    (new$hash.insert)$(a.2)
                            ****/
                            wipe(lnode); /* This is the built-in object, which got increased refcount to evade untimely wiping. */
                            return functionOk(Pnode);
                            }
                        else
                            { /* method failed. E.g. (new$hash.insert)$(a) */
                            wipe(lnode);
                            }
                        }
                    else
                        {
#if defined NO_EXIT_ON_NON_SEVERE_ERRORS
                        return functionFail(Pnode);
#else
                        errorprintf("(Syntax error) The following is not a function:\n\n  ");
                        writeError(Pnode->LEFT);
                        exit(116);
#endif
                        }
                    }
                else
                    {
                    if(Object.theMethod)
                        {
                        if(Object.theMethod((struct typedObjectnode*)Object.object, &Pnode))
                            {
                            /**** Evaluate a built-in method of a named object.
                                                            |
                                      new$hash:?H           |
                                    & (H..insert)$(XYZ.2) <-
                            ****/
                            return functionOk(Pnode);
                            }
                        }
                    }
                break;
            default:
                {
                /* /('(x.$x^2)) when evaluating /('(x.$x^2))$3 */
                if((Op(lnode) == FUU)
                   && (lnode->v.fl & FRACTION)
                   && (Op(lnode->RIGHT) == DOT)
                   && (!is_op(lnode->RIGHT->LEFT))
                   )
                    {
                    lnode = lambda(lnode->RIGHT->RIGHT, lnode->RIGHT->LEFT, Pnode->RIGHT);
                    /**** Evaluate a lambda expression

                            /('(x.$x^2))$3
                    ****/
                    if(lnode)
                        {
                        /*
                              /(
                               ' ( g
                                 .   /('(x.$g'($x'$x)))
                                   $ /('(x.$g'($x'$x)))
                                 )
                               )
                            $ /(
                               ' ( r
                                 . /(
                                    ' ( n
                                      .   $n:~>0&1
                                        | $n*($r)$($n+-1)
                                      )
                                    )
                                 )
                               )
                        */
                        wipe(Pnode);
                        Pnode = lnode;
                        }
                    else
                        {
                        /*
                            /('(x./('(x.$x ()$x))$aap))$noot
                        */
                        lnode = subtreecopy(Pnode->LEFT->RIGHT->RIGHT);
                        wipe(Pnode);
                        Pnode = lnode;
                        if(!is_op(Pnode) && !(Pnode->v.fl & INDIRECT))
                            Pnode->v.fl |= READY;  /*  /('(x.u))$7  */
                        }
                    return functionOk(Pnode);
                    }
                return functionFail(Pnode);/* completely wrong expression, e.g. (+(x.$x^2))$3 */
                }
            }
        }
    else
        {
#if 1
        lnode = getValueByVariableName(lnode);
#else
        /* Unsafe, esp. in JNI */
        if(oldlnode == lnode)
            lnode = lastEvaluatedFunction;  /* Speeding up! Esp. in map, vap, mop. */
        else
            {
            oldlnode = lnode;
            lnode = getValueByVariableName(lnode);
            lastEvaluatedFunction = lnode;
            }
#endif

        if(lnode) /* lnode is null if either the function wasn't found or it is a built-in member function of an object. */
            {
            if(Op(lnode) == DOT) /* The dot separating local variables from the function body. */
                {
                psh(&argNode, Pnode->RIGHT, NULL);
                Pnode = dopb(Pnode, lnode);
                if(Op(Pnode) == DOT)
                    {
                    psh(Pnode->LEFT, &zeroNode, NULL);
                    Pnode = eval(Pnode);
                    /**** Evaluate a named function
                                             |
                          (  f=.2*!arg       |
                          & f$6        <-----
                          )
                    ****/
                    pop(Pnode->LEFT);
                    Pnode = dopb(Pnode, Pnode->RIGHT);
                    }
                deleteNode(&argNode);
                return functionOk(Pnode);
                }
            else
                {
#if defined NO_EXIT_ON_NON_SEVERE_ERRORS
                return functionFail(Pnode);
#else
                errorprintf("(Syntax error) The following is not a function:\n\n  ");
                writeError(Pnode->LEFT);
                exit(116);
#endif
                }
            }
        }
    DBGSRC(errorprintf("Function not found"); writeError(Pnode); Printf("\n");)
        return functionFail(Pnode);
    }

static int hasSubObject(psk src)
    {
    while(is_op(src))
        {
        if(Op(src) == EQUALS)
            return TRUE;
        else
            {
            if(hasSubObject(src->LEFT))
                return TRUE;
            src = src->RIGHT;
            }
        }
    return FALSE;
    }

static psk objectcopysub(psk src);

static psk objectcopysub2(psk src) /* src is NOT an object */
    {
    psk goal;
    if(is_op(src) && hasSubObject(src))
        {
        goal = (psk)bmalloc(__LINE__, sizeof(knode));
        goal->v.fl = src->v.fl & COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
        goal->LEFT = objectcopysub(src->LEFT);
        goal->RIGHT = objectcopysub(src->RIGHT);
        return goal;
        }
    else
        return same_as_w(src);
    }

static psk objectcopysub(psk src)
    {
    psk goal;
    if(is_object(src))
        {
        if(ISBUILTIN((objectnode*)src))
            {
            return same_as_w(src);
            }
        else
            {
            goal = (psk)bmalloc(__LINE__, sizeof(objectnode));
#if WORD32
            ((typedObjectnode*)goal)->u.Int = 0;
#else
            ((typedObjectnode*)goal)->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
#endif
            }
        goal->v.fl = src->v.fl & COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
        goal->LEFT = same_as_w(src->LEFT);
        goal->RIGHT = same_as_w(src->RIGHT);
        return goal;
        }
    else
        return objectcopysub2(src);
    }

static psk objectcopy(psk src)
    {
    psk goal;
    if(is_object(src))                              /* Make a copy of this '=' node ... */
        {
        if(ISBUILTIN((objectnode*)src))
            {
            goal = (psk)bmalloc(__LINE__, sizeof(typedObjectnode));
#if WORD32
            ((typedObjectnode*)goal)->u.Int = BUILTIN;
#else
            ((typedObjectnode*)goal)->v.fl |= BUILT_IN;
#endif
            ((typedObjectnode*)goal)->vtab = ((typedObjectnode*)src)->vtab;
            ((typedObjectnode*)goal)->voiddata = NULL;
            }
        else
            {
            goal = (psk)bmalloc(__LINE__, sizeof(objectnode));
#if WORD32
            ((typedObjectnode*)goal)->u.Int = 0;
#else
            ((typedObjectnode*)goal)->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
#endif
            }
        goal->v.fl = src->v.fl & COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
        goal->LEFT = same_as_w(src->LEFT);
        /*?? This adds an extra level of copying, but ONLY for objects that have a '=' node as the lhs of the main '=' node*/
        /* What is it good for? Bart 20010220 */
        goal->RIGHT = objectcopysub(src->RIGHT); /* and of all '=' child nodes (but not of grandchildren!) */
        return goal;
        }
    else
        return objectcopysub2(src);
    }

static psk getObjectDef(psk src)
    {
    psk def;
    typedObjectnode* dest;
    if(!is_op(src))
        {
        classdef* df = classes;
        for(; df->name && strcmp(df->name, (char*)POBJ(src)); ++df)
            ;
        if(df->vtab)
            {
            dest = (typedObjectnode*)bmalloc(__LINE__, sizeof(typedObjectnode));
            dest->v.fl = EQUALS | SUCCESS;
            dest->left = same_as_w(&nilNode);
            dest->right = same_as_w(src);
#if WORD32
            dest->u.Int = BUILTIN;
#else
            dest->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
            dest->v.fl |= BUILT_IN;
#endif
            VOID(dest) = NULL;
            dest->vtab = df->vtab;
            return (psk)dest;
            }
        }
    else if(Op(src) == EQUALS)
        {
        src->RIGHT = Head(src->RIGHT);
        return objectcopy(src);
        }



    if((def = SymbolBinding_w(src, src->v.fl & DOUBLY_INDIRECT)) != NULL)
        {
        dest = (typedObjectnode*)bmalloc(__LINE__, sizeof(typedObjectnode));
        dest->v.fl = EQUALS | SUCCESS;
        dest->left = same_as_w(&nilNode);
        dest->right = objectcopy(def); /* TODO Head(&def) ? */
        wipe(def);
#if WORD32
        dest->u.Int = 0;
#else
        dest->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
#endif
        VOID(dest) = NULL;
        dest->vtab = NULL;
        return (psk)dest;
        }
    return NULL;
    }

static psk changeCase(psk Pnode
#if CODEPAGE850
                      , int dos
#endif
                      , int low)
    {
#if !CODEPAGE850
    const
#endif
        char* s;
    psk pnode;
    size_t len;
    pnode = same_as_w(Pnode);
    s = SPOBJ(Pnode);
    len = strlen((const char*)s);
    if(len > 0)
        {
        char* d;
        char* dwarn;
        char* buf = NULL;
        char* obuf;
        pnode = isolated(pnode);
        d = SPOBJ(pnode);
        obuf = d;
        dwarn = obuf + strlen((const char*)obuf) - 6;
#if CODEPAGE850
        if(dos)
            {
            if(low)
                {
                for(; *s; ++s)
                    {
                    *s = ISO8859toCodePage850(lowerEquivalent[(int)(const unsigned char)*s]);
                    }
                }
            else
                {
                for(; *s; ++s)
                    {
                    *s = ISO8859toCodePage850(upperEquivalent[(int)(const unsigned char)*s]);
                    }
                }
            }
        else
#endif
            {
            int isutf = 1;
            struct ccaseconv* t = low ? u2l : l2u;
            for(; *s;)
                {
                int S = getCodePoint2(&s, &isutf);
                int D = convertLetter(S, t);
                if(isutf)
                    {
                    if(d >= dwarn)
                        {
                        int nb = utf8bytes(D);
                        if(d + nb >= dwarn + 6)
                            {
                            /* overrun */
                            buf = (char*)bmalloc(__LINE__, 2 * ((dwarn + 6) - obuf));
                            dwarn = buf + 2 * ((dwarn + 6) - obuf) - 6;
                            memcpy(buf, obuf, d - obuf);
                            d = buf + (d - obuf);
                            if(obuf != SPOBJ(pnode))
                                bfree(obuf);
                            obuf = buf;
                            }
                        }
                    d = putCodePoint(D, d);
                    }
                else
                    *d++ = (unsigned char)D;
                }
            *d = 0;
            if(buf)
                {
                wipe(pnode);
                pnode = scopy(buf);
                bfree(buf);
                }
            }
        }
    return pnode;
    }

#if !defined NO_C_INTERFACE
static void* strToPointer(const char* str)
    {
    size_t res = 0;
    while(*str)
        res = 10 * res + (*str++ - '0');
    return (void*)res;
    }

static void pointerToStr(char* pc, void* p)
    {
    size_t P = (size_t)p;
    char* PC = pc;
    while(P)
        {
        *pc++ = (char)((P % 10) + '0');
        P /= 10;
        }
    *pc-- = '\0';
    while(PC < pc)
        {
        char sav = *PC;
        *PC = *pc;
        *pc = sav;
        ++PC;
        --pc;
        }
    }
#endif

#if O_S
static psk swi(psk Pnode, psk rlnode, psk rrightnode)
    {
    int i;
    union
        {
        unsigned int i[sizeof(os_regset) + 1];
        struct
            {
            int swicode;
            os_regset regs;
            } s;
        } u;
    char pc[121];
    for(i = 0; i < sizeof(os_regset) / sizeof(int); i++)
        u.s.regs.r[i] = 0;
    rrightnode = Pnode;
    i = 0;
    do
        {
        rrightnode = rrightnode->RIGHT;
        rlnode = is_op(rrightnode) ? rrightnode->LEFT : rrightnode;
        if(is_op(rlnode) || !INTEGER_NOT_NEG(rlnode))
            return functionFail(Pnode);
        u.i[i++] = (unsigned int)
            strtoul((char*)POBJ(rlnode), (char**)NULL, 10);
        } while(is_op(rrightnode) && i < 10);
#ifdef __TURBOC__
        intr(u.s.swicode, (struct REGPACK*)&u.s.regs);
        sprintf(pc, "0.%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
                u.i[1], u.i[2], u.i[3], u.i[4], u.i[5],
                u.i[6], u.i[7], u.i[8], u.i[9], u.i[10]);
#else
#if defined ARM
        i = (int)os_swix(u.s.swicode, &u.s.regs);
        sprintf(pc, "%u.%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
                i,
                u.i[1], u.i[2], u.i[3], u.i[4], u.i[5],
                u.i[6], u.i[7], u.i[8], u.i[9], u.i[10]);
#endif
#endif
        return build_up(Pnode, pc, NULL);
    }
#endif

static void stringreverse(char* a, size_t len)
    {
    char* b;
    b = a + len;
    while(a < --b)
        {
        char c = *a;
        *a = *b;
        *b = c;
        ++a;
        }
    }

#if 0
static void print_clock(char* pjotter, clock_t time)
    {
    if(time == (clock_t)-1)
        sprintf(pjotter, "-1");
    else
#if defined __TURBOC__ && !defined __BORLANDC__
        sprintf(pjotter, "%0lu/%lu", (ULONG)time, (ULONG)(10.0 * CLOCKS_PER_SEC));/* CLOCKS_PER_SEC == 18.2 */
#else
        sprintf(pjotter, LONG0D "/" LONGD, (LONG)time, (LONG)CLOCKS_PER_SEC);
#endif
    }
#else
static void print_clock(char* pjotter)
    {
    clock_t time = clock();
#ifdef DELAY_DUE_TO_INPUT
    time -= delayDueToInput;
#endif
    if(time == (clock_t)-1)
        sprintf(pjotter, "-1");
    else
#if defined __TURBOC__ && !defined __BORLANDC__
        sprintf(pjotter, "%0lu/%lu", (ULONG)time, (ULONG)(10.0 * CLOCKS_PER_SEC));/* CLOCKS_PER_SEC == 18.2 */
#else
        sprintf(pjotter, LONG0D "/" LONGD, (LONG)time, (LONG)CLOCKS_PER_SEC);
#endif
    }
#endif

#define LONGCASE

#ifdef LONGCASE
#define SWITCH(v) switch(v)
#define FIRSTCASE(a) case a :
#define CASE(a) case a :
#define DEFAULT default :
#else
#define SWITCH(v) LONG lob;lob = v;
#define FIRSTCASE(a) if(lob == a)
#define CASE(a) else if(lob == a)
#define DEFAULT else
#endif

static function_return_type setIndex(psk Pnode)
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

static function_return_type functions(psk Pnode)
    {
    static char draft[22];
    psk lnode, rightnode, rrightnode, rlnode;
    union {
        int i;
        ULONG ul;
        } intVal;
#if 1
    lnode = Pnode->LEFT;
#else
    if(is_op(lnode = Pnode->LEFT))
        {
        if(!is_op(rlnode = lnode->LEFT) && !strcmp((char*)POBJ(rlnode), "math"))
            {
            if(is_op(Pnode->RIGHT))
                {
                if(REAL_COMP(Pnode->RIGHT->LEFT))
                    {
                    if(is_op(Pnode->RIGHT->RIGHT))
                        {
                        ;
                        }
                    else
                        {
                        if(REAL_COMP(Pnode->RIGHT->RIGHT))
                            {
                            psk restl = Cmath2(lnode->RIGHT, Pnode->RIGHT->LEFT, Pnode->RIGHT->RIGHT);
                            if(restl)
                                {
                                wipe(Pnode);
                                Pnode = restl;
                                return functionOk(Pnode);
                                }
                            else
                                return functionFail(Pnode);
                            }
                        }
                    }
                }
            else
                {
                if(REAL_COMP(Pnode->RIGHT))
                    {
                    psk restl = Cmath(lnode->RIGHT, Pnode->RIGHT);
                    if(restl)
                        {
                        wipe(Pnode);
                        Pnode = restl;
                        return functionOk(Pnode);
                        }
                    else
                        return functionFail(Pnode);
                    }
                }
            }

        return find_func(Pnode);
        }
#endif
    rightnode = Pnode->RIGHT;
    {
    SWITCH(PLOBJ(lnode))
        {
        FIRSTCASE(STR) /* str$(arg arg arg .. ..) */
            {
            beNice = FALSE;
            hum = 0;
            telling = 1;
            process = tstr;
            result(rightnode);
            rlnode = (psk)bmalloc(__LINE__, sizeof(ULONG) + telling);
            process = pstr;
            source = POBJ(rlnode);
            result(rightnode);
            rlnode->v.fl = (READY | SUCCESS) | (numbercheck(SPOBJ(rlnode)) & ~DEFINITELYNONUMBER);
            beNice = TRUE;
            hum = 1;
            process = myputc;
            wipe(Pnode);
            Pnode = rlnode;
            return functionOk(Pnode);
            }
#if O_S
        CASE(SWI) /* swi$(<interrupt number>.(input regs)) */
            {
            Pnode = swi(Pnode, rlnode, rrightnode);
            return functionOk(Pnode);
            }
#endif
#if _BRACMATEMBEDDED
#if defined PYTHONINTERFACE
        CASE(NI) /* Ni$"Statements to be executed by Python" */
            {
            if(Ni && !is_op(rightnode) && !HAS_VISIBLE_FLAGS_OR_MINUS(rightnode))
                {
                Ni((const char*)POBJ(rightnode));
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
        CASE(NII) /* Ni!$"Expression to be evaluated by Python" */
            {
            if(Nii && !is_op(rightnode) && !HAS_VISIBLE_FLAGS_OR_MINUS(rightnode))
                {
                const char* val;
                val = Nii((const char*)POBJ(rightnode));
                wipe(Pnode);
                Pnode = scopy((const char*)val);
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
#endif
#endif

#ifdef ERR
        /*#if !_BRACMATEMBEDDED*/
        CASE(ERR) /* err $ <file name to direct errorStream to> */
            {
            if(!is_op(rightnode))
                {
                if(redirectError((char*)POBJ(rightnode)))
                    return functionOk(Pnode);
                }
            return functionFail(Pnode);
            }
        /*#endif*/
#endif
#if !defined NO_C_INTERFACE
        CASE(ALC)  /* alc $ <number of bytes> */
            {
            void* p;
            if(is_op(rightnode)
               || !INTEGER_POS(rightnode)
               || (p = bmalloc(__LINE__, (int)strtoul((char*)POBJ(rightnode), (char**)NULL, 10)))
               == NULL)
                return functionFail(Pnode);
            pointerToStr(draft, p);
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(FRE) /* fre $ <pointer> */
            {
            void* p;
            if(is_op(rightnode) || !INTEGER_POS(rightnode))
                return functionFail(Pnode);
            p = strToPointer((char*)POBJ(rightnode));
            pskfree((psk)p);
            return functionOk(Pnode);
            }
        CASE(PEE) /* pee $ (<pointer>[,<number of bytes>]) (1,2,4,8)*/
            {
            void* p;
            intVal.i = 1;
            if(is_op(rightnode))
                {
                rlnode = rightnode->LEFT;
                rrightnode = rightnode->RIGHT;
                if(!is_op(rrightnode))
                    {
                    switch(rrightnode->u.obj)
                        {
                        case '2':
                            intVal.i = 2;
                            break;
                        case '4':
                            intVal.i = 4;
                            break;
                        }
                    }
                }
            else
                rlnode = rightnode;
            if(is_op(rlnode) || !INTEGER_POS(rlnode))
                return functionFail(Pnode);
            p = strToPointer((char*)POBJ(rlnode));
            p = (void*)((char*)p - (ptrdiff_t)((size_t)p % intVal.i));
            switch(intVal.i)
                {
                case 2:
                    sprintf(draft, "%hu", *(short unsigned int*)p);
                    break;
                case 4:
                    sprintf(draft, "%lu", (unsigned long)*(UINT32_T*)p);
                    break;
#ifndef __BORLANDC__
#if (!defined ARM || defined __SYMBIAN32__)
                case 8:
                    sprintf(draft, "%llu", *(unsigned long long*)p);
                    break;
#endif
#endif                    
                case 1:
                default:
                    sprintf(draft, "%u", (unsigned int)*(unsigned char*)p);
                    break;
                }
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(POK) /* pok $ (<pointer>,<number>[,<number of bytes>]) */
            {
            psk rrlnode;
            void* p;
            LONG val;
            intVal.i = 1;
            if(!is_op(rightnode))
                return functionFail(Pnode);
            rlnode = rightnode->LEFT;
            rrightnode = rightnode->RIGHT;
            if(is_op(rrightnode))
                {
                psk rrrightnode;
                rrrightnode = rrightnode->RIGHT;
                rrlnode = rrightnode->LEFT;
                if(!is_op(rrrightnode))
                    {
                    switch(rrrightnode->u.obj)
                        {
                        case '2':
                            intVal.i = 2;
                            break;
                        case '4':
                            intVal.i = 4;
                            break;
                        case '8':
                            intVal.i = 8;
                            break;
                        default:
                            ;
                        }
                    }
                }
            else
                rrlnode = rrightnode;
            if(is_op(rlnode) || !INTEGER_POS(rlnode)
               || is_op(rrlnode) || !INTEGER(rrlnode))
                return functionFail(Pnode);
            p = strToPointer((char*)POBJ(rlnode));
            p = (void*)((char*)p - (ptrdiff_t)((size_t)p % intVal.i));
            val = toLong(rrlnode);
            switch(intVal.i)
                {
                case 2:
                    *(unsigned short int*)p = (unsigned short int)val;
                    break;
                case 4:
                    *(UINT32_T*)p = (UINT32_T)val;
                    break;
#ifndef __BORLANDC__
                case 8:
                    *(ULONG*)p = (ULONG)val;
                    break;
#endif
                case 1:
                default:
                    *(unsigned char*)p = (unsigned char)val;
                    break;
                }
            return functionOk(Pnode);
            }
        CASE(FNC) /* fnc $ (<function pointer>.<struct pointer>) */
            {
            typedef Boolean(*fncTp)(void*);
            union
                {
                fncTp pfnc; /* Hoping this works. */
                void* vp;  /* Pointers to data and pointers to functions may
                               have different sizes. */
                } u;
            void* argStruct;
            if(sizeof(int(*)(void*)) != sizeof(void*) || !is_op(rightnode))
                return functionFail(Pnode);
            u.vp = strToPointer((char*)POBJ(rightnode->LEFT));
            if(!u.pfnc)
                return functionFail(Pnode);
            argStruct = strToPointer((char*)POBJ(rightnode->RIGHT));
            return u.pfnc(argStruct) ? functionOk(Pnode) : functionFail(Pnode);
            }
#endif
        CASE(X2D) /* x2d $ hexnumber */
            {
            char* endptr;
            ULONG val;
            if(is_op(rightnode)
               || HAS_VISIBLE_FLAGS_OR_MINUS(rightnode)
               )
                return functionFail(Pnode);
            errno = 0;
            val = STRTOUL((char*)POBJ(rightnode), &endptr, 16);
            if(errno == ERANGE || (endptr && *endptr))
                return functionFail(Pnode); /*not all characters scanned*/
            sprintf(draft, LONGU, val);
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(D2X) /* d2x $ decimalnumber */
            {
            char* endptr;
            ULONG val;
            if(is_op(rightnode) || !INTEGER_NOT_NEG(rightnode))
                return functionFail(Pnode);
#ifdef __BORLANDC__
            if(strlen((char*)POBJ(rightnode)) > 10
               || strlen((char*)POBJ(rightnode)) == 10
               && strcmp((char*)POBJ(rightnode), "4294967295") > 0
               )
                return functionFail(Pnode); /*not all characters scanned*/
#endif
            errno = 0;
            val = STRTOUL((char*)POBJ(rightnode), &endptr, 10);
            if(errno == ERANGE
               || (endptr && *endptr)
               )
                return functionFail(Pnode); /*not all characters scanned*/
            sprintf(draft, LONGX, val);
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(Chr) /* chr $ number */
            {
            if(is_op(rightnode) || !INTEGER_POS(rightnode))
                return functionFail(Pnode);
            intVal.ul = strtoul((char*)POBJ(rightnode), (char**)NULL, 10);
            if(intVal.ul > 255)
                return functionFail(Pnode);
            draft[0] = (char)intVal.ul;
            draft[1] = 0;
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(Chu) /* chu $ number */
            {
            ULONG val;
            if(is_op(rightnode) || !INTEGER_POS(rightnode))
                return functionFail(Pnode);
            val = STRTOUL((char*)POBJ(rightnode), (char**)NULL, 10);
            if(putCodePoint(val, draft) == NULL)
                return functionFail(Pnode);
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(ASC) /* asc $ character */
            {
            if(is_op(rightnode))
                return functionFail(Pnode);
            sprintf(draft, "%d", (int)rightnode->u.obj);
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }
        CASE(UGC)
            {
            if(is_op(rightnode))
                {
                Pnode->v.fl |= FENCE;
                return functionFail(Pnode);
                }
            else
                {
                const char* s = (const char*)POBJ(rightnode);
                if(*s)
                    {
                    intVal.i = getCodePoint(&s);
                    if(intVal.i < 0 || *s)
                        {
                        if(intVal.i != -2)
                            {
                            Pnode->v.fl |= IMPLIEDFENCE;
                            }
                        return functionFail(Pnode);
                        }
                    sprintf(draft, "%s", gencat(intVal.i));
                    wipe(Pnode);
                    Pnode = scopy((const char*)draft);
                    return functionOk(Pnode);
                    }
                else
                    return functionFail(Pnode);
                }
            }
        CASE(UTF)
            {
            /*
            @(abcædef:? (%@>"~" ?:?a & utf$!a) ?)
            @(str$(abc chu$200 def):? (%@>"~" ?:?a & utf$!a) ?)
            */
            if(is_op(rightnode))
                {
                Pnode->v.fl |= FENCE;
                return functionFail(Pnode);
                }
            else
                {
                const char* s = (const char*)POBJ(rightnode);
                intVal.i = getCodePoint(&s);
                if(intVal.i < 0 || *s)
                    {
                    if(intVal.i != -2)
                        {
                        Pnode->v.fl |= IMPLIEDFENCE;
                        }
                    return functionFail(Pnode);
                    }
                sprintf(draft, "%d", intVal.i);
                wipe(Pnode);
                Pnode = scopy((const char*)draft);
                return functionOk(Pnode);
                }
            }
#if !defined NO_LOW_LEVEL_FILE_HANDLING
#if !defined NO_FOPEN
        CASE(FIL) /* fil $ (<name>,[<offset>,[set|cur|end]]) */
            {
            return fil(&Pnode) ? functionOk(Pnode) : functionFail(Pnode);
            }
#endif
#endif
        CASE(FLG) /* flg $ <expr>  or flg$(=<expr>) */
            {
            if(is_object(rightnode) && !(rightnode->LEFT->v.fl & VISIBLE_FLAGS))
                rightnode = rightnode->RIGHT;
            intVal.ul = rightnode->v.fl;
            addr[3] = same_as_w(rightnode);
            addr[3] = isolated(addr[3]);
            addr[3]->v.fl = addr[3]->v.fl & ~VISIBLE_FLAGS;
            addr[2] = same_as_w(&nilNode);
            addr[2] = isolated(addr[2]);
            addr[2]->v.fl &= ~VISIBLE_FLAGS;
            addr[2]->v.fl |= VISIBLE_FLAGS & intVal.ul;
            if(addr[2]->v.fl & INDIRECT)
                {
                addr[2]->v.fl &= ~READY; /* {?} flg$(=!a):(=?X.?)&lst$X */
                addr[3]->v.fl |= READY;  /* {?} flg$(=!a):(=?.?Y)&!Y */
                }
            if(NOTHINGF(intVal.ul))
                {
                addr[2]->v.fl ^= SUCCESS;
                addr[3]->v.fl ^= SUCCESS;
                }
            sprintf(draft, "=\2.\3");
            Pnode = build_up(Pnode, draft, NULL);
            wipe(addr[2]);
            wipe(addr[3]);
            return functionOk(Pnode);
            }
        CASE(GLF) /* glf $ (=<flags>.<exp>) : (=?a)  a=<flags><exp> */
            {
            if(is_object(rightnode)
               && Op(rightnode->RIGHT) == DOT
               )
                {
                intVal.ul = rightnode->RIGHT->LEFT->v.fl & VISIBLE_FLAGS;
                if(intVal.ul && (rightnode->RIGHT->RIGHT->v.fl & intVal.ul))
                    return functionFail(Pnode);
                addr[3] = same_as_w(rightnode->RIGHT->RIGHT);
                addr[3] = isolated(addr[3]);
                addr[3]->v.fl |= intVal.ul;
                if(NOTHINGF(intVal.ul))
                    {
                    addr[3]->v.fl ^= SUCCESS;
                    }
                if(intVal.ul & INDIRECT)
                    {
                    addr[3]->v.fl &= ~READY;/* ^= --> &= ~ */
                    }
                sprintf(draft, "=\3");
                Pnode = build_up(Pnode, draft, NULL);
                wipe(addr[3]);
                return functionOk(Pnode);
                }
            return functionFail(Pnode);
            }
#if TELMAX
        CASE(BEZ) /* bez $  */
            {
            Bez(draft);
            Pnode = build_up(Pnode, draft, NULL);
#if TELLING
            bezetting();
#endif
            return functionOk(Pnode);
            }
#endif
        CASE(MMF) /* mem $ [EXT] */
            {
            mmf(&Pnode);
            return functionOk(Pnode);
            }
        CASE(MOD)
            {
            if(RATIONAL_COMP(rlnode = rightnode->LEFT) &&
               RATIONAL_COMP_NOT_NUL(rrightnode = rightnode->RIGHT))
                {
                psk pnode;
                pnode = qModulo(rlnode, rrightnode);
                wipe(Pnode);
                Pnode = pnode;
                return functionOk(Pnode);
                }
            return functionFail(Pnode);
            }
        CASE(REV)
            {
            if(!is_op(rightnode))
                {
                size_t len = strlen((char*)POBJ(rightnode));
                psk pnode;
                pnode = same_as_w(rightnode);
                if(len > 1)
                    {
                    pnode = isolated(pnode);
                    stringreverse((char*)POBJ(pnode), len);
                    }
                wipe(Pnode);
                Pnode = pnode;
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
        CASE(LOW)
            {
            psk pnode;
            if(!is_op(rightnode))
                pnode = changeCase(rightnode
#if CODEPAGE850
                                   , FALSE
#endif
                                   , TRUE);
            else if(!is_op(rlnode = rightnode->LEFT))
                pnode = changeCase(rlnode
#if CODEPAGE850
                                   , search_opt(rightnode->RIGHT, DOS)
#endif
                                   , TRUE);
            else
                return functionFail(Pnode);
            wipe(Pnode);
            Pnode = pnode;
            return functionOk(Pnode);
            }
        CASE(UPP)
            {
            psk pnode;
            if(!is_op(rightnode))
                pnode = changeCase(rightnode
#if CODEPAGE850
                                   , FALSE
#endif
                                   , FALSE);
            else if(!is_op(rlnode = rightnode->LEFT))
                pnode = changeCase(rlnode
#if CODEPAGE850
                                   , search_opt(rightnode->RIGHT, DOS)
#endif
                                   , FALSE);
            else
                return functionFail(Pnode);
            wipe(Pnode);
            Pnode = pnode;
            return functionOk(Pnode);
            }
        CASE(DIV)
            {
            if(is_op(rightnode)
               && RATIONAL_COMP(rlnode = rightnode->LEFT)
               && RATIONAL_COMP_NOT_NUL(rrightnode = rightnode->RIGHT)
               )
                {
                psk pnode;
                pnode = qIntegerDivision(rlnode, rrightnode);
                wipe(Pnode);
                Pnode = pnode;
                return functionOk(Pnode);
                }
            return functionFail(Pnode);
            }
        CASE(DEN)
            {
            if(RATIONAL_COMP(rightnode))
                {
                Pnode = qDenominator(Pnode);
                }
            return functionOk(Pnode);
            }
        CASE(LST)
            {
            return output(&Pnode, lst) ? functionOk(Pnode) : functionFail(Pnode);
            }
        CASE(MAP) /* map $ (<function>.<list>) */
            {
            if(is_op(rightnode))
                {/*XXX*/
                psk nnode;
                psk nPnode;
                ppsk ppnode = &nPnode;
                rrightnode = rightnode->RIGHT;
                while(is_op(rrightnode) && Op(rrightnode) == WHITE)
                    {
                    nnode = (psk)bmalloc(__LINE__, sizeof(knode));
                    nnode->v.fl = Pnode->v.fl;
                    nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                    nnode->LEFT = same_as_w(rightnode->LEFT);
                    nnode->RIGHT = same_as_w(rrightnode->LEFT);
                    rlnode = setIndex(nnode);
                    if(rlnode)
                        nnode = rlnode;
                    else
                        {
                        if(not_built_in(nnode->LEFT)) /* Do not use ternary operator! That eats stack! */
                            nnode = execFnc(nnode);
                        else
                            {
                            rlnode = functions(nnode);
                            if(rlnode)
                                nnode = rlnode;
                            else
                                nnode = execFnc(nnode);
                            }
                        }

                    if(!is_op(nnode) && IS_NIL(nnode))
                        {
                        wipe(nnode);
                        }
                    else
                        {
                        rlnode = (psk)bmalloc(__LINE__, sizeof(knode));
                        rlnode->v.fl = WHITE | SUCCESS;
                        *ppnode = rlnode;
                        ppnode = &(rlnode->RIGHT);
                        rlnode->LEFT = nnode;
                        }

                    rrightnode = rrightnode->RIGHT;
                    }
                if(is_op(rrightnode) || !IS_NIL(rrightnode))
                    {
                    nnode = (psk)bmalloc(__LINE__, sizeof(knode));
                    nnode->v.fl = Pnode->v.fl;
                    nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                    nnode->LEFT = same_as_w(rightnode->LEFT);
                    nnode->RIGHT = same_as_w(rrightnode);
                    rlnode = setIndex(nnode);
                    if(rlnode)
                        nnode = rlnode;
                    else
                        {
                        if(not_built_in(nnode->LEFT)) /* Do not use ternary operator! That eats stack! */
                            nnode = execFnc(nnode);
                        else
                            {
                            rlnode = functions(nnode);
                            if(rlnode)
                                nnode = rlnode;
                            else
                                nnode = execFnc(nnode);
                            }
                        }
                    *ppnode = nnode;
                    }
                else
                    {
                    *ppnode = same_as_w(rrightnode);
                    }
                wipe(Pnode);
                Pnode = nPnode;
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
        CASE(MOP)/*     mop $ (<function>.<tree1>.(=<tree2>))
                 mop regards tree1 as a right descending 'list' with a backbone
                 operator that is the same as the heading operator of tree 2.
                 For example,
                        mop$((=.!arg).2*a+e\Lb+c^2.(=+))
                 generates the list
                        2*a e\Lb c^2
                 */
            {
            if(is_op(rightnode))
                {/*XXX*/
                psk nnode;
                psk nPnode;
                ppsk ppnode = &nPnode;
                rrightnode = rightnode->RIGHT;
                if(Op(rightnode) == DOT)
                    {
                    if(Op(rrightnode) == DOT)
                        {
                        lnode = rrightnode->RIGHT;
                        rrightnode = rrightnode->LEFT;
                        if(Op(lnode) == EQUALS)
                            {
                            intVal.ul = Op(lnode->RIGHT);
                            if(intVal.ul)
                                {
                                while(is_op(rrightnode) && Op(rrightnode) == intVal.ul)
                                    {
                                    nnode = (psk)bmalloc(__LINE__, sizeof(knode));
                                    nnode->v.fl = Pnode->v.fl;
                                    nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                                    nnode->LEFT = same_as_w(rightnode->LEFT);
                                    nnode->RIGHT = same_as_w(rrightnode->LEFT);
                                    rlnode = setIndex(nnode);
                                    if(rlnode)
                                        nnode = rlnode;
                                    else
                                        {
                                        if(not_built_in(nnode->LEFT)) /* Do not use ternary operator! That eats stack! */
                                            nnode = execFnc(nnode);
                                        else
                                            {
                                            rlnode = functions(nnode);
                                            if(rlnode)
                                                nnode = rlnode;
                                            else
                                                nnode = execFnc(nnode);
                                            }
                                        }

                                    if(!is_op(nnode) && IS_NIL(nnode))
                                        {
                                        wipe(nnode);
                                        }
                                    else
                                        {
                                        rlnode = (psk)bmalloc(__LINE__, sizeof(knode));
                                        rlnode->v.fl = WHITE | SUCCESS;
                                        *ppnode = rlnode;
                                        ppnode = &(rlnode->RIGHT);
                                        rlnode->LEFT = nnode;
                                        }
                                    rrightnode = rrightnode->RIGHT;
                                    }
                                }
                            else
                                {
                                /* No operator specified */
                                return functionFail(Pnode);
                                }
                            }
                        else
                            {
                            /* The last argument must have heading = operator */
                            return functionFail(Pnode);
                            }
                        }
                    else
                        {
                        /* Expecting a dot */
                        return functionFail(Pnode);
                        }
                    }
                else
                    {
                    /* Expecting a dot */
                    return functionFail(Pnode);
                    }
                if(is_op(rrightnode) || !IS_NIL(rrightnode))
                    {
                    nnode = (psk)bmalloc(__LINE__, sizeof(knode));
                    nnode->v.fl = Pnode->v.fl;
                    nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                    nnode->LEFT = same_as_w(rightnode->LEFT);
                    nnode->RIGHT = same_as_w(rrightnode);
                    rlnode = setIndex(nnode);
                    if(rlnode)
                        nnode = rlnode;
                    else
                        {
                        if(not_built_in(nnode->LEFT)) /* Do not use ternary operator! That eats stack! */
                            nnode = execFnc(nnode);
                        else
                            {
                            rlnode = functions(nnode);
                            if(rlnode)
                                nnode = rlnode;
                            else
                                nnode = execFnc(nnode);
                            }
                        }

                    *ppnode = nnode;
                    }
                else
                    {
                    *ppnode = same_as_w(rrightnode);
                    }
                wipe(Pnode);
                Pnode = nPnode;
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
        CASE(Vap) /*    vap $ (<function>.<string>.<char>)
                     or vap $ (<function>.<string>)
                     Second form splits in characters. */
            {
            if(is_op(rightnode))
                {/*XXX*/
                rrightnode = rightnode->RIGHT;
                if(is_op(rrightnode))
                    { /* first form */
                    psk sepnode = rrightnode->RIGHT;
                    rrightnode = rrightnode->LEFT;
                    if(is_op(sepnode) || is_op(rrightnode))
                        {
                        return functionFail(Pnode);
                        }
                    else
                        {
                        const char* separator = &sepnode->u.sobj;
                        if(!*separator)
                            {
                            return functionFail(Pnode);
                            }
                        else
                            {
                            char* subject = &rrightnode->u.sobj;
                            psk nPnode;
                            ppsk ppnode = &nPnode;
                            char* oldsubject = subject;
                            while(subject)
                                {
                                psk nnode;
                                nnode = (psk)bmalloc(__LINE__, sizeof(knode));
                                nnode->v.fl = Pnode->v.fl;
                                nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                                nnode->LEFT = same_as_w(rightnode->LEFT);
                                subject = strstr(oldsubject, separator);
                                if(subject)
                                    {
                                    *subject = '\0';
                                    nnode->RIGHT = scopy(oldsubject);
                                    *subject = separator[0];
                                    oldsubject = subject + strlen(separator);
                                    }
                                else
                                    {
                                    nnode->RIGHT = scopy(oldsubject);
                                    }
                                rlnode = setIndex(nnode);
                                if(rlnode)
                                    nnode = rlnode;
                                else
                                    {
                                    if(not_built_in(nnode->LEFT)) /* Do not use ternary operator! That eats stack! */
                                        nnode = execFnc(nnode);
                                    else
                                        {
                                        rlnode = functions(nnode);
                                        if(rlnode)
                                            nnode = rlnode;
                                        else
                                            nnode = execFnc(nnode);
                                        }
                                    }

                                if(subject)
                                    {
                                    rlnode = (psk)bmalloc(__LINE__, sizeof(knode));
                                    rlnode->v.fl = WHITE | SUCCESS;
                                    *ppnode = rlnode;
                                    ppnode = &(rlnode->RIGHT);
                                    rlnode->LEFT = nnode;
                                    }
                                else
                                    {
                                    *ppnode = nnode;
                                    }
                                }
                            wipe(Pnode);
                            Pnode = nPnode;
                            return functionOk(Pnode);
                            }
                        }
                    }
                else
                    {
                    /* second form */
                    const char* subject = &rrightnode->u.sobj;
                    psk nPnode = 0;
                    ppsk ppnode = &nPnode;
                    const char* oldsubject = subject;
                    int k;
                    if(hasUTF8MultiByteCharacters(subject))
                        {
                        for(; (k = getCodePoint(&subject)) > 0; oldsubject = subject)
                            {
                            psk nnode;
                            nnode = (psk)bmalloc(__LINE__, sizeof(knode));
                            nnode->v.fl = Pnode->v.fl;
                            nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                            nnode->LEFT = same_as_w(rightnode->LEFT);
                            nnode->RIGHT = charcopy(oldsubject, subject);
                            rlnode = setIndex(nnode);
                            if(rlnode)
                                nnode = rlnode;
                            else
                                {
                                if(not_built_in(nnode->LEFT)) /* Do not use ternary operator! That eats stack! */
                                    nnode = execFnc(nnode);
                                else
                                    {
                                    rlnode = functions(nnode);
                                    if(rlnode)
                                        nnode = rlnode;
                                    else
                                        nnode = execFnc(nnode);
                                    }
                                }

                            if(*subject)
                                {
                                rlnode = (psk)bmalloc(__LINE__, sizeof(knode));
                                rlnode->v.fl = WHITE | SUCCESS;
                                *ppnode = rlnode;
                                ppnode = &(rlnode->RIGHT);
                                rlnode->LEFT = nnode;
                                }
                            else
                                {
                                *ppnode = nnode;
                                }
                            }
                        }
                    else
                        {
                        for(; (k = *subject++) != 0; oldsubject = subject)
                            {
                            psk nnode;
                            nnode = (psk)bmalloc(__LINE__, sizeof(knode));
                            nnode->v.fl = Pnode->v.fl;
                            nnode->v.fl &= COPYFILTER;/* ~ALL_REFCOUNT_BITS_SET;*/
                            nnode->LEFT = same_as_w(rightnode->LEFT);
                            nnode->RIGHT = charcopy(oldsubject, subject);
                            rlnode = setIndex(nnode);
                            if(rlnode)
                                nnode = rlnode;
                            else
                                {
                                if(not_built_in(nnode->LEFT)) /* Do not use ternary operator! That eats stack! */
                                    nnode = execFnc(nnode);
                                else
                                    {
                                    rlnode = functions(nnode);
                                    if(rlnode)
                                        nnode = rlnode;
                                    else
                                        nnode = execFnc(nnode);
                                    }
                                }

                            if(*subject)
                                {
                                rlnode = (psk)bmalloc(__LINE__, sizeof(knode));
                                rlnode->v.fl = WHITE | SUCCESS;
                                *ppnode = rlnode;
                                ppnode = &(rlnode->RIGHT);
                                rlnode->LEFT = nnode;
                                }
                            else
                                {
                                *ppnode = nnode;
                                }
                            }
                        }
                    wipe(Pnode);
                    Pnode = nPnode ? nPnode : same_as_w(&nilNode);
                    return functionOk(Pnode);
                    }
                }
            else
                return functionFail(Pnode);
            }
#if !defined NO_FILE_RENAME
        CASE(REN)
            {
            if(is_op(rightnode)
               && !is_op(rlnode = rightnode->LEFT)
               && !is_op(rrightnode = rightnode->RIGHT)
               )
                {
                intVal.i = rename((const char*)POBJ(rlnode), (const char*)POBJ(rrightnode));
                if(intVal.i)
                    {
#ifndef EACCES
                    sprintf(draft, "%d", intVal.i);
#else
                    switch(errno)
                        {
                        case EACCES:
                            /*
                            File or directory specified by newname already exists or
                            could not be created (invalid path); or oldname is a directory
                            and newname specifies a different path.
                            */
                            strcpy(draft, "EACCES");
                            break;
                        case ENOENT:
                            /*
                            File or path specified by oldname not found.
                            */
                            strcpy(draft, "ENOENT");
                            break;
                        case EINVAL:
                            /*
                            Name contains invalid characters.
                            */
                            strcpy(draft, "EINVAL");
                            break;
                        default:
                            sprintf(draft, "%d", errno);
                            break;
                        }
#endif
                    }
                else
                    strcpy(draft, "0");
                wipe(Pnode);
                Pnode = scopy((const char*)draft);
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
#endif
#if !defined NO_FILE_REMOVE
        CASE(RMV)
            {
            if(!is_op(rightnode))
                {
#if defined __SYMBIAN32__
                intVal.i = unlink((const char*)POBJ(rightnode));
#else
                intVal.i = remove((const char*)POBJ(rightnode));
#endif
                if(intVal.i)
                    {
#ifndef EACCES
                    sprintf(draft, "%d", intVal.i);
#else
                    switch(errno)
                        {
                        case EACCES:
                            /*
                            File or directory specified by newname already exists or
                            could not be created (invalid path); or oldname is a directory
                            and newname specifies a different path.
                            */
                            strcpy(draft, "EACCES");
                            break;
                        case ENOENT:
                            /*
                            File or path specified by oldname not found.
                            */
                            strcpy(draft, "ENOENT");
                            break;
                        default:
                            {
#ifdef __VMS
                            if(!strcmp("file currently locked by another user", (const char*)strerror(errno)))
                                {  /* OpenVMS */
                                strcpy(draft, "EACCES");
                                break;
                                }
                            else
#endif
                                {
                                wipe(Pnode);
                                Pnode = scopy((const char*)strerror(errno));
                                return functionOk(Pnode);
                                }
                            /* sprintf(draft,"%d",errno);
                             break;*/
                            }
                        }
#endif
                    }
                else
                    strcpy(draft, "0");
                wipe(Pnode);
                Pnode = scopy((const char*)draft);
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
#endif
        CASE(ARG) /* arg$ or arg$N  (N == 0,1,... and N < argc) */
            {
            static int argno = 0;
            if(is_op(rightnode))
                return functionFail(Pnode);
            if(PLOBJ(rightnode) != '\0')
                {
                LONG val;
                if(!INTEGER_NOT_NEG(rightnode))
                    return functionFail(Pnode);
                val = STRTOUL((char*)POBJ(rightnode), (char**)NULL, 10);
                if(val >= ARGC)
                    return functionFail(Pnode);
                wipe(Pnode);
                Pnode = scopy((const char*)ARGV[val]);
                return functionOk(Pnode);
                }
            if(argno < ARGC)
                {
                wipe(Pnode);
                Pnode = scopy((const char*)ARGV[argno++]);
                return functionOk(Pnode);
                }
            else
                {
                return functionFail(Pnode);
                }
            }
        CASE(GET) /* get$file */
            {
            Boolean GoOn;
            int err = 0;
            if(is_op(rightnode))
                {
                if(is_op(rlnode = rightnode->LEFT))
                    return functionFail(Pnode);
                rrightnode = rightnode->RIGHT;
                intVal.i = (search_opt(rrightnode, ECH) << SHIFT_ECH)
                    + (search_opt(rrightnode, MEM) << SHIFT_MEM)
                    + (search_opt(rrightnode, VAP) << SHIFT_VAP)
                    + (search_opt(rrightnode, STG) << SHIFT_STR)
                    + (search_opt(rrightnode, ML) << SHIFT_ML)
                    + (search_opt(rrightnode, TRM) << SHIFT_TRM)
                    + (search_opt(rrightnode, HT) << SHIFT_HT)
                    + (search_opt(rrightnode, X) << SHIFT_X)
                    + (search_opt(rrightnode, JSN) << SHIFT_JSN)
                    + (search_opt(rrightnode, TXT) << SHIFT_TXT)
                    + (search_opt(rrightnode, BIN) << SHIFT_BIN);
                }
            else
                {
                intVal.i = 0;
                rlnode = rightnode;
                }
            if(intVal.i & OPT_MEM)
                {
                addr[1] = same_as_w(rlnode);
                source = POBJ(addr[1]);
                for(;;)
                    {
                    Pnode = input(NULL, Pnode, intVal.i, &err, &GoOn);
                    if(!GoOn || err)
                        break;
                    Pnode = eval(Pnode);
                    }
                wipe(addr[1]);
                }
            else
                {
                if(rlnode->u.obj && strcmp((char*)POBJ(rlnode), "stdin"))
                    {
#if defined NO_FOPEN
                    return functionFail(Pnode);
#else
                    psk pnode = fileget(rlnode, intVal.i, Pnode, &err, &GoOn);
                    if(pnode)
                        Pnode = pnode;
                    else
                        return functionFail(Pnode);
#endif
                    }
                else
                    {
                    intVal.i |= OPT_ECH;
#ifdef DELAY_DUE_TO_INPUT
                    for(;;)
                        {
                        clock_t time0;
                        time0 = clock();
                        Pnode = input(stdin, Pnode, intVal.i, &err, &GoOn);
                        delayDueToInput += clock() - time0;
                        if(!GoOn || err)
                            break;
                        Pnode = eval(Pnode);
                        }
#else
                    for(;;)
                        {
                        Pnode = input(stdin, Pnode, intVal.i, &err, &GoOn);
                        if(!GoOn || err)
                            break;
                        Pnode = eval(Pnode);
                        }
#endif
                    }
                }
            return err ? functionFail(Pnode) : functionOk(Pnode);
            }
        CASE(PUT) /* put$(file,mode,node) of put$node */
            {
            return output(&Pnode, result) ? functionOk(Pnode) : functionFail(Pnode);
            }
#if !defined __SYMBIAN32__
#if !defined NO_SYSTEM_CALL
        CASE(SYS)
            {
            if(is_op(rightnode) || (PLOBJ(rightnode) == '\0'))
                return functionFail(Pnode);
            else
                {
                intVal.i = system((const char*)POBJ(rightnode));
                if(intVal.i)
                    {
#ifndef E2BIG
                    sprintf(draft, "%d", intVal.i);
#else
                    switch(errno)
                        {
                        case E2BIG:
                            /*
                            Argument list (which is system-dependent) is too big.
                            */
                            strcpy(draft, "E2BIG");
                            break;
                        case ENOENT:
                            /*
                            Command interpreter cannot be found.
                            */
                            strcpy(draft, "ENOENT");
                            break;
                        case ENOEXEC:
                            /*
                            Command-interpreter file has invalid format and is not executable.
                            */
                            strcpy(draft, "ENOEXEC");
                            break;
                        case ENOMEM:
                            /*
                            Not enough memory is available to execute command; or available
                            memory has been corrupted; or invalid block exists, indicating
                            that process making call was not allocated properly.
                            */
                            strcpy(draft, "ENOMEM");
                            break;
                        default:
                            sprintf(draft, "%d", errno);
                            break;
                        }
#endif
                    }
                else
                    strcpy(draft, "0");
                wipe(Pnode);
                Pnode = scopy((const char*)draft);
                return functionOk(Pnode);
                }
            }
#endif
#endif
        CASE(TBL) /* tbl$(varname,length) */
            {
            if(is_op(rightnode))
                return psh(rightnode->LEFT, &zeroNode, rightnode->RIGHT) ? functionOk(Pnode) : functionFail(Pnode);
            else
                return functionFail(Pnode);
            }
#if 0
        /*
        The same effect is obtained by <expr>:?!(=)
        */
        CASE(PRV) /* "?"$<expr> */
            {
            if((rightnode->v.fl & SUCCESS)
               && (is_op(rightnode) || rightnode->u.obj || HAS_UNOPS(rightnode)))
                insert(&nilNode, rightnode);
            Pnode = rightbranch(Pnode);
            return functionOk(Pnode);
            }
#endif
        CASE(CLK) /* clk' */
            {
            print_clock(draft);
            wipe(Pnode);
            Pnode = scopy((const char*)draft);
            return functionOk(Pnode);
            }

        CASE(SIM) /* sim$(<atom>,<atom>) , fuzzy compare (percentage) */
            {
            if(is_op(rightnode)
               && !is_op(rlnode = rightnode->LEFT)
               && !is_op(rrightnode = rightnode->RIGHT))
                {
                Sim(draft, (char*)POBJ(rlnode), (char*)POBJ(rrightnode));
                wipe(Pnode);
                Pnode = scopy((const char*)draft);
                return functionOk(Pnode);
                }
            else
                return functionFail(Pnode);
            }
#if DEBUGBRACMAT
        CASE(DBG) /* dbg$<expr> */
            {
            ++debug;
            if(Op(Pnode) != FUU)
                {
                errorprintf("Use dbg'(expression), not dbg$(expression)!\n");
                writeError(Pnode);
                }
            Pnode = rightbranch(Pnode);
            Pnode = eval(Pnode);
            --debug;
            return functionOk(Pnode);
            }
#endif
        CASE(WHL)
            {
            while(isSUCCESSorFENCE(rightnode = eval(same_as_w(Pnode->RIGHT))))
                {
                wipe(rightnode);
                }
            wipe(rightnode);
            return functionOk(Pnode);
            }
        CASE(New) /* new$<object>*/
            {
            if(Op(rightnode) == COMMA)
                {
                addr[2] = getObjectDef(rightnode->LEFT);
                if(!addr[2])
                    return functionFail(Pnode);
                addr[3] = rightnode->RIGHT;
                if(ISBUILTIN((objectnode*)addr[2]))
                    Pnode = build_up(Pnode, "(((\2.New)'\3)|)&\2", NULL);
                /* We might be able to call 'new' if 'New' had attached the argument
                    (containing the definition of a 'new' method) to the rhs of the '='.
                   This cannot be done in a general way without introducing new syntax rules for the new$ function.
                */
                else
                    Pnode = build_up(Pnode, "(((\2.new)'\3)|)&\2", NULL);
                }
            else
                {
                addr[2] = getObjectDef(rightnode);
                if(!addr[2])
                    return functionFail(Pnode);
                if(ISBUILTIN((objectnode*)addr[2]))
                    Pnode = build_up(Pnode, "(((\2.New)')|)&\2", NULL);
                /* There cannot be a user-defined 'new' method on a built-in object if there is no way to supply it*/
                /* 'die' CAN be user-supplied. The built-in function is 'Die' */
                else
                    Pnode = build_up(Pnode, "(((\2.new)')|)&\2", NULL);
                }
            SETCREATEDWITHNEW((objectnode*)addr[2]);
            wipe(addr[2]);
            return functionOk(Pnode);
            }
        CASE(0) /* $<expr>  '<expr> */
            {
            if(Op(Pnode) == FUU)
                {
                if(!HAS_UNOPS(Pnode->LEFT))
                    {
                    intVal.ul = Pnode->v.fl & UNOPS;
                    if(intVal.ul == FRACTION
                       && is_op(Pnode->RIGHT)
                       && Op(Pnode->RIGHT) == DOT
                       && !is_op(Pnode->RIGHT->LEFT)
                       )
                        { /* /('(a.a+2))*/
                        return functionOk(Pnode);
                        }
                    else
                        {
                        rightnode = evalmacro(Pnode->RIGHT);
                        rrightnode = (psk)bmalloc(__LINE__, sizeof(objectnode));
#if WORD32
                        ((typedObjectnode*)rrightnode)->u.Int = 0;
#else
                        ((typedObjectnode*)rrightnode)->v.fl &= ~(BUILT_IN | CREATEDWITHNEW);
#endif
                        rrightnode->v.fl = EQUALS | SUCCESS;
                        rrightnode->LEFT = same_as_w(&nilNode);
                        if(rightnode)
                            {
                            rrightnode->RIGHT = rightnode;
                            }
                        else
                            {
                            rrightnode->RIGHT = same_as_w(Pnode->RIGHT);
                            }
                        wipe(Pnode);
                        Pnode = rrightnode;
                        Pnode->v.fl |= intVal.ul; /* (a=b)&!('$a)*/
                        }
                    }
                else
                    {
                    combiflags(Pnode);
                    Pnode = rightbranch(Pnode);
                    }
                return functionOk(Pnode);
                }
            else
                {
                return functionFail(Pnode);
                }
            }
        DEFAULT
            {
#if 1
            return 0;
#else
#if 1
            if(INTEGER(lnode))
                {
                vars* nxtvar;
                if(is_op(rightnode))
                    return functionFail(Pnode);
                for(nxtvar = variables[rightnode->u.obj];
                    nxtvar && (STRCMP(VARNAME(nxtvar),POBJ(rightnode)) < 0);
                    nxtvar = nxtvar->next);
                /* find first name in a row of equal names */
                if(nxtvar && !STRCMP(VARNAME(nxtvar),POBJ(rightnode)))
                    {
                    nxtvar->selector =
                       (int)toLong(lnode)
                     % (nxtvar->n + 1);
                    if(nxtvar->selector < 0)
                        nxtvar->selector += (nxtvar->n + 1);
                    Pnode = rightbranch(Pnode);
                    return functionOk(Pnode);
                    }
                }
            if(!(rightnode->v.fl & SUCCESS))
                return functionFail(Pnode);
            addr[1] = NULL;
            return execFnc(Pnode);
#else
            if(INTEGER(lnode))
                {
                if(is_op(rightnode))
                    return functionFail(Pnode);
                else
                    {
                    if(setIndex(rightnode,lnode))
                        {
                        Pnode = rightbranch(Pnode);
                        return functionOk(Pnode);
                        }
                    }
                }
            if(!(rightnode->v.fl & SUCCESS))
                return functionFail(Pnode);
            addr[1] = NULL;
            return execFnc(Pnode);
#endif
#endif
            }
        }
    }
    /*return functionOk(Pnode); 20 Dec 1995, unreachable code in Borland C */
    }

static psk handleExponents(psk Pnode)
    {
    psk lnode;
    Boolean done = FALSE;
    for(; ((lnode = Pnode->LEFT)->v.fl & READY) && Op(lnode) == EXP;)
        {
        done = TRUE;
        Pnode->LEFT = lnode = isolated(lnode);
        lnode->v.fl &= ~READY & ~OPERATOR;/* turn off READY flag */
        lnode->v.fl |= TIMES;
        addr[1] = lnode->LEFT;
        addr[2] = lnode->RIGHT;
        addr[3] = Pnode->RIGHT;
        Pnode = build_up(Pnode, "(\1^(\2*\3))", NULL);
        }
    if(done)
        {
        return Pnode;
        }
    else
        {
        static const char* conc_arr[] = { NULL,NULL,NULL,NULL,NULL,NULL };

        Qnumber iexponent,
            hiexponent;

        psk rightnode;
        if(!is_op(rightnode = Pnode->RIGHT))
            {
            if(RAT_NUL(rightnode))
                {
                wipe(Pnode);
                return copyof(&oneNode);
                }
            if(IS_ONE(rightnode))
                {
                return leftbranch(Pnode);
                }
            }
        lnode = Pnode->LEFT;
        if(!is_op(lnode))
            {
            if((RAT_NUL(lnode) && !RAT_NEG_COMP(rightnode)) || IS_ONE(lnode))
                {
                return leftbranch(Pnode);
                }

            if(!is_op(rightnode) && RATIONAL_COMP(rightnode))
                {
                if(RATIONAL_COMP(lnode))
                    {
                    if(RAT_NEG_COMP(rightnode) && absone(rightnode))
                        {
                        conc_arr[1] = NULL;
                        conc_arr[2] = hash6;
                        conc_arr[3] = NULL;
                        addr[6] = qqDivide(&oneNode, lnode);
                        assert(lnode == Pnode->LEFT);
                        Pnode = vbuildup(Pnode, conc_arr + 2);
                        wipe(addr[6]);
                        return Pnode;
                        }
                    else if(RAT_NEG_COMP(lnode) && RAT_RAT_COMP(rightnode))
                        {
                        return Pnode; /*{?} -3^2/3 => -3^2/3 */
                        }
                    /* Missing here is n^m, with m > 2.
                        That case is handled in casemacht. */
                    }
                else if(PLOBJ(lnode) == IM)
                    {
                    if(qCompare(rightnode, &zeroNode) & MINUS)
                        { /* i^-n -> -i^n */ /*{?} i^-7 => i */
                        /* -i^-n -> i^n */ /*{?} -i^-7 => -i */
                        conc_arr[0] = "(\2^\3)";
                        addr[2] = qTimesMinusOne(lnode);
                        addr[3] = qTimesMinusOne(rightnode);
                        conc_arr[1] = NULL;
                        Pnode = vbuildup(Pnode, conc_arr);
                        wipe(addr[2]);
                        wipe(addr[3]);
                        return Pnode;
                        }
                    else if(qCompare(&twoNode, rightnode) & (QNUL | MINUS))
                        {
                        iexponent = qModulo(rightnode, &fourNode);
                        if(iexponent->v.fl & QNUL)
                            {
                            wipe(Pnode); /*{?} i^4 => 1 */
                            Pnode = copyof(&oneNode);
                            }
                        else
                            {
                            int Sign;
                            Sign = qCompare(iexponent, &twoNode);
                            if(Sign & QNUL)
                                {
                                wipe(Pnode);
                                Pnode = copyof(&minusOneNode);
                                }
                            else
                                {
                                if(!(Sign & MINUS))
                                    {
                                    hiexponent = iexponent;
                                    iexponent = qPlus(&fourNode, hiexponent, MINUS);
                                    wipe(hiexponent);
                                    }
                                addr[2] = lnode;
                                addr[6] = iexponent;
                                conc_arr[0] = "(-1*\2)^";
                                conc_arr[1] = "(\6)";
                                conc_arr[2] = NULL;
                                Pnode = vbuildup(Pnode, conc_arr);
                                }
                            }
                        wipe(iexponent);
                        return Pnode;
                        }
                    }
                }
            }

        if(Op(lnode) == TIMES)
            {
            addr[1] = lnode->LEFT;
            addr[2] = lnode->RIGHT;
            addr[3] = Pnode->RIGHT;
            return build_up(Pnode, "\1^\3*\2^\3", NULL);
            }

        if(RATIONAL_COMP(lnode))
            {
            static const char parenonepow[] = "(\1^";
            if(INTEGER_NOT_NUL_COMP(rightnode) && !absone(rightnode))
                {
                addr[1] = lnode;
                if(INTEGER_POS_COMP(rightnode))
                    {
                    if(qCompare(&twoNode, rightnode) & MINUS)
                        {
                        /* m^n = (m^(n\2))^2*m^(n mod 2) */ /*{?} 9^7 => 4782969 */
                        conc_arr[0] = parenonepow;
                        conc_arr[1] = hash5;
                        conc_arr[3] = hash6;
                        conc_arr[4] = NULL;
                        addr[5] = qIntegerDivision(rightnode, &twoNode);
                        conc_arr[2] = ")^2*\1^";
                        addr[6] = qModulo(rightnode, &twoNode);
                        Pnode = vbuildup(Pnode, conc_arr);
                        wipe(addr[5]);
                        wipe(addr[6]);
                        }
                    else
                        {
                        /* m^2 = m*m */
                        Pnode = build_up(Pnode, "(\1*\1)", NULL);
                        }
                    }
                else
                    {
                    /*{?} 7^-13 => 1/96889010407 */
                    conc_arr[0] = parenonepow;
                    conc_arr[1] = hash6;
                    addr[6] = qTimesMinusOne(rightnode);
                    conc_arr[2] = ")^-1";
                    conc_arr[3] = 0;
                    Pnode = vbuildup(Pnode, conc_arr);
                    wipe(addr[6]);
                    }
                return Pnode;
                }
            else if(RAT_RAT(rightnode))
                {
                char** conc, slash = 0;
                int wipe[20], ind;
                nnumber numerator = { 0 }, denominator = { 0 };
                for(ind = 0; ind < 20; wipe[ind++] = TRUE);
                ind = 0;
                conc = (char**)bmalloc(__LINE__, 20 * sizeof(char**));
                /* 20 is safe value for ULONGs */
                addr[1] = Pnode->RIGHT;
                if(RAT_RAT_COMP(Pnode->LEFT))
                    {
                    split(Pnode->LEFT, &numerator, &denominator);
                    if(!subroot(&numerator, conc, &ind))
                        {
                        wipe[ind] = FALSE;
                        conc[ind++] = numerator.number;
                        slash = numerator.number[numerator.length];
                        numerator.number[numerator.length] = 0;

                        wipe[ind] = FALSE;
                        conc[ind++] = "^\1";
                        }
                    wipe[ind] = FALSE;
                    conc[ind++] = "*(";
                    if(!subroot(&denominator, conc, &ind))
                        {
                        wipe[ind] = FALSE;
                        conc[ind++] = denominator.number;
                        wipe[ind] = FALSE;
                        conc[ind++] = "^\1";
                        }
                    wipe[ind] = FALSE;
                    conc[ind++] = ")^-1";
                    }
                else
                    {
                    numerator.number = (char*)POBJ(Pnode->LEFT);
                    numerator.alloc = NULL;
                    numerator.length = strlen(numerator.number);
                    if(!subroot(&numerator, conc, &ind))
                        {
                        bfree(conc);
                        return Pnode;
                        }
                    }
                conc[ind--] = NULL;
                Pnode = vbuildup(Pnode, (const char**)conc);
                if(slash)
                    numerator.number[numerator.length] = slash;
                for(; ind >= 0; ind--)
                    if(wipe[ind])
                        bfree(conc[ind]);
                bfree(conc);
                return Pnode;
                }
            }
        }
    if(is_op(Pnode->RIGHT))
        {
        int ok;
        Pnode = tryq(Pnode, f4, &ok);
        }
    return Pnode;
    }


/*
Improvement that DOES evaluate b+(i*c+i*d)+-i*c
It also allows much deeper structures, because the right place for insertion
is found iteratively, not recursively. This also causes some operations to
be tremendously faster. e.g. (1+a+b+c)^30+1&ready evaluates in about
4,5 seconds now, previously in 330 seconds! (AST Bravo MS 5233M 233 MHz MMX Pentium)
*/
static void splitProduct_number_im_rest(psk pnode, ppsk Nm, ppsk I, ppsk NNNI)
    {
    psk temp;
    if(Op(pnode) == TIMES)
        {
        if(RATIONAL_COMP(pnode->LEFT))
            {/* 17*x */
            *Nm = pnode->LEFT;
            temp = pnode->RIGHT;
            }/* Nm*temp */
        else
            {
            *Nm = NULL;
            temp = pnode;
            }/* temp */
        if(Op(temp) == TIMES)
            {
            if(!is_op(temp->LEFT) && PLOBJ(temp->LEFT) == IM)
                {/* Nm*i*x */
                *I = temp->LEFT;
                *NNNI = temp->RIGHT;
                }/* Nm*I*NNNI */
            else
                {
                *I = NULL;
                *NNNI = temp;
                }/* Nm*NNNI */
            }
        else
            {
            if(!is_op(temp) && PLOBJ(temp) == IM)
                {/* Nm*i */
                *I = temp;
                *NNNI = NULL;
                }/* Nm*I */
            else
                {
                *I = NULL;
                *NNNI = temp;
                }/* Nm*NNNI */
            }
        }
    else if(!is_op(pnode) && PLOBJ(pnode) == IM)
        {/* i */
        *Nm = NULL;
        *I = pnode;
        *NNNI = NULL;
        }/* I */
    else
        {/* x */
        *Nm = NULL;
        *I = NULL;
        *NNNI = pnode;
        }/* NNNI */
    }

static psk rightoperand_and_tail(psk Pnode, ppsk head, ppsk tail)
    {
    psk temp;
    assert(is_op(Pnode));
    temp = Pnode->RIGHT;
    if(Op(Pnode) == Op(temp))
        {
        *head = temp->LEFT;
        *tail = temp->RIGHT;
        }
    else
        {
        *head = temp;
        *tail = NULL;
        }
    return temp;
    }

static psk leftoperand_and_tail(psk Pnode, ppsk head, ppsk tail)
    {
    psk temp;
    assert(is_op(Pnode));
    temp = Pnode->LEFT;
    if(Op(Pnode) == Op(temp))
        {
        *head = temp->LEFT;
        *tail = temp->RIGHT;
        }
    else
        {
        *head = temp;
        *tail = NULL;
        }
    return temp;
    }

#define EXPAND 0
#if EXPAND
static psk expandDummy(psk Pnode, int* ok)
    {
    *ok = FALSE;
    return Pnode;
    }
#endif

static psk expandProduct(psk Pnode, int* ok)
    {
    switch(Op(Pnode))
        {
        case TIMES:
        case EXP:
            {
            if(((match(0, Pnode, m0, NULL, 0, Pnode, 3333) & TRUE)
                && ((Pnode = tryq(Pnode, f0, ok)), *ok)
                )
               || ((match(0, Pnode, m1, NULL, 0, Pnode, 4444) & TRUE)
                   && ((Pnode = tryq(Pnode, f1, ok)), *ok)
                   )
               )
                {
                if(is_op(Pnode)) /*{?} (1+i)*(1+-i)+Z => 2+Z */
                    Pnode->v.fl &= ~READY;
                return Pnode;
                }
            break;
            }
        }
    *ok = FALSE;
    return Pnode;
    }

static psk mergeOrSortTerms(psk Pnode)
    {
    /*
    Split Pnode in left L and right R argument

    If L is zero,
        return R

    If R is zero,
        return L

    If L is a product containing a sum,
        expand it

    If R is a product containing a sum,
        expand it

    Find the proper place split of R into Rhead , RtermS,  RtermGE and Rtail for L to insert into:
        L + R -> Rhead + RtermS + L + RtermGE + Rtail:
    Start with Rhead = NIL, RtermS = NIL  RtermGE is first term of R and Rtail is remainder of R
    Split L into Lterm and Ltail
    If Lterm is a number
        if RtermGE is a number
            return sum(Lterm,RtermGE) + Ltail + Rtail
        else
            return Lterm + Ltail + R
    Else if Rterm is a number
        return Rterm + L + Rtail
    Else
        get the non-numerical factor LtermNN of Lterm
        if LtermNN is imaginary
            get the nonimaginary factors of LtermNN (these may also include 'e' and 'pi') LtermNNNI
            find Rhead,  RtermS, RtermGE and Rtail
                such that Rhead does contain all non-imaginary terms
                and such that RtermGE and Rtail
                    either are NIL
                    or RtermGE is imaginary
                        and (RtermS is NIL or RtermSNNNI <  LtermNNNI) and LtermNNNI <= RtermGENNNI
            if RtermGE is NIL
                return R + L
            else
                if LtermNNNI < RtermGENNNI
                    return Rhead + RtermS + L + RtermGE + Rtail
                else
                    return Rhead + RtermS + sum(L,RtermGE) + Rtail
        else
            find Rhead,  RtermS, RtermGE and Rtail
                such that RtermGE and Rtail
                    either are NIL
                    or (RtermS is NIL or RtermSNN <  LtermNN) and LtermNN <= RtermGENN
            if RtermGE is NIL
                return R + L
            else
                if LtermNN < RtermGENN
                    return Rhead + RtermS + L + RtermGE + Rtail
                else
                    return Rhead + RtermS + sum(L,RtermGE) + Rtail


    */
    static const char* conc[] = { NULL,NULL,NULL,NULL };
    int res = FALSE;
    psk top = Pnode;

    psk L = top->LEFT;
    psk Lterm, Ltail;
    psk LtermN, LtermI, LtermNNNI;

    psk R;
    psk Rterm, Rtail;
    psk RtermN, RtermI, RtermNNNI;

    int ok;
    if(!is_op(L) && RAT_NUL_COMP(L))
        {
        /* 0+x -> x */
        return rightbranch(top);
        }

    R = top->RIGHT;
    if(!is_op(R) && RAT_NUL_COMP(R))
        {
        /*{?} x+0 => x */
        return leftbranch(top);
        }

    if(is_op(L)
       && ((top->LEFT = expandProduct(top->LEFT, &ok)), ok)
       )
        {
        res = TRUE;
        }
    if(is_op(R)
       && ((top->RIGHT = expandProduct(top->RIGHT, &ok)), ok)
       )
        { /*
          {?} a*b+u*(x+y) => a*b+u*x+u*y
          */
        res = TRUE;
        }
    if(res)
        {
        Pnode->v.fl &= ~READY;
        return Pnode;
        }
    rightoperand_and_tail(top, &Rterm, &Rtail);
    leftoperand_and_tail(top, &Lterm, &Ltail);
    assert(Ltail == NULL);
    if(RATIONAL_COMP(Lterm))
        {
        if(RATIONAL_COMP(Rterm))
            {
            conc[0] = hash6;
            if(Lterm == Rterm)
                {
                /* 7+7 -> 2*7 */ /*{?} 7+7 => 14 */
                addr[6] = qTimes(&twoNode, Rterm);
                }
            else
                {
                /* 4+7 -> 11 */ /*{?} 4+7 => 11 */
                addr[6] = qPlus(Lterm, Rterm, 0);
                }
            conc[1] = NULL;
            conc[2] = NULL;
            if(Rtail != NULL)
                {
                addr[4] = Rtail;
                conc[1] = "+\4";
                }
            Pnode = vbuildup(top, conc);
            wipe(addr[6]);
            }
        return Pnode;
        }
    else if(RATIONAL_COMP(Rterm))
        {
        addr[1] = Rterm;
        addr[2] = L;
        if(Rtail)
            {
            /* How to get here?
                   (1+a)*(1+b)+c+(1+d)*(1+f)
            The lhs (1+a)*(1+b) is not expanded before the merge starts
            Comparing (1+a)*(1+b) with 1, c, d, f and d*f, this product lands
            after d*f. Thereafter (1+a)*(1+b) is expanded, giving a
            numeral 1 in the middle of the expression:
                   1+c+d+f+d*f+(1+a)*(1+b)
                   1+c+d+f+d*f+1+a+b+a*b
            Rterm is 1+a+b+a*b
            Rtail is a+b+a*b
            */
            addr[3] = Rtail;
            return build_up(top, "\1+\2+\3", NULL);
            }
        else
            {
            /* 4*(x+7)+p+-4*x */
            return build_up(top, "\1+\2", NULL);
            }
        }

    if(Op(Lterm) == LOG
       && Op(Rterm) == LOG
       && !equal(Lterm->LEFT, Rterm->LEFT)
       )
        {
        addr[1] = Lterm->LEFT;
        addr[2] = Lterm->RIGHT;
        addr[3] = Rterm->RIGHT;
        if(Rtail == NULL)
            return build_up(top, "\1\016(\2*\3)", NULL); /*{?} 2\L3+2\L9 => 4+2\L27/16 */
        else
            {
            addr[4] = Rtail;
            return build_up(top, "\1\016(\2*\3)+\4", NULL); /*{?} 2\L3+2\L9+3\L123 => 8+2\L27/16+3\L41/27 */
            }
        }

    splitProduct_number_im_rest(Lterm, &LtermN, &LtermI, &LtermNNNI);

    if(LtermI)
        {
        ppsk runner = &Pnode;
        splitProduct_number_im_rest(Rterm, &RtermN, &RtermI, &RtermNNNI);
        while(RtermI == NULL
              && Op((*runner)->RIGHT) == PLUS
              )
            {
            runner = &(*runner)->RIGHT;
            *runner = isolated(*runner);
            rightoperand_and_tail((*runner), &Rterm, &Rtail);
            splitProduct_number_im_rest(Rterm, &RtermN, &RtermI, &RtermNNNI);/*{?} i*x+-i*x+a => a */
            }
        if(RtermI != NULL)
            {                        /*{?} i*x+-i*x => 0 */
            int indx;
            int dif;
            if(LtermNNNI == NULL)
                {
                dif = RtermNNNI == NULL ? 0 : -1;/*{?} i+-i*x => i+-i*x */
                /*{?} i+-i => 0 */
                }
            else
                {
                assert(RtermNNNI != NULL);
                dif = equal(LtermNNNI, RtermNNNI);
                }
            if(dif == 0)
                {                        /*{?} i*x+-i*x => 0 */
                if(RtermN)
                    {
                    addr[2] = RtermN; /*{?} i*x+3*i*x => 4*i*x */
                    if(LtermN == NULL)
                        {
                        /* a+n*a */ /*{?} i*x+3*i*x => 4*i*x */
                        if(HAS_MINUS_SIGN(LtermI))
                            {  /*{?} -i*x+3*i*x => 2*i*x */
                            if(HAS_MINUS_SIGN(RtermI))
                                {
                                conc[0] = "(1+\2)*-i";/*{?} -i*x+3*-i*x => 4*-i*x */
                                }
                            else
                                {
                                conc[0] = "(-1+\2)*i";/*{?} -i*x+3*i*x => 2*i*x */
                                }
                            }
                        else if(HAS_MINUS_SIGN(RtermI))
                            {
                            conc[0] = "(-1+\2)*-i"; /*{?} i*x+3*-i*x => 2*-i*x */
                            }
                        else
                            {
                            conc[0] = "(1+\2)*i";/*{?} i*x+3*i*x => 4*i*x */
                            }
                        }
                    /* (1+n)*a */
                    else
                        {
                        /* n*a+m*a */ /*{?} 3*-i*x+-3*i*x => 6*-i*x */
                        addr[3] = LtermN;
                        if(HAS_MINUS_SIGN(LtermI))
                            {
                            if(HAS_MINUS_SIGN(RtermI))
                                {
                                conc[0] = "(\3+\2)*-i";/*{?} 3*-i*x+-3*i*x => 6*-i*x */
                                }
                            else
                                {
                                conc[0] = "(-1*\3+\2)*i";/*{?} 3*-i*x+-3*-i*x => 0 */
                                }
                            }
                        else if(HAS_MINUS_SIGN(RtermI))
                            {
                            conc[0] = "(\3+-1*\2)*i"; /*{?} 3*i*x+-3*i*x => 0 */
                            }
                        else
                            {
                            conc[0] = "(\3+\2)*i"; /*{?} 3*i*x+4*i*x => 7*i*x */
                            }
                        /* (n+m)*a */
                        }
                    }
                else
                    {                        /*{?} i*x+-i*x => 0 */
                    addr[1] = LtermNNNI;
                    if(LtermN != NULL)
                        {
                        /* m*a+a */
                        addr[2] = LtermN; /*{?} 3*i*x+i*x => 4*i*x */
                        if(HAS_MINUS_SIGN(LtermI))

                            if(HAS_MINUS_SIGN(RtermI))
                                {
                                conc[0] = "(1+\2)*-i";/*{?} 3*-i*x+-i*x => 4*-i*x */
                                }
                            else
                                {
                                conc[0] = "(-1+\2)*-i";/*{?} 3*-i*x+i*x => 2*-i*x */
                                }

                        else if(HAS_MINUS_SIGN(RtermI))
                            {
                            conc[0] = "(-1+\2)*i"; /*{?} 3*i*x+-i*x => 2*i*x */
                            }
                        else
                            {
                            conc[0] = "(1+\2)*i"; /*{?} 3*i*x+i*x => 4*i*x */
                            }
                        /* (1+m)*a */
                        }
                    else
                        {
                        /* a+a */                        /*{?} i*x+-i*x => 0 */
                        if(HAS_MINUS_SIGN(LtermI))

                            if(HAS_MINUS_SIGN(RtermI))
                                {
                                conc[0] = "2*-i"; /*{?} -i+-i => 2*-i */
                                }
                            else
                                {
                                conc[0] = "0"; /*{?} -i+i => 0 */
                                }

                        else if(HAS_MINUS_SIGN(RtermI))
                            {
                            conc[0] = "0";                        /*{?} i*x+-i*x => 0 */
                            }
                        else
                            {
                            conc[0] = "2*i"; /*{?} i+i => 2*i */
                            }
                        }
                    /* 2*a */
                    }
                if(LtermNNNI != NULL)
                    {                        /*{?} i*x+-i*x => 0 */
                    addr[1] = RtermNNNI;
                    conc[1] = "*\1";
                    indx = 2;
                    }
                else
                    indx = 1; /*{?} i+-i => 0 */
                if(Rtail != NULL)
                    {
                    addr[4] = Rtail; /*{?} -i+-i+i*y => 2*-i+i*y */
                    conc[indx++] = "+\4";
                    }
                conc[indx] = NULL;                        /*{?} i*x+-i*x => 0 */
                (*runner)->RIGHT = vbuildup((*runner)->RIGHT, conc);
                if(runner != &Pnode)
                    {
                    (*runner)->v.fl &= ~READY;/*{?} i*x+-i*x+a => a */
                    *runner = eval(*runner);
                    }
                return rightbranch(top);                        /*{?} i*x+-i*x => 0 */
                }
            assert(Ltail == NULL);
            }
        else  /* LtermI != NULL && RtermI == NULL */
            {
            addr[1] = Rterm; /*{?} i*x+-i*x+a => a */
            addr[2] = L;
            (*runner)->RIGHT = build_up((*runner)->RIGHT, "\1+\2", NULL);
            (*runner)->RIGHT->v.fl |= READY;
            return rightbranch(top);
            }
        }
    else /* LtermI == NULL */
        {
        ppsk runner = &Pnode;
        int dif = 1;
        assert(LtermNNNI != NULL);
        splitProduct_number_im_rest(Rterm, &RtermN, &RtermI, &RtermNNNI);
        while(RtermNNNI != NULL
              && RtermI == NULL
              && (dif = cmp(LtermNNNI, RtermNNNI)) > 0
              && Op((*runner)->RIGHT) == PLUS
              )
            {
            /*
            x^(y*(a+b))+z^(y*(a+b))+-1*x^(a*y+b*y) => z^(y*(a+b))
            cos$(a+b)+-1*(cos$a*cos$b+-1*sin$a*sin$b) => 0
            x^(y*(a+b))+x^(a*y+b*y) => 2*x^(y*(a+b))
            x^(y*(a+b))+-1*x^(a*y+b*y) => 0
            fct$(a^((b+c)*(d+e))+a^((b+c)*(d+f))) => a^(b*d+c*d) * (a^(b*f+c*f)+a^(e*b+e*c))
            (a^((b+c)*(d+e))+a^((b+c)*(d+f))) + -1 * a^(b*d+c*d) * (a^(b*f+c*f)+a^(e*b+e*c)) => 0
            -1*(a^((b+c)*(d+e))+a^((b+c)*(d+f)))  +   a^(b*d+c*d) * (a^(b*f+c*f)+a^(e*b+e*c)) => 0
            a^(b*d+c*d) * (a^(b*f+c*f)+a^(e*b+e*c)) + -1*(a^((b+c)*(d+e))+a^((b+c)*(d+f))) => 0
            -1 * a^(b*d+c*d) * (a^(b*f+c*f)+a^(e*b+e*c)) + a^((b+c)*(d+e)) + a^((b+c)*(d+f)) => 0
            */
            runner = &(*runner)->RIGHT; /*{?} (b^3+c^3)+b^3+c+b^3 => c+3*b^3+c^3 */
            *runner = isolated(*runner);
            rightoperand_and_tail((*runner), &Rterm, &Rtail);
            splitProduct_number_im_rest(Rterm, &RtermN, &RtermI, &RtermNNNI);
            }
        if(RtermI != NULL)
            dif = -1; /*{?} (-i+a)+-i => a+2*-i */
        if(dif == 0)
            {
            if(RtermN)
                {
                addr[1] = RtermNNNI;
                addr[2] = RtermN;
                if(LtermN == NULL)
                    /*{?} a+n*a => a+a*n */
                    conc[0] = "(1+\2)*\1";
                /* (1+n)*a */
                else
                    {
                    /*{?} n*a+m*a => a*m+a*n */ /*{?} 7*a+9*a => 16*a */
                    addr[3] = LtermN;
                    conc[0] = "(\3+\2)*\1";
                    /* (n+m)*a */
                    }
                }
            else
                {
                addr[1] = LtermNNNI;
                if(LtermN != NULL)
                    {
                    /* m*a+a */
                    addr[2] = LtermN;
                    conc[0] = "(1+\2)*\1"; /*{?} 3*a+a => 4*a */
                    /* (1+m)*a */
                    }
                else
                    {
                    /*{?} a+a => 2*a */
                    conc[0] = "2*\1";
                    }
                /* 2*a */
                }
            assert(Ltail == NULL);
            conc[1] = NULL;
            conc[2] = NULL;
            if(Rtail != NULL)
                {
                addr[4] = Rtail;
                conc[1] = "+\4";
                }
            (*runner)->RIGHT = vbuildup((*runner)->RIGHT, conc);
            if(runner != &Pnode)
                {
                (*runner)->v.fl &= ~READY;
                /*{?} (b^3+c^3)+b^3+c+b^3 => c+3*b^3+c^3 */
                /* This would evaluate to c+(1+2)*b^3+c^3 if READY flag wasn't turned off */
                /* Correct evaluation: c+3*b^3+c^3 */
                *runner = eval(*runner);
                }
            return rightbranch(top);
            }
        else if(dif > 0)  /*{?} b+a => a+b */
            {
            addr[1] = Rterm;
            addr[2] = L;
            (*runner)->RIGHT = build_up((*runner)->RIGHT, "\1+\2", NULL);
            (*runner)->RIGHT->v.fl |= READY;
            return rightbranch(top);
            }
        else if((*runner) != top) /* b + a + c */
            {
            addr[1] = L;
            addr[2] = (*runner)->RIGHT;
            (*runner)->RIGHT = build_up((*runner)->RIGHT, "\1+\2", NULL);
            (*runner)->RIGHT = eval((*runner)->RIGHT);
            return rightbranch(top);          /* (1+a+b+c)^30+1 */
            }
        assert(Ltail == NULL);
        }
    return Pnode;
    }

static psk substtimes(psk Pnode)
    {
    static const char* conc[] = { NULL,NULL,NULL,NULL };
    psk rkn, lkn;
    psk rvar, lvar;
    psk temp, llnode, rlnode;
    int nodedifference;
    rkn = rightoperand(Pnode);

    if(is_op(rkn))
        rvar = NULL; /* (f.e)*(y.s) */
    else
        {
        if(IS_ONE(rkn))
            {
            return leftbranch(Pnode); /*{?} (a=7)&!(a*1) => 7 */
            }
        else if(RAT_NUL(rkn))/*{?} -1*140/1000 => -7/50 */
            {
            wipe(Pnode); /*{?} x*0 => 0 */
            return copyof(&zeroNode);
            }
        rvar = rkn; /*{?} a*a => a^2 */
        }

    lkn = Pnode->LEFT;
    if(!is_op(lkn))
        {
        if(RAT_NUL(lkn)) /*{?} -1*140/1000 => -7/50 */
            {
            wipe(Pnode); /*{?} 0*x => 0 */
            return copyof(&zeroNode);
            }
        lvar = lkn;

        if(IS_ONE(lkn))
            {
            return rightbranch(Pnode); /*{?} 1*-1 => -1 */
            }
        else if(RATIONAL_COMP(lkn) && rvar)
            {
            if(RATIONAL_COMP(rkn))
                {
                if(rkn == lkn)
                    lvar = (Pnode->LEFT = isolated(lkn)); /*{?} 1/10*1/10 => 1/100 */
                conc[0] = hash6;
                addr[6] = qTimes(rvar, lvar);
                if(rkn == Pnode->RIGHT)
                    conc[1] = NULL; /*{?} -1*140/1000 => -7/50 */
                else
                    {
                    addr[1] = Pnode->RIGHT->RIGHT; /*{?} -1*1/4*e^(2*i*x) => -1/4*e^(2*i*x) */
                    conc[1] = "*\1";
                    }
                Pnode = vbuildup(Pnode, conc);
                wipe(addr[6]);
                return Pnode;
                }
            else
                {
                if(PLOBJ(rkn) == IM && RAT_NEG_COMP(lkn))
                    {
                    conc[0] = "(\2*\3)";
                    addr[2] = qTimesMinusOne(lkn);
                    addr[3] = qTimesMinusOne(rkn);
                    if(rkn == Pnode->RIGHT)
                        conc[1] = NULL; /*{?} -1*i => -i */
                    else
                        {
                        addr[1] = Pnode->RIGHT->RIGHT; /*{?} -3*i*x => 3*-i*x */
                        conc[1] = "*\1";
                        }
                    Pnode = vbuildup(Pnode, conc);
                    wipe(addr[2]);
                    wipe(addr[3]);
                    return Pnode;
                    }
                }
            }
        }


    rlnode = Op(rkn) == EXP ? rkn->LEFT : rkn; /*{?} (f.e)*(y.s) => (f.e)*(y.s) */
    llnode = Op(lkn) == EXP ? lkn->LEFT : lkn;
    if((nodedifference = equal(llnode, rlnode)) == 0)
        {
        /* a^n*a^m */
        if(rlnode != rkn)
            {
            addr[1] = rlnode; /*{?} e^(i*x)*e^(-i*x) => 1 */
            addr[2] = rkn->RIGHT;
            if(llnode == lkn)
                {
                conc[0] = "\1^(1+\2)"; /*{?} a*a^n => a^(1+n) */
                }/* a^(1+n) */
            else
                {
                /* a^n*a^m */
                addr[3] = lkn->RIGHT; /*{?} e^(i*x)*e^(-i*x) => 1 */
                conc[0] = "\1^(\3+\2)";
                /* a^(n+m) */
                }
            }
        else
            {
            if(llnode != lkn)
                {
                addr[1] = llnode;     /*{?} a^m*a => a^(1+m) */
                addr[2] = lkn->RIGHT;
                conc[0] = "\1^(1+\2)";
                /* a^(m+1) */
                }
            else
                {
                /*{?} a*a => a^2 */
                addr[1] = llnode; /*{?} i*i*(-1/2*e^(i*a)+1/2*e^(-i*a))*(-1/2*e^(i*b)+1/2*e^(-i*b)) => -1*(-1/2*e^(i*a)+1/2*e^(-i*a))*(-1/2*e^(i*b)+1/2*e^(-i*b)) */
                conc[0] = "\1^2";
                /* a^2 */
                }
            }
        if(rkn != (temp = Pnode->RIGHT))
            {
            addr[4] = temp->RIGHT; /*{?} i*i*(-1/2*e^(i*a)+1/2*e^(-i*a))*(-1/2*e^(i*b)+1/2*e^(-i*b)) => -1*(-1/2*e^(i*a)+1/2*e^(-i*a))*(-1/2*e^(i*b)+1/2*e^(-i*b)) */
            conc[1] = "*\4";
            }
        else
            conc[1] = NULL; /*{?} e^(i*x)*e^(-i*x) => 1 */
        return vbuildup(Pnode, (const char**)conc);
        }
    else
        {
        int degree;
        degree = number_degree(rlnode) - number_degree(llnode); /*{?} (f.e)*(y.s) => (f.e)*(y.s) */
        if(degree > 0
           || (degree == 0 && (nodedifference > 0)))
            {
            /* b^n*a^m */
            /* l^n*a^m */
            if((temp = Pnode->RIGHT) == rkn)
                {
                Pnode->RIGHT = lkn; /*{?} x*2 => 2*x */
                Pnode->LEFT = rkn;
                Pnode->v.fl &= ~READY;
                }
            else
                {
                addr[1] = lkn; /*{?} i*2*x => 2*i*x */
                addr[2] = temp->LEFT;
                addr[3] = temp->RIGHT;
                Pnode = build_up(Pnode, "\2*\1*\3", NULL);
                }
            return Pnode;
            /* a^m*b^n */
            /* a^m*l^n */
            }
        else if(PLOBJ(rlnode) == IM)
            {
            if(PLOBJ(llnode) == IM) /*{?} -1*i^1/3 => -i^5/3 */
                {
                /*{?} i^n*-i^m => i^(-1*m+n) */
                if(rlnode != rkn)
                    {
                    addr[1] = llnode;
                    addr[2] = rkn->RIGHT;
                    if(llnode == lkn)
                        /*{?} i*-i^n => i^(1+-1*n) */
                        conc[0] = "\1^(1+-1*\2)";
                    /* i^(1-n) */
                    else
                        {
                        addr[3] = lkn->RIGHT; /*{?} i^n*-i^m => i^(-1*m+n) */
                        conc[0] = "\1^(\3+-1*\2)";
                        /* i^(n-m) */
                        }
                    }
                else
                    {
                    if(llnode != lkn)
                        {
                        /*{?} i^m*-i => i^(-1+m) */
                        addr[1] = llnode;
                        addr[2] = lkn->RIGHT;
                        conc[0] = "\1^(-1+\2)";
                        /* i^(m-1) */
                        }
                    else
                        {
                        /*{?} i*-i => 1 */
                        conc[0] = "1";
                        /* 1 */
                        }
                    }
                }
            else if(RAT_NEG_COMP(llnode)
                    /* -n*i^m -> n*-i^(2+m) */
                    /*{?} -7*i^9 => 7*i */ /*{!} 7*-i^11 */
                    /* -n*-i^m -> n*i^(2+m) */
                    /*{?} -7*-i^9 => 7*-i */ /*{!}-> 7*i^11 */
                    && rlnode != rkn
                    && llnode == lkn
                    )
                { /*{?} -1*i^1/3 => -i^5/3 */
                addr[1] = llnode;
                addr[2] = rkn->LEFT;
                addr[3] = rkn->RIGHT;
                addr[4] = &twoNode;
                conc[0] = "(-1*\1)*\2^(\3+\4)";
                }
            else
                return Pnode; /*{?} 2*i*x => 2*i*x */
            if(rkn != (temp = Pnode->RIGHT))
                {
                addr[4] = temp->RIGHT; /*{?} i^n*-i^m*z => i^(-1*m+n)*z */
                conc[1] = "*\4";
                }
            else
                conc[1] = NULL; /*{?} -1*i^1/3 => -i^5/3 */ /*{?} i^n*-i^m => i^(-1*m+n) */
            return vbuildup(Pnode, (const char**)conc);
            }
        else
            return Pnode; /*{?} (f.e)*(y.s) => (f.e)*(y.s) */
        }
    }

static int bringright(psk pnode)
    {
    /* (a*b*c*d)*a*b*c*d -> a*b*c*d*a*b*c*d */
    psk lnode;
    int done;
    done = FALSE;
    for(; Op(lnode = pnode->LEFT) == Op(pnode);)
        {
        lnode = isolated(lnode);
        lnode->v.fl &= ~READY;
        pnode->LEFT = lnode->LEFT;
        lnode->LEFT = lnode->RIGHT;
        lnode->RIGHT = pnode->RIGHT;
        pnode->v.fl &= ~READY;
        pnode->RIGHT = lnode;
        pnode = lnode;
        done = TRUE;
        }
    return done;
    }
/*
       1*
       / \
      /   \
     /     \
   2*      3*
   / \     / \
  a   x    b  c

lhead = a
ltail = x
rhead = b
rtail = c

rennur = NIL
runner = (a * x) * b * c
lloper = a * x

  2*
  / \
 a
 rennur = a*NIL


             1*
             / \
            x  3*
               / \
              b   c
*/
static int cmpplus(psk kn1, psk kn2)
    {
    if(RATIONAL_COMP(kn2))
        {
        if(RATIONAL_COMP(kn1))
            return 0;
        return 1; /* switch places */
        }
    else if(RATIONAL_COMP(kn1))
        return -1;
    else
        {
        psk N1;
        psk I1;
        psk NNNI1;
        psk N2;
        psk I2;
        psk NNNI2;
        splitProduct_number_im_rest(kn1, &N1, &I1, &NNNI1);
        splitProduct_number_im_rest(kn2, &N2, &I2, &NNNI2);
        if(I1 != NULL && I2 == NULL)
            return 1; /* switch places: imaginary terms after real terms */
        if(NNNI2 == NULL)
            {
            if(NNNI1 != NULL)
                return 1;
            return 0;
            }
        else if(NNNI1 == NULL)
            return -1;
        else
            {
            int diff;
            diff = equal(NNNI1, NNNI2);
            if(diff > 0)
                {
                return 1; /* switch places */
                }
            else if(diff < 0)
                {
                return -1;
                }
            else
                {
                return 0;
                }
            }
        }
    }

static int cmptimes(psk kn1, psk kn2)
    {
    int diff;
    if(Op(kn1) == EXP)
        kn1 = kn1->LEFT;
    if(Op(kn2) == EXP)
        kn2 = kn2->LEFT;

    diff = number_degree(kn2);
    if(diff != 4)
        diff -= number_degree(kn1);
    else
        {
        diff = -number_degree(kn1);
        if(!diff)
            return 0; /* two numbers */
        }
    if(diff > 0)
        return 1; /* switch places */
    else if(diff == 0)
        {
        diff = equal(kn1, kn2);
        if(diff > 0)
            {
            return 1; /* switch places */
            }
        else if(diff < 0)
            {
            return -1;
            }
        else
            {
            return 0;
            }
        }
    else
        {
        return -1;
        }
    }

static psk merge
(psk Pnode
 , int(*comp)(psk, psk)
 , psk(*combine)(psk)
#if EXPAND
 , psk(*expand)(psk, int*)
#endif
)
    {
    psk lhead, ltail, rhead, rtail;
    psk Rennur = &nilNode; /* Will contain all evaluated nodes in inverse order.*/
    psk tmp;
    for(;;)
        {/* traverse from left to right to evaluate left side branches */
#if EXPAND
        Boolean ok;
#endif
        Pnode = isolated(Pnode);
        assert(!shared(Pnode));
        Pnode->v.fl |= READY;
#if EXPAND
        do
            {
            Pnode->LEFT = eval(Pnode->LEFT);
            Pnode->LEFT = expand(Pnode->LEFT, &ok);
            } while(ok);
#else
        Pnode->LEFT = eval(Pnode->LEFT);
#endif
        tmp = Pnode->RIGHT;
        if(tmp->v.fl & READY)
            {
            break;
            }
        if(!is_op(tmp)
           || Op(Pnode) != Op(tmp)
           )
            {
#if EXPAND
            do
                {
                tmp = eval(tmp);
                tmp = expand(tmp, &ok);
                } while(ok);
                Pnode->RIGHT = tmp;
#else
            Pnode->RIGHT = eval(tmp);
#endif
            break;
            }
        Pnode->RIGHT = Rennur;
        Rennur = Pnode;
        Pnode = tmp;
        }
    for(;;)
        { /* From right to left, prepend sorted elements to result */
        psk rennur = &nilNode; /*Will contain branches in inverse sorted order*/
        psk L = leftoperand_and_tail(Pnode, &lhead, &ltail);
        psk R = rightoperand_and_tail(Pnode, &rhead, &rtail);
        for(;;)
            { /* From right to left, prepend smallest of lhs and rhs
                 to rennur
              */
            assert(rhead->v.fl & READY);
            assert((L == lhead && ltail == NULL) || L->RIGHT == ltail);
            assert((L == lhead && ltail == NULL) || L->LEFT == lhead);
            assert(Pnode->LEFT == L);
            assert((R == rhead && rtail == NULL) || R->RIGHT == rtail);
            assert((R == rhead && rtail == NULL) || R->LEFT == rhead);
            assert(Pnode->RIGHT == R);
            assert(L->v.fl & READY);
            assert(Pnode->RIGHT->v.fl & READY);
            if(comp(lhead, rhead) <= 0) /* a * b */
                {
                if(ltail == NULL)   /* a * (b*c) */
                    {
                    assert(Pnode->RIGHT->v.fl & READY);
                    break;
                    }
                else                /* (a*d) * (b*c) */
                    {
                    L = isolated(L);
                    assert(!shared(L));
                    if(ltail != L->RIGHT)
                        {
                        wipe(L->RIGHT); /* rare, set REFCOUNTSTRESSTEST 1 */
                        ltail = same_as_w(ltail);
                        }
                    L->RIGHT = rennur;
                    rennur = L;
                    assert(!shared(Pnode));
                    Pnode = isolated(Pnode);
                    assert(!shared(Pnode));
                    Pnode->LEFT = ltail;
                    L = leftoperand_and_tail(Pnode, &lhead, &ltail);
                    }               /* rennur := a*rennur */
                /* d * (b*c) */
                }
            else /* Wrong order */
                {
                Pnode = isolated(Pnode);
                assert(!shared(Pnode));
                assert(L->v.fl & READY);
                if(rtail == NULL) /* (b*c) * a */
                    {
                    Pnode->LEFT = R;
                    assert(L->v.fl & READY);
                    Pnode->RIGHT = L;
                    break;
                    }             /* a * (b*c) */
                else          /* (b*c) * (a*d)         c * (b*a) */
                    {
                    R = isolated(R);
                    assert(!shared(R));
                    if(R->RIGHT != rtail)
                        {
                        wipe(R->RIGHT); /* rare, set REFCOUNTSTRESSTEST 1 */
                        rtail = same_as_w(rtail);
                        }
                    R->RIGHT = rennur;
                    rennur = R;
                    assert(!shared(Pnode));
                    Pnode->RIGHT = rtail;
                    R = rightoperand_and_tail(Pnode, &rhead, &rtail);
                    }         /* rennur :=  a*rennur    rennur := b*rennur */
                /* (b*c) * d */
                }
            }
        for(;;)
            { /*Combine combinable elements and prepend to result*/
            Pnode->v.fl |= READY;

            Pnode = combine(Pnode);
            if(!(Pnode->v.fl & READY))
                { /*This may results in recursive call to merge
                    if the result of the evaluation is not in same
                    sorting position as unevaluated expression. */
                assert(!shared(Pnode));
                Pnode = eval(Pnode);
                }
#if DATAMATCHESITSELF
            if(is_op(Pnode))
                {
                if((Pnode->LEFT->v.fl & SELFMATCHING) && (Pnode->RIGHT->v.fl & SELFMATCHING))
                    Pnode->v.fl |= SELFMATCHING;
                else
                    Pnode->v.fl &= ~SELFMATCHING;
                }
#endif
            if(rennur != &nilNode)
                {
                psk n = rennur->RIGHT;
                assert(!shared(rennur));
                rennur->RIGHT = Pnode;
                Pnode = rennur;
                rennur = n;
                }
            else
                break;
            }
        if(Rennur == &nilNode)
            break;
        tmp = Rennur->RIGHT;
        assert(!shared(Rennur));
        Rennur->RIGHT = Pnode;
        Pnode = Rennur;
        assert(!shared(Pnode));
        Pnode->v.fl |= READY;
#if DATAMATCHESITSELF
        if(is_op(Pnode))
            {
            if((Pnode->LEFT->v.fl & SELFMATCHING) && (Pnode->RIGHT->v.fl & SELFMATCHING))
                Pnode->v.fl |= SELFMATCHING;
            else
                Pnode->v.fl &= ~SELFMATCHING;
            }
#endif
        Rennur = tmp;
        }
    return Pnode;
    }

static psk substlog(psk Pnode)
    {
    static const char* conc[] = { NULL,NULL,NULL,NULL };
    psk lnode = Pnode->LEFT, rightnode = Pnode->RIGHT;
    if(!equal(lnode, rightnode))
        {
        wipe(Pnode);
        return copyof(&oneNode);
        }
    else if(is_op(rightnode))  /*{?} x\L(2+y) => x\L(2+y) */
        {
        int ok;
        return tryq(Pnode, f5, &ok); /*{?} x\L(a*x^n*z) => n+x\L(a*z) */
        }
    else if(IS_ONE(rightnode))  /*{?} x\L1 => 0 */ /*{!} 0 */
        {
        wipe(Pnode);
        return copyof(&zeroNode);
        }
    else if(RAT_NUL(rightnode)) /*{?} z\L0 => z\L0 */
        return Pnode;
    else if(is_op(lnode)) /*{?} (x+y)\Lz => (x+y)\Lz */
        return Pnode;
    else if(RAT_NUL(lnode))   /*{?} 0\Lx => 0\Lx */
        return Pnode;
    else if(IS_ONE(lnode))   /*{?} 1\Lx => 1\Lx */
        return Pnode;
    else if(RAT_NEG(lnode))  /*{?} -7\Lx => -7\Lx */
        return Pnode;
    else if(RATIONAL_COMP(rightnode))  /*{?} x\L7 => x\L7 */
        {
        if(qCompare(rightnode, &zeroNode) & MINUS)
            {
            /* (nL-m = i*pi/eLn+nLm)  */ /*{?} 7\L-9 => 1+7\L9/7+i*pi*e\L7^-1 */ /*{!} i*pi/e\L7+7\L9)  */
            addr[1] = lnode;
            addr[2] = rightnode;
            return build_up(Pnode, "(i*pi*e\016\1^-1+\1\016(-1*\2))", NULL);
            }
        else if(RATIONAL_COMP(lnode)) /* m\Ln */ /*{?} 7\L9 => 1+7\L9/7 */
            {
            if(qCompare(lnode, &oneNode) & MINUS)
                {
                /* (1/n)Lm = -1*nLm */ /*{?} 1/7\L9 => -1*(1+7\L9/7) */ /*{!} -1*nLm */
                addr[1] = rightnode;
                conc[0] = "(-1*";
                conc[1] = hash6;
                addr[6] = qqDivide(&oneNode, lnode);
                conc[2] = "\016\1)";
                Pnode = vbuildup(Pnode, conc);
                wipe(addr[6]);
                return Pnode;
                }
            else if(qCompare(lnode, rightnode) & MINUS)
                {
                /* nL(n+m) = 1+nL((n+m)/n) */ /*{?} 7\L(7+9) => 1+7\L16/7 */ /*{!} 1+nL((n+m)/n) */
                conc[0] = "(1+\1\016";
                assert(lnode != rightnode);
                addr[1] = lnode;
                conc[1] = hash6;
                addr[6] = qqDivide(rightnode, lnode);
                conc[2] = ")";
                Pnode = vbuildup(Pnode, conc);
                wipe(addr[6]);
                return Pnode;
                }
            else if(qCompare(rightnode, &oneNode) & MINUS)
                {
                /* nL(1/m) = -1+nL(n/m) */ /*{?} 7\L1/9 => -2+7\L49/9 */ /*{!} -1+nL(n/m) */
                conc[0] = "(-1+\1\016";
                assert(lnode != rightnode);
                addr[1] = lnode;
                conc[1] = hash6;
                addr[6] = qTimes(rightnode, lnode);
                conc[2] = ")";
                Pnode = vbuildup(Pnode, conc);
                wipe(addr[6]);
                return Pnode;
                }
            }
        }
    return Pnode;
    }

static psk substdiff(psk Pnode)
    {
    psk lnode, rightnode;
    lnode = Pnode->LEFT;
    rightnode = Pnode->RIGHT;
    if(is_constant(lnode) || is_constant(rightnode))
        {
        wipe(Pnode);
        Pnode = copyof(&zeroNode);
        }
    else if(!equal(lnode, rightnode))
        {
        wipe(Pnode);
        Pnode = copyof(&oneNode);
        }
    else if(!is_op(rightnode)
            && is_dependent_of(lnode, rightnode)
            )
        {
        ;
        }
    else if(!is_op(rightnode))
        {
        wipe(Pnode);
        Pnode = copyof(&zeroNode);
        }
    else if(Op(rightnode) == PLUS)
        {
        addr[1] = lnode;
        addr[2] = rightnode->LEFT;
        addr[3] = rightnode->RIGHT;
        Pnode = build_up(Pnode, "((\1\017\2)+(\1\017\3))", NULL);
        }
    else if(is_op(rightnode))
        {
        addr[2] = rightnode->LEFT;
        addr[1] = lnode;
        addr[3] = rightnode->RIGHT;
        switch(Op(rightnode))
            {
            case TIMES:
                Pnode = build_up(Pnode, "(\001\017\2*\3+\2*\001\017\3)", NULL);
                break;
            case EXP:
                Pnode = build_up(Pnode,
                                 "(\2^(-1+\3)*\3*\001\017\2+\2^\3*e\016\2*\001\017\3)", NULL);
                break;
            case LOG:
                Pnode = build_up(Pnode,
                                 "(\2^-1*e\016\2^-2*e\016\3*\001\017\2+\3^-1*e\016\2^-1*\001\017\3)", NULL);
                break;
            }
        }
    return Pnode;
    }


#if JMP /* Often no need for polling in multithreaded apps.*/
#include <windows.h>
#include <dde.h>
static void PeekMsg(void)
    {
    static MSG msg;
    while(PeekMessage(&msg, NULL, WM_PAINT, WM_DDE_LAST, PM_REMOVE))
        {
        if(msg.message == WM_QUIT)
            {
            PostThreadMessage(GetCurrentThreadId(), WM_QUIT, 0, 0L);
            longjmp(jumper, 1);
            }
        TranslateMessage(&msg);        /* Translates virtual key codes */
        DispatchMessage(&msg);        /* Dispatches message to window*/
        }
    }
#endif

/*
Iterative handling of WHITE operator in evaluate.
Can now handle very deep structures without stack overflow
*/

static psk handleWhitespace(psk Pnode)
    { /* assumption: (Op(*Pnode) == WHITE) && !((*Pnode)->v.fl & READY) */
    static psk apnode;
    psk whitespacenode;
    psk next;
    ppsk pwhitespacenode = &Pnode;
    ppsk prevpwhitespacenode = NULL;
    for(;;)
        {
        whitespacenode = *pwhitespacenode;
        whitespacenode->LEFT = eval(whitespacenode->LEFT);
        if(!is_op(apnode = whitespacenode->LEFT)
           && !(apnode->u.obj)
           && !HAS_UNOPS(apnode)
           )
            {
            *pwhitespacenode = rightbranch(whitespacenode);
            }
        else
            {
            prevpwhitespacenode = pwhitespacenode;
            pwhitespacenode = &(whitespacenode->RIGHT);
            }
        if(Op(whitespacenode = *pwhitespacenode) == WHITE && !(whitespacenode->v.fl & READY))
            {
            if(shared(*pwhitespacenode))
                *pwhitespacenode = copyop(*pwhitespacenode);
            }
        else
            {
            *pwhitespacenode = eval(*pwhitespacenode);
            if(prevpwhitespacenode
               && !is_op(whitespacenode = *pwhitespacenode)
               && !((whitespacenode)->u.obj)
               && !HAS_UNOPS(whitespacenode)
               )
                *prevpwhitespacenode = leftbranch(*prevpwhitespacenode);
            break;
            }
        }

    whitespacenode = Pnode;
    while(Op(whitespacenode) == WHITE)
        {
        next = whitespacenode->RIGHT;
        bringright(whitespacenode);
        if(next->v.fl & READY)
            break;
        whitespacenode = next;
        whitespacenode->v.fl |= READY;
        }
    return Pnode;
    }
/*
Iterative handling of COMMA operator in evaluate.
Can now handle very deep structures without stack overflow
*/
static psk handleComma(psk Pnode)
    { /* assumption: (Op(*Pnode) == COMMA) && !((*Pnode)->v.fl & READY) */
    psk commanode = Pnode;
    psk next;
    ppsk pcommanode;
    while(Op(commanode->RIGHT) == COMMA && !(commanode->RIGHT->v.fl & READY))
        {
        commanode->LEFT = eval(commanode->LEFT);
        pcommanode = &(commanode->RIGHT);
        commanode = commanode->RIGHT;
        if(shared(commanode))
            {
            *pcommanode = commanode = copyop(commanode);
            }
        }
    commanode->LEFT = eval(commanode->LEFT);
    commanode->RIGHT = eval(commanode->RIGHT);
    commanode = Pnode;
    while(Op(commanode) == COMMA)
        {
        next = commanode->RIGHT;
        bringright(commanode);
        if(next->v.fl & READY)
            break;
        commanode = next;
        commanode->v.fl |= READY;
        }
    return Pnode;
    }

static psk evalvar(psk Pnode)
    {
    psk loc_adr = SymbolBinding_w(Pnode, Pnode->v.fl & DOUBLY_INDIRECT);
    if(loc_adr != NULL)
        {
        wipe(Pnode);
        Pnode = loc_adr;
        }
    else
        {
        if(shared(Pnode))
            {
            /*You can get here if a !variable is unitialized*/
            dec_refcount(Pnode);
            Pnode = iCopyOf(Pnode);
            }
        assert(!shared(Pnode));
        Pnode->v.fl |= READY;
        Pnode->v.fl ^= SUCCESS;
        }
    return Pnode;
    }

static void privatized(psk Pnode, psk plkn)
    {
    *plkn = *Pnode;
    if(shared(plkn))
        {
        dec_refcount(Pnode);
        plkn->LEFT = same_as_w(plkn->LEFT);
        plkn->RIGHT = same_as_w(plkn->RIGHT);
        }
    else
        pskfree(Pnode);
    }

static psk __rightbranch(psk Pnode)
    {
    psk ret;
    int success = Pnode->v.fl & SUCCESS;
    if(shared(Pnode))
        {
        ret = same_as_w(Pnode->RIGHT);
        dec_refcount(Pnode);
        }
    else
        {
        ret = Pnode->RIGHT;
        wipe(Pnode->LEFT);
        pskfree(Pnode);
        }
    if(!success)
        {
        ret = isolated(ret);
        ret->v.fl ^= SUCCESS;
        }
    return ret;
    }


static psk eval(psk Pnode)
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

int startProc(
#if _BRACMATEMBEDDED
    startStruct* init
#else
    void
#endif
)
    {
    int tel;
    int err; /* evaluation of version string */
    static int called = 0;
    if(called)
        {
        return 2;
        }
    called = 1;
#if _BRACMATEMBEDDED
    if(init)
        {
        if(init->WinIn)
            {
            WinIn = init->WinIn;
            }
        if(init->WinOut)
            {
            WinOut = init->WinOut;
            }
        if(init->WinFlush)
            {
            WinFlush = init->WinFlush;
            }
#if defined PYTHONINTERFACE
        if(init->Ni)
            {
            Ni = init->Ni;
            }
        if(init->Nii)
            {
            Nii = init->Nii;
            }
#endif
        }
#endif
    for(tel = 0; tel < 256; variables[tel++] = NULL)
        ;
    if(!init_memoryspace())
        return 0;
    init_opcode();
    global_anchor = NULL;
    global_fpi = stdin;
    global_fpo = stdout;

    argNode.v.fl = READY | SUCCESS;
    argNode.u.lobj = O('a', 'r', 'g');

    sjtNode.v.fl = READY | SUCCESS;
    sjtNode.u.lobj = O('s', 'j', 't');

    selfNode.v.fl = READY | SUCCESS;
    selfNode.u.lobj = O('i', 't', 's');

    SelfNode.v.fl = READY | SUCCESS;
    SelfNode.u.lobj = O('I', 't', 's');

    nilNode.v.fl = READY | SUCCESS | IDENT;
    nilNode.u.lobj = 0L;

    nilNodeNotNeutral.v.fl = READY | SUCCESS;
    nilNodeNotNeutral.u.lobj = 0L;

    zeroNode.v.fl = READY | SUCCESS | IDENT | QNUMBER | QNUL BITWISE_OR_SELFMATCHING;
    zeroNode.u.lobj = 0L;
    zeroNode.u.obj = '0';

    zeroNodeNotNeutral.v.fl = READY | SUCCESS | QNUMBER | QNUL BITWISE_OR_SELFMATCHING;
    zeroNodeNotNeutral.u.lobj = 0L;
    zeroNodeNotNeutral.u.obj = '0';

    oneNode.u.lobj = 0L;
    oneNode.u.obj = '1';
    oneNode.v.fl = READY | SUCCESS | IDENT | QNUMBER BITWISE_OR_SELFMATCHING;
    *(&(oneNode.u.obj) + 1) = 0;

    oneNodeNotNeutral.u.lobj = 0L;
    oneNodeNotNeutral.u.obj = '1';
    oneNodeNotNeutral.v.fl = READY | SUCCESS | QNUMBER BITWISE_OR_SELFMATCHING;
    *(&(oneNode.u.obj) + 1) = 0;

    minusTwoNode.u.lobj = 0L;
    minusTwoNode.u.obj = '2';
    minusTwoNode.v.fl = READY | SUCCESS | QNUMBER | MINUS BITWISE_OR_SELFMATCHING;
    *(&(minusTwoNode.u.obj) + 1) = 0;

    minusOneNode.u.lobj = 0L;
    minusOneNode.u.obj = '1';
    minusOneNode.v.fl = READY | SUCCESS | QNUMBER | MINUS BITWISE_OR_SELFMATCHING;
    *(&(minusOneNode.u.obj) + 1) = 0;

    twoNode.u.lobj = 0L;
    twoNode.u.obj = '2';
    twoNode.v.fl = READY | SUCCESS | QNUMBER BITWISE_OR_SELFMATCHING;
    *(&(twoNode.u.obj) + 1) = 0;

    fourNode.u.lobj = 0L;
    fourNode.u.obj = '4';
    fourNode.v.fl = READY | SUCCESS | QNUMBER BITWISE_OR_SELFMATCHING;
    *(&(fourNode.u.obj) + 1) = 0;

    minusFourNode.u.lobj = 0L;
    minusFourNode.u.obj = '4';
    minusFourNode.v.fl = READY | SUCCESS | QNUMBER | MINUS BITWISE_OR_SELFMATCHING;
    *(&(minusFourNode.u.obj) + 1) = 0;

    m0 = build_up(m0, "?*(%+%)^~/#>1*?", NULL);
    m1 = build_up(m1, "?*(%+%)*?", NULL);
    f0 = build_up(f0, "(g,k,pow"
                  ".(pow"
                  "=b,c,d,l,s,f"
                  ".!arg:(%?b+%?c)^?d"
                  "&1:?f"
                  "&(s"
                  "=!d:1&0"
                  "|(!f*!d*(1+!l:?l)^-1:?f)"
                  "*!b^!l"
                  "*pow$(!c^(-1+!d:?d))"
                  "+!s"
                  ")"
                  "&!b^!d+!c^!d+!s"
                  "|!arg"
                  ")"
                  "&!arg:?g*((%+%)^~/#>1:?arg)*?k"
                  "&!g*pow$!arg*!k)", NULL);
    f1 = build_up(f1,
                  "((\177g,\177h,\177i).!arg:?\177g*(%?\177h+%?\177i)*",
                  "?arg&!\177g*!\177h*!arg+!\177g*!\177i*!arg)", NULL);
    f4 = build_up(f4, "l,a,b,c,e,f"
                  ".(a"
                  "=j,g,h,i"
                  ".!arg:?l^(?j+?g*!l\016?h*?i+?arg)"
                  "&!l^(!j+!arg)*!h^(!g*!i)"
                  ")"
                  "&(e"
                  "=j,g,I"
                  ".!arg:?j+#?g*((i|-i):?I)*pi+?arg"
                  "&1:?l"
                  "&!j+(mod$(1+!g,2)+-1)*!I*pi+!arg"
                  ")"
                  "&(f"
                  "=j,i"
                  ".!arg:?j+#?l*((i|-i):?I)*pi+?arg"
                  "&!I^(2*!l):?l"
                  "&!j+!arg"
                  ")"
                  "&(b"
                  "="
                  "(!l:(<-1|>1)&e"
                  "|(-1|1/2|-1/2)&f"
                  ")"
                  ":?l"
                  ")"
                  "&(c"
                  "="
                  ".1+!arg:?arg"
                  "&1:?l"
                  "&-1+!arg"
                  ")"
                  "&(!arg:?l^(?+?*!l\016?*?+?)&a$!arg"
                  "|!arg"
                  ":e^((?+#?l*(i|-i)*pi+?&`!b"
                  "|?"
                  "*(pi|i|-i)"
                  "*?"
                  "*(?+?*(pi|i|-i)*?+?:%+%)"
                  "*?"
                  "&c:?l"
                  ")"
                  ":?arg"
                  ")"
                  "&e^!l$!arg*!l"
                  ")", NULL);
    f5 = build_up(f5,
                  "l,d"
                  ".(d"
                  "=j,g,h"
                  ".!arg:(~1:?l)\016(?j*!l^?g*?h)&!g+!l\016(!j*!h)"
                  ")"
                  "&!arg:?l\016(?*!l^?*?)"
                  "&d$!arg", NULL);

    global_anchor = starttree_w(global_anchor,
                                "(cat=flt,sin,tay,fct,cos,out,sgn.!arg:((?flt,(?sin,?tay)|?sin&:?tay)|?flt&:?sin:"
                                "?tay)&(fct=.!arg:%?cos ?arg&!cos:((?out.?)|?out)&'(? ($out|($out.?)"
                                ") ?):(=?sgn)&(!flt:!sgn&!(glf$(=~.!sin:!sgn))&!cos|) fct$!arg|)&(:!flt:!sin&mem$!tay|(:!flt&mem$:?flt"
                                "|)&fct$(mem$!tay))),",
                                "(let=.@(ugc$!arg:(L|M) ?)&!arg),",
                                "(out=(.put$!arg:?arg&put$\212&!arg)),"
                                "(flt=((e,d,m,s,f).!arg:(?arg,~<0:?d)&!arg:0|(-1*!arg:>0:?arg&-1|1):?s&"
                                "10\016!arg:?e+(10\016?m|0&1:?m)&(!m+1/2*1/10^!d:~<10&1+!e:?e&!m*1/10"
                                ":?m|)&@(div$(!m+1/2*(1/10^!d:?d),!d):"
                                "`%?f ?m)&str$(!s*!f (!d:~1&\254|) !m \252\261\260\305 !e))),",

                                "(tay=((f,tot,x,fac,cnt,res,R).",
                                "(R=!cnt:!tot&!res|!res+(sub$(!x\017!f:?f.!x.0))*((!fac*(!cnt+1:?cnt))"
                                ":?fac)^-1*!x^!cnt:?res&!R)&",
                                "!arg:(?f,?x,?tot)&(fac=1)&(cnt=0)&((sub$(!f.!x.0)):?res)&!R)),",

                                /*"(ego=(r.sub$(sub$(!arg.(sin.?r).'sin$!r).(cos.?r).'cos$!r))),",

                                "(goe=((h,s).sub$(!arg.'(e^?h).",
                                "'(-1*(sgn$(-1+i*!h+1:?h):?s)*i*(sin.-1+!s*!h+1:?s)+(cos.!s))))),",*/

                                "(sin=(.i*(-1/2*e^(i*!arg)+1/2*e^(-i*!arg)))),",

                                "(cos=(.1/2*(e^(i*!arg)+e^(-i*!arg)))),",

                                "(jsn=Q R O C T H I X Y.(Q=.!arg:(,?arg)&R$!arg|!arg:(.@?arg)&I$!arg|"
                                "!arg:(0|(?.?)+?,)&O$!arg|!arg:(true|false|null)|!arg:/&(X$!arg|Y$("
                                "!arg,20))|!arg)&(R=J S L.\333:?S&whl'(!arg:%?J ?arg&\254 Q$!J !S:?S)&"
                                "(!S:\254 ?S|)&\335 !S:?S&:?L&whl'(!S:%?a ?S&!a !L:?L)&str$!L)&(O=J V "
                                "S.!arg:(?arg,)&:?S&whl'(!arg:(@?J.?V)+?arg&!S \254 I$!J \272 Q$!V:?S)"
                                "&(!S:\254 ?S|)&str$(\373 !S \375))&(C=a c z.@(!arg:?a (%@:<\240:?c) "
                                "?z)&str$(!a (!c:\210&\334\342|!c:\212&\334\356|!c:\215&\334\362|!c:"
                                "\214&\334\346|!c:\211&\334\364|\334\365\260\260 d2x$(asc$!c)) C$!z)|"
                                "!arg)&(T=a z.@(!arg:?a \" ?z)&str$(C$!a \334\242 T$!z)|C$!arg)&(H=a z"
                                ".@(!arg:?a \334 ?z)&str$(T$!a \334\334 H$!z)|T$!arg)&(I=.str$(\" H$"
                                "!arg \"))&(X=a z d n A B D F G L x.den$!arg:?d&!d*(!arg:~<0|-1*!arg):"
                                "?n&(5\016!d:#%?A+?B&`(!B:0&2^!A|!B:5\016?B&2\016(!B*(den$!B:?B)):#?D&"
                                "!A+-1*5\016!B+-1*!D:?A&(!A:>0&2|1/5)^!A)|2\016!d:#%?A&5^!A):?F&-1+-1*"
                                "10\016(!d*!F):?x&!n*!F:?G&@(!G:? [?L)&whl'(!L+!x:<0&str$(0 !G):?G&1+"
                                "!L:?L)&@(!G:?a [!x ?z)&whl'@(!z:?z 0)&str$((!arg:<0&\255|) !a (!z:|"
                                "\256 !z)))&(Y=e,d,m,s,f.!arg:(?arg,~<0:?d)&!arg:0|(-1*!arg:>0:?arg&-1"
                                "|1):?s&10\016!arg:?e+(10\016?m|0&1:?m)&(!m+1/2*1/10^!d:~<10&1+!e:?e&"
                                "!m*1/10:?m|)&@(div$(!m+1/2*(1/10^!d:?d),!d):%?`f ?m)&str$(!s*!f (!d:"
                                "~1&\256 (@(rev$!m:? #?m)&rev$!m)|!m) E !e))&str$(Q$!arg)),",
                                "(sgn=(.!arg:?#%arg*%+?&sgn$!arg|!arg:<0&-1|1)),",
                                "(abs=(.sgn$!arg*!arg)),",
                                "(sub=\177e,\177x,\177v,\177F.(\177F=\177l,\177r.!arg:!\177x&!\177v|"
                                "!arg:%?\177l_%?\177r&(\177F$!\177l)_(\177F$!\177r)|!arg)&!arg:(("
                                "?\177e.?\177x.?\177v)|out$(str$((=sub$(expr.var.rep)) !arg))&get'&~`)"
                                "&\177F$!\177e),",

                                "(MLencoding=D N M J h m R.:?N:?D&(R=? (charset.?N) ?|? (content.@(?:? "
                                "charset ? \275 ?N)) ?)&(M=? (~<>meta.(!R,?)|!R) ?)&(J=? (~<>head.(?,"
                                "!M)|?&~`) ?|? (~<>head:?h.?) ?m (.!h.) ?&!m:!M)&(!arg:(@ (\277.@(?:"
                                "~<>XML (? encoding ? \275 ? (\242 ?N \242|\247 ?N \247) ?|?&"
                                "utf\255\270:?D))) ?&!D:|? (~<>html.(?,!J)|?&~`) ?|? (~<>html.?) !J)|)&(!N"
                                ":~|!arg:? (\241DOCTYPE.@(?:? html ?)) ?&utf\255\270|!D)),",
                                "(nestML=a L B s e x.:?L&whl'(!arg:%?a ?arg&!a !L:?L)&!L:?arg&:?L:?B:"
                                "?s&whl'(!arg:%?a ?arg&(!a:(.?e.)&(!L.) !B:?B&:?L&!e !s:?s|(!a:(?e.?,?"
                                ")&!a|!a:(?e.?x)&(!s:!e ?s&(!e.!x,!L) (!B:(%?L.) ?B&)|!a)|!a) !L:?L))&"
                                "!L),",
                                "(toML=O d t g l \246 w \242 \247 H G S D E F I U x.:?O&0:?x&chr$\261\262\267"
                                ":?D&(d=a.whl'(!arg:%?a ?arg&!a !O:?O))&'(a c n.@(!arg:?a (>%@($D) ?:"
                                "?arg))&(@(!arg:(%?c&utf$!c:?n) ?arg)|@(!arg:(%?c&asc$!c:?n) ?arg))&!a"
                                " \246\243 !n \273 S$!arg|!arg):(=?S)&'(a c n.@(!arg:?a (>%@($D) ?:"
                                "?arg))&(@(!arg:(%?c&utf$!c:?n) ?arg)&(!n:>\262\265\265&!a \246\243 !n"
                                " \273 I$!arg|!a chr$!n I$!arg)|!a !arg)|!arg):(=?I)&'(a c n.@(!arg:?a"
                                " (>%@($D) ?:?arg))&(@(!arg:(%?c&utf$!c:?n) ?)&!a !arg|@(!arg:(%?c&asc"
                                "$!c:?n) ?arg)&!a chu$!n U$!arg)|!arg):(=?U)&MLencoding$!arg:("
                                "~<>utf\255\270&U:?F|~<>iso\255\270\270\265\271\255\261&I:?F|?&S:?F)&'"
                                "($F)$!a:(=?H)&'($F)$!arg:(=?G)&'(a b.@(!arg:?a \246 ?arg)&$H "
                                "\246amp\273 \246$!arg|$G):(=?\246)&'(a b.@(!arg:?a ()$D ?arg)&\246$!a"
                                " (@(!arg:%?b ?arg)&!b) E$!arg|\246$!arg):(=?E)&'(a.@(!arg:?a \274 "
                                "?arg)&E$!a \246lt\273 l$!arg|E$!arg):(=?l)&(g=a.@(!arg:?a \276 ?arg)&"
                                "l$!a \246gt\273 g$!arg|l$!arg)&(\242=a.@(!arg:?a \242 ?arg)&g$!a "
                                "\246quot\273 \242$!arg|g$!arg)&(\247=a.@(!arg:?a \247 ?arg)&\242$!a "
                                "\246apos\273 \247$!arg|\242$!arg)&(t=a v.!arg:(?a.?v) ?arg&\240 g$!a "
                                "\275\242 \247$!v \242 t$!arg|)&(!arg:? (html|HTML.?) ?&(Q=!C:&low$!A:"
                                "(area|base|br|col|command|embed|hr|img|input|keygen|link|meta|param|"
                                "source|track|wbr))|(Q=!C:))&'(r A B C T.whl'(!arg:%?r ?arg&(!r:(?A."
                                "?B)&(!B:(?T,?C)&($Q&d$(\274 !A t$!T \240\257\276)|d$(\274 !A t$!T "
                                "\276)&(!A:(~<>script|~<>style)&d$!C|w$!C)&d$(\274\257 !A \276))|!A:(&"
                                "!B:(?B.)&d$(\274\257 g$!B \276)|\241&d$(\274\241 !F$!B \276)|"
                                "\241\255\255&d$(\274\241\255\255 !F$!B \255\255\276)|\277&d$(\274\277"
                                " (=.@(!arg:(?arg \277|xml ?:?arg))&1:?x&!arg \277|!arg (!x:1&\277|))$"
                                "(!F$!B) \276)|\241\333CDATA\333&d$(\274\241\333CDATA\333 !F$!B "
                                "\335\335\276)|\241DOCTYPE&d$(\274\241DOCTYPE !F$!B \276)|?&d$(\274 !A"
                                " t$!B \276)))|d$(g$!r)))):(=?w)&w$!arg&str$((=a L.:?L&whl'(!arg:%?a "
                                "?arg&!a !L:?L)&!L)$!O)),",
                                fct,
                                NULL);


#if JMP
    if(setjmp(jumper) != 0)
        return;
#endif
    global_anchor = eval(global_anchor);
    stringEval("(v=\"Bracmat version " VERSION ", build " BUILD " (" DATUM ")\")", NULL, &err);
    return 1;
    }

void endProc(void)
    {
    int err;
    static int called = 0;
    if(called)
        return;
    called = 1;
    stringEval("cat$:? CloseDown ? & CloseDown$ | ", NULL, &err);
    if(err)
        errorprintf("Error executing CloseDown\n");
    }

/* main - the text-mode front end for bracmat */


#if _BRACMATEMBEDDED
#else

#include <stddef.h>

#if defined __EMSCRIPTEN__
int oneShot(char* inp)
    {
    int err;
    char* mainLoopExpression;
    const char* ret;
    char* argv[1] = { NULL };
    argv[0] = inp;
    ARGV = argv;
    ARGC = 1;
    mainLoopExpression = "out$get$(arg$0,MEM)";
    stringEval(mainLoopExpression, &ret, &err);
    return (int)STRTOL(ret, 0, 10);
    }
#endif

int mainLoop(int argc, char* argv[])
    {
    int err;
    char* mainLoopExpression;
    const char* ret = 0;
#if defined __EMSCRIPTEN__
    if(argc == 2)
        { /* to get here, e.g.: ./bracmat out$hello */
        return oneShot(argv[1]);
        }
    else
#endif
        if(argc > 1)
            { /* to get here, e.g.: ./bracmat "you:?a" "out$(hello !a)" */
            ARGC = argc;
            ARGV = argv;
            mainLoopExpression = "arg$&whl'(get$(arg$,MEM):?\"?...@#$*~%!\")&!\"?...@#$*~%!\"";
            stringEval(mainLoopExpression, &ret, &err);
            return ret ? (int)STRTOL(ret, 0, 10) : -1;
            }
        else
            {
            mainLoopExpression =
                "(w=\"11. BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY\\n"
                "FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT WHEN\\n"
                "OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES\\n"
                "PROVIDE THE PROGRAM \\\"AS IS\\\" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED\\n"
                "OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF\\n"
                "MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS\\n"
                "TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE\\n"
                "PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING,\\n"
                "REPAIR OR CORRECTION.\\n\")&"
                "(c=\"12. IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING\\n"
                "WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR\\n"
                "REDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES,\\n"
                "INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING\\n"
                "OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED\\n"
                "TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY\\n"
                "YOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER\\n"
                "PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BONE ADVISED OF THE\\n"
                "POSSIBILITY OF SUCH DAMAGES.\\n\")&"
                "out$!v&"
                "out$\"Copyright (C) 2002 Bart Jongejan\\n"
                "Bracmat comes with ABSOLUTELY NO WARRANTY; for details type `!w'.\\n"
                "This is free software, and you are welcome to redistribute it\\n"
                "under certain conditions; type `!c' for details.\\n\\n"
                "\\n\\n{?} get$help { tutorial }\\n{?} )        { stop }\"&"
                "(main=put$\"{?} \"&clk$():?SEC&((get':?!(=):(|?&clk$+-1*!SEC:?SEC&"
                "put$\"{!} \"&put$!&put$(\"\\n    S  \" str$(div$(!SEC,1) \",\" (div$(mod$("
                "!SEC*100,100),1):?SEC&!SEC:<10&0|) !SEC) sec))|put$\"\\n    F\")|"
                "put$\"\\n    I\")&"




#if TELMAX

                "out$str$(\"  \" bez')&"
#else
                "out$&"
#endif

                "!main)&!main";
            stringEval(mainLoopExpression, NULL, &err);
            }
    return 0;
    }

extern void JSONtest(void);

#if EMSCRIPTEN_HTML
int main()
    {
    int ret = 0;
    errorStream = stderr;
    if(!startProc())
        return -1;
    return ret;
    }
#else
int main(int argc, char* argv[])
    {
    int ret; /* numerical result of mainLoop becomes exit code.
             If out of integer range or not numerical: 0*/
    char* p = argv[0] + strlen(argv[0]);
#if 0
    char s1[] = { 160,0 };
    char s2[] = { 32,0 };
#ifdef _WIN64 /* Microsoft 64 bit */
    printf("_WIN64\n");
#else /* 32 bit and gcc 64 bit */
    printf("!_WIN64\n");
#endif
    printf("sizeof(char *) " LONGU "\n", (ULONG)sizeof(char*));
    printf("sizeof(LONG) " LONGU "\n", (ULONG)sizeof(LONG));
    printf("sizeof(ULONG) " LONGU "\n", (ULONG)sizeof(ULONG));
    printf("sizeof(size_t) " LONGU "\n", (ULONG)sizeof(size_t));
    printf("sizeof(sk) " LONGU "\n", (ULONG)sizeof(sk));
    printf("sizeof(tFlags) " LONGU "\n", (ULONG)sizeof(tFlags));
    printf("WORD32 %d\n", WORD32);
    printf("RADIX " LONGD "\n", (LONG)RADIX);
    printf("RADIX2 " LONGD "\n", (LONG)RADIX2);
    printf("TEN_LOG_RADIX " LONGD "\n", (LONG)TEN_LOG_RADIX);
    printf("HEADROOM " LONGD "\n", (LONG)HEADROOM);
    printf("strcmp:%d\n", strcmp(s1, s2));
    return 0;
#endif
#ifndef NDEBUG
    {
    LONG radix, ten_log_radix;
#if defined _WIN64
    assert(HEADROOM * RADIX2 < LLONG_MAX);
#else
    assert(HEADROOM * RADIX2 < LONG_MAX);
#endif
    for(radix = 1, ten_log_radix = 1; ten_log_radix <= TEN_LOG_RADIX; radix *= 10, ++ten_log_radix)
        ;
    assert(RADIX == radix);
    }
#endif
    assert(sizeof(tFlags) == sizeof(ULONG));
#if !defined __EMSCRIPTEN__
    while(--p >= argv[0])
        if(*p == '\\' || *p == '/')
            {
            ++p;
#if !defined NO_FOPEN
            targetPath = (char*)malloc(p - argv[0] + 1);
            if(targetPath)
                {
                strncpy(targetPath, argv[0], p - argv[0]);
                targetPath[p - argv[0]] = '\0';
                }
#endif
            break;
            }
#endif

    errorStream = stderr;
    if(!startProc())
        return -1;
    ret = mainLoop(argc, argv);
    endProc();/* to get here, eg: {?} main=out$bye! */
#if !defined NO_FOPEN
    if(targetPath)
        free(targetPath);
#endif
    return ret;
    }
#endif
#endif /*#if !_BRACMATEMBEDDED*/

