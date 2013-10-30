#include <stdio.h>
#include "bracmatdll.h"

void myputc(int c)
    {
    fputc(c,stdout);
    }

int mygetc()
    {
    return fgetc(stdin);
    }

void flush(void)
    {
    fflush(stdout);
    }

int main(int argc, char * argv[])
{
	int nRetCode = 0;

    CBracmatdll bracmat(myputc,mygetc,flush);
    bracmat.evaluate("(a=4*2/16)&out$(a is !a type)&get':?a&out$get$(!a,MEM,VAP)&get'",NULL);
    fnBracmatdll();
	return nRetCode;
}
