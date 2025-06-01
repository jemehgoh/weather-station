#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bme280_defs.h"
#include "bme280.h"
#include "bme280_funs.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "portmacro.h"
#include "sgp30.h"

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_log.h"
#include "protocol_examples_common.h"
#include "esp_wifi.h"

#include "esp_http_client.h"
#include "esp_tls.h"

#include <esp_netif_sntp.h>
#include <time.h>

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048
static const char *TAG = "HTTP_CLIENT";

#define HTTP_HOST "192.168.116.237:8080"
#define HTTP_POST_URL "http://192.168.116.237:8080/post"

#define JSON_FORMAT_STRING "{ \"time\" : \"%s\", \"temperature\" : %.2f, \"pressure\" : %.2f, \"humidity\" : %.2f, \"co2Ppm\" : %d, \"tvoc\" : %d }"

// I2C devices for the BME280 and SGP30
i2c_master_dev_handle_t bme_dev_handle;
i2c_master_dev_handle_t sgp_dev_handle;

struct bme280_dev bme280_device;

struct bme280_data sensor_data;

struct sgp30_data sgp_data;
static volatile uint32_t tick_count = 0;

// Queues for transferring sensor data to the HTTP task
QueueHandle_t bme_data_queue;
QueueHandle_t sgp_data_queue;

esp_http_client_handle_t client;
static char response_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};

time_t now;
char strftime_buf[64];
struct tm timeinfo;

int MIN(int first, int second) {
    if (first < second) {
        return first;
    }

    return second;
}

static void setup_i2c_master_dev(i2c_master_dev_handle_t *dev_handle, uint8_t port, uint8_t scl, 
        uint8_t sda, uint8_t dev_address) {

    i2c_master_bus_config_t i2c_master_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = port,
        .scl_io_num = scl,
        .sda_io_num = sda,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    i2c_master_bus_handle_t bus_handle;
    i2c_new_master_bus(&i2c_master_config, &bus_handle);

    i2c_device_config_t dev_cfg = {
    .dev_addr_length = I2C_ADDR_BIT_LEN_7,
    .device_address = dev_address,
    .scl_speed_hz = 100000,
    };

    i2c_master_bus_add_device(bus_handle, &dev_cfg, dev_handle);
}

// HTTP event handler - taken from the ESP-IDF HTTP client example
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (output_len == 0 && evt->user_data) {
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            if (!esp_http_client_is_chunked_response(evt->client)) {
                int copy_len = 0;
                if (evt->user_data) {
                    copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                    if (copy_len) {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                    }
                } else {
                    int content_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        output_buffer = (char *) calloc(content_len + 1, sizeof(char));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (content_len - output_len));
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                }
                output_len += copy_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
    }
    return ESP_OK;
}

// Sets up the HTTP client
esp_http_client_handle_t setup_http_client(char* response_buffer) {
    esp_http_client_config_t client_config = {
        .host = HTTP_HOST,
        .path = "/post",
        .query = "esp",
        .event_handler = _http_event_handler,
        .user_data = response_buffer,
        .disable_auto_redirect = true
    };

    return esp_http_client_init(&client_config);
}

// Sends a post request to the HTTP server
esp_err_t http_client_post(esp_http_client_handle_t client, char* post_data) {
    esp_http_client_set_url(client, HTTP_POST_URL);
    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    return esp_http_client_perform(client);    
}

// Task for getting data from the BME280
void bme280_measure_task(void *pvParameter) {
    while (1) {
        if(bme280_get_sensor_data(BME280_ALL, &sensor_data, &bme280_device) == 0) {
            // printf("%f\n", sensor_data.pressure);
            // printf("%f\n", sensor_data.temperature);
            // printf("%f\n", sensor_data.humidity);
            xQueueSendToBack(bme_data_queue, &sensor_data, 10);
        }

        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

void sgp30_measure_task(void *pvParameter) {
    while (1) {
        if (tick_count == 0) {
            vTaskDelay(2/portTICK_PERIOD_MS);
            uint8_t command[2] = IAQ_INIT_COMMAND;
            sgp30_transmit_command(sgp_dev_handle, command);
            vTaskDelay(10/portTICK_PERIOD_MS);
            tick_count++;
        } else if (tick_count < 16) {
            uint8_t command[2] = MEASURE_IAQ_COMMAND;
            sgp30_transmit_command(sgp_dev_handle, command);
            vTaskDelay(12/portTICK_PERIOD_MS);
            tick_count++;
        } else {
            uint8_t command[2] = MEASURE_IAQ_COMMAND;

            if (sgp30_transmit_command(sgp_dev_handle, command) != ESP_OK) {
                printf("Transmission failure");
                continue;
            }
            vTaskDelay(12/portTICK_PERIOD_MS);
            if (sgp30_receive_result(sgp_dev_handle, &sgp_data) != ESP_OK) {
                printf("Read failure");
                continue;
            }
            // printf("CO2 concentration: %d\n", sgp_data.co2_eq);
            // printf("TVOC: %d\n", sgp_data.tvoc);
            xQueueSendToBack(sgp_data_queue, &sgp_data, 10);
        }

        vTaskDelay(1000/portTICK_PERIOD_MS);
    }
}

void http_task() {
    while (1) {
        // Get sensor data
        struct bme280_data bme_data;
        struct sgp30_data sgp_data;

        xQueueReceive(bme_data_queue, &bme_data, portMAX_DELAY);
        xQueueReceive(sgp_data_queue, &sgp_data, portMAX_DELAY);

        // Set timezone
        time(&now);
        setenv("TZ", "CST-8", 1);
        tzset();

        localtime_r(&now, &timeinfo);
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

        char* post_data;
        asprintf(&post_data, JSON_FORMAT_STRING, strftime_buf, bme_data.temperature, 
                bme_data.pressure, bme_data.humidity, sgp_data.co2_eq, sgp_data.tvoc);

        esp_err_t outcome =  http_client_post(client, post_data);
        if (outcome == ESP_OK) {
            printf("Data posted\n");
        } else {
            printf("Data not sent\n");
        }

        free(post_data);
    }
}

void app_main(void)
{
    // Setup Wi-Fi connection
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());

    // Sync time
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
    esp_netif_sntp_init(&config);

    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK) {
        printf("Failed to update system time within 10s timeout");
    }

    // esp_netif_t *netif = get_example_netif();
    // esp_netif_ip_info_t ip_info;
    // esp_netif_get_ip_info(netif, &ip_info);
    // printf("IP: " IPSTR "\n", IP2STR(&ip_info.ip));
    // printf("GW: " IPSTR "\n", IP2STR(&ip_info.gw));
    // printf("NETMASK: " IPSTR "\n", IP2STR(&ip_info.netmask));

    client = setup_http_client(response_buffer);

    // Initialise BME280
    setup_i2c_master_dev(&bme_dev_handle, I2C_NUM_0, 5, 6, 0x76);
    bme280_setup(&bme280_device, bme_dev_handle);

    struct bme280_settings default_settings ={
        .osr_p = 1,
        .osr_t = 1,
        .osr_h = 1,
        .filter = 0,
        .standby_time = 0,
    };

    bme280_init(&bme280_device);
    bme280_set_sensor_settings(BME280_SEL_ALL_SETTINGS, &default_settings, &bme280_device);
    bme280_set_sensor_mode(BME280_POWERMODE_NORMAL, &bme280_device);

    // Initialise SGP30
    setup_i2c_master_dev(&sgp_dev_handle, I2C_NUM_1, 15, 16, 0x58);

    bme_data_queue = xQueueCreate(16, sizeof(struct bme280_data));
    sgp_data_queue = xQueueCreate(16, sizeof(struct sgp30_data));

    xTaskCreate(sgp30_measure_task, "measure task 1", 4096, NULL, 7, NULL);
    xTaskCreate(bme280_measure_task, "measure task", 4096, NULL, 5, NULL);
    xTaskCreate(http_task, "transmit task", 4096, NULL, 3, NULL);
}