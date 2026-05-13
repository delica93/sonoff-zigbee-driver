#ifndef ZIGBEE_DONGLE_HPP
#define ZIGBEE_DONGLE_HPP

#include "zstack_protocol.hpp"
#include "serial_communication.hpp"
#include "device_manager.hpp"
#include <memory>
#include <functional>
#include <map>

namespace zigbee {

// Event types for device callbacks
enum class EventType {
    DEVICE_JOINED,
    DEVICE_LEFT,
    MESSAGE_RECEIVED,
    NETWORK_STARTED,
    NETWORK_STOPPED,
    PERMIT_JOINING_ENABLED,
    PERMIT_JOINING_DISABLED,
    ERROR
};

struct DeviceEvent {
    EventType type;
    uint16_t device_address;
    std::vector<uint8_t> data;
    std::string error_message;
};

class ZigbeeDongle {
public:
    using EventCallback = std::function<void(const DeviceEvent&)>;

    explicit ZigbeeDongle(const std::string& port = "/dev/ttyUSB0");
    ~ZigbeeDongle();

    // Initialization and network management
    bool initialize();
    bool shutdown();
    bool startNetwork();
    bool stopNetwork();
    bool permitJoining(uint8_t duration_sec = 120);

    // Device management
    bool addDevice(uint16_t address, uint8_t endpoint = 1);
    bool removeDevice(uint16_t address);
    const DeviceManager& getDeviceManager() const { return device_manager_; }

    // Communication
    bool sendMessage(uint16_t dest_addr, uint8_t dest_endpoint,
                     const std::vector<uint8_t>& data,
                     uint8_t radius = 30);
    bool sendCommand(uint16_t dest_addr, uint8_t cluster_id,
                    const std::vector<uint8_t>& command_data);

    // Event handling
    void setEventCallback(EventCallback callback) { event_callback_ = callback; }

    // Status
    bool isNetworkActive() const { return network_active_; }
    std::string getDeviceVersion() const { return device_version_; }

private:
    std::unique_ptr<serial::SerialPort> serial_port_;
    DeviceManager device_manager_;
    EventCallback event_callback_;
    bool network_active_;
    std::string device_version_;

    // Message handling
    void handleAsyncMessage(const zstack::Frame& frame);
    void handleIncomingMessage(const std::vector<uint8_t>& data);
    void handleDeviceAnnounce(const std::vector<uint8_t>& data);

    // Command wrappers
    bool sendSysCommand(zstack::SysCommand cmd, const std::vector<uint8_t>& data = {});
    bool sendAFCommand(zstack::AFCommand cmd, const std::vector<uint8_t>& data);
    bool sendZDOCommand(zstack::ZDOCommand cmd, const std::vector<uint8_t>& data);

    // Protocol helpers
    zstack::Frame buildFrame(zstack::FrameType type, zstack::CommandSubsystem subsys,
                            uint8_t cmd, const std::vector<uint8_t>& data = {});
};

}  // namespace zigbee

#endif  // ZIGBEE_DONGLE_HPP