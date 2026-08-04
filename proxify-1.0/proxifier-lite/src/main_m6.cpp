#include "config.h"
#include "relay.h"
#include <winsock2.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

int main(int argc, char* argv[]) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    std::string configPath = "../proxy-config.txt";
    if (argc > 1) {
        configPath = argv[1];
    }

    Config cfg;
    if (!LoadConfigFromFile(configPath, cfg)) {
        if (!LoadConfigFromFile("proxy-config.txt", cfg)) {
            std::cerr << "Failed to load configuration file." << std::endl;
            WSACleanup();
            return 1;
        }
    }

    std::cout << "=== ProxyBridge Local Relay (Milestone 6) ===" << std::endl;
    std::cout << "Listening local port: " << cfg.relayPort << std::endl;
    std::cout << "Upstream proxy target: " << cfg.proxyIp << ":" << cfg.proxyPort << std::endl;
    std::cout << "Upstream credentials:  " << cfg.proxyUser << std::endl;

    RunRelay(nullptr, cfg);

    WSACleanup();
    return 0;
}
