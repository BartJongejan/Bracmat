#include "quote.h"
#include "nonnodetypes.h"
#include "nodeutil.h"
#include "filewrite.h"

static const char needsquotes[256] = {
    /*
       1 : needsquotes if first character;
       3 : needsquotes always
       4 : needsquotes if \t and \n must be expanded
    */
    0,0,0,0,0,0,0,0,0,4,4,0,0,0,3,3, /* \L \D */
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    3,1,0,1,3,1,3,3,3,3,3,3,3,1,3,1, /* SP ! # $ % & ' ( ) * + , - . / */
    0,0,0,0,0,0,0,0,0,0,3,3,1,3,1,1, /* : < = > ? */
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* @ */
    0,0,0,0,0,0,0,0,0,0,0,1,3,1,3,3, /* [ \ ] ^ _ */
    1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* ` */
    0,0,0,0,0,0,0,0,0,0,0,3,3,3,1,0 };/* { | } ~ */

int quote(unsigned char *strng)
    {
    unsigned char *pstring;
    if (needsquotes[*strng] & 1)
        return TRUE;
    for (pstring = strng; *pstring; pstring++)
        if (needsquotes[*pstring] & 2)
            return TRUE;
        else if (  (needsquotes[*pstring] & 4)
                && lineTooLong(strng)
                )
            return TRUE;
    return FALSE;
    }
