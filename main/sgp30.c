#include "sgp30.h"
#include <string.h>

esp_err_t sgp30_transmit_command(i2c_master_dev_handle_t i2c_device, uint8_t* command) {
    esp_err_t result = i2c_master_transmit(i2c_device, command, 2, -1);
    return result;
}

esp_err_t sgp30_receive_result(i2c_master_dev_handle_t i2c_device, struct sgp30_data *readings) {
    uint8_t read_data[6];
    esp_err_t read_result = i2c_master_receive(i2c_device, read_data, 6, -1);

    if (read_result != ESP_OK) {
        return read_result;
    }

    uint16_t co2_eq;
    uint16_t tvoc;

    memcpy(read_data, &co2_eq, 2);
    memcpy((read_data + 3), &tvoc, 2);

    readings -> co2_eq = co2_eq;
    readings -> tvoc = tvoc;

    return ESP_OK;
}