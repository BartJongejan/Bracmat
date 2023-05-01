#ifndef CHARPUT_H
#define CHARPUT_H

#ifdef __SYMBIAN32__
/* #define DEFAULT_INPUT_BUFFER_SIZE 0x100*/ /* If too high you get __chkstk error. Stack = 8K only! */
/* #define DEFAULT_INPUT_BUFFER_SIZE 0x7F00*/
#define DEFAULT_INPUT_BUFFER_SIZE 0x2000
#else
#ifdef _MSC_VER
#define DEFAULT_INPUT_BUFFER_SIZE 0x7F00 /* Microsoft C allows 32k automatic data */

#else
#ifdef __BORLANDC__
#if __BORLANDC__ >= 0x500
#define DEFAULT_INPUT_BUFFER_SIZE 0x7000
#else
#define DEFAULT_INPUT_BUFFER_SIZE 0x7FFC
#endif
#else
#define DEFAULT_INPUT_BUFFER_SIZE 0x7FFC
#endif
#endif
#endif

typedef struct inputBuffer
    {
    unsigned char* buffer;
    unsigned int cutoff : 8;    /* Set to true if very long string does not fit in buffer of size DEFAULT_INPUT_BUFFER_SIZE */
    unsigned int mallocallocated : 8; /* True if allocated with malloc. Otherwise on stack (except EPOC). */
    } inputBuffer;

extern inputBuffer* InputArray;
extern unsigned char* inputBufferPointer;
extern unsigned char* maxInputBufferPointer; /* inputBufferPointer <= maxInputBufferPointer,
                            if inputBufferPointer == maxInputBufferPointer, don't assign to *inputBufferPointer */

void lput(int c);
void putOperatorChar(int c);
void putLeafChar(int c);

#endif
