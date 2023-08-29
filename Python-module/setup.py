# setup.py
# 
from setuptools import setup
from Cython.Build import cythonize
from setuptools.extension import Extension

sourcefiles = ['../safe/bracmatso.c', 'prythat.pyx']

extensions = [Extension("prythat", sourcefiles, extra_compile_args=["-I../singlesource/","-lm","-D_CRT_SECURE_NO_WARNINGS","-DPYTHONINTERFACE"])]

setup(
    ext_modules = cythonize(extensions, emit_linenums=True, compiler_directives={'language_level': 3})
)
