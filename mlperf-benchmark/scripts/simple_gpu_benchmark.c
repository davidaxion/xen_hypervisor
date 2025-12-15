/*
 * Simple GPU Benchmark using CUDA
 * Measures memory bandwidth and compute performance
 */

#include <cuda.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

// Fix for CUDA 13.x API compatibility
#define cuCtxCreate cuCtxCreate_v2

#define CHECK_CUDA(call) do { \
    CUresult res = call; \
    if (res != CUDA_SUCCESS) { \
        const char *errStr; \
        cuGetErrorString(res, &errStr); \
        fprintf(stderr, "CUDA Error at %s:%d - %s\n", __FILE__, __LINE__, errStr); \
        exit(1); \
    } \
} while(0)

double get_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

void benchmark_memory_bandwidth(CUcontext ctx, size_t size_mb) {
    printf("\n=== Memory Bandwidth Benchmark ===\n");
    printf("Size: %zu MB\n", size_mb);

    size_t size = size_mb * 1024 * 1024;
    char *host_data = (char *)malloc(size);
    memset(host_data, 0xAB, size);

    CUdeviceptr device_ptr;
    CHECK_CUDA(cuMemAlloc(&device_ptr, size));

    // Host to Device
    double start = get_time();
    CHECK_CUDA(cuMemcpyHtoD(device_ptr, host_data, size));
    CHECK_CUDA(cuCtxSynchronize());
    double h2d_time = get_time() - start;
    double h2d_bandwidth = (size / h2d_time) / (1024.0 * 1024.0 * 1024.0);

    // Device to Host
    start = get_time();
    CHECK_CUDA(cuMemcpyDtoH(host_data, device_ptr, size));
    CHECK_CUDA(cuCtxSynchronize());
    double d2h_time = get_time() - start;
    double d2h_bandwidth = (size / d2h_time) / (1024.0 * 1024.0 * 1024.0);

    printf("Host to Device: %.2f GB/s\n", h2d_bandwidth);
    printf("Device to Host: %.2f GB/s\n", d2h_bandwidth);

    CHECK_CUDA(cuMemFree(device_ptr));
    free(host_data);
}

void benchmark_throughput(CUcontext ctx, size_t alloc_size, int iterations) {
    printf("\n=== Throughput Benchmark (Alloc/Free) ===\n");
    printf("Allocation size: %zu KB\n", alloc_size / 1024);
    printf("Iterations: %d\n", iterations);

    double start = get_time();

    for (int i = 0; i < iterations; i++) {
        CUdeviceptr ptr;
        CHECK_CUDA(cuMemAlloc(&ptr, alloc_size));
        CHECK_CUDA(cuMemFree(ptr));
    }

    CHECK_CUDA(cuCtxSynchronize());
    double elapsed = get_time() - start;
    double ops_per_sec = iterations / elapsed;

    printf("Total time: %.3f seconds\n", elapsed);
    printf("Throughput: %.0f ops/sec\n", ops_per_sec);
    printf("Average latency: %.2f ms\n", (elapsed / iterations) * 1000);
}

void benchmark_latency(CUcontext ctx, size_t alloc_size, int samples) {
    printf("\n=== Latency Benchmark ===\n");
    printf("Allocation size: %zu KB\n", alloc_size / 1024);
    printf("Samples: %d\n", samples);

    double *latencies = (double *)malloc(sizeof(double) * samples);

    for (int i = 0; i < samples; i++) {
        double start = get_time();

        CUdeviceptr ptr;
        CHECK_CUDA(cuMemAlloc(&ptr, alloc_size));
        CHECK_CUDA(cuMemFree(ptr));
        CHECK_CUDA(cuCtxSynchronize());

        latencies[i] = (get_time() - start) * 1000; // ms
    }

    // Sort for percentiles
    for (int i = 0; i < samples - 1; i++) {
        for (int j = i + 1; j < samples; j++) {
            if (latencies[i] > latencies[j]) {
                double temp = latencies[i];
                latencies[i] = latencies[j];
                latencies[j] = temp;
            }
        }
    }

    double p50 = latencies[samples / 2];
    double p90 = latencies[(int)(samples * 0.90)];
    double p99 = latencies[(int)(samples * 0.99)];

    printf("p50 latency: %.3f ms\n", p50);
    printf("p90 latency: %.3f ms\n", p90);
    printf("p99 latency: %.3f ms\n", p99);

    free(latencies);
}

int main() {
    printf("=== Simple GPU Benchmark ===\n\n");

    // Initialize CUDA
    CHECK_CUDA(cuInit(0));

    int device_count;
    CHECK_CUDA(cuDeviceGetCount(&device_count));
    printf("Found %d CUDA device(s)\n", device_count);

    if (device_count == 0) {
        fprintf(stderr, "No CUDA devices found\n");
        return 1;
    }

    CUdevice device;
    CHECK_CUDA(cuDeviceGet(&device, 0));

    char device_name[256];
    CHECK_CUDA(cuDeviceGetName(device_name, sizeof(device_name), device));
    printf("Using device: %s\n", device_name);

    size_t total_mem;
    CHECK_CUDA(cuDeviceTotalMem(&total_mem, device));
    printf("Total memory: %.2f GB\n", total_mem / (1024.0 * 1024.0 * 1024.0));

    CUcontext context;
    CHECK_CUDA(cuCtxCreate(&context, 0, device));

    // Run benchmarks
    benchmark_memory_bandwidth(context, 100); // 100MB
    benchmark_throughput(context, 1024 * 1024, 1000); // 1MB x 1000
    benchmark_latency(context, 1024 * 1024, 500); // 1MB x 500

    printf("\n=== Benchmark Complete ===\n");

    CHECK_CUDA(cuCtxDestroy(context));

    return 0;
}
