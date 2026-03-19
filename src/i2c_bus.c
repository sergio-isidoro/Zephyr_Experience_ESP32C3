/*
 * I2C Bus Manager Implementation.
 * Handles hardware binding and synchronization primitives.
 */

#include "i2c_bus.h"

/**
 * @brief I2C Device Specification.
 * Links the software spec to the 'aht10' label in the app.overlay.
 * This structure contains the controller device and the target slave address.
 */
const struct i2c_dt_spec sensor_bus = I2C_DT_SPEC_GET(DT_NODELABEL(aht10));

/**
 * @brief Global I2C Mutex Definition.
 * This mutex is shared across the display and sensor tasks to 
 * prevent simultaneous access to the I2C hardware registers.
 */
K_MUTEX_DEFINE(i2c_mutex);

/**
 * @brief Initializes the I2C hardware peripheral.
 * * * Verifies that the I2C driver is ready for use.
 * * Configures the bus for Standard Speed (100kHz) and Controller (Master) mode.
 * * This ensures a stable communication state following a CPU reset.
 */
void init_i2c_bus(void) {
    /* Check if the underlying I2C controller driver is initialized */
    if (device_is_ready(sensor_bus.bus)) {
        /* * Force I2C configuration. This helps recover the bus if a 
         * peripheral was left in an unstable state during a quick reset.
         */
        i2c_configure(sensor_bus.bus, 
                      I2C_SPEED_SET(I2C_SPEED_STANDARD) | 
                      I2C_MODE_CONTROLLER);
    }
}