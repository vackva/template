#pragma once

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

namespace tpl::transcript::test {

/// An empty directory in the build tree, unique to the running test (parallel-safe).
inline std::filesystem::path fresh_dir() {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    const auto dir = std::filesystem::path(TPL_TRANSCRIPT_TEST_BINARY_DIR) / "transcript_tmp" /
                     (std::string(info->test_suite_name()) + "." + info->name());
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

/// The value of `optional`, or a test failure (exception) if it is empty.
template <typename T>
const T& must(const std::optional<T>& optional) {
    if (!optional) { throw std::logic_error("optional is empty"); }
    return *optional;
}

}  // namespace tpl::transcript::test
