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
#include <cmath>
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

// variables
bool config_file_exists = false;

// function prototypes
uart_word_length_t get_data_bits(int val);
uart_parity_t get_parity(std::string val);
uart_stop_bits_t get_stop_bits(int val);

double get_modbus_parameter(std::string key, JsonDocument& meter_config);

extern "C" void app_main(void)
{
    run_config_server();
    config_file_exists = is_config_file_present();
    JsonDocument meter_config;
    JsonDocument meter_data; // this will hold the values read from the meter and will be used to send to the cloud
    // Configure UART
    if (config_file_exists)
    {
        deserializeJson(meter_config, load_file_to_string());

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
// This prints the number of bytes NEVER used in the stack (the "safety margin")
// ESP_LOGI(TAG, "Stack High Water Mark: %d bytes", uxTaskGetStackHighWaterMark(NULL));
    while (1)
    {

        if (config_file_exists)
        {
            int phase = meter_config["meter_info"]["phase"].as<int>();
            // based on phase; loop and get voltage and current
            for (int i = 1; i <= phase; i++)
            {
                std::string Vp = std::string("Vl") + std::to_string(i); // phase voltage key in config file
                std::string Ip = std::string("Il") + std::to_string(i); // phase current key in config file
                int voltage_divider = meter_config["regs"][Vp]["divider"].as<int>();
                int current_divider = meter_config["regs"][Ip]["divider"].as<int>();

                int voltage_precision = static_cast<int>(std::log10(voltage_divider));
                int current_precision = static_cast<int>(std::log10(current_divider)); // Number of decimal places based on divider

                double voltage = get_modbus_parameter(Vp, meter_config);
                double current = get_modbus_parameter(Ip, meter_config);

                std::string voltage_unit = meter_config["regs"][Vp]["unit"].as<const char *>();
                std::string current_unit = meter_config["regs"][Ip]["unit"].as<const char *>();

                ESP_LOGI(TAG_MODBUS, "Phase %d: Voltage = %.*f %s, Current = %.*f %s\n", i, voltage_precision, voltage, voltage_unit.c_str(), current_precision, current, current_unit.c_str());
                meter_data[Vp] = voltage;
                meter_data[Ip] = current;
            }

            // then get frequency,power factor, power and energy
            double frequency = get_modbus_parameter("freq", meter_config);
            double power_factor = get_modbus_parameter("pf", meter_config);
            double energy = get_modbus_parameter("energy", meter_config);
            double power = get_modbus_parameter("power", meter_config);

            meter_data["freq"] = frequency;
            meter_data["pf"] = power_factor;
            meter_data["energy"] = energy;
            meter_data["power"] = power;

            // for logging sake doesnt really contribute to the functionality, 
            //we can remove later if we want. The main work is done in get_modbus_parameter function
            std::string frequency_unit = meter_config["regs"]["freq"]["unit"].as<const char *>();
            std::string energy_unit = meter_config["regs"]["energy"]["unit"].as<const char *>();
            std::string power_unit = meter_config["regs"]["power"]["unit"].as<const char *>();

            int frequency_divider = meter_config["regs"]["freq"]["divider"].as<int>();
            int energy_divider = meter_config["regs"]["energy"]["divider"].as<int>();
            int power_divider = meter_config["regs"]["power"]["divider"].as<int>();


            int frequency_precision = static_cast<int>(std::log10(frequency_divider));
            int energy_precision = static_cast<int>(std::log10(energy_divider));
            int power_precision = static_cast<int>(std::log10(power_divider));



            ESP_LOGI(TAG_MODBUS, "Frequency = %.*f %s, Power Factor = %.*f, Energy = %.*f %s, Power = %.*f %s\n", frequency_precision, frequency, frequency_unit.c_str(), power_precision, power_factor, energy_precision, energy, energy_unit.c_str(), power_precision, power, power_unit.c_str());

            // Delay 2 seconds before next poll
            // std::string output;
            // serializeJsonPretty(meter_data, output);
            // printf("Meter Data JSON:\n%s\n", output.c_str());
            vTaskDelay(pdMS_TO_TICKS(2000));
        }
        else
        {
            // IMPORTANT: If no config exists, we must still sleep
            // to prevent the Watchdog Timer from rebooting the chip
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
    if (val == 1)
        return UART_STOP_BITS_1;
    if (val == 1.5)
        return UART_STOP_BITS_1_5;
    if (val == 2)
        return UART_STOP_BITS_2;
    return UART_STOP_BITS_MAX; // Default
}

// double get_modbus_parameter(std::string key, JsonDocument meter_config)
// {
//     uint8_t rx_buffer[BUF_SIZE];
//     auto voltages = Modbus::ADU::prepareReadRequest(
//         meter_config["meter_info"]["slave_id"], // Slave ID
//         meter_config["regs"][key]["func"],      // function code
//         meter_config["regs"][key]["addr"],      // Start Address
//         meter_config["regs"][key]["qty"]);
//     // 2. Transmit via UART

//     uart_write_bytes(UART_PORT_NUM, (const char *)voltages.data(), voltages.size());

//     // 3. Receive Response (Timeout 1000ms)
//     int len = uart_read_bytes(UART_PORT_NUM, rx_buffer, BUF_SIZE, pdMS_TO_TICKS(1000));
//     if (len > 0)
//     {
//         // 4. Parse using your ADU class
//         auto result = Modbus::ADU::parseResponse(rx_buffer, len);

//         if (result.has_value())
//         {
//             if (result->isError)
//             {

//                 ESP_LOGE(TAG_MODBUS, "Modbus Exception: 0x%02X", (uint8_t)result->exceptionCode);
//                 return -3.0; // indicate modbus exception with a specific error code
//             }
//             else
//             {
//                 ESP_LOGI(TAG_MODBUS, "Data Received (%d registers):\n", result->registers.size());
//                 for (size_t i = 0; i < result->registers.size(); ++i)
//                 {
//                     printf(" Register[%d]: %u (0x%04X)\n", i, result->registers[i], result->registers[i]);
//                 }
//                 int divider = meter_config["regs"][key]["divider"].as<int>();
//                 double value = static_cast<double>(result->value) / divider; // cast to double for division otherwise it truncates to an integer
//                 return value;
//             }
//         }
//         else
//         {
//             ESP_LOGE(TAG_MODBUS, "Error: CRC mismatch or malformed packet\n");
//             return -1.0;
//         }
//     }
//     else
//     {
//         ESP_LOGE(TAG, "Error: No response from slave (Timeout)\n");
//         return -2.0;
//     }
// return -999.0; // default error code for unexpected cases
// }


double get_modbus_parameter(std::string key, JsonDocument& meter_config)
{
   static uint8_t rx_buffer[BUF_SIZE];
    auto voltages = Modbus::ADU::prepareReadRequest(
        meter_config["meter_info"]["slave_id"], 
        meter_config["regs"][key]["func"],      
        meter_config["regs"][key]["addr"],      
        meter_config["regs"][key]["qty"]);

    // 1. Transmit
    uart_write_bytes(UART_PORT_NUM, (const char *)voltages.data(), voltages.size());

    // 2. Receive Response (Timeout Check)
    int len = uart_read_bytes(UART_PORT_NUM, rx_buffer, BUF_SIZE, pdMS_TO_TICKS(1000));
    if (len <= 0) {
        ESP_LOGE(TAG, "Error: No response from slave (Timeout)");
        return -1.0; 
    }

    // 3. Parse Response (Format Check)
    auto result = Modbus::ADU::parseResponse(rx_buffer, len);
    if (!result.has_value()) {
        ESP_LOGE(TAG_MODBUS, "Error: CRC mismatch or malformed packet");
        return -2.0;
    }

    // 4. Check for Modbus Exceptions (Device-level error)
    if (result->isError) {
        ESP_LOGE(TAG_MODBUS, "Modbus Exception: 0x%02X", (uint8_t)result->exceptionCode);
        return -3.0;
    }

    // 5. SUCCESS PATH: The "Happy Path" stays clean and un-nested
    ESP_LOGI(TAG_MODBUS, "Data Received (%d registers)", result->registers.size());
    
    // Optional: Log registers for debugging
    for (size_t i = 0; i < result->registers.size(); ++i) {
        printf(" Register[%d]: %u (0x%04X)\n", i, result->registers[i], result->registers[i]);
    }

    int divider = meter_config["regs"][key]["divider"].as<int>();
    
    // Safety check for division by zero if config is missing
    if (divider == 0) divider = 1; 

    double value = static_cast<double>(result->value) / divider;
    return value;
}