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
#define DATUM "28 April 2017"
#define VERSION "6 'newspeak'"
#define BUILD "212"
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
/* 20010309 evalueer on WHITE and COMMA strings is now iterative on the
   right descending (deep) branch, not recursive.
   (It is now possible to read long lists, e.g. dictionairies, without causing
   stack overflow when evaluating the list.)
   20010821 a () () was evaluated to a (). Removed last ().
   20010903 (a.) () was evaluated to a
*/
#define DEBUGBRACMAT 0 /* implement dbg'(expression), see DBGSRC macro */
#define REFCOUNTSTRESSTEST 0
#define DOSUMCHECK 0
#define CHECKALLOCBOUNDS 0 /* only if NDEBUG is false */
#define PVNAME 0 /* allocate strings of variable names separately from variable structure. */
#define STRINGMATCH_CAN_BE_NEGATED 0 /* ~@(a:a) */
#define CODEPAGE850 0
#define MAXSTACK 0 /* 1: show max stack depth (eval function only)*/
#define CUTOFFSUGGEST 1
#define READMARKUPFAMILY 1 /* read SGML, HTML and XML files. (include xml.c in your project!) */
#define READJSON 1 /* read JSON files. (include json.c in your project!) */
/* 
About reference counting.
Most nodes can be shared by no more than 1024 referers. Copies must be made as needed.
Objects (nodes with = ('EQUALS') are objects and cane be shared by almost 2^40 referers.

small refcounter       large refcounter              comment
(10 bits, all nodes)   (30 bits, only objects)
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

/*#define reslt hreslt */ /* to show ALL parentheses (one pair for each operator)*/
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
typedef   signed int  INT32_T;
#elif ULONG_MAX == 4294967295ul
typedef unsigned long UINT32_T;
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
/* flags (prefixes) in knoop */
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
#define BREUK           (1<<12) /* / */
#define UNIFY           (1<<13) /* ? */
#define IDENT           (1<<14)
#define IMPLIEDFENCE    (1<<15) /* 20070222 */

#define VISIBLE_FLAGS_WEAK      (INDIRECT|DOUBLY_INDIRECT|FENCE|UNIFY)
#define VISIBLE_FLAGS_NON_COMP  (INDIRECT|DOUBLY_INDIRECT|ATOM|NONIDENT|NUMBER|BREUK|UNIFY) /* allows < > ~< and ~> as flags on numbers */
#define VISIBLE_FLAGS_POS0      (INDIRECT|DOUBLY_INDIRECT|NONIDENT|QBREUK|UNIFY|QGETAL)
#define VISIBLE_FLAGS_POS       (INDIRECT|DOUBLY_INDIRECT|NONIDENT|QBREUK|UNIFY|QGETAL|NOT|GREATER_THAN|SMALLER_THAN)
#define VISIBLE_FLAGS           (INDIRECT|DOUBLY_INDIRECT|ATOM|NONIDENT|NUMBER|BREUK|UNIFY|NOT|GREATER_THAN|SMALLER_THAN|FENCE|POSITION)

#define HAS_VISIBLE_FLAGS_OR_MINUS(psk) ((psk)->ops & (VISIBLE_FLAGS|MINUS))
#define RATIONAAL(psk)      (((psk)->ops & (QGETAL|IS_OPERATOR|VISIBLE_FLAGS)) == QGETAL)
#define RATIONAAL_COMP(psk) (((psk)->ops & (QGETAL|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP)) == QGETAL)
#define RATIONAAL_COMP_NOT_NUL(psk) (((psk)->ops & (QGETAL|QNUL|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP)) == QGETAL)
#define RATIONAAL_WEAK(psk) (((psk)->ops & (QGETAL|IS_OPERATOR|INDIRECT|DOUBLY_INDIRECT|FENCE|UNIFY)) == QGETAL)/* allows < > ~< and ~> as flags on numbers */
#define       LESS(psk)    (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QGETAL|SMALLER_THAN))
#define LESS_EQUAL(psk)    (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QGETAL|NOT|GREATER_THAN))
#define MORE_EQUAL(psk)    (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QGETAL|NOT|SMALLER_THAN))
#define       MORE(psk)    (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QGETAL|GREATER_THAN))
#define    UNEQUAL(psk)    (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QGETAL|NOT))
#define LESSORMORE(psk)    (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QGETAL|SMALLER_THAN|GREATER_THAN))
#define      EQUAL(psk)    (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == QGETAL)
#define NOTLESSORMORE(psk) (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QGETAL|NOT|SMALLER_THAN|GREATER_THAN))

#define INTEGER(kn)               (((kn)->ops & (QGETAL|QBREUK|IS_OPERATOR|VISIBLE_FLAGS))                      == QGETAL)
#define INTEGER_COMP(kn)          (((kn)->ops & (QGETAL|QBREUK|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP))             == QGETAL)

#define INTEGER_NIET_NEG(kn)      (((kn)->ops & (QGETAL|MINUS|QBREUK|IS_OPERATOR|VISIBLE_FLAGS))                == QGETAL)
#define INTEGER_NIET_NEG_COMP(kn) (((kn)->ops & (QGETAL|MINUS|QBREUK|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP))       == QGETAL)

#define INTEGER_POS(kn)           (((kn)->ops & (QGETAL|MINUS|QNUL|QBREUK|IS_OPERATOR|VISIBLE_FLAGS))           == QGETAL)
#define INTEGER_POS_COMP(kn)      (((kn)->ops & (QGETAL|MINUS|QNUL|QBREUK|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP))  == QGETAL)

#define INTEGER_NIET_NUL_COMP(kn) (((kn)->ops & (QGETAL|QNUL|QBREUK|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP))        == QGETAL)
#define HAS_MINUS_SIGN(kn)         (((kn)->ops & (MINUS|IS_OPERATOR)) == MINUS)

#define RAT_NUL(kn) (((kn)->v.fl & (QNUL|IS_OPERATOR|VISIBLE_FLAGS)) == QNUL)
#define RAT_NUL_COMP(kn) (((kn)->v.fl & (QNUL|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP)) == QNUL)
#define RAT_NEG(kn) (((kn)->ops & (QGETAL|MINUS|IS_OPERATOR|VISIBLE_FLAGS)) \
                                == (QGETAL|MINUS))
#define RAT_NEG_COMP(kn) (((kn)->ops & (QGETAL|MINUS|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP)) \
                                == (QGETAL|MINUS))

#define RAT_RAT(kn) (((kn)->ops & (QGETAL|QBREUK|IS_OPERATOR|VISIBLE_FLAGS))\
                                == (QGETAL|QBREUK))

#define RAT_RAT_COMP(kn) (((kn)->ops & (QGETAL|QBREUK|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP))\
                                == (QGETAL|QBREUK))
#define IS_EEN(kn) ((kn)->u.lobj == EEN && !((kn)->ops & (MINUS | VISIBLE_FLAGS)))


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

#define SHL 16

#define ops v.fl
#define flgs v.fl

#ifdef __TURBOC__
#if O_S
typedef struct
    {
    int r[10];
    } os_regset;
#endif
#endif

#define RIGHT u.p.rechts
#define LEFT  u.p.links
#define TRUE 1
#define FALSE 0
#define PRISTINE (1<<2)
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
#else
#if !WORD32
#define iobj lobj
#endif
#define O(a,b,c) (a+b*0x100+c*0x10000L)
#endif

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

#define ECH O('E','C','H')
#define EEN O('1', 0 , 0 )
/* err$foo redirects error messages to foo */
#define ERR O('e','r','r')
#define EXT O('E','X','T')
#define FLG O('f','l','g')
#define GLF O('g','l','f') /* The opposite of flg */
#define GET O('g','e','t')
/*#define HUM O('H','U','M')*/
#define HT  O('H','T', 0 )
#define IM  O('i', 0 , 0 )
#define JSN O('J','S','N')
#define KAR O('c','h','r')
#define KAU O('c','h','u')
#define LIN O('L','I','N')
#define LOW O('l','o','w')
#define REV O('r','e','v') /* strrev */
#define LST O('l','s','t')
#define MEM O('M','E','M')
#define MINEEN O('-','1',0)
#define ML  O('M','L',0)
#define MMF O('m','e','m')
#define MOD O('m','o','d')
#define NEW O('N','E','W')
#define New O('n','e','w')
#define PI  O('p','i', 0 )
#define PUT O('p','u','t')
#define PRV O('?', 0 , 0 )
#define RAW O('R','A','W')
#define STR O('s','t','r')
#define SIM O('s','i','m')
#define STG O('S','T','R')
#define TBL O('t','b','l')
#define TRM O('T','R','M')
#define TWEE O('2', 0 , 0 )
#define TXT O('T','X','T')
#define UPP O('u','p','p')
#define UTF O('u','t','f')
#define VAP O('V','A','P')
#define WHL O('w','h','l')
#define X   O('X', 0 , 0 )
#define X2D O('x','2','d') /* hex -> dec */
#define D2X O('d','2','x') /* dec -> hex */
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
extern void XMLtext(FILE * fpi,char * bron,int trim,int html,int xml);
#endif

#if READJSON
#define OPT_JSON (1 << SHIFT_JSN)
extern int JSONtext(FILE * fpi,char * bron);
#endif


#if REFCOUNTSTRESSTEST
#define REF_COUNT_BITS 1
#else
#define REF_COUNT_BITS 10 /* 1 - 10, 5 - 8 is very nice */
#endif
/*#define REF_COUNT_BITS 1 */ /* stress test!*/
#define NON_REF_COUNT_BITS (16-REF_COUNT_BITS)

#define FILTERS     (BREUK | NUMBER | SMALLER_THAN | GREATER_THAN | ATOM | NONIDENT)
#define ATOMFILTERS (BREUK | NUMBER | SMALLER_THAN | GREATER_THAN | ATOM | FENCE | IDENT)
#define SATOMFILTERS (/*ATOM | */FENCE | IDENT)

#define FLGS (FILTERS | FENCE | DOUBLY_INDIRECT | INDIRECT | POSITION)

#define ONTKENNING(Flgs,flag)  ((Flgs & NOT ) && \
                                (Flgs & FILTERS) >= (flag) && \
                                (Flgs & FILTERS) < ((flag) << 1))
#define ANYNEGATION(Flgs)  ((Flgs & NOT ) && (Flgs & FILTERS))
#define ASSERTIVE(Flgs,flag) ((Flgs & flag) && !ONTKENNING(Flgs,flag))
#define FAAL (pat->v.fl & NOT)
#define NIKS(p) (((p)->v.fl & NOT) && !((p)->v.fl & FILTERS))
#define NIKSF(Flgs) ((Flgs & NOT) && !(Flgs & FILTERS))
#define ERFENIS (FILTERS | FENCE | UNIFY)
#define UNOPS (UNIFY | FLGS | NOT | MINUS)
#define HAS_UNOPS(a) ((a)->v.fl & UNOPS)
#define HAS__UNOPS(a) (is_op(a) && (a)->v.fl & (UNIFY | FLGS | NOT))
#define IS_VARIABLE(a) ((a)->v.fl & (UNIFY | INDIRECT | DOUBLY_INDIRECT))
#define IS_BANG_VARIABLE(a) ((a)->v.fl & (INDIRECT | DOUBLY_INDIRECT))

/*#define SUBJECTNOTNIL(sub,pat) (is_op(sub) || HAS_UNOPS(sub) || (PIOBJ(sub) != PIOBJ(nil(pat))))*/
#define SUBJECTNOTNIL(sub,pat) (is_op(sub) || HAS_UNOPS(sub) || (PLOBJ(sub) != PLOBJ(nil(pat))))


typedef int Boolean;
typedef struct Vars vars;

typedef union
        {
#ifndef NDEBUG
        struct
            {
            unsigned int Not             :1; /* ~ */
            unsigned int success         :1;
            unsigned int ready           :1;
            unsigned int position        :1; /* [ */

            unsigned int indirect        :1; /* ! */
            unsigned int doubly_indirect :1; /* !! */
            unsigned int fence           :1; /* `   (within same byte as ATOM and NOT) */
            unsigned int atom            :1; /* @ */

            unsigned int nonident        :1; /* % */
            unsigned int greater_than    :1; /* > */
            unsigned int smaller_than    :1; /* < */
            unsigned int number          :1; /* # */

            unsigned int breuk           :1; /* / */
            unsigned int unify           :1; /* ? */
            unsigned int ident           :1;
            unsigned int impliedfence    :1;

            unsigned int IS_OPERATOR     :1;
            unsigned int binop           :4;
            /* EQUALS DOT COMMA OR AND MATCH WHITE PLUS TIMES EXP LOG DIF FUU FUN UNDERSCORE */
            unsigned int latebind        :1;
            unsigned int refcount        :10;
            } node;
        struct
            {
            unsigned int Not             :1; /* ~ */
            unsigned int success         :1;
            unsigned int ready           :1;
            unsigned int position        :1; /* [ */

            unsigned int indirect        :1; /* ! */
            unsigned int doubly_indirect :1; /* !! */
            unsigned int fence           :1; /* `   (within same byte as ATOM and NOT) */
            unsigned int atom            :1; /* @ */

            unsigned int nonident        :1; /* % */
            unsigned int greater_than    :1; /* > */
            unsigned int smaller_than    :1; /* < */
            unsigned int number          :1; /* # */

            unsigned int breuk           :1; /* / */
            unsigned int unify           :1; /* ? */
            unsigned int ident           :1;
            unsigned int impliedfence    :1;

            unsigned int is_operator     :1;
            unsigned int qgetal          :1;
            unsigned int minus           :1;
            unsigned int qnul            :1;
            unsigned int qbreuk          :1;

            unsigned int latebind        :1;
            unsigned int refcount        :10;
            } leaf;
#endif
        unsigned int fl;
        } tFlags;


typedef struct sk
    {
    tFlags v;

    union
        {
        struct
            {
            struct sk *links,*rechts;
            } p;
        LONG lobj; /* This part of the structure can be used for comparisons
                      with short strings that fit into one word in one machine
                      operation, like "\0\0\0\0" or "1\0\0\0" */
        unsigned char obj;
        char sobj;
        } u;
    } sk;

static sk nilk,nulk,eenk,argk,selfkn,Selfkn,mintweek,mineenk,tweek,minvierk,vierk,sjt;

#if !defined NO_FOPEN
static char * targetPath = NULL; /* Path that can be prepended to filenames. */
#endif

typedef  sk * psk;
typedef psk * ppsk;

static psk adr[7],m0 = NULL,m1 = NULL,f0 = NULL,f1 = NULL,f4 = NULL,f5 = NULL;

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

typedef struct ngetal
    {
    ptrdiff_t length;
    ptrdiff_t ilength;
    void * alloc;
    void * ialloc;
    LONG * inumber;
    char * number;
    size_t allocated;
    size_t iallocated;
    int sign; /* 0: positive, QNUL: zero, MINUS: negative number */
    } ngetal;

#define NGETALIS1(x) ((x)->sign == 0 && (x)->length == 1 && ((char*)((x)->number))[0] == '1')

#define Qgetal psk

typedef struct varia
    {
    struct varia *prev; /* verdi[-1] */
    psk verdi[1];       /* verdi[0], arraysize is adjusted by psh */
    } varia;

struct Vars /* sizeof(vars) = n * 4 bytes */
    {
#if PVNAME
    unsigned char *vname;
#define VARNAME(x) x->vname
#endif
    vars *next;
    int n;
    int selector;
    varia *pvaria; /* Can also contain entry[0]   (if n == 0) */
#if !PVNAME
    union
        {
        LONG Lobj;
        unsigned char Obj;
        } u;
#define VARNAME(x) &x->u.Obj
#endif
    };

static vars * variabelen[256];

static int ARGC = 0;
static char ** ARGV = NULL;

typedef struct kknoop
    {
    tFlags v;
    psk links,rechts;
    } kknoop;

#define BUILTIN (1 << 30)

typedef struct objectknoop /* createdWithNew == 0 */
    {
    tFlags v;
    psk links,rechts;
#ifdef BUILTIN
    union
        {
        struct
            {
            unsigned int refcount : 30;
            unsigned int built_in:1;
            unsigned int createdWithNew:1;
            } s;
        int Int:32;
        } u;
#else
    unsigned int refcount : 30;
    unsigned int built_in:1;
    unsigned int createdWithNew:1;
#endif
    } objectknoop;

typedef struct stringrefknoop
    {
    tFlags v;
    psk kn;
    char * str;
    size_t length;
    } stringrefknoop;

typedef psk function_return_type;

#define functionFail(x) ((x)->v.fl ^= SUCCESS,(x))
#define functionOk(x) (x)

struct typedObjectknoop;
typedef Boolean (*method_pnt)(struct typedObjectknoop * This,ppsk pkn);

typedef struct method
    {
    char * name;
    method_pnt func;
    } method;

struct Hash;

typedef struct typedObjectknoop /* createdWithNew == 1 */
    {
    tFlags v;
    psk links,rechts; /* links == nil, rechts == data (if vtab == NULL)
            or name of object type, e.g. [set], [hash], [file], [float] (if vtab != NULL)*/
#ifdef BUILTIN
    union
        {
        struct
            {
            unsigned int refcount : 30;
            unsigned int built_in:1;
            unsigned int createdWithNew:1;
            } s;
        int Int:32;
        } u;
#else
    unsigned int refcount : 30; /* Always 0L */
    unsigned int built_in:1;
    unsigned int createdWithNew:1;
#endif
    struct Hash * voiddata;
    #define HASH(x) (Hash*)x->voiddata
    #define VOID(x) x->voiddata
    #define PHASH(x) (Hash**)&(x->voiddata)
    method * vtab; /* The last element n of the array must have vtab[n].name == NULL */
    } typedObjectknoop;

#ifdef BUILTIN
#define INCREFCOUNT(a) { ((objectknoop*)a)->u.s.refcount++;(a)->ops &= ((~ALL_REFCOUNT_BITS_SET)|ONE); }
#define DECREFCOUNT(a) { ((objectknoop*)a)->u.s.refcount--;(a)->ops |= ALL_REFCOUNT_BITS_SET; }
#define REFCOUNTNONZERO(a) ((a)->u.s.refcount)
#define ISBUILTIN(a) ((a)->u.s.built_in)
#define ISCREATEDWITHNEW(a) ((a)->u.s.createdWithNew)
#define SETCREATEDWITHNEW(a) (a)->u.s.createdWithNew = 1
#else
#define INCREFCOUNT(a) { (a)->refcount++;(a)->ops &= ((~ALL_REFCOUNT_BITS_SET)|ONE); }
#define DECREFCOUNT(a) { (a)->refcount--;(a)->ops |= ALL_REFCOUNT_BITS_SET; }
#define REFCOUNTNONZERO(a) ((a)->refcount)
#define ISBUILTIN(a) ((a)->built_in)
#define SETBUILTIN(a) (a)->built_in = 1
#define UNSETBUILTIN(a) (a)->built_in = 0
#define ISCREATEDWITHNEW(a) ((a)->createdWithNew)
#define SETCREATEDWITHNEW(a) (a)->createdWithNew = 1
#define UNSETCREATEDWITHNEW(a) (a)->createdWithNew = 0
#endif

/*#if !_BRACMATEMBEDDED*/
#if !defined NO_FOPEN
static char * errorFileName = NULL;
#endif
static FILE * errorStream = NULL;
/*#endif*/

#if !defined NO_FOPEN
enum {NoPending,Writing,Reading};
typedef struct filehendel
    {
    char *naam;
    FILE *fp;
    struct filehendel *next;
#if !defined NO_LOW_LEVEL_FILE_HANDLING
    Boolean dontcloseme;
    LONG filepos; /* Normally -1. If >= 0, then the file is closed.
                When reopening, filepos is used to find the position
                before the file was closed. */
    LONG mode;
    LONG type;
    LONG size;
    LONG getal;
    LONG tijd;
    int rwstatus;
    char * stop; /* contains characters to stop reading at, default NULL */
#endif
    } filehendel;

static filehendel *fh0 = NULL;
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
    char *fileName;
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

/*          operator              leaf                optab
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
    12                  BREUK
    13                  UNIFY
    14                  IDENT
    15               IMPLIEDFENCE
ops 16  0             IS_OPERATOR
    17  1   (operators 0-14)      QGETAL
    18  2       "                 MINUS
    19  3       "                 QNUL
    20  4       "                 QBREUK
    21  5             LATEBIND                        NOOP
    22  6          (reference count)
    23  7                 "
    24  8                 "
    25  9                 "
    26 10                 "
    27 11                 "
    28 12                 "
    29 13                 "
    30 14                 "
    31 15                 "

Reference count starts with 0, not 1
*/

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
{NULL,NULL,NULL,NULL,NULL,NULL,&nilk,&nulk,
&eenk,NULL,NULL,NULL,NULL,NULL,NULL,NULL};

static const char opchar[16] =
{'=','.',',','|','&',':',' ','+','*','^','\016','\017','\'','$','_','?'};

#define OPERATOR ((0xF<<OPSH) + IS_OPERATOR)

#define kop(kn) ((kn)->ops & OPERATOR)
#define kopo(kn) ((kn).ops & OPERATOR)
#define is_op(kn) ((kn)->ops & IS_OPERATOR)
#define is_object(kn) (((kn)->ops & OPERATOR) == EQUALS)
#define klopcode(kn) (kop(kn) >> OPSH)

#define nil(p) knil[klopcode(p)]


#define NOOP                (OPERATOR+1)
#define QGETAL              (1 << (SHL+1))
#define MINUS               (1 << (SHL+2))
#define QNUL                (1 << (SHL+3))
#define QBREUK              (1 << (SHL+4))
#define LATEBIND            (1 << (SHL+5))
#define DEFINITELYNONUMBER  (1 << (SHL+6)) /* this is not stored in a node! */
#define ONE   (unsigned int)(1 << (SHL+NON_REF_COUNT_BITS))

#define ALL_REFCOUNT_BITS_SET \
       ((((unsigned int)(~0)) >> (SHL+NON_REF_COUNT_BITS)) << (SHL+NON_REF_COUNT_BITS))

#define shared(kn) ((kn)->ops & ALL_REFCOUNT_BITS_SET)
#define sharedo(kn) ((kn).ops & ALL_REFCOUNT_BITS_SET)


static int all_refcount_bits_set(psk kn)
    {
    return (shared(kn) == ALL_REFCOUNT_BITS_SET) && !is_object(kn);
    }

static void dec_refcount(psk kn)
    {
    assert(kn->ops & ALL_REFCOUNT_BITS_SET);
    kn->ops -= ONE;
    if((kn->ops & (OPERATOR|ALL_REFCOUNT_BITS_SET)) == EQUALS)
        {
        if(REFCOUNTNONZERO((objectknoop*)kn))
            {
            DECREFCOUNT(kn);
            }
        }
    }

#define STRING    1
#define VAPORIZED 2
#define MEMORY    4
#define ECHO      8

#include <stdarg.h>

#if defined EMSCRIPTEN /* This is set if compiling with emscripten. */
#define EMSCRIPTEN_HTML 1 /* set to 1 if using emscripten to convert this file to HTML*/
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
 /* must be 0: compiling with emcc (emscripten C to JavaScript compiler) */
#else
#define GLOBALARGPTR 1 /* 0 if compiling with emcc (emscripten C to JavaScript compiler) */
#endif

#if GLOBALARGPTR
static va_list argptr;
#define VOIDORARGPTR void
#else
#define VOIDORARGPTR va_list * pargptr
#endif
static unsigned char *startPos;

static const char
hekje5[] = "\5",
hekje6[] = "\6",
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

#if TELMAX
static size_t globalloc = 0, maxgloballoc = 0;
#endif

#if TELLING
static size_t cnts[256],alloc_cnt = 0,totcnt = 0;
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
static int mooi = TRUE;
static int optab[256];
static int dummy_op = WHITE;
#if DEBUGBRACMAT
static int debug = 0;
#endif

#if TELMAX
static unsigned int maxbez = 0;
#endif

static FILE * global_fpi;
static FILE * global_fpo;

#if 0
/*
Based on UnicodeData.txt (16-May-2016 19:23)
(Does not convert final sigma to non-final sigma)

Structures created with CaseFolding.bra
*/
struct cletter {int L:21;int range:11;};
struct cletter Cletters[]={{0x41,25},{0x61,25},
{0xAA,0},{0xB5,0},{0xBA,0},{0xC0,22},{0xD8,30},{0xF8,457},{0x2C6,11},{0x2E0,4},
{0x2EC,0},{0x2EE,0},{0x370,4},{0x376,1},{0x37A,3},{0x37F,0},{0x386,0},{0x388,2},
{0x38C,0},{0x38E,19},{0x3A3,82},{0x3F7,138},{0x48A,165},{0x531,37},{0x559,0},{0x561,38},
{0x5D0,26},{0x5F0,2},{0x620,42},{0x66E,1},{0x671,98},{0x6D5,0},{0x6E5,1},{0x6EE,1},
{0x6FA,2},{0x6FF,0},{0x710,0},{0x712,29},{0x74D,88},{0x7B1,0},{0x7CA,32},{0x7F4,1},
{0x7FA,0},{0x800,21},{0x81A,0},{0x824,0},{0x828,0},{0x840,24},{0x8A0,20},{0x8B6,7},
{0x904,53},{0x93D,0},{0x950,0},{0x958,9},{0x971,15},{0x985,7},{0x98F,1},{0x993,21},
{0x9AA,6},{0x9B2,0},{0x9B6,3},{0x9BD,0},{0x9CE,0},{0x9DC,1},{0x9DF,2},{0x9F0,1},
{0xA05,5},{0xA0F,1},{0xA13,21},{0xA2A,6},{0xA32,1},{0xA35,1},{0xA38,1},{0xA59,3},
{0xA5E,0},{0xA72,2},{0xA85,8},{0xA8F,2},{0xA93,21},{0xAAA,6},{0xAB2,1},{0xAB5,4},
{0xABD,0},{0xAD0,0},{0xAE0,1},{0xAF9,0},{0xB05,7},{0xB0F,1},{0xB13,21},{0xB2A,6},
{0xB32,1},{0xB35,4},{0xB3D,0},{0xB5C,1},{0xB5F,2},{0xB71,0},{0xB83,0},{0xB85,5},
{0xB8E,2},{0xB92,3},{0xB99,1},{0xB9C,0},{0xB9E,1},{0xBA3,1},{0xBA8,2},{0xBAE,11},
{0xBD0,0},{0xC05,7},{0xC0E,2},{0xC12,22},{0xC2A,15},{0xC3D,0},{0xC58,2},{0xC60,1},
{0xC80,0},{0xC85,7},{0xC8E,2},{0xC92,22},{0xCAA,9},{0xCB5,4},{0xCBD,0},{0xCDE,0},
{0xCE0,1},{0xCF1,1},{0xD05,7},{0xD0E,2},{0xD12,40},{0xD3D,0},{0xD4E,0},{0xD54,2},
{0xD5F,2},{0xD7A,5},{0xD85,17},{0xD9A,23},{0xDB3,8},{0xDBD,0},{0xDC0,6},{0xE01,47},
{0xE32,1},{0xE40,6},{0xE81,1},{0xE84,0},{0xE87,1},{0xE8A,0},{0xE8D,0},{0xE94,3},
{0xE99,6},{0xEA1,2},{0xEA5,0},{0xEA7,0},{0xEAA,1},{0xEAD,3},{0xEB2,1},{0xEBD,0},
{0xEC0,4},{0xEC6,0},{0xEDC,3},{0xF00,0},{0xF40,7},{0xF49,35},{0xF88,4},{0x1000,42},
{0x103F,0},{0x1050,5},{0x105A,3},{0x1061,0},{0x1065,1},{0x106E,2},{0x1075,12},{0x108E,0},
{0x10A0,37},{0x10C7,0},{0x10CD,0},{0x10D0,42},{0x10FC,332},{0x124A,3},{0x1250,6},{0x1258,0},
{0x125A,3},{0x1260,40},{0x128A,3},{0x1290,32},{0x12B2,3},{0x12B8,6},{0x12C0,0},{0x12C2,3},
{0x12C8,14},{0x12D8,56},{0x1312,3},{0x1318,66},{0x1380,15},{0x13A0,85},{0x13F8,5},{0x1401,619},
{0x166F,16},{0x1681,25},{0x16A0,74},{0x16F1,7},{0x1700,12},{0x170E,3},{0x1720,17},{0x1740,17},
{0x1760,12},{0x176E,2},{0x1780,51},{0x17D7,0},{0x17DC,0},{0x1820,87},{0x1880,4},{0x1887,33},
{0x18AA,0},{0x18B0,69},{0x1900,30},{0x1950,29},{0x1970,4},{0x1980,43},{0x19B0,25},{0x1A00,22},
{0x1A20,52},{0x1AA7,0},{0x1B05,46},{0x1B45,6},{0x1B83,29},{0x1BAE,1},{0x1BBA,43},{0x1C00,35},
{0x1C4D,2},{0x1C5A,35},{0x1C80,8},{0x1CE9,3},{0x1CEE,3},{0x1CF5,1},{0x1D00,191},{0x1E00,277},
{0x1F18,5},{0x1F20,37},{0x1F48,5},{0x1F50,7},{0x1F59,0},{0x1F5B,0},{0x1F5D,0},{0x1F5F,30},
{0x1F80,52},{0x1FB6,6},{0x1FBE,0},{0x1FC2,2},{0x1FC6,6},{0x1FD0,3},{0x1FD6,5},{0x1FE0,12},
{0x1FF2,2},{0x1FF6,6},{0x2071,0},{0x207F,0},{0x2090,12},{0x2102,0},{0x2107,0},{0x210A,9},
{0x2115,0},{0x2119,4},{0x2124,0},{0x2126,0},{0x2128,0},{0x212A,3},{0x212F,10},{0x213C,3},
{0x2145,4},{0x214E,0},{0x2183,1},{0x2C00,46},{0x2C30,46},{0x2C60,132},{0x2CEB,3},{0x2CF2,1},
{0x2D00,37},{0x2D27,0},{0x2D2D,0},{0x2D30,55},{0x2D6F,0},{0x2D80,22},{0x2DA0,6},{0x2DA8,6},
{0x2DB0,6},{0x2DB8,6},{0x2DC0,6},{0x2DC8,6},{0x2DD0,6},{0x2DD8,6},{0x2E2F,0},{0x3005,1},
{0x3031,4},{0x303B,1},{0x3041,85},{0x309D,2},{0x30A1,89},{0x30FC,3},{0x3105,40},{0x3131,93},
{0x31A0,26},{0x31F0,15},{0x3400,0},{0x4DB5,0},{0x4E00,0},{0x9FD5,0},{0xA000,1164},{0xA4D0,45},
{0xA500,268},{0xA610,15},{0xA62A,1},{0xA640,46},{0xA67F,30},{0xA6A0,69},{0xA717,8},{0xA722,102},
{0xA78B,35},{0xA7B0,7},{0xA7F7,10},{0xA803,2},{0xA807,3},{0xA80C,22},{0xA840,51},{0xA882,49},
{0xA8F2,5},{0xA8FB,0},{0xA8FD,0},{0xA90A,27},{0xA930,22},{0xA960,28},{0xA984,46},{0xA9CF,0},
{0xA9E0,4},{0xA9E6,9},{0xA9FA,4},{0xAA00,40},{0xAA40,2},{0xAA44,7},{0xAA60,22},{0xAA7A,0},
{0xAA7E,49},{0xAAB1,0},{0xAAB5,1},{0xAAB9,4},{0xAAC0,0},{0xAAC2,0},{0xAADB,2},{0xAAE0,10},
{0xAAF2,2},{0xAB01,5},{0xAB09,5},{0xAB11,5},{0xAB20,6},{0xAB28,6},{0xAB30,42},{0xAB5C,9},
{0xAB70,114},{0xAC00,0},{0xD7A3,0},{0xD7B0,22},{0xD7CB,48},{0xF900,365},{0xFA70,105},{0xFB00,6},
{0xFB13,4},{0xFB1D,0},{0xFB1F,9},{0xFB2A,12},{0xFB38,4},{0xFB3E,0},{0xFB40,1},{0xFB43,1},
{0xFB46,107},{0xFBD3,362},{0xFD50,63},{0xFD92,53},{0xFDF0,11},{0xFE70,4},{0xFE76,134},{0xFF21,25},
{0xFF41,25},{0xFF66,88},{0xFFC2,5},{0xFFCA,5},{0xFFD2,5},{0xFFDA,2},{0x10000,11},{0x1000D,25},
{0x10028,18},{0x1003C,1},{0x1003F,14},{0x10050,13},{0x10080,122},{0x10280,28},{0x102A0,48},{0x10300,31},
{0x10330,16},{0x10342,7},{0x10350,37},{0x10380,29},{0x103A0,35},{0x103C8,7},{0x10400,157},{0x104B0,35},
{0x104D8,35},{0x10500,39},{0x10530,51},{0x10600,310},{0x10740,21},{0x10760,7},{0x10800,5},{0x10808,0},
{0x1080A,43},{0x10837,1},{0x1083C,0},{0x1083F,22},{0x10860,22},{0x10880,30},{0x108E0,18},{0x108F4,1},
{0x10900,21},{0x10920,25},{0x10980,55},{0x109BE,1},{0x10A00,0},{0x10A10,3},{0x10A15,2},{0x10A19,26},
{0x10A60,28},{0x10A80,28},{0x10AC0,7},{0x10AC9,27},{0x10B00,53},{0x10B40,21},{0x10B60,18},{0x10B80,17},
{0x10C00,72},{0x10C80,50},{0x10CC0,50},{0x11003,52},{0x11083,44},{0x110D0,24},{0x11103,35},{0x11150,34},
{0x11176,0},{0x11183,47},{0x111C1,3},{0x111DA,0},{0x111DC,0},{0x11200,17},{0x11213,24},{0x11280,6},
{0x11288,0},{0x1128A,3},{0x1128F,14},{0x1129F,9},{0x112B0,46},{0x11305,7},{0x1130F,1},{0x11313,21},
{0x1132A,6},{0x11332,1},{0x11335,4},{0x1133D,0},{0x11350,0},{0x1135D,4},{0x11400,52},{0x11447,3},
{0x11480,47},{0x114C4,1},{0x114C7,0},{0x11580,46},{0x115D8,3},{0x11600,47},{0x11644,0},{0x11680,42},
{0x11700,25},{0x118A0,63},{0x118FF,0},{0x11AC0,56},{0x11C00,8},{0x11C0A,36},{0x11C40,0},{0x11C72,29},
{0x12000,921},{0x12480,195},{0x13000,1070},{0x14400,582},{0x16800,568},{0x16A40,30},{0x16AD0,29},{0x16B00,47},
{0x16B40,3},{0x16B63,20},{0x16B7D,18},{0x16F00,68},{0x16F50,0},{0x16F93,12},{0x16FE0,0},{0x17000,0},
{0x187EC,0},{0x18800,754},{0x1B000,1},{0x1BC00,106},{0x1BC70,12},{0x1BC80,8},{0x1BC90,9},{0x1D400,84},
{0x1D456,70},{0x1D49E,1},{0x1D4A2,0},{0x1D4A5,1},{0x1D4A9,3},{0x1D4AE,11},{0x1D4BB,0},{0x1D4BD,6},
{0x1D4C5,64},{0x1D507,3},{0x1D50D,7},{0x1D516,6},{0x1D51E,27},{0x1D53B,3},{0x1D540,4},{0x1D546,0},
{0x1D54A,6},{0x1D552,339},{0x1D6A8,24},{0x1D6C2,24},{0x1D6DC,30},{0x1D6FC,24},{0x1D716,30},{0x1D736,24},
{0x1D750,30},{0x1D770,24},{0x1D78A,30},{0x1D7AA,24},{0x1D7C4,7},{0x1E800,196},{0x1E900,67},{0x1EE00,3},
{0x1EE05,26},{0x1EE21,1},{0x1EE24,0},{0x1EE27,0},{0x1EE29,9},{0x1EE34,3},{0x1EE39,0},{0x1EE3B,0},
{0x1EE42,0},{0x1EE47,0},{0x1EE49,0},{0x1EE4B,0},{0x1EE4D,2},{0x1EE51,1},{0x1EE54,0},{0x1EE57,0},
{0x1EE59,0},{0x1EE5B,0},{0x1EE5D,0},{0x1EE5F,0},{0x1EE61,1},{0x1EE64,0},{0x1EE67,3},{0x1EE6C,6},
{0x1EE74,3},{0x1EE79,3},{0x1EE7E,0},{0x1EE80,9},{0x1EE8B,16},{0x1EEA1,2},{0x1EEA5,4},{0x1EEAB,16},
{0x20000,0},{0x2A6D6,0},{0x2A700,0},{0x2B734,0},{0x2B740,0},{0x2B81D,0},{0x2B820,0},{0x2CEA1,0},
{0x2F800,541},{0x1FFFFF,0}};

bool isAlpha(int a)
    {
    static int start_i = 0;
    int i;
    if(a <= 0)
        return false;
    if((unsigned int)a < Cletters[start_i].L)
        {
        for(i=0;;++i)
            {
            if((unsigned int)a < Cletters[i].L)
                {
                start_i = --i;
                return (unsigned int)a <= Cletters[i].L+Cletters[i].range;
                }
            }
        }
    else
        {
        for(i=start_i+1;;++i)
            {
            if((unsigned int)a < Cletters[i].L)
                {
                start_i = --i;
                return (unsigned int)a <= Cletters[i].L+Cletters[i].range;
                }
            }
        }
    return false;
    }

static int isLetter(int a)
    {
    int i;
    for(i=0;;++i)
        {
        if(a < Cletters[i].L)
            {
            --i;
            return a <= Cletters[i].L+Cletters[i].range;
            }
        }
    }
#endif

/* Based on http://unicode.org/Public/UNIDATA/UnicodeData.txt 16-May-2016 19:23 */
/* Structures created with uni.bra */
    
#define Y2016 1
#if Y2016
/* Based on http://unicode.org/Public/UNIDATA/UnicodeData.txt 16-May-2016 19:23 */
/* Structures created with uni.bra */
                  
struct ccaseconv {unsigned int L:21;int range:11;unsigned int inc:2;int dif:20;};
struct ccaseconv u2l[]={
{0x41,25,1,32},
{0xC0,22,1,32},
{0xD8,6,1,32},
{0x100,46,2,1},
{0x130,0,0,-199},
{0x132,4,2,1},
{0x139,14,2,1},
{0x14A,44,2,1},
{0x178,0,0,-121},
{0x179,4,2,1},
{0x181,0,0,210},
{0x182,2,2,1},
{0x186,0,0,206},
{0x187,0,0,1},
{0x189,1,1,205},
{0x18B,0,0,1},
{0x18E,0,0,79},
{0x18F,0,0,202},
{0x190,0,0,203},
{0x191,0,0,1},
{0x193,0,0,205},
{0x194,0,0,207},
{0x196,0,0,211},
{0x197,0,0,209},
{0x198,0,0,1},
{0x19C,0,0,211},
{0x19D,0,0,213},
{0x19F,0,0,214},
{0x1A0,4,2,1},
{0x1A6,0,0,218},
{0x1A7,0,0,1},
{0x1A9,0,0,218},
{0x1AC,0,0,1},
{0x1AE,0,0,218},
{0x1AF,0,0,1},
{0x1B1,1,1,217},
{0x1B3,2,2,1},
{0x1B7,0,0,219},
{0x1B8,0,0,1},
{0x1BC,0,0,1},
{0x1C4,0,0,2},
{0x1C5,0,0,1},
{0x1C7,0,0,2},
{0x1C8,0,0,1},
{0x1CA,0,0,2},
{0x1CB,16,2,1},
{0x1DE,16,2,1},
{0x1F1,0,0,2},
{0x1F2,2,2,1},
{0x1F6,0,0,-97},
{0x1F7,0,0,-56},
{0x1F8,38,2,1},
{0x220,0,0,-130},
{0x222,16,2,1},
{0x23A,0,0,10795},
{0x23B,0,0,1},
{0x23D,0,0,-163},
{0x23E,0,0,10792},
{0x241,0,0,1},
{0x243,0,0,-195},
{0x244,0,0,69},
{0x245,0,0,71},
{0x246,8,2,1},
{0x370,2,2,1},
{0x376,0,0,1},
{0x37F,0,0,116},
{0x386,0,0,38},
{0x388,2,1,37},
{0x38C,0,0,64},
{0x38E,1,1,63},
{0x391,16,1,32},
{0x3A3,8,1,32},
{0x3CF,0,0,8},
{0x3D8,22,2,1},
{0x3F4,0,0,-60},
{0x3F7,0,0,1},
{0x3F9,0,0,-7},
{0x3FA,0,0,1},
{0x3FD,2,1,-130},
{0x400,15,1,80},
{0x410,31,1,32},
{0x460,32,2,1},
{0x48A,52,2,1},
{0x4C0,0,0,15},
{0x4C1,12,2,1},
{0x4D0,94,2,1},
{0x531,37,1,48},
{0x10A0,36,1,7264},
{0x10C5,2,2,7264},
{0x10CD,0,0,7264},
{0x13A0,79,1,38864},
{0x13F0,5,1,8},
{0x1E00,148,2,1},
{0x1E9E,0,0,-7615},
{0x1EA0,94,2,1},
{0x1F08,7,1,-8},
{0x1F18,5,1,-8},
{0x1F28,7,1,-8},
{0x1F38,7,1,-8},
{0x1F48,5,1,-8},
{0x1F59,6,2,-8},
{0x1F68,7,1,-8},
{0x1F88,7,1,-8},
{0x1F98,7,1,-8},
{0x1FA8,7,1,-8},
{0x1FB8,1,1,-8},
{0x1FBA,1,1,-74},
{0x1FBC,0,0,-9},
{0x1FC8,3,1,-86},
{0x1FCC,0,0,-9},
{0x1FD8,1,1,-8},
{0x1FDA,1,1,-100},
{0x1FE8,1,1,-8},
{0x1FEA,1,1,-112},
{0x1FEC,0,0,-7},
{0x1FF8,1,1,-128},
{0x1FFA,1,1,-126},
{0x1FFC,0,0,-9},
{0x2126,0,0,-7517},
{0x212A,0,0,-8383},
{0x212B,0,0,-8262},
{0x2132,0,0,28},
{0x2160,15,1,16},
{0x2183,0,0,1},
{0x24B6,25,1,26},
{0x2C00,46,1,48},
{0x2C60,0,0,1},
{0x2C62,0,0,-10743},
{0x2C63,0,0,-3814},
{0x2C64,0,0,-10727},
{0x2C67,4,2,1},
{0x2C6D,0,0,-10780},
{0x2C6E,0,0,-10749},
{0x2C6F,0,0,-10783},
{0x2C70,0,0,-10782},
{0x2C72,0,0,1},
{0x2C75,0,0,1},
{0x2C7E,1,1,-10815},
{0x2C80,98,2,1},
{0x2CEB,2,2,1},
{0x2CF2,0,0,1},
{0xA640,44,2,1},
{0xA680,26,2,1},
{0xA722,12,2,1},
{0xA732,60,2,1},
{0xA779,2,2,1},
{0xA77D,0,0,-35332},
{0xA77E,8,2,1},
{0xA78B,0,0,1},
{0xA78D,0,0,-42280},
{0xA790,2,2,1},
{0xA796,18,2,1},
{0xA7AA,0,0,-42308},
{0xA7AB,0,0,-42319},
{0xA7AC,0,0,-42315},
{0xA7AD,0,0,-42305},
{0xA7AE,0,0,-42308},
{0xA7B0,0,0,-42258},
{0xA7B1,0,0,-42282},
{0xA7B2,0,0,-42261},
{0xA7B3,0,0,928},
{0xA7B4,2,2,1},
{0xFF21,25,1,32},
{0x10400,39,1,40},
{0x104B0,35,1,40},
{0x10C80,50,1,64},
{0x118A0,31,1,32},
{0x1E900,33,1,34},
{0x1FFFFF,0,0,0}};

struct ccaseconv l2u[]={
{0x61,25,1,-32},
{0xB5,0,0,743},
{0xE0,22,1,-32},
{0xF8,6,1,-32},
{0xFF,0,0,121},
{0x101,46,2,-1},
{0x131,0,0,-232},
{0x133,4,2,-1},
{0x13A,14,2,-1},
{0x14B,44,2,-1},
{0x17A,4,2,-1},
{0x17F,0,0,-300},
{0x180,0,0,195},
{0x183,2,2,-1},
{0x188,0,0,-1},
{0x18C,0,0,-1},
{0x192,0,0,-1},
{0x195,0,0,97},
{0x199,0,0,-1},
{0x19A,0,0,163},
{0x19E,0,0,130},
{0x1A1,4,2,-1},
{0x1A8,0,0,-1},
{0x1AD,0,0,-1},
{0x1B0,0,0,-1},
{0x1B4,2,2,-1},
{0x1B9,0,0,-1},
{0x1BD,0,0,-1},
{0x1BF,0,0,56},
{0x1C5,0,0,-1},
{0x1C6,0,0,-2},
{0x1C8,0,0,-1},
{0x1C9,0,0,-2},
{0x1CB,0,0,-1},
{0x1CC,0,0,-2},
{0x1CE,14,2,-1},
{0x1DD,0,0,-79},
{0x1DF,16,2,-1},
{0x1F2,0,0,-1},
{0x1F3,0,0,-2},
{0x1F5,0,0,-1},
{0x1F9,38,2,-1},
{0x223,16,2,-1},
{0x23C,0,0,-1},
{0x23F,1,1,10815},
{0x242,0,0,-1},
{0x247,8,2,-1},
{0x250,0,0,10783},
{0x251,0,0,10780},
{0x252,0,0,10782},
{0x253,0,0,-210},
{0x254,0,0,-206},
{0x256,1,1,-205},
{0x259,0,0,-202},
{0x25B,0,0,-203},
{0x25C,0,0,42319},
{0x260,0,0,-205},
{0x261,0,0,42315},
{0x263,0,0,-207},
{0x265,0,0,42280},
{0x266,0,0,42308},
{0x268,0,0,-209},
{0x269,0,0,-211},
{0x26A,0,0,42308},
{0x26B,0,0,10743},
{0x26C,0,0,42305},
{0x26F,0,0,-211},
{0x271,0,0,10749},
{0x272,0,0,-213},
{0x275,0,0,-214},
{0x27D,0,0,10727},
{0x280,0,0,-218},
{0x283,0,0,-218},
{0x287,0,0,42282},
{0x288,0,0,-218},
{0x289,0,0,-69},
{0x28A,1,1,-217},
{0x28C,0,0,-71},
{0x292,0,0,-219},
{0x29D,0,0,42261},
{0x29E,0,0,42258},
{0x345,0,0,84},
{0x371,2,2,-1},
{0x377,0,0,-1},
{0x37B,2,1,130},
{0x3AC,0,0,-38},
{0x3AD,2,1,-37},
{0x3B1,16,1,-32},
{0x3C2,0,0,-31},
{0x3C3,8,1,-32},
{0x3CC,0,0,-64},
{0x3CD,1,1,-63},
{0x3D0,0,0,-62},
{0x3D1,0,0,-57},
{0x3D5,0,0,-47},
{0x3D6,0,0,-54},
{0x3D7,0,0,-8},
{0x3D9,22,2,-1},
{0x3F0,0,0,-86},
{0x3F1,0,0,-80},
{0x3F2,0,0,7},
{0x3F3,0,0,-116},
{0x3F5,0,0,-96},
{0x3F8,0,0,-1},
{0x3FB,0,0,-1},
{0x430,31,1,-32},
{0x450,15,1,-80},
{0x461,32,2,-1},
{0x48B,52,2,-1},
{0x4C2,12,2,-1},
{0x4CF,0,0,-15},
{0x4D1,94,2,-1},
{0x561,37,1,-48},
{0x13F8,5,1,-8},
{0x1C80,0,0,-6254},
{0x1C81,0,0,-6253},
{0x1C82,0,0,-6244},
{0x1C83,1,1,-6242},
{0x1C85,0,0,-6243},
{0x1C86,0,0,-6236},
{0x1C87,0,0,-6181},
{0x1C88,0,0,35266},
{0x1D79,0,0,35332},
{0x1D7D,0,0,3814},
{0x1E01,148,2,-1},
{0x1E9B,0,0,-59},
{0x1EA1,94,2,-1},
{0x1F00,7,1,8},
{0x1F10,5,1,8},
{0x1F20,7,1,8},
{0x1F30,7,1,8},
{0x1F40,5,1,8},
{0x1F51,6,2,8},
{0x1F60,7,1,8},
{0x1F70,1,1,74},
{0x1F72,3,1,86},
{0x1F76,1,1,100},
{0x1F78,1,1,128},
{0x1F7A,1,1,112},
{0x1F7C,1,1,126},
{0x1F80,7,1,8},
{0x1F90,7,1,8},
{0x1FA0,7,1,8},
{0x1FB0,1,1,8},
{0x1FB3,0,0,9},
{0x1FBE,0,0,-7205},
{0x1FC3,0,0,9},
{0x1FD0,1,1,8},
{0x1FE0,1,1,8},
{0x1FE5,0,0,7},
{0x1FF3,0,0,9},
{0x214E,0,0,-28},
{0x2170,15,1,-16},
{0x2184,0,0,-1},
{0x24D0,25,1,-26},
{0x2C30,46,1,-48},
{0x2C61,0,0,-1},
{0x2C65,0,0,-10795},
{0x2C66,0,0,-10792},
{0x2C68,4,2,-1},
{0x2C73,0,0,-1},
{0x2C76,0,0,-1},
{0x2C81,98,2,-1},
{0x2CEC,2,2,-1},
{0x2CF3,0,0,-1},
{0x2D00,36,1,-7264},
{0x2D25,2,2,-7264},
{0x2D2D,0,0,-7264},
{0xA641,44,2,-1},
{0xA681,26,2,-1},
{0xA723,12,2,-1},
{0xA733,60,2,-1},
{0xA77A,2,2,-1},
{0xA77F,8,2,-1},
{0xA78C,0,0,-1},
{0xA791,2,2,-1},
{0xA797,18,2,-1},
{0xA7B5,2,2,-1},
{0xAB53,0,0,-928},
{0xAB70,79,1,-38864},
{0xFF41,25,1,-32},
{0x10428,39,1,-40},
{0x104D8,35,1,-40},
{0x10CC0,50,1,-64},
{0x118C0,31,1,-32},
{0x1E922,33,1,-34},
{0x1FFFFF,0,0,0}};

#else
struct ccaseconv {unsigned int L:21;int range:11;unsigned int inc:2;int dif:20;};
struct ccaseconv l2u[]={
{0x61,25,1,-32},
{0xB5,0,0,743},
{0xE0,22,1,-32},
{0xF8,6,1,-32},
{0xFF,0,0,121},
{0x101,46,2,-1},
{0x131,0,0,-232},
{0x133,4,2,-1},
{0x13A,14,2,-1},
{0x14B,44,2,-1},
{0x17A,4,2,-1},
{0x17F,0,0,-300},
{0x180,0,0,195},
{0x183,2,2,-1},
{0x188,0,0,-1},
{0x18C,0,0,-1},
{0x192,0,0,-1},
{0x195,0,0,97},
{0x199,0,0,-1},
{0x19A,0,0,163},
{0x19E,0,0,130},
{0x1A1,4,2,-1},
{0x1A8,0,0,-1},
{0x1AD,0,0,-1},
{0x1B0,0,0,-1},
{0x1B4,2,2,-1},
{0x1B9,0,0,-1},
{0x1BD,0,0,-1},
{0x1BF,0,0,56},
{0x1C5,0,0,-1},
{0x1C6,0,0,-2},
{0x1C8,0,0,-1},
{0x1C9,0,0,-2},
{0x1CB,0,0,-1},
{0x1CC,0,0,-2},
{0x1CE,14,2,-1},
{0x1DD,0,0,-79},
{0x1DF,16,2,-1},
{0x1F2,0,0,-1},
{0x1F3,0,0,-2},
{0x1F5,0,0,-1},
{0x1F9,38,2,-1},
{0x223,16,2,-1},
{0x23C,0,0,-1},
{0x23F,1,1,10815},
{0x242,0,0,-1},
{0x247,8,2,-1},
{0x250,0,0,10783},
{0x251,0,0,10780},
{0x252,0,0,10782},
{0x253,0,0,-210},
{0x254,0,0,-206},
{0x256,1,1,-205},
{0x259,0,0,-202},
{0x25B,0,0,-203},
{0x25C,0,0,42319},
{0x260,0,0,-205},
{0x261,0,0,42315},
{0x263,0,0,-207},
{0x265,0,0,42280},
{0x266,0,0,42308},
{0x268,0,0,-209},
{0x269,0,0,-211},
{0x26B,0,0,10743},
{0x26C,0,0,42305},
{0x26F,0,0,-211},
{0x271,0,0,10749},
{0x272,0,0,-213},
{0x275,0,0,-214},
{0x27D,0,0,10727},
{0x280,0,0,-218},
{0x283,0,0,-218},
{0x287,0,0,42282},
{0x288,0,0,-218},
{0x289,0,0,-69},
{0x28A,1,1,-217},
{0x28C,0,0,-71},
{0x292,0,0,-219},
{0x29D,0,0,42261},
{0x29E,0,0,42258},
{0x345,0,0,84},
{0x371,2,2,-1},
{0x377,0,0,-1},
{0x37B,2,1,130},
{0x3AC,0,0,-38},
{0x3AD,2,1,-37},
{0x3B1,16,1,-32},
{0x3C2,0,0,-31},
{0x3C3,8,1,-32},
{0x3CC,0,0,-64},
{0x3CD,1,1,-63},
{0x3D0,0,0,-62},
{0x3D1,0,0,-57},
{0x3D5,0,0,-47},
{0x3D6,0,0,-54},
{0x3D7,0,0,-8},
{0x3D9,22,2,-1},
{0x3F0,0,0,-86},
{0x3F1,0,0,-80},
{0x3F2,0,0,7},
{0x3F3,0,0,-116},
{0x3F5,0,0,-96},
{0x3F8,0,0,-1},
{0x3FB,0,0,-1},
{0x430,31,1,-32},
{0x450,15,1,-80},
{0x461,32,2,-1},
{0x48B,52,2,-1},
{0x4C2,12,2,-1},
{0x4CF,0,0,-15},
{0x4D1,94,2,-1},
{0x561,37,1,-48},
{0x13F8,5,1,-8},
{0x1D79,0,0,35332},
{0x1D7D,0,0,3814},
{0x1E01,148,2,-1},
{0x1E9B,0,0,-59},
{0x1EA1,94,2,-1},
{0x1F00,7,1,8},
{0x1F10,5,1,8},
{0x1F20,7,1,8},
{0x1F30,7,1,8},
{0x1F40,5,1,8},
{0x1F51,6,2,8},
{0x1F60,7,1,8},
{0x1F70,1,1,74},
{0x1F72,3,1,86},
{0x1F76,1,1,100},
{0x1F78,1,1,128},
{0x1F7A,1,1,112},
{0x1F7C,1,1,126},
{0x1F80,7,1,8},
{0x1F90,7,1,8},
{0x1FA0,7,1,8},
{0x1FB0,1,1,8},
{0x1FB3,0,0,9},
{0x1FBE,0,0,-7205},
{0x1FC3,0,0,9},
{0x1FD0,1,1,8},
{0x1FE0,1,1,8},
{0x1FE5,0,0,7},
{0x1FF3,0,0,9},
{0x214E,0,0,-28},
{0x2170,15,1,-16},
{0x2184,0,0,-1},
{0x24D0,25,1,-26},
{0x2C30,46,1,-48},
{0x2C61,0,0,-1},
{0x2C65,0,0,-10795},
{0x2C66,0,0,-10792},
{0x2C68,4,2,-1},
{0x2C73,0,0,-1},
{0x2C76,0,0,-1},
{0x2C81,98,2,-1},
{0x2CEC,2,2,-1},
{0x2CF3,0,0,-1},
{0x2D00,36,1,-7264},
{0x2D25,2,2,-7264},
{0x2D2D,0,0,-7264},
{0xA641,44,2,-1},
{0xA681,26,2,-1},
{0xA723,12,2,-1},
{0xA733,60,2,-1},
{0xA77A,2,2,-1},
{0xA77F,8,2,-1},
{0xA78C,0,0,-1},
{0xA791,2,2,-1},
{0xA797,18,2,-1},
{0xA7B5,2,2,-1},
{0xAB53,0,0,-928},
{0xAB70,79,1,-38864},
{0xFF41,25,1,-32},
{0x10428,39,1,-40},
{0x10CC0,50,1,-64},
{0x118C0,31,1,-32},
{0x1FFFFF,0,0,0}};

struct ccaseconv u2l[]={
{0x41,25,1,32},
{0xC0,22,1,32},
{0xD8,6,1,32},
{0x100,46,2,1},
{0x130,0,0,-199},
{0x132,4,2,1},
{0x139,14,2,1},
{0x14A,44,2,1},
{0x178,0,0,-121},
{0x179,4,2,1},
{0x181,0,0,210},
{0x182,2,2,1},
{0x186,0,0,206},
{0x187,0,0,1},
{0x189,1,1,205},
{0x18B,0,0,1},
{0x18E,0,0,79},
{0x18F,0,0,202},
{0x190,0,0,203},
{0x191,0,0,1},
{0x193,0,0,205},
{0x194,0,0,207},
{0x196,0,0,211},
{0x197,0,0,209},
{0x198,0,0,1},
{0x19C,0,0,211},
{0x19D,0,0,213},
{0x19F,0,0,214},
{0x1A0,4,2,1},
{0x1A6,0,0,218},
{0x1A7,0,0,1},
{0x1A9,0,0,218},
{0x1AC,0,0,1},
{0x1AE,0,0,218},
{0x1AF,0,0,1},
{0x1B1,1,1,217},
{0x1B3,2,2,1},
{0x1B7,0,0,219},
{0x1B8,0,0,1},
{0x1BC,0,0,1},
{0x1C4,0,0,2},
{0x1C5,0,0,1},
{0x1C7,0,0,2},
{0x1C8,0,0,1},
{0x1CA,0,0,2},
{0x1CB,16,2,1},
{0x1DE,16,2,1},
{0x1F1,0,0,2},
{0x1F2,2,2,1},
{0x1F6,0,0,-97},
{0x1F7,0,0,-56},
{0x1F8,38,2,1},
{0x220,0,0,-130},
{0x222,16,2,1},
{0x23A,0,0,10795},
{0x23B,0,0,1},
{0x23D,0,0,-163},
{0x23E,0,0,10792},
{0x241,0,0,1},
{0x243,0,0,-195},
{0x244,0,0,69},
{0x245,0,0,71},
{0x246,8,2,1},
{0x370,2,2,1},
{0x376,0,0,1},
{0x37F,0,0,116},
{0x386,0,0,38},
{0x388,2,1,37},
{0x38C,0,0,64},
{0x38E,1,1,63},
{0x391,16,1,32},
{0x3A3,8,1,32},
{0x3CF,0,0,8},
{0x3D8,22,2,1},
{0x3F4,0,0,-60},
{0x3F7,0,0,1},
{0x3F9,0,0,-7},
{0x3FA,0,0,1},
{0x3FD,2,1,-130},
{0x400,15,1,80},
{0x410,31,1,32},
{0x460,32,2,1},
{0x48A,52,2,1},
{0x4C0,0,0,15},
{0x4C1,12,2,1},
{0x4D0,94,2,1},
{0x531,37,1,48},
{0x10A0,36,1,7264},
{0x10C5,2,2,7264},
{0x10CD,0,0,7264},
{0x13A0,79,1,38864},
{0x13F0,5,1,8},
{0x1E00,148,2,1},
{0x1E9E,0,0,-7615},
{0x1EA0,94,2,1},
{0x1F08,7,1,-8},
{0x1F18,5,1,-8},
{0x1F28,7,1,-8},
{0x1F38,7,1,-8},
{0x1F48,5,1,-8},
{0x1F59,6,2,-8},
{0x1F68,7,1,-8},
{0x1F88,7,1,-8},
{0x1F98,7,1,-8},
{0x1FA8,7,1,-8},
{0x1FB8,1,1,-8},
{0x1FBA,1,1,-74},
{0x1FBC,0,0,-9},
{0x1FC8,3,1,-86},
{0x1FCC,0,0,-9},
{0x1FD8,1,1,-8},
{0x1FDA,1,1,-100},
{0x1FE8,1,1,-8},
{0x1FEA,1,1,-112},
{0x1FEC,0,0,-7},
{0x1FF8,1,1,-128},
{0x1FFA,1,1,-126},
{0x1FFC,0,0,-9},
{0x2126,0,0,-7517},
{0x212A,0,0,-8383},
{0x212B,0,0,-8262},
{0x2132,0,0,28},
{0x2160,15,1,16},
{0x2183,0,0,1},
{0x24B6,25,1,26},
{0x2C00,46,1,48},
{0x2C60,0,0,1},
{0x2C62,0,0,-10743},
{0x2C63,0,0,-3814},
{0x2C64,0,0,-10727},
{0x2C67,4,2,1},
{0x2C6D,0,0,-10780},
{0x2C6E,0,0,-10749},
{0x2C6F,0,0,-10783},
{0x2C70,0,0,-10782},
{0x2C72,0,0,1},
{0x2C75,0,0,1},
{0x2C7E,1,1,-10815},
{0x2C80,98,2,1},
{0x2CEB,2,2,1},
{0x2CF2,0,0,1},
{0xA640,44,2,1},
{0xA680,26,2,1},
{0xA722,12,2,1},
{0xA732,60,2,1},
{0xA779,2,2,1},
{0xA77D,0,0,-35332},
{0xA77E,8,2,1},
{0xA78B,0,0,1},
{0xA78D,0,0,-42280},
{0xA790,2,2,1},
{0xA796,18,2,1},
{0xA7AA,0,0,-42308},
{0xA7AB,0,0,-42319},
{0xA7AC,0,0,-42315},
{0xA7AD,0,0,-42305},
{0xA7B0,0,0,-42258},
{0xA7B1,0,0,-42282},
{0xA7B2,0,0,-42261},
{0xA7B3,0,0,928},
{0xA7B4,2,2,1},
{0xFF21,25,1,32},
{0x10400,39,1,40},
{0x10C80,50,1,64},
{0x118A0,31,1,32},
{0x1FFFFF,0,0,0}};
#endif

static int convertLetter(int a,struct ccaseconv * T)
    {
    int i;
    if(a > 0x10FFFF)
        return a;
    for(i=0;;++i)
        {
        if((unsigned int)a < T[i].L)
            {
            if(i == 0)
                return a;
            --i;
            if  (   (unsigned int)a <= T[i].L+T[i].range
                &&  (   T[i].inc < 2
                    ||  !((a - T[i].L) & 1)
                    )
                )
                {
                return a + T[i].dif;
                }
            else
                {
                break;
                }
            }
        }
    return a;
    }
/*
static int toUpperUnicode(int a)
    {
    return convertLetter(a,l2u);
    }
*/
static int toLowerUnicode(int a)
    {
    return convertLetter(a,u2l);
    }

static const char quote[256] = {
/*
   1 : quote if first character;
   3 : quote always
   4 : quote if \t and \n must be expanded
*/
0,0,0,0,0,0,0,0,0,4,4,0,0,0,3,3, /* \L \D */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
3,1,0,1,3,1,3,3,3,3,3,3,3,1,3,1, /* SP ! # $ % & ' ( ) * + , - . / */
0,0,0,0,0,0,0,0,0,0,3,3,1,3,1,1, /* : < = > ? */
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* @ */
0,0,0,0,0,0,0,0,0,0,0,1,3,1,3,3, /* [ \ ] ^ _ */
1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ` */
0,0,0,0,0,0,0,0,0,0,0,3,3,3,1,0};/* { | } ~ */

/*#define LATIN_1*/
#ifdef LATIN_1 /* ISO8859 */ /* NOT DOS compatible! */
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

static psk global_anker;

typedef struct inputBuffer
    {
    unsigned char * buffer;
    unsigned int cutoff:1;    /* Set to true if very long string does not fit in buffer of size DEFAULT_INPUT_BUFFER_SIZE */
    unsigned int mallocallocated:1; /* True if allocated with malloc. Otherwise on stack (except EPOC). */
    } inputBuffer;

static inputBuffer * InputArray;
static inputBuffer * InputElement; /* Points to member of InputArray */
static unsigned char *start,**pstart,*bron;
static unsigned char * wijzer;
static unsigned char * maxwijzer; /* wijzer <= maxwijzer,
                            if wijzer == maxwijzer, don't assign to *wijzer */


/* FUNCTIONS */

static void hreslt(psk wortel,int nivo,int ind,int space);
#if DEBUGBRACMAT
static void hreslts(psk wortel,int nivo,int ind,int space,psk snijaf);
#endif

static psk eval(psk pkn);
#define evalueer(x) \
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

static psk subboomcopie(psk src);

#if _BRACMATEMBEDDED

static int (*WinIn)(void) = NULL;
static void (*WinOut)(int c) = NULL;
static void (*WinFlush)(void) = NULL;
#if defined PYTHONINTERFACE
static void (*Ni)(const char *) = NULL;
static const char * (*Nii)(const char *) = NULL;
#endif
static int mygetc(FILE * fpi)
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
        fputc(c,global_fpo);
    }
#else
static void myputc(int c)
    {
    fputc(c, global_fpo);
    }

static int mygetc(FILE * fpi)
    {
#ifdef __SYMBIAN32__
    if(fpi == stdin)
        {
        static unsigned char inputbuffer[256] = {0};
        static unsigned char * out = inputbuffer;
        if(!*out)
            {
            static unsigned char * in = inputbuffer;
            static int kar;
            while(  in < inputbuffer + sizeof(inputbuffer) - 2
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

static void (*verwerk)(int c) = myputc;

static void myprintf(char *string,...)
{
char *i,*j;
va_list ap;
va_start(ap,string);
i = string;
while(i)
    {
    for(j = i;*j;j++)
        (*verwerk)(*j);
    i = va_arg(ap,char *);
    }
va_end(ap);
}


#if _BRACMATEMBEDDED && !defined _MT
static int Printf(const char *fmt, ...)
    {
    char buffer[1000];
    int ret;
    va_list ap;
    va_start(ap,fmt);
    ret = vsprintf(buffer,fmt,ap);
    myprintf(buffer,NULL);
    va_end(ap);
    return ret;
    }
#else
#define Printf printf
#endif

static int errorprintf(const char *fmt, ...)
    {
    char buffer[1000];
    int ret;
    FILE * save = global_fpo;
    va_list ap;
    va_start(ap,fmt);
    ret = vsprintf(buffer,fmt,ap);
    global_fpo = errorStream;
#if !defined NO_FOPEN
    if(global_fpo == NULL && errorFileName != NULL)
        global_fpo = fopen(errorFileName,APPENDTXT);
#endif
/*#endif*/
    if(global_fpo)
        myprintf(buffer,NULL);
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
    void * lowestAddress;
    void * highestAddress;
    void * firstFreeElementBetweenAddresses; /* if NULL : no more free elements */
    struct memblock * previousOfSameLength; /* address of older ands smaller block with same sized elements */ 
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
    struct memblock * memoryBlock;
    };

static struct allocation * global_allocations;
static int global_nallocations = 0;


struct memoryElement
    {
    void * next;
    };

struct pointerStruct
    {
    struct pointerStruct * lp;
    } *global_p, *global_ep;

struct memblock ** pMemBlocks = 0; /* list of memblock, sorted
                                      according to memory address */
static int NumberOfMemBlocks = 0;
#if TELMAX
static int malloced = 0;
#endif

#if DOSUMCHECK

static int LineNo;
static int N;

static int getchecksum(void)
    {
    int i;
    int sum = 0;
    for(i = 0; i < NumberOfMemBlocks;++i)
        {
        struct memoryElement * me;
        struct memblock * mb = pMemBlocks[i];
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

static void setChecksum(int lineno,int n)
    {
    if(lineno)
        {
        LineNo = lineno;
        N = n;
        }
    Checksum = getchecksum();
    }

static void checksum(int line)
    {
    static int nChecksum = 0;
    nChecksum = getchecksum();
    if(Checksum && Checksum != nChecksum)
        {
        Printf("Line %d: Illegal write after bmalloc(%d) on line %d",line,N,LineNo);
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
static int isFree(void * p)
    {
    LONG * q;
    int i;
    struct memoryElement * I;
    q = (LONG *)p - 2;
    I = (struct memoryElement *) q;
    for(i = 0; i < NumberOfMemBlocks;++i)
        {
        struct memoryElement * me;
        struct memblock * mb = pMemBlocks[i];
        me = (struct memoryElement *) mb->firstFreeElementBetweenAddresses;
        while(me)
            {
            if(I == me)
                return 1;
            me = (struct memoryElement *) me->next;
            }
        }
    return 0;
    }

static void result(psk wortel);
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

static int areFree(char * t,psk p)
    {
    if(rfree(p))
        {
        POINT = 1;
        printf("%s:areFree(",t);
        result(p);
        POINT = 0;
        printf("\n");
        return 1;
        }
    return 0;
    }

static void checkMem(void * p)
    {
    LONG * q;
    q = (LONG *)p - 2;
    if(q[0] == ('s'<<24)+('t'<<16)+('a'<<8)+('r')
        && q[q[1]] == ('t'<<24)+('e'<<16)+('n'<<8)+('d')
        )
        {
        ;
        }
    else
        {
        char * s = (char *)q;
        printf("s:[");
        for(;s < (char *)(q + q[1] + 1);++s)
            {
            if((((int)s) % 4) == 0)
                printf("|");
            if(' ' <= *s && *s <= 127)
                printf(" %c",*s);
            else
                printf("%.2x",(int)((unsigned char)*s));
            }
        printf("] %p\n",p);
        }
    assert(q[0] == ('s'<<24)+('t'<<16)+('a'<<8)+('r'));
    assert(q[q[1]] == ('t'<<24)+('e'<<16)+('n'<<8)+('d'));
    }


static void checkBounds(void * p)
    {
    struct memblock ** q;
    LONG * lp = (LONG *)p;
    assert(p != 0);
    checkMem(p);
    lp = lp - 2;
    p = lp;
    assert(lp[0] == ('s'<<24)+('t'<<16)+('a'<<8)+('r'));
    assert(lp[lp[1]] == ('t'<<24)+('e'<<16)+('n'<<8)+('d'));
    for(q = pMemBlocks+NumberOfMemBlocks; --q >= pMemBlocks ;)
        {
        size_t stepSize = (*q)->sizeOfElement / sizeof(struct pointerStruct);
        if((*q)->lowestAddress <= p && p < (*q)->highestAddress)
            {
            assert(lp[stepSize - 1] == ('t'<<24)+('e'<<16)+('n'<<8)+('d'));
            return;
            }
        }
    }

static void checkAllBounds()
    {
    struct memblock ** q;
    for(q = pMemBlocks+NumberOfMemBlocks; --q >= pMemBlocks ;)
        {
        size_t stepSize = (*q)->sizeOfElement / sizeof(struct pointerStruct);

        struct pointerStruct * p = (struct pointerStruct *)(*q)->lowestAddress;
        struct pointerStruct * e = (struct pointerStruct *)(*q)->highestAddress;
        size_t L = (*q)->sizeOfElement - 1;
        struct pointerStruct * x;
        for(x = p;x < e;x += stepSize)
            {
            struct pointerStruct * a = ((struct memoryElement *)x)->next;
            if(a == 0 || (p <= a && a < e))
                ;
            else
                {
                if((((LONG *)x)[0] == ('s'<<24)+('t'<<16)+('a'<<8)+('r'))
                    &&(((LONG *)x)[stepSize - 1] == ('t'<<24)+('e'<<16)+('n'<<8)+('d')))
                    ;
                else
                    {
                    char * s = (char *)x;
                    printf("s:[");
                    for(;s <= (char *)x + L;++s)
                        if(' ' <= *s && *s <= 127)
                            printf("%c",*s);
                        else if(*s == 0)
                            printf("NIL");
                        else
                            printf("-",*s);
                    printf("] %p\n",x);
                    }
                assert(((LONG *)x)[0] == ('s'<<24)+('t'<<16)+('a'<<8)+('r'));
                assert(((LONG *)x)[stepSize - 1] == ('t'<<24)+('e'<<16)+('n'<<8)+('d'));
                }
            }
        }
    }
#endif

static void bfree(void *p)
    {
    struct memblock ** q;
#if CHECKALLOCBOUNDS
    LONG * lp = (LONG *)p;
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
    for(q = pMemBlocks+NumberOfMemBlocks; --q >= pMemBlocks ;)
        {
        if((*q)->lowestAddress <= p && p < (*q)->highestAddress)
            {
#if TELMAX
            ++((*q)->numberOfFreeElementsBetweenAddresses);
#endif
            ((struct memoryElement *)p)->next = (*q)->firstFreeElementBetweenAddresses;
            (*q)->firstFreeElementBetweenAddresses = p;
            setChecksum(LineNo,N);
            return;
            }
        }
    free(p);
#if TELMAX
    --malloced;
#endif
    setChecksum(LineNo,N);
    }

#if TELLING
static void bezetting(void)
    {
    struct memblock * mb = 0;
    size_t words = 0;
    int i;
    Printf("\noccupied (promilles)\n");
    for(i = 0; i < NumberOfMemBlocks;++i)
        {
        mb = pMemBlocks[i];
        Printf("%d word : %lu\n",mb->sizeOfElement/sizeof(struct pointerStruct),1000UL-(1000UL * mb->numberOfFreeElementsBetweenAddresses)/mb->numberOfElementsBetweenAddresses);
        }
    Printf("\nmax occupied (promilles)\n");
    for(i = 0; i < NumberOfMemBlocks;++i)
        {
        mb = pMemBlocks[i];
        Printf("%d word : %lu\n",mb->sizeOfElement/sizeof(struct pointerStruct),1000UL-(1000UL * mb->minimumNumberOfFreeElementsBetweenAddresses)/mb->numberOfElementsBetweenAddresses);
        }
    Printf("\noccupied (absolute)\n");
    for(i = 0; i < NumberOfMemBlocks;++i)
        {
        mb = pMemBlocks[i];
        words = mb->sizeOfElement/sizeof(struct pointerStruct);
        Printf("%d word : %lu\n",words,(mb->numberOfElementsBetweenAddresses - mb->numberOfFreeElementsBetweenAddresses));
        }
    Printf("more than %d words : %lu\n",words,malloced);
    }
#endif

static struct memblock * initializeMemBlock(size_t elementSize, size_t numberOfElements)
    {
    size_t nlongpointers;
    size_t stepSize;
    struct memblock * mb;
    mb = (struct memblock *)malloc(sizeof(struct memblock));
    mb->sizeOfElement = elementSize;
    mb->previousOfSameLength = 0;
    stepSize = elementSize / sizeof(struct pointerStruct);
    nlongpointers = elementSize * numberOfElements / sizeof(struct pointerStruct);
    mb->firstFreeElementBetweenAddresses = mb->lowestAddress = malloc(sizeof(struct pointerStruct) * nlongpointers);
#if TELMAX
    mb->numberOfElementsBetweenAddresses = numberOfElements;
#endif
    if(mb->lowestAddress == 0)
        {
#if _BRACMATEMBEDDED
        return 0;
#else
        exit(-1);
#endif
        }
    global_p = (struct pointerStruct *)mb->lowestAddress;
    global_ep = global_p + nlongpointers;
    mb->highestAddress = (void*)global_ep;
#if TELMAX
    mb->numberOfFreeElementsBetweenAddresses = numberOfElements;
#endif
    global_ep -= stepSize;
    for (
        ; global_p < global_ep
        ; global_p = global_p->lp
        )
        global_p->lp = global_p+stepSize;
    global_p->lp = 0;
    return mb;
    }
/*
static void showMemBlocks()
    {
    int totalbytes;
    int i;
    for(i = 0;i < NumberOfMemBlocks;++i)
        {
        printf  ("%p %d %p <= %p <= %p [%p] %lu\n"
            ,pMemBlocks[i]
        ,i
            ,pMemBlocks[i]->lowestAddress
            ,pMemBlocks[i]->firstFreeElementBetweenAddresses
            ,pMemBlocks[i]->highestAddress
            ,pMemBlocks[i]->previousOfSameLength
            ,pMemBlocks[i]->sizeOfElement
            );
        }
    totalbytes = 0;
    for(i = 0;i < nallocations;++i)
        {
        printf  ("%d %d %lu\n"
            ,i+1
            ,allocations[i].numberOfElements
            ,allocations[i].numberOfElements * (i+1) * sizeof(struct pointerStruct)
            );
        totalbytes += allocations[i].numberOfElements * (i+1) * sizeof(struct pointerStruct);
        }
    printf("total bytes = %d\n",totalbytes);
    }
*/

/* The newMemBlocks function is introduced because the same code,
if in-line in bmalloc, and if compiled with -O3, doesn't run. */
static struct memblock * newMemBlocks(size_t n)
    {
    struct memblock * mb;
    int i,j = 0;
    struct memblock ** npMemBlocks;
    mb = initializeMemBlock(global_allocations[n].elementSize, global_allocations[n].numberOfElements);
    if(!mb)
        return 0;
    global_allocations[n].numberOfElements *= 2;
    mb->previousOfSameLength = global_allocations[n].memoryBlock;
    global_allocations[n].memoryBlock = mb;

    ++NumberOfMemBlocks;
    npMemBlocks = (struct memblock **)malloc((NumberOfMemBlocks)*sizeof(struct memblock *));
    for(i = 0;i < NumberOfMemBlocks - 1;++i)
        {
        if(mb < pMemBlocks[i])
            {
            npMemBlocks[j++] = mb;
            for(;i < NumberOfMemBlocks - 1;++i)
                {
                npMemBlocks[j++] = pMemBlocks[i];
                }
            free(pMemBlocks);
            pMemBlocks = npMemBlocks;
            /** /
            showMemBlocks();
            / **/
            return mb;
            }
        npMemBlocks[j++] = pMemBlocks[i];
        }
    npMemBlocks[j] = mb;
    free(pMemBlocks);
    pMemBlocks = npMemBlocks;
    /** /
    showMemBlocks();
    / **/
    return mb;
    }


static void * bmalloc(int lineno,size_t n)
    {
    void * ret;
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
    n = (n - 1) / sizeof(struct pointerStruct);
    if  (n <
#if _5_6
             6
#elif _4
             4
#else
             3
#endif
        )
        {
        struct memblock * mb = global_allocations[n].memoryBlock;
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
            mb->firstFreeElementBetweenAddresses = ((struct memoryElement *)mb->firstFreeElementBetweenAddresses)->next;
            /** /
            memset(ret,0,(n+1) * sizeof(struct pointerStruct));
            / **/
            ((LONG*)ret)[n] = 0;
            ((LONG*)ret)[0] = 0;
            setChecksum(lineno,nn);
#if CHECKALLOCBOUNDS
            ((LONG*)ret)[n-1] = 0;
            ((LONG*)ret)[2] = 0;
            ((LONG*)ret)[1] = n;
            ((LONG*)ret)[n] = ('t'<<24)+('e'<<16)+('n'<<8)+('d');
            ((LONG*)ret)[0] = ('s'<<24)+('t'<<16)+('a'<<8)+('r');
            return (void *)(((LONG*)ret) + 2);
#else
            return ret;
#endif
            }
        }
    ret = malloc((n+1) * sizeof(struct pointerStruct));
    if(!ret)
        {
#if TELLING
        errorprintf(
        "MEMORY FULL AFTER %lu ALLOCATIONS WITH MEAN LENGTH %lu\n",
            globalloc,totcnt/alloc_cnt);
        for(tel = 0;tel<16;tel++)
            {
            int tel1;
            for(tel1 = 0;tel1<256;tel1 += 16)
                errorprintf("%lu ",(cnts[tel+tel1]*1000UL+500UL)/alloc_cnt);
            errorprintf("\n");
            }
        bezetting();
#endif
        errorprintf(
            "memory full (requested block of %d bytes could not be allocated)",
            (n<<2)+4);

        exit(1);
        }

#if TELMAX
    ++malloced;
#endif
    ((LONG*)ret)[n] = 0;
    ((LONG*)ret)[0] = 0;
    setChecksum(lineno,n);
#if CHECKALLOCBOUNDS
    ((LONG*)ret)[n-1] = 0;
    ((LONG*)ret)[2] = 0;
    ((LONG*)ret)[1] = n;
    ((LONG*)ret)[n] = ('t'<<24)+('e'<<16)+('n'<<8)+('d');
    ((LONG*)ret)[0] = ('s'<<24)+('t'<<16)+('a'<<8)+('r');
    return (void *)(((LONG*)ret) + 2);
#else
    return ret;
#endif
    }

int addAllocation(size_t size,int number,int nallocations,struct allocation * allocations)
    {
    int i;
    for(i = 0;i < nallocations;++i)
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

int memblocksort(const void * a, const void * b)
    {
    struct memblock * A = *(struct memblock **)a;
    struct memblock * B = *(struct memblock **)b;
    if(A->lowestAddress < B->lowestAddress)
        return -1;
    return 1;
    }

static int init_ruimte(void)
    {
    int i;
    global_allocations = (struct allocation *)malloc( sizeof(struct allocation)
                        *
#if _5_6
                            6
#elif _4
                            4
#else
                            3
#endif
                        );
    global_nallocations = addAllocation(1*sizeof(struct pointerStruct),MEM1SIZE,0, global_allocations);
    global_nallocations = addAllocation(2*sizeof(struct pointerStruct),MEM2SIZE, global_nallocations, global_allocations);
    global_nallocations = addAllocation(3*sizeof(struct pointerStruct),MEM3SIZE, global_nallocations, global_allocations);
#if _4
    global_nallocations = addAllocation(4*sizeof(struct pointerStruct),MEM4SIZE, global_nallocations, global_allocations);
#endif
#if _5_6
    global_nallocations = addAllocation(5*sizeof(struct pointerStruct),MEM5SIZE, global_nallocations, global_allocations);
    global_nallocations = addAllocation(6*sizeof(struct pointerStruct),MEM6SIZE, global_nallocations, global_allocations);
#endif
    NumberOfMemBlocks = global_nallocations;
    pMemBlocks = (struct memblock **)malloc(NumberOfMemBlocks*sizeof(struct memblock *));

    for(i = 0;i < NumberOfMemBlocks;++i)
        {
        pMemBlocks[i] = global_allocations[i].memoryBlock = initializeMemBlock(global_allocations[i].elementSize, global_allocations[i].numberOfElements);
        }
    qsort(pMemBlocks,NumberOfMemBlocks,sizeof(struct memblock *),memblocksort);
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

static void pskfree(psk p)
    {
    bfree(p);
    }

#if defined DEBUGBRACMAT
static void result(psk wortel);
#endif

static psk new_operator_like(psk kn)
    {
    if(kop(kn) == EQUALS)
        {
        DBGSRC(printf("new_operator_like:");result(kn);printf("\n");)
        assert(!ISBUILTIN((objectknoop*)kn));
        objectknoop * goal = (objectknoop *)bmalloc(__LINE__,sizeof(objectknoop));
#ifdef BUILTIN
        goal->u.Int = 0;
#else
        goal->refcount = 0;
        UNSETCREATEDWITHNEW(goal);
        UNSETBUILTIN(goal);
#endif
        return (psk)goal;
        }
    else
        return (psk)bmalloc(__LINE__,sizeof(kknoop));
    }

static unsigned char *shift_nw(VOIDORARGPTR)
/* Used from startboom_w and opb */
    {
    if(startPos)
        {
#if GLOBALARGPTR
        startPos = va_arg(argptr,unsigned char *);
#else
        startPos = va_arg(*pargptr,unsigned char *);
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
    inputBuffer * nextInputElement = InputElement + 1;
    inputBuffer * next2;
    unsigned char * bigBuffer;
    size_t len;
    while(nextInputElement->cutoff)
        ++nextInputElement;

    len = (nextInputElement - InputElement) * (DEFAULT_INPUT_BUFFER_SIZE - 1) + 1;

    if(nextInputElement->buffer)
        {
        len += strlen((const char *)nextInputElement->buffer);
        }

    bigBuffer = (unsigned char *)bmalloc(__LINE__,len);

    nextInputElement = InputElement;

    while(nextInputElement->cutoff)
        {
        strncpy((char *)bigBuffer + (nextInputElement - InputElement)*(DEFAULT_INPUT_BUFFER_SIZE - 1),(char *)nextInputElement->buffer,DEFAULT_INPUT_BUFFER_SIZE - 1);
        bfree(nextInputElement->buffer);
        ++nextInputElement;
        }

    if(nextInputElement->buffer)
        {
        strcpy((char *)bigBuffer + (nextInputElement - InputElement)*(DEFAULT_INPUT_BUFFER_SIZE - 1),(char *)nextInputElement->buffer);
        if(nextInputElement->mallocallocated)
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

    for(next2 = InputElement + 1;nextInputElement->buffer;++next2,++nextInputElement)
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
/* used from bouwboom_w, which receives a list of bmalloc-allocated string
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

static unsigned char *vshift_nw(VOIDORARGPTR)
/* Used from vopb */
    {
    if(*pstart && *++pstart)
        start = *pstart;
    return start;
    }

static unsigned char *(*shift)(VOIDORARGPTR) = shift_nw;

static void tel(int c)
    {
    UNREFERENCED_PARAMETER(c);
    telling++;
    }

static void tstr(int c)
    {
    static int esc = FALSE,str = FALSE;
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
    static int esc = FALSE,str = FALSE;
    if(esc)
        {
        esc = FALSE;
        switch(c)
            {
            case 'n' :
                c = '\n';
                break;
            case 'f' :
                c = '\f';
                break;
            case 'r' :
                c = '\r';
                break;
            case 'b' :
                c = '\b';
                break;
            case 'a' :
                c = ALERT;
                break;
            case 'v' :
                c = '\v';
                break;
            case 't' :
                c = '\t';
                break;
            case 'L' :
                c = 016;
                break;
            case 'D' :
                c = 017;
                break;
            }
        *bron++ = (char)c;
        }
    else if(c == '\\')
        esc = TRUE;
    else if(str)
        {
        if(c == '"')
            str = FALSE;
        else
            *bron++ = (char)c;
        }
    else if(c == '"')
        str = TRUE;
    else if(c != ' ')
        *bron++ = (char)c;
    }

static void plak(int c)
    {
    *bron++ = (char)c;
    }

#define COMPLEX_MAX 80
#define LINELENGTH 80

static size_t complexiteit(psk wortel,size_t max)
    {
    static int ouder,kind;
    while(is_op(wortel))
        {
        switch(kop(wortel))
            {
            case OR :
            case AND :
                max += COMPLEX_MAX/5;
                break;
            case EQUALS :
            case MATCH :
                max += COMPLEX_MAX/10;
                break;
            case DOT :
            case COMMA :
            case WHITE :
                switch(kop(wortel->LEFT))
                    {
                    case DOT:
                    case COMMA:
                    case WHITE :
                        max += COMPLEX_MAX/10;
                        break;
                    default:
                        max += COMPLEX_MAX/LINELENGTH;
                    }
                break;
            default:
                max += COMPLEX_MAX/LINELENGTH;
            }
        ouder = kop(wortel);
        kind = kop(wortel->LEFT);
        if(HAS__UNOPS(wortel->LEFT) || ouder >= kind)
            max += (2 * COMPLEX_MAX)/LINELENGTH; /* 2 parentheses */

        kind = kop(wortel->RIGHT);
        if(HAS__UNOPS(wortel->RIGHT) || ouder > kind || (ouder == kind && ouder > TIMES))
            max += (2 * COMPLEX_MAX)/LINELENGTH; /* 2 parentheses */

        if(max > COMPLEX_MAX)
            return max;
        max = complexiteit(wortel->LEFT,max);
        wortel = wortel->RIGHT;
        }
    if(!is_op(wortel))
        max += (COMPLEX_MAX*strlen((char *)POBJ(wortel))) / LINELENGTH;
    return max;
    }

static int indtel = 0,extraspatie = 0,number_of_flags_on_node=0;

static int indent(psk wortel,int nivo,int ind)
    {
    if(hum)
        {
        if(ind > 0 || (ind == 0 && complexiteit(wortel,2*nivo) > COMPLEX_MAX))
            {  /*    blanks that start a line    */
            int p;
            (*verwerk)('\n');
            for(p = 2*nivo+number_of_flags_on_node;p;p--)
                (*verwerk)(' ');
            ind = TRUE;
            }
        else
            {  /* blanks after an operator or parenthesis */
            for(indtel = extraspatie + 2*indtel;indtel;indtel--)
                (*verwerk)(' ');
            ind = FALSE;
            }
        extraspatie = 0;
        }
    return ind;
    }

static int moetIndent(psk wortel,int ind,int nivo)
    {
    return hum && !ind && complexiteit(wortel,2*nivo) > COMPLEX_MAX;
    }

static void bewerk(int c)
    {
    if(c == 016 || c == 017)
        {
        (*verwerk)('\\');
        (*verwerk)(c == 016 ? 'L' : 'D');
        }
    else
        (*verwerk)(c);
    }

static int lineToLong(unsigned char *string)
    {
    if(  hum
      && strlen((const char *)string) > 10 /*LINELENGTH*/
      /* very short strings are allowed to keep \n and \t */
      )
        return TRUE;
    return FALSE;
    }

static int haalaan(unsigned char *string)
    {
    unsigned char *pstring;
    if(quote[*string] & 1)
        return TRUE;
    for(pstring = string;*pstring;pstring++)
        if(quote[*pstring] & 2)
            return TRUE;
        else if(  quote[*pstring] & 4
            && lineToLong(string)
            )
            return TRUE;
    return FALSE;
    }

static int printflags(psk wortel)
    {
    int count = 0;
    int Flgs = wortel->v.fl;
    if(Flgs & FENCE)
        {
        (*verwerk)('`');
        ++count;
        }
    if(Flgs & POSITION)
        {
        (*verwerk)('[');
        ++count;
        }
    if(Flgs & NOT)
        {
        (*verwerk)('~');
        ++count;
        }
    if(Flgs & BREUK)
        {
        (*verwerk)('/');
        ++count;
        }
    if(Flgs & NUMBER)
        {
        (*verwerk)('#');
        ++count;
        }
    if(Flgs & SMALLER_THAN)
        {
        (*verwerk)('<');
        ++count;
        }
    if(Flgs & GREATER_THAN)
        {
        (*verwerk)('>');
        ++count;
        }
    if(Flgs & NONIDENT)
        {
        (*verwerk)('%');
        ++count;
        }
    if(Flgs & ATOM)
        {
        (*verwerk)('@');
        ++count;
        }
    if(Flgs & UNIFY)
        {
        (*verwerk)('?');
        ++count;
        }
    if(Flgs & INDIRECT)
        {
        (*verwerk)('!');
        ++count;
        }
    if(Flgs & DOUBLY_INDIRECT)
        {
        (*verwerk)('!');
        ++count;
        }
    return count;
    }

#define LHS 1
#define RHS 2

static void eindknoop(psk wortel,int space)
    {
    unsigned char *pstring;
    int q,ikar;
#if CHECKALLOCBOUNDS
    if(POINT)
        printf("\n[%p %d]",wortel,(wortel->ops & ALL_REFCOUNT_BITS_SET)/ ONE);
#endif
    if(!wortel->u.obj
        && !HAS_UNOPS(wortel)
        && space)
        {
        (*verwerk)('(');
        (*verwerk)(')');
        return;
        }
    printflags(wortel);
    if(wortel->ops & MINUS)
        (*verwerk)('-');
    if(mooi)
        {
        for(pstring = POBJ(wortel);*pstring;pstring++)
            bewerk(*pstring);
        }
    else
        {
        Boolean longline = FALSE;
        if((q = haalaan(POBJ(wortel))) == TRUE)
            (*verwerk)('"');
        for(pstring = POBJ(wortel);(ikar = *pstring) != 0;pstring++)
            {
            switch(ikar)
                {
                case '\n' :
                    if(longline || lineToLong(POBJ(wortel)))
                    /* We need to call this, even though haalaan returned TRUE,
                    because haalaan may have returned before reaching this character.
                    */
                        {
                        longline = TRUE;
                        (*verwerk)('\n');
                        continue;
                        }
                    ikar = 'n';
                    break;
                case '\f' :
                    ikar = 'f';
                    break;
                case '\r' :
                    ikar = 'r';
                    break;
                case '\b' :
                    ikar = 'b';
                    break;
                case ALERT :
                    ikar = 'a';
                    break;
                case '\v' :
                    ikar = 'v';
                    break;
                case '\t' :
                    if(longline || lineToLong(POBJ(wortel)))
                    /* We need to call this, even though haalaan returned TRUE,
                    because haalaan may have returned before reaching this character.
                    */
                        {
                        longline = TRUE;
                        (*verwerk)('\t');
                        continue;
                        }
                    ikar = 't';
                    break;
                case '"' :
                case '\\' :
                    break;
                case 016 :
                    ikar = 'L';
                    break;
                case 017 :
                    ikar = 'D';
                    break;
                default :
                    (*verwerk)(ikar);
                    continue;
                }
            (*verwerk)('\\');
            (*verwerk)(ikar);
            }
        if(q)
            (*verwerk)('"');
        }
    }

static psk zelfde_als_w(psk kn)
    {
    if(shared(kn) != ALL_REFCOUNT_BITS_SET)
        {
        (kn)->ops += ONE;
        return kn;
        }
    else if(is_object(kn))
        {
        INCREFCOUNT(kn);
        return kn;
        }
    else
        {
        return subboomcopie(kn);
        }
    }

static psk zelfde_als_w_2(ppsk pkn)
    {
    psk kn = *pkn;
    if(shared(kn) != ALL_REFCOUNT_BITS_SET)
        {
        kn->ops += ONE;
        return kn;
        }
    else if(is_object(kn))
        {
        INCREFCOUNT(kn);
        return kn;
        }
    else
        {
        /*
        0:?n&:?L&whl'(!n+1:?n:<10000&out$!n&XXX !L:?L)
        0:?n&:?L&whl'(!n+1:?n:<10000&out$!n&!L XXX:?L) This is not improved!
        */
        *pkn = subboomcopie(kn);
        return kn;
        }
    }

#if ICPY
static void icpy(LONG *d,LONG *b,int words)
    {
    while(words--)
        *d++ = *b++;
    }
#endif

static psk icopievan(psk kn)
    {
    /* REQUIREMENTS : After the string delimiting 0 all remaining bytes in the
	current computer word must be 0 as well.
    Argument must start on a word boundary. */
    psk ret;
    size_t len;
    len = sizeof(unsigned LONG)+strlen((char *)POBJ(kn));
    ret = (psk)bmalloc(__LINE__,len+1);
#if ICPY
    MEMCPY(ret,kn,(len >> LOGWORDLENGTH) + 1);
#else
    MEMCPY(ret,kn,((len / sizeof(LONG)) + 1) * sizeof(LONG));
#endif
    ret->ops &= ~ALL_REFCOUNT_BITS_SET;
    return ret;
    }

static void wis(psk top);

static void copyToSnijaf(psk * ppknoop,psk pknoop,psk snijaf)
    {
    for(;;)
        {
        if(is_op(pknoop))
            {
            if(pknoop->RIGHT == snijaf)
                {
                *ppknoop = zelfde_als_w(pknoop->LEFT);
                break;
                }
            else
                {
                psk p = new_operator_like(pknoop);
                p->ops = pknoop->ops & ~ALL_REFCOUNT_BITS_SET;
                p->LEFT = zelfde_als_w(pknoop->LEFT);
                *ppknoop = p;
                ppknoop = &(p->RIGHT);
                pknoop = pknoop->RIGHT;
                }
            }
        else
            {
            *ppknoop = icopievan(pknoop);
            break;
            }
        }
    }

static psk Head(psk pknoop)
{
if(pknoop->ops & LATEBIND)
    {
    assert(!shared(pknoop));
    if(is_op(pknoop))
        {
        psk root = pknoop;
        copyToSnijaf(&pknoop,root->LEFT,root->RIGHT);
        wis(root);
        }
    else
        {
        stringrefknoop * ps = (stringrefknoop *)pknoop;
        pknoop = (psk)bmalloc(__LINE__,sizeof(unsigned LONG) + 1 + ps->length);
        pknoop->ops = (ps->ops & ~ALL_REFCOUNT_BITS_SET & ~LATEBIND);
        strncpy((char *)(pknoop)+sizeof(unsigned LONG),(char *)ps->str,ps->length);
        wis(ps->kn);
        bfree(ps);
        }
    }
return pknoop;
}

#define RSP (ouder == WHITE ? RHS : 0)
#define LSP (ouder == WHITE ? LHS : 0)

#ifndef reslt
static void reslt(psk wortel,int nivo,int ind,int space)
{
static int ouder,kind,newind;
while(is_op(wortel))
    {
    if(kop(wortel) == EQUALS)
        wortel->RIGHT = Head(wortel->RIGHT);
    ouder = kop(wortel);
    kind = kop(wortel->LEFT);
    if(moetIndent(wortel,ind,nivo))
        indtel++;
    if(HAS__UNOPS(wortel->LEFT) || ouder >= kind)
        hreslt(wortel->LEFT,nivo+1,FALSE,(space & LHS) | RSP);
    else
        reslt(wortel->LEFT,nivo+1,FALSE,(space & LHS) | RSP);
    newind = indent(wortel,nivo,ind);
    if(newind)
        extraspatie = 1;
#if CHECKALLOCBOUNDS
    if(POINT)
        printf("\n[%p %d]",wortel,(wortel->ops & ALL_REFCOUNT_BITS_SET)/ ONE);
#endif
    bewerk(opchar[klopcode(wortel)]);
    ouder = kop(wortel);
    kind = kop(wortel->RIGHT);
    if(HAS__UNOPS(wortel->RIGHT) || ouder > kind || (ouder == kind && ouder > TIMES))
        {
        hreslt(wortel->RIGHT,nivo+1,FALSE,LSP | (space & RHS));
        return;
        }
    else if(ouder < kind)
        {
        reslt(wortel->RIGHT,nivo+1,FALSE,LSP | (space & RHS));
        return;
        }
    else if(newind != ind || ((LSP | (space & RHS)) != space))
        {
        reslt(wortel->RIGHT,nivo,newind,LSP | (space & RHS));
        return;
        }
    wortel = wortel->RIGHT;
    }
indent(wortel,nivo,-1);
eindknoop(wortel,space);
}

#if DEBUGBRACMAT

static void reslts(psk wortel,int nivo,int ind,int space,psk snijaf)
    {
    static int ouder,kind,newind;
    if(is_op(wortel))
        {
        if(kop(wortel) == EQUALS)
            wortel->RIGHT = Head(wortel->RIGHT);

        do
            {
            if(snijaf && wortel->RIGHT == snijaf)
                {
                reslt(wortel->LEFT,nivo,ind,space);
                return;
                }
            ouder = kop(wortel);
             kind = kop(wortel->LEFT);
            if(moetIndent(wortel,ind,nivo))
                indtel++;
            if(HAS__UNOPS(wortel->LEFT) || ouder >= kind)
                hreslt(wortel->LEFT,nivo+1,FALSE,(space & LHS) | RSP);
            else
                reslt(wortel->LEFT,nivo+1,FALSE,(space & LHS) | RSP);
            newind = indent(wortel,nivo,ind);
            if(newind)
                extraspatie = 1;
            bewerk(opchar[klopcode(wortel)]);
            ouder = kop(wortel);
            kind = kop(wortel->RIGHT);
            if(HAS__UNOPS(wortel->RIGHT) || ouder > kind || (ouder == kind && ouder > TIMES))
                hreslts(wortel->RIGHT,nivo+1,FALSE,LSP | (space & RHS),snijaf);
            else if(ouder < kind)
                {
                reslts(wortel->RIGHT,nivo+1,FALSE,LSP | (space & RHS),snijaf);
                return;
                }
            else if(newind != ind || ((LSP | (space & RHS)) != space))
                {
                reslts(wortel->RIGHT,nivo,newind,LSP | (space & RHS),snijaf);
                return;
                }
            wortel = wortel->RIGHT;
            }
        while(is_op(wortel));
        }
    else
        {
        indent(wortel,nivo,-1);
        eindknoop(wortel,space);
        }
    }
#endif /* DEBUGBRACMAT */
#endif

static void hreslt(psk wortel,int nivo,int ind,int space)
{
static int ouder,kind;
if(is_op(wortel))
    {
    int number_of_flags;
    if(kop(wortel) == EQUALS)
        wortel->RIGHT = Head(wortel->RIGHT);
    indent(wortel,nivo,-1);
    number_of_flags = printflags(wortel);
    number_of_flags_on_node += number_of_flags;
    (*verwerk)('(');
    indtel = 0;
    if(moetIndent(wortel,ind,nivo))
        extraspatie = 1;
    ouder = kop(wortel);
    kind = kop(wortel->LEFT);
    if(HAS__UNOPS(wortel->LEFT) || ouder >= kind)
        hreslt(wortel->LEFT,nivo+1,FALSE,RSP);
    else
        reslt(wortel->LEFT,nivo+1,FALSE,RSP);
    ind = indent(wortel,nivo,ind);
    if(ind)
        extraspatie = 1;
#if CHECKALLOCBOUNDS
    if(POINT)
        printf("\n[%p %d]",wortel,(wortel->ops & ALL_REFCOUNT_BITS_SET)/ ONE);
#endif
    bewerk(opchar[klopcode(wortel)]);
    ouder = kop(wortel);

    kind = kop(wortel->RIGHT);
    if(HAS__UNOPS(wortel->RIGHT) || ouder > kind || (ouder == kind && ouder > TIMES))
        hreslt(wortel->RIGHT,nivo+1,FALSE,LSP);
    else if(ouder < kind)
        reslt(wortel->RIGHT,nivo+1,FALSE,LSP);
    else
        reslt(wortel->RIGHT,nivo,ind,LSP);
    indent(wortel,nivo,FALSE);
    (*verwerk)(')');
    number_of_flags_on_node -= number_of_flags;
    }
else
    {
    indent(wortel,nivo,-1);
    eindknoop(wortel,space);
    }
}

static void result(psk wortel)
{
if(HAS__UNOPS(wortel))
    {
    hreslt(wortel,0,FALSE,0);
    }
else
    reslt(wortel,0,FALSE,0);
}

#if 1
#define testMul(a,b,c,d) 
#else
#if CHECKALLOCBOUNDS
static int testMul(char * txt,psk variabele,psk pbinding,int doit)
    {
    if(  doit 
      || is_op(pbinding) && kop(pbinding) == TIMES && pbinding->LEFT->u.obj == 'a'
      )
        {
        POINT = 1;
        printf("%s ",txt);
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
static void hreslts(psk wortel,int nivo,int ind,int space,psk snijaf)
{
static int ouder,kind;
if(is_op(wortel))
    {
    int number_of_flags;
    if(kop(wortel) == EQUALS)
        wortel->RIGHT = Head(wortel->RIGHT);
    if(snijaf && wortel->RIGHT == snijaf)
        {
        hreslt(wortel->LEFT,nivo,ind,space);
        return;
        }
    indent(wortel,nivo,-1);
    number_of_flags = printflags(wortel);
    number_of_flags_on_node += number_of_flags;
    (*verwerk)('(');
    indtel = 0;
    if(moetIndent(wortel,ind,nivo))
        extraspatie = 1;
    ouder = kop(wortel);
    kind = kop(wortel->LEFT);
    if(HAS__UNOPS(wortel->LEFT) || ouder >= kind)
        hreslt(wortel->LEFT,nivo+1,FALSE,RSP);
    else
        reslt(wortel->LEFT,nivo+1,FALSE,RSP);
    ind = indent(wortel,nivo,ind);
    if(ind)
        extraspatie = 1;
    bewerk(opchar[klopcode(wortel)]);
    ouder = kop(wortel);
    kind = kop(wortel->RIGHT);
    if(HAS__UNOPS(wortel->RIGHT) || ouder > kind || (ouder == kind && ouder > TIMES))
        hreslts(wortel->RIGHT,nivo+1,FALSE,LSP,snijaf);
    else if(ouder < kind)
        reslts(wortel->RIGHT,nivo+1,FALSE,LSP,snijaf);
    else
        reslts(wortel->RIGHT,nivo,ind,LSP,snijaf);
    indent(wortel,nivo,FALSE);
    (*verwerk)(')');
    number_of_flags_on_node -= number_of_flags;
    }
else
    {
    indent(wortel,nivo,-1);
    eindknoop(wortel,space);
    }
}

static void results(psk wortel,psk snijaf)
{
if(HAS__UNOPS(wortel))
    {
    hreslts(wortel,0,FALSE,0,snijaf);
    }
else
    reslts(wortel,0,FALSE,0,snijaf);
}
#endif

static LONG toLong(psk kn)
    {
    LONG res;
    res = (LONG)STRTOUL((char *)POBJ(kn),(char **)NULL,10);
    if(kn->ops & MINUS)
        res = -res;
    return res;
    }

static int numbercheck(const char *begin)
    {
    int op_of_0,check;
    int needNonZeroDigit = FALSE;
    if(!*begin)
        return 0;
    check = QGETAL;
    op_of_0 = *begin;

    if(op_of_0 >= '0' && op_of_0 <= '9')
        {
        if(op_of_0 == '0')
            check |= QNUL;
        while(optab[op_of_0 = *++begin] != -1)
            {
            if(op_of_0 == '/')
                {
                /* check &= ~QNUL;*/
                if(check & QBREUK)
                    {
                    check = DEFINITELYNONUMBER;
                    break;
                    }
                else
                    {
                    needNonZeroDigit = TRUE;
                    check |= QBREUK;
                    }
                }
            else if(op_of_0 < '0' || op_of_0 > '9')
                {
                check = DEFINITELYNONUMBER;
                break;
                }
            else
                {
                /* initial zero followed by
                                 0 <= k <= 9 makes no number */
                if((check & (QNUL|QBREUK)) == QNUL)
                    {
                    check = DEFINITELYNONUMBER;
                    break;
                    }
                else if(op_of_0 != '0')
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
        if(op_of_0 == ')') /* "2)"+3       @("-23/4)))))":-23/4)  */
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

static int fullnumbercheck(const char *begin)
    {
    if(*begin == '-')
        {
        int ret = numbercheck(begin+1);
        if(ret & ~DEFINITELYNONUMBER)
            return ret | MINUS;
        else
            return ret;
        }
    else
        return numbercheck(begin);
    }

static int sfullnumbercheck(char *begin,char * snijaf)
    {
    unsigned char sav = *snijaf;
    int ret;
    *snijaf = '\0';
    ret = fullnumbercheck(begin);
    *snijaf = sav;
    return ret;
    }

static int flags(void)
{
int Flgs = 0;

for(;;start++)
    {
    switch(*start)
        {
        case '!' :
            if(Flgs & INDIRECT)
                Flgs |= DOUBLY_INDIRECT;
            else
                Flgs |= INDIRECT;
            continue;
        case '[' :
            Flgs |= POSITION;
            continue;
        case '?' :
            Flgs |= UNIFY;
            continue;
        case '#' :
            Flgs |= NUMBER;
            continue;
        case '/' :
            Flgs |= BREUK;
            continue;
        case '@' :
            Flgs |= ATOM;
            continue;
        case '`' :
            Flgs |= FENCE;
            continue;
        case '%' :
            Flgs |= NONIDENT;
            continue;
        case '~' :
            Flgs ^= NOT;
            continue;
        case '<' :
            Flgs |= SMALLER_THAN;
            continue;
        case '>' :
            Flgs |= GREATER_THAN;
            continue;
        case '-' :
            Flgs ^= MINUS;
            continue;
        }
    break;
    }

if((Flgs & NOT) && (Flgs < ATOM))
    Flgs ^= SUCCESS;
return Flgs;
}

#define flags(OPSFLGS) flags()


#define atoom(FLGS,OPSFLGS) atoom(FLGS)

static psk atoom(int Flgs,int opsflgs)
    {
    unsigned char *begin,*eind;
    size_t af = 0;
    psk pkn;
    begin = start;

    while(optab[*start] == NOOP)
        if(*start++ == 0x7F)
            af++;

    eind = start;
    pkn = (psk)bmalloc(__LINE__,sizeof(unsigned LONG) + 1 + (size_t)(eind - begin) - af);
    start = begin;
    begin = POBJ(pkn);
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
        (pkn)->v.fl = Flgs ^ SUCCESS;
        }
    else
        {
        if(ONTKENNING(Flgs,NUMBER))
            (pkn)->v.fl = (Flgs ^ (READY|SUCCESS));
        else
            (pkn)->v.fl = (Flgs ^ (READY|SUCCESS)) | (numbercheck(SPOBJ(pkn)) & ~DEFINITELYNONUMBER);
        }
#undef opsflgs
    return pkn;
    }

#if GLOBALARGPTR
#define lex(NXT,GRENS,FLGS,OPSFLGS) lex(NXT,GRENS,FLGS)
#else
#define lex(NXT,GRENS,FLGS,OPSFLGS,PVA_LIST) lex(NXT,GRENS,FLGS,PVA_LIST)
#endif

#if GLOBALARGPTR
static psk lex(int * nxt,int priority,int Flags,int opsflgs)
#else
static psk lex(int * nxt,int priority,int Flgs,int opsflgs,va_list * pargptr)
#endif
/* *nxt (if nxt != 0) is set to the character following the expression. */
    {
    int op_of_0;
    psk pkn;
    if(*start > 0 && *start <= '\6')
        pkn = zelfde_als_w(adr[*start++]);
    else
        {
        int Flgs;
        Flgs = flags(&locopsflgs);
        if(*start == '(')
            {
            if(*++start == 0)
#if GLOBALARGPTR
                (*shift)();
            pkn = lex(NULL,0,Flgs,locopsflgs);
#else
                (*shift)(pargptr);
            pkn = lex(NULL,0,Flgs,locopsflgs,pargptr);
#endif
            }
        else
            pkn = atoom(Flgs,locopsflgs);
        }

    if(*start == 0)
        {
#if GLOBALARGPTR
        if(!*(*shift)())
#else
        if(!*(*shift)(pargptr))
#endif
            return /*0*/pkn;
        }

    op_of_0 = *start;

    if(*++start == 0)
#if GLOBALARGPTR
        (*shift)();
#else
        (*shift)(pargptr);
#endif

    if(optab[op_of_0] == NOOP) /* 20080910 Otherwise problem with the k in ()k */
        errorprintf("malformed input\n");
    else
        {
        Flags &= ~MINUS;/* 20110831 Bitwise, operators cannot have the - flag. */
        do
            {
            /* op_of_0 == an operator */
            psk operatorNode;
            int child_op_of_0;
            if(optab[op_of_0] < priority) /* 'op_of_0' has too low priority */
                {
#if STRINGMATCH_CAN_BE_NEGATED
                if(  (Flgs & (NOT|FILTERS)) == (NOT|ATOM)
                  && kop(*pkn) == MATCH
                  ) /* 20071229 Undo setting of
                        success == FALSE
                       if ~@ flags are attached to : operator
                       Notice that op_of_0 is ')'
                       This is a special case. In ~@(a:b) the ~ operator must
                       not negate the @ but the result of the string match.
                    */
                    {
                    Flgs ^= SUCCESS;
                    }
#endif
                (pkn)->v.fl ^= Flags; /*19970821*/
                if(nxt)
                    *nxt = op_of_0;
                return pkn;
                }
            if(optab[op_of_0] == EQUALS)
                {
                operatorNode = (psk)bmalloc(__LINE__,sizeof(objectknoop));
        /*        ((objectknoop*)psk)->refcount = 0; done by bmalloc */
                }
            else
                operatorNode = (psk)bmalloc(__LINE__,sizeof(kknoop));
            assert(optab[op_of_0] != NOOP);
            assert(optab[op_of_0] >= 0);
            operatorNode->v.fl = optab[op_of_0] | SUCCESS;
            /*operatorNode->v.fl ^= Flgs;*/
            operatorNode->LEFT = pkn;
            pkn = operatorNode;/* 'op_of_0' has sufficient priority */
            if(optab[op_of_0] == priority) /* 'op_of_0' has same priority */
                {
                (pkn)->v.fl ^= Flags; /*19970821*/
                operatorNode->RIGHT = NULL;
                if(nxt)
                    *nxt = op_of_0;
                return pkn;
                }
            for(;;)
                {
                child_op_of_0 = 0;
                assert(optab[op_of_0] >= 0);
#if GLOBALARGPTR
                operatorNode->RIGHT = lex(&child_op_of_0,optab[op_of_0],0,0);
#else
                operatorNode->RIGHT = lex(&child_op_of_0,optab[op_of_0],0,0,pargptr);
#endif
                if(child_op_of_0 != op_of_0)
                    break;
                operatorNode = operatorNode->RIGHT;
                }
            op_of_0 = child_op_of_0;
            }
        while(op_of_0 != 0);
        }
    (pkn)->v.fl ^= Flags; /*19970821*/
    return /*0*/pkn;
    }

static psk bouwboom_w(psk pkn)
    {
    if(pkn)
        wis(pkn);
    InputElement = InputArray;
    if(InputElement->cutoff)
        {
        combineInputBuffers();
        }
    start = InputElement->buffer;
    shift = vshift_w;
#if GLOBALARGPTR
    pkn = lex(NULL,0,0,0);
#else
    pkn = lex(NULL,0,0,0,0);
#endif
    shift = shift_nw;
    if((--InputElement)->mallocallocated)
        {
        bfree(InputElement->buffer);
        }
    bfree(InputArray);
    return pkn;
    }

static void lput(int c)
    {
    if(wijzer >= maxwijzer)
        {
        inputBuffer * newInputArray;
        unsigned char * lijst;
        unsigned char * dest;
        int len;
    size_t L;

        for(len = 0;InputArray[++len].buffer;)
            ;
        /* len = index of last element in InputArray array */

        lijst = InputArray[len - 1].buffer;
        /* The last string (probably on the stack, not on the heap) */

        while(wijzer > lijst && optab[*--wijzer] == NOOP)
            ;
        /* wijzer points at last operator (where string can be split) or at
           the start of the string. */

        newInputArray = (inputBuffer *)bmalloc(__LINE__,(2 + len) * sizeof(inputBuffer));
        /* allocate new array one element bigger than the previous. */

        newInputArray[len + 1].buffer = NULL;
        newInputArray[len + 1].cutoff = FALSE;
        newInputArray[len + 1].mallocallocated = FALSE;
        newInputArray[len].buffer = lijst;
    /*The buffer pointers with lower index are copied further down.*/

        /*Printf("lijst %p\n",lijst);*/

        newInputArray[len].cutoff = FALSE;
        newInputArray[len].mallocallocated = FALSE;
        /*The active buffer is still the one declared in input(),
      so on the stack (except under EPOC).*/
        --len; /* point at the second last element, the one that got filled up. */
        if(wijzer == lijst)
            {
            /* copy the full content of lijst to the second last element */
            dest = newInputArray[len].buffer = (unsigned char *)bmalloc(__LINE__,DEFAULT_INPUT_BUFFER_SIZE);
            strncpy((char *)dest,(char *)lijst,DEFAULT_INPUT_BUFFER_SIZE - 1);
        dest[DEFAULT_INPUT_BUFFER_SIZE - 1] = '\0';
            /* Make a notice that the element's string is cut-off */
            newInputArray[len].cutoff = TRUE;
            newInputArray[len].mallocallocated = TRUE;
            }
        else
            {
            ++wijzer; /* wijzer points at first character after the operator */
            /* maxwijzer - wijzer >= 0 */
        L = (size_t)(wijzer - lijst);
            dest = newInputArray[len].buffer = (unsigned char *)bmalloc(__LINE__,L + 1);
            strncpy((char *)dest,(char *)lijst,L);
            dest[L] = '\0';
            newInputArray[len].cutoff = FALSE;
            newInputArray[len].mallocallocated = TRUE;

            /* Now remove the substring up to wijzer from lijst */
        L = (size_t)(maxwijzer - wijzer);
            strncpy((char *)lijst,(char *)wijzer,L);
        lijst[L] = '\0';
            wijzer = lijst + L;
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
    assert(wijzer <= maxwijzer);
    *wijzer++ = (unsigned char)c;
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

void writeError(psk pkn)
    {
    FILE *redfpo;
    int redMooi;
    redMooi = mooi;
    mooi = FALSE;
    redfpo = global_fpo;
    global_fpo = errorStream;
#if !defined NO_FOPEN
    if(global_fpo == NULL && errorFileName != NULL)
        global_fpo = fopen(errorFileName,APPENDBIN);
#endif
    if(global_fpo)
        {
        result(pkn);
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
    global_fpo = redfpo;
    mooi = redMooi;
    }

/*#if !_BRACMATEMBEDDED*/
static int redirectError(char * name)
    {
#if !defined NO_FOPEN
    if(errorFileName)
        {
        free(errorFileName);
        errorFileName = NULL;
        }
#endif
    if(!strcmp(name,"stdout"))
        {
        errorStream = stdout;
        return TRUE;
        }
    else if(!strcmp(name,"stderr"))
        {
        errorStream = stderr;
        return TRUE;
        }
    else
        {
#if !defined NO_FOPEN
        errorStream = fopen(name,APPENDTXT);
        if(errorStream)
            {
            fclose(errorStream);
            errorStream = NULL;
            errorFileName = (char *)malloc(strlen(name)+1);
            strcpy(errorFileName,name);
            return TRUE;
            }
#endif
        errorStream = stderr;
        }
    return FALSE;
    }
/*#endif*/

static void politelyWriteError(psk pkn)
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
        int i,ikar;
        Printf("\nType name of file to write erroneous expression to and then press <return>\n(Type nothing: screen, type dot '.': skip): ");
        for(i = 0;i < 255 && (ikar = mygetc(stdin)) != '\n';)
            {
            name[i++] = (unsigned char)ikar;
            }
        name[i] = '\0';
        if(name[0] && name[0] != '.')
            redirectError((char *)name);
        }
#endif
#endif
    if(name[0] != '.')
        writeError(pkn);
    }

static psk input(FILE * fpi,psk pkn,int echmemvapstrmltrm,Boolean * err,Boolean * GoOn)
    {
    static int stdinEOF = FALSE;
    int braces,ikar,hasop,whiteSpaceSeen,escape,backslashesAreEscaped,inString,parentheses,error;
#ifdef __SYMBIAN32__
    unsigned char * lijst;
    lijst = bmalloc(__LINE__,DEFAULT_INPUT_BUFFER_SIZE);
#else
    unsigned char lijst[DEFAULT_INPUT_BUFFER_SIZE];
#endif
    if((fpi == stdin) && (stdinEOF == TRUE))
        exit(0);
    maxwijzer = lijst + (DEFAULT_INPUT_BUFFER_SIZE - 1);/* there must be room  for terminating 0 */
    /* Array of pointers to inputbuffers. Initially 2 elements,
       large enough for small inputs (< DEFAULT_INPUT_BUFFER_SIZE)*/
    InputArray = (inputBuffer *)bmalloc(__LINE__,2*sizeof(inputBuffer));
    InputArray[0].buffer = lijst;
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
        wijzer = lijst;
        XMLtext(fpi,(char*)bron,(echmemvapstrmltrm & OPT_TRM),(echmemvapstrmltrm & OPT_HT),(echmemvapstrmltrm & OPT_X));
        *wijzer = 0;
        pkn = bouwboom_w(pkn);
        if(err) *err = error;
#ifdef __SYMBIAN32__
        bfree(lijst);
#endif
        if(GoOn)
            *GoOn = FALSE;
        return pkn;
        }
    else
#endif
#if READJSON
    if(echmemvapstrmltrm & OPT_JSON)
        {
        wijzer = lijst;
        error = JSONtext(fpi,(char*)bron);
        *wijzer = 0;
        pkn = bouwboom_w(pkn);
        if(err) *err = error;
#ifdef __SYMBIAN32__
        bfree(lijst);
#endif
        if(GoOn)
            *GoOn = FALSE;
        return pkn;
        }
    else
#endif
        if(echmemvapstrmltrm & (OPT_VAP|OPT_STR))
        {
        for(wijzer = lijst;;)
            {
            if(fpi)
                {
                ikar = mygetc(fpi);
                if  (ikar == EOF)
                    {
                    if(fpi == stdin)
                        stdinEOF = TRUE;
                    break;
                    }
                if  (  (fpi == stdin) 
                    && (ikar == '\n')
                    )
                    break;
                }
            else
                if((ikar = *bron++) == 0)
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
        *wijzer = 0;
        pkn = bouwboom_w(pkn);
        if(err) *err = error;
#ifdef __SYMBIAN32__
        bfree(lijst);
#endif
        if(GoOn)
            *GoOn = FALSE;
        return pkn;
        }
    for( wijzer = lijst
       ;    
#if _BRACMATEMBEDDED
            !error
         &&
#endif
            (ikar = fpi ? mygetc(fpi) : *bron++) != EOF
         && ikar
         && parentheses >= 0
       ;
       )
        {
        if(echmemvapstrmltrm & OPT_ECH)
            {
            if(fpi != stdin)
                Printf("%c",ikar);
            if(ikar == '\n')
                {
                if(braces)
                    Printf("{com} ");
                else if(inString)
                    Printf("{str} ");
                else if(parentheses > 0 || fpi != stdin)
                    {
                    int tel;
                    Printf("{%d} ",parentheses);
                    if(fpi == stdin)
                        for(tel = parentheses;tel;tel--)
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
                    case 'n' :
                        ikar = '\n' | 0x80;
                        break;
                    case 'f' :
                        ikar = '\f' | 0x80;
                        break;
                    case 'r' :
                        ikar = '\r' | 0x80;
                        break;
                    case 'b' :
                        ikar = '\b' | 0x80;
                        break;
                    case 'a' :
                        ikar = ALERT | 0x80;
                        break;
                    case 'v' :
                        ikar = '\v' | 0x80;
                        break;
                    case 't' :
                        ikar = '\t' | 0x80;
                        break;
                    case '"' :
                        ikar = '"' | 0x80;
                        break;
                    case 'L' :
                        ikar = 016;
                        break;
                    case 'D' :
                        ikar = 017;
                        break;
                    default:
                        ikar = ikar | 0x80;
                    }
                }
            else if(  ikar == '\\' 
                   && (  backslashesAreEscaped
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
                    case '{' :
                        braces = 1;
                        break;
                    case '}' :
                        *wijzer = 0;
                        errorprintf(
                        "\n%s brace }",
                            unbalanced);
                        error = TRUE;
                        break;
                    default :
                        {
                        if(  optab[ikar] == WHITE
                          && (  ikar != '\n' 
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
                                case ';' :
                                    if(parentheses)
                                        {
                                        *wijzer = 0;
                                        errorprintf("\n%d %s \"(\"",parentheses,unbalanced);
                                        error = TRUE;
                                        }
                                    if(echmemvapstrmltrm & OPT_ECH)
                                        Printf("\n");
                                    /* fall through */
                                case '\n':
                                    /* You get here only directly if fpi==stdin */
                                    *wijzer = 0;
                                    pkn = bouwboom_w(pkn);
                                    if(error)
                                        politelyWriteError(pkn);
                                    if(err) *err = error;
#ifdef __SYMBIAN32__
                                    bfree(lijst);
#endif
                                    if(GoOn)
                                        *GoOn = ikar == ';' && !error;
                                    return pkn;
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

                                    if(  whiteSpaceSeen
                                      && !hasop
                                      && optab[ikar] == NOOP
                                      )
                                        lput(' ');

                                    whiteSpaceSeen = FALSE;
                                    hasop =
                                        (  (ikar == '(')
                                        || (  (optab[ikar] < NOOP)
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
    *wijzer = 0;
#if _BRACMATEMBEDDED
    if(!error)
#endif
        {
        if(inString)
            {
            errorprintf("\n%s \"",unbalanced);
            error = TRUE;
            /*exit(1);*/
            }
        if(braces)
            {
            errorprintf("\n%d %s \"{\"",braces,unbalanced);
            error = TRUE;
            /*exit(1);*/
            }
        if(parentheses > 0)
            {
            errorprintf("\n%d %s \"(\"",parentheses,unbalanced);
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
                Printf(
                "\nend session? (y/n)"
                );
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
                errorprintf("\n%d %s \")\"",-parentheses,unbalanced);
                error = TRUE;
                /*exit(1);*/
                }
            }
        if(echmemvapstrmltrm & OPT_ECH)
            Printf("\n");
        if(*InputArray[0].buffer)
            {
            pkn = bouwboom_w(pkn);
            if(error)
                {
                politelyWriteError(pkn);
                }
            }
        else
            {
            bfree(InputArray);
            }
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
    bfree(lijst);
    #endif
    if(GoOn)
        *GoOn = FALSE;
    return pkn;
    }

#if JMP
#include <setjmp.h>
static jmp_buf jumper;
#endif

void stringEval(const char *s,const char ** out,int * err)
    {
#if _BRACMATEMBEDDED
    char * buf = (char *)malloc(strlen(s) + 11);
    sprintf(buf,"put$(%s,MEM)",s);
#else
    char * buf = (char *)malloc(strlen(s) + 7);
    sprintf(buf,"str$(%s)",s);
#endif
    bron = (unsigned char *)buf;
    global_anker = input(NULL, global_anker,OPT_MEM,err,NULL); /* 4 -> OPT_MEM*/
    if(err && *err)
        return;
#if JMP
    if(setjmp(jumper) != 0)
        {
        free(buf);
        return -1;
        }
#endif
    global_anker = eval(global_anker);
    if(out != NULL)
        *out = is_op(global_anker) ? (const char *)"" : (const char *)POBJ(global_anker);
    free(buf);
    return;
    }

static psk copievan(psk kn)
    {
    psk res;
    res = icopievan(kn);
    res->v.fl &= ~IDENT;
    return res;
    }

static psk _copyop(psk pkn)
    {
    psk hulp;
    hulp = new_operator_like(pkn);
    hulp->ops = pkn->ops & ~ALL_REFCOUNT_BITS_SET;
    hulp->LEFT = zelfde_als_w_2(&pkn->LEFT);
    hulp->RIGHT = zelfde_als_w(pkn->RIGHT);
    return hulp;
    }

static psk copyop(psk pkn)
    {
    dec_refcount(pkn);
    return _copyop(pkn);
    }

static psk subboomcopie(psk src)
    {
    if(is_op(src))
        return _copyop(src);
    else
        return icopievan(src);
    }

static int getal_graad(psk kn)
    {
    if(RATIONAAL_COMP(kn))
        return 4;
    switch(PLOBJ(kn))
        {
        case IM: return 3;
        case PI: return 2;
        case XX: return 1;
        default: return 0;
        }
    }

static int is_constant(psk kn)
    {
    while(is_op(kn))
        {
        if(!is_constant(kn->LEFT))
            return FALSE;
        kn = kn->RIGHT;
        }
    return getal_graad(kn);
    }

static void init_opcode(void)
    {
    int tel;
    for(tel = 0;tel<256;tel++)
        {
#if TELLING
        cnts[tel] = 0;
#endif
        switch (tel)
            {
            case 0   :
            case ')' : optab[tel] = -1   ;break;
            case '=' : optab[tel] = EQUALS;break;
            case '.' : optab[tel] = DOT  ;break;
            case ',' : optab[tel] = COMMA;break;
            case '|' : optab[tel] = OR   ;break;
            case '&' : optab[tel] = AND   ;break;
            case ':' : optab[tel] = MATCH;break;
            case '+' : optab[tel] = PLUS ;break;
            case '*' : optab[tel] = TIMES ;break;
            case '^' : optab[tel] = EXP  ;break;
            case 016 : optab[tel] = LOG  ;break;
            case 017 : optab[tel] = DIF  ;break;
            case '$' : optab[tel] = FUN  ;break;
            case '\'': optab[tel] = FUU  ;break;
            case '_' : optab[tel] = UNDERSCORE;break;
            default  : optab[tel] = (tel <= ' ') ? WHITE : NOOP;
            }
        }
    }

static psk prive(psk pkn)
    {
    if(shared(pkn))
        {
        dec_refcount(pkn);
        return subboomcopie(pkn);
        }
    return pkn;
    }

static psk setflgs(psk pokn,int Flgs)
    {
    if((Flgs & ERFENIS) || !(Flgs & SUCCESS))
        {
        pokn = prive(pokn);
        pokn->v.fl ^= ((Flgs & SUCCESS) ^ SUCCESS);
        pokn->v.fl |= (Flgs & ERFENIS);
        if(ANYNEGATION(Flgs))
            pokn->v.fl |= NOT;
        }
    return pokn;
    }

static psk startboom_w(psk pkn,...)
    {
#if !GLOBALARGPTR
    va_list argptr;
#endif
    if(pkn)
        wis(pkn);
    va_start(argptr,pkn);
    start = startPos = va_arg(argptr,unsigned char *);
#if GLOBALARGPTR
    pkn = lex(NULL,0,0,0);
#else
    pkn = lex(NULL,0,0,0,&argptr);
#endif
    va_end(argptr);
    return pkn;
    }

static psk vopbnowis(psk pkn,const char *conc[])
    {
    psk okn;
    assert(pkn != NULL);
    pstart = (unsigned char **)conc;
    start = (unsigned char *)conc[0];
    shift = vshift_nw;
#if GLOBALARGPTR
    okn = lex(NULL,0,0,0);
#else
    okn = lex(NULL,0,0,0,0);
#endif
    shift = shift_nw;
    okn = setflgs(okn,pkn->v.fl);
    return okn;
    }

static psk vopb(psk pkn,const char *conc[])
    {
    psk okn = vopbnowis(pkn,conc);
    wis(pkn);
    return okn;
    }

static psk opb(psk pkn,...)
    {
    psk okn;
#if !GLOBALARGPTR
    va_list argptr;
#endif
    va_start(argptr,pkn);
    start = startPos = va_arg(argptr,unsigned char *);
#if GLOBALARGPTR
    okn = lex(NULL,0,0,0);
#else
    okn = lex(NULL,0,0,0,&argptr);
#endif
    va_end(argptr);
    if(pkn)
        {
        okn = setflgs(okn,pkn->v.fl);
        wis(pkn);
        }
    return okn;
    }

static psk dopb(psk pkn,psk src)
    {
    psk okn;
    okn = zelfde_als_w(src);
    okn = setflgs(okn,(pkn)->v.fl);
    wis(pkn);
    return okn;
    }

static method_pnt findBuiltInMethodByName(typedObjectknoop * object,const char * name)
    {
    method * methods = object->vtab;
    if(methods)
        {
        for(;methods->name && strcmp(methods->name,name);++methods)
            ;
        return methods->func;
        }
    return NULL;
    }

static void wis(psk top)
    {
    while(!shared(top)) /* tail recursion optimisation; delete deep structures*/
        {
        psk kn = NULL;
        if(is_object(top) && ISCREATEDWITHNEW((objectknoop*)top))
            {
            adr[1] = top->RIGHT;
            kn = opb(kn,"(((=\1).die)')",NULL);
            kn = eval(kn);
            wis(kn);
            if(ISBUILTIN((objectknoop*)top))
                {
                method_pnt theMethod = findBuiltInMethodByName((typedObjectknoop*)top,"Die");
                if(theMethod)
                    {
                    theMethod((struct typedObjectknoop *)top,NULL);
                    }
                }
            }
        if(is_op(top))
            {
            wis(top->LEFT);
            kn = top;
            top = top->RIGHT;
            pskfree(kn);
            }
        else
            {
            if(top->ops & LATEBIND)
                {
                wis(((stringrefknoop*)top)->kn);
                }
            pskfree(top);
            return;
            }
        }
    dec_refcount(top);
    }

static int macht2(int n)
/* returns MSB of n */
    {
    int m;
    for( m = 1
       ; n
       ; n >>= 1,m <<= 1
       )
       ;
    return m >> 1;
    }

static ppsk Entry(int n,int index,varia **pv)
    {
    if(n == 0)
        {
        return (ppsk)pv;  /* no varia records are needed for 1 entry */
        }
    else
        {
        varia *hv;
        int MSB = macht2(n);
        for( hv = *pv /* begin with longest varia record */
           ; MSB > 1 && index < MSB
           ; MSB >>= 1
           )
           hv = hv->prev;
        index -= MSB;   /* if index == 0, then index becomes -1 */
        return &hv->verdi[index];  /* verdi[-1] == (psk)*prev */
        }
    }

static psk Entry2(int n,int index,varia * pv)
    {
    if(n == 0)
        {
        return (psk)pv;  /* no varia records needed for 1 entry */
        }
    else
        {
        varia *hv;
        int MSB = macht2(n);
        for( hv = pv /* begin with longest varia record */
           ; MSB > 1 && index < MSB
           ; MSB >>= 1
           )
           hv = hv->prev;
        index -= MSB;   /* if index == 0, then index becomes -1 */
        return hv->verdi[index];  /* verdi[-1] == (psk)*prev */
        }
    }

#if INTSCMP
static int intscmp(LONG *s1,LONG *s2) /* this routine produces different results
                                  depending on BIGENDIAN */
{
while(*((char *)s1 + 3))
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


static int zoeknaam(psk name,
                    vars **pvoorvar,
                    vars **pnavar)
    {
    unsigned char *string;
    vars *navar,*voorvar;
    string = POBJ(name);
    for( voorvar = NULL,navar = variabelen[*string]
       ;  navar && (STRCMP(VARNAME(navar),string) < 0)
       ; voorvar = navar,navar = navar->next
       )
       ;
    /* voorvar < string <= navar */
    *pvoorvar = voorvar;
    *pnavar = navar;
    return navar && !STRCMP(VARNAME(navar),string);
    }

static Qgetal _qmaalmineen(Qgetal _qx)
    {
    Qgetal res;
    size_t len;
    len = offsetof(sk,u.obj) + 1 + strlen((char *)POBJ(_qx));
    res = (Qgetal)bmalloc(__LINE__,len);
    memcpy(res,_qx,len);
    res->ops ^= MINUS;
    res->ops &= ~ALL_REFCOUNT_BITS_SET;
    return res;
    }

/* Create a node from a number, allocating memory for the node.
The numbers' memory isn't deallocated. */

static char * iconvert2decimal(ngetal * res, char * g)
    {
    LONG * iwyzer;
    g[0] = '0';
    g[1] = 0;
    for(iwyzer = res->inumber;iwyzer < res->inumber + res->ilength;++iwyzer)
        {
        assert(*iwyzer >= 0);
        assert(*iwyzer < RADIX);
        if(*iwyzer)
            {
            g += sprintf(g,LONGD,*iwyzer);
            for(;++iwyzer < res->inumber + res->ilength;)
                {
                assert(*iwyzer >= 0);
                assert(*iwyzer < RADIX);
                g += sprintf(g,/*"%0*ld"*/LONG0nD,(int)TEN_LOG_RADIX,*iwyzer);
                }
            break;
            }
        }
    return g;
    }

static ptrdiff_t numlength(ngetal * n)
    {
    ptrdiff_t len;
    LONG H;
    assert(n->ilength >= 1);
    len = TEN_LOG_RADIX*n->ilength;
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

static psk inumberNode(ngetal * g)
    {
    psk res;
    size_t len;
    len = offsetof(sk,u.obj) + numlength(g);
    res = (psk)bmalloc(__LINE__,len + 1);
    if(g->sign & QNUL)
        res->u.obj = '0';
    else
        {
        iconvert2decimal(g,(char *)POBJ(res));
        }

    res->v.fl = READY | SUCCESS | QGETAL;
    res->ops |= g->sign;
    return res;
    }

/* Create a node from a number, only allocating memory for the node if the
number hasn't the right size (== too large). If new memory is allocated,
the number's memory is deallocated. */
static psk numberNode2(ngetal * g)
    {
    psk res;
    size_t neededlen;
    size_t availablelen = g->allocated;
    neededlen = offsetof(sk,u.obj) + 1 + g->length;
    if((neededlen - 1)/sizeof(LONG) == (availablelen - 1)/sizeof(LONG))
        {
        res = (psk)g->alloc;
        if(g->sign & QNUL)
            {
            res->u.lobj = 0;
            res->u.obj = '0';
            }
        else
            {
            char * end = (char *)POBJ(res) + g->length;
            char * limit = (char *)g->alloc + (1+(availablelen - 1)/sizeof(LONG))*sizeof(LONG);
            memmove((void*)POBJ(res),g->number,g->length);
            while(end < limit)
                *end++ = '\0';
            }
        }
    else
        {
        res = (psk)bmalloc(__LINE__,neededlen);
        if(g->sign & QNUL)
            res->u.obj = '0';
        else
            {
            memcpy((void*)POBJ(res),g->number,g->length);
            /*(char *)POBJ(res) + g.length = '\0'; not necessary, is done in bmalloc */
            }
        bfree(g->alloc);
        }
    res->v.fl = READY | SUCCESS | QGETAL;
    res->ops |= g->sign;
    return res;
    }

static Qgetal not_a_number(void)
    {
    Qgetal res;
    res = copievan(&nulk);
    res->v.fl ^= SUCCESS;
    return res;
    }

static void convert2binary(ngetal * x)
    {
    LONG * iwyzer;
    char * wyzer;
    ptrdiff_t n;
 
    x->ilength = x->iallocated = ((x->sign & QNUL ? 1 : x->length) + TEN_LOG_RADIX - 1) / TEN_LOG_RADIX;
    x->inumber = x->ialloc = (LONG *)bmalloc(__LINE__,sizeof(LONG) * x->iallocated);
 
    for(   iwyzer = x->inumber
         , wyzer = x->number
         , n = x->length
       ; iwyzer < x->inumber + x->ilength
       ; ++iwyzer
       )
        {
        *iwyzer = 0;
        do
            {
            *iwyzer = 10 * (*iwyzer) + *wyzer++ - '0';
            }
        while(--n % TEN_LOG_RADIX != 0);
        }
    assert((LONG)TEN_LOG_RADIX * x->ilength >= wyzer - x->number);
    }

static char * isplits(Qgetal _qget,ngetal * ptel,ngetal * pnoem)
    {
    ptel->sign = _qget->ops & (MINUS|QNUL);
    pnoem->sign = 0;
    pnoem->alloc = ptel->alloc = NULL;
    ptel->number = (char *)POBJ(_qget);
    if(_qget->v.fl & QBREUK)
        {
        char * on = strchr(ptel->number,'/');
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
        assert(!(_qget->v.fl & QBREUK));
        ptel->length = strlen(ptel->number);
        pnoem->number = "1";
        pnoem->length = 1;
        convert2binary(ptel);
        convert2binary(pnoem);
        return NULL;
        }
    }

static char * splits(Qgetal _qget,ngetal * ptel,ngetal * pnoem)
    {
    ptel->sign = _qget->ops & (MINUS|QNUL);
    pnoem->sign = 0;
    pnoem->alloc = ptel->alloc = NULL;
    ptel->number = (char *)POBJ(_qget);

    if(_qget->v.fl & QBREUK)
        {
        char * on = strchr(ptel->number,'/');
        assert(on);
        ptel->length = on - ptel->number;
        pnoem->number = on + 1;
        pnoem->length = strlen(on + 1);
        return on;
        }
    else
        {
        assert(!(_qget->v.fl & QBREUK));
        ptel->length = strlen(ptel->number);
        pnoem->number = "1";
        pnoem->length = 1;
        return NULL;
        }
    }


static int opaffinal(char * i1,char * i2,char tmp,char ** pres,char *bx)
    {
    for(;i2 >= bx;)
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
    *pres = i1+1;
    return tmp;
    }


static int op(char **pres,char *bx,char *ex,char *by,char *ey)
    {
    char *i1 = *pres,*i2 = ex,*wyzer = ey;
    char tmp = 0;
    do
        {
        tmp += (*i2 + *wyzer-'0');
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
        --wyzer;
        }
    while(wyzer >= by);
    return opaffinal(i1,i2,tmp,pres,bx);
    }

static int af(char **pres,char *bx,char *ex,char *by,char *ey)
    {
    char *i1 = *pres,*i2 = ex,*wyzer = ey;
    char tmp = 0;
    do
        {
        tmp += (*i2 - *wyzer + '0');
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
        --wyzer;
        }
    while(wyzer >= by);
    return opaffinal(i1,i2,tmp,pres,bx);
    }



static void skipnullen(ngetal * nget,int teken)
    {
    for(
       ; nget->length > 0 && *(nget->number) == '0'
       ; nget->number++,nget->length--
       )
       ;
    nget->sign = nget->length ? (teken & MINUS) : QNUL;
    }
/*
multiply number with 204586 digits with itself
New algorithm:   17,13 s
Old algorithm: 1520,32 s

{?} get$longint&!x*!x:?y&lst$(y,verylongint,NEW)&ok
*/
#ifndef NDEBUG
static void pbint(LONG * high,LONG * low)
    {
    for(;high <= low;++high)
        {
        if(*high)
            {
            printf(LONGD " ",*high);
            break;
            }
        else
            printf("NUL ");
        }
    for(;++high <= low;)
        {
        printf("%0*ld ",(int)TEN_LOG_RADIX,*high);
        }
    }

static void pbin(ngetal * res)
    {
    pbint(res->inumber,res->inumber + res->ilength - 1);
    }

static void fpbint(FILE * fp,LONG * high,LONG * low)
    {
    for(;high <= low;++high)
        {
        if(*high)
            {
            fprintf(fp,LONGD " ",*high);
            break;
            }
        else
            fprintf(fp,"NUL ");
        }
    for(;++high <= low;)
        {
        fprintf(fp,"%0*ld ",(int)TEN_LOG_RADIX,*high);
        }
    }

static void fpbin(FILE * fp,ngetal * res)
    {
    fpbint(fp,res->inumber,res->inumber + res->ilength - 1);
    }

static void validt(LONG * high,LONG * low)
    {
    for(;high <= low;++high)
        {
        assert(0 <= *high);
        assert(*high < RADIX);
        }
    }

static void valid(ngetal * res)
    {
    validt(res->inumber,res->inumber + res->ilength - 1);
    }
#endif

static void nmaal(ngetal * x,ngetal * y,ngetal * product)
    {
    LONG *I1,*I2;
    LONG *iwyzer,*itussen;

    assert(product->length == 0);
    assert(product->ilength == 0);
    assert(product->alloc == 0);
    assert(product->ialloc == 0);
    product->ilength = product->iallocated = x->ilength + y->ilength;
    assert(product->iallocated > 0);
    product->inumber = product->ialloc = (LONG *)bmalloc(__LINE__,sizeof(LONG) * product->iallocated);

    for(iwyzer = product->inumber;iwyzer < product->inumber + product->ilength;*iwyzer++ = 0)
        ;

    for(I1 = x->inumber + x->ilength - 1;I1 >= x->inumber;I1--)
        {
        itussen = --iwyzer; /* pointer to result, starting from LSW. */
        assert(itussen >= product->inumber);
        for(I2 = y->inumber + y->ilength - 1;I2 >= y->inumber;I2--)
            {
            LONG prod;
            LONG *itussen2;
            prod = (*I1)*(*I2);
            *itussen += prod;
            itussen2 = itussen--;
            while(*itussen2 >= HEADROOM * RADIX2)
                {
                LONG karry;
                karry = *itussen2/RADIX;
                *itussen2 %= RADIX;
                --itussen2;
                assert(itussen2 >= product->inumber);
                *itussen2 += karry;
                }
            assert(itussen2 >= product->inumber);
            }
        if(*iwyzer >= RADIX)
            {
            LONG karry = *iwyzer/RADIX;
            *iwyzer %= RADIX;
            itussen = iwyzer - 1;
            assert(itussen >= product->inumber);
            *itussen += karry;
            while(*itussen >= HEADROOM * RADIX2/* 2000000000 */)
                {
                karry = *itussen/RADIX;
                *itussen %= RADIX;
                --itussen;
                assert(itussen >= product->inumber);
                *itussen += karry;
                }
            assert(itussen >= product->inumber);
            }
        }
    while(iwyzer >= product->inumber)
        {
        if(*iwyzer >= RADIX)
            {
            LONG karry = *iwyzer/RADIX;
            *iwyzer %= RADIX;
            --iwyzer;
            assert(iwyzer >= product->inumber);
            *iwyzer += karry;
            }
        else
            --iwyzer;
        }

    for(iwyzer = product->inumber;product->ilength > 1 && *iwyzer == 0;++iwyzer)
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

static LONG iopaffinal(LONG *highRemainder,LONG * lowRemainder,LONG carry)
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


static LONG iop(LONG *highRemainder,LONG *lowRemainder,LONG *highDivisor,LONG *lowDivisor)
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
        }
    while(highDivisor <= lowDivisor );
    assert(*highRemainder == 0);
    return iopaffinal(highRemainder,lowRemainder,carry);
    }

static LONG iaf( LONG *highRemainder
              , LONG *lowRemainder
              , LONG *highDivisor
              , LONG *lowDivisor
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
            *lowRemainder = carry + f*RADIX;
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
        }
    while(highDivisor <= lowDivisor );
    return iopaffinal(highRemainder,lowRemainder,carry);
    }

static LONG iaf2( LONG *highRemainder
               , LONG *lowRemainder
               , LONG *highDivisor
               , LONG *lowDivisor
               , LONG factor
               )
    {
    int allzero = TRUE;
    LONG carry = 0;
    LONG *slowRemainder = lowRemainder;
    do
        {
        assert(highRemainder <= lowRemainder);
        assert(*lowDivisor >= 0);
        carry += (*lowRemainder - factor * (*lowDivisor));
        if(carry < 0)
            {
            LONG f = 1 + ((-carry - 1) / RADIX);
            *lowRemainder = carry + f*RADIX;
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
        }
    while(highDivisor <= lowDivisor );
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
        assert(-((HEADROOM + 1)*RADIX) < HEADROOM * *lowRemainder && HEADROOM * *lowRemainder < ((HEADROOM + 1)*RADIX));
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
            assert(0 <= *lowRemainder && (*lowRemainder < RADIX || (lowRemainder == highRemainder && HEADROOM * *lowRemainder < ((HEADROOM + 1)*RADIX))));
            if(*lowRemainder)
                allzero = FALSE;
            --lowRemainder;
            }
        assert(carry == 0);

        assert(0 <= *++lowRemainder && HEADROOM * *lowRemainder < ((HEADROOM + 1)*RADIX));
        if(*lowRemainder == 0)
            return allzero ? 0 : -1;
        else
            return -1;
        }
    }

static LONG nndeel(ngetal * dividend,ngetal * divisor,ngetal * quotient, ngetal * remainder)
    {
    LONG *low,*quot,*head,*oldhead;
    /* Remainder starts out as copy of dividend. As the division progresses,
       the leading element of the remainder moves to the right.
       The movement is interupted if the divisor is greater than the
       leading elements of the dividend.
    */
    LONG divRADIX,divRADIX2; /* leading one or two elements of divisor */
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
    remainder->inumber = remainder->ialloc = (LONG *)bmalloc(__LINE__,sizeof(LONG) * remainder->iallocated);
    *(remainder->inumber) = 0;
    assert(dividend->ilength != 0);/*if(dividend->ilength == 0)
        remainder->inumber[0] = 0;
    else*/
        memcpy(remainder->inumber,dividend->inumber,dividend->ilength*sizeof(LONG));

    if(dividend->ilength >= divisor->ilength)
        quotient->ilength = quotient->iallocated = 1 + dividend->ilength - divisor->ilength;
    else
        quotient->ilength = quotient->iallocated = 1;

    quotient->inumber = quotient->ialloc = (LONG *)bmalloc(__LINE__,(size_t)(quotient->iallocated)*sizeof(LONG));
    memset(quotient->inumber,0,(size_t)(quotient->iallocated)*sizeof(LONG));
    quot = quotient->inumber;

    divRADIX = divisor->inumber[0];
    if(divisor->ilength > 1)
        {
        divRADIX2 = RADIX*divisor->inumber[0]+divisor->inumber[1];
        }
    else
        {
        divRADIX2 = 0;
        }
    assert(divRADIX > 0);
    for( low = remainder->inumber + (size_t)divisor->ilength - 1
        ,head = oldhead = remainder->inumber
       ; low < remainder->inumber + dividend->ilength
       ; ++low,++quot,++head
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
            assert(low < remainder->inumber+remainder->ilength);
            assert(quot < quotient->inumber+quotient->ilength);
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
                assert(factor * HEADROOM < RADIX * (HEADROOM + 1));
                *quot += sign * factor;
                assert(0 <= *quot);
                /*assert(*quot < RADIX);*/
                nsign = iaf2( head
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
            }
        while(sign < 0);
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
    for(low = remainder->inumber;remainder->ilength > 1 && *low == 0;++low)
        {
        --(remainder->ilength);
        ++(remainder->inumber);
        }

    assert(remainder->ilength >= 1);
    assert(remainder->inumber >= (LONG*)remainder->ialloc);
    assert(remainder->inumber < (LONG*)remainder->ialloc + remainder->iallocated);
    assert(remainder->inumber + remainder->ilength <= (LONG*)remainder->ialloc + remainder->iallocated);

    for(low = quotient->inumber;quotient->ilength > 1 && *low == 0;++low)
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


static Qgetal nn2q(ngetal * num,ngetal * den)
    {
    Qgetal res;
    assert(!(num->sign & QNUL));
    assert(!(den->sign & QNUL));
/*    if(num->sign & QNUL)
        return copievan(&nulk);
    else if(den->sign & QNUL)
        return not_a_number();*/
    
    num->sign ^= (den->sign & MINUS);

    if(den->ilength == 1 && den->inumber[0] == 1)
        {
        res = inumberNode(num);
        }
    else
        {
        char * endp;
        size_t len = offsetof(sk,u.obj) + 2 + numlength(num) + numlength(den);
        res = (psk)bmalloc(__LINE__,len);
        endp = iconvert2decimal(num,(char *)POBJ(res));
        *endp++ = '/';
        endp = iconvert2decimal(den,endp);
        assert(endp - (char *)res <= len);
        res->v.fl = READY | SUCCESS | QGETAL | QBREUK;
        res->ops |= num->sign;
        }
    return res;
    }

static Qgetal _qndeel(ngetal * x,ngetal * y)
    {
    Qgetal res;
    ngetal gcd = {0},hrem = {0};
    ngetal quotientx = {0},remainderx = {0};
    ngetal quotienty = {0},remaindery = {0};

#ifndef NDEBUG
    valid(x);
    valid(y);
#endif
    if(x->sign & QNUL)
        return copievan(&nulk);
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
        nndeel(&gcd,&hrem,&quotientx,&remainderx);
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
        }
    while(!(hrem.sign & QNUL));

    if(hrem.ialloc)
        bfree(hrem.ialloc);

    nndeel(x,&gcd,&quotientx,&remainderx);
#ifndef NDEBUG
        valid(&gcd);
        valid(x);
        valid(&quotientx);
        valid(&remainderx);
#endif
    bfree(remainderx.ialloc);
    nndeel(y,&gcd,&quotienty,&remaindery);
#ifndef NDEBUG
        valid(&gcd);
        valid(y);
        valid(&quotienty);
        valid(&remaindery);
#endif
    bfree(remaindery.ialloc);

    if(gcd.ialloc)
        bfree(gcd.ialloc);
    res = nn2q(&quotientx,&quotienty);
    bfree(quotientx.ialloc);
    bfree(quotienty.ialloc);
    return res;
    }


static ngetal nplus(ngetal * x,ngetal * y)
    {
    ngetal res = {0};
    char *hres;
    ptrdiff_t xgrotery;
    res.length = 1+(x->length > y->length ? x->length : y->length);
    res.allocated = (size_t)res.length + offsetof(sk,u.obj);
    res.alloc = res.number = (char *)bmalloc(__LINE__,res.allocated);
    *res.number = '0';
    hres = res.number+(size_t)res.length - 1;
    if(x->length == y->length)
        xgrotery = strncmp(x->number,y->number,(size_t)res.length);
    else
        xgrotery = x->length - y->length;
    if(xgrotery < 0)
        {
        ngetal * hget = x;
        x = y;
        y = hget;
        }
    if(x->sign == y->sign)
        {
        if(op(&hres,x->number,x->number + x->length - 1,y->number,y->number + y->length - 1))
            *--hres = '1';
        }
    else
        af(&hres,x->number,x->number + x->length - 1,y->number,y->number + y->length - 1);
    skipnullen(&res,x->sign);
    return res;
    }

static void inplus(ngetal * x,ngetal * y,ngetal * som)
    {
    LONG * hres, * px, * py, * ex;
    ptrdiff_t xgrotery;

    som->ilength = 1+(x->ilength > y->ilength ? x->ilength : y->ilength);
    som->iallocated = som->ilength;
    som->ialloc = som->inumber = (LONG *)bmalloc(__LINE__,sizeof(LONG)*som->iallocated);
    *som->inumber = 0;
    hres = som->inumber + som->ilength;

    if(x->ilength == y->ilength)
        {
        px = x->inumber;
        ex = px + x->ilength;
        py = y->inumber;
        do
            {
            xgrotery = *px++ - *py++;
            }
        while(!xgrotery && px < ex);
        }
    else
        xgrotery = x->ilength - y->ilength;
    if(xgrotery < 0)
        {
        ngetal * hget = x;
        x = y;
        y = hget;
        }

    for(px = x->inumber + x->ilength;px > x->inumber;)
        *--hres = *--px;
    if(x->sign == y->sign)
        {
#ifndef NDEBUG
        LONG carry = 
#endif
            iop(som->inumber,som->inumber + som->ilength - 1,y->inumber,y->inumber + y->ilength - 1);
        assert(carry == 0);
        }
    else
        {
        iaf(som->inumber,som->inumber + som->ilength - 1,y->inumber,y->inumber + y->ilength - 1,1);
        }
    for(hres = som->inumber;som->ilength > 1 && *hres == 0;++hres)
        {
        --(som->ilength);
        ++(som->inumber);
        }

    som->sign = som->inumber[0] ? (x->sign & MINUS) : QNUL;
    }

static Qgetal _qplus(Qgetal _qx,Qgetal _qy,int minus)
    {
    ngetal xt = {0},xn = {0},yt = {0},yn = {0};
    
    char *xb,*yb;
    xb = splits(_qx,&xt,&xn);
    yb = splits(_qy,&yt,&yn);
    yt.sign ^= minus;

    if(!xb && !yb)
        {
        ngetal g = nplus(&xt,&yt);
        psk res = numberNode2(&g);
        return res;
        }
    else
        {
        ngetal pa = {0},pb = {0},som = {0};
        Qgetal res;
        convert2binary(&xt);
        convert2binary(&xn);
        convert2binary(&yt);
        convert2binary(&yn);
        nmaal(&xt,&yn,&pa);
        nmaal(&yt,&xn,&pb);
        inplus(&pa,&pb,&som);
        bfree(pa.ialloc);
        bfree(pb.ialloc);
        pa.ilength = 0;
        pa.ialloc = 0;
        pa.length = 0;
        pa.alloc = 0;
        nmaal(&xn,&yn,&pa);
        res = _qndeel(&som,&pa);
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

static Qgetal _qdema(ngetal * x1,ngetal * x2,ngetal * y1,ngetal * y2)
    {
    ngetal pa = {0},pb = {0};
    Qgetal res;
    
    nmaal(x1,y1,&pa);
    nmaal(x2,y2,&pb);
    res = _qndeel(&pa,&pb);
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

static Qgetal _qdema2(ngetal * x1,ngetal * x2,ngetal * y1,ngetal * y2)
    {
    ngetal pa = {0},pb = {0};
    Qgetal res;
    nmaal(x1,y1,&pa);
    nmaal(x2,y2,&pb);
    res = nn2q(&pa,&pb);
    bfree(pa.alloc);
    bfree(pb.alloc);
    return(res);
    }

static Qgetal _qmaal2(Qgetal _qx,Qgetal _qy)
    {
    ngetal xt = {0},xn = {0},yt = {0},yn = {0};
    char *xb,*yb;
    xb = splits(_qx,&xt,&xn);
    yb = splits(_qy,&yt,&yn);
    if(!xb && !yb)
        {
        ngetal g = {0};
        nmaal(&xt,&yt,&g);
        psk res = numberNode2(&g);
        return res;
        }
    else
        {
        Qgetal res;
        res = _qdema2(&xt,&xn,&yt,&yn);
        return res;
        }
    }

static int _nndeel(ngetal * x,ngetal * y)
    {
    division resx,resy;
    ngetal gcd = {0},hrem = {0};

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
        nndeel(&gcd,&hrem,&resx);
        if(gcd.alloc)
            bfree(gcd.alloc);
        gcd = hrem;
        hrem = resx.remainder;
        bfree(resx.quotient.alloc);
        }
    while(!(hrem.sign & QNUL));

    if(hrem.alloc)
        bfree(hrem.alloc);

    nndeel(x,&gcd,&resx);
    bfree(resx.remainder.alloc);

    nndeel(y,&gcd,&resy);
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
static Qgetal _qdema(ngetal * x1,ngetal * x2,ngetal * y1,ngetal * y2)
    {
    Qgetal res;
    ngetal z1 = {0},z2 = {0};
    _nndeel(x1,y2);
    _nndeel(y1,x2);
    _nndeel(x1,x2);
    _nndeel(y1,y2);
    nmaal(x1,y1,&z1);
    nmaal(x2,y2,&z2);
    if(x1->alloc) bfree(x1->alloc);
    if(x2->alloc) bfree(x2->alloc);
    if(y1->alloc) bfree(y1->alloc);
    if(y2->alloc) bfree(y2->alloc);
    res = nn2q(&z1,&z2);
    if(z1.alloc) bfree(z1.alloc);
    if(z2.alloc) bfree(z2.alloc);
    return res;
    }
#endif

static Qgetal _qmaal(Qgetal _qx,Qgetal _qy)
    {
    Qgetal res;
    ngetal xt = {0},xn = {0},yt = {0},yn = {0};
    char *xb,*yb;
    
    xb = isplits(_qx,&xt,&xn);
    yb = isplits(_qy,&yt,&yn);
    if(!xb && !yb)
        {
        ngetal g = {0};
        nmaal(&xt,&yt,&g);
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
        res = _qdema(&xt,&xn,&yt,&yn);
        bfree(xt.ialloc);
        bfree(xn.ialloc);
        bfree(yt.ialloc);
        bfree(yn.ialloc);
        return res;
        }
    }

static Qgetal _q_qdeel(Qgetal _qx,Qgetal _qy)
    {
    ngetal xt = {0},xn = {0},yt = {0},yn = {0};
    char *xb,*yb;
    
    xb = isplits(_qx,&xt,&xn);
    yb = isplits(_qy,&yt,&yn);
    if(!xb && !yb)
        {
        Qgetal res = _qndeel(&xt,&yt);
        bfree(xt.ialloc);
        bfree(xn.ialloc);
        bfree(yt.ialloc);
        bfree(yn.ialloc);
        return res;
        }
    else
        {
        Qgetal res;
        res = _qdema(&xt,&xn,&yn,&yt);
        bfree(xt.ialloc);
        bfree(xn.ialloc);
        bfree(yt.ialloc);
        bfree(yn.ialloc);
        return res;
        }
    }


static Qgetal _qheeldeel(Qgetal _qx,Qgetal _qy)
    {
    Qgetal res,hulp;
    ngetal xt = {0},xn = {0},yt = {0},yn = {0},p1 = {0},p2 = {0},quotient = {0},remainder = {0};

    isplits(_qx,&xt,&xn);
    isplits(_qy,&yt,&yn);

    nmaal(&xt,&yn,&p1);
    nmaal(&xn,&yt,&p2);
    nndeel(&p1,&p2,&quotient,&remainder);
    bfree(p1.ialloc);
    bfree(p2.ialloc);
    res = _qplus
        (      (remainder.sign & QNUL)
            || !(_qx->ops & MINUS)
          ? &nulk
          :   (_qy->ops & MINUS)
            ? &eenk
            : &mineenk
            , (hulp = inumberNode(&quotient))
        , 0
        );
    pskfree(hulp);
    bfree(quotient.ialloc);
    bfree(remainder.ialloc);
    bfree(xt.ialloc);
    bfree(xn.ialloc);
    bfree(yt.ialloc);
    bfree(yn.ialloc);
    return res;
    }

static Qgetal _qmodulo(Qgetal _qx,Qgetal _qy)
    {
    Qgetal res,_q2,_q3;
    
    _q2 = _qheeldeel(_qx,_qy);
    _q3 = _qmaal(_qy,_q2);
    pskfree(_q2);
    res = _qplus(_qx,_q3,MINUS);
    pskfree(_q3);
    return res;
    }

static psk _qdenominator(psk pkn)
    {
    Qgetal _qx = pkn->RIGHT;
    psk res;
    size_t len;
    ngetal xt = {0},xn = {0};
    splits(_qx,&xt,&xn);
    len = offsetof(sk,u.obj) + 1 + xn.length;
    res = (psk)bmalloc(__LINE__,len);
    assert(!(xn.sign & QNUL)); /*Because RATIONAAL_COMP(_qx)*/
    memcpy((void*)POBJ(res),xn.number,xn.length);
    res->v.fl = READY | SUCCESS | QGETAL;
    res->ops |= xn.sign;
    wis(pkn);
    return res;
    }

static int _qvergelijk(Qgetal _qx,Qgetal _qy)
    {
    Qgetal som;
    int res;
    som = _qplus(_qx,_qy,MINUS);
    res = som->ops & (MINUS|QNUL);
    pskfree(som);
    return res;
    }


static int equal(psk kn1,psk kn2)
    {
    while(kn1 != kn2)
        {
        int r;
        if(is_op(kn1))
            {
            if(is_op(kn2))
                {
                r = (int)kop(kn2) - (int)kop(kn1);
                if(r)
                    {
                    return r;
                    }
                r = equal(kn1->LEFT,kn2->LEFT);
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
        else if(RATIONAAL_COMP(kn1))
            {
            if(RATIONAAL_COMP(kn2))
                {
                switch(_qvergelijk(kn1,kn2))
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
        else if(RATIONAAL_COMP(kn2))
            {
            return 1;
            }
        else
            return (r = HAS_MINUS_SIGN(kn1) - HAS_MINUS_SIGN(kn2)) == 0
                    ? strcmp((char *)POBJ(kn1),(char *)POBJ(kn2))
                    : r;
        }
    return 0;
    }

static int vgl(psk kn1,psk kn2);

static int vglsub(psk kn1,psk kn2)
    {
    int ret;
    psk kn = NULL;
    adr[1] = kn1;
    kn = opb(kn,"1+\1+-1)",NULL);
    kn = eval(kn);
    ret = vgl(kn,kn2);
    wis(kn);
    return ret;
    }

static int vgl(psk kn1,psk kn2)
    {
    DBGSRC(Printf("vgl      (");result(kn1);Printf(",");result(kn2);Printf(")\n");)
    while(kn1 != kn2)
        {
        int r;
        if(is_op(kn1))
            {
            if(is_op(kn2))
                {
                r = (int)kop(kn2) - (int)kop(kn1);
                if(r)
                    {
                    /*{?} x^(y*(a+b))+-1*x^(a*y+b*y) => 0 */ /*{!} 0 */
                    if(  kop(kn1) == TIMES
                      && kop(kn2) == PLUS
                      && is_op(kn1->RIGHT)
                      && kop(kn1->RIGHT) == PLUS
                      )
                        {
                        return vglsub(kn1,kn2);
                        }
                    else if(  kop(kn2) == TIMES
                      && kop(kn1) == PLUS
                      && is_op(kn2->RIGHT)
                      && kop(kn2->RIGHT) == PLUS
                      )
                        {
                        /*{?} a^(b*d+c*d) * (a^(b*f+c*f)+a^(e*b+e*c)) + -1*(a^((b+c)*(d+e))+a^((b+c)*(d+f))) => 0 */
                        return -vglsub(kn2,kn1);
                        }
                    return r;
                    }
                switch(kop(kn1))
                    {
                    case PLUS:
                    case EXP:
                        r = vgl(kn1->LEFT,kn2->LEFT);
                        if(r)
                            return r;
                        kn1 = kn1->RIGHT;
                        kn2 = kn2->RIGHT;
                        break;
                    default:
                        r = equal(kn1->LEFT,kn2->LEFT);
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
        else if(RATIONAAL_COMP(kn1))
            {
            if(RATIONAAL_COMP(kn2))
                {
                switch(_qvergelijk(kn1,kn2))
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
        else if(RATIONAAL_COMP(kn2))
            {
            return 1;
            }
        else
            return (r = HAS_MINUS_SIGN(kn1) - HAS_MINUS_SIGN(kn2)) == 0
                    ? strcmp((char *)POBJ(kn1),(char *)POBJ(kn2))
                    : r;
        }
    return 0;
    }

/*
name must be atom or <atom>.<atom>.<atom>...
*/
static int setmember(psk name,psk tree,psk nieuw)
    {
    while(is_op(tree))
        {
        if(kop(tree) == EQUALS)
            {
            psk nname;
            if(kop(name) == DOT)
                nname = name->LEFT;
            else
                nname = name;
            if(equal(tree->LEFT,nname))
                {
                return FALSE;
                }
            else if(nname == name)
                {
                wis(tree->RIGHT);
                tree->RIGHT = zelfde_als_w(nieuw);
                return TRUE;
                }
            else /* Found partial match for member name,
                    recurse in both name and member */
                {
                name = name->RIGHT;
                }
            }
        else if(setmember(name,tree->LEFT,nieuw))
            {
            return TRUE;
            }
        tree = tree->RIGHT;
        }
    return FALSE;
    }

static int update(psk name,psk pknoop) /* name = tree with DOT in root */
/*
    x:?(k.b)
    x:?((=(a=) (b=)).b)
*/
    {
    vars * navar;
    vars * voorvar;
    if(is_op(name->LEFT))
        {
        if(kop(name->LEFT) == EQUALS)
            /*{?} x:?((=(a=) (b=)).b) => x */
            /*          ^              */
            return setmember(name->RIGHT,name->LEFT->RIGHT,pknoop);
        else
            {
            /*{?} b:?((x.y).z) => x */
            return FALSE;
            }
        }
    if(kop(name) == EQUALS) /* {?} (=a+b)=5 ==> =5 */
        {
        wis(name->RIGHT);
        name->RIGHT = zelfde_als_w(pknoop);
        return TRUE;
        }
    else if(zoeknaam(name->LEFT,
        &voorvar,
        &navar))
        {
        assert(navar->pvaria);
        return setmember(name->RIGHT,Entry2(navar->n,navar->selector,navar->pvaria),pknoop);
        }
    else
        {
        return FALSE; /*{?} 66:?(someUnidentifiableObject.x) */
        }
    }


static int insert(psk name,psk pknoop)
    {
    vars *navar,*voorvar,*nieuwvar;

    if(is_op(name))
        {
        if(kop(name) == EQUALS)
            {
            wis(name->RIGHT);
            name->RIGHT = zelfde_als_w(pknoop);  /*{?} monk2:?(=monk1) => monk2 */
            return TRUE;
            }
        else
            { /* This allows, in fact, other operators than DOT, e.g. b:?(x y) */
            return update(name,pknoop); /*{?} (borfo=klot=)&bk:?(borfo klot)&!(borfo.klot):bk => bk */
            }
        }
    if(zoeknaam(name,
                &voorvar,
                &navar))
        {
        ppsk ppkn;
        wis(*(ppkn = Entry(navar->n,navar->selector,&navar->pvaria)));
        *ppkn = zelfde_als_w(pknoop);
        }
    else
        {
        size_t len;
        unsigned char *string;
        string = POBJ(name);
        len = strlen((char *)string);
#if PVNAME
        nieuwvar = (vars*)bmalloc(__LINE__,sizeof(vars));
        if(*string)
            {
#if ICPY
            MEMCPY(nieuwvar->vname = (unsigned char *)
                 bmalloc(__LINE__,len+1),string,(len >> LOGWORDLENGTH)+1);
#else
            MEMCPY(nieuwvar->vname = (unsigned char *)
                 bmalloc(__LINE__,len+1),string,((len / sizeof(LONG))+1) * sizeof(LONG));
#endif
            }
#else
        if(len < 4)
            nieuwvar = (vars*)bmalloc(__LINE__,sizeof(vars));
        else
            nieuwvar = (vars*)bmalloc(__LINE__,sizeof(vars) - 3 + len);
        if(*string)
            {
#if ICPY
            MEMCPY(&nieuwvar->u.Obj,string,(len / sizeof(LONG))+1);
#else
            MEMCPY(&nieuwvar->u.Obj,string,((len / sizeof(LONG))+1) * sizeof(LONG));
#endif
            }
#endif
        else
            {
#if PVNAME
            nieuwvar->vname = OBJ(nilk);
#else
            nieuwvar->u.Lobj = LOBJ(nilk);
#endif
            }
        nieuwvar->next = navar;
        if(voorvar == NULL)
            variabelen[*string] = nieuwvar;
        else
            voorvar->next = nieuwvar;
        nieuwvar->n = 0;
        nieuwvar->selector = 0;
        nieuwvar->pvaria = (varia*)zelfde_als_w(pknoop);
        }
    return TRUE;
    }

static int copy_insert(psk name,psk pknoop,psk snijaf)
    {
    psk kn;
    int ret;
    assert((pknoop->RIGHT == 0 && snijaf == 0) || pknoop->RIGHT != snijaf);
    DBGSRC(printf("copy_insert:");result(pknoop);printf("\n");)
    if(  (pknoop->v.fl & INDIRECT) 
      && (pknoop->v.fl & READY)
        /*
        {?} !dagj a:?dagj a
        {!} !dagj
        The test (pknoop->v.fl & READY) does not solve stackoverflow 
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
    else if(pknoop->v.fl & IDENT)
        {
        kn = copievan(pknoop);
        }
    else if(snijaf == NULL)
        {
        return insert(name,pknoop);
        }
    else
        {
        assert(!is_object(pknoop));
        if((shared(pknoop) != ALL_REFCOUNT_BITS_SET) && !all_refcount_bits_set(snijaf))
            {/* snijaf: either node with headroom in the small refcounter 
                        or object */
            DBGSRC(printf("name:[");result(name);printf("] pknoop:[");result(pknoop);printf("] snijaf(%d):[",snijaf->v.fl/ONE);result(snijaf);printf("]\n");)
            kn = new_operator_like(pknoop);
            kn->ops = (pknoop->ops & ~ALL_REFCOUNT_BITS_SET) | LATEBIND;
            pknoop->ops += ONE;
            if(shared(snijaf) == ALL_REFCOUNT_BITS_SET)
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
                INCREFCOUNT(snijaf);
                }
            else
                snijaf->ops += ONE;

            kn->LEFT = pknoop;
            kn->RIGHT = snijaf;
            }
        else
            {
            copyToSnijaf(&kn,pknoop,snijaf); /*{?} a b c:(?z:?x:?y:?a:?b) c => a b c */
            /*{?} 0:?n&a b c:?abc&whl'(!n+1:?n:<2000&str$(v !n):?v&!abc:?!v c) =>   whl
    ' ( !n+1:?n:<2000
      & str$(v !n):?v
      & !abc:?!v c
      ) */
            }
        }

    ret = insert(name,kn);
    wis(kn);
    return ret;
    }

static psk scopy(const char * str)
    {
    int nr = fullnumbercheck(str) & ~DEFINITELYNONUMBER;
    psk kn;
    if(nr & MINUS)
        { /* bracmat out$arg$() -123 */
        kn = (psk)bmalloc(__LINE__,sizeof(unsigned LONG) + strlen((const char *)str));
        strcpy((char *)(kn)+sizeof(unsigned LONG),str + 1);
        }
    else
        {
        kn = (psk)bmalloc(__LINE__,sizeof(unsigned LONG) + 1 + strlen((const char *)str));
        strcpy((char *)(kn)+sizeof(unsigned LONG),str);
        }
    kn->v.fl = READY | SUCCESS | nr;
    return kn;
    }

static int scopy_insert(psk name,const char * str)
    {
    int ret;
    psk kn;
    kn = scopy(str);
    ret = insert(name,kn);
    wis(kn);
    return ret;
    }

static int icopy_insert(psk name,LONG number)
    {
    char buf[22];
    sprintf((char*)buf,LONGD,number);
    return scopy_insert(name,buf);
    }

static int string_copy_insert(psk name,psk pknoop,char * str,char * snijaf)
    {
    char sav = *snijaf;
    int ret;
    *snijaf = '\0';
    if((pknoop->v.fl & IDENT) || all_refcount_bits_set(pknoop))
        {
        ret = scopy_insert(name,str);
        }
    else
        {
        stringrefknoop * kn;
        int nr;
        nr = fullnumbercheck(str) & ~DEFINITELYNONUMBER;
        if((nr & MINUS) && !(name->v.fl & NUMBER))
            nr = 0; /* "-1" is only converted to -1 if the # flag is present on the pattern */
        kn = (stringrefknoop *)bmalloc(__LINE__,sizeof(stringrefknoop));
        kn->ops = /*(pknoop->ops & ~(ALL_REFCOUNT_BITS_SET|VISIBLE_FLAGS)) substring doesn't inherit flags like */
            READY | SUCCESS | LATEBIND | nr;
        /*kn->ops |= SUCCESS;*/ /*{?} @(~`ab:%?x %?y)&!x => a */ /*{!} a */
        kn->kn = zelfde_als_w(pknoop);
        if(nr & MINUS)
            {
            kn->str = str+1;
            kn->length = snijaf - str - 1;
            }
        else
            {
            kn->str = str;
            kn->length = snijaf - str;
            }
        DBGSRC(int redMooi;int redhum;redMooi = mooi;\
            redhum = hum;mooi = FALSE;hum = FALSE;\
            Printf("str [%s] length " LONGU "\n",kn->str,(unsigned LONG int)kn->length);\
            mooi = redMooi;hum = redhum;)
        ret = insert(name,(psk)kn);
        if(ret)
            dec_refcount((psk)kn);
        else
            {
            wis(pknoop);
            bfree((void*)kn);
            }
        }
    *snijaf = sav;
    return ret;
    }

static int getCodePoint(const char ** ps)
    {
    /*
    return values:
    > 0:    code point
    -1:     no UTF-8
    -2:     too short for being UTF-8
    */
    int K;
    const char * s = *ps;
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
            for(i = 1;(K << i) & 0x80;++i)
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

static int getCodePoint2(const char ** ps,int * isutf)
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

static int utf8bytes(unsigned LONG val)
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
char * putCodePoint(unsigned LONG val,char * s)
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
            *s++ = (char)(0xc0 | (val >> 6 ));
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

static int strcasecompu(char ** S, char ** P,char * snijaf)
/* Additional argument snijaf */
    {
    int sutf = 1;
    int putf = 1;
    char * s = *S;
    char * p = *P;
    while(s < snijaf && *s && *p)
        {
        int diff;
        char * ns = s;
        char * np = p;
        int ks = getCodePoint2((const char **)&ns,&sutf);
        int kp = getCodePoint2((const char **)&np,&putf);
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
    return (s < snijaf ? (int)(unsigned char)*s : 0) - (int)(unsigned char)*p;
    }


static int strcasecomp(const char *s, const char *p)
    {
    int sutf = 1; /* assume UTF-8, becomes 0 if it is not */
    int putf = 1;
    while(*s && *p)
        {
        int ks = getCodePoint2((const char **)&s,&sutf);
        int kp = getCodePoint2((const char **)&p,&putf);
        int diff = toLowerUnicode(ks) - toLowerUnicode(kp);
        if(diff)
            {
            return diff;
            }
        }
    return (int)(const unsigned char)*s - (int)(const unsigned char)*p;
    }

#if CODEPAGE850
static int strcasecmpDOS(const char *s, const char *p)
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

#define NIET ((p->v.fl & NOT) && (p->v.fl & FLGS) < NUMBER)
#define PGRT (p->v.fl & GREATER_THAN)
#define PKLN (p->v.fl & SMALLER_THAN)
#define PONG  (PGRT && PKLN)
#define EPGRT (PGRT && !PKLN)
#define EPKLN (PKLN && !PGRT)

static int compare(psk s,psk p)
    {
    int teken;
    if(RATIONAAL_COMP(s) && RATIONAAL_WEAK(p))
        teken = _qvergelijk /*bereken_verschil*/(s,p);
    else
        {
        if(is_op(s))
            return NIKS(p);
        if(PLOBJ(s) == IM && PLOBJ(p) == IM)
            {
            int TMP = ((s->v.fl & MINUS) ^ (p->v.fl & MINUS));
            int Niet = (p->v.fl & FLGS) < NUMBER && ((p->v.fl & NOT) && 1);
            int ul = (p->v.fl & (GREATER_THAN|SMALLER_THAN)) == (GREATER_THAN|SMALLER_THAN);
            int e1 = ((p->v.fl & GREATER_THAN) && 1);
            int e2 = ((p->v.fl & SMALLER_THAN) && 1);
            int ee = (e1 ^ e2) && 1;
            int R = !ee && (Niet ^ ul ^ !TMP);
            return R;
            }
        if((p->v.fl & (NOT|BREUK|NUMBER|GREATER_THAN|SMALLER_THAN)) == (NOT|GREATER_THAN|SMALLER_THAN))
            { /* Case insensitive match: ~<> means "not different" */
            teken = strcasecomp((char *)POBJ(s),(char *)POBJ(p));
            }
        else
            {
            teken = strcmp((char *)POBJ(s),(char *)POBJ(p));
            }
        if(teken > 0)
            teken = 0;
        else if(teken < 0)
            teken = MINUS;
        else
            teken = QNUL;
        }
    switch(teken)
        {
        case 0 :
            {
            return NIET ^ (PGRT && 1);
            }
        case QNUL :
            {
            return !NIET ^ (PGRT || PKLN);
            }
        default :
            {
            return NIET ^ (PKLN && 1);
            }
        }
    }



#if CUTOFFSUGGEST
#define scompare(wh,s,c,p,suggestedCutOff,mayMoveStartOfSubject) scompare(s,c,p,suggestedCutOff,mayMoveStartOfSubject)
#else
#define scompare(wh,s,c,p) scompare(s,c,p)
#endif


/*
(1) Fractions are now matched.
(2) With the % flag on an otherwise numeric pattern, the pattern is treated 
    as a string, not a number.
*/
#if CUTOFFSUGGEST
static int scompare(char * wh,char * s,char * snijaf,psk p,char ** suggestedCutOff,char ** mayMoveStartOfSubject)
#else
static int scompare(char * wh,unsigned char * s,unsigned char * snijaf,psk p)
#endif
    {
    int teken = 0;
    char * P;
#if CUTOFFSUGGEST
    char * S = s;
#endif
    char sav;
    int smallerIfMoreDigitsAdded; /* -1/22 smaller than -1/2 */ /* 1/22 smaller than 1/2 */
    int samesign;
    int less;
    int Flgs = p->v.fl;
    enum {NoIndication,AnInteger,NotAFraction,NotANumber,AFraction,ANumber};
    int status = NoIndication;

    if(ONTKENNING(Flgs, NONIDENT))
        {
        Flgs &= ~(NOT|NONIDENT);
        }
    if(ONTKENNING(Flgs, BREUK))
        {
        Flgs &= ~(NOT|BREUK);
        if(Flgs & NUMBER)
            status = AnInteger;
        else
            status = NotAFraction;
        }
    else if(ONTKENNING(Flgs, NUMBER))
        {
        Flgs &= ~(NOT|NUMBER);
        status = NotANumber;
        }
    else if(Flgs & BREUK)
        status = AFraction;
    else if(Flgs & NUMBER)
        status = ANumber;


    if(  !(Flgs & NONIDENT) /* % as flag on number forces comparison as string, not as number */
      && RATIONAAL_WEAK(p)
      && (status != NotANumber)
      && (  (  ((Flgs & (QBREUK|IS_OPERATOR)) == QBREUK)
            && (status != NotAFraction)
            )
         || (  ((Flgs & (QBREUK|QGETAL|IS_OPERATOR)) == QGETAL)
            && (status != AFraction   )
            )
         )
      )
        {
        int check = sfullnumbercheck(s,snijaf);
        if(check & QGETAL)
            {
            int anythingGoes = 0;
            psk n = NULL;
            sav = *snijaf;
            *snijaf = '\0';
            n = opb(n,s,NULL);
            *snijaf = sav;

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
                    char * t = snijaf;
                    for(;*t;++t)
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
            teken = _qvergelijk(n,p);
            samesign = ((n->v.fl & MINUS) == (p->v.fl & MINUS));
            wis(n);
            switch(Flgs & (NOT|GREATER_THAN|SMALLER_THAN))
                {
                case NOT|GREATER_THAN|SMALLER_THAN:    /* n:~<>p */
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
                    switch(teken)
                        {
                        case QNUL:    /* n == p */
                            if(anythingGoes)
                                return TRUE;
                            else
                                return TRUE|ONCE;
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
                    switch(teken)
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
                    switch(teken)
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
                case GREATER_THAN|SMALLER_THAN:    /* n:<>p */
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
                    switch(teken)
                        {
                        case QNUL:    /* n == p */
                            return FALSE;
                        default:    /* n < p, n > p */
                            return TRUE;
                        }
                    }
                case NOT|SMALLER_THAN:    /* n:~<p */
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
                    switch(teken)
                        {
                        case QNUL:    /* n == p */
                            if(anythingGoes || !less)
                                return TRUE;
                            else
                                return TRUE|ONCE;
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
                    switch(teken)
                        {
                        case QNUL:    /* n == p */
                            if(anythingGoes || less)
                                return TRUE;
                            else
                                return TRUE|ONCE;
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
            /* End (check & QGETAL) == TRUE. */
            /*printf("Not expected here!");getchar();*/
            }
        else if(((s == snijaf) && (Flgs & (NUMBER|BREUK))) 
               || (   (s < snijaf)
                  && (  ((*s == '-') && (snijaf < s + 2))
                     || snijaf[-1] == '/'
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

    P = (char *)SPOBJ(p);

    if((Flgs & (NOT|BREUK|NUMBER|GREATER_THAN|SMALLER_THAN)) == (NOT|GREATER_THAN|SMALLER_THAN))
        { /* Case insensitive match: ~<> means "not different" */
        teken = strcasecompu(&s,&P,snijaf); /* Additional argument snijaf */
        }
    else
        {
#if CUTOFFSUGGEST
        if(suggestedCutOff)
            {
            switch(Flgs & (NOT|GREATER_THAN|SMALLER_THAN))
                {
                case NOT|GREATER_THAN:    /* n:~>p */
                case SMALLER_THAN:    /* n:<p */
                case GREATER_THAN|SMALLER_THAN:    /* n:<>p */
                case NOT:                        /* n:~p */
                    {
                    while(((teken = (s < snijaf ? (int)(unsigned char)*s : 0) - (int)(unsigned char)*P) == 0) && *P) /* Additional argument snijaf */
                        {
                        ++s;
                        ++P;
                        }
                    if(teken > 0)
                        {
                        switch(Flgs & (NOT|GREATER_THAN|SMALLER_THAN))
                            {
                            case NOT|GREATER_THAN:
                            case SMALLER_THAN:
                                return ONCE;
                            default:
                                return TRUE;
                            }
                        }
                    else if(teken == 0)
                        {
                        switch(Flgs & (NOT|GREATER_THAN|SMALLER_THAN))
                            {
                            case NOT|GREATER_THAN:
                                return TRUE|ONCE;
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
                    while(((teken = (int)(unsigned char)*s - (int)(unsigned char)*P) == 0) && *s && *P)
                        {
                        ++s;
                        ++P;
                        }
                    if(s >= snijaf)
                        {
                        if(teken > 0)
                            *suggestedCutOff = s + 1;
                        break;
                        }
                    return ONCE;
                    }
                case NOT|GREATER_THAN|SMALLER_THAN:    /* n:~<>p */
                case 0:                                /* n:p */
                    while(((teken = (int)(unsigned char)*s - (int)(unsigned char)*P) == 0) && *s && *P)
                        {
                        ++s;
                        ++P;
                        }
                    if(s >= snijaf && *P == 0)
                        {
                        *suggestedCutOff = s;
                        /*teken = 0;*/
                        if(mayMoveStartOfSubject)
                            *mayMoveStartOfSubject = 0;
                        return TRUE|ONCE;
                        }
                    if(mayMoveStartOfSubject && *mayMoveStartOfSubject != 0)
                        {
                        char * startpos;
/* #ifndef NDEBUG
                        char * pat = (char *)POBJ(p);
#endif */
                        startpos = strstr((char *)S,(char *)POBJ(p));
                        if(startpos != 0)
                            {
                            if(Flgs & MINUS)
                                --startpos;
                            }
                        /*assert(  startpos == 0 
                              || (startpos+strlen((char *)POBJ(p)) < snijaf - 1)
                              || (startpos+strlen((char *)POBJ(p)) >= snijaf)
                              );*/
                        *mayMoveStartOfSubject = (char *)startpos;
                        return ONCE;
                        }

                    if(teken > 0 || s < snijaf)
                        {
                        return ONCE;
                        }
                    return FALSE;/*subject too short*/
                case NOT|SMALLER_THAN:    /* n:~<p */
                    while(((teken = (int)(unsigned char)*s - (int)(unsigned char)*P) == 0) && *s && *P)
                        {
                        ++s;
                        ++P;
                        }
                    if(teken >= 0)
                        {
                        if(s >= snijaf)
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
            switch(Flgs & (NOT|GREATER_THAN|SMALLER_THAN))
                {
#if 1
                case NOT|GREATER_THAN|SMALLER_THAN:    /* n:~<>p */
                case 0:                                /* n:p */
                    while(((teken = (s < snijaf ? (int)(unsigned char)*s : 0) - (int)(unsigned char)*P) == 0) && *s && *P)
                        {
                        ++s;
                        ++P;
                        }
                    if(teken != 0 && mayMoveStartOfSubject && *mayMoveStartOfSubject != 0)
                        {
                        char * startpos;
                        char * ep = SPOBJ(p) + strlen((char*)POBJ(p));
                        char * es = snijaf ? snijaf : S + strlen((char*)S);
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
                        if((char *)startpos > *mayMoveStartOfSubject)
                            *mayMoveStartOfSubject = (char *)startpos;
                        return ONCE;
                        }
                        
                    break;
#endif
                default:
#else
                {
#endif
                    while(((teken = (s < snijaf ? (int)(unsigned char)*s : 0) - (int)(unsigned char)*P) == 0) && *P) /* Additional argument snijaf */
                        {
                        ++s;
                        ++P;
                        }
                }
            }
        }


    switch(Flgs & (NOT|GREATER_THAN|SMALLER_THAN))
        {
        case NOT|GREATER_THAN|SMALLER_THAN:    /* n:~<>p */
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
            if(teken == 0)
                {
                return TRUE|ONCE;
                }
            else if(teken < 0 && s >= snijaf)
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
            if(teken >= 0)
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
            if(teken > 0)
                {
                return TRUE;
                }
            else if(teken < 0 && s < snijaf)
                {
                return ONCE;
                }
            return FALSE;
            }
        case GREATER_THAN|SMALLER_THAN:    /* n:<>p */
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
            if(teken == 0)
                {
                return FALSE;
                }
            return TRUE;
            }
        case NOT|SMALLER_THAN:    /* n:~<p */
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
            if(teken < 0)
                {
                if(s < snijaf)
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
        case NOT|GREATER_THAN:    /* n:~>p */
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
            if(teken > 0)
                {
                return ONCE;
                }
            else if(teken < 0)
                {
                return TRUE;
                }
            return TRUE|ONCE;
            }
        }
    }

static int psh(psk name,psk pknoop,psk dim)
    {
    /* string must fulfill requirements of icpy */
    vars *navar,*voorvar;
    varia *nvaria;
    psk cknoop;
    int oldn,n,m2,m22;
    while(is_op(name))
        {
        /* return psh(name->LEFT,pknoop,dim) && psh(name->RIGHT,pknoop,dim); */
        if(!psh(name->LEFT,pknoop,dim))
            return FALSE;
        name = name->RIGHT;
        }
    if(dim && !INTEGER(dim))
        return FALSE;
    if(!zoeknaam(name,
                 &voorvar,
                 &navar))
        {
        insert(name,pknoop);
        if(dim)
            {
            zoeknaam(name,
                     &voorvar,
                     &navar);
            }
        else
            {
            return TRUE;
            }
        }
    n = oldn = navar->n;
    if(dim)
        {
        int newn;
        errno = 0;
        newn = (int)STRTOUL((char *)POBJ(dim),(char **)NULL,10);
        if(errno == ERANGE)
            return FALSE;
        if(RAT_NEG(dim))
            newn = oldn - newn + 1;
        if(newn < 0)
            return FALSE;
        navar->n = newn;
        if(oldn >= navar->n)
            {
            assert(navar->pvaria);
            for(;oldn >= navar->n;)
                wis(Entry2(n,oldn--,navar->pvaria));
            }
        navar->n--;
        if(navar->selector > navar->n)
            navar->selector = navar->n;
        }
    else
        {
        navar->n++;
        navar->selector = navar->n;
        }
    m2 = macht2(n);
    if(m2 == 0)
        m22 = 1;
    else
        m22 = m2 << 1;
    if(navar->n >= m22)
        /* allocate */
        {
        for(;navar->n >= m22;m22 <<= 1)
            {
            nvaria = (varia*)bmalloc(__LINE__,sizeof(varia) + (m22-1)*sizeof(psk));
            nvaria->prev = navar->pvaria;
            navar->pvaria = nvaria;
            }
        }
    else if(navar->n < m2)
        /* deallocate */
        {
        for(;m2 && navar->n < m2;m2 >>= 1)
            {
            nvaria = navar->pvaria;
            navar->pvaria = nvaria->prev;
            bfree(nvaria);
            }
        if(navar->n < 0)
            {
            if(voorvar)
                voorvar->next = navar->next;
            else
                variabelen[*POBJ(name)] = navar->next;
#if PVNAME
            if(navar->vname != OBJ(nilk))
                bfree(navar->vname);
#endif
            bfree(navar);
            return TRUE;
            }
        }
    assert(navar->pvaria);
    for( cknoop = pknoop
       ; ++oldn <= navar->n
       ; cknoop = *Entry(navar->n,oldn,&navar->pvaria) = zelfde_als_w(cknoop)
       )
       ;
    return TRUE;
    }

typedef struct classdef
    {
    char * name;
    method * vtab;
    } classdef;

typedef struct pskRecord
    {
    psk entry;
    struct pskRecord * next;
    } pskRecord;

typedef int (*cmpfuncTp)(const char *s, const char *p);
typedef LONG (*hashfuncTp)(const char *s);

typedef struct Hash
    {
    pskRecord **hash_table;
    unsigned LONG hash_size;
    unsigned LONG elements;     /* elements >= record_count */
    unsigned LONG record_count; /* record_count >= size - unoccupied */
    unsigned LONG unoccupied;
    cmpfuncTp cmpfunc;
    hashfuncTp hashfunc;
    } Hash;

static LONG casesensitivehash(const char * cp)
    {
    LONG hash_temp = 0;
    while (*cp != '\0')
        {
        if(hash_temp < 0)
            {
            hash_temp = (hash_temp << 1) +1;
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

static LONG caseinsensitivehash(const char * cp)
    {
    LONG hash_temp = 0;
    int isutf = 1;
    while (*cp != '\0')
        {
        if(hash_temp < 0)
            {
            hash_temp = (hash_temp << 1) +1;
            }
        else
            {
            hash_temp = hash_temp << 1;
            }
        hash_temp ^= toLowerUnicode(getCodePoint2((const char **)&cp,&isutf));
        }
    return hash_temp;
    }

#if CODEPAGE850
static LONG caseinsensitivehashDOS(const char * cp)
    {
    LONG hash_temp = 0;
    while (*cp != '\0')
        {
        if(hash_temp < 0)
            hash_temp = (hash_temp << 1) +1;
        else
            hash_temp = hash_temp << 1;
        hash_temp ^= ISO8859toCodePage850(lowerEquivalent[CodePage850toISO8859(*cp)]);
        ++cp;
        }
    return hash_temp;
    }
#endif

static psk removeFromHash(Hash * temp,psk Arg)
    {
    const char * key = (const char *)POBJ(Arg);
    LONG i;
    LONG hash_temp;
    pskRecord ** pr;
    hash_temp = (*temp->hashfunc)(key);
    assert(temp->hash_size);
    i = ((unsigned int)hash_temp) % temp->hash_size;
    pr = temp->hash_table + i;
    if(*pr)
        {
        while(*pr)
            {
            if(kop((*pr)->entry) == WHITE)
                {
                if(!(*temp->cmpfunc)(key,(const char *)POBJ((*pr)->entry->LEFT->LEFT)))
                    break;
                }
            else if(!(*temp->cmpfunc)(key,(const char *)POBJ((*pr)->entry->LEFT)))
               break;
            pr = &(*pr)->next;
            }
        if(*pr)
            {
            pskRecord * next = (*pr)->next;
            psk ret = (*pr)->entry;
            bfree(*pr);
            *pr = next;
            --temp->record_count;
            return ret;
            }
        }
    return NULL;
    }

static psk inserthash(Hash * temp,psk Arg)
    {
    const char * key = (const char *)POBJ(Arg->LEFT);
    LONG i;
    LONG hash_temp;
    pskRecord * r;
    hash_temp = (*temp->hashfunc)(key);
    assert(temp->hash_size);
    i = ((unsigned int)hash_temp) % temp->hash_size;
    r = temp->hash_table[i];
    if(!r)
        --temp->unoccupied;
    else
        while(r)
            {
            if(kop(r->entry) == WHITE)
                {
                if(!(*temp->cmpfunc)(key,(const char *)POBJ(r->entry->LEFT->LEFT)))
                    {
                    break;
                    }
                }
            else
                {
                if(!(*temp->cmpfunc)(key,(const char *)POBJ(r->entry->LEFT)))
                    {
                    break;
                    }
                }
            r = r->next;
            }
    if(r)
        {
        psk goal = (psk)bmalloc(__LINE__,sizeof(kknoop));
        goal->v.fl = WHITE | SUCCESS;
        goal->ops &= ~ALL_REFCOUNT_BITS_SET;
        goal->LEFT = zelfde_als_w(Arg);
        goal->RIGHT = r->entry;
        r->entry = goal;
        }
    else
        {
        r = (pskRecord *)bmalloc(__LINE__,sizeof(pskRecord));
        r->entry = zelfde_als_w(Arg);
        r->next = temp->hash_table[i];
        temp->hash_table[i] = r;
        ++temp->record_count;
        }
    ++temp->elements;
    return r->entry;
    }

static psk findhash(Hash * temp,psk Arg)
    {
    const char * key = (const char *)POBJ(Arg);
    LONG i;
    LONG hash_temp;
    pskRecord * r;
    hash_temp = (*temp->hashfunc)(key);
    assert(temp->hash_size);
    i = ((unsigned int)hash_temp) % temp->hash_size;
    r = temp->hash_table[i];
    if(r)
        {
        while(r)
            {
            if(kop(r->entry) == WHITE)
                {
                if(!(*temp->cmpfunc)(key,(const char *)POBJ(r->entry->LEFT->LEFT)))
                    break;
                }
            else if(!(*temp->cmpfunc)(key,(const char *)POBJ(r->entry->LEFT)))
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
            unsigned LONG i;
            for(i = temp->hash_size;i > 0;)
                {
                pskRecord * r = temp->hash_table[--i];
                pskRecord * next;
                while(r)
                    {
                    wis(r->entry);
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

static Hash * newhash(unsigned LONG size)
    {
    unsigned LONG i;
    Hash * temp = (Hash *)bmalloc(__LINE__,sizeof(Hash));
    assert(size > 0);
    temp->hash_size = size;
    temp->record_count = (unsigned int)0;
    temp->hash_table = (pskRecord **)bmalloc(__LINE__,sizeof(pskRecord *) * temp->hash_size);
    temp->cmpfunc = strcmp;
    temp->hashfunc = casesensitivehash;
    temp->elements = 0L;     /* elements >= record_count */
    temp->record_count = 0L; /* record_count >= size - unoccupied */
    temp->unoccupied = size;
    for(i = temp->hash_size;i > 0;)
        temp->hash_table[--i] = NULL;
    return temp;
    }

static unsigned LONG nextprime(unsigned LONG g)
    {
    /* For primality test, only try divisors that are 2, 3 or 5 or greater
    numbers that are not multiples of 2, 3 or 5. Candidates below 100 are:
     2  3  5  7 11 13 17 19 23 29 31 37 41 43
    47 49 53 59 61 67 71 73 77 79 83 89 91 97
    Of these 28 candidates, three are not prime:
    49 (7*7), 77 (7*11) and 91 (7*13) */
    int i;
    unsigned LONG smalldivisor;
    static int bijt[12]=
      {1,  2,  2,  4,    2,    4,    2,    4,    6,    2,  6};
    /*2-3,3-5,5-7,7-11,11-13,13-17,17-19,19-23,23-29,29-1,1-7*/
    unsigned LONG bigdivisor;
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

static void rehash(Hash ** ptemp,int loadFactor/*1-100*/)
    {
    Hash * temp = *ptemp;
    if(temp)
        {
        unsigned LONG newsize;
        Hash * newtable;
        newsize = nextprime((100 * temp->record_count)/loadFactor);
        if(!newsize)
            newsize = 1;
        newtable = newhash(newsize);
        newtable->cmpfunc = temp->cmpfunc;
        newtable->hashfunc = temp->hashfunc;
        if(temp->hash_table)
            {
            unsigned LONG i;
            for(i = temp->hash_size;i > 0;)
                {
                pskRecord * r = temp->hash_table[--i];
                while(r)
                    {
                    psk pkn = r->entry;
                    while(is_op(pkn) && kop(pkn) == WHITE)
                        {
                        inserthash(newtable,pkn->LEFT);
                        pkn = pkn->RIGHT;
                        }
                    inserthash(newtable,pkn);
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
        return (int)(temp->record_count / (temp->hash_size/100));
    }

static Boolean hashinsert(struct typedObjectknoop * This,ppsk arg)
    {
    psk Arg = (*arg)->RIGHT;
    if(is_op(Arg) && !is_op(Arg->LEFT))
        {
        psk ret;
        int lf = loadfactor(HASH(This));
        if(lf > 100)
            rehash((Hash **)(&(This->voiddata)),60);
        ret = inserthash(HASH(This),Arg);
        wis(*arg);
        *arg = zelfde_als_w(ret);
        return TRUE;
        }
    return FALSE;
    }

static Boolean hashfind(struct typedObjectknoop * This,ppsk arg)
    {
    psk Arg = (*arg)->RIGHT;
    if(!is_op(Arg))
        {
        psk ret = findhash(HASH(This),Arg);
        if(ret)
            {
            wis(*arg);
            *arg = zelfde_als_w(ret);
            return TRUE;
            }
        }
    return FALSE;
    }

static Boolean hashremove(struct typedObjectknoop * This,ppsk arg)
    {
    psk Arg = (*arg)->RIGHT;
    if(!is_op(Arg))
        {
        Hash * temp = HASH(This);
        psk ret = removeFromHash(temp,Arg);
        if(ret)
            {
            if(loadfactor(temp) < 50 && temp->hash_size > 97)
                rehash(PHASH(This),90);
            wis(*arg);
            *arg = ret;
            return TRUE;
            }
        }
    return FALSE;
    }

static Boolean hashnew(struct typedObjectknoop * This,ppsk arg)
    {
/*    UNREFERENCED_PARAMETER(arg);*/
    unsigned long N = 97;
    if(INTEGER_POS_COMP((*arg)->RIGHT))
        {
        N = strtoul((char *)POBJ((*arg)->RIGHT),NULL,10);
        if(N == 0 || N == ULONG_MAX)
            N = 97;
        }
    VOID(This) = (void *)newhash(N);
    return TRUE;
    }

static Boolean hashdie(struct typedObjectknoop * This,ppsk arg)
    {
    UNREFERENCED_PARAMETER(arg);
    freehash(HASH(This));
    return TRUE;
    }

#if CODEPAGE850
static Boolean hashDOS(struct typedObjectknoop * This,ppsk arg)
    {
    UNREFERENCED_PARAMETER(arg);
    (HASH(This))->hashfunc = caseinsensitivehashDOS;
    (HASH(This))->cmpfunc = strcasecmpDOS;
    rehash(PHASH(This),100);
    return TRUE;
    }
#endif

static Boolean hashISO(struct typedObjectknoop * This,ppsk arg)
    {
    UNREFERENCED_PARAMETER(arg);
    (HASH(This))->hashfunc = caseinsensitivehash;
    (HASH(This))->cmpfunc = strcasecomp;
    rehash(PHASH(This),100);
    return TRUE;
    }

static Boolean hashcasesensitive(struct typedObjectknoop * This,ppsk arg)
    {
    UNREFERENCED_PARAMETER(arg);
    (HASH(This))->hashfunc = casesensitivehash;
    (HASH(This))->cmpfunc = strcmp;
    rehash(PHASH(This),100);
    return TRUE;
    }


static Boolean hashforall(struct typedObjectknoop * This,ppsk arg)
    {
    unsigned LONG i;
    int ret = TRUE;
    This = (typedObjectknoop *)zelfde_als_w((psk)This);
    for( i = 0
        ;    ret && HASH(This)
          && i < (HASH(This))->hash_size
        ;
       )
        {
        pskRecord * r = (HASH(This))->hash_table[i];
        int j = 0;
        while(r)
            {
            int m;
            psk pkn = NULL;
            adr[2] = (*arg)->RIGHT; /* each time! adr[n] may be overwritten by evalueer (below)*/
            adr[3] = r->entry;
            pkn = opb(pkn,"(\2'\3)",NULL);
            pkn = eval(pkn);
            ret = isSUCCESSorFENCE(pkn);
            wis(pkn);
            if(  !ret
              || !HASH(This)
              || i >= (HASH(This))->hash_size
              || !(HASH(This))->hash_table[i]
              )
                break;
            ++j;
            for(m = 0,r = (HASH(This))->hash_table[i]
               ;r && m < j
               ;++m
               )
                 r = r->next;
            }
        ++i;
        }
    wis((psk)This);
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
    {NULL,NULL}};
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

classdef classes[] = {{"hash",hash},{NULL,NULL}};

static method_pnt findBuiltInMethod(typedObjectknoop * object,psk methodName)
    {
    if(!is_op(methodName))
        {
        return findBuiltInMethodByName(object,(const char *)POBJ(methodName));
        }
    return NULL;
    }

typedef struct 
    {
    psk self;
    psk object;
    method_pnt theMethod;
    } objectStuff;

static psk getmember(psk name,psk tree,objectStuff * Object)
    {
    DBGSRC(Printf("getmember(");result(name);Printf(",");result(tree);Printf(")\n");)
    while(is_op(tree))
        {
        if(kop(tree) == EQUALS)
            {
            psk nname;
            if(  Object
              && ISBUILTIN((objectknoop*)tree)
              && kop(name) == DOT
              )
                {
                Object->object = tree;  /* object == (=) */
                Object->theMethod = findBuiltInMethod((typedObjectknoop *)tree,name->RIGHT);
                /* findBuiltInMethod((=),(insert)) */
                if(Object->theMethod)
                    {
                    return NULL;
                    }
                }

            if(kop(name) == DOT)
                nname = name->LEFT;
            else
                nname = name;
            if(equal(tree->LEFT,nname))
                return NULL;
            else if(nname == name)
                {
                return tree->RIGHT = Head(tree->RIGHT);
                }
            else
                {
                if(Object)
                    Object->self = tree->RIGHT;
                name = name->RIGHT;
                }
            }
        else
            {
            psk tmp;
            if((tmp = getmember(name,tree->LEFT,Object)) != NULL)
                {
                return tmp;
                }
            }
        tree = tree->RIGHT;
        }
    return NULL;
    }

static psk find(psk naamknoop,int *newval,objectStuff * Object)
/*
'naamknoop' is the expression that has to lead to a binding.
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
        The parameter 'newval' is set to TRUE if 'doel' has increased a reference counter. (must be ignored if find returns FALSE.)
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
    vars *navar;
    DBGSRC(Printf("find(");result(naamknoop);Printf(")\n");)
    if(is_op(naamknoop))
        {
        switch(kop(naamknoop))
            {
            case EQUALS: /* Lambda function: (=.out$!arg)$HELLO -> naamknoop == (=.out$!arg) */
                {
                *newval = TRUE;
                naamknoop->RIGHT = Head(naamknoop->RIGHT);
                return zelfde_als_w(naamknoop->RIGHT);
                }
            case DOT: /*
                      e.g.

                      (1)
                            x  =  (a=2) (b=3)

                            !(x.a)
                      and
                            !((=  (a=2) (b=3)).a)

                      must give same result.

                      (2)

                      new$hash:?y

                      ((=).insert)$
                      (y..insert)$
                      (!y.insert)$
                      */
                {
                psk tmp;
                psk tmp2;
                psk doel;
                int nieuw = FALSE;
                if(is_op(naamknoop->LEFT))
                    {
                    if(kop(naamknoop->LEFT) == EQUALS) /* naamknoop->LEFT == (=  (a=2) (b=3))   */
                        {
                        if(  Object
                          && ISBUILTIN((objectknoop*)(naamknoop->LEFT))
                          )
                            {
                            Object->theMethod = findBuiltInMethod((typedObjectknoop *)(naamknoop->LEFT),naamknoop->RIGHT);
                                                /* findBuiltInMethod((=),(insert)) */
                            Object->object = naamknoop->LEFT;  /* object == (=) */
                            if(Object->theMethod)
                                {
                                return NULL;
                                }
                            }
                        tmp = naamknoop->LEFT->RIGHT; /* tmp == ((a=2) (b=3))   */
                        }
                    else
                        return NULL;
                    }
                else                                   /* x */
                    {
                    if((tmp = find(naamknoop->LEFT,&nieuw,NULL)) == NULL)
                        return NULL;
                    /*
                    tmp == ((a=2) (b=3))
                    tmp == (=)
                    */
                    }
                if(Object)
                    Object->self = tmp; /* self == ((a=2) (b=3))   */
                /* The number of '=' to reach the method name in 'tmp' must be one greater
                   than the number of '.' that precedes the method name in 'naamknoop->RIGHT'

                   e.g (= (say=.out$!arg)) and (.say) match

                   For built-in methods (definitions of which are not visible) an invisible '=' has to be assumed.

                   The function getmember resolves this.
                */
                tmp2 = getmember(naamknoop->RIGHT,tmp,Object);

                if(tmp2)
                    {
                    *newval = TRUE;
                    doel = zelfde_als_w(tmp2);
                    }
                else
                    doel = NULL;
                assert(!nieuw);
                return doel;
                }
            default:
                {
                *newval = FALSE;
                return NULL;
                }
            }
        }
    else
        {
        for(navar = variabelen[naamknoop->u.obj];
            navar && (STRCMP(VARNAME(navar),POBJ(naamknoop)) < 0);
            navar = navar->next)
            ;
        if(navar && !STRCMP(VARNAME(navar),POBJ(naamknoop))
           && navar->selector <= navar->n
          )
            {
            ppsk self;
            assert(navar->pvaria);
            *newval = FALSE;
            self = Entry(navar->n,navar->selector,&navar->pvaria);
            *self = Head(*self);
            return *self;
            }
        else
            {
            return NULL;
            }
        }
    }

static int deleteNode(psk name)
    {
    vars *navar,*voorvar;
    varia *hv;
    if(zoeknaam(name,
        &voorvar,
        &navar))
        {
        psk tmp;
        assert(navar->pvaria);
        tmp = Entry2(navar->n,navar->n,navar->pvaria);
        wis(tmp);
        if(navar->n)
            {
            if((navar->n)-1 < macht2(navar->n))
                {
                hv = navar->pvaria;
                navar->pvaria = hv->prev;
                bfree(hv);
                }
            navar->n--;
            if(navar->n < navar->selector)
                navar->selector = navar->n;
            }
        else
            {
            if(voorvar)
                voorvar->next = navar->next;
            else
                variabelen[*POBJ(name)] = navar->next;
#if PVNAME
            if(navar->vname != OBJ(nilk))
                bfree(navar->vname);
#endif
            bfree(navar);
            }
        return TRUE;
        }
    else
        return FALSE;
    }

static psk Naamwoord_w(psk variabele,int twolevelsofindirection);

/* changed signature. Before, naamwoord and naamwoord_w returned TRUE
  or FALSE, while the binding was returned in a pointer-to-a-pointer.*/
static psk Naamwoord(psk variabele,int *newval,int twolevelsofindirection)
    {
    psk pbinding;
    if((pbinding = find(variabele,newval,NULL)) != NULL)
        {
        if(twolevelsofindirection)
            {
            psk peval;
            
            if(pbinding->v.fl & INDIRECT)
                {
                peval = subboomcopie(pbinding);
                peval = eval(peval);
                if(  !isSUCCESS(peval)
                  || (  is_op(peval)
                     && kop(peval) != EQUALS
                     && kop(peval) != DOT
                     )
                  )
                    {
                    wis(peval);
                    return 0;
                    }
                if(*newval)
                    wis(pbinding);
                *newval = TRUE;
                pbinding = peval;
                }
            if(is_op(pbinding))
                {
                if(is_object(pbinding))
                    {
                    peval = zelfde_als_w(pbinding);
                    }
                else
                    {
                    peval = subboomcopie(pbinding);
                    /*
                    a=b=(c.d)
                    c=(d=e)
                    f:?!!(a.b)
                    !e 
                    f
                    */
                    peval = eval(peval);
                    if(  !isSUCCESS(peval)
                      || (  is_op(peval)
                         && kop(peval) != EQUALS
                         && kop(peval) != DOT
                         )
                      )
                        {
                        wis(peval);
                        if(*newval)
                            {
                            *newval = FALSE;
                            wis(pbinding);
                            }
                        return NULL;
                        }
                    }
                assert(pbinding);
                if(*newval)
                    {
                    *newval = FALSE;
                    wis(pbinding);
                    }
                pbinding = Naamwoord(peval,newval,(peval->v.fl & DOUBLY_INDIRECT));
                wis(peval);
                }
            else
                {
                int newv = *newval;
                psk binding;
                *newval = FALSE;
                binding = Naamwoord(pbinding,newval,pbinding->v.fl & DOUBLY_INDIRECT);
                if(newv)
                    {
                    wis(pbinding);
                    }
                pbinding = binding;
                }
            }
        }
    return pbinding;
    }

static psk Naamwoord_w(psk variabele,int twolevelsofindirection)
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
    DBGSRC(printf("Naamwoord_w(");result(variabele);printf(")\n");)
    if((pbinding = Naamwoord(variabele,&newval,twolevelsofindirection)) != NULL)
        {
        unsigned int nameflags,valueflags;
        nameflags = (variabele->v.fl & (ERFENIS|SUCCESS));
        if(ANYNEGATION(variabele->v.fl))
            nameflags |= NOT;

        valueflags = (pbinding)->v.fl;
        valueflags |= (nameflags & (ERFENIS|NOT));
        valueflags ^= ((nameflags & SUCCESS) ^ SUCCESS);

        assert(pbinding != NULL);
        DBGSRC(printf("pbinding:");result(pbinding);printf("\n");)

        if(kop(pbinding) == EQUALS)
            {
            if(!newval)
                {
                pbinding->RIGHT = Head(pbinding->RIGHT);
                pbinding = zelfde_als_w(pbinding);
                }
            }
        else if((pbinding)->v.fl == valueflags)
            {
            if(!newval)
                {
                pbinding = zelfde_als_w(pbinding);
                }
            }
        else
            {
            assert(kop(pbinding) != EQUALS);
            if(newval)
                {
                DBGSRC(printf("prive\n");)
                pbinding = prive(pbinding);
                }
            else
                {
                DBGSRC(printf("subboomcopie\n");)
                pbinding = subboomcopie(pbinding);
                }
            (pbinding)->v.fl = valueflags & ~ALL_REFCOUNT_BITS_SET;
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
    DBGSRC(printf("stringOncePattern:");result(pat);printf("\n");)
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
    else if (  IS_VARIABLE(pat)
            || NIKS(pat)
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
        switch(kop(pat))
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
    if((pat->v.fl & ATOM) && ONTKENNING(pat->v.fl, ATOM))
        {
        return FALSE;
        }
    else if(pat->v.fl & ATOMFILTERS)
        {
        pat->v.fl |= IMPLIEDFENCE;
        return TRUE;
        }
    else if (  IS_VARIABLE(pat)
            || NIKS(pat)
            || (pat->v.fl & NONIDENT) /*{?} a b c:% c => a b c */
            )
        return FALSE;
    else if(!is_op(pat))
        {
        pat->v.fl |= IMPLIEDFENCE;
        return TRUE;
        }
    else
        switch(kop(pat))
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
        unsigned int bsave                      :8;

        unsigned int blmr_true                  :1;
        unsigned int blmr_success               :1; /* SUCCESS */
        unsigned int blmr_pristine              :1;
        unsigned int blmr_once                  :1;
        unsigned int blmr_position_once         :1;
        unsigned int blmr_position_max_reached  :1;
        unsigned int blmr_fence                 :1; /* FENCE */
        unsigned int blmr_unused_15             :1;

        unsigned int brmr_true                  :1;
        unsigned int brmr_success               :1; /* SUCCESS */
        unsigned int brmr_pristine              :1;
        unsigned int brmr_once                  :1;
        unsigned int brmr_position_once         :1;
        unsigned int brmr_position_max_reached  :1;
        unsigned int brmr_fence                 :1; /* FENCE */
        unsigned int brmr_unused_23             :1;

        unsigned int unused_24_26               :3;
        unsigned int bonce                      :1;
        unsigned int unused_28_31               :4;
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
static void printMatchState(const char * msg,matchstate s,int pos,int len)
    {
    Printf("\n%s pos %d len %d once %d",msg,pos,len,s.b.bonce);
    Printf("\n     t o p m f i");
    Printf("\n lmr %d %d %d %d %d %d",
        s.b.blmr_true,s.b.blmr_once,s.b.blmr_position_once,s.b.blmr_position_max_reached,s.b.blmr_fence,s.b.blmr_pristine);
    Printf("\n rmr %d %d %d %d %d %d\n",
        s.b.brmr_true,s.b.brmr_once,s.b.brmr_position_once,s.b.brmr_position_max_reached,s.b.brmr_fence,s.b.brmr_pristine);
    }
#endif
#endif

static LONG expressionLength(psk pkn,unsigned int op)
    {
    if(!is_op(pkn) && pkn->u.lobj == knil[op >> OPSH]->u.lobj)
        return 0;
    else
        {
        LONG len = 1;
        while(kop(pkn) == op)
            {
            ++len;
            pkn = pkn->RIGHT;
            }
        return len;
        }
    }

static char doPosition(matchstate s,psk pat,LONG pposition,size_t stringLength,psk expr
#if CUTOFFSUGGEST
                      ,char ** mayMoveStartOfSubject
#endif
                      ,unsigned int op
                       )
    {
    unsigned int Flgs;
    psk name;
    LONG pos;
    Flgs = pat->v.fl;
#if CUTOFFSUGGEST
    if(  ((Flgs & (SUCCESS|VISIBLE_FLAGS_POS0|IS_OPERATOR)) == (SUCCESS|QGETAL))
      && mayMoveStartOfSubject 
      && *mayMoveStartOfSubject != 0
      )
        {
        pos = toLong(pat); /* [20 */
        if(pos < 0)
            pos += (expr == NULL ? (LONG)stringLength : expr ? expressionLength(expr,op) : 0) + 1; /* [(20+-1*(!len+1)) -> `-7 */

        if(  pposition < pos 
          && (MORE_EQUAL(pat) || EQUAL(pat) || NOTLESSORMORE(pat))
          )
            {
            if((long)stringLength > pos)
                *mayMoveStartOfSubject += pos - pposition;
            s.c.rmr = FALSE; /* [20 */
            return s.c.rmr;
            }
        else if(  pposition <= pos 
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
    Flgs = pat->v.fl & (UNIFY|INDIRECT|DOUBLY_INDIRECT);

    DBGSRC(printf("patA:");result(pat);printf("\n");)
    name = subboomcopie(pat);
    name->v.fl |= SUCCESS;
    if((Flgs & UNIFY) && (is_op(pat) || (Flgs & INDIRECT)))
        {
        name->v.fl &= ~VISIBLE_FLAGS;
        if(!is_op(name))
            name->v.fl |= READY;
        s.c.rmr = (char)evalueer(name) & TRUE;

        if (!(s.c.rmr))
            {
            wis(name);
            return FALSE;
            }
        }
    else
        {
        DBGSRC(printf("patA:");result(pat);printf("\n");)
        s.c.rmr = (char)evalueer(name) & TRUE;

        if (!(s.c.rmr))
            { 
            wis(name);
            return FALSE;
            }

        Flgs = pat->v.fl & UNIFY;
        Flgs |= name->v.fl;
        }
    pat = name;
    DBGSRC(printf("patB:");result(pat);printf("\n");)
    if(Flgs & UNIFY)
        {
        if (  is_op(pat)
           || pat->u.obj
           )
            {
            if (Flgs & INDIRECT)        /* ?! of ?!! */
                {
                psk loc;
                if ((loc=Naamwoord_w(pat,Flgs & DOUBLY_INDIRECT)) != NULL)
                    {
                    if (is_object(loc))
                        s.c.rmr = (char)icopy_insert(loc,pposition);
                    else
                        {
                        s.c.rmr = (char)evalueer(loc) & (TRUE|FENCE);
                        if(!icopy_insert(loc,pposition))
                            s.c.rmr = FALSE;
                        }
                    wis(loc);
                    }
                else
                    s.c.rmr = FALSE;
                }
            else
                {
                s.c.rmr = (char)icopy_insert(pat,pposition); /* [?a */
                }
            }
        else
            s.c.rmr = TRUE;

        if(name)
            wis(name); /* [?a */
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

    if( ((pat->v.fl & (SUCCESS|VISIBLE_FLAGS_POS0|IS_OPERATOR)) == (SUCCESS|QGETAL)))
        {
        pos = toLong(pat); /* [20 */
        DBGSRC(Printf("pat:");result(pat);Printf("\n");)
        if(pos < 0)
            pos += (expr == NULL ? (LONG)stringLength : expressionLength(expr,op)) + 1; /* [(20+-1*(!len+1)) -> `-7 */
        if(LESS(pat))
            { /* [<18 */
            if(pposition < pos)
                {
                s.c.rmr = TRUE;/* [<18 */
                }
            else
                {
                s.c.rmr = FALSE|POSITION_MAX_REACHED;
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
                s.c.rmr = TRUE|POSITION_MAX_REACHED;
                }
            else
                {
                s.c.rmr = FALSE|POSITION_MAX_REACHED;
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
                s.c.rmr = TRUE|POSITION_MAX_REACHED; /* [20 */
                }
            else if(pposition > pos)
                {
                s.c.rmr = FALSE|POSITION_MAX_REACHED;
                }
            else
                s.c.rmr = FALSE; /* [20 */
            }
        }
    else
        {
        s.c.rmr = FALSE;
        }
    wis(pat); /* [20 */
    s.c.rmr |= ONCE | POSITION_ONCE;
    return s.c.rmr;
    }

static int atomtest(psk kn)
    {
    return (!is_op(kn) && !HAS_UNOPS(kn)) ? (int)kn->u.obj : -1;
    }

static char sdoEval(char * sub,char * snijaf, psk pat, psk subkn)
    {
    char ret;
    psk loc;
    psh(&sjt,&nilk,NULL);
    string_copy_insert(&sjt,subkn,sub,snijaf);
    loc = subboomcopie(pat);
    loc->v.fl &= ~(POSITION|NONIDENT|IMPLIEDFENCE|ONCE);
    loc = eval(loc);
    deleteNode(&sjt);
    if(isSUCCESS(loc))
        {
        ret = (loc->v.fl & FENCE) ? (TRUE|ONCE) : TRUE;
        }
    else
        {
        ret = (loc->v.fl & FENCE) ? ONCE : FALSE;
        }
    wis(loc);
    return ret;
    }

/* 
    ( Dogs and Cats are friends: ? [%(out$(!sjt SJT)&~) (|))&
    ( Dogs and Cats are friends: ? [%(out$(!sjt)&~) (|))&
*/
static char doEval(psk sub,psk snijaf, psk pat)
    {
    char ret;
    psk loc;
    psh(&sjt,&nilk,NULL);
    copy_insert(&sjt, sub, snijaf);
    loc = subboomcopie(pat);
    loc->v.fl &= ~(POSITION|NONIDENT|IMPLIEDFENCE|ONCE);
    loc = eval(loc);
    deleteNode(&sjt);
    if(isSUCCESS(loc))
        {
        ret = (loc->v.fl & FENCE) ? (TRUE|ONCE) : TRUE;
        }
    else
        {
        ret = (loc->v.fl & FENCE) ? ONCE : FALSE;
        }
    wis(loc);
    
    return ret;
    }


#if CUTOFFSUGGEST
static char stringmatch
        (int ind
        ,char * wh
        ,char * sub
        ,char * snijaf
        , psk pat
        , psk subkn
        , LONG pposition
        ,size_t stringLength
        ,char ** suggestedCutOff 
        ,char ** mayMoveStartOfSubject
        )
#else
static char stringmatch
        (int ind
        ,char * wh
        ,unsigned char * sub
        ,unsigned char * snijaf
        , psk pat
        , psk subkn
        , LONG pposition
        ,size_t stringLength
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
    char * sloc;
    unsigned int Flgs;
    matchstate s;
    int ci;
    psk name = NULL;
    assert(sizeof(s) == 4);
    if(!snijaf)
        snijaf = sub+stringLength;
#if CUTOFFSUGGEST
    if(  (pat->flgs & ATOM)
      ||    (NIKS(pat) 
         && (  is_op(pat) 
            || !pat->u.obj)
            )
      )
        {
        suggestedCutOff = NULL;
        }
#endif
    DBGSRC(int redMooi;int redhum;redMooi = mooi;redhum = hum;mooi = FALSE;\
        hum = FALSE;Printf("%d  %.*s|%s",ind,snijaf-sub,sub,snijaf);\
        Printf(":");result(pat);\
        Printf  (",pos=%ld,sLen=%ld,sugCut=%s,mayMoveStart=%s)"\
                ,pposition\
                ,(long int)stringLength\
                ,suggestedCutOff ? *suggestedCutOff ? *suggestedCutOff : (char*)"(0)" : (char*)"0"\
                ,mayMoveStartOfSubject ? *mayMoveStartOfSubject ? *mayMoveStartOfSubject : (char*)"(0)" : (char*)"0"\
                );\
        Printf("\n");mooi = redMooi;hum = redhum;)
    s.i = (PRISTINE << SHIFT_LMR) + (PRISTINE << SHIFT_RMR);

    Flgs = pat->v.fl;
    if(Flgs & POSITION)
        {
        if(Flgs & NONIDENT)
            return sdoEval(sub,snijaf,pat,subkn);
        else if(snijaf > sub)
            {
#if CUTOFFSUGGEST
            if(mayMoveStartOfSubject && *mayMoveStartOfSubject)
                {
                *mayMoveStartOfSubject = snijaf;
                }
#endif
            return FALSE | ONCE | POSITION_ONCE;
            }
        else
            return doPosition(s,pat,pposition,stringLength,NULL
#if CUTOFFSUGGEST
            ,mayMoveStartOfSubject
#endif
            ,12345
            );
        }
    if(!(  (  (Flgs & NONIDENT)
           && ( ONTKENNING(Flgs, NONIDENT)
              ? ( (s.c.once = ONCE)
                , snijaf > sub
                )
              : snijaf == sub
              )
           )
        || (  (Flgs & ATOM)
           && ( ONTKENNING(Flgs, ATOM)
              ?    (snijaf < sub + 2) /*!(sub[0] && sub[1])*/
              :    snijaf > sub
                && ( (s.c.once = ONCE)
                   , snijaf > sub + 1 /*sub[1]*/
                   )
              )
           )
        || (  (Flgs & (BREUK|NUMBER))
           && ( (ci = sfullnumbercheck(sub,snijaf))
              , (  (  (Flgs & BREUK)
                   && ((ci != (QBREUK | QGETAL)) ^ ONTKENNING(Flgs, BREUK))
                   )
                || (  (Flgs & NUMBER)
                   && (((ci & QGETAL) == 0)      ^ ONTKENNING(Flgs, NUMBER))
                   )
                )
              )
           && (  s.c.rmr = (ci == DEFINITELYNONUMBER) ? ONCE : FALSE
              ,  (s.c.lmr = PRISTINE)
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
                unsigned int saveflgs = Flgs & VISIBLE_FLAGS;
                name = subboomcopie(pat);
                name->v.fl &= ~VISIBLE_FLAGS;
                name->v.fl |= SUCCESS;
                if ((s.c.rmr = (char)evalueer(name)) != TRUE)
                    ok = FALSE;
                name->v.fl |= saveflgs;
                pat = name;
                }
            if(ok)
                {
                if (Flgs & UNIFY)        /* ?  */
                    {
                    if (!NIKS(pat) || snijaf > sub)
                        {
                        if (  is_op(pat)
                           || pat->u.obj
                           )
                            {
                            if (Flgs & INDIRECT)        /* ?! of ?!! */
                                {
                                if ((loc=Naamwoord_w(pat,Flgs & DOUBLY_INDIRECT)) != NULL)
                                    {
                                    if (is_object(loc))
                                        s.c.rmr = (char)string_copy_insert(loc,subkn,sub,snijaf);
                                    else
                                        {
                                        s.c.rmr = (char)evalueer(loc);
                                        if(!string_copy_insert(loc,subkn,sub,snijaf))
                                            s.c.rmr = FALSE;
                                        }
                                    wis(loc);
                                    }
                                else
                                    s.c.rmr = (char)NIKS(pat);
                                }
                            else
                                {
                                s.c.rmr = (char)string_copy_insert(pat,subkn,sub,snijaf);
                                }
                            }
                        else
                            s.c.rmr = TRUE;
                        }
                    }
                else if (Flgs & INDIRECT)        /* ! or !! */
                    {
                    if ((loc=Naamwoord_w(pat,Flgs & DOUBLY_INDIRECT)) != NULL)
                        {
                        cleanOncePattern(loc);
#if CUTOFFSUGGEST
                        if(mayMoveStartOfSubject)
                            {
                            *mayMoveStartOfSubject = 0;
                            }
                        s.c.rmr = (char)(stringmatch(ind+1,"A",sub,snijaf,loc,subkn,pposition,stringLength
                            ,suggestedCutOff
                            ,0
                            ) ^ NIKS(pat));
#else
                        s.c.rmr = (char)(stringmatch(ind+1,"A",sub,snijaf,loc,subkn,pposition,stringLength
                            ) ^ NIKS(pat));
#endif
                        wis(loc);
                        }
                    else
                        s.c.rmr = (char)NIKS(pat);
                    }
                }
            }
        else
            {
            switch (kop(pat))
                {
                case PLUS:
                case TIMES:
                    break;
                case WHITE:
                    {
                    LONG locpos = pposition;
#if CUTOFFSUGGEST
                    char * suggested_Cut_Off = sub;
                    char * may_Move_Start_Of_Subject;
                    may_Move_Start_Of_Subject = sub;
#endif
                    /* This code mirrors that of match(). (see below)*/
                    
                    sloc = sub;                                     /* A    divisionPoint=S */
                                                                    
#if CUTOFFSUGGEST
                    s.c.lmr = stringmatch(ind+1,"I",sub, sloc       /* B    leftResult=0(P):car(P) */
                                ,pat->LEFT,subkn,pposition
                                ,stringLength,&suggested_Cut_Off
                                ,mayMoveStartOfSubject);
                    if((s.c.lmr & ONCE) && mayMoveStartOfSubject && *mayMoveStartOfSubject > sub)
                        {
                        return ONCE;
                        }
#else
                    s.c.lmr = stringmatch(ind+1,"I",sub, sloc, pat->LEFT, subkn,pposition,stringLength);
#endif
                    
#if CUTOFFSUGGEST
                    if(suggested_Cut_Off > sloc)
                        {
                        if(snijaf && suggested_Cut_Off > snijaf)
                            {
                            if(suggestedCutOff)
                                {
                                locpos += suggested_Cut_Off - sloc;
                                snijaf = sloc = *suggestedCutOff = suggested_Cut_Off;
                                }
                            else
                                {
                                locpos += snijaf - sloc;
                                sloc = snijaf;
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
                    while(sloc < snijaf)                            /* C    while divisionPoint */
                        {
                        if(s.c.lmr & TRUE)                          /* D        if leftResult.success */
                            {
#if CUTOFFSUGGEST
                            if(s.c.lmr & ONCE)
                                may_Move_Start_Of_Subject = 0;
                            else if(may_Move_Start_Of_Subject != 0)
                                may_Move_Start_Of_Subject = sloc;
                            s.c.rmr = stringmatch(ind+1,"J",sloc    /* E            rightResult=SR:cdr(P) */
                                ,snijaf, pat->RIGHT, subkn
                                ,locpos,stringLength,suggestedCutOff
                                ,&may_Move_Start_Of_Subject);
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
                            s.c.rmr = stringmatch(ind+1,"J",sloc,snijaf, pat->RIGHT, subkn,locpos,stringLength);
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
                        if(  (s.c.rmr & TRUE)                       /* F        if(1) full success */
                          || (s.c.lmr & (POSITION_ONCE                  /*     or (2) may not be shifted. In the first pass, a position flag on car(P) counts as criterion for being done. */
                                        | ONCE                          
                                        )                               
                             )                                          /* In all but the first pass, the left and right */
                          || (s.c.rmr & (ONCE                           /* results can indicate that the loop is done.   */
                                        |POSITION_MAX_REACHED           /* In all passes a position_max_reached on the   */
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
                            return s.c.rmr ^ (char)NIKS(pat);
                            }
                                                                    /* H        SL,SR=shift_right divisionPoint */
                                                                        /* SL = lhs divisionPoint S, SR = rhs divisionPoint S */
                                                                    /* I        leftResult=SL:car(P) */
#if CUTOFFSUGGEST
                        suggested_Cut_Off = sub;
                        s.c.lmr = stringmatch(ind+1,"I",sub,sloc, pat->LEFT, subkn,/* 0 ? */pposition,/* strlen(sub) ? */ stringLength,&suggested_Cut_Off,mayMoveStartOfSubject);
                        if(suggested_Cut_Off > sloc)
                            {
                            if(!(snijaf && suggested_Cut_Off > snijaf))
                                {
                                assert(suggested_Cut_Off > sloc);
                                locpos += suggested_Cut_Off - sloc;
                                sloc = suggested_Cut_Off;
                                }
                            }
#else
                        s.c.lmr = stringmatch(ind+1,"I",sub,sloc, pat->LEFT, subkn,/* 0 ? */pposition,/* strlen(sub) ? */ stringLength);
#endif
                        }
                    
                    if(s.c.lmr & TRUE)                              /* J    if leftResult.success */
                        {
#if CUTOFFSUGGEST
                        s.c.rmr = stringmatch(ind+1,"J",sloc,snijaf /* K        rightResult=0(P):cdr(pat) */
                            ,pat->RIGHT, subkn,locpos,stringLength
                            ,suggestedCutOff,mayMoveStartOfSubject);
#else
                        s.c.rmr = stringmatch(ind+1,"J",sloc,snijaf,pat->RIGHT, subkn,locpos,stringLength);
#endif
                        s.c.rmr &= ~ONCE;
                        }
                    /* L    return */
                    if(!(s.c.rmr & POSITION_MAX_REACHED))
                        s.c.rmr &= ~POSITION_ONCE;
                    if(/*(snijaf > sub) &&*/ stringOncePattern(pat))    /* The test snijaf > sub merely avoids that stringOncePattern is called when it is useless. */
                        {/* Test:
                         @(abcde:`(a ?x) (?z:d) ? )
                          z=b
                         */
                        s.c.rmr |= ONCE;
                        s.c.rmr |= (char)(pat->v.fl & FENCE);
                        }
                    return s.c.rmr ^ (char)NIKS(pat);               /* end */
                    }
                case UNDERSCORE:
                    if(snijaf > sub + 1)
                        {
#if CUTOFFSUGGEST
                        s.c.lmr = stringmatch(ind+1,"M",sub,sub+1,pat->LEFT,subkn,pposition,stringLength,NULL,mayMoveStartOfSubject);
#else
                        s.c.lmr = stringmatch(ind+1,"M",sub,sub+1,pat->LEFT,subkn,pposition,stringLength);
#endif
                        if(  (s.c.lmr & TRUE)
#if CUTOFFSUGGEST
                          && ((s.c.rmr = stringmatch(ind+1,"N",sub+1,snijaf,pat->RIGHT, subkn,pposition,stringLength,suggestedCutOff,mayMoveStartOfSubject)) & TRUE)
#else
                          && ((s.c.rmr = stringmatch(ind+1,"N",sub+1,snijaf,pat->RIGHT, subkn,pposition,stringLength)) & TRUE)
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
                    if ((s.c.lmr = stringmatch(ind+1,"O",sub,snijaf, pat->LEFT, subkn,pposition,stringLength,suggestedCutOff,mayMoveStartOfSubject)) & TRUE)
#else
                    if ((s.c.lmr = stringmatch(ind+1,"O",sub,snijaf, pat->LEFT, subkn,pposition,stringLength)) & TRUE)
#endif
                        {
                        loc = zelfde_als_w(pat->RIGHT);
                        loc = eval(loc);
                        if (loc->v.fl & SUCCESS)
                            {
                            s.c.rmr = TRUE;
                            if (loc->v.fl & FENCE)
                                s.c.rmr |= ONCE;
                            }
                        else
                            {
                            s.c.rmr = FALSE;
                            if (loc->v.fl & FENCE)
                                s.c.rmr |= (FENCE | ONCE);
                            if (loc->v.fl & IMPLIEDFENCE) /* (for function utf$) */
                                s.c.rmr |= ONCE;
                            }
                        wis(loc);
                        }
                    s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                    break;
                case MATCH:
#if CUTOFFSUGGEST
                    if ((s.c.lmr = stringmatch(ind+1,"P",sub,snijaf, pat->LEFT, subkn,pposition,stringLength,suggestedCutOff,mayMoveStartOfSubject)) & TRUE)
#else
                    if ((s.c.lmr = stringmatch(ind+1,"P",sub,snijaf, pat->LEFT, subkn,pposition,stringLength)) & TRUE)
#endif
                        {
#if CUTOFFSUGGEST
                        if(suggestedCutOff && *suggestedCutOff > snijaf)
                            {
                            snijaf = *suggestedCutOff;
                            }

                        s.c.rmr = (char)(stringmatch(ind+1,"Q",sub,snijaf,pat->RIGHT,subkn,pposition,stringLength,0,0));
#else
                        s.c.rmr = (char)(stringmatch(ind+1,"Q",sub,snijaf,pat->RIGHT,subkn,pposition,stringLength));
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
                    if ( (s.c.lmr = (char)( stringmatch(ind+1,"R",sub,snijaf,pat->LEFT,subkn,pposition,stringLength,NULL,0)))
#else
                    if ( (s.c.lmr = (char)( stringmatch(ind+1,"R",sub,snijaf,pat->LEFT,subkn,pposition,stringLength)))
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
                            s.c.rmr = (char)(s.c.lmr & (TRUE|ONCE));
                            }
                        }
                    else
                        {
#if CUTOFFSUGGEST
                        s.c.rmr = stringmatch(ind+1,"S",sub,snijaf,pat->RIGHT, subkn,pposition,stringLength,NULL,0);
#else
                        s.c.rmr = stringmatch(ind+1,"S",sub,snijaf,pat->RIGHT, subkn,pposition,stringLength);
#endif
                        if(  (s.c.rmr & ONCE)
                          && !(s.c.lmr & ONCE)
                          )
                            {
                            s.c.rmr &= ~(ONCE|POSITION_ONCE);
                            }
                        if(  (s.c.rmr & POSITION_MAX_REACHED)
                          && !(s.c.lmr & POSITION_MAX_REACHED)
                          )
                            {
                            s.c.rmr &= ~(POSITION_MAX_REACHED|POSITION_ONCE);
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
                    psh(&sjt,&nilk,NULL);
                    string_copy_insert(&sjt,subkn,sub,snijaf);
                    loc = zelfde_als_w(pat);
                    loc = eval(loc);
                    deleteNode(&sjt);
#if CUTOFFSUGGEST
                    if(mayMoveStartOfSubject)
                        *mayMoveStartOfSubject = 0;
#endif
                    if(isSUCCESS(loc))
                        {
                        loc = setflgs(loc,pat->v.fl);
                        if (equal(pat, loc))
                            {
#if CUTOFFSUGGEST
                            s.c.rmr = (char)(stringmatch(ind+1,"T",sub,snijaf,loc,subkn,pposition,stringLength,NULL,0) ^ NIKS(loc));
#else
                            s.c.rmr = (char)(stringmatch(ind+1,"T",sub,snijaf,loc,subkn,pposition,stringLength) ^ NIKS(loc));
#endif
                            wis(loc);
                            break;
                            }
                        }
                    else
                        {
                        if(loc->v.fl & (FENCE|IMPLIEDFENCE))
                            s.c.rmr = ONCE; /* '~ as return value from function stops stretching subject */
                        }
                    wis(loc);
                    break;
                default:
                    if(!is_op(pat))
                        {
                        if (  !pat->u.obj
                           && (Flgs & (BREUK | NUMBER | NONIDENT | ATOM | IDENT))
                           )
                            {         /* e.g.    a b c : % */
                            s.c.rmr = TRUE;
                            }
                        else
                            {
#if CUTOFFSUGGEST
                            s.c.rmr = (char)(scompare("b",(char *)sub,snijaf, pat
                                                , ( (  !(Flgs & ATOM)
                                                    || ONTKENNING(Flgs, ATOM)
                                                    ) 
                                                    ? suggestedCutOff
                                                    : NULL
                                                    )
                                                ,mayMoveStartOfSubject
                                                )
                                             );
#else
                            s.c.rmr = (char)(scompare("b",(unsigned char *)sub,snijaf, pat));
#endif
                            DBGSRC(Printf("%s %d%*sscompare(%.*s,",wh,ind,ind,"",snijaf-sub,sub);result(pat);Printf(") ");\
                                if(s.c.rmr & ONCE) Printf("ONCE|");\
                                if(s.c.rmr & TRUE) Printf("TRUE"); else Printf("FALSE");\
                                Printf("\n");)
                            }
                        }
                }
            }
        }
    DBGSRC(if(s.c.rmr & (FENCE | ONCE))\
        {Printf("%s %d%*s+",wh,ind,ind,"");if(s.c.rmr & FENCE)\
         Printf(" FENCE ");if(s.c.rmr & ONCE)Printf(" ONCE ");Printf("\n");})
    s.c.rmr |= (char)(pat->v.fl & FENCE);
    if(stringOncePattern(pat) || /* @("abXk":(|? b|`) X ?id) must fail*/ (s.c.rmr & (TRUE|FENCE|ONCE)) == FENCE)
        {
        s.c.rmr |= ONCE;
        DBGSRC(int redMooi;int redhum;redMooi = mooi;redhum = hum;\
            mooi = FALSE;hum = FALSE;\
            Printf("%d%*sstringmatch(%.*s",ind,ind,"",snijaf-sub,sub);\
            Printf(":");result(pat);\
            mooi = redMooi;hum = redhum;\
            Printf(") s.c.rmr %d (B)",s.c.rmr);\
            if(pat->v.fl & POSITION) Printf("POSITION ");\
            if(pat->v.fl & BREUK)Printf("BREUK ");\
            if(pat->v.fl & NUMBER)Printf("NUMBER ");\
            if(pat->v.fl & SMALLER_THAN)Printf("SMALLER_THAN ");\
            if(pat->v.fl & GREATER_THAN) Printf("GREATER_THAN ");\
            if(pat->v.fl & ATOM)  Printf("ATOM ");\
            if(pat->v.fl & FENCE) Printf("FENCE ");\
            if(pat->v.fl & IDENT) Printf("IDENT");\
            Printf("\n");)
        }
    if(is_op(pat))
        s.c.rmr ^= (char)NIKS(pat);
    if(name)
        wis(name);
    return (char)(s.c.once | s.c.rmr);
    }



static char match(int ind,psk sub, psk pat, psk snijaf,LONG pposition,psk expr,unsigned int op)
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

FENCE      Onbereidheid van het subject om door alternatieve patronen gematcht
           te worden. Van belang voor de | en : operatoren in een patroon.
           Wordt aangezet door ` vlag (al dan niet in een patroon).
           Wordt uitgezet in patroon met spatie + * | of : operator.
           (Bij | en : operatoren geldt dit alleen voor de linkeroperand,
           bij de andere voor alle behalve de laatste operand in een lijst.)
*/
    matchstate s;
    psk loc;
    unsigned int Flgs;
    psk name = NULL;
    DBGSRC(Printf("%d%*smatch(",ind,ind,"");results(sub,snijaf);Printf(":");\
        result(pat);Printf(")");Printf("\n");)
    if (is_op(sub))
        {
        if(kop(sub) == EQUALS)
            sub->RIGHT = Head(sub->RIGHT);

        if (sub->RIGHT == snijaf)
            return match(ind+1,sub->LEFT, pat, NULL,pposition,expr,op);
        }
    s.i = (PRISTINE << SHIFT_LMR) + (PRISTINE << SHIFT_RMR);
    Flgs = pat->v.fl;
    if(Flgs & POSITION)
        {
        if(Flgs & NONIDENT)
            return doEval(sub,snijaf,pat);
        else if(snijaf || !(sub->v.fl & IDENT))
            return FALSE | ONCE | POSITION_ONCE;
        else
            return doPosition(s,pat,pposition,0,expr
#if CUTOFFSUGGEST
                   ,0
#endif
                   ,op
                   );
        }
    if ( !(  ((Flgs & NONIDENT) && (((sub->v.fl & IDENT) && 1) ^ ONTKENNING(Flgs, NONIDENT)))
          || ((Flgs & ATOM    ) && ((is_op(sub)          && 1) ^ ONTKENNING(Flgs, ATOM    )))
          || ((Flgs & BREUK   ) && ( !RAT_RAT(sub)             ^ ONTKENNING(Flgs, BREUK   )))
          || ((Flgs & NUMBER  ) && ( !RATIONAAL_COMP(sub)      ^ ONTKENNING(Flgs, NUMBER  )))
          )
       )
        {
        if(IS_VARIABLE(pat))
            {
            int ok = TRUE;
            if(is_op(pat))
                {
                unsigned int saveflgs = Flgs & VISIBLE_FLAGS;
                DBGSRC(printf("pat:");result(pat);printf("\n");)
                name = subboomcopie(pat);
                name->v.fl &= ~VISIBLE_FLAGS;
                name->v.fl |= SUCCESS;
                if ((s.c.rmr = (char)evalueer(name)) != TRUE)
                    ok = FALSE;
                name->v.fl |= saveflgs;
                pat = name;
                }
            if(ok)
                {
                if (Flgs & UNIFY)        /* ?  */
                    {
                    if (!NIKS(pat) || is_op(sub) || (sub->u.obj))
                        {
                        if (  is_op(pat)
                           || pat->u.obj
                           )
                            if (Flgs & INDIRECT)        /* ?! of ?!! */
                                {
                                if ((loc=Naamwoord_w(pat,Flgs & DOUBLY_INDIRECT)) != NULL)
                                    {
                                    if (is_object(loc))
                                        s.c.rmr = (char)copy_insert(loc, sub, snijaf);
                                    else
                                        {
                                        s.c.rmr = (char) evalueer(loc);
                                        if(!copy_insert(loc, sub, snijaf))
                                            s.c.rmr = FALSE;
                                            /* Previously, s.c.rmr was not influenced by failure of copy_insert */

                                        }
                                    wis(loc);
                                    }
                                else
                                    s.c.rmr = (char)NIKS(pat);
                                }
                            else
                                {
                                s.c.rmr = (char)copy_insert(pat, sub, snijaf);
                                /* Previously, s.c.rmr was unconditionally set to TRUE */
                                }

                        else
                            s.c.rmr = TRUE;
                        }
                    /*
                     * else NIKS(pat) && !is_op(sub) && !sub->u.obj
                     * which means   ~?[`][!][!]
                     */
                    }
                else if (Flgs & INDIRECT)        /* ! or !! */
                    {
                    if ((loc=Naamwoord_w(pat,Flgs & DOUBLY_INDIRECT)) != NULL)
                        {
                        cleanOncePattern(loc);
                        s.c.rmr = (char)(match(ind+1,sub, loc, snijaf,pposition,expr,op) ^ NIKS(pat));
                        wis(loc);
                        }
                    else
                        s.c.rmr = (char)NIKS(pat);
                    }
                }
            }
        else
            switch (kop(pat))
                {
                case WHITE:
                case PLUS:
                case TIMES:
                    {
                    LONG locpos = pposition;
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
                    if(SUBJECTNOTNIL(sub,pat))                      /* A    divisionPoint=S */
                        loc = sub;
                    else
                        loc = NULL;
                                                                    /* B    leftResult=0(P):car(P) */
                    s.c.lmr = (char)match(ind+1,nil(pat),pat->LEFT  /* a*b+c*d:?+[1+?*[1*%@?q*?+?            (q = c) */
                                         ,NULL,pposition,expr       /* a b c d:? [1 (? [1 %@?q ?) ?          (q = b) */
                                         ,kop(pat));                /* a b c d:? [1  ? [1 %@?q ?  ?          (q = b) */
                    s.c.lmr &= ~ONCE;                               
                    while(loc)                                      /* C    while divisionPoint */  
                        {
                        if(s.c.lmr & TRUE)                          /* D        if leftResult.success */
                            {                                       /* E            rightResult=SR:cdr(P) */
                            s.c.rmr = match(ind+1,loc,pat->RIGHT,snijaf,locpos,expr,op);
                            if(!(s.c.lmr & ONCE))
                                s.c.rmr &= ~ONCE;
                            }
                        if(  (s.c.rmr & TRUE)                       /* F        if(1) full success */
                          || (s.c.lmr & (POSITION_ONCE                  /*     or (2) may not be shifted. In the first pass, a position flag on car(P) counts as criterion for being done. */
                                        | ONCE                          
                                        )                               
                             )                                          /* In all but the first pass, the left and right */
                          || (s.c.rmr & (ONCE                           /* results can indicate that the loop is done.   */
                                        |POSITION_MAX_REACHED           /* In all passes a position_max_reached on the   */
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
                            DBGSRC(Printf("%d%*smatch(",ind,ind,"");\
                                results(sub,snijaf);Printf(":");result(pat);)
#ifndef NDEBUG
                            DBGSRC(printMatchState("EXIT-MID",s,pposition,0);)
#endif
                            DBGSRC(if(pat->v.fl & BREUK) Printf("BREUK ");\
                                if(pat->v.fl & NUMBER) Printf("NUMBER ");\
                                if(pat->v.fl & SMALLER_THAN)\
                                    Printf("SMALLER_THAN ");\
                                if(pat->v.fl & GREATER_THAN)\
                                    Printf("GREATER_THAN ");\
                                if(pat->v.fl & ATOM) Printf("ATOM ");\
                                if(pat->v.fl & FENCE) Printf("FENCE ");\
                                if(pat->v.fl & IDENT) Printf("IDENT");\
                                Printf("\n");)
                            return s.c.rmr ^ (char)NIKS(pat);
                            }
                    /* H        SL,SR=shift_right divisionPoint */
                        if(  kop(loc) == kop(pat)
                          && loc->RIGHT != snijaf
                          )
                            loc = loc->RIGHT;
                        else
                            loc = NULL;
                        /* SL = lhs divisionPoint S, SR = rhs divisionPoint S
                        */
                        ++locpos;
                    /* I        leftResult=SL:car(P) */
                        s.c.lmr = match(ind+1,sub, pat->LEFT, loc,pposition,sub,kop(pat));
                        }
                    /* J    if leftResult.success */
                    if(s.c.lmr & TRUE)
                    /* K        rightResult=0(P):cdr(pat) */
                        {
                        s.c.rmr = match(ind+1,nil(pat),pat->RIGHT, NULL,locpos,expr,kop(pat));
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
                    if(/*snijaf &&*/ oncePattern(pat))
                        /* The test snijaf != NULL merely avoids that
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
                    s.c.rmr ^= (char)NIKS(pat);
                    return s.c.rmr;
                    /* end */
                    }
                case EXP:
                    if(kop(sub) == EXP)
                        {
                        if ((s.c.lmr = match(ind+1,sub->LEFT, pat->LEFT, NULL,0,sub->LEFT,12345)) & TRUE)
                            s.c.rmr = match(ind+1,sub->RIGHT, pat->RIGHT, NULL,0,sub->RIGHT,12345);
#ifndef NDEBUG
                            DBGSRC(printMatchState("EXP:EXIT-MID",s,pposition,0);)
#endif
                        s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE)); /* a*b^2*c:?x*?y^(~1:?t)*?z */
                        }
                    if  ( !(s.c.rmr & TRUE)
                        && ((s.c.lmr = match(ind+1,sub, pat->LEFT, snijaf,pposition,expr,op)) & TRUE)
                        && ((s.c.rmr = match(ind+1,&eenk, pat->RIGHT, NULL,0,&eenk,1234567)) & TRUE)
                        )
                        { /* a^2*b*c*d^3 : ?x^(?s:~1)*?y^?t*?z^(>2:?u) */
                        s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                        }
                    s.c.rmr &= ~POSITION_MAX_REACHED;
                    break;
                case UNDERSCORE:
                    if (is_op(sub))
                        {
                        if(kop(sub) == EQUALS)
                            {
                            if(ISBUILTIN((objectknoop*)sub))
                                {
                                errorprintf("You cannot match an object '=' with '_' if the object is built-in\n");
                                s.c.rmr = ONCE;
                                }
                            else
                                {
                                if((s.c.lmr = match(ind+1,sub->LEFT, pat->LEFT, NULL,0,sub->LEFT,12345)) & TRUE)
                                    {
                                    loc = zelfde_als_w(sub->RIGHT); /* Object might change as a side effect!*/
                                    if((s.c.rmr = match(ind+1,loc, pat->RIGHT, snijaf,0,loc,123)) & TRUE)
                                        {
                                        dummy_op = kop(sub);
                                        }
                                    wis(loc);
                                    }
                                }
                            }
                        else if(  ((s.c.lmr = match(ind+1,sub->LEFT, pat->LEFT, NULL,0,sub->LEFT,12345)) & TRUE)
                               && ((s.c.rmr = match(ind+1,sub->RIGHT, pat->RIGHT, snijaf,0,sub->RIGHT,123)) & TRUE)
                               )
                            {
                            dummy_op = kop(sub);
                            }
#ifndef NDEBUG
                        DBGSRC(printMatchState("UNDERSCORE:EXIT-MID",s,pposition,0);)
#endif
                            switch( kop(sub))
                            {
                            case WHITE:
                            case PLUS:
                            case TIMES:
                                break;
                            default:
                                s.c.rmr &= ~POSITION_MAX_REACHED;
                            }
#ifndef NDEBUG
                        DBGSRC(printMatchState("streep:EXIT-MID",s,pposition,0);)
#endif
                        }
                    if (s.c.lmr != PRISTINE)
                        s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                    break;
                case AND:
                    if ((s.c.lmr = match(ind+1,sub, pat->LEFT, snijaf,pposition,expr,op)) & TRUE)
                        {
                        loc = zelfde_als_w(pat->RIGHT);
                        loc = eval(loc);
                        if (loc->v.fl & SUCCESS)
                            {
                            s.c.rmr = TRUE;
                            if(loc->v.fl & FENCE)
                                s.c.rmr |= ONCE;
                            }
                        else
                            {
                            s.c.rmr = FALSE;
                            if (loc->v.fl & FENCE)
                                s.c.rmr |= (FENCE | ONCE);
                            }
                        wis(loc);
                        }
                    s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                    break;
                case MATCH:
                    if ((s.c.lmr = match(ind+1,sub, pat->LEFT, snijaf,pposition,expr,op)) & TRUE)
                        {
                        if((pat->v.fl & ATOM)
#if !STRINGMATCH_CAN_BE_NEGATED
                            && !ONTKENNING(pat->v.fl,ATOM)
#endif
                            )
#if CUTOFFSUGGEST
                            s.c.rmr = (char)(stringmatch(ind+1,"U",SPOBJ(sub),NULL,pat->RIGHT, sub,0,strlen((char*)SPOBJ(sub)),NULL,0) & TRUE);
#else
                            s.c.rmr = (char)(stringmatch(ind+1,"U",POBJ(sub),NULL,pat->RIGHT, sub,0,strlen((char*)POBJ(sub))) & TRUE);
#endif
                        else
                            s.c.rmr = (char)(match(ind+1,sub, pat->RIGHT, snijaf,pposition,expr,op) & TRUE);
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
                    if ( (s.c.lmr = (char)match(ind+1,sub, pat->LEFT, snijaf,pposition,expr,op))
                       & (TRUE | FENCE)
                       )
                        {
                        if((s.c.lmr & ONCE) && !oncePattern(pat->RIGHT))
                            {
                            s.c.rmr = (char)(s.c.lmr & TRUE);
                            }
                        else
                            {
                            s.c.rmr = (char)(s.c.lmr & (TRUE|ONCE));
                            }
                        }
                    else
                        {
                        s.c.rmr = match(ind+1,sub, pat->RIGHT, snijaf,pposition,expr,op);
                        if(  (s.c.rmr & ONCE)
                          && !(s.c.lmr & ONCE)
                          )
                            {
                            s.c.rmr &= ~(ONCE|POSITION_ONCE);
                            }
                        if(  (s.c.rmr & POSITION_MAX_REACHED)
                          && !(s.c.lmr & POSITION_MAX_REACHED)
                          )
                            {
                            s.c.rmr &= ~(POSITION_MAX_REACHED|POSITION_ONCE);
                            }
                        }
                    DBGSRC(Printf("%d%*s",ind,ind,"");\
                        Printf("OR s.c.lmr %d s.c.rmr %d\n",s.c.lmr,s.c.rmr);)
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
                        if(  ((sub->v.fl & (UNIFY|FLGS|NOT)) & (pat->RIGHT->v.fl & (UNIFY|FLGS|NOT))) 
                          == (pat->RIGHT->v.fl & (UNIFY|FLGS|NOT))
                          )
                            {
                            if(is_op(pat->RIGHT))
                                {
                                if(kop(sub) == kop(pat->RIGHT))
                                    {
                                    if ((s.c.lmr = match(ind+1,sub->LEFT, pat->RIGHT->LEFT, NULL,0,sub->LEFT,9999)) & TRUE)
                                        s.c.rmr = match(ind+1,sub->RIGHT, pat->RIGHT->RIGHT, NULL,0,sub->RIGHT,8888);
                                    s.c.rmr |= (char)(s.c.lmr & ONCE); /*{?} (=!(a.b)):(=$!(a.b)) => =!(a.b)*/
                                    }
                                else
                                    s.c.rmr = (char)ONCE; /*{?} (=!(a.b)):(=$!(a,b)) => F */
                                }
                            else
                                { 
                                if(  !(pat->RIGHT->v.fl & MINUS)
                                  || (  !is_op(sub) 
                                     && (sub->v.fl & MINUS)
                                     )
                                  )
                                    {
                                    if(!pat->RIGHT->u.obj)
                                        {
                                        if(pat->RIGHT->v.fl & UNOPS)
                                            s.c.rmr = (char)(TRUE|ONCE); /*{?} (=!):(=$!) => =! */
                                                                         /*{?} (=!(a.b)):(=$!) => =!(a.b) */
                                                                         /*{?} (=-a):(=$-) => =-a */
                                        else if(  !(sub->v.fl & (UNIFY|FLGS|NOT))
                                               && (is_op(sub) || !(sub->v.fl & MINUS))
                                               )
                                            {
                                            s.c.rmr = (char)(TRUE|ONCE); /*{?} (=):(=$) => = */
                                                                         /*{?} (=(a.b)):(=$) => =a.b */
                                            }
                                        else
                                            s.c.rmr = (char)ONCE;/*{?} (=-a):(=$) => F */
                                                                         /*{?} (=-):(=$) => F */
                                                                         /*{?} (=#):(=$) => F */
                                        }
                                    else if(  !is_op(sub) 
                                           && pat->RIGHT->u.obj == sub->u.obj
                                           )
                                        s.c.rmr = (char)(TRUE|ONCE); /*{?} (=-!a):(=$-!a) => =!-a */
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
                    psh(&sjt,&nilk,NULL);
                    copy_insert(&sjt, sub, snijaf);
                    loc = zelfde_als_w(pat);
                    loc = eval(loc);
                    deleteNode(&sjt);
                    if(isSUCCESS(loc))
                        {
                        loc = setflgs(loc,pat->v.fl);
                        if (equal(pat, loc))
                            {
                            s.c.rmr = (char)(match(ind+1,sub, loc, snijaf,pposition,expr,op) ^ NIKS(loc));
                            wis(loc);
                            break;
                            }
                        }
                    else /*"cat" as return value is used as pattern, ~"cat" however is not, because the function failed. */
                        {
                        if(loc->v.fl & FENCE)
                            s.c.rmr = ONCE; /* '~ as return value from function stops stretching subject */
                        }
                    wis(loc);
                    /* fall through */
                default:
                    if(is_op(pat))
                        {
                        if(kop(sub) == kop(pat))
                            {
                            if((s.c.lmr = match(ind+1,sub->LEFT, pat->LEFT, NULL,0,sub->LEFT,4432)) & TRUE)
                                {
                                if(kop(sub) == EQUALS)
                                    {
                                    loc = zelfde_als_w(sub->RIGHT); /* Object might change as a side effect!*/
                                    s.c.rmr = match(ind+1,loc, pat->RIGHT, NULL,0,loc,2234);
                                    wis(loc);
                                    }
                                else                                    
                                    s.c.rmr = match(ind+1,sub->RIGHT, pat->RIGHT, NULL,0,sub->RIGHT,2234);
                                }
                            s.c.rmr |= (char)(s.c.lmr & (FENCE | ONCE));
                            s.c.rmr &= ~POSITION_MAX_REACHED;
                            }
#ifndef NDEBUG
                            DBGSRC(printMatchState("DEFAULT:EXIT-MID",s,pposition,0);)
#endif
                        }
                    else
                        {
                        if (  pat->u.obj
                           || !(Flgs & (BREUK | NUMBER | NONIDENT | ATOM | IDENT))
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
    if(oncePattern(pat) || /* (a b X k:(|? b|`) X ?id) must fail*/ (s.c.rmr & (TRUE|FENCE|ONCE)) == FENCE)
        {
        s.c.rmr |= (char)(pat->v.fl & FENCE);
        s.c.rmr |= ONCE;
        DBGSRC(Printf("%d%*smatch(",ind,ind,"");results(sub,snijaf);\
            Printf(":");result(pat);Printf(") (B)");)
#ifndef NDEBUG
        DBGSRC(Printf(" rmr t %d o %d p %d m %d f %d ",\
                    s.b.brmr_true,s.b.brmr_once,s.b.brmr_position_once,s.b.brmr_position_max_reached,s.b.brmr_fence);)
#endif
        DBGSRC(if(pat->v.fl & POSITION) Printf("POSITION ");\
            if(pat->v.fl & BREUK) Printf("BREUK ");\
            if(pat->v.fl & NUMBER) Printf("NUMBER ");\
            if(pat->v.fl & SMALLER_THAN) Printf("SMALLER_THAN ");\
            if(pat->v.fl & GREATER_THAN) Printf("GREATER_THAN ");\
            if(pat->v.fl & ATOM) Printf("ATOM ");\
            if(pat->v.fl & FENCE) Printf("FENCE ");\
            if(pat->v.fl & IDENT) Printf("IDENT");\
            Printf("\n");)
        }
    if(is_op(pat))
        s.c.rmr ^= (char)NIKS(pat);
    if(name)
        wis(name);
    return s.c.rmr;
    }

static int subroot(ngetal * ag,char *conc[],int *pind)
    {
    int macht,i;
    unsigned LONG g,smalldivisor;
    unsigned LONG ores;
    static int bijt[12]=
        {1,  2,  2,  4,    2,    4,    2,    4,    6,    2,  6};
    /* 2-3,3-5,5-7,7-11,11-13,13-17,17-19,19-23,23-29,29-1,1-7*/
    unsigned LONG bigdivisor;

#ifdef ERANGE   /* ANSI C : strtoul() out of range */
    errno = 0;
    g = STRTOUL(ag->number,NULL,10);
    if(errno == ERANGE)
        return FALSE; /*{?} 45237183544316235476^1/2 => 45237183544316235476^1/2 */
#else  /* TURBOC, vcc */
    if(ag.length > 10 || ag.length == 10  && strcmp(ag.number,"4294967295") > 0)
        return FALSE;
    g = STRTOUL(ag.number,NULL,10);
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
                        conc[(*pind)] = (char *)bmalloc(__LINE__,12);/*{?} 327365274^1/2 => 2^1/2*3^1/2*2477^1/2*22027^1/2 */
                        }
                    else
                        {
                        conc[*pind] = (char *)bmalloc(__LINE__,20);
                        }
                    sprintf(conc[(*pind)++],LONGU "^(%d*\1)*",ores,macht);
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
    conc[*pind] = (char *)bmalloc(__LINE__,32);
    if((ores == g && ++macht) || ores == 1)
        sprintf(conc[(*pind)++],LONGU "^(%d*\1)",g,macht); /*{?} 32^1/2 => 2^5/2 */
    else
        sprintf(conc[(*pind)++],LONGU "^(%d*\1)*" LONGU "^\1",ores,macht,g);
    return TRUE;
    }

static int abseen(psk kn)
{
char *pstring;
pstring = SPOBJ(kn);
return(*pstring == '1' && *++pstring == 0);
}

static psk _linkertak(psk pkn)
{
psk lknoop;
lknoop = pkn->LEFT;
if(!(pkn->v.fl & SUCCESS))
    {
    lknoop = prive(lknoop);
    lknoop->v.fl ^= SUCCESS;
    }
if((pkn->v.fl & FENCE) && !(lknoop->v.fl & FENCE))
    {
    lknoop = prive(lknoop);
    lknoop->v.fl |= FENCE;
    }
wis(pkn->RIGHT);
return lknoop;
}

static psk linkertak(psk pkn)
{
psk lknoop = _linkertak(pkn);
pskfree(pkn);
return lknoop;
}

static psk _flinkertak(psk pkn)
{
psk lknoop;
lknoop = pkn->LEFT;
if(pkn->v.fl & SUCCESS)
    {
    lknoop = prive(lknoop);
    lknoop->v.fl ^= SUCCESS;
    }
if((pkn->v.fl & FENCE) && !(lknoop->v.fl & FENCE))
    {
    lknoop = prive(lknoop);
    lknoop->v.fl |= FENCE;
    }
wis(pkn->RIGHT);
return lknoop;
}

static psk flinkertak(psk pkn)
{
psk lknoop = _flinkertak(pkn);
pskfree(pkn);
return lknoop;
}

static psk _fencelinkertak(psk pkn)
{
psk lknoop;
lknoop = pkn->LEFT;
if(!(pkn->v.fl & SUCCESS))
    {
    lknoop = prive(lknoop);
    lknoop->v.fl ^= SUCCESS;
    }
if(pkn->v.fl & FENCE)
    {
    if(!(lknoop->v.fl & FENCE))
        {
        lknoop = prive(lknoop);
        lknoop->v.fl |= FENCE;
        }
    }
else if(lknoop->v.fl & FENCE)
    {
    lknoop = prive(lknoop);
    lknoop->v.fl &= ~FENCE;
    }
wis(pkn->RIGHT);
return lknoop;
}

static psk _rechtertak(psk pkn)
{
psk rknoop;
rknoop = pkn->RIGHT;
if(!(pkn->v.fl & SUCCESS))
    {
    rknoop = prive(rknoop);
    rknoop->v.fl ^= SUCCESS;
    }
if((pkn->v.fl & FENCE) && !(rknoop->v.fl & FENCE))
    {
    rknoop = prive(rknoop);
    rknoop->v.fl |= FENCE;
    }
wis(pkn->LEFT);
return rknoop;
}

static psk rechtertak(psk pkn)
{
psk rknoop = _rechtertak(pkn);
pskfree(pkn);
return rknoop;
}

static void pop(psk kn)
    {
    while(is_op(kn))
        {
        pop(kn->LEFT);
        kn = kn->RIGHT;
        }
    deleteNode(kn);
    }

static psk tryq(psk pkn,psk fun,Boolean * ok)
    {
    psk anker;
    psh(&argk,pkn,NULL);
    pkn->v.fl |= READY;

    anker = subboomcopie(fun->RIGHT);

    psh(fun->LEFT,&nulk,NULL);
    anker = eval(anker);
    pop(fun->LEFT);
    if(anker->v.fl & SUCCESS)
        {
        *ok = TRUE;
        wis(pkn);
        pkn = anker;
        }
    else
        {
        *ok = FALSE;
        wis(anker);
        }
    deleteNode(&argk);
    return pkn;
    }

static psk * backbone(psk arg,psk pkn,psk * pfirst)
    {
    psk first = *pfirst = subboomcopie(arg);
    psk * plast = pfirst;
    while(arg != pkn)
        {
        psk R = subboomcopie((*plast)->RIGHT);
        wis((*plast)->RIGHT);
        (*plast)->RIGHT = R;
        plast = &((*plast)->RIGHT);
        arg = arg->RIGHT;
        }
    *pfirst = first;
    return plast;
    }

static psk rechteroperand(psk pkn)
{
psk temp;
unsigned int teken;
temp = (pkn->RIGHT);
return((teken = kop(pkn)) == kop(temp) &&
        (teken == PLUS || teken == TIMES || teken == WHITE) ?
       temp->LEFT : temp);
}

static psk evalmacro(psk pkn)
    {
    if(!is_op(pkn))
        {
        return NULL;
        }
    else
        {
        psk arg = pkn;
        while(!(pkn->v.fl & READY))
            {
            if(atomtest(pkn->LEFT) != 0)
                {
                psk left = evalmacro(pkn->LEFT);
                if(left != NULL)
                    {
                    /* copy backbone from evalmacro's argument to current pkn
                       release lhs of copy of current and replace with 'left'
                       assign copy to 'pkn'
                       evalmacro current, if not null, replace current
                       return current
                    */
                    psk ret;
                    psk first = NULL;
                    psk * last = backbone(arg,pkn,&first);
                    wis((*last)->LEFT);
                    (*last)->LEFT = left;
                    if(atomtest((*last)->LEFT) == 0 && kop((*last)) == FUN)
                        {
                        ret = evalmacro(*last);
                        if(ret)
                            {
                            wis(*last);
                            *last = ret;
                            }
                        }
                    else
                        {
                        psk right = evalmacro((*last)->RIGHT);
                        if(right)
                            {
                            wis((*last)->RIGHT);
                            (*last)->RIGHT = right;
                            }
                        }
                    return first;
                    }
                }
            else if(kop(pkn) == FUN)
                {
                if(kop(pkn->RIGHT) == UNDERSCORE)
                    {
                    int Flgs;
                    psk h;
                    psk hh;
                    psk first = NULL;
                    psk * last;
                    Flgs = pkn->v.fl & (UNOPS|SUCCESS);
                    h = subboomcopie(pkn->RIGHT);
                    if(dummy_op == EQUALS)
                        {
                        psk becomes = (psk)bmalloc(__LINE__,sizeof(objectknoop));
#ifdef BUILTIN
                        ((typedObjectknoop*)becomes)->u.Int = 0;
#else
                        ((typedObjectknoop*)*R)->refcount = 0;
                        UNSETCREATEDWITHNEW((typedObjectknoop*)*R);
                        UNSETBUILTIN((typedObjectknoop*)*R);
#endif
                        becomes->LEFT = zelfde_als_w(h->LEFT);
                        becomes->RIGHT = zelfde_als_w(h->RIGHT);
                        wis(h);
                        h = becomes;
                        }
                    h->v.fl = dummy_op | Flgs;
                    hh = evalmacro(h->LEFT);
                    if(hh)
                        {
                        wis(h->LEFT);
                        h->LEFT = hh;
                        }
                    hh = evalmacro(h->RIGHT);
                    if(hh)
                        {
                        wis(h->RIGHT);
                        h->RIGHT = hh;
                        }
                    last = backbone(arg,pkn,&first);
                    wis(*last);
                    *last = h;
                    return first;
                    }
                else if(  kop(pkn->RIGHT) == FUN
                       && atomtest(pkn->RIGHT->LEFT) == 0
                       )
                    {
                    int Flgs;
                    psk h;
                    psk hh;
                    psk first = NULL;
                    psk * last;
                    Flgs = pkn->v.fl & UNOPS;
                    h = subboomcopie(pkn->RIGHT);
                    h->v.fl |= Flgs;
                    assert(atomtest(h->LEFT) == 0);
                    /* hh = evalmacro(h->LEFT);
                    if(hh)
                        {
                        wis(h->LEFT);
                        h->LEFT = hh;
                        }*/
                    hh = evalmacro(h->RIGHT);
                    if(hh)
                        {
                        wis(h->RIGHT);
                        h->RIGHT = hh;
                        }
                    last = backbone(arg,pkn,&first);
                    wis(*last);
                    *last = h;
                    return first;
                    }
                else
                    {
                    int newval;
                    psk tmp = zelfde_als_w(pkn->RIGHT);
                    psk h;
                    tmp = eval(tmp);

                    if((h = find(tmp,&newval,NULL)) != NULL)
                        {
                        int Flgs;
                        psk first = NULL;
                        psk * last;
                        if((kop(h) == EQUALS) && ISBUILTIN((objectknoop *)h))
                            {
                            if(!newval)
                                h = zelfde_als_w(h);
                            }
                        else
                            {
                            Flgs = pkn->v.fl & (UNOPS);
                            if(!newval)
                                {
                                h = subboomcopie(h);
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
                            else if(kop(h) == EQUALS)
                                { 
                                h->v.fl &= ~READY;
                                }
                            }

                        wis(tmp);
                        last = backbone(arg,pkn,&first);
                        wis(*last);
                        *last = h;
                        return first;
                        }
                    else
                        {
                        errorprintf("\nmacro evaluation fails because rhs of $ operator is not bound to a value");writeError(pkn->RIGHT);errorprintf("\n");
                        wis(tmp);
                        return NULL;
                        }
                    }
                }
            pkn = pkn->RIGHT;
            if(!is_op(pkn))
                {
                break;
                }
            }
        }
    return NULL;
    }

static psk lambda(psk pkn,psk name,psk Arg)
    {
    if(!is_op(pkn))
        {
        return NULL;
        }
    else
        {
        psk arg = pkn;
        while(!(pkn->v.fl & READY))
            {
            if(atomtest(pkn->LEFT) != 0)
                {
                psk left = lambda(pkn->LEFT,name,Arg);
                if(left != NULL)
                    {
                    /* copy backbone from lambda's argument to current pkn
                       release lhs of copy of current and replace with 'left'
                       assign copy to 'pkn'
                       lambda current, if not null, replace current
                       return current
                    */
                    psk ret;
                    psk first = NULL;
                    psk * last = backbone(arg,pkn,&first);
                    wis((*last)->LEFT);
                    (*last)->LEFT = left;
                    if(atomtest((*last)->LEFT) == 0 && kop((*last)) == FUN)
                        {
                        ret = lambda(*last,name,Arg);
                        if(ret)
                            {
                            wis(*last);
                            *last = ret;
                            }
                        }
                    else
                        {
                        psk right = lambda((*last)->RIGHT,name,Arg);
                        if(right)
                            {
                            wis((*last)->RIGHT);
                            (*last)->RIGHT = right;
                            }
                        }
                    return first;
                    }
                }
            else if(kop(pkn) == FUN)
                {
                if(kop(pkn->RIGHT) == UNDERSCORE)
                    {
                    int Flgs;
                    psk h;
                    psk hh;
                    psk first = NULL;
                    psk * last;
                    Flgs = pkn->v.fl & (UNOPS|SUCCESS);
                    h = subboomcopie(pkn->RIGHT);
                    if(dummy_op == EQUALS)
                        {
                        psk becomes = (psk)bmalloc(__LINE__,sizeof(objectknoop));
#ifdef BUILTIN
                        ((typedObjectknoop*)becomes)->u.Int = 0;
#else
                        ((typedObjectknoop*)*R)->refcount = 0;
                        UNSETCREATEDWITHNEW((typedObjectknoop*)*R);
                        UNSETBUILTIN((typedObjectknoop*)*R);
#endif
                        becomes->LEFT = zelfde_als_w(h->LEFT);
                        becomes->RIGHT = zelfde_als_w(h->RIGHT);
                        wis(h);
                        h = becomes;
                        }
                    h->v.fl = dummy_op | Flgs;
                    hh = lambda(h->LEFT,name,Arg);
                    if(hh)
                        {
                        wis(h->LEFT);
                        h->LEFT = hh;
                        }
                    hh = lambda(h->RIGHT,name,Arg);
                    if(hh)
                        {
                        wis(h->RIGHT);
                        h->RIGHT = hh;
                        }
                    last = backbone(arg,pkn,&first);
                    wis(*last);
                    *last = h;
                    return first;
                    }
                else if(  kop(pkn->RIGHT) == FUN
                       && atomtest(pkn->RIGHT->LEFT) == 0
                       )
                    {
                    int Flgs;
                    psk h;
                    psk hh;
                    psk first = NULL;
                    psk * last;
                    Flgs = pkn->v.fl & UNOPS;
                    h = subboomcopie(pkn->RIGHT);
                    h->v.fl |= Flgs;
                    assert(atomtest(h->LEFT) == 0);
                    hh = lambda(h->RIGHT,name,Arg);
                    if(hh)
                        {
                        wis(h->RIGHT);
                        h->RIGHT = hh;
                        }
                    last = backbone(arg,pkn,&first);
                    wis(*last);
                    *last = h;
                    return first;
                    }
                else if(!equal(name,pkn->RIGHT))
                    {
                    psk h;
                    psk first;
                    psk * last;
                    h = subboomcopie(Arg);
                    if(h->v.fl & INDIRECT)
                        {
                        h->v.fl &= ~READY;
                        }
                    else if(is_op(h) && kop(h) == EQUALS)
                        { 
                        h->v.fl &= ~READY;
                        }

                    last = backbone(arg,pkn,&first);
                    wis(*last);
                    *last = h;
                    return first;
                    }
                }
            else if (  kop(pkn) == FUU 
                    && (pkn->v.fl & BREUK)
                    && kop(pkn->RIGHT) == DOT
                    && !equal(name,pkn->RIGHT->LEFT)
                    )
                {
                return NULL;
                }
            pkn = pkn->RIGHT;
            if(!is_op(pkn))
                {
                break;
                }
            }
        }
    return NULL;
    }


static void combiflags(psk kn)
{
int lflgs;
if((lflgs = kn->LEFT->v.fl & UNOPS) != 0)
    {
    kn->RIGHT = prive(kn->RIGHT);
    if(NIKSF(lflgs))
        {
        kn->RIGHT->v.fl |= lflgs & ~NOT;
        kn->RIGHT->v.fl ^= NOT|SUCCESS;
        }
    else
        kn->RIGHT->v.fl |= lflgs;
    }
}


static int is_afhankelyk_van(psk el,psk lijst)
    {
    int ret;
    assert(!is_op(lijst));
    psk kn = NULL;
    adr[1] = lijst;
    adr[2] = el;
    kn = opb(kn,"(!dep:(? (\1.? \2 ?) ?)",NULL);
    kn = eval(kn);
    ret = isSUCCESS(kn);
    wis(kn);
    return ret;
    }

static int zoekopt(psk kn,LONG opt)
    {
    while(is_op(kn))
        {
        if(zoekopt(kn->LEFT,opt))
            return TRUE;
        kn = kn->RIGHT;
        }
    return PLOBJ(kn) == opt;
    }

static void mmf(ppsk pk)
{
psk goal;
ppsk pgoal;
vars *navar;
int alfabet,ext;
char dim[22];
ext = zoekopt(*pk,EXT);
wis(*pk);
pgoal = pk;
for(alfabet = 0;alfabet < 256/*0x80*/;alfabet++)
    {
    for(navar = variabelen[alfabet];
        navar;
        navar = navar->next)
        {
        goal = *pgoal = (psk)bmalloc(__LINE__,sizeof(kknoop));
        goal->v.fl = WHITE | SUCCESS;
        if(ext && navar->n > 0)
            {
            goal = goal->LEFT = (psk)bmalloc(__LINE__,sizeof(kknoop));
            goal->v.fl = DOT | SUCCESS;
            sprintf(dim,"%d.%d",navar->n,navar->selector);
            goal->RIGHT = NULL;
            goal->RIGHT = opb(goal->RIGHT,dim,NULL);
            }
        goal = goal->LEFT =
            (psk)bmalloc(__LINE__,sizeof(unsigned LONG) + 1 + strlen((char *)VARNAME(navar)));
        goal->v.fl = (READY|SUCCESS);
        strcpy((char *)(goal)+sizeof(unsigned LONG),(char *)VARNAME(navar));
        pgoal = &(*pgoal)->RIGHT;
        }
    }
*pgoal = zelfde_als_w(&nilk);
}

static void lstsub(psk kn)
{
vars *navar;
unsigned char *naam;
int alfabet,n;
mooi = FALSE;
naam = POBJ(kn);
for(alfabet = 0;alfabet<256;alfabet++)
    {
    for(navar = variabelen[alfabet];
        navar;
        navar = navar->next)
        {
        if((kn->u.obj == 0 && alfabet < 0x80) || !STRCMP(VARNAME(navar),naam))
            {
            for(n = navar->n;n >= 0;n--)
                {
                ppsk tmp;
                if(listWithName)
                    {
                    if(global_fpo == stdout)
                        {
                        if(navar->n > 0)
                            Printf("%c%d (",n == navar->selector ? '>' : ' ',n);
                        else
                            Printf("(");
                        }
                    if(haalaan(VARNAME(navar)))
                        myprintf("\"",(char *)VARNAME(navar),"\"=",NULL);
                    else
                        myprintf((char *)VARNAME(navar),"=",NULL);
                    if(hum)
                        myprintf("\n",NULL);
                    }
                assert(navar->pvaria);
                tmp = Entry(navar->n,n,&navar->pvaria);
                result(*tmp = Head(*tmp));
                if (listWithName)
                    {
                    if (global_fpo == stdout)
                        Printf("\n)");
                    myprintf(";\n", NULL);
                    }
                else
                    break; /*Only list variable on top of stack if RAW*/
                }
            }
        }
    }
mooi = TRUE;
}

static void lst(psk kn)
    {
    while(is_op(kn))
        {
        if(kop(kn) == EQUALS)
            {
            mooi = FALSE;
            myprintf("(",NULL);
            if(hum)
                myprintf("\n",NULL);
            if(listWithName)
                {
                result(kn);
                }
            else
                {
                result(kn->RIGHT);
                }
            if(hum)
                myprintf("\n",NULL);
            myprintf(")\n",NULL);
            mooi = TRUE;
            return;
            }
        else
            {
            lst(kn->LEFT);
            kn = kn->RIGHT;
            }
        }
    lstsub(kn);
    }

#if !defined NO_FOPEN
static filehendel * findFilehendelByName(const char * name)
    {
    filehendel * fh;
    for(fh = fh0
       ; fh
       ; fh = fh->next
       )
        if(!strcmp(fh->naam,name))
            return fh;
    return NULL;
    }

static filehendel * allocateFilehendel(const char * name,FILE * fp
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                                       ,Boolean dontcloseme
#endif
                                       )
    {
    filehendel * fh = (filehendel*)bmalloc(__LINE__,sizeof(filehendel));
    fh->naam = (char *)bmalloc(__LINE__,strlen(name) + 1);
    strcpy(fh->naam,name);
    fh->fp = fp;
#if !defined NO_LOW_LEVEL_FILE_HANDLING
    fh->dontcloseme = dontcloseme;
#endif
    fh->next = fh0;
    fh0 = fh;
    return fh0;
    }

static void deallocateFilehendel(filehendel * fh)
    {
    filehendel * fhvorig, * fhh;
    for(fhvorig = NULL,fhh = fh0
        ;fhh != fh
        ;fhvorig = fhh,fhh = fhh->next
        )
        ;
    if(fhvorig)
        fhvorig->next = fh->next;
    else
        fh0 = fh->next;
    if(fh->fp)
        fclose(fh->fp);
    bfree(fh->naam);
#if !defined NO_LOW_LEVEL_FILE_HANDLING
    if(fh->stop)
#ifdef BMALLLOC
        bfree(fh->stop);
#else
        free(fh->stop);
#endif
#endif
    bfree(fh);
    }

filehendel * mygetfilehendel(const char * filename,const char * mode
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                             ,Boolean dontcloseme
#endif
                             )
    {
    FILE * fp = fopen(filename,mode);
    if(fp)
        {
        filehendel * fh = allocateFilehendel(filename,fp
#if !defined NO_LOW_LEVEL_FILE_HANDLING
            ,dontcloseme
#endif
            );
        return fh;
        }
    return NULL;
    }

filehendel * myfopen(const char * filename,const char * mode
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                     ,Boolean dontcloseme
#endif
                     )
    {
#if !defined NO_FOPEN
    if(!findFilehendelByName(filename))
        {
        filehendel * fh = mygetfilehendel(filename,mode
#if !defined NO_LOW_LEVEL_FILE_HANDLING
            ,dontcloseme
#endif
            );
        if(!fh && targetPath && strchr(mode,'r'))
            {
            const char * p = filename;
            char * q;
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
            q = (char *)malloc((len = strlen(targetPath)) + strlen(filename) + 1);
            if(q)
                {
                strcpy(q,targetPath);
                strcpy(q+len,filename);
                fh = mygetfilehendel(q,mode
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                    ,dontcloseme
#endif
                    );
                free(q);
                }
            }
        return fh;
        }
#endif
    return NULL;
    }
#endif

#if !defined NO_LOW_LEVEL_FILE_HANDLING
static LONG someopt(psk kn,LONG opt[])
    {
    int i;
    assert(!is_op(kn));
    for(i=0;opt[i];i++)
        if(PLOBJ(kn) == opt[i])
            return opt[i];
    return 0L;
    }

#if !defined NO_FOPEN

static LONG tijdnr = 0L;

static int closeAFile(void)
    {
    filehendel *fh,*fhmin;
    if(fh0 == NULL)
        return FALSE;
    for(fh = fh0,fhmin = fh0;
        fh != NULL;
        fh = fh->next)
        {
        if( !fh->dontcloseme
          && fh->filepos == -1L
          && fh->tijd < fhmin->tijd
          )
            fhmin = fh;
        }
    if(fhmin == NULL || fhmin->dontcloseme)
        {
        return FALSE;
        }
    fhmin->filepos = FTELL(fhmin->fp);
    /* fh->filepos != -1 means that the file is closed */
    fclose(fhmin->fp);
    fhmin->fp = NULL;
    return TRUE;
    }
#if defined NDEBUG
#define preparefp(fh,naam,mode) preparefp(fh,mode)
#endif

static filehendel * preparefp(filehendel * fh,char * naam,LONG mode)
    {
    assert(fh != NULL);
    assert(!strcmp(fh->naam,naam));
    if( mode != 0L
    &&  mode != fh->mode
    && fh->fp != NULL)
        {
        fh->mode = mode;
        if((fh->fp = freopen(fh->naam,(char *)&(fh->mode),fh->fp)) == NULL)
            return NULL;
        fh->rwstatus = NoPending;
        }
    else if(fh->filepos >= 0L)
        {
        if((fh->fp = fopen(fh->naam,(char *)&(fh->mode))) == NULL)
            {
            if(closeAFile())
                fh->fp = fopen(fh->naam,(char *)&(fh->mode));
            }
        if(fh->fp == NULL)
            return NULL;
        fh->rwstatus = NoPending;
        FSEEK(fh->fp,fh->filepos,SEEK_SET);
        }
    fh->filepos = -1L;
    fh->tijd = tijdnr++;
    return fh;
    }
/*
Find an existing or create a fresh file handle for a known file name
If the file mode differs from the current file mode,
    reopen the file with the new file mode.
If the file is known but has been closed (e.g. to save file handles),
    open the file with the memorized file mode and go to the memorized position
*/
static filehendel *zoekfp(char *naam,LONG mode)
    {
    filehendel *fh;
    for(fh = fh0;fh;fh = fh->next)
        if(!strcmp(naam,fh->naam))
            return preparefp(fh,naam,mode);
    return NULL;
    }

static void setStop(filehendel *fh,char * stopstring)
    {
    if(fh->stop)
#ifdef BMALLLOC
        bfree(fh->stop);
    fh->stop = (char *)bmalloc(__LINE__,strlen(stopstring + 1);
#else
        free(fh->stop);
    fh->stop = (char *)malloc(strlen(stopstring) + 1);
#endif
    strcpy(fh->stop,stopstring);
    }

static int fil(ppsk pkn)
{
FILE *fp;
psk kns[4];
LONG ind;
int sh;
psk kn;
static filehendel *fh = NULL;
char *naam;

static LONG types[]={CHR,DEC,STRt,0L};
static LONG whences[]={SET,CUR,END,0L};
static LONG modes[]={
O('r', 0 , 0 ),/*open text file for reading                                  */
O('w', 0 , 0 ),/*create text file for writing, or trucate to zero length     */
O('a', 0 , 0 ),/*append; open text file or create for writing at eof         */
O('r','b', 0 ),/*open binary file for reading                                */
O('w','b', 0 ),/*create binary file for writing, or trucate to zero length   */
O('a','b', 0 ),/*append; open binary file or create for writing at eof       */
O('r','+', 0 ),/*open text file for update (reading and writing)             */
O('w','+', 0 ),/*create text file for update, or trucate to zero length      */
O('a','+', 0 ),/*append; open text file or create for update, writing at eof */
O('r','+','b'),
O('r','b','+'),/*open binary file for update (reading and writing)           */
O('w','+','b'),
O('w','b','+'),/*create binary file for update, or trucate to zero length    */
O('a','+','b'),
O('a','b','+'),/*append;open binary file or create for update, writing at eof*/
0L};

static LONG type,numwaarde,whence;
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
for(ind = 0,kn = (*pkn)->RIGHT;
    is_op(kn);
    kn = kn->RIGHT)
    {
    if(is_op(kn->LEFT) || ind > 2)
        {
        return FALSE;
        }
    kns[ind++] = kn->LEFT;
    }
kns[ind++] = kn;
for(;ind < 4;)
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
    naam = (char *)POBJ(kns[0]);
    if(fh && strcmp(naam,fh->naam))
        {
        fh = NULL;
        }
    }
else
    {
    if(fh)
        naam = fh->naam;
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
                file handel is set to current naam

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
    if((mode.l = someopt(kns[1],modes)) != 0L)
        {
        if(fh)
            fh = preparefp(fh,naam,mode.l);
        else
            fh = zoekfp(naam,mode.l);
        if(fh == NULL)
            {
            if((fh=myfopen(naam,(char *)&mode,FALSE)) == NULL)
                {
                if(closeAFile())
                    fh=myfopen(naam,(char *)&mode,FALSE);
                }
            if(fh == NULL)
                {
                return FALSE;
                }
        fh->filepos = -1L;
            fh->mode = mode.l;
            fh->type = CHR;
            fh->size = 1;
            fh->getal = 1;
            fh->tijd = tijdnr++;
            fh->rwstatus = NoPending;
            fh->stop = NULL;
            assert(fh->fp != 0);
            }
        assert(fh->fp != 0);
        return TRUE;
        }
    else
        {
    /*
    We do not open a file now, so we should have a file handle in memory.
    */
        if(fh)
            {
            fh = preparefp(fh,naam,0L);
            }
        else
            {
            fh = zoekfp(naam,0L);
            }


        if(!fh)
            {
            return FALSE;
            }
        assert(fh->fp != 0);

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
        if((type = someopt(kns[1],types)) != 0L)
            {
            fh->type = type;
            if(type == STRt)
                {
                /*
                  THIRD ARGUMENT: primary stopping character (e.g. "\n")

                  An empty string "" sets stopping string to NULL,
                  (Changed behaviour! Previously default stop was newline!)
                */
                if(kns[2] && kns[2]->u.obj)
                    {
                    setStop(fh,(char *)&kns[2]->u.obj);
                    }
                else
                    {
                    if(fh->stop)
#ifdef BMALLLOC
                       bfree(fh->stop);
                    fh->stop = NULL;
#else
                        free(fh->stop);
                    fh->stop = NULL;
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
                    fh->size = toLong(kns[2]);
                    }
                else
                    {
                    fh->size = 1;
                    fh->getal = 1;
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
                    fh->getal = toLong(kns[3]);
                    }
                else
                    fh->getal = 1;
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
        else if((whence = someopt(kns[1],whences)) != 0L)
            {
            LONG offset;
            assert(fh->fp != 0);
            fh->tijd = tijdnr++;
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
            || FSEEK(fh->fp,offset,whence == SET ? SEEK_SET
                            : whence == END ? SEEK_END
                                            : SEEK_CUR))
                {
                deallocateFilehendel(fh);
                fh = NULL;
                return FALSE;
                }
            fh->rwstatus = NoPending;
            return TRUE;
            }
    /*
    SECOND ARGUMENT:TELL POSITION
    fil$(,TEL)
    */
        else if(PLOBJ(kns[1]) == TEL)
            {
            char pos[11];
            sprintf(pos,LONGD,FTELL(fh->fp));
            wis(*pkn);
            *pkn = scopy((const char *)pos);
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
    if(fh)
        {
        fh = preparefp(fh,naam,0L);
        }
    else
        {
        fh = zoekfp(naam,0L);
        }
    }

if(!fh)
    {
    return FALSE;
    }
/*
READ OR WRITE
Now we are either going to read or to write
*/

type = fh->type;
mode.l = fh->mode;
fp = fh->fp;

/*
THIRD ARGUMENT: the number of elements to read or write
OR stop characters, depending on type (20081113)
*/

if(kns[2] && kns[2]->u.obj)
    {
    if(type == STRt)
        {
        setStop(fh,(char *)&kns[2]->u.obj);
        }
    else
        {
        if(!INTEGER(kns[2]))
            {
            return FALSE;
            }
        fh->getal = toLong(kns[2]);
        }
    }

/*
We allow 1, 2 or 4 bytes to be read/written in one fil$ operation
These can be distributed over decimal numbers.
*/

if(type == DEC)
    {
    switch((int)fh->size)
        {
        case 1 :
            if(fh->getal > 4)
                fh->getal = 4;
            break;
        case 2 :
            if(fh->getal > 2)
                fh->getal = 2;
            break;
        default :
            fh->size = 4; /*Invalid size declaration adjusted*/
            fh->getal = 1;
        }
    }
fh->tijd = tijdnr++;
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
        if(fh->rwstatus == Reading)
            {
            LONG fpos = FTELL(fh->fp);
            FSEEK(fh->fp,fpos,SEEK_SET);
            }
        fh->rwstatus = Writing;
        if(type == DEC)
            {
            numwaarde = toLong(kns[3]);
            for(ind=0;ind < fh->getal;ind++)
                switch((int)fh->size)
                    {
                    case 1 :
                        fputc((int)numwaarde & 0xFF,fh->fp);
                        numwaarde >>= 8;
                        break;
                    case 2 :
                        snum.s = (short)(numwaarde & 0xFFFF);
                        fwrite(snum.c,1,2,fh->fp);
                        numwaarde >>= 16;
                        break;
                    case 4 :
                        snum.i = (INT32_T)(numwaarde & 0xFFFFFFFF);
                        fwrite(snum.c,1,4,fh->fp);
                        assert(fh->getal == 1);
                        break;
                    default :
                        fwrite((char *)&numwaarde,1,4,fh->fp);
                        break;
                    }
            }
        else if(type == CHR)
            {
            size_t len,len1,minl;
            len1 = (size_t)(fh->size*fh->getal);
            len = strlen((char *)POBJ(kns[3]));
            minl = len1 < len ? (len1 > 0 ? len1 : len) : len;
            if(fwrite(POBJ(kns[3]),1,minl,fh->fp) == minl)
                for(;len < len1 && putc(' ',fh->fp) != EOF;len++);
            }
        else /*if(type == STRt)*/
            {
            if(  fh->stop
              && fh->stop[0]
              )/* stop string also works when writing. */
                {
                char * s = (char *)POBJ(kns[3]);
                while(!strchr(fh->stop,*s))
                    fputc(*s++,fh->fp);
                }
            else
                {
                fputs((char *)POBJ(kns[3]),fh->fp);
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
        unsigned char * bbuffer;/* = buffer;*/
        if(fh->rwstatus == Writing)
            {
            fflush(fh->fp);
            fh->rwstatus = NoPending;
            }
        if(feof(fp))
            {
            return FALSE;
            }
        fh->rwstatus = Reading;
        if(type == STRt)
            {
            psk lpkn = NULL;
            psk rpkn = NULL;
            char * conc[2];
            int count = 0;
            LONG pos = FTELL(fp);
            int kar = 0;
            while(  count < (INPUTBUFFERSIZE - 1)
                 && (kar = fgetc(fp)) != EOF
                 && (  !fh->stop
                    || !strchr(fh->stop,kar)
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
                while(  (kar = fgetc(fp)) != EOF
                     && (  !fh->stop
                        || !strchr(fh->stop,kar)
                        )
                     )
                    count++;
                if(count >= INPUTBUFFERSIZE)
                    {
                    bbuffer = (unsigned char *)bmalloc(__LINE__,(size_t)count+1);
                    strcpy((char *)bbuffer,(char *)buffer);
                    FSEEK(fp,pos+(INPUTBUFFERSIZE - 1),SEEK_SET);
                    if(fread((char *)bbuffer+(INPUTBUFFERSIZE - 1),1,count - (INPUTBUFFERSIZE - 1),fh->fp) == 0)
                        {
                        bfree(bbuffer);
                        return FALSE;
                        }
                    if(ferror(fh->fp))
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
            bron = bbuffer;
            lpkn = input(NULL,lpkn,1,NULL,NULL);
            if(kar == EOF)
                bbuffer[0] = '\0';
            else
                {
                bbuffer[0] = (char)kar;
                bbuffer[1] = '\0';
                }
            bron = bbuffer;
            rpkn = input(NULL,rpkn,1,NULL,NULL);
            conc[0] = "(\1.\2)";
            adr[1] = lpkn;
            adr[2] = rpkn;
            conc[1] = NULL;
            *pkn = vopb(*pkn,(const char **)conc);
            wis(adr[1]);
            wis(adr[2]);
            }
        else
            {
            size_t readbytes = fh->size * fh->getal;
            if(readbytes >= INPUTBUFFERSIZE)
                bbuffer = (unsigned char *)bmalloc(__LINE__,readbytes+1);
            else
                bbuffer = buffer;
            if( (readbytes = fread((char *)bbuffer,(size_t)fh->size,(size_t)fh->getal,fh->fp)) == 0
              && fh->size != 0
              && fh->getal != 0
              )
                {
                return FALSE;
                }
            if(ferror(fh->fp))
                {
                perror("fread");
                return FALSE;
                }
            *(bbuffer+(int)readbytes) = 0;
            if(type == DEC)
                {
                numwaarde = 0L;
                sh = 0;
                for(ind = 0;ind < fh->getal;)
                    {
                    switch((int)fh->size)
                        {
                        case 1 :
                            numwaarde += (LONG)bbuffer[ind++] << sh;
                            sh += 8;
                            continue;
                        case 2 :
                            numwaarde += (LONG)(*(short*)(bbuffer+ind)) << sh;
                            ind += 2;
                            sh += 16;
                            continue;
                        case 4 :
                            numwaarde += (LONG)(*(INT32_T*)(bbuffer+ind)) << sh;
                            ind += 4;
                            sh += 32;
                            continue;
                        default :
                            numwaarde += *(LONG*)bbuffer;
                            break;
                        }
                    break;
                    }
                sprintf((char *)bbuffer,LONGD,numwaarde);
                }
            bron = bbuffer;
            *pkn = input(NULL,*pkn,1,NULL,NULL);
            }
        if(bbuffer != (unsigned char *)&buffer[0])
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

static int allopts(psk kn,LONG opt[])
    {
    int i;
    while(is_op(kn))
        {
        if(!allopts(kn->LEFT,opt))
            return FALSE;
        kn = kn->RIGHT;
        }
    for(i=0;opt[i];i++)
        if(PLOBJ(kn) == opt[i])
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


static int output(ppsk pkn,void (*hoe)(psk k))
{
FILE *redfpo;
psk rknoop,rlknoop,rrknoop,rrrknoop;
static LONG opts[] =
    {APP,BIN,CON,EXT,MEM,LIN,NEW,RAW,TXT,VAP,0L};
if(kop(rknoop = (*pkn)->RIGHT) == COMMA)
   {
   redfpo = global_fpo;
   rlknoop = rknoop->LEFT;
   rrknoop = rknoop->RIGHT;
   hum = !zoekopt(rrknoop,LIN);
   listWithName = !zoekopt(rrknoop,RAW);
   if(allopts(rrknoop,opts))
        {
        if(zoekopt(rrknoop,MEM))
            {
            psk ret;
            telling = 1;
            verwerk = tel;
            global_fpo = NULL;
            (*hoe)(rlknoop);
            ret = (psk)bmalloc(__LINE__,sizeof(unsigned LONG)+telling);
            ret->v.fl = READY | SUCCESS;
            verwerk = plak;
            bron = POBJ(ret);
            (*hoe)(rlknoop);
            hum = 1;
            verwerk = myputc;
            wis(*pkn);
            *pkn = ret;
            global_fpo = redfpo;
            return TRUE;
            }
        else
            {
            (*hoe)(rlknoop);
            flush();
            adr[2] = rlknoop;
            }
        }
    else if(kop(rrknoop) == COMMA
         && !is_op(rrknoop->LEFT)
         && allopts((rrrknoop = rrknoop->RIGHT),opts))
        {
#if !defined NO_FOPEN
        int binmode = ((hoe == lst) && !zoekopt(rrrknoop,TXT)) || zoekopt(rrrknoop,BIN);
        filehendel * fh = 
            myfopen((char *)POBJ(rrknoop->LEFT),
                      zoekopt(rrrknoop,NEW) 
                    ? ( binmode
                      ? WRITEBIN 
                      : WRITETXT
                      ) 
                    : ( binmode 
                      ? APPENDBIN 
                      : APPENDTXT
                      )
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                    ,TRUE
#endif
                    );
        if(fh == NULL)
            {
            errorprintf("cannot open %s\n",POBJ(rrknoop->LEFT));
            global_fpo = redfpo;
            hum = 1;
            return FALSE;
            }
        else
            {
            global_fpo = fh->fp;
            (*hoe)(rlknoop);
            deallocateFilehendel(fh);
            global_fpo = redfpo;
            adr[2] = rlknoop;
            }
#else
        hum = 1;
        return FALSE;
#endif
        }
    else
        {
        (*hoe)(rknoop);
        flush();
        adr[2] = rknoop;
        }
    *pkn = dopb(*pkn,adr[2]);
    }
else
    {
    (*hoe)(rknoop);
    flush();
    *pkn = rechtertak(*pkn);
    }
hum = 1;
listWithName = 1;
return TRUE;
}

static LONG simil
    (const char * s1
    ,const char * s1end
    ,const char * s2
    ,const char * s2end
    ,int * putf1
    ,int * putf2
    ,LONG * plen1
    ,LONG * plen2
    )
    {
    const char * ls1;
    const char * s1l = NULL;
    const char * s1r = NULL;
    const char * s2l = NULL;
    const char * s2r = NULL;
    LONG max;
    LONG len1;
    LONG len2 = 0;
    /* regard each character in s1 as possible starting point for match */
    for(max = 0,ls1 = s1,len1 = 0
       ;ls1 < s1end
       ;getCodePoint2(&ls1,putf1),++len1)
        {
        const char * ls2;
        /* compare with s2 */
        for(ls2 = s2,len2 = 0;ls2 < s2end;getCodePoint2(&ls2,putf2),++len2)
            {
            const char * lls1 = ls1;
            const char * lls2 = ls2;
            /* determine lenght of equal parts */
            LONG len12 = 0;
            for(;;)
                   {
                   if(lls1 < s1end && lls2 < s2end)
                       {
                       const char * ns1 = lls1,* ns2 = lls2;
                       int K1 = getCodePoint2(&ns1,putf1);
                       int K2 = getCodePoint2(&ns2,putf2);
                       if(convertLetter(K1,u2l) == convertLetter(K2,u2l))
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
        max += simil(s1,s1l,s2,s2l,putf1,putf2,NULL,NULL) + simil(s1r,s1end,s2r,s2end,putf1,putf2,NULL,NULL);
        }
    if(plen1)
        {
        *plen1 = len1;
        }
    if(plen2)
        {
        if(len1 == 0)
            {
            for(len2 = 0;*s2;getCodePoint2(&s2,putf2),++len2)
                ;
            }
        *plen2 = len2;
        }
    return max;
    }

static void Sim(char * klad,char * str1,char * str2)
    {
    int utf1 = 1;
    int utf2 = 1;
    LONG len1 = 0;
    LONG len2 = 0;
    LONG sim = simil(str1,str1+strlen((char *)str1),str2,str2+strlen((char *)str2),&utf1,&utf2,&len1,&len2);
    sprintf(klad,LONGD "/" LONGD,(2L*(LONG)sim),len1+len2);
    }

static function_return_type find_func(psk pkn)
    {
    psk lknoop = pkn->LEFT;
    objectStuff Object = {0,0,0};
    int nieuw = FALSE;
    adr[1] = NULL;
    DBGSRC(Printf("find_func(");result(pkn);Printf(")\n");)
    adr[1] = find(lknoop,&nieuw,&Object);
    if(adr[1])
        {
        if(  is_op(adr[1])
          && kop(adr[1]) == DOT
          )
            {
            psh(&argk,pkn->RIGHT,NULL);
            if(Object.self)
                {
                psh(&selfkn,Object.self,NULL);
                }
            pkn = dopb(pkn,adr[1]);
            if(nieuw)
                wis(adr[1]);
            if(Object.self)
                {
                /*
                psh(&selfkn,self,NULL); Must precede dopb(...).
                Example where this is relevant:

                {?} ((==.lst$its).)'
                (its=
                =.lst$its);
                {!} its

                */
                if(Object.object)
                    {
                    psh(&Selfkn,Object.object,NULL);
                    if(kop(pkn) == DOT)
                        {
                        psh(pkn->LEFT,&nulk,NULL);
                        pkn = eval(pkn);
                        pop(pkn->LEFT);
                        pkn = dopb(pkn,pkn->RIGHT);
                        }
                    deleteNode(&argk);
                    deleteNode(&selfkn);
                    deleteNode(&Selfkn);
                    return functionOk(pkn);
                    }
                else
                    {
                    if(kop(pkn) == DOT)
                        {
                        psh(pkn->LEFT,&nulk,NULL);
                        pkn = eval(pkn);
                        pop(pkn->LEFT);
                        pkn = dopb(pkn,pkn->RIGHT);
                        }
                    deleteNode(&argk);
                    deleteNode(&selfkn);
                    return functionOk(pkn);
                    }
                }
            else
                {
                if(kop(pkn) == DOT)
                    {
                    psh(pkn->LEFT,&nulk,NULL);
                    pkn = eval(pkn);
                    pop(pkn->LEFT);
                    pkn = dopb(pkn,pkn->RIGHT);
                    }
                deleteNode(&argk);
                return functionOk(pkn);
                }
            }
        else
            {
#if defined NO_EXIT_ON_NON_SEVERE_ERRORS
            return functionFail(pkn);
#else
            errorprintf("(Syntax error) The following is not a function:\n\n  ");
            writeError(lknoop);
            exit(116);
#endif
            }
        }
    else if(Object.theMethod)
        {
        DBGSRC(printf("Object.theMethod\n");)
        if(Object.theMethod((struct typedObjectknoop *)Object.object,&pkn))
            {
            DBGSRC(printf("functionOk");result(pkn);printf("\n");)
            return functionOk(pkn);
            }
        }
    else if (  (kop(pkn->LEFT) == FUU)
            && (pkn->LEFT->v.fl & BREUK)
            && (kop(pkn->LEFT->RIGHT) == DOT)
            && (!is_op(pkn->LEFT->RIGHT->LEFT))
            )
        {
        psk rknoop;
        rknoop = lambda(pkn->LEFT->RIGHT->RIGHT,pkn->LEFT->RIGHT->LEFT,pkn->RIGHT);
        if(rknoop)
            {
            wis(pkn);
            pkn = rknoop;
            }
        else
            {
            psk npkn = subboomcopie(pkn->LEFT->RIGHT->RIGHT);
            wis(pkn);
            pkn = npkn;
            if(!is_op(pkn) && !(pkn->v.fl & INDIRECT))
                pkn->v.fl |= READY;
            }
        return functionOk(pkn);
        }
    else
        {
        DBGSRC(errorprintf("Function not found");writeError(pkn);\
            Printf("\n");)
        }
    return functionFail(pkn);
    }

static int hasSubObject(psk src)
    {
    while(is_op(src))
        {
        if(kop(src) == EQUALS)
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

static psk objectcopiesub(psk src);

static psk objectcopiesub2(psk src) /* src is NOT an object */
    {
    psk goal;
    if(is_op(src) && hasSubObject(src))
        {
        goal = (psk)bmalloc(__LINE__,sizeof(kknoop));
        goal->ops = src->ops & ~ALL_REFCOUNT_BITS_SET;
        goal->LEFT = objectcopiesub(src->LEFT);
        goal->RIGHT = objectcopiesub(src->RIGHT);
        return goal;
        }
    else
        return zelfde_als_w(src);
    }

static psk objectcopiesub(psk src)
    {
    psk goal;
    if(is_object(src))
        {
        if(ISBUILTIN((objectknoop*)src))
            {
            return zelfde_als_w(src);
           }
        else
            {
            goal = (psk)bmalloc(__LINE__,sizeof(objectknoop));
#ifdef BUILTIN
            ((typedObjectknoop*)goal)->u.Int = 0;
#else
            ((typedObjectknoop*)goal)->refcount = 0;
            UNSETBUILTIN((typedObjectknoop*)goal);
#endif
            }
#ifndef BUILTIN
        UNSETCREATEDWITHNEW((typedObjectknoop*)goal);
#endif
        goal->ops = src->ops & ~ALL_REFCOUNT_BITS_SET;
        goal->LEFT = zelfde_als_w(src->LEFT);
        goal->RIGHT = zelfde_als_w(src->RIGHT);
        return goal;
        }
    else
        return objectcopiesub2(src);
    }

static psk objectcopie(psk src)
    {
    psk goal;
    if(is_object(src))                              /* Make a copy of this '=' node ... */
        {
        if(ISBUILTIN((objectknoop*)src))
            {
            goal = (psk)bmalloc(__LINE__,sizeof(typedObjectknoop));
#ifdef BUILTIN
            ((typedObjectknoop*)goal)->u.Int = BUILTIN;
#else
            ((typedObjectknoop*)goal)->refcount = 0;
            UNSETCREATEDWITHNEW((typedObjectknoop*)goal);/*TODO: This line seems to be superfluous*/
            SETBUILTIN((typedObjectknoop*)goal);
#endif
            ((typedObjectknoop*)goal)->vtab = ((typedObjectknoop*)src)->vtab;
            ((typedObjectknoop*)goal)->voiddata = NULL;
            }
        else
            {
            goal = (psk)bmalloc(__LINE__,sizeof(objectknoop));
#ifdef BUILTIN
            ((typedObjectknoop*)goal)->u.Int = 0;
#else
            ((typedObjectknoop*)goal)->refcount = 0;
            UNSETBUILTIN((typedObjectknoop*)goal);
#endif
            }
#ifndef BUILTIN
        UNSETCREATEDWITHNEW((typedObjectknoop*)goal);
#endif
        goal->ops = src->ops & ~ALL_REFCOUNT_BITS_SET;
        goal->LEFT = zelfde_als_w(src->LEFT);
        /*?? This adds an extra level of copying, but ONLY for objects that have a '=' node as the lhs of the main '=' node*/
        /* What is it good for? Bart 20010220 */
        goal->RIGHT = objectcopiesub(src->RIGHT); /* and of all '=' child nodes (but not of grandchildren!) */
        return goal;
        }
    else
        return objectcopiesub2(src);
    }

static psk getObjectDef(psk source)
    {
    psk def;
    typedObjectknoop * dest;
    if(!is_op(source))
        {
        classdef * df = classes;
        for(;df->name && strcmp(df->name,(char *)POBJ(source));++df)
            ;
        if(df->vtab)
            {
            dest = (typedObjectknoop *)bmalloc(__LINE__,sizeof(typedObjectknoop));
            dest->v.fl = EQUALS | SUCCESS;
            dest->links = zelfde_als_w(&nilk);
            dest->rechts = zelfde_als_w(source);
#ifdef BUILTIN
            dest->u.Int = BUILTIN;
#else
            dest->refcount = 0;
            SETBUILTIN(dest);
#endif
            VOID(dest) = NULL;
            dest->vtab = df->vtab;
            return (psk)dest;
            }
        }
    else if(kop(source) == EQUALS)
        {
        source->RIGHT = Head(source->RIGHT);
        return objectcopie(source);
        }



    if((def = Naamwoord_w(source,source->v.fl & DOUBLY_INDIRECT)) != NULL)
        {
        dest = (typedObjectknoop *)bmalloc(__LINE__,sizeof(typedObjectknoop));
        dest->v.fl = EQUALS | SUCCESS;
        dest->links = zelfde_als_w(&nilk);
        dest->rechts = objectcopie(def); /* TODO Head(&def) ? */
        wis(def);
#ifdef BUILTIN
        dest->u.Int = 0;
#else
        dest->refcount = 0;
        UNSETBUILTIN(dest);
#endif
        VOID(dest) = NULL;
        dest->vtab = NULL;
        return (psk)dest;
        }
    return NULL;
    }

static psk changeCase(psk pkn
#if CODEPAGE850
                      ,int dos
#endif
                      ,int low)
    {
    const char * s;
    psk kn;
    size_t len;
    kn = zelfde_als_w(pkn);
    s = SPOBJ(pkn);
    len = strlen((const char *)s);
    if(len > 0)
        {
        char * d;
        char * dwarn;
        char * buf = NULL;
        char * obuf;
        kn = prive(kn);
        d = SPOBJ(kn);
        obuf = d;
        dwarn = obuf + strlen((const char*)obuf) - 6;
#if CODEPAGE850
        if(dos)
            {
            if(low)
                {
                for(;*s;++s)
                    {
                    *s = ISO8859toCodePage850(lowerEquivalent[(int)(const unsigned char)*s]);
                    }
                }
            else
                {
                for(;*s;++s)
                    {
                    *s = ISO8859toCodePage850(upperEquivalent[(int)(const unsigned char)*s]);
                    }
                }
            }
        else
#endif
            {
            int isutf = 1;
            struct ccaseconv * t = low ? u2l : l2u;
            for(;*s;)
                {
                int S = getCodePoint2(&s,&isutf);
                int D = convertLetter(S,t);
                if(isutf)
                    {
                    if(d >= dwarn)
                        {
                        int nb = utf8bytes(D);
                        if(d + nb >= dwarn+6)
                            {
                            /* overrun */
                            buf = (char *)bmalloc(__LINE__,2*((dwarn+6) - obuf));
                            dwarn = buf + 2*((dwarn+6) - obuf) - 6;
                            memcpy(buf,obuf,d - obuf);
                            d = buf + (d - obuf);
                            if(obuf != SPOBJ(kn))
                                bfree(obuf);
                            obuf = buf;
                            }
                        }
                    d = putCodePoint(D,d);
                    }
                else
                    *d++ = (unsigned char)D;
                }
            *d = 0;
            if(buf)
                {
                wis(kn);
                kn = scopy(buf);
                bfree(buf);
                }
            }
        }
    return kn;
    }

#if !defined NO_C_INTERFACE
static void * strToPointer(const char * str)
    {
    size_t res = 0;
    while(*str)
        res = 10*res+(*str++ - '0');
    return (void *)res;
    }

static void pointerToStr(char * pc,void * p)
    {
    size_t P = (size_t)p;
    char * PC = pc;
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
static psk swi(psk pkn,psk rlknoop,psk rrknoop)
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
    for(i = 0;i < sizeof(os_regset)/sizeof(int);i++)
        u.s.regs.r[i] = 0;
    rrknoop = pkn;
    i=0;
    do
        {
        rrknoop = rrknoop->RIGHT;
        rlknoop = is_op(rrknoop) ? rrknoop->LEFT : rrknoop;
        if(is_op(rlknoop) || !INTEGER_NIET_NEG(rlknoop))
            return functionFail(pkn);
        u.i[i++] = (unsigned int)
            strtoul((char *)POBJ(rlknoop),(char **)NULL,10);
        }
    while(is_op(rrknoop) && i < 10);
#ifdef __TURBOC__
    intr(u.s.swicode,(struct REGPACK *)&u.s.regs);
    sprintf(pc,"0.%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
        u.i[1],u.i[2],u.i[3],u.i[4],u.i[5],
        u.i[6],u.i[7],u.i[8],u.i[9],u.i[10]);
#else
#if defined ARM
    i = (int)os_swix(u.s.swicode,&u.s.regs);
    sprintf(pc,"%u.%u,%u,%u,%u,%u,%u,%u,%u,%u,%u",
        i,
        u.i[1],u.i[2],u.i[3],u.i[4],u.i[5],
        u.i[6],u.i[7],u.i[8],u.i[9],u.i[10]);
#endif
#endif
    return opb(pkn,pc,NULL);
    }
#endif

static void stringreverse(char * a,size_t len)
    {
    char * b;
    b = a + len;
    while(a < --b)
        {
        char c = *a;
        *a = *b;
        *b = c;
        ++a;
        }
    }

static void print_clock(char * pklad,clock_t time)
    {
    if(time == -1)
        sprintf(pklad,"-1");
    else
#if defined __TURBOC__ && !defined __BORLANDC__
        sprintf(pklad,"%0lu/%lu",(unsigned LONG)time,(unsigned LONG)(10.0*CLOCKS_PER_SEC));/* CLOCKS_PER_SEC == 18.2 */
#else
        sprintf(pklad,LONG0D "/" LONGD,(LONG )time,(LONG)CLOCKS_PER_SEC);
#endif
    }

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


static function_return_type functies(psk pkn)
    {
    static char klad[22];
    psk lknoop,rknoop,rrknoop,rlknoop;
    int intVal;
    if(is_op(lknoop = pkn->LEFT))
        return find_func(pkn);
    rknoop = pkn->RIGHT;
    {
    SWITCH(PLOBJ(lknoop))
        {
        FIRSTCASE(STR) /* str$(arg arg arg .. ..) */
            {
            mooi = FALSE;
            hum = 0;
            telling = 1;
            verwerk = tstr;
            result(rknoop);
            rlknoop = (psk)bmalloc(__LINE__, sizeof(unsigned LONG) + telling);
            verwerk = pstr;
            bron = POBJ(rlknoop);
            result(rknoop);
            rlknoop->v.fl = (READY | SUCCESS) | (numbercheck(SPOBJ(rlknoop)) & ~DEFINITELYNONUMBER);
            mooi = TRUE;
            hum = 1;
            verwerk = myputc;
            wis(pkn);
            pkn = rlknoop;
            return functionOk(pkn);
            }
#if O_S
        CASE(SWI) /* swi$(<interrupt number>.(input regs)) */
            {
            pkn = swi(pkn,rlknoop,rrknoop);
            return functionOk(pkn);
            }
#endif
#if _BRACMATEMBEDDED
#if defined PYTHONINTERFACE
        CASE(NI) /* Ni$"Statements to be executed by Python" */
            {
            if(Ni && !is_op(rknoop) && !HAS_VISIBLE_FLAGS_OR_MINUS(rknoop))
                {
                Ni((const char *)POBJ(rknoop));
                return functionOk(pkn);
                }
            else
                return functionFail(pkn);
            }
        CASE(NII) /* Ni!$"Expression to be evaluated by Python" */
            {
            if(Nii && !is_op(rknoop) && !HAS_VISIBLE_FLAGS_OR_MINUS(rknoop))
                {
                const char * val;
                val = Nii((const char *)POBJ(rknoop));
                wis(pkn);
                pkn = scopy((const char *)val);
                return functionOk(pkn);
                }
            else
                return functionFail(pkn);
            }
#endif
#endif

#ifdef ERR
/*#if !_BRACMATEMBEDDED*/
        CASE(ERR) /* err $ <file name to direct errorStream to> */
            {
            if(!is_op(rknoop))
                {
                if(redirectError((char *)POBJ(rknoop)))
                    return functionOk(pkn);
                }
            return functionFail(pkn);
            }
/*#endif*/
#endif
#if !defined NO_C_INTERFACE
        CASE(ALC)  /* alc $ <number of bytes> */
            {
            void *p;
            if(is_op(rknoop)
            || !INTEGER_POS(rknoop)
            || (p = bmalloc(__LINE__,(int)strtoul((char *)POBJ(rknoop),(char **)NULL,10)))
                  == NULL)
                return functionFail(pkn);
            pointerToStr(klad,p);
            wis(pkn);
            pkn = scopy((const char *)klad);
            return functionOk(pkn);
            }
        CASE(FRE) /* fre $ <pointer> */
            {
            void * p;
            if(is_op(rknoop) || !INTEGER_POS(rknoop))
                return functionFail(pkn);
            p = strToPointer((char *)POBJ(rknoop));
            pskfree((psk)p);
            return functionOk(pkn);
            }
        CASE(PEE) /* pee $ (<pointer>[,<number of bytes>]) (1,2,4,8)*/
            {
            void *p;
            intVal = 1;
            if(is_op(rknoop))
                {
                rlknoop = rknoop->LEFT;
                rrknoop = rknoop->RIGHT;
                if(!is_op(rrknoop))
                    switch(rrknoop->u.obj)
                        {
                        case '2':
                            intVal = 2;
                            break;
                        case '4':
                            intVal = 4;
                            break;
                        }
                }
            else
                rlknoop = rknoop;
            if(is_op(rlknoop) || !INTEGER_POS(rlknoop))
                return functionFail(pkn);
            p = strToPointer((char *)POBJ(rlknoop));
            p = (void*)((char *)p - (ptrdiff_t)((size_t)p % intVal));
            switch(intVal)
                {
                case 2:
                    sprintf(klad,"%hu",*(short unsigned int*)p);
                    break;
                case 4:
                    sprintf(klad,"%lu",(unsigned long)*(UINT32_T*)p);
                    break;
#ifndef __BORLANDC__
#if (!defined ARM || defined __SYMBIAN32__)
                case 8:
                    sprintf(klad,"%llu",*(unsigned long long*)p);
                    break;
#endif
#endif                    
                case 1:
                default:
                    sprintf(klad,"%u",(int)*(unsigned char *)p);
                    break;
                }
            wis(pkn);
            pkn = scopy((const char *)klad);
            return functionOk(pkn);
            }
        CASE(POK) /* pok $ (<pointer>,<number>[,<number of bytes>]) */
            {
            psk rrlknoop;
            void *p;
            LONG val;
            intVal = 1;
            if(!is_op(rknoop))
                return functionFail(pkn);
            rlknoop = rknoop->LEFT;
            rrknoop = rknoop->RIGHT;
            if(is_op(rrknoop))
                {
                psk rrrknoop;
                rrrknoop = rrknoop->RIGHT;
                rrlknoop = rrknoop->LEFT;
                if(!is_op(rrrknoop))
                    switch(rrrknoop->u.obj)
                        {
                        case '2':
                            intVal = 2;
                            break;
                        case '4':
                            intVal = 4;
                            break;
                        case '8':
                            intVal = 8;
                            break;
                        default:
                            ;
                        }
                }
            else
                rrlknoop = rrknoop;
            if(is_op(rlknoop) || !INTEGER_POS(rlknoop)
            || is_op(rrlknoop) || !INTEGER(rrlknoop))
                return functionFail(pkn);
            p = strToPointer((char *)POBJ(rlknoop));
            p = (void*)((char *)p - (ptrdiff_t)((size_t)p % intVal));
            val = toLong(rrlknoop);
            switch(intVal)
                {
                case 2:
                    *(unsigned short int*)p = (unsigned short int)val;
                    break;
                case 4:
                    *(UINT32_T*)p = (UINT32_T)val;
                    break;
#ifndef __BORLANDC__
                case 8:
                    *(unsigned LONG*)p = (unsigned LONG)val;
                    break;
#endif
                case 1:
                default:
                    *(unsigned char *)p = (unsigned char)val;
                    break;
                }
            return functionOk(pkn);
            }
        CASE(FNC) /* fnc $ (<function pointer>.<struct pointer>) */
            {
            typedef Boolean (*fncTp)(void *);
            union
                {
                fncTp pfnc; /* Hoping this works. */
                void * vp;  /* Pointers to data and pointers to functions may
                               have different sizes. */
                } u;
            void * argStruct;
            if(sizeof(int (*)(void *)) != sizeof(void *) || !is_op(rknoop))
                return functionFail(pkn);
            u.vp = strToPointer((char *)POBJ(rknoop->LEFT));
            if(!u.pfnc)
                return functionFail(pkn);
            argStruct = strToPointer((char *)POBJ(rknoop->RIGHT));
            return u.pfnc(argStruct) ? functionOk(pkn) : functionFail(pkn);
            }
#endif
        CASE(X2D) /* x2d $ hexnumber */
            {
            char * endptr;
            unsigned LONG val;
            if(  is_op(rknoop)
              || HAS_VISIBLE_FLAGS_OR_MINUS(rknoop)
              )
                return functionFail(pkn);
            errno = 0;
            val = STRTOUL((char *)POBJ(rknoop),&endptr,16);
            if(errno == ERANGE || (endptr && *endptr))
                return functionFail(pkn); /*not all characters scanned*/
            sprintf(klad,LONGU,val);
            wis(pkn);
            pkn = scopy((const char *)klad);
            return functionOk(pkn);
            }
        CASE(D2X) /* d2x $ decimalnumber */
            {
            char * endptr;
            unsigned LONG val;
            if(is_op(rknoop) || !INTEGER_NIET_NEG(rknoop))
                return functionFail(pkn);
#ifdef __BORLANDC__
            if(  strlen((char *)POBJ(rknoop)) > 10
              ||    strlen((char *)POBJ(rknoop)) == 10
                 && strcmp((char *)POBJ(rknoop),"4294967295") > 0
              )
                return functionFail(pkn); /*not all characters scanned*/
#endif
            errno = 0;
            val = STRTOUL((char *)POBJ(rknoop),&endptr,10);
            if(  errno == ERANGE
              || (endptr && *endptr)
              )
                return functionFail(pkn); /*not all characters scanned*/
            sprintf(klad,LONGX,val);
            wis(pkn);
            pkn = scopy((const char *)klad);
            return functionOk(pkn);
            }
        CASE(KAR) /* chr $ number */
            {
            if(is_op(rknoop) || !INTEGER_POS(rknoop))
                return functionFail(pkn);
            intVal = strtoul((char *)POBJ(rknoop),(char **)NULL,10);
            if(intVal > 255)
                return functionFail(pkn);
            klad[0] = (char)intVal;
            klad[1] = 0;
            wis(pkn);
            pkn = scopy((const char *)klad);
            return functionOk(pkn);
            }
        CASE(KAU) /* chu $ number */
            {
            unsigned LONG val;
            if(is_op(rknoop) || !INTEGER_POS(rknoop))
                return functionFail(pkn);
            val = STRTOUL((char *)POBJ(rknoop),(char **)NULL,10);
            if(putCodePoint(val,(char *)klad) == NULL)
                return functionFail(pkn);
            wis(pkn);
            pkn = scopy((const char *)klad);
            return functionOk(pkn);
            }
        CASE(ASC) /* asc $ character */
            {
            if(is_op(rknoop))
                return functionFail(pkn);
            sprintf(klad,"%d",(int)rknoop->u.obj);
            wis(pkn);
            pkn = scopy((const char *)klad);
            return functionOk(pkn);
            }
        CASE(UTF)
            {
/*
@(abcædef:? (%@>"~" ?:?a & utf$!a) ?)
@(str$(abc chu$200 def):? (%@>"~" ?:?a & utf$!a) ?)
*/
            if(is_op(rknoop))
                {
                pkn->v.fl |= FENCE;
                return functionFail(pkn);
                }
            else
                {
                const char * s = (const char *)POBJ(rknoop);
                intVal = getCodePoint(&s);
                if(intVal < 0 || *s)
                    {
                    if(intVal != -2)
                        {
                        pkn->v.fl |= IMPLIEDFENCE;
                        }
                    return functionFail(pkn);
                    }
                sprintf(klad,"%d",intVal);
                wis(pkn);
                pkn = scopy((const char *)klad);
                return functionOk(pkn);
                }
            }
#if !defined NO_LOW_LEVEL_FILE_HANDLING
#if !defined NO_FOPEN
        CASE(FIL) /* fil $ (<name>,[<offset>,[set|cur|end]]) */
            {
            return fil(&pkn) ? functionOk(pkn) : functionFail(pkn);
            }
#endif
#endif
        CASE(FLG) /* flg $ <expr>  or flg$(=<expr>) */
            {
            if(is_object(rknoop) && !(rknoop->LEFT->v.fl & VISIBLE_FLAGS))
                rknoop = rknoop->RIGHT;
            intVal = rknoop->v.fl;
            adr[3] = zelfde_als_w(rknoop);
            adr[3] = prive(adr[3]);
            adr[3]->v.fl = adr[3]->v.fl & ~VISIBLE_FLAGS;
            adr[2] = zelfde_als_w(&nilk);
            adr[2] = prive(adr[2]);
            adr[2]->v.fl &= ~VISIBLE_FLAGS;
            adr[2]->v.fl |= VISIBLE_FLAGS & intVal;
            if(adr[2]->v.fl & INDIRECT)
                {
                adr[2]->v.fl &= ~READY; /* {?} flg$(=!a):(=?X.?)&lst$X */
                adr[3]->v.fl |= READY;  /* {?} flg$(=!a):(=?.?Y)&!Y */
                }
            if(NIKSF(intVal))
                {
                adr[2]->v.fl ^= SUCCESS;
                adr[3]->v.fl ^= SUCCESS;
                }
            sprintf(klad,"=\2.\3");
            pkn = opb(pkn,klad,NULL);
            wis(adr[2]);
            wis(adr[3]);
            return functionOk(pkn);
            }
        CASE(GLF) /* glf $ (=<flags>.<exp>) : (=?a)  a=<flags><exp> */
            {
            if(  is_object(rknoop)
              && kop(rknoop->RIGHT) == DOT
              )
                {
                intVal = rknoop->RIGHT->LEFT->v.fl & VISIBLE_FLAGS;
                if(intVal && (rknoop->RIGHT->RIGHT->v.fl & intVal))
                    return functionFail(pkn);
                adr[3] = zelfde_als_w(rknoop->RIGHT->RIGHT);
                adr[3] = prive(adr[3]);
                adr[3]->v.fl |= intVal;
                if(NIKSF(intVal))
                    {
                    adr[3]->v.fl ^= SUCCESS;
                    }
                if(intVal & INDIRECT)
                    {
                    adr[3]->v.fl &= ~READY;/* ^= --> &= ~ */
                    }
                sprintf(klad,"=\3");
                pkn = opb(pkn,klad,NULL);
                wis(adr[3]);
                return functionOk(pkn);
                }
            return functionFail(pkn);
            }
#if TELMAX
        CASE(BEZ) /* bez $  */
            {
#if MAXSTACK
#if defined _WIN32
            sprintf(klad,"%lu.%lu.%u.%d",(unsigned long)globalloc,(unsigned long)maxgloballoc,maxbez / ONE,maxstack);
#else
            sprintf(klad,"%zu.%zu.%u.%d",globalloc,maxgloballoc,maxbez / ONE,maxstack);
#endif
#else
#if defined _WIN32
            sprintf(klad,"%lu.%lu.%u",(unsigned long)globalloc,(unsigned long)maxgloballoc,maxbez / ONE);
#else
            sprintf(klad,"%zu.%zu.%u",globalloc,maxgloballoc,maxbez / ONE);
#endif
#endif
            pkn = opb(pkn,klad,NULL);
#if TELLING
            bezetting();
#endif
            return functionOk(pkn);
            }
#endif
        CASE(MMF) /* mem $ [EXT] */
            {
            mmf(&pkn);
            return functionOk(pkn);
            }
        CASE(MOD)
            {
            if(RATIONAAL_COMP(rlknoop = rknoop->LEFT) &&
               RATIONAAL_COMP_NOT_NUL(rrknoop = rknoop->RIGHT))
                {
                psk kn;
                kn = _qmodulo(rlknoop,rrknoop);
                wis(pkn);
                pkn = kn;
                return functionOk(pkn);
                }
            return functionFail(pkn);
            }
        CASE(REV)
            {
            if(!is_op(rknoop))
                {
                size_t len = strlen((char *)POBJ(rknoop));
                psk kn;
                kn = zelfde_als_w(rknoop);
                if(len > 1)
                    {
                    kn = prive(kn);
                    stringreverse((char *)POBJ(kn),len);
                    }
                wis(pkn);
                pkn = kn;
                return functionOk(pkn);
                }
            else
                return functionFail(pkn);
            }
        CASE(LOW)
            {
            psk kn;
            if(!is_op(rknoop))
                kn = changeCase(rknoop
#if CODEPAGE850
                ,FALSE
#endif
                ,TRUE);
            else if(!is_op(rlknoop = rknoop->LEFT))
                kn = changeCase(rlknoop
#if CODEPAGE850
                ,zoekopt(rknoop->RIGHT,DOS)
#endif
                ,TRUE);
            else
                return functionFail(pkn);
            wis(pkn);
            pkn = kn;
            return functionOk(pkn);
            }
        CASE(UPP)
            {
            psk kn;
            if(!is_op(rknoop))
                kn = changeCase(rknoop
#if CODEPAGE850
                ,FALSE
#endif
                ,FALSE);
            else if(!is_op(rlknoop = rknoop->LEFT))
                kn = changeCase(rlknoop
#if CODEPAGE850
                ,zoekopt(rknoop->RIGHT,DOS)
#endif
                ,FALSE);
            else
                return functionFail(pkn);
            wis(pkn);
            pkn = kn;
            return functionOk(pkn);
            }
        CASE(DIV)
            {
            if(  is_op(rknoop)
              && RATIONAAL_COMP(rlknoop = rknoop->LEFT)
              && RATIONAAL_COMP_NOT_NUL(rrknoop = rknoop->RIGHT)
              )
                {
                psk kn;
                kn = _qheeldeel(rlknoop,rrknoop);
                wis(pkn);
                pkn = kn;
                return functionOk(pkn);
                }
            return functionFail(pkn);
            }
        CASE(DEN)
            {
            if(RATIONAAL_COMP(rknoop))
                {
                pkn = _qdenominator(pkn);
                }
            return functionOk(pkn);
            }
        CASE(LST)
            {
            return output(&pkn,lst) ? functionOk(pkn) : functionFail(pkn);
            }
#if !defined NO_FILE_RENAME
        CASE(REN)
            {
            if(   is_op(rknoop)
              && !is_op(rlknoop = rknoop->LEFT)
              && !is_op(rrknoop = rknoop->RIGHT)
              )
                {
                intVal = rename((const char *)POBJ(rlknoop),(const char *)POBJ(rrknoop));
                if(intVal)
                    {
#ifndef EACCES
                    sprintf(klad,"%d",intVal);
#else
                    switch(errno)
                        {
                        case EACCES:
                            /*
                            File or directory specified by newname already exists or
                            could not be created (invalid path); or oldname is a directory
                            and newname specifies a different path.
                            */
                            strcpy(klad,"EACCES");
                            break;
                        case ENOENT:
                            /*
                            File or path specified by oldname not found.
                            */
                            strcpy(klad,"ENOENT");
                            break;
                        case EINVAL:
                            /*
                            Name contains invalid characters.
                            */
                            strcpy(klad,"EINVAL");
                            break;
                        default:
                            sprintf(klad,"%d",errno);
                            break;
                        }
#endif
                    }
                else
                    strcpy(klad,"0");
                wis(pkn);
                pkn = scopy((const char *)klad);
                return functionOk(pkn);
                }
            else
                return functionFail(pkn);
            }
#endif
#if !defined NO_FILE_REMOVE
        CASE(RMV)
            {
            if(!is_op(rknoop))
                {
#if defined __SYMBIAN32__
                intVal = unlink((const char *)POBJ(rknoop));
#else
                intVal = remove((const char *)POBJ(rknoop));
#endif
                if(intVal)
                    {
#ifndef EACCES
                    sprintf(klad,"%d",intVal);
#else
                    switch(errno)
                        {
                        case EACCES:
                            /*
                            File or directory specified by newname already exists or
                            could not be created (invalid path); or oldname is a directory
                            and newname specifies a different path.
                            */
                            strcpy(klad,"EACCES");
                            break;
                        case ENOENT:
                            /*
                            File or path specified by oldname not found.
                            */
                            strcpy(klad,"ENOENT");
                            break;
                        default:
                            sprintf(klad,"%d",errno);
                            break;
                        }
#endif
                    }
                else
                    strcpy(klad,"0");
                wis(pkn);
                pkn = scopy((const char *)klad);
                return functionOk(pkn);
                }
            else
                return functionFail(pkn);
            }
#endif
        CASE(ARG) /* arg$ or arg$N  (N == 0,1,... and N < argc) */
            {
            static int argno = 0;
            if(is_op(rknoop))
                return functionFail(pkn);
            if(PLOBJ(rknoop) != '\0')
                {
                LONG val;
                if(!INTEGER_NIET_NEG(rknoop))
                    return functionFail(pkn);
                val = STRTOUL((char *)POBJ(rknoop),(char **)NULL,10);
                if(val >= ARGC)
                    return functionFail(pkn);
                wis(pkn);
                pkn = scopy((const char *)ARGV[val]);
                return functionOk(pkn);
                }
            if(argno < ARGC)
                {
                wis(pkn);
                pkn = scopy((const char *)ARGV[argno++]);
                return functionOk(pkn);
                }
            else
                {
                return functionFail(pkn);
                }
            }
        CASE(GET) /* get$file */
            {
            Boolean GoOn;
            int err = 0;
            if(is_op(rknoop))
                {
                if(is_op(rlknoop = rknoop->LEFT))
                    return functionFail(pkn);
                rrknoop = rknoop->RIGHT;
                intVal = (zoekopt(rrknoop,ECH) << SHIFT_ECH)
                       + (zoekopt(rrknoop,MEM) << SHIFT_MEM)
                       + (zoekopt(rrknoop,VAP) << SHIFT_VAP)
                       + (zoekopt(rrknoop,STG) << SHIFT_STR)
                       + (zoekopt(rrknoop,ML ) << SHIFT_ML)
                       + (zoekopt(rrknoop,TRM) << SHIFT_TRM)
                       + (zoekopt(rrknoop,HT)  << SHIFT_HT)
                       + (zoekopt(rrknoop,X)   << SHIFT_X)
                       + (zoekopt(rrknoop,JSN) << SHIFT_JSN)
                       + (zoekopt(rrknoop,TXT) << SHIFT_TXT)
                       + (zoekopt(rrknoop,BIN) << SHIFT_BIN);
                }
            else
                {
                intVal = 0;
                rlknoop = rknoop;
                }
            if(intVal & OPT_MEM)
                {
                adr[1] = zelfde_als_w(rlknoop);
                bron = POBJ(adr[1]);
                for(;;)
                    {
                    pkn = input(NULL,pkn,intVal,&err,&GoOn);
                    if(!GoOn || err)
                        break;
                    pkn = eval(pkn);
                    }
                wis(adr[1]);
                }
            else
                {
                if(rlknoop->u.obj && strcmp((char *)POBJ(rlknoop),"stdin"))
                    {
#if !defined NO_FOPEN
                    FILE *red;
                    filehendel * fh;
                    red = global_fpi;
                    fh = myfopen((char *)POBJ(rlknoop),(intVal & OPT_TXT) ? READTXT : READBIN
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                        ,TRUE
#endif
                        );
                    if(fh == NULL)
                        {
                        global_fpi = red;
                        return functionFail(pkn);
                        }
                    else
                        global_fpi = fh->fp;
                    for(;;)
                        {
                        pkn = input(global_fpi,pkn,intVal,&err,&GoOn);
                        if(!GoOn || err)
                            break;
                        pkn = eval(pkn);
                        }
                    deallocateFilehendel(fh);
                    global_fpi = red;
#else
                    return functionFail(pkn);
#endif
                    }
                else
                    {
                    intVal |= OPT_ECH;
#ifdef DELAY_DUE_TO_INPUT
                    for(;;)
                        {
                        clock_t time0;
                        time0 = clock();
                        pkn = input(stdin,pkn,intVal,&err,&GoOn);
                        delayDueToInput += clock() - time0;
                        if(!GoOn || err)
                            break;
                        pkn = eval(pkn);
                        }
#else
                    for(;;)
                        {
                        pkn = input(stdin,pkn,intVal,&err,&GoOn);
                        if(!GoOn || err)
                            break;
                        pkn = eval(pkn);
                        }
#endif
                    }
                }
            return err ? functionFail(pkn) : functionOk(pkn);
            }
        CASE(PUT) /* put$(file,mode,knoop) of put$knoop */
            {
            return output(&pkn,result) ? functionOk(pkn) : functionFail(pkn);
            }
#if !defined __SYMBIAN32__
#if !defined NO_SYSTEM_CALL
        CASE(SYS)
            {
            if(is_op(rknoop) || (PLOBJ(rknoop) == '\0'))
                return functionFail(pkn);
            else
                {
                intVal = system((const char *)POBJ(rknoop));
                if(intVal)
#ifndef E2BIG
                    sprintf(klad,"%d",intVal);
#else
                    switch(errno)
                        {
                        case E2BIG:
                            /*
                            Argument list (which is system-dependent) is too big.
                            */
                            strcpy(klad,"E2BIG");
                            break;
                        case ENOENT:
                            /*
                            Command interpreter cannot be found.
                            */
                            strcpy(klad,"ENOENT");
                            break;
                        case ENOEXEC:
                            /*
                            Command-interpreter file has invalid format and is not executable.
                            */
                            strcpy(klad,"ENOEXEC");
                            break;
                        case ENOMEM:
                            /*
                            Not enough memory is available to execute command; or available
                            memory has been corrupted; or invalid block exists, indicating
                            that process making call was not allocated properly.
                            */
                            strcpy(klad,"ENOMEM");
                            break;
                        default:
                            sprintf(klad,"%d",errno);
                            break;
                        }
#endif
                else
                    strcpy(klad,"0");
                wis(pkn);
                pkn = scopy((const char *)klad);
                return functionOk(pkn);
                }
            }
#endif
#endif
        CASE(TBL) /* tbl$(varname,length) */
            {
            if(is_op(rknoop))
                return psh(rknoop->LEFT,&nulk,rknoop->RIGHT) ? functionOk(pkn) : functionFail(pkn);
            else
                return functionFail(pkn);
            }
#if 0
/*
The same effect is obtained by <expr>:?!(=)
*/
        CASE(PRV) /* "?"$<expr> */
            {
            if((rknoop->v.fl & SUCCESS)
            && (is_op(rknoop) || rknoop->u.obj || HAS_UNOPS(rknoop)))
                insert(&nilk,rknoop);
            pkn = rechtertak(pkn);
            return functionOk(pkn);
            }
#endif
        CASE(CLK) /* clk' */
            {
            clock_t time = clock();
#ifdef DELAY_DUE_TO_INPUT
            time -= delayDueToInput;
#endif
            print_clock(klad,time);
            wis(pkn);
            pkn = scopy((const char *)klad);
            return functionOk(pkn);
            }

        CASE(SIM) /* sim$(<atom>,<atom>) , fuzzy compare (percentage) */
            {
            if(is_op(rknoop)
            && !is_op(rlknoop = rknoop->LEFT)
            && !is_op(rrknoop = rknoop->RIGHT))
                {
                Sim(klad,(char *)POBJ(rlknoop),(char *)POBJ(rrknoop));
                wis(pkn);
                pkn = scopy((const char *)klad);
                return functionOk(pkn);
                }
            else
                return functionFail(pkn);
            }
#if DEBUGBRACMAT
        CASE(DBG) /* dbg$<expr> */
            {
            ++debug;
            if(kop(pkn) != FUU)
                {
                errorprintf("Use dbg'(expression), not dbg$(expression)!\n");
                writeError(pkn);
                }
            pkn = rechtertak(pkn);
            pkn = eval(pkn);
            --debug;
            return functionOk(pkn);
            }
#endif
        CASE(WHL)
            {
            while(isSUCCESSorFENCE(rknoop = eval(zelfde_als_w(pkn->RIGHT))))
                {
                wis(rknoop);
                }
            wis(rknoop);
            return functionOk(pkn);
            }
        CASE(New) /* new$<object>*/
            {
            if(kop(rknoop) == COMMA)
                {
                adr[2] = getObjectDef(rknoop->LEFT);
                if(!adr[2])
                    return functionFail(pkn);
                adr[3] = rknoop->RIGHT;
                if(ISBUILTIN((objectknoop*)adr[2]))
                    pkn = opb(pkn,"(((\2.New)'\3)|)&\2",NULL);
    /* We might be able to call 'new' if 'New' had attached the argument
        (containing the definition of a 'new' method) to the rhs of the '='.
       This cannot be done in a general way without introducing new syntax rules for the new$ function.
    */
                else
                    pkn = opb(pkn,"(((\2.new)'\3)|)&\2",NULL);
                }
            else
                {
                adr[2] = getObjectDef(rknoop);
                DBGSRC(printf("adr[2]:");result(adr[2]);printf("\n");)
                if(!adr[2])
                    return functionFail(pkn);
                if(ISBUILTIN((objectknoop*)adr[2]))
                    pkn = opb(pkn,"(((\2.New)')|)&\2",NULL);
                /* There cannot be a user-defined 'new' method on a built-in object if there is no way to supply it*/
                /* 'die' CAN be user-supplied. The built-in function is 'Die' */
                else
                    pkn = opb(pkn,"(((\2.new)')|)&\2",NULL);
                }
            SETCREATEDWITHNEW((objectknoop*)adr[2]);
            wis(adr[2]);
            return functionOk(pkn);
            }
        CASE(0) /* $<expr>  '<expr> */
            {
            if(kop(pkn) == FUU)
                {
                if(!HAS_UNOPS(pkn->LEFT))
                    {
                    intVal = pkn->v.fl & UNOPS;
                    if(  intVal == BREUK 
                      && is_op(pkn->RIGHT)
                      && kop(pkn->RIGHT) == DOT
                      && !is_op(pkn->RIGHT->LEFT)
                      )
                        { /* /('(a.a+2))*/
                        return functionOk(pkn);
                        }
                    else
                        {
                        rknoop = evalmacro(pkn->RIGHT);
                        rrknoop = (psk)bmalloc(__LINE__,sizeof(objectknoop));
#ifdef BUILTIN
                        ((typedObjectknoop*)rrknoop)->u.Int = 0;
#else
                        ((typedObjectknoop*)rrknoop)->refcount = 0;
                        UNSETCREATEDWITHNEW((typedObjectknoop*)rrknoop);
                        UNSETBUILTIN((typedObjectknoop*)rrknoop);
#endif
                        rrknoop->v.fl = EQUALS | SUCCESS;
                        rrknoop->LEFT = zelfde_als_w(&nilk);
                        if(rknoop)
                            {
                            rrknoop->RIGHT = rknoop;
                            }
                        else
                            {
                            rrknoop->RIGHT = zelfde_als_w(pkn->RIGHT);
                            }
                        wis(pkn);
                        pkn = rrknoop;
                        pkn->v.fl |= intVal; /* (a=b)&!('$a)*/
                        }
                    }
                else
                    {
                    combiflags(pkn);
                    pkn = rechtertak(pkn);
                    }
                return functionOk(pkn);
                }
            else
                {
                return functionFail(pkn);
                }
            }
        DEFAULT
            {
            if(INTEGER(lknoop))
                {
                vars *navar;
                if(is_op(rknoop))
                    return functionFail(pkn);
                for(navar = variabelen[rknoop->u.obj];
                    navar && (STRCMP(VARNAME(navar),POBJ(rknoop)) < 0);
                    navar = navar->next);
                /* find first name in a row of equal names */
                if(navar && !STRCMP(VARNAME(navar),POBJ(rknoop)))
                    {
                    navar->selector =
                       (int)toLong(lknoop)
                     % (navar->n + 1);
                    if(navar->selector < 0)
                        navar->selector += (navar->n + 1);
                    pkn = rechtertak(pkn);
                    return functionOk(pkn);
                    }
                }
            if(!(rknoop->v.fl & SUCCESS))
                return functionFail(pkn);
            adr[1] = NULL;
            return find_func(pkn);
            }
        }
        }
    /*return functionOk(pkn); 20 Dec 1995, unreachable code in Borland C */
    }

static psk stapelmacht(psk pkn)
    {
    psk lknoop;
    Boolean done = FALSE;
    for(;((lknoop = pkn->LEFT)->v.fl & READY) && kop(lknoop) == EXP;)
        {
        done = TRUE;
        pkn->LEFT = lknoop = prive(lknoop);
        lknoop->v.fl &= ~READY & ~OPERATOR;/* turn off READY flag */
        lknoop->ops |= TIMES;
        adr[1] = lknoop->LEFT;
        adr[2] = lknoop->RIGHT;
        adr[3] = pkn->RIGHT;
        pkn = opb(pkn,"(\1^(\2*\3))",NULL);
        }
    if(done)
        {
        return pkn;
        }
    else
        {
        static const char * conc_arr[] = {NULL,NULL,NULL,NULL,NULL,NULL};

        Qgetal iexponent,
        hiexponent;

        psk rknoop;
        if(!is_op(rknoop = pkn->RIGHT))
            {
            if(RAT_NUL(rknoop))
                {
                wis(pkn);
                return copievan(&eenk);
                }
            if(IS_EEN(rknoop))
                {
                return linkertak(pkn);
                }
            }
        lknoop = pkn->LEFT;
        if(!is_op(lknoop))
            {
            if((RAT_NUL(lknoop) && !RAT_NEG_COMP(rknoop)) || IS_EEN(lknoop))
                {
                return linkertak(pkn);
                }

            if(!is_op(rknoop) && RATIONAAL_COMP(rknoop))
                {
                if(RATIONAAL_COMP(lknoop))
                    {
                    if(RAT_NEG_COMP(rknoop) && abseen(rknoop))
                        {
                        conc_arr[1] = NULL;
                        conc_arr[2] = hekje6;
                        conc_arr[3] = NULL;
                        adr[6] = _q_qdeel(&eenk,lknoop);
                        assert(lknoop == pkn->LEFT);
                        pkn = vopb(pkn, conc_arr +2);
                        wis(adr[6]);
                        return pkn;
                        }
                    else if(RAT_NEG_COMP(lknoop) && RAT_RAT_COMP(rknoop))
                        {
                        return pkn; /*{?} -3^2/3 => -3^2/3 */
                        }
                    /* Missing here is n^m, with m > 2.
                       That case is handled in casemacht. */
                    }
                else if(PLOBJ(lknoop) == IM)
                    {
                    if(_qvergelijk(rknoop,&nulk) & MINUS)
                        { /* i^-n -> -i^n */ /*{?} i^-7 => i */
                          /* -i^-n -> i^n */ /*{?} -i^-7 => -i */
                        conc_arr[0] = "(\2^\3)";
                        adr[2] = _qmaalmineen(lknoop);
                        adr[3] = _qmaalmineen(rknoop);
                        conc_arr[1] = NULL;
                        pkn = vopb(pkn, conc_arr);
                        wis(adr[2]);
                        wis(adr[3]);
                        return pkn;
                        }
                    else if(_qvergelijk(&tweek,rknoop) & (QNUL|MINUS))
                        {
                        iexponent = _qmodulo(rknoop,&vierk);
                        if(iexponent->ops & QNUL)
                            {
                            wis(pkn); /*{?} i^4 => 1 */
                            pkn = copievan(&eenk);
                            }
                        else
                            {
                            int teken;
                            teken = _qvergelijk(iexponent,&tweek);
                            if(teken & QNUL)
                                {
                                wis(pkn);
                                pkn = copievan(&mineenk);
                                }
                            else
                                {
                                if(!(teken & MINUS))
                                    {
                                    hiexponent = iexponent;
                                    iexponent = _qplus(&vierk,hiexponent,MINUS);
                                    wis(hiexponent);
                                    }
                                adr[2] = lknoop;
                                adr[6] = iexponent;
                                conc_arr[0] = "(-1*\2)^";
                                conc_arr[1] = "(\6)";
                                conc_arr[2] = NULL;
                                pkn = vopb(pkn, conc_arr);
                                }
                            }
                        wis(iexponent);
                        return pkn;
                        }
                    }
                }
            }

        if(kop(lknoop) == TIMES)
            {
            adr[1] = lknoop->LEFT;
            adr[2] = lknoop->RIGHT;
            adr[3] = pkn->RIGHT;
            return opb(pkn,"\1^\3*\2^\3",NULL);
            }

        if(RATIONAAL_COMP(lknoop))
            {
            static const char
                haakmineen[] = ")^-1",
                haakhekje1macht[] = "(\1^",
                macht2maaleenmacht[] = ")^2*\1^";
            if(INTEGER_NIET_NUL_COMP(rknoop) && !abseen(rknoop))
                {
                adr[1] = lknoop;
                if(INTEGER_POS_COMP(rknoop))
                    {
                    if(_qvergelijk(&tweek,rknoop) & MINUS)
                        {
                        /* m^n = (m^(n\2))^2*m^(n mod 2) */ /*{?} 9^7 => 4782969 */
                        conc_arr[0] = haakhekje1macht;
                        conc_arr[1] = hekje5;
                        conc_arr[3] = hekje6;
                        conc_arr[4] = NULL;
                        adr[5] = _qheeldeel(rknoop,&tweek);
                        conc_arr[2] = macht2maaleenmacht;
                        adr[6] = _qmodulo(rknoop,&tweek);
                        pkn = vopb(pkn, conc_arr);
                        wis(adr[5]);
                        wis(adr[6]);
                        }
                    else
                        {
                        /* m^2 = m*m */
                        pkn = opb(pkn,"(\1*\1)",NULL);
                        }
                    }
                else
                    {
                    /*{?} 7^-13 => 1/96889010407 */
                    conc_arr[0] = haakhekje1macht;
                    conc_arr[1] = hekje6;
                    adr[6] = _qmaalmineen(rknoop);
                    conc_arr[2] = haakmineen;
                    conc_arr[3] = 0;
                    pkn = vopb(pkn, conc_arr);
                    wis(adr[6]);
                    }
                return pkn;
                }
            else if(RAT_RAT(rknoop))
                {
                char **conc,slash = 0;
                int wipe[20],ind;
                ngetal teller = {0},noemer = {0};
                for(ind = 0; ind < 20; wipe[ind++] = TRUE);
                ind = 0;
                conc = (char **)bmalloc(__LINE__,20 * sizeof(char **));
                   /* 20 is safe value for ULONGs */
                adr[1] = pkn->RIGHT;
                if(RAT_RAT_COMP(pkn->LEFT))
                    {
                    splits(pkn->LEFT,&teller,&noemer);
                    if(!subroot(&teller,conc,&ind))
                        {
                        wipe[ind] = FALSE;
                        conc[ind++] = teller.number;
                        slash = teller.number[teller.length];
                        teller.number[teller.length] = 0;

                        wipe[ind] = FALSE;
                        conc[ind++] = "^\1";
                        }
                    wipe[ind] = FALSE;
                    conc[ind++] = "*(";
                    if(!subroot(&noemer,conc,&ind))
                        {
                        wipe[ind] = FALSE;
                        conc[ind++] = noemer.number;
                        wipe[ind] = FALSE;
                        conc[ind++] = "^\1";
                        }
                    wipe[ind] = FALSE;
                    conc[ind++] = ")^-1";
                    }
                else
                    {
                    teller.number = (char *)POBJ(pkn->LEFT);
                    teller.alloc = NULL;
                    teller.length = strlen(teller.number);
                    if(!subroot(&teller,conc,&ind))
                        {
                        bfree(conc);
                        return pkn;
                        }
                    }
                conc[ind--] = NULL;
                pkn = vopb(pkn,(const char **)conc);
                if(slash)
                    teller.number[teller.length] = slash;
                for(;ind >= 0;ind--)
                   if(wipe[ind])
                       bfree(conc[ind]);
                bfree(conc);
                return pkn;
                }
            }
        }
    if(is_op(pkn->RIGHT))
        {
        int ok;
        pkn = tryq(pkn,f4,&ok);
        }
    return pkn;
    }

/*
Improvement that DOES evaluate b+(i*c+i*d)+-i*c
It also allows much deeper structures, because the right place for insertion
is found iteratively, not recursively. This also causes some operations to
be tremendously faster. e.g. (1+a+b+c)^30+1&ready evaluates in about
4,5 seconds now, previously in 330 seconds! (AST Bravo MS 5233M 233 MHz MMX Pentium)
*/
static void splitProduct_number_im_rest(psk pknoop,ppsk N,ppsk I,ppsk NNNI)
    {
    psk temp;
    if(kop(pknoop) == TIMES)
        {
        if(RATIONAAL_COMP(pknoop->LEFT))
            {/* 17*x */
            *N = pknoop->LEFT;
            temp = pknoop->RIGHT;
            }/* N*temp */
        else
            {
            *N = NULL;
            temp = pknoop;
            }/* temp */
        if(kop(temp) == TIMES)
            {
            if(!is_op(temp->LEFT) && PLOBJ(temp->LEFT) == IM)
                {/* N*i*x */
                *I = temp->LEFT;
                *NNNI = temp->RIGHT;
                }/* N*I*NNNI */
            else
                {
                *I = NULL;
                *NNNI = temp;
                }/* N*NNNI */
            }
        else
            {
            if(!is_op(temp) && PLOBJ(temp) == IM)
                {/* N*i */
                *I = temp;
                *NNNI = NULL;
                }/* N*I */
            else
                {
                *I = NULL;
                *NNNI = temp;
                }/* N*NNNI */
            }
        }
    else if(!is_op(pknoop) && PLOBJ(pknoop) == IM)
        {/* i */
        *N = NULL;
        *I = pknoop;
        *NNNI = NULL;
        }/* I */
    else
        {/* x */
        *N = NULL;
        *I = NULL;
        *NNNI = pknoop;
        }/* NNNI */
    }

static psk rechteroperand_and_tail(psk pkn,ppsk head,ppsk tail)
    {
    psk temp;
    assert(is_op(pkn));
    temp = pkn->RIGHT;
    if(kop(pkn) == kop(temp))
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

static psk linkeroperand_and_tail(psk pkn,ppsk head,ppsk tail)
    {
    psk temp;
    assert(is_op(pkn));
    temp = pkn->LEFT;
    if(kop(pkn) == kop(temp))
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
static psk expandDummy(psk pkn,int * ok)
    {
    *ok = FALSE;
    return pkn;
    }
#endif

static psk expandProduct(psk pkn,int * ok)
    {
    switch(kop(pkn))
        {
        case TIMES :
        case EXP  :
            {
            if(  (  (match(0,pkn,m0,NULL,0,pkn,3333) & TRUE)
                 && ((pkn = tryq(pkn,f0,ok)),*ok)
                 )
              || (  (match(0,pkn,m1,NULL,0,pkn,4444) & TRUE)
                 && ((pkn = tryq(pkn,f1,ok)),*ok)
                 )
              )
                {
                if(is_op(pkn)) /*{?} (1+i)*(1+-i)+Z => 2+Z */
                    pkn->v.fl &= ~READY;
                return pkn;
                }
            break;
            }
        }
    *ok = FALSE;
    return pkn;
    }

static psk plus_samenvoegen_of_sorteren(psk pkn)
    {
    /*
    Split pkn in left L and right R argument

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
    static const char *conc[] = {NULL,NULL,NULL,NULL};
    int res = FALSE;
    psk top = pkn;

    psk L = top->LEFT;
    psk Lterm,Ltail;
    psk LtermN,LtermI,LtermNNNI;

    psk R;
    psk Rterm,Rtail;
    psk RtermN,RtermI,RtermNNNI;

    int ok;
    if(!is_op(L) && RAT_NUL_COMP(L))
        {
        /* 0+x -> x */
        return rechtertak(top);
        }

    R = top->RIGHT;
    if(!is_op(R) && RAT_NUL_COMP(R))
        {
        /*{?} x+0 => x */
        return linkertak(top);
        }

    if(  is_op(L)
      && ((top->LEFT = expandProduct(top->LEFT,&ok)),ok)
      )
        {
        res = TRUE;
        }
    if(  is_op(R)
      && ((top->RIGHT = expandProduct(top->RIGHT,&ok)),ok)
      )
        { /*
          {?} a*b+u*(x+y) => a*b+u*x+u*y
          */
        res = TRUE;
        }
    if(res)
        {
        (pkn)->v.fl &= ~READY;
        return pkn;
        }
    rechteroperand_and_tail(top,&Rterm,&Rtail);
    linkeroperand_and_tail(top,&Lterm,&Ltail);
    assert(Ltail == NULL);
    if(RATIONAAL_COMP(Lterm))
        {
        if(RATIONAAL_COMP(Rterm))
            {
            conc[0] = hekje6;
            if(Lterm == Rterm)
                {
                /* 7+7 -> 2*7 */ /*{?} 7+7 => 14 */
                adr[6] = _qmaal(&tweek,Rterm);
                }
            else
                {
                /* 4+7 -> 11 */ /*{?} 4+7 => 11 */
                adr[6] = _qplus(Lterm,Rterm,0);
                }
            conc[1] = NULL;
            conc[2] = NULL;
            if(Rtail != NULL)
                {
                adr[4] = Rtail;
                conc[1] = "+\4";
                }
            pkn = vopb     (top,conc);
            wis(adr[6]);
            }
        return pkn;
        }
    else if(RATIONAAL_COMP(Rterm))
        {
        adr[1] = Rterm;
        adr[2] = L;
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
            adr[3] = Rtail;
            return opb     (top,"\1+\2+\3",NULL);
            }
        else
            {
            /* 4*(x+7)+p+-4*x */
            return opb     (top,"\1+\2",NULL);
            }
        }

    if(  kop(Lterm) == LOG
      && kop(Rterm) == LOG
      && !equal(Lterm->LEFT,Rterm->LEFT)
      )
        {
        adr[1] = Lterm->LEFT;
        adr[2] = Lterm->RIGHT;
        adr[3] = Rterm->RIGHT;
        if(Rtail == NULL)
            return opb     (top,"\1\016(\2*\3)",NULL); /*{?} 2\L3+2\L9 => 4+2\L27/16 */
        else
            {
            adr[4] = Rtail;
            return opb     (top,"\1\016(\2*\3)+\4",NULL); /*{?} 2\L3+2\L9+3\L123 => 8+2\L27/16+3\L41/27 */
            }
        }

    splitProduct_number_im_rest(Lterm,&LtermN,&LtermI,&LtermNNNI);

    if(LtermI)
        {
        ppsk loper = &pkn;
        splitProduct_number_im_rest(Rterm,&RtermN,&RtermI,&RtermNNNI);
        while(  RtermI == NULL
             && kop((*loper)->RIGHT) == PLUS
             )
            {
            loper = &(*loper)->RIGHT;
            *loper = prive(*loper);
            rechteroperand_and_tail((*loper),&Rterm,&Rtail);
            splitProduct_number_im_rest(Rterm,&RtermN,&RtermI,&RtermNNNI);/*{?} i*x+-i*x+a => a */
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
                dif = equal(LtermNNNI,RtermNNNI);
                assert(dif <= 0 || kop((*loper)->RIGHT) != PLUS);
                }
            if(dif == 0)
                {                        /*{?} i*x+-i*x => 0 */
                if(RtermN)
                    {
                    adr[2] = RtermN; /*{?} i*x+3*i*x => 4*i*x */
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
                        adr[3] = LtermN;
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
                    adr[1] = LtermNNNI;
                    if(LtermN != NULL)
                        {
                        /* m*a+a */
                        adr[2] = LtermN; /*{?} 3*i*x+i*x => 4*i*x */
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
                    adr[1] = RtermNNNI;
                    conc[1] = "*\1";
                    indx = 2;
                    }
                else
                    indx = 1; /*{?} i+-i => 0 */
                if(Rtail != NULL)
                    {
                    adr[4] = Rtail; /*{?} -i+-i+i*y => 2*-i+i*y */
                    conc[indx++] = "+\4";
                    }
                conc[indx] = NULL;                        /*{?} i*x+-i*x => 0 */
                (*loper)->RIGHT = vopb((*loper)->RIGHT,conc);
                if(loper != &pkn)
                    {
                    (*loper)->v.fl &= ~READY;/*{?} i*x+-i*x+a => a */
                    *loper = eval(*loper);
                    }
                return rechtertak(top);                        /*{?} i*x+-i*x => 0 */
                }
            assert(dif <= 0);
            assert((*loper) == top);
            assert(Ltail == NULL);
            }
        else  /* LtermI != NULL && RtermI == NULL */
            {
            adr[1] = Rterm; /*{?} i*x+-i*x+a => a */
            adr[2] = L;
            (*loper)->RIGHT = opb((*loper)->RIGHT,"\1+\2",NULL);
            (*loper)->RIGHT->v.fl |= READY;
            return rechtertak(top);
            }
        }
    else /* LtermI == NULL */
        {
        ppsk loper = &pkn;
        int dif = 1;
        assert(LtermNNNI != NULL);
        splitProduct_number_im_rest(Rterm,&RtermN,&RtermI,&RtermNNNI);
        while(  RtermNNNI != NULL
             && RtermI == NULL
             && (dif = vgl(LtermNNNI,RtermNNNI)) > 0
             && kop((*loper)->RIGHT) == PLUS
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
            loper = &(*loper)->RIGHT; /*{?} (b^3+c^3)+b^3+c+b^3 => c+3*b^3+c^3 */
            *loper = prive(*loper);
            rechteroperand_and_tail((*loper),&Rterm,&Rtail);
            splitProduct_number_im_rest(Rterm,&RtermN,&RtermI,&RtermNNNI);
            }
        if(RtermI != NULL)
            dif = -1; /*{?} (-i+a)+-i => a+2*-i */
        if(dif == 0)
            {
            if(RtermN)
                {
                adr[1] = RtermNNNI;
                adr[2] = RtermN;
                if(LtermN == NULL)
                    /*{?} a+n*a => a+a*n */
                    conc[0] = "(1+\2)*\1";
                /* (1+n)*a */
                else
                    {
                    /*{?} n*a+m*a => a*m+a*n */ /*{?} 7*a+9*a => 16*a */
                    adr[3] = LtermN;
                    conc[0] = "(\3+\2)*\1";
                    /* (n+m)*a */
                    }
                }
            else
                {
                adr[1] = LtermNNNI;
                if(LtermN != NULL)
                    {
                    /* m*a+a */
                    adr[2] = LtermN;
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
                adr[4] = Rtail;
                conc[1] = "+\4";
                }
            (*loper)->RIGHT = vopb((*loper)->RIGHT,conc);
            if(loper != &pkn)
                {
                (*loper)->v.fl &= ~READY;
                /*{?} (b^3+c^3)+b^3+c+b^3 => c+3*b^3+c^3 */
                /* This would evaluate to c+(1+2)*b^3+c^3 if READY flag wasn't turned off */
                /* Correct evaluation: c+3*b^3+c^3 */
                *loper = eval(*loper);
                }
            return rechtertak(top);
            }
        else if(dif > 0)  /*{?} b+a => a+b */
            {
            adr[1] = Rterm;
            adr[2] = L;
            (*loper)->RIGHT = opb((*loper)->RIGHT,"\1+\2",NULL);
            (*loper)->RIGHT->v.fl |= READY;
            return rechtertak(top);
            }
        else if((*loper) != top) /* b + a + c */
            {
            adr[1] = L;
            adr[2] = (*loper)->RIGHT;
            (*loper)->RIGHT = opb((*loper)->RIGHT,"\1+\2",NULL);
            (*loper)->RIGHT = eval((*loper)->RIGHT);
            return rechtertak(top);          /* (1+a+b+c)^30+1 */
            }
        assert(Ltail == NULL);
        }
    return pkn;
    }

static psk substmaal(psk pkn)
    {
    static const char *conc[] = {NULL,NULL,NULL,NULL};
    psk rkn,lkn;
    psk rvar,lvar;
    psk temp,llknoop,rlknoop;
    int knverschil;
    rkn = rechteroperand(pkn);

    if(is_op(rkn))
        rvar = NULL; /* (f.e)*(y.s) */
    else
        {
        if(IS_EEN(rkn))
            {
            return linkertak(pkn); /*{?} (a=7)&!(a*1) => 7 */
            }
        else if(RAT_NUL(rkn))/*{?} -1*140/1000 => -7/50 */
            {
            wis(pkn); /*{?} x*0 => 0 */
            return copievan(&nulk);
            }
        rvar = rkn; /*{?} a*a => a^2 */
        }

    lkn = pkn->LEFT;
    if(!is_op(lkn))
        {
        if(RAT_NUL(lkn)) /*{?} -1*140/1000 => -7/50 */
            {
            wis(pkn); /*{?} 0*x => 0 */
            return copievan(&nulk);
            }
        lvar = lkn;

        if(IS_EEN(lkn))
            {
            return rechtertak(pkn); /*{?} 1*-1 => -1 */
            }
        else if(RATIONAAL_COMP(lkn) && rvar)
            {
            if(RATIONAAL_COMP(rkn))
                {
                if(rkn == lkn)
                    lvar = (pkn->LEFT = prive(lkn)); /*{?} 1/10*1/10 => 1/100 */
                conc[0] = hekje6;
                adr[6] = _qmaal(rvar,lvar);
                if(rkn == pkn->RIGHT)
                    conc[1] = NULL; /*{?} -1*140/1000 => -7/50 */
                else
                    {
                    adr[1] = pkn->RIGHT->RIGHT; /*{?} -1*1/4*e^(2*i*x) => -1/4*e^(2*i*x) */
                    conc[1] = "*\1";
                    }
                pkn = vopb(pkn,conc);
                wis(adr[6]);
                return pkn;
                }
            else
                {
                if(PLOBJ(rkn) == IM && RAT_NEG_COMP(lkn))
                    {
                    conc[0] = "(\2*\3)";
                    adr[2] = _qmaalmineen(lkn);
                    adr[3] = _qmaalmineen(rkn);
                    if(rkn == pkn->RIGHT)
                        conc[1] = NULL; /*{?} -1*i => -i */
                    else
                        {
                        adr[1] = pkn->RIGHT->RIGHT; /*{?} -3*i*x => 3*-i*x */
                        conc[1] = "*\1";
                        }
                    pkn = vopb(pkn,conc);
                    wis(adr[2]);
                    wis(adr[3]);
                    return pkn;
                    }
                }
            }
        }


    rlknoop = kop(rkn) == EXP ? rkn->LEFT : rkn; /*{?} (f.e)*(y.s) => (f.e)*(y.s) */
    llknoop = kop(lkn) == EXP ? lkn->LEFT : lkn;
    if((knverschil = equal(llknoop,rlknoop)) == 0)
        {
        /* a^n*a^m */
        if(rlknoop != rkn)
            {
            adr[1] = rlknoop; /*{?} e^(i*x)*e^(-i*x) => 1 */
            adr[2] = rkn->RIGHT;
            if(llknoop == lkn)
                {
                conc[0] = "\1^(1+\2)"; /*{?} a*a^n => a^(1+n) */
                }/* a^(1+n) */
            else
                {
                /* a^n*a^m */
                adr[3] = lkn->RIGHT; /*{?} e^(i*x)*e^(-i*x) => 1 */
                conc[0] = "\1^(\3+\2)";
                /* a^(n+m) */
                }
            }
        else
            {
            if(llknoop != lkn)
                {
                adr[1] = llknoop;     /*{?} a^m*a => a^(1+m) */
                adr[2] = lkn->RIGHT;
                conc[0] = "\1^(1+\2)";
                /* a^(m+1) */
                }
            else
                {
                /*{?} a*a => a^2 */
                adr[1] = llknoop; /*{?} i*i*(-1/2*e^(i*a)+1/2*e^(-i*a))*(-1/2*e^(i*b)+1/2*e^(-i*b)) => -1*(-1/2*e^(i*a)+1/2*e^(-i*a))*(-1/2*e^(i*b)+1/2*e^(-i*b)) */
                conc[0] = "\1^2";
                /* a^2 */
                }
            }
        if(rkn != (temp = pkn->RIGHT))
            {
            adr[4] = temp->RIGHT; /*{?} i*i*(-1/2*e^(i*a)+1/2*e^(-i*a))*(-1/2*e^(i*b)+1/2*e^(-i*b)) => -1*(-1/2*e^(i*a)+1/2*e^(-i*a))*(-1/2*e^(i*b)+1/2*e^(-i*b)) */
            conc[1] = "*\4";
            }
        else
            conc[1] = NULL; /*{?} e^(i*x)*e^(-i*x) => 1 */
        return vopb(pkn,(const char **)conc);
        }
    else
        {
        int graad;
        graad = getal_graad(rlknoop) - getal_graad(llknoop); /*{?} (f.e)*(y.s) => (f.e)*(y.s) */
        if(graad > 0
        || (graad == 0 && (knverschil > 0)))
            {
            /* b^n*a^m */
            /* l^n*a^m */
            if((temp = pkn->RIGHT) == rkn)
                {
                pkn->RIGHT = lkn; /*{?} x*2 => 2*x */
                pkn->LEFT = rkn;
                pkn->v.fl &= ~READY;
                }
            else
                {
                adr[1] = lkn; /*{?} i*2*x => 2*i*x */
                adr[2] = temp->LEFT;
                adr[3] = temp->RIGHT;
                pkn = opb(pkn,"\2*\1*\3",NULL);
                }
            return pkn;
            /* a^m*b^n */
            /* a^m*l^n */
            }
        else if(PLOBJ(rlknoop) == IM)
            {
            if(PLOBJ(llknoop) == IM) /*{?} -1*i^1/3 => -i^5/3 */
                {
                /*{?} i^n*-i^m => i^(-1*m+n) */
                if(rlknoop != rkn)
                    {
                    adr[1] = llknoop;
                    adr[2] = rkn->RIGHT;
                    if(llknoop == lkn)
                        /*{?} i*-i^n => i^(1+-1*n) */
                        conc[0] = "\1^(1+-1*\2)";
                        /* i^(1-n) */
                    else
                        {
                        adr[3] = lkn->RIGHT; /*{?} i^n*-i^m => i^(-1*m+n) */
                        conc[0] = "\1^(\3+-1*\2)";
                        /* i^(n-m) */
                        }
                    }
                else
                    {
                    if(llknoop != lkn)
                        {
                        /*{?} i^m*-i => i^(-1+m) */
                        adr[1] = llknoop;
                        adr[2] = lkn->RIGHT;
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
            else if(  RAT_NEG_COMP(llknoop)
                /* -n*i^m -> n*-i^(2+m) */
                /*{?} -7*i^9 => 7*i */ /*{!} 7*-i^11 */
                /* -n*-i^m -> n*i^(2+m) */
                /*{?} -7*-i^9 => 7*-i */ /*{!}-> 7*i^11 */
                   && rlknoop != rkn
                   && llknoop == lkn
                   )
                { /*{?} -1*i^1/3 => -i^5/3 */
                adr[1] = llknoop;
                adr[2] = rkn->LEFT;
                adr[3] = rkn->RIGHT;
                adr[4] = &tweek;
                conc[0] = "(-1*\1)*\2^(\3+\4)";
                }
            else
                return pkn; /*{?} 2*i*x => 2*i*x */
            if(rkn != (temp = pkn->RIGHT))
                {
                adr[4] = temp->RIGHT; /*{?} i^n*-i^m*z => i^(-1*m+n)*z */
                conc[1] = "*\4";
                }
            else
                conc[1] = NULL; /*{?} -1*i^1/3 => -i^5/3 */ /*{?} i^n*-i^m => i^(-1*m+n) */
            return vopb(pkn,(const char **)conc);
            }
        else
            return pkn; /*{?} (f.e)*(y.s) => (f.e)*(y.s) */
        }
    }

static int rechtsbrengen(psk kn)
    {
    /* (a*b*c*d)*a*b*c*d -> a*b*c*d*a*b*c*d */
    psk lknoop;
    int gedaan;
    gedaan = FALSE;
    for(;kop(lknoop = kn->LEFT) == kop(kn);)
        {
        lknoop = prive(lknoop);
        lknoop->v.fl &= ~READY;
        kn->LEFT = lknoop->LEFT;
        lknoop->LEFT = lknoop->RIGHT;
        lknoop->RIGHT = kn->RIGHT;
        kn->v.fl &= ~READY;
        kn->RIGHT = lknoop;
        kn = lknoop;
        gedaan = TRUE;
        }
    return gedaan;
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

repol = NIL
loper = (a * x) * b * c
lloper = a * x

  2*
  / \
 a
 repol = a*NIL


             1*
             / \
            x  3*
               / \
              b   c
*/
static int vglplus(psk kn1,psk kn2)
    {
    if(RATIONAAL_COMP(kn2))
        {
        if(RATIONAAL_COMP(kn1))
            return 0;
        return 1; /* switch places */
        }
    else if(RATIONAAL_COMP(kn1))
        return -1;
    else
        {
        psk N1;
        psk I1;
        psk NNNI1;
        psk N2;
        psk I2;
        psk NNNI2;
        splitProduct_number_im_rest(kn1,&N1,&I1,&NNNI1);
        splitProduct_number_im_rest(kn2,&N2,&I2,&NNNI2);
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
            diff = equal(NNNI1,NNNI2);
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

static int vglmaal(psk kn1,psk kn2)
    {
    int diff;
    if(kop(kn1) == EXP)
        kn1 = kn1->LEFT;
    if(kop(kn2) == EXP)
        kn2 = kn2->LEFT;

    diff = getal_graad(kn2);
    if(diff != 4)
        diff -= getal_graad(kn1);
    else
        {
        diff = -getal_graad(kn1);
        if(!diff)
            return 0; /* two numbers */
        }
    if(diff > 0)
        return 1; /* switch places */
    else if(diff == 0)
        {
        diff = equal(kn1,kn2);
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
            (psk pkn
            ,int (*comp)(psk,psk)
            ,psk (*combine)(psk)
#if EXPAND
            ,psk (*expand)(psk,int*)
#endif
            )
    {
    psk lhead,ltail,rhead,rtail;
    psk Repol = &nilk; /* Will contain all evaluated nodes in inverse order.*/
    psk tmp;
    for(;;)
        {/* traverse from left to right to evaluate left side branches */
#if EXPAND
        Boolean ok;
#endif
        pkn = prive(pkn);
        assert(!shared(pkn));
        pkn->v.fl |= READY;
#if EXPAND
        do
            {
            pkn->LEFT = eval(pkn->LEFT);
            pkn->LEFT = expand(pkn->LEFT,&ok);
            }
            while(ok);
#else
        pkn->LEFT = eval(pkn->LEFT);
#endif
        tmp = pkn->RIGHT;
        if(tmp->v.fl & READY)
            {
            break;
            }
        if(  !is_op(tmp)
          || kop(pkn) != kop(tmp)
          )
            {
#if EXPAND
            do
                {
                tmp = eval(tmp);
                tmp = expand(tmp,&ok);
                }
                while(ok);
            pkn->RIGHT = tmp;
#else
            pkn->RIGHT = eval(tmp);
#endif
            break;
            }
        pkn->RIGHT = Repol;
        Repol = pkn;
        pkn = tmp;
        }
    for(;;)
        { /* From right to left, prepend sorted elements to result */
        psk repol = &nilk; /*Will contain branches in inverse sorted order*/
        psk L = linkeroperand_and_tail(pkn,&lhead,&ltail);
        psk R = rechteroperand_and_tail(pkn,&rhead,&rtail);
        for(;;)
            { /* From right to left, prepend smallest of lhs and rhs
                 to repol
              */
            assert(rhead->v.fl & READY);
            assert((L == lhead && ltail == NULL) || L->RIGHT == ltail);
            assert((L == lhead && ltail == NULL) || L->LEFT == lhead);
            assert(pkn->LEFT == L);
            assert((R == rhead && rtail == NULL) || R->RIGHT == rtail);
            assert((R == rhead && rtail == NULL) || R->LEFT == rhead);
            assert(pkn->RIGHT == R);
            assert(L->v.fl & READY);
            assert(pkn->RIGHT->v.fl & READY);
            if(comp(lhead,rhead) <= 0) /* a * b */
                {
                if(ltail == NULL)   /* a * (b*c) */
                    {
                    assert(pkn->RIGHT->v.fl & READY);
                    break;
                    }
                else                /* (a*d) * (b*c) */
                    {
                    L = prive(L);
                    assert(!shared(L));
                    if(ltail != L->RIGHT)
                        {
                        wis(L->RIGHT); /* rare, set REFCOUNTSTRESSTEST 1 */
                        ltail = zelfde_als_w(ltail);
                        }
                    L->RIGHT = repol;
                    repol = L;
                    assert(!shared(pkn));
                    pkn = prive(pkn);
                    assert(!shared(pkn));
                    pkn->LEFT = ltail;
                    L = linkeroperand_and_tail(pkn,&lhead,&ltail);
                    }               /* repol := a*repol */
                /* d * (b*c) */
                }
            else /* Wrong order */
                {
                pkn = prive(pkn);
                assert(!shared(pkn));
                assert(L->v.fl & READY);
                if(rtail == NULL) /* (b*c) * a */
                    {
                    pkn->LEFT = R;
                    assert(L->v.fl & READY);
                    pkn->RIGHT = L;
                    break;
                    }             /* a * (b*c) */
                else          /* (b*c) * (a*d)         c * (b*a) */
                    {
                    R = prive(R);
                    assert(!shared(R));
                    if(R->RIGHT != rtail)
                        {
                        wis(R->RIGHT); /* rare, set REFCOUNTSTRESSTEST 1 */
                        rtail = zelfde_als_w(rtail);
                        }
                    R->RIGHT = repol;
                    repol = R;
                    assert(!shared(pkn));
                    pkn->RIGHT = rtail;
                    R = rechteroperand_and_tail(pkn,&rhead,&rtail);
                    }         /* repol :=  a*repol    repol := b*repol */
                /* (b*c) * d */
                }
            }
        for(;;)
            { /*Combine combinable elements and prepend to result*/
            pkn->v.fl |= READY;
            pkn = combine(pkn);
            if(!(pkn->v.fl & READY))
                { /*This may results in recursive call to merge
                    if the result of the evaluation is not in same
                    sorting position as unevaluated expression. */
                assert(!shared(pkn));
                pkn = eval(pkn);
                }
            if(repol != &nilk)
                {
                psk n = repol->RIGHT;
                assert(!shared(repol));
                repol->RIGHT = pkn;
                pkn = repol;
                repol = n;
                }
            else
                break;
            }
        if(Repol == &nilk)
            break;
        tmp = Repol->RIGHT;
        assert(!shared(Repol));
        Repol->RIGHT = pkn;
        pkn = Repol;
        assert(!shared(pkn));
        pkn->v.fl |= READY;
        Repol = tmp;
        }
    return pkn;
    }

static psk substlog(psk pkn)
    {
    static const char *conc[] = {NULL,NULL,NULL,NULL};
    psk lknoop = pkn->LEFT,rknoop = pkn->RIGHT;
    if(!equal(lknoop,rknoop))
        {
        wis(pkn);
        return copievan(&eenk);
        }
    else if(is_op(rknoop))  /*{?} x\L(2+y) => x\L(2+y) */
        {
        int ok;
        return tryq(pkn,f5,&ok); /*{?} x\L(a*x^n*z) => n+x\L(a*z) */
        }
    else if(IS_EEN(rknoop))  /*{?} x\L1 => 0 */ /*{!} 0 */
        {
        wis(pkn);
        return copievan(&nulk);
        }
    else if(RAT_NUL(rknoop)) /*{?} z\L0 => z\L0 */
        return pkn;
    else if(is_op(lknoop)) /*{?} (x+y)\Lz => (x+y)\Lz */
        return pkn;
    else if(RAT_NUL(lknoop))   /*{?} 0\Lx => 0\Lx */
        return pkn;
    else if(IS_EEN(lknoop))   /*{?} 1\Lx => 1\Lx */
        return pkn;
    else if(RAT_NEG(lknoop))  /*{?} -7\Lx => -7\Lx */
        return pkn;
    else if(RATIONAAL_COMP(rknoop))  /*{?} x\L7 => x\L7 */
        {
        if(_qvergelijk(rknoop,&nulk) & MINUS)
            {
            /* (nL-m = i*pi/eLn+nLm)  */ /*{?} 7\L-9 => 1+7\L9/7+i*pi*e\L7^-1 */ /*{!} i*pi/e\L7+7\L9)  */
            adr[1] = lknoop;
            adr[2] = rknoop;
            return opb(pkn,"(i*pi*e\016\1^-1+\1\016(-1*\2))",NULL);
            }
        else if(RATIONAAL_COMP(lknoop)) /* m\Ln */ /*{?} 7\L9 => 1+7\L9/7 */
            {
            if(_qvergelijk(lknoop,&eenk) & MINUS)
                {
                                        /* (1/n)Lm = -1*nLm */ /*{?} 1/7\L9 => -1*(1+7\L9/7) */ /*{!} -1*nLm */
                adr[1] = rknoop;
                conc[0] = "(-1*";
                conc[1] = hekje6;
                adr[6] = _q_qdeel(&eenk,lknoop);
                conc[2] = "\016\1)";
                pkn = vopb(pkn,conc);
                wis(adr[6]);
                return pkn;
                }
            else if(_qvergelijk(lknoop,rknoop) & MINUS)
                {
                                    /* nL(n+m) = 1+nL((n+m)/n) */ /*{?} 7\L(7+9) => 1+7\L16/7 */ /*{!} 1+nL((n+m)/n) */
                conc[0] = "(1+\1\016";
                assert(lknoop != rknoop);
                adr[1] = lknoop;
                conc[1] = hekje6;
                adr[6] = _q_qdeel(rknoop,lknoop);
                conc[2] = ")";
                pkn = vopb(pkn,conc);
                wis(adr[6]);
                return pkn;
                }
            else if(_qvergelijk(rknoop,&eenk) & MINUS)
                {
                                    /* nL(1/m) = -1+nL(n/m) */ /*{?} 7\L1/9 => -2+7\L49/9 */ /*{!} -1+nL(n/m) */
                conc[0] = "(-1+\1\016";
                assert(lknoop != rknoop);
                adr[1] = lknoop;
                conc[1] = hekje6;
                adr[6] = _qmaal(rknoop,lknoop);
                conc[2] = ")";
                pkn = vopb(pkn,conc);
                wis(adr[6]);
                return pkn;
                }
            }
        }
    return pkn;
    }

static psk substdiff(psk pkn)
    {
    psk lknoop,rknoop;
    lknoop = pkn->LEFT;
    rknoop = pkn->RIGHT;
    if(is_constant(lknoop) || is_constant(rknoop))
        {
        wis(pkn);
        pkn = copievan(&nulk);
        }
    else if(!equal(lknoop,rknoop))
        {
        wis(pkn);
        pkn = copievan(&eenk);
        }
    else if(  !is_op(rknoop)
           && is_afhankelyk_van(lknoop,rknoop)
           )
        {
        ;
        }
    else if(!is_op(rknoop))
        {
        wis(pkn);
        pkn = copievan(&nulk);
        }
    else if(kop(rknoop) == PLUS)
        {
        adr[1] = lknoop;
        adr[2] = rknoop->LEFT;
        adr[3] = rknoop->RIGHT;
        pkn = opb(pkn,"((\1\017\2)+(\1\017\3))",NULL);
        }
    else if(is_op(rknoop))
        {
        adr[2] = rknoop->LEFT;
        adr[1] = lknoop;
        adr[3] = rknoop->RIGHT;
        switch(kop(rknoop))
            {
            case TIMES :
                pkn = opb(pkn,"(\001\017\2*\3+\2*\001\017\3)",NULL);
                break;
            case EXP:
                pkn = opb(pkn,
                    "(\2^(-1+\3)*\3*\001\017\2+\2^\3*e\016\2*\001\017\3)",NULL);
                break;
            case LOG :
                pkn = opb(pkn,
                    "(\2^-1*e\016\2^-2*e\016\3*\001\017\2+\3^-1*e\016\2^-1*\001\017\3)",NULL);
                break;
            }
        }
    return pkn;
    }


#if JMP /* Often no need for polling in multithreaded apps.*/
#include <windows.h>
#include <dde.h>
static void PeekMsg(void)
    {
    static MSG msg;
    while(PeekMessage(&msg,NULL,WM_PAINT,WM_DDE_LAST,PM_REMOVE))
        {
        if(msg.message == WM_QUIT)
            {
            PostThreadMessage(GetCurrentThreadId(), WM_QUIT,0,0L);
            longjmp(jumper,1);
            }
        TranslateMessage(&msg);        /* Translates virtual key codes */
        DispatchMessage(&msg);        /* Dispatches message to window*/
        }
    }
#endif

/*
Iterative handling of WHITE operator in evalueer.
Can now handle very deep structures without stack overflow
*/

static psk handleLUCHT(psk pkn)
    { /* assumption: (kop(*pkn) == WHITE) && !((*pkn)->v.fl & READY) */
    static psk hulp;
    psk luchtknoop;
    psk next;
    ppsk pluchtknoop = &pkn;
    ppsk prevpluchtknoop = NULL;
    for(;;)
        {
        luchtknoop = *pluchtknoop;
        luchtknoop->LEFT = eval(luchtknoop->LEFT);
        if  (  !is_op(hulp=luchtknoop->LEFT)
            && !(hulp->u.obj)
            && !HAS_UNOPS(hulp)
            )
            {
            *pluchtknoop = rechtertak(luchtknoop);
            }
        else
            {
            prevpluchtknoop = pluchtknoop;
            pluchtknoop = &(luchtknoop->RIGHT);
            }
        if(kop(luchtknoop = *pluchtknoop) == WHITE && !(luchtknoop->v.fl & READY))
            {
            if(shared(*pluchtknoop))
                *pluchtknoop = copyop(*pluchtknoop);
            }
        else
            {
            *pluchtknoop = eval(*pluchtknoop);
            if(  prevpluchtknoop
              && !is_op(luchtknoop = *pluchtknoop)
              && !((luchtknoop)->u.obj)
              && !HAS_UNOPS(luchtknoop)
              )
                *prevpluchtknoop = linkertak(*prevpluchtknoop);
            break;
            }
        }

    luchtknoop = pkn;
    while(kop(luchtknoop) == WHITE)
        {
        next = luchtknoop->RIGHT;
        rechtsbrengen(luchtknoop);
        if(next->v.fl & READY)
            break;
        luchtknoop = next;
        luchtknoop->v.fl |= READY;
        }
    return pkn;
    }
/*
Iterative handling of COMMA operator in evalueer.
Can now handle very deep structures without stack overflow
*/
static psk handleKOMMA(psk pkn)
    { /* assumption: (kop(*pkn) == COMMA) && !((*pkn)->v.fl & READY) */
    psk kommaknoop = pkn;
    psk next;
    ppsk pkommaknoop;
    while(kop(kommaknoop->RIGHT) == COMMA && !(kommaknoop->RIGHT->v.fl & READY))
        {
        kommaknoop->LEFT = eval(kommaknoop->LEFT);
        pkommaknoop = &(kommaknoop->RIGHT);
        kommaknoop = kommaknoop->RIGHT;
        if(shared(kommaknoop))
            {
            *pkommaknoop = kommaknoop = copyop(kommaknoop);
            }
        }
    kommaknoop->LEFT = eval(kommaknoop->LEFT);
    kommaknoop->RIGHT = eval(kommaknoop->RIGHT);
    kommaknoop = pkn;
    while(kop(kommaknoop) == COMMA)
        {
        next = kommaknoop->RIGHT;
        rechtsbrengen(kommaknoop);
        if(next->v.fl & READY)
            break;
        kommaknoop = next;
        kommaknoop->v.fl |= READY;
        }
    return pkn;
    }

static psk evalvar(psk pkn)
    {
    psk loc_adr = Naamwoord_w(pkn, pkn->v.fl & DOUBLY_INDIRECT);
    if(loc_adr != NULL)
        {
        wis(pkn);
        pkn = loc_adr;
        }
    else
        {
        DBGSRC(printf("evalvar  (");result(pkn);printf("\n");)
        if(shared(pkn))
            {
            /*You can get here if a !variable is unitialized*/
            dec_refcount(pkn);
            pkn = icopievan(pkn);
            }
        assert(!shared(pkn));
        (pkn)->v.fl |= READY;
        (pkn)->v.fl ^= SUCCESS;
        }
    return pkn;
    }

static void privatized(psk pkn,psk plkn)
    {
    *plkn = *pkn;
    if(shared(plkn))
        {
        dec_refcount(pkn);
        plkn->LEFT = zelfde_als_w(plkn->LEFT);
        plkn->RIGHT = zelfde_als_w(plkn->RIGHT);
        }
    else
        pskfree(pkn);
    }

static psk __rechtertak(psk pkn)
    {
    psk ret;
    int success = pkn->v.fl & SUCCESS;
    if(shared(pkn))
        {
        ret = zelfde_als_w(pkn->RIGHT);
        dec_refcount(pkn);
        }
    else
        {
        ret = pkn->RIGHT;
        wis(pkn->LEFT);
        pskfree(pkn);
        }
    if(!success)
        {
        ret = prive(ret);
        ret->v.fl ^= SUCCESS;
        }
    return ret;
    }


static psk eval(psk pkn)
    {
    ASTACK
    /*
    Notice the low number of local variables on the stack. This ensures maximal
    utilisation of stack-depth for recursion.
    */
    DBGSRC(Printf("evaluate :");result(pkn);Printf("\n");)
    while(!(pkn->v.fl & READY))
        {
        if(is_op(pkn))
            {
            sk lkn = *pkn;
            /* The operators MATCH, AND and OR are treated in another way than
            the other operators. These three operators are the only 'volatile'
            operators: they cannot occur in a fully evaluated tree. For that reason
            there is no need to allocate space for an evaluated version of such
            operators on the stack. Instead the local variable lkn is used.
            */
            switch(kop(pkn))
                {
                case MATCH :
                    {
                    privatized(pkn,&lkn);
                    lkn.LEFT = eval(lkn.LEFT);
                    if(isSUCCESSorFENCE(lkn.LEFT))
                        /*
                        `~a:?b will assign `~a to b
                        */
                        {
                        DBGSRC(Printf("before match:");result(&lkn);\
                            Printf("\n");)
#if STRINGMATCH_CAN_BE_NEGATED
                        if(lkn.flgs & ATOM) /* should other flags be
                                            excluded, including ~ ?*/
#else
                        if((lkn.flgs & ATOM) && !ONTKENNING(lkn.flgs,ATOM))
#endif
                            {
#if CUTOFFSUGGEST
                            if(!is_op(lkn.LEFT) && stringmatch(0,"V",SPOBJ(lkn.LEFT),NULL,lkn.RIGHT, lkn.LEFT,0,strlen((char*)POBJ(lkn.LEFT)),NULL,0) & TRUE)
#else
                            if(!is_op(lkn.LEFT) && stringmatch(0,"V",POBJ(lkn.LEFT),NULL,lkn.RIGHT, lkn.LEFT,0,strlen((char*)POBJ(lkn.LEFT))) & TRUE)
#endif
                                pkn = _linkertak(&lkn); /* ~@(a:a) is now treated like ~(a:a)*/
                            else
                                {
                                if(is_op(lkn.LEFT))
                                    {
#if !defined NO_EXIT_ON_NON_SEVERE_ERRORS
                                    errorprintf("Error in stringmatch: left operand is not atomic ");writeError(&lkn);
                                    exit(117);
#endif
                                    }
                                pkn = _flinkertak(&lkn);/* ~@(a:b) is now treated like ~(a:b)*/
                                }
                            }
                        else
                            {
                            if(match(0,lkn.LEFT,lkn.RIGHT,NULL,0,lkn.LEFT,5555) & TRUE)
                                pkn = _linkertak(&lkn);
                            else
                                pkn = _flinkertak(&lkn);
                            }
                        }
                    else
                        {
                        pkn = _linkertak(&lkn);
                        }
                    DBGSRC(Printf("after match:");result(pkn);Printf("\n");\
                        if((pkn)->v.fl & SUCCESS) Printf(" SUCCESS\n");\
                        else Printf(" FENCE\n");)
                    break;
                    }
                    /* The operators AND and OR are tail-recursion optimised. */
                case AND :
                    {
                    privatized(pkn,&lkn);
                    lkn.LEFT = eval(lkn.LEFT);
                    if(isSUCCESSorFENCE(lkn.LEFT))
                        pkn = _rechtertak(&lkn);/* TRUE or FENCE */
                    else
                        pkn = _linkertak(&lkn);/* FAIL */
                    break;
                    }
                case OR :
                    {
                    privatized(pkn,&lkn);
                    lkn.LEFT = eval(lkn.LEFT);
                    if(isSUCCESSorFENCE(lkn.LEFT))
                        pkn = _fencelinkertak(&lkn);/* FENCE or TRUE */
                    else
                        pkn = _rechtertak(&lkn);/* FAIL */
                    break;
                    }
                    /* Operators that can occur in evaluated expressions: */
                case EQUALS :
                    if(ISBUILTIN((objectknoop *)pkn))
                        {
                        pkn->v.fl |= READY;
                        break;
                        }

                    if(  !is_op(pkn->LEFT)
                      && !pkn->LEFT->u.obj
                      && (pkn->v.fl & INDIRECT)
                      && !(pkn->v.fl & DOUBLY_INDIRECT)
                      )
                        {
                        static int fl;
                        fl = pkn->v.fl & (UNOPS & ~INDIRECT);
                        pkn = __rechtertak(pkn);
                        if(fl)
                            {
                            pkn = prive(pkn);
                            pkn->v.fl |= fl; /* {?} <>#@`/%?!(=b) => /#<>%@?`b */
                            }
                        break;

                        /*                    pkn = pkn->RIGHT;*/
                        }
                    else
                        {
                        if(shared(pkn))
                            {
                            pkn = copyop(pkn);
                            }
                        pkn->v.fl |= READY;
                        pkn->LEFT = eval(pkn->LEFT);
                        if(is_op(pkn->LEFT))
                            {
                            if(update(pkn->LEFT,pkn->RIGHT))
                                pkn = linkertak(pkn);
                            else
                                pkn = flinkertak(pkn);
                            }
                        else if(pkn->LEFT->u.obj)
                            {
                            insert(pkn->LEFT,pkn->RIGHT);
                            pkn = linkertak(pkn);
                            }
                        else if(pkn->v.fl & INDIRECT)
                            /* !(=a) -> a */
                            {
                            pkn = evalvar(pkn);
                            }
                        }
                    break;
                case DOT :
                    {
                    if(shared(pkn))
                        {
                        pkn = copyop(pkn);
                        }
                    pkn->v.fl |= READY;
                    pkn->LEFT = eval(pkn->LEFT);
                    pkn->RIGHT = eval(pkn->RIGHT);
                    if(pkn->v.fl & INDIRECT)
                        {
                        pkn = evalvar(pkn);
                        }
                    break;
                    }
                case COMMA :
                    if(shared(pkn))
                        {
                        pkn = copyop(pkn);
                        }
                    pkn->v.fl |= READY;
                    pkn = handleKOMMA(pkn);/* do not recurse, iterate! */
                    if(lkn.v.fl & INDIRECT)
                        {
                        pkn = evalvar(pkn);
                        }
                    break;
                case WHITE :
                    if(shared(pkn))
                        {
                        pkn = copyop(pkn);
                        }
                    pkn->v.fl |= READY;
                    pkn = handleLUCHT(pkn);/* do not recurse, iterate! */
                    if(lkn.v.fl & INDIRECT)
                        {
                        pkn = evalvar(pkn);
                        }
                    break;
                case PLUS :
                    if(shared(pkn))
                        {
                        pkn = copyop(pkn);
                        }
                    pkn = merge(pkn,vglplus,plus_samenvoegen_of_sorteren
#if EXPAND
                        ,expandProduct
#endif
                        );
                    if(lkn.v.fl & INDIRECT)
                        {
                        pkn = evalvar(pkn);
                        }
                    break;
                case TIMES :
                    if(shared(pkn))
                        {
                        pkn = copyop(pkn);
                        }
                    pkn->v.fl |= READY;
                    {
                    pkn = merge(pkn,vglmaal,substmaal
#if EXPAND
                        ,expandDummy
#endif
                        );
                    }
                    if(lkn.v.fl & INDIRECT)
                        {
                        pkn = evalvar(pkn);                             /* {?} a=7 & !(a*1) */
                        }
                    break;
                case EXP :
                    if(shared(pkn))
                        {
                        pkn = copyop(pkn);
                        }
                    pkn->v.fl |= READY;
                    if(  evalueer((pkn->LEFT)) == TRUE
                      && evalueer((pkn->RIGHT)) == TRUE
                      )
                        {
                        pkn = stapelmacht(pkn);
                        }
                    else
                        pkn->v.fl ^= SUCCESS;
                    if(lkn.v.fl & INDIRECT)
                        {
                        pkn = evalvar(pkn);
                        }
                    break;
                case LOG :
                    if(shared(pkn))
                        {
                        pkn = copyop(pkn);
                        }
                    pkn->v.fl |= READY;
                    if(  evalueer((pkn->LEFT)) == TRUE
                      && evalueer((pkn->RIGHT)) == TRUE
                      )
                        {
                        pkn = substlog(pkn);
                        }
                    else
                        {
                        pkn->v.fl ^= SUCCESS;
                        }
                    if(lkn.v.fl & INDIRECT)
                        {
                        pkn = evalvar(pkn);
                        }
                    break;
                case DIF :
                    if(shared(pkn))
                        {
                        pkn = copyop(pkn);
                        }
                    pkn->v.fl |= READY;
                    if(  !(lkn.v.fl & INDIRECT)
                      && evalueer((pkn->LEFT)) == TRUE
                      && evalueer((pkn->RIGHT)) == TRUE
                      )
                        {
                        pkn = substdiff(pkn);
                        break;
                        }
                    pkn->v.fl ^= SUCCESS;
                    break;
                case FUN :
                case FUU :
                    if(shared(pkn))
                        {
                        pkn = copyop(pkn);
                        }
                    pkn->v.fl |= READY;
                    pkn->LEFT = eval(pkn->LEFT);
                    if(kop(pkn) == FUN)
                        {
                        pkn->RIGHT = eval(pkn->RIGHT);
                        }
                    pkn = functies(pkn);
                    if(lkn.v.fl & INDIRECT)
                        {
                        pkn = evalvar(pkn);
                        }
                    break;
                case UNDERSCORE :
                    if(shared(pkn))
                        {
                        pkn = copyop(pkn);
                        }
                    pkn->v.fl |= READY;
                    if(dummy_op == EQUALS)
                        {
                        psk old = pkn;
                        pkn = (psk)bmalloc(__LINE__,sizeof(objectknoop));
#ifdef BUILTIN
                        ((typedObjectknoop*)(pkn))->u.Int = 0;
#else
                        ((typedObjectknoop*)(pkn))->refcount = 0;
                        UNSETCREATEDWITHNEW((typedObjectknoop*)pkn);
                        UNSETBUILTIN((typedObjectknoop*)pkn);
#endif
                        pkn->LEFT = subboomcopie(old->LEFT);
                        old->RIGHT = Head(old->RIGHT);
                        pkn->RIGHT = subboomcopie(old->RIGHT);
                        wis(old);
                        }
                    pkn->v.fl &= (~OPERATOR & ~READY);
                    pkn->ops |= dummy_op;
                    pkn->v.fl |= SUCCESS;
                    if(lkn.v.fl & INDIRECT)
                        {/* (a=b=127)&(.):(_)&!(a_b) */
                        pkn = evalvar(pkn);
                        }
                    break;
                }
            }
        else
            {
            /* An unevaluated leaf can only be an atom with ! or !!,
            so we don't need to test for this condition.*/
            pkn = evalvar(pkn);
            /* After evaluation of a variable, the loop continues.
            Together with how & and | (AND and OR) are treated, this ensures that
            a loop can run indefinitely, without using stack space. */
            }
        }
#if JMP
    PeekMsg();
#endif
    ZSTACK
    return pkn;
    }

int startProc(
#if _BRACMATEMBEDDED
    startStruct * init
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
    for(tel = 0;tel<256;variabelen[tel++] = NULL)
        ;
    if(!init_ruimte())
        return 0;
    init_opcode();
    global_anker = NULL;
    global_fpi = stdin;
    global_fpo = stdout;

    argk.flgs = READY | SUCCESS;
    argk.u.lobj = O('a','r','g');

    sjt.flgs = READY | SUCCESS;
    sjt.u.lobj = O('s','j','t');

    selfkn.flgs = READY | SUCCESS;
    selfkn.u.lobj = O('i','t','s');

    Selfkn.flgs = READY | SUCCESS;
    Selfkn.u.lobj = O('I','t','s');

    nilk.flgs = READY | SUCCESS | IDENT;
    nilk.u.lobj = 0L;

    nulk.flgs = READY | SUCCESS | IDENT | QGETAL | QNUL;
    nulk.u.lobj = 0L;
    nulk.u.obj = '0';

    eenk.u.lobj = 0L;
    eenk.u.obj = '1';
    eenk.flgs = READY | SUCCESS | IDENT | QGETAL;
    *(&(eenk.u.obj)+1) = 0;

    mintweek.u.lobj = 0L;
    mintweek.u.obj = '2';
    mintweek.flgs = READY | SUCCESS | QGETAL | MINUS;
    *(&(mintweek.u.obj)+1) = 0;

    mineenk.u.lobj = 0L;
    mineenk.u.obj = '1';
    mineenk.flgs = READY | SUCCESS | QGETAL | MINUS;
    *(&(mineenk.u.obj)+1) = 0;

    tweek.u.lobj = 0L;
    tweek.u.obj = '2';
    tweek.flgs = READY | SUCCESS | QGETAL;
    *(&(tweek.u.obj)+1) = 0;

    vierk.u.lobj = 0L;
    vierk.u.obj = '4';
    vierk.flgs = READY | SUCCESS | QGETAL;
    *(&(vierk.u.obj)+1) = 0;

    minvierk.u.lobj = 0L;
    minvierk.u.obj = '4';
    minvierk.flgs = READY | SUCCESS | QGETAL | MINUS;
    *(&(minvierk.u.obj)+1) = 0;

    m0 = opb(m0,"?*(%+%)^~/#>1*?" , NULL);
    m1 = opb(m1,"?*(%+%)*?" , NULL);
    f0 = opb(f0,"(g,k,pow"
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
                  "&!g*pow$!arg*!k)",NULL);
    f1 = opb(f1,
        "((\177g,\177h,\177i).!arg:?\177g*(%?\177h+%?\177i)*",
        "?arg&!\177g*!\177h*!arg+!\177g*!\177i*!arg)",NULL);
    f4 = opb(f4,  "l,a,b,c,e,f"
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
                 ")",NULL);
    f5 = opb(f5,
         "l,d"
         ".(d"
          "=j,g,h"
          ".!arg:(~1:?l)\016(?j*!l^?g*?h)&!g+!l\016(!j*!h)"
          ")"
          "&!arg:?l\016(?*!l^?*?)"
          "&d$!arg", NULL);

    global_anker = startboom_w(global_anker ,
        "(cat=flt,sin,tay,fct,cos,out,sgn.!arg:((?flt,(?sin,?tay)|?sin&:?tay)|?flt&:?sin:"
        "?tay)&(fct=.!arg:%?cos ?arg&!cos:((?out.?)|?out)&'(? ($out|($out.?)"
        ") ?):(=?sgn)&(!flt:!sgn&!(glf$(=~.!sin:!sgn))&!cos|) fct$!arg|)&(:!flt:!sin&mem$!tay|(:!flt&mem$:?flt"
        "|)&fct$(mem$!tay))),",

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

        "(MLencoding=N M J h m R.:?N&(R=? (charset.?N) ?|? (content.@(?:? "
        "charset ? \275 ?N)) ?)&(M=? (~<>M.(!R,?)|!R) ?)&(J=? (~<>head.(?,!M)|"
        "?&~`) ?|? (~<>head:?h.?) ?m (.!h.) ?&!m:!M)&(!arg:(@ (\277.@(?:~<>XML"
        " (? encoding ? \275 ? (\242 ?N \242|\247 ?N \247) ?|?&utf\255\270:?N)"
        ")) ?|? (~<>html.(?,!J)|?&~`) ?|? (~<>html.?) !J)|)&(!N:~|!arg:? ("
        "\241DOCTYPE.@(?:? html ?)) ?&utf\255\270|)),",
        "(nestML=a L B s e x.:?L&whl'(!arg:%?a ?arg&!a !L:?L)&!L:?arg&:?L:?B:"
        "?s&whl'(!arg:%?a ?arg&(!a:(.?e.)&(!L.) !B:?B&:?L&!e !s:?s|(!a:(?e.?,?"
        ")&!a|!a:(?e.?x)&(!s:!e ?s&(!e.!x,!L) (!B:(%?L.) ?B&)|!a)|!a) !L:?L))&"
        "!L),",
        "(toML=O d t g l \246 w \242 \247 H G S D E F I U.:?O&chr$\261\262\267"
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
        " !F$!B \277\276)|\241\333CDATA\333&d$(\274\241\333CDATA\333 !F$!B "
        "\335\335\276)|\241DOCTYPE&d$(\274\241DOCTYPE !F$!B \276)|?&d$(\274 !A"
        " t$!B \276)))|d$(g$!r)))):(=?w)&w$!arg&str$((=a L.:?L&whl'(!arg:%?a "
        "?arg&!a !L:?L)&!L)$!O)),",
        fct,
        NULL);


#if JMP
    if(setjmp(jumper) != 0)
        return;
#endif
    global_anker = eval(global_anker);
    stringEval("(v=\"Bracmat version " VERSION ", build " BUILD " (" DATUM ")\")",NULL,&err);
    return 1;
}

void endProc(void)
    {
    int err;
    static int called = 0;
    if(called)
        return;
    called = 1;
    stringEval("cat$:? CloseDown ? & CloseDown$ | ",NULL,&err);
    if(err)
        errorprintf("Error executing CloseDown\n");
    }

/* main - the text-mode front end for bracmat */


#if _BRACMATEMBEDDED
#else

#include <stddef.h>

#if defined EMSCRIPTEN
int oneShot(char * inp)
    {
    int err;
    char * mainloop;
    const char * ret;
    char * argv[1] = {NULL};
    argv[0] = inp;
    ARGV = argv;
    ARGC = 1;
    mainloop = "out$get$(arg$0,MEM)";
    stringEval(mainloop,&ret,&err);
    return (int)STRTOL(ret,0,10);
    }
#endif

int mainlus(int argc,char *argv[])
    {
    int err;
    char * mainloop;
    const char * ret = 0;
#if defined EMSCRIPTEN
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
        mainloop = "arg$&whl'(get$(arg$,MEM):?\"?...@#$*~%!\")&!\"?...@#$*~%!\"";
        stringEval(mainloop,&ret,&err);
        return ret ? (int)STRTOL(ret,0,10) : -1;
        }
    else
        {
        mainloop =
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
        "PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE\\n"
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
        stringEval(mainloop,NULL,&err);
        }
    return 0;
    }

extern void JSONtest(void);

#if EMSCRIPTEN_HTML
#else
int main(int argc,char *argv[])
    {
    int ret; /* numerical result of mainlus becomes exit code.
             If out of integer range or not numerical: 0*/
    char * p = argv[0] + strlen(argv[0]);
#if 0
    char s1[] = {160,0};
    char s2[] = {32,0};
#ifdef _WIN64 /* Microsoft 64 bit */
    printf("_WIN64\n");
#else /* 32 bit and gcc 64 bit */
    printf("!_WIN64\n");
#endif
    printf("sizeof(char *) " LONGU "\n",(unsigned long)sizeof(char *));
    printf("sizeof(LONG) " LONGU "\n",(unsigned long)sizeof(LONG));
    printf("sizeof(size_t) " LONGU "\n",(unsigned long)sizeof(size_t));
    printf("WORD32 %d\n",WORD32);
    printf("RADIX " LONGD "\n",(LONG)RADIX);
    printf("RADIX2 " LONGD "\n",(LONG)RADIX2);
    printf("TEN_LOG_RADIX " LONGD "\n",(LONG)TEN_LOG_RADIX);
    printf("HEADROOM " LONGD "\n",(LONG)HEADROOM);
    printf("strcmp:%d\n",strcmp(s1,s2));
    return 0;
#endif
#ifndef NDEBUG
        {
        LONG radix,ten_log_radix;
#if defined _WIN64
        assert(HEADROOM*RADIX2 < LLONG_MAX);
#else
        assert(HEADROOM*RADIX2 < LONG_MAX);
#endif
        for(radix = 1,ten_log_radix = 1;ten_log_radix <= TEN_LOG_RADIX;radix *= 10,++ten_log_radix)
            ;
        assert(RADIX == radix);
        }
#endif
    assert(sizeof(tFlags) == sizeof(unsigned int));
#if !defined EMSCRIPTEN
    while(--p >= argv[0])
        if(*p == '\\' || *p == '/')
            {
            ++p;
#if !defined NO_FOPEN
            targetPath = (char *)malloc(p - argv[0] + 1);
            if(targetPath)
                {
                strncpy(targetPath,argv[0],p - argv[0]);
                targetPath[p - argv[0]] = '\0';
                }
#endif
            break;
            }
#endif

    errorStream = stderr;
    if(!startProc())
        return -1;
    ret = mainlus(argc,argv);
    endProc();/* to get here, eg: {?} main=out$bye! */
#if !defined NO_FOPEN
    if(targetPath)
        free(targetPath);
#endif
    return ret;
    }
#endif
#endif /*#if !_BRACMATEMBEDDED*/

