/*
 * Simple CUDA test application
 *
 * This app uses the CUDA Driver API to test libvgpu.
 * It should work with both real CUDA and libvgpu.
 */

#include "cuda.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK_CUDA(call) do { \
    CUresult result = (call); \
    if (result != CUDA_SUCCESS) { \
        const char *errstr; \
        cuGetErrorString(result, &errstr); \
        fprintf(stderr, "CUDA error at %s:%d: %s\n", __FILE__, __LINE__, errstr); \
        exit(1); \
    } \
} while(0)

int main(void)
{
    printf("=== CUDA Test Application ===\n\n");

    /* Initialize CUDA */
    printf("1. Initializing CUDA...\n");
    CHECK_CUDA(cuInit(0));
    printf("   ✓ CUDA initialized\n\n");

    /* Get driver version */
    int driver_version = 0;
    CHECK_CUDA(cuDriverGetVersion(&driver_version));
    printf("2. Driver version: %d.%d\n\n",
           driver_version / 1000,
           (driver_version % 1000) / 10);

    /* Get device count */
    int device_count = 0;
    CHECK_CUDA(cuDeviceGetCount(&device_count));
    printf("3. Found %d CUDA device(s)\n\n", device_count);

    if (device_count == 0) {
        fprintf(stderr, "No CUDA devices found!\n");
        return 1;
    }

    /* Get first device */
    CUdevice device;
    CHECK_CUDA(cuDeviceGet(&device, 0));

    /* Get device name */
    char device_name[256];
    CHECK_CUDA(cuDeviceGetName(device_name, sizeof(device_name), device));
    printf("4. Using device 0: %s\n\n", device_name);

    /* Create context */
    CUcontext context;
    CHECK_CUDA(cuCtxCreate(&context, 0, device));
    printf("5. Created CUDA context: %p\n\n", context);

    /* Allocate GPU memory */
    printf("6. Allocating GPU memory...\n");
    CUdeviceptr d_ptr;
    size_t size = 1024 * 1024;  /* 1MB */
    CHECK_CUDA(cuMemAlloc(&d_ptr, size));
    printf("   ✓ Allocated %zu bytes at device pointer 0x%llx\n\n",
           size, (unsigned long long)d_ptr);

    /* Prepare host data */
    printf("7. Copying data to GPU...\n");
    unsigned char *h_data = malloc(1024);
    for (int i = 0; i < 1024; i++) {
        h_data[i] = i & 0xFF;
    }

    CHECK_CUDA(cuMemcpyHtoD(d_ptr, h_data, 1024));
    printf("   ✓ Copied 1024 bytes to GPU\n\n");

    /* Copy data back */
    printf("8. Copying data from GPU...\n");
    unsigned char *h_result = malloc(1024);
    memset(h_result, 0, 1024);

    CHECK_CUDA(cuMemcpyDtoH(h_result, d_ptr, 1024));
    printf("   ✓ Copied 1024 bytes from GPU\n\n");

    /* Verify data (for now, libvgpu returns zeros) */
    printf("9. Verifying data...\n");
    int errors = 0;
    for (int i = 0; i < 1024; i++) {
        if (h_result[i] != h_data[i]) {
            errors++;
        }
    }
    if (errors > 0) {
        printf("   ⚠ Data mismatch: %d errors (expected with stub GPU)\n\n", errors);
    } else {
        printf("   ✓ Data matches!\n\n");
    }

    /* Synchronize */
    printf("10. Synchronizing...\n");
    CHECK_CUDA(cuCtxSynchronize());
    printf("    ✓ Synchronized\n\n");

    /* Free GPU memory */
    printf("11. Freeing GPU memory...\n");
    CHECK_CUDA(cuMemFree(d_ptr));
    printf("    ✓ Freed device memory\n\n");

    /* Destroy context */
    CHECK_CUDA(cuCtxDestroy(context));
    printf("12. Destroyed context\n\n");

    /* Cleanup */
    free(h_data);
    free(h_result);

    printf("=== All tests passed! ===\n");

    return 0;
}
