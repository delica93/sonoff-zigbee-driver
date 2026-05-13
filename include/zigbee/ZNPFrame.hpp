#pragma once

#include "Types.hpp"

#include <expected>
#include <optional>
#include <span>
#include <vector>
#include <cstdint>

namespace zigbee::znp {

// ── ZNP wire constants ────────────────────────────────────────────────────────

/// Start-of-frame sentinel byte.
constexpr uint8_t SOF = 0xFE;

/// Maximum payload length inside a single ZNP frame.
constexpr size_t MAX_DATA_LEN = 250;

// ── Command descriptor fields ─────────────────────────────────────────────────

/// Upper 3 bits of CMD0 — direction/type of the command.
enum class CommandType : uint8_t {
    POLL = 0x00, ///< Poll (rarely used)
    SREQ = 0x20, ///< Synchronous request (host → device, expects SRSP)
    AREQ = 0x40, ///< Asynchronous request (either direction, no direct reply)
    SRSP = 0x60, ///< Synchronous response (device → host, answers a SREQ)
};

/// Lower 5 bits of CMD0 — subsystem identifier.
enum class Subsystem : uint8_t {
    RPC_Error = 0x00,
    SYS       = 0x01,
    MAC       = 0x02,
    NWK       = 0x03,
    AF        = 0x04,
    ZDO       = 0x05,
    SAPI      = 0x06,
    UTIL      = 0x07,
    DEBUG     = 0x08,
    APP       = 0x09,
    APP_CNF   = 0x0F,
    ZGP       = 0x15,
};

// ── Frame ─────────────────────────────────────────────────────────────────────

/// A fully parsed ZNP protocol frame.
///
/// Wire layout:
///   SOF(1) | LEN(1) | CMD0(1) | CMD1(1) | DATA(LEN) | FCS(1)
///
/// CMD0 = CommandType(3 bits) | Subsystem(5 bits)
struct Frame {
    CommandType          type;
    Subsystem            subsystem;
    uint8_t              command;
    std::vector<uint8_t> data;

    /// Serialise the frame into a byte buffer ready to be sent over the wire.
    [[nodiscard]] std::vector<uint8_t> serialize() const;

    /// Deserialise a complete frame from a raw byte buffer.
    /// Returns an error if the buffer is too short or the FCS is wrong.
    [[nodiscard]] static std::expected<Frame, ZigbeeError>
    deserialize(std::span<const uint8_t> bytes);
};

// ── Streaming parser ──────────────────────────────────────────────────────────

/// Feed raw bytes from the serial port; extract complete frames one at a time.
class FrameParser {
public:
    /// Ingest raw bytes.  Any complete, FCS-valid frames are queued internally.
    void feed(std::span<const uint8_t> bytes);

    /// Returns true when at least one parsed frame is available.
    [[nodiscard]] bool hasFrame() const noexcept { return !frames_.empty(); }

    /// Pop and return the oldest parsed frame, or std::nullopt.
    [[nodiscard]] std::optional<Frame> nextFrame();

private:
    enum class State : uint8_t { WaitSOF, Length, CMD0, CMD1, Data, FCS };

    State                state_  = State::WaitSOF;
    uint8_t              length_ = 0;
    uint8_t              cmd0_   = 0;
    uint8_t              cmd1_   = 0;
    std::vector<uint8_t> dataAccum_;
    std::vector<Frame>   frames_;
};

} // namespace zigbee::znp
