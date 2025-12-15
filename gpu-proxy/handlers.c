/*
 * GPU Proxy Handlers
 *
 * Handles IDM messages and calls real CUDA driver.
 */

#include "../idm-protocol/idm.h"
#include "handle_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef STUB_CUDA
#include <cuda.h>
#else
/* Stub CUDA types and results */
typedef unsigned long long CUdeviceptr;
typedef int CUresult;
#define CUDA_SUCCESS 0

static inline CUresult cuMemAlloc(CUdeviceptr *ptr, size_t size) {
    *ptr = (CUdeviceptr)malloc(size);
    return *ptr ? CUDA_SUCCESS : 1;
}

static inline CUresult cuMemFree(CUdeviceptr ptr) {
    free((void *)ptr);
    return CUDA_SUCCESS;
}

static inline CUresult cuMemcpyHtoD(CUdeviceptr dst, const void *src, size_t size) {
    memcpy((void *)dst, src, size);
    return CUDA_SUCCESS;
}

static inline CUresult cuMemcpyDtoH(void *dst, CUdeviceptr src, size_t size) {
    memcpy(dst, (void *)src, size);
    return CUDA_SUCCESS;
}

static inline CUresult cuCtxSynchronize(void) {
    return CUDA_SUCCESS;
}

static inline CUresult cuGetErrorString(CUresult error, const char **pStr) {
    *pStr = "stub error";
    return CUDA_SUCCESS;
}
#endif

/* Forward declarations from transport.c */
extern struct idm_message *idm_build_message(uint32_t dst_zone, enum idm_msg_type msg_type,
                                              const void *payload, size_t payload_len);
extern int idm_send(struct idm_message *msg);
extern void idm_free_message(struct idm_message *msg);

/**
 * Send success response
 */
static int send_response_ok(
    uint32_t dst_zone,
    uint64_t request_seq,
    uint64_t result_handle,
    const void *data,
    size_t data_len)
{
    struct idm_response_ok resp;
    memset(&resp, 0, sizeof(resp));

    resp.request_seq = request_seq;
    resp.result_handle = result_handle;
    resp.result_value = 0;
    resp.data_len = data_len;

    /* For now, no additional data support */
    struct idm_message *msg = idm_build_message(
        dst_zone,
        IDM_RESPONSE_OK,
        &resp,
        sizeof(resp)
    );

    if (!msg) {
        return -1;
    }

    int ret = idm_send(msg);
    idm_free_message(msg);

    return ret;
}

/**
 * Send error response
 */
static int send_response_error(
    uint32_t dst_zone,
    uint64_t request_seq,
    enum idm_error error_code,
    uint32_t cuda_error,
    const char *error_msg)
{
    struct idm_response_error resp;
    memset(&resp, 0, sizeof(resp));

    resp.request_seq = request_seq;
    resp.error_code = error_code;
    resp.cuda_error = cuda_error;
    strncpy(resp.error_msg, error_msg, sizeof(resp.error_msg) - 1);

    struct idm_message *msg = idm_build_message(
        dst_zone,
        IDM_RESPONSE_ERROR,
        &resp,
        sizeof(resp)
    );

    if (!msg) {
        return -1;
    }

    int ret = idm_send(msg);
    idm_free_message(msg);

    return ret;
}

/**
 * Handle GPU_ALLOC
 */
void handle_gpu_alloc(const struct idm_message *msg)
{
    const struct idm_gpu_alloc *req = (const struct idm_gpu_alloc *)msg->payload;
    uint32_t zone_id = msg->header.src_zone;
    uint64_t seq = msg->header.seq_num;

    printf("[GPU_ALLOC] Zone %u requests %lu bytes\n", zone_id, req->size);

    /* Call real CUDA */
    CUdeviceptr device_ptr = 0;
    CUresult res = cuMemAlloc(&device_ptr, req->size);

    if (res != CUDA_SUCCESS) {
        const char *err_str;
        cuGetErrorString(res, &err_str);
        fprintf(stderr, "  cuMemAlloc failed: %s\n", err_str);

        send_response_error(
            zone_id,
            seq,
            IDM_ERROR_CUDA_ERROR,
            res,
            "cuMemAlloc failed"
        );
        return;
    }

    printf("  CUDA allocated: 0x%lx\n", (unsigned long)device_ptr);

    /* Create opaque handle */
    uint64_t handle = handle_table_insert(zone_id, (void *)device_ptr, req->size);
    if (handle == 0) {
        fprintf(stderr, "  Failed to create handle\n");
        cuMemFree(device_ptr);

        send_response_error(
            zone_id,
            seq,
            IDM_ERROR_OUT_OF_MEMORY,
            0,
            "Failed to create handle"
        );
        return;
    }

    printf("  Assigned handle: 0x%lx\n", handle);

    /* Send success */
    send_response_ok(zone_id, seq, handle, NULL, 0);
}

/**
 * Handle GPU_FREE
 */
void handle_gpu_free(const struct idm_message *msg)
{
    const struct idm_gpu_free *req = (const struct idm_gpu_free *)msg->payload;
    uint32_t zone_id = msg->header.src_zone;
    uint64_t seq = msg->header.seq_num;

    printf("[GPU_FREE] Zone %u frees handle 0x%lx\n", zone_id, req->handle);

    /* Lookup and remove handle */
    void *device_ptr = handle_table_remove(zone_id, req->handle);
    if (!device_ptr) {
        fprintf(stderr, "  Invalid handle or permission denied\n");

        send_response_error(
            zone_id,
            seq,
            IDM_ERROR_INVALID_HANDLE,
            0,
            "Invalid handle or permission denied"
        );
        return;
    }

    /* Call real CUDA */
    CUresult res = cuMemFree((CUdeviceptr)device_ptr);
    if (res != CUDA_SUCCESS) {
        const char *err_str;
        cuGetErrorString(res, &err_str);
        fprintf(stderr, "  cuMemFree failed: %s\n", err_str);

        send_response_error(
            zone_id,
            seq,
            IDM_ERROR_CUDA_ERROR,
            res,
            "cuMemFree failed"
        );
        return;
    }

    printf("  Freed GPU pointer: 0x%lx\n", (unsigned long)device_ptr);

    /* Send success */
    send_response_ok(zone_id, seq, 0, NULL, 0);
}

/**
 * Handle GPU_COPY_H2D
 */
void handle_gpu_copy_h2d(const struct idm_message *msg)
{
    const struct idm_gpu_copy_h2d *req = (const struct idm_gpu_copy_h2d *)msg->payload;
    uint32_t zone_id = msg->header.src_zone;
    uint64_t seq = msg->header.seq_num;

    printf("[GPU_COPY_H2D] Zone %u copies %lu bytes to handle 0x%lx+%lu\n",
           zone_id, req->size, req->dst_handle, req->dst_offset);

    /* Lookup destination handle */
    size_t alloc_size;
    void *device_ptr = handle_table_lookup(zone_id, req->dst_handle, &alloc_size);
    if (!device_ptr) {
        fprintf(stderr, "  Invalid handle or permission denied\n");

        send_response_error(
            zone_id,
            seq,
            IDM_ERROR_INVALID_HANDLE,
            0,
            "Invalid handle"
        );
        return;
    }

    /* Validate bounds */
    if (req->dst_offset + req->size > alloc_size) {
        fprintf(stderr, "  Out of bounds access\n");

        send_response_error(
            zone_id,
            seq,
            IDM_ERROR_INVALID_SIZE,
            0,
            "Out of bounds"
        );
        return;
    }

    /* Get host data (follows struct in payload) */
    const uint8_t *host_data = (const uint8_t *)msg->payload + sizeof(struct idm_gpu_copy_h2d);

    /* Copy to GPU */
    CUdeviceptr dst = (CUdeviceptr)device_ptr + req->dst_offset;
    CUresult res = cuMemcpyHtoD(dst, host_data, req->size);

    if (res != CUDA_SUCCESS) {
        const char *err_str;
        cuGetErrorString(res, &err_str);
        fprintf(stderr, "  cuMemcpyHtoD failed: %s\n", err_str);

        send_response_error(
            zone_id,
            seq,
            IDM_ERROR_CUDA_ERROR,
            res,
            "cuMemcpyHtoD failed"
        );
        return;
    }

    printf("  Copied %lu bytes to GPU\n", req->size);

    /* Send success */
    send_response_ok(zone_id, seq, 0, NULL, 0);
}

/**
 * Handle GPU_COPY_D2H
 */
void handle_gpu_copy_d2h(const struct idm_message *msg)
{
    const struct idm_gpu_copy_d2h *req = (const struct idm_gpu_copy_d2h *)msg->payload;
    uint32_t zone_id = msg->header.src_zone;
    uint64_t seq = msg->header.seq_num;

    printf("[GPU_COPY_D2H] Zone %u reads %lu bytes from handle 0x%lx+%lu\n",
           zone_id, req->size, req->src_handle, req->src_offset);

    /* Lookup source handle */
    size_t alloc_size;
    void *device_ptr = handle_table_lookup(zone_id, req->src_handle, &alloc_size);
    if (!device_ptr) {
        fprintf(stderr, "  Invalid handle or permission denied\n");

        send_response_error(
            zone_id,
            seq,
            IDM_ERROR_INVALID_HANDLE,
            0,
            "Invalid handle"
        );
        return;
    }

    /* Validate bounds */
    if (req->src_offset + req->size > alloc_size) {
        fprintf(stderr, "  Out of bounds access\n");

        send_response_error(
            zone_id,
            seq,
            IDM_ERROR_INVALID_SIZE,
            0,
            "Out of bounds"
        );
        return;
    }

    /* Allocate host buffer */
    void *host_data = malloc(req->size);
    if (!host_data) {
        send_response_error(
            zone_id,
            seq,
            IDM_ERROR_OUT_OF_MEMORY,
            0,
            "Out of memory"
        );
        return;
    }

    /* Copy from GPU */
    CUdeviceptr src = (CUdeviceptr)device_ptr + req->src_offset;
    CUresult res = cuMemcpyDtoH(host_data, src, req->size);

    if (res != CUDA_SUCCESS) {
        const char *err_str;
        cuGetErrorString(res, &err_str);
        fprintf(stderr, "  cuMemcpyDtoH failed: %s\n", err_str);

        free(host_data);

        send_response_error(
            zone_id,
            seq,
            IDM_ERROR_CUDA_ERROR,
            res,
            "cuMemcpyDtoH failed"
        );
        return;
    }

    printf("  Read %lu bytes from GPU\n", req->size);

    /* Send success with data */
    /* TODO: Actually send data back (need to extend send_response_ok) */
    /* For now, just send success */
    free(host_data);
    send_response_ok(zone_id, seq, 0, NULL, 0);
}

/**
 * Handle GPU_SYNC
 */
void handle_gpu_sync(const struct idm_message *msg)
{
    uint32_t zone_id = msg->header.src_zone;
    uint64_t seq = msg->header.seq_num;

    printf("[GPU_SYNC] Zone %u synchronizes\n", zone_id);

    /* Synchronize */
    CUresult res = cuCtxSynchronize();

    if (res != CUDA_SUCCESS) {
        const char *err_str;
        cuGetErrorString(res, &err_str);
        fprintf(stderr, "  cuCtxSynchronize failed: %s\n", err_str);

        send_response_error(
            zone_id,
            seq,
            IDM_ERROR_CUDA_ERROR,
            res,
            "cuCtxSynchronize failed"
        );
        return;
    }

    printf("  Synchronized\n");

    /* Send success */
    send_response_ok(zone_id, seq, 0, NULL, 0);
}
