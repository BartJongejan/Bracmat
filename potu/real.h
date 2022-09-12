#ifndef REAL_H
#define REAL_H

#include "nodedefs.h"

Qnumber fTimes(Qnumber _qx, Qnumber _qy);
Qnumber dfTimes(double _qx, Qnumber _qy);
Qnumber fPlus(Qnumber _qx, Qnumber _qy);
Qnumber dfPlus(Qnumber _qx, double _qy);
Qnumber fExp(Qnumber _qx, Qnumber _qy);

psk Cmath(Qnumber name, Qnumber argument);
#endif
