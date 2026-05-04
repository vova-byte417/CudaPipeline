#include <cuda_runtime.h>

// ============================================
// SGEMM: C = A * B + C
// A: M x K
// B: K x N
// C: M x N
// ============================================

// Naive版本（正确性优先）
__global__ void sgemm_naive_kernel(int M, int N, int K,
    const float* A, const float* B, float* C) {
    
    int row = blockIdx.y * blockDim.y + threadIdx.y;
    int col = blockIdx.x * blockDim.x + threadIdx.x;
    
    if (row < M && col < N) {
        float sum = 0.0f;
        for (int k = 0; k < K; ++k) {
            sum += A[row * K + k] * B[k * N + col];
        }
        C[row * N + col] = sum;
    }
}

// Tiled优化版本（展示性能优化思路）
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
        // 加载到shared memory
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
        
        // 计算
        for (int k = 0; k < BLOCK_SIZE; ++k) {
            sum += As[ty][k] * Bs[k][tx];
        }
        
        __syncthreads();
    }
    
    if (row < M && col < N) {
        C[row * N + col] = sum;
    }
}

extern "C" void sgemm_naive(int M, int N, int K,
    const float* A, const float* B, float* C) {
    
    dim3 block(16, 16);
    dim3 grid((N + 15) / 16, (M + 15) / 16);
    
    sgemm_naive_kernel<<<grid, block>>>(M, N, K, A, B, C);
}

extern "C" void sgemm_tiled(int M, int N, int K,
    const float* A, const float* B, float* C) {
    
    dim3 block(16, 16);
    dim3 grid((N + 15) / 16, (M + 15) / 16);
    
    sgemm_tiled_kernel<<<grid, block>>>(M, N, K, A, B, C);
}

// 兼容原有接口：用方阵乘法
extern "C" void sgemm(float* A, float* B, float* C, int n) {
    // n是矩阵边长 M=N=K=n
    sgemm_tiled(n, n, n, A, B, C);
}
