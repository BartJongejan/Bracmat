#include "filewrite.h"

#include "nonnodetypes.h"
#include "globals.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

void(*process)(int c);

#if _BRACMATEMBEDDED

static int(*WinIn)(void) = NULL;
static void(*WinOut)(int c) = NULL;
static void(*WinFlush)(void) = NULL;
#if defined PYTHONINTERFACE
static void(*Ni)(const char *) = NULL;
static const char * (*Nii)(const char *) = NULL;
#endif
int mygetc(FILE * fpi)
    {
    if (WinIn && fpi == stdin)
        {
        return WinIn();
        }
    else
        return fgetc(fpi);
    }


void myputc(int c)
    {
    if (WinOut && (global_fpo == stdout || global_fpo == stderr))
        {
        WinOut(c);
        }
    else
        fputc(c, global_fpo);
    }
#else
void myputc(int c)
    {
#ifdef __EMSCRIPTEN__
    fputc(c, stdout);
#else
    fputc(c, global_fpo);
#endif
    }

int mygetc(FILE * fpi)
    {
#ifdef __SYMBIAN32__
    if (fpi == stdin)
        {
        static unsigned char inputbuffer[256] = { 0 };
        static unsigned char * out = inputbuffer;
        if (!*out)
            {
            static unsigned char * in = inputbuffer;
            static int kar;
            while (in < inputbuffer + sizeof(inputbuffer) - 2
                   && (kar = fgetc(fpi)) != '\n'
                   )
                {
                switch (kar)
                    {
                        case '\r':
                            break;
                        case 8:
                            if (in > inputbuffer)
                                {
                                --in;
                                putchar(' ');
                                putchar(8);
                                }
                            break;
                        default:
                            *in++ = kar;
                    }
                }
            *in = kar;
            *++in = '\0';
            out = in = inputbuffer;
            }
        return *out++;
        }
#endif
    return fgetc(fpi);
    }
#endif

void(*process)(int c) = myputc;

void myprintf(char *strng, ...)
    {
    char *i, *j;
    va_list ap;
    va_start(ap, strng);
    i = strng;
    while (i)
        {
        for (j = i; *j; j++)
            (*process)(*j);
        i = va_arg(ap, char *);
        }
    va_end(ap);
    }


#if _BRACMATEMBEDDED && !defined _MT
static int Printf(const char *fmt, ...)
    {
    char buffer[1000];
    int ret;
    va_list ap;
    va_start(ap, fmt);
    ret = vsprintf(buffer, fmt, ap);
    myprintf(buffer, NULL);
    va_end(ap);
    return ret;
    }
#else
#define Printf printf
#endif

int errorprintf(const char *fmt, ...)
    {
    char buffer[1000];
    int ret;
    FILE * save = global_fpo;
    va_list ap;
    va_start(ap, fmt);
    ret = vsprintf(buffer, fmt, ap);
    global_fpo = errorStream;
#if !defined NO_FOPEN
    if (global_fpo == NULL && errorFileName != NULL)
        global_fpo = fopen(errorFileName, APPENDTXT);
#endif
    /*#endif*/
    if (global_fpo)
        myprintf(buffer, NULL);
    else
        ret = 0;
    /*#if !_BRACMATEMBEDDED*/
#if !defined NO_FOPEN
    if (errorStream == NULL && global_fpo != NULL)
        fclose(global_fpo);
#endif
    /*#endif*/
    global_fpo = save;
    va_end(ap);
    return ret;
    }

int lineTooLong(unsigned char *strng)
    {
    if (hum
        && strlen((const char *)strng) > 10 /*LineLength*/
        /* very short strings are allowed to keep \n and \t */
        )
        return TRUE;
    return FALSE;
    }

