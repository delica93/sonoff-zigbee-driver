#ifndef SERIAL_COMMUNICATION_HPP
#define SERIAL_COMMUNICATION_HPP

#include <string>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <functional>
#include <termios.h>

namespace serial {

class SerialPort {
public:
    using DataCallback = std::function<void(const std::vector<uint8_t>&)>;

    explicit SerialPort(const std::string& port = "/dev/ttyUSB0");
    ~SerialPort();

    // Connection management
    bool open(uint32_t baud_rate = 115200);
    bool close();
    bool isOpen() const { return is_open_; }

    // Data transmission
    bool write(const std::vector<uint8_t>& data);
    std::vector<uint8_t> read(size_t timeout_ms = 1000);

    // Asynchronous reception
    void setDataCallback(DataCallback callback) { data_callback_ = callback; }
    void startReading();
    void stopReading();

    // Port enumeration
    static std::vector<std::string> enumeratePorts();

    // Configuration
    bool setFlowControl(bool enabled);
    bool setBaudRate(uint32_t baud_rate);

private:
    std::string port_name_;
    int port_fd_;
    bool is_open_;
    bool reading_;

    // Receive thread and queue
    std::thread rx_thread_;
    std::queue<std::vector<uint8_t>> rx_queue_;
    std::mutex rx_mutex_;
    std::condition_variable rx_cv_;

    DataCallback data_callback_;

    // Helper methods
    void rxThreadFunction();
    bool configurePort(uint32_t baud_rate, bool flow_control);
    speed_t getBaudRateConstant(uint32_t baud_rate) const;
};

}  // namespace serial

#endif  // SERIAL_COMMUNICATION_HPP