@echo off

set configuration=Debug
set tests=OFF
set clean=false
set install=
set doc=OFF
set shared=OFF
set generator="Visual Studio 16 2019"

:loop
    if "%~1" == "" goto build
    if %1 == -d set configuration=Debug
    if %1 == -r set configuration=Release
    if %1 == -t set tests=ON
    if %1 == -c set clean=true
    if %1 == -i set install=--target install
    if %1 == --doc set doc=ON
    if %1 == --shared set shared=ON
    if %1 == --mingw set generator="MinGW Makefiles"
    if %1 == -h goto usage
    shift
    if not "%~1" == "" goto loop

:build
    if %clean% == true (
        rmdir /s /q build\%configuration%
        echo Removed %cd%\build\%configuration%!
    )
    mkdir build\%configuration%
    cd build\%configuration%
    cmake ..\.. -G %generator% -DBUILD_SHARED_LIBS=%shared% -DMOON_BUILD_TESTS=%tests% -DMOON_BUILD_DOC=%doc% -DCMAKE_BUILD_TYPE=%configuration% -DCMAKE_INSTALL_PREFIX=./bin || exit 1
    cmake --build . --config %configuration% %install% || exit 1
    cd ..\..
    goto end

:usage
    echo Usage
    echo   build [options]
    echo   Builds the game, by default, with debug configuration.
    echo Options
    echo   -h          - Show help
    echo   -d          - Build game with debug configuration
    echo   -r          - Build game with release configuration
    echo   -c          - Clean build of configuration
    echo   -t          - Build tests
    echo   --docs      - Build docs
    echo   --shared    - Build shared libs
    echo   --mingw     - Use mingw compiler
    goto end

:end
