#include "bracmat.h"

/*#include <stddef.h>*/

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
    startProc(0);
    ret = mainlus(argc,argv);
    endProc();
    return ret;
    }

