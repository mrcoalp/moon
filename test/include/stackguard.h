#pragma once

#ifndef STACK_GUARD_H
#define STACK_GUARD_H

#include "moon/moon.h"

#define BEGIN_STACK_GUARD \
    int stack_top_begin;  \
    int stack_top_end;    \
    {                     \
        StackGuard guard{stack_top_begin, stack_top_end};

#define END_STACK_GUARD \
    }                   \
    REQUIRE(stack_top_begin == stack_top_end);

#define CHECK_STACK_GUARD REQUIRE(guard.Check());

class StackGuard {
public:
    StackGuard(int& begin, int& end) : m_begin(begin), m_end(end) { m_begin = Moon::GetTop(); }

    ~StackGuard() { m_end = Moon::GetTop(); }

    [[nodiscard]] inline bool Check() const { return m_begin == m_end; }

private:
    int& m_begin;
    int& m_end;
};

#endif
