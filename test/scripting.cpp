#include "scripting.h"

std::string Scripting::testCPPFunction;
int Scripting::testClass = -1;

MOON_DEFINE_BINDING(Scripting, false)
MOON_ADD_PROPERTY(m_prop)
MOON_ADD_METHOD(Getter)
MOON_ADD_METHOD(Setter);

MOON_METHOD(cppFunction) {
    Scripting::testCPPFunction = Moon::GetValue<std::string>();
    return 0;
}
