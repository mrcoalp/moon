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

set(BINARY ${CMAKE_PROJECT_NAME}_tst)

set(TESTS basic.cpp scripting.cpp functions.cpp moontypes.cpp)

add_executable(${BINARY} test-main.cpp ${TESTS})

add_test(NAME ${BINARY} COMMAND ${BINARY})

if (CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

    include(CodeCoverage)
    setup_target_for_coverage(${CMAKE_PROJECT_NAME}_coverage ${BINARY} coverage)

    SET(CMAKE_CXX_FLAGS "-g -O0 -fprofile-arcs -ftest-coverage")
    SET(CMAKE_C_FLAGS "-g -O0 -fprofile-arcs -ftest-coverage")
endif ()

target_link_libraries(${BINARY} PRIVATE Catch2 moon)
# Copy scripts folder to binary destination
file(COPY scripts DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

if (MSVC AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET ${BINARY} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_BINARY_DIR}/lua/lua.dll"
            $<TARGET_FILE_DIR:${BINARY}>)
endif ()
