#pragma once

#include <vector>
#include <cstdint>
#include <optional>
#include <span> // Note: If your compiler is strictly C++17, use a custom span or const uint8_t*
#include <variant>

namespace Modbus {

/** @enum FunctionCode - Standard Modbus Function Codes */
enum class FunctionCode : uint8_t {
    ReadCoils = 0x01,
    ReadDiscreteInputs = 0x02,
    ReadHoldingRegisters = 0x03,
    ReadInputRegisters = 0x04,
    WriteSingleRegister = 0x06,
    WriteMultipleRegisters = 0x10
};

/** @enum ExceptionCode - Modbus Error Responses */
enum class ExceptionCode : uint8_t {
    IllegalFunction = 0x01,
    IllegalDataAddress = 0x02,
    IllegalDataValue = 0x03,
    SlaveDeviceFailure = 0x04
};

/** @struct ModbusResult - Structured output of a parsed RTU frame */
struct ModbusResult {
    uint8_t slaveAddress;
    FunctionCode functionCode;
    std::vector<uint16_t> registers;
    std::vector<bool> discreteInputs;
    bool isError = false;
    ExceptionCode exceptionCode = static_cast<ExceptionCode>(0);
};

/** @struct PDU - Protocol Data Unit handling core logic */
struct PDU {
    FunctionCode func;
    std::vector<uint8_t> data;

    std::vector<uint8_t> serialize() const;
};

/** @class ADU - Application Data Unit handling Address and CRC */
class ADU {
public:
    // Command Generators
    static std::vector<uint8_t> prepareReadRequest(uint8_t slave, uint8_t func, uint16_t startAddr, uint16_t quantity);
    static std::vector<uint8_t> prepareWriteSingle(uint8_t slave, uint16_t addr, uint16_t value);
    static std::vector<uint8_t> prepareWriteMultiple(uint8_t slave, uint16_t startAddr, const std::vector<uint16_t>& values);

    // Parsing
    static std::optional<ModbusResult> parseResponse(const uint8_t* buffer, size_t length);

private:
    static uint16_t calculateCRC(const uint8_t* data, size_t len);
    static uint16_t swapEndian(uint16_t val);
};

} // namespace Modbus