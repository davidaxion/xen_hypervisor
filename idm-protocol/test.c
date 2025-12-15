/*
 * IDM Protocol Test
 *
 * Tests the IDM protocol in stub mode (no Xen required).
 * Simulates driver domain and user domain communication.
 *
 * Usage:
 *   Terminal 1: ./idm_test server
 *   Terminal 2: ./idm_test client
 */

#include "idm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

/* Declare transport functions */
int idm_init(uint32_t local_zone_id, uint32_t remote_zone_id, bool is_server);
int idm_send(struct idm_message *msg);
int idm_recv(struct idm_message **msg_out, int timeout_ms);
struct idm_message *idm_build_message(uint32_t dst_zone, enum idm_msg_type msg_type,
                                       const void *payload, size_t payload_len);
void idm_free_message(struct idm_message *msg);
void idm_cleanup(void);

/* Zone IDs */
#define DRIVER_ZONE_ID 1
#define USER_ZONE_ID 2

/* ============================================================================
 * Server (Driver Domain) Mode
 * ============================================================================ */

void run_server(void)
{
    printf("=== Driver Domain (Server) ===\n");

    /* Initialize IDM */
    if (idm_init(DRIVER_ZONE_ID, USER_ZONE_ID, true) < 0) {
        fprintf(stderr, "Failed to initialize IDM\n");
        return;
    }

    printf("IDM initialized. Waiting for requests...\n\n");

    /* Handle requests */
    int handled = 0;
    while (handled < 10) {  /* Handle 10 requests then exit */
        struct idm_message *req = NULL;

        /* Receive request */
        int ret = idm_recv(&req, -1);  /* Block forever */
        if (ret < 0) {
            fprintf(stderr, "idm_recv failed: %d\n", ret);
            continue;
        }

        printf("[%d] Received %s (seq=%lu)\n",
               handled + 1,
               idm_msg_type_str(req->header.msg_type),
               req->header.seq_num);

        /* Handle based on type */
        if (req->header.msg_type == IDM_GPU_ALLOC) {
            const struct idm_gpu_alloc *alloc = (const struct idm_gpu_alloc *)req->payload;
            printf("    Request: Allocate %lu bytes\n", alloc->size);

            /* Simulate allocation */
            uint64_t fake_handle = 0x42 + handled;

            /* Build response */
            struct idm_response_ok resp = {
                .request_seq = req->header.seq_num,
                .result_handle = fake_handle,
                .result_value = 0,
                .data_len = 0
            };

            struct idm_message *resp_msg = idm_build_message(
                USER_ZONE_ID,
                IDM_RESPONSE_OK,
                &resp,
                sizeof(resp)
            );

            if (resp_msg) {
                if (idm_send(resp_msg) == 0) {
                    printf("    Response: Handle 0x%lx\n\n", fake_handle);
                } else {
                    fprintf(stderr, "    Failed to send response\n");
                }
                idm_free_message(resp_msg);
            }

            handled++;
        }
        else if (req->header.msg_type == IDM_GPU_FREE) {
            const struct idm_gpu_free *free_req = (const struct idm_gpu_free *)req->payload;
            printf("    Request: Free handle 0x%lx\n", free_req->handle);

            /* Build response */
            struct idm_response_ok resp = {
                .request_seq = req->header.seq_num,
                .result_handle = 0,
                .result_value = 0,
                .data_len = 0
            };

            struct idm_message *resp_msg = idm_build_message(
                USER_ZONE_ID,
                IDM_RESPONSE_OK,
                &resp,
                sizeof(resp)
            );

            if (resp_msg) {
                idm_send(resp_msg);
                printf("    Response: OK\n\n");
                idm_free_message(resp_msg);
            }

            handled++;
        }

        idm_free_message(req);
    }

    printf("Handled %d requests. Exiting.\n", handled);

    idm_cleanup();
}

/* ============================================================================
 * Client (User Domain) Mode
 * ============================================================================ */

void run_client(void)
{
    printf("=== User Domain (Client) ===\n");

    /* Give server time to start */
    printf("Waiting for server to start...\n");
    sleep(2);

    /* Initialize IDM */
    if (idm_init(USER_ZONE_ID, DRIVER_ZONE_ID, false) < 0) {
        fprintf(stderr, "Failed to initialize IDM\n");
        return;
    }

    printf("IDM initialized. Sending requests...\n\n");

    /* Test 1: GPU Allocation */
    for (int i = 0; i < 5; i++) {
        printf("[%d] Sending GPU_ALLOC request\n", i + 1);

        struct idm_gpu_alloc alloc = {
            .size = 1024 * (i + 1),
            .flags = 0,
            .reserved = 0
        };

        struct idm_message *req = idm_build_message(
            DRIVER_ZONE_ID,
            IDM_GPU_ALLOC,
            &alloc,
            sizeof(alloc)
        );

        if (!req) {
            fprintf(stderr, "Failed to build message\n");
            continue;
        }

        uint64_t req_seq = req->header.seq_num;

        /* Send request */
        if (idm_send(req) < 0) {
            fprintf(stderr, "Failed to send request\n");
            idm_free_message(req);
            continue;
        }

        idm_free_message(req);

        /* Wait for response */
        printf("    Waiting for response...\n");

        struct idm_message *resp = NULL;
        if (idm_recv(&resp, 5000) < 0) {  /* 5 second timeout */
            fprintf(stderr, "    Timeout waiting for response\n");
            continue;
        }

        /* Validate response */
        if (resp->header.msg_type == IDM_RESPONSE_OK) {
            const struct idm_response_ok *ok = (const struct idm_response_ok *)resp->payload;

            if (ok->request_seq == req_seq) {
                printf("    Response: Handle 0x%lx\n\n", ok->result_handle);
            } else {
                fprintf(stderr, "    Response sequence mismatch\n");
            }
        } else {
            fprintf(stderr, "    Received error response\n");
        }

        idm_free_message(resp);

        /* Small delay between requests */
        usleep(100000);  /* 100ms */
    }

    /* Test 2: GPU Free */
    for (int i = 0; i < 5; i++) {
        printf("[%d] Sending GPU_FREE request\n", i + 6);

        struct idm_gpu_free free_req = {
            .handle = 0x42 + i
        };

        struct idm_message *req = idm_build_message(
            DRIVER_ZONE_ID,
            IDM_GPU_FREE,
            &free_req,
            sizeof(free_req)
        );

        if (!req) {
            fprintf(stderr, "Failed to build message\n");
            continue;
        }

        uint64_t req_seq = req->header.seq_num;

        if (idm_send(req) < 0) {
            fprintf(stderr, "Failed to send request\n");
            idm_free_message(req);
            continue;
        }

        idm_free_message(req);

        printf("    Waiting for response...\n");

        struct idm_message *resp = NULL;
        if (idm_recv(&resp, 5000) < 0) {
            fprintf(stderr, "    Timeout waiting for response\n");
            continue;
        }

        if (resp->header.msg_type == IDM_RESPONSE_OK) {
            printf("    Response: OK\n\n");
        }

        idm_free_message(resp);

        usleep(100000);
    }

    printf("All requests sent. Exiting.\n");

    idm_cleanup();
}

/* ============================================================================
 * Performance Test
 * ============================================================================ */

void run_perf_test(void)
{
    printf("=== Performance Test ===\n");

    sleep(2);

    if (idm_init(USER_ZONE_ID, DRIVER_ZONE_ID, false) < 0) {
        fprintf(stderr, "Failed to initialize IDM\n");
        return;
    }

    printf("Measuring IDM round-trip latency...\n");

    const int iterations = 1000;
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < iterations; i++) {
        /* Send request */
        struct idm_gpu_alloc alloc = { .size = 1024, .flags = 0 };
        struct idm_message *req = idm_build_message(
            DRIVER_ZONE_ID,
            IDM_GPU_ALLOC,
            &alloc,
            sizeof(alloc)
        );

        idm_send(req);
        idm_free_message(req);

        /* Receive response */
        struct idm_message *resp = NULL;
        idm_recv(&resp, 5000);
        idm_free_message(resp);
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;
    double avg_latency = (elapsed / iterations) * 1e6;  /* microseconds */

    printf("\nResults:\n");
    printf("  Iterations: %d\n", iterations);
    printf("  Total time: %.3f seconds\n", elapsed);
    printf("  Average round-trip: %.2f µs\n", avg_latency);
    printf("  Throughput: %.2f ops/sec\n", iterations / elapsed);

    idm_cleanup();
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s {server|client|perf}\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "Run in two terminals:\n");
        fprintf(stderr, "  Terminal 1: %s server\n", argv[0]);
        fprintf(stderr, "  Terminal 2: %s client\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "Or for performance test:\n");
        fprintf(stderr, "  Terminal 1: %s server\n", argv[0]);
        fprintf(stderr, "  Terminal 2: %s perf\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "server") == 0) {
        run_server();
    } else if (strcmp(argv[1], "client") == 0) {
        run_client();
    } else if (strcmp(argv[1], "perf") == 0) {
        run_perf_test();
    } else {
        fprintf(stderr, "Unknown mode: %s\n", argv[1]);
        return 1;
    }

    return 0;
}
