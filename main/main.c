#include <stdio.h>
#include <string.h>
#include "bme280_defs.h"
#include "bme280.h"
#include "driver/i2c_master.h"

// Function to write data to BME280 registers
static int8_t bme280_set_data(uint8_t reg_addr, const uint8_t *reg_data, uint32_t len, void *intf_ptr) {
    esp_err_t write_result = i2c_master_transmit((i2c_master_dev_handle_t)(intf_ptr), reg_data, 
            len, -1);
    if (write_result != ESP_OK) {
        return 1;
    }

    return 0;
}

// Function to get register data from BME280
static int8_t bme280_get_data(uint8_t reg_addr, uint8_t *reg_data, uint32_t len, void *intf_ptr) {
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
static void bme280_delay(uint32_t period, void *intf_ptr) {
    uint32_t delay_cycles = period * 240;
    for (uint32_t i = 0; i < delay_cycles; i++);
}

void app_main(void)
{
    i2c_master_bus_config_t i2c_master_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = 5,
        .sda_io_num = 6,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle;
    i2c_new_master_bus(&i2c_master_config, &bus_handle);

    i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = 0x76,
    .scl_speed_hz = 100000,
    };

    i2c_master_dev_handle_t dev_handle;
    i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle);

    int8_t result = 0;
    struct bme280_calib_data dummy_data = 
    {
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

    struct bme280_dev bme280_device = {
        .chip_id = 0x00,
        .intf = BME280_I2C_INTF,
        .intf_ptr = dev_handle,
        .intf_rslt = result,
        .read = bme280_get_data,
        .write = bme280_set_data,
        .delay_us = bme280_delay,
        .calib_data = dummy_data,
    };

    bme280_init(&bme280_device);
    bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &bme280_device);
    
    struct bme280_data sensor_data ={
        .pressure = 0.0,
        .temperature = 0.0,
        .humidity = 0.0
    };

    bme280_get_sensor_data(BME280_ALL, &sensor_data, &bme280_device);
    printf("%f\n", sensor_data.pressure);
    printf("%f\n", sensor_data.temperature);
    printf("%f\n", sensor_data.humidity);
}