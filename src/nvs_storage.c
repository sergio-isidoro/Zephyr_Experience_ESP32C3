/*
 * Non-Volatile Storage (NVS) Manager.
 * Handles persistent data across reboots and power cycles.
 */

#include "nvs_storage.h"
#include <zephyr/drivers/flash.h>
#include <zephyr/fs/nvs.h>
#include <zephyr/device.h>

/* File system structure for NVS */
static struct nvs_fs fs;

/* Link to the 'my_storage' partition defined in the app.overlay */
#define NVS_PARTITION_NODE DT_NODELABEL(my_storage)

/**
 * @brief Initializes the NVS file system in Flash memory.
 * * 1. Locates the flash device and memory offset.
 * * 2. Retrieves sector information (page size).
 * * 3. Mounts the file system or formats it if corrupted.
 * * @return 0 on success, negative error code otherwise.
 */
int init_nvs_storage(void) {
    struct flash_pages_info info;

    /* Bind the NVS structure to the physical flash partition */
    fs.flash_device = DEVICE_DT_GET(DT_MTD_FROM_FIXED_PARTITION(NVS_PARTITION_NODE));
    fs.offset = DT_REG_ADDR(NVS_PARTITION_NODE);

    /* Ensure the flash driver is ready */
    if (!device_is_ready(fs.flash_device)) {
        return -1;
    }

    /* Retrieve page info to set the sector size correctly */
    flash_get_page_info_by_offs(fs.flash_device, fs.offset, &info);
    fs.sector_size = info.size;
    fs.sector_count = 3; /* Using 3 sectors for wear leveling */

    /* Attempt to mount the NVS filesystem */
    int rc = nvs_mount(&fs);
    if (rc) {
        /* * If mounting fails (e.g., empty or corrupted flash), 
         * erase the area and try again.
         */
        flash_erase(fs.flash_device, fs.offset, fs.sector_count * fs.sector_size);
        return nvs_mount(&fs);
    }
    return 0;
}

/**
 * @brief Reads, increments, and stores the boot counter.
 * Logic: Cycles from 1 to 5. If it reaches 5, it resets to 1.
 * * @return The updated boot count.
 */
uint32_t increment_boot_count(void) {
    uint32_t count = 0;

    /* Read the existing value from ID 1. If not found, count remains 0. */
    nvs_read(&fs, BOOT_COUNT_ID, &count, sizeof(count));

    /* Increment logic with a hard limit of 5 */
    if (count >= 5) {
        count = 1;
    } else {
        count++;
    }

    /* Write the new value back to Flash */
    nvs_write(&fs, BOOT_COUNT_ID, &count, sizeof(count));

    return count;
}