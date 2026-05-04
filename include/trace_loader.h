#pragma once
#include "estimator.h"
#include "request.h"
#include <vector>
#include <string>

class TraceLoader {
public:
    static std::vector<Request> load_from_json(const std::string& filepath);
    static std::vector<Request> load_from_csv(const std::string& filepath);

};