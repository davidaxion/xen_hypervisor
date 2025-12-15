/*
 * IDM Transport Layer
 *
 * Implements message transport over Xen grant tables and event channels.
 * Also includes stub mode using POSIX shared memory for local testing.
 *
 * Compile with -DUSE_XEN for Xen mode, without for stub mode.
 */

#include "idm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>

#ifdef USE_XEN
#include <xenctrl.h>
#include <xenstore.h>
#include <xenevtchn.h>
#include <xen/xen.h>
#include <xen/grant_table.h>
#else
/* Stub mode: POSIX shared memory */
#include <sys/ipc.h>
#include <sys/shm.h>
#include <semaphore.h>
#include <fcntl.h>
#endif

/* ============================================================================
 * Connection State
 * ============================================================================ */

struct idm_connection {
    uint32_t local_zone_id;
    uint32_t remote_zone_id;
    bool is_server;  /* true if driver domain, false if user domain */

#ifdef USE_XEN
    /* Xen-specific fields */
    xenevtchn_handle *evtchn_handle;
    evtchn_port_t local_port;
    evtchn_port_t remote_port;

    /* Grant table references */
    uint32_t tx_gref;  /* TX ring grant reference */
    uint32_t rx_gref;  /* RX ring grant reference */

    /* Mapped ring buffers */
    struct idm_ring *tx_ring;
    struct idm_ring *rx_ring;
#else
    /* Stub mode: POSIX shared memory */
    int tx_shmid;
    int rx_shmid;
    struct idm_ring *tx_ring;
    struct idm_ring *rx_ring;

    sem_t *tx_sem;  /* Notify remote */
    sem_t *rx_sem;  /* Wait for messages */
#endif

    /* Sequence number tracking */
    uint64_t next_seq;
    pthread_mutex_t seq_lock;

    /* Connection state */
    bool connected;
    pthread_mutex_t conn_lock;
};

/* Global connection (simplified for POC - one connection per process) */
static struct idm_connection *global_conn = NULL;

/* ============================================================================
 * Memory Barriers
 * ============================================================================ */

#if defined(__x86_64__) || defined(__i386__)
/* x86/x86-64 memory barriers */
#define rmb() __asm__ __volatile__("lfence":::"memory")
#define wmb() __asm__ __volatile__("sfence":::"memory")
#define mb()  __asm__ __volatile__("mfence":::"memory")
#elif defined(__aarch64__) || defined(__arm__)
/* ARM/ARM64 memory barriers */
#define rmb() __asm__ __volatile__("dmb ld":::"memory")
#define wmb() __asm__ __volatile__("dmb st":::"memory")
#define mb()  __asm__ __volatile__("dmb sy":::"memory")
#else
/* Generic compiler barriers (not hardware barriers) */
#define rmb() __asm__ __volatile__("":::"memory")
#define wmb() __asm__ __volatile__("":::"memory")
#define mb()  __asm__ __volatile__("":::"memory")
#endif

/* ============================================================================
 * Xen-Specific Functions
 * ============================================================================ */

#ifdef USE_XEN

/**
 * Initialize Xen event channel
 */
static int init_event_channel(struct idm_connection *conn)
{
    conn->evtchn_handle = xenevtchn_open(NULL, 0);
    if (!conn->evtchn_handle) {
        fprintf(stderr, "Failed to open event channel: %s\n", strerror(errno));
        return -1;
    }

    /* Bind to remote domain's event channel */
    conn->local_port = xenevtchn_bind_interdomain(
        conn->evtchn_handle,
        conn->remote_zone_id,
        conn->remote_port
    );

    if (conn->local_port < 0) {
        fprintf(stderr, "Failed to bind event channel: %s\n", strerror(errno));
        xenevtchn_close(conn->evtchn_handle);
        return -1;
    }

    return 0;
}

/**
 * Map grant table pages
 */
static int map_grant_pages(struct idm_connection *conn)
{
    /* Map TX ring (we write, remote reads) */
    void *tx_addr = xc_gnttab_map_grant_ref(
        conn->local_zone_id,
        conn->remote_zone_id,
        conn->tx_gref,
        PROT_READ | PROT_WRITE
    );

    if (tx_addr == NULL) {
        fprintf(stderr, "Failed to map TX grant page\n");
        return -1;
    }

    conn->tx_ring = (struct idm_ring *)tx_addr;

    /* Map RX ring (remote writes, we read) */
    void *rx_addr = xc_gnttab_map_grant_ref(
        conn->local_zone_id,
        conn->remote_zone_id,
        conn->rx_gref,
        PROT_READ | PROT_WRITE
    );

    if (rx_addr == NULL) {
        fprintf(stderr, "Failed to map RX grant page\n");
        xc_gnttab_munmap(tx_addr, 1);
        return -1;
    }

    conn->rx_ring = (struct idm_ring *)rx_addr;

    /* Initialize ring indices */
    if (conn->is_server) {
        conn->tx_ring->producer = 0;
        conn->tx_ring->consumer = 0;
    }

    return 0;
}

#endif /* USE_XEN */

/* ============================================================================
 * Stub Mode Functions (POSIX Shared Memory)
 * ============================================================================ */

#ifndef USE_XEN

/**
 * Initialize POSIX shared memory (for testing without Xen)
 */
static int init_shm(struct idm_connection *conn)
{
    key_t tx_key = 0x1000 + conn->local_zone_id;
    key_t rx_key = 0x1000 + conn->remote_zone_id;

    /* Create or get TX shared memory */
    size_t ring_size = sizeof(struct idm_ring);
    conn->tx_shmid = shmget(tx_key, ring_size, IPC_CREAT | 0666);
    if (conn->tx_shmid < 0) {
        fprintf(stderr, "Failed to create TX shared memory: %s\n", strerror(errno));
        return -1;
    }

    /* Attach TX shared memory */
    conn->tx_ring = (struct idm_ring *)shmat(conn->tx_shmid, NULL, 0);
    if (conn->tx_ring == (void *)-1) {
        fprintf(stderr, "Failed to attach TX shared memory: %s\n", strerror(errno));
        return -1;
    }

    /* Create or get RX shared memory */
    conn->rx_shmid = shmget(rx_key, ring_size, IPC_CREAT | 0666);
    if (conn->rx_shmid < 0) {
        fprintf(stderr, "Failed to create RX shared memory: %s\n", strerror(errno));
        shmdt(conn->tx_ring);
        return -1;
    }

    /* Attach RX shared memory */
    conn->rx_ring = (struct idm_ring *)shmat(conn->rx_shmid, NULL, 0);
    if (conn->rx_ring == (void *)-1) {
        fprintf(stderr, "Failed to attach RX shared memory: %s\n", strerror(errno));
        shmdt(conn->tx_ring);
        return -1;
    }

    /* Initialize rings if server */
    if (conn->is_server) {
        memset(conn->tx_ring, 0, ring_size);
        memset(conn->rx_ring, 0, ring_size);
    }

    /* Create semaphores for notifications
     * TX semaphore: signals remote that we sent a message
     * RX semaphore: signals us that remote sent a message
     */
    char sem_name[64];

    /* TX semaphore is for signaling the remote zone */
    snprintf(sem_name, sizeof(sem_name), "/idm_sem_%u", conn->remote_zone_id);
    conn->tx_sem = sem_open(sem_name, O_CREAT, 0666, 0);
    if (conn->tx_sem == SEM_FAILED) {
        fprintf(stderr, "Failed to create TX semaphore: %s\n", strerror(errno));
        shmdt(conn->tx_ring);
        shmdt(conn->rx_ring);
        return -1;
    }

    /* RX semaphore is for our zone being signaled */
    snprintf(sem_name, sizeof(sem_name), "/idm_sem_%u", conn->local_zone_id);
    conn->rx_sem = sem_open(sem_name, O_CREAT, 0666, 0);
    if (conn->rx_sem == SEM_FAILED) {
        fprintf(stderr, "Failed to create RX semaphore: %s\n", strerror(errno));
        sem_close(conn->tx_sem);
        shmdt(conn->tx_ring);
        shmdt(conn->rx_ring);
        return -1;
    }

    return 0;
}

#endif /* !USE_XEN */

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * Initialize IDM connection
 */
int idm_init(uint32_t local_zone_id, uint32_t remote_zone_id, bool is_server)
{
    if (global_conn != NULL) {
        fprintf(stderr, "IDM already initialized\n");
        return -EALREADY;
    }

    struct idm_connection *conn = calloc(1, sizeof(*conn));
    if (!conn) {
        return -ENOMEM;
    }

    conn->local_zone_id = local_zone_id;
    conn->remote_zone_id = remote_zone_id;
    conn->is_server = is_server;
    conn->next_seq = 1;

    pthread_mutex_init(&conn->seq_lock, NULL);
    pthread_mutex_init(&conn->conn_lock, NULL);

#ifdef USE_XEN
    fprintf(stderr, "IDM: Initializing Xen transport\n");

    if (init_event_channel(conn) < 0) {
        free(conn);
        return -1;
    }

    if (map_grant_pages(conn) < 0) {
        xenevtchn_close(conn->evtchn_handle);
        free(conn);
        return -1;
    }

    fprintf(stderr, "IDM: Xen transport initialized\n");
#else
    fprintf(stderr, "IDM: Initializing stub mode (POSIX shared memory)\n");

    if (init_shm(conn) < 0) {
        free(conn);
        return -1;
    }

    fprintf(stderr, "IDM: Stub mode initialized\n");
#endif

    conn->connected = true;
    global_conn = conn;

    return 0;
}

/**
 * Send message
 */
int idm_send(struct idm_message *msg)
{
    if (!global_conn || !global_conn->connected) {
        return -ENOTCONN;
    }

    struct idm_connection *conn = global_conn;
    struct idm_ring *ring = conn->tx_ring;

    /* Validate message */
    if (!idm_message_valid(msg)) {
        fprintf(stderr, "IDM: Invalid message\n");
        return -EINVAL;
    }

    size_t msg_size = idm_message_size(msg);
    if (msg_size > sizeof(struct idm_ring_entry)) {
        fprintf(stderr, "IDM: Message too large: %zu (max %zu)\n",
                msg_size, sizeof(struct idm_ring_entry));
        return -EINVAL;
    }

    /* Get producer/consumer indices */
    uint32_t prod = ring->producer;
    uint32_t cons = ring->consumer;

    /* Check if ring is full */
    if (prod - cons >= IDM_RING_SIZE) {
        fprintf(stderr, "IDM: Ring buffer full\n");
        return -ENOSPC;
    }

    /* Write message to ring */
    uint32_t idx = prod % IDM_RING_SIZE;
    memcpy(&ring->entries[idx].msg, msg, msg_size);

    /* Memory barrier before updating producer */
    wmb();

    /* Update producer index */
    ring->producer = prod + 1;

    /* Notify remote domain */
#ifdef USE_XEN
    xenevtchn_notify(conn->evtchn_handle, conn->local_port);
#else
    sem_post(conn->tx_sem);
#endif

    return 0;
}

/**
 * Receive message (blocking)
 */
int idm_recv(struct idm_message **msg_out, int timeout_ms)
{
    if (!global_conn || !global_conn->connected) {
        return -ENOTCONN;
    }

    struct idm_connection *conn = global_conn;
    struct idm_ring *ring = conn->rx_ring;

#ifdef USE_XEN
    /* Wait for event channel notification */
    evtchn_port_or_error_t port = xenevtchn_pending(conn->evtchn_handle);

    if (port < 0) {
        if (timeout_ms == 0) {
            return -EAGAIN;
        }

        /* TODO: Implement timeout using select/poll */
        port = xenevtchn_pending(conn->evtchn_handle);
        if (port < 0) {
            return -EAGAIN;
        }
    }

    /* Unmask event channel */
    xenevtchn_unmask(conn->evtchn_handle, port);
#else
    /* Wait for semaphore */
    if (timeout_ms < 0) {
        /* Block forever */
        if (sem_wait(conn->rx_sem) < 0) {
            return -errno;
        }
    } else if (timeout_ms == 0) {
        /* Non-blocking */
        if (sem_trywait(conn->rx_sem) < 0) {
            return -EAGAIN;
        }
    } else {
        /* Timed wait */
#ifdef __APPLE__
        /* macOS doesn't support sem_timedwait, use polling */
        int waited_ms = 0;
        while (sem_trywait(conn->rx_sem) < 0) {
            if (waited_ms >= timeout_ms) {
                return -EAGAIN;
            }
            usleep(1000);  /* Sleep 1ms */
            waited_ms++;
        }
#else
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += timeout_ms / 1000;
        ts.tv_nsec += (timeout_ms % 1000) * 1000000;

        if (sem_timedwait(conn->rx_sem, &ts) < 0) {
            return -EAGAIN;
        }
#endif
    }
#endif

    /* Read from ring */
    uint32_t cons = ring->consumer;
    uint32_t prod = ring->producer;

    /* Memory barrier before reading */
    rmb();

    /* Check if messages available */
    if (cons == prod) {
        return -EAGAIN;
    }

    /* Get message from ring */
    uint32_t idx = cons % IDM_RING_SIZE;
    struct idm_message *ring_msg = &ring->entries[idx].msg;

    /* Validate message */
    if (!idm_message_valid(ring_msg)) {
        fprintf(stderr, "IDM: Received invalid message\n");
        ring->consumer = cons + 1;
        return -EINVAL;
    }

    /* Allocate copy for caller */
    size_t msg_size = idm_message_size(ring_msg);
    struct idm_message *msg = malloc(msg_size);
    if (!msg) {
        return -ENOMEM;
    }

    memcpy(msg, ring_msg, msg_size);

    /* Memory barrier before updating consumer */
    wmb();

    /* Update consumer index */
    ring->consumer = cons + 1;

    *msg_out = msg;
    return 0;
}

/**
 * Build message helper
 */
struct idm_message *idm_build_message(
    uint32_t dst_zone,
    enum idm_msg_type msg_type,
    const void *payload,
    size_t payload_len)
{
    if (!global_conn) {
        return NULL;
    }

    struct idm_connection *conn = global_conn;

    /* Allocate message */
    size_t total_size = sizeof(struct idm_header) + payload_len;
    struct idm_message *msg = malloc(total_size);
    if (!msg) {
        return NULL;
    }

    /* Get sequence number */
    pthread_mutex_lock(&conn->seq_lock);
    uint64_t seq = conn->next_seq++;
    pthread_mutex_unlock(&conn->seq_lock);

    /* Fill header */
    msg->header.magic = IDM_MAGIC;
    msg->header.version = IDM_VERSION;
    msg->header.msg_type = msg_type;
    msg->header.src_zone = conn->local_zone_id;
    msg->header.dst_zone = dst_zone;
    msg->header.seq_num = seq;
    msg->header.payload_len = payload_len;
    msg->header.reserved = 0;

    /* Copy payload */
    if (payload && payload_len > 0) {
        memcpy(msg->payload, payload, payload_len);
    }

    return msg;
}

/**
 * Free message
 */
void idm_free_message(struct idm_message *msg)
{
    free(msg);
}

/**
 * Cleanup
 */
void idm_cleanup(void)
{
    if (!global_conn) {
        return;
    }

    struct idm_connection *conn = global_conn;

#ifdef USE_XEN
    if (conn->evtchn_handle) {
        xenevtchn_close(conn->evtchn_handle);
    }
    /* TODO: Unmap grant pages */
#else
    if (conn->tx_ring) {
        shmdt(conn->tx_ring);
    }
    if (conn->rx_ring) {
        shmdt(conn->rx_ring);
    }
    if (conn->tx_sem) {
        sem_close(conn->tx_sem);
    }
    if (conn->rx_sem) {
        sem_close(conn->rx_sem);
    }
#endif

    pthread_mutex_destroy(&conn->seq_lock);
    pthread_mutex_destroy(&conn->conn_lock);

    free(conn);
    global_conn = NULL;
}
