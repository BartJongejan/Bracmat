@echo off
rem Builds the bracmat package in place, runs the tests (if pytest is
rem installed) and then the launch.py demo.
rem Needs: Microsoft C++ Build Tools and Cython (python -m pip install cython).
cd /d "%~dp0"
if exist bracmat\_core.c del bracmat\_core.c
if exist build rmdir /s /q build
del /q bracmat\_core*.pyd 2>nul
python setup.py build_ext --inplace || exit /b 1
python -c "import pytest" 2>nul && (python -m pytest -q || exit /b 1)
python launch.py
