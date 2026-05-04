# CudaPipeline

CudaPipeline 是一个基于 CUDA 的计算任务流水线框架，旨在通过高效的任务调度与 GPU 加速处理，支持大规模并行任务的执行。该框架支持将计算任务从 CPU 转移到 GPU，并提供了灵活的任务调度与后端支持。

## 项目结构

- `bench/`: 用于性能测试和基准测试的文件夹。包括一个测试程序，用于验证框架的性能。
  - `main.cpp`: 程序入口，包含基准测试逻辑。
  - `runtime_bench.cpp`: 定义了 GPU 后端与调度器如何与测试任务交互。

- `runtime/`: 存放与后端处理相关的代码。
  - `cuda_backend.cpp`: GPU 后端的实现，负责将任务提交给 CUDA 进行计算。
  - `worker.cpp`: 定义 `Worker` 类，负责从请求队列中拉取任务并提交给后端执行。

- `include/`: 存放公共头文件，供各个模块调用。
  - `cuda_backend.h`: 声明了 GPU 后端的接口和实现。
  - `worker.h`: 声明了 `Worker` 类，用于执行任务。
  - `scheduler.h`: 定义任务调度器的接口，负责选择要执行的任务。
  - `metrics/metrics.h`: 用于度量计算性能的模块，记录任务的队列延迟和执行延迟。
  - `runtime/batch.h` : 该文件定义了 Batch 类或结构，主要用于批量任务的管理
  - `runtime/worker.h`: 是执行具体计算任务的组件
  - `scheduler/fcfs_scheduler.h`: `fcfs_scheduler.h`: 声明了 `FCFS_Scheduler` 类。
  - `scheduler/scheduler.h` : 定义了任务调度器的接口，所有调度器类都应实现该接口。

- `metrics/`: 负责计算任务的延迟和性能指标。
  - `metrics.cpp`: 实现了度量记录函数，用于记录并输出队列延迟和执行延迟。

- `scheduler/`: 存放与任务调度相关的代码。
  - `fcfs_scheduler.cpp`: 实现了 `FCFS_Scheduler`，这是一个简单的先来先服务调度器。


## 安装

1. **克隆代码仓库**:
   ```bash
   git clone https://github.com/vova-byte417/CudaPipeline.git
   cd CudaPipeline
2. 安装依赖:
本项目依赖 CUDA 工具包和 C++ 编译器（如 g++ 或 clang）。请确保已经安装 CUDA 和相关库。

编译项目:
```
meson setup build
meson compile -C build
```
3. TODO
 v实现 CUDABackend 类，负责 GPU 上的计算任务执行。
 v实现 Worker 类，用于任务执行。
 v实现基本的任务调度器（FCFS_Scheduler）。
 优化调度算法，支持更多调度策略（如优先级调度、轮询调度等）。
 增加多线程支持，提高计算吞吐量。
 完善性能分析模块，记录 GPU 执行延迟等指标。
 支持批量请求提交，并优化批量处理流程。
 提供更详细的文档和示例代码。
   
