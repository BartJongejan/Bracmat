#include "../src/bracmat.h"
#include <stdio.h>

/* Compile with BRACMATEMBEDDED defined */

static int In(void)
    {
    return getchar();
    }

static void Out(int c)
    {
    putchar(c);
    }

static void Flush(void)
    {
#ifdef __GNUC__
    fflush(stdout);
#endif
    }

static startStruct StartStruct = {In,Out,Flush};

int mainlus(int argc,char *argv[])
    {
    int err;
    char * mainloop = 
    "(main=put$\"{?} \"&((get':?!(=):(|?&put$\"{!} \"&put$!&put$\\n)|put$\"\\n    F\")|put$\"\\n    I\")&out$&!main)&!main";
    stringEval(mainloop,0,&err);
    return 0;
    }

int main(int argc,char *argv[])
    {
    int ret; 
    startProc(&StartStruct);
    ret = mainlus(argc,argv);
    endProc();
    return ret;
    }

