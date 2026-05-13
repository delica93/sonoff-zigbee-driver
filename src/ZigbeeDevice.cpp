#include "zigbee/ZigbeeDevice.hpp"

#include <format>
#include <bit>

namespace zigbee {

ZigbeeDevice::ZigbeeDevice(EUI64 eui64, ShortAddr shortAddr, uint8_t capabilities)
    : eui64_(eui64)
    , shortAddr_(shortAddr)
    , capabilities_(capabilities)
    , joinedAt_(std::chrono::system_clock::now())
    , lastSeen_(joinedAt_)
{}

// ─────────────────────────────────────────────────────────────────────────────
// Derived properties
// ─────────────────────────────────────────────────────────────────────────────

DeviceType ZigbeeDevice::deviceType() const noexcept {
    if (capabilities_ & static_cast<uint8_t>(Capability::AlternatePanCoordinator))
        return DeviceType::Coordinator;
    if (capabilities_ & static_cast<uint8_t>(Capability::FullFunctionDevice))
        return DeviceType::Router;
    return DeviceType::EndDevice;
}

bool ZigbeeDevice::isRouterCapable() const noexcept {
    return (capabilities_ & static_cast<uint8_t>(Capability::FullFunctionDevice)) != 0;
}

bool ZigbeeDevice::isMainsPowered() const noexcept {
    return (capabilities_ & static_cast<uint8_t>(Capability::MainsPowered)) != 0;
}

bool ZigbeeDevice::rxOnWhenIdle() const noexcept {
    return (capabilities_ & static_cast<uint8_t>(Capability::ReceiverOnWhenIdle)) != 0;
}

// ─────────────────────────────────────────────────────────────────────────────
// Formatting
// ─────────────────────────────────────────────────────────────────────────────

std::string ZigbeeDevice::eui64ToString() const {
    // EUI-64 is stored LSB-first; display MSB-first (big-endian, colon-separated)
    return std::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}",
        eui64_[7], eui64_[6], eui64_[5], eui64_[4],
        eui64_[3], eui64_[2], eui64_[1], eui64_[0]);
}

// ─────────────────────────────────────────────────────────────────────────────
// Timestamp
// ─────────────────────────────────────────────────────────────────────────────

void ZigbeeDevice::updateLastSeen() noexcept {
    lastSeen_ = std::chrono::system_clock::now();
}

} // namespace zigbee
