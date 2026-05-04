#pragma once

#include <string>
#include <cstdint>

class Request
{
public:

    // request tracing
    uint64_t request_id;

    // execution metadata
    int input_size;
    int output_size;

    int priority;

    // operator dispatch
    std::string operator_name;

    // host buffers
    float* h_a;
    float* h_b;
    float* h_c;

    // timestamp
    uint64_t enqueue_ts;
};