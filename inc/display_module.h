/*
 * Display Module Interface.
 * Defines the shared synchronization primitives and task entry points
 * for the OLED and Sensor management system.
 */

#ifndef DISPLAY_MODULE_H
#define DISPLAY_MODULE_H

#include <zephyr/kernel.h>

/* * Shared Semaphores
 * These are defined in main.c and used here to coordinate 
 * timing and system-ready states.
 */

/** @brief Semaphore triggered by a timer to request a UI redraw. */
extern struct k_sem sem_ui_refresh;

/** @brief Semaphore used to signal that all hardware is ready for use. */
extern struct k_sem sem_system_ready;

/** @brief Optional: Semaphore used for precise sensor measurement timing. */
extern struct k_sem sem_sensor_wait;

/**
 * @brief Main entry point for the display thread.
 * * This function handles the I2C communication with the AHT10 
 * and the frame buffer rendering for the SSD1306.
 */
void display_task_entry(void);

#endif /* DISPLAY_MODULE_H */