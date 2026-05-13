#include <zigbee/ZigbeeDongle.hpp>

#include <atomic>
#include <chrono>
#include <csignal>
#include <format>
#include <print>
#include <thread>

// ─── Graceful Ctrl-C handling ─────────────────────────────────────────────────

static std::atomic<bool> g_stop{ false };

extern "C" void sigHandler(int) { g_stop.store(true); }

// ─── Event printer ────────────────────────────────────────────────────────────

void onEvent(const zigbee::ZigbeeEvent& event) {
    std::visit([](const auto& ev) {
        using T = std::decay_t<decltype(ev)>;

        if constexpr (std::is_same_v<T, zigbee::DongleConnectedEvent>) {
            std::println("[event] Dongle connected on {}", ev.portName);

        } else if constexpr (std::is_same_v<T, zigbee::DongleDisconnectedEvent>) {
            std::println("[event] Dongle disconnected: {}", ev.reason);

        } else if constexpr (std::is_same_v<T, zigbee::CoordinatorStateChangedEvent>) {
            std::println("[event] Coordinator state → 0x{:02X}", ev.newState);

        } else if constexpr (std::is_same_v<T, zigbee::DeviceJoinedEvent>) {
            std::println("[event] Device joined  addr=0x{:04X}  eui64={:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}  cap=0x{:02X}",
                ev.shortAddr,
                ev.eui64[7], ev.eui64[6], ev.eui64[5], ev.eui64[4],
                ev.eui64[3], ev.eui64[2], ev.eui64[1], ev.eui64[0],
                ev.capabilities);

        } else if constexpr (std::is_same_v<T, zigbee::DeviceLeftEvent>) {
            std::println("[event] Device left    addr=0x{:04X}  rejoin={}",
                ev.shortAddr, ev.rejoin);

        } else if constexpr (std::is_same_v<T, zigbee::MessageReceivedEvent>) {
            std::println("[event] Message        from=0x{:04X}  cluster=0x{:04X}  ep={}/{}  lqi={}  len={}",
                ev.srcAddr, ev.clusterId,
                ev.srcEndpoint, ev.dstEndpoint,
                ev.linkQuality,
                ev.payload.size());
        }
    }, event);
}

// ─── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    // Default port; override with the first command-line argument
    const std::string portName = (argc > 1) ? argv[1] : "COM3";

    std::signal(SIGINT,  sigHandler);
    std::signal(SIGTERM, sigHandler);

    std::println("Sonoff Zigbee 3.0 USB Dongle Plus driver — connecting on {}…", portName);

    zigbee::ZigbeeDongle dongle;
    dongle.setEventCallback(onEvent);

    auto result = dongle.connect({
        .portName        = portName,
        .baudRate        = 115200,
        .channel         = 15,
        .panId           = 0x1A62,
        .permitJoinOnStart = false,
        .postResetDelayMs  = 1500,
        .startupTimeoutMs  = 10000,
    });

    if (!result) {
        std::println(stderr, "Failed to connect: {}",
                     zigbee::errorToString(result.error()));
        return 1;
    }

    std::println("Coordinator is up.  Enabling permit-join for 60 seconds…");
    std::ignore = dongle.setPermitJoin(60);

    // ── Main loop ─────────────────────────────────────────────────────────────
    using namespace std::chrono_literals;
    while (!g_stop.load()) {
        std::this_thread::sleep_for(500ms);
    }

    // ── Shutdown ──────────────────────────────────────────────────────────────
    std::println("\nShutting down…");

    const auto devs = dongle.devices();
    std::println("Known devices ({}):", devs.size());
    for (const auto& dev : devs) {
        std::println("  {} @ 0x{:04X}  {}",
            dev->eui64ToString(),
            dev->shortAddr(),
            dev->isRouterCapable() ? "router" : "end-device");
    }

    dongle.disconnect();
    return 0;
}
