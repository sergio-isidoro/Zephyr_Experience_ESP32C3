/*
 * I2C Bus Manager Interface.
 * Centralizes hardware access and synchronization for all I2C devices.
 */

#ifndef I2C_BUS_H
#define I2C_BUS_H

#include <zephyr/drivers/i2c.h>
#include <zephyr/kernel.h>

/** * @brief External reference to the I2C device specification.
 * * This spec is linked to the 'aht10' node in the devicetree.
 * * Used by both sensor reading and display rendering tasks.
 */
extern const struct i2c_dt_spec sensor_bus;

/** * @brief System-wide I2C Mutex.
 * * Prevents concurrent access to the SDA/SCL lines.
 * * Essential for stability when multiple threads share the same bus.
 */
extern struct k_mutex i2c_mutex;

/**
 * @brief Initializes and verifies the I2C bus hardware.
 * * Checks if the I2C controller is ready.
 * * Performs a software configuration/reset of the bus to ensure 
 * it is in a clean state after a hardware reset.
 */
void init_i2c_bus(void);

#endif /* I2C_BUS_H */