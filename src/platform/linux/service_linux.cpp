// src/platform/linux/service_linux.cpp

#include "core/service.h"

#include <spdlog/spdlog.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef HAVE_SYSTEMD
#include <systemd/sd-daemon.h>
#endif

namespace axiom::core {

namespace {

std::atomic<bool> g_stop{false};

extern "C" void on_signal(int /*sig*/) {
    g_stop.store(true, std::memory_order_relaxed);
}

// ----------------------------------------------------------------
// Helpers for install / uninstall.
// ----------------------------------------------------------------

bool is_safe_service_name(const std::string& name) {
    if (name.empty()) return false;
    for (char c : name) {
        const bool ok =
            (c >= 'a' && c <= 'z') ||
            (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') ||
            c == '.' || c == '_' || c == '-';
        if (!ok) return false;
    }
    return true;
}

// Run a program with an argument vector — no shell involved.
// Returns true if the program exited with status 0.
bool run_program(const std::string& program,
                 const std::vector<std::string>& args) {
    std::vector<char*> argv;
    argv.reserve(args.size() + 2);
    argv.push_back(const_cast<char*>(program.c_str()));
    for (const auto& a : args) {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);

    const pid_t pid = ::fork();
    if (pid < 0) {
        SPDLOG_WARN("fork failed for {}", program);
        return false;
    }

    if (pid == 0) {
        const int devnull = ::open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            ::dup2(devnull, STDOUT_FILENO);
            ::dup2(devnull, STDERR_FILENO);
            ::close(devnull);
        }
        ::execvp(program.c_str(), argv.data());
        ::_exit(127);
    }

    int status = 0;
    if (::waitpid(pid, &status, 0) < 0) {
        SPDLOG_WARN("waitpid failed for {}", program);
        return false;
    }
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

} // namespace

// ----------------------------------------------------------------
// The Linux service implementation.
// ----------------------------------------------------------------

class LinuxService final : public IService {
public:
    explicit LinuxService(std::string name) : name_(std::move(name)) {}

    int run() override {
        struct sigaction sa{};
        sa.sa_handler = on_signal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        ::sigaction(SIGTERM, &sa, nullptr);
        ::sigaction(SIGINT,  &sa, nullptr);
        std::signal(SIGHUP, SIG_IGN);

        SPDLOG_INFO("{} starting (Linux service mode)", name_);

#ifdef HAVE_SYSTEMD
        sd_notify(0, "READY=1");
        SPDLOG_INFO("sd_notify READY sent");
#endif

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

// ----------------------------------------------------------------
// Factory + platform metadata.
// ----------------------------------------------------------------

std::unique_ptr<IService> make_service(const std::string& service_name) {
    return std::make_unique<LinuxService>(service_name);
}

std::string default_service_name() {
    return "axiomdlp-agent";
}

// ----------------------------------------------------------------
// Install.
//
// Full sequence:
//   1. Write the unit file.
//   2. systemctl daemon-reload   (make systemd notice it)
//   3. systemctl enable --now    (enable at boot + start right now)
// ----------------------------------------------------------------

int install_service(const std::string& binary_path,
                    const std::string& service_name) {
    if (!is_safe_service_name(service_name)) {
        SPDLOG_ERROR("unsafe service name: '{}'", service_name);
        return 1;
    }

    const std::string unit_path =
        "/etc/systemd/system/" + service_name + ".service";

    // 1. Write the unit file.
    {
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
    }

    // 2. Reload systemd so it picks up the new unit.
    if (run_program("systemctl", {"daemon-reload"})) {
        SPDLOG_INFO("reloaded systemd configuration");
    } else {
        SPDLOG_WARN("systemctl daemon-reload failed");
    }

    // 3. Enable at boot and start now.
    if (run_program("systemctl", {"enable", "--now", service_name})) {
        SPDLOG_INFO("enabled and started service {}", service_name);
    } else {
        SPDLOG_WARN("systemctl enable --now failed for {}; "
                    "run manually: systemctl enable --now {}",
                    service_name, service_name);
    }

    return 0;
}

// ----------------------------------------------------------------
// Uninstall.
// ----------------------------------------------------------------

int uninstall_service(const std::string& service_name) {
    if (!is_safe_service_name(service_name)) {
        SPDLOG_ERROR("unsafe service name: '{}'", service_name);
        return 1;
    }

    SPDLOG_INFO("stopping service {}", service_name);
    if (run_program("systemctl", {"stop", service_name})) {
        SPDLOG_INFO("  stop: ok");
    } else {
        SPDLOG_INFO("  stop: failed or service was not running");
    }

    SPDLOG_INFO("disabling service {}", service_name);
    if (run_program("systemctl", {"disable", service_name})) {
        SPDLOG_INFO("  disable: ok");
    } else {
        SPDLOG_INFO("  disable: failed or service was not enabled");
    }

    const std::string unit_path =
        "/etc/systemd/system/" + service_name + ".service";

    errno = 0;
    if (std::remove(unit_path.c_str()) != 0) {
        if (errno != ENOENT) {
            SPDLOG_ERROR("failed to remove {}: {}",
                         unit_path, std::strerror(errno));
            return 1;
        }
        SPDLOG_INFO("unit file already absent: {}", unit_path);
    } else {
        SPDLOG_INFO("removed unit file: {}", unit_path);
    }

    if (run_program("systemctl", {"daemon-reload"})) {
        SPDLOG_INFO("reloaded systemd configuration");
    } else {
        SPDLOG_WARN("systemctl daemon-reload failed");
    }

    SPDLOG_INFO("uninstalled service {}", service_name);
    return 0;
}

} // namespace axiom::core