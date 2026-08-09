#include "core/engine.h"
#include <cstdio>

EngineController::~EngineController() {
    StopEngine();
}

void EngineController::StartEngine(const Config& cfg) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running) {
        std::printf("[Controller] Engine already running, ignoring start request.\n");
        return;
    }

    m_cfg = cfg;
    std::printf("\n========================================================\n");
    std::printf("[Controller] Starting ProxyMan engine...\n");
    std::printf("  Relay:           127.0.0.1:%u\n", m_cfg.relayPort);
    std::printf("  Upstream:        %s:%u\n", m_cfg.proxyIp.c_str(), m_cfg.proxyPort);
    std::printf("  DNS Interceptor: ACTIVE (UDP:53 -> Synthetic IP NAT)\n");
    std::printf("  QUIC Blocker:    ACTIVE\n");
    std::printf("========================================================\n\n");

    // Start in dependency order: relay & DNS interceptor first, then packet engine, then QUIC blocker
    m_relay.Start(&m_table, m_cfg, &m_dnsTable);
    m_dnsInterceptor.Start(&m_dnsTable);
    m_packetEngine.Start(m_table, m_cfg);
    m_quicBlocker.Start();

    m_running = true;
    std::printf("[Controller] Engine started successfully.\n\n");
}

void EngineController::StopEngine() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running) {
        return;
    }

    std::printf("\n========================================================\n");
    std::printf("[Controller] Stopping ProxyMan engine...\n");
    std::printf("========================================================\n\n");

    // Stop in reverse order: packet engine first, QUIC blocker, DNS interceptor, then relay
    m_packetEngine.Stop();
    m_quicBlocker.Stop();
    m_dnsInterceptor.Stop();
    m_relay.Stop();

    m_running = false;
    std::printf("[Controller] Engine stopped.\n\n");
}

bool EngineController::IsRunning() const {
    return m_running;
}
