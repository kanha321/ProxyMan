#pragma once

#include "config/config.h"
#include "core/conn_table.h"
#include "net/packet_engine.h"
#include "net/quic_blocker.h"
#include "core/relay.h"
#include "net/dns_interceptor.h"
#include <mutex>

class EngineController {
public:
    EngineController() = default;
    ~EngineController();

    EngineController(const EngineController&) = delete;
    EngineController& operator=(const EngineController&) = delete;

    void StartEngine(const Config& cfg);
    void StopEngine();
    bool IsRunning() const;

private:
    std::mutex m_mutex;
    bool m_running = false;
    Config m_cfg;
    ConnTable m_table;
    DnsTable m_dnsTable;
    StoppableRelay m_relay;
    StoppablePacketEngine m_packetEngine;
    StoppableQuicBlocker m_quicBlocker;
    StoppableDnsInterceptor m_dnsInterceptor;
};
