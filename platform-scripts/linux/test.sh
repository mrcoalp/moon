#!/bin/bash

# Whether or not to output results to file
COVERAGE=0
# Default is debug build
CONFIGURATION=Debug

usage() {
  echo "Usage

    test.sh [options]

    Runs tests defined under test folder.

Options
    -h | --help     - Show help
    -d | --debug    - Run moon tests with debug configuration
    -r | --release  - Run moon tests with release configuration
    -c | --coverage - Output results to file"
}

# Check for arguments
while test $# -gt 0; do
  case "$1" in
  -d | --debug | --Debug)
    CONFIGURATION=Debug
    ;;
  -r | --release | --Release)
    CONFIGURATION=Release
    ;;
  -c | --coverage)
    COVERAGE=1
    ;;
  -h | --help)
    usage
    exit 0
    ;;
  -*)
    echo "Ignored argument \"$1\". Continuing..."
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
cd build/$CONFIGURATION/test || exit 1

if [ $COVERAGE = 1 ]; then
  ./moon_test -o "$OUT"/test_results.xml -r junit

  mkdir moon_coverage_html

  gcovr --exclude-unreachable-branches --html --html-details -r "$OUT" -e "$OUT"/test --object-directory=. -o moon_coverage_html/index.html || echo "Could not generate html coverage report"
  gcovr --exclude-unreachable-branches --xml -r "$OUT" -e "$OUT"/test --object-directory=. -o moon_coverage.xml || echo "Could not generate xml coverage report"

  rm -rf "$OUT"/moon_coverage_html
  rm -rf "$OUT"/moon_coverage.xml

  mv moon_coverage_html "$OUT" && echo "Open $OUT/moon_coverage_html/index.html in a browser"
  mv moon_coverage.xml "$OUT"
fi

./moon_test --benchmark-no-analysis --benchmark-samples 1000 || exit 1

exit 0
