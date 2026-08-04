#include "config.h"
#include "engine_controller.h"
#include "network_watcher.h"
#include "proxy_settings.h"

#include <winsock2.h>
#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <string>
#include <atomic>
#include <cstdlib>

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
    sei.lpVerb = L"runas";
    sei.lpFile = exePath;
    sei.lpParameters = args.c_str();
    sei.nShow = SW_SHOWNORMAL;

    return ShellExecuteExW(&sei) != FALSE;
}

static bool InstallStartupTask() {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) {
        std::cerr << "[Main] Failed to get executable path.\n";
        return false;
    }

    std::wstring cmd = L"schtasks /create /tn \"ProxyMan\" /tr \"\\\"";
    cmd += exePath;
    cmd += L"\\\"\" /sc ONLOGON /rl HIGHEST /f";

    int ret = _wsystem(cmd.c_str());
    if (ret == 0) {
        std::cout << "\n==========================================================\n";
        std::cout << "  ProxyMan Task Scheduler Autostart Registered! ✨\n";
        std::cout << "==========================================================\n";
        std::cout << "Method:          Task Scheduler (/sc ONLOGON /rl HIGHEST) [RECOMMENDED]\n";
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
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) {
        std::cerr << "[Main] Failed to get executable path.\n";
        return false;
    }

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
        std::cout << "Execution:       Starts at System Boot (Before User Logon Screen)\n";
        std::cout << "Privileges:      SYSTEM (Invisible Headless Background Service)\n\n";
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

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

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

    // Check command line flags for startup task/service management
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

    // Load config
    Config cfg;
    std::string defaultConfigPath = GetDefaultConfigPath();
    bool loaded = false;

    if (argc > 1 && argv[1][0] != '-') {
        loaded = LoadConfigFromFile(argv[1], cfg);
    } else {
        loaded = LoadConfigFromFile(defaultConfigPath, cfg);
        if (!loaded) {
            loaded = LoadConfigFromFile("proxy-config.txt", cfg);
        }
    }

    if (!loaded) {
        if (!PromptAndSaveConfig(defaultConfigPath, cfg)) {
            std::cerr << "Failed to initialize configuration." << std::endl;
            WSACleanup();
            return 1;
        }
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
    } else {
        std::printf("[Main] Not on Ethernet - engine idle, waiting for network change.\n");
        ClearSystemProxy();
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
            }
        } else {
            if (controller.IsRunning()) {
                std::printf("[Main] Ethernet disconnected - stopping engine and clearing system proxy.\n");
                controller.StopEngine();
                ClearSystemProxy();
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
