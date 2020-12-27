#include <map>

#include "scripting.h"

#define ANSI_COLOR_RED "\x1b[31m"
#define ANSI_COLOR_GREEN "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE "\x1b[34m"
#define ANSI_COLOR_MAGENTA "\x1b[35m"
#define ANSI_COLOR_CYAN "\x1b[36m"
#define ANSI_COLOR_RESET "\x1b[0m"

using Test = std::function<bool()>;
static std::map<const char*, Test> tests = {
    {"call_cpp_function_from_lua", call_cpp_function_from_lua},
    {"call_lua_function_pass_binding_object_modify_inside_lua", call_lua_function_pass_binding_object_modify_inside_lua},
    {"call_lua_function_pass_vector_get_string", call_lua_function_pass_vector_get_string},
    {"call_lua_function_pass_multiple_params_get_int", call_lua_function_pass_multiple_params_get_int},
    {"call_lua_function_pass_multiple_params_get_vector", call_lua_function_pass_multiple_params_get_vector},
    {"call_lua_function_get_anonymous_function_and_call_it_no_args", call_lua_function_get_anonymous_function_and_call_it_no_args},
    {"call_lua_function_get_anonymous_function_and_call_it_with_args", call_lua_function_get_anonymous_function_and_call_it_with_args},
    {"cpp_class_bind_lua", cpp_class_bind_lua},
    {"get_global_lua_var_from_cpp", get_global_lua_var_from_cpp},
    {"lua_run_code", lua_run_code},
    {"get_dynamic_map_from_lua", get_dynamic_map_from_lua},
    {"create_object_ref_and_use_it", create_object_ref_and_use_it},
    {"create_empty_dynamic_map_in_lua_with_expected_fail", create_empty_dynamic_map_in_lua_with_expected_fail},
    {"complex_data_containers", complex_data_containers}};

int RunTests() {
    const int nrOfTests = (int)tests.size();
    int passed = 0;
    printf(ANSI_COLOR_CYAN "Executing %d tests..." ANSI_COLOR_RESET "\n", nrOfTests);
    for (const auto& test : tests) {
        try {
            if (test.second()) {
                ++passed;
                printf(ANSI_COLOR_GREEN "'%s' - passed" ANSI_COLOR_RESET "\n", test.first);
            } else {
                printf(ANSI_COLOR_RED "'%s' - failed" ANSI_COLOR_RESET "\n", test.first);
            }
        } catch (const std::exception& e) {
            printf(ANSI_COLOR_RED "'%s' raised an exception: %s" ANSI_COLOR_RESET "\n", test.first, e.what());
        } catch (...) {
            printf(ANSI_COLOR_RED "'%s' raised an unknown exception" ANSI_COLOR_RESET "\n", test.first);
        }
    }
    const int result = nrOfTests - passed;
    if (result == 0) {
        printf("\n" ANSI_COLOR_GREEN "ALL TESTS PASSED!" ANSI_COLOR_RESET "\n");
    } else {
        printf("\n" ANSI_COLOR_RED "%d TESTS FAILED!" ANSI_COLOR_RESET "\n", result);
    }
    return result;
}

int main() {
    Moon::Init();
    Moon::SetLogger([](const auto& error) { printf(ANSI_COLOR_RED "%s" ANSI_COLOR_RESET "\n", error.c_str()); });
    Moon::RegisterClass<Script>();
    auto failed = RunTests();
    Moon::CloseState();
    return failed;
}
