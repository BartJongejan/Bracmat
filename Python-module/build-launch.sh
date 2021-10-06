#!/bin/bash
rm prythat.c*
rm -r build
python setup.py build_ext --inplace
python launch.py
