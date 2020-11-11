#include "moon.h"

#include <sstream>

lua_State* Moon::s_state = nullptr;

std::function<void(const std::string&)> Moon::s_logger;

void Moon::Init() {
    s_state = luaL_newstate();
    luaL_openlibs(s_state);
    moon_types::LuaFunction::Register(s_state);
}

void Moon::CloseState() {
    lua_close(s_state);
    s_state = nullptr;
}

std::string Moon::GetStackDump() {
    int top = lua_gettop(s_state);
    std::stringstream dump;
    dump << "***** LUA STACK *****" << std::endl;
    for (int i = 1; i <= top; ++i) {
        int t = lua_type(s_state, i);
        switch (t) {
            case LUA_TSTRING:
                dump << GetValue<const char*>(i) << std::endl;
                break;
            case LUA_TBOOLEAN:
                dump << std::boolalpha << GetValue<bool>(i) << std::endl;
                break;
            case LUA_TNUMBER:
                dump << GetValue<double>(i) << std::endl;
                break;
            default:  // NOTE(mpinto): Other values, print type
                dump << lua_typename(s_state, t) << std::endl;
                break;
        }
    }
    return dump.str();
}

bool Moon::LoadFile(const char* filePath) {
    if (!checkStatus(luaL_loadfile(s_state, filePath), "Error loading file")) {
        return false;
    }
    return checkStatus(lua_pcall(s_state, 0, LUA_MULTRET, 0), "Loading file failed");
}

bool Moon::RunCode(const char* code) {
    if (!checkStatus(luaL_loadstring(s_state, code), "Error running code")) {
        return false;
    }
    return checkStatus(lua_pcall(s_state, 0, LUA_MULTRET, 0), "Running code failed");
}

void Moon::PushNull() { MS::PushNull(s_state); }

void Moon::RegisterFunction(const char* name, moon_types::LuaCFunction fn) { lua_register(s_state, name, fn); }

bool Moon::CallFunction(const char* name) {
    lua_getglobal(s_state, name);
    if (!lua_isfunction(s_state, -1)) {
        lua_pop(s_state, 1);
        return false;
    }
    return checkStatus(lua_pcall(s_state, 0, 0, 0), "Failed to call LUA function");
}

bool Moon::checkStatus(int status, const char* errMessage) {
    if (status != LUA_OK) {
        if (status == LUA_ERRSYNTAX) {
            const char* msg = lua_tostring(s_state, -1);
            s_logger(msg != nullptr ? msg : errMessage);
        } else if (status == LUA_ERRFILE) {
            const char* msg = lua_tostring(s_state, -1);
            s_logger(msg != nullptr ? msg : errMessage);
        }
        return false;
    }
    return true;
}
