/*
 * GPU Proxy Daemon
 *
 * Runs in driver domain, has exclusive GPU access.
 * Receives IDM messages from user domains and calls real CUDA driver.
 */

#include "../idm-protocol/idm.h"
#include "handle_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

#ifndef STUB_CUDA
#include <cuda.h>
#else
/* Stub CUDA types */
typedef int CUdevice;
typedef void *CUcontext;
typedef int CUresult;
#define CUDA_SUCCESS 0

static inline CUresult cuInit(unsigned int flags) {
    (void)flags;
    printf("[STUB] cuInit called\n");
    return CUDA_SUCCESS;
}

static inline CUresult cuDeviceGetCount(int *count) {
    *count = 1;
    printf("[STUB] cuDeviceGetCount: 1 device\n");
    return CUDA_SUCCESS;
}

static inline CUresult cuDeviceGet(CUdevice *device, int ordinal) {
    *device = ordinal;
    printf("[STUB] cuDeviceGet: device %d\n", ordinal);
    return CUDA_SUCCESS;
}

static inline CUresult cuDeviceGetName(char *name, int len, CUdevice dev) {
    snprintf(name, len, "STUB GPU Device %d", dev);
    printf("[STUB] cuDeviceGetName: %s\n", name);
    return CUDA_SUCCESS;
}

static inline CUresult cuCtxCreate(CUcontext *pctx, unsigned int flags, CUdevice dev) {
    (void)flags;
    (void)dev;
    *pctx = (void *)0x12345678;
    printf("[STUB] cuCtxCreate: context created\n");
    return CUDA_SUCCESS;
}

static inline CUresult cuGetErrorString(CUresult error, const char **pStr) {
    *pStr = "stub error";
    return CUDA_SUCCESS;
}
#endif

/* Forward declarations */
extern int idm_init(uint32_t local_zone_id, uint32_t remote_zone_id, bool is_server);
extern int idm_recv(struct idm_message **msg_out, int timeout_ms);
extern void idm_free_message(struct idm_message *msg);
extern void idm_cleanup(void);

extern void handle_gpu_alloc(const struct idm_message *msg);
extern void handle_gpu_free(const struct idm_message *msg);
extern void handle_gpu_copy_h2d(const struct idm_message *msg);
extern void handle_gpu_copy_d2h(const struct idm_message *msg);
extern void handle_gpu_sync(const struct idm_message *msg);

/* Zone IDs */
#define DRIVER_ZONE_ID 1
#define USER_ZONE_ID 2

/* Global state */
static volatile sig_atomic_t running = 1;

/**
 * Signal handler
 */
static void signal_handler(int sig)
{
    (void)sig;
    running = 0;
}

/**
 * Initialize CUDA
 */
static int init_cuda(void)
{
    printf("Initializing CUDA...\n");

    CUresult res = cuInit(0);
    if (res != CUDA_SUCCESS) {
        const char *err_str;
        cuGetErrorString(res, &err_str);
        fprintf(stderr, "cuInit failed: %s\n", err_str);
        return -1;
    }

    /* Get device count */
    int device_count = 0;
    res = cuDeviceGetCount(&device_count);
    if (res != CUDA_SUCCESS) {
        const char *err_str;
        cuGetErrorString(res, &err_str);
        fprintf(stderr, "cuDeviceGetCount failed: %s\n", err_str);
        return -1;
    }

    if (device_count == 0) {
        fprintf(stderr, "No CUDA devices found!\n");
        return -1;
    }

    printf("Found %d CUDA device(s)\n", device_count);

    /* Get first device */
    CUdevice device;
    res = cuDeviceGet(&device, 0);
    if (res != CUDA_SUCCESS) {
        const char *err_str;
        cuGetErrorString(res, &err_str);
        fprintf(stderr, "cuDeviceGet failed: %s\n", err_str);
        return -1;
    }

    /* Get device name */
    char device_name[256];
    res = cuDeviceGetName(device_name, sizeof(device_name), device);
    if (res == CUDA_SUCCESS) {
        printf("Using device: %s\n", device_name);
    }

    /* Create context */
    CUcontext context;
    res = cuCtxCreate(&context, 0, device);
    if (res != CUDA_SUCCESS) {
        const char *err_str;
        cuGetErrorString(res, &err_str);
        fprintf(stderr, "cuCtxCreate failed: %s\n", err_str);
        return -1;
    }

    printf("CUDA initialized successfully\n\n");

    return 0;
}

/**
 * Print statistics
 */
static void print_stats(void)
{
    uint64_t total_handles, total_memory;
    handle_table_stats(&total_handles, &total_memory);

    printf("\n=== Statistics ===\n");
    printf("Active handles: %lu\n", total_handles);
    printf("Total GPU memory: %lu bytes (%.2f MB)\n",
           total_memory,
           total_memory / (1024.0 * 1024.0));
    printf("==================\n\n");
}

/**
 * Main loop
 */
static int run_server(void)
{
    printf("=== GPU Proxy Daemon ===\n");
    printf("Driver Zone ID: %d\n", DRIVER_ZONE_ID);
    printf("User Zone ID: %d\n\n", USER_ZONE_ID);

    /* Initialize IDM */
    printf("Initializing IDM...\n");
    if (idm_init(DRIVER_ZONE_ID, USER_ZONE_ID, true) < 0) {
        fprintf(stderr, "Failed to initialize IDM\n");
        return 1;
    }
    printf("IDM initialized\n\n");

    /* Initialize handle table */
    if (handle_table_init() < 0) {
        fprintf(stderr, "Failed to initialize handle table\n");
        idm_cleanup();
        return 1;
    }

    /* Initialize CUDA */
    if (init_cuda() < 0) {
        handle_table_cleanup();
        idm_cleanup();
        return 1;
    }

    printf("Ready to process GPU requests...\n\n");

    /* Main loop */
    int requests_handled = 0;
    while (running) {
        struct idm_message *msg = NULL;

        /* Receive message (blocking with 1 second timeout) */
        int ret = idm_recv(&msg, 1000);
        if (ret < 0) {
            if (ret == -EAGAIN) {
                /* Timeout - check if we should exit */
                continue;
            }
            fprintf(stderr, "idm_recv failed: %d\n", ret);
            continue;
        }

        /* Dispatch based on message type */
        switch (msg->header.msg_type) {
            case IDM_GPU_ALLOC:
                handle_gpu_alloc(msg);
                break;

            case IDM_GPU_FREE:
                handle_gpu_free(msg);
                break;

            case IDM_GPU_COPY_H2D:
                handle_gpu_copy_h2d(msg);
                break;

            case IDM_GPU_COPY_D2H:
                handle_gpu_copy_d2h(msg);
                break;

            case IDM_GPU_SYNC:
                handle_gpu_sync(msg);
                break;

            default:
                fprintf(stderr, "Unknown message type: 0x%x\n", msg->header.msg_type);
                break;
        }

        idm_free_message(msg);

        requests_handled++;

        /* Print stats every 100 requests */
        if (requests_handled % 100 == 0) {
            print_stats();
        }
    }

    printf("\nShutting down...\n");
    print_stats();

    /* Cleanup */
    handle_table_cleanup();
    idm_cleanup();

    printf("GPU Proxy Daemon exited\n");

    return 0;
}

/**
 * Main
 */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    return run_server();
}
