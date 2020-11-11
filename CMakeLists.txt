cmake_minimum_required(VERSION 3.10)
project(moon)

set(CMAKE_CXX_STANDARD 17)

option(BUILD_TESTS "Build test programs" OFF)
option(BUILD_DOC "Build documentation" OFF)

message("Building MOON...")

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build, options are: Debug Release." FORCE)
endif ()

if (BUILD_SHARED_LIBS)
    set(LIB_TYPE SHARED)
else ()
    set(LIB_TYPE STATIC)
endif ()

if (${CMAKE_BUILD_TYPE} MATCHES Debug)
    message("-- Build type: Debug")
    message("Adding Debug flag...")
    add_definitions(-DNDEBUG)
else ()
    message("-- Build type: Release")
    add_definitions(-UNDEBUG)
endif ()

if (BUILD_DOC)
    find_package(Doxygen)

    if (DOXYGEN_FOUND)
        add_subdirectory(docs)
    else (DOXYGEN_FOUND)
        message("Doxygen need to be installed to generate the doxygen documentation")
    endif (DOXYGEN_FOUND)
endif (BUILD_DOC)

add_subdirectory(lua-5.4.1)

file(GLOB_RECURSE SOURCES include/*.h)

add_library(moon INTERFACE)

if (MOON_BUILD_TESTS)
    add_subdirectory(test)
endif ()

target_include_directories(moon INTERFACE include)

target_link_libraries(moon INTERFACE lua)

file(GLOB_RECURSE INCLUDES include/*.h)
install(FILES ${INCLUDES} DESTINATION include/moon)
