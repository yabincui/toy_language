#!/bin/bash

set -e
set -x

rm runtest_output.txt
cat runtest_input.txt | ./lexer >>runtest_output.txt
cat runtest_input.txt | ./ast >>runtest_output.txt
cat runtest_input.txt | ./code >>runtest_output.txt

diff runtest_output.txt runtest_std_output.txt
if [[ $? -eq 0 ]]; then
  echo "runtest successfully!"
else
  echo "please fix above difference."
fi