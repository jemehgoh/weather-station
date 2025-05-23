#include <stdio.h>
#include <string.h>
#include "driver/uart.h"
#include "driver/i2c_master.h"

#define TX_PIN 

void app_main(void)
{
    // // Set UART port and config (for 8N1)
    // const uart_port_t uart_num = UART_NUM_1; 
    // uart_config_t uart_config = {
    //     .baud_rate = 115200,
    //     .data_bits = UART_DATA_8_BITS,
    //     .parity = UART_PARITY_DISABLE,
    //     .stop_bits = UART_STOP_BITS_1,
    //     .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
    //     .rx_flow_ctrl_thresh = 122,
    // };

    // // Configure UART parameters
    // ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

    // uart_set_pin(uart_num, 5, 6, UART_PIN_NO_CHANGE, 
    //         UART_PIN_NO_CHANGE);
    
    // const uint32_t buffer_size = (1024 * 2);
    // QueueHandle_t uart_queue;

    // // Install UART driver for UART1
    // uart_driver_install(uart_num, buffer_size, buffer_size, 
    //         12, &uart_queue, 0);
    
    // char* test_string = "Hello world!";
    // for (uint8_t i = 1; i < 11; i++) 
    // {
    //     uart_write_bytes_with_break(uart_num, test_string, strlen(test_string), 100);
    //     uart_wait_tx_done(uart_num, 100);
    // }
    // uart_driver_delete(uart_num);

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

    uint8_t data = 0xD0;

    i2c_master_transmit(dev_handle, &data, 1, -1);

    uint8_t result;

    i2c_master_receive(dev_handle, &result, 1, -1);

    printf("%x", result);
}