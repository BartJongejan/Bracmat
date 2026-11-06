#!/bin/bash
# Builds the prythat module in place, runs the tests (if pytest is
# installed) and then the launch.py demo.
# Needs: a C compiler, the Python development headers, and Cython
# (python3 -m pip install cython).
set -e
cd "$(dirname "$0")"
PYTHON=${PYTHON:-python3}
rm -rf prythat.c prythat*.so build
"$PYTHON" setup.py build_ext --inplace
if "$PYTHON" -c "import pytest" 2>/dev/null; then
    "$PYTHON" -m pytest -q
fi
"$PYTHON" launch.py
