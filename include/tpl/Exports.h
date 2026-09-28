// Export decoration of the tpl library — the selector tanh_apply_symbol_policy()
// drives: TPL_STATIC (PUBLIC, static builds) -> no decoration, TPL_BUILDING
// (PRIVATE, while compiling the library) -> export, otherwise import.
#pragma once

#if defined(TPL_STATIC)
#define TPL_API
#elif defined(TPL_BUILDING)
#if defined(_WIN32)
#define TPL_API __declspec(dllexport)
#elif defined(__GNUC__) || defined(__clang__)
#define TPL_API __attribute__((visibility("default")))
#else
#define TPL_API
#endif
#else
#if defined(_WIN32)
#define TPL_API __declspec(dllimport)
#elif defined(__GNUC__) || defined(__clang__)
#define TPL_API __attribute__((visibility("default")))
#else
#define TPL_API
#endif
#endif

// Marks a function RealtimeSanitizer checks at runtime: no allocation, no lock, no
// blocking syscall may happen inside it. Placed after noexcept, on the declaration
// and the definition. Active only in the rtsan preset (TPL_WITH_RTSAN, Clang >= 20).
#if defined(TPL_WITH_RTSAN)
#define TPL_NONBLOCKING [[clang::nonblocking]]
#else
#define TPL_NONBLOCKING
#endif
