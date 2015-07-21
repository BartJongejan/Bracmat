 /* ALGEB.h */

#ifndef ALGEB_H
#define ALGEB_H

typedef struct startStruct
    {
	int (*WinIn)(void);
    void (*WinOut)(int c);
    void (*WinFlush)(void);
#if defined PYTHONINTERFACE
    void (*Ni)(const char * str); /* exec */
    const char * (*Nii)(const char * str); /* eval() */
#endif
    } startStruct;

#ifdef __cplusplus
extern "C" int startProc(startStruct * init);
extern "C" void /*int*/ stringEval(const char * s,const char ** out,int * err);
/* 0 = FALSE, 1 = SUCCESS, otherwise FENCE */
/* *out = "" if evaluation of s is a tree */
extern "C" void endProc(void);
#else
extern int startProc(startStruct * init);
extern void /*int*/ stringEval(const char * s,const char ** out,int * err);
/* 0 = FALSE, 1 = SUCCESS, otherwise FENCE */
/* *out = "" if evaluation of s is a tree */
extern void endProc(void);
#endif

#endif /* defined ALGEB_H */
