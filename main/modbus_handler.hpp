#ifndef MODBUS_HANDLER_HPP
#define MODBUS_HANDLER_HPP

#include <string>
#include "ArduinoJson.h"
#include "driver/uart.h"

// Hardware Defines - keeping them here makes them easy to find
#define UART_PORT_NUM UART_NUM_1
#define BUF_SIZE 127

/**
 * @brief Configures the UART hardware and handles the Modbus polling logic.
 */
esp_err_t init_meter_hardware(const JsonDocument& config);
double get_modbus_parameter(const std::string& key, const JsonDocument& config);

// Private helpers (can be moved to the .cpp if you don't need them elsewhere)
uart_word_length_t get_data_bits(int val);
uart_parity_t get_parity(std::string val);
uart_stop_bits_t get_stop_bits(int val);

#endif