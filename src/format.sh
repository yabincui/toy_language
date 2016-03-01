#!/bin/sh
find . -name "*.cpp" -or -name "*.h" | grep -v gtest_src | xargs clang-format -style=file -i
