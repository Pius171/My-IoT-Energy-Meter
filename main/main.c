#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>
#include "nanomodbus.h"

// constants
static const char *TAG = "METER_APP";

#define UART_PORT_NUM UART_NUM_1
#define UART_BAUD_RATE 4800//9600
#define TXD_PIN 23
#define RXD_PIN 22
#define RTS_PIN 18
#define CTS_PIN UART_PIN_NO_CHANGE
#define BUF_SIZE 127
#define RX_BUF_SIZE 1024

int32_t read(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg){
    int32_t len = uart_read_bytes(UART_PORT_NUM, buf, count, byte_timeout_ms / portTICK_PERIOD_MS);
    return len;
}
int32_t write(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg){
    int32_t len = uart_write_bytes(UART_PORT_NUM, (const char*)buf, count);
    return len;
}

void app_main(void)
{

    const uart_config_t uart_config = {
        .baud_rate = UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT};

    esp_log_level_set(TAG, ESP_LOG_INFO);

    ESP_LOGI(TAG, "Start RS485 application test and configure UART.");

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));

    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, TXD_PIN, RXD_PIN, RTS_PIN, CTS_PIN));
    // Set RS485 half duplex mode
    ESP_ERROR_CHECK(uart_set_mode(UART_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX));

    ESP_LOGI(TAG, "UART configured successfully.");

    uint8_t *data = (uint8_t *)malloc(RX_BUF_SIZE + 1);

  nmbs_platform_conf platform_conf;
  nmbs_platform_conf_create(&platform_conf);
  platform_conf.transport = NMBS_TRANSPORT_RTU;
  platform_conf.read = read;
  platform_conf.write = write;

    while (1)
    {
  nmbs_t nmbs;
  nmbs_error err = nmbs_client_create(&nmbs, &platform_conf);
  if (err != NMBS_ERROR_NONE)
    {
        ESP_LOGE(TAG, "Failed to create Modbus client: %d", err);
        return;
    }

  nmbs_set_read_timeout(&nmbs, 1000/portTICK_PERIOD_MS);
  nmbs_set_byte_timeout(&nmbs, 100/portTICK_PERIOD_MS);

  nmbs_set_destination_rtu_address(&nmbs, 1);
    uint16_t r_regs[2];
  err = nmbs_input_holding_registers(&nmbs, 0, 1, r_regs);
  if (err != NMBS_ERROR_NONE){
    ESP_LOGE(TAG, "Failed to read holding registers: %d", err);
    return;
  }


        // Send data
        uint8_t cmd[] = {0x01, 0x03, 0x00, 0x00, 0x00, 0x02, 0xC4, 0x0B};
        //uint8_t cmd[] = {0x01, 0x04, 0x07, 0x32, 0x00, 0x02, 0xD1, 0x70}; // for meter
        // int cmd_len = sizeof(cmd) / sizeof(cmd[0]);
        uart_write_bytes(UART_PORT_NUM, cmd, sizeof(cmd));

        // Read data (non-blocking check with short timeout)
        int len = uart_read_bytes(UART_PORT_NUM, data, RX_BUF_SIZE, 100 / portTICK_PERIOD_MS);
       
        if (len > 0)
        {
        ESP_LOGI(TAG, "Read %d bytes from UART", len);

        // Print received data
        for (int i = 0; i < len; i++)
        {
            printf("0x%.2X \n", (uint8_t)data[i]);
        }
    }
        else
        {
            ESP_LOGW(TAG, "No data received within timeout period");
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    free(data);
}
