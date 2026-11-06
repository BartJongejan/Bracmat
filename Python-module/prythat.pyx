# brapy.pyx - Python Module, this code will be translated to C by Cython.
import sys
import json
import codecs
import traceback

ctypedef int (*f_type_intvoid)() noexcept
ctypedef void (*f_type_int)(int) noexcept
ctypedef void (*f_type_void)() noexcept
ctypedef void (*f_type_char)(const char *) noexcept
ctypedef const char * (*f_type_charchar)(const char *) noexcept

# The callbacks below are called from C code, which cannot handle Python
# exceptions. Each callback therefore catches every exception, prints the
# traceback to stderr and returns a value that Bracmat can handle.

# Bracmat reads and writes UTF-8 one byte at a time, while sys.stdin and
# sys.stdout are text streams. Input characters are therefore encoded to
# UTF-8 bytes and handed out one by one, and output bytes are collected by
# an incremental decoder until they form complete characters. This also
# works when sys.stdin/sys.stdout have no binary .buffer (IDLE, Jupyter).

# UTF-8 bytes of the last character read from stdin, not yet given to Bracmat.
cdef bytes _in_pending = b""
_out_decoder = codecs.getincrementaldecoder('UTF-8')(errors='replace')

# Call back function for fetching a single byte from stdin. Activated by get$.
cdef int WinInFunc() noexcept:
    global _in_pending
    try:
        if not _in_pending:
            ch = sys.stdin.read(1)
            if not ch:
                return -1 # EOF
            _in_pending = ch.encode('UTF-8', 'surrogateescape')
        b = _in_pending[0]
        _in_pending = _in_pending[1:]
        return b
    except BaseException:
        traceback.print_exc()
        return -1 # EOF

# Call back function for writing a single byte to stdout. Activated by put$ and lst$.
cdef void WinOutFunc(int a) noexcept:
    try:
        # Bracmat passes bytes as (possibly signed) char: mask to 0..255.
        text = _out_decoder.decode(bytes((a & 0xFF,)))
        if text:
            sys.stdout.write(text)
    except BaseException:
        traceback.print_exc()

# Call back function for flushing stdout. Activated by put$ and lst$
cdef void WinFlushFunc() noexcept:
    try:
        sys.stdout.flush()
    except BaseException:
        traceback.print_exc()

# The namespace in which Ni$ executes statements and Ni!$ evaluates
# expressions. It persists between calls, so a variable set by Ni$ can be
# read by a later Ni$ or Ni!$. Set by init().
_namespace = {}

# Call back function Ni$"<stmnt>;<stmnt>;.." for executing one or more Python statements
cdef void NiFunc(const char * strng) noexcept:
    try:
        py = (<bytes>strng).decode('UTF-8')
        exec(py, _namespace)
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
        val = eval(py, _namespace)
        _nii_result = json.dumps(val, ensure_ascii=False).encode('UTF-8')
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

# Bracmat has global state and is not reentrant. These flags guard against
# calling HolyGrail() before init() or after final(), calling init() twice,
# and calling HolyGrail(), init() or final() from Python code that Bracmat is
# running via Ni$ or Ni!$.
cdef bint _initialised = False
cdef bint _busy = False

cdef _check_not_busy(name):
    if _busy:
        raise RuntimeError("prythat.%s() cannot be called while Bracmat is evaluating "
                           "(e.g. from code run by Ni$ or Ni!$)" % name)

# Function to be called once, before the first call to HolyGrail()
# The optional 'namespace' is the dict in which Ni$ and Ni!$ run Python code,
# for example globals() of the calling module. Without it, a new empty dict
# is used.
# Note: Bracmat's C state can be initialised only once per process. Calling
# init() again after final() makes HolyGrail() usable again, but Bracmat's
# variables and memory are those of the earlier session.
def init(namespace=None):
    global _namespace, _initialised
    _check_not_busy("init")
    if _initialised:
        raise RuntimeError("prythat is already initialised; call prythat.final() first")
    if namespace is None:
        namespace = {}
    elif not isinstance(namespace, dict):
        raise TypeError("namespace must be a dict")
    cdef startStruct Init
    Init.WinIn = WinInFunc
    Init.WinOut = WinOutFunc
    Init.WinFlush = WinFlushFunc
    Init.Ni = NiFunc
    Init.Nii = NiiFunc
    # startProc returns 1 on first initialisation, 2 if Bracmat was already
    # initialised earlier in this process, and 0 on failure.
    if startProc(&Init) == 0:
        raise MemoryError("Bracmat could not be initialised")
    _namespace = namespace
    _initialised = True

# Returns the dict in which Ni$ and Ni!$ run Python code.
def namespace():
    return _namespace

# Function to evaluate a Bracmat expression.
# Raises ValueError if the expression cannot be parsed.
# Strings are passed to and from Bracmat as UTF-8. Bytes that are not valid
# UTF-8 are mapped to lone surrogates and back ('surrogateescape'), so that a
# result can be passed to HolyGrail again without loss.
def HolyGrail(Str):
    global _busy
    cdef const char * Sout = NULL
    cdef int Err = 0
    _check_not_busy("HolyGrail")
    if not _initialised:
        raise RuntimeError("prythat is not initialised; call prythat.init() first")
    s = Str.encode('UTF-8', 'surrogateescape')
    _busy = True
    try:
        stringEval(s,&Sout,&Err)
    finally:
        _busy = False
    if Err != 0:
        raise ValueError("Bracmat could not evaluate the expression (error code %d)" % Err)
    if Sout == NULL:
        return ""
    cdef bytes py_string = Sout
    return py_string.decode('UTF-8', 'surrogateescape')

# Function to be called after the last call to HolyGrail().
# Does nothing if prythat is not initialised.
def final():
    global _busy, _initialised
    _check_not_busy("final")
    if not _initialised:
        return
    _initialised = False
    _busy = True
    try:
        endProc()
    finally:
        _busy = False

