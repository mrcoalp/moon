cmake_minimum_required(VERSION 3.10)

if (EXISTS ${SRC_PATH}/${BUILD_TYPE}/lua.dll)
    set(LUA_DLL_PATH ${SRC_PATH}/${BUILD_TYPE}/lua.dll)
elseif (EXISTS ${SRC_PATH}/lua.dll)
    set(LUA_DLL_PATH ${SRC_PATH}/lua.dll)
else ()
    message(FATAL_ERROR "Could not find lua.dll")
endif ()

message("Copying lua.dll from ${LUA_DLL_PATH} to ${DEST_PATH}")

execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LUA_DLL_PATH}" ${DEST_PATH} RESULT_VARIABLE res)

if (NOT res STREQUAL "0")
    message(FATAL_ERROR "Error copying lua.dll")
endif ()
