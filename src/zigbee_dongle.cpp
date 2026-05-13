#include "zigbee_dongle.hpp"
#include <iostream>

namespace zigbee {

ZigbeeDongle::ZigbeeDongle(const std::string& port)
    : serial_port_(std::make_unique<serial::SerialPort>(port)),
      network_active_(false) {}

ZigbeeDongle::~ZigbeeDongle() {
    shutdown();
}

bool ZigbeeDongle::initialize() {
    if (!serial_port_->open(115200)) {
        std::cerr << "Failed to open serial port" << std::endl;
        return false;
    }

    serial_port_->setDataCallback([this](const std::vector<uint8_t>& data) {
        // Parse incoming frames
        zstack::Frame frame;
        if (frame.deserialize(data)) {
            handleAsyncMessage(frame);
        }
    });

    serial_port_->startReading();
    return true;
}

bool ZigbeeDongle::shutdown() {
    if (serial_port_->isOpen()) {
        serial_port_->close();
    }
    device_manager_.clearAll();
    network_active_ = false;
    return true;
}

bool ZigbeeDongle::startNetwork() {
    if (!serial_port_->isOpen()) {
        return false;
    }

    std::vector<uint8_t> data = {0x00};
    zstack::Frame frame(zstack::FrameType::SREQ, 0x00, data);
    
    if (!serial_port_->write(frame.serialize())) {
        return false;
    }

    network_active_ = true;

    if (event_callback_) {
        DeviceEvent event;
        event.type = EventType::NETWORK_STARTED;
        event_callback_(event);
    }

    return true;
}

bool ZigbeeDongle::stopNetwork() {
    network_active_ = false;

    if (event_callback_) {
        DeviceEvent event;
        event.type = EventType::NETWORK_STOPPED;
        event_callback_(event);
    }

    return true;
}

bool ZigbeeDongle::permitJoining(uint8_t duration_sec) {
    if (!serial_port_->isOpen()) {
        return false;
    }

    std::vector<uint8_t> data = {0xFF, 0xFF, duration_sec};
    zstack::Frame frame(zstack::FrameType::SREQ, 0x36, data);
    
    if (!serial_port_->write(frame.serialize())) {
        return false;
    }

    if (event_callback_) {
        DeviceEvent event;
        event.type = EventType::PERMIT_JOINING_ENABLED;
        event_callback_(event);
    }

    return true;
}

bool ZigbeeDongle::addDevice(uint16_t address, uint8_t endpoint) {
    ZigbeeDevice device(address, endpoint);
    device.is_active = true;
    return device_manager_.addDevice(device);
}

bool ZigbeeDongle::removeDevice(uint16_t address) {
    return device_manager_.removeDevice(address);
}

bool ZigbeeDongle::sendMessage(uint16_t dest_addr, uint8_t dest_endpoint,
                                const std::vector<uint8_t>& data,
                                uint8_t radius) {
    if (!serial_port_->isOpen()) {
        return false;
    }

    // Build AF_DATA_REQUEST frame
    std::vector<uint8_t> payload;
    payload.push_back((dest_addr >> 8) & 0xFF);
    payload.push_back(dest_addr & 0xFF);
    payload.push_back(dest_endpoint);
    payload.insert(payload.end(), data.begin(), data.end());

    zstack::Frame frame(zstack::FrameType::SREQ, 0x03, payload);
    return serial_port_->write(frame.serialize());
}

void ZigbeeDongle::handleAsyncMessage(const zstack::Frame& frame) {
    uint8_t cmd = frame.getCommand();

    if (cmd == 0x87) {  // AF_INCOMING_MSG
        handleIncomingMessage(frame.getData());
    }
}

void ZigbeeDongle::handleIncomingMessage(const std::vector<uint8_t>& data) {
    if (data.size() < 4) {
        return;
    }

    // Parse device address from incoming message
    uint16_t device_addr = (data[0] << 8) | data[1];

    if (event_callback_) {
        DeviceEvent event;
        event.type = EventType::MESSAGE_RECEIVED;
        event.device_address = device_addr;
        if (data.size() > 4) {
            event.data.assign(data.begin() + 4, data.end());
        }
        event_callback_(event);
    }
}

}  // namespace zigbee
