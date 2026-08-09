#include "cli/status.h"
#include "config/config.h"
#include "platform/proxy_settings.h"
#include "platform/wifi_utils.h"
#include "net/network_watcher.h"

#include <iostream>
#include <windows.h>

void ShowStatus(const Config& cfg) {
    HANDLE hMutex = OpenMutexW(MUTEX_ALL_ACCESS, FALSE, L"Global\\ProxyManSingletonMutex");
    bool isRunning = (hMutex != NULL);
    if (hMutex) CloseHandle(hMutex);

    bool sysProxyEnabled = false;
    std::string sysProxyAddr;
    GetSystemProxy(sysProxyEnabled, sysProxyAddr);

    LinkType link = GetActiveLinkType();
    std::string linkStr = LinkTypeToString(link);
    if (link == LinkType::WiFi) {
        std::string ssid = GetActiveWifiSSID();
        if (!ssid.empty()) linkStr += " (SSID: " + ssid + ")";
    }

    std::cout << "\n\x1b[1;36m===================================================================\x1b[0m\n";
    std::cout << "\x1b[1;36m  ⚡ ProxyMan Engine & System Proxy Status\x1b[0m\n";
    std::cout << "\x1b[1;36m===================================================================\x1b[0m\n";
    std::cout << "Engine Status:       " << (isRunning ? "\x1b[1;32m✔ RUNNING (Active Daemon)\x1b[0m" : "\x1b[1;31m❌ STOPPED\x1b[0m") << "\n";
    std::cout << "System Proxy:        " << (sysProxyEnabled ? ("\x1b[1;32m✔ ENABLED (" + sysProxyAddr + ")\x1b[0m") : "\x1b[1;31m❌ DISABLED (Direct)\x1b[0m") << "\n";
    std::cout << "Configured Proxy:    \x1b[1;33m" << cfg.proxyIp << ":" << cfg.proxyPort << "\x1b[0m (user: " << cfg.proxyUser << ")\n";
    std::cout << "Current Network:     \x1b[1;37m" << linkStr << "\x1b[0m\n";
    std::cout << "Configuration File:  \x1b[1;33m" << GetDefaultConfigPath() << "\x1b[0m\n";
    std::cout << "\x1b[1;36m===================================================================\x1b[0m\n\n";
}
