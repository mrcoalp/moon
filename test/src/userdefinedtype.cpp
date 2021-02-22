#include "userdefinedtype.h"

int UserDefinedType::s_testClass{-1};

MOON_DEFINE_BINDING(UserDefinedType)
MOON_ADD_PROPERTY(m_prop)
MOON_ADD_PROPERTY_CUSTOM(prop, Getter, Setter)
MOON_ADD_METHOD(Getter)
MOON_ADD_METHOD(Setter)
MOON_REMOVE_GC;
