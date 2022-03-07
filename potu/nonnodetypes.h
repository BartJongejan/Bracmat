#ifndef NONNODETYPES_H
#define NONNODETYPES_H

typedef int Boolean;

#define TRUE 1
#define FALSE 0

#define RADIX2 (RADIX * RADIX)


#define READBIN   "rb" /* BIN is default for get$, overrule with option TXT */
#define READTXT   "r"
#define WRITEBIN  "wb" /* BIN is default for lst$, overrule with option TXT */
#define APPENDBIN "ab" 
#define WRITETXT  "w"  /* TXT is default for put$, overrule with option BIN */
#define APPENDTXT "a"

#define PRISTINE (1<<2) /* Used to initialise matchstate variables. */
#define ONCE (1<<3)
#define POSITION_ONCE (1<<4)
#define POSITION_MAX_REACHED (1<<5)
#define SHIFT_LMR 8
#define SHIFT_RMR 16


#ifndef UNREFERENCED_PARAMETER
#if defined _MSC_VER
#define UNREFERENCED_PARAMETER(P) (P)
#else
#define UNREFERENCED_PARAMETER(P)
#endif
#endif



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

/* FUNCTIONS */


#define isSUCCESS(x) ((x)->v.fl & SUCCESS)
#define isFENCE(x) (((x)->v.fl & (SUCCESS|FENCE)) == FENCE)
#define isSUCCESSorFENCE(x) ((x)->v.fl & (SUCCESS|FENCE))
#define isFailed(x) (((x)->v.fl & (SUCCESS|FENCE)) == 0)

#endif