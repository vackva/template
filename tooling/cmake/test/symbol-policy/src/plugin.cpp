// Plugin-shaped module embedding the library (statically or by linking the shared one).
#include <foo/foo.h>

#if defined(_WIN32)
#define PLUGIN_EXPORT __declspec(dllexport)
#else
#define PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

extern "C" PLUGIN_EXPORT int plugin_entry() {
    auto w = foo::Widget::make();
    return foo::compute(20) + w->value();
}
