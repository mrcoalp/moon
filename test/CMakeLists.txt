set(BINARY ${CMAKE_PROJECT_NAME}_tst)
add_executable(${BINARY} test.cpp)
add_test(NAME ${BINARY} COMMAND ${BINARY})
target_link_libraries(${BINARY} PUBLIC moon)
# Copy scripts folder to binary destination
file(COPY scripts DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
