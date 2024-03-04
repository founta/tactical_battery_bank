#!/bin/bash

mkdir -p build && cd build
cmake ../firmware && make -j 8

cd ..

rm -rf dist
mkdir dist && cp build/*.uf2 ./dist
