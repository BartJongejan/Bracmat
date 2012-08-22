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
attributes can be empty (no =[value])
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
extern void putLeaveChar(int c);
extern char * putCodePoint(unsigned LONG val,char * s);

typedef enum {notag,tag,endoftag,endoftag_startoftag} estate;
static estate (*tagState)(int kar);

static int Put(char * c);
static int (*xput)(char * c) = Put;

static int rawput(int c)
    {
    putLeaveChar(c);
    return TRUE;
    }

static int nrawput(char * c)
    {
    while(*c)
        if(!rawput(*c++))
            return FALSE;
    return TRUE;
    }
/*
Handle such things:  &amp;security-level;

In this case, &amp; must not be converted to &, because &security-level; is
(probably) not a declared entity reference name.
 
[4]   	NameStartChar	   ::=   	":" | [A-Z] | "_" | [a-z] | [#xC0-#xD6] | [#xD8-#xF6] | [#xF8-#x2FF] | [#x370-#x37D] | [#x37F-#x1FFF] | [#x200C-#x200D] | [#x2070-#x218F] | [#x2C00-#x2FEF] | [#x3001-#xD7FF] | [#xF900-#xFDCF] | [#xFDF0-#xFFFD] | [#x10000-#xEFFFF]
[4a]   	NameChar	   ::=   	NameStartChar | "-" | "." | [0-9] | #xB7 | [#x0300-#x036F] | [#x203F-#x2040]
[5]   	Name	   ::=   	NameStartChar (NameChar)*


The charref thing can first be done after the string following it is sure
not to be an entity reference.

If you want &amp; always to be converted to &, #define ALWAYSREPLACEAMP 1
*/
#define ALWAYSREPLACEAMP 1

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

static char * buf;
static char * p;
#if !ALWAYSREPLACEAMP
static char * q;
#endif
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

static int decimal(int c)
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
    if(decimal(c))
        {
        namechar = decimal;
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

#if !ALWAYSREPLACEAMP
static int charrefamp(char * c)
    {
    if(*c == ';')
        { /* Found something like &amp;security-level; */
        *p = '\0';
        rawput('&');
        nrawput(buf);
        rawput(';');
        p = buf;
        xput = Put;
        return FALSE;
        }
    else if(!namechar(*c))
        { /* Found something like &amp;security-level# */
        rawput('&');
        *p = '\0';
        nrawput(q);
        if(*c > 0)
            rawput(*c);
        p = buf;
        xput = Put;
        return FALSE;
        }
    else if(p < buf+BUFSIZE-1)
        {
        *p++ = *c;
        }
    return TRUE;
    }
#endif

typedef struct Entity {char * ent;int code;} Entity;
static Entity entities[]={
{"AElig",198},	/* latin capital letter AE = latin capital ligature AE, U+00C6 ISOlat1 */
{"Aacute",193},	/* latin capital letter A with acute, U+00C1 ISOlat1 */
{"Acirc",194},	/* latin capital letter A with circumflex, U+00C2 ISOlat1 */
{"Agrave",192},	/* latin capital letter A with grave = latin capital letter A grave, U+00C0 ISOlat1 */
{"Alpha",913},	/* greek capital letter alpha, U+0391 */
{"Aring",197},	/* latin capital letter A with ring above = latin capital letter A ring, U+00C5 ISOlat1 */
{"Atilde",195},	/* latin capital letter A with tilde, U+00C3 ISOlat1 */
{"Auml",196},	/* latin capital letter A with diaeresis, U+00C4 ISOlat1 */
{"Beta",914},	/* greek capital letter beta, U+0392 */
{"Ccedil",199},	/* latin capital letter C with cedilla, U+00C7 ISOlat1 */
{"Chi",935},	/* greek capital letter chi, U+03A7 */
{"Dagger",8225},	/* double dagger, U+2021 ISOpub */
{"Delta",916},	/* greek capital letter delta, U+0394 ISOgrk3 */
{"ETH",208},	/* latin capital letter ETH, U+00D0 ISOlat1 */
{"Eacute",201},	/* latin capital letter E with acute, U+00C9 ISOlat1 */
{"Ecirc",202},	/* latin capital letter E with circumflex, U+00CA ISOlat1 */
{"Egrave",200},	/* latin capital letter E with grave, U+00C8 ISOlat1 */
{"Epsilon",917},	/* greek capital letter epsilon, U+0395 */
{"Eta",919},	/* greek capital letter eta, U+0397 */
{"Euml",203},	/* latin capital letter E with diaeresis, U+00CB ISOlat1 */
{"Gamma",915},	/* greek capital letter gamma, U+0393 ISOgrk3 */
{"Iacute",205},	/* latin capital letter I with acute, U+00CD ISOlat1 */
{"Icirc",206},	/* latin capital letter I with circumflex, U+00CE ISOlat1 */
{"Igrave",204},	/* latin capital letter I with grave, U+00CC ISOlat1 */
{"Iota",921},	/* greek capital letter iota, U+0399 */
{"Iuml",207},	/* latin capital letter I with diaeresis, U+00CF ISOlat1 */
{"Kappa",922},	/* greek capital letter kappa, U+039A */
{"Lambda",923},	/* greek capital letter lambda, U+039B ISOgrk3 */
{"Mu",924},	/* greek capital letter mu, U+039C */
{"Ntilde",209},	/* latin capital letter N with tilde, U+00D1 ISOlat1 */
{"Nu",925},	/* greek capital letter nu, U+039D */
{"OElig",338},	/* latin capital ligature OE, U+0152 ISOlat2 */
{"Oacute",211},	/* latin capital letter O with acute, U+00D3 ISOlat1 */
{"Ocirc",212},	/* latin capital letter O with circumflex, U+00D4 ISOlat1 */
{"Ograve",210},	/* latin capital letter O with grave, U+00D2 ISOlat1 */
{"Omega",937},	/* greek capital letter omega, U+03A9 ISOgrk3 */
{"Omicron",927},	/* greek capital letter omicron, U+039F */
{"Oslash",216},	/* latin capital letter O with stroke = latin capital letter O slash, U+00D8 ISOlat1 */
{"Otilde",213},	/* latin capital letter O with tilde, U+00D5 ISOlat1 */
{"Ouml",214},	/* latin capital letter O with diaeresis, U+00D6 ISOlat1 */
{"Phi",934},	/* greek capital letter phi, U+03A6 ISOgrk3 */
{"Pi",928},	/* greek capital letter pi, U+03A0 ISOgrk3 */
{"Prime",8243},	/* double prime = seconds = inches, U+2033 ISOtech */
{"Psi",936},	/* greek capital letter psi, U+03A8 ISOgrk3 */
{"Rho",929},	/* greek capital letter rho, U+03A1 */
{"Scaron",352},	/* latin capital letter S with caron, U+0160 ISOlat2 */
{"Sigma",931},	/* greek capital letter sigma, U+03A3 ISOgrk3 */
{"THORN",222},	/* latin capital letter THORN, U+00DE ISOlat1 */
{"Tau",932},	/* greek capital letter tau, U+03A4 */
{"Theta",920},	/* greek capital letter theta, U+0398 ISOgrk3 */
{"Uacute",218},	/* latin capital letter U with acute, U+00DA ISOlat1 */
{"Ucirc",219},	/* latin capital letter U with circumflex, U+00DB ISOlat1 */
{"Ugrave",217},	/* latin capital letter U with grave, U+00D9 ISOlat1 */
{"Upsilon",933},	/* greek capital letter upsilon, U+03A5 ISOgrk3 */
{"Uuml",220},	/* latin capital letter U with diaeresis, U+00DC ISOlat1 */
{"Xi",926},	/* greek capital letter xi, U+039E ISOgrk3 */
{"Yacute",221},	/* latin capital letter Y with acute, U+00DD ISOlat1 */
{"Yuml",376},	/* latin capital letter Y with diaeresis, U+0178 ISOlat2 */
{"Zeta",918},	/* greek capital letter zeta, U+0396 */
{"aacute",225},	/* latin small letter a with acute, U+00E1 ISOlat1 */
{"acirc",226},	/* latin small letter a with circumflex, U+00E2 ISOlat1 */
{"acute",180},	/* acute accent = spacing acute, U+00B4 ISOdia */
{"aelig",230},	/* latin small letter ae = latin small ligature ae, U+00E6 ISOlat1 */
{"agrave",224},	/* latin small letter a with grave = latin small letter a grave, U+00E0 ISOlat1 */
{"alefsym",8501},	/* alef symbol = first transfinite cardinal, U+2135 NEW */
{"alpha",945},	/* greek small letter alpha, U+03B1 ISOgrk3 */
/*{"amp",38},*/	/* ampersand, U+0026 ISOnum */
{"and",8743},	/* logical and = wedge, U+2227 ISOtech */
{"ang",8736},	/* angle, U+2220 ISOamso */
{"aring",229},	/* latin small letter a with ring above = latin small letter a ring, U+00E5 ISOlat1 */
{"asymp",8776},	/* almost equal to = asymptotic to, U+2248 ISOamsr */
{"atilde",227},	/* latin small letter a with tilde, U+00E3 ISOlat1 */
{"auml",228},	/* latin small letter a with diaeresis, U+00E4 ISOlat1 */
{"bdquo",8222},	/* double low-9 quotation mark, U+201E NEW */
{"beta",946},	/* greek small letter beta, U+03B2 ISOgrk3 */
{"brvbar",166},	/* broken bar = broken vertical bar, U+00A6 ISOnum */
{"bull",8226},	/* bullet = black small circle, U+2022 ISOpub */
{"cap",8745},	/* intersection = cap, U+2229 ISOtech */
{"ccedil",231},	/* latin small letter c with cedilla, U+00E7 ISOlat1 */
{"cedil",184},	/* cedilla = spacing cedilla, U+00B8 ISOdia */
{"cent",162},	/* cent sign, U+00A2 ISOnum */
{"chi",967},	/* greek small letter chi, U+03C7 ISOgrk3 */
{"circ",710},	/* modifier letter circumflex accent, U+02C6 ISOpub */
{"clubs",9827},	/* black club suit = shamrock, U+2663 ISOpub */
{"cong",8773},	/* approximately equal to, U+2245 ISOtech */
{"copy",169},	/* copyright sign, U+00A9 ISOnum */
{"crarr",8629},	/* downwards arrow with corner leftwards = carriage return, U+21B5 NEW */
{"cup",8746},	/* union = cup, U+222A ISOtech */
{"curren",164},	/* currency sign, U+00A4 ISOnum */
{"dArr",8659},	/* downwards double arrow, U+21D3 ISOamsa */
{"dagger",8224},	/* dagger, U+2020 ISOpub */
{"darr",8595},	/* downwards arrow, U+2193 ISOnum */
{"deg",176},	/* degree sign, U+00B0 ISOnum */
{"delta",948},	/* greek small letter delta, U+03B4 ISOgrk3 */
{"diams",9830},	/* black diamond suit, U+2666 ISOpub */
{"divide",247},	/* division sign, U+00F7 ISOnum */
{"eacute",233},	/* latin small letter e with acute, U+00E9 ISOlat1 */
{"ecirc",234},	/* latin small letter e with circumflex, U+00EA ISOlat1 */
{"egrave",232},	/* latin small letter e with grave, U+00E8 ISOlat1 */
{"empty",8709},	/* empty set = null set = diameter, U+2205 ISOamso */
{"emsp",8195},	/* em space, U+2003 ISOpub */
{"ensp",8194},	/* en space, U+2002 ISOpub */
{"epsilon",949},	/* greek small letter epsilon, U+03B5 ISOgrk3 */
{"equiv",8801},	/* identical to, U+2261 ISOtech */
{"eta",951},	/* greek small letter eta, U+03B7 ISOgrk3 */
{"eth",240},	/* latin small letter eth, U+00F0 ISOlat1 */
{"euml",235},	/* latin small letter e with diaeresis, U+00EB ISOlat1 */
{"euro",8364},	/* euro sign, U+20AC NEW */
{"exist",8707},	/* there exists, U+2203 ISOtech */
{"fnof",402},	/* latin small f with hook = function = florin, U+0192 ISOtech */
{"forall",8704},	/* for all, U+2200 ISOtech */
{"frac12",189},	/* vulgar fraction one half = fraction one half, U+00BD ISOnum */
{"frac14",188},	/* vulgar fraction one quarter = fraction one quarter, U+00BC ISOnum */
{"frac34",190},	/* vulgar fraction three quarters = fraction three quarters, U+00BE ISOnum */
{"frasl",8260},	/* fraction slash, U+2044 NEW */
{"gamma",947},	/* greek small letter gamma, U+03B3 ISOgrk3 */
{"ge",8805},	/* greater-than or equal to, U+2265 ISOtech */
/*{"gt",62},*/	/* greater-than sign, U+003E ISOnum */
{"hArr",8660},	/* left right double arrow, U+21D4 ISOamsa */
{"harr",8596},	/* left right arrow, U+2194 ISOamsa */
{"hearts",9829},	/* black heart suit = valentine, U+2665 ISOpub */
{"hellip",8230},	/* horizontal ellipsis = three dot leader, U+2026 ISOpub */
{"iacute",237},	/* latin small letter i with acute, U+00ED ISOlat1 */
{"icirc",238},	/* latin small letter i with circumflex, U+00EE ISOlat1 */
{"iexcl",161},	/* inverted exclamation mark, U+00A1 ISOnum */
{"igrave",236},	/* latin small letter i with grave, U+00EC ISOlat1 */
{"image",8465},	/* blackletter capital I = imaginary part, U+2111 ISOamso */
{"infin",8734},	/* infinity, U+221E ISOtech */
{"int",8747},	/* integral, U+222B ISOtech */
{"iota",953},	/* greek small letter iota, U+03B9 ISOgrk3 */
{"iquest",191},	/* inverted question mark = turned question mark, U+00BF ISOnum */
{"isin",8712},	/* element of, U+2208 ISOtech */
{"iuml",239},	/* latin small letter i with diaeresis, U+00EF ISOlat1 */
{"kappa",954},	/* greek small letter kappa, U+03BA ISOgrk3 */
{"lArr",8656},	/* leftwards double arrow, U+21D0 ISOtech */
{"lambda",955},	/* greek small letter lambda, U+03BB ISOgrk3 */
{"lang",9001},	/* left-pointing angle bracket = bra, U+2329 ISOtech */
{"laquo",171},	/* left-pointing double angle quotation mark = left pointing guillemet, U+00AB ISOnum */
{"larr",8592},	/* leftwards arrow, U+2190 ISOnum */
{"lceil",8968},	/* left ceiling = apl upstile, U+2308 ISOamsc */
{"ldquo",8220},	/* left double quotation mark, U+201C ISOnum */
{"le",8804},	/* less-than or equal to, U+2264 ISOtech */
{"lfloor",8970},	/* left floor = apl downstile, U+230A ISOamsc */
{"lowast",8727},	/* asterisk operator, U+2217 ISOtech */
{"loz",9674},	/* lozenge, U+25CA ISOpub */
{"lrm",8206},	/* left-to-right mark, U+200E NEW RFC 2070 */
{"lsaquo",8249},	/* single left-pointing angle quotation mark, U+2039 ISO proposed */
{"lsquo",8216},	/* left single quotation mark, U+2018 ISOnum */
/*{"lt",60},*/	/* less-than sign, U+003C ISOnum */
{"macr",175},	/* macron = spacing macron = overline = APL overbar, U+00AF ISOdia */
{"mdash",8212},	/* em dash, U+2014 ISOpub */
{"micro",181},	/* micro sign, U+00B5 ISOnum */
{"middot",183},	/* middle dot = Georgian comma = Greek middle dot, U+00B7 ISOnum */
{"minus",8722},	/* minus sign, U+2212 ISOtech */
{"mu",956},	/* greek small letter mu, U+03BC ISOgrk3 */
{"nabla",8711},	/* nabla = backward difference, U+2207 ISOtech */
{"nbsp",160},	/* no-break space = non-breaking space, U+00A0 ISOnum */
{"ndash",8211},	/* en dash, U+2013 ISOpub */
{"ne",8800},	/* not equal to, U+2260 ISOtech */
{"ni",8715},	/* contains as member, U+220B ISOtech */
{"not",172},	/* not sign, U+00AC ISOnum */
{"notin",8713},	/* not an element of, U+2209 ISOtech */
{"nsub",8836},	/* not a subset of, U+2284 ISOamsn */
{"ntilde",241},	/* latin small letter n with tilde, U+00F1 ISOlat1 */
{"nu",957},	/* greek small letter nu, U+03BD ISOgrk3 */
{"oacute",243},	/* latin small letter o with acute, U+00F3 ISOlat1 */
{"ocirc",244},	/* latin small letter o with circumflex, U+00F4 ISOlat1 */
{"oelig",339},	/* latin small ligature oe, U+0153 ISOlat2 */
{"ograve",242},	/* latin small letter o with grave, U+00F2 ISOlat1 */
{"oline",8254},	/* overline = spacing overscore, U+203E NEW */
{"omega",969},	/* greek small letter omega, U+03C9 ISOgrk3 */
{"omicron",959},	/* greek small letter omicron, U+03BF NEW */
{"oplus",8853},	/* circled plus = direct sum, U+2295 ISOamsb */
{"or",8744},	/* logical or = vee, U+2228 ISOtech */
{"ordf",170},	/* feminine ordinal indicator, U+00AA ISOnum */
{"ordm",186},	/* masculine ordinal indicator, U+00BA ISOnum */
{"oslash",248},	/* latin small letter o with stroke, = latin small letter o slash, U+00F8 ISOlat1 */
{"otilde",245},	/* latin small letter o with tilde, U+00F5 ISOlat1 */
{"otimes",8855},	/* circled times = vector product, U+2297 ISOamsb */
{"ouml",246},	/* latin small letter o with diaeresis, U+00F6 ISOlat1 */
{"para",182},	/* pilcrow sign = paragraph sign, U+00B6 ISOnum */
{"part",8706},	/* partial differential, U+2202 ISOtech */
{"permil",8240},	/* per mille sign, U+2030 ISOtech */
{"perp",8869},	/* up tack = orthogonal to = perpendicular, U+22A5 ISOtech */
{"phi",966},	/* greek small letter phi, U+03C6 ISOgrk3 */
{"pi",960},	/* greek small letter pi, U+03C0 ISOgrk3 */
{"piv",982},	/* greek pi symbol, U+03D6 ISOgrk3 */
{"plusmn",177},	/* plus-minus sign = plus-or-minus sign, U+00B1 ISOnum */
{"pound",163},	/* pound sign, U+00A3 ISOnum */
{"prime",8242},	/* prime = minutes = feet, U+2032 ISOtech */
{"prod",8719},	/* n-ary product = product sign, U+220F ISOamsb */
{"prop",8733},	/* proportional to, U+221D ISOtech */
{"psi",968},	/* greek small letter psi, U+03C8 ISOgrk3 */
/*{"quot",34},*/	/* quotation mark = APL quote, U+0022 ISOnum */
{"rArr",8658},	/* rightwards double arrow, U+21D2 ISOtech */
{"radic",8730},	/* square root = radical sign, U+221A ISOtech */
{"rang",9002},	/* right-pointing angle bracket = ket, U+232A ISOtech */
{"raquo",187},	/* right-pointing double angle quotation mark = right pointing guillemet, U+00BB ISOnum */
{"rarr",8594},	/* rightwards arrow, U+2192 ISOnum */
{"rceil",8969},	/* right ceiling, U+2309 ISOamsc */
{"rdquo",8221},	/* right double quotation mark, U+201D ISOnum */
{"real",8476},	/* blackletter capital R = real part symbol, U+211C ISOamso */
{"reg",174},	/* registered sign = registered trade mark sign, U+00AE ISOnum */
{"rfloor",8971},	/* right floor, U+230B ISOamsc */
{"rho",961},	/* greek small letter rho, U+03C1 ISOgrk3 */
{"rlm",8207},	/* right-to-left mark, U+200F NEW RFC 2070 */
{"rsaquo",8250},	/* single right-pointing angle quotation mark, U+203A ISO proposed */
{"rsquo",8217},	/* right single quotation mark, U+2019 ISOnum */
{"sbquo",8218},	/* single low-9 quotation mark, U+201A NEW */
{"scaron",353},	/* latin small letter s with caron, U+0161 ISOlat2 */
{"sdot",8901},	/* dot operator, U+22C5 ISOamsb */
{"sect",167},	/* section sign, U+00A7 ISOnum */
{"shy",173},	/* soft hyphen = discretionary hyphen, U+00AD ISOnum */
{"sigma",963},	/* greek small letter sigma, U+03C3 ISOgrk3 */
{"sigmaf",962},	/* greek small letter final sigma, U+03C2 ISOgrk3 */
{"sim",8764},	/* tilde operator = varies with = similar to, U+223C ISOtech */
{"spades",9824},	/* black spade suit, U+2660 ISOpub */
{"sub",8834},	/* subset of, U+2282 ISOtech */
{"sube",8838},	/* subset of or equal to, U+2286 ISOtech */
{"sum",8721},	/* n-ary sumation, U+2211 ISOamsb */
{"sup",8835},	/* superset of, U+2283 ISOtech */
{"sup1",185},	/* superscript one = superscript digit one, U+00B9 ISOnum */
{"sup2",178},	/* superscript two = superscript digit two = squared, U+00B2 ISOnum */
{"sup3",179},	/* superscript three = superscript digit three = cubed, U+00B3 ISOnum */
{"supe",8839},	/* superset of or equal to, U+2287 ISOtech */
{"szlig",223},	/* latin small letter sharp s = ess-zed, U+00DF ISOlat1 */
{"tau",964},	/* greek small letter tau, U+03C4 ISOgrk3 */
{"there4",8756},	/* therefore, U+2234 ISOtech */
{"theta",952},	/* greek small letter theta, U+03B8 ISOgrk3 */
{"thetasym",977},	/* greek small letter theta symbol, U+03D1 NEW */
{"thinsp",8201},	/* thin space, U+2009 ISOpub */
{"thorn",254},	/* latin small letter thorn, U+00FE ISOlat1 */
{"tilde",732},	/* small tilde, U+02DC ISOdia */
{"times",215},	/* multiplication sign, U+00D7 ISOnum */
{"trade",8482},	/* trade mark sign, U+2122 ISOnum */
{"uArr",8657},	/* upwards double arrow, U+21D1 ISOamsa */
{"uacute",250},	/* latin small letter u with acute, U+00FA ISOlat1 */
{"uarr",8593},	/* upwards arrow, U+2191 ISOnum*/
{"ucirc",251},	/* latin small letter u with circumflex, U+00FB ISOlat1 */
{"ugrave",249},	/* latin small letter u with grave, U+00F9 ISOlat1 */
{"uml",168},	/* diaeresis = spacing diaeresis, U+00A8 ISOdia */
{"upsih",978},	/* greek upsilon with hook symbol, U+03D2 NEW */
{"upsilon",965},	/* greek small letter upsilon, U+03C5 ISOgrk3 */
{"uuml",252},	/* latin small letter u with diaeresis, U+00FC ISOlat1 */
{"weierp",8472},	/* script capital P = power set = Weierstrass p, U+2118 ISOamso */
{"xi",958},	/* greek small letter xi, U+03BE ISOgrk3 */
{"yacute",253},	/* latin small letter y with acute, U+00FD ISOlat1 */
{"yen",165},	/* yen sign = yuan sign, U+00A5 ISOnum */
{"yuml",255},	/* latin small letter y with diaeresis, U+00FF ISOlat1 */
{"zeta",950},	/* greek small letter zeta, U+03B6 ISOgrk3 */
{"zwj",8205},	/* zero width joiner, U+200D NEW RFC 2070 */
{"zwnj",8204}};	/* zero width non-joiner, U+200C NEW RFC 2070 */

int compareEntities(const void * a, const void * b)
    {
    return strcmp(((Entity*)a)->ent,((Entity*)b)->ent);
    }

static int HT = 0;
static int charref(char * c)
    {
    if(*c == ';')
        {
        *p = '\0';
        if(  !strcmp(buf,"amp")
          || !strcmp(buf,"#38")
          || !strcmp(buf,"#x26")
          )
#if ALWAYSREPLACEAMP
            rawput('&');
#else
            {
            *p++ = ';';
            q = p;
            namechar = entity;
            xput = charrefamp;
            return TRUE;
            }
#endif
        else if(!strcmp(buf,"apos"))
            rawput('\'');
        else if(!strcmp(buf,"quot"))
            {
            rawput('\"');
            }
        else if(!strcmp(buf,"lt"))
            rawput('<');
        else if(!strcmp(buf,"gt"))
            rawput('>');
        else if(buf[0] == '#')
            {
            unsigned long N;
            char tmp[22];
            N = (buf[1] == 'x') ? strtoul(buf+2,NULL,16) : strtoul(buf+1,NULL,10);
            p = buf;
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
            if(HT)
                {
                Entity * pItem;
                Entity key;
                key.ent = buf;
                pItem = (Entity*)bsearch( &key
                                        , entities
                                        , sizeof(entities)/sizeof(entities[0])
                                        , sizeof(entities[0])
                                        , compareEntities
                                        );
                if (pItem!=NULL)
                    {
                    char tmp[22];
                    p = buf;
                    xput = Put;
                    if(putCodePoint(pItem->code,tmp))
                        {
                        return nrawput(tmp);
                        }
                    }
                }
            rawput('&');
            nrawput(buf);
            rawput(';');
            p = buf;
            xput = Put;
            return FALSE;
            }
        p = buf;
        xput = Put;
        }
    else if(!namechar(*c))
        {
        rawput('&');
        *p = '\0';
        nrawput(buf);
        if(*c > 0)
            rawput(*c);
        p = buf;
        xput = Put;
        return FALSE;
        }
    else if(p < buf+BUFSIZE-1)
        {
        *p++ = *c;
        }
    return TRUE;
    }

static int Put(char * c)
    {
    if(*c == '&')
        {
        xput = charref;
        namechar = entity;
        return TRUE;
        }
    return rawput(*c);
    }

static void flush()
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

static void nxputWithoutEntityUnfolding(char * start,char *end)
    {
    for(;start < end;++start)
        rawput(*start);
    }

static void nonTagWithoutEntityUnfolding(char * kind,char * start,char * end)
    {
    nrawput(kind);
    putOperatorChar('.');
    nxputWithoutEntityUnfolding(start,end);
    putOperatorChar(')');
    }

static void nonTag(char * kind,char * start,char * end)
    {
    nrawput(kind);
    putOperatorChar('.');
    nxput(start,end);
    putOperatorChar(')');
    }

static char * ch;
static char * startScript = 0;
static char * startMarkup = NULL;
static char * startComment = NULL;
static char * startDOCTYPE = NULL;
static char * startCDATA = NULL;
static char * startElementName = NULL;
static char * startAttributeName = NULL;
static char * startValue = NULL;

static void cbStartMarkUp()/* called when <!X has been read, where X != [ or - */
    {
    flush();
    startMarkup = ch+2;
    putOperatorChar(' ');
    putOperatorChar('(');
    }

static void cbEndMarkUp()/* called when > has been read */
    {
    if(startMarkup)
        nonTag("!",startMarkup,ch);
    }

static void cbEndDOCTYPE()/* called when > has been read */
    {
    startMarkup = NULL;
    nonTagWithoutEntityUnfolding("!DOCTYPE",startDOCTYPE,ch);
    }

static void cbEndElementName()
    {
    nxput(startElementName,ch);
    putOperatorChar('.');
    }

static void cbEndAttributeName()
    {
    nxput(startAttributeName,ch);
    putOperatorChar('.');
    }

static void cbEndAttribute()
    {
    putOperatorChar(')');
    putOperatorChar(' ');
    }

static estate def(int kar);
static estate lt(int kar);
static estate element(int kar);
static estate elementonly(int kar);
static estate gt(int kar);
static estate emptytag(int kar);
static estate atts(int kar);
static estate name(int kar);
static estate value(int kar);
static estate atts_or_value(int kar);
static estate invalue(int kar);
static estate singlequotes(int kar);
static estate doublequotes(int kar);
static estate insinglequotedvalue(int kar);
static estate indoublequotedvalue(int kar);
static estate endvalue(int kar);
static estate markup(int kar); /* <! */
static estate unknownmarkup(int kar);
static estate script(int kar);
static estate endscript(int kar);
static estate DOCTYPE1(int kar); /* <!D */
static estate DOCTYPE7(int kar); /* <!DOCTYPE */
static estate DOCTYPE8(int kar); /* <!DOCTYPE S */
static estate DOCTYPE9(int kar); /* <!DOCTYPE S [ */
static estate DOCTYPE10(int kar); /* <!DOCTYPE S [ ] */
static estate CDATA1(int kar); /* <![ */
static estate CDATA7(int kar); /* <![CDATA[ */
static estate CDATA8(int kar); /* <![CDATA[ ] */
static estate CDATA9(int kar); /* <![CDATA[ ]] */
static estate h1(int kar); /* <!- */
static estate h2(int kar); /* <!-- */
static estate h3(int kar); /* <!--  - */
static estate endtag(int kar);

static estate def(int kar)
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbStartMarkUp();
            return tag;
        default:
            return notag;
        }
    }

static estate lt(int kar)
    {
    switch(kar)
        {
        case '>':
            tagState = def;
            return notag;
        case '!':
            tagState = markup;
            return tag;
        case '?':
            startScript = ch;
            tagState = script;
            return tag;
        case '/':
            putOperatorChar('.');
            tagState = endtag;
        case 0xA0:
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            return tag;
        default:
            if(('A' <= kar && kar <= 'Z') || ('a' <= kar && kar <= 'z') || (kar & 0x80))
                {
                tagState = element;
                startElementName = ch;
                return tag;
                }
            else
                {
                tagState = def;
                return notag;
                }
        }
    }

static estate element(int kar)
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbEndElementName();
            putOperatorChar(')');
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = def;
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
        case '\t':
        case '\n':
        case '\r':
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
                return tag;
            else
                {
                tagState = def;
                return notag;
                }
        }
    }

static estate elementonly(int kar)
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbEndElementName();
            putOperatorChar(')');
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = def;
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
        case '\t':
        case '\n':
        case '\r':
            cbEndElementName();
            tagState = gt;
            return tag;
        default:
            if(('0' <= kar && kar <= '9') || ('A' <= kar && kar <= 'Z') || ('a' <= kar && kar <= 'z') || (kar & 0x80))
                return tag;
            else
                {
                tagState = def;
                return notag;
                }
        }
    }

static estate gt(int kar)
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            putOperatorChar(')');
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = def;
            putOperatorChar(')');
            return endoftag;
        case 0xA0:
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            return tag;
        default:
            tagState = def;
            return notag;
        }
    }

static estate emptytag(int kar)
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = def;
            return endoftag;
        case 0xA0:
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            return tag;
        default:
            tagState = def;
            return notag;
        }
    }

static estate atts(int kar)
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            putOperatorChar(')');
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = def;
            putOperatorChar(')');
            return endoftag;
        case 0xA0:
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            return tag;
        case '/':
            putOperatorChar(',');
            putOperatorChar(')');
            tagState = emptytag;
            return tag;
        default:
            if(('A' <= kar && kar <= 'Z') || ('a' <= kar && kar <= 'z') || (kar & 0x80))
                {
                putOperatorChar('(');
                startAttributeName = ch;
                tagState = name;
                return tag;
                }
            else
                {
                tagState = def;
                return notag;
                }
        }
    }

static estate name(int kar)
    {
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
            tagState = def;
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
        case '\t':
        case '\n':
        case '\r':
            cbEndAttributeName();
            tagState = atts_or_value;
            return tag;
        case '/':
            cbEndAttributeName();
            cbEndAttribute();
            putOperatorChar(',');
            tagState = emptytag;
            return tag;
        case '=':
            cbEndAttributeName();
            tagState = value;
            return tag;
        default:
            if(('0' <= kar && kar <= '9') || ('A' <= kar && kar <= 'Z') || ('a' <= kar && kar <= 'z') || (kar & 0x80))
                return tag;
            else
                {
                tagState = def;
                return notag;
                }
        }
    }

static estate value(int kar)
    {
    switch(kar)
        {
        case '>':
        case '/':
        case '=':
            tagState = def;
            return notag;
        case 0xA0:
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            return tag;
        case '\'':
            tagState = singlequotes;
            return tag;
        case '"':
            tagState = doublequotes;
            return tag;
        default:
            if(('0' <= kar && kar <= '9') || ('A' <= kar && kar <= 'Z') || ('a' <= kar && kar <= 'z') || (kar & 0x80))
                {
                startValue = ch;
                tagState = invalue;
                return tag;
                }
            else
                {
                tagState = def;
                return notag;
                }
        }
    }

static estate atts_or_value(int kar)
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbEndAttribute();
            putOperatorChar(')');
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = def;
            cbEndAttribute();
            putOperatorChar(')');
            return endoftag;
        case '-':
        case '_':
        case ':':
        case '.':
            tagState = def;
            return notag;
        case 0xA0:
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            return tag;
        case '/':
            cbEndAttribute();
            putOperatorChar(',');
            putOperatorChar(')');
            tagState = emptytag;
            return tag;
        case '=':
            tagState = value;
            return tag;
        default:
            if(('A' <= kar && kar <= 'Z') || ('a' <= kar && kar <= 'z') || (kar & 0x80))
                {
                cbEndAttribute();
                putOperatorChar('(');
                startAttributeName = ch;
                tagState = name;
                return tag;
                }
            else
                {
                tagState = def;
                return notag;
                }
        }
    }

static estate invalue(int kar)
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            nxput(startValue,ch);
            cbEndAttribute();
            putOperatorChar(')');
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = def;
            nxput(startValue,ch);
            cbEndAttribute();
            putOperatorChar(')');
            return endoftag;
        case 0xA0:
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            nxput(startValue,ch);
            cbEndAttribute();
            tagState = atts;
            return tag;
        default:
            if(('0' <= kar && kar <= '9') || ('A' <= kar && kar <= 'Z') || ('a' <= kar && kar <= 'z') || (kar & 0x80))
                {
                return tag;
                }
            else
                {
                tagState = def;
                return notag;
                }
        }
    }

static estate singlequotes(int kar)
    {
    startValue = ch;
    switch(kar)
        {
        case '\'':
            nxput(startValue,ch);
            cbEndAttribute();
            tagState = endvalue;
            return tag;
        default:
            tagState = insinglequotedvalue;
            return tag;
        }
    }

static estate doublequotes(int kar)
    {
    startValue = ch;
    switch(kar)
        {
        case '\"':
            nxput(startValue,ch);
            cbEndAttribute();
            tagState = endvalue;
            return tag;
        default:
            tagState = indoublequotedvalue;
            return tag;
        }
    }

static estate insinglequotedvalue(int kar)
    {
    switch(kar)
        {
        case '\'':
            nxput(startValue,ch);
            cbEndAttribute();
            tagState = endvalue;
            return tag;
        default:
            return tag;
        }
    }

static estate indoublequotedvalue(int kar)
    {
    switch(kar)
        {
        case '\"':
            nxput(startValue,ch);
            cbEndAttribute();
            tagState = endvalue;
            return tag;
        default:
            return tag;
        }
    }


static estate endvalue(int kar)
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            putOperatorChar(')');
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = def;
            putOperatorChar(')');
            return endoftag;
        case 0xA0:
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            tagState = atts;
            return tag;
        case '/':
            putOperatorChar(',');
            putOperatorChar(')');
            tagState = emptytag;
            return tag;
        default:
            tagState = def;
            return notag;
        }
    }

static estate markup(int kar) /* <! */
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbEndMarkUp();
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = def;
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

static estate unknownmarkup(int kar) /* <! */
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbEndMarkUp();
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = def;
            cbEndMarkUp();
            return endoftag;
        default:
            return tag;
        }
    }

static estate script(int kar)
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            nonTagWithoutEntityUnfolding("?",startScript+1,ch-1);
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = def;
            nonTagWithoutEntityUnfolding("?",startScript+1,ch-1);
            return endoftag;
        case '?':
            tagState = endscript;
            return tag;
        default:
            return tag;
        }
    }

static estate endscript(int kar)
    {
    switch(kar)
        {
        case '>':
            tagState = def;
            nonTagWithoutEntityUnfolding("?",startScript+1,ch-1);
            return endoftag;
        default:
            tagState = CDATA7;
            return tag;
        }
    }

static int doctypei = 0;
static estate DOCTYPE1(int kar) /* <!D */
    {
    static char octype[] = "OCTYPE";
    static int doctypei = 0;
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbStartMarkUp();
            doctypei = 0;
            return endoftag_startoftag;
        case '>':
            tagState = def;
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


static estate DOCTYPE7(int kar) /* <!DOCTYPE */
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = def;
            cbEndDOCTYPE();
            return endoftag;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            startDOCTYPE = ch+1;
            tagState = DOCTYPE8;
            return tag;
        default:
            tagState = unknownmarkup;
            return tag;
        }
    }

static estate DOCTYPE8(int kar) /* <!DOCTYPE S */
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = def;
            cbEndDOCTYPE();
            return endoftag;
        case '[':
            tagState = DOCTYPE9;
            return tag;
        default:
            return tag;
        }
    }

static estate DOCTYPE9(int kar)  /* <!DOCTYPE S [ */
    {
    switch(kar)
        {
        case ']':
            tagState = DOCTYPE10;
            return tag;
        default:
            tagState = DOCTYPE9;
            return tag;
        }
    }

static estate DOCTYPE10(int kar)  /* <!DOCTYPE S [ ] */
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = def;
            cbEndDOCTYPE();
            return endoftag;
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            tagState = DOCTYPE10;
            return tag;
        default:
            tagState = markup;
            return tag;
        }
    }


static int cdatai = 0;
static estate CDATA1(int kar) /* <![ */
    {
    static char cdata[] = "CDATA[";
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbStartMarkUp();
            cdatai = 0;
            return endoftag_startoftag;
        case '>':
            tagState = def;
            cdatai = 0;
            return endoftag;
        default:
            if(cdata[cdatai] == kar)
                {
                if(cdatai == sizeof(cdata) - 2)
                    {
                    tagState = CDATA7;
                    startCDATA = ch+1;
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

static estate CDATA7(int kar) /* <![CDATA[ */
    {
    switch(kar)
        {
        case ']':
            tagState = CDATA8;
            return tag;
        default:
            return tag;
        }
    }

static estate CDATA8(int kar) /* <![CDATA[ ] */
    {
    switch(kar)
        {
        case ']':
            tagState = CDATA9;
            return tag;
        default:
            tagState = CDATA7;
            return tag;
        }
    }

static estate CDATA9(int kar) /* <![CDATA[ ]] */
    {
    switch(kar)
        {
        case '>':  /* <![CDATA[ ]]> */
            tagState = def;
            startMarkup = NULL;
            nonTagWithoutEntityUnfolding("![CDATA[",startCDATA,ch-2);
            return endoftag;
        default:
            tagState = CDATA7;
            return tag;
        }
    }


static estate h1(int kar) /* <!- */
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            cbStartMarkUp();
            return notag;
        case '>':
            tagState = def;
            return notag;
        case '-':
            tagState = h2;
            startComment = ch+1;
            return tag;
        default:
            tagState = unknownmarkup;
            return tag;
        }
    }

static estate h2(int kar) /* <!-- */
    {
    switch(kar)
        {
        case '-':
            tagState = h3;
            return tag;
        default:
            return tag;
        }
    }

static estate h3(int kar) /* <!--  - */
    {
    switch(kar)
        {
        case '-': /* <!-- -- */
            tagState = markup;
            startMarkup = NULL;
            nonTagWithoutEntityUnfolding("!--",startComment,ch-1);
            return tag;
        default:
            tagState = h2;
            return tag;
        }
    }

static estate endtag(int kar)
    {
    switch(kar)
        {
        case '<':
            tagState = lt;
            putOperatorChar(')');
            cbStartMarkUp();
            return endoftag_startoftag;
        case '>':
            tagState = def;
            putOperatorChar(')');
            return endoftag;
        case 0xA0:
        case ' ':
        case '\t':
        case '\n':
        case '\r':
            return tag;
        default:
            if(('A' <= kar && kar <= 'Z') || ('a' <= kar && kar <= 'z') || (kar & 0x80))
                {
                tagState = elementonly;
                startElementName = ch;
                return tag;
                }
            else
                {
                tagState = def;
                return notag;
                }
        }
    }

int XMLtext(FILE * fpi,char * bron,int trim,int html)
    {
    int kar;
    int retval = 0;
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
        doctypei = 0;
        cdatai = 0;
        retval = 1;
        buf = (char*)malloc(BUFSIZE);
        p = buf;
#if !ALWAYSREPLACEAMP
        q = buf;
#endif
        alltext = (fpi || trim) ? (char*)malloc(filesize+1) : bron;
        HT = html;
        if(buf && alltext)
            {
            char * curr_pos;
            char * endpos;
            estate Seq = notag;
            if(trim)
                {
                char * p = alltext;
                int whitespace = FALSE;
                if(fpi)
                    {
                    while((kar = getc(fpi)) != EOF)
                        {
                        switch(kar)
                            {
                            case ' ':
                            case '\t':
                            case '\n':
                            case '\r':
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
                                *p++ = (char)kar;
                                }
                            }
                        if(p >= alltext + incs * inc)
                            {
                            size_t dif = p - alltext; 
                            ++incs;                            
                            alltext = realloc(alltext,incs * inc);
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
                            case '\t':
                            case '\n':
                            case '\r':
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
                                *p++ = (char)kar;
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
                    char * endp = alltext + incs * inc;
                    char * p = alltext;
                    while((kar = getc(fpi)) != EOF)
                        {
                        *p++ = (char)kar;
                        if(p >= endp)
                            {
                            size_t dif = p - alltext; 
                            ++incs;                            
                            alltext = realloc(alltext,incs * inc);
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

            tagState = def;

            ch = alltext;

            curr_pos = alltext;
            while(*ch)
                {
                while(  *ch 
                     && (( Seq = (*tagState)(*ch)) == tag 
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
                         && (Seq = (*tagState)(*ch)) == notag
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
                { /* Incomplete SGML tag. Backtrack. */
                endpos = ch;
                ch = curr_pos;
                while(ch < endpos)
                    {
                    rawput(*ch);
                    ch++;
                    }
                }
            }
        else
            retval = 0;
        if(buf)
            free(buf);
        if(alltext && alltext != bron)
            free(alltext);
        }
    return retval;
    }
