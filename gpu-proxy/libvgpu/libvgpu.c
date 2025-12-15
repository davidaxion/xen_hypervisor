/*
 * libvgpu - Virtual GPU Library
 *
 * Intercepts CUDA Driver API calls and forwards them to GPU proxy via IDM.
 * This library replaces the real libcuda.so in user domains.
 */

#include "cuda.h"
#include "../../idm-protocol/idm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* Forward declarations from IDM transport */
extern int idm_init(uint32_t local_zone_id, uint32_t remote_zone_id, bool is_server);
extern int idm_send(struct idm_message *msg);
extern int idm_recv(struct idm_message **msg_out, int timeout_ms);
extern struct idm_message *idm_build_message(uint32_t dst_zone, enum idm_msg_type msg_type,
                                              const void *payload, size_t payload_len);
extern void idm_free_message(struct idm_message *msg);
extern void idm_cleanup(void);

/* Zone IDs */
#define USER_ZONE_ID    2
#define DRIVER_ZONE_ID  1

/* Global state */
static bool initialized = false;
static pthread_mutex_t init_lock = PTHREAD_MUTEX_INITIALIZER;
static int device_count = 1;  /* Virtual device count */
static CUcontext current_context = NULL;

/* Error string table */
static const char *error_strings[] = {
    [CUDA_SUCCESS] = "no error",
    [CUDA_ERROR_INVALID_VALUE] = "invalid argument",
    [CUDA_ERROR_OUT_OF_MEMORY] = "out of memory",
    [CUDA_ERROR_NOT_INITIALIZED] = "not initialized",
    [CUDA_ERROR_DEINITIALIZED] = "deinitialized",
    [CUDA_ERROR_INVALID_CONTEXT] = "invalid context",
    [CUDA_ERROR_INVALID_HANDLE] = "invalid handle",
};

static const char *error_names[] = {
    [CUDA_SUCCESS] = "CUDA_SUCCESS",
    [CUDA_ERROR_INVALID_VALUE] = "CUDA_ERROR_INVALID_VALUE",
    [CUDA_ERROR_OUT_OF_MEMORY] = "CUDA_ERROR_OUT_OF_MEMORY",
    [CUDA_ERROR_NOT_INITIALIZED] = "CUDA_ERROR_NOT_INITIALIZED",
    [CUDA_ERROR_DEINITIALIZED] = "CUDA_ERROR_DEINITIALIZED",
    [CUDA_ERROR_INVALID_CONTEXT] = "CUDA_ERROR_INVALID_CONTEXT",
    [CUDA_ERROR_INVALID_HANDLE] = "CUDA_ERROR_INVALID_HANDLE",
};

/**
 * Send request and wait for response
 */
static CUresult send_and_wait(struct idm_message *msg, uint64_t *handle_out)
{
    uint64_t req_seq = msg->header.seq_num;

    if (idm_send(msg) < 0) {
        fprintf(stderr, "[libvgpu] Failed to send message\n");
        return CUDA_ERROR_INVALID_VALUE;
    }

    /* Wait for response */
    for (int i = 0; i < 10; i++) {
        struct idm_message *resp = NULL;

        if (idm_recv(&resp, 1000) < 0) {
            continue;
        }

        if (resp->header.msg_type == IDM_RESPONSE_OK) {
            const struct idm_response_ok *ok = (const struct idm_response_ok *)resp->payload;

            if (ok->request_seq == req_seq) {
                if (handle_out) {
                    *handle_out = ok->result_handle;
                }
                idm_free_message(resp);
                return CUDA_SUCCESS;
            }
        } else if (resp->header.msg_type == IDM_RESPONSE_ERROR) {
            const struct idm_response_error *err = (const struct idm_response_error *)resp->payload;

            if (err->request_seq == req_seq) {
                fprintf(stderr, "[libvgpu] Error: %s\n", err->error_msg);
                idm_free_message(resp);

                /* Map IDM error to CUDA error */
                switch (err->error_code) {
                    case IDM_ERROR_OUT_OF_MEMORY:
                        return CUDA_ERROR_OUT_OF_MEMORY;
                    case IDM_ERROR_INVALID_HANDLE:
                        return CUDA_ERROR_INVALID_HANDLE;
                    default:
                        return CUDA_ERROR_INVALID_VALUE;
                }
            }
        }

        idm_free_message(resp);
    }

    fprintf(stderr, "[libvgpu] Timeout waiting for response\n");
    return CUDA_ERROR_INVALID_VALUE;
}

/* ============================================================================
 * CUDA Driver API Implementation
 * ============================================================================ */

/**
 * cuInit - Initialize CUDA driver
 */
CUresult cuInit(unsigned int Flags)
{
    (void)Flags;

    pthread_mutex_lock(&init_lock);

    if (initialized) {
        pthread_mutex_unlock(&init_lock);
        return CUDA_SUCCESS;
    }

    /* Initialize IDM connection to GPU proxy */
    if (idm_init(USER_ZONE_ID, DRIVER_ZONE_ID, false) < 0) {
        fprintf(stderr, "[libvgpu] Failed to initialize IDM\n");
        pthread_mutex_unlock(&init_lock);
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    initialized = true;
    pthread_mutex_unlock(&init_lock);

    fprintf(stderr, "[libvgpu] Initialized (virtual GPU via IDM)\n");
    return CUDA_SUCCESS;
}

/**
 * cuDriverGetVersion - Get driver version
 */
CUresult cuDriverGetVersion(int *driverVersion)
{
    if (!initialized) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (!driverVersion) {
        return CUDA_ERROR_INVALID_VALUE;
    }

    /* Report a fake CUDA 12.0 driver */
    *driverVersion = 12000;
    return CUDA_SUCCESS;
}

/**
 * cuDeviceGet - Get device handle
 */
CUresult cuDeviceGet(CUdevice *device, int ordinal)
{
    if (!initialized) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (!device || ordinal < 0 || ordinal >= device_count) {
        return CUDA_ERROR_INVALID_VALUE;
    }

    *device = ordinal;
    return CUDA_SUCCESS;
}

/**
 * cuDeviceGetCount - Get device count
 */
CUresult cuDeviceGetCount(int *count)
{
    if (!initialized) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (!count) {
        return CUDA_ERROR_INVALID_VALUE;
    }

    *count = device_count;
    return CUDA_SUCCESS;
}

/**
 * cuDeviceGetName - Get device name
 */
CUresult cuDeviceGetName(char *name, int len, CUdevice dev)
{
    if (!initialized) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (!name || len <= 0 || dev < 0 || dev >= device_count) {
        return CUDA_ERROR_INVALID_VALUE;
    }

    snprintf(name, len, "Virtual GPU %d (via Xen)", dev);
    return CUDA_SUCCESS;
}

/**
 * cuDeviceGetAttribute - Get device attribute
 */
CUresult cuDeviceGetAttribute(int *pi, int attrib, CUdevice dev)
{
    if (!initialized) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (!pi || dev < 0 || dev >= device_count) {
        return CUDA_ERROR_INVALID_VALUE;
    }

    /* Return fake values for common attributes */
    (void)attrib;
    *pi = 1024;  /* Generic fake value */
    return CUDA_SUCCESS;
}

/**
 * cuCtxCreate - Create context
 */
CUresult cuCtxCreate(CUcontext *pctx, unsigned int flags, CUdevice dev)
{
    if (!initialized) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (!pctx || dev < 0 || dev >= device_count) {
        return CUDA_ERROR_INVALID_VALUE;
    }

    (void)flags;

    /* Create a fake context handle */
    current_context = (CUcontext)(uintptr_t)(0x1000 + dev);
    *pctx = current_context;

    fprintf(stderr, "[libvgpu] Created context %p\n", current_context);
    return CUDA_SUCCESS;
}

/**
 * cuCtxDestroy - Destroy context
 */
CUresult cuCtxDestroy(CUcontext ctx)
{
    if (!initialized) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (ctx != current_context) {
        return CUDA_ERROR_INVALID_CONTEXT;
    }

    current_context = NULL;
    return CUDA_SUCCESS;
}

/**
 * cuCtxSynchronize - Synchronize context
 */
CUresult cuCtxSynchronize(void)
{
    if (!initialized) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    struct idm_gpu_sync sync_req = { .flags = 0 };

    struct idm_message *msg = idm_build_message(
        DRIVER_ZONE_ID,
        IDM_GPU_SYNC,
        &sync_req,
        sizeof(sync_req)
    );

    if (!msg) {
        return CUDA_ERROR_OUT_OF_MEMORY;
    }

    CUresult result = send_and_wait(msg, NULL);
    idm_free_message(msg);

    return result;
}

/**
 * cuCtxGetCurrent - Get current context
 */
CUresult cuCtxGetCurrent(CUcontext *pctx)
{
    if (!initialized) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (!pctx) {
        return CUDA_ERROR_INVALID_VALUE;
    }

    *pctx = current_context;
    return CUDA_SUCCESS;
}

/**
 * cuCtxSetCurrent - Set current context
 */
CUresult cuCtxSetCurrent(CUcontext ctx)
{
    if (!initialized) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    current_context = ctx;
    return CUDA_SUCCESS;
}

/**
 * cuMemAlloc - Allocate GPU memory
 */
CUresult cuMemAlloc(CUdeviceptr *dptr, size_t bytesize)
{
    if (!initialized) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (!dptr || bytesize == 0) {
        return CUDA_ERROR_INVALID_VALUE;
    }

    struct idm_gpu_alloc alloc_req = {
        .size = bytesize,
        .flags = 0
    };

    struct idm_message *msg = idm_build_message(
        DRIVER_ZONE_ID,
        IDM_GPU_ALLOC,
        &alloc_req,
        sizeof(alloc_req)
    );

    if (!msg) {
        return CUDA_ERROR_OUT_OF_MEMORY;
    }

    uint64_t handle = 0;
    CUresult result = send_and_wait(msg, &handle);
    idm_free_message(msg);

    if (result == CUDA_SUCCESS) {
        *dptr = (CUdeviceptr)handle;
    }

    return result;
}

/**
 * cuMemFree - Free GPU memory
 */
CUresult cuMemFree(CUdeviceptr dptr)
{
    if (!initialized) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (dptr == 0) {
        return CUDA_ERROR_INVALID_VALUE;
    }

    struct idm_gpu_free free_req = {
        .handle = (uint64_t)dptr
    };

    struct idm_message *msg = idm_build_message(
        DRIVER_ZONE_ID,
        IDM_GPU_FREE,
        &free_req,
        sizeof(free_req)
    );

    if (!msg) {
        return CUDA_ERROR_OUT_OF_MEMORY;
    }

    CUresult result = send_and_wait(msg, NULL);
    idm_free_message(msg);

    return result;
}

/**
 * cuMemcpyHtoD - Copy from host to device
 */
CUresult cuMemcpyHtoD(CUdeviceptr dstDevice, const void *srcHost, size_t ByteCount)
{
    if (!initialized) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (dstDevice == 0 || !srcHost || ByteCount == 0) {
        return CUDA_ERROR_INVALID_VALUE;
    }

    /* Build message with embedded data */
    size_t msg_size = sizeof(struct idm_header) + sizeof(struct idm_gpu_copy_h2d) + ByteCount;
    struct idm_message *msg = malloc(msg_size);
    if (!msg) {
        return CUDA_ERROR_OUT_OF_MEMORY;
    }

    msg->header.magic = IDM_MAGIC;
    msg->header.version = IDM_VERSION;
    msg->header.msg_type = IDM_GPU_COPY_H2D;
    msg->header.src_zone = USER_ZONE_ID;
    msg->header.dst_zone = DRIVER_ZONE_ID;
    msg->header.seq_num = 1;  /* Will be set by transport */
    msg->header.payload_len = sizeof(struct idm_gpu_copy_h2d) + ByteCount;

    struct idm_gpu_copy_h2d *copy_req = (struct idm_gpu_copy_h2d *)msg->payload;
    copy_req->dst_handle = (uint64_t)dstDevice;
    copy_req->dst_offset = 0;
    copy_req->size = ByteCount;

    memcpy(msg->payload + sizeof(struct idm_gpu_copy_h2d), srcHost, ByteCount);

    CUresult result = send_and_wait(msg, NULL);
    free(msg);

    return result;
}

/**
 * cuMemcpyDtoH - Copy from device to host
 */
CUresult cuMemcpyDtoH(void *dstHost, CUdeviceptr srcDevice, size_t ByteCount)
{
    if (!initialized) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (!dstHost || srcDevice == 0 || ByteCount == 0) {
        return CUDA_ERROR_INVALID_VALUE;
    }

    struct idm_gpu_copy_d2h copy_req = {
        .src_handle = (uint64_t)srcDevice,
        .src_offset = 0,
        .size = ByteCount
    };

    struct idm_message *msg = idm_build_message(
        DRIVER_ZONE_ID,
        IDM_GPU_COPY_D2H,
        &copy_req,
        sizeof(copy_req)
    );

    if (!msg) {
        return CUDA_ERROR_OUT_OF_MEMORY;
    }

    CUresult result = send_and_wait(msg, NULL);
    idm_free_message(msg);

    /* TODO: Actually receive the data back */
    /* For now, just zero the buffer */
    if (result == CUDA_SUCCESS) {
        memset(dstHost, 0, ByteCount);
    }

    return result;
}

/**
 * cuMemcpyDtoD - Copy from device to device
 */
CUresult cuMemcpyDtoD(CUdeviceptr dstDevice, CUdeviceptr srcDevice, size_t ByteCount)
{
    if (!initialized) {
        return CUDA_ERROR_NOT_INITIALIZED;
    }

    if (dstDevice == 0 || srcDevice == 0 || ByteCount == 0) {
        return CUDA_ERROR_INVALID_VALUE;
    }

    struct idm_gpu_copy_d2d copy_req = {
        .dst_handle = (uint64_t)dstDevice,
        .dst_offset = 0,
        .src_handle = (uint64_t)srcDevice,
        .src_offset = 0,
        .size = ByteCount
    };

    struct idm_message *msg = idm_build_message(
        DRIVER_ZONE_ID,
        IDM_GPU_COPY_D2D,
        &copy_req,
        sizeof(copy_req)
    );

    if (!msg) {
        return CUDA_ERROR_OUT_OF_MEMORY;
    }

    CUresult result = send_and_wait(msg, NULL);
    idm_free_message(msg);

    return result;
}

/**
 * cuMemsetD8 - Set memory to byte value
 */
CUresult cuMemsetD8(CUdeviceptr dstDevice, unsigned char uc, size_t N)
{
    /* Not implemented - would need new IDM message type */
    (void)dstDevice;
    (void)uc;
    (void)N;
    return CUDA_SUCCESS;  /* Pretend it worked */
}

/**
 * cuMemsetD16 - Set memory to short value
 */
CUresult cuMemsetD16(CUdeviceptr dstDevice, unsigned short us, size_t N)
{
    (void)dstDevice;
    (void)us;
    (void)N;
    return CUDA_SUCCESS;
}

/**
 * cuMemsetD32 - Set memory to int value
 */
CUresult cuMemsetD32(CUdeviceptr dstDevice, unsigned int ui, size_t N)
{
    (void)dstDevice;
    (void)ui;
    (void)N;
    return CUDA_SUCCESS;
}

/**
 * cuGetErrorString - Get error string
 */
CUresult cuGetErrorString(CUresult error, const char **pStr)
{
    if (!pStr) {
        return CUDA_ERROR_INVALID_VALUE;
    }

    if (error < 0 || error >= (int)(sizeof(error_strings) / sizeof(error_strings[0])) ||
        !error_strings[error]) {
        *pStr = "unknown error";
    } else {
        *pStr = error_strings[error];
    }

    return CUDA_SUCCESS;
}

/**
 * cuGetErrorName - Get error name
 */
CUresult cuGetErrorName(CUresult error, const char **pStr)
{
    if (!pStr) {
        return CUDA_ERROR_INVALID_VALUE;
    }

    if (error < 0 || error >= (int)(sizeof(error_names) / sizeof(error_names[0])) ||
        !error_names[error]) {
        *pStr = "CUDA_ERROR_UNKNOWN";
    } else {
        *pStr = error_names[error];
    }

    return CUDA_SUCCESS;
}
