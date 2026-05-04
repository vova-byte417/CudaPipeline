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
        Batch batch;

        auto start = std::chrono::high_resolution_clock::now();

        // 1. 调度选择任务
        if (scheduler_->select_batch(*queue_, batch))
        {
            auto mid = std::chrono::high_resolution_clock::now();

            // 2. 执行 GPU backend
            std::cout << "[Worker] executing batch with " << batch.requests.size() << " requests." << std::endl;

            // 逐个请求提交并输出 request_id
            for (auto& req : batch.requests)
            {
                std::cout << "[Worker] executing request with ID " << req.request_id << std::endl;
                backend_->submit(req);  // 提交单个请求
            }

            auto end = std::chrono::high_resolution_clock::now();

            // 3. metrics：queue latency
            metrics_->record_queue_latency(
                std::chrono::duration_cast<std::chrono::nanoseconds>(mid - start).count()
            );

            // 4. metrics：execution latency
            metrics_->record_execution_latency(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - mid).count()
            );
        }
    }
}