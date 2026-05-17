#ifndef MODBUS_HANDLER_HPP
#define MODBUS_HANDLER_HPP

#include <string>
#include "ArduinoJson.h"
#include "driver/uart.h"

// Hardware Defines - keeping them here makes them easy to find
#define UART_PORT_NUM UART_NUM_2
#define BUF_SIZE 127

/**
 * @brief Configures the UART hardware and handles the Modbus polling logic.
 */
esp_err_t RS485_setup(const JsonDocument& config);
double get_modbus_parameter(const std::string& key, const JsonDocument& config);

#endif