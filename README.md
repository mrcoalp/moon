# Moon

[![GitHub](https://img.shields.io/github/license/mrcoalp/moon)](LICENSE)
[![Build Status](https://travis-ci.org/mrcoalp/moon.svg?branch=main)](https://travis-ci.org/mrcoalp/moon)
[![linux](https://github.com/mrcoalp/moon/workflows/linux/badge.svg)](https://github.com/mrcoalp/moon/actions?query=workflow%3Alinux)
[![windows](https://github.com/mrcoalp/moon/workflows/windows/badge.svg)](https://github.com/mrcoalp/moon/actions?query=workflow%3Awindows)
[![codecov](https://codecov.io/gh/mrcoalp/moon/branch/main/graph/badge.svg?token=70AAHGSCDY)](https://codecov.io/gh/mrcoalp/moon)

## WIP

Library to map data, functions and classes between C++ and Lua scripting language. With simple static methods to avoid
the need for a constructed object. Still in current development. Performance still needs to be measured and improved.

## Getting started

To get started with Moon simply download and include
the [single header file](https://raw.githubusercontent.com/mrcoalp/moon/main/include/moon/moon.h) in your project. Moon
needs a C++ 17 compiler and Lua. If you don't want to handle the hassle of including Lua yourself, Moon does that for
you via CMake. Add it as a sub directory to your project and link it with your binary. If Moon can't find Lua in your
system, it will download it and compile it.

## Example

``` cpp
#include <moon/moon.h>

int main() {
    Moon::Init();
    
    Moon::Push(true);
    auto b = Moon::Get<bool>(-1);
    Moon::Pop();
        
    Moon::RegisterFunction("TestVec", [](std::vector<std::string> vec) { return vec; });
    Moon::RunCode("local passed = TestVec({'passed', 'failed', 'other'})[1]; assert(passed == 'passed')");
    
    Moon::CloseState();
}
```

## Documentation

As soon as a pseudo stable version arises... In the meantime, if you want to use the lib, check the examples of the test
cases.
