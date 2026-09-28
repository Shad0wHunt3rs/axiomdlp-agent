// src/platform/windows/service_win.cpp

#include "platform/service.h"
#include "modules/module_manager.h"

#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <thread>

#include <windows.h>

namespace axiom::core {

namespace {

std::atomic<bool> g_stop{false};
SERVICE_STATUS_HANDLE g_status_handle = nullptr;
SERVICE_STATUS        g_status{};
std::string           g_name;

ModuleManager* g_module_manager = nullptr;

void report(DWORD state, DWORD exit_code = 0, DWORD wait_hint = 0) {
    g_status.dwCurrentState  = state;
    g_status.dwWin32ExitCode = exit_code;
    g_status.dwWaitHint      = wait_hint;
    if (g_status_handle) {
        SetServiceStatus(g_status_handle, &g_status);
    }
}

DWORD WINAPI ctrl_handler(DWORD control, DWORD, LPVOID, LPVOID) {
    switch (control) {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        report(SERVICE_STOP_PENDING, 0, 3000);
        g_stop.store(true, std::memory_order_relaxed);
        return NO_ERROR;
    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;
    default:
        return ERROR_CALL_NOT_IMPLEMENTED;
    }
}

void WINAPI service_main(DWORD, LPSTR*) {
    g_status_handle = RegisterServiceCtrlHandlerExA(g_name.c_str(), ctrl_handler, nullptr);
    if (!g_status_handle) {
        return;
    }

    g_status.dwServiceType      = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    report(SERVICE_START_PENDING, 0, 3000);

    // Start modules before reporting running.
    if (g_module_manager) {
        g_module_manager->start_all();
    }

    report(SERVICE_RUNNING);

    SPDLOG_INFO("{} running (Windows service mode)", g_name);

    while (!g_stop.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    SPDLOG_INFO("{} stopping (Windows service mode)", g_name);

    // Stop modules before reporting stopped.
    if (g_module_manager) {
        g_module_manager->stop_all();
        g_module_manager->shutdown_all();
    }

    report(SERVICE_STOPPED);
}

} // namespace

// ----------------------------------------------------------------
// The Windows service implementation.
// ----------------------------------------------------------------

class WindowsService final : public IService {
public:
    explicit WindowsService(std::string name) : name_(std::move(name)) {}

    void set_module_manager(ModuleManager* mgr) override {
        g_module_manager = mgr;
    }

    int run() override {
        g_name = name_;
        g_stop.store(false, std::memory_order_relaxed);

        SERVICE_TABLE_ENTRYA table[] = {
            { const_cast<LPSTR>(g_name.c_str()), service_main },
            { nullptr, nullptr }
        };

        if (!StartServiceCtrlDispatcherA(table)) {
            DWORD err = GetLastError();
            if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
                SPDLOG_INFO("not launched by SCM; running in foreground");
                return run_foreground();
            }
            SPDLOG_ERROR("StartServiceCtrlDispatcher failed: {}", err);
            return 1;
        }
        return 0;
    }

    void request_stop() override {
        g_stop.store(true, std::memory_order_relaxed);
    }

private:
    int run_foreground() {
        if (g_module_manager) {
            g_module_manager->start_all();
        }

        SPDLOG_INFO("{} running (Windows foreground mode)", name_);

        while (!g_stop.load(std::memory_order_relaxed)) {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        if (g_module_manager) {
            g_module_manager->stop_all();
            g_module_manager->shutdown_all();
        }

        SPDLOG_INFO("{} stopping (foreground)", name_);
        return 0;
    }

    std::string name_;
};

// ----------------------------------------------------------------
// Factory + platform metadata.
// ----------------------------------------------------------------

std::unique_ptr<IService> make_service(const std::string& service_name) {
    return std::make_unique<WindowsService>(service_name);
}

std::string default_service_name() {
    return "AxiomDLPAgent";
}

// ----------------------------------------------------------------
// Install.
// ----------------------------------------------------------------

int install_service(const std::string& binary_path,
                    const std::string& service_name) {
    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!scm) {
        SPDLOG_ERROR("OpenSCManager failed: {}", GetLastError());
        return 1;
    }

    const std::string quoted = "\"" + binary_path + "\"";

    SC_HANDLE svc = CreateServiceA(
        scm, service_name.c_str(), service_name.c_str(),
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START, SERVICE_ERROR_NORMAL,
        quoted.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr);

    if (!svc) {
        const DWORD err = GetLastError();
        if (err == ERROR_SERVICE_EXISTS) {
            SPDLOG_WARN("service {} already installed", service_name);
            svc = OpenServiceA(scm, service_name.c_str(), SERVICE_START | SERVICE_QUERY_STATUS);
            if (!svc) {
                CloseServiceHandle(scm);
                return 1;
            }
        } else {
            SPDLOG_ERROR("CreateService failed: {}", err);
            CloseServiceHandle(scm);
            return 1;
        }
    } else {
        SPDLOG_INFO("service {} installed", service_name);
    }

    SERVICE_STATUS status{};
    if (!QueryServiceStatus(svc, &status)) {
        SPDLOG_WARN("QueryServiceStatus failed: {}", GetLastError());
    } else if (status.dwCurrentState == SERVICE_RUNNING) {
        SPDLOG_INFO("service {} already running", service_name);
    } else {
        if (StartServiceA(svc, 0, nullptr)) {
            SPDLOG_INFO("started service {}", service_name);
        } else {
            const DWORD err = GetLastError();
            if (err == ERROR_SERVICE_ALREADY_RUNNING) {
                SPDLOG_INFO("service {} already running", service_name);
            } else {
                SPDLOG_WARN("StartService failed (err={}); "
                            "start manually with: sc start {}",
                            err, service_name);
            }
        }
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return 0;
}

// ----------------------------------------------------------------
// Uninstall.
// ----------------------------------------------------------------

int uninstall_service(const std::string& service_name) {
    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) {
        SPDLOG_ERROR("OpenSCManager failed: {}", GetLastError());
        return 1;
    }

    SC_HANDLE svc = OpenServiceA(
        scm, service_name.c_str(),
        SERVICE_STOP | SERVICE_QUERY_STATUS | DELETE);

    if (!svc) {
        const DWORD err = GetLastError();
        if (err == ERROR_SERVICE_DOES_NOT_EXIST) {
            SPDLOG_INFO("service {} not installed; nothing to do", service_name);
            CloseServiceHandle(scm);
            return 0;
        }
        SPDLOG_ERROR("OpenService failed: {}", err);
        CloseServiceHandle(scm);
        return 1;
    }

    SERVICE_STATUS status{};
    if (ControlService(svc, SERVICE_CONTROL_STOP, &status)) {
        SPDLOG_INFO("stop requested for {}", service_name);

        for (int i = 0; i < 100; ++i) {
            if (!QueryServiceStatus(svc, &status)) break;
            if (status.dwCurrentState == SERVICE_STOPPED) {
                SPDLOG_INFO("service {} stopped", service_name);
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } else {
        const DWORD err = GetLastError();
        if (err == ERROR_SERVICE_NOT_ACTIVE) {
            SPDLOG_INFO("service {} was not running", service_name);
        } else {
            SPDLOG_WARN("ControlService failed (err={}); continuing with delete", err);
        }
    }

    if (!DeleteService(svc)) {
        const DWORD err = GetLastError();
        if (err == ERROR_SERVICE_MARKED_FOR_DELETE) {
            SPDLOG_INFO("service {} already marked for deletion", service_name);
        } else {
            SPDLOG_ERROR("DeleteService failed: {}", err);
            CloseServiceHandle(svc);
            CloseServiceHandle(scm);
            return 1;
        }
    } else {
        SPDLOG_INFO("service {} uninstalled", service_name);
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return 0;
}

} // namespace axiom::core