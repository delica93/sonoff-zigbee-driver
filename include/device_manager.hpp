#ifndef DEVICE_MANAGER_HPP
#define DEVICE_MANAGER_HPP

#include <cstdint>
#include <string>
#include <map>
#include <vector>
#include <memory>

namespace zigbee {

struct ZigbeeDevice {
    uint16_t address;
    uint64_t ieee_address;  // Extended address
    uint8_t endpoint;
    std::string model;
    std::string manufacturer;
    std::vector<uint16_t> clusters;
    bool is_active;

    ZigbeeDevice(uint16_t addr, uint8_t ep = 1)
        : address(addr), ieee_address(0), endpoint(ep),
          model(""), manufacturer(""), is_active(false) {}
};

class DeviceManager {
public:
    DeviceManager();
    ~DeviceManager();

    // Device management
    bool addDevice(const ZigbeeDevice& device);
    bool removeDevice(uint16_t address);
    bool updateDevice(uint16_t address, const ZigbeeDevice& device);

    // Device queries
    ZigbeeDevice* getDevice(uint16_t address);
    const ZigbeeDevice* getDevice(uint16_t address) const;
    const std::map<uint16_t, ZigbeeDevice>& getAllDevices() const { return devices_; }

    // Device status
    bool deviceExists(uint16_t address) const;
    size_t getDeviceCount() const { return devices_.size(); }

    // Clear all devices
    void clearAll();

private:
    std::map<uint16_t, ZigbeeDevice> devices_;
};

}  // namespace zigbee

#endif  // DEVICE_MANAGER_HPP