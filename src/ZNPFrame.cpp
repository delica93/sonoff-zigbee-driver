#include "zigbee/ZNPFrame.hpp"

#include <algorithm>
#include <cassert>
#include <optional>

namespace zigbee::znp {

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace {

/// XOR checksum over the bytes that follow the SOF byte in a ZNP frame
/// (i.e. Length, CMD0, CMD1, and all payload bytes).
[[nodiscard]] uint8_t computeFCS(std::span<const uint8_t> data) noexcept {
    uint8_t fcs = 0;
    for (uint8_t b : data) fcs ^= b;
    return fcs;
}

} // namespace

// ─────────────────────────────────────────────────────────────────────────────
// Frame
// ─────────────────────────────────────────────────────────────────────────────

std::vector<uint8_t> Frame::serialize() const {
    assert(data.size() <= MAX_DATA_LEN && "ZNP frame payload exceeds maximum length");

    const uint8_t len  = static_cast<uint8_t>(data.size());
    const uint8_t cmd0 = static_cast<uint8_t>(
        static_cast<unsigned>(type) | static_cast<unsigned>(subsystem));
    const uint8_t cmd1 = command;

    // FCS covers: Length | CMD0 | CMD1 | Data...
    std::vector<uint8_t> fcsInput;
    fcsInput.reserve(3 + data.size());
    fcsInput.push_back(len);
    fcsInput.push_back(cmd0);
    fcsInput.push_back(cmd1);
    fcsInput.insert(fcsInput.end(), data.begin(), data.end());

    const uint8_t fcs = computeFCS(fcsInput);

    std::vector<uint8_t> out;
    out.reserve(5 + data.size());
    out.push_back(SOF);
    out.push_back(len);
    out.push_back(cmd0);
    out.push_back(cmd1);
    out.insert(out.end(), data.begin(), data.end());
    out.push_back(fcs);
    return out;
}

std::expected<Frame, ZigbeeError>
Frame::deserialize(std::span<const uint8_t> bytes) {
    // Minimum: SOF(1) + LEN(1) + CMD0(1) + CMD1(1) + FCS(1) = 5 bytes
    if (bytes.size() < 5)               return std::unexpected(ZigbeeError::InvalidFrame);
    if (bytes[0] != SOF)                return std::unexpected(ZigbeeError::InvalidFrame);

    const uint8_t len  = bytes[1];
    const uint8_t cmd0 = bytes[2];
    const uint8_t cmd1 = bytes[3];

    if (bytes.size() < static_cast<size_t>(5 + len))
        return std::unexpected(ZigbeeError::InvalidFrame);

    const auto dataSpan  = bytes.subspan(4, len);
    const uint8_t fcsRx  = bytes[4 + len];

    // Build FCS input and verify
    std::vector<uint8_t> fcsInput;
    fcsInput.reserve(3 + len);
    fcsInput.push_back(len);
    fcsInput.push_back(cmd0);
    fcsInput.push_back(cmd1);
    fcsInput.insert(fcsInput.end(), dataSpan.begin(), dataSpan.end());

    if (computeFCS(fcsInput) != fcsRx)
        return std::unexpected(ZigbeeError::InvalidFrame);

    Frame f;
    f.type      = static_cast<CommandType>(cmd0 & 0xE0u);
    f.subsystem = static_cast<Subsystem>  (cmd0 & 0x1Fu);
    f.command   = cmd1;
    f.data.assign(dataSpan.begin(), dataSpan.end());
    return f;
}

// ─────────────────────────────────────────────────────────────────────────────
// FrameParser
// ─────────────────────────────────────────────────────────────────────────────

void FrameParser::feed(std::span<const uint8_t> bytes) {
    for (const uint8_t b : bytes) {
        switch (state_) {

        case State::WaitSOF:
            if (b == SOF) state_ = State::Length;
            break;

        case State::Length:
            if (b > MAX_DATA_LEN) {
                // Spurious SOF — discard and resynchronise
                state_ = State::WaitSOF;
                break;
            }
            length_ = b;
            state_  = State::CMD0;
            break;

        case State::CMD0:
            cmd0_  = b;
            state_ = State::CMD1;
            break;

        case State::CMD1:
            cmd1_ = b;
            dataAccum_.clear();
            state_ = (length_ == 0) ? State::FCS : State::Data;
            break;

        case State::Data:
            dataAccum_.push_back(b);
            if (dataAccum_.size() == static_cast<size_t>(length_))
                state_ = State::FCS;
            break;

        case State::FCS: {
            // Verify checksum: XOR of len, cmd0, cmd1, data bytes
            uint8_t computed = cmd0_ ^ cmd1_ ^ length_;
            for (const uint8_t x : dataAccum_) computed ^= x;

            if (computed == b) {
                Frame f;
                f.type      = static_cast<CommandType>(cmd0_ & 0xE0u);
                f.subsystem = static_cast<Subsystem>  (cmd0_ & 0x1Fu);
                f.command   = cmd1_;
                f.data      = std::move(dataAccum_);
                frames_.push_back(std::move(f));
                dataAccum_.clear();
            }
            // Whether valid or not, return to resynchronisation state
            state_ = State::WaitSOF;
            break;
        }
        } // switch
    }
}

std::optional<Frame> FrameParser::nextFrame() {
    if (frames_.empty()) return std::nullopt;
    Frame f = std::move(frames_.front());
    frames_.erase(frames_.begin());
    return f;
}

} // namespace zigbee::znp
