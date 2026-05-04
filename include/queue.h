#pragma once

#include <queue>
#include <mutex>

#include "request.h"

class RequestQueue
{
public:

    void push(const Request& req)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(req);
    }

    bool pop(Request& req)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (queue_.empty())
            return false;

        req = queue_.front();
        queue_.pop();

        return true;
    }

private:

    std::queue<Request> queue_;

    std::mutex mutex_;
};