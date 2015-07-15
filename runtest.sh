#!/bin/bash

set -e
set -x

rm -rf runtest_output.txt
./toy --dump token,ast,code -i runtest_input.txt >>runtest_output.txt 2>&1
cat runtest_input.txt | ./toy --dump token,ast,code >>runtest_output.txt 2>&1

diff runtest_output.txt runtest_std_output.txt
if [[ $? -eq 0 ]]; then
  echo "runtest successfully!"
else
  echo "please fix above difference."
fi