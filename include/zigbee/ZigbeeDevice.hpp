#pragma once

#include "Types.hpp"

#include <chrono>
#include <string>

namespace zigbee {

// ── Capability flags (from ZDO End Device Announce) ──────────────────────────

/// Bits from the Capabilities field in the End Device Announce frame.
enum class Capability : uint8_t {
    AlternatePanCoordinator = 0x01,
    FullFunctionDevice      = 0x02, ///< FFD (router or coordinator)
    MainsPowered            = 0x04,
    ReceiverOnWhenIdle      = 0x08,
    SecurityCapable         = 0x40,
    AllocateAddress         = 0x80,
};

/// Logical device type inferred from capability bits.
enum class DeviceType : uint8_t {
    Coordinator = 0,
    Router      = 1,
    EndDevice   = 2,
    Unknown     = 0xFF,
};

// ─────────────────────────────────────────────────────────────────────────────

/// Represents a remote Zigbee device seen on the network.
class ZigbeeDevice {
public:
    ZigbeeDevice(EUI64 eui64, ShortAddr shortAddr, uint8_t capabilities);

    // ── Identity ────────────────────────────────────────────────────────────

    [[nodiscard]] const EUI64&  eui64()        const noexcept { return eui64_;        }
    [[nodiscard]] ShortAddr     shortAddr()    const noexcept { return shortAddr_;    }
    [[nodiscard]] uint8_t       capabilities() const noexcept { return capabilities_; }

    /// Human-readable EUI-64 (e.g. "0x00:12:4B:00:XX:XX:XX:XX").
    [[nodiscard]] std::string eui64ToString() const;

    // ── Derived properties ──────────────────────────────────────────────────

    [[nodiscard]] DeviceType deviceType()      const noexcept;
    [[nodiscard]] bool       isRouterCapable() const noexcept;
    [[nodiscard]] bool       isMainsPowered()  const noexcept;
    [[nodiscard]] bool       rxOnWhenIdle()    const noexcept;

    // ── Mutable state ───────────────────────────────────────────────────────

    [[nodiscard]] const std::string& name() const noexcept { return name_; }
    void setName(std::string name)                         { name_ = std::move(name); }

    void setShortAddr(ShortAddr addr) noexcept { shortAddr_ = addr; }

    /// Update the "last seen" timestamp to now.
    void updateLastSeen() noexcept;

    // ── Timestamps ──────────────────────────────────────────────────────────

    [[nodiscard]] std::chrono::system_clock::time_point joinedAt()  const noexcept { return joinedAt_;  }
    [[nodiscard]] std::chrono::system_clock::time_point lastSeen()  const noexcept { return lastSeen_;  }

private:
    EUI64       eui64_;
    ShortAddr   shortAddr_;
    uint8_t     capabilities_;
    std::string name_;
    std::chrono::system_clock::time_point joinedAt_;
    std::chrono::system_clock::time_point lastSeen_;
};

} // namespace zigbee
