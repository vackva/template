#pragma once
#include <foo/Exports.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace foo {

// Thrown across the library boundary: needs default-visibility typeinfo (FOO_API).
class FOO_API Error : public std::runtime_error {
public:
    explicit Error(const std::string& what) : std::runtime_error(what) {}
};

class FOO_API Widget {
public:
    Widget();
    virtual ~Widget();
    virtual int value() const;
    std::string name() const { return m_name; }  // inline member, compared across DSOs
    static std::shared_ptr<Widget> make();     // std::make_shared inside the library
    [[noreturn]] void fail() const;

private:
    std::string m_name;
    std::vector<int> m_values;
};

FOO_API int compute(int x);  // runs a std::thread inside the library

namespace detail {
int helper(int x);  // no decoration: must never be exported
}

}  // namespace foo
