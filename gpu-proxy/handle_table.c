/*
 * Handle Table Implementation
 */

#include "handle_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

/* Handle entry */
struct handle_entry {
    uint64_t handle;       /* Opaque handle */
    uint32_t zone_id;      /* Owner zone */
    void *ptr;             /* Real GPU pointer */
    size_t size;           /* Allocation size */
    struct handle_entry *next;  /* Hash chain */
};

/* Hash table */
#define HASH_TABLE_SIZE 1024
static struct handle_entry *hash_table[HASH_TABLE_SIZE];
static pthread_mutex_t table_lock = PTHREAD_MUTEX_INITIALIZER;

/* Next handle to allocate */
static uint64_t next_handle = 1;

/* Statistics */
static uint64_t total_handles = 0;
static uint64_t total_memory = 0;

/**
 * Hash function
 */
static uint32_t hash_handle(uint64_t handle)
{
    return (uint32_t)(handle % HASH_TABLE_SIZE);
}

/**
 * Initialize
 */
int handle_table_init(void)
{
    memset(hash_table, 0, sizeof(hash_table));
    next_handle = 1;
    total_handles = 0;
    total_memory = 0;
    return 0;
}

/**
 * Insert
 */
uint64_t handle_table_insert(uint32_t zone_id, void *ptr, size_t size)
{
    if (!ptr) {
        return 0;
    }

    struct handle_entry *entry = malloc(sizeof(*entry));
    if (!entry) {
        return 0;
    }

    pthread_mutex_lock(&table_lock);

    /* Allocate handle */
    entry->handle = next_handle++;
    entry->zone_id = zone_id;
    entry->ptr = ptr;
    entry->size = size;

    /* Insert into hash table */
    uint32_t idx = hash_handle(entry->handle);
    entry->next = hash_table[idx];
    hash_table[idx] = entry;

    /* Update stats */
    total_handles++;
    total_memory += size;

    pthread_mutex_unlock(&table_lock);

    return entry->handle;
}

/**
 * Lookup
 */
void *handle_table_lookup(uint32_t zone_id, uint64_t handle, size_t *size_out)
{
    pthread_mutex_lock(&table_lock);

    uint32_t idx = hash_handle(handle);
    struct handle_entry *entry = hash_table[idx];

    while (entry) {
        if (entry->handle == handle) {
            /* Found - check ownership */
            if (entry->zone_id != zone_id) {
                fprintf(stderr, "SECURITY: Zone %u tried to access zone %u's handle 0x%lx!\n",
                        zone_id, entry->zone_id, handle);
                pthread_mutex_unlock(&table_lock);
                return NULL;
            }

            void *ptr = entry->ptr;
            if (size_out) {
                *size_out = entry->size;
            }

            pthread_mutex_unlock(&table_lock);
            return ptr;
        }
        entry = entry->next;
    }

    pthread_mutex_unlock(&table_lock);
    return NULL;  /* Not found */
}

/**
 * Remove
 */
void *handle_table_remove(uint32_t zone_id, uint64_t handle)
{
    pthread_mutex_lock(&table_lock);

    uint32_t idx = hash_handle(handle);
    struct handle_entry **ptr = &hash_table[idx];
    struct handle_entry *entry;

    while ((entry = *ptr) != NULL) {
        if (entry->handle == handle) {
            /* Found - check ownership */
            if (entry->zone_id != zone_id) {
                fprintf(stderr, "SECURITY: Zone %u tried to free zone %u's handle 0x%lx!\n",
                        zone_id, entry->zone_id, handle);
                pthread_mutex_unlock(&table_lock);
                return NULL;
            }

            /* Remove from chain */
            *ptr = entry->next;

            void *gpu_ptr = entry->ptr;
            size_t size = entry->size;

            /* Update stats */
            total_handles--;
            total_memory -= size;

            free(entry);

            pthread_mutex_unlock(&table_lock);
            return gpu_ptr;
        }
        ptr = &entry->next;
    }

    pthread_mutex_unlock(&table_lock);
    return NULL;  /* Not found */
}

/**
 * Get statistics
 */
void handle_table_stats(uint64_t *total_handles_out, uint64_t *total_memory_out)
{
    pthread_mutex_lock(&table_lock);
    if (total_handles_out) {
        *total_handles_out = total_handles;
    }
    if (total_memory_out) {
        *total_memory_out = total_memory;
    }
    pthread_mutex_unlock(&table_lock);
}

/**
 * Cleanup
 */
void handle_table_cleanup(void)
{
    pthread_mutex_lock(&table_lock);

    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        struct handle_entry *entry = hash_table[i];
        while (entry) {
            struct handle_entry *next = entry->next;
            free(entry);
            entry = next;
        }
        hash_table[i] = NULL;
    }

    next_handle = 1;
    total_handles = 0;
    total_memory = 0;

    pthread_mutex_unlock(&table_lock);
}
