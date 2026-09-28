#include <foo/foo.h>

#include <cstdio>

int main() {
    auto w = foo::Widget::make();
    if (w->value() != 4 || w->name() != "widget") {
        std::printf("unexpected widget: value=%d name=%s\n", w->value(), w->name().c_str());
        return 1;
    }
    if (foo::compute(20) != 63) {
        std::printf("unexpected compute: %d\n", foo::compute(20));
        return 1;
    }
    try {
        w->fail();
    } catch (const foo::Error& e) {  // typeinfo must match across the boundary
        std::printf("caught: %s\n", e.what());
        return 0;
    }
    std::printf("foo::Error was not caught across the library boundary\n");
    return 1;
}
