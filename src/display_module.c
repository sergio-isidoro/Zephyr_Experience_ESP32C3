/*
 * Display and Sensor Task Module.
 * Handles OLED rendering (SSD1306) and Environment Sensing (AHT10).
 */

#include "display_module.h"
#include "i2c_bus.h"
#include <zephyr/display/cfb.h>
#include <stdio.h>
#include <stdlib.h>

/* Global values updated by the Main module */
extern uint32_t flash_boot_val;
extern const char* reset_reason_str;

/**
 * @brief Thread entry point for display management.
 */
void display_task_entry(void) {
    /* Initialize display device from Devicetree */
    const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    
    /* Wait until the main system signals that hardware is ready */
    k_sem_take(&sem_system_ready, K_FOREVER);

    /* --- Initial Display Setup --- */
    k_mutex_lock(&i2c_mutex, K_FOREVER);
    
    cfb_framebuffer_init(disp);
    cfb_framebuffer_set_font(disp, 0);   /* Set default font */
    cfb_framebuffer_invert(disp);       /* Optional: Invert colors */

    /* Get screen dimensions for centering logic */
    uint16_t screen_w = cfb_get_display_parameter(disp, CFB_DISPLAY_WIDTH);
    uint16_t screen_h = cfb_get_display_parameter(disp, CFB_DISPLAY_HEIGHT);
    uint8_t font_w, font_h;
    cfb_get_font_size(disp, 0, &font_w, &font_h);

    k_mutex_unlock(&i2c_mutex);

    /* --- Main UI Loop --- */
    while (1) {
        /* Wait for the refresh timer signal (approx. every 300ms) */
        k_sem_take(&sem_ui_refresh, K_FOREVER);

        int temp_i = 0, temp_d = 0, hum_i = 0;
        uint8_t cmd[] = {0xAC, 0x33, 0x00}; /* AHT10 Measure Command */
        uint8_t raw[6];
        bool data_ok = false;

        /* --- AHT10 Sensor Reading ---
         * We lock the bus to send the command, then unlock it during the 
         * conversion wait (80ms) to allow other I2C traffic.
         */
        k_mutex_lock(&i2c_mutex, K_FOREVER);
        if (i2c_write_dt(&sensor_bus, cmd, sizeof(cmd)) == 0) {
            k_mutex_unlock(&i2c_mutex); 
            
            k_msleep(80); /* Wait for AHT10 to finish measuring */
            
            k_mutex_lock(&i2c_mutex, K_FOREVER);
            if (i2c_read_dt(&sensor_bus, raw, sizeof(raw)) == 0) {
                data_ok = true;
            }
        }
        k_mutex_unlock(&i2c_mutex);

        /* Process raw bits into meaningful units */
        if (data_ok) {
            /* Humidity Calculation (20-bit value) */
            uint32_t r_h = ((uint32_t)raw[1] << 12) | ((uint32_t)raw[2] << 4) | (raw[3] >> 4);
            hum_i = (r_h * 100) / 1048576;

            /* Temperature Calculation (20-bit value) */
            uint32_t r_t = ((uint32_t)(raw[3] & 0x0F) << 16) | ((uint32_t)raw[4] << 8) | raw[5];
            float f_t = ((float)r_t * 200 / 1048576) - 50;
            
            /* Split float into integer and decimal for display */
            temp_i = (int)f_t; 
            temp_d = abs((int)((f_t - temp_i) * 10));
        }

        /* --- Rendering Phase --- */
        k_mutex_lock(&i2c_mutex, K_FOREVER);

        cfb_framebuffer_clear(disp, false);
        char buffer[32];

        /* Line 1: Boot counter and Reset Reason (Centered) */
        snprintf(buffer, sizeof(buffer), "%u:%s", flash_boot_val, reset_reason_str);
        cfb_print(disp, buffer, (screen_w - strlen(buffer)*font_w)/2, 0);

        /* Line 2: Temperature (Centered) */
        snprintf(buffer, sizeof(buffer), "%d.%dC", temp_i, temp_d);
        cfb_print(disp, buffer, (screen_w - strlen(buffer)*font_w)/2, (screen_h/2)-(font_h/2)+1);

        /* Line 3: Humidity (Centered at the bottom) */
        snprintf(buffer, sizeof(buffer), "%d%%", hum_i);
        cfb_print(disp, buffer, (screen_w - strlen(buffer)*font_w)/2, screen_h - font_h+1);

        /* Push the updated buffer to the physical OLED controller */
        cfb_framebuffer_finalize(disp);

        k_mutex_unlock(&i2c_mutex);
    }
}

/**
 * @brief Thread definition for the display task.
 * Stack: 2048 bytes, Priority: 7.
 */
K_THREAD_DEFINE(display_tid, 2048, display_task_entry, NULL, NULL, NULL, 7, 0, 0);