#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "modbus_parser.hpp"
#include "config_server.hpp"
#include "ArduinoJson.h"
#include "modbus_handler.hpp"
#include "myPPP.h"
#include "my_mqtt.h"
#include <cmath>

// constants
static const char *TAG = "METER_APP";
static const char *TAG_MODBUS = "MODBUS_RTU";
static const char *TAG_FS = "FS";

// #define UART_PORT_NUM UART_NUM_2

// variables
bool config_file_exists = false;

extern "C" void app_main(void)
{
    run_config_server();
    config_file_exists = is_config_file_present();
    JsonDocument meter_config; // hold the config file in memory for easy access
    JsonDocument meter_data;   // hold the latest meter data in memory for easy access and publishing

    ppp_setup();
    modem_config();
    ppp_data_mode_start();

     my_mqtt_config();

    esp_log_level_set(TAG, ESP_LOG_INFO);
    esp_log_level_set(TAG_MODBUS, ESP_LOG_INFO);
    esp_log_level_set(TAG_FS, ESP_LOG_INFO);

    if (config_file_exists)
    {
        deserializeJson(meter_config, load_file_to_string());

        ESP_LOGI(TAG_MODBUS, "Modbus RTU Master Initialized...\n");
        ESP_ERROR_CHECK(RS485_setup(meter_config));
        ESP_LOGI(TAG_MODBUS, "MODBUS RTU Successful configured");
    }
    else
    {
        ESP_LOGW(TAG_FS, "config file doesn't exist, skipping RS485-UART configuration");
    }
    // This prints the number of bytes NEVER used in the stack (the "safety margin")
    // ESP_LOGI(TAG, "Stack High Water Mark: %d bytes", uxTaskGetStackHighWaterMark(NULL));
    while (1)
    {

        if (config_file_exists)
        {
            int phase = meter_config["meter_info"]["phase"].as<int>();
            // based on phase; loop and get voltage,current and power factor
            for (int i = 1; i <= phase; i++)
            {
                std::string Vp = std::string("V") + std::to_string(i);  // phase voltage key in config file
                std::string Ip = std::string("I") + std::to_string(i);  // phase current key in config file
                std::string pf = std::string("pf") + std::to_string(i); // phase current key in config file

                double voltage = get_modbus_parameter(Vp, meter_config);
                double current = get_modbus_parameter(Ip, meter_config);
                double power_factor = get_modbus_parameter(pf, meter_config);
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
            // Delay 2 seconds before next poll
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
