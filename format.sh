#!/bin/sh
for file in $(find ./src)
do
    [ -f $file ] && clang-format -i $file
done
