#ifndef FILEWRITE_H
#define FILEWRITE_H
#include "platformdependentdefs.h"
#include <stdio.h>

#if _BRACMATEMBEDDED

int(*WinIn)(void);
void(*WinOut)(int c);
void(*WinFlush)(void);
#if defined PYTHONINTERFACE
void(*Ni)(const char*);
const char* (*Nii)(const char*);
#endif
int mygetc(FILE* fpi);
void myputc(int c);
#else
void myputc(int c);
int mygetc(FILE* fpi);
#endif

extern void(*process)(int c);

void myprintf(const char* strng, ...);

#if _BRACMATEMBEDDED && !defined _MT
int Printf(const char* fmt, ...);
#else
#define Printf printf
#endif

int errorprintf(const char* fmt, ...);
int lineTooLong(unsigned char* strng);



#endif
