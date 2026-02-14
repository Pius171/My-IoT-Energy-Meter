#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>
#include "modbus_parser.hpp"
#include "config_server.hpp"
#include "ArduinoJson.h"

// constants
static const char *TAG = "METER_APP";
static const char *TAG_UART = "UART";
static const char *TAG_MODBUS = "MODBUS";
static const char *TAG_FS = "FS";

#define UART_PORT_NUM UART_NUM_1
#define TXD_PIN 23
#define RXD_PIN 22
#define RTS_PIN 18
#define CTS_PIN UART_PIN_NO_CHANGE
#define BUF_SIZE 127
#define RX_BUF_SIZE 1024

// variables
bool config_file_exists = false;

// function prototypes
uart_word_length_t get_data_bits(int val);
uart_parity_t get_parity(std::string val);
uart_stop_bits_t get_stop_bits(int val);


extern "C" void app_main(void)
{
    run_config_server();
    config_file_exists = is_config_file_present();
    JsonDocument meter_config;
    // Configure UART
    if (config_file_exists)
    {
        deserializeJson(meter_config, load_file_to_string());
        std::string output;
        serializeJsonPretty(meter_config, output);
        printf("%s\n", output.c_str());


        const uart_config_t uart_config = {
            .baud_rate = meter_config["serial"]["baud_rate"],
            .data_bits = get_data_bits(meter_config["serial"]["data_bits"]),
            .parity = get_parity(meter_config["serial"]["parity"]),
            .stop_bits = get_stop_bits(meter_config["serial"]["stop_bits"]),
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
            .source_clk = UART_SCLK_DEFAULT};

        esp_log_level_set(TAG, ESP_LOG_INFO);
        esp_log_level_set(TAG_UART, ESP_LOG_INFO);
        esp_log_level_set(TAG_MODBUS, ESP_LOG_INFO);
        esp_log_level_set(TAG_FS, ESP_LOG_INFO);

        ESP_LOGI(TAG, "Start RS485 application test and configure UART.");

        ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
        ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));

        ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, TXD_PIN, RXD_PIN, RTS_PIN, CTS_PIN));
        // Set RS485 half duplex mode
        ESP_ERROR_CHECK(uart_set_mode(UART_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX));

        ESP_LOGI(TAG_UART, "UART configured successfully.");

        ESP_LOGI(TAG_MODBUS, "Modbus RTU Master Initialized...\n");
    }
    else
    {
        ESP_LOGI(TAG_FS, "config file doesn't exist, skipping UART configuration");
    }
    uint8_t rx_buffer[BUF_SIZE];


    while (1)
    {

        if (config_file_exists)
        {
            // get voltage
            auto request = Modbus::ADU::prepareReadRequest(
                meter_config["meter_info"]["slave_id"], // Slave ID
                meter_config["regs"]["Vl1"]["func"], // function code
                meter_config["regs"]["Vl1"]["addr"], //0 Start Address
                meter_config["regs"]["Vl1"]["bytes"]  //2 Quantity of registers
            );

            // 2. Transmit via UART

            uart_write_bytes(UART_PORT_NUM, (const char *)request.data(), request.size());

            // 3. Receive Response (Timeout 1000ms)
            int len = uart_read_bytes(UART_PORT_NUM, rx_buffer, BUF_SIZE, pdMS_TO_TICKS(1000));

            if (len > 0)
            {
                // 4. Parse using your ADU class
                auto result = Modbus::ADU::parseResponse(rx_buffer, len);

                if (result.has_value())
                {
                    if (result->isError)
                    {

                        ESP_LOGE(TAG_MODBUS, "Modbus Exception: 0x%02X", (uint8_t)result->exceptionCode);
                    }
                    else
                    {
                        printf("Data Received (%d registers):\n", result->registers.size());
                        for (size_t i = 0; i < result->registers.size(); ++i)
                        {
                            printf(" Register[%d]: %u (0x%04X)\n", i, result->registers[i], result->registers[i]);
                        }
                    }
                }
                else
                {
                    printf("Error: CRC mismatch or malformed packet\n");
                }
            }
            else
            {
                printf("Error: No response from sensor (Timeout)\n");
            }

            // Delay 2 seconds before next poll
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        else
        {
            // IMPORTANT: If no config exists, we must still sleep
            // to prevent the Watchdog Timer from rebooting the chip.
            
       // ESP_LOGI(TAG, "Waiting for Config file...");
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}

uart_word_length_t get_data_bits(int val)
{
    if (val == 5)
        return UART_DATA_5_BITS;
    if (val == 6)
        return UART_DATA_6_BITS;
    if (val == 7)
        return UART_DATA_7_BITS;
    if (val == 8)
        return UART_DATA_8_BITS;
    return UART_DATA_BITS_MAX; // Default
}

uart_parity_t get_parity(std::string val)
{
    if (val == "even")
        return UART_PARITY_EVEN;
    if (val == "odd")
        return UART_PARITY_ODD;
    return UART_PARITY_DISABLE; // Default
}

uart_stop_bits_t get_stop_bits(int val)
{   
if(val == 1 ) return UART_STOP_BITS_1;
if(val == 1.5 ) return UART_STOP_BITS_1_5;
if(val == 2 ) return UART_STOP_BITS_2;
return UART_STOP_BITS_MAX; // Default
}

