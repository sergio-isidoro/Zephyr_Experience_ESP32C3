/**
 * Project: ESP32-C3 SuperMini Weather Station & Multi-PWM Control with Thread
 * Hardware: SSD1306 (I2C), AHT10 (I2C), Internal LED (GPIO8), Button (GPIO9-INT)
 * Dual PWM: GPIO2 (20kHz Static 10% - Timer 0), GPIO3 (10kHz Static 20% - Timer 1)
 * Zephyr OS Version: 4.3.99
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/display/cfb.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/drivers/watchdog.h>
#include <zephyr/sys/atomic.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/** --- PWM DUTY --- */
#define PWM_PERCENT(period, percent)    ((uint64_t)(period) * (percent) / 100)
#define PWM_GPIO2_DUTY_PERCENT          10
#define PWM_GPIO3_DUTY_PERCENT          20

/** --- System Constants & Timing Defines --- */
#define DISPLAY_REFRESH_MS              250     /* OLED update interval */
#define MAIN_LOOP_STEP_MS               50      /* Main loop tick */
#define HEARTBEAT_INTERVAL_MS           250     /* Heartbeat toggle interval */
#define WDT_TIMEOUT_MS                  3000    /* Watchdog safety timeout */
#define THREAD_STACK_SIZE               2048    
#define THREAD_PRIORITY                 7       

/** --- AHT10 Raw I2C Protocol Defines --- */
#define AHT10_CMD_MEASURE               0xAC   
#define AHT10_MEASURE_WAIT              80     
#define AHT10_DATA_SIZE                 6      

/** --- Hardware Specifications from Devicetree --- */
static const struct gpio_dt_spec    led_internal    = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec    btn_boot        = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
static const struct pwm_dt_spec     pwm_fade        = PWM_DT_SPEC_GET(DT_NODELABEL(pwm_gpio2));
static const struct pwm_dt_spec     pwm_static      = PWM_DT_SPEC_GET(DT_NODELABEL(pwm_gpio3));
static const struct i2c_dt_spec     sensor_bus      = I2C_DT_SPEC_GET(DT_NODELABEL(aht10));

/** --- Synchronization & Atomic Flags --- */
atomic_t heartbeat_enabled = ATOMIC_INIT(1);
atomic_t heartbeat_trigger = ATOMIC_INIT(0);
K_SEM_DEFINE(sem_ui_refresh, 0, 1);
K_SEM_DEFINE(sem_system_ready, 0, 1);          
K_SEM_DEFINE(sem_sensor_wait, 0, 1);
K_SEM_DEFINE(sem_main_tick, 0, 1);

/** @brief Timer callbacks para sinalização */
void display_timer_cb(struct k_timer *dummy) { k_sem_give(&sem_ui_refresh); }
void heartbeat_timer_cb(struct k_timer *dummy) { atomic_set(&heartbeat_trigger, 1); }
void sensor_timer_cb(struct k_timer *dummy) { k_sem_give(&sem_sensor_wait); }
void main_tick_cb(struct k_timer *dummy) { k_sem_give(&sem_main_tick); }

K_TIMER_DEFINE(ui_timer, display_timer_cb, NULL);
K_TIMER_DEFINE(heartbeat_timer, heartbeat_timer_cb, NULL);
K_TIMER_DEFINE(sensor_timer, sensor_timer_cb, NULL);
K_TIMER_DEFINE(main_timer, main_tick_cb, NULL);

/** @brief Button ISR to toggle the internal status LED */
void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    static uint32_t last_press = 0;
    uint32_t now = k_uptime_get_32();
    if (now - last_press > 250) {
        atomic_set(&heartbeat_enabled, !atomic_get(&heartbeat_enabled));
        last_press = now;
    }
}
static struct gpio_callback btn_cb_data;

/** ==========================================================================
 * DISPLAY TASK: Fetches sensor data and renders to SSD1306 OLED
 * ========================================================================== */
void display_task_entry(void *p1, void *p2, void *p3) {
    const struct device *disp = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    const struct device *die_temp_dev = DEVICE_DT_GET(DT_NODELABEL(coretemp));

    k_sem_take(&sem_system_ready, K_FOREVER);

    if (!device_is_ready(disp)) return;
    cfb_framebuffer_init(disp);
    cfb_framebuffer_set_font(disp, 0);
    cfb_framebuffer_invert(disp);

    uint16_t screen_w = cfb_get_display_parameter(disp, CFB_DISPLAY_WIDTH);
    uint16_t screen_h = cfb_get_display_parameter(disp, CFB_DISPLAY_HEIGHT);
    uint8_t font_w, font_h;
    cfb_get_font_size(disp, 0, &font_w, &font_h);

    while (1) {
        k_sem_take(&sem_ui_refresh, K_FOREVER);
        cfb_framebuffer_clear(disp, false);

        struct sensor_value cpu_val;
        int aht_temp_i = 0, aht_temp_d = 0, aht_hum_i = 0;
        char buffer[32];

        sensor_sample_fetch(die_temp_dev);
        sensor_channel_get(die_temp_dev, SENSOR_CHAN_DIE_TEMP, &cpu_val);

        if (i2c_is_ready_dt(&sensor_bus)) {
            uint8_t measure_cmd[] = {AHT10_CMD_MEASURE, 0x33, 0x00};
            uint8_t raw_data[AHT10_DATA_SIZE];
            if (i2c_write_dt(&sensor_bus, measure_cmd, sizeof(measure_cmd)) == 0) {
                /* Substituição do k_msleep por Timer One-shot + Semaforo */
                k_timer_start(&sensor_timer, K_MSEC(AHT10_MEASURE_WAIT), K_NO_WAIT);
                k_sem_take(&sem_sensor_wait, K_FOREVER);

                if (i2c_read_dt(&sensor_bus, raw_data, sizeof(raw_data)) == 0) {
                    uint32_t raw_h = ((uint32_t)raw_data[1] << 12) | ((uint32_t)raw_data[2] << 4) | (raw_data[3] >> 4);
                    aht_hum_i = (raw_h * 100) / 1048576;
                    uint32_t raw_t = ((uint32_t)(raw_data[3] & 0x0F) << 16) | ((uint32_t)raw_data[4] << 8) | raw_data[5];
                    float f_t = ((float)raw_t * 200 / 1048576) - 50;
                    aht_temp_i = (int)f_t; aht_temp_d = (int)((f_t - aht_temp_i) * 10);
                }
            }
        }

        snprintf(buffer, sizeof(buffer), "Manoper");
        cfb_print(disp, buffer, (screen_w - strlen(buffer)*font_w)/2, 0);
        snprintf(buffer, sizeof(buffer), "%d.%dC", aht_temp_i, abs(aht_temp_d));
        cfb_print(disp, buffer, (screen_w - strlen(buffer)*font_w)/2, (screen_h/2)-(font_h/2)+1);
        snprintf(buffer, sizeof(buffer), "%d%%", aht_hum_i);
        cfb_print(disp, buffer, (screen_w - strlen(buffer)*font_w)/2, screen_h - font_h+1);

        cfb_framebuffer_finalize(disp);
    }
}
K_THREAD_DEFINE(display_tid, THREAD_STACK_SIZE, display_task_entry, NULL, NULL, NULL, THREAD_PRIORITY, 0, 0);

/** ==========================================================================
 * MAIN: System Initialisation and Logic Loop
 * ========================================================================== */
int main(void) {
    const struct device *wdt_dev = DEVICE_DT_GET(DT_NODELABEL(wdt0));

    gpio_pin_configure_dt(&led_internal, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&btn_boot, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure_dt(&btn_boot, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&btn_cb_data, button_isr, BIT(btn_boot.pin));
    gpio_add_callback(btn_boot.port, &btn_cb_data);

    int wdt_channel = -1;
    if (device_is_ready(wdt_dev)) {
        struct wdt_timeout_cfg wdt_config = {.window.max = WDT_TIMEOUT_MS, .flags = WDT_FLAG_RESET_SOC};
        wdt_channel = wdt_install_timeout(wdt_dev, &wdt_config);
        wdt_setup(wdt_dev, WDT_OPT_PAUSE_HALTED_BY_DBG);
    }

    k_sem_give(&sem_system_ready);
    k_timer_start(&ui_timer, K_MSEC(DISPLAY_REFRESH_MS), K_MSEC(DISPLAY_REFRESH_MS));
    k_timer_start(&heartbeat_timer, K_MSEC(HEARTBEAT_INTERVAL_MS), K_MSEC(HEARTBEAT_INTERVAL_MS));
    k_timer_start(&main_timer, K_MSEC(MAIN_LOOP_STEP_MS), K_MSEC(MAIN_LOOP_STEP_MS));
    
    if (device_is_ready(pwm_fade.dev)) {
        pwm_set_dt(&pwm_fade, pwm_fade.period, PWM_PERCENT(pwm_fade.period, PWM_GPIO2_DUTY_PERCENT));
    }
    if (device_is_ready(pwm_static.dev)) {
        pwm_set_dt(&pwm_static, pwm_static.period, PWM_PERCENT(pwm_static.period, PWM_GPIO3_DUTY_PERCENT));
    }
    
    while (1) {
        k_sem_take(&sem_main_tick, K_FOREVER);

        if (atomic_cas(&heartbeat_trigger, 1, 0)) {
            if (atomic_get(&heartbeat_enabled)){
                gpio_pin_toggle_dt(&led_internal);
            } else {
                gpio_pin_set_dt(&led_internal, 0);
            }
        }

        if (wdt_channel >= 0) wdt_feed(wdt_dev, wdt_channel);
    }
    return 0;
}