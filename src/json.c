/*
    Bracmat. Programming language with pattern matching on tree structures.
    Copyright (C) 2002  Bart Jongejan

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
/*
json.c
Convert JSONL file to Bracmat file.
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <limits.h>

#if defined _WIN64
/*Microsoft*/
#define LONG long long
#else
#define LONG long
#endif


#define TRUE 1
#define FALSE 0

extern void putOperatorChar(int c);
extern void putLeafChar(int c);
extern char * putCodePoint(unsigned LONG val,char * s);

typedef enum {nojson,json} jstate;
static jstate (*jsonState)(int kar);

static int Put(char * c);
static int (*xput)(char * c) = Put;

static int rawput(int c)
    {
    putLeafChar(c);
    return TRUE;
    }

static int nrawput(char * c)
    {
    while(*c)
        if(!rawput(*c++))
            return FALSE;
    return TRUE;
    }

#define BUFSIZE 35000

static char * buf;
static char * p;
static char * q;
static int anychar = FALSE;

static int decimals;
static int leadingzeros;

static int Put(char * c)
    {
    return rawput(*c);
    }

static void flush(void)
    {
    if(xput != Put)
        xput("");
    }

static void nxput(char * start,char *end)
    {
    for(;start < end;++start)
        xput(start);
    flush();
    }


static char * ch;

static void startString(void)
    {
    putOperatorChar(' ');
    putOperatorChar('(');
/*  nrawput("String");*/
    putOperatorChar('.');
    }

static void endString(void)
    {
    putOperatorChar(')');
    }

static void startValue(void)
    {
    putOperatorChar(' ');
    putOperatorChar('(');
    }

static void endValue(void)
    {
    putOperatorChar(')');
    }

static void firstNamedValue(void)
    {
    putOperatorChar(' ');
    putOperatorChar('(');
    }

static void nextNamedValue(void)
    {
    putOperatorChar(')');
    putOperatorChar('+');
    putOperatorChar('(');
    }

static void lastNamedValue(void)
    {
    putOperatorChar(')');
    }

static void jcbStartArray(void)/* called when [ has been read */
    {
    flush();
    putOperatorChar(' ');
    putOperatorChar('(');
    /*nrawput("Array");*/
    putOperatorChar(',');
    putOperatorChar('(');
    }

static void jcbEndArray(void)/* called when ] has been read */
    {
    flush();
    putOperatorChar(')');
    putOperatorChar(')');
    }


typedef jstate (*stateFncTp)(int);
stateFncTp stack[100]; /* TODO make variable */
stateFncTp * stackpointer = NULL;
stateFncTp action;

stateFncTp push(stateFncTp arg)
    {
    *++stackpointer = arg;
    return arg;
    }

stateFncTp pop(void)
    {
    return *--stackpointer;
    }


int needed;
unsigned LONG hexvalue;

jstate hexdigits(int arg)
    {
    if     ('0' <= arg && arg <= '9')
        arg -= '0';
    else if('A' <= arg && arg <= 'F')
        arg -= ('A'+10);
    else if('a' <= arg && arg <= 'f')
        arg -= ('a'+10);
    else return nojson;
    hexvalue = (hexvalue << 4) + arg; 
    if(--needed == 0)
        {
        char tmp[22];
        if(putCodePoint(hexvalue,tmp))
            nrawput(tmp);
        action = pop();
        }
    return json;
    }

jstate escape(int arg)
    {
    switch(arg)
        {
        case '"': rawput('\"'); action = pop(); return json;
        case '\\': rawput('\\'); action = pop(); return json;
        case '/': rawput('/'); action = pop(); return json;
        case 'b': rawput('\b'); action = pop(); return json;
        case 'f': rawput('\f'); action = pop(); return json;
        case 'n': rawput('\n'); action = pop(); return json;
        case 'r': rawput('\r'); action = pop(); return json;
        case 't': rawput('\t'); action = pop(); return json;
        case 'u': pop(); needed = 4; hexvalue = 0L; action = push(hexdigits); return json;
        default:
            pop();
        }
    return nojson;
    }

jstate string(int arg)
    {
    switch(arg)
        {
        case '"': endString(); action = pop(); break;
        case '\\': action = push(escape); break;
        default:
            if(arg < ' ')
                return nojson;
            rawput(arg);
        }
    return json;
    }

jstate name(int arg)
    {
    switch(arg)
        {
        case '"': action = pop(); break;
        case '\\': action = push(escape); break;
        default:
            if(arg < ' ')
                return nojson;
            rawput(arg);
        }
    return json;
    }

jstate value(int arg);

jstate commaOrCloseSquareBracket(int arg)
    {
    switch(arg)
        {
        case ']': action = pop(); endValue(); jcbEndArray(); return json;
        case ',': action = push(value); endValue(); startValue(); return json;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            return json;
        default:
            return nojson;
        }
    }

jstate startNamestring(int arg);

jstate commaOrCloseBrace(int arg)
    {
    switch(arg)
        {
        case '}': action = pop(); lastNamedValue(); return json;
        case ',': pop(); action = push(startNamestring); nextNamedValue(); return json;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            return json;
        default:
            return nojson;
        }
    }

jstate colon(int arg)
        {
        switch(arg)
            {
            case ':': pop(); putOperatorChar('.'); push(commaOrCloseBrace); action = push(value); return json;
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                return json;
            default:
                return nojson;
            }
        }

jstate startNamestring(int arg)
    {
    switch(arg)
        {
        case '"': pop(); push(colon); action = push(name); return json;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
             return json;
        default:
            return nojson;
        }
    return nojson;
    }


jstate valueOrCloseSquareBracket(int arg)
    {
    switch(arg)
        {
        case ']': action = pop(); jcbEndArray(); return json;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            return json;
        default:
            pop(); 
            startValue(); 
            push(commaOrCloseSquareBracket);
            action = push(value);
            return value(arg);
        }
    return nojson;
    }

const char * FIXED;

jstate fixed(int arg)
    {
    int next;
    next = *FIXED++;
    if(arg == next)
        {
        if(!*FIXED)
            {
            action = pop();
            }
        rawput(next);
        return json;
        }
    return nojson;
    }

int sign;
int exp;

void aftermath(int zeros)
    {
    if(leadingzeros)
        {
        rawput('0');
        }
    else
        {
        if(zeros < 0)
            {
            nrawput("/1");
            zeros = -zeros;
            }
        while(zeros--)
            rawput('0');
        }
    }

jstate exponentdigits(int arg)
    {
    if('0' <= arg && arg <= '9')
        {
        exp = 10*exp + arg - '0';
        return json;
        }
    aftermath(sign*exp - decimals);
    action = pop();
    return action(arg);
    }

jstate exponent(int arg)
    {
    if('0' <= arg && arg <= '9')
        {
        exp = 10*exp + arg - '0';
        pop();
        action = push(exponentdigits);
        return json;
        }
    return nojson;
    }

jstate plusOrMinusOrDigit(int arg)
    {
    switch(arg)
        {
        case '-':
            sign = -1;
        case '+':
            pop();
            action = push(exponent);
            return json;
        default:
            if('0' <= arg && arg <= '9')
                {
                pop();
                exp = 10*exp + arg - '0';
                action = push(exponentdigits);
                return json;
                }
        }
    return nojson;
    }


jstate decimal(int arg)
    {
    switch(arg)
        {
        case 'e':
        case 'E':
            pop();
            sign = 1;
            exp = 0;
            action = push(plusOrMinusOrDigit);
            return json;
        case '0':
                {
                ++decimals;
                if(!leadingzeros)
                    rawput(arg);
                return json;
                }
        default:
            if('1' <= arg && arg <= '9')
                {
                leadingzeros = FALSE;
                ++decimals;
                rawput(arg);
                return json;
                }
        }
    aftermath(-decimals);
    action = pop();
    return action(arg);
    }

jstate firstdecimal(int arg)
    {
    if('0' == arg)
        {
        pop();
        decimals = 1;
        if(!leadingzeros)
            rawput(arg);
        action = push(decimal);
        return json;
        }
    else if('1' <= arg && arg <= '9')
        {
        pop();
        decimals = 1;
        leadingzeros = FALSE;
        rawput(arg);
        action = push(decimal);
        return json;
        }
    return nojson;
    }

jstate dotOrE(int arg)
    {
    switch(arg)
        {
        case 'e':
        case 'E':
            pop();
            sign = 1;
            exp = 0;
            action = push(plusOrMinusOrDigit);
            return json;
        case '.': pop(); action = push(firstdecimal); return json;
        default: action = pop(); return action(arg);
        }
    }

jstate digitsOrDotOrE(int arg)
    {
    switch(arg)
        {
        case 'e':
        case 'E':
            pop();
            sign = 1;
            exp = 0;
            action = push(plusOrMinusOrDigit);
            return json;
        case '.': pop(); action = push(firstdecimal); return json;
        default:
            if('0' <= arg && arg <= '9')
                {
                rawput(arg);
                return json;
                }
        }
    action = pop();
    return action(arg);
    }

jstate firstdigit(int arg)
    {
    switch(arg)
        {
        case '0': leadingzeros = TRUE;/*rawput(arg);*/ pop(); action = push(dotOrE); return json;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
             return json;
        default:
            if('1' <= arg && arg <= '9')
                {
                leadingzeros = FALSE;
                rawput(arg);
                pop();
                action = push(digitsOrDotOrE);
                return json;
                }
        }
    return nojson;
    }

jstate startNamestringOrCloseBrace(int arg)
    {
    switch(arg)
        {
        case '"': pop(); push(colon); action = push(name); return json;
        case '}': action = pop(); endValue(); return json;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
             return json;
        default:
            return nojson;
        }
    return nojson;
    }

jstate value(int arg)
    {
    switch(arg)
        {
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            return json;
        case '"': pop(); startString(); action = push(string); return json;
        case '[': pop(); action = push(valueOrCloseSquareBracket); jcbStartArray(); return json;
        case '{': pop(); firstNamedValue(); action = push(startNamestringOrCloseBrace); return json;
        case 't': pop(); action = push(fixed); rawput('t'); FIXED = "rue"; return json;
        case 'f': pop(); action = push(fixed); rawput('f'); FIXED = "alse"; return json;
        case 'n': pop(); action = push(fixed); rawput('n'); FIXED = "ull"; return json;
        case '-': pop(); startValue(); action = push(firstdigit); rawput(arg); return json;
        case '0': pop(); startValue(); action = push(dotOrE); leadingzeros = TRUE;/*rawput(arg);*/ return json;
        default:
            if('1' <= arg && arg <= '9')
                {
                rawput(arg);
                pop(); 
                action = push(digitsOrDotOrE); 
                leadingzeros = FALSE;
                return json;
                }
        }
    return nojson;
    }


jstate top(int arg)
    {
    switch(arg)
        {
        case '{': action = push(startNamestringOrCloseBrace); firstNamedValue(); return json;
        case '[': action = push(valueOrCloseSquareBracket); jcbStartArray(); return json;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
             return json;
        default:
            return nojson;
        }
    return nojson;
    }


static int doit(char * arg)
    {
    stackpointer = stack + 0;
    action = top;
    for(;*arg && action;++arg)
        {
        if(action(*arg) == nojson)
            return FALSE;
        }
    for(;*arg;++arg)
        {
        switch(*arg)
            {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                break;
            default:
                return FALSE;
            }
        }
    
    return stackpointer == stack;
    }


void JSONtext(FILE * fpi,char * bron)
    {
    int kar;
    int inc = 0x10000;
    int incs = 1;
    LONG filesize;
    if(fpi)
        {
        if(fpi == stdin)
            {
            filesize = inc - 1;
            }
        else
            {
            fseek(fpi,0,SEEK_END);
            filesize = ftell(fpi);
            rewind(fpi);
            }
        }
    else if(bron)
        {
        filesize = strlen(bron);
        }
    else
        return 0;
    if(filesize > 0)
        {
        char * alltext;
        buf = (char*)malloc(BUFSIZE);
        p = buf;
#if !ALWAYSREPLACEAMP
        q = buf;
#endif
        alltext = fpi ? (char*)malloc(filesize+1) : bron;
        if(buf && alltext)
            {
            jstate Seq = nojson;
            char * p = alltext;
            if(fpi)
                {
                while((kar = getc(fpi)) != EOF)
                    {
                    *p++ = (char)kar;
                    if(p >= alltext + incs * inc)
                        {
                        size_t dif = p - alltext; 
                        ++incs;                            
                        alltext = (char *)realloc(alltext,incs * inc);
                        p = alltext + dif;
                        }
                    }
                }
            else
                {
                while((kar = *bron++) != 0)
                    {
                    *p++ = (char)kar;
                    }
                }
            *p = '\0';
            doit(alltext);
            /*
            if(doit(alltext))
                printf("OKJSON\n");
            else
                printf("FAULTY JSON\n");*/
            }
        if(buf)
            free(buf);
        if(alltext && alltext != bron)
            free(alltext);
        }
    }

void JSONtest(void)
    {
    char * json = strdup("{ \"ABC\" :  [0e+9,true,[false],[null]]  }"); 
    FILE * fp = fopen("jsontest.txt","r");
    if(fp)
        {
        JSONtext(fp,0);
        fclose(fp);
        }
    getchar();
    exit(0);
    }