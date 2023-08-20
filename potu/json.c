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

#include "json.h"
#include "encoding.h"
#include "input.h"
#include "filewrite.h"
#include "charput.h"

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

typedef enum {nojson,json} jstate;

#define BUFSIZE 35000

static int decimals;
static int leadingzeros;

static void startString(void)
    {
    putOperatorChar(' ');
    putOperatorChar('(');
    putOperatorChar('.');
    }

static void endString(void)
    {
    putOperatorChar(')');
    }

static void firstValue(void)
    {
    putOperatorChar(' ');
    putOperatorChar('(');
    }

static void nextValue(void)
    {
    putOperatorChar(')');
    putOperatorChar('+');
    putOperatorChar('(');
    }

static void lastValue(void)
    {
    putOperatorChar(')');
    }

static void startArray(void)/* called when [ has been read */
    {
    putOperatorChar(' ');
    putOperatorChar('(');
    putOperatorChar(',');
    putOperatorChar('(');
    }

static void endArray(void)/* called when ] has been read */
    {
    putOperatorChar(')');
    putOperatorChar(')');
    }

static void startObject(void)/* called when { has been read */
    {
    putOperatorChar(' ');
    putOperatorChar('(');
    putOperatorChar('(');
    }

static void endObject(void)/* called when } has been read */
    {
    putOperatorChar(')');
    putOperatorChar(',');
    putOperatorChar(')');
    }

typedef jstate (*stateFncTp)(int);
static unsigned int stacksiz;
static stateFncTp * theStack;
static stateFncTp * stackpointer;
static stateFncTp action;

static stateFncTp push(stateFncTp arg)
    {
    ++stackpointer;
    if(stackpointer == theStack + stacksiz)
        {
        stateFncTp* newstack;
        unsigned int newsiz = (2 * stacksiz + 1);
        newstack = (stateFncTp *)realloc(theStack,newsiz * sizeof(stateFncTp));
        if(newstack)
            {
            theStack = newstack;
            stackpointer = theStack + stacksiz;
            stacksiz = newsiz;
            }
        else
            {
            errorprintf(
                "memory full (requested block of %d bytes could not be allocated)",
                newsiz * sizeof(stateFncTp));
            exit(1);
            }
        }
    return *stackpointer=(arg);
    }

static stateFncTp popj(void)
    {
    --stackpointer;
    assert(stackpointer >= theStack);
    return *stackpointer;
    }

static int needed;
static unsigned LONG hexvalue;

static jstate hexdigits(int arg)
    {
    if     ('0' <= arg && arg <= '9')
        arg -= '0';
    else if('A' <= arg && arg <= 'F')
        arg -= ('A'-10);
    else if('a' <= arg && arg <= 'f')
        arg -= ('a'-10);
    else return nojson;
    hexvalue = (hexvalue << 4) + arg; 
    if(--needed == 0)
        {
        unsigned char tmp[22];
        if(putCodePoint(hexvalue,tmp))
            {
            unsigned char * c = tmp;
            while(*c)
                putLeafChar(*c++);
            }
        action = popj();
        }
    return json;
    }

static jstate escape(int arg)
    {
    switch(arg)
        {
        case '"': 
            putLeafChar('\"'); break;
        case '\\': 
            putLeafChar('\\'); break;
        case '/': 
            putLeafChar('/'); break;
        case 'b': 
            putLeafChar('\b'); break;
        case 'f': 
            putLeafChar('\f'); break;
        case 'n': 
            putLeafChar('\n'); break;
        case 'r': 
            putLeafChar('\r'); break;
        case 't': 
            putLeafChar('\t'); break;
        case 'u': 
            popj(); needed = 4; hexvalue = 0L; action = push(hexdigits); return json;
        default: 
            action = popj(); return nojson;
        }
    action = popj();
    return json;
    }

static jstate string(int arg)
    {
    switch(arg)
        {
        case '"': 
            endString(); action = popj(); break;
        case '\\': 
            action = push(escape); break;
        default:
            if(0 < arg && arg < ' ')
                switch(arg)
                    {
                    case 8:
                    case 9:
                    case 10:
                    case 12:
                    case 13:
                        /*See http://www.bennadel.com/blog/2576-testing-which-ascii-characters-break-json-javascript-object-notation-parsing.htm*/
                        break;
                    default:
                        return nojson;
                    }
            putLeafChar(arg);
        }
    return json;
    }

static jstate name(int arg)
    {
    if(arg == '"')
        {
        action = popj();
        return json;
        }
    else
        return string(arg);
    }

static jstate value(int arg);

static jstate commaOrCloseSquareBracket(int arg)
    {
    switch(arg)
        {
        case ']': 
            action = popj(); lastValue(); endArray(); return json;
        case ',': 
            action = push(value); lastValue(); firstValue(); return json;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            return json;
        default:
            return nojson;
        }
    }

static jstate startNamestring(int arg);

static jstate commaOrCloseBrace(int arg)
    {
    switch(arg)
        {
        case '}': 
            action = popj(); lastValue(); endObject(); return json;
        case ',': 
            popj(); action = push(startNamestring); nextValue(); return json;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            return json;
        default:
            return nojson;
        }
    }

static jstate colon(int arg)
        {
        switch(arg)
            {
            case ':': 
                popj(); putOperatorChar('.'); push(commaOrCloseBrace); action = push(value);
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                return json;
            default:
                return nojson;
            }
        }

static jstate startNamestring(int arg)
    {
    switch(arg)
        {
        case '"': 
            popj(); push(colon); action = push(name);
        case ' ':
        case '\t':
        case '\r':
        case '\n':
             return json;
        default:
            return nojson;
        }
    }


static jstate valueOrCloseSquareBracket(int arg)
    {
    switch(arg)
        {
        case ']': 
            action = popj(); endArray();
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            return json;
        default:
            popj(); 
            firstValue(); 
            push(commaOrCloseSquareBracket);
            action = push(value);
            return value(arg);
        }
    }

static const char * FIXED;

static jstate fixed(int arg)
    {
    int next;
    next = *FIXED++;
    if(arg == next)
        {
        if(!*FIXED)
            {
            action = popj();
            }
        putLeafChar(next);
        return json;
        }
    return nojson;
    }

static int signj;
static int Nexp;

static void aftermath(int zeros)
    {
    if(leadingzeros)
        {
        putLeafChar('0');
        }
    else
        {
        if(zeros < 0)
            {
            putLeafChar('/');
            putLeafChar('1');
            zeros = -zeros;
            }
        while(zeros--)
            putLeafChar('0');
        }
    }

static jstate exponentdigits(int arg)
    {
    if('0' <= arg && arg <= '9')
        {
        Nexp = 10*Nexp + arg - '0';
        return json;
        }
    aftermath(signj*Nexp - decimals);
    action = popj();
    return action(arg);
    }

static jstate exponent(int arg)
    {
    if('0' <= arg && arg <= '9')
        {
        Nexp = 10*Nexp + arg - '0';
        popj();
        action = push(exponentdigits);
        return json;
        }
    return nojson;
    }

static jstate plusOrMinusOrDigit(int arg)
    {
    switch(arg)
        {
        case '-':
            signj = -1;
        case '+':
            popj();
            action = push(exponent);
            return json;
        default:
            return exponent(arg);
        }
    }

static jstate decimal(int arg)
    {
    switch(arg)
        {
        case 'e':
        case 'E':
            popj();
            signj = 1;
            Nexp = 0;
            action = push(plusOrMinusOrDigit);
            return json;
        case '0':
            ++decimals;
            if(!leadingzeros)
                putLeafChar(arg);
            return json;
        default:
            if('1' <= arg && arg <= '9')
                {
                leadingzeros = FALSE;
                ++decimals;
                putLeafChar(arg);
                return json;
                }
        }
    aftermath(-decimals);
    action = popj();
    return action(arg);
    }

static jstate firstdecimal(int arg)
    {
    if('0' == arg)
        {
        popj();
        decimals = 1;
        if(!leadingzeros)
            putLeafChar(arg);
        action = push(decimal);
        return json;
        }
    else if('1' <= arg && arg <= '9')
        {
        popj();
        decimals = 1;
        leadingzeros = FALSE;
        putLeafChar(arg);
        action = push(decimal);
        return json;
        }
    return nojson;
    }

static jstate dotOrE(int arg)
    {
    switch(arg)
        {
        case 'e':
        case 'E':
            popj();
            signj = 1;
            Nexp = 0;
            action = push(plusOrMinusOrDigit);
            return json;
        case '.': 
            popj(); action = push(firstdecimal); return json;
        default: 
            action = popj(); aftermath(0); return action(arg);
        }
    }

static jstate digitsOrDotOrE(int arg)
    {
    if('0' <= arg && arg <= '9')
        {
        putLeafChar(arg);
        return json;
        }
    else
        return dotOrE(arg);
    }

static jstate firstdigit(int arg)
    {
    decimals = 0;
    switch(arg)
        {
        case '0': 
            popj(); action = push(dotOrE); leadingzeros = TRUE;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
             return json;
        default:
            if('1' <= arg && arg <= '9')
                {
                leadingzeros = FALSE;
                putLeafChar(arg);
                popj();
                action = push(digitsOrDotOrE);
                return json;
                }
        }
    return nojson;
    }

static jstate startNamestringOrCloseBrace(int arg)
    {
    switch(arg)
        {
        case '"':
            popj(); push(colon); action = push(name); return json;
        case '}':
            action = popj(); putLeafChar('0'); lastValue(); endObject(); return json;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
             return json;
        default:
            return nojson;
        }
    }

static jstate value(int arg)
    {
    switch(arg)
        {
        case '"':
            popj(); action = push(string); startString(); return json;
        case '[':
            popj
            (); action = push(valueOrCloseSquareBracket); startArray(); return json;
        case '{':
            popj(); action = push(startNamestringOrCloseBrace); startObject(); firstValue(); return json;
        case 't':
            popj(); action = push(fixed); putLeafChar('t'); FIXED = "rue"; return json;
        case 'f': 
            popj(); action = push(fixed); putLeafChar('f'); FIXED = "alse"; return json;
        case 'n':
            popj(); action = push(fixed); putLeafChar('n'); FIXED = "ull"; return json;
        case '-':
            popj(); action = push(firstdigit); putOperatorChar(arg); return json;
        default:
            return firstdigit(arg);
        }
    }

static jstate top(int arg)
    {
    switch(arg)
        {
        case '{':
            action = push(startNamestringOrCloseBrace); startObject(); firstValue(); return json;
        case '[':
            action = push(valueOrCloseSquareBracket); startArray(); return json;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
             return json;
        default:
            return nojson;
        }
    }

static int doit(char * arg)
    {
    stacksiz = 1;
    theStack = (stateFncTp *)malloc(stacksiz * sizeof(stateFncTp));
    if(theStack)
        {
        int R;
        *theStack = 0;
        stackpointer = theStack + 0;

        action = top;
        for(; *arg && action; ++arg)
            {
            if(action(*arg) == nojson)
                return FALSE;
            }
        for(; *arg; ++arg)
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
        R = stackpointer == theStack;
        free(theStack);
        return R;
        }
    return 0;
    }


int JSONtext(FILE * fpi,char * bron)
    {
    int kar;
    int inc = 0x10000;
    int incs = 1;
    int ok = 1;
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
        char * alltext = fpi ? (char*)malloc(filesize+1) : bron;
        if(alltext)
            {
            if(fpi)
                {
                char * p = alltext;
                while((kar = getc(fpi)) != EOF)
                    {
                    *p++ = (char)kar;
                    if(p >= alltext + incs * inc)
                        {
                        size_t dif = p - alltext; 
                        char* newalltext;
                        ++incs;                            
                        newalltext = (char *)realloc(alltext,incs * inc);
                        if(newalltext)
                            {
                            alltext = newalltext;
                            p = alltext + dif;
                            }
                        else
                            break; /* out of memory! */
                        }
                    }
                *p = '\0';
                }
            ok = doit(alltext);
            if(alltext != bron)
                free(alltext);
            }
        }
    return !ok;
    }
