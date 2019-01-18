#!/bin/bash
./configure.sh
make
cd test_prog
make
make run
cd ..
