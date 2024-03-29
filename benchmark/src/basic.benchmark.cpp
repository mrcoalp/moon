#include <catch2/catch.hpp>

#include "moon/moon.h"

TEST_CASE("get values from stack benchmark", "[basic][benchmark]") {
    Moon::Init();
    Moon::Push(true, 1, "string", new int[20]);
    CHECK(Moon::RunCode("return function(s) return s end"));

    CHECK(Moon::Get<bool>(1));
    CHECK(Moon::Get<int>(2) == 1);
    CHECK(Moon::Get<std::string>(3) == "string");
    CHECK(Moon::Get<void*>(4) != nullptr);

    BENCHMARK("get bool") { return Moon::Get<bool>(1); };
    BENCHMARK("get number") { return Moon::Get<int>(2); };
    BENCHMARK("get string") { return Moon::Get<std::string>(3); };
    BENCHMARK("get const char") { return Moon::Get<const char*>(3); };
    BENCHMARK("get pointer") { return Moon::Get<void*>(4); };
    BENCHMARK("get function") { return Moon::Get<std::function<bool(bool)>>(5); };
    BENCHMARK("get reference") { return Moon::Get<moon::Reference>(1); };
    BENCHMARK("get object") { return Moon::Get<moon::Object>(1); };
    {
        auto obj = Moon::MakeObject(std::vector<int>{1, 2, 3});
        BENCHMARK("object deep access") { return (int)obj[3]; };
    }

    Moon::CloseState();
}
