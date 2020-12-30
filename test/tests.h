struct TestCase {
    std::vector<std::string> failures;
    std::vector<std::string> successes;
    std::string name;
    std::function<void()> check;
    explicit TestCase(const std::string& name_, const std::function<void()>& check_) : name(name_), check(check_) {}
};

class StackGuard {
public:
    StackGuard(int& begin, int& end) : m_begin(begin), m_end(end) { m_begin = Moon::GetTop(); }

    ~StackGuard() { m_end = Moon::GetTop(); }

    inline bool Check() const { return m_begin == m_end; }

private:
    int& m_begin;
    int& m_end;
};

std::unordered_map<size_t, TestCase> s_tests;
std::string s_currentTest;

#define TEST(_name)                                                                                        \
    struct _name {                                                                                         \
        _name() { s_tests.emplace(std::hash<std::string>{}(#_name), TestCase(#_name, &_name::TestBody)); } \
        static void TestBody();                                                                            \
    };                                                                                                     \
    _name test_##_name;                                                                                    \
    void _name::TestBody()

#define EXPECT(_condition)                                                                    \
    if (!(_condition)) {                                                                      \
        s_tests.at(std::hash<std::string>{}(s_currentTest)).failures.push_back(#_condition);  \
    } else {                                                                                  \
        s_tests.at(std::hash<std::string>{}(s_currentTest)).successes.push_back(#_condition); \
    }

#define BEGIN_STACK_GUARD               \
    int stack_top_begin, stack_top_end; \
    {                                   \
        StackGuard guard{stack_top_begin, stack_top_end};

#define END_STACK_GUARD \
    }                   \
    EXPECT(stack_top_begin == stack_top_end)
