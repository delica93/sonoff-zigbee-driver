#include "zigbee/SerialPort.hpp"

#include <format>
#include <stdexcept>
#include <string>

namespace zigbee::serial {

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

SerialPort::SerialPort(asio::io_context& ioc)
    : ioc_(ioc)
    , port_(ioc)
{}

SerialPort::~SerialPort() {
    close();
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

/// On Windows, COM ports with a number ≥ 10 must be addressed as \\.\COMxx.
std::string SerialPort::normalizePortName(std::string_view name) {
#if defined(_WIN32)
    // Already in the extended form?
    if (name.starts_with("\\\\.\\")) return std::string(name);

    // Extract the numeric suffix of "COMx" (case-insensitive)
    std::string upper(name);
    for (char& c : upper) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    if (upper.starts_with("COM")) {
        const auto numStr = upper.substr(3);
        if (!numStr.empty() && std::ranges::all_of(numStr, ::isdigit)) {
            const int num = std::stoi(numStr);
            if (num >= 10) {
                return std::format("\\\\.\\COM{}", num);
            }
        }
    }
#endif
    return std::string(name);
}

// ─────────────────────────────────────────────────────────────────────────────
// open / close
// ─────────────────────────────────────────────────────────────────────────────

std::expected<void, ZigbeeError>
SerialPort::open(std::string_view portName, uint32_t baudRate) {
    portName_ = normalizePortName(portName);

    asio::error_code ec;
    port_.open(portName_, ec);
    if (ec) return std::unexpected(ZigbeeError::PortOpenFailed);

    // 8N1, no flow control — matches the Sonoff dongle defaults
    port_.set_option(asio::serial_port::baud_rate(baudRate),       ec);
    port_.set_option(asio::serial_port::character_size(8),         ec);
    port_.set_option(asio::serial_port::parity(
                         asio::serial_port::parity::none),          ec);
    port_.set_option(asio::serial_port::stop_bits(
                         asio::serial_port::stop_bits::one),        ec);
    port_.set_option(asio::serial_port::flow_control(
                         asio::serial_port::flow_control::none),    ec);

    if (ec) {
        port_.close();
        return std::unexpected(ZigbeeError::PortOpenFailed);
    }

    startAsyncRead();
    return {};
}

void SerialPort::close() noexcept {
    asio::error_code ec;
    if (port_.is_open()) port_.close(ec);
}

bool SerialPort::isOpen() const noexcept {
    return port_.is_open();
}

// ─────────────────────────────────────────────────────────────────────────────
// Write
// ─────────────────────────────────────────────────────────────────────────────

std::expected<void, ZigbeeError>
SerialPort::write(std::span<const uint8_t> data) {
    asio::error_code ec;
    asio::write(port_, asio::buffer(data.data(), data.size()), ec);
    if (ec) return std::unexpected(ZigbeeError::PortWriteError);
    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// Async read loop
// ─────────────────────────────────────────────────────────────────────────────

void SerialPort::startAsyncRead() {
    port_.async_read_some(
        asio::buffer(readBuf_),
        [this](const asio::error_code& ec, std::size_t n) {
            handleRead(ec, n);
        });
}

void SerialPort::handleRead(const asio::error_code& ec, std::size_t bytesRead) {
    if (ec) {
        if (ec != asio::error::operation_aborted && errorCb_) {
            errorCb_(ZigbeeError::PortReadError, ec.message());
        }
        return;
    }

    if (bytesRead > 0 && readCb_) {
        readCb_(std::span<const uint8_t>{ readBuf_.data(), bytesRead });
    }

    // Continue reading
    startAsyncRead();
}

} // namespace zigbee::serial
