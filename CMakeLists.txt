cmake_minimum_required(VERSION 3.10)
project(moon)

set(CMAKE_CXX_STANDARD 17)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    message("Using GCC version ${CMAKE_CXX_COMPILER_VERSION}")
    # require at least gcc 7.1
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 7.1)
        message(FATAL_ERROR "GCC version must be at least 7.1!")
    endif ()
endif ()

option(MOON_BUILD_TESTS "Build test programs" ON)
option(MOON_BUILD_DOC "Build documentation" OFF)

set(LUA_LIB_BRANCH "main")
set(LUA_LIB_URL "https://github.com/mrcoalp/lua-cmake.git")
SET(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")

message("Building Moon...")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build, options are: Debug Release." FORCE)
endif ()

if (${CMAKE_BUILD_TYPE} MATCHES Debug)
    message("-- Build type: Debug")
    message("Adding Debug flag...")
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
        message("Doxygen need to be installed to generate the doxygen documentation")
    endif (DOXYGEN_FOUND)
endif (MOON_BUILD_DOC)

find_package(Lua)
if (NOT LUA_FOUND)
    message("-- Lua not found, compiling it...")
    if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/lua)
        execute_process(COMMAND git clone --recurse-submodules -b ${LUA_LIB_BRANCH} --depth 1 ${LUA_LIB_URL} ${CMAKE_CURRENT_SOURCE_DIR}/lua)
    endif ()
    add_subdirectory(lua)
    set(LUA_LIBRARIES lua)
    set(LUA_INCLUDE_DIR " ")
endif ()

add_library(moon INTERFACE)

if (MOON_BUILD_TESTS)
    add_subdirectory(test)
endif ()

target_include_directories(moon INTERFACE include ${LUA_INCLUDE_DIR})

target_link_libraries(moon INTERFACE ${LUA_LIBRARIES})

file(GLOB_RECURSE INCLUDES include/*.h)
install(FILES ${INCLUDES} DESTINATION include)
