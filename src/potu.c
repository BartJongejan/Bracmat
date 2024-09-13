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

#define DATUM "13 September 2024"
#define VERSION "6.25.2"
#define BUILD "307"
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
implemented reading XML files. Another separate source file, json.c,
implemented reading JSON files.
Since autumn 2023, bracmat.c is again complete source code for bracmat,
but it is no longer meant to be edited by hand! bracmat.c is now
in the folder 'singlesource' and created from multiple source files that
are in the 'src' folder. The script doing this is called 'one.bra'. It is
of course also possible to compile the source code in 'src' without first 
creating bracmat.c.

On *N?X, just compile with

    gcc -std=c99 -pedantic -Wall -O3 -static -DNDEBUG -o bracmat bracmat.c -lm


The options are not necessary to successfully compile the program just

    gcc *.c -lm

also works and produces the executable a.out.

Profiling:

    gcc -Wall -c -pg -DNDEBUG bracmat.c
    gcc -Wall -pg bracmat.o -lm
    ./a.out 'get$"valid.bra";!r'
    gprof a.out

Test coverage:
(see http://gcc.gnu.org/onlinedocs/gcc/Invoking-Gcov.html#Invoking-Gcov)

    gcc -fprofile-arcs -ftest-coverage -DNDEBUG bracmat.c -lm
    ./a.out
    gcov bracmat.c

*/
/* How to compile with a ANSI-C compiler or newer 
(OBSOLETE, Norcroft compiler, Microsoft qcl, Borland C, Atari compiler are not C99 compatible)

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

#include "defines01.h"
#include "platformdependentdefs.h"
#include "flags.h"
#include "nodedefs.h"
#include "nodestruct.h"
#include "nonnodetypes.h"
#include "typedobjectnode.h"
#include "objectnode.h"
#include "nnumber.h"
#include "hashtypes.h"
#include "unicaseconv.h"
#include "matchstate.h"
#include "charput.h"

#if defined SINGLESOURCE
//#define NODESTRUCT_H
#define UNICASECONV_H
#define UNICHARTYPES_H
#define GLOBALS_H                  
#define FILEWRITE_H
#define NUMBERCHECK_H
#define MEMORY_H
#define COPY_H
#define HEAD_H
#define BUILTINMETHOD_H
#define QUOTE_H
#define RESULT_H
#define CHARPUT_H
#define ENCODING_H
#define JSON_H
#define XML_H
#define WRITEERR_H
#define INPUT_H
#define WIPECOPY_H
#define RATIONAL_H
#define REAL_H
#define EQUAL_H
#define OBJECT_H
#define NODEUTIL_H
#define LAMBDA_H
#define OPT_H
#define VARIABLES_H
#define MACRO_H
#define HASH_H
#define CALCULATION_H
#define BINDING_H
#define POSITION_H
#define STRINGMATCH_H
#define TREEMATCH_H
#define BRANCH_H
#define FILESTATUS_H
#define SIMIL_H
#define OBJECTDEF_H
//#define FUNCTIONS_H
#define CANONIZATION_C
#define EVALUATE_H

#include "unicaseconv.c"
#include "unichartypes.c"
#include "globals.c"
#include "filewrite.c"
#include "numbercheck.c"
#include "memory.c"
#include "copy.c"
#include "head.c"
#include "builtinmethod.c"
#include "quote.c"
#include "result.c"
#include "charput.c"
#include "encoding.c"
#include "json.c"
#include "xml.c"
#include "writeerr.c"
#include "input.c"
#include "wipecopy.c"
#include "rational.c"
#include "equal.c"
#include "object.c"
#include "nodeutil.c"
#include "lambda.c"
#include "opt.c"
#include "branch.c"
#include "variables.c"
#include "macro.c"
#include "hash.c"
#include "calculation.c"
#include "binding.c"
#include "position.c"
#include "stringmatch.c"
#include "treematch.c"
#include "filestatus.c"
#include "simil.c"
#include "objectdef.c"
#include "functions.c"
#include "canonization.c"
#include "evaluate.c"
#else /*#if defined SINGLESOURCE*/

#include "globals.h"
#include "filewrite.h"
#include "numbercheck.h"
#include "memory.h"
#include "copy.h"
#include "head.h"
#include "builtinmethod.h"
#include "quote.h"
#include "result.h"
#include "charput.h"
#include "json.h"
#include "xml.h"
#include "writeerr.h"
#include "input.h"
#include "wipecopy.h"
#include "rational.h"
#include "encoding.h"
#include "equal.h"
#include "object.h"
#include "nodeutil.h"
#include "lambda.h"
#include "opt.h"
#include "branch.h"
#include "variables.h"
#include "macro.h"
#include "hash.h"
#include "calculation.h"
#include "binding.h"
#include "position.h"
#include "stringmatch.h"
#include "treematch.h"
#include "filestatus.h"
#include "simil.h"
#include "objectdef.h"
#include "functions.h"
#include "canonization.h"
#include "eval.h"
#include "evaluate.h"
#endif /*#if defined SINGLESOURCE*/


#include <assert.h>

#if SHOWCURRENTLYALLOCATED
#if SHOWMAXALLOCATED == 0
#undef SHOWMAXALLOCATED
#define SHOWMAXALLOCATED 1
#endif
#ifndef SHOWMAXALLOCATED
#define SHOWMAXALLOCATED 1
#endif
#endif

/*#define reslt parenthesised_result */ /* to show ALL parentheses (one pair for each operator)*/

#define LOGWORDLENGTH 2



#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

#if READMARKUPFAMILY
#include "xml.h"
#endif

#if READJSON
#include "json.h"
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

#include <stdarg.h>






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


static const char
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

int startProc(
#if _BRACMATEMBEDDED
    startStruct* init
#else
    void
#endif
)
    {
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
    initVariables();
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
                                "`%?f ?m)&str$(!s*!f (!d:~1&\256|) !m \305 !e))),",

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
                                "(nestML=a L B s e x HT5.:?L&whl'(!arg:%?a ?arg&!a !L:?L)&!L:?arg&:?L:?B:"
                                "?s&whl'(!arg:%?a ?arg&(!a:(.?e.)&(!L.) !B:?B&:?L&!e !s:?s|(!a:(?e.?,?"
                                ")&!a|!a:(?e.?x)&(!s:!e ?s&(!e.!x,!L) (!B:(%?L.) ?B&)|!a)|!a) !L:?L))&"
//                                "(low$!s:html&!L:?B ((?.?,?) ?:?L)&!B (!s.,!L)|!L)),",
                                "(low$!s:html&!L:?B ((?.?,?) ?:?L)&!B (!s.,!L):?L|)&(!L:(? (\241DOCTYPE."
                                "@(?:? ~<>html ?)) ?|? (~<>html.?) ?)&(HT5=L M e x a C p g A.(p=li&(g=li)"
                                "|(dt|dd)&(g=dt|dd)|p&(g=address|article|aside|blockquote|details|div|dl|"
                                "fieldset|figcaption|figure|footer|form|h1|h2|h3|h4|h5|h6|header|hgroup|"
                                "hr|main|menu|nav|ol|p|pre|search|section|table|ul)|(rt|rp)&(g=rt|rp)|"
                                "optgroup&(g=optgroup|hr)|option&(g=option|optgroup|hr)|(thead|tbody)&"
                                "(g=tbody|tfoot)|tfoot&(g=)|tr&(g=tr)|(td|th)&(g=td|th))&:?L:?M&whl'(!arg"
                                ":%?x ?arg&(!x:(?e.?a,?C)&(!e.!a,HT5$!C):?x|)&(!x:(!p:?e.~(?,?):?C)&("
                                "!arg:?A ((!g.?) ?:?arg)&(!e.!C,HT5$!A):?x|(!e.!C,HT5$!arg):?x&:?arg)|!x:"
                                "((area|base|br|col|command|embed|hr|img|input|keygen|link|meta|param|"
                                "source|track|wbr):?e.~(?,?):?A)&(!e.!A,):?x|)&!x !L:?L)&whl'(!L:%?x ?L"
                                "&!x !M:?M)&!M)&HT5$!L|!L)),",
                                "(toML=O d t g l \246 w \242 \247 H G S D E F I U x Q.:?O&0:?x&chr$\261\262\267"
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
                                "?B)&(!B:(?T,?C)&($Q&d$(\274 !A t$!T \257\276)|d$(\274 !A t$!T "
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

#if !_BRACMATEMBEDDED

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




#if SHOWMAXALLOCATED

                "out$str$(\"  \" bez')&"
#else
                "out$&"
#endif

                "!main)&!main";
            stringEval(mainLoopExpression, NULL, &err);
            }
    return 0;
    }

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

