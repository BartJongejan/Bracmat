#include "filestatus.h"
#include "nonnodetypes.h"
#include "platformdependentdefs.h"
#include "memory.h"
#include "globals.h"
#include "nodedefs.h"
#include "nodeutil.h"
#include "wipecopy.h"
#include "copy.h"
#include "input.h"
#include "opt.h"
#include "filewrite.h"
#include "branch.h"
#include "variables.h"
#include "eval.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#if !defined NO_FOPEN

enum { NoPending, Writing, Reading };
typedef struct fileStatus
    {
    char* fname;
    FILE* fp;
    struct fileStatus* next;
#if !defined NO_LOW_LEVEL_FILE_HANDLING
    Boolean dontcloseme;
    LONG filepos; /* Normally -1. If >= 0, then the file is closed.
                When reopening, filepos is used to find the position
                before the file was closed. */
    LONG mode;
    LONG type;
    LONG size;
    LONG number;
    LONG time;
    int rwstatus;
    char* stop; /* contains characters to stop reading at, default NULL */
#endif
    } fileStatus;

static fileStatus* fs0 = NULL;
#endif

#if !defined NO_FOPEN
static fileStatus* findFileStatusByName(const char* name)
    {
    fileStatus* fs;
    for(fs = fs0
        ; fs
        ; fs = fs->next
        )
        if(!strcmp(fs->fname, name))
            return fs;
    return NULL;
    }

static fileStatus* allocateFileStatus(const char* name, FILE* fp
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                                      , Boolean dontcloseme
#endif
)
    {
    fileStatus* fs = (fileStatus*)bmalloc(sizeof(fileStatus));
    fs->fname = (char*)bmalloc(strlen(name) + 1);
    strcpy(fs->fname, name);
    fs->fp = fp;
#if !defined NO_LOW_LEVEL_FILE_HANDLING
    fs->dontcloseme = dontcloseme;
#endif
    fs->next = fs0;
    fs0 = fs;
    return fs0;
    }

static void deallocateFileStatus(fileStatus* fs)
    {
    fileStatus* fsPrevious, * fsaux;
    for(fsPrevious = NULL, fsaux = fs0
        ; fsaux != fs
        ; fsPrevious = fsaux, fsaux = fsaux->next
        )
        ;
    if(fsPrevious)
        fsPrevious->next = fs->next;
    else
        fs0 = fs->next;
    if(fs->fp)
        fclose(fs->fp);
    bfree(fs->fname);
#if !defined NO_LOW_LEVEL_FILE_HANDLING
    if(fs->stop)
#ifdef BMALLLOC
        bfree(fs->stop);
#else
        free(fs->stop);
#endif
#endif
    bfree(fs);
    }

fileStatus* mygetFileStatus(const char* filename, const char* mode
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                            , Boolean dontcloseme
#endif
)
    {
    FILE* fp = fopen(filename, mode);
    if(fp)
        {
        fileStatus* fs = allocateFileStatus(filename, fp
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                                            , dontcloseme
#endif
        );
        return fs;
        }
    return NULL;
    }

fileStatus* myfopen(const char* filename, const char* mode
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                    , Boolean dontcloseme
#endif
)
    {
#if !defined NO_FOPEN
    if(!findFileStatusByName(filename))
        {
        fileStatus* fs = mygetFileStatus(filename, mode
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                                         , dontcloseme
#endif
        );
        if(!fs && targetPath && strchr(mode, 'r'))
            {
            const char* p = filename;
            char* q;
            size_t len;
            while(*p)
                {
                if(*p == '\\' || *p == '/')
                    {
                    if(p == filename)
                        return NULL;
                    break;
                    }
                else if((*p == ':') && (p == filename + 1))
                    return NULL;
                ++p;
                }
            q = (char*)malloc((len = strlen(targetPath)) + strlen(filename) + 1);
            if(q)
                {
                strcpy(q, targetPath);
                strcpy(q + len, filename);
                fs = mygetFileStatus(q, mode
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                                     , dontcloseme
#endif
                );
                free(q);
                }
            }
        return fs;
        }
#endif
    return NULL;
    }
#endif

#if !defined NO_LOW_LEVEL_FILE_HANDLING
static LONG someopt(psk pnode, LONG opt[])
    {
    int i;
    assert(!is_op(pnode));
    for(i = 0; opt[i]; i++)
        if(PLOBJ(pnode) == opt[i])
            return opt[i];
    return 0L;
    }

#if !defined NO_FOPEN

static LONG tijdnr = 0L;

static int closeAFile(void)
    {
    fileStatus* fs, * fsmin;
    if(fs0 == NULL)
        return FALSE;
    for(fs = fs0, fsmin = fs0;
        fs != NULL;
        fs = fs->next)
        {
        if(!fs->dontcloseme
           && fs->filepos == -1L
           && fs->time < fsmin->time
           )
            fsmin = fs;
        }
    if(fsmin == NULL || fsmin->dontcloseme)
        {
        return FALSE;
        }
    fsmin->filepos = FTELL(fsmin->fp);
    /* fs->filepos != -1 means that the file is closed */
    fclose(fsmin->fp);
    fsmin->fp = NULL;
    return TRUE;
    }
#if defined NDEBUG
#define preparefp(fs,name,mode) preparefp(fs,mode)
#endif

static fileStatus* preparefp(fileStatus* fs, char* name, LONG mode)
    {
    assert(fs != NULL);
    assert(!strcmp(fs->fname, name));
    if(mode != 0L
       && mode != fs->mode
       && fs->fp != NULL)
        {
        fs->mode = mode;
        if((fs->fp = freopen(fs->fname, (char*)&(fs->mode), fs->fp)) == NULL)
            return NULL;
        fs->rwstatus = NoPending;
        }
    else if(fs->filepos >= 0L)
        {
        if((fs->fp = fopen(fs->fname, (char*)&(fs->mode))) == NULL)
            {
            if(closeAFile())
                fs->fp = fopen(fs->fname, (char*)&(fs->mode));
            }
        if(fs->fp == NULL)
            return NULL;
        fs->rwstatus = NoPending;
        FSEEK(fs->fp, fs->filepos, SEEK_SET);
        }
    fs->filepos = -1L;
    fs->time = tijdnr++;
    return fs;
    }
/*
Find an existing or create a fresh file handle for a known file name
If the file mode differs from the current file mode,
    reopen the file with the new file mode.
If the file is known but has been closed (e.g. to save file handles),
    open the file with the memorized file mode and go to the memorized position
*/
static fileStatus* search_fp(char* name, LONG mode)
    {
    fileStatus* fs;
    for(fs = fs0; fs; fs = fs->next)
        if(!strcmp(name, fs->fname))
            return preparefp(fs, name, mode);
    return NULL;
    }

static void setStop(fileStatus* fs, char* stopstring)
    {
    if(fs->stop)
#ifdef BMALLLOC
        bfree(fs->stop);
    fs->stop = (char*)bmalloc(strlen(stopstring + 1);
#else
                              free(fs->stop);
    fs->stop = (char*)malloc(strlen(stopstring) + 1);
#endif
    if(fs->stop)
        strcpy(fs->stop, stopstring);
    }

int fil(ppsk PPnode)
    {
    FILE* fp;
    psk kns[4];
    LONG ind;
    int sh;
    psk pnode;
    static fileStatus* fs = NULL;
    char* name;

    static LONG types[] = { CHR,DEC,STRt,0L };
    static LONG whences[] = { SET,CUR,END,0L };
    static LONG modes[] = {
    O('r', 0 , 0),/*open text file for reading                                  */
    O('w', 0 , 0),/*create text file for writing, or trucate to zero length     */
    O('a', 0 , 0),/*append; open text file or create for writing at eof         */
    O('r','b', 0),/*open binary file for reading                                */
    O('w','b', 0),/*create binary file for writing, or trucate to zero length   */
    O('a','b', 0),/*append; open binary file or create for writing at eof       */
    O('r','+', 0),/*open text file for update (reading and writing)             */
    O('w','+', 0),/*create text file for update, or trucate to zero length      */
    O('a','+', 0),/*append; open text file or create for update, writing at eof */
    O('r','+','b'),
    O('r','b','+'),/*open binary file for update (reading and writing)           */
    O('w','+','b'),
    O('w','b','+'),/*create binary file for update, or trucate to zero length    */
    O('a','+','b'),
    O('a','b','+'),/*append;open binary file or create for update, writing at eof*/
    0L };

    static LONG type, numericalvalue, whence;
    union
        {
        LONG l;
        char c[4];
        } mode;

    union
        {
        short s;
        INT32_T i;
        char c[4];
        } snum;

    /*
    Fail if there are more than four arguments or if an argument is non-atomic
    */
    for(ind = 0, pnode = (*PPnode)->RIGHT;
        is_op(pnode);
        pnode = pnode->RIGHT)
        {
        if(is_op(pnode->LEFT) || ind > 2)
            {
            return FALSE;
            }
        kns[ind++] = pnode->LEFT;
        }
    kns[ind++] = pnode;
    for(; ind < 4;)
        kns[ind++] = NULL;

    /*
      FIRST ARGUMENT: File name
      if the current file name is different from the argument,
            reset the current file name
      if the first argument is empty, the current file name must not be NULL
      fil$(name)
      fil$(name,...)
      name field is optional in all fil$ operations
    */
    if(kns[0]->u.obj)
        {
        name = (char*)POBJ(kns[0]);
        if(fs && strcmp(name, fs->fname))
            {
            fs = NULL;
            }
        }
    else
        {
        if(fs)
            name = fs->fname;
        else
            {
            return FALSE;
            }
        }

    /*
      SECOND ARGUMENT: mode, type, whence or TEL
            if the second argument is a mode string,
                    the file handel is found and adapted to the  mode
                    or a new file handel is made
            else
                    file handel is set to current name

            If the second argument is set, fil$ does never read or write!
    */
    if(kns[1] && kns[1]->u.obj)
        {
        /*
        SECOND ARGUMENT:FILE MODE
        fil$(,"r")
        fil$(,"b")
        fil$(,"a")
        etc.
        */
        if((mode.l = someopt(kns[1], modes)) != 0L)
            {
            if(fs)
                fs = preparefp(fs, name, mode.l);
            else
                fs = search_fp(name, mode.l);
            if(fs == NULL)
                {
                if((fs = myfopen(name, (char*)&mode, FALSE)) == NULL)
                    {
                    if(closeAFile())
                        fs = myfopen(name, (char*)&mode, FALSE);
                    }
                if(fs == NULL)
                    {
                    return FALSE;
                    }
                fs->filepos = -1L;
                fs->mode = mode.l;
                fs->type = CHR;
                fs->size = 1;
                fs->number = 1;
                fs->time = tijdnr++;
                fs->rwstatus = NoPending;
                fs->stop = NULL;
                assert(fs->fp != 0);
                }
            assert(fs->fp != 0);
            return TRUE;
            }
        else
            {
            /*
            We do not open a file now, so we should have a file handle in memory.
            */
            if(fs)
                {
                fs = preparefp(fs, name, 0L);
                }
            else
                {
                fs = search_fp(name, 0L);
                }


            if(!fs)
                {
                return FALSE;
                }
            assert(fs->fp != 0);

            /*
            SECOND ARGUMENT:TYPE
            fil$(,CHR)
            fil$(,DEC)
            fil$(,CHR,size)
            fil$(,DEC,size)
            fil$(,CHR,size,number)
            fil$(,DEC,size,number)
            fil$(,STR)        (stop == NULL)
            fil$(,STR,stop)
            */
            if((type = someopt(kns[1], types)) != 0L)
                {
                fs->type = type;
                if(type == STRt)
                    {
                    /*
                      THIRD ARGUMENT: primary stopping character (e.g. "\n")

                      An empty string "" sets stopping string to NULL,
                      (Changed behaviour! Previously default stop was newline!)
                    */
                    if(kns[2] && kns[2]->u.obj)
                        {
                        setStop(fs, (char*)&kns[2]->u.obj);
                        }
                    else
                        {
                        if(fs->stop)
#ifdef BMALLLOC
                            bfree(fs->stop);
                        fs->stop = NULL;
#else
                            free(fs->stop);
                        fs->stop = NULL;
#endif
                        }
                    }
                else
                    {
                    /*
                      THIRD ARGUMENT: a size of elements to read or write
                    */
                    if(kns[2] && kns[2]->u.obj)
                        {
                        if(!INTEGER(kns[2]))
                            {
                            return FALSE;
                            }
                        fs->size = toLong(kns[2]);
                        }
                    else
                        {
                        fs->size = 1;
                        fs->number = 1;
                        }
                    /*
                      FOURTH ARGUMENT: the number of elements to read or write
                    */
                    if(kns[3] && kns[3]->u.obj)
                        {
                        if(!INTEGER(kns[3]))
                            {
                            return FALSE;
                            }
                        fs->number = toLong(kns[3]);
                        }
                    else
                        fs->number = 1;
                    }
                return TRUE;
                }
            /*
            SECOND ARGUMENT:POSITIONING
            fil$(,SET)
            fil$(,END)
            fil$(,CUR)
            fil$(,SET,offset)
            fil$(,END,offset)
            fil$(,CUR,offset)
            */
            else if((whence = someopt(kns[1], whences)) != 0L)
                {
                LONG offset;
                assert(fs->fp != 0);
                fs->time = tijdnr++;
                /*
                  THIRD ARGUMENT: an offset
                */
                if(kns[2] && kns[2]->u.obj)
                    {
                    if(!INTEGER(kns[2]))
                        {
                        return FALSE;
                        }
                    offset = toLong(kns[2]);
                    }
                else
                    offset = 0L;

                if((offset < 0L && whence == SEEK_SET)
                   || (offset > 0L && whence == SEEK_END)
                   || FSEEK(fs->fp, offset, whence == SET ? SEEK_SET
                            : whence == END ? SEEK_END
                            : SEEK_CUR))
                    {
                    deallocateFileStatus(fs);
                    fs = NULL;
                    return FALSE;
                    }
                fs->rwstatus = NoPending;
                return TRUE;
                }
            /*
            SECOND ARGUMENT:TELL POSITION
            fil$(,TEL)
            */
            else if(PLOBJ(kns[1]) == TEL)
                {
                char pos[11];
                sprintf(pos, LONGD, FTELL(fs->fp));
                wipe(*PPnode);
                *PPnode = scopy((const char*)pos);
                return TRUE;
                }
            else
                {
                return FALSE;
                }
            }
        /*
        return FALSE if the second argument is not empty but could not be recognised
        */
        }
    else
        {
        if(fs)
            {
            fs = preparefp(fs, name, 0L);
            }
        else
            {
            fs = search_fp(name, 0L);
            }
        }

    if(!fs)
        {
        return FALSE;
        }
    /*
    READ OR WRITE
    Now we are either going to read or to write
    */

    type = fs->type;
    mode.l = fs->mode;
    fp = fs->fp;

    /*
    THIRD ARGUMENT: the number of elements to read or write
    OR stop characters, depending on type (20081113)
    */

    if(kns[2] && kns[2]->u.obj)
        {
        if(type == STRt)
            {
            setStop(fs, (char*)&kns[2]->u.obj);
            }
        else
            {
            if(!INTEGER(kns[2]))
                {
                return FALSE;
                }
            fs->number = toLong(kns[2]);
            }
        }

    /*
    We allow 1, 2 or 4 bytes to be read/written in one fil$ operation
    These can be distributed over decimal numbers.
    */

    if(type == DEC)
        {
        switch((int)fs->size)
            {
            case 1:
                if(fs->number > 4)
                    fs->number = 4;
                break;
            case 2:
                if(fs->number > 2)
                    fs->number = 2;
                break;
            default:
                fs->size = 4; /*Invalid size declaration adjusted*/
                fs->number = 1;
            }
        }
    fs->time = tijdnr++;
    /*
    FOURTH ARGUMENT:VALUE TO WRITE
    */
    if(kns[3])
        {
        if(mode.c[0] != 'r' || mode.c[1] == '+' || mode.c[2] == '+')
            /*
            WRITE
            */
            {
            if(fs->rwstatus == Reading)
                {
                LONG fpos = FTELL(fs->fp);
                FSEEK(fs->fp, fpos, SEEK_SET);
                }
            fs->rwstatus = Writing;
            if(type == DEC)
                {
                numericalvalue = toLong(kns[3]);
                for(ind = 0; ind < fs->number; ind++)
                    switch((int)fs->size)
                        {
                        case 1:
                            fputc((int)numericalvalue & 0xFF, fs->fp);
                            numericalvalue >>= 8;
                            break;
                        case 2:
                            snum.s = (short)(numericalvalue & 0xFFFF);
                            fwrite(snum.c, 1, 2, fs->fp);
                            numericalvalue >>= 16;
                            break;
                        case 4:
                            snum.i = (INT32_T)(numericalvalue & 0xFFFFFFFF);
                            fwrite(snum.c, 1, 4, fs->fp);
                            assert(fs->number == 1);
                            break;
                        default:
                            fwrite((char*)&numericalvalue, 1, 4, fs->fp);
                            break;
                        }
                }
            else if(type == CHR)
                {
                size_t len, len1, minl;
                len1 = (size_t)(fs->size * fs->number);
                len = strlen((char*)POBJ(kns[3]));
                minl = len1 < len ? (len1 > 0 ? len1 : len) : len;
                if(fwrite(POBJ(kns[3]), 1, minl, fs->fp) == minl)
                    for(; len < len1 && putc(' ', fs->fp) != EOF; len++);
                }
            else /*if(type == STRt)*/
                {
                if(fs->stop
                   && fs->stop[0]
                   )/* stop string also works when writing. */
                    {
                    char* s = (char*)POBJ(kns[3]);
                    while(!strchr(fs->stop, *s))
                        fputc(*s++, fs->fp);
                    }
                else
                    {
                    fputs((char*)POBJ(kns[3]), fs->fp);
                    }
                }
            }
        else
            {
            /*
            Fail if not in write mode
            */
            return FALSE;
            }
        }
    else
        {
        if(mode.c[0] == 'r' || mode.c[1] == '+' || mode.c[2] == '+')
            {
            /*
            READ
            */
#define INPUTBUFFERSIZE 256
            unsigned char buffer[INPUTBUFFERSIZE];
            unsigned char* bbuffer;/* = buffer;*/
            if(fs->rwstatus == Writing)
                {
                fflush(fs->fp);
                fs->rwstatus = NoPending;
                }
            if(feof(fp))
                {
                return FALSE;
                }
            fs->rwstatus = Reading;
            if(type == STRt)
                {
                psk lpkn = NULL;
                psk rpkn = NULL;
                char* conc[2];
                int count = 0;
                LONG pos = FTELL(fp);
                int kar = 0;
                while(count < (INPUTBUFFERSIZE - 1)
                      && (kar = fgetc(fp)) != EOF
                      && (!fs->stop
                          || !strchr(fs->stop, kar)
                          )
                      )
                    {
                    buffer[count++] = (char)kar;
                    }
                if(count < (INPUTBUFFERSIZE - 1))
                    {
                    buffer[count] = '\0';
                    bbuffer = buffer;
                    }
                else
                    {
                    buffer[(INPUTBUFFERSIZE - 1)] = '\0';
                    while((kar = fgetc(fp)) != EOF
                          && (!fs->stop
                              || !strchr(fs->stop, kar)
                              )
                          )
                        count++;
                    if(count >= INPUTBUFFERSIZE)
                        {
                        bbuffer = (unsigned char*)bmalloc((size_t)count + 1);
                        strcpy((char*)bbuffer, (char*)buffer);
                        FSEEK(fp, pos + (INPUTBUFFERSIZE - 1), SEEK_SET);
                        if(fread((char*)bbuffer + (INPUTBUFFERSIZE - 1), 1, count - (INPUTBUFFERSIZE - 1), fs->fp) == 0)
                            {
                            bfree(bbuffer);
                            return FALSE;
                            }
                        if(ferror(fs->fp))
                            {
                            bfree(bbuffer);
                            perror("fread");
                            return FALSE;
                            }
                        if(kar != EOF)
                            fgetc(fp); /* skip stopping character (which is in 'kar') */
                        }
                    else
                        bbuffer = buffer;
                    }
                source = bbuffer;
                lpkn = input(NULL, lpkn, 1, NULL, NULL);
                if(kar == EOF)
                    bbuffer[0] = '\0';
                else
                    {
                    bbuffer[0] = (char)kar;
                    bbuffer[1] = '\0';
                    }
                source = bbuffer;
                rpkn = input(NULL, rpkn, 1, NULL, NULL);
                conc[0] = "(\1.\2)";
                addr[1] = lpkn;
                addr[2] = rpkn;
                conc[1] = NULL;
                *PPnode = vbuildup(*PPnode, (const char**)conc);
                wipe(addr[1]);
                wipe(addr[2]);
                }
            else
                {
                size_t readbytes = fs->size * fs->number;
                if(readbytes >= INPUTBUFFERSIZE)
                    bbuffer = (unsigned char*)bmalloc(readbytes + 1);
                else
                    bbuffer = buffer;
                if((readbytes = fread((char*)bbuffer, (size_t)fs->size, (size_t)fs->number, fs->fp)) == 0
                   && fs->size != 0
                   && fs->number != 0
                   )
                    {
                    return FALSE;
                    }
                if(ferror(fs->fp))
                    {
                    perror("fread");
                    return FALSE;
                    }
                *(bbuffer + (int)readbytes) = 0;
                if(type == DEC)
                    {
                    numericalvalue = 0L;
                    sh = 0;
                    for(ind = 0; ind < fs->number;)
                        {
                        switch((int)fs->size)
                            {
                            case 1:
                                numericalvalue += (LONG)bbuffer[ind++] << sh;
                                sh += 8;
                                continue;
                            case 2:
                                numericalvalue += (LONG)(*(short*)(bbuffer + ind)) << sh;
                                ind += 2;
                                sh += 16;
                                continue;
                            case 4:
                                numericalvalue += (LONG)(*(INT32_T*)(bbuffer + ind)) << sh;
                                ind += 4;
                                sh += 32;
                                continue;
                            default:
                                numericalvalue += *(LONG*)bbuffer;
                                break;
                            }
                        break;
                        }
                    sprintf((char*)bbuffer, LONGD, numericalvalue);
                    }
                source = bbuffer;
                *PPnode = input(NULL, *PPnode, 1, NULL, NULL);
                }
            if(bbuffer != (unsigned char*)&buffer[0])
                bfree(bbuffer);
            return TRUE;
            }
        else
            {
            return FALSE;
            }
        }
    return TRUE;
    }
#endif
#endif


static int allopts(psk pnode, LONG opt[])
    {
    int i;
    while(is_op(pnode))
        {
        if(!allopts(pnode->LEFT, opt))
            return FALSE;
        pnode = pnode->RIGHT;
        }
    for(i = 0; opt[i]; i++)
        if(PLOBJ(pnode) == opt[i])
            return TRUE;
    return FALSE;
    }

static int flush(void)
    {
#ifdef __GNUC__
    return fflush(global_fpo);
#else
#if _BRACMATEMBEDDED
    if(WinFlush)
        WinFlush();
    return 1;
#else
    return 1;
#endif
#endif
    }

int output(ppsk PPnode, void(*how)(psk k))
    {
    FILE* saveFpo;
    psk rightnode, rlnode, rrightnode, rrrightnode;
    static LONG opts[] =
        { APP,BIN,CON,EXT,MEM,LIN,NEW,RAW,TXT,VAP,VIS,WYD,0L };
    if(Op(rightnode = (*PPnode)->RIGHT) == COMMA)
        {
        int wide;
        saveFpo = global_fpo;
        rlnode = rightnode->LEFT;
        rrightnode = rightnode->RIGHT;
        wide = search_opt(rrightnode, WYD);
        if(wide)
            LineLength = WIDELINELENGTH;
#if SHOWWHETHERNEVERVISITED
        vis = search_opt(rrightnode, VIS);
#endif
        hum = !search_opt(rrightnode, LIN);
        listWithName = !search_opt(rrightnode, RAW);
        if(allopts(rrightnode, opts))
            {
            if(search_opt(rrightnode, MEM))
                {
                psk ret;
                telling = 1;
                process = tel;
                global_fpo = NULL;
                (*how)(rlnode);
                ret = (psk)bmalloc(sizeof(ULONG) + telling);
                ret->v.fl = READY | SUCCESS;
                process = glue;
                source = POBJ(ret);
                (*how)(rlnode);
                hum = 1;
                process = myputc;
                wipe(*PPnode);
                *PPnode = ret;
                global_fpo = saveFpo;
#if SHOWWHETHERNEVERVISITED
                vis = FALSE;
#endif
                return TRUE;
                }
            else
                {
                (*how)(rlnode);
                flush();
                addr[2] = rlnode;
                }
            }
        else if(Op(rrightnode) == COMMA
                && !is_op(rrightnode->LEFT)
                && allopts((rrrightnode = rrightnode->RIGHT), opts))
            {
#if !defined NO_FOPEN
            int binmode = ((how == lst) && !search_opt(rrrightnode, TXT)) || search_opt(rrrightnode, BIN);
            fileStatus* fs =
                myfopen((char*)POBJ(rrightnode->LEFT),
                        search_opt(rrrightnode, NEW)
                        ? (binmode
                           ? WRITEBIN
                           : WRITETXT
                           )
                        : (binmode
                           ? APPENDBIN
                           : APPENDTXT
                           )
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                        , TRUE
#endif
                );
            if(fs == NULL)
                {
                errorprintf("cannot open %s\n", POBJ(rrightnode->LEFT));
                global_fpo = saveFpo;
                hum = 1;
#if SHOWWHETHERNEVERVISITED
                vis = FALSE;
#endif
                return FALSE;
                }
            else
                {
                global_fpo = fs->fp;
                (*how)(rlnode);
                deallocateFileStatus(fs);
                global_fpo = saveFpo;
                addr[2] = rlnode;
                }
#else
            hum = 1;
#if SHOWWHETHERNEVERVISITED
            vis = FALSE;
#endif
            return FALSE;
#endif
            }
        else
            {
            (*how)(rightnode);
            flush();
            addr[2] = rightnode;
            }
        *PPnode = dopb(*PPnode, addr[2]);
        if(wide)
            LineLength = NARROWLINELENGTH;
        }
    else
        {
        (*how)(rightnode);
        flush();
        *PPnode = rightbranch(*PPnode);
        }
    hum = 1;
    listWithName = 1;
#if SHOWWHETHERNEVERVISITED
    vis = FALSE;
#endif
    return TRUE;
    }

#if !defined NO_FOPEN
psk fileget(psk rlnode, int intval_i, psk Pnode, int* err, Boolean* GoOn)
    {
    FILE* saveFp;
    fileStatus* fs;
    saveFp = global_fpi;
    fs = myfopen((char*)POBJ(rlnode), (intval_i & OPT_TXT) ? READTXT : READBIN
#if !defined NO_LOW_LEVEL_FILE_HANDLING
                 , TRUE
#endif
    );
    if(fs == NULL)
        {
        global_fpi = saveFp;
        return 0L;
        }
    else
        global_fpi = fs->fp;
    for(;;)
        {
        Pnode = input(global_fpi, Pnode, intval_i, err, GoOn);
        if(!*GoOn || *err)
            break;
        Pnode = eval(Pnode);
        }
    deallocateFileStatus(fs);
    global_fpi = saveFp;
    return Pnode;
    }
#endif
