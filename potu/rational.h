#ifndef RATIONAL_H
#define RATIONAL_H

#include "nodedefs.h"
#include "nnumber.h"

Qnumber qPlus(Qnumber _qx, Qnumber _qy, int minus);
int qCompare(Qnumber _qx, Qnumber _qy);
char * split(Qnumber _qget, nnumber * ptel, nnumber * pnoem);
Qnumber qModulo(Qnumber _qx, Qnumber _qy);
Qnumber qIntegerDivision(Qnumber _qx, Qnumber _qy);
psk qDenominator(psk Pnode);
Qnumber qqDivide(Qnumber _qx, Qnumber _qy);
Qnumber qTimes(Qnumber _qx, Qnumber _qy);
/*void convert2binary(nnumber * x);*/
Qnumber qIntegerDivision(Qnumber _qx, Qnumber _qy);
Qnumber qTimesMinusOne(Qnumber _qx);
int subroot(nnumber* ag, char* conc[], int* pind);

#endif
