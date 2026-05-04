#pragma once

#include "request.h"
#include <string>
#include <unordered_map>

class Estimator
{
public:
    struct OperatorProfile {
        double time_per_element_ns = 1.0;
        size_t memory_overhead_bytes = 0;
    };

    static void estimate(Request& req);
    static void register_profile(const std::string& op_name, const OperatorProfile& profile);

private:
    static std::unordered_map<std::string, OperatorProfile> profiles_;
};
