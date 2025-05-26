#ifndef __SGP30_H
#define __SGP30_H

#include "driver/i2c_master.h"

/**
 * Macros for commands.
 */

 #define IAQ_INIT_COMMAND {0x20, 0x03}
 #define MEASURE_IAQ_COMMAND {0x20, 0x08}

/**
 * Helper functions for reading data from the SGP30.
 */

struct sgp30_data {
    uint16_t co2_eq;
    uint16_t tvoc;
};

esp_err_t sgp30_transmit_command(i2c_master_dev_handle_t i2c_device, uint8_t* command);

esp_err_t sgp30_receive_result(i2c_master_dev_handle_t i2c_device, struct sgp30_data *readings);

#endif