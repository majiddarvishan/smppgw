#!/bin/bash


# Get the directory of the bash script
ROOT="$(dirname "$(dirname "$(readlink -f "$0")")")"

# Check if clang-tidy is available
if ! command -v clang-tidy &> /dev/null
then
  echo "clang-tidy is not installed. Please install it before running this script."
  exit 1
fi
cpp_files=$(find "$ROOT/src" -name "*.cpp")


# Run clang-tidy with common options and config file
for file in $cpp_files; do
  clang-tidy -p=$ROOT/build/debug-x64 \
            -header-filter=.* \
            --quiet \
            --config-file=$ROOT/.clang-tidy \
            "$file"
done

echo "clang-tidy finished running on all CPP files."