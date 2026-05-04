#pragma once

#include <thread>
#include <atomic>

#include "backend.h"
#include "queue.h"
#include "scheduler/scheduler.h"

#include "metrics/metrics.h"


class Worker
{
public:

    Worker(
        Backend* backend,
        RequestQueue* queue,
        Scheduler* scheduler,
        Metrics* metrics
    );

    void start();
    void stop();

private:

    void loop();

    Backend* backend_;
    RequestQueue* queue_;
    Scheduler* scheduler_;
    Metrics* metrics_;

    std::thread thread_;
    std::atomic<bool> running_;
};