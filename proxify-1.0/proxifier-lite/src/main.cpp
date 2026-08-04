#include "config.h"
#include "engine_controller.h"
#include "network_watcher.h"
#include "proxy_settings.h"
#include "http_proxy_client.h"

#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <string>
#include <atomic>
#include <cstdlib>
#include <vector>
#include <algorithm>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")

static std::atomic<bool> g_shutdownRequested{false};

static BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT || ctrlType == CTRL_CLOSE_EVENT) {
        std::printf("\n[Main] Shutdown signal received...\n");
        g_shutdownRequested = true;
        return TRUE;
    }
    return FALSE;
}

static bool IsElevated() {
    BOOL isElevated = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            isElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return isElevated != FALSE;
}

static bool RelaunchElevated(int argc, char* argv[]) {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) {
        return false;
    }

    std::wstring args;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) args += L" ";
        std::string a = argv[i];
        args += std::wstring(a.begin(), a.end());
    }

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.cbSize = sizeof(sei);
    sei.hwnd = GetConsoleWindow();
    sei.lpVerb = L"runas";
    sei.lpFile = exePath;
    sei.lpParameters = args.c_str();
    sei.nShow = SW_SHOWNORMAL;

    return ShellExecuteExW(&sei) != FALSE;
}

static void PrintHelp() {
    std::cout << "\x1b[1;36m";
    std::cout << "===================================================================\n";
    std::cout << "  ⚡ ProxyMan - Network-Aware Transparent Proxy Engine (v1.0.0)\n";
    std::cout << "===================================================================\n";
    std::cout << "\x1b[0m";
    std::cout << "\x1b[1;32mUSAGE:\x1b[0m\n";
    std::cout << "    ProxyMan.exe [FLAGS] [COMMANDS] [CONFIG_PATH]\n\n";

    std::cout << "\x1b[1;32mINFORMATIONAL & POOL COMMANDS:\x1b[0m\n";
    std::cout << "    \x1b[1;36m-h, --help\x1b[0m               Display this help & usage guide\n";
    std::cout << "    \x1b[1;36m-v, --version\x1b[0m            Display ProxyMan version details\n";
    std::cout << "    \x1b[1;36m--speedtest\x1b[0m              Run MNNIT proxy speed & latency benchmark\n";
    std::cout << "    \x1b[1;36m--check-proxies\x1b[0m          Test health & latency across MNNIT proxy pool\n";
    std::cout << "    \x1b[1;36m--set-user <u0> <p0>\x1b[0m     Update proxy credentials in config\n\n";

    std::cout << "\x1b[1;32mAUTOSTART MANAGEMENT:\x1b[0m\n";
    std::cout << "    \x1b[1;36m--install-startup\x1b[0m        Install Task Scheduler autostart (Zero UAC Prompts)\n";
    std::cout << "    \x1b[1;36m--uninstall-startup\x1b[0m      Remove Task Scheduler autostart\n";
    std::cout << "    \x1b[1;36m--install-service\x1b[0m        Install Windows System Service (Starts at Boot)\n";
    std::cout << "    \x1b[1;36m--uninstall-service\x1b[0m      Remove Windows System Service\n\n";

    std::cout << "\x1b[1;32mCONFIGURATION FILE (TOML):\x1b[0m\n";
    std::cout << "    Default Path: \x1b[1;33m%USERPROFILE%\\.config\\proxyman\\config.toml\x1b[0m\n\n";

    std::cout << "\x1b[1;32mMNNIT DEFAULT PROXY POOL:\x1b[0m\n";
    std::cout << "    172.31.100.25:3128   (Primary Gateway)\n";
    std::cout << "    172.31.100.27:3128   (Secondary Failover)\n";
    std::cout << "    172.31.102.29:3128   (Secondary Failover)\n";
    std::cout << "    172.31.103.29:3128   (Hostel Failover)\n";
    std::cout << "    172.31.100.14:3128   (EDC Failover)\n";
    std::cout << "===================================================================\n";
}

static bool InstallStartupTask() {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) return false;

    std::wstring cmd = L"schtasks /create /tn \"ProxyMan\" /tr \"\\\"";
    cmd += exePath;
    cmd += L"\\\"\" /sc ONLOGON /rl HIGHEST /f";

    int ret = _wsystem(cmd.c_str());
    if (ret == 0) {
        std::cout << "\n==========================================================\n";
        std::cout << "  ProxyMan Task Scheduler Autostart Registered! ✨\n";
        std::cout << "==========================================================\n";
        std::cout << "Method:          Task Scheduler (/sc ONLOGON /rl HIGHEST)\n";
        std::cout << "Execution:       Runs automatically at user logon\n";
        std::cout << "Privileges:      Highest Administrator (No UAC Prompts!)\n\n";
        return true;
    } else {
        std::cerr << "[Main] schtasks failed with error code: " << ret << std::endl;
        return false;
    }
}

static bool UninstallStartupTask() {
    int ret = _wsystem(L"schtasks /delete /tn \"ProxyMan\" /f");
    if (ret == 0) {
        std::cout << "[Main] ProxyMan Task Scheduler autostart removed successfully.\n";
        return true;
    } else {
        std::cerr << "[Main] Failed to remove Task Scheduler autostart.\n";
        return false;
    }
}

static bool InstallWindowsService() {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) return false;

    std::wstring cmd = L"sc.exe create ProxyMan binPath= \"\\\"";
    cmd += exePath;
    cmd += L"\\\"\" start= auto DisplayName= \"ProxyMan Transparent Proxy Engine\"";

    int ret = _wsystem(cmd.c_str());
    if (ret == 0) {
        _wsystem(L"sc.exe description ProxyMan \"ProxyMan Network-Aware Transparent Proxy Engine\"");
        std::cout << "\n==========================================================\n";
        std::cout << "  ProxyMan Windows System Service Registered! 🛡️\n";
        std::cout << "==========================================================\n";
        std::cout << "Method:          Windows System Service (NT AUTHORITY\\SYSTEM)\n";
        std::cout << "Execution:       Starts at System Boot (Before User Logon Screen)\n\n";
        return true;
    } else {
        std::cerr << "[Main] sc.exe create failed with error code: " << ret << std::endl;
        return false;
    }
}

static bool UninstallWindowsService() {
    _wsystem(L"sc.exe stop ProxyMan");
    int ret = _wsystem(L"sc.exe delete ProxyMan");
    if (ret == 0) {
        std::cout << "[Main] ProxyMan Windows System Service removed successfully.\n";
        return true;
    } else {
        std::cerr << "[Main] Failed to remove Windows System Service.\n";
        return false;
    }
}

static void RunSpeedTest(const Config& cfg) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "[SpeedTest] WSAStartup failed." << std::endl;
        return;
    }

    std::cout << "\n\x1b[1;36m===================================================================\x1b[0m\n";
    std::cout << "\x1b[1;36m  ⚡ ProxyMan MNNIT Speedtest & Latency Leaderboard                \x1b[0m\n";
    std::cout << "\x1b[1;36m===================================================================\x1b[0m\n";
    std::cout << "Testing pool with credentials: user=\x1b[1;33m" << cfg.proxyUser << "\x1b[0m\n\n";

    struct TestEntry {
        std::string ip;
        uint16_t port;
        bool isHealthy;
        long latencyMs;
    };
    std::vector<TestEntry> results;

    for (const auto& proxyEntry : cfg.proxyPool) {
        std::string ip = proxyEntry;
        uint16_t port = 3128;
        auto pos = proxyEntry.find(':');
        if (pos != std::string::npos) {
            ip = proxyEntry.substr(0, pos);
            try { port = static_cast<uint16_t>(std::stoi(proxyEntry.substr(pos + 1))); } catch (...) {}
        }

        std::cout << "  Testing " << ip << ":" << port << " ... ";
        std::cout.flush();

        ProxyHealthResult res = TestProxyHealth(ip, port, cfg.proxyUser, cfg.proxyPass, 2000);
        TestEntry entry{ip, port, res.isHealthy, res.latencyMs};
        results.push_back(entry);

        if (res.isHealthy) {
            std::cout << "\x1b[32m✔ ONLINE\x1b[0m (" << res.latencyMs << " ms)\n";
        } else {
            std::cout << "\x1b[31m❌ OFFLINE\x1b[0m\n";
        }
    }

    std::sort(results.begin(), results.end(), [](const TestEntry& a, const TestEntry& b) {
        if (a.isHealthy != b.isHealthy) return a.isHealthy > b.isHealthy;
        return a.latencyMs < b.latencyMs;
    });

    std::cout << "\n\x1b[1;32mLEADERBOARD:\x1b[0m\n";
    int rank = 1;
    for (const auto& r : results) {
        std::cout << "  Rank " << rank++ << ": " << r.ip << ":" << r.port << "  ";
        if (r.isHealthy) {
            std::cout << "\x1b[32m✔ ONLINE\x1b[0m  (" << r.latencyMs << " ms)";
            if (rank == 2) std::cout << " \x1b[1;33m[FASTEST 🚀]\x1b[0m";
        } else {
            std::cout << "\x1b[31m❌ OFFLINE\x1b[0m";
        }
        std::cout << "\n";
    }

    std::cout << "\x1b[1;36m===================================================================\x1b[0m\n\n";

    WSACleanup();
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // Check for --help or -h without requiring elevation first
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help" || arg == "/?") {
            PrintHelp();
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << "ProxyMan v1.0.0 (x64) - Network-Aware Transparent Proxy Engine for Windows\n";
            return 0;
        }
    }

    // Check Administrator privileges - relaunch in new elevated window if not Admin
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

    // Load config
    Config cfg;
    std::string defaultConfigPath = GetDefaultConfigPath();
    bool loaded = false;

    if (argc > 1 && argv[1][0] != '-') {
        loaded = LoadConfigFromFile(argv[1], cfg);
    } else {
        loaded = LoadConfigFromFile(defaultConfigPath, cfg);
        if (!loaded) {
            loaded = LoadConfigFromFile("config.toml", cfg);
            if (!loaded) {
                loaded = LoadConfigFromFile("proxy-config.txt", cfg);
            }
        }
    }

    // Check command line flags for management & pool tools
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--install-startup") {
            InstallStartupTask();
            return 0;
        } else if (arg == "--uninstall-startup") {
            UninstallStartupTask();
            return 0;
        } else if (arg == "--install-service") {
            InstallWindowsService();
            return 0;
        } else if (arg == "--uninstall-service") {
            UninstallWindowsService();
            return 0;
        } else if (arg == "--check-proxies" || arg == "--speedtest") {
            RunSpeedTest(cfg);
            return 0;
        } else if (arg == "--set-user" && i + 2 < argc) {
            cfg.proxyUser = argv[i + 1];
            cfg.proxyPass = argv[i + 2];
            SaveConfigToFile(defaultConfigPath, cfg);
            std::cout << "[Config] EDC credentials updated successfully: user=" << cfg.proxyUser << "\n";
            return 0;
        }
    }

    // Ensure Singleton Instance using a Named Windows Mutex
    HANDLE hMutex = CreateMutexW(NULL, FALSE, L"Local\\ProxyManSingletonMutex");
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

    // Perform quick initial proxy pool health check on launch
    std::string activeProxyIp = cfg.proxyIp;
    uint16_t activeProxyPort = cfg.proxyPort;
    long bestLatency = 999999;

    std::printf("[Main] Auto-selecting optimal proxy from MNNIT pool...\n");
    for (const auto& pe : cfg.proxyPool) {
        std::string ip = pe;
        uint16_t port = 3128;
        auto pos = pe.find(':');
        if (pos != std::string::npos) {
            ip = pe.substr(0, pos);
            try { port = static_cast<uint16_t>(std::stoi(pe.substr(pos + 1))); } catch (...) {}
        }
        std::printf("  • Testing %s:%u ... ", ip.c_str(), port);
        std::fflush(stdout);

        ProxyHealthResult res = TestProxyHealth(ip, port, cfg.proxyUser, cfg.proxyPass, 1500);
        if (res.isHealthy) {
            std::printf("\x1b[32m✔ ONLINE\x1b[0m (%ld ms)\n", res.latencyMs);
            if (res.latencyMs < bestLatency) {
                bestLatency = res.latencyMs;
                activeProxyIp = ip;
                activeProxyPort = port;
            }
        } else {
            std::printf("\x1b[31m❌ OFFLINE\x1b[0m\n");
        }
    }

    cfg.proxyIp = activeProxyIp;
    cfg.proxyPort = activeProxyPort;

    if (bestLatency < 999999) {
        std::printf("\x1b[1;32m[Pool] Selected optimal proxy -> %s:%u (%ld ms latency)\x1b[0m\n\n",
                    cfg.proxyIp.c_str(), cfg.proxyPort, bestLatency);
    } else {
        std::printf("\x1b[1;33m[Pool] Defaulting to %s:%u\x1b[0m\n\n", cfg.proxyIp.c_str(), cfg.proxyPort);
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

    // Watch for network changes with debounced callback
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

    std::printf("\n[Main] ProxyMan is running. Press Ctrl+C to exit.\n\n");

    // Block until shutdown signal
    while (!g_shutdownRequested.load()) {
        Sleep(200);
    }

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
