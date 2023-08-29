#ifndef COPY_H
#define COPY_H

#include "nodestruct.h"
psk iCopyOf(psk pnode);
psk copyof(psk pnode);
psk new_operator_like(psk pnode);
psk scopy(const char* str);


psk same_as_w(psk pnode);
psk _copyop(psk Pnode);
psk subtreecopy(psk src);
psk isolated(psk Pnode);
psk setflgs(psk pokn, ULONG Flgs);
#if ICPY
void icpy(LONG* d, LONG* b, int words);
#endif
psk copyop(psk Pnode);
psk charcopy(const char* strt, const char* until);


#endif
