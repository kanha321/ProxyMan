#include "config.h"
#include "conn_table.h"
#include "relay.h"
#include "packet_engine.h"
#include "quic_blocker.h"
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

    std::cout << "=== ProxyBridge Full Redirector & QUIC Blocker (Milestone 9) ===" << std::endl;
    std::cout << "Relay port:      127.0.0.1:" << cfg.relayPort << std::endl;
    std::cout << "Upstream target: " << cfg.proxyIp << ":" << cfg.proxyPort << std::endl;
    std::cout << "Upstream user:   " << cfg.proxyUser << std::endl;
    std::cout << "QUIC Blocker:    ACTIVE (UDP:443 dropped)" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;

    ConnTable table;

    // 1. Start local relay thread
    std::thread relayThread([&table, &cfg]() {
        RunRelay(&table, cfg);
    });
    relayThread.detach();

    // 2. Start QUIC blocker thread
    std::thread quicThread([]() {
        RunQuicBlocker();
    });
    quicThread.detach();

    // 3. Run main transparent TCP NAT packet engine
    RunPacketEngine(table, cfg);

    WSACleanup();
    return 0;
}
