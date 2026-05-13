#include "zigbee/ZigbeeDongle.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <format>
#include <future>
#include <print>
#include <ranges>
#include <thread>

namespace zigbee {

using namespace std::chrono_literals;

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

ZigbeeDongle::ZigbeeDongle() {
    workGuard_.emplace(asio::make_work_guard(ioc_));
}

ZigbeeDongle::~ZigbeeDongle() {
    disconnect();
}

// ─────────────────────────────────────────────────────────────────────────────
// connect
// ─────────────────────────────────────────────────────────────────────────────

std::expected<void, ZigbeeError> ZigbeeDongle::connect(DongleConfig config) {
    if (connected_.load()) return std::unexpected(ZigbeeError::AlreadyConnected);

    config_ = std::move(config);
    startupComplete_ = false;

    // ── 1. Open serial port ──────────────────────────────────────────────────
    port_ = std::make_unique<serial::SerialPort>(ioc_);
    port_->setReadCallback ([this](auto data) { onSerialData(data);        });
    port_->setErrorCallback([this](auto err, auto msg) { onSerialError(err, msg); });

    if (auto r = port_->open(config_.portName, config_.baudRate); !r)
        return r;

    // ── 2. Start the io_context thread ───────────────────────────────────────
    ioThread_ = std::thread([this] { ioc_.run(); });

    // ── 3. Hard-reset the coordinator ────────────────────────────────────────
    if (auto r = sysReset(); !r) {
        disconnect();
        return r;
    }

    // ── 4. Wait for SYS_RESET_IND ────────────────────────────────────────────
    auto resetResult = waitForFrame(
        [](const znp::Frame& f) {
            return f.type      == znp::CommandType::AREQ
                && f.subsystem == znp::Subsystem::SYS
                && f.command   == znp::cmd::sys::RESET_IND;
        },
        std::chrono::milliseconds(config_.postResetDelayMs + 3000));

    if (!resetResult) {
        disconnect();
        return std::unexpected(resetResult.error());
    }

    // Brief pause to let the firmware settle
    std::this_thread::sleep_for(
        std::chrono::milliseconds(config_.postResetDelayMs));

    // ── 5. Register a default AF endpoint ────────────────────────────────────
    if (auto r = afRegisterEndpoint(1, 0x0104 /*HA profile*/, 0x0005 /*config profile*/); !r) {
        // Non-fatal — the endpoint may already be registered from a prior session
        std::println(stderr, "[zigbee] afRegisterEndpoint warning: {}",
                     errorToString(r.error()));
    }

    // ── 6. Start the coordinator stack ───────────────────────────────────────
    if (auto r = zdoStartupFromApp(); !r) {
        disconnect();
        return r;
    }

    // ── 7. Wait for coordinator to reach ZB_COORD state ──────────────────────
    {
        std::unique_lock lock(startupMutex_);
        const bool ok = startupCv_.wait_for(
            lock,
            std::chrono::milliseconds(config_.startupTimeoutMs),
            [this] { return startupComplete_; });

        if (!ok) {
            disconnect();
            return std::unexpected(ZigbeeError::Timeout);
        }
    }

    connected_ = true;
    emitEvent(DongleConnectedEvent{ config_.portName, std::chrono::system_clock::now() });

    if (config_.permitJoinOnStart) {
        if (auto r = setPermitJoin(0xFE); !r) {
            std::println(stderr, "[zigbee] setPermitJoin warning: {}",
                         errorToString(r.error()));
        }
    }

    return {};
}

// ─────────────────────────────────────────────────────────────────────────────
// disconnect
// ─────────────────────────────────────────────────────────────────────────────

void ZigbeeDongle::disconnect() noexcept {
    if (!ioThread_.joinable() && !port_) return;

    const bool wasConnected = connected_.exchange(false);

    // Cancel waiters so blocked startup threads unblock immediately
    {
        std::lock_guard lock(waitersMutex_);
        for (auto& w : waiters_) w.cancelled->store(true);
        waiters_.clear();
    }

    // Unblock startupCv_ in case connect() is still waiting
    {
        std::lock_guard lock(startupMutex_);
        startupComplete_ = true;
    }
    startupCv_.notify_all();

    // Stop ASIO
    if (workGuard_) workGuard_->reset();
    workGuard_.reset();
    ioc_.stop();

    if (ioThread_.joinable()) ioThread_.join();

    if (port_) {
        port_->close();
        port_.reset();
    }

    // Reset for potential reuse
    ioc_.restart();
    workGuard_.emplace(asio::make_work_guard(ioc_));
    startupComplete_ = false;
    parser_ = znp::FrameParser{};

    if (wasConnected) {
        emitEvent(DongleDisconnectedEvent{
            config_.portName,
            "Disconnected by user",
            std::chrono::system_clock::now()
        });
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// setPermitJoin
// ─────────────────────────────────────────────────────────────────────────────

std::expected<void, ZigbeeError> ZigbeeDongle::setPermitJoin(uint8_t duration) {
    // ZDO_MGMT_PERMIT_JOIN_REQ broadcast to all routers + coordinator (0xFFFC)
    znp::Frame frame;
    frame.type      = znp::CommandType::SREQ;
    frame.subsystem = znp::Subsystem::ZDO;
    frame.command   = znp::cmd::zdo::MGMT_PERMIT_JOIN_REQ;
    frame.data      = {
        0x0F,                   // addrMode: addr16 + broadcast
        0xFC, 0xFF,             // dstAddr: 0xFFFC (all routers + coord)
        duration,               // permit join duration (seconds; 0xFF = forever)
        0x01                    // TC significance
    };
    return sendFrame(frame);
}

// ─────────────────────────────────────────────────────────────────────────────
// Device registry
// ─────────────────────────────────────────────────────────────────────────────

std::vector<std::shared_ptr<ZigbeeDevice>> ZigbeeDongle::devices() const {
    std::lock_guard lock(devicesMutex_);
    std::vector<std::shared_ptr<ZigbeeDevice>> result;
    result.reserve(devicesByEui_.size());
    for (const auto& [_, dev] : devicesByEui_)
        result.push_back(dev);
    return result;
}

std::optional<std::shared_ptr<ZigbeeDevice>>
ZigbeeDongle::findDevice(const EUI64& eui64) const {
    std::lock_guard lock(devicesMutex_);
    const auto it = devicesByEui_.find(eui64);
    if (it == devicesByEui_.end()) return std::nullopt;
    return it->second;
}

std::optional<std::shared_ptr<ZigbeeDevice>>
ZigbeeDongle::findDevice(ShortAddr addr) const {
    std::lock_guard lock(devicesMutex_);
    const auto addrIt = addrToEui_.find(addr);
    if (addrIt == addrToEui_.end()) return std::nullopt;
    const auto devIt = devicesByEui_.find(addrIt->second);
    if (devIt == devicesByEui_.end()) return std::nullopt;
    return devIt->second;
}

bool ZigbeeDongle::removeDevice(const EUI64& eui64) {
    std::lock_guard lock(devicesMutex_);
    const auto it = devicesByEui_.find(eui64);
    if (it == devicesByEui_.end()) return false;
    addrToEui_.erase(it->second->shortAddr());
    devicesByEui_.erase(it);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// sendMessage
// ─────────────────────────────────────────────────────────────────────────────

std::expected<void, ZigbeeError>
ZigbeeDongle::sendMessage(ShortAddr               dst,
                          EndpointId              dstEndpoint,
                          EndpointId              srcEndpoint,
                          ClusterId               clusterId,
                          ProfileId               /*profileId*/,
                          std::span<const uint8_t> payload) {
    if (!connected_.load()) return std::unexpected(ZigbeeError::NotInitialized);
    if (payload.size() > 128) return std::unexpected(ZigbeeError::ProtocolError);

    const uint8_t seqNum = afSeqNum_++;

    znp::Frame frame;
    frame.type      = znp::CommandType::SREQ;
    frame.subsystem = znp::Subsystem::AF;
    frame.command   = znp::cmd::af::DATA_REQUEST;
    frame.data = {
        static_cast<uint8_t>(dst & 0xFF),
        static_cast<uint8_t>(dst >> 8),
        dstEndpoint,
        srcEndpoint,
        static_cast<uint8_t>(clusterId & 0xFF),
        static_cast<uint8_t>(clusterId >> 8),
        seqNum,
        0x00,                                        // options
        0x07,                                        // radius (default 7 hops)
        static_cast<uint8_t>(payload.size()),
    };
    frame.data.insert(frame.data.end(), payload.begin(), payload.end());

    return sendFrame(frame);
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal — serial/frame plumbing
// ─────────────────────────────────────────────────────────────────────────────

void ZigbeeDongle::onSerialData(std::span<const uint8_t> data) {
    parser_.feed(data);
    while (parser_.hasFrame()) {
        if (auto f = parser_.nextFrame(); f.has_value())
            dispatchFrame(*f);
    }
}

void ZigbeeDongle::onSerialError(ZigbeeError err, std::string_view msg) {
    std::println(stderr, "[zigbee] Serial error ({}): {}", errorToString(err), msg);

    if (connected_.load()) {
        connected_ = false;
        emitEvent(DongleDisconnectedEvent{
            config_.portName,
            std::string(msg),
            std::chrono::system_clock::now()
        });
    }
}

void ZigbeeDongle::dispatchFrame(const znp::Frame& frame) {
    // Notify any synchronous waiters first
    notifyWaiters(frame);

    // Only process AREQ frames for async events
    if (frame.type != znp::CommandType::AREQ) return;

    if (frame.subsystem == znp::Subsystem::SYS) {
        if (frame.command == znp::cmd::sys::RESET_IND)
            onSysResetInd(frame);
    } else if (frame.subsystem == znp::Subsystem::ZDO) {
        switch (frame.command) {
        case znp::cmd::zdo::STATE_CHANGE_IND:   onZdoStateChange(frame); break;
        case znp::cmd::zdo::END_DEVICE_ANNCE_IND: onDeviceAnnounce(frame); break;
        case znp::cmd::zdo::LEAVE_IND:           onLeaveInd(frame);       break;
        default: break;
        }
    } else if (frame.subsystem == znp::Subsystem::AF) {
        if (frame.command == znp::cmd::af::INCOMING_MSG)
            onAfIncomingMsg(frame);
    }
}

void ZigbeeDongle::onSysResetInd(const znp::Frame& /*frame*/) {
    // Device has (re)booted.  The startup sequence handles this via the waiter.
}

void ZigbeeDongle::onZdoStateChange(const znp::Frame& frame) {
    if (frame.data.empty()) return;

    const auto state = static_cast<znp::cmd::DevState>(frame.data[0]);
    emitEvent(CoordinatorStateChangedEvent{ frame.data[0], std::chrono::system_clock::now() });

    if (state == znp::cmd::DevState::ZB_COORD) {
        std::lock_guard lock(startupMutex_);
        startupComplete_ = true;
        startupCv_.notify_all();
    }
}

void ZigbeeDongle::onDeviceAnnounce(const znp::Frame& frame) {
    // Payload: NwkAddr(2) | IEEE(8) | Capabilities(1) = 11 bytes
    if (frame.data.size() < 11) return;

    const ShortAddr shortAddr =
        static_cast<uint16_t>(frame.data[0]) |
        (static_cast<uint16_t>(frame.data[1]) << 8);

    EUI64 eui64{};
    std::copy_n(frame.data.begin() + 2, 8, eui64.begin());

    const uint8_t capabilities = frame.data[10];

    addOrUpdateDevice(eui64, shortAddr, capabilities);

    emitEvent(DeviceJoinedEvent{
        eui64, shortAddr, capabilities,
        std::chrono::system_clock::now()
    });
}

void ZigbeeDongle::onLeaveInd(const znp::Frame& frame) {
    // Payload: SrcAddr(2) | IEEE(8) | RemoveChildren(1) | Rejoin(1) = 12 bytes
    if (frame.data.size() < 12) return;

    const ShortAddr shortAddr =
        static_cast<uint16_t>(frame.data[0]) |
        (static_cast<uint16_t>(frame.data[1]) << 8);

    EUI64 eui64{};
    std::copy_n(frame.data.begin() + 2, 8, eui64.begin());

    const bool rejoin = (frame.data[11] != 0);

    emitEvent(DeviceLeftEvent{
        eui64, shortAddr, rejoin,
        std::chrono::system_clock::now()
    });

    if (!rejoin) removeDevice(eui64);
}

void ZigbeeDongle::onAfIncomingMsg(const znp::Frame& frame) {
    // Fixed header: GroupId(2) | ClusterId(2) | SrcAddr(2) | SrcEP(1) |
    //               DstEP(1) | WasBroadcast(1) | LQI(1) | SecurityUse(1) |
    //               Timestamp(4) | TransSeqNum(1) | Len(1) = 16 bytes
    if (frame.data.size() < 17) return;

    const ClusterId clusterId =
        static_cast<uint16_t>(frame.data[2]) |
        (static_cast<uint16_t>(frame.data[3]) << 8);

    const ShortAddr srcAddr =
        static_cast<uint16_t>(frame.data[4]) |
        (static_cast<uint16_t>(frame.data[5]) << 8);

    const EndpointId srcEp  = frame.data[6];
    const EndpointId dstEp  = frame.data[7];
    const uint8_t    lqi    = frame.data[9];
    const uint8_t    payLen = frame.data[16];

    if (frame.data.size() < static_cast<size_t>(17 + payLen)) return;

    std::vector<uint8_t> payload(
        frame.data.begin() + 17,
        frame.data.begin() + 17 + payLen);

    // Update device last-seen
    if (auto dev = findDevice(srcAddr); dev.has_value())
        (*dev)->updateLastSeen();

    // Profile ID is not present in AF_INCOMING_MSG; assume Home Automation (0x0104)
    constexpr ProfileId kDefaultProfile = 0x0104;
    emitEvent(MessageReceivedEvent{
        srcAddr, srcEp, dstEp,
        clusterId,
        kDefaultProfile,
        lqi,
        std::move(payload),
        std::chrono::system_clock::now()
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal — frame sending helpers
// ─────────────────────────────────────────────────────────────────────────────

std::expected<void, ZigbeeError>
ZigbeeDongle::sendFrame(const znp::Frame& frame) {
    if (!port_ || !port_->isOpen())
        return std::unexpected(ZigbeeError::NotInitialized);
    return port_->write(frame.serialize());
}

std::expected<void, ZigbeeError>
ZigbeeDongle::sysReset(znp::cmd::ResetType type) {
    znp::Frame frame;
    frame.type      = znp::CommandType::AREQ;
    frame.subsystem = znp::Subsystem::SYS;
    frame.command   = znp::cmd::sys::RESET_REQ;
    frame.data      = { static_cast<uint8_t>(type) };
    return sendFrame(frame);
}

std::expected<void, ZigbeeError>
ZigbeeDongle::zdoStartupFromApp() {
    znp::Frame frame;
    frame.type      = znp::CommandType::SREQ;
    frame.subsystem = znp::Subsystem::ZDO;
    frame.command   = znp::cmd::zdo::STARTUP_FROM_APP;
    frame.data      = { 0x64, 0x00 }; // startDelay = 100 ms (little-endian)
    return sendFrame(frame);
}

std::expected<void, ZigbeeError>
ZigbeeDongle::afRegisterEndpoint(EndpointId ep, ProfileId profile, uint16_t deviceId) {
    znp::Frame frame;
    frame.type      = znp::CommandType::SREQ;
    frame.subsystem = znp::Subsystem::AF;
    frame.command   = znp::cmd::af::REGISTER;
    frame.data = {
        ep,
        static_cast<uint8_t>(profile   & 0xFF),
        static_cast<uint8_t>(profile   >> 8),
        static_cast<uint8_t>(deviceId  & 0xFF),
        static_cast<uint8_t>(deviceId  >> 8),
        0x00,                   // device version
        0x00,                   // latency (no latency)
        0x00,                   // number of input clusters
        0x00,                   // number of output clusters
    };
    return sendFrame(frame);
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal — waiter mechanism
// ─────────────────────────────────────────────────────────────────────────────

std::expected<znp::Frame, ZigbeeError>
ZigbeeDongle::waitForFrame(std::function<bool(const znp::Frame&)> predicate,
                           std::chrono::milliseconds timeout) {
    auto cancelled = std::make_shared<std::atomic<bool>>(false);
    auto promise   = std::make_shared<std::promise<znp::Frame>>();
    auto future    = promise->get_future();

    {
        std::lock_guard lock(waitersMutex_);
        waiters_.push_back(FrameWaiter{ std::move(predicate), cancelled, promise });
    }

    const auto status = future.wait_for(timeout);
    if (status != std::future_status::ready) {
        cancelled->store(true);

        // Clean up the expired waiter so the handler doesn't try to set_value
        std::lock_guard lock(waitersMutex_);
        std::erase_if(waiters_, [](const FrameWaiter& w) {
            return w.cancelled->load();
        });

        return std::unexpected(ZigbeeError::Timeout);
    }

    return future.get();
}

void ZigbeeDongle::notifyWaiters(const znp::Frame& frame) {
    std::lock_guard lock(waitersMutex_);

    // Remove cancelled entries, then match the first un-cancelled predicate
    std::erase_if(waiters_, [](const FrameWaiter& w) {
        return w.cancelled->load();
    });

    for (auto it = waiters_.begin(); it != waiters_.end(); ++it) {
        if (!it->cancelled->load() && it->predicate(frame)) {
            it->promise->set_value(frame);
            waiters_.erase(it);
            return;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Internal — helpers
// ─────────────────────────────────────────────────────────────────────────────

void ZigbeeDongle::emitEvent(ZigbeeEvent event) {
    if (eventCb_) eventCb_(event);
}

void ZigbeeDongle::addOrUpdateDevice(const EUI64& eui64,
                                     ShortAddr    shortAddr,
                                     uint8_t      capabilities) {
    std::lock_guard lock(devicesMutex_);
    const auto it = devicesByEui_.find(eui64);
    if (it != devicesByEui_.end()) {
        // Device already known — update its short address if it changed
        const ShortAddr old = it->second->shortAddr();
        if (old != shortAddr) {
            addrToEui_.erase(old);
            addrToEui_[shortAddr] = eui64;
            it->second->setShortAddr(shortAddr);
        }
        it->second->updateLastSeen();
    } else {
        auto dev = std::make_shared<ZigbeeDevice>(eui64, shortAddr, capabilities);
        devicesByEui_[eui64]   = dev;
        addrToEui_[shortAddr]  = eui64;
    }
}

} // namespace zigbee
