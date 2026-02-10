#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>
#include "modbus_parser.hpp"

// constants
static const char *TAG = "METER_APP";

#define UART_PORT_NUM UART_NUM_1
#define UART_BAUD_RATE 4800
#define TXD_PIN 23
#define RXD_PIN 22
#define RTS_PIN 18
#define CTS_PIN UART_PIN_NO_CHANGE
#define BUF_SIZE 127
#define RX_BUF_SIZE 1024

extern "C" void app_main(void)
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

   // uint8_t *data = (uint8_t *)malloc(RX_BUF_SIZE + 1);
    printf("Modbus RTU Master Initialized...\n");

    uint8_t rx_buffer[BUF_SIZE];

   while (1) {
        // 1. Generate Request using your ADU class
        // Example: Read 2 Holding Registers starting at 0x006B
        auto request = Modbus::ADU::prepareReadRequest(
            1,                                 // Slave ID
            Modbus::FunctionCode::ReadHoldingRegisters, 
            0,                               // Start Address
            2                                     // Quantity of registers
        );

        // 2. Transmit via UART

        uart_write_bytes(UART_PORT_NUM, (const char*)request.data(), request.size());
        
        // 3. Receive Response (Timeout 1000ms)
        int len = uart_read_bytes(UART_PORT_NUM, rx_buffer, BUF_SIZE, pdMS_TO_TICKS(1000));

        if (len > 0) {
            // 4. Parse using your ADU class
            auto result = Modbus::ADU::parseResponse(rx_buffer, len);

            if (result.has_value()) {
                if (result->isError) {
                    printf("Modbus Exception: 0x%02X\n", (uint8_t)result->exceptionCode);
                } else {
                    printf("Data Received (%d registers):\n", result->registers.size());
                    for (size_t i = 0; i < result->registers.size(); ++i) {
                        printf(" Register[%d]: %u (0x%04X)\n", i, result->registers[i], result->registers[i]);
                    }
                }
            } else {
                printf("Error: CRC mismatch or malformed packet\n");
            }
        } else {
            printf("Error: No response from sensor (Timeout)\n");
        }

        // Delay 2 seconds before next poll
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}