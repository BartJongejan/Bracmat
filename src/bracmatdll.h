
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the BRACMATDLL_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// BRACMATDLL_API functions as being imported from a DLL, wheras this DLL sees symbols
// defined with this macro as being exported.
#ifdef BRACMATDLL_EXPORTS
#define BRACMATDLL_API __declspec(dllexport)
#else
#define BRACMATDLL_API __declspec(dllimport)
#endif

// This class is exported from the bracmatdll.dll
typedef void (*tpout)(int c);
typedef int (*tpin)(void);
typedef void (*tpflush)(void);

class funcstruct
    {
public:
    tpout Out;
    tpin In;
    tpflush Flush;
    };

class BRACMATDLL_API CBracmatdll {
public:
    CBracmatdll(tpout Out,tpin In,tpflush Flush);
    CBracmatdll(funcstruct * fncs);
    CBracmatdll();
    CBracmatdll(char *);
    ~CBracmatdll();
    bool evaluate(const char * str,const char ** pout);
};

extern BRACMATDLL_API int nBracmatdll;

BRACMATDLL_API int fnBracmatdll(void);
//BRACMATDLL_API bool evaluate(const char * str,const char ** pout);
//BRACMATDLL_API void setCallBackFns(int (*WinIn)(void),void (*WinOut)(int c),void (*WinFlush)(void));

