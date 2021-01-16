#!/bin/bash

# Run clang-tidy with fix or not
FIX=
# 0 - disable
# 1 - format and check
# 2 - format without check
FORMAT=0

usage() {
  echo "Usage

    check.sh [options]

    Checks source code with clang-tidy.

Options
    -h | --help           - Show help
    -f | --fix            - Tries to fix warnings or errors found
    --format              - Use clang-format
    --format-only | -fo   - Use clang-format and exit without checks"
}

# Check for arguments
while test $# -gt 0; do
  case "$1" in
  -f | --fix)
    FIX=-fix
    ;;
  --format)
    FORMAT=1
    ;;
  --format-only | -fo)
    FORMAT=2
    ;;
  -h | --help)
    usage
    exit 0
    ;;
  -*)
    echo -e "\033[0;31mBad argument: $1\033[0m"
    usage
    exit 1
    ;;
  *)
    echo -e "\033[0;31mUnknown argument: $1\033[0m"
    usage
    exit 1
    ;;
  esac
  shift
done

find include -regex '.*\.\(h\|hpp\)' -exec clang-format -output-replacements-xml {} + | grep "<replacement " >>'format_coverage.txt'

# Check if format_coverage.txt exists and is not empty
if [ -s format_coverage.txt ]; then
  echo -e '\033[1;33mStyle corrections must be made. Please use clang-format and commit again!\033[0m'
fi

if [ $FORMAT -lt 2 ]; then
  run-clang-tidy -p='build/Debug/' -header-filter='include/' $FIX include/ >'tidy_coverage.txt' || exit 1
  sed -i '/Enabled checks:/,/^$/d' tidy_coverage.txt # Remove enabled checks from file and keep only results
  sed -i '/-header-filter=/d' tidy_coverage.txt      # Remove clang-tidy specific text

  # Check if tidy_coverage.txt exists and is not empty
  if [ -s tidy_coverage.txt ]; then
    echo -e '\033[1;33mErrors and/or warnings were found. Please check "tidy_coverage.txt" and address them, if possible!\033[0m'
    exit 0
  fi
fi

if [ $FORMAT -ge 1 ]; then
  find include -regex '.*\.\(h\|hpp\)' -exec clang-format -i {} +
  echo -e '\033[0;32mRan clang-format!\033[0m'
fi

exit 0
