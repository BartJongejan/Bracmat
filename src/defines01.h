#ifndef DEFINES01_H
#define DEFINES01_H

#define DEBUGBRACMAT 0 /* Implement dbg'(expression), see DBGSRC macro */
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

#define INTSCMP 0   /* dangerous way of comparing strings */
#define ICPY 0      /* copy word by word instead of byte by byte */

/* Optional #defines for debugging and adaptation to machine */

#define SHOWMAXALLOCATED  1 /* Show the maximum number of allocated nodes. */
#define SHOWCURRENTLYALLOCATED 0 /* Same, plus current number of allocated nodes, in groups of
                       4,8,12 and >12 bytes */
#ifdef UNVISITED /* set in Makefile */
#define SHOWWHETHERNEVERVISISTED 1 /* If set, operator nodes that were never
                       visited during running a Bracmat program are marked
                       with an affixed pair of curly brackets {} when
                       the program is lst$ed*/
#else
#define SHOWWHETHERNEVERVISISTED 0
#endif

#define EXPAND 0

#if DOSUMCHECK
#define BMALLOC(x,y,z) Bmalloc(x,y,z)
#define bmalloc(n) BMALLOC(__FILE__,__LINE__,n)
#else
#define bmalloc(n) Bmalloc(n)
#endif

#endif
