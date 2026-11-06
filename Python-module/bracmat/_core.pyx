# bracmat/_core.pyx - the compiled part of the 'bracmat' Python package.
# This code is translated to C by Cython. See bracmat/__init__.py for the
# public interface and documentation.
"""Compiled core of the 'bracmat' package. Use 'import bracmat' instead."""
import sys
import json
import codecs
import traceback

__all__ = ["init", "final", "evaluate", "HolyGrail", "namespace", "Session"]

# The callbacks are called from Bracmat while the GIL is released, so they
# are declared 'with gil' and the function pointer types 'nogil'.
ctypedef int (*f_type_intvoid)() noexcept nogil
ctypedef void (*f_type_int)(int) noexcept nogil
ctypedef void (*f_type_void)() noexcept nogil
ctypedef void (*f_type_char)(const char *) noexcept nogil
ctypedef const char * (*f_type_charchar)(const char *) noexcept nogil

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
cdef int WinInFunc() noexcept with gil:
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
cdef void WinOutFunc(int a) noexcept with gil:
    try:
        # Bracmat passes bytes as (possibly signed) char: mask to 0..255.
        text = _out_decoder.decode(bytes((a & 0xFF,)))
        if text:
            sys.stdout.write(text)
    except BaseException:
        traceback.print_exc()

# Call back function for flushing stdout. Activated by put$ and lst$
cdef void WinFlushFunc() noexcept with gil:
    try:
        sys.stdout.flush()
    except BaseException:
        traceback.print_exc()

# The namespace in which Ni$ executes statements and Ni!$ evaluates
# expressions. It persists between calls, so a variable set by Ni$ can be
# read by a later Ni$ or Ni!$. Set by init().
_namespace = {}

# Call back function Ni$"<stmnt>;<stmnt>;.." for executing one or more Python statements
cdef void NiFunc(const char * strng) noexcept with gil:
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
cdef const char * NiiFunc(const char * strng) noexcept with gil:
    global _nii_result
    try:
        py = (<bytes>strng).decode('UTF-8')
        val = eval(py, _namespace)
        _nii_result = json.dumps(val, ensure_ascii=False).encode('UTF-8')
        return _nii_result
    except BaseException:
        traceback.print_exc()
        return NULL

# startStruct only has the Ni and Nii members if PYTHONINTERFACE is defined.
# Without it, the struct below would silently not match the C definition.
cdef extern from *:
    """
    #ifndef PYTHONINTERFACE
    #error "bracmat._core must be compiled with PYTHONINTERFACE defined (see setup.py)"
    #endif
    """

cdef extern from "bracmat.h":
    ctypedef struct startStruct "startStruct":
        f_type_intvoid WinIn
        f_type_int WinOut
        f_type_void WinFlush
        f_type_char Ni
        f_type_charchar Nii

    cdef extern int startProc(startStruct * init)
    cdef extern void stringEval(const char * s,const char ** out,int * err) nogil
    cdef extern void endProc() nogil

# Bracmat has global state and is not reentrant. These flags guard against
# calling HolyGrail() before init() or after final(), calling init() twice,
# and calling HolyGrail(), init() or final() while Bracmat is evaluating:
# from Python code that Bracmat runs via Ni$ or Ni!$, or from another thread.
# The flags are only read and written while holding the GIL.
cdef bint _initialised = False
cdef bint _busy = False

cdef _check_not_busy(name):
    if _busy:
        raise RuntimeError("bracmat.%s() cannot be called while Bracmat is evaluating "
                           "(from code run by Ni$ or Ni!$, or from another thread)" % name)

def init(namespace=None):
    """Start Bracmat. Call once, before the first call to evaluate().

    'namespace' is the dict in which Ni$ and Ni!$ run Python code, for
    example globals() of the calling module. Without it, a new empty dict
    is used.

    Raises RuntimeError if Bracmat is already initialised or busy,
    TypeError if 'namespace' is not a dict, and MemoryError if Bracmat
    cannot be initialised.

    Bracmat's C state can be initialised only once per process. Calling
    init() again after final() makes evaluate() usable again, but Bracmat's
    variables and memory are those of the earlier session.
    """
    global _namespace, _initialised
    _check_not_busy("init")
    if _initialised:
        raise RuntimeError("Bracmat is already initialised; call bracmat.final() first")
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

def namespace():
    """Return the dict in which Ni$ and Ni!$ run Python code."""
    return _namespace

def HolyGrail(Str):
    """Evaluate the Bracmat expression 'Str' and return the result as a string.

    A result that is not an atom (e.g. a sum or a list) is returned as
    Bracmat would print it. Strings are passed to and from Bracmat as UTF-8.
    Bytes that are not valid UTF-8 are mapped to lone surrogates and back
    ('surrogateescape'), so that a result can be passed to evaluate() again
    without loss.

    Other Python threads can run while Bracmat evaluates.

    Raises RuntimeError if Bracmat is not initialised or is busy, and
    ValueError if the expression cannot be parsed.
    """
    global _busy
    cdef const char * Sout = NULL
    cdef const char * cs
    cdef int Err = 0
    _check_not_busy("HolyGrail")
    if not _initialised:
        raise RuntimeError("Bracmat is not initialised; call bracmat.init() first")
    cdef bytes s = Str.encode('UTF-8', 'surrogateescape')
    cs = s
    _busy = True
    try:
        with nogil:
            stringEval(cs,&Sout,&Err)
    finally:
        _busy = False
    if Err != 0:
        raise ValueError("Bracmat could not evaluate the expression (error code %d)" % Err)
    if Sout == NULL:
        return ""
    cdef bytes py_string = Sout
    return py_string.decode('UTF-8', 'surrogateescape')

evaluate = HolyGrail

def final():
    """Stop Bracmat. Call after the last call to evaluate().

    Runs Bracmat's CloseDown function, if defined (only the first time in a
    process). Does nothing if Bracmat is not initialised.

    Raises RuntimeError if Bracmat is busy.
    """
    global _busy, _initialised
    _check_not_busy("final")
    if not _initialised:
        return
    _initialised = False
    _busy = True
    try:
        with nogil:
            endProc()
    finally:
        _busy = False

class Session:
    """Context manager for a Bracmat session.

    ::

        with bracmat.Session() as b:
            b.evaluate("Ni$\\"x = 6 * 7\\"")
            print(b.namespace["x"])          # 42

    Entering calls init(namespace), leaving calls final(). Only one session
    can be active at a time.
    """

    def __init__(self, namespace=None):
        self._initial_namespace = namespace

    def __enter__(self):
        init(self._initial_namespace)
        return self

    def __exit__(self, exc_type, exc, tb):
        final()
        return False

    def evaluate(self, expr):
        """Evaluate a Bracmat expression; see bracmat.evaluate()."""
        return HolyGrail(expr)

    @property
    def namespace(self):
        """The dict in which Ni$ and Ni!$ run Python code."""
        return _namespace
