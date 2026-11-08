#ifndef TEST_CASES_H
#define TEST_CASES_H

#include <string>
#include <vector>

// Test case structure
struct TestCase {
    std::string name;
    void (*run)() = nullptr;
};

// Test registry + self-registration hooks (defined in test_main.cpp).
std::vector<TestCase>& test_registry();
bool register_test(const std::string& name, void (*fn)());

/**
 * REGISTER_TEST(test_fn) - define and register a test in one line.
 *
 * Replaces the old three-place ceremony (declare in test_cases.h, define in
 * the test file, add to the static registry in test_main.cpp). Now a new test
 * is a one-file change: write
 *
 *     REGISTER_TEST(test_something) {
 *         ...
 *     }
 *
 * and it is appended to the registry at static-init time. The filter string
 * registered with --filter / ctest is exactly the function name.
 */
#define REGISTER_TEST(test_fn)                                                                    \
    static void test_fn();                                                                        \
    [[maybe_unused]] static const bool test_fn##_registered_ = register_test(#test_fn, &test_fn); \
    static void test_fn()

#endif  // TEST_CASES_H
