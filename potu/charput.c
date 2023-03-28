#include "charput.h"
#include "platformdependentdefs.h"
#include "globals.h"
#include "nodedefs.h"
#include "nonnodetypes.h"
#include "memory.h"
#include "numbercheck.h"
#include "copy.h"
#include "objectnode.h"
#include "filewrite.h"
#include "wipecopy.h"
#include "result.h"
#include "eval.h"
#include "xml.h"
#include "json.h"
#include "nodeutil.h"
#include "writeerr.h"
#include <stdarg.h>
#include <string.h>
#include <assert.h>


void lput(int c)
    {
    if(inputBufferPointer >= maxInputBufferPointer)
        {
        inputBuffer* newInputArray;
        unsigned char* input_buffer;
        unsigned char* dest;
        int len;
        size_t L;

        for(len = 0; InputArray[++len].buffer;)
            ;
        /* len = index of last element in InputArray array */

        input_buffer = InputArray[len - 1].buffer;
        /* The last string (probably on the stack, not on the heap) */

        while(inputBufferPointer > input_buffer && optab[*--inputBufferPointer] == NOOP)
            ;
        /* inputBufferPointer points at last operator (where string can be split) or at
           the start of the string. */

        newInputArray = (inputBuffer*)bmalloc(__LINE__, (2 + len) * sizeof(inputBuffer));
        /* allocate new array one element bigger than the previous. */

        newInputArray[len + 1].buffer = NULL;
        newInputArray[len + 1].cutoff = FALSE;
        newInputArray[len + 1].mallocallocated = FALSE;
        newInputArray[len].buffer = input_buffer;
        /*The buffer pointers with lower index are copied further down.*/

            /*Printf("input_buffer %p\n",input_buffer);*/

        newInputArray[len].cutoff = FALSE;
        newInputArray[len].mallocallocated = FALSE;
        /*The active buffer is still the one declared in input(),
          so on the stack (except under EPOC).*/
        --len; /* point at the second last element, the one that got filled up. */
        if(inputBufferPointer == input_buffer)
            {
            /* copy the full content of input_buffer to the second last element */
            dest = newInputArray[len].buffer = (unsigned char*)bmalloc(__LINE__, DEFAULT_INPUT_BUFFER_SIZE);
            strncpy((char*)dest, (char*)input_buffer, DEFAULT_INPUT_BUFFER_SIZE - 1);
            dest[DEFAULT_INPUT_BUFFER_SIZE - 1] = '\0';
            /* Make a notice that the element's string is cut-off */
            newInputArray[len].cutoff = TRUE;
            newInputArray[len].mallocallocated = TRUE;
            }
        else
            {
            ++inputBufferPointer; /* inputBufferPointer points at first character after the operator */
            /* maxInputBufferPointer - inputBufferPointer >= 0 */
            L = (size_t)(inputBufferPointer - input_buffer);
            dest = newInputArray[len].buffer = (unsigned char*)bmalloc(__LINE__, L + 1);
            strncpy((char*)dest, (char*)input_buffer, L);
            dest[L] = '\0';
            newInputArray[len].cutoff = FALSE;
            newInputArray[len].mallocallocated = TRUE;

            /* Now remove the substring up to inputBufferPointer from input_buffer */
            L = (size_t)(maxInputBufferPointer - inputBufferPointer);
            strncpy((char*)input_buffer, (char*)inputBufferPointer, L);
            input_buffer[L] = '\0';
            inputBufferPointer = input_buffer + L;
            }

        /* Copy previous element's fields */
        while(len)
            {
            --len;
            newInputArray[len].buffer = InputArray[len].buffer;
            newInputArray[len].cutoff = InputArray[len].cutoff;
            newInputArray[len].mallocallocated = InputArray[len].mallocallocated;
            }
        bfree(InputArray);
        InputArray = newInputArray;
        }
    assert(inputBufferPointer <= maxInputBufferPointer);
    *inputBufferPointer++ = (unsigned char)c;
    }

/* referenced from xml.c json.c */
void putOperatorChar(int c)
/* c == parenthesis, operator of flag */
    {
    lput(c);
    }

/* referenced from xml.c json.c */
void putLeafChar(int c)
/* c == any character that should end as part of an atom (string) */
    {
    if(c & 0x80)
        lput(0x7F);
    lput(c | 0x80);
    }

