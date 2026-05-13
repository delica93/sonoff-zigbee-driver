#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace zigbee {

// ── Fundamental address types ─────────────────────────────────────────────────

/// 64-bit IEEE extended unique identifier (EUI-64), stored LSB-first.
using EUI64 = std::array<uint8_t, 8>;

/// 16-bit short network address.
using ShortAddr = uint16_t;

/// Zigbee cluster identifier.
using ClusterId = uint16_t;

/// Zigbee endpoint identifier.
using EndpointId = uint8_t;

/// Zigbee application profile identifier.
using ProfileId = uint16_t;

// ── Well-known addresses ──────────────────────────────────────────────────────

constexpr ShortAddr ADDR_COORDINATOR      = 0x0000;
constexpr ShortAddr ADDR_BROADCAST_ALL    = 0xFFFF;
constexpr ShortAddr ADDR_BROADCAST_RX_ON  = 0xFFFD;
constexpr ShortAddr ADDR_BROADCAST_ROUTER = 0xFFFC;

// ── Error codes ───────────────────────────────────────────────────────────────

enum class ZigbeeError : uint8_t {
    None = 0,
    PortNotFound,
    PortOpenFailed,
    PortReadError,
    PortWriteError,
    Timeout,
    InvalidFrame,
    ProtocolError,
    NotInitialized,
    DeviceNotFound,
    AlreadyConnected,
};

[[nodiscard]] constexpr std::string_view errorToString(ZigbeeError err) noexcept {
    switch (err) {
        case ZigbeeError::None:             return "None";
        case ZigbeeError::PortNotFound:     return "PortNotFound";
        case ZigbeeError::PortOpenFailed:   return "PortOpenFailed";
        case ZigbeeError::PortReadError:    return "PortReadError";
        case ZigbeeError::PortWriteError:   return "PortWriteError";
        case ZigbeeError::Timeout:          return "Timeout";
        case ZigbeeError::InvalidFrame:     return "InvalidFrame";
        case ZigbeeError::ProtocolError:    return "ProtocolError";
        case ZigbeeError::NotInitialized:   return "NotInitialized";
        case ZigbeeError::DeviceNotFound:   return "DeviceNotFound";
        case ZigbeeError::AlreadyConnected: return "AlreadyConnected";
        default:                            return "Unknown";
    }
}

} // namespace zigbee
