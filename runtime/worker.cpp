#include "runtime/worker.h"
#include "runtime/batch.h"

#include <iostream>

Worker::Worker(
    Backend* backend,
    RequestQueue* queue,
    Scheduler* scheduler,
    Metrics* metrics
) : backend_(backend), queue_(queue), scheduler_(scheduler), metrics_(metrics),
    running_(false)
{
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

        // 调度选择任务
        if (scheduler_->select_batch(*queue_, batch))
        {
            auto mid = std::chrono::high_resolution_clock::now();

            // 执行GPU backend
            std::cout << "[Worker] batch=" << batch.requests.size() 
                      << " requests" << std::endl;

            for (auto& req : batch.requests) {
                std::cout << "[Worker] executing req=" << req.request_id 
                          << " size=" << req.input_size << std::endl;
                backend_->submit(req);
            }

            auto end = std::chrono::high_resolution_clock::now();

            // 记录指标
            metrics_->record_queue_latency(
                std::chrono::duration_cast<std::chrono::nanoseconds>(mid - start).count()
            );

            metrics_->record_execution_latency(
                std::chrono::duration_cast<std::chrono::nanoseconds>(end - mid).count()
            );
            
            metrics_->record_batch_size(batch.requests.size());
        }
        else
        {
            // 队列为空时短暂休眠，避免CPU空转
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}
