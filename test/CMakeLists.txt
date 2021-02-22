enable_testing()

if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/catch)
    set(CATCH_LIB_URL "https://github.com/catchorg/Catch2.git")
    set(CATCH_LIB_BRANCH "v2.x")
    execute_process(COMMAND git clone --recurse-submodules -b ${CATCH_LIB_BRANCH} --depth 1 ${CATCH_LIB_URL} ${CMAKE_CURRENT_SOURCE_DIR}/catch)
endif ()

set(CATCH_BUILD_TESTING OFF)
set(CATCH_INSTALL_DOCS OFF)
set(CATCH_INSTALL_HELPERS OFF)
add_subdirectory(catch)

option(MOON_COVERAGE OFF)
option(MOON_BENCHMARKING OFF)

if (CMAKE_COMPILER_IS_GNUCXX AND MOON_COVERAGE)
    set(COVERAGE_COMPILER_FLAGS "-fprofile-arcs -ftest-coverage -fPIC -O0")
    message("-- Adding coverage compiler flags: ${COVERAGE_COMPILER_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_COMPILER_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_COMPILER_FLAGS}")
endif ()

set(BINARY ${CMAKE_PROJECT_NAME}_tst)

file(GLOB_RECURSE SOURCE src/*.cpp)

if (MOON_BENCHMARKING)
    file(GLOB_RECURSE BENCHMARK benchmark/*.cpp)
    set(SOURCE ${SOURCE} ${BENCHMARK})
endif ()

add_executable(${BINARY} test-main.cpp ${SOURCE})

add_test(NAME ${BINARY} COMMAND ${BINARY})

target_include_directories(${BINARY} PRIVATE include)
target_link_libraries(${BINARY} PRIVATE Catch2 moon)

if (MOON_BENCHMARKING)
    target_compile_definitions(${BINARY} PRIVATE -DCATCH_CONFIG_ENABLE_BENCHMARKING)
endif ()

# Copy scripts folder to binary destination
file(COPY scripts DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

if ((MSVC OR MINGW) AND BUILD_SHARED_LIBS)
    set(LUA_BINARY_DIR "${PROJECT_BINARY_DIR}/lua")
    add_custom_command(TARGET ${BINARY} POST_BUILD COMMAND ${CMAKE_COMMAND} -DBUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DSRC_PATH=${LUA_BINARY_DIR} -DDEST_PATH=$<TARGET_FILE_DIR:${BINARY}>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyLUA.cmake)
endif ()
