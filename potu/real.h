#ifndef REAL_H
#define REAL_H

#include "nodedefs.h"

#define MPI 3.14159265358979323846 
#define ME  2.718281828459045090795598298427648842334747314453125

Qnumber fTimes(Qnumber _qx, Qnumber _qy);
Qnumber dfTimes(double _qx, Qnumber _qy);
Qnumber fPlus(Qnumber _qx, Qnumber _qy);
Qnumber dfPlus(Qnumber _qx, double _qy);
Qnumber fExp(Qnumber _qx, Qnumber _qy);

psk Cmath(Qnumber name, Qnumber argument);
psk Cmath2(Qnumber name, Qnumber argument1, Qnumber argument2);
#endif
