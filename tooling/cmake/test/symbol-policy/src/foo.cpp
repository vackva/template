#include <foo/foo.h>

#include <thread>

extern "C" int ma_fake_init(void);  // "vendored" C code compiled into the library

namespace vendored {
int weight() { return 3; }  // undecorated C++ in another namespace: must stay local
}

namespace foo {

Widget::Widget() : m_name("widget"), m_values{1, 2, 3} {}
Widget::~Widget() = default;
int Widget::value() const { return static_cast<int>(m_values.size()) + ma_fake_init(); }
std::shared_ptr<Widget> Widget::make() { return std::make_shared<Widget>(); }
void Widget::fail() const { throw Error("expected failure"); }

int compute(int x) {
    int result = 0;
    std::thread t([&] { result = detail::helper(x) * vendored::weight(); });
    t.join();
    return result;
}

namespace detail {
int helper(int x) { return x + 1; }
}

}  // namespace foo
