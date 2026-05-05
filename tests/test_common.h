#pragma once

#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <functional>

// 简单的单元测试框架
namespace test {

struct TestResult {
    bool passed = true;
    std::string message;
    std::chrono::microseconds duration{0};
};

struct TestCase {
    std::string name;
    std::string category;
    std::function<TestResult()> test_func;
};

class TestRunner {
public:
    static TestRunner& instance() {
        static TestRunner runner;
        return runner;
    }

    void add_test(const std::string& category, const std::string& name, std::function<TestResult()> func) {
        tests_.push_back({name, category, func});
    }

    int run_all() {
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "           CudaPipeline - 单元测试套件 v2.0           \n";
        std::cout << std::string(70, '=') << "\n\n";

        int total = 0;
        int passed = 0;
        int failed = 0;

        std::string current_category;

        for (const auto& test : tests_) {
            if (test.category != current_category) {
                current_category = test.category;
                std::cout << "[" << current_category << "]\n";
            }

            std::cout << "  ► " << test.name << "... " << std::flush;

            auto start = std::chrono::high_resolution_clock::now();
            TestResult result = test.test_func();
            auto end = std::chrono::high_resolution_clock::now();
            result.duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

            total++;

            if (result.passed) {
                std::cout << "✅ PASS (" << result.duration.count() << " us)\n";
                passed++;
            } else {
                std::cout << "❌ FAIL\n";
                std::cout << "    原因: " << result.message << "\n";
                failed++;
            }
        }

        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "测试结果汇总:\n";
        std::cout << "  总计: " << total << "\n";
        std::cout << "  ✅ 通过: " << passed << "\n";
        std::cout << "  ❌ 失败: " << failed << "\n";
        std::cout << std::string(70, '=') << "\n";

        if (failed > 0) {
            std::cout << "\n❌ 部分测试失败，请检查代码!\n\n";
            return 1;
        } else {
            std::cout << "\n🎉 所有测试通过!\n\n";
            return 0;
        }
    }

private:
    TestRunner() = default;
    std::vector<TestCase> tests_;
};

// 测试注册宏
#define TEST_ASSERT(condition, msg) \
    if (!(condition)) { \
        TestResult r; \
        r.passed = false; \
        r.message = msg "  [文件: " __FILE__ " 行: " + std::to_string(__LINE__) + "]"; \
        return r; \
    }

#define TEST_ASSERT_EQ(a, b, msg) \
    if ((a) != (b)) { \
        TestResult r; \
        r.passed = false; \
        r.message = std::string(msg) + " 期望: " + std::to_string(a) + " 实际: " + std::to_string(b); \
        return r; \
    }

#define TEST_CATEGORY(category, name) \
    TestResult test_##category##_##name(); \
    namespace { bool _registered_##category##_##name = [](){ \
        TestRunner::instance().add_test(#category, #name, test_##category##_##name); \
        return true; \
    }(); } \
    TestResult test_##category##_##name()

} // namespace test
