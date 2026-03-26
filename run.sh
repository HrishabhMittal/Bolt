#!/bin/bash
echo ------------ RUNNING ON EXAMPLE --------------
cd build
./boltc ../examples main
../bvm/build/stdprint fmt
../bvm/build/objdump main
../bvm/build/bvm main print
rm main print
