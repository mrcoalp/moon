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

if [ $COVERAGE = 1 ]; then
  ./moon_tst -o "$OUT"/test_results.xml -r junit

  mkdir moon_coverage_html

  gcovr --html --html-details -r "$OUT" -e "$OUT"/test --object-directory=. -o moon_coverage_html/index.html || echo "Could not generate html coverage report"
  gcovr --xml -r "$OUT" -e "$OUT"/test --object-directory=. -o moon_coverage.xml || echo "Could not generate xml coverage report"

  rm -rf "$OUT"/moon_coverage_html
  rm -rf "$OUT"/moon_coverage.xml

  mv moon_coverage_html "$OUT" && echo "Open $OUT/moon_coverage_html/index.html in a browser"
  mv moon_coverage.xml "$OUT"
fi

./moon_tst --benchmark-no-analysis --benchmark-samples 1000 || exit 1

exit 0
