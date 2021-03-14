enable_testing()

set(BINARY ${CMAKE_PROJECT_NAME}_benchmark)

file(GLOB_RECURSE SOURCE src/*.cpp)

add_executable(${BINARY} benchmark-main.cpp ${SOURCE})

add_test(NAME ${BINARY} COMMAND ${BINARY})

target_link_libraries(${BINARY} PRIVATE Catch2 moon)

target_compile_definitions(${BINARY} PRIVATE -DCATCH_CONFIG_ENABLE_BENCHMARKING)

if ((MSVC OR MINGW) AND BUILD_SHARED_LIBS)
    set(LUA_BINARY_DIR "${PROJECT_BINARY_DIR}/external/lua")
    add_custom_command(TARGET ${BINARY} POST_BUILD COMMAND ${CMAKE_COMMAND} -DBUILD_TYPE=${CMAKE_BUILD_TYPE}
            -DSRC_PATH=${LUA_BINARY_DIR} -DDEST_PATH=$<TARGET_FILE_DIR:${BINARY}>
            -P ${PROJECT_SOURCE_DIR}/cmake/CopyLUA.cmake)
endif ()
