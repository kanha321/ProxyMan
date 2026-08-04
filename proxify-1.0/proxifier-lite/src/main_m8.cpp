#include "config.h"
#include "conn_table.h"
#include "relay.h"
#include "packet_engine.h"
#include <winsock2.h>
#include <iostream>
#include <thread>

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
            std::cerr << "Failed to load config file." << std::endl;
            WSACleanup();
            return 1;
        }
    }

    std::cout << "=== ProxyBridge Transparent NAT Engine (Milestone 8) ===" << std::endl;
    std::cout << "Relay port:    127.0.0.1:" << cfg.relayPort << std::endl;
    std::cout << "Upstream target: " << cfg.proxyIp << ":" << cfg.proxyPort << std::endl;
    std::cout << "Upstream user:   " << cfg.proxyUser << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;

    ConnTable table;

    // Start local relay thread
    std::thread relayThread([&table, &cfg]() {
        RunRelay(&table, cfg);
    });
    relayThread.detach();

    // Run transparent NAT packet engine (blocking capture loop)
    RunPacketEngine(table, cfg);

    WSACleanup();
    return 0;
}
