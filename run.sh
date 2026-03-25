#!/bin/bash
echo ------------ RUNNING ON EXAMPLE --------------
./build/boltc examples main
./bvm/build/objdump main
./bvm/build/bvm main
rm main
