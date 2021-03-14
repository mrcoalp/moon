cmake_minimum_required(VERSION 3.10)

if (NOT WIN32)
    message(FATAL_ERROR "Not win32 system! No need to copy dll")
endif ()

find_file(LUA_DLL_PATH NAMES lua.dll liblua.dll PATHS ${SRC_PATH} ${SRC_PATH}/${BUILD_TYPE})

if ("${LUA_DLL_PATH}" MATCHES "LUA_DLL_PATH-NOTFOUND")
    message(FATAL_ERROR "Could not find dll")
endif ()

message("Copying dll from ${LUA_DLL_PATH} to ${DEST_PATH}")

execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LUA_DLL_PATH}" ${DEST_PATH} RESULT_VARIABLE res)

if (NOT res STREQUAL "0")
    message(FATAL_ERROR "Error copying dll")
endif ()
