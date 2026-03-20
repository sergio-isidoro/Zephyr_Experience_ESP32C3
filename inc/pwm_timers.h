/*
 * PWM and GPIO Peripheral Interface.
 * Defines the shared atomic flags, PWM utility macros, and hardware 
 * initialization prototypes for the ESP32-C3 SuperMini.
 */

#ifndef PWM_TIMERS_H
#define PWM_TIMERS_H

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/pwm.h>
#include <zephyr/sys/atomic.h>

/**
 * @brief Macro to calculate PWM duty cycle from a percentage.
 * * Converts 0-100% into the period units required by the driver.
 * @param period Total period of the PWM signal (usually in nanoseconds).
 * @param percent Desired duty cycle (0 to 100).
 */
//#define PWM_PERCENT(period, percent)    ((uint64_t)(period) * (percent) / 100)

/* * Shared Atomic Flags
 * Used for thread-safe communication between Timers, ISRs, and the Main Loop.
 */

/** @brief Flag to enable/disable the LED blinking (toggled by Button). */
extern atomic_t heartbeat_enabled;

/** @brief Flag set by the timer to trigger an LED toggle in the main loop. */
extern atomic_t heartbeat_trigger;

/**
 * @brief Initializes all GPIO pins, Interrupts (ISR), and PWM channels.
 * * * Configures internal LED (GPIO8).
 * * Configures Boot Button (GPIO9) with pull-up.
 * * Starts static PWM on GPIO2 and GPIO3.
 */
void init_pwm_gpio(void);

/**
 * @brief Processes the heartbeat LED toggle logic.
 * * Should be called periodically within the main loop.
 * * Consumes the heartbeat_trigger flag atomically.
 */
void handle_heartbeat(void);

/**
 * @brief Callback function for the heartbeat kernel timer.
 * * Runs in ISR context. Sets the heartbeat_trigger flag.
 * @param dummy Pointer to the timer structure (unused).
 */
void heartbeat_timer_cb(struct k_timer *dummy);

#endif /* PWM_TIMERS_H */