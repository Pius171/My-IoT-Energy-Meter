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
#include "modbus_handler.hpp"
#include <cmath>
// constants
static const char *TAG = "METER_APP";
static const char *TAG_MODBUS = "MODBUS_RTU";
static const char *TAG_FS = "FS";

#define UART_PORT_NUM UART_NUM_1
#define TXD_PIN 23
#define RXD_PIN 22
#define RTS_PIN 18
#define CTS_PIN UART_PIN_NO_CHANGE
#define BUF_SIZE 127

// variables
bool config_file_exists = false;


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

        esp_log_level_set(TAG, ESP_LOG_INFO);
        esp_log_level_set(TAG_MODBUS, ESP_LOG_INFO);
        esp_log_level_set(TAG_FS, ESP_LOG_INFO);


        ESP_LOGI(TAG_MODBUS, "Modbus RTU Master Initialized...\n");
        ESP_ERROR_CHECK(init_meter_hardware(meter_config));
        ESP_LOGI(TAG_MODBUS,"MODBUS RTU Successful configured");

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
                int voltage_divider = meter_config["regs"][Vp]["divider"].as<int>(); // the divider is bascially a scale
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
