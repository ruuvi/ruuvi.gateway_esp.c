#!/bin/bash

find ./main -iname *.c -o -iname *.cpp -o -iname *.h | xargs clang-format -i

