#include "config.h"
#include "http_proxy_client.h"
#include "socks5_client.h"
#include <winsock2.h>
#include <ws2tcpip.h>
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
        // Fallback check current directory
        if (!LoadConfigFromFile("proxy-config.txt", cfg)) {
            std::cerr << "Failed to load config from " << configPath << std::endl;
            WSACleanup();
            return 1;
        }
    }

    std::cout << "=== ProxyBridge Protocol Backend Test (Milestones 4 & 5) ===" << std::endl;
    std::cout << "Target Proxy: " << cfg.proxyIp << ":" << cfg.proxyPort << std::endl;
    std::cout << "Proxy User:   " << cfg.proxyUser << std::endl;

    // Test destination: 1.1.1.1:80 (Cloudflare DNS HTTP)
    in_addr targetAddr{};
    inet_pton(AF_INET, "1.1.1.1", &targetAddr);
    uint32_t targetAddrNet = targetAddr.s_addr;
    uint16_t targetPortNet = htons(80);

    std::cout << "\n--- Milestone 5: Testing HTTP CONNECT with Basic Auth ---" << std::endl;
    SOCKET s = HttpProxyConnect(cfg.proxyIp, cfg.proxyPort, cfg.proxyUser, cfg.proxyPass, targetAddrNet, targetPortNet);

    if (s == INVALID_SOCKET) {
        std::cerr << "[FAIL] HTTP CONNECT to proxy failed." << std::endl;
    } else {
        std::cout << "[SUCCESS] HTTP CONNECT tunnel established successfully!" << std::endl;

        // Send a simple HTTP GET request over the established tunnel
        const char* getReq = "GET / HTTP/1.1\r\nHost: 1.1.1.1\r\nUser-Agent: ProxyBridgeTest/1.0\r\nConnection: close\r\n\r\n";
        send(s, getReq, static_cast<int>(strlen(getReq)), 0);

        char buf[512] = {0};
        int bytes = recv(s, buf, sizeof(buf) - 1, 0);
        if (bytes > 0) {
            buf[bytes] = '\0';
            std::cout << "[RESPONSE RECEIVED VIA TUNNEL]\n" << buf << std::endl;
        }

        closesocket(s);
    }

    WSACleanup();
    return 0;
}
