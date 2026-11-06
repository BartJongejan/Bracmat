"""Call the Bracmat evaluator from Python, and Python from Bracmat.

Basic use::

    import bracmat
    with bracmat.Session() as b:
        print(b.evaluate("1+1"))          # '2'

or, without the context manager::

    bracmat.init()
    print(bracmat.evaluate("1+1"))        # '2'
    bracmat.final()

HolyGrail() is another name for evaluate().

Bracmat code can call back into Python:

* ``Ni$"<statements>"`` executes Python statements.
* ``Ni!$"<expression>"`` evaluates a Python expression and returns the
  result as JSON text.

Both run in one persistent namespace dict, see init() and namespace().

Bracmat has global state: only one Bracmat session exists per process, and
it cannot be used from two threads at the same time. Bracmat's C state is
initialised only once per process: init() after final() continues the
earlier session.
"""

from ._core import (
    HolyGrail,
    Session,
    evaluate,
    final,
    init,
    namespace,
)

try:
    from ._version import __version__  # written by setup.py
except ImportError:
    __version__ = "unknown"

__all__ = ["init", "final", "evaluate", "HolyGrail", "namespace", "Session"]
