#!/bin/bash
echo ------------ RUNNING ON EXAMPLE --------------
./build/boltc "$1" main
./bvm/build/objdump main
./bvm/build/bvm main
rm main
