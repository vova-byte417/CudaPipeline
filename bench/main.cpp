#include <thread>
#include <chrono>
#include <iostream>

#include "cpu_backend.h"
#include "queue.h"
#include "runtime/worker.h"

#include "scheduler/fcfs_scheduler.h"

#include "metrics/metrics.h"

#include "request.h"


int main()
{
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "           CudaPipeline - Runtime Benchmark           " << std::endl;
    std::cout << std::string(60, '=') << "\n" << std::endl;

    // --------------------------------
    // runtime components
    // --------------------------------

    CPUBackend backend;

    RequestQueue queue;

    FCFS_Scheduler scheduler;

    Metrics metrics;

    Worker worker(
        &backend,
        &queue,
        &scheduler,
        &metrics
    );

    // --------------------------------
    // init backend
    // --------------------------------

    backend.initialize();

    // --------------------------------
    // create requests
    // --------------------------------

    constexpr int n = 1024;

    for (int r = 0; r < 5; r++)
    {
        float* a = new float[n];
        float* b = new float[n];
        float* c = new float[n];

        for (int i = 0; i < n; i++)
        {
            a[i] = i;
            b[i] = i;
        }

        Request req;

        req.request_id = r;

        req.input_size = n;
        req.output_size = n;

        req.priority = Priority::MEDIUM;

        req.operator_name = "vector_add";

        req.h_a = a;
        req.h_b = b;
        req.h_c = c;

        queue.push(req);
    }

    // --------------------------------
    // start runtime
    // --------------------------------

    worker.start();

    // --------------------------------
    // wait
    // --------------------------------

    std::this_thread::sleep_for(
        std::chrono::seconds(2)
    );

    // --------------------------------
    // shutdown
    // --------------------------------

    worker.stop();

    backend.shutdown();

    metrics.print();

    return 0;
}