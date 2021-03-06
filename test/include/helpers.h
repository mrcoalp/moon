#pragma once

#ifndef STACK_GUARD_H
#define STACK_GUARD_H

#include "moon/moon.h"

#define BEGIN_STACK_GUARD   \
    int stack_top_begin{0}; \
    int stack_top_end{0};   \
    {                       \
        StackGuard guard{stack_top_begin, stack_top_end};

#define END_STACK_GUARD \
    }                   \
    REQUIRE(stack_top_begin == stack_top_end);

#define CHECK_STACK_GUARD REQUIRE(guard.Check());

class StackGuard {
public:
    StackGuard(int& begin, int& end) : m_begin(begin), m_end(end) { m_begin = Moon::GetTop(); }

    ~StackGuard() { m_end = Moon::GetTop(); }

    [[nodiscard]] inline bool Check() const { return Moon::GetTop() == m_begin; }

private:
    int& m_begin;
    int& m_end;
};

struct LoggerSetter {
    LoggerSetter(std::string& info, std::string& warning, std::string& error) : m_info(info), m_warning(warning), m_error(error) {
        m_info.clear();
        m_warning.clear();
        m_error.clear();
        Moon::SetLogger([this](moon::Logger::Level level, const auto& msg) {
            switch (level) {
                case moon::Logger::Level::Info:
                    m_info = msg;
                    break;
                case moon::Logger::Level::Warning:
                    m_warning = msg;
                    break;
                case moon::Logger::Level::Error:
                    m_error = msg;
                    break;
            }
        });
    }

    inline const std::string& GetInfo() const { return m_info; }

    inline const std::string& GetWarning() const { return m_warning; }

    inline const std::string& GetError() const { return m_error; }

    void Clear() {
        m_info.clear();
        m_warning.clear();
        m_error.clear();
    }

    bool ErrorCheck() {
        bool has = !m_error.empty();
        Clear();
        return has;
    }

    inline bool NoErrors() const { return m_error.empty(); }

private:
    std::string& m_info;
    std::string& m_warning;
    std::string& m_error;
};

#endif
