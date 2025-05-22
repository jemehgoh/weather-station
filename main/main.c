#include <stdio.h>
#include <string.h>
#include "driver/uart.h"

#define TX_PIN 

void app_main(void)
{
    // Set UART port and config (for 8N1)
    const uart_port_t uart_num = UART_NUM_1; 
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = 122,
    };

    // Configure UART parameters
    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

    uart_set_pin(uart_num, 5, 6, UART_PIN_NO_CHANGE, 
            UART_PIN_NO_CHANGE);
    
    const uint32_t buffer_size = (1024 * 2);
    QueueHandle_t uart_queue;

    // Install UART driver for UART1
    uart_driver_install(uart_num, buffer_size, buffer_size, 
            12, &uart_queue, 0);
    
    char* test_string = "Hello world!";
    for (uint8_t i = 1; i < 11; i++) 
    {
        uart_write_bytes_with_break(uart_num, test_string, strlen(test_string), 100);
        uart_wait_tx_done(uart_num, 100);
    }
    uart_driver_delete(uart_num);
}