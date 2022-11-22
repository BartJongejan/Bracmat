#ifndef UNICASECONV_H
#define UNICASECONV_H
/* Uses the fields Uppercase Mapping and Lowercase Mapping in Unicodedata.txt to convert lower case to upper case and vice versa */

struct ccaseconv {unsigned int L:21;int range:11;unsigned int inc:2;int dif:20;};
extern struct ccaseconv l2u[];
extern struct ccaseconv u2l[];

int convertLetter(int a, struct ccaseconv * T);
int toUpperUnicode(int a);
int toLowerUnicode(int a);

#endif
