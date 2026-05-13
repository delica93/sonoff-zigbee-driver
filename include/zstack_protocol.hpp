#ifndef ZSTACK_PROTOCOL_HPP
#define ZSTACK_PROTOCOL_HPP

#include <cstdint>
#include <vector>
#include <memory>
#include <array>

namespace zstack {

// Z-Stack Frame Structure
// SOF (1) | Length (1) | Type (1) | CMD (1) | Data (0-250) | FCS (1)

// Frame type bits
enum class FrameType : uint8_t {
    SREQ = 0x20,  // Synchronous request
    SRSP = 0x60,  // Synchronous response
    AREQ = 0x40,  // Asynchronous request
    ARSP = 0x80   // Asynchronous response
};

// Command subsystems (upper 5 bits of CMD)
enum class CommandSubsystem : uint8_t {
    SYS = 0x41,      // System
    AF = 0x04,       // Application Framework
    ZDO = 0x25,      // Zigbee Device Object
    SAPI = 0x06,     // Simple API
    NVRAM = 0x08,    // Non-volatile RAM
    DEBUG = 0x0F,    // Debug
    UTIL = 0x27      // Utilities
};

// System commands
enum class SysCommand : uint8_t {
    RESET_REQ = 0x09,
    PING_REQ = 0x01,
    VERSION_REQ = 0x02,
    SET_EXTADDR_REQ = 0x03,
    GET_EXTADDR_REQ = 0x04
};

// AF commands
enum class AFCommand : uint8_t {
    REGISTER_REQ = 0x00,
    DATA_REQUEST = 0x03,
    DATA_SRSP = 0x83,
    INCOMING_MSG = 0x87
};

// ZDO commands
enum class ZDOCommand : uint8_t {
    START_REQ = 0x00,
    PERMIT_JOINING_REQ = 0x36,
    MATCH_DESC_REQ = 0x06
};

// Frame class for Z-Stack protocol
class Frame {
public:
    static constexpr uint8_t SOF = 0xFE;  // Start of frame
    static constexpr size_t MAX_DATA_SIZE = 250;
    static constexpr size_t FRAME_OVERHEAD = 4;  // SOF + Length + Type + CMD + FCS

    Frame();
    explicit Frame(FrameType type, uint8_t cmd, const std::vector<uint8_t>& data = {});

    // Serialization
    std::vector<uint8_t> serialize() const;
    bool deserialize(const std::vector<uint8_t>& buffer);

    // Getters
    FrameType getType() const { return type_; }
    uint8_t getCommand() const { return cmd_; }
    const std::vector<uint8_t>& getData() const { return data_; }
    uint8_t getChecksum() const { return checksum_; }

    // Setters
    void setType(FrameType type) { type_ = type; }
    void setCommand(uint8_t cmd) { cmd_ = cmd; }
    void setData(const std::vector<uint8_t>& data) { data_ = data; }

    // Validation
    bool isValid() const;

private:
    FrameType type_;
    uint8_t cmd_;
    std::vector<uint8_t> data_;
    uint8_t checksum_;

    uint8_t calculateChecksum() const;
};

// Protocol constants
struct ZStackConstants {
    static constexpr uint32_t DEFAULT_BAUD_RATE = 115200;
    static constexpr uint8_t DEFAULT_FLOW_CONTROL = 1;
    static constexpr uint32_t FRAME_TIMEOUT_MS = 5000;
};

// Helper functions
uint8_t buildCommandByte(CommandSubsystem subsys, uint8_t cmd);
std::pair<CommandSubsystem, uint8_t> parseCommandByte(uint8_t cmd);

}  // namespace zstack

#endif  // ZSTACK_PROTOCOL_HPP