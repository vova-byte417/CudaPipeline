#pragma once

#include <vector>

#include "request.h"

class Batch
{
public:

    std::vector<Request> requests;

    int total_input_size = 0;
};