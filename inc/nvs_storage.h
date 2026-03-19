/*
 * Non-Volatile Storage (NVS) Manager Interface.
 * Provides a simplified API for persistent data storage in Flash memory.
 */

#ifndef NVS_STORAGE_H
#define NVS_STORAGE_H

#include <zephyr/types.h>

/** * @brief NVS Data ID for the boot counter.
 * * Each unique piece of data in NVS must have a unique ID (1 to 65535).
 */
#define BOOT_COUNT_ID 1

/**
 * @brief Initializes the Non-Volatile Storage system.
 * * * Locates the flash partition.
 * * Mounts the NVS filesystem.
 * * Formats/Erases the storage area if initialization fails.
 * * @return 0 on success, negative error code on failure.
 */
int init_nvs_storage(void);

/**
 * @brief Increments the boot counter stored in Flash.
 * * * Reads the current value from NVS.
 * * Increments the value (resetting to 1 if it exceeds 5).
 * * Writes the updated value back to Flash.
 * * @return The newly updated boot count (1 to 5).
 */
uint32_t increment_boot_count(void);

#endif /* NVS_STORAGE_H */