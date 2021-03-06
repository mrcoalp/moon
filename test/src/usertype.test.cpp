#include <catch2/catch.hpp>

#include "helpers.h"
#include "userdefinedtype.h"

#ifdef RegisterClass
#undef RegisterClass
#endif

TEST_CASE("register user type", "[binding]") {
    Moon::Init();
    Moon::RegisterClass<UserDefinedType>();

    SECTION("member properties access") {
        BEGIN_STACK_GUARD
        REQUIRE(Moon::RunCode("local s = UserDefinedType(20);return s.m_prop;"));
        REQUIRE(Moon::Get<int>(-1) == 20);
        REQUIRE(Moon::RunCode("local s = UserDefinedType(30);return s.prop;"));
        REQUIRE(Moon::Get<int>(-1) == 30);
        Moon::Pop(2);
        END_STACK_GUARD
    }

    SECTION("member methods access") {
        Moon::RunCode("local s = UserDefinedType(20);s.Setter(s.Getter(s.prop));");
        REQUIRE(UserDefinedType::Test() == 40);
    }

    Moon::CloseState();
}

TEST_CASE("push user type", "[basic][scripting]") {
    Moon::Init();
    Moon::RegisterClass<UserDefinedType>();

    SECTION("get stack") {
        BEGIN_STACK_GUARD
        Moon::RunCode("return UserDefinedType(30)");
        auto o = Moon::Get<UserDefinedType>(-1);
        REQUIRE(o.GetProp() == 30);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("get heap") {
        BEGIN_STACK_GUARD
        Moon::RunCode("return UserDefinedType(34)");
        auto* o = Moon::Get<UserDefinedType*>(-1);
        REQUIRE(o->GetProp() == 34);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get stack") {
        UserDefinedType o(20);
        BEGIN_STACK_GUARD
        Moon::Push(&o);
        auto o2 = Moon::Get<UserDefinedType>(-1);
        REQUIRE(o2.GetProp() == o.GetProp());
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get heap") {
        auto* o = new UserDefinedType(10);
        BEGIN_STACK_GUARD
        Moon::Push(o);
        auto* o2 = Moon::Get<UserDefinedType*>(-1);
        REQUIRE(o2->GetProp() == o->GetProp());
        Moon::Pop();
        END_STACK_GUARD
    }

    Moon::CloseState();
}

TEST_CASE("use user type in functions", "[scripting][functions]") {
    Moon::Init();
    Moon::RegisterClass<UserDefinedType>();

    SECTION("push user type to Lua function") {
        Moon::RunCode("function Object(object, increment) return object.m_prop + increment == 6 end");
        UserDefinedType o(3);
        BEGIN_STACK_GUARD
        REQUIRE(Moon::Call<bool>("Object", &o, 3));
        END_STACK_GUARD
    }

    SECTION("get user type from Lua function") {
        Moon::RunCode("function GetObject(prop) return UserDefinedType(prop) end");
        BEGIN_STACK_GUARD
        REQUIRE(Moon::Call<UserDefinedType>("GetObject", 2).GetProp() == 2);
        END_STACK_GUARD
    }

    Moon::CloseState();
}
