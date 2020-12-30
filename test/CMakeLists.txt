set(BINARY ${CMAKE_PROJECT_NAME}_tst)
set(TESTS scripting.cpp basic.cpp)
add_executable(${BINARY} tests.cpp main.cpp ${TESTS})
add_test(NAME ${BINARY} COMMAND ${BINARY})
target_link_libraries(${BINARY} PUBLIC moon)
# Copy scripts folder to binary destination
file(COPY scripts DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

if (MSVC AND BUILD_SHARED_LIBS)
    add_custom_command(TARGET ${BINARY} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${PROJECT_BINARY_DIR}/lua/lua.dll"
            $<TARGET_FILE_DIR:${BINARY}>)
endif ()
