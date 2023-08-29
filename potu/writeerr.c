#include "writeerr.h"
#include "nonnodetypes.h"
#include "globals.h"
#include "result.h"
#include "filewrite.h"
#include <stdio.h>
#include <string.h>

void writeError(psk Pnode)
    {
    FILE* saveFpo;
    int saveNice;
    saveNice = beNice;
    beNice = FALSE;
    saveFpo = global_fpo;
    global_fpo = errorStream;
#if !defined NO_FOPEN
    if(global_fpo == NULL && errorFileName != NULL)
        global_fpo = fopen(errorFileName, APPENDBIN);
#endif
    if(global_fpo)
        {
        result(Pnode);
        myputc('\n');
#if !defined NO_FOPEN
        if(errorStream == NULL && global_fpo != stderr && global_fpo != stdout)
            {
            fclose(global_fpo);
            }
#endif
        }
    global_fpo = saveFpo;
    beNice = saveNice;
    }

int redirectError(char* name)
    {
#if !defined NO_FOPEN
    if(errorFileName)
        {
        free(errorFileName);
        errorFileName = NULL;
        }
#endif
    if(!strcmp(name, "stdout"))
        {
        errorStream = stdout;
        return TRUE;
        }
    else if(!strcmp(name, "stderr"))
        {
        errorStream = stderr;
        return TRUE;
        }
    else
        {
#if !defined NO_FOPEN
        errorStream = fopen(name, APPENDTXT);
        if(errorStream)
            {
            fclose(errorStream);
            errorStream = NULL;
            errorFileName = (char*)malloc(strlen(name) + 1);
            if(errorFileName)
                {
                strcpy(errorFileName, name);
                return TRUE;
                }
            else
                return FALSE;
            }
#endif
        errorStream = stderr;
        }
    return FALSE;
    }
