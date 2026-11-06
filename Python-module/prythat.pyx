# brapy.pyx - Python Module, this code will be translated to C by Cython.
import sys
import json
import traceback

ctypedef int (*f_type_intvoid)() noexcept
ctypedef void (*f_type_int)(int) noexcept
ctypedef void (*f_type_void)() noexcept
ctypedef void (*f_type_char)(const char *) noexcept
ctypedef const char * (*f_type_charchar)(const char *) noexcept

# The callbacks below are called from C code, which cannot handle Python
# exceptions. Each callback therefore catches every exception, prints the
# traceback to stderr and returns a value that Bracmat can handle.

# Call back function for fetching a single byte from stdin. Activated by get$.
cdef int WinInFunc() noexcept:
    try:
        return ord(sys.stdin.read(1))
    except BaseException:
        traceback.print_exc()
        return -1 # EOF

# Call back function for writing a single byte to stdout. Activated by put$ and lst$.
cdef void WinOutFunc(int a) noexcept:
    try:
        sys.stdout.write(chr(a))
    except BaseException:
        traceback.print_exc()

# Call back function for flushing stdout. Activated by put$ and lst$
cdef void WinFlushFunc() noexcept:
    try:
        sys.stdout.flush()
    except BaseException:
        traceback.print_exc()

# Call back function Ni$"<stmnt>;<stmnt>;.." for executing one or more Python statements
cdef void NiFunc(const char * strng) noexcept:
    try:
        py = (<bytes>strng).decode('UTF-8')
        exec(py, globals(),locals())
    except BaseException:
        traceback.print_exc()

# The result of the last Ni!$ call. The C string returned by NiiFunc points
# into this bytes object, so it must stay alive until Bracmat has copied it.
cdef bytes _nii_result = b""

# Call back function Ni!$"<expr>" for evaluating a Python expression.
# Returns NULL if Python raised an exception; Ni!$ then fails.
cdef const char * NiiFunc(const char * strng) noexcept:
    global _nii_result
    try:
        py = (<bytes>strng).decode('UTF-8')
        val = eval(py,globals(),locals())
        _nii_result = json.dumps(val).encode('UTF-8')
        return _nii_result
    except BaseException:
        traceback.print_exc()
        return NULL

cdef extern from "bracmat.h":
    ctypedef struct startStruct "startStruct":
        f_type_intvoid WinIn
        f_type_int WinOut
        f_type_void WinFlush
        f_type_char Ni
        f_type_charchar Nii

    cdef extern int startProc(startStruct * init)
    cdef extern void stringEval(const char * s,const char ** out,int * err)
    cdef extern void endProc()

# Function to be called once, before the first call to HolyGrail()
def init():
    cdef startStruct Init
    Init.WinIn = WinInFunc
    Init.WinOut = WinOutFunc
    Init.WinFlush = WinFlushFunc
    Init.Ni = NiFunc
    Init.Nii = NiiFunc
    startProc(&Init)

# Function to evaluate a Bracmat expression.
# Raises ValueError if the expression cannot be parsed.
def HolyGrail(Str):
    cdef const char * Sout = NULL
    cdef int Err = 0
    stringEval(bytes(Str,'iso8859-1'),&Sout,&Err)
    if Err != 0:
        raise ValueError("Bracmat could not evaluate the expression (error code %d)" % Err)
    if Sout == NULL:
        return ""
    cdef bytes py_string = Sout
    return py_string.decode('UTF-8')

# Function to be called after the last call to HolyGrail()
def final():
    endProc()

