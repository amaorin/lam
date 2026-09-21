#!/bin/bash

cd "$(dirname -- "${BASH_SOURCE[0]}")"

mkdir -p build
cd build

clang -o lam -Og -g ../src/main.c
