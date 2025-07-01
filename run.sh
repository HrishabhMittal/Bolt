#!/bin/bash
echo ------------ MAKING COMPILER -----------------
make
echo ------------ COMPILING PROGRAM -----------------
./boltc examples/ex.txt
nasm -f elf64 a.asm -o main.o
ld main.o -o main
echo ------------ RUNNNING PROGRAM -----------------
./main
echo exitcode $?
echo ------------ CLEANUP -----------------
if [[ "$1" == "keep" ]]; then
mv a.asm assembly
else
rm a.asm
fi
rm main.o main
