# setup.py
# 
from distutils.core import setup
from Cython.Build import cythonize
from distutils.extension import Extension

sourcefiles = ['../bracmatso.c', '../xml.c', '../json.c', 'prythat.pyx']

extensions = [Extension("prythat", sourcefiles, extra_compile_args=["-D_CRT_SECURE_NO_WARNINGS ","-DPYTHONINTERFACE"])]

setup(
    ext_modules = cythonize(extensions)
)
