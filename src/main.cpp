// ============================================================
// ProxyMan - Network-Aware Transparent Proxy Engine for Windows
// Entry point: WinMain (headless) / main (CLI)
// ============================================================

#include "config/config.h"
#include "config/pool.h"
#include "core/engine.h"
#include "net/network_watcher.h"
#include "platform/proxy_settings.h"
#include "platform/elevation.h"
#include "platform/ipc.h"
#include "platform/console.h"
#include "cli/parser.h"

#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstdlib>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

// Global shutdown flag (defined here, declared extern in platform/console.h)
std::atomic<bool> g_shutdownRequested{false};

static int real_main(int argc, char* argv[]) {
    if (argc > 1) {
        EnsureConsoleOutput();
    }

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Load configuration
    Config cfg;
    std::string defaultConfigPath = GetDefaultConfigPath();
    bool loaded = LoadConfigFromFile(defaultConfigPath, cfg);
    if (!loaded) {
        loaded = LoadConfigFromFile("config.toml", cfg);
    }

    // Handle non-elevated CLI commands (--help, --status, --logs, --stop, --stats)
    int cliResult = ParseAndDispatch(argc, argv, cfg);
    if (cliResult != -1) return cliResult;

    // Elevate if needed
    if (!IsElevated()) {
        std::cout << "[Main] Administrator privileges required.\n";
        std::cout << "[Main] Requesting UAC elevation...\n";
        if (RelaunchElevated(argc, argv)) {
            return 0;
        } else {
            std::cerr << "[Main] Elevation request failed or was cancelled by user. Exiting.\n";
            return 1;
        }
    }

    // Handle elevated CLI commands (--install-startup, --speedtest, etc.)
    int elevResult = ParseElevatedCommands(argc, argv, cfg, defaultConfigPath);
    if (elevResult != -1) return elevResult;

    // ── Engine Startup ──────────────────────────────────────
    HANDLE hMutex = CreateGlobalMutex();
    if (hMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        std::cerr << "[Main] Another instance of ProxyMan is already running. Exiting.\n";
        if (hMutex != NULL) CloseHandle(hMutex);
        return 0;
    }

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        CloseHandle(hMutex);
        return 1;
    }

    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    if (!loaded) {
        if (!PromptAndSaveConfig(defaultConfigPath, cfg)) {
            std::cerr << "Failed to initialize configuration." << std::endl;
            WSACleanup();
            return 1;
        }
    }

    // Auto-select optimal proxy from pool
    ProxySelectionResult poolResult = SelectOptimalProxy(cfg);
    if (poolResult.found) {
        cfg.proxyIp = poolResult.ip;
        cfg.proxyPort = poolResult.port;
    }

    std::printf("==========================================================\n");
    std::printf("  ProxyMan - Network-Aware Transparent Proxy Engine\n");
    std::printf("==========================================================\n");
    std::printf("  Proxy:  %s:%u  (user: %s)\n", cfg.proxyIp.c_str(), cfg.proxyPort, cfg.proxyUser.c_str());
    std::printf("  Relay:  127.0.0.1:%u\n", cfg.relayPort);
    std::printf("  Mode:   Auto (Ethernet=ON, Wi-Fi=OFF)\n");
    std::printf("==========================================================\n\n");

    EngineController controller;

    // Detect initial network state
    LinkType initialLink = GetActiveLinkType();
    std::printf("[Main] Initial network: %s\n", LinkTypeToString(initialLink).c_str());

    if (initialLink == LinkType::Ethernet) {
        std::printf("[Main] Ethernet detected - starting engine and setting system proxy.\n");
        std::string proxyStr = "127.0.0.1:" + std::to_string(cfg.relayPort);
        SetSystemProxy(proxyStr);
        controller.StartEngine(cfg);
        ShowNotificationToast("⚡ ProxyMan", "Connected to MNNIT Ethernet (" + cfg.proxyIp + ":" + std::to_string(cfg.proxyPort) + ")");
    } else {
        std::printf("[Main] Not on Ethernet - engine idle, waiting for network change.\n");
        ClearSystemProxy();
        ShowNotificationToast("📶 ProxyMan", "Switched to Wi-Fi / Hotspot (Proxy Bypassed)");
    }

    // Watch for network changes
    NetworkWatcher watcher([&controller, &cfg](LinkType newType) {
        std::printf("[Main] Network changed -> %s\n", LinkTypeToString(newType).c_str());

        if (newType == LinkType::Ethernet) {
            if (!controller.IsRunning()) {
                std::printf("[Main] Ethernet connected - starting engine and setting system proxy.\n");
                std::string proxyStr = "127.0.0.1:" + std::to_string(cfg.relayPort);
                SetSystemProxy(proxyStr);
                controller.StartEngine(cfg);
                ShowNotificationToast("⚡ ProxyMan", "Connected to MNNIT Ethernet (" + cfg.proxyIp + ":" + std::to_string(cfg.proxyPort) + ")");
            }
        } else {
            if (controller.IsRunning()) {
                std::printf("[Main] Ethernet disconnected - stopping engine and clearing system proxy.\n");
                controller.StopEngine();
                ClearSystemProxy();
                ShowNotificationToast("📶 ProxyMan", "Switched to Wi-Fi / Hotspot (Proxy Bypassed)");
            }
        }
    });

    HANDLE hStopEvent = CreateGlobalShutdownEvent();

    std::printf("\n[Main] ProxyMan is running. Press Ctrl+C or run 'ProxyMan --stop' to exit.\n\n");

    // Block until shutdown
    while (!g_shutdownRequested.load()) {
        if (hStopEvent != NULL && WaitForSingleObject(hStopEvent, 200) == WAIT_OBJECT_0) {
            std::printf("\n[Main] Received cross-process stop signal (--stop)...\n");
            break;
        }
    }

    if (hStopEvent != NULL) CloseHandle(hStopEvent);

    // Clean shutdown
    std::printf("[Main] Shutting down...\n");
    if (controller.IsRunning()) {
        controller.StopEngine();
    }
    ClearSystemProxy();
    std::printf("[Main] System proxy cleared. Goodbye.\n");

    WSACleanup();
    CloseHandle(hMutex);
    return 0;
}

int main(int argc, char* argv[]) {
    return real_main(argc, argv);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int argc = 0;
    LPWSTR* argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    std::vector<std::string> argsStorage;
    std::vector<char*> argvVec;

    for (int i = 0; i < argc; ++i) {
        int len = WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, NULL, 0, NULL, NULL);
        if (len > 0) {
            std::string s(len - 1, 0);
            WideCharToMultiByte(CP_UTF8, 0, argvW[i], -1, &s[0], len, NULL, NULL);
            argsStorage.push_back(s);
        } else {
            argsStorage.push_back("");
        }
    }
    for (auto& s : argsStorage) {
        argvVec.push_back(s.data());
    }
    if (argvW) LocalFree(argvW);

    return real_main(static_cast<int>(argvVec.size()), argvVec.data());
}
