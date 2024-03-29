cmake_minimum_required(VERSION 3.10)
project(moon)

set(CMAKE_CXX_STANDARD 17)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    message("-- Using GCC version ${CMAKE_CXX_COMPILER_VERSION}")
    # require at least gcc 7.1
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.1)
        message(FATAL_ERROR "GCC version must be at least 7.1!")
    endif ()
endif ()

option(MOON_BUILD_TESTS "Build test programs" OFF)
option(MOON_BUILD_DOC "Build documentation" OFF)
option(MOON_BENCHMARKING "Build benchmark" OFF)
option(MOON_COVERAGE OFF)

if (CMAKE_COMPILER_IS_GNUCXX AND MOON_COVERAGE)
    set(COVERAGE_COMPILER_FLAGS "-fprofile-arcs -ftest-coverage -fPIC -O0")
    message("-- Adding coverage compiler flags: ${COVERAGE_COMPILER_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_COMPILER_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_COMPILER_FLAGS}")
endif ()

SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

message("-- Building Moon...")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build, options are: Debug Release." FORCE)
endif ()

if (${CMAKE_BUILD_TYPE} MATCHES Debug)
    message("-- Build type: Debug")
    message("-- Adding Debug flag...")
    add_definitions(-DNDEBUG)
else ()
    message("-- Build type: Release")
    add_definitions(-UNDEBUG)
endif ()

if (MOON_BUILD_DOC)
    find_package(Doxygen)

    if (DOXYGEN_FOUND)
        add_subdirectory(docs)
    else (DOXYGEN_FOUND)
        message(WARNING "Doxygen need to be installed to generate the doxygen documentation")
    endif (DOXYGEN_FOUND)
endif (MOON_BUILD_DOC)

find_package(Lua)
if (NOT LUA_FOUND)
    message("-- Lua not found, compiling it...")
    if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/lua)
        set(LUA_LIB_BRANCH "main")
        set(LUA_LIB_URL "https://github.com/mrcoalp/lua-cmake.git")
        execute_process(COMMAND git clone --recurse-submodules -b ${LUA_LIB_BRANCH} --depth 1 ${LUA_LIB_URL} ${CMAKE_CURRENT_SOURCE_DIR}/external/lua)
    endif ()
    add_subdirectory(external/lua)
    set(LUA_LIBRARIES lua)
    set(LUA_INCLUDE_DIR " ")
endif ()

add_library(moon INTERFACE)

if (MOON_BUILD_TESTS OR MOON_BENCHMARKING)
    if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/catch)
        set(CATCH_LIB_URL "https://github.com/catchorg/Catch2.git")
        set(CATCH_LIB_BRANCH "v2.x")
        execute_process(COMMAND git clone --recurse-submodules -b ${CATCH_LIB_BRANCH} --depth 1 ${CATCH_LIB_URL} ${CMAKE_CURRENT_SOURCE_DIR}/external/catch)
    endif ()

    set(CATCH_BUILD_TESTING OFF)
    set(CATCH_INSTALL_DOCS OFF)
    set(CATCH_INSTALL_HELPERS OFF)
    add_subdirectory(external/catch)

    if (MOON_BUILD_TESTS)
        add_subdirectory(test)
    endif ()
    if (MOON_BENCHMARKING)
        add_subdirectory(benchmark)
    endif ()
endif ()

target_include_directories(moon INTERFACE include ${LUA_INCLUDE_DIR})

target_link_libraries(moon INTERFACE ${LUA_LIBRARIES})

file(GLOB_RECURSE INCLUDES include/*.h)
install(FILES ${INCLUDES} DESTINATION include)
