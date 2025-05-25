#include "bme280_funs.h"

// Function to write data to BME280 registers
int8_t bme280_set_data(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    esp_err_t write_result = i2c_master_transmit((i2c_master_dev_handle_t)(intf_ptr), reg_data, 
            len, -1);
    if (write_result != ESP_OK) {
        return 1;
    }

    return 0;
}

// Function to get register data from BME280
int8_t bme280_get_data(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    esp_err_t request_result = i2c_master_transmit((i2c_master_dev_handle_t)(intf_ptr), &reg_addr, 
            1, -1);
    if (request_result != ESP_OK) {
        return 1;
    }

    esp_err_t get_result = i2c_master_receive((i2c_master_dev_handle_t)(intf_ptr), reg_data, 
            len, -1);
    if (get_result != ESP_OK) {
        return 1;
    }

    // Return 0 if data read is successful
    return 0;
}

// Delay function for BME280 - just polls CPU for the period.
void bme280_delay(uint32_t period, void *intf_ptr) {
    uint32_t delay_cycles = period * 240;
    for (uint32_t i = 0; i < delay_cycles; i++);
}

// Fills BME280 device struct
void bme280_setup(struct bme280_dev *bme280_device, i2c_master_dev_handle_t dev_handle) {
    int8_t result = 0;
    struct bme280_calib_data dummy_data = {
        .dig_t1 = 0,
        .dig_t2 = 0,
        .dig_t3 = 0,
        .dig_p1 = 0,
        .dig_p2 = 0,
        .dig_p3 = 0,
        .dig_p4 = 0,
        .dig_p5 = 0,
        .dig_p6 = 0,
        .dig_p7 = 0,
        .dig_p8 = 0,
        .dig_p9 = 0,
        .dig_h1 = 0,
        .dig_h2 = 0,
        .dig_h3 = 0,
        .dig_h4 = 0,
        .dig_h5 = 0,
        .dig_h6 = 0,
        .t_fine = 0
    };

    bme280_device -> chip_id = 0x00;
    bme280_device -> intf = BME280_I2C_INTF;
    bme280_device -> intf_ptr = dev_handle;
    bme280_device -> intf_rslt = result;
    bme280_device -> read = bme280_get_data;
    bme280_device -> write = bme280_set_data;
    bme280_device -> delay_us = bme280_delay;
    bme280_device -> calib_data = dummy_data;
}