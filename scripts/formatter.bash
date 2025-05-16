#!/bin/bash


# Get the directory of the bash script
ROOT="$(dirname "$(dirname "$(readlink -f "$0")")")"

# Check if clang-format executable exists
if ! $(which clang-format > /dev/null 2>&1); then
  echo "clang-format not found. Please install clang-format."
else
  clang-format -i $(find "$ROOT/src" -name "*.cpp" -o -name "*.h")
fi

# Check if cmake-format executable exists
if ! $(which cmake-format > /dev/null 2>&1); then
  echo "cmake-format not found. Please install cmake-format."
else
  $(which cmake-format) -i $(find "$ROOT" -name "CMakeLists.txt")
fi
