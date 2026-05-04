#pragma once
#include "request.h"
#include <string>

class Estimator {
public:
    static void estimate(Request& req);

private:
    static float simple_exec_time_model(int input_size, const std::string& op_name);
    static uint32_t flops_model(int input_size, const std::string& op_name);
};