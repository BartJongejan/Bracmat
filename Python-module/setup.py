# setup.py
# 
from distutils.core import setup
from Cython.Build import cythonize
from distutils.extension import Extension

sourcefiles = ['../safe/bracmatso.c', '../src/xml.c', '../src/json.c', 'prythat.pyx']

extensions = [Extension("prythat", sourcefiles, extra_compile_args=["-I../src/", "-D_CRT_SECURE_NO_WARNINGS","-DPYTHONINTERFACE"])]

setup(
    ext_modules = cythonize(extensions)
)
