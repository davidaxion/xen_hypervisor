/*
 * IDM (Inter-Domain Messaging) Protocol
 *
 * Communication protocol between user domains and driver domain.
 * Built on Xen grant tables (shared memory) and event channels (notifications).
 *
 * Design goals:
 * - Simple message format (header + payload)
 * - Type-safe payloads
 * - Sequence numbers for request/response matching
 * - Zero-copy where possible
 */

#ifndef IDM_H
#define IDM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Protocol magic number ("IDM\0") */
#define IDM_MAGIC 0x49444D00

/* Protocol version */
#define IDM_VERSION_MAJOR 1
#define IDM_VERSION_MINOR 0
#define IDM_VERSION ((IDM_VERSION_MAJOR << 8) | IDM_VERSION_MINOR)

/* Maximum payload size (4MB - enough for small transfers) */
#define IDM_MAX_PAYLOAD_SIZE (4 * 1024 * 1024)

/* Ring buffer size (power of 2) - reduced for macOS limits */
#define IDM_RING_SIZE 32

/* ============================================================================
 * Message Types
 * ============================================================================ */

enum idm_msg_type {
    /* GPU Memory Management */
    IDM_GPU_ALLOC           = 0x01,    /* cudaMalloc() */
    IDM_GPU_FREE            = 0x02,    /* cudaFree() */

    /* GPU Data Transfer */
    IDM_GPU_COPY_H2D        = 0x10,    /* Host to Device */
    IDM_GPU_COPY_D2H        = 0x11,    /* Device to Host */
    IDM_GPU_COPY_D2D        = 0x12,    /* Device to Device */
    IDM_GPU_MEMSET          = 0x13,    /* Fill memory */

    /* GPU Execution */
    IDM_GPU_LAUNCH_KERNEL   = 0x20,    /* Launch kernel */
    IDM_GPU_SYNC            = 0x21,    /* Synchronize */

    /* GPU Information */
    IDM_GPU_GET_INFO        = 0x30,    /* Get GPU info */
    IDM_GPU_GET_PROPS       = 0x31,    /* Get device properties */

    /* Responses */
    IDM_RESPONSE_OK         = 0xF0,    /* Success */
    IDM_RESPONSE_ERROR      = 0xF1,    /* Error */
};

/* ============================================================================
 * Message Header
 * ============================================================================ */

struct idm_header {
    uint32_t magic;        /* Always IDM_MAGIC */
    uint16_t version;      /* Protocol version */
    uint16_t msg_type;     /* One of idm_msg_type */
    uint32_t src_zone;     /* Source zone ID */
    uint32_t dst_zone;     /* Destination zone ID */
    uint64_t seq_num;      /* Sequence number (for matching req/resp) */
    uint32_t payload_len;  /* Size of payload in bytes */
    uint32_t reserved;     /* Reserved for future use */
} __attribute__((packed));

/* ============================================================================
 * Message Payloads
 * ============================================================================ */

/* GPU_ALLOC: Allocate GPU memory */
struct idm_gpu_alloc {
    uint64_t size;         /* Size in bytes */
    uint32_t flags;        /* Allocation flags */
    uint32_t reserved;
} __attribute__((packed));

/* GPU_FREE: Free GPU memory */
struct idm_gpu_free {
    uint64_t handle;       /* Handle from GPU_ALLOC */
} __attribute__((packed));

/* GPU_COPY_H2D: Copy host to device */
struct idm_gpu_copy_h2d {
    uint64_t dst_handle;   /* Destination GPU handle */
    uint64_t dst_offset;   /* Offset in destination */
    uint64_t size;         /* Size to copy */
    /* Data follows immediately after this struct */
} __attribute__((packed));

/* GPU_COPY_D2H: Copy device to host */
struct idm_gpu_copy_d2h {
    uint64_t src_handle;   /* Source GPU handle */
    uint64_t src_offset;   /* Offset in source */
    uint64_t size;         /* Size to copy */
} __attribute__((packed));

/* GPU_COPY_D2D: Copy device to device */
struct idm_gpu_copy_d2d {
    uint64_t dst_handle;   /* Destination GPU handle */
    uint64_t src_handle;   /* Source GPU handle */
    uint64_t dst_offset;   /* Offset in destination */
    uint64_t src_offset;   /* Offset in source */
    uint64_t size;         /* Size to copy */
} __attribute__((packed));

/* GPU_MEMSET: Fill GPU memory */
struct idm_gpu_memset {
    uint64_t handle;       /* GPU handle */
    uint64_t offset;       /* Offset */
    uint32_t value;        /* Value to fill (repeated byte) */
    uint64_t size;         /* Size */
} __attribute__((packed));

/* GPU_LAUNCH_KERNEL: Launch GPU kernel */
struct idm_gpu_launch_kernel {
    uint64_t function_handle;  /* Kernel function handle */
    uint32_t grid_dim_x;       /* Grid dimensions */
    uint32_t grid_dim_y;
    uint32_t grid_dim_z;
    uint32_t block_dim_x;      /* Block dimensions */
    uint32_t block_dim_y;
    uint32_t block_dim_z;
    uint32_t shared_mem;       /* Shared memory size */
    uint32_t num_args;         /* Number of kernel arguments */
    /* Kernel arguments follow */
} __attribute__((packed));

/* GPU_SYNC: Synchronize */
struct idm_gpu_sync {
    uint32_t flags;        /* Sync flags */
    uint32_t reserved;
} __attribute__((packed));

/* GPU_GET_INFO: Get GPU information */
struct idm_gpu_get_info {
    uint32_t info_type;    /* What info to get */
    uint32_t reserved;
} __attribute__((packed));

/* RESPONSE_OK: Success response */
struct idm_response_ok {
    uint64_t request_seq;  /* Sequence number of request */
    uint64_t result_handle;/* Result handle (if applicable) */
    uint32_t result_value; /* Result value (if applicable) */
    uint32_t data_len;     /* Length of additional data */
    /* Additional data follows (if data_len > 0) */
} __attribute__((packed));

/* RESPONSE_ERROR: Error response */
struct idm_response_error {
    uint64_t request_seq;  /* Sequence number of request */
    uint32_t error_code;   /* IDM error code */
    uint32_t cuda_error;   /* CUDA error code (if applicable) */
    char error_msg[256];   /* Human-readable error message */
} __attribute__((packed));

/* ============================================================================
 * Complete Message Structure
 * ============================================================================ */

struct idm_message {
    struct idm_header header;
    uint8_t payload[];     /* Variable-length payload */
} __attribute__((packed));

/* ============================================================================
 * Error Codes
 * ============================================================================ */

enum idm_error {
    IDM_ERROR_NONE              = 0,
    IDM_ERROR_INVALID_MESSAGE   = 1,
    IDM_ERROR_INVALID_HANDLE    = 2,
    IDM_ERROR_PERMISSION_DENIED = 3,
    IDM_ERROR_OUT_OF_MEMORY     = 4,
    IDM_ERROR_INVALID_SIZE      = 5,
    IDM_ERROR_TIMEOUT           = 6,
    IDM_ERROR_CONNECTION_LOST   = 7,
    IDM_ERROR_CUDA_ERROR        = 8,
    IDM_ERROR_UNKNOWN           = 99,
};

/* ============================================================================
 * Ring Buffer (for Xen grant table transport)
 * ============================================================================ */

struct idm_ring_entry {
    struct idm_message msg;
    uint8_t padding[4096 - sizeof(struct idm_message)];  /* Align to page */
} __attribute__((packed));

struct idm_ring {
    uint32_t producer;     /* Producer index (written by sender) */
    uint32_t consumer;     /* Consumer index (written by receiver) */
    uint32_t reserved[2];
    struct idm_ring_entry entries[IDM_RING_SIZE];
} __attribute__((packed));

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Calculate message size (header + payload)
 */
static inline size_t idm_message_size(const struct idm_message *msg)
{
    return sizeof(struct idm_header) + msg->header.payload_len;
}

/**
 * Validate message header
 */
static inline bool idm_message_valid(const struct idm_message *msg)
{
    return msg->header.magic == IDM_MAGIC &&
           msg->header.version == IDM_VERSION &&
           msg->header.payload_len <= IDM_MAX_PAYLOAD_SIZE;
}

/**
 * Get payload pointer
 */
static inline void *idm_message_payload(struct idm_message *msg)
{
    return msg->payload;
}

/**
 * Get payload pointer (const version)
 */
static inline const void *idm_message_payload_const(const struct idm_message *msg)
{
    return msg->payload;
}

/**
 * Message type to string (for debugging)
 */
static inline const char *idm_msg_type_str(enum idm_msg_type type)
{
    switch (type) {
        case IDM_GPU_ALLOC:         return "GPU_ALLOC";
        case IDM_GPU_FREE:          return "GPU_FREE";
        case IDM_GPU_COPY_H2D:      return "GPU_COPY_H2D";
        case IDM_GPU_COPY_D2H:      return "GPU_COPY_D2H";
        case IDM_GPU_COPY_D2D:      return "GPU_COPY_D2D";
        case IDM_GPU_MEMSET:        return "GPU_MEMSET";
        case IDM_GPU_LAUNCH_KERNEL: return "GPU_LAUNCH_KERNEL";
        case IDM_GPU_SYNC:          return "GPU_SYNC";
        case IDM_GPU_GET_INFO:      return "GPU_GET_INFO";
        case IDM_GPU_GET_PROPS:     return "GPU_GET_PROPS";
        case IDM_RESPONSE_OK:       return "RESPONSE_OK";
        case IDM_RESPONSE_ERROR:    return "RESPONSE_ERROR";
        default:                     return "UNKNOWN";
    }
}

#endif /* IDM_H */
