#!/bin/bash

find ./main ./tests/test* -iname *.c -o -iname *.cpp -o -iname *.h | xargs clang-format -i

