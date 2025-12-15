/*
 * GPU Proxy Test Client
 *
 * Sends real GPU requests via IDM and verifies they work.
 * This tests the complete flow: IDM → GPU Proxy → Real CUDA → Response
 */

#include "../idm-protocol/idm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* Forward declarations */
extern int idm_init(uint32_t local_zone_id, uint32_t remote_zone_id, bool is_server);
extern int idm_send(struct idm_message *msg);
extern int idm_recv(struct idm_message **msg_out, int timeout_ms);
extern struct idm_message *idm_build_message(uint32_t dst_zone, enum idm_msg_type msg_type,
                                              const void *payload, size_t payload_len);
extern void idm_free_message(struct idm_message *msg);
extern void idm_cleanup(void);

#define DRIVER_ZONE_ID 1
#define USER_ZONE_ID 2

/**
 * Wait for response matching request sequence
 */
static int wait_for_response(uint64_t req_seq, uint64_t *handle_out)
{
    struct idm_message *resp = NULL;

    for (int i = 0; i < 10; i++) {  /* Try 10 times */
        if (idm_recv(&resp, 1000) < 0) {  /* 1 second timeout */
            continue;
        }

        if (resp->header.msg_type == IDM_RESPONSE_OK) {
            const struct idm_response_ok *ok = (const struct idm_response_ok *)resp->payload;

            if (ok->request_seq == req_seq) {
                if (handle_out) {
                    *handle_out = ok->result_handle;
                }
                idm_free_message(resp);
                return 0;  /* Success */
            }
        } else if (resp->header.msg_type == IDM_RESPONSE_ERROR) {
            const struct idm_response_error *err = (const struct idm_response_error *)resp->payload;

            if (err->request_seq == req_seq) {
                fprintf(stderr, "Error response: %s (code=%u, cuda=%u)\n",
                        err->error_msg, err->error_code, err->cuda_error);
                idm_free_message(resp);
                return -1;
            }
        }

        idm_free_message(resp);
    }

    fprintf(stderr, "Timeout waiting for response\n");
    return -1;
}

/**
 * Test: Allocate and free GPU memory
 */
static int test_alloc_free(void)
{
    printf("\n=== Test 1: Allocate and Free ===\n");

    /* Allocate 1MB */
    printf("Allocating 1MB...\n");

    struct idm_gpu_alloc alloc_req = {
        .size = 1024 * 1024,
        .flags = 0
    };

    struct idm_message *msg = idm_build_message(
        DRIVER_ZONE_ID,
        IDM_GPU_ALLOC,
        &alloc_req,
        sizeof(alloc_req)
    );

    uint64_t req_seq = msg->header.seq_num;
    if (idm_send(msg) < 0) {
        fprintf(stderr, "Failed to send\n");
        idm_free_message(msg);
        return -1;
    }
    idm_free_message(msg);

    /* Wait for response */
    uint64_t handle = 0;
    if (wait_for_response(req_seq, &handle) < 0) {
        return -1;
    }

    printf("✓ Allocated: handle=0x%lx\n", handle);

    /* Free */
    printf("Freeing handle 0x%lx...\n", handle);

    struct idm_gpu_free free_req = {
        .handle = handle
    };

    msg = idm_build_message(
        DRIVER_ZONE_ID,
        IDM_GPU_FREE,
        &free_req,
        sizeof(free_req)
    );

    req_seq = msg->header.seq_num;
    if (idm_send(msg) < 0) {
        fprintf(stderr, "Failed to send\n");
        idm_free_message(msg);
        return -1;
    }
    idm_free_message(msg);

    if (wait_for_response(req_seq, NULL) < 0) {
        return -1;
    }

    printf("✓ Freed successfully\n");

    return 0;
}

/**
 * Test: Multiple allocations
 */
static int test_multiple_alloc(void)
{
    printf("\n=== Test 2: Multiple Allocations ===\n");

    #define NUM_ALLOCS 10
    uint64_t handles[NUM_ALLOCS];

    /* Allocate multiple buffers */
    for (int i = 0; i < NUM_ALLOCS; i++) {
        size_t size = (i + 1) * 1024;  /* 1KB, 2KB, 3KB, ... */

        printf("Allocating %zu bytes...\n", size);

        struct idm_gpu_alloc alloc_req = {
            .size = size,
            .flags = 0
        };

        struct idm_message *msg = idm_build_message(
            DRIVER_ZONE_ID,
            IDM_GPU_ALLOC,
            &alloc_req,
            sizeof(alloc_req)
        );

        uint64_t req_seq = msg->header.seq_num;
        idm_send(msg);
        idm_free_message(msg);

        if (wait_for_response(req_seq, &handles[i]) < 0) {
            return -1;
        }

        printf("  Handle: 0x%lx\n", handles[i]);
    }

    printf("✓ Allocated %d buffers\n", NUM_ALLOCS);

    /* Free all */
    printf("Freeing all buffers...\n");

    for (int i = 0; i < NUM_ALLOCS; i++) {
        struct idm_gpu_free free_req = {
            .handle = handles[i]
        };

        struct idm_message *msg = idm_build_message(
            DRIVER_ZONE_ID,
            IDM_GPU_FREE,
            &free_req,
            sizeof(free_req)
        );

        uint64_t req_seq = msg->header.seq_num;
        idm_send(msg);
        idm_free_message(msg);

        if (wait_for_response(req_seq, NULL) < 0) {
            return -1;
        }
    }

    printf("✓ Freed all buffers\n");

    return 0;
}

/**
 * Test: Host to Device copy
 */
static int test_copy_h2d(void)
{
    printf("\n=== Test 3: Host to Device Copy ===\n");

    /* Allocate GPU buffer */
    struct idm_gpu_alloc alloc_req = {
        .size = 4096,
        .flags = 0
    };

    struct idm_message *msg = idm_build_message(
        DRIVER_ZONE_ID,
        IDM_GPU_ALLOC,
        &alloc_req,
        sizeof(alloc_req)
    );

    uint64_t req_seq = msg->header.seq_num;
    idm_send(msg);
    idm_free_message(msg);

    uint64_t handle = 0;
    if (wait_for_response(req_seq, &handle) < 0) {
        return -1;
    }

    printf("Allocated buffer: handle=0x%lx\n", handle);

    /* Prepare host data */
    uint8_t host_data[256];
    for (int i = 0; i < 256; i++) {
        host_data[i] = i;
    }

    /* Copy to GPU */
    printf("Copying 256 bytes to GPU...\n");

    size_t msg_size = sizeof(struct idm_header) + sizeof(struct idm_gpu_copy_h2d) + 256;
    msg = malloc(msg_size);
    if (!msg) {
        return -1;
    }

    msg->header.magic = IDM_MAGIC;
    msg->header.version = IDM_VERSION;
    msg->header.msg_type = IDM_GPU_COPY_H2D;
    msg->header.src_zone = USER_ZONE_ID;
    msg->header.dst_zone = DRIVER_ZONE_ID;
    msg->header.seq_num = 100;  /* Dummy seq */
    msg->header.payload_len = sizeof(struct idm_gpu_copy_h2d) + 256;

    struct idm_gpu_copy_h2d *copy_req = (struct idm_gpu_copy_h2d *)msg->payload;
    copy_req->dst_handle = handle;
    copy_req->dst_offset = 0;
    copy_req->size = 256;

    memcpy(msg->payload + sizeof(struct idm_gpu_copy_h2d), host_data, 256);

    req_seq = msg->header.seq_num;
    idm_send(msg);
    free(msg);

    if (wait_for_response(req_seq, NULL) < 0) {
        return -1;
    }

    printf("✓ Copied to GPU successfully\n");

    /* Free */
    struct idm_gpu_free free_req = { .handle = handle };
    msg = idm_build_message(DRIVER_ZONE_ID, IDM_GPU_FREE, &free_req, sizeof(free_req));
    req_seq = msg->header.seq_num;
    idm_send(msg);
    idm_free_message(msg);
    wait_for_response(req_seq, NULL);

    return 0;
}

/**
 * Test: Synchronization
 */
static int test_sync(void)
{
    printf("\n=== Test 4: Synchronization ===\n");

    struct idm_gpu_sync sync_req = {
        .flags = 0
    };

    struct idm_message *msg = idm_build_message(
        DRIVER_ZONE_ID,
        IDM_GPU_SYNC,
        &sync_req,
        sizeof(sync_req)
    );

    uint64_t req_seq = msg->header.seq_num;
    idm_send(msg);
    idm_free_message(msg);

    if (wait_for_response(req_seq, NULL) < 0) {
        return -1;
    }

    printf("✓ Synchronized successfully\n");

    return 0;
}

/**
 * Performance test
 */
static int test_performance(void)
{
    printf("\n=== Test 5: Performance ===\n");

    const int iterations = 1000;
    struct timespec start, end;

    printf("Running %d allocations...\n", iterations);

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < iterations; i++) {
        /* Allocate */
        struct idm_gpu_alloc alloc_req = { .size = 1024, .flags = 0 };
        struct idm_message *msg = idm_build_message(
            DRIVER_ZONE_ID, IDM_GPU_ALLOC, &alloc_req, sizeof(alloc_req));

        uint64_t req_seq = msg->header.seq_num;
        idm_send(msg);
        idm_free_message(msg);

        uint64_t handle = 0;
        wait_for_response(req_seq, &handle);

        /* Free */
        struct idm_gpu_free free_req = { .handle = handle };
        msg = idm_build_message(DRIVER_ZONE_ID, IDM_GPU_FREE, &free_req, sizeof(free_req));
        req_seq = msg->header.seq_num;
        idm_send(msg);
        idm_free_message(msg);
        wait_for_response(req_seq, NULL);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    double avg_latency = (elapsed / iterations) * 1e6;  /* microseconds */

    printf("\nResults:\n");
    printf("  Iterations: %d\n", iterations);
    printf("  Total time: %.3f seconds\n", elapsed);
    printf("  Average latency: %.2f µs (alloc+free)\n", avg_latency);
    printf("  Throughput: %.2f ops/sec\n", iterations / elapsed);

    return 0;
}

/**
 * Main
 */
int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    printf("=== GPU Proxy Test Client ===\n");

    /* Give server time to start */
    printf("Waiting for server...\n");
    sleep(2);

    /* Initialize IDM */
    if (idm_init(USER_ZONE_ID, DRIVER_ZONE_ID, false) < 0) {
        fprintf(stderr, "Failed to initialize IDM\n");
        return 1;
    }

    printf("IDM initialized\n");

    /* Run tests */
    int failed = 0;

    if (test_alloc_free() < 0) {
        fprintf(stderr, "✗ Test 1 FAILED\n");
        failed++;
    }

    if (test_multiple_alloc() < 0) {
        fprintf(stderr, "✗ Test 2 FAILED\n");
        failed++;
    }

    if (test_copy_h2d() < 0) {
        fprintf(stderr, "✗ Test 3 FAILED\n");
        failed++;
    }

    if (test_sync() < 0) {
        fprintf(stderr, "✗ Test 4 FAILED\n");
        failed++;
    }

    if (test_performance() < 0) {
        fprintf(stderr, "✗ Test 5 FAILED\n");
        failed++;
    }

    /* Summary */
    printf("\n=== Test Summary ===\n");
    printf("Total tests: 5\n");
    printf("Passed: %d\n", 5 - failed);
    printf("Failed: %d\n", failed);

    if (failed == 0) {
        printf("\n✓ All tests passed!\n");
    } else {
        printf("\n✗ Some tests failed!\n");
    }

    idm_cleanup();

    return failed > 0 ? 1 : 0;
}
