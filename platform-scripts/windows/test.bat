@echo off

set configuration=Debug

:loop
    if "%~1" == "" goto test
    if %1 == -d set configuration=Debug
    if %1 == -r set configuration=Release
    if %1 == -h goto usage
    shift
    if not "%~1" == "" goto loop

:test
    cd build\%configuration%\test || exit 1
    %configuration%\moon_test || moon_test || exit 1
    cd ..\..\..
    goto end

:usage
    echo Usage
    echo   test [options]
    echo   Runs the test suite, by default, with debug configuration.
    echo Options
    echo   -h  - Show help
    echo   -d  - Run tests with debug configuration
    echo   -r  - Run tests with release configuration
    goto end

:end
