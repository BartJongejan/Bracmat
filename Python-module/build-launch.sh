#!/bin/bash
rm prythat.c*
rm -r build
pushd ../singlesource && make && popd
python setup.py build_ext --inplace
python launch.py
