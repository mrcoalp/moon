#include <catch2/catch.hpp>

#include "moon/moon.h"
#include "stackguard.h"

#ifdef RegisterClass
#undef RegisterClass
#endif

int testClass = -1;

class Scripting {
public:
    Scripting() = default;

    explicit Scripting(int prop) : m_prop(prop) {}

    explicit Scripting(lua_State*) : m_prop(Moon::Get<int>()) {}

    MOON_DECLARE_CLASS(Scripting)

    MOON_PROPERTY(m_prop)

    MOON_METHOD(Getter) {
        Moon::Push(m_prop + Moon::Get<int>());
        return 1;
    }

    MOON_METHOD(Setter) {
        testClass = m_prop = Moon::Get<int>();
        return 0;
    }

    [[nodiscard]] inline int GetProp() const { return m_prop; }

private:
    int m_prop;
};

MOON_DEFINE_BINDING(Scripting)
MOON_ADD_PROPERTY(m_prop)
MOON_ADD_METHOD(Getter)
MOON_ADD_METHOD(Setter)
MOON_REMOVE_GC;

TEST_CASE("register user type", "[binding]") {
    Moon::Init();
    Moon::RegisterClass<Scripting>();

    SECTION("member properties access") {
        BEGIN_STACK_GUARD
        Moon::RunCode("local s = Scripting(20);return s.m_prop;");
        REQUIRE(Moon::Get<int>(-1) == 20);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("member methods access") {
        Moon::RunCode("local s = Scripting(20);s.Setter(s.Getter(s.m_prop));");
        REQUIRE(testClass == 40);
    }

    Moon::CloseState();
}

TEST_CASE("push user type", "[basic][scripting]") {
    Moon::Init();
    Moon::RegisterClass<Scripting>();

    SECTION("get stack") {
        BEGIN_STACK_GUARD
        Moon::RunCode("return Scripting(30)");
        auto o = Moon::Get<Scripting>(-1);
        REQUIRE(o.GetProp() == 30);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("get heap") {
        BEGIN_STACK_GUARD
        Moon::RunCode("return Scripting(34)");
        auto* o = Moon::Get<Scripting*>(-1);
        REQUIRE(o->GetProp() == 34);
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get stack") {
        Scripting o(20);
        BEGIN_STACK_GUARD
        Moon::Push(&o);
        auto o2 = Moon::Get<Scripting>(-1);
        REQUIRE(o2.GetProp() == o.GetProp());
        Moon::Pop();
        END_STACK_GUARD
    }

    SECTION("push and get heap") {
        auto* o = new Scripting(10);
        BEGIN_STACK_GUARD
        Moon::Push(o);
        auto* o2 = Moon::Get<Scripting*>(-1);
        REQUIRE(o2->GetProp() == o->GetProp());
        Moon::Pop();
        END_STACK_GUARD
    }

    Moon::CloseState();
}

TEST_CASE("use user type in functions", "[scripting][functions]") {
    Moon::Init();
    Moon::SetLogger([](const auto& error) { printf("%s\n", error.c_str()); });
    Moon::RegisterClass<Scripting>();

    SECTION("push user type to Lua function") {
        Moon::RunCode("function Object(object, increment) assert(object.m_prop + increment == 6) end");
        Scripting o(3);
        BEGIN_STACK_GUARD
        REQUIRE(Moon::Call("Object", &o, 3));
        END_STACK_GUARD
    }

    SECTION("get user type from Lua function") {
        Moon::RunCode("function GetObject(prop) return Scripting(prop) end");
        Scripting o;
        BEGIN_STACK_GUARD
        REQUIRE(Moon::Call(o, "GetObject", 2));
        END_STACK_GUARD
        REQUIRE(o.GetProp() == 2);
    }

    Moon::CloseState();
}
