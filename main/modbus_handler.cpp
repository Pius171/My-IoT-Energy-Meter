/**
 * @file modbus_handler.cpp
 * @brief Implementation of the Modbus handler for RS485 communication.
 * 
 * This file provides functions to configure the ESP32 UART peripheral for RS485
 * communication based on a dynamically loaded JSON configuration. It also contains 
 * the core logic for polling Modbus parameters by constructing requests, 
 * transmitting them over UART, reading the responses, and parsing/scaling the data.
 */

#include "modbus_handler.hpp"
#include "modbus_parser.hpp"
#include "freertos/task.h"
#include "esp_log.h"

// Private helper declarations with internal linkage
static uart_word_length_t get_data_bits(int val);
static uart_parity_t get_parity(std::string val);
static uart_stop_bits_t get_stop_bits(double val);


esp_err_t RS485_setup(const JsonDocument& config) {
    const uart_config_t uart_config = {
        .baud_rate = config["serial"]["baud_rate"], // get the baud rate from the JSON config file
        .data_bits = get_data_bits(config["serial"]["data_bits"]),
        .parity = get_parity(config["serial"]["parity"]),
        .stop_bits = get_stop_bits(config["serial"]["stop_bits"]),
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    // Install the UART driver and configure the UART peripheral with the specified settings
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, CONFIG_RS485_UART_TX_PIN, CONFIG_RS485_UART_RX_PIN, CONFIG_RS485_UART_DERE_PIN, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_set_mode(UART_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX));
    
    return ESP_OK;
}

// Function to get a Modbus parameter by constructing a request, sending it over UART, and parsing the response
double get_modbus_parameter(const std::string& key, const JsonDocument& config) {
    static uint8_t rx_buffer[BUF_SIZE]; // Saves stack space!

    // create the request
    auto request = Modbus::ADU::prepareReadRequest(
        config["meter_info"]["slave_id"], // slave ID from config file
        config["regs"][key]["func"],      // function code for the specific parameter, e.g 0x03 for holding registers, from config file
        config["regs"][key]["addr"],      // starting register address for the parameter
        config["regs"][key]["qty"]);      // number of registers to read for the parameter

    // send the request over UART
    uart_write_bytes(UART_PORT_NUM, (const char *)request.data(), request.size());

    // read the response from UART
    int len = uart_read_bytes(UART_PORT_NUM, rx_buffer, BUF_SIZE, pdMS_TO_TICKS(1000));
    if (len <= 0) return -1.0; // no response

    auto result = Modbus::ADU::parseResponse(rx_buffer, len);
    if (!result.has_value()) return -2.0; // CRC error or invalid response
    if (result->isError) return -3.0; // probably the config file is wrong, e.g wrong register address or function code, or the meter doesn't support that parameter

    int divider = config["regs"][key]["divider"] | 1;
    /*
    If a specific parameter in your JSON file (like "pf") 
    didn't explicitly define a "divider" key, 
    standard ArduinoJson behavior would return 0 
    when converting it to an integer. 
    Dividing the result by 0 would cause your program to crash 
    or produce invalid NaN (Not a Number) / Infinity values.
    */
    return static_cast<double>(result->value) / divider;
}

//
static uart_word_length_t get_data_bits(int val)
{
    switch (val) {
        case 5: return UART_DATA_5_BITS;
        case 6: return UART_DATA_6_BITS;
        case 7: return UART_DATA_7_BITS;
        case 8: return UART_DATA_8_BITS;
        default: return UART_DATA_BITS_MAX; // Default
    }
}

static uart_parity_t get_parity(std::string val)
{
    if (val == "even")
        return UART_PARITY_EVEN;
    if (val == "odd")
        return UART_PARITY_ODD;
    return UART_PARITY_DISABLE; // Default
}

static uart_stop_bits_t get_stop_bits(double val)
{
    if (val == 1)
        return UART_STOP_BITS_1;
    if (val == 1.5)
        return UART_STOP_BITS_1_5;
    if (val == 2)
        return UART_STOP_BITS_2;
    return UART_STOP_BITS_MAX; // Default
}
