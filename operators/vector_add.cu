#include <cuda_runtime.h>

__global__
void vector_add_kernel(
    float* a,
    float* b,
    float* c,
    int n
)
{
    int idx =
        blockIdx.x *
        blockDim.x +
        threadIdx.x;

    if (idx < n)
    {
        c[idx] = a[idx] + b[idx];
    }
}

extern "C"
void vector_add(
    float* a,
    float* b,
    float* c,
    int n
)
{
    vector_add_kernel<<<
        (n + 255) / 256,
        256
    >>>(
        a,
        b,
        c,
        n
    );
}