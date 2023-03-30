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
xml.c
Convert XML file to Bracmat file.
XML entities are converted to their UTF-8 equivalents.

Each tag is output to the Bracmat file. Combining opening tags with their
closing tags must be done elsewhere, if needed.

Because the program does not make an attempt at nesting, combining
closing tags with their opening tags, the program can handle SGML and HTML as
well as XML.

Attribute values need not be surrounded in ' ' or " ".
Attribute values need not even be present.
A closing angled parenthesis > is implied in certain circumstances:
<p<span>  is interpreted as <p><span>
attributes can be empty (no =[valuex])
*/

#include "xml.h"
#include "encoding.h"
#include "input.h"

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

typedef enum {notag,tag,endoftag,endoftag_startoftag} estate;
static estate (*tagState)(const unsigned char * pkar);

static int Put(const unsigned char * c);
static int (*xput)(const unsigned char * c) = Put;

static int assumeUTF8 = 1; /* Turned off when a non-UTF-8 sequence is seen. */

static int rawput(int c)
    {
    putLeafChar(c);
    return TRUE;
    }

static int nrawput(const unsigned char * c)
    {
    while(*c)
        if(!rawput(*c++))
            return FALSE;
    return TRUE;
    }

struct lowup
    {
    int  l;
    int u;
    int s; /* NameStartChar */
    };

struct lowup lu[] =
    {     {'-','-',FALSE}
        , {'.','.',FALSE}
        , {'0','9',FALSE}
        , {':',':',TRUE}
        , {'A','Z',TRUE}
        , {'_','_',TRUE}
        , {'a','z',TRUE} 
        , {0xB7,0xB7,FALSE}
        , {0xC0,0xD6,TRUE} 
        , {0xD8,0xF6,TRUE}
        , {0xF8,0x2FF,TRUE}
        , {0x300,0x36F,FALSE}
        , {0x370,0x37D,TRUE}
        , {0x37F,0x1FFF,TRUE}
        , {0x200C,0x200D,TRUE}
        , {0x203F,0x2040,FALSE}
        , {0x2070,0x218F,TRUE}
        , {0x2C00,0x2FEF,TRUE}
        , {0x3001,0xD7FF,TRUE}
        , {0xF900,0xFDCF,TRUE}
        , {0xFDF0,0xFFFD,TRUE}
        , {0x10000,0xEFFFF,TRUE}
        , {0x7FFFFFFF,0x7FFFFFFF,TRUE}
    };

#define BUFSIZE 35000

static unsigned char * bufx;
static unsigned char * glob_p;
static int anychar = FALSE;

static int (*namechar)(int c);

static int NameChar(int c)
    {
    int i;
    for( i = 0
       ; c > lu[i].u
       ; ++i
       )
       ;
    return c >= lu[i].l && (anychar || lu[i].s);        
    }

static int decimalx(int c)
    {
    if('0' <= c && c <= '9')
        return TRUE;
    return FALSE;
    }

static int hex(int c)
    {
    if(  ('0' <= c && c <= '9')
      || ('A' <= c && c <= 'F')
      || ('a' <= c && c <= 'f')
      )
        return TRUE;
    return FALSE;
    }

static int number(int c)
    {
    if(c == 'x' || c == 'X')
        {
        namechar = hex;
        return TRUE;
        }
    if(decimalx(c))
        {
        namechar = decimalx;
        return TRUE;
        }
    return FALSE;
    }

static int entity(int c)
    {
    if(c == '#')
        {
        namechar = number;
        return TRUE;
        }
    else
        {
        anychar = FALSE;
        if(NameChar(c))
            {
            anychar = TRUE;
            namechar = NameChar;
            return TRUE;
            }
        }
    return FALSE;
    }

typedef struct Entity { const char * ent; int code; int morecode; } Entity;
static Entity entities[] = {
        { "AElig", 198, 0 },	/* latin capital letter AE = latin capital ligature AE, U+00C6 ISOlat1 */
        { "AMP", 38, 0 },
        { "Aacute", 193, 0 },	/* latin capital letter A with acute, U+00C1 ISOlat1 */
        { "Abreve", 258, 0 },
        { "Acirc", 194, 0 },	/* latin capital letter A with circumflex, U+00C2 ISOlat1 */
        { "Acy", 1040, 0 },
        { "Afr", 120068, 0 },
        { "Agrave", 192, 0 },	/* latin capital letter A with grave = latin capital letter A grave, U+00C0 ISOlat1 */
        { "Alpha", 913, 0 },	/* greek capital letter alpha, U+0391 */
        { "Amacr", 256, 0 },
        { "And", 10835, 0 },
        { "Aogon", 260, 0 },
        { "Aopf", 120120, 0 },
        { "ApplyFunction", 8289, 0 },
        { "Aring", 197, 0 },	/* latin capital letter A with ring above = latin capital letter A ring, U+00C5 ISOlat1 */
        { "Ascr", 119964, 0 },
        { "Assign", 8788, 0 },
        { "Atilde", 195, 0 },	/* latin capital letter A with tilde, U+00C3 ISOlat1 */
        { "Auml", 196, 0 },	/* latin capital letter A with diaeresis, U+00C4 ISOlat1 */
        { "Backslash", 8726, 0 },
        { "Barv", 10983, 0 },
        { "Barwed", 8966, 0 },
        { "Bcy", 1041, 0 },
        { "Because", 8757, 0 },
        { "Bernoullis", 8492, 0 },
        { "Beta", 914, 0 },	/* greek capital letter beta, U+0392 */
        { "Bfr", 120069, 0 },
        { "Bopf", 120121, 0 },
        { "Breve", 728, 0 },
        { "Bscr", 8492, 0 },
        { "Bumpeq", 8782, 0 },
        { "CHcy", 1063, 0 },
        { "COPY", 169, 0 },
        { "Cacute", 262, 0 },
        { "Cap", 8914, 0 },
        { "CapitalDifferentialD", 8517, 0 },
        { "Cayleys", 8493, 0 },
        { "Ccaron", 268, 0 },
        { "Ccedil", 199, 0 },	/* latin capital letter C with cedilla, U+00C7 ISOlat1 */
        { "Ccirc", 264, 0 },
        { "Cconint", 8752, 0 },
        { "Cdot", 266, 0 },
        { "Cedilla", 184, 0 },
        { "CenterDot", 183, 0 },
        { "Cfr", 8493, 0 },
        { "Chi", 935, 0 },	/* greek capital letter chi, U+03A7 */
        { "CircleDot", 8857, 0 },
        { "CircleMinus", 8854, 0 },
        { "CirclePlus", 8853, 0 },
        { "CircleTimes", 8855, 0 },
        { "ClockwiseContourIntegral", 8754, 0 },
        { "CloseCurlyDoubleQuote", 8221, 0 },
        { "CloseCurlyQuote", 8217, 0 },
        { "Colon", 8759, 0 },
        { "Colone", 10868, 0 },
        { "Congruent", 8801, 0 },
        { "Conint", 8751, 0 },
        { "ContourIntegral", 8750, 0 },
        { "Copf", 8450, 0 },
        { "Coproduct", 8720, 0 },
        { "CounterClockwiseContourIntegral", 8755, 0 },
        { "Cross", 10799, 0 },
        { "Cscr", 119966, 0 },
        { "Cup", 8915, 0 },
        { "CupCap", 8781, 0 },
        { "DD", 8517, 0 },
        { "DDotrahd", 10513, 0 },
        { "DJcy", 1026, 0 },
        { "DScy", 1029, 0 },
        { "DZcy", 1039, 0 },
        { "Dagger", 8225, 0 },	/* double dagger, U+2021 ISOpub */
        { "Darr", 8609, 0 },
        { "Dashv", 10980, 0 },
        { "Dcaron", 270, 0 },
        { "Dcy", 1044, 0 },
        { "Del", 8711, 0 },
        { "Delta", 916, 0 },	/* greek capital letter delta, U+0394 ISOgrk3 */
        { "Dfr", 120071, 0 },
        { "DiacriticalAcute", 180, 0 },
        { "DiacriticalDot", 729, 0 },
        { "DiacriticalDoubleAcute", 733, 0 },
        { "DiacriticalGrave", 96, 0 },
        { "DiacriticalTilde", 732, 0 },
        { "Diamond", 8900, 0 },
        { "DifferentialD", 8518, 0 },
        { "Dopf", 120123, 0 },
        { "Dot", 168, 0 },
        { "DotDot", 8412, 0 },
        { "DotEqual", 8784, 0 },
        { "DoubleContourIntegral", 8751, 0 },
        { "DoubleDot", 168, 0 },
        { "DoubleDownArrow", 8659, 0 },
        { "DoubleLeftArrow", 8656, 0 },
        { "DoubleLeftRightArrow", 8660, 0 },
        { "DoubleLeftTee", 10980, 0 },
        { "DoubleLongLeftArrow", 10232, 0 },
        { "DoubleLongLeftRightArrow", 10234, 0 },
        { "DoubleLongRightArrow", 10233, 0 },
        { "DoubleRightArrow", 8658, 0 },
        { "DoubleRightTee", 8872, 0 },
        { "DoubleUpArrow", 8657, 0 },
        { "DoubleUpDownArrow", 8661, 0 },
        { "DoubleVerticalBar", 8741, 0 },
        { "DownArrow", 8595, 0 },
        { "DownArrowBar", 10515, 0 },
        { "DownArrowUpArrow", 8693, 0 },
        { "DownBreve", 785, 0 },
        { "DownLeftRightVector", 10576, 0 },
        { "DownLeftTeeVector", 10590, 0 },
        { "DownLeftVector", 8637, 0 },
        { "DownLeftVectorBar", 10582, 0 },
        { "DownRightTeeVector", 10591, 0 },
        { "DownRightVector", 8641, 0 },
        { "DownRightVectorBar", 10583, 0 },
        { "DownTee", 8868, 0 },
        { "DownTeeArrow", 8615, 0 },
        { "Downarrow", 8659, 0 },
        { "Dscr", 119967, 0 },
        { "Dstrok", 272, 0 },
        { "ENG", 330, 0 },
        { "ETH", 208, 0 },	/* latin capital letter ETH, U+00D0 ISOlat1 */
        { "Eacute", 201, 0 },	/* latin capital letter E with acute, U+00C9 ISOlat1 */
        { "Ecaron", 282, 0 },
        { "Ecirc", 202, 0 },	/* latin capital letter E with circumflex, U+00CA ISOlat1 */
        { "Ecy", 1069, 0 },
        { "Edot", 278, 0 },
        { "Efr", 120072, 0 },
        { "Egrave", 200, 0 },	/* latin capital letter E with grave, U+00C8 ISOlat1 */
        { "Element", 8712, 0 },
        { "Emacr", 274, 0 },
        { "EmptySmallSquare", 9723, 0 },
        { "EmptyVerySmallSquare", 9643, 0 },
        { "Eogon", 280, 0 },
        { "Eopf", 120124, 0 },
        { "Epsilon", 917, 0 },	/* greek capital letter epsilon, U+0395 */
        { "Equal", 10869, 0 },
        { "EqualTilde", 8770, 0 },
        { "Equilibrium", 8652, 0 },
        { "Escr", 8496, 0 },
        { "Esim", 10867, 0 },
        { "Eta", 919, 0 },	/* greek capital letter eta, U+0397 */
        { "Euml", 203, 0 },	/* latin capital letter E with diaeresis, U+00CB ISOlat1 */
        { "Exists", 8707, 0 },
        { "ExponentialE", 8519, 0 },
        { "Fcy", 1060, 0 },
        { "Ffr", 120073, 0 },
        { "FilledSmallSquare", 9724, 0 },
        { "FilledVerySmallSquare", 9642, 0 },
        { "Fopf", 120125, 0 },
        { "ForAll", 8704, 0 },
        { "Fouriertrf", 8497, 0 },
        { "Fscr", 8497, 0 },
        { "GJcy", 1027, 0 },
        { "GT", 62, 0 },
        { "Gamma", 915, 0 },	/* greek capital letter gamma, U+0393 ISOgrk3 */
        { "Gammad", 988, 0 },
        { "Gbreve", 286, 0 },
        { "Gcedil", 290, 0 },
        { "Gcirc", 284, 0 },
        { "Gcy", 1043, 0 },
        { "Gdot", 288, 0 },
        { "Gfr", 120074, 0 },
        { "Gg", 8921, 0 },
        { "Gopf", 120126, 0 },
        { "GreaterEqual", 8805, 0 },
        { "GreaterEqualLess", 8923, 0 },
        { "GreaterFullEqual", 8807, 0 },
        { "GreaterGreater", 10914, 0 },
        { "GreaterLess", 8823, 0 },
        { "GreaterSlantEqual", 10878, 0 },
        { "GreaterTilde", 8819, 0 },
        { "Gscr", 119970, 0 },
        { "Gt", 8811, 0 },
        { "HARDcy", 1066, 0 },
        { "Hacek", 711, 0 },
        { "Hat", 94, 0 },
        { "Hcirc", 292, 0 },
        { "Hfr", 8460, 0 },
        { "HilbertSpace", 8459, 0 },
        { "Hopf", 8461, 0 },
        { "HorizontalLine", 9472, 0 },
        { "Hscr", 8459, 0 },
        { "Hstrok", 294, 0 },
        { "HumpDownHump", 8782, 0 },
        { "HumpEqual", 8783, 0 },
        { "IEcy", 1045, 0 },
        { "IJlig", 306, 0 },
        { "IOcy", 1025, 0 },
        { "Iacute", 205, 0 },	/* latin capital letter I with acute, U+00CD ISOlat1 */
        { "Icirc", 206, 0 },	/* latin capital letter I with circumflex, U+00CE ISOlat1 */
        { "Icy", 1048, 0 },
        { "Idot", 304, 0 },
        { "Ifr", 8465, 0 },
        { "Igrave", 204, 0 },	/* latin capital letter I with grave, U+00CC ISOlat1 */
        { "Im", 8465, 0 },
        { "Imacr", 298, 0 },
        { "ImaginaryI", 8520, 0 },
        { "Implies", 8658, 0 },
        { "Int", 8748, 0 },
        { "Integral", 8747, 0 },
        { "Intersection", 8898, 0 },
        { "InvisibleComma", 8291, 0 },
        { "InvisibleTimes", 8290, 0 },
        { "Iogon", 302, 0 },
        { "Iopf", 120128, 0 },
        { "Iota", 921, 0 },	/* greek capital letter iota, U+0399 */
        { "Iscr", 8464, 0 },
        { "Itilde", 296, 0 },
        { "Iukcy", 1030, 0 },
        { "Iuml", 207, 0 },	/* latin capital letter I with diaeresis, U+00CF ISOlat1 */
        { "Jcirc", 308, 0 },
        { "Jcy", 1049, 0 },
        { "Jfr", 120077, 0 },
        { "Jopf", 120129, 0 },
        { "Jscr", 119973, 0 },
        { "Jsercy", 1032, 0 },
        { "Jukcy", 1028, 0 },
        { "KHcy", 1061, 0 },
        { "KJcy", 1036, 0 },
        { "Kappa", 922, 0 },	/* greek capital letter kappa, U+039A */
        { "Kcedil", 310, 0 },
        { "Kcy", 1050, 0 },
        { "Kfr", 120078, 0 },
        { "Kopf", 120130, 0 },
        { "Kscr", 119974, 0 },
        { "LJcy", 1033, 0 },
        { "LT", 60, 0 },
        { "Lacute", 313, 0 },
        { "Lambda", 923, 0 },	/* greek capital letter lambda, U+039B ISOgrk3 */
        { "Lang", 10218, 0 },
        { "Laplacetrf", 8466, 0 },
        { "Larr", 8606, 0 },
        { "Lcaron", 317, 0 },
        { "Lcedil", 315, 0 },
        { "Lcy", 1051, 0 },
        { "LeftAngleBracket", 10216, 0 },	/* U+27E8⟨ */
        { "LeftArrow", 8592, 0 },
        { "LeftArrowBar", 8676, 0 },
        { "LeftArrowRightArrow", 8646, 0 },
        { "LeftCeiling", 8968, 0 },
        { "LeftDoubleBracket", 10214, 0 },
        { "LeftDownTeeVector", 10593, 0 },
        { "LeftDownVector", 8643, 0 },
        { "LeftDownVectorBar", 10585, 0 },
        { "LeftFloor", 8970, 0 },
        { "LeftRightArrow", 8596, 0 },
        { "LeftRightVector", 10574, 0 },
        { "LeftTee", 8867, 0 },
        { "LeftTeeArrow", 8612, 0 },
        { "LeftTeeVector", 10586, 0 },
        { "LeftTriangle", 8882, 0 },
        { "LeftTriangleBar", 10703, 0 },
        { "LeftTriangleEqual", 8884, 0 },
        { "LeftUpDownVector", 10577, 0 },
        { "LeftUpTeeVector", 10592, 0 },
        { "LeftUpVector", 8639, 0 },
        { "LeftUpVectorBar", 10584, 0 },
        { "LeftVector", 8636, 0 },
        { "LeftVectorBar", 10578, 0 },
        { "Leftarrow", 8656, 0 },
        { "Leftrightarrow", 8660, 0 },
        { "LessEqualGreater", 8922, 0 },
        { "LessFullEqual", 8806, 0 },
        { "LessGreater", 8822, 0 },
        { "LessLess", 10913, 0 },
        { "LessSlantEqual", 10877, 0 },
        { "LessTilde", 8818, 0 },
        { "Lfr", 120079, 0 },
        { "Ll", 8920, 0 },
        { "Lleftarrow", 8666, 0 },
        { "Lmidot", 319, 0 },
        { "LongLeftArrow", 10229, 0 },
        { "LongLeftRightArrow", 10231, 0 },
        { "LongRightArrow", 10230, 0 },
        { "Longleftarrow", 10232, 0 },
        { "Longleftrightarrow", 10234, 0 },
        { "Longrightarrow", 10233, 0 },
        { "Lopf", 120131, 0 },
        { "LowerLeftArrow", 8601, 0 },
        { "LowerRightArrow", 8600, 0 },
        { "Lscr", 8466, 0 },
        { "Lsh", 8624, 0 },
        { "Lstrok", 321, 0 },
        { "Lt", 8810, 0 },
        { "Map", 10501, 0 },
        { "Mcy", 1052, 0 },
        { "MediumSpace", 8287, 0 },
        { "Mellintrf", 8499, 0 },
        { "Mfr", 120080, 0 },
        { "MinusPlus", 8723, 0 },
        { "Mopf", 120132, 0 },
        { "Mscr", 8499, 0 },
        { "Mu", 924, 0 },	/* greek capital letter mu, U+039C */
        { "NJcy", 1034, 0 },
        { "Nacute", 323, 0 },
        { "Ncaron", 327, 0 },
        { "Ncedil", 325, 0 },
        { "Ncy", 1053, 0 },
        { "NegativeMediumSpace", 8203, 0 },
        { "NegativeThickSpace", 8203, 0 },
        { "NegativeThinSpace", 8203, 0 },
        { "NegativeVeryThinSpace", 8203, 0 },
        { "NestedGreaterGreater", 8811, 0 },
        { "NestedLessLess", 8810, 0 },
        { "NewLine", 10, 0 },	/* U+A
                                */
        { "Nfr", 120081, 0 },
        { "NoBreak", 8288, 0 },
        { "NonBreakingSpace", 160, 0 },
        { "Nopf", 8469, 0 },
        { "Not", 10988, 0 },
        { "NotCongruent", 8802, 0 },
        { "NotCupCap", 8813, 0 },
        { "NotDoubleVerticalBar", 8742, 0 },
        { "NotElement", 8713, 0 },
        { "NotEqual", 8800, 0 },
        { "NotEqualTilde", 8770, 824 },
        { "NotExists", 8708, 0 },
        { "NotGreater", 8815, 0 },
        { "NotGreaterEqual", 8817, 0 },
        { "NotGreaterFullEqual", 8807, 824 },
        { "NotGreaterGreater", 8811, 824 },
        { "NotGreaterLess", 8825, 0 },
        { "NotGreaterSlantEqual", 10878, 824 },
        { "NotGreaterTilde", 8821, 0 },
        { "NotHumpDownHump", 8782, 824 },
        { "NotHumpEqual", 8783, 824 },
        { "NotLeftTriangle", 8938, 0 },
        { "NotLeftTriangleBar", 10703, 824 },
        { "NotLeftTriangleEqual", 8940, 0 },
        { "NotLess", 8814, 0 },
        { "NotLessEqual", 8816, 0 },
        { "NotLessGreater", 8824, 0 },
        { "NotLessLess", 8810, 824 },
        { "NotLessSlantEqual", 10877, 824 },
        { "NotLessTilde", 8820, 0 },
        { "NotNestedGreaterGreater", 10914, 824 },
        { "NotNestedLessLess", 10913, 824 },
        { "NotPrecedes", 8832, 0 },
        { "NotPrecedesEqual", 10927, 824 },
        { "NotPrecedesSlantEqual", 8928, 0 },
        { "NotReverseElement", 8716, 0 },
        { "NotRightTriangle", 8939, 0 },
        { "NotRightTriangleBar", 10704, 824 },
        { "NotRightTriangleEqual", 8941, 0 },
        { "NotSquareSubset", 8847, 824 },
        { "NotSquareSubsetEqual", 8930, 0 },
        { "NotSquareSuperset", 8848, 824 },
        { "NotSquareSupersetEqual", 8931, 0 },
        { "NotSubset", 8834, 8402 },
        { "NotSubsetEqual", 8840, 0 },
        { "NotSucceeds", 8833, 0 },
        { "NotSucceedsEqual", 10928, 824 },
        { "NotSucceedsSlantEqual", 8929, 0 },
        { "NotSucceedsTilde", 8831, 824 },
        { "NotSuperset", 8835, 8402 },
        { "NotSupersetEqual", 8841, 0 },
        { "NotTilde", 8769, 0 },
        { "NotTildeEqual", 8772, 0 },
        { "NotTildeFullEqual", 8775, 0 },
        { "NotTildeTilde", 8777, 0 },
        { "NotVerticalBar", 8740, 0 },
        { "Nscr", 119977, 0 },
        { "Ntilde", 209, 0 },	/* latin capital letter N with tilde, U+00D1 ISOlat1 */
        { "Nu", 925, 0 },	/* greek capital letter nu, U+039D */
        { "OElig", 338, 0 },	/* latin capital ligature OE, U+0152 ISOlat2 */
        { "Oacute", 211, 0 },	/* latin capital letter O with acute, U+00D3 ISOlat1 */
        { "Ocirc", 212, 0 },	/* latin capital letter O with circumflex, U+00D4 ISOlat1 */
        { "Ocy", 1054, 0 },
        { "Odblac", 336, 0 },
        { "Ofr", 120082, 0 },
        { "Ograve", 210, 0 },	/* latin capital letter O with grave, U+00D2 ISOlat1 */
        { "Omacr", 332, 0 },
        { "Omega", 937, 0 },	/* greek capital letter omega, U+03A9 ISOgrk3 */
        { "Omicron", 927, 0 },	/* greek capital letter omicron, U+039F */
        { "Oopf", 120134, 0 },
        { "OpenCurlyDoubleQuote", 8220, 0 },
        { "OpenCurlyQuote", 8216, 0 },
        { "Or", 10836, 0 },
        { "Oscr", 119978, 0 },
        { "Oslash", 216, 0 },	/* latin capital letter O with stroke = latin capital letter O slash, U+00D8 ISOlat1 */
        { "Otilde", 213, 0 },	/* latin capital letter O with tilde, U+00D5 ISOlat1 */
        { "Otimes", 10807, 0 },
        { "Ouml", 214, 0 },	/* latin capital letter O with diaeresis, U+00D6 ISOlat1 */
        { "OverBar", 8254, 0 },
        { "OverBrace", 9182, 0 },
        { "OverBracket", 9140, 0 },
        { "OverParenthesis", 9180, 0 },
        { "PartialD", 8706, 0 },
        { "Pcy", 1055, 0 },
        { "Pfr", 120083, 0 },
        { "Phi", 934, 0 },	/* greek capital letter phi, U+03A6 ISOgrk3 */
        { "Pi", 928, 0 },	/* greek capital letter pi, U+03A0 ISOgrk3 */
        { "PlusMinus", 177, 0 },
        { "Poincareplane", 8460, 0 },
        { "Popf", 8473, 0 },
        { "Pr", 10939, 0 },
        { "Precedes", 8826, 0 },
        { "PrecedesEqual", 10927, 0 },
        { "PrecedesSlantEqual", 8828, 0 },
        { "PrecedesTilde", 8830, 0 },
        { "Prime", 8243, 0 },	/* double prime = seconds = inches, U+2033 ISOtech */
        { "Product", 8719, 0 },
        { "Proportion", 8759, 0 },
        { "Proportional", 8733, 0 },
        { "Pscr", 119979, 0 },
        { "Psi", 936, 0 },	/* greek capital letter psi, U+03A8 ISOgrk3 */
        { "QUOT", 34, 0 },
        { "Qfr", 120084, 0 },
        { "Qopf", 8474, 0 },
        { "Qscr", 119980, 0 },
        { "RBarr", 10512, 0 },
        { "REG", 174, 0 },
        { "Racute", 340, 0 },
        { "Rang", 10219, 0 },
        { "Rarr", 8608, 0 },
        { "Rarrtl", 10518, 0 },
        { "Rcaron", 344, 0 },
        { "Rcedil", 342, 0 },
        { "Rcy", 1056, 0 },
        { "Re", 8476, 0 },
        { "ReverseElement", 8715, 0 },
        { "ReverseEquilibrium", 8651, 0 },
        { "ReverseUpEquilibrium", 10607, 0 },
        { "Rfr", 8476, 0 },
        { "Rho", 929, 0 },	/* greek capital letter rho, U+03A1 */
        { "RightAngleBracket", 10217, 0 },	/* U+27E9⟩ */
        { "RightArrow", 8594, 0 },
        { "RightArrowBar", 8677, 0 },
        { "RightArrowLeftArrow", 8644, 0 },
        { "RightCeiling", 8969, 0 },
        { "RightDoubleBracket", 10215, 0 },
        { "RightDownTeeVector", 10589, 0 },
        { "RightDownVector", 8642, 0 },
        { "RightDownVectorBar", 10581, 0 },
        { "RightFloor", 8971, 0 },
        { "RightTee", 8866, 0 },
        { "RightTeeArrow", 8614, 0 },
        { "RightTeeVector", 10587, 0 },
        { "RightTriangle", 8883, 0 },
        { "RightTriangleBar", 10704, 0 },
        { "RightTriangleEqual", 8885, 0 },
        { "RightUpDownVector", 10575, 0 },
        { "RightUpTeeVector", 10588, 0 },
        { "RightUpVector", 8638, 0 },
        { "RightUpVectorBar", 10580, 0 },
        { "RightVector", 8640, 0 },
        { "RightVectorBar", 10579, 0 },
        { "Rightarrow", 8658, 0 },
        { "Ropf", 8477, 0 },
        { "RoundImplies", 10608, 0 },
        { "Rrightarrow", 8667, 0 },
        { "Rscr", 8475, 0 },
        { "Rsh", 8625, 0 },
        { "RuleDelayed", 10740, 0 },
        { "SHCHcy", 1065, 0 },
        { "SHcy", 1064, 0 },
        { "SOFTcy", 1068, 0 },
        { "Sacute", 346, 0 },
        { "Sc", 10940, 0 },
        { "Scaron", 352, 0 },	/* latin capital letter S with caron, U+0160 ISOlat2 */
        { "Scedil", 350, 0 },
        { "Scirc", 348, 0 },
        { "Scy", 1057, 0 },
        { "Sfr", 120086, 0 },
        { "ShortDownArrow", 8595, 0 },
        { "ShortLeftArrow", 8592, 0 },
        { "ShortRightArrow", 8594, 0 },
        { "ShortUpArrow", 8593, 0 },
        { "Sigma", 931, 0 },	/* greek capital letter sigma, U+03A3 ISOgrk3 */
        { "SmallCircle", 8728, 0 },
        { "Sopf", 120138, 0 },
        { "Sqrt", 8730, 0 },
        { "Square", 9633, 0 },
        { "SquareIntersection", 8851, 0 },
        { "SquareSubset", 8847, 0 },
        { "SquareSubsetEqual", 8849, 0 },
        { "SquareSuperset", 8848, 0 },
        { "SquareSupersetEqual", 8850, 0 },
        { "SquareUnion", 8852, 0 },
        { "Sscr", 119982, 0 },
        { "Star", 8902, 0 },
        { "Sub", 8912, 0 },
        { "Subset", 8912, 0 },
        { "SubsetEqual", 8838, 0 },
        { "Succeeds", 8827, 0 },
        { "SucceedsEqual", 10928, 0 },
        { "SucceedsSlantEqual", 8829, 0 },
        { "SucceedsTilde", 8831, 0 },
        { "SuchThat", 8715, 0 },
        { "Sum", 8721, 0 },
        { "Sup", 8913, 0 },
        { "Superset", 8835, 0 },
        { "SupersetEqual", 8839, 0 },
        { "Supset", 8913, 0 },
        { "THORN", 222, 0 },	/* latin capital letter THORN, U+00DE ISOlat1 */
        { "TRADE", 8482, 0 },
        { "TSHcy", 1035, 0 },
        { "TScy", 1062, 0 },
        { "Tab", 9, 0 },	/* U+9	 */
        { "Tau", 932, 0 },	/* greek capital letter tau, U+03A4 */
        { "Tcaron", 356, 0 },
        { "Tcedil", 354, 0 },
        { "Tcy", 1058, 0 },
        { "Tfr", 120087, 0 },
        { "Therefore", 8756, 0 },
        { "Theta", 920, 0 },	/* greek capital letter theta, U+0398 ISOgrk3 */
        { "ThickSpace", 8287, 8202 },
        { "ThinSpace", 8201, 0 },
        { "Tilde", 8764, 0 },
        { "TildeEqual", 8771, 0 },
        { "TildeFullEqual", 8773, 0 },
        { "TildeTilde", 8776, 0 },
        { "Topf", 120139, 0 },
        { "TripleDot", 8411, 0 },
        { "Tscr", 119983, 0 },
        { "Tstrok", 358, 0 },
        { "Uacute", 218, 0 },	/* latin capital letter U with acute, U+00DA ISOlat1 */
        { "Uarr", 8607, 0 },
        { "Uarrocir", 10569, 0 },
        { "Ubrcy", 1038, 0 },
        { "Ubreve", 364, 0 },
        { "Ucirc", 219, 0 },	/* latin capital letter U with circumflex, U+00DB ISOlat1 */
        { "Ucy", 1059, 0 },
        { "Udblac", 368, 0 },
        { "Ufr", 120088, 0 },
        { "Ugrave", 217, 0 },	/* latin capital letter U with grave, U+00D9 ISOlat1 */
        { "Umacr", 362, 0 },
        { "UnderBar", 95, 0 },
        { "UnderBrace", 9183, 0 },
        { "UnderBracket", 9141, 0 },
        { "UnderParenthesis", 9181, 0 },
        { "Union", 8899, 0 },
        { "UnionPlus", 8846, 0 },
        { "Uogon", 370, 0 },
        { "Uopf", 120140, 0 },
        { "UpArrow", 8593, 0 },
        { "UpArrowBar", 10514, 0 },
        { "UpArrowDownArrow", 8645, 0 },
        { "UpDownArrow", 8597, 0 },
        { "UpEquilibrium", 10606, 0 },
        { "UpTee", 8869, 0 },
        { "UpTeeArrow", 8613, 0 },
        { "Uparrow", 8657, 0 },
        { "Updownarrow", 8661, 0 },
        { "UpperLeftArrow", 8598, 0 },
        { "UpperRightArrow", 8599, 0 },
        { "Upsi", 978, 0 },
        { "Upsilon", 933, 0 },	/* greek capital letter upsilon, U+03A5 ISOgrk3 */
        { "Uring", 366, 0 },
        { "Uscr", 119984, 0 },
        { "Utilde", 360, 0 },
        { "Uuml", 220, 0 },	/* latin capital letter U with diaeresis, U+00DC ISOlat1 */
        { "VDash", 8875, 0 },
        { "Vbar", 10987, 0 },
        { "Vcy", 1042, 0 },
        { "Vdash", 8873, 0 },
        { "Vdashl", 10982, 0 },
        { "Vee", 8897, 0 },
        { "Verbar", 8214, 0 },
        { "Vert", 8214, 0 },
        { "VerticalBar", 8739, 0 },
        { "VerticalLine", 124, 0 },
        { "VerticalSeparator", 10072, 0 },
        { "VerticalTilde", 8768, 0 },
        { "VeryThinSpace", 8202, 0 },
        { "Vfr", 120089, 0 },
        { "Vopf", 120141, 0 },
        { "Vscr", 119985, 0 },
        { "Vvdash", 8874, 0 },
        { "Wcirc", 372, 0 },
        { "Wedge", 8896, 0 },
        { "Wfr", 120090, 0 },
        { "Wopf", 120142, 0 },
        { "Wscr", 119986, 0 },
        { "Xfr", 120091, 0 },
        { "Xi", 926, 0 },	/* greek capital letter xi, U+039E ISOgrk3 */
        { "Xopf", 120143, 0 },
        { "Xscr", 119987, 0 },
        { "YAcy", 1071, 0 },
        { "YIcy", 1031, 0 },
        { "YUcy", 1070, 0 },
        { "Yacute", 221, 0 },	/* latin capital letter Y with acute, U+00DD ISOlat1 */
        { "Ycirc", 374, 0 },
        { "Ycy", 1067, 0 },
        { "Yfr", 120092, 0 },
        { "Yopf", 120144, 0 },
        { "Yscr", 119988, 0 },
        { "Yuml", 376, 0 },	/* latin capital letter Y with diaeresis, U+0178 ISOlat2 */
        { "ZHcy", 1046, 0 },
        { "Zacute", 377, 0 },
        { "Zcaron", 381, 0 },
        { "Zcy", 1047, 0 },
        { "Zdot", 379, 0 },
        { "ZeroWidthSpace", 8203, 0 },
        { "Zeta", 918, 0 },	/* greek capital letter zeta, U+0396 */
        { "Zfr", 8488, 0 },
        { "Zopf", 8484, 0 },
        { "Zscr", 119989, 0 },
        { "aacute", 225, 0 },	/* latin small letter a with acute, U+00E1 ISOlat1 */
        { "abreve", 259, 0 },
        { "ac", 8766, 0 },
        { "acE", 8766, 819 },
        { "acd", 8767, 0 },
        { "acirc", 226, 0 },	/* latin small letter a with circumflex, U+00E2 ISOlat1 */
        { "acute", 180, 0 },	/* acute accent = spacing acute, U+00B4 ISOdia */
        { "acy", 1072, 0 },
        { "aelig", 230, 0 },	/* latin small letter ae = latin small ligature ae, U+00E6 ISOlat1 */
        { "af", 8289, 0 },
        { "afr", 120094, 0 },
        { "agrave", 224, 0 },	/* latin small letter a with grave = latin small letter a grave, U+00E0 ISOlat1 */
        { "alefsym", 8501, 0 },	/* alef symbol = first transfinite cardinal, U+2135 NEW */
        { "aleph", 8501, 0 },
        { "alpha", 945, 0 },	/* greek small letter alpha, U+03B1 ISOgrk3 */
        { "amacr", 257, 0 },
        { "amalg", 10815, 0 },
        { "amp", 38, 0 },	/* ampersand, U+0026 ISOnum */
        { "and", 8743, 0 },	/* logical and = wedge, U+2227 ISOtech */
        { "andand", 10837, 0 },
        { "andd", 10844, 0 },
        { "andslope", 10840, 0 },
        { "andv", 10842, 0 },
        { "ang", 8736, 0 },	/* angle, U+2220 ISOamso */
        { "ange", 10660, 0 },
        { "angle", 8736, 0 },
        { "angmsd", 8737, 0 },
        { "angmsdaa", 10664, 0 },
        { "angmsdab", 10665, 0 },
        { "angmsdac", 10666, 0 },
        { "angmsdad", 10667, 0 },
        { "angmsdae", 10668, 0 },
        { "angmsdaf", 10669, 0 },
        { "angmsdag", 10670, 0 },
        { "angmsdah", 10671, 0 },
        { "angrt", 8735, 0 },
        { "angrtvb", 8894, 0 },
        { "angrtvbd", 10653, 0 },
        { "angsph", 8738, 0 },
        { "angst", 197, 0 },
        { "angzarr", 9084, 0 },
        { "aogon", 261, 0 },
        { "aopf", 120146, 0 },
        { "ap", 8776, 0 },
        { "apE", 10864, 0 },
        { "apacir", 10863, 0 },
        { "ape", 8778, 0 },
        { "apid", 8779, 0 },
        { "apos", 39, 0 },
        { "approx", 8776, 0 },
        { "approxeq", 8778, 0 },
        { "aring", 229, 0 },	/* latin small letter a with ring above = latin small letter a ring, U+00E5 ISOlat1 */
        { "ascr", 119990, 0 },
        { "ast", 42, 0 },
        { "asymp", 8776, 0 },	/* almost equal to = asymptotic to, U+2248 ISOamsr */
        { "asympeq", 8781, 0 },
        { "atilde", 227, 0 },	/* latin small letter a with tilde, U+00E3 ISOlat1 */
        { "auml", 228, 0 },	/* latin small letter a with diaeresis, U+00E4 ISOlat1 */
        { "awconint", 8755, 0 },
        { "awint", 10769, 0 },
        { "bNot", 10989, 0 },
        { "backcong", 8780, 0 },
        { "backepsilon", 1014, 0 },
        { "backprime", 8245, 0 },
        { "backsim", 8765, 0 },
        { "backsimeq", 8909, 0 },
        { "barvee", 8893, 0 },
        { "barwed", 8965, 0 },
        { "barwedge", 8965, 0 },
        { "bbrk", 9141, 0 },
        { "bbrktbrk", 9142, 0 },
        { "bcong", 8780, 0 },
        { "bcy", 1073, 0 },
        { "bdquo", 8222, 0 },	/* double low-9 quotation mark, U+201E NEW */
        { "becaus", 8757, 0 },
        { "because", 8757, 0 },
        { "bemptyv", 10672, 0 },
        { "bepsi", 1014, 0 },
        { "bernou", 8492, 0 },
        { "beta", 946, 0 },	/* greek small letter beta, U+03B2 ISOgrk3 */
        { "beth", 8502, 0 },
        { "between", 8812, 0 },
        { "bfr", 120095, 0 },
        { "bigcap", 8898, 0 },
        { "bigcirc", 9711, 0 },
        { "bigcup", 8899, 0 },
        { "bigodot", 10752, 0 },
        { "bigoplus", 10753, 0 },
        { "bigotimes", 10754, 0 },
        { "bigsqcup", 10758, 0 },
        { "bigstar", 9733, 0 },
        { "bigtriangledown", 9661, 0 },
        { "bigtriangleup", 9651, 0 },
        { "biguplus", 10756, 0 },
        { "bigvee", 8897, 0 },
        { "bigwedge", 8896, 0 },
        { "bkarow", 10509, 0 },
        { "blacklozenge", 10731, 0 },
        { "blacksquare", 9642, 0 },
        { "blacktriangle", 9652, 0 },
        { "blacktriangledown", 9662, 0 },
        { "blacktriangleleft", 9666, 0 },
        { "blacktriangleright", 9656, 0 },
        { "blank", 9251, 0 },
        { "blk12", 9618, 0 },
        { "blk14", 9617, 0 },
        { "blk34", 9619, 0 },
        { "block", 9608, 0 },
        { "bne", 61, 8421 },
        { "bnequiv", 8801, 8421 },
        { "bnot", 8976, 0 },
        { "bopf", 120147, 0 },
        { "bot", 8869, 0 },
        { "bottom", 8869, 0 },
        { "bowtie", 8904, 0 },
        { "boxDL", 9559, 0 },
        { "boxDR", 9556, 0 },
        { "boxDl", 9558, 0 },
        { "boxDr", 9555, 0 },
        { "boxH", 9552, 0 },
        { "boxHD", 9574, 0 },
        { "boxHU", 9577, 0 },
        { "boxHd", 9572, 0 },
        { "boxHu", 9575, 0 },
        { "boxUL", 9565, 0 },
        { "boxUR", 9562, 0 },
        { "boxUl", 9564, 0 },
        { "boxUr", 9561, 0 },
        { "boxV", 9553, 0 },
        { "boxVH", 9580, 0 },
        { "boxVL", 9571, 0 },
        { "boxVR", 9568, 0 },
        { "boxVh", 9579, 0 },
        { "boxVl", 9570, 0 },
        { "boxVr", 9567, 0 },
        { "boxbox", 10697, 0 },
        { "boxdL", 9557, 0 },
        { "boxdR", 9554, 0 },
        { "boxdl", 9488, 0 },
        { "boxdr", 9484, 0 },
        { "boxh", 9472, 0 },
        { "boxhD", 9573, 0 },
        { "boxhU", 9576, 0 },
        { "boxhd", 9516, 0 },
        { "boxhu", 9524, 0 },
        { "boxminus", 8863, 0 },
        { "boxplus", 8862, 0 },
        { "boxtimes", 8864, 0 },
        { "boxuL", 9563, 0 },
        { "boxuR", 9560, 0 },
        { "boxul", 9496, 0 },
        { "boxur", 9492, 0 },
        { "boxv", 9474, 0 },
        { "boxvH", 9578, 0 },
        { "boxvL", 9569, 0 },
        { "boxvR", 9566, 0 },
        { "boxvh", 9532, 0 },
        { "boxvl", 9508, 0 },
        { "boxvr", 9500, 0 },
        { "bprime", 8245, 0 },
        { "breve", 728, 0 },
        { "brvbar", 166, 0 },	/* broken bar = broken vertical bar, U+00A6 ISOnum */
        { "bscr", 119991, 0 },
        { "bsemi", 8271, 0 },
        { "bsim", 8765, 0 },
        { "bsime", 8909, 0 },
        { "bsol", 92, 0 },
        { "bsolb", 10693, 0 },
        { "bsolhsub", 10184, 0 },
        { "bull", 8226, 0 },	/* bullet = black small circle, U+2022 ISOpub */
        { "bullet", 8226, 0 },
        { "bump", 8782, 0 },
        { "bumpE", 10926, 0 },
        { "bumpe", 8783, 0 },
        { "bumpeq", 8783, 0 },
        { "cacute", 263, 0 },
        { "cap", 8745, 0 },	/* intersection = cap, U+2229 ISOtech */
        { "capand", 10820, 0 },
        { "capbrcup", 10825, 0 },
        { "capcap", 10827, 0 },
        { "capcup", 10823, 0 },
        { "capdot", 10816, 0 },
        { "caps", 8745, 65024 },
        { "caret", 8257, 0 },
        { "caron", 711, 0 },
        { "ccaps", 10829, 0 },
        { "ccaron", 269, 0 },
        { "ccedil", 231, 0 },	/* latin small letter c with cedilla, U+00E7 ISOlat1 */
        { "ccirc", 265, 0 },
        { "ccups", 10828, 0 },
        { "ccupssm", 10832, 0 },
        { "cdot", 267, 0 },
        { "cedil", 184, 0 },	/* cedilla = spacing cedilla, U+00B8 ISOdia */
        { "cemptyv", 10674, 0 },
        { "cent", 162, 0 },	/* cent sign, U+00A2 ISOnum */
        { "centerdot", 183, 0 },
        { "cfr", 120096, 0 },
        { "chcy", 1095, 0 },
        { "check", 10003, 0 },
        { "checkmark", 10003, 0 },
        { "chi", 967, 0 },	/* greek small letter chi, U+03C7 ISOgrk3 */
        { "cir", 9675, 0 },
        { "cirE", 10691, 0 },
        { "circ", 710, 0 },	/* modifier letter circumflex accent, U+02C6 ISOpub */
        { "circeq", 8791, 0 },
        { "circlearrowleft", 8634, 0 },
        { "circlearrowright", 8635, 0 },
        { "circledR", 174, 0 },
        { "circledS", 9416, 0 },
        { "circledast", 8859, 0 },
        { "circledcirc", 8858, 0 },
        { "circleddash", 8861, 0 },
        { "cire", 8791, 0 },
        { "cirfnint", 10768, 0 },
        { "cirmid", 10991, 0 },
        { "cirscir", 10690, 0 },
        { "clubs", 9827, 0 },	/* black club suit = shamrock, U+2663 ISOpub */
        { "clubsuit", 9827, 0 },
        { "colon", 58, 0 },
        { "colone", 8788, 0 },
        { "coloneq", 8788, 0 },
        { "comma", 44, 0 },
        { "commat", 64, 0 },
        { "comp", 8705, 0 },
        { "compfn", 8728, 0 },
        { "complement", 8705, 0 },
        { "complexes", 8450, 0 },
        { "cong", 8773, 0 },	/* approximately equal to, U+2245 ISOtech */
        { "congdot", 10861, 0 },
        { "conint", 8750, 0 },
        { "copf", 120148, 0 },
        { "coprod", 8720, 0 },
        { "copy", 169, 0 },	/* copyright sign, U+00A9 ISOnum */
        { "copysr", 8471, 0 },
        { "crarr", 8629, 0 },	/* downwards arrow with corner leftwards = carriage return, U+21B5 NEW */
        { "cross", 10007, 0 },
        { "cscr", 119992, 0 },
        { "csub", 10959, 0 },
        { "csube", 10961, 0 },
        { "csup", 10960, 0 },
        { "csupe", 10962, 0 },
        { "ctdot", 8943, 0 },
        { "cudarrl", 10552, 0 },
        { "cudarrr", 10549, 0 },
        { "cuepr", 8926, 0 },
        { "cuesc", 8927, 0 },
        { "cularr", 8630, 0 },
        { "cularrp", 10557, 0 },
        { "cup", 8746, 0 },	/* union = cup, U+222A ISOtech */
        { "cupbrcap", 10824, 0 },
        { "cupcap", 10822, 0 },
        { "cupcup", 10826, 0 },
        { "cupdot", 8845, 0 },
        { "cupor", 10821, 0 },
        { "cups", 8746, 65024 },
        { "curarr", 8631, 0 },
        { "curarrm", 10556, 0 },
        { "curlyeqprec", 8926, 0 },
        { "curlyeqsucc", 8927, 0 },
        { "curlyvee", 8910, 0 },
        { "curlywedge", 8911, 0 },
        { "curren", 164, 0 },	/* currency sign, U+00A4 ISOnum */
        { "curvearrowleft", 8630, 0 },
        { "curvearrowright", 8631, 0 },
        { "cuvee", 8910, 0 },
        { "cuwed", 8911, 0 },
        { "cwconint", 8754, 0 },
        { "cwint", 8753, 0 },
        { "cylcty", 9005, 0 },
        { "dArr", 8659, 0 },	/* downwards double arrow, U+21D3 ISOamsa */
        { "dHar", 10597, 0 },
        { "dagger", 8224, 0 },	/* dagger, U+2020 ISOpub */
        { "daleth", 8504, 0 },
        { "darr", 8595, 0 },	/* downwards arrow, U+2193 ISOnum */
        { "dash", 8208, 0 },
        { "dashv", 8867, 0 },
        { "dbkarow", 10511, 0 },
        { "dblac", 733, 0 },
        { "dcaron", 271, 0 },
        { "dcy", 1076, 0 },
        { "dd", 8518, 0 },
        { "ddagger", 8225, 0 },
        { "ddarr", 8650, 0 },
        { "ddotseq", 10871, 0 },
        { "deg", 176, 0 },	/* degree sign, U+00B0 ISOnum */
        { "delta", 948, 0 },	/* greek small letter delta, U+03B4 ISOgrk3 */
        { "demptyv", 10673, 0 },
        { "dfisht", 10623, 0 },
        { "dfr", 120097, 0 },
        { "dharl", 8643, 0 },
        { "dharr", 8642, 0 },
        { "diam", 8900, 0 },
        { "diamond", 8900, 0 },
        { "diamondsuit", 9830, 0 },
        { "diams", 9830, 0 },	/* black diamond suit, U+2666 ISOpub */
        { "die", 168, 0 },
        { "digamma", 989, 0 },
        { "disin", 8946, 0 },
        { "div", 247, 0 },
        { "divide", 247, 0 },	/* division sign, U+00F7 ISOnum */
        { "divideontimes", 8903, 0 },
        { "divonx", 8903, 0 },
        { "djcy", 1106, 0 },
        { "dlcorn", 8990, 0 },
        { "dlcrop", 8973, 0 },
        { "dollar", 36, 0 },
        { "dopf", 120149, 0 },
        { "dot", 729, 0 },
        { "doteq", 8784, 0 },
        { "doteqdot", 8785, 0 },
        { "dotminus", 8760, 0 },
        { "dotplus", 8724, 0 },
        { "dotsquare", 8865, 0 },
        { "doublebarwedge", 8966, 0 },
        { "downarrow", 8595, 0 },
        { "downdownarrows", 8650, 0 },
        { "downharpoonleft", 8643, 0 },
        { "downharpoonright", 8642, 0 },
        { "drbkarow", 10512, 0 },
        { "drcorn", 8991, 0 },
        { "drcrop", 8972, 0 },
        { "dscr", 119993, 0 },
        { "dscy", 1109, 0 },
        { "dsol", 10742, 0 },
        { "dstrok", 273, 0 },
        { "dtdot", 8945, 0 },
        { "dtri", 9663, 0 },
        { "dtrif", 9662, 0 },
        { "duarr", 8693, 0 },
        { "duhar", 10607, 0 },
        { "dwangle", 10662, 0 },
        { "dzcy", 1119, 0 },
        { "dzigrarr", 10239, 0 },
        { "eDDot", 10871, 0 },
        { "eDot", 8785, 0 },
        { "eacute", 233, 0 },	/* latin small letter e with acute, U+00E9 ISOlat1 */
        { "easter", 10862, 0 },
        { "ecaron", 283, 0 },
        { "ecir", 8790, 0 },
        { "ecirc", 234, 0 },	/* latin small letter e with circumflex, U+00EA ISOlat1 */
        { "ecolon", 8789, 0 },
        { "ecy", 1101, 0 },
        { "edot", 279, 0 },
        { "ee", 8519, 0 },
        { "efDot", 8786, 0 },
        { "efr", 120098, 0 },
        { "eg", 10906, 0 },
        { "egrave", 232, 0 },	/* latin small letter e with grave, U+00E8 ISOlat1 */
        { "egs", 10902, 0 },
        { "egsdot", 10904, 0 },
        { "el", 10905, 0 },
        { "elinters", 9191, 0 },
        { "ell", 8467, 0 },
        { "els", 10901, 0 },
        { "elsdot", 10903, 0 },
        { "emacr", 275, 0 },
        { "empty", 8709, 0 },	/* empty set = null set = diameter, U+2205 ISOamso */
        { "emptyset", 8709, 0 },
        { "emptyv", 8709, 0 },
        { "emsp", 8195, 0 },	/* em space, U+2003 ISOpub */
        { "emsp13", 8196, 0 },
        { "emsp14", 8197, 0 },
        { "eng", 331, 0 },
        { "ensp", 8194, 0 },	/* en space, U+2002 ISOpub */
        { "eogon", 281, 0 },
        { "eopf", 120150, 0 },
        { "epar", 8917, 0 },
        { "eparsl", 10723, 0 },
        { "eplus", 10865, 0 },
        { "epsi", 949, 0 },
        { "epsilon", 949, 0 },	/* greek small letter epsilon, U+03B5 ISOgrk3 */
        { "epsiv", 1013, 0 },
        { "eqcirc", 8790, 0 },
        { "eqcolon", 8789, 0 },
        { "eqsim", 8770, 0 },
        { "eqslantgtr", 10902, 0 },
        { "eqslantless", 10901, 0 },
        { "equals", 61, 0 },
        { "equest", 8799, 0 },
        { "equiv", 8801, 0 },	/* identical to, U+2261 ISOtech */
        { "equivDD", 10872, 0 },
        { "eqvparsl", 10725, 0 },
        { "erDot", 8787, 0 },
        { "erarr", 10609, 0 },
        { "escr", 8495, 0 },
        { "esdot", 8784, 0 },
        { "esim", 8770, 0 },
        { "eta", 951, 0 },	/* greek small letter eta, U+03B7 ISOgrk3 */
        { "eth", 240, 0 },	/* latin small letter eth, U+00F0 ISOlat1 */
        { "euml", 235, 0 },	/* latin small letter e with diaeresis, U+00EB ISOlat1 */
        { "euro", 8364, 0 },	/* euro sign, U+20AC NEW */
        { "excl", 33, 0 },
        { "exist", 8707, 0 },	/* there exists, U+2203 ISOtech */
        { "expectation", 8496, 0 },
        { "exponentiale", 8519, 0 },
        { "fallingdotseq", 8786, 0 },
        { "fcy", 1092, 0 },
        { "female", 9792, 0 },
        { "ffilig", 64259, 0 },
        { "fflig", 64256, 0 },
        { "ffllig", 64260, 0 },
        { "ffr", 120099, 0 },
        { "filig", 64257, 0 },
        { "fjlig", 102, 106 },
        { "flat", 9837, 0 },
        { "fllig", 64258, 0 },
        { "fltns", 9649, 0 },
        { "fnof", 402, 0 },	/* latin small f with hook = function = florin, U+0192 ISOtech */
        { "fopf", 120151, 0 },
        { "forall", 8704, 0 },	/* for all, U+2200 ISOtech */
        { "fork", 8916, 0 },
        { "forkv", 10969, 0 },
        { "fpartint", 10765, 0 },
        { "frac12", 189, 0 },	/* vulgar fraction one half = fraction one half, U+00BD ISOnum */
        { "frac13", 8531, 0 },
        { "frac14", 188, 0 },	/* vulgar fraction one quarter = fraction one quarter, U+00BC ISOnum */
        { "frac15", 8533, 0 },
        { "frac16", 8537, 0 },
        { "frac18", 8539, 0 },
        { "frac23", 8532, 0 },
        { "frac25", 8534, 0 },
        { "frac34", 190, 0 },	/* vulgar fraction three quarters = fraction three quarters, U+00BE ISOnum */
        { "frac35", 8535, 0 },
        { "frac38", 8540, 0 },
        { "frac45", 8536, 0 },
        { "frac56", 8538, 0 },
        { "frac58", 8541, 0 },
        { "frac78", 8542, 0 },
        { "frasl", 8260, 0 },	/* fraction slash, U+2044 NEW */
        { "frown", 8994, 0 },
        { "fscr", 119995, 0 },
        { "gE", 8807, 0 },
        { "gEl", 10892, 0 },
        { "gacute", 501, 0 },
        { "gamma", 947, 0 },	/* greek small letter gamma, U+03B3 ISOgrk3 */
        { "gammad", 989, 0 },
        { "gap", 10886, 0 },
        { "gbreve", 287, 0 },
        { "gcirc", 285, 0 },
        { "gcy", 1075, 0 },
        { "gdot", 289, 0 },
        { "ge", 8805, 0 },	/* greater-than or equal to, U+2265 ISOtech */
        { "gel", 8923, 0 },
        { "geq", 8805, 0 },
        { "geqq", 8807, 0 },
        { "geqslant", 10878, 0 },
        { "ges", 10878, 0 },
        { "gescc", 10921, 0 },
        { "gesdot", 10880, 0 },
        { "gesdoto", 10882, 0 },
        { "gesdotol", 10884, 0 },
        { "gesl", 8923, 65024 },
        { "gesles", 10900, 0 },
        { "gfr", 120100, 0 },
        { "gg", 8811, 0 },
        { "ggg", 8921, 0 },
        { "gimel", 8503, 0 },
        { "gjcy", 1107, 0 },
        { "gl", 8823, 0 },
        { "glE", 10898, 0 },
        { "gla", 10917, 0 },
        { "glj", 10916, 0 },
        { "gnE", 8809, 0 },
        { "gnap", 10890, 0 },
        { "gnapprox", 10890, 0 },
        { "gne", 10888, 0 },
        { "gneq", 10888, 0 },
        { "gneqq", 8809, 0 },
        { "gnsim", 8935, 0 },
        { "gopf", 120152, 0 },
        { "grave", 96, 0 },
        { "gscr", 8458, 0 },
        { "gsim", 8819, 0 },
        { "gsime", 10894, 0 },
        { "gsiml", 10896, 0 },
        { "gt", 62, 0 },	/* greater-than sign, U+003E ISOnum */
        { "gtcc", 10919, 0 },
        { "gtcir", 10874, 0 },
        { "gtdot", 8919, 0 },
        { "gtlPar", 10645, 0 },
        { "gtquest", 10876, 0 },
        { "gtrapprox", 10886, 0 },
        { "gtrarr", 10616, 0 },
        { "gtrdot", 8919, 0 },
        { "gtreqless", 8923, 0 },
        { "gtreqqless", 10892, 0 },
        { "gtrless", 8823, 0 },
        { "gtrsim", 8819, 0 },
        { "gvertneqq", 8809, 65024 },
        { "gvnE", 8809, 65024 },
        { "hArr", 8660, 0 },	/* left right double arrow, U+21D4 ISOamsa */
        { "hairsp", 8202, 0 },
        { "half", 189, 0 },
        { "hamilt", 8459, 0 },
        { "hardcy", 1098, 0 },
        { "harr", 8596, 0 },	/* left right arrow, U+2194 ISOamsa */
        { "harrcir", 10568, 0 },
        { "harrw", 8621, 0 },
        { "hbar", 8463, 0 },
        { "hcirc", 293, 0 },
        { "hearts", 9829, 0 },	/* black heart suit = valentine, U+2665 ISOpub */
        { "heartsuit", 9829, 0 },
        { "hellip", 8230, 0 },	/* horizontal ellipsis = three dot leader, U+2026 ISOpub */
        { "hercon", 8889, 0 },
        { "hfr", 120101, 0 },
        { "hksearow", 10533, 0 },
        { "hkswarow", 10534, 0 },
        { "hoarr", 8703, 0 },
        { "homtht", 8763, 0 },
        { "hookleftarrow", 8617, 0 },
        { "hookrightarrow", 8618, 0 },
        { "hopf", 120153, 0 },
        { "horbar", 8213, 0 },
        { "hscr", 119997, 0 },
        { "hslash", 8463, 0 },
        { "hstrok", 295, 0 },
        { "hybull", 8259, 0 },
        { "hyphen", 8208, 0 },
        { "iacute", 237, 0 },	/* latin small letter i with acute, U+00ED ISOlat1 */
        { "ic", 8291, 0 },
        { "icirc", 238, 0 },	/* latin small letter i with circumflex, U+00EE ISOlat1 */
        { "icy", 1080, 0 },
        { "iecy", 1077, 0 },
        { "iexcl", 161, 0 },	/* inverted exclamation mark, U+00A1 ISOnum */
        { "iff", 8660, 0 },
        { "ifr", 120102, 0 },
        { "igrave", 236, 0 },	/* latin small letter i with grave, U+00EC ISOlat1 */
        { "ii", 8520, 0 },
        { "iiiint", 10764, 0 },
        { "iiint", 8749, 0 },
        { "iinfin", 10716, 0 },
        { "iiota", 8489, 0 },
        { "ijlig", 307, 0 },
        { "imacr", 299, 0 },
        { "image", 8465, 0 },	/* blackletter capital I = imaginary part, U+2111 ISOamso */
        { "imagline", 8464, 0 },
        { "imagpart", 8465, 0 },
        { "imath", 305, 0 },
        { "imof", 8887, 0 },
        { "imped", 437, 0 },
        { "in", 8712, 0 },
        { "incare", 8453, 0 },
        { "infin", 8734, 0 },	/* infinity, U+221E ISOtech */
        { "infintie", 10717, 0 },
        { "inodot", 305, 0 },
        { "int", 8747, 0 },	/* integral, U+222B ISOtech */
        { "intcal", 8890, 0 },
        { "integers", 8484, 0 },
        { "intercal", 8890, 0 },
        { "intlarhk", 10775, 0 },
        { "intprod", 10812, 0 },
        { "iocy", 1105, 0 },
        { "iogon", 303, 0 },
        { "iopf", 120154, 0 },
        { "iota", 953, 0 },	/* greek small letter iota, U+03B9 ISOgrk3 */
        { "iprod", 10812, 0 },
        { "iquest", 191, 0 },	/* inverted question mark = turned question mark, U+00BF ISOnum */
        { "iscr", 119998, 0 },
        { "isin", 8712, 0 },	/* element of, U+2208 ISOtech */
        { "isinE", 8953, 0 },
        { "isindot", 8949, 0 },
        { "isins", 8948, 0 },
        { "isinsv", 8947, 0 },
        { "isinv", 8712, 0 },
        { "it", 8290, 0 },
        { "itilde", 297, 0 },
        { "iukcy", 1110, 0 },
        { "iuml", 239, 0 },	/* latin small letter i with diaeresis, U+00EF ISOlat1 */
        { "jcirc", 309, 0 },
        { "jcy", 1081, 0 },
        { "jfr", 120103, 0 },
        { "jmath", 567, 0 },
        { "jopf", 120155, 0 },
        { "jscr", 119999, 0 },
        { "jsercy", 1112, 0 },
        { "jukcy", 1108, 0 },
        { "kappa", 954, 0 },	/* greek small letter kappa, U+03BA ISOgrk3 */
        { "kappav", 1008, 0 },
        { "kcedil", 311, 0 },
        { "kcy", 1082, 0 },
        { "kfr", 120104, 0 },
        { "kgreen", 312, 0 },
        { "khcy", 1093, 0 },
        { "kjcy", 1116, 0 },
        { "kopf", 120156, 0 },
        { "kscr", 120000, 0 },
        { "lAarr", 8666, 0 },
        { "lArr", 8656, 0 },	/* leftwards double arrow, U+21D0 ISOtech */
        { "lAtail", 10523, 0 },
        { "lBarr", 10510, 0 },
        { "lE", 8806, 0 },
        { "lEg", 10891, 0 },
        { "lHar", 10594, 0 },
        { "lacute", 314, 0 },
        { "laemptyv", 10676, 0 },
        { "lagran", 8466, 0 },
        { "lambda", 955, 0 },	/* greek small letter lambda, U+03BB ISOgrk3 */
        { "lang", 9001, 0 },	/* left-pointing angle bracket = bra, U+2329 ISOtech */
        { "langd", 10641, 0 },
        { "langle", 10216, 0 },	/* U+27E8⟨ */
        { "lap", 10885, 0 },
        { "laquo", 171, 0 },	/* left-pointing double angle quotation mark = left pointing guillemet, U+00AB ISOnum */
        { "larr", 8592, 0 },	/* leftwards arrow, U+2190 ISOnum */
        { "larrb", 8676, 0 },
        { "larrbfs", 10527, 0 },
        { "larrfs", 10525, 0 },
        { "larrhk", 8617, 0 },
        { "larrlp", 8619, 0 },
        { "larrpl", 10553, 0 },
        { "larrsim", 10611, 0 },
        { "larrtl", 8610, 0 },
        { "lat", 10923, 0 },
        { "latail", 10521, 0 },
        { "late", 10925, 0 },
        { "lates", 10925, 65024 },
        { "lbarr", 10508, 0 },
        { "lbbrk", 10098, 0 },
        { "lbrace", 123, 0 },
        { "lbrack", 91, 0 },
        { "lbrke", 10635, 0 },
        { "lbrksld", 10639, 0 },
        { "lbrkslu", 10637, 0 },
        { "lcaron", 318, 0 },
        { "lcedil", 316, 0 },
        { "lceil", 8968, 0 },	/* left ceiling = apl upstile, U+2308 ISOamsc */
        { "lcub", 123, 0 },
        { "lcy", 1083, 0 },
        { "ldca", 10550, 0 },
        { "ldquo", 8220, 0 },	/* left double quotation mark, U+201C ISOnum */
        { "ldquor", 8222, 0 },
        { "ldrdhar", 10599, 0 },
        { "ldrushar", 10571, 0 },
        { "ldsh", 8626, 0 },
        { "le", 8804, 0 },	/* less-than or equal to, U+2264 ISOtech */
        { "leftarrow", 8592, 0 },
        { "leftarrowtail", 8610, 0 },
        { "leftharpoondown", 8637, 0 },
        { "leftharpoonup", 8636, 0 },
        { "leftleftarrows", 8647, 0 },
        { "leftrightarrow", 8596, 0 },
        { "leftrightarrows", 8646, 0 },
        { "leftrightharpoons", 8651, 0 },
        { "leftrightsquigarrow", 8621, 0 },
        { "leftthreetimes", 8907, 0 },
        { "leg", 8922, 0 },
        { "leq", 8804, 0 },
        { "leqq", 8806, 0 },
        { "leqslant", 10877, 0 },
        { "les", 10877, 0 },
        { "lescc", 10920, 0 },
        { "lesdot", 10879, 0 },
        { "lesdoto", 10881, 0 },
        { "lesdotor", 10883, 0 },
        { "lesg", 8922, 65024 },
        { "lesges", 10899, 0 },
        { "lessapprox", 10885, 0 },
        { "lessdot", 8918, 0 },
        { "lesseqgtr", 8922, 0 },
        { "lesseqqgtr", 10891, 0 },
        { "lessgtr", 8822, 0 },
        { "lesssim", 8818, 0 },
        { "lfisht", 10620, 0 },
        { "lfloor", 8970, 0 },	/* left floor = apl downstile, U+230A ISOamsc */
        { "lfr", 120105, 0 },
        { "lg", 8822, 0 },
        { "lgE", 10897, 0 },
        { "lhard", 8637, 0 },
        { "lharu", 8636, 0 },
        { "lharul", 10602, 0 },
        { "lhblk", 9604, 0 },
        { "ljcy", 1113, 0 },
        { "ll", 8810, 0 },
        { "llarr", 8647, 0 },
        { "llcorner", 8990, 0 },
        { "llhard", 10603, 0 },
        { "lltri", 9722, 0 },
        { "lmidot", 320, 0 },
        { "lmoust", 9136, 0 },
        { "lmoustache", 9136, 0 },
        { "lnE", 8808, 0 },
        { "lnap", 10889, 0 },
        { "lnapprox", 10889, 0 },
        { "lne", 10887, 0 },
        { "lneq", 10887, 0 },
        { "lneqq", 8808, 0 },
        { "lnsim", 8934, 0 },
        { "loang", 10220, 0 },
        { "loarr", 8701, 0 },
        { "lobrk", 10214, 0 },
        { "longleftarrow", 10229, 0 },
        { "longleftrightarrow", 10231, 0 },
        { "longmapsto", 10236, 0 },
        { "longrightarrow", 10230, 0 },
        { "looparrowleft", 8619, 0 },
        { "looparrowright", 8620, 0 },
        { "lopar", 10629, 0 },
        { "lopf", 120157, 0 },
        { "loplus", 10797, 0 },
        { "lotimes", 10804, 0 },
        { "lowast", 8727, 0 },	/* asterisk operator, U+2217 ISOtech */
        { "lowbar", 95, 0 },
        { "loz", 9674, 0 },	/* lozenge, U+25CA ISOpub */
        { "lozenge", 9674, 0 },
        { "lozf", 10731, 0 },
        { "lpar", 40, 0 },
        { "lparlt", 10643, 0 },
        { "lrarr", 8646, 0 },
        { "lrcorner", 8991, 0 },
        { "lrhar", 8651, 0 },
        { "lrhard", 10605, 0 },
        { "lrm", 8206, 0 },	/* left-to-right mark, U+200E NEW RFC 2070 */
        { "lrtri", 8895, 0 },
        { "lsaquo", 8249, 0 },	/* single left-pointing angle quotation mark, U+2039 ISO proposed */
        { "lscr", 120001, 0 },
        { "lsh", 8624, 0 },
        { "lsim", 8818, 0 },
        { "lsime", 10893, 0 },
        { "lsimg", 10895, 0 },
        { "lsqb", 91, 0 },
        { "lsquo", 8216, 0 },	/* left single quotation mark, U+2018 ISOnum */
        { "lsquor", 8218, 0 },
        { "lstrok", 322, 0 },
        { "lt", 60, 0 },	/* less-than sign, U+003C ISOnum */
        { "ltcc", 10918, 0 },
        { "ltcir", 10873, 0 },
        { "ltdot", 8918, 0 },
        { "lthree", 8907, 0 },
        { "ltimes", 8905, 0 },
        { "ltlarr", 10614, 0 },
        { "ltquest", 10875, 0 },
        { "ltrPar", 10646, 0 },
        { "ltri", 9667, 0 },
        { "ltrie", 8884, 0 },
        { "ltrif", 9666, 0 },
        { "lurdshar", 10570, 0 },
        { "luruhar", 10598, 0 },
        { "lvertneqq", 8808, 65024 },
        { "lvnE", 8808, 65024 },
        { "mDDot", 8762, 0 },
        { "macr", 175, 0 },	/* macron = spacing macron = overline = APL overbar, U+00AF ISOdia */
        { "male", 9794, 0 },
        { "malt", 10016, 0 },
        { "maltese", 10016, 0 },
        { "map", 8614, 0 },
        { "mapsto", 8614, 0 },
        { "mapstodown", 8615, 0 },
        { "mapstoleft", 8612, 0 },
        { "mapstoup", 8613, 0 },
        { "marker", 9646, 0 },
        { "mcomma", 10793, 0 },
        { "mcy", 1084, 0 },
        { "mdash", 8212, 0 },	/* em dash, U+2014 ISOpub */
        { "measuredangle", 8737, 0 },
        { "mfr", 120106, 0 },
        { "mho", 8487, 0 },
        { "micro", 181, 0 },	/* micro sign, U+00B5 ISOnum */
        { "mid", 8739, 0 },
        { "midast", 42, 0 },
        { "midcir", 10992, 0 },
        { "middot", 183, 0 },	/* middle dot = Georgian comma = Greek middle dot, U+00B7 ISOnum */
        { "minus", 8722, 0 },	/* minus sign, U+2212 ISOtech */
        { "minusb", 8863, 0 },
        { "minusd", 8760, 0 },
        { "minusdu", 10794, 0 },
        { "mlcp", 10971, 0 },
        { "mldr", 8230, 0 },
        { "mnplus", 8723, 0 },
        { "models", 8871, 0 },
        { "mopf", 120158, 0 },
        { "mp", 8723, 0 },
        { "mscr", 120002, 0 },
        { "mstpos", 8766, 0 },
        { "mu", 956, 0 },	/* greek small letter mu, U+03BC ISOgrk3 */
        { "multimap", 8888, 0 },
        { "mumap", 8888, 0 },
        { "nGg", 8921, 824 },
        { "nGt", 8811, 8402 },
        { "nGtv", 8811, 824 },
        { "nLeftarrow", 8653, 0 },
        { "nLeftrightarrow", 8654, 0 },
        { "nLl", 8920, 824 },
        { "nLt", 8810, 8402 },
        { "nLtv", 8810, 824 },
        { "nRightarrow", 8655, 0 },
        { "nVDash", 8879, 0 },
        { "nVdash", 8878, 0 },
        { "nabla", 8711, 0 },	/* nabla = backward difference, U+2207 ISOtech */
        { "nacute", 324, 0 },
        { "nang", 8736, 8402 },
        { "nap", 8777, 0 },
        { "napE", 10864, 824 },
        { "napid", 8779, 824 },
        { "napos", 329, 0 },
        { "napprox", 8777, 0 },
        { "natur", 9838, 0 },
        { "natural", 9838, 0 },
        { "naturals", 8469, 0 },
        { "nbsp", 160, 0 },	/* no-break space = non-breaking space, U+00A0 ISOnum */
        { "nbump", 8782, 824 },
        { "nbumpe", 8783, 824 },
        { "ncap", 10819, 0 },
        { "ncaron", 328, 0 },
        { "ncedil", 326, 0 },
        { "ncong", 8775, 0 },
        { "ncongdot", 10861, 824 },
        { "ncup", 10818, 0 },
        { "ncy", 1085, 0 },
        { "ndash", 8211, 0 },	/* en dash, U+2013 ISOpub */
        { "ne", 8800, 0 },	/* not equal to, U+2260 ISOtech */
        { "neArr", 8663, 0 },
        { "nearhk", 10532, 0 },
        { "nearr", 8599, 0 },
        { "nearrow", 8599, 0 },
        { "nedot", 8784, 824 },
        { "nequiv", 8802, 0 },
        { "nesear", 10536, 0 },
        { "nesim", 8770, 824 },
        { "nexist", 8708, 0 },
        { "nexists", 8708, 0 },
        { "nfr", 120107, 0 },
        { "ngE", 8807, 824 },
        { "nge", 8817, 0 },
        { "ngeq", 8817, 0 },
        { "ngeqq", 8807, 824 },
        { "ngeqslant", 10878, 824 },
        { "nges", 10878, 824 },
        { "ngsim", 8821, 0 },
        { "ngt", 8815, 0 },
        { "ngtr", 8815, 0 },
        { "nhArr", 8654, 0 },
        { "nharr", 8622, 0 },
        { "nhpar", 10994, 0 },
        { "ni", 8715, 0 },	/* contains as member, U+220B ISOtech */
        { "nis", 8956, 0 },
        { "nisd", 8954, 0 },
        { "niv", 8715, 0 },
        { "njcy", 1114, 0 },
        { "nlArr", 8653, 0 },
        { "nlE", 8806, 824 },
        { "nlarr", 8602, 0 },
        { "nldr", 8229, 0 },
        { "nle", 8816, 0 },
        { "nleftarrow", 8602, 0 },
        { "nleftrightarrow", 8622, 0 },
        { "nleq", 8816, 0 },
        { "nleqq", 8806, 824 },
        { "nleqslant", 10877, 824 },
        { "nles", 10877, 824 },
        { "nless", 8814, 0 },
        { "nlsim", 8820, 0 },
        { "nlt", 8814, 0 },
        { "nltri", 8938, 0 },
        { "nltrie", 8940, 0 },
        { "nmid", 8740, 0 },
        { "nopf", 120159, 0 },
        { "not", 172, 0 },	/* not sign, U+00AC ISOnum */
        { "notin", 8713, 0 },	/* not an element of, U+2209 ISOtech */
        { "notinE", 8953, 824 },
        { "notindot", 8949, 824 },
        { "notinva", 8713, 0 },
        { "notinvb", 8951, 0 },
        { "notinvc", 8950, 0 },
        { "notni", 8716, 0 },
        { "notniva", 8716, 0 },
        { "notnivb", 8958, 0 },
        { "notnivc", 8957, 0 },
        { "npar", 8742, 0 },
        { "nparallel", 8742, 0 },
        { "nparsl", 11005, 8421 },
        { "npart", 8706, 824 },
        { "npolint", 10772, 0 },
        { "npr", 8832, 0 },
        { "nprcue", 8928, 0 },
        { "npre", 10927, 824 },
        { "nprec", 8832, 0 },
        { "npreceq", 10927, 824 },
        { "nrArr", 8655, 0 },
        { "nrarr", 8603, 0 },
        { "nrarrc", 10547, 824 },
        { "nrarrw", 8605, 824 },
        { "nrightarrow", 8603, 0 },
        { "nrtri", 8939, 0 },
        { "nrtrie", 8941, 0 },
        { "nsc", 8833, 0 },
        { "nsccue", 8929, 0 },
        { "nsce", 10928, 824 },
        { "nscr", 120003, 0 },
        { "nshortmid", 8740, 0 },
        { "nshortparallel", 8742, 0 },
        { "nsim", 8769, 0 },
        { "nsime", 8772, 0 },
        { "nsimeq", 8772, 0 },
        { "nsmid", 8740, 0 },
        { "nspar", 8742, 0 },
        { "nsqsube", 8930, 0 },
        { "nsqsupe", 8931, 0 },
        { "nsub", 8836, 0 },	/* not a subset of, U+2284 ISOamsn */
        { "nsubE", 10949, 824 },
        { "nsube", 8840, 0 },
        { "nsubset", 8834, 8402 },
        { "nsubseteq", 8840, 0 },
        { "nsubseteqq", 10949, 824 },
        { "nsucc", 8833, 0 },
        { "nsucceq", 10928, 824 },
        { "nsup", 8837, 0 },
        { "nsupE", 10950, 824 },
        { "nsupe", 8841, 0 },
        { "nsupset", 8835, 8402 },
        { "nsupseteq", 8841, 0 },
        { "nsupseteqq", 10950, 824 },
        { "ntgl", 8825, 0 },
        { "ntilde", 241, 0 },	/* latin small letter n with tilde, U+00F1 ISOlat1 */
        { "ntlg", 8824, 0 },
        { "ntriangleleft", 8938, 0 },
        { "ntrianglelefteq", 8940, 0 },
        { "ntriangleright", 8939, 0 },
        { "ntrianglerighteq", 8941, 0 },
        { "nu", 957, 0 },	/* greek small letter nu, U+03BD ISOgrk3 */
        { "num", 35, 0 },
        { "numero", 8470, 0 },
        { "numsp", 8199, 0 },
        { "nvDash", 8877, 0 },
        { "nvHarr", 10500, 0 },
        { "nvap", 8781, 8402 },
        { "nvdash", 8876, 0 },
        { "nvge", 8805, 8402 },
        { "nvgt", 62, 8402 },
        { "nvinfin", 10718, 0 },
        { "nvlArr", 10498, 0 },
        { "nvle", 8804, 8402 },
        { "nvlt", 60, 8402 },
        { "nvltrie", 8884, 8402 },
        { "nvrArr", 10499, 0 },
        { "nvrtrie", 8885, 8402 },
        { "nvsim", 8764, 8402 },
        { "nwArr", 8662, 0 },
        { "nwarhk", 10531, 0 },
        { "nwarr", 8598, 0 },
        { "nwarrow", 8598, 0 },
        { "nwnear", 10535, 0 },
        { "oS", 9416, 0 },
        { "oacute", 243, 0 },	/* latin small letter o with acute, U+00F3 ISOlat1 */
        { "oast", 8859, 0 },
        { "ocir", 8858, 0 },
        { "ocirc", 244, 0 },	/* latin small letter o with circumflex, U+00F4 ISOlat1 */
        { "ocy", 1086, 0 },
        { "odash", 8861, 0 },
        { "odblac", 337, 0 },
        { "odiv", 10808, 0 },
        { "odot", 8857, 0 },
        { "odsold", 10684, 0 },
        { "oelig", 339, 0 },	/* latin small ligature oe, U+0153 ISOlat2 */
        { "ofcir", 10687, 0 },
        { "ofr", 120108, 0 },
        { "ogon", 731, 0 },
        { "ograve", 242, 0 },	/* latin small letter o with grave, U+00F2 ISOlat1 */
        { "ogt", 10689, 0 },
        { "ohbar", 10677, 0 },
        { "ohm", 937, 0 },
        { "oint", 8750, 0 },
        { "olarr", 8634, 0 },
        { "olcir", 10686, 0 },
        { "olcross", 10683, 0 },
        { "oline", 8254, 0 },	/* overline = spacing overscore, U+203E NEW */
        { "olt", 10688, 0 },
        { "omacr", 333, 0 },
        { "omega", 969, 0 },	/* greek small letter omega, U+03C9 ISOgrk3 */
        { "omicron", 959, 0 },	/* greek small letter omicron, U+03BF NEW */
        { "omid", 10678, 0 },
        { "ominus", 8854, 0 },
        { "oopf", 120160, 0 },
        { "opar", 10679, 0 },
        { "operp", 10681, 0 },
        { "oplus", 8853, 0 },	/* circled plus = direct sum, U+2295 ISOamsb */
        { "or", 8744, 0 },	/* logical or = vee, U+2228 ISOtech */
        { "orarr", 8635, 0 },
        { "ord", 10845, 0 },
        { "order", 8500, 0 },
        { "orderof", 8500, 0 },
        { "ordf", 170, 0 },	/* feminine ordinal indicator, U+00AA ISOnum */
        { "ordm", 186, 0 },	/* masculine ordinal indicator, U+00BA ISOnum */
        { "origof", 8886, 0 },
        { "oror", 10838, 0 },
        { "orslope", 10839, 0 },
        { "orv", 10843, 0 },
        { "oscr", 8500, 0 },
        { "oslash", 248, 0 },	/* latin small letter o with stroke, = latin small letter o slash, U+00F8 ISOlat1 */
        { "osol", 8856, 0 },
        { "otilde", 245, 0 },	/* latin small letter o with tilde, U+00F5 ISOlat1 */
        { "otimes", 8855, 0 },	/* circled times = vector product, U+2297 ISOamsb */
        { "otimesas", 10806, 0 },
        { "ouml", 246, 0 },	/* latin small letter o with diaeresis, U+00F6 ISOlat1 */
        { "ovbar", 9021, 0 },
        { "par", 8741, 0 },
        { "para", 182, 0 },	/* pilcrow sign = paragraph sign, U+00B6 ISOnum */
        { "parallel", 8741, 0 },
        { "parsim", 10995, 0 },
        { "parsl", 11005, 0 },
        { "part", 8706, 0 },	/* partial differential, U+2202 ISOtech */
        { "pcy", 1087, 0 },
        { "percnt", 37, 0 },
        { "period", 46, 0 },
        { "permil", 8240, 0 },	/* per mille sign, U+2030 ISOtech */
        { "perp", 8869, 0 },	/* up tack = orthogonal to = perpendicular, U+22A5 ISOtech */
        { "pertenk", 8241, 0 },
        { "pfr", 120109, 0 },
        { "phi", 966, 0 },	/* greek small letter phi, U+03C6 ISOgrk3 */
        { "phiv", 981, 0 },
        { "phmmat", 8499, 0 },
        { "phone", 9742, 0 },
        { "pi", 960, 0 },	/* greek small letter pi, U+03C0 ISOgrk3 */
        { "pitchfork", 8916, 0 },
        { "piv", 982, 0 },	/* greek pi symbol, U+03D6 ISOgrk3 */
        { "planck", 8463, 0 },
        { "planckh", 8462, 0 },
        { "plankv", 8463, 0 },
        { "plus", 43, 0 },
        { "plusacir", 10787, 0 },
        { "plusb", 8862, 0 },
        { "pluscir", 10786, 0 },
        { "plusdo", 8724, 0 },
        { "plusdu", 10789, 0 },
        { "pluse", 10866, 0 },
        { "plusmn", 177, 0 },	/* plus-minus sign = plus-or-minus sign, U+00B1 ISOnum */
        { "plussim", 10790, 0 },
        { "plustwo", 10791, 0 },
        { "pm", 177, 0 },
        { "pointint", 10773, 0 },
        { "popf", 120161, 0 },
        { "pound", 163, 0 },	/* pound sign, U+00A3 ISOnum */
        { "pr", 8826, 0 },
        { "prE", 10931, 0 },
        { "prap", 10935, 0 },
        { "prcue", 8828, 0 },
        { "pre", 10927, 0 },
        { "prec", 8826, 0 },
        { "precapprox", 10935, 0 },
        { "preccurlyeq", 8828, 0 },
        { "preceq", 10927, 0 },
        { "precnapprox", 10937, 0 },
        { "precneqq", 10933, 0 },
        { "precnsim", 8936, 0 },
        { "precsim", 8830, 0 },
        { "prime", 8242, 0 },	/* prime = minutes = feet, U+2032 ISOtech */
        { "primes", 8473, 0 },
        { "prnE", 10933, 0 },
        { "prnap", 10937, 0 },
        { "prnsim", 8936, 0 },
        { "prod", 8719, 0 },	/* n-ary product = product sign, U+220F ISOamsb */
        { "profalar", 9006, 0 },
        { "profline", 8978, 0 },
        { "profsurf", 8979, 0 },
        { "prop", 8733, 0 },	/* proportional to, U+221D ISOtech */
        { "propto", 8733, 0 },
        { "prsim", 8830, 0 },
        { "prurel", 8880, 0 },
        { "pscr", 120005, 0 },
        { "psi", 968, 0 },	/* greek small letter psi, U+03C8 ISOgrk3 */
        { "puncsp", 8200, 0 },
        { "qfr", 120110, 0 },
        { "qint", 10764, 0 },
        { "qopf", 120162, 0 },
        { "qprime", 8279, 0 },
        { "qscr", 120006, 0 },
        { "quaternions", 8461, 0 },
        { "quatint", 10774, 0 },
        { "quest", 63, 0 },
        { "questeq", 8799, 0 },
        { "quot", 34, 0 },	/* quotation mark = APL quote, U+0022 ISOnum */
        { "rAarr", 8667, 0 },
        { "rArr", 8658, 0 },	/* rightwards double arrow, U+21D2 ISOtech */
        { "rAtail", 10524, 0 },
        { "rBarr", 10511, 0 },
        { "rHar", 10596, 0 },
        { "race", 8765, 817 },
        { "racute", 341, 0 },
        { "radic", 8730, 0 },	/* square root = radical sign, U+221A ISOtech */
        { "raemptyv", 10675, 0 },
        { "rang", 9002, 0 },	/* right-pointing angle bracket = ket, U+232A ISOtech */
        { "rangd", 10642, 0 },
        { "range", 10661, 0 },
        { "rangle", 10217, 0 },	/* U+27E9⟩ */
        { "raquo", 187, 0 },	/* right-pointing double angle quotation mark = right pointing guillemet, U+00BB ISOnum */
        { "rarr", 8594, 0 },	/* rightwards arrow, U+2192 ISOnum */
        { "rarrap", 10613, 0 },
        { "rarrb", 8677, 0 },
        { "rarrbfs", 10528, 0 },
        { "rarrc", 10547, 0 },
        { "rarrfs", 10526, 0 },
        { "rarrhk", 8618, 0 },
        { "rarrlp", 8620, 0 },
        { "rarrpl", 10565, 0 },
        { "rarrsim", 10612, 0 },
        { "rarrtl", 8611, 0 },
        { "rarrw", 8605, 0 },
        { "ratail", 10522, 0 },
        { "ratio", 8758, 0 },
        { "rationals", 8474, 0 },
        { "rbarr", 10509, 0 },
        { "rbbrk", 10099, 0 },
        { "rbrace", 125, 0 },
        { "rbrack", 93, 0 },
        { "rbrke", 10636, 0 },
        { "rbrksld", 10638, 0 },
        { "rbrkslu", 10640, 0 },
        { "rcaron", 345, 0 },
        { "rcedil", 343, 0 },
        { "rceil", 8969, 0 },	/* right ceiling, U+2309 ISOamsc */
        { "rcub", 125, 0 },
        { "rcy", 1088, 0 },
        { "rdca", 10551, 0 },
        { "rdldhar", 10601, 0 },
        { "rdquo", 8221, 0 },	/* right double quotation mark, U+201D ISOnum */
        { "rdquor", 8221, 0 },
        { "rdsh", 8627, 0 },
        { "real", 8476, 0 },	/* blackletter capital R = real part symbol, U+211C ISOamso */
        { "realine", 8475, 0 },
        { "realpart", 8476, 0 },
        { "reals", 8477, 0 },
        { "rect", 9645, 0 },
        { "reg", 174, 0 },	/* registered sign = registered trade mark sign, U+00AE ISOnum */
        { "rfisht", 10621, 0 },
        { "rfloor", 8971, 0 },	/* right floor, U+230B ISOamsc */
        { "rfr", 120111, 0 },
        { "rhard", 8641, 0 },
        { "rharu", 8640, 0 },
        { "rharul", 10604, 0 },
        { "rho", 961, 0 },	/* greek small letter rho, U+03C1 ISOgrk3 */
        { "rhov", 1009, 0 },
        { "rightarrow", 8594, 0 },
        { "rightarrowtail", 8611, 0 },
        { "rightharpoondown", 8641, 0 },
        { "rightharpoonup", 8640, 0 },
        { "rightleftarrows", 8644, 0 },
        { "rightleftharpoons", 8652, 0 },
        { "rightrightarrows", 8649, 0 },
        { "rightsquigarrow", 8605, 0 },
        { "rightthreetimes", 8908, 0 },
        { "ring", 730, 0 },
        { "risingdotseq", 8787, 0 },
        { "rlarr", 8644, 0 },
        { "rlhar", 8652, 0 },
        { "rlm", 8207, 0 },	/* right-to-left mark, U+200F NEW RFC 2070 */
        { "rmoust", 9137, 0 },
        { "rmoustache", 9137, 0 },
        { "rnmid", 10990, 0 },
        { "roang", 10221, 0 },
        { "roarr", 8702, 0 },
        { "robrk", 10215, 0 },
        { "ropar", 10630, 0 },
        { "ropf", 120163, 0 },
        { "roplus", 10798, 0 },
        { "rotimes", 10805, 0 },
        { "rpar", 41, 0 },
        { "rpargt", 10644, 0 },
        { "rppolint", 10770, 0 },
        { "rrarr", 8649, 0 },
        { "rsaquo", 8250, 0 },	/* single right-pointing angle quotation mark, U+203A ISO proposed */
        { "rscr", 120007, 0 },
        { "rsh", 8625, 0 },
        { "rsqb", 93, 0 },
        { "rsquo", 8217, 0 },	/* right single quotation mark, U+2019 ISOnum */
        { "rsquor", 8217, 0 },
        { "rthree", 8908, 0 },
        { "rtimes", 8906, 0 },
        { "rtri", 9657, 0 },
        { "rtrie", 8885, 0 },
        { "rtrif", 9656, 0 },
        { "rtriltri", 10702, 0 },
        { "ruluhar", 10600, 0 },
        { "rx", 8478, 0 },
        { "sacute", 347, 0 },
        { "sbquo", 8218, 0 },	/* single low-9 quotation mark, U+201A NEW */
        { "sc", 8827, 0 },
        { "scE", 10932, 0 },
        { "scap", 10936, 0 },
        { "scaron", 353, 0 },	/* latin small letter s with caron, U+0161 ISOlat2 */
        { "sccue", 8829, 0 },
        { "sce", 10928, 0 },
        { "scedil", 351, 0 },
        { "scirc", 349, 0 },
        { "scnE", 10934, 0 },
        { "scnap", 10938, 0 },
        { "scnsim", 8937, 0 },
        { "scpolint", 10771, 0 },
        { "scsim", 8831, 0 },
        { "scy", 1089, 0 },
        { "sdot", 8901, 0 },	/* dot operator, U+22C5 ISOamsb */
        { "sdotb", 8865, 0 },
        { "sdote", 10854, 0 },
        { "seArr", 8664, 0 },
        { "searhk", 10533, 0 },
        { "searr", 8600, 0 },
        { "searrow", 8600, 0 },
        { "sect", 167, 0 },	/* section sign, U+00A7 ISOnum */
        { "semi", 59, 0 },
        { "seswar", 10537, 0 },
        { "setminus", 8726, 0 },
        { "setmn", 8726, 0 },
        { "sext", 10038, 0 },
        { "sfr", 120112, 0 },
        { "sfrown", 8994, 0 },
        { "sharp", 9839, 0 },
        { "shchcy", 1097, 0 },
        { "shcy", 1096, 0 },
        { "shortmid", 8739, 0 },
        { "shortparallel", 8741, 0 },
        { "shy", 173, 0 },	/* soft hyphen = discretionary hyphen, U+00AD ISOnum */
        { "sigma", 963, 0 },	/* greek small letter sigma, U+03C3 ISOgrk3 */
        { "sigmaf", 962, 0 },	/* greek small letter final sigma, U+03C2 ISOgrk3 */
        { "sigmav", 962, 0 },
        { "sim", 8764, 0 },	/* tilde operator = varies with = similar to, U+223C ISOtech */
        { "simdot", 10858, 0 },
        { "sime", 8771, 0 },
        { "simeq", 8771, 0 },
        { "simg", 10910, 0 },
        { "simgE", 10912, 0 },
        { "siml", 10909, 0 },
        { "simlE", 10911, 0 },
        { "simne", 8774, 0 },
        { "simplus", 10788, 0 },
        { "simrarr", 10610, 0 },
        { "slarr", 8592, 0 },
        { "smallsetminus", 8726, 0 },
        { "smashp", 10803, 0 },
        { "smeparsl", 10724, 0 },
        { "smid", 8739, 0 },
        { "smile", 8995, 0 },
        { "smt", 10922, 0 },
        { "smte", 10924, 0 },
        { "smtes", 10924, 65024 },
        { "softcy", 1100, 0 },
        { "sol", 47, 0 },
        { "solb", 10692, 0 },
        { "solbar", 9023, 0 },
        { "sopf", 120164, 0 },
        { "spades", 9824, 0 },	/* black spade suit, U+2660 ISOpub */
        { "spadesuit", 9824, 0 },
        { "spar", 8741, 0 },
        { "sqcap", 8851, 0 },
        { "sqcaps", 8851, 65024 },
        { "sqcup", 8852, 0 },
        { "sqcups", 8852, 65024 },
        { "sqsub", 8847, 0 },
        { "sqsube", 8849, 0 },
        { "sqsubset", 8847, 0 },
        { "sqsubseteq", 8849, 0 },
        { "sqsup", 8848, 0 },
        { "sqsupe", 8850, 0 },
        { "sqsupset", 8848, 0 },
        { "sqsupseteq", 8850, 0 },
        { "squ", 9633, 0 },
        { "square", 9633, 0 },
        { "squarf", 9642, 0 },
        { "squf", 9642, 0 },
        { "srarr", 8594, 0 },
        { "sscr", 120008, 0 },
        { "ssetmn", 8726, 0 },
        { "ssmile", 8995, 0 },
        { "sstarf", 8902, 0 },
        { "star", 9734, 0 },
        { "starf", 9733, 0 },
        { "straightepsilon", 1013, 0 },
        { "straightphi", 981, 0 },
        { "strns", 175, 0 },
        { "sub", 8834, 0 },	/* subset of, U+2282 ISOtech */
        { "subE", 10949, 0 },
        { "subdot", 10941, 0 },
        { "sube", 8838, 0 },	/* subset of or equal to, U+2286 ISOtech */
        { "subedot", 10947, 0 },
        { "submult", 10945, 0 },
        { "subnE", 10955, 0 },
        { "subne", 8842, 0 },
        { "subplus", 10943, 0 },
        { "subrarr", 10617, 0 },
        { "subset", 8834, 0 },
        { "subseteq", 8838, 0 },
        { "subseteqq", 10949, 0 },
        { "subsetneq", 8842, 0 },
        { "subsetneqq", 10955, 0 },
        { "subsim", 10951, 0 },
        { "subsub", 10965, 0 },
        { "subsup", 10963, 0 },
        { "succ", 8827, 0 },
        { "succapprox", 10936, 0 },
        { "succcurlyeq", 8829, 0 },
        { "succeq", 10928, 0 },
        { "succnapprox", 10938, 0 },
        { "succneqq", 10934, 0 },
        { "succnsim", 8937, 0 },
        { "succsim", 8831, 0 },
        { "sum", 8721, 0 },	/* n-ary sumation, U+2211 ISOamsb */
        { "sung", 9834, 0 },
        { "sup", 8835, 0 },	/* superset of, U+2283 ISOtech */
        { "sup1", 185, 0 },	/* superscript one = superscript digit one, U+00B9 ISOnum */
        { "sup2", 178, 0 },	/* superscript two = superscript digit two = squared, U+00B2 ISOnum */
        { "sup3", 179, 0 },	/* superscript three = superscript digit three = cubed, U+00B3 ISOnum */
        { "supE", 10950, 0 },
        { "supdot", 10942, 0 },
        { "supdsub", 10968, 0 },
        { "supe", 8839, 0 },	/* superset of or equal to, U+2287 ISOtech */
        { "supedot", 10948, 0 },
        { "suphsol", 10185, 0 },
        { "suphsub", 10967, 0 },
        { "suplarr", 10619, 0 },
        { "supmult", 10946, 0 },
        { "supnE", 10956, 0 },
        { "supne", 8843, 0 },
        { "supplus", 10944, 0 },
        { "supset", 8835, 0 },
        { "supseteq", 8839, 0 },
        { "supseteqq", 10950, 0 },
        { "supsetneq", 8843, 0 },
        { "supsetneqq", 10956, 0 },
        { "supsim", 10952, 0 },
        { "supsub", 10964, 0 },
        { "supsup", 10966, 0 },
        { "swArr", 8665, 0 },
        { "swarhk", 10534, 0 },
        { "swarr", 8601, 0 },
        { "swarrow", 8601, 0 },
        { "swnwar", 10538, 0 },
        { "szlig", 223, 0 },	/* latin small letter sharp s = ess-zed, U+00DF ISOlat1 */
        { "target", 8982, 0 },
        { "tau", 964, 0 },	/* greek small letter tau, U+03C4 ISOgrk3 */
        { "tbrk", 9140, 0 },
        { "tcaron", 357, 0 },
        { "tcedil", 355, 0 },
        { "tcy", 1090, 0 },
        { "tdot", 8411, 0 },
        { "telrec", 8981, 0 },
        { "tfr", 120113, 0 },
        { "there4", 8756, 0 },	/* therefore, U+2234 ISOtech */
        { "therefore", 8756, 0 },
        { "theta", 952, 0 },	/* greek small letter theta, U+03B8 ISOgrk3 */
        { "thetasym", 977, 0 },	/* greek small letter theta symbol, U+03D1 NEW */
        { "thetav", 977, 0 },
        { "thickapprox", 8776, 0 },
        { "thicksim", 8764, 0 },
        { "thinsp", 8201, 0 },	/* thin space, U+2009 ISOpub */
        { "thkap", 8776, 0 },
        { "thksim", 8764, 0 },
        { "thorn", 254, 0 },	/* latin small letter thorn, U+00FE ISOlat1 */
        { "tilde", 732, 0 },	/* small tilde, U+02DC ISOdia */
        { "times", 215, 0 },	/* multiplication sign, U+00D7 ISOnum */
        { "timesb", 8864, 0 },
        { "timesbar", 10801, 0 },
        { "timesd", 10800, 0 },
        { "tint", 8749, 0 },
        { "toea", 10536, 0 },
        { "top", 8868, 0 },
        { "topbot", 9014, 0 },
        { "topcir", 10993, 0 },
        { "topf", 120165, 0 },
        { "topfork", 10970, 0 },
        { "tosa", 10537, 0 },
        { "tprime", 8244, 0 },
        { "trade", 8482, 0 },	/* trade mark sign, U+2122 ISOnum */
        { "triangle", 9653, 0 },
        { "triangledown", 9663, 0 },
        { "triangleleft", 9667, 0 },
        { "trianglelefteq", 8884, 0 },
        { "triangleq", 8796, 0 },
        { "triangleright", 9657, 0 },
        { "trianglerighteq", 8885, 0 },
        { "tridot", 9708, 0 },
        { "trie", 8796, 0 },
        { "triminus", 10810, 0 },
        { "triplus", 10809, 0 },
        { "trisb", 10701, 0 },
        { "tritime", 10811, 0 },
        { "trpezium", 9186, 0 },
        { "tscr", 120009, 0 },
        { "tscy", 1094, 0 },
        { "tshcy", 1115, 0 },
        { "tstrok", 359, 0 },
        { "twixt", 8812, 0 },
        { "twoheadleftarrow", 8606, 0 },
        { "twoheadrightarrow", 8608, 0 },
        { "uArr", 8657, 0 },	/* upwards double arrow, U+21D1 ISOamsa */
        { "uHar", 10595, 0 },
        { "uacute", 250, 0 },	/* latin small letter u with acute, U+00FA ISOlat1 */
        { "uarr", 8593, 0 },	/* upwards arrow, U+2191 ISOnum */
        { "ubrcy", 1118, 0 },
        { "ubreve", 365, 0 },
        { "ucirc", 251, 0 },	/* latin small letter u with circumflex, U+00FB ISOlat1 */
        { "ucy", 1091, 0 },
        { "udarr", 8645, 0 },
        { "udblac", 369, 0 },
        { "udhar", 10606, 0 },
        { "ufisht", 10622, 0 },
        { "ufr", 120114, 0 },
        { "ugrave", 249, 0 },	/* latin small letter u with grave, U+00F9 ISOlat1 */
        { "uharl", 8639, 0 },
        { "uharr", 8638, 0 },
        { "uhblk", 9600, 0 },
        { "ulcorn", 8988, 0 },
        { "ulcorner", 8988, 0 },
        { "ulcrop", 8975, 0 },
        { "ultri", 9720, 0 },
        { "umacr", 363, 0 },
        { "uml", 168, 0 },	/* diaeresis = spacing diaeresis, U+00A8 ISOdia */
        { "uogon", 371, 0 },
        { "uopf", 120166, 0 },
        { "uparrow", 8593, 0 },
        { "updownarrow", 8597, 0 },
        { "upharpoonleft", 8639, 0 },
        { "upharpoonright", 8638, 0 },
        { "uplus", 8846, 0 },
        { "upsi", 965, 0 },
        { "upsih", 978, 0 },	/* greek upsilon with hook symbol, U+03D2 NEW */
        { "upsilon", 965, 0 },	/* greek small letter upsilon, U+03C5 ISOgrk3 */
        { "upuparrows", 8648, 0 },
        { "urcorn", 8989, 0 },
        { "urcorner", 8989, 0 },
        { "urcrop", 8974, 0 },
        { "uring", 367, 0 },
        { "urtri", 9721, 0 },
        { "uscr", 120010, 0 },
        { "utdot", 8944, 0 },
        { "utilde", 361, 0 },
        { "utri", 9653, 0 },
        { "utrif", 9652, 0 },
        { "uuarr", 8648, 0 },
        { "uuml", 252, 0 },	/* latin small letter u with diaeresis, U+00FC ISOlat1 */
        { "uwangle", 10663, 0 },
        { "vArr", 8661, 0 },
        { "vBar", 10984, 0 },
        { "vBarv", 10985, 0 },
        { "vDash", 8872, 0 },
        { "vangrt", 10652, 0 },
        { "varepsilon", 1013, 0 },
        { "varkappa", 1008, 0 },
        { "varnothing", 8709, 0 },
        { "varphi", 981, 0 },
        { "varpi", 982, 0 },
        { "varpropto", 8733, 0 },
        { "varr", 8597, 0 },
        { "varrho", 1009, 0 },
        { "varsigma", 962, 0 },
        { "varsubsetneq", 8842, 65024 },
        { "varsubsetneqq", 10955, 65024 },
        { "varsupsetneq", 8843, 65024 },
        { "varsupsetneqq", 10956, 65024 },
        { "vartheta", 977, 0 },
        { "vartriangleleft", 8882, 0 },
        { "vartriangleright", 8883, 0 },
        { "vcy", 1074, 0 },
        { "vdash", 8866, 0 },
        { "vee", 8744, 0 },
        { "veebar", 8891, 0 },
        { "veeeq", 8794, 0 },
        { "vellip", 8942, 0 },
        { "verbar", 124, 0 },
        { "vert", 124, 0 },
        { "vfr", 120115, 0 },
        { "vltri", 8882, 0 },
        { "vnsub", 8834, 8402 },
        { "vnsup", 8835, 8402 },
        { "vopf", 120167, 0 },
        { "vprop", 8733, 0 },
        { "vrtri", 8883, 0 },
        { "vscr", 120011, 0 },
        { "vsubnE", 10955, 65024 },
        { "vsubne", 8842, 65024 },
        { "vsupnE", 10956, 65024 },
        { "vsupne", 8843, 65024 },
        { "vzigzag", 10650, 0 },
        { "wcirc", 373, 0 },
        { "wedbar", 10847, 0 },
        { "wedge", 8743, 0 },
        { "wedgeq", 8793, 0 },
        { "weierp", 8472, 0 },	/* script capital P = power set = Weierstrass p, U+2118 ISOamso */
        { "wfr", 120116, 0 },
        { "wopf", 120168, 0 },
        { "wp", 8472, 0 },
        { "wr", 8768, 0 },
        { "wreath", 8768, 0 },
        { "wscr", 120012, 0 },
        { "xcap", 8898, 0 },
        { "xcirc", 9711, 0 },
        { "xcup", 8899, 0 },
        { "xdtri", 9661, 0 },
        { "xfr", 120117, 0 },
        { "xhArr", 10234, 0 },
        { "xharr", 10231, 0 },
        { "xi", 958, 0 },	/* greek small letter xi, U+03BE ISOgrk3 */
        { "xlArr", 10232, 0 },
        { "xlarr", 10229, 0 },
        { "xmap", 10236, 0 },
        { "xnis", 8955, 0 },
        { "xodot", 10752, 0 },
        { "xopf", 120169, 0 },
        { "xoplus", 10753, 0 },
        { "xotime", 10754, 0 },
        { "xrArr", 10233, 0 },
        { "xrarr", 10230, 0 },
        { "xscr", 120013, 0 },
        { "xsqcup", 10758, 0 },
        { "xuplus", 10756, 0 },
        { "xutri", 9651, 0 },
        { "xvee", 8897, 0 },
        { "xwedge", 8896, 0 },
        { "yacute", 253, 0 },	/* latin small letter y with acute, U+00FD ISOlat1 */
        { "yacy", 1103, 0 },
        { "ycirc", 375, 0 },
        { "ycy", 1099, 0 },
        { "yen", 165, 0 },	/* yen sign = yuan sign, U+00A5 ISOnum */
        { "yfr", 120118, 0 },
        { "yicy", 1111, 0 },
        { "yopf", 120170, 0 },
        { "yscr", 120014, 0 },
        { "yucy", 1102, 0 },
        { "yuml", 255, 0 },	/* latin small letter y with diaeresis, U+00FF ISOlat1 */
        { "zacute", 378, 0 },
        { "zcaron", 382, 0 },
        { "zcy", 1079, 0 },
        { "zdot", 380, 0 },
        { "zeetrf", 8488, 0 },
        { "zeta", 950, 0 },	/* greek small letter zeta, U+03B6 ISOgrk3 */
        { "zfr", 120119, 0 },
        { "zhcy", 1078, 0 },
        { "zigrarr", 8669, 0 },
        { "zopf", 120171, 0 },
        { "zscr", 120015, 0 },
        { "zwj", 8205, 0 },	/* zero width joiner, U+200D NEW RFC 2070 */
        { "zwnj", 8204, 0 } };	/* zero width non-joiner, U+200C NEW RFC 2070 */

int compareEntities(const void * a, const void * b)
    {
    return strcmp(((Entity*)a)->ent,((Entity*)b)->ent);
    }

static int HTvar = 0;
static int Xvar = 0;
static int charref(const unsigned char * c)
    {
    if(*c == ';')
        {
        *glob_p = '\0';
        if(  !strcmp((const char *)bufx,"amp")
          || !strcmp((const char*)bufx,"#38")
          || !strcmp((const char*)bufx,"#x26")
          )
            rawput('&');
        else if(!strcmp((const char*)bufx,"apos"))
            rawput('\'');
        else if(!strcmp((const char*)bufx,"quot"))
            {
            rawput('\"');
            }
        else if(!strcmp((const char*)bufx,"lt"))
            rawput('<');
        else if(!strcmp((const char*)bufx,"gt"))
            rawput('>');
        else if(bufx[0] == '#')
            {
            unsigned long N;
            unsigned char tmp[22];
            N = (bufx[1] == 'x') ? strtoul((const char*)(bufx+2),NULL,16) : strtoul((const char*)(bufx+1),NULL,10);
            glob_p = bufx;
            xput = Put;
            if(putCodePoint(N,tmp))
                {
                return nrawput(tmp);
                }
            else
                return 0;
            }
        else
            {
            if(HTvar)
                {
                Entity * pItem;
                Entity key;
                key.ent = (const char*)bufx;
                pItem = (Entity*)bsearch( &key
                                        , entities
                                        , sizeof(entities)/sizeof(entities[0])
                                        , sizeof(entities[0])
                                        , compareEntities
                                        );
                if (pItem!=NULL)
                    {
                    unsigned char tmp[100];
                    unsigned char * endp;
                    glob_p = bufx;
                    xput = Put;
                    endp = putCodePoint(pItem->code, tmp);
                    if(endp)
                        {
                        if (pItem->morecode)
                            {
                            endp = putCodePoint(pItem->morecode, endp);
                            if (endp)
                                return nrawput(tmp);
                            }
                        else
                            return nrawput(tmp);
                        }
                    }
                }
            rawput(127); /* Escape with DEL */
            rawput('&');
            nrawput(bufx);
            rawput(';');
            glob_p = bufx;
            xput = Put;
            return FALSE;
            }
        glob_p = bufx;
        xput = Put;
        }
    else if(!namechar(*c))
        {
        rawput('&');
        *glob_p = '\0';
        nrawput(bufx);
        if(*c > 0)
            rawput(*c);
        glob_p = bufx;
        xput = Put;
        return FALSE;
        }
    else if(glob_p < bufx+BUFSIZE-1)
        {
        *glob_p++ = *c;
        }
    return TRUE;
    }

static int Put(const unsigned char * c)
    {
    if(*c == '&')
        {
        xput = charref;
        namechar = entity;
        return TRUE;
        }
    else if(*c == 127) /* DEL, used as escape character for & before unrecognised entity reference */
        rawput(*c); /* double the escape character */
    if(*c & 0x80)
        {
        /* Since entities always are converted to UTF-8 encoded characters, we have to do the same
           with ALL characters. */
        unsigned char tmp[8];
        if(assumeUTF8)
            {
            if(*c & 0x40) /* first byte of multibyte char */
                {
                const char* d = (const char*)c;
                int R = getCodePoint(&d); /* look ahead. Is it UTF-8 encoded ? */
                if(R >= 0)
                    return rawput(*c);
                else
                    assumeUTF8 = FALSE;
                }
            else
                return rawput(*c); /* second or later byte of multubute char */
            }
        if(putCodePoint(*c, tmp))
            return nrawput(tmp);
        else
            return 0;
        }
    return rawput(*c);
    }

static void flushx(void)
    {
    if(xput != Put)
        xput((const unsigned char*)"");
    }

static void nxput(unsigned char * start,unsigned char *end)
    {
    for(;start < end;++start)
        xput(start);
    flushx();
    }

static void nxputWithoutEntityUnfolding(unsigned char * start,unsigned char *end)
    {
    for(;start < end;++start)
        rawput(*start);
    }

static void nonTagWithoutEntityUnfolding(const char * kind,unsigned char * start,unsigned char * end)
    {
    nrawput((const unsigned char*)kind);
    putOperatorChar('.');
    nxputWithoutEntityUnfolding(start,end);
    putOperatorChar(')');
    }

static void nonTag(const char * kind,unsigned char * start,unsigned char * end)
    {
    nrawput((const unsigned char*)kind);
    putOperatorChar('.');
    nxput(start,end);
    putOperatorChar(')');
    }

static unsigned char * ch;
static unsigned char * StaRt = 0;
static int isMarkup = 0;

static void cbStartMarkUp(void)
    {
    flushx();
    StaRt = ch+2;
    isMarkup = 1;
    putOperatorChar(' ');
    putOperatorChar('(');
    }

static void cbEndMarkUp(void)/* called when > has been read */
    {
    if(isMarkup)
        nonTag("!",StaRt,ch);
    }

static void cbEndDOCTYPE(void)/* called when > has been read */
    {
    isMarkup = 0;
    nonTagWithoutEntityUnfolding("!DOCTYPE",StaRt,ch);
    }

static unsigned char * endElementName;
static void cbEndElementName(void)
    {
    nxput(StaRt,endElementName ? endElementName : ch);
    putOperatorChar('.');
    endElementName = NULL;
    }

static void cbEndAttributeName(void)
    {
    nxput(StaRt,ch);
    putOperatorChar('.');
    }

static void cbEndAttribute(void)
    {
    putOperatorChar(')');
    putOperatorChar(' ');
    }

static estate def_pcdata(const unsigned char * pkar);
static estate (*defx)(const unsigned char * pkar) = def_pcdata;
static estate def_cdata(const unsigned char * pkar);
static estate lt(const unsigned char * pkar);
static estate lt_cdata(const unsigned char * pkar);
static estate lts_cdata(const unsigned char * pkar);
static estate element(const unsigned char * pkar);
static estate elementonly(const unsigned char * pkar);
static estate gt(const unsigned char * pkar);
static estate emptytag(const unsigned char * pkar);
static estate atts(const unsigned char * pkar);
static estate namex(const unsigned char * pkar);
static estate valuex(const unsigned char * pkar);
static estate atts_or_value(const unsigned char * pkar);
static estate invalue(const unsigned char * pkar);
static estate singlequotes(const unsigned char * pkar);
static estate doublequotes(const unsigned char * pkar);
static estate insinglequotedvalue(const unsigned char * pkar);
static estate indoublequotedvalue(const unsigned char * pkar);
static estate endvalue(const unsigned char * pkar);
static estate markup(const unsigned char * pkar); /* <! */
static estate perhapsScriptOrStyle(const unsigned char * pkar); /* <s or <S */
static estate scriptOrStyleElement(const unsigned char * pkar); /* <sc or <SC or <Sc or <sC or <st or <ST or <St or <sT */
static estate scriptOrStyleEndElement(const unsigned char * pkar); /* </s or </S */
static estate scriptOrStyleEndElementL(const unsigned char * pkar); /* </script or </SCRIPT or </style or </STYLE */
static estate unknownmarkup(const unsigned char * pkar);
static estate PrInstr(const unsigned char * pkar);       /* processing instruction <?...?> (XML) or <?...> (non-XML) */
static estate endPI(const unsigned char * pkar);
static estate DOCTYPE1(const unsigned char * pkar); /* <!D */
static estate DOCTYPE7(const unsigned char * pkar); /* <!DOCTYPE */
static estate DOCTYPE8(const unsigned char * pkar); /* <!DOCTYPE S */
static estate DOCTYPE9(const unsigned char * pkar); /* <!DOCTYPE S [ */
static estate DOCTYPE10(const unsigned char * pkar); /* <!DOCTYPE S [ ] */
static estate CDATA1(const unsigned char * pkar); /* <![ */
static estate CDATA7(const unsigned char * pkar); /* <![CDATA[ */
static estate CDATA8(const unsigned char * pkar); /* <![CDATA[ ] */
static estate CDATA9(const unsigned char * pkar); /* <![CDATA[ ]] */
static estate h1(const unsigned char * pkar); /* <!- */
static estate h2(const unsigned char * pkar); /* <!-- */
static estate h3(const unsigned char * pkar); /* <!--  - */
static estate endtag(const unsigned char * pkar);

static estate def_pcdata(const unsigned char * pkar)
    {
    switch(*pkar)
        {
        case '<':
            tagState = lt;
            cbStartMarkUp();
            return tag;
        default:
            return notag;
        }
    }
    
static estate def_cdata(const unsigned char * pkar)
    {
    switch(*pkar)
        {
        case '<':
            tagState = lt_cdata;
            return tag;
        default:
            return notag;
        }
    }
    
static estate lt(const unsigned char * pkar)
    {
    const int kar = *pkar;
    endElementName = NULL;
    switch(kar)
        {
        case '>':
            tagState = defx;
            return notag;
        case '!':
            tagState = markup;
            return tag;
        case '?':
            StaRt = ch;
            tagState = PrInstr;
            return tag;
        case '/':
            tagState = endtag;
            /* fall through */
            return tag;
        case 's':
        case 'S':
            if(HTvar && !Xvar)
                {
                tagState = perhapsScriptOrStyle;
                StaRt = ch;
                return tag;
                }
            /* fall through */
        default:
            if(':' == kar || ('A' <= kar && kar <= 'Z') || '_' == kar || ('a' <= kar && kar <= 'z') || (kar & 0x80))
                {
                tagState = element;
                StaRt = ch;
                return tag;
                }
            else
                {
                tagState = defx;
                return notag;
                }
        }
    }

static estate lt_cdata(const unsigned char * pkar)
    {
    switch(*pkar)
        {
        case '/':
            tagState = lts_cdata;
            return tag;
        default:
            tagState = def_cdata;
            return notag;            
        }
    }

static int ScriptStyleiMax = 0;
static int scriptstylei = 0;
static int scriptstylei2 = 0;
static int scriptstyleimax = 0;
static unsigned char * elementEndNameLower;
static unsigned char * elementEndNameUpper;
static estate scriptOrStyleEndElement(const unsigned char * pkar) /* <sc or <SC or <Sc or <sC or <st or <ST or <St or <sT */
    {
    const int kar = *pkar;
    if(kar == elementEndNameLower[scriptstylei2] || kar == elementEndNameUpper[scriptstylei2])
        {
        estate ret;
        if(scriptstylei2 == scriptstyleimax)
            {
            tagState = scriptOrStyleEndElementL;
            scriptstylei2 = 0;
            endElementName = NULL;
            }
        else
            ++scriptstylei2;
        ret = element(pkar);
        return ret;
        }
    else
        {
        scriptstylei2 = 0;
        tagState = def_cdata;
        return notag;
        }
    }

static estate scriptOrStyleEndElementL(const unsigned char * pkar) /* <sc or <SC or <Sc or <sC or <st or <ST or <St or <sT */
    {
    switch(*pkar)
        {
        case '>':
            flushx();
            isMarkup = 1;
            putOperatorChar(' ');
            putOperatorChar('(');
            putOperatorChar('.');
            cbEndElementName();
            putOperatorChar(')');
            defx = def_pcdata;
            tagState = defx;
            StaRt = ch+1;
            return endoftag;
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            if(endElementName == NULL)
                endElementName = ch;
            return tag;           
        default:
            endElementName = NULL;
            scriptstylei2 = 0;
            tagState = def_cdata;
            return notag;
        }
    }

static estate lts_cdata(const unsigned char * pkar)
    {
    const int kar = *pkar;
    scriptstyleimax = ScriptStyleiMax;
    scriptstylei2 = 0;
    if(  kar == elementEndNameLower[scriptstylei2]
      || kar == elementEndNameUpper[scriptstylei2]
      )
        {
        StaRt = ch;
        ++scriptstylei2;
        tagState = scriptOrStyleEndElement;
        return tag;            
        }
    tagState = def_cdata;
    return notag;
    }


static void stillCdata(void)
    {
    if(scriptstylei > 0 && defx == def_cdata)
        {
        defx = def_pcdata;
        scriptstylei = 0;
        }
    }

static estate element(const unsigned char * pkar)
    {
    const int kar = *pkar;
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbEndElementName();
            putOperatorChar(')');
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = defx;
            cbEndElementName();
            putOperatorChar(')');
            return endoftag;
        case '-':
        case '_':
        case ':':
        case '.':
            stillCdata();
            return tag;
        case 0xA0:
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            cbEndElementName();
            tagState = atts;
            return tag;
        case '/':
            cbEndElementName();
            putOperatorChar(',');
            putOperatorChar(')');
            tagState = emptytag;
            return tag;
        default:
            if(('0' <= kar && kar <= '9') || ('A' <= kar && kar <= 'Z') || ('a' <= kar && kar <= 'z') || (kar & 0x80))
                {
                stillCdata();
                return tag;
                }
            else
                {
                stillCdata();
                tagState = defx;
                return notag;
                }
        }
    }

static estate elementonly(const unsigned char * pkar)
    {
    const int kar = *pkar;
    switch(kar)
        {
        case '<':
            tagState = lt;
            putOperatorChar('.');
            cbEndElementName();
            putOperatorChar(')');
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = defx;
            putOperatorChar('.');
            cbEndElementName();
            putOperatorChar(')');
            return endoftag;
        case '-':
        case '_':
        case ':':
        case '.':
            return tag;
        case 0xA0:
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            if(endElementName == NULL)
                endElementName = ch;
            tagState = gt;
            return tag;
        default:
            if(('0' <= kar && kar <= '9') || ('A' <= kar && kar <= 'Z') || ('a' <= kar && kar <= 'z') || (kar & 0x80))
                return tag;
            else
                {
                tagState = defx;
                return notag;
                }
        }
    }

static estate gt(const unsigned char * pkar)
    {
    switch(*pkar)
        {
        case '<':
            tagState = lt;
            putOperatorChar('.');
            cbEndElementName();
            putOperatorChar(')');
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = defx;
            putOperatorChar('.');
            cbEndElementName();
            putOperatorChar(')');
            return endoftag;
        case 0xA0:
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            return tag;
        default:
            tagState = defx;
            return notag;
        }
    }

static estate emptytag(const unsigned char * pkar)
    {
    switch(*pkar)
        {
        case '<':
            tagState = lt;
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = defx;
            return endoftag;
        default:
            tagState = defx;
            return notag;
        }
    }

static estate atts(const unsigned char * pkar)
    {
    const int kar = *pkar;
    switch(kar)
        {
        case '<':
            tagState = lt;
            putOperatorChar(')');
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = defx;
            putOperatorChar(')');
            return endoftag;
        case 0xA0:
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            return tag;
        case '/':
            putOperatorChar(',');
            putOperatorChar(')');
            tagState = emptytag;
            return tag;
        default:
            if(':' == kar || ('A' <= kar && kar <= 'Z') || '_' == kar || ('a' <= kar && kar <= 'z') || (kar & 0x80))
                {
                putOperatorChar('(');
                StaRt = ch;
                tagState = namex;
                return tag;
                }
            else
                {
                tagState = defx;
                return notag;
                }
        }
    }

static estate namex(const unsigned char * pkar)
    {
    const int kar = *pkar;
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbEndAttributeName();
            cbEndAttribute();
            putOperatorChar(')');
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = defx;
            cbEndAttributeName();
            cbEndAttribute();
            putOperatorChar(')');
            return endoftag;
        case '-':
        case '_':
        case ':':
        case '.':
            return tag;
        case 0xA0:
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            cbEndAttributeName();
            tagState = atts_or_value;
            return tag;
        case '/':
            cbEndAttributeName();
            cbEndAttribute();
            putOperatorChar(',');
            putOperatorChar(')');
            tagState = emptytag;
            return tag;
        case '=':
            cbEndAttributeName();
            tagState = valuex;
            return tag;
        default:
            if(('0' <= kar && kar <= '9') || ('A' <= kar && kar <= 'Z') || ('a' <= kar && kar <= 'z') || (kar & 0x80))
                return tag;
            else
                {
                tagState = defx;
                return notag;
                }
        }
    }

static estate valuex(const unsigned char * pkar)
    {
    switch(*pkar)
        {
        case '>':
        case '/':
        case '=':
            tagState = defx;
            return notag;
        case 0xA0:
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            return tag;
        case '\'':
            tagState = singlequotes;
            return tag;
        case '"':
            tagState = doublequotes;
            return tag;
        default:
                StaRt = ch;
                tagState = invalue;
                return tag;
        }
    }

static estate atts_or_value(const unsigned char * pkar)
    {
    const int kar = *pkar;
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbEndAttribute();
            putOperatorChar(')');
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = defx;
            cbEndAttribute();
            putOperatorChar(')');
            return endoftag;
        case '-':
        /*case '_':
        case ':':*/
        case '.':
            tagState = defx;
            return notag;
        case 0xA0:
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            return tag;
        case '/':
            cbEndAttribute();
            putOperatorChar(',');
            putOperatorChar(')');
            tagState = emptytag;
            return tag;
        case '=':
            tagState = valuex;
            return tag;
        default:
            if(':' == kar || ('A' <= kar && kar <= 'Z') || '_' == kar || ('a' <= kar && kar <= 'z') || (kar & 0x80))
                {
                cbEndAttribute();
                putOperatorChar('(');
                StaRt = ch;
                tagState = namex;
                return tag;
                }
            else
                {
                tagState = defx;
                return notag;
                }
        }
    }

/* This is far from conforming to and more forgiving than 
https://html.spec.whatwg.org/multipage/syntax.html#unquoted
*/
static estate invalue(const unsigned char * pkar)
    {
    switch(*pkar)
        {
        case '<':
            tagState = lt;
            nxput(StaRt,ch);
            cbEndAttribute();
            putOperatorChar(')');
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = defx;
            nxput(StaRt,ch);
            cbEndAttribute();
            putOperatorChar(')');
            return endoftag;
        case 0xA0:
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            nxput(StaRt,ch);
            cbEndAttribute();
            tagState = atts;
            return tag;
        /* Unsafe to end valuex at first /
           See fx get$("<a href=http://www.cst.dk/esslli2010/resources.html>",HT ML MEM)
           */
        case '/':
            if (*(pkar + 1) == '>')
                {
                nxput(StaRt, ch);
                cbEndAttribute();
                putOperatorChar(',');
                putOperatorChar(')');
                tagState = emptytag;
                return tag;
                }
            /* fall through */
        default:
            return tag;
        }
    }

static estate singlequotes(const unsigned char * pkar)
    {
    StaRt = ch;
    switch(*pkar)
        {
        case '\'':
            nxput(StaRt,ch);
            cbEndAttribute();
            tagState = endvalue;
            return tag;
        default:
            tagState = insinglequotedvalue;
            return tag;
        }
    }

static estate doublequotes(const unsigned char * pkar)
    {
    StaRt = ch;
    switch(*pkar)
        {
        case '\"':
            nxput(StaRt,ch);
            cbEndAttribute();
            tagState = endvalue;
            return tag;
        default:
            tagState = indoublequotedvalue;
            return tag;
        }
    }

static estate insinglequotedvalue(const unsigned char * pkar)
    {
    switch(*pkar)
        {
        case '\'':
            nxput(StaRt,ch);
            cbEndAttribute();
            tagState = endvalue;
            return tag;
        default:
            return tag;
        }
    }

static estate indoublequotedvalue(const unsigned char * pkar)
    {
    switch(*pkar)
        {
        case '\"':
            nxput(StaRt,ch);
            cbEndAttribute();
            tagState = endvalue;
            return tag;
        default:
            return tag;
        }
    }


static estate endvalue(const unsigned char * pkar)
    {
    switch(*pkar)
        {
        case '<':
            tagState = lt;
            putOperatorChar(')');
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = defx;
            putOperatorChar(')');
            return endoftag;
        case 0xA0:
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            tagState = atts;
            return tag;
        case '/':
            putOperatorChar(',');
            putOperatorChar(')');
            tagState = emptytag;
            return tag;
        default:
            tagState = defx;
            return notag;
        }
    }


static unsigned char * elementNameLower;
static unsigned char * elementNameUpper;
static estate scriptOrStyleElement(const unsigned char * pkar) /* <sc or <SC or <Sc or <sC or <st or <ST or <St or <sT */
    {
    const int kar = *pkar;
    if(kar == elementNameLower[scriptstylei] || kar == elementNameUpper[scriptstylei])
        {
        estate ret;
        if(scriptstylei == scriptstyleimax)
            {
            defx = def_cdata;
            tagState = element;
            scriptstylei = 0;
            }
        else
            ++scriptstylei;
        ret = element(pkar);
        /*tagState = scriptOrStyleElement;*/
        return ret;
        }
    else
        {
        scriptstylei = 0;
        }
    return element(pkar);
    }

static unsigned char script[] = "script";
static unsigned char SCRIPT[] = "SCRIPT";
static unsigned char style[] = "style";
static unsigned char STYLE[] = "STYLE";
static estate perhapsScriptOrStyle(const unsigned char * pkar) /* <s or <S */
    {
    estate ret;
    switch(*pkar)
        {
        case 'C':
        case 'c':
            elementNameLower = script+2;
            elementNameUpper = SCRIPT+2;
            elementEndNameLower = script;
            elementEndNameUpper = SCRIPT;
            scriptstyleimax = sizeof(script) - 4;
            ScriptStyleiMax = sizeof(script) - 2;
            ret = element(pkar);
            tagState = scriptOrStyleElement;
            return ret;
        case 'T':
        case 't':
            elementNameLower = style+2;
            elementNameUpper = STYLE+2;
            elementEndNameLower = style;
            elementEndNameUpper = STYLE;
            scriptstyleimax = sizeof(style) - 4;
            ScriptStyleiMax = sizeof(style) - 2;
            ret = element(pkar);
            tagState = scriptOrStyleElement;
            return ret;
        default:
            tagState = element;
        }
    return element(pkar);
    }


static estate markup(const unsigned char * pkar) /* <! */
    {
    switch(*pkar)
        {
        case '<':
            tagState = lt;
            cbEndMarkUp();
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = defx;
            cbEndMarkUp();
            return endoftag;
        case '-':
            tagState = h1;
            return tag;
        case '[':
            tagState = CDATA1;
            return tag;
        case 'D':
            tagState = DOCTYPE1;
            return tag;
        default:
            tagState = unknownmarkup;
            return tag;
        }
    }

static estate unknownmarkup(const unsigned char * pkar) /* <! */
    {
    switch(*pkar)
        {
        case '<':
            tagState = lt;
            cbEndMarkUp();
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = defx;
            cbEndMarkUp();
            return endoftag;
        default:
            return tag;
        }
    }

static estate PrInstr(const unsigned char * pkar)
    {
    if(Xvar)
        switch(*pkar)
            {
            case '?':
                tagState = endPI;
                return tag;
            default:
                return tag;
            }
    else
        switch(*pkar)
            {
            case '>':
                tagState = defx;
                nonTagWithoutEntityUnfolding("?",StaRt+1,ch);
                return endoftag;
            default:
                return tag;
            }
    }

static estate endPI(const unsigned char * pkar)
    {
    switch(*pkar)
        {
        case '>':
            tagState = defx;
            nonTagWithoutEntityUnfolding("?",StaRt+1,ch-1);
            return endoftag;
        case '?':
            return tag;
        default:
            tagState = PrInstr;
            return tag;
        }
    }

static int doctypei = 0;
static estate DOCTYPE1(const unsigned char * pkar) /* <!D */
    {
    const int kar = *pkar;
    static unsigned char octype[] = "OCTYPE";
    switch(*pkar)
        {
        case '<':
            tagState = lt;
            cbStartMarkUp();
            doctypei = 0;
            return endoftag_startoftag;
        case '>':
            tagState = defx;
            doctypei = 0;
            return endoftag;
        default:
            if(kar == octype[doctypei])
                {
                if(doctypei == sizeof(octype) - 2)
                    {
                    tagState = DOCTYPE7;
                    }
                else
                    ++doctypei;
                return tag;
                }
            else
                {
                doctypei = 0;
                tagState = unknownmarkup;
                return tag;
                }
        }
    }


static estate DOCTYPE7(const unsigned char * pkar) /* <!DOCTYPE */
    {
    switch(*pkar)
        {
        case '<':
            tagState = lt;
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = defx;
            cbEndDOCTYPE();
            return endoftag;
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            StaRt = ch;
            tagState = DOCTYPE8;
            return tag;
        default:
            tagState = unknownmarkup;
            return tag;
        }
    }

static estate DOCTYPE8(const unsigned char * pkar) /* <!DOCTYPE S */
    {
    switch(*pkar)
        {
        case '<':
            tagState = lt;
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = defx;
            cbEndDOCTYPE();
            return endoftag;
        case '[':
            tagState = DOCTYPE9;
            return tag;
        default:
            return tag;
        }
    }

static estate DOCTYPE9(const unsigned char * pkar)  /* <!DOCTYPE S [ */
    {
    switch(*pkar)
        {
        case ']':
            tagState = DOCTYPE10;
            return tag;
        default:
            tagState = DOCTYPE9;
            return tag;
        }
    }

static estate DOCTYPE10(const unsigned char * pkar)  /* <!DOCTYPE S [ ] */
    {
    switch(*pkar)
        {
        case '<':
            tagState = lt;
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = defx;
            cbEndDOCTYPE();
            return endoftag;
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
            tagState = DOCTYPE10;
            return tag;
        default:
            tagState = markup;
            return tag;
        }
    }


static int cdatai = 0;
static estate CDATA1(const unsigned char * pkar) /* <![ */
    {
    const int kar = *pkar;
    static unsigned char cdata[] = "CDATA[";
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbStartMarkUp();
            cdatai = 0;
            return endoftag_startoftag;
        case '>':
            tagState = defx;
            cdatai = 0;
            return endoftag;
        default:
            if(cdata[cdatai] == kar)
                {
                if(cdatai == sizeof(cdata) - 2)
                    {
                    tagState = CDATA7;
                    StaRt = ch+1;
                    cdatai = 0;
                    }
                else
                    ++cdatai;
                return tag;
                }
            else
                {
                tagState = unknownmarkup;
                cdatai = 0;
                return tag;
                }
        }
    }

static estate CDATA7(const unsigned char * pkar) /* <![CDATA[ */
    {
    switch(*pkar)
        {
        case ']':
            tagState = CDATA8;
            return tag;
        default:
            return tag;
        }
    }

static estate CDATA8(const unsigned char * pkar) /* <![CDATA[ ] */
    {
    switch(*pkar)
        {
        case ']':
            tagState = CDATA9;
            return tag;
        default:
            tagState = CDATA7;
            return tag;
        }
    }

static estate CDATA9(const unsigned char * pkar) /* <![CDATA[ ]] */
    {
    switch(*pkar)
        {
        case '>':  /* <![CDATA[ ]]> */
            tagState = defx;
            isMarkup = 0;
            nonTagWithoutEntityUnfolding("![CDATA[",StaRt,ch-2);
            return endoftag;
        default:
            tagState = CDATA7;
            return tag;
        }
    }


static estate h1(const unsigned char * pkar) /* <!- */
    {
    switch(*pkar)
        {
        case '<':
            tagState = lt;
            cbStartMarkUp();
            return notag;
        case '>':
            tagState = defx;
            return notag;
        case '-':
            tagState = h2;
            StaRt = ch+1;
            return tag;
        default:
            tagState = unknownmarkup;
            return tag;
        }
    }

static estate h2(const unsigned char * pkar) /* <!-- */
    {
    switch(*pkar)
        {
        case '-':
            tagState = h3;
            return tag;
        default:
            return tag;
        }
    }

static estate h3(const unsigned char * pkar) /* <!--  - */
    {
    switch(*pkar)
        {
        case '-': /* <!-- -- */
            tagState = markup;
            isMarkup = 0;
            nonTagWithoutEntityUnfolding("!--",StaRt,ch-1);
            return tag;
        default:
            tagState = h2;
            return tag;
        }
    }

static estate endtag(const unsigned char * pkar)
    {
    const int kar = *pkar;
    if(':' == kar || ('A' <= kar && kar <= 'Z') || '_' == kar || ('a' <= kar && kar <= 'z') || (kar & 0x80))
        {
        endElementName = NULL;
        tagState = elementonly;
        StaRt = ch;
        return tag;
        }
    else
        {
        tagState = defx;
        return notag;
        }
    }

void XMLtext(FILE * fpi,unsigned char * bron,int trim,int html,int xml)
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
        filesize = strlen((const char *)bron);
        }
    else
        return;
    defx = def_pcdata;
    if(filesize > 0)
        {
        unsigned char * alltext;
        assumeUTF8 = TRUE;
        doctypei = 0;
        cdatai = 0;
        bufx = (unsigned char*)malloc(BUFSIZE);
        glob_p = bufx;
        alltext = (fpi || trim) ? (unsigned char*)malloc(filesize+1) : (unsigned char *)bron;
        HTvar = html;
        Xvar = xml;
        if(bufx && alltext)
            {
            unsigned char * curr_pos;
            unsigned char * endpos;
            estate Seq = notag;
            if(trim)
                {
                unsigned char * p = alltext;
                int whitespace = FALSE;
                if(fpi)
                    {
                    while((kar = getc(fpi)) != EOF)
                        {
                        switch(kar)
                            {
                            case ' ':
                            case '\f':
                            case '\n':
                            case '\r':
                            case '\t':
                                {
                                if(!whitespace)
                                    {
                                    whitespace = TRUE;
                                    *p++ = ' ';
                                    }
                                break;
                                }
                            default:
                                {
                                whitespace = FALSE;
                                *p++ = (unsigned char)kar;
                                }
                            }
                        if(p >= alltext + incs * inc)
                            {
                            size_t dif = p - alltext; 
                            ++incs;                            
                            alltext = (unsigned char*)realloc(alltext,incs * inc);
                            p = alltext + dif;
                            }
                        }
                    }
                else
                    {
                    while((kar = *bron++) != 0)
                        {
                        switch(kar)
                            {
                            case ' ':
                            case '\f':
                            case '\n':
                            case '\r':
                            case '\t':
                                {
                                if(!whitespace)
                                    {
                                    whitespace = TRUE;
                                    *p++ = ' ';
                                    }
                                break;
                                }
                            default:
                                {
                                whitespace = FALSE;
                                *p++ = (unsigned char)kar;
                                }
                            }
                        }
                    }
                *p = '\0';
                }
            else if(fpi)
                {
                if(fpi == stdin)
                    {
                    unsigned char * endp = alltext + incs * inc;
                    unsigned char * p = alltext;
                    while((kar = getc(fpi)) != EOF)
                        {
                        *p++ = (unsigned char)kar;
                        if(p >= endp)
                            {
                            size_t dif = p - alltext; 
                            ++incs;                            
                            alltext = (unsigned char*)realloc(alltext,incs * inc);
                            p = alltext + dif;
                            endp = alltext + incs * inc;
                            }
                        }
                    *p = '\0';
                    }
                else
                    {
                    LONG result = fread(alltext,1,filesize,fpi);
                    assert(result <= filesize); 
                    /* The file is perhaps not opened
                    in binary mode, but in text mode. In that case the number of
                    bytes read can be smaller than the number of bytes in the file.
                    */
                    alltext[result] = '\0';
                    }
                }

            tagState = defx;

            ch = alltext;
            curr_pos = alltext;
            while(*ch)
                {
                while(  *ch 
                     && (( Seq = (*tagState)(ch)) == tag 
                        || Seq == endoftag_startoftag
                        )
                     )
                    {
                    ch++;
                    }
                if(Seq == notag)
                    { /* Not an HTML tag. Backtrack. */
                    endpos = ch;
                    ch = curr_pos;
                    while(ch < endpos)
                        {
                        rawput(*ch);
                        ch++;
                        }
                    }
                else if(Seq == endoftag)
                    {
                    putOperatorChar(' ');
                    ++ch; /* skip > */
                    }
                if(*ch)
                    {
                    while(  *ch 
                         && (Seq = (*tagState)(ch)) == notag
                         )
                        {
                        xput(ch);
                        ++ch;
                        }
                    if(Seq == tag)
                        {
                        curr_pos = ch++; /* skip < */
                        }
                    }
                }
            if(Seq == tag)
                {
                Seq = (*tagState)((const unsigned char *)" ");
                if(Seq == tag)
                    putOperatorChar(')');
                }
#if 0
                { /* Incomplete SGML tag. Backtrack. */
                endpos = ch;
                ch = curr_pos;
                while(ch < endpos)
                    {
                    rawput(*ch);
                    ch++;
                    }
                }
#endif
            }
        if(bufx)
            free(bufx);
        if(alltext && alltext != (unsigned char *)bron)
            free(alltext);
        }
    }
