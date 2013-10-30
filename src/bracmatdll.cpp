// bracmatdll.cpp : Defines the entry point for the DLL application.
//

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

#include <windows.h>
#include "bracmatdll.h"

#include "bracmat.h"

int braIn()
    {
    return '\n';
    }

void braOut(int c)
    {
    }

void braFlush()
    {
    }

startStruct bracmatStartStruct = { braIn, braOut, braFlush };

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
    switch (ul_reason_for_call)
	{
		case DLL_PROCESS_ATTACH:
		case DLL_THREAD_ATTACH:
		case DLL_THREAD_DETACH:
		case DLL_PROCESS_DETACH:
			break;
    }
    return TRUE;
}


// This is an example of an exported variable
BRACMATDLL_API int nBracmatdll=0;

// This is an example of an exported function.
BRACMATDLL_API int fnBracmatdll(void)
{
	return 42;
}

/*
BRACMATDLL_API void setCallBackFns(int (*WinIn)(void),void (*WinOut)(int c),void (*WinFlush)(void))
    {
    if(WinIn)
        bracmatStartStruct.WinIn = WinIn;

    if(WinOut)
        bracmatStartStruct.WinOut = WinOut;

    if(WinFlush)
        bracmatStartStruct.WinFlush = WinFlush;
    }
*/

//BRACMATDLL_API bool evaluate(const char * str,const char ** pout)
bool CBracmatdll::evaluate(const char * str,const char ** pout)
    {
    if(str)
        {
        const char * out;
        int err;
        stringEval(str,&out,&err);
        if(pout)
            *pout = out;
        return !err;
        }
    else
        return false;
    }


// This is the constructor of a class that has been exported.
// see bracmatdll.h for the class definition
CBracmatdll::CBracmatdll(void (*WinOut)(int c),int (*WinIn)(void),void (*WinFlush)(void))
    { 
    if(WinIn)
        bracmatStartStruct.WinIn = WinIn;

    if(WinOut)
        bracmatStartStruct.WinOut = WinOut;

    if(WinFlush)
        bracmatStartStruct.WinFlush = WinFlush;
    startProc(&bracmatStartStruct);
    }

CBracmatdll::CBracmatdll(funcstruct * fncs)
    {
    if(fncs)
        {
        if(fncs->In)
            bracmatStartStruct.WinIn = fncs->In;

        if(fncs->Out)
            bracmatStartStruct.WinOut = fncs->Out;

        if(fncs->Flush)
            bracmatStartStruct.WinFlush = fncs->Flush;
        startProc(&bracmatStartStruct);
        }
    }

CBracmatdll::CBracmatdll()
    {
    }

CBracmatdll::CBracmatdll(char *)
    {
    }


CBracmatdll::~CBracmatdll()
    {
    endProc();
    }
