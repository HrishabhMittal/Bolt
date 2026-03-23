#!/bin/bash
echo ------------ MAKING COMPILER -----------------
make
echo ------------ MAKING VM "&" UTILS -------------
cd bvm
make -B
cd ..
echo ------------ RUNNING ON EXAMPLE --------------
./build/boltc examples/main.bolt main
./bvm/build/objdump main
./bvm/build/bvm main
