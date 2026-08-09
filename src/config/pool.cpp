#include "config/pool.h"
#include "core/http_tunnel.h"
#include <cstdio>
#include <string>

ProxySelectionResult SelectOptimalProxy(const Config& cfg) {
    ProxySelectionResult best;
    best.ip = cfg.proxyIp;
    best.port = cfg.proxyPort;
    best.latencyMs = 999999;
    best.found = false;

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
            if (res.latencyMs < best.latencyMs) {
                best.latencyMs = res.latencyMs;
                best.ip = ip;
                best.port = port;
                best.found = true;
            }
        } else {
            std::printf("\x1b[31m❌ OFFLINE\x1b[0m\n");
        }
    }
    return best;
}
