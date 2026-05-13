#pragma once

#include <cstdint>

/// ZNP command IDs (CMD1 values), grouped by subsystem.
/// The CMD0 type bits (SREQ/AREQ/SRSP) are NOT encoded here; they are set
/// explicitly when constructing Frame objects.
namespace zigbee::znp::cmd {

// ── SYS ───────────────────────────────────────────────────────────────────────
namespace sys {
    /// AREQ to device: request a reset.
    constexpr uint8_t RESET_REQ         = 0x09;
    /// AREQ from device: device has (re)booted.
    constexpr uint8_t RESET_IND         = 0x80;
    /// SREQ / SRSP: capability ping.
    constexpr uint8_t PING              = 0x01;
    /// SREQ / SRSP: firmware version.
    constexpr uint8_t VERSION           = 0x02;
    /// SREQ / SRSP: read a non-volatile item.
    constexpr uint8_t OSAL_NV_READ      = 0x08;
    /// SREQ / SRSP: write a non-volatile item.
    constexpr uint8_t OSAL_NV_WRITE     = 0x09; // same number as RESET_REQ but SREQ type
} // namespace sys

// ── ZDO ───────────────────────────────────────────────────────────────────────
namespace zdo {
    /// SREQ: start the Zigbee coordinator stack.
    constexpr uint8_t STARTUP_FROM_APP      = 0x40;
    /// AREQ: coordinator network state changed.
    constexpr uint8_t STATE_CHANGE_IND      = 0xC0;
    /// AREQ: a device announced itself (joined or rejoined).
    constexpr uint8_t END_DEVICE_ANNCE_IND  = 0xC1;
    /// AREQ: a device has left the network.
    constexpr uint8_t LEAVE_IND             = 0xC9;
    /// SREQ / SRSP: open or close the network for joining.
    constexpr uint8_t MGMT_PERMIT_JOIN_REQ  = 0x36;
    /// AREQ: permit-join response.
    constexpr uint8_t MGMT_PERMIT_JOIN_RSP  = 0xB6;
    /// SREQ / SRSP: active endpoint request.
    constexpr uint8_t ACTIVE_EP_REQ         = 0x05;
    /// AREQ: active endpoint response.
    constexpr uint8_t ACTIVE_EP_RSP         = 0x85;
    /// SREQ: IEEE address lookup by short address.
    constexpr uint8_t IEEE_ADDR_REQ         = 0x01;
    /// AREQ: IEEE address lookup response.
    constexpr uint8_t IEEE_ADDR_RSP         = 0x81;
} // namespace zdo

// ── AF (Application Framework) ────────────────────────────────────────────────
namespace af {
    /// SREQ / SRSP: register an application endpoint.
    constexpr uint8_t REGISTER      = 0x00;
    /// SREQ / SRSP: send a data frame to a device.
    constexpr uint8_t DATA_REQUEST  = 0x01;
    /// AREQ: data-request delivery confirmation.
    constexpr uint8_t DATA_CONFIRM  = 0x80;
    /// AREQ: an incoming data frame from the network.
    constexpr uint8_t INCOMING_MSG  = 0x81;
} // namespace af

// ── UTIL ──────────────────────────────────────────────────────────────────────
namespace util {
    /// SREQ / SRSP: retrieve dongle device information.
    constexpr uint8_t GET_DEVICE_INFO = 0x00;
    /// SREQ / SRSP: subscribe to / unsubscribe from AREQ callbacks.
    constexpr uint8_t CALLBACK_SUB   = 0x06;
} // namespace util

// ── APP_CNF ───────────────────────────────────────────────────────────────────
namespace app_cnf {
    /// SREQ / SRSP: start BDB commissioning (Z-Stack 3.x).
    constexpr uint8_t BDB_START_COMMISSIONING  = 0x00;
    /// SREQ / SRSP: set primary Zigbee channel mask.
    constexpr uint8_t BDB_SET_CHANNEL          = 0x08;
    /// AREQ: BDB commissioning notification.
    constexpr uint8_t BDB_COMMISSIONING_NOTIF  = 0x80;
} // namespace app_cnf

// ── Coordinator device-state values (ZDO_STATE_CHANGE_IND) ───────────────────
enum class DevState : uint8_t {
    HOLD              = 0x00,
    INIT              = 0x01,
    NWK_DISC          = 0x02,
    NWK_JOINING       = 0x03,
    NWK_REJOINING     = 0x04,
    END_DEVICE_UNAUTH = 0x05,
    END_DEVICE        = 0x06,
    ROUTER            = 0x07,
    COORD_STARTING    = 0x08,
    ZB_COORD          = 0x09, ///< Coordinator is up and running
    NWK_ORPHAN        = 0x0A,
};

// ── Reset types ───────────────────────────────────────────────────────────────
enum class ResetType : uint8_t {
    Hard = 0x00,
    Soft = 0x01,
};

} // namespace zigbee::znp::cmd
