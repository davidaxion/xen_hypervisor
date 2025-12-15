/*
 * Handle Table
 *
 * Maps opaque handles to real GPU pointers.
 * Enforces security: zones can only access their own handles.
 *
 * Why needed:
 * - User zones never see real GPU pointers
 * - Prevents cross-zone memory access
 * - Prevents pointer forgery attacks
 */

#ifndef HANDLE_TABLE_H
#define HANDLE_TABLE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * Initialize handle table
 */
int handle_table_init(void);

/**
 * Insert new allocation
 *
 * @param zone_id Owner zone ID
 * @param ptr Real GPU pointer (CUdeviceptr)
 * @param size Size of allocation
 * @return Opaque handle (>0 on success, 0 on error)
 */
uint64_t handle_table_insert(uint32_t zone_id, void *ptr, size_t size);

/**
 * Lookup handle and validate ownership
 *
 * @param zone_id Requesting zone ID
 * @param handle Opaque handle
 * @param size_out [out] Size of allocation (optional)
 * @return Real GPU pointer, or NULL if invalid/not owned
 */
void *handle_table_lookup(uint32_t zone_id, uint64_t handle, size_t *size_out);

/**
 * Remove handle (for cudaFree)
 *
 * @param zone_id Requesting zone ID
 * @param handle Opaque handle
 * @return Real GPU pointer, or NULL if invalid/not owned
 */
void *handle_table_remove(uint32_t zone_id, uint64_t handle);

/**
 * Get statistics
 */
void handle_table_stats(uint64_t *total_handles, uint64_t *total_memory);

/**
 * Cleanup
 */
void handle_table_cleanup(void);

#endif /* HANDLE_TABLE_H */
