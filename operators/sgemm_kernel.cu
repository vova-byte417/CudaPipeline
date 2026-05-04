#include <cuda_runtime.h>

__global__ void sgemm_tiled_kernel(int M, int N, int K,
    const float* A, const float* B, float* C) {
    
    const int BLOCK_SIZE = 16;
    __shared__ float As[16][16];
    __shared__ float Bs[16][16];
    
    int bx = blockIdx.x;
    int by = blockIdx.y;
    int tx = threadIdx.x;
    int ty = threadIdx.y;
    
    int row = by * BLOCK_SIZE + ty;
    int col = bx * BLOCK_SIZE + tx;
    
    float sum = 0.0f;
    
    for (int t = 0; t < (K + BLOCK_SIZE - 1) / BLOCK_SIZE; ++t) {
        if (row < M && t * BLOCK_SIZE + tx < K) {
            As[ty][tx] = A[row * K + t * BLOCK_SIZE + tx];
        } else {
            As[ty][tx] = 0.0f;
        }
        
        if (col < N && t * BLOCK_SIZE + ty < K) {
            Bs[ty][tx] = B[(t * BLOCK_SIZE + ty) * N + col];
        } else {
            Bs[ty][tx] = 0.0f;
        }
        
        __syncthreads();
        
        for (int k = 0; k < BLOCK_SIZE; ++k) {
            sum += As[ty][k] * Bs[k][tx];
        }
        
        __syncthreads();
    }
    
    if (row < M && col < N) {
        C[row * N + col] = sum;
    }
}

// 导出C接口，给C++调用
extern "C" void sgemm_tiled(int n, float* A, float* B, float* C, cudaStream_t stream) {
    dim3 block(16, 16);
    dim3 grid((n + 15) / 16, (n + 15) / 16);
    sgemm_tiled_kernel<<<grid, block, 0, stream>>>(n, n, n, A, B, C);
}
