// trace_loader.cpp
#include "trace_loader.h"
#include <fstream>
#include <sstream>
#include <iostream>

std::vector<Request> TraceLoader::load_from_json(const std::string& filepath)
{
    std::vector<Request> trace;
    std::ifstream file(filepath);
    // 简化版：实际推荐集成 nlohmann/json
    // 这里演示思路
    std::string line;
    uint64_t id = 0;
    while (std::getline(file, line)) {
        if (line.empty()) continue;
        Request req{};
        req.request_id = id++;
        req.input_size = 1024 + (id % 10000);   // 示例解析逻辑
        req.priority = id % 5;
        req.operator_name = "vector_add";
        Estimator::estimate(req);
        trace.push_back(req);
    }
    return trace;
}