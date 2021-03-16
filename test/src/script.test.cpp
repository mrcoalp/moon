#include <catch2/catch.hpp>

#include "helpers.h"

SCENARIO("run scripts and get values inline", "[basic][script]") {
    Moon::Init();
    std::string info, warning, error;
    LoggerSetter logs{info, warning, error};
    BEGIN_STACK_GUARD

    GIVEN("an empty Lua stack") {
        REQUIRE(Moon::GetTop() == 0);

        WHEN("simple scripts are run to retrieve values") {
            int i = Moon::Run("return 1");
            std::string s = Moon::Run("return 'passed'");
            bool b = Moon::Run("return true");

            THEN("values should be valid") {
                REQUIRE(i == 1);
                REQUIRE(s == "passed");
                REQUIRE(b);
            }
        }
    }

    END_STACK_GUARD
    INFO(logs.GetError())
    REQUIRE(logs.NoErrors());
    Moon::CloseState();
}
