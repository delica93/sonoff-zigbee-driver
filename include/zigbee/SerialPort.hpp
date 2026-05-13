#pragma once

// Prevent Windows.h from defining min/max macros that conflict with the STL.
#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#endif

#include "Types.hpp"

#include <asio.hpp>

#include <expected>
#include <functional>
#include <span>
#include <string>
#include <array>
#include <cstdint>


namespace zigbee::serial {

/// Called when new raw bytes arrive from the serial port.
using ReadCallback  = std::function<void(std::span<const uint8_t>)>;

/// Called when a serial error occurs (port disconnected, read failure, etc.).
using ErrorCallback = std::function<void(ZigbeeError, std::string_view message)>;

// ─────────────────────────────────────────────────────────────────────────────

/// Asynchronous serial port wrapper built on top of standalone ASIO.
///
/// One-shot lifecycle: open() → write() / [read callbacks] → close().
/// Async reads are started automatically after open() and driven by the
/// caller-supplied asio::io_context.
class SerialPort {
public:
    explicit SerialPort(asio::io_context& ioc);
    ~SerialPort();

    SerialPort(const SerialPort&)            = delete;
    SerialPort& operator=(const SerialPort&) = delete;
    SerialPort(SerialPort&&)                 = delete;
    SerialPort& operator=(SerialPort&&)      = delete;

    // ── Lifecycle ───────────────────────────────────────────────────────────

    /// Open the port and begin async reads.
    /// @param portName  Platform port name (e.g. "COM3" on Windows,
    ///                  "/dev/ttyUSB0" on Linux).  COM10+ on Windows are
    ///                  automatically translated to the \\.\COMxx form.
    /// @param baudRate  Baud rate; default matches the Sonoff dongle (115 200).
    [[nodiscard]] std::expected<void, ZigbeeError>
    open(std::string_view portName, uint32_t baudRate = 115200);

    /// Stop async reads and close the underlying handle.
    void close() noexcept;

    [[nodiscard]] bool isOpen() const noexcept;

    // ── I/O ─────────────────────────────────────────────────────────────────

    /// Synchronously write all bytes.  Thread-safe via an internal mutex.
    [[nodiscard]] std::expected<void, ZigbeeError>
    write(std::span<const uint8_t> data);

    // ── Callbacks ───────────────────────────────────────────────────────────

    void setReadCallback (ReadCallback  cb) { readCb_  = std::move(cb); }
    void setErrorCallback(ErrorCallback cb) { errorCb_ = std::move(cb); }

    // ── Accessors ───────────────────────────────────────────────────────────

    [[nodiscard]] const std::string& portName() const noexcept { return portName_; }

private:
    static std::string normalizePortName(std::string_view name);
    void startAsyncRead();
    void handleRead(const asio::error_code& ec, std::size_t bytesRead);

    asio::io_context&         ioc_;
    asio::serial_port         port_;
    std::array<uint8_t, 512>  readBuf_{};
    std::string               portName_;
    ReadCallback              readCb_;
    ErrorCallback             errorCb_;
};

} // namespace zigbee::serial
