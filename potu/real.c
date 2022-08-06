#include "real.h"

#include "nonnodetypes.h"
#include "globals.h"
#include "memory.h"
#include "copy.h"
#include "wipecopy.h"
#include <assert.h>
#include <string.h>
#include <math.h>

/* "1.2E3"*"4.5E6" */

Qnumber fTimes(Qnumber lvar, Qnumber rvar)
    {
    double left, right, product;
    char* rest;
    psk res;
    size_t len;
    char buf[64]; /* 64 bytes is even enough for quad https://people.eecs.berkeley.edu/~wkahan/ieee754status/IEEE754.PDF*/
    left = strtod(&(lvar->u.sobj), &rest);
    right = strtod(&(rvar->u.sobj), &rest);

    product = left * right;

    sprintf(buf, "%.16E", product);
    len = offsetof(sk, u.obj) + strlen(buf);
    res = (psk)bmalloc(__LINE__, len + 1);
    strcpy((char*)POBJ(res), buf);
    if(strcmp(buf,"INF"))
        res->v.fl = READY | SUCCESS | QNUMBER | QDOUBLE BITWISE_OR_SELFMATCHING;
    else
        res->v.fl = READY BITWISE_OR_SELFMATCHING;
    /*    res->v.fl |= g->sign;*/
    return res;
    }

Qnumber dfTimes(double left, Qnumber rvar)
    {
    double right, product;
    char* rest;
    psk res;
    size_t len;
    char buf[64]; /* 64 bytes is even enough for quad https://people.eecs.berkeley.edu/~wkahan/ieee754status/IEEE754.PDF*/
    right = strtod(&(rvar->u.sobj), &rest);

    product = left * right;

    sprintf(buf, "%.16E", product);
    len = offsetof(sk, u.obj) + strlen(buf);
    res = (psk)bmalloc(__LINE__, len + 1);
    strcpy((char*)POBJ(res), buf);
    if(strcmp(buf, "INF"))
        res->v.fl = READY | SUCCESS | QNUMBER | QDOUBLE BITWISE_OR_SELFMATCHING;
    else
        res->v.fl = READY BITWISE_OR_SELFMATCHING;
    /*    res->v.fl |= g->sign;*/
    return res;
    }

Qnumber fPlus(Qnumber lvar, Qnumber rvar)
    {
    double left, right, sum;
    char* rest;
    psk res;
    size_t len;
    char buf[64]; /* 64 bytes is even enough for quad https://people.eecs.berkeley.edu/~wkahan/ieee754status/IEEE754.PDF*/
    left = strtod(&(lvar->u.sobj), &rest);
    right = strtod(&(rvar->u.sobj), &rest);

    sum = left + right;

    sprintf(buf, "%.16E", sum);
    len = offsetof(sk, u.obj) + strlen(buf);
    res = (psk)bmalloc(__LINE__, len + 1);
    strcpy((char*)POBJ(res), buf);
    if(strcmp(buf, "INF"))
        res->v.fl = READY | SUCCESS | QNUMBER | QDOUBLE BITWISE_OR_SELFMATCHING;
    else
        res->v.fl = READY BITWISE_OR_SELFMATCHING;
    /*    res->v.fl |= g->sign;*/
    return res;
    }

Qnumber dfPlus(Qnumber lvar, double right)
    {
    double left, sum;
    char* rest;
    psk res;
    size_t len;
    char buf[64]; /* 64 bytes is even enough for quad https://people.eecs.berkeley.edu/~wkahan/ieee754status/IEEE754.PDF*/
    left = strtod(&(lvar->u.sobj), &rest);

    sum = left + right;

    sprintf(buf, "%.16E", sum);
    len = offsetof(sk, u.obj) + strlen(buf);
    res = (psk)bmalloc(__LINE__, len + 1);
    strcpy((char*)POBJ(res), buf);
    if(strcmp(buf, "INF"))
        res->v.fl = READY | SUCCESS | QNUMBER | QDOUBLE BITWISE_OR_SELFMATCHING;
    else
        res->v.fl = READY BITWISE_OR_SELFMATCHING;
    /*    res->v.fl |= g->sign;*/
    return res;
    }

Qnumber fExp(Qnumber lvar, Qnumber rvar)
    {
    double left, right, power;
    char* rest;
    psk res;
    size_t len;
    char buf[64]; /* 64 bytes is even enough for quad https://people.eecs.berkeley.edu/~wkahan/ieee754status/IEEE754.PDF*/
    left = strtod(&(lvar->u.sobj), &rest);
    right = strtod(&(rvar->u.sobj), &rest);

    power = pow(left,right);

    sprintf(buf, "%.16E", power);
    len = offsetof(sk, u.obj) + strlen(buf);
    res = (psk)bmalloc(__LINE__, len + 1);
    strcpy((char*)POBJ(res), buf);
    if(strcmp(buf, "INF"))
        res->v.fl = READY | SUCCESS | QNUMBER | QDOUBLE BITWISE_OR_SELFMATCHING;
    else
        res->v.fl = READY BITWISE_OR_SELFMATCHING;
    /*    res->v.fl |= g->sign;*/
    return res;
    }

psk Cmath(Qnumber name, Qnumber argument)
    {
    char* rest;
    double arg = strtod(&(argument->u.sobj), &rest);
    char* fun = (char*)POBJ(name);
    double(*cfun)(double a) = 0;
    double result;
    psk resultNode = 0;
    if(!strcmp(fun, "sin"))
        {
        cfun = sin;
        }
    if(cfun)
        {
        size_t len;
        char buf[64]; /* 64 bytes is even enough for quad https://people.eecs.berkeley.edu/~wkahan/ieee754status/IEEE754.PDF*/

        result = (*cfun)(arg);

        sprintf(buf, "%.16E", result);
        len = offsetof(sk, u.obj) + strlen(buf);
        resultNode = (psk)bmalloc(__LINE__, len + 1);
        strcpy((char*)POBJ(resultNode), buf);
        if(strcmp(buf, "INF"))
            resultNode->v.fl = READY | SUCCESS | QNUMBER | QDOUBLE BITWISE_OR_SELFMATCHING;
        else
            resultNode->v.fl = READY BITWISE_OR_SELFMATCHING;
        }
    return resultNode;
    }
