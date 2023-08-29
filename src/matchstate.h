#ifndef MATCHSTATE_H
#define MATCHSTATE_H

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

#endif
