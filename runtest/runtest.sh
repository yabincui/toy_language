#!/bin/bash

set -e
set -x

if [[ $# -eq 4 ]]; then
	TOY_EXE=$1
	INPUT_TXT=$2
	OUTPUT_TXT=$3
	STD_OUTPUT_TXT=$4
elif [[ $# -eq 0 ]]; then
	TOY_EXE=./toy
	INPUT_TXT=runtest_input.txt
	OUTPUT_TXT=runtest_output.txt
	STD_OUTPUT_TXT=runtest_std_output.txt
else
	echo "unexpected arg count: $#"
fi

rm -rf ${OUTPUT_TXT}
${TOY_EXE} --dump token,ast,code -i ${INPUT_TXT} >${OUTPUT_TXT} 2>&1
cat ${INPUT_TXT} | ${TOY_EXE} --dump token,ast,code >>${OUTPUT_TXT} 2>&1

diff ${OUTPUT_TXT} ${STD_OUTPUT_TXT}
if [[ $? -eq 0 ]]; then
  echo "runtest successfully!"
else
  echo "please fix above difference."
fi