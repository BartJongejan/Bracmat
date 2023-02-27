#ifndef FLAGS_H
#define FLAGS_H

#include "defines01.h"
#include "platformdependentdefs.h"

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
#define SELFMATCHING    (1<<23) /* 20210801 */
#define BITWISE_OR_SELFMATCHING |SELFMATCHING
#else
#define BITWISE_OR_SELFMATCHING 
#endif

#if WORD32
#else
#if DATAMATCHESITSELF
#define BUILT_IN        (1<<24) /* 20210801 only used for objects (operator =) */
#define CREATEDWITHNEW  (1<<25) /* 20210801 only used for objects (operator =) */
#else
#define BUILT_IN        (1<<23) /* 20210801 only used for objects (operator =) */
#define CREATEDWITHNEW  (1<<24) /* 20210801 only used for objects (operator =) */
#endif
#endif

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

#if DATAMATCHESITSELF
#if WORD32
#define NON_REF_COUNT_BITS 24 /* prefixes, hidden flags, operator bits */
#else
#define NON_REF_COUNT_BITS 26 /* prefixes, hidden flags, operator bits */
#endif
#else
#if WORD32
#define NON_REF_COUNT_BITS 23 /* prefixes, hidden flags, operator bits */
#else
#define NON_REF_COUNT_BITS 25 /* prefixes, hidden flags, operator bits */
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

#define NOOP                (OPERATOR+1)
#define QNUMBER             (1 << (SHL+1))
#define MINUS               (1 << (SHL+2))
#define QNUL                (1 << (SHL+3))
#define QFRACTION           (1 << (SHL+4))
#define QDOUBLE             (1 << (SHL+5))
#define LATEBIND            (1 << (SHL+6))
#define DEFINITELYNONUMBER  (1 << (SHL+7)) /* this is not stored in a node! */
#define ONEREF   (ULONG)(1 << NON_REF_COUNT_BITS)

#define ALL_REFCOUNT_BITS_SET \
       ((((ULONG)(~0)) >> NON_REF_COUNT_BITS) << NON_REF_COUNT_BITS)
#if WORD32
#define COPYFILTER ~ALL_REFCOUNT_BITS_SET
#else
#define COPYFILTER ~(ALL_REFCOUNT_BITS_SET | BUILT_IN | CREATEDWITHNEW)
#endif

#define VISIBLE_FLAGS_WEAK              (INDIRECT|DOUBLY_INDIRECT|FENCE|UNIFY)
#define VISIBLE_FLAGS_NON_COMP          (INDIRECT|DOUBLY_INDIRECT|ATOM|NONIDENT|NUMBER|FRACTION|UNIFY) /* allows < > ~< and ~> as flags on numbers */
#define VISIBLE_FLAGS_POS0              (INDIRECT|DOUBLY_INDIRECT|NONIDENT|QFRACTION|QDOUBLE|UNIFY|QNUMBER)
#define VISIBLE_FLAGS_POS               (INDIRECT|DOUBLY_INDIRECT|NONIDENT|QFRACTION|QDOUBLE|UNIFY|QNUMBER|NOT|GREATER_THAN|SMALLER_THAN)
#define VISIBLE_FLAGS                   (INDIRECT|DOUBLY_INDIRECT|ATOM|NONIDENT|NUMBER|FRACTION|UNIFY|NOT|GREATER_THAN|SMALLER_THAN|FENCE|POSITION)

#define HAS_VISIBLE_FLAGS_OR_MINUS(psk) ((psk)->v.fl & (VISIBLE_FLAGS|MINUS))
#define RATIONAL(psk)                   (((psk)->v.fl & (QNUMBER|IS_OPERATOR|VISIBLE_FLAGS)) == QNUMBER)
//#define REAL_COMP(psk)                  (((psk)->v.fl & (QNUMBER|QDOUBLE|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP)) == (QNUMBER|QDOUBLE))
#define RATIONAL_COMP(psk)              (((psk)->v.fl & (QNUMBER|QDOUBLE|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP)) == QNUMBER)
#define RATIONAL_COMP_NOT_NUL(psk)      (((psk)->v.fl & (QNUMBER|QNUL|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP)) == QNUMBER)
#define RATIONAL_WEAK(psk)              (((psk)->v.fl & (QNUMBER|IS_OPERATOR|INDIRECT|DOUBLY_INDIRECT|FENCE|UNIFY)) == QNUMBER)/* allows < > ~< and ~> as flags on numbers */
#define          LESS(psk)              (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|SMALLER_THAN))
#define    LESS_EQUAL(psk)              (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|NOT|GREATER_THAN))
#define    MORE_EQUAL(psk)              (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|NOT|SMALLER_THAN))
#define          MORE(psk)              (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|GREATER_THAN))
#define       UNEQUAL(psk)              (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|NOT))
#define    LESSORMORE(psk)              (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|SMALLER_THAN|GREATER_THAN))
#define         EQUAL(psk)              (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == QNUMBER)
#define NOTLESSORMORE(psk)              (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|NOT|SMALLER_THAN|GREATER_THAN))

#define          FLESS(psk)             (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|QDOUBLE|SMALLER_THAN))
#define    FLESS_EQUAL(psk)             (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|QDOUBLE|NOT|GREATER_THAN))
#define    FMORE_EQUAL(psk)             (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|QDOUBLE|NOT|SMALLER_THAN))
#define          FMORE(psk)             (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|QDOUBLE|GREATER_THAN))
#define       FUNEQUAL(psk)             (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|QDOUBLE|NOT))
#define    FLESSORMORE(psk)             (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|QDOUBLE|SMALLER_THAN|GREATER_THAN))
#define         FEQUAL(psk)             (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|QDOUBLE))
#define FNOTLESSORMORE(psk)             (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|QDOUBLE|NOT|SMALLER_THAN|GREATER_THAN))

#define          ILESS(psk)             (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|SMALLER_THAN))
#define    ILESS_EQUAL(psk)             (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|NOT|GREATER_THAN))
#define    IMORE_EQUAL(psk)             (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|NOT|SMALLER_THAN))
#define          IMORE(psk)             (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|GREATER_THAN))
#define       IUNEQUAL(psk)             (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|NOT))
#define    ILESSORMORE(psk)             (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|SMALLER_THAN|GREATER_THAN))
#define         IEQUAL(psk)             (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER))
#define INOTLESSORMORE(psk)             (((psk)->v.fl & (VISIBLE_FLAGS_POS)) == (QNUMBER|NOT|SMALLER_THAN|GREATER_THAN))

#define INTEGER(pn)                     (((pn)->v.fl & (QNUMBER|QFRACTION|QDOUBLE|IS_OPERATOR|VISIBLE_FLAGS))                      == QNUMBER)
#define INTEGER_COMP(pn)                (((pn)->v.fl & (QNUMBER|QFRACTION|QDOUBLE|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP))             == QNUMBER)

#define INTEGER_NOT_NEG(pn)             (((pn)->v.fl & (QNUMBER|MINUS|QFRACTION|QDOUBLE|IS_OPERATOR|VISIBLE_FLAGS))                == QNUMBER)
#define INTEGER_NOT_NEG_COMP(pn)        (((pn)->v.fl & (QNUMBER|MINUS|QFRACTION|QDOUBLE|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP))       == QNUMBER)

#define INTEGER_POS(pn)                 (((pn)->v.fl & (QNUMBER|MINUS|QNUL|QFRACTION|QDOUBLE|IS_OPERATOR|VISIBLE_FLAGS))           == QNUMBER)
#define INTEGER_POS_COMP(pn)            (((pn)->v.fl & (QNUMBER|MINUS|QNUL|QFRACTION|QDOUBLE|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP))  == QNUMBER)

#define INTEGER_NOT_NUL_COMP(pn)        (((pn)->v.fl & (QNUMBER|QNUL|QFRACTION|QDOUBLE|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP))        == QNUMBER)
#define HAS_MINUS_SIGN(pn)              (((pn)->v.fl & (MINUS|IS_OPERATOR)) == MINUS)

#define RAT_NUL(pn)                     (((pn)->v.fl & (QNUL|IS_OPERATOR|VISIBLE_FLAGS)) == QNUL)
#define RAT_NUL_COMP(pn)                (((pn)->v.fl & (QNUL|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP)) == QNUL)
#define RAT_NEG(pn)                     (((pn)->v.fl & (QNUMBER|MINUS|IS_OPERATOR|VISIBLE_FLAGS)) == (QNUMBER|MINUS))
#define RAT_NEG_COMP(pn)                (((pn)->v.fl & (QNUMBER|MINUS|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP)) == (QNUMBER|MINUS))

#define RAT_RAT(pn)                     (((pn)->v.fl & (QNUMBER|QFRACTION|QDOUBLE|IS_OPERATOR|VISIBLE_FLAGS)) == (QNUMBER|QFRACTION))

#define RAT_RAT_COMP(pn)                (((pn)->v.fl & (QNUMBER|QFRACTION|QDOUBLE|IS_OPERATOR|VISIBLE_FLAGS_NON_COMP)) == (QNUMBER|QFRACTION))
#define IS_ONE(pn)                      ((pn)->u.lobj == ONE && !((pn)->v.fl & (MINUS | VISIBLE_FLAGS)))
#define IS_NIL(pn)                      ((pn)->u.lobj == 0   && !((pn)->v.fl & (MINUS | VISIBLE_FLAGS)))

#define functionFail(x) ((x)->v.fl ^= SUCCESS,(x))
#define functionOk(x) (x)

#endif
