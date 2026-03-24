#!/bin/bash
echo ------------ MAKING COMPILER -----------------
make
echo ------------ MAKING VM "&" UTILS -------------
cd bvm
make -B
cd ..
