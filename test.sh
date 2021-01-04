#!/bin/bash

# Wether or not to output results to file
COVERAGE=0

usage() {
  echo "Usage

    test.sh [options]

    Runs tests defined under test folder.

Options
    -h | --help     - Show help
    -c | --coverage - Output results to file"
}

# Check for arguments
while test $# -gt 0; do
  case "$1" in
  -c | --coverage)
    COVERAGE=1
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

OUT=$(pwd)
cd build/Debug/test || exit 1

./moon_tst || exit 1

if [ $COVERAGE = 1 ]; then
  # Output to file
  ./moon_tst -o "$OUT"/test_results.xml -r junit
  make moon_coverage || exit 1
  mv moon_coverage "$OUT"
fi

exit 0
