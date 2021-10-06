# brapy.pyx - Python Module, this code will be translated to C by Cython.
import sys
import json

ctypedef int (*f_type_intvoid)()
ctypedef void (*f_type_int)(int)
ctypedef void (*f_type_void)()
ctypedef void (*f_type_char)(const char *)
ctypedef const char * (*f_type_charchar)(const char *)

# Call back function for fetching a single byte from stdin. Activated by get$.
cdef int WinInFunc():
    return ord(sys.stdin.read(1))

# Call back function for writing a single byte to stdout. Activated by put$ and lst$.
cdef void WinOutFunc(int a):
    sys.stdout.write(chr(a))

# Call back function for flushing stdout. Activated by put$ and lst$
cdef void WinFlushFunc():
    sys.stdout.flush()

# Call back function Ni$"<stmnt>;<stmnt>;.." for executing one or more Python statements
cdef void NiFunc(const char * strng):
    cdef bytes py_string = strng
    py = py_string.decode('UTF-8')
    exec(py, globals(),locals())

# Call back function Ni!$"<expr>" for evaluating a Python expression
cdef const char * NiiFunc(const char * strng):
    cdef bytes py_string = strng
    py = py_string.decode('UTF-8')
    val = eval(py,globals(),locals())
    JsOn = json.dumps(val)
    return JsOn.encode('UTF-8')

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

# Function to evaluate a Bracmat expression
def HolyGrail(Str):
    cdef const char * Sout
    cdef int Err
    stringEval(bytes(Str,'iso8859-1'),&Sout,&Err)
    cdef bytes py_string = Sout
    return py_string.decode('UTF-8')

# Function to be called after the last call to HolyGrail()
def final():
    endProc()
    
