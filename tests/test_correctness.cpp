#include <iostream>
#include <vector>
#include <cmath>
#include <iomanip>
#include <random>

#include "cuda_backend.h"

// 测试配置
constexpr float EPSILON = 1e-3f;  // 浮点误差容忍度

// 生成随机矩阵
void generate_random_matrix(std::vector<float>& mat, int n) {
    std::mt19937 rng(42);  // 固定种子，可复现
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (int i = 0; i < n * n; ++i) {
        mat[i] = dist(rng);
    }
}

// CPU参考实现
void sgemm_cpu(int n, const float* A, const float* B, float* C) {
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            float sum = 0.0f;
            for (int k = 0; k < n; ++k) {
                sum += A[i * n + k] * B[k * n + j];
            }
            C[i * n + j] = sum;
        }
    }
}

// 计算相对误差
float compute_relative_error(const float* ref, const float* test, int n) {
    float max_error = 0.0f;
    float max_value = 0.0f;
    
    for (int i = 0; i < n * n; ++i) {
        float abs_error = std::abs(ref[i] - test[i]);
        max_error = std::max(max_error, abs_error);
        max_value = std::max(max_value, std::abs(ref[i]));
    }
    
    return max_error / (max_value + 1e-10f);
}

// 单个测试用例
bool test_sgemm_size(int n, const std::string& test_name) {
    std::cout << "  Test: " << std::setw(25) << std::left << test_name
              << " (n=" << std::setw(5) << n << ") ... ";
    
    std::vector<float> A(n * n);
    std::vector<float> B(n * n);
    std::vector<float> C_cpu(n * n, 0.0f);
    std::vector<float> C_gpu(n * n, 0.0f);
    
    generate_random_matrix(A, n);
    generate_random_matrix(B, n);
    
    // CPU计算
    sgemm_cpu(n, A.data(), B.data(), C_cpu.data());
    
    // GPU计算（通过CUDABackend）
    Request req;
    req.input_size = n;
    req.h_a = A.data();
    req.h_b = B.data();
    req.h_c = C_gpu.data();
    req.operator_name = "sgemm_tiled";
    
    CUDABackend backend;
    backend.initialize();
    bool success = backend.submit(req);
    
    if (!success) {
        std::cout << "❌ FAILED (submit error)\n";
        return false;
    }
    
    // 验证结果
    float rel_error = compute_relative_error(C_cpu.data(), C_gpu.data(), n);
    
    if (rel_error < EPSILON) {
        std::cout << "✅ PASSED (rel_error=" 
                  << std::scientific << std::setprecision(2) << rel_error << ")\n";
        return true;
    } else {
        std::cout << "❌ FAILED (rel_error=" 
                  << std::scientific << std::setprecision(2) << rel_error 
                  << " > " << EPSILON << ")\n";
        
        // 打印前几个错误的值
        std::cout << "    First 5 mismatches:\n";
        int printed = 0;
        for (int i = 0; i < n * n && printed < 5; ++i) {
            if (std::abs(C_cpu[i] - C_gpu[i]) > EPSILON) {
                std::cout << "      idx=" << i << ": CPU=" << C_cpu[i] 
                          << ", GPU=" << C_gpu[i] << "\n";
                printed++;
            }
        }
        return false;
    }
}

int main() {
    print_header("CudaPipeline Correctness Tests", 70);
    
    std::cout << "  Testing SGEMM operator\n";
    std::cout << "  Epsilon tolerance: " << EPSILON << "\n";
    std::cout << std::string(70, '-') << "\n\n";
    
    int passed = 0;
    int total = 0;
    
    // 测试矩阵大小
    struct TestCase {
        int n;
        std::string name;
    };
    
    std::vector<TestCase> test_cases = {
        { 4,    "Tiny Matrix" },
        { 16,   "Small Matrix" },
        { 64,   "Medium Matrix" },
        { 128,  "Large Matrix" },
        { 512,  "Huge Matrix" },
    };
    
    for (const auto& tc : test_cases) {
        total++;
        if (test_sgemm_size(tc.n, tc.name)) {
            passed++;
        }
    }
    
    // 总结
    std::cout << "\n" << std::string(70, '-') << "\n";
    std::cout << "  Results: " << passed << "/" << total << " tests passed\n";
    
    if (passed == total) {
        std::cout << "  ✅ All tests PASSED!\n\n";
        return 0;
    } else {
        std::cout << "  ❌ Some tests FAILED!\n\n";
        return 1;
    }
}
