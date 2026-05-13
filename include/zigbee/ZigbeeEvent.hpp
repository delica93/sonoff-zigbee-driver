#pragma once

#include "Types.hpp"

#include <chrono>
#include <string>
#include <variant>
#include <vector>

namespace zigbee {

// ─────────────────────────────────────────────────────────────────────────────
// Individual event types
// ─────────────────────────────────────────────────────────────────────────────

/// The USB dongle has been opened and the coordinator stack is running.
struct DongleConnectedEvent {
    std::string                           portName;
    std::chrono::system_clock::time_point timestamp;
};

/// The USB dongle serial port has been closed (intentionally or due to error).
struct DongleDisconnectedEvent {
    std::string                           portName;
    std::string                           reason;
    std::chrono::system_clock::time_point timestamp;
};

/// The coordinator's internal network state has changed.
struct CoordinatorStateChangedEvent {
    uint8_t                               newState; ///< znp::cmd::DevState value
    std::chrono::system_clock::time_point timestamp;
};

/// A new or rejoining device has announced itself on the network.
struct DeviceJoinedEvent {
    EUI64                                 eui64;
    ShortAddr                             shortAddr;
    uint8_t                               capabilities;
    std::chrono::system_clock::time_point timestamp;
};

/// A device has left (or been removed from) the network.
struct DeviceLeftEvent {
    EUI64                                 eui64;
    ShortAddr                             shortAddr;
    bool                                  rejoin;   ///< Device plans to rejoin
    std::chrono::system_clock::time_point timestamp;
};

/// An AF (Application Framework) data frame has been received from the network.
struct MessageReceivedEvent {
    ShortAddr                             srcAddr;
    EndpointId                            srcEndpoint;
    EndpointId                            dstEndpoint;
    ClusterId                             clusterId;
    ProfileId                             profileId;
    uint8_t                               linkQuality;
    std::vector<uint8_t>                  payload;
    std::chrono::system_clock::time_point timestamp;
};

// ─────────────────────────────────────────────────────────────────────────────
// Unified event variant
// ─────────────────────────────────────────────────────────────────────────────

/// All events that can be emitted by ZigbeeDongle.
using ZigbeeEvent = std::variant<
    DongleConnectedEvent,
    DongleDisconnectedEvent,
    CoordinatorStateChangedEvent,
    DeviceJoinedEvent,
    DeviceLeftEvent,
    MessageReceivedEvent
>;

} // namespace zigbee
