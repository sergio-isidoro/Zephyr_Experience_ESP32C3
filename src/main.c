/*
 * Main Orchestrator for ESP32-C3 SuperMini Weather Station.
 * This module coordinates initialization, system timing, and persistence.
 */

#include "i2c_bus.h"
#include "nvs_storage.h"
#include "pwm_timers.h"
#include "display_module.h"
#include <zephyr/drivers/watchdog.h>
#include <esp_system.h>

/* Global variables for system-wide status */
uint32_t flash_boot_val;           /* Stores the current boot cycle (1-5) */
const char* reset_reason_str;      /* Human-readable reset reason */

/* * Synchronization Semaphores 
 * Binary semaphores used to trigger thread execution and main loop ticks.
 */
K_SEM_DEFINE(sem_ui_refresh, 0, 1);    /* Signals the Display Task to update */
K_SEM_DEFINE(sem_system_ready, 0, 1);  /* Blocks tasks until hardware is initialized */
K_SEM_DEFINE(sem_main_tick, 0, 1);     /* Controls the main loop execution frequency */

/* * Timer Callback Functions
 * These run in ISR context to give semaphores for precise timing.
 */
void ui_cb(struct k_timer *t) { k_sem_give(&sem_ui_refresh); }
void main_cb(struct k_timer *t) { k_sem_give(&sem_main_tick); }

/* * Kernel Timer Definitions 
 * ui_timer: 300ms for OLED refresh.
 * main_timer: 50ms for general logic processing.
 * heartbeat_timer: 250ms for LED toggling (callback defined in pwm_timers.c).
 */
K_TIMER_DEFINE(ui_timer, ui_cb, NULL);
K_TIMER_DEFINE(main_timer, main_cb, NULL);
K_TIMER_DEFINE(heartbeat_timer, heartbeat_timer_cb, NULL);

/**
 * @brief Retrieves the hardware reset reason using ESP-IDF HAL.
 * @return A short 3 or 4 character string representing the reason.
 */
const char* get_reset_reason_text(void) {
    esp_reset_reason_t reason = esp_reset_reason();
    switch (reason) {
        case ESP_RST_POWERON:  return "PWR";   /* Power-on reset (cold boot) */
        case ESP_RST_EXT:      return "BTN";   /* External pin reset (Physical button) */
        case ESP_RST_SW:       return "SW";    /* Software-triggered restart */
        case ESP_RST_PANIC:    return "ERR";   /* System crash/panic */
        case ESP_RST_TASK_WDT: return "WDT";   /* Task Watchdog timeout */
        case ESP_RST_BROWNOUT: return "VOLT";  /* Voltage drop detected */
        default:               return "UNK";   /* Unknown reset source */
    }
}

/**
 * @brief Main Entry Point
 */
int main(void) {
    /* * CRITICAL: Hardware stabilization delay.
     * Necessary for the OLED's RC Reset circuit to discharge/charge properly.
     */
    k_msleep(1000); 
    
    /* Initialize Shared I2C Bus and Mutex */
    init_i2c_bus();
    
    /* * Initialize Non-Volatile Storage (NVS).
     * If successful, increment the boot counter stored in Flash.
     */
    if (init_nvs_storage() == 0) {
        flash_boot_val = increment_boot_count();
    }
    
    /* Initialize PWM Channels and GPIO Interrupts */
    init_pwm_gpio();

    /* Identify why the chip started/restarted */
    reset_reason_str = get_reset_reason_text();

    /* Start all system timers */
    k_timer_start(&ui_timer, K_MSEC(300), K_MSEC(300));   /* UI Update rate */
    k_timer_start(&main_timer, K_MSEC(50), K_MSEC(50));    /* Main logic rate */
    k_timer_start(&heartbeat_timer, K_MSEC(250), K_MSEC(250)); /* LED flash rate */
    
    /* Release tasks waiting for system initialization */
    k_sem_give(&sem_system_ready);

    /* * Main Control Loop 
     * Synchronized by sem_main_tick to run at exactly 20Hz (every 50ms).
     */
    while (1) {
        /* Wait for the next 50ms tick */
        k_sem_take(&sem_main_tick, K_FOREVER);
        
        /* Process LED status and PWM heartbeat logic */
        handle_heartbeat();
        
        /* * Note: Watchdog feeding should be added here 
         * if the watchdog module is enabled.
         */
    }

    return 0; /* Should never reach here */
}