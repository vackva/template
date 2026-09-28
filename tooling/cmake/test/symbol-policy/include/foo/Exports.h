// Export decoration of the test library, in the shape every tanh-lab library uses:
// <P>_STATIC (PUBLIC, set by tanh_apply_symbol_policy for static libraries) -> empty,
// <P>_BUILDING (PRIVATE while compiling the library) -> export, else import.
#pragma once
#if defined(FOO_STATIC)
#define FOO_API
#elif defined(_WIN32)
#if defined(FOO_BUILDING)
#define FOO_API __declspec(dllexport)
#else
#define FOO_API __declspec(dllimport)
#endif
#else
#define FOO_API __attribute__((visibility("default")))
#endif
