#include "modbus_handler.hpp"
#include "modbus_parser.hpp"
#include "freertos/task.h"
#include "esp_log.h"



esp_err_t RS485_setup(const JsonDocument& config) {
    const uart_config_t uart_config = {
        .baud_rate = config["serial"]["baud_rate"],
        .data_bits = get_data_bits(config["serial"]["data_bits"]),
        .parity = get_parity(config["serial"]["parity"]),
        .stop_bits = get_stop_bits(config["serial"]["stop_bits"]),
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    // Pins can be hardcoded here or pulled from JSON
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, CONFIG_RS485_UART_TX_PIN, CONFIG_RS485_UART_RX_PIN, CONFIG_RS485_UART_DERE_PIN, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_set_mode(UART_PORT_NUM, UART_MODE_RS485_HALF_DUPLEX));
    
    return ESP_OK;
}
// i can use a pointer as a paramter and load the result into the pointer, then I can return an error
double get_modbus_parameter(const std::string& key, const JsonDocument& config) {
    static uint8_t rx_buffer[BUF_SIZE]; // Saves stack space!

    auto request = Modbus::ADU::prepareReadRequest(
        config["meter_info"]["slave_id"], 
        config["regs"][key]["func"],      
        config["regs"][key]["addr"],      
        config["regs"][key]["qty"]);

    uart_write_bytes(UART_PORT_NUM, (const char *)request.data(), request.size());

    int len = uart_read_bytes(UART_PORT_NUM, rx_buffer, BUF_SIZE, pdMS_TO_TICKS(1000));
    if (len <= 0) return -1.0;

    auto result = Modbus::ADU::parseResponse(rx_buffer, len);
    if (!result.has_value()) return -2.0;
    if (result->isError) return -3.0;

    int divider = config["regs"][key]["divider"] | 1;
    return static_cast<double>(result->value) / divider;
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
