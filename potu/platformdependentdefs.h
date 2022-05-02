#ifndef PLATFORMDEPENDENTDEFS_H
#define PLATFORMDEPENDENTDEFS_H

#include "defines01.h"

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

#if BIGENDIAN
#define iobj lobj
#define O(a,b,c) (a*0x1000000L+b*0x10000L+c*0x100)
#else
#if !WORD32
#define iobj lobj
#endif
#define O(a,b,c) (a+b*0x100+c*0x10000L)
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

#ifdef ATARI
extern int _stksize = -1;
#endif

#if (defined __TURBOC__ && !defined __WIN32__) || (defined ARM && !defined __SYMBIAN32__)
#define O_S 1 /* 1 = with operating system interface swi$ (RISC_OS or TURBO-C), 0 = without  */
#else
#define O_S 0
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

#if ICPY
#define MEMCPY(a,b,n) icpy((LONG*)(a),(LONG*)(b),n)
#else
#define MEMCPY(a,b,n) memcpy((char *)(a),(char *)(b),n)
#endif

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

#if DEBUGBRACMAT
#define DBGSRC(code)    if(debug){code}
#else
#define DBGSRC(code)
#endif

#if INTSCMP
#define STRCMP(a,b) intscmp((LONG*)(a),(LONG*)(b))
#else
#define STRCMP(a,b) strcmp((char *)(a),(char *)(b))
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
#define ONE O('1', 0 , 0 )
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
#define Chr O('c','h','r')
#define Chu O('c','h','u')
#define LIN O('L','I','N')
#define LOW O('l','o','w')
#define REV O('r','e','v') /* strrev */
#define LST O('l','s','t')
#define MEM O('M','E','M')
#define MINONE O('-','1',0)
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
#define TWO O('2', 0 , 0 )
#define TXT O('T','X','T')
#define UPP O('u','p','p')
#define UTF O('u','t','f')
#define VAP O('V','A','P')
#define WHL O('w','h','l')
#define X   O('X', 0 , 0 )
#define X2D O('x','2','d') /* hex -> dec */
#define D2X O('d','2','x') /* dec -> hex */
#define MAP O('m','a','p')
#define MOP O('m','o','p')
#define Vap O('v','a','p') /* map for string instead of list */
#define XX  O('e', 0 , 0 )


#if GLOBALARGPTR
#define VOIDORARGPTR void
#else
#define VOIDORARGPTR va_list * pargptr
#endif

#if !DEBUGBRACMAT
#define match(IND,SUB,PAT,SNIJAF,POS,LENGTH,OP) match(SUB,PAT,SNIJAF,POS,LENGTH,OP)
#if CUTOFFSUGGEST
#define stringmatch(IND,WH,SUB,SNIJAF,PAT,PKN,POS,LENGTH,SUGGESTEDCUTOFF,MAYMOVESTARTOFSUBJECT) stringmatch(SUB,SNIJAF,PAT,PKN,POS,LENGTH,SUGGESTEDCUTOFF,MAYMOVESTARTOFSUBJECT)
#else
#define stringmatch(IND,WH,SUB,SNIJAF,PAT,PKN,POS,LENGTH) stringmatch(SUB,SNIJAF,PAT,PKN,POS,LENGTH)
#endif
#endif

#endif
