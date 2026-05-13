#pragma once

// Prevent Windows.h min/max macro pollution.
#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#endif

#include "Types.hpp"
#include "SerialPort.hpp"
#include "ZNPFrame.hpp"
#include "ZNPCommands.hpp"
#include "ZigbeeDevice.hpp"
#include "ZigbeeEvent.hpp"

#include <asio.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <expected>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace zigbee {

// ─────────────────────────────────────────────────────────────────────────────
// Configuration
// ─────────────────────────────────────────────────────────────────────────────

struct DongleConfig {
    /// Serial port name ("COM3" on Windows, "/dev/ttyUSB0" on Linux).
    std::string portName;

    /// Baud rate — the Sonoff dongle runs at 115 200 bps.
    uint32_t baudRate = 115200;

    /// Zigbee channel (11–26).  Channel 15 avoids most Wi-Fi overlap.
    uint8_t channel = 15;

    /// Personal area network ID.  0xFFFF lets the coordinator choose.
    uint16_t panId = 0x1A62;

    /// When true, permit join is enabled immediately after startup.
    bool permitJoinOnStart = false;

    /// Milliseconds to wait after issuing a hard reset before sending
    /// the startup command.  Increase if the dongle is slow to boot.
    uint32_t postResetDelayMs = 1500;

    /// Maximum time (ms) to wait for the coordinator to reach ZB_COORD state.
    uint32_t startupTimeoutMs = 10000;
};

// ─────────────────────────────────────────────────────────────────────────────
// Event callback
// ─────────────────────────────────────────────────────────────────────────────

/// Signature for the event callback.
/// NOTE: Invoked from the internal I/O thread — keep the handler short or
///       dispatch heavy work to a dedicated thread.
using EventCallback = std::function<void(const ZigbeeEvent&)>;

// ─────────────────────────────────────────────────────────────────────────────
// ZigbeeDongle
// ─────────────────────────────────────────────────────────────────────────────

/// Driver for the Sonoff Zigbee 3.0 USB Dongle Plus (CC2652P / Z-Stack).
///
/// @par Thread safety
/// All public methods are thread-safe except setEventCallback(), which must be
/// called before connect().
///
/// @par Typical usage
/// @code
/// zigbee::ZigbeeDongle dongle;
/// dongle.setEventCallback([](const zigbee::ZigbeeEvent& ev) {
///     std::visit([](const auto& e) { /* handle */ }, ev);
/// });
/// dongle.connect({ .portName = "COM3" });
/// // …
/// dongle.disconnect();
/// @endcode
class ZigbeeDongle {
public:
    ZigbeeDongle();
    ~ZigbeeDongle();

    ZigbeeDongle(const ZigbeeDongle&)            = delete;
    ZigbeeDongle& operator=(const ZigbeeDongle&) = delete;

    // ── Lifecycle ────────────────────────────────────────────────────────────

    /// Open the serial port and initialise the Z-Stack coordinator.
    /// Blocks until the coordinator is up or the startup timeout elapses.
    [[nodiscard]] std::expected<void, ZigbeeError>
    connect(DongleConfig config);

    /// Gracefully shut down the coordinator and close the serial port.
    void disconnect() noexcept;

    [[nodiscard]] bool isConnected() const noexcept { return connected_.load(); }

    // ── Network control ──────────────────────────────────────────────────────

    /// Open (or close) the network for joining.
    /// @param duration  0 = disabled; 0xFF = open indefinitely; other = seconds.
    [[nodiscard]] std::expected<void, ZigbeeError>
    setPermitJoin(uint8_t duration = 0xFE);

    // ── Device registry ──────────────────────────────────────────────────────

    /// Return a snapshot of all known devices.  Thread-safe.
    [[nodiscard]] std::vector<std::shared_ptr<ZigbeeDevice>> devices() const;

    /// Look up a device by EUI-64.  Returns nullopt if not found.
    [[nodiscard]] std::optional<std::shared_ptr<ZigbeeDevice>>
    findDevice(const EUI64& eui64) const;

    /// Look up a device by short network address.  Returns nullopt if not found.
    [[nodiscard]] std::optional<std::shared_ptr<ZigbeeDevice>>
    findDevice(ShortAddr addr) const;

    /// Remove a device from the local registry (does not send a ZDO leave).
    bool removeDevice(const EUI64& eui64);

    // ── Messaging ────────────────────────────────────────────────────────────

    /// Send an AF (Application Framework) unicast data request.
    [[nodiscard]] std::expected<void, ZigbeeError>
    sendMessage(ShortAddr               dst,
                EndpointId              dstEndpoint,
                EndpointId              srcEndpoint,
                ClusterId               clusterId,
                ProfileId               profileId,
                std::span<const uint8_t> payload);

    // ── Event callback ───────────────────────────────────────────────────────

    /// Register the event callback.  Must be called before connect().
    void setEventCallback(EventCallback cb) { eventCb_ = std::move(cb); }

private:
    // ── Internal — serial/frame plumbing ─────────────────────────────────────

    void onSerialData (std::span<const uint8_t> data);
    void onSerialError(ZigbeeError err, std::string_view msg);

    void dispatchFrame       (const znp::Frame& frame);
    void onSysResetInd       (const znp::Frame& frame);
    void onZdoStateChange    (const znp::Frame& frame);
    void onDeviceAnnounce    (const znp::Frame& frame);
    void onLeaveInd          (const znp::Frame& frame);
    void onAfIncomingMsg     (const znp::Frame& frame);

    // ── Internal — frame sending ──────────────────────────────────────────────

    [[nodiscard]] std::expected<void, ZigbeeError>
    sendFrame(const znp::Frame& frame);

    [[nodiscard]] std::expected<void, ZigbeeError>
    sysReset(znp::cmd::ResetType type = znp::cmd::ResetType::Hard);

    [[nodiscard]] std::expected<void, ZigbeeError>
    zdoStartupFromApp();

    [[nodiscard]] std::expected<void, ZigbeeError>
    afRegisterEndpoint(EndpointId ep, ProfileId profile, uint16_t deviceId);

    // ── Internal — waiter mechanism (sync rendezvous during startup) ──────────

    struct FrameWaiter {
        std::function<bool(const znp::Frame&)>  predicate;
        std::shared_ptr<std::atomic<bool>>      cancelled;
        std::shared_ptr<std::promise<znp::Frame>> promise;
    };

    [[nodiscard]] std::expected<znp::Frame, ZigbeeError>
    waitForFrame(std::function<bool(const znp::Frame&)> predicate,
                 std::chrono::milliseconds timeout);

    void notifyWaiters(const znp::Frame& frame);

    // ── Internal — helpers ────────────────────────────────────────────────────

    void emitEvent(ZigbeeEvent event);
    void addOrUpdateDevice(const EUI64& eui64, ShortAddr shortAddr, uint8_t capabilities);

    // ── Data members ──────────────────────────────────────────────────────────

    DongleConfig                                   config_;
    asio::io_context                               ioc_;
    std::optional<asio::executor_work_guard<asio::io_context::executor_type>> workGuard_;
    std::unique_ptr<serial::SerialPort>            port_;
    znp::FrameParser                               parser_;
    EventCallback                                  eventCb_;

    // Device registry
    mutable std::mutex                             devicesMutex_;
    std::map<EUI64, std::shared_ptr<ZigbeeDevice>> devicesByEui_;
    std::map<ShortAddr, EUI64>                     addrToEui_;

    // Frame waiters (used during startup handshake)
    std::mutex                                     waitersMutex_;
    std::vector<FrameWaiter>                       waiters_;

    // Startup sync (coord reached ZB_COORD state)
    std::mutex                                     startupMutex_;
    std::condition_variable                        startupCv_;
    bool                                           startupComplete_ = false;

    // IO thread & state
    std::thread                                    ioThread_;
    std::atomic<bool>                              connected_{ false };

    // AF sequence number (wraps at 256)
    uint8_t                                        afSeqNum_{ 0 };
};

} // namespace zigbee
