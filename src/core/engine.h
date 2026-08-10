#pragma once

#include "config/config.h"
#include "core/conn_table.h"
#include "net/packet_engine.h"
#include "net/quic_blocker.h"
#include "core/relay.h"
#include "net/dns_interceptor.h"
#include "net/network_watcher.h"
#include <mutex>
#include <thread>
#include <atomic>

class EngineController {
public:
    EngineController() = default;
    ~EngineController();

    EngineController(const EngineController&) = delete;
    EngineController& operator=(const EngineController&) = delete;

    void StartEngine(const Config& cfg, LinkType initialLink);
    void SetNetworkState(LinkType link);
    void EvaluateReachability();
    void StopEngine();
    bool IsRunning() const;

private:
    void HeartbeatLoop();

    std::mutex m_mutex;
    bool m_running = false;
    LinkType m_currentLink = LinkType::Unknown;
    Config m_cfg;
    ConnTable m_table;
    DnsTable m_dnsTable;
    StoppableRelay m_relay;
    StoppablePacketEngine m_packetEngine;
    StoppableQuicBlocker m_quicBlocker;
    StoppableDnsInterceptor m_dnsInterceptor;

    std::thread m_heartbeatThread;
    std::atomic<bool> m_stoppingHeartbeat{false};
    int m_failedProbeCount = 0;
};
