#include <stdio.h>               // Standard C library for basic input/output operations like printf
#include "freertos/FreeRTOS.h"   // Core definitions for the FreeRTOS real-time operating system
#include "freertos/task.h"       // FreeRTOS task management features, such as vTaskDelay
#include "driver/uart.h"         // ESP-IDF driver for UART hardware (used for RS485 communication)
#include "driver/gpio.h"         // ESP-IDF driver for controlling general purpose I/O pins
#include "esp_log.h"             // ESP-IDF logging facility for formatted debug/info terminal output
#include "modbus_parser.hpp"     // Custom parser to build and decode Modbus RTU protocol frames
#include "config_server.hpp"     // Custom local web server module for provisioning via Wi-Fi SoftAP
#include "ArduinoJson.h"         // Popular 3rd-party library for serializing and deserializing JSON data
#include "modbus_handler.hpp"    // Custom helper module wrapping UART requests to poll Modbus registers
#include "myPPP.h"               // Handles cellular point-to-point (PPP) connectivity over Serial
#include "my_mqtt.h"             // Custom wrapper to manage MQTT connections and data publishing
#include <cmath>                 // Standard C++ math library for complex mathematical functions

// constants

// Logging tags for different modules to differentiate log messages in the terminal output
static const char *TAG = "METER_APP";
static const char *TAG_MODBUS = "MODBUS_RTU";
static const char *TAG_FS = "FS";


// variables
bool config_file_exists = false; // Global flag to track if the configuration file is present in the filesystem, used to conditionally execute Modbus polling and MQTT publishing logic

extern "C" void app_main(void)
{
    run_config_server(); // Start the local configuration server in a separate FreeRTOS task
    config_file_exists = is_config_file_present(); 
    JsonDocument meter_config; // hold the config file in memory for easy access
    JsonDocument meter_data;   // hold the latest meter data in memory for easy access and publishing

    ppp_setup(); // Initialize the cellular modem and establish a PPP connection to the internet
    modem_config(); // Configure modem settings such as APN, username, and password for cellular connectivity
    ppp_data_mode_start();

    my_mqtt_config(); // sets the MQTT broker address, port, topic and credentials

    esp_log_level_set(TAG, ESP_LOG_INFO);
    esp_log_level_set(TAG_MODBUS, ESP_LOG_INFO);
    esp_log_level_set(TAG_FS, ESP_LOG_INFO);

    if (config_file_exists)
    {
        deserializeJson(meter_config, load_file_to_string()); // loads the meter configuration file to memory
        ESP_LOGI(TAG_MODBUS, "Modbus RTU Master Initialized...\n"); 
        ESP_ERROR_CHECK(RS485_setup(meter_config)); // use the details in meter_config to setup the RS485 UART peripheral
        ESP_LOGI(TAG_MODBUS, "MODBUS RTU Successful configured");
    }
    else
    {
        ESP_LOGW(TAG_FS, "config file doesn't exist, skipping RS485-UART configuration");
    }

    while (1)
    {

        if (config_file_exists)
        {
            int phase = meter_config["meter_info"]["phase"].as<int>(); // check if the meter is single or three phase
            // based on phase; loop and get voltage,current and power factor
            for (int i = 1; i <= phase; i++)
            {
                // construct the keys for voltage, current and power factor based on the phase number, e.g. V1, I1, pf1 for phase 1
                std::string Vp = std::string("V") + std::to_string(i);  // phase voltage key in config file
                std::string Ip = std::string("I") + std::to_string(i);  // phase current key in config file
                std::string pf = std::string("pf") + std::to_string(i); // phase power factor key in config file

                // get the values for voltage, current and power factor
                double voltage = get_modbus_parameter(Vp, meter_config);
                double current = get_modbus_parameter(Ip, meter_config);
                double power_factor = get_modbus_parameter(pf, meter_config);

                // store the values in the meter_data JSON document using the same keys, e.g. "V1", "I1", "pf1"
                meter_data[Vp] = voltage;
                meter_data[Ip] = current;
                meter_data[pf] = power_factor;
            }

            // then get frequency, power and energy
            double frequency = get_modbus_parameter("freq", meter_config);
            double energy = get_modbus_parameter("energy", meter_config);
            double power = get_modbus_parameter("power", meter_config);

            meter_data["freq"] = frequency;
            meter_data["energy"] = energy;
            meter_data["power"] = power;
            
            //ESP_LOGI(TAG_MODBUS, "Energy: %f kWh", energy);
           // ESP_LOGI(TAG_MODBUS, "Energy: %f kWh, Power: %f W", energy,power);

            std::string output;
            serializeJson(meter_data, output);
             my_mqtt_publish(output.c_str(), 1);
            printf("Meter Data JSON:\n%s\n", output.c_str());
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
