/*
 * PWM and GPIO Peripheral Manager.
 * Handles the status LED, User Input (Button), and Static PWM signals.
 */

#include "pwm_timers.h"
#include <zephyr/kernel.h>

/* Hardware specifications retrieved from the Devicetree */
static const struct gpio_dt_spec led_internal = GPIO_DT_SPEC_GET(DT_ALIAS(led0), gpios);
static const struct gpio_dt_spec btn_boot     = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios);
//static const struct pwm_dt_spec  pwm_fade      = PWM_DT_SPEC_GET(DT_NODELABEL(pwm_gpio2));
//static const struct pwm_dt_spec  pwm_static    = PWM_DT_SPEC_GET(DT_NODELABEL(pwm_gpio3));

/* Atomic flags for thread-safe state management */
atomic_t heartbeat_enabled = ATOMIC_INIT(1);  /* Tracks if the LED should blink */
atomic_t heartbeat_trigger = ATOMIC_INIT(0);  /* Signal from timer to toggle LED */
static struct gpio_callback btn_cb_data;

/**
 * @brief Button Interrupt Service Routine (ISR).
 * Toggles the heartbeat status when the button is pressed.
 * Includes a 250ms debounce window.
 */
void button_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins) {
    static uint32_t last_press = 0;
    uint32_t now = k_uptime_get_32();

    /* Software Debouncing */
    if (now - last_press > 250) {
        /* Toggle the heartbeat state atomically */
        atomic_set(&heartbeat_enabled, !atomic_get(&heartbeat_enabled));
        last_press = now;
    }
}

/**
 * @brief Initializes GPIOs, Interrupts, and PWM channels.
 * * Configures internal LED as output.
 * * Sets up the Boot Button with Pull-up and Edge Interrupt.
 * * Starts static PWM signals on GPIO2 and GPIO3.
 */
void init_pwm_gpio(void) {
    /* Status LED setup */
    gpio_pin_configure_dt(&led_internal, GPIO_OUTPUT_INACTIVE);

    /* Button setup with interrupt callback */
    gpio_pin_configure_dt(&btn_boot, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_interrupt_configure_dt(&btn_boot, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&btn_cb_data, button_isr, BIT(btn_boot.pin));
    gpio_add_callback(btn_boot.port, &btn_cb_data);

    /* PWM Channel 1: Static 10% Duty Cycle */
    //if (device_is_ready(pwm_fade.dev)) {
        //pwm_set_dt(&pwm_fade, pwm_fade.period, PWM_PERCENT(pwm_fade.period, 10));
    //}

    /* PWM Channel 2: Static 20% Duty Cycle */
    //if (device_is_ready(pwm_static.dev)) {
        //pwm_set_dt(&pwm_static, pwm_static.period, PWM_PERCENT(pwm_static.period, 20));
    //}
}

/**
 * @brief Processes the LED heartbeat toggle.
 * Called from the main loop. Uses atomic_cas to consume the trigger.
 */
void handle_heartbeat(void) {
    /* If the trigger was set to 1 by the timer, reset it to 0 and proceed */
    if (atomic_cas(&heartbeat_trigger, 1, 0)) {
        if (atomic_get(&heartbeat_enabled)) {
            gpio_pin_toggle_dt(&led_internal);
        } else {
            /* If disabled via button, ensure LED stays OFF */
            gpio_pin_set_dt(&led_internal, 0);
        }
    }
}

/**
 * @brief Timer callback to signal a heartbeat tick.
 * Runs in ISR context; sets the trigger flag atomically.
 */
void heartbeat_timer_cb(struct k_timer *dummy) { 
    atomic_set(&heartbeat_trigger, 1); 
}