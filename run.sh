#!/bin/bash
echo ------------ RUNNING ON EXAMPLE --------------
./build/boltc examples/e2 main
./bvm/build/stdprint main
./bvm/build/objdump main
./bvm/build/bvm main print
