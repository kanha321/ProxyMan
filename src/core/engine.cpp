#include "core/engine.h"
#include "config/pool.h"
#include "platform/proxy_settings.h"
#include <cstdio>
#include <string>
#include <chrono>

EngineController::~EngineController() {
    StopEngine();
}

void EngineController::EvaluateReachability() {
    // 1. Perform Functional Proxy Probe (Cached single-host lookup first, parallel pool race if failed)
    ProxySelectionResult poolResult = SelectOptimalProxy(m_cfg);

    if (poolResult.found) {
        // Instant Proxy Mode transition on success (zero delay required for instant setup)
        m_failedProbeCount = 0;
        m_cfg.proxyIp = poolResult.ip;
        m_cfg.proxyPort = poolResult.port;

        if (m_relay.GetMode() != RelayMode::Proxy) {
            std::printf("\n[Controller] Functional proxy probe succeeded -> %s:%u (%ld ms). Engaging Proxy Mode.\n",
                        m_cfg.proxyIp.c_str(), m_cfg.proxyPort, poolResult.latencyMs);
            m_relay.SetMode(RelayMode::Proxy);
            m_dnsInterceptor.Start(&m_dnsTable);
            m_packetEngine.Start(m_table, m_cfg);
            m_quicBlocker.Start();

            std::string proxyStr = "127.0.0.1:" + std::to_string(m_cfg.relayPort);
            SetSystemProxy(proxyStr);
            ShowNotificationToast("⚡ ProxyMan", "Connected to MNNIT Network (" + m_cfg.proxyIp + ":" + std::to_string(m_cfg.proxyPort) + ")");
        }
    } else {
        // Asymmetric Debounce: Require 2 consecutive failed probes before flipping to Direct Mode (prevents Wi-Fi flap drops)
        m_failedProbeCount++;
        std::printf("\n[Controller] Proxy probe failed (Attempt %d/2).\n", m_failedProbeCount);

        if (m_failedProbeCount >= 2 && m_relay.GetMode() == RelayMode::Proxy) {
            std::printf("[Controller] 2 consecutive failed probes confirmed — flipping to Direct Passthrough Mode.\n");
            InvalidateCachedProxy();
            m_packetEngine.Stop();
            m_quicBlocker.Stop();
            m_dnsInterceptor.Stop();
            m_relay.SetMode(RelayMode::DirectPassthrough);

            ClearSystemProxy();
            ShowNotificationToast("📶 ProxyMan", "Switched to Wi-Fi / Hotspot (Proxy Bypassed)");
        }
    }
}

void EngineController::HeartbeatLoop() {
    while (!m_stoppingHeartbeat.load()) {
        for (int i = 0; i < 450; ++i) { // 45 seconds heartbeat interval
            if (m_stoppingHeartbeat.load()) return;
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_running) continue;
            EvaluateReachability();
        }
    }
}

void EngineController::StartEngine(const Config& cfg, LinkType initialLink) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running) {
        std::printf("[Controller] Engine already running, ignoring start request.\n");
        return;
    }

    m_cfg = cfg;
    m_currentLink = initialLink;
    m_failedProbeCount = 0;
    m_stoppingHeartbeat = false;

    std::printf("\n========================================================\n");
    std::printf("[Controller] Starting ProxyMan engine...\n");
    std::printf("  Relay:           127.0.0.1:%u (Always running for seamless app fallback)\n", m_cfg.relayPort);
    std::printf("  Upstream Pool:   %zu proxies configured\n", m_cfg.proxyPool.size());
    std::printf("  Initial Link:    %s\n", LinkTypeToString(initialLink).c_str());
    std::printf("========================================================\n\n");

    // Always start local relay so 127.0.0.1:55555 proxy listener stays open for running processes
    m_relay.Start(&m_table, m_cfg, &m_dnsTable);
    m_running = true;

    // Start background reachability heartbeat thread
    m_heartbeatThread = std::thread(&EngineController::HeartbeatLoop, this);

    // Initial reachability evaluation
    EvaluateReachability();
}

void EngineController::SetNetworkState(LinkType newLink) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running) return;

    m_currentLink = newLink;
    std::printf("\n[Controller] Network interface event -> %s (Debouncing 300ms before reachability probe)...\n", LinkTypeToString(newLink).c_str());

    // Debounce 300ms for OS DHCP IP assignment to complete
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    // Force refresh optimal proxy selection
    InvalidateCachedProxy();
    EvaluateReachability();
}

void EngineController::StopEngine() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running) {
        return;
    }

    std::printf("\n========================================================\n");
    std::printf("[Controller] Stopping ProxyMan engine...\n");
    std::printf("========================================================\n\n");

    m_stoppingHeartbeat = true;
    if (m_heartbeatThread.joinable()) {
        m_heartbeatThread.join();
    }

    // Stop in reverse order
    m_packetEngine.Stop();
    m_quicBlocker.Stop();
    m_dnsInterceptor.Stop();
    m_relay.Stop();

    ClearSystemProxy();

    m_running = false;
    std::printf("[Controller] Engine stopped.\n\n");
}

bool EngineController::IsRunning() const {
    return m_running;
}
