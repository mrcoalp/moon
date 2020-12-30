#include "scripting.h"
#include "tests.h"

int main() {
    Moon::Init();
    Moon::SetLogger([](const auto& error) { printf(ANSI_COLOR_RED "%s" ANSI_COLOR_RESET "\n", error.c_str()); });
    Moon::RegisterClass<Scripting>();
    auto failed = RunTests();
    Moon::CloseState();
    return failed;
}
