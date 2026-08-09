#include "cli/diagnostics.h"
#include "config/config.h"
#include "core/http_tunnel.h"
#include <winsock2.h>
#include <iostream>
#include <vector>
#include <algorithm>
#include <string>

void RunSpeedTest(const Config& cfg) {
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
