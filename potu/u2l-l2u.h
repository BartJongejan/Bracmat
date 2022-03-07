#ifndef U2L_L2U_H
#define U2L_L2U_H
/* Based on http://unicode.org/Public/UNIDATA/UnicodeData.txt 01-Apr-2019 00:08 */
/* Structures created with uni.bra */

struct ccaseconv {unsigned int L:21;int range:11;unsigned int inc:2;int dif:20;};
extern struct ccaseconv u2l[];
extern struct ccaseconv l2u[];
#endif
