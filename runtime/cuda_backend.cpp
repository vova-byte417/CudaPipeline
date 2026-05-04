#include "cuda_backend.h"
#include "request.h"
#include "runtime/batch.h"

#include <cuda_runtime.h>
#include <dlfcn.h>

#include <iostream>
#include <vector>


#define CHECK_CUDA(x)                                   \
do {                                                    \
    cudaError_t err = (x);                              \
    if (err != cudaSuccess)                             \
    {                                                   \
        std::cerr                                       \
            << "CUDA Error: "                           \
            << cudaGetErrorString(err)                  \
            << std::endl;                               \
    }                                                   \
} while(0)

CUDABackend::CUDABackend()
    : stream_(nullptr),
    operator_handle_(nullptr),
    vector_add_(nullptr)
{

}

CUDABackend::~CUDABackend()
{
}

bool CUDABackend::initialize()
{
    cudaSetDevice(0);

    cudaStreamCreate(&stream_);

    return load_operator(
        "./build/operators/libvector_add.so"
    );
}

bool CUDABackend::load_operator(
    const std::string& path
)
{
    operator_handle_ =
        dlopen(
            // path.c_str(),
            "./build/operators/libvector_add.so",
            RTLD_NOW
        );

    if (!operator_handle_)
    {
        std::cerr
            << "dlopen failed"
            << std::endl;

        return false;
    }

    std::cout << "dlopen success!" << std::endl;

    void* sym = dlsym(operator_handle_, "vector_add");
    if (sym) {
        vector_add_ = reinterpret_cast<operator_func_t>(sym);
        std::cout << "dlsym success" << std::endl;
    } else {
        // 处理错误
        std::cerr << "dlsym failed: " << dlerror() << std::endl;
        return false;
    }

    if (!vector_add_)
    {
        std::cerr
            << "dlsym failed"
            << std::endl;

        return false;
    } else {
         std::cout << "vector add success" 
         << " vector_add_ = " << (void*)vector_add_<< std::endl;
    }

    return true;
}

bool CUDABackend::submit(Request& req)
{
    int n = req.input_size;  // 获取输入数据大小
    if (n <= 0) return false;

    // 准备 Host 数据
    // 假设 Request 中存储了指向主机数据的指针
    std::vector<float> h_a(n), h_b(n), h_c(n);
    
    // 假设请求包含输入数据
    for (int i = 0; i < n; i++) {
        h_a[i] = req.h_a[i];  // 假设 Request 中包含 `input_data_a` 数组
        h_b[i] = req.h_b[i];  // 假设 Request 中包含 `input_data_b` 数组
    }

    // 为设备数据分配内存
    float *d_a = nullptr, *d_b = nullptr, *d_c = nullptr;
    CHECK_CUDA(cudaMalloc(&d_a, n * sizeof(float)));
    CHECK_CUDA(cudaMalloc(&d_b, n * sizeof(float)));
    CHECK_CUDA(cudaMalloc(&d_c, n * sizeof(float)));

    // 将主机数据拷贝到设备
    CHECK_CUDA(cudaMemcpyAsync(d_a, h_a.data(), n * sizeof(float), cudaMemcpyHostToDevice, stream_));
    CHECK_CUDA(cudaMemcpyAsync(d_b, h_b.data(), n * sizeof(float), cudaMemcpyHostToDevice, stream_));

    // 调用 CUDA 核心函数（假设已经通过动态加载的方式获得该函数指针）
    if (vector_add_) {
        std::cout << "[CUDA] Calling kernel with n = " << n << std::endl;
        vector_add_(d_a, d_b, d_c, n);  // 执行 CUDA 核心计算
    } else {
        std::cerr << "Error: vector_add_ is nullptr!" << std::endl;
        // 清理内存
        cudaFree(d_a); cudaFree(d_b); cudaFree(d_c);
        return false;
    }

    // 从设备拷贝结果回主机
    CHECK_CUDA(cudaMemcpyAsync(h_c.data(), d_c, n * sizeof(float), cudaMemcpyDeviceToHost, stream_));

    // 等待设备计算完成
    cudaError_t err = cudaStreamSynchronize(stream_);
    if (err != cudaSuccess) {
        std::cerr << "cudaStreamSynchronize failed: " << cudaGetErrorString(err) << std::endl;
        // 清理内存
        cudaFree(d_a); cudaFree(d_b); cudaFree(d_c);
        return false;
    }

    // 输出计算结果（显示前 3 个结果）
    std::cout << "[CUDA] result[0] = " << h_c[0] << std::endl;
    std::cout << "[CUDA] result[1] = " << h_c[1] << std::endl;
    std::cout << "[CUDA] result[2] = " << h_c[2] << std::endl;

    // 清理设备内存
    cudaFree(d_a);
    cudaFree(d_b);
    cudaFree(d_c);

    return true;
}

bool CUDABackend::submit_batch(
    Batch& batch
)
{
    std::cout
        << "[backend] batch execute size = "
        << batch.requests.size()
        << std::endl;

    for (auto& req : batch.requests)
    {
        std::cout
            << "[backend] dispatch req "
            << req.request_id
            << std::endl;

        submit(req);
    }

    return true;
}

void CUDABackend::shutdown()
{
    cudaStreamDestroy(stream_);

    if (operator_handle_)
    {
        dlclose(operator_handle_);
    }
}