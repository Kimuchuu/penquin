#!/bin/sh

set -e

echo "[building]"
./build.sh

echo "[compiling]"
# build/penquin ./res/definition.pq
# build/penquin ./res/puts.pq
# build/penquin ./res/c_exit.pq
# build/penquin ./res/declare.pq
# build/penquin ./res/variables.pq
# build/penquin ./res/import.pq
# build/penquin ./res/expression.pq
# build/penquin ./res/getchar.pq
# build/penquin ./res/while.pq
# build/penquin ./res/if.pq
# build/penquin ./res/fibo.pq
build/penquin ./res/array.pq

# echo "[running]"
# lli test.ll
