#include "config/pool.h"
#include "core/http_tunnel.h"
#include <cstdio>
#include <string>
#include <vector>
#include <future>
#include <mutex>
#include <atomic>

namespace {
    static std::mutex g_cacheMutex;
    static std::string g_cachedProxyIp;
    static uint16_t g_cachedProxyPort = 3128;
}

void InvalidateCachedProxy() {
    std::lock_guard<std::mutex> lock(g_cacheMutex);
    g_cachedProxyIp.clear();
}

ProxySelectionResult SelectOptimalProxy(const Config& cfg, bool forceRefresh) {
    // 1. Try cached proxy first (avoids multi-host scanning bursts on network flaps)
    if (!forceRefresh) {
        std::string cachedIp;
        uint16_t cachedPort;
        {
            std::lock_guard<std::mutex> lock(g_cacheMutex);
            cachedIp = g_cachedProxyIp;
            cachedPort = g_cachedProxyPort;
        }

        if (!cachedIp.empty()) {
            ProxyHealthResult res = TestProxyHealth(cachedIp, cachedPort, cfg.proxyUser, cfg.proxyPass, 600);
            if (res.isHealthy) {
                ProxySelectionResult result;
                result.ip = cachedIp;
                result.port = cachedPort;
                result.latencyMs = res.latencyMs;
                result.found = true;
                return result;
            }
        }
    }

    // 2. Parallel Pool Racing: Probe all proxy pool IPs concurrently with 600ms timeout
    std::vector<std::pair<std::string, uint16_t>> poolEntries;
    for (const auto& pe : cfg.proxyPool) {
        std::string ip = pe;
        uint16_t port = 3128;
        auto pos = pe.find(':');
        if (pos != std::string::npos) {
            ip = pe.substr(0, pos);
            try { port = static_cast<uint16_t>(std::stoi(pe.substr(pos + 1))); } catch (...) {}
        }
        poolEntries.emplace_back(ip, port);
    }

    std::vector<std::future<ProxyHealthResult>> futures;
    futures.reserve(poolEntries.size());

    for (const auto& entry : poolEntries) {
        std::string ip = entry.first;
        uint16_t port = entry.second;
        std::string user = cfg.proxyUser;
        std::string pass = cfg.proxyPass;

        futures.push_back(std::async(std::launch::async, [ip, port, user, pass]() {
            return TestProxyHealth(ip, port, user, pass, 600);
        }));
    }

    ProxySelectionResult best;
    best.ip = cfg.proxyIp;
    best.port = cfg.proxyPort;
    best.latencyMs = 999999;
    best.found = false;

    for (size_t i = 0; i < futures.size(); ++i) {
        ProxyHealthResult res = futures[i].get();
        if (res.isHealthy && res.latencyMs < best.latencyMs) {
            best.latencyMs = res.latencyMs;
            best.ip = res.ip;
            best.port = res.port;
            best.found = true;
        }
    }

    if (best.found) {
        std::lock_guard<std::mutex> lock(g_cacheMutex);
        g_cachedProxyIp = best.ip;
        g_cachedProxyPort = best.port;
    }

    return best;
}
