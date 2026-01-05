#ifndef RESULT_H
#define RESULT_H

#include "nodestruct.h"

void result(psk Root);
void results(psk Root, psk cutoff);
#if (1 && SHOWWHETHERNEVERVISITED)
extern Boolean vis = FALSE;
#endif

#endif
