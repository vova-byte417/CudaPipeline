/**
 * CudaPipeline - 测试主程序
 *
 * 用法:
 *   ./test_runner                    运行所有测试
 *   ./test_runner --unit             只运行单元测试
 *   ./test_runner --integration      只运行集成测试
 *   ./test_runner --list             列出所有测试
 */

#include "test_common.h"
#include <iostream>
#include <cstring>
#include <vector>
#include <string>

void print_help() {
    std::cout << "\nCudaPipeline 测试套件 v2.0\n\n";
    std::cout << "用法:\n";
    std::cout << "  ./test_runner [选项]\n\n";
    std::cout << "选项:\n";
    std::cout << "  -h, --help          显示帮助\n";
    std::cout << "  -a, --all           运行所有测试 (默认)\n";
    std::cout << "  -u, --unit          只运行单元测试\n";
    std::cout << "  -i, --integration   只运行集成测试\n";
    std::cout << "  -l, --list          列出所有可用测试\n\n";
    std::cout << "示例:\n";
    std::cout << "  ./test_runner --unit\n";
    std::cout << "  ./test_runner --integration\n\n";
}

extern void run_unit_tests();
extern void run_integration_tests();

int main(int argc, char* argv[]) {
    bool run_unit = true;
    bool run_integration = true;
    bool list_only = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else if (strcmp(argv[i], "-a") == 0 || strcmp(argv[i], "--all") == 0) {
            run_unit = true;
            run_integration = true;
        } else if (strcmp(argv[i], "-u") == 0 || strcmp(argv[i], "--unit") == 0) {
            run_unit = true;
            run_integration = false;
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--integration") == 0) {
            run_unit = false;
            run_integration = true;
        } else if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--list") == 0) {
            list_only = true;
        }
    }

    if (list_only) {
        std::cout << "可用测试分类:\n";
        std::cout << "  [ResourceQuota]  - 资源配额管理器测试\n";
        std::cout << "  [FCFSScheduler] - FCFS 调度器测试\n";
        std::cout << "  [PriorityScheduler] - 优先级调度器测试\n";
        std::cout << "  [RLScheduler]   - RL 调度器测试\n";
        std::cout << "  [RequestQueue]  - 请求队列测试\n";
        std::cout << "  [EndToEnd]      - 端到端流程测试\n";
        std::cout << "  [Concurrency]   - 并发测试\n";
        std::cout << "  [Isolation]     - 资源隔离测试\n";
        std::cout << "  [Performance]   - 性能基准测试\n";
        return 0;
    }

    // 运行测试
    return test::TestRunner::instance().run_all();
}
