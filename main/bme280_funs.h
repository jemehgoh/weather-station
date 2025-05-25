#ifndef __BME280_FUNS_H
#define __BME280_FUNS_H

#include <stdio.h>
#include "bme280.h"
#include "driver/i2c_master.h"

// Function to write data to BME280 registers
int8_t bme280_set_data(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr);

// Function to get register data from BME280
int8_t bme280_get_data(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr);

// Delay function for BME280 - just polls CPU for the period.
void bme280_delay(uint32_t period, void *intf_ptr);

// Initialises the given BME280 struct with the given I2C master device.
void bme280_setup(struct bme280_dev *bme280_device, i2c_master_dev_handle_t dev_handle);

#endif