#include "runtime/worker.h"
#include "runtime/batch.h"

#include <iostream>

Worker::Worker(
    Backend* backend,
    RequestQueue* queue,
    Scheduler* scheduler,
    Metrics* metrics
)
{
    backend_ = backend;
    queue_ = queue;
    scheduler_ = scheduler;
    metrics_ = metrics;
    running_ = false;
}

void Worker::start()
{
    running_ = true;
    thread_ = std::thread(&Worker::loop, this);
}

void Worker::stop()
{
    running_ = false;

    if (thread_.joinable())
        thread_.join();
}

void Worker::loop()
{
    while (running_)
    {
        // 优雅等待：没有请求时不占用 CPU
        bool has_request = queue_->wait_for_request(std::chrono::milliseconds(50));
        if (!has_request) {
            continue;  // 超时，继续循环检查 running_ 标志
        }

        Batch batch;

        auto start = std::chrono::high_resolution_clock::now();

        // 1. 调度选择任务
        if (scheduler_->select_batch(*queue_, batch))
        {
            auto mid = std::chrono::high_resolution_clock::now();

            // 2. 执行 GPU backend - TRUE BATCH PROCESSING
            std::cout << "[Worker] executing batch with " << batch.requests.size() << " requests." << std::endl;
            backend_->submit_batch(batch);  // 批量提交，不是逐个！

            auto end = std::chrono::high_resolution_clock::now();

            // 3. metrics：记录各种指标
            metrics_->record_queue_latency(
                std::chrono::duration_cast<std::chrono::nanoseconds>(mid - start).count()
            );

            metrics_->record_execution_latency(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - mid).count()
            );

            metrics_->record_batch_size(static_cast<int>(batch.requests.size()));
            metrics_->increment_requests(static_cast<int>(batch.requests.size()));
        }
    }
}