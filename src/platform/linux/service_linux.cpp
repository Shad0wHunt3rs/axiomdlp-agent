// src/platform/linux/service_linux.cpp

#include "core/service.h"
#include "core/logger.h"

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <fstream>
#include <thread>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

namespace axiom::core {

namespace {

std::atomic<bool> g_stop{false};

extern "C" void on_signal(int /*sig*/) {
    g_stop.store(true, std::memory_order_relaxed);
}

} // namespace

class LinuxService final : public IService {
public:
    explicit LinuxService(std::string name) : name_(std::move(name)) {}

    int run() override {
        // 1. Signal handlers.
        struct sigaction sa{};
        sa.sa_handler = on_signal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        ::sigaction(SIGTERM, &sa, nullptr);
        ::sigaction(SIGINT,  &sa, nullptr);
        std::signal(SIGHUP, SIG_IGN);

        SPDLOG_INFO("{} starting (Linux service mode)", name_);

        // 2. Tell systemd we are ready (requires Type=notify).
#ifdef HAVE_SYSTEMD
        sd_notify(0, "READY=1");
        SPDLOG_INFO("sd_notify READY sent");
#endif

        // 3. Main loop. Real agent work goes here in later milestones.
        while (!g_stop.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        SPDLOG_INFO("{} received stop request", name_);

#ifdef HAVE_SYSTEMD
        sd_notify(0, "STOPPING=1");
#endif

        return 0;
    }

    void request_stop() override {
        g_stop.store(true, std::memory_order_relaxed);
    }

private:
    std::string name_;
};

std::unique_ptr<IService> make_service(const std::string& service_name) {
    return std::make_unique<LinuxService>(service_name);
}

std::string default_service_name() {
    return "axiomdlp-agent";
}

int install_service(const std::string& binary_path,
                    const std::string& service_name) {
    const std::string unit_path = "/etc/systemd/system/" + service_name + ".service";

    std::ofstream f(unit_path);
    if (!f.is_open()) {
        SPDLOG_ERROR("cannot write unit file to {} (need root?)", unit_path);
        return 1;
    }

    f << "[Unit]\n"
      << "Description=AxiomDLP Endpoint Agent\n"
      << "After=network.target\n\n"
      << "[Service]\n"
      << "Type=notify\n"
      << "ExecStart=" << binary_path << "\n"
      << "Restart=on-failure\n"
      << "RestartSec=5\n"
      << "StandardOutput=journal\n"
      << "StandardError=journal\n\n"
      << "[Install]\n"
      << "WantedBy=multi-user.target\n";
    f.close();

    SPDLOG_INFO("installed unit file: {}", unit_path);
    SPDLOG_INFO("next: systemctl daemon-reload && systemctl enable --now {}",
                service_name);
    return 0;
}

int uninstall_service(const std::string& service_name) {
    const std::string unit_path = "/etc/systemd/system/" + service_name + ".service";
    if (std::remove(unit_path.c_str()) != 0) {
        SPDLOG_ERROR("failed to remove {}", unit_path);
        return 1;
    }
    SPDLOG_INFO("removed {}", unit_path);
    SPDLOG_INFO("next: systemctl daemon-reload");
    return 0;
}

} // namespace axiom::core