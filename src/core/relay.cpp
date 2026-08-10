#include "core/relay.h"
#include "core/http_tunnel.h"
#include "platform/process_utils.h"
#include "platform/wifi_utils.h"
#include "logging/data_tracker.h"
#include "net/network_watcher.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <cstdio>
#include <iostream>
#include <string>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace {

static std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

void PumpOneDirectionWithStats(SOCKET src, SOCKET dst, const std::string& processName, bool isTunneled) {
    char buf[65536]; // 64KB buffer for high throughput (~100 Mbps)
    for (;;) {
        int n = recv(src, buf, sizeof(buf), 0);
        if (n <= 0) break;
        int sent = 0;
        while (sent < n) {
            int w = send(dst, buf + sent, n - sent, 0);
            if (w <= 0) { shutdown(dst, SD_SEND); return; }
            sent += w;
        }
        DataTracker::Instance().AddBytes(processName, n, isTunneled);
    }
    shutdown(dst, SD_SEND);
}

bool ParseHostPort(const std::string& hostPort, std::string& host, uint16_t& port, uint16_t defaultPort) {
    auto pos = hostPort.find(':');
    if (pos != std::string::npos) {
        host = hostPort.substr(0, pos);
        port = static_cast<uint16_t>(std::stoi(hostPort.substr(pos + 1)));
    } else {
        host = hostPort;
        port = defaultPort;
    }
    return !host.empty();
}

static std::string ExtractSNI(const char* buf, size_t len) {
    if (len < 5) return "";
    if (static_cast<unsigned char>(buf[0]) != 0x16) return ""; // TLS Handshake
    if (static_cast<unsigned char>(buf[1]) != 0x03) return ""; // TLS major version 3

    size_t recordLen = (static_cast<unsigned char>(buf[3]) << 8) | static_cast<unsigned char>(buf[4]);
    if (len < 5 + recordLen) return "";

    size_t pos = 5;
    if (static_cast<unsigned char>(buf[pos]) != 0x01) return ""; // Client Hello (0x01)
    pos += 4; // Type (1) + Length (3)
    pos += 2; // Version (2)
    pos += 32; // Random (32)

    if (pos >= len) return "";
    size_t sessLen = static_cast<unsigned char>(buf[pos]);
    pos += 1 + sessLen; // Session ID

    if (pos + 2 > len) return "";
    size_t cipherLen = (static_cast<unsigned char>(buf[pos]) << 8) | static_cast<unsigned char>(buf[pos + 1]);
    pos += 2 + cipherLen; // Cipher Suites

    if (pos + 1 > len) return "";
    size_t compLen = static_cast<unsigned char>(buf[pos]);
    pos += 1 + compLen; // Compression Methods

    if (pos + 2 > len) return "";
    size_t extLen = (static_cast<unsigned char>(buf[pos]) << 8) | static_cast<unsigned char>(buf[pos + 1]);
    pos += 2;

    size_t endExt = pos + extLen;
    if (endExt > len) endExt = len;

    while (pos + 4 <= endExt) {
        uint16_t extType = (static_cast<unsigned char>(buf[pos]) << 8) | static_cast<unsigned char>(buf[pos + 1]);
        uint16_t extDataLen = (static_cast<unsigned char>(buf[pos + 2]) << 8) | static_cast<unsigned char>(buf[pos + 3]);
        pos += 4;

        if (extType == 0x0000) { // Server Name Indication (SNI)
            if (pos + 5 <= endExt) {
                uint8_t nameType = static_cast<unsigned char>(buf[pos + 2]);
                if (nameType == 0) { // host_name
                    size_t nameLen = (static_cast<unsigned char>(buf[pos + 3]) << 8) | static_cast<unsigned char>(buf[pos + 4]);
                    if (pos + 5 + nameLen <= endExt) {
                        return std::string(buf + pos + 5, nameLen);
                    }
                }
            }
        }
        pos += extDataLen;
    }
    return "";
}

static bool MatchWildcard(const std::string& pattern, const std::string& str) {
    if (pattern == str) return true;
    if (pattern == "*") return true;
    if (!pattern.empty() && pattern.back() == '*') {
        std::string prefix = pattern.substr(0, pattern.size() - 1);
        return str.rfind(prefix, 0) == 0;
    }
    return false;
}

static bool ShouldBypassProxy(const std::string& targetHostOrIp, const std::vector<std::string>& bypassList) {
    // Check if targetHostOrIp is a private/local IP address
    in_addr addr{};
    if (inet_pton(AF_INET, targetHostOrIp.c_str(), &addr) == 1) {
        uint32_t ip = ntohl(addr.s_addr);
        uint8_t b1 = static_cast<uint8_t>((ip >> 24) & 0xFF);
        uint8_t b2 = static_cast<uint8_t>((ip >> 16) & 0xFF);
        if (b1 == 127 || b1 == 10 || (b1 == 172 && b2 >= 16 && b2 <= 31) || (b1 == 192 && b2 == 168) || (b1 == 169 && b2 == 254)) {
            return true;
        }
    }

    for (const auto& entry : bypassList) {
        if (MatchWildcard(entry, targetHostOrIp)) return true;
    }
    return false;
}

static SOCKET ConnectDirectHost(const std::string& host, uint16_t port) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        addrinfo hints{}, *res = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        std::string portStr = std::to_string(port);
        if (getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) == 0 && res) {
            addr = *reinterpret_cast<sockaddr_in*>(res->ai_addr);
            freeaddrinfo(res);
        } else {
            closesocket(s);
            return INVALID_SOCKET;
        }
    }

    if (connect(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

void HandleConnection(SOCKET appSock, ConnTable* table, const Config* cfg, StoppableRelay* relay, DnsTable* dnsTable) {
    if (relay) relay->IncrementActiveConnections();

    // Enable TCP_NODELAY for zero latency
    int flag = 1;
    setsockopt(appSock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag));

    sockaddr_in peer{};
    int peerLen = sizeof(peer);
    uint16_t clientPort = 0;
    if (getpeername(appSock, reinterpret_cast<sockaddr*>(&peer), &peerLen) == 0) {
        clientPort = ntohs(peer.sin_port);
    }

    ProcessInfo procInfo = GetProcessInfoFromLocalPort(clientPort);

    std::string netModeStr = "Ethernet";
    if (GetActiveLinkType() == LinkType::WiFi) {
        std::string ssid = GetActiveWifiSSID();
        netModeStr = ssid.empty() ? "Wi-Fi" : ("Wi-Fi (" + ssid + ")");
    }

    uint32_t origAddrNet = 0;
    uint16_t origPortNet = 0;
    bool foundInTable = false;

    if (table != nullptr && clientPort != 0) {
        foundInTable = table->lookup(clientPort, origAddrNet, origPortNet);
    }

    SOCKET upstream = INVALID_SOCKET;
    std::string targetHost;
    uint16_t targetPort = 80;
    bool isTunneled = true;

    if (foundInTable) {
        // WinDivert NAT-redirected flow
        targetPort = ntohs(origPortNet);
        in_addr origInAddr{};
        origInAddr.s_addr = origAddrNet;
        char origIpStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &origInAddr, origIpStr, sizeof(origIpStr));

        if (dnsTable != nullptr && dnsTable->LookupDomain(origAddrNet, targetHost)) {
            // Resolved domain via DNS Interceptor
        } else {
            char peekBuf[2048] = {0};
            int peekLen = recv(appSock, peekBuf, sizeof(peekBuf), MSG_PEEK);
            if (peekLen > 5) {
                targetHost = ExtractSNI(peekBuf, peekLen);
            }
            if (targetHost.empty()) {
                targetHost = origIpStr;
            }
        }

        bool isDirectPassthrough = (relay && relay->GetMode() == RelayMode::DirectPassthrough);
        if (isDirectPassthrough || (cfg && ShouldBypassProxy(targetHost, cfg->bypassList))) {
            isTunneled = false;
            upstream = ConnectDirectHost(targetHost, targetPort);
        } else {
            isTunneled = true;
            upstream = HttpProxyConnectHost(cfg->proxyIp, cfg->proxyPort,
                                            cfg->proxyUser, cfg->proxyPass,
                                            targetHost, targetPort);
        }
    } else {
        // Direct local HTTP/CONNECT request (e.g. from System Proxy or env var HTTP_PROXY)
        char reqBuf[4096] = {0};
        int bytes = recv(appSock, reqBuf, sizeof(reqBuf) - 1, MSG_PEEK);
        if (bytes > 0) {
            reqBuf[bytes] = '\0';
            std::string reqStr(reqBuf);
            std::istringstream iss(reqStr);
            std::string method, url, proto;
            iss >> method >> url >> proto;

            bool isDirectPassthrough = (relay && relay->GetMode() == RelayMode::DirectPassthrough);

            if (method == "CONNECT") {
                char dummyBuf[4096];
                int readLen = static_cast<int>(reqStr.find("\r\n\r\n"));
                if (readLen != (int)std::string::npos) {
                    recv(appSock, dummyBuf, readLen + 4, 0);
                } else {
                    recv(appSock, dummyBuf, bytes, 0);
                }

                ParseHostPort(url, targetHost, targetPort, 443);

                if (isDirectPassthrough || (cfg && ShouldBypassProxy(targetHost, cfg->bypassList))) {
                    isTunneled = false;
                    upstream = ConnectDirectHost(targetHost, targetPort);
                } else {
                    isTunneled = true;
                    upstream = HttpProxyConnectHost(cfg->proxyIp, cfg->proxyPort,
                                                    cfg->proxyUser, cfg->proxyPass,
                                                    targetHost, targetPort);
                }

                if (upstream != INVALID_SOCKET) {
                    const char* okResp = "HTTP/1.1 200 Connection Established\r\n\r\n";
                    send(appSock, okResp, static_cast<int>(strlen(okResp)), 0);
                }
            } else {
                if (url.rfind("http://", 0) == 0) {
                    std::string hostAndPath = url.substr(7);
                    auto pathPos = hostAndPath.find('/');
                    std::string hostPart = (pathPos != std::string::npos) ? hostAndPath.substr(0, pathPos) : hostAndPath;
                    ParseHostPort(hostPart, targetHost, targetPort, 80);

                    if (isDirectPassthrough || (cfg && ShouldBypassProxy(targetHost, cfg->bypassList))) {
                        isTunneled = false;
                        upstream = ConnectDirectHost(targetHost, targetPort);
                    } else {
                        isTunneled = true;
                        upstream = HttpProxyConnectHost(cfg->proxyIp, cfg->proxyPort,
                                                        cfg->proxyUser, cfg->proxyPass,
                                                        targetHost, targetPort);
                    }
                }
            }
        }
    }

    if (upstream == INVALID_SOCKET) {
        closesocket(appSock);
        if (foundInTable && table != nullptr) table->erase(clientPort);
        if (relay) relay->DecrementActiveConnections();
        return;
    }

    // Record Live Connection Log
    ConnectionLogEntry logEntry;
    logEntry.timestamp = GetCurrentTimestamp();
    logEntry.processName = procInfo.name;
    logEntry.pid = procInfo.pid;
    logEntry.networkMode = netModeStr;
    logEntry.targetHost = targetHost;
    logEntry.targetPort = targetPort;
    logEntry.proxyStatus = isTunneled ? ("TUNNELED (" + cfg->proxyIp + ":" + std::to_string(cfg->proxyPort) + ")") : "DIRECT";

    DataTracker::Instance().RecordConnection(logEntry);

    // Enable TCP_NODELAY on upstream socket as well
    setsockopt(upstream, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&flag), sizeof(flag));

    std::thread t1(PumpOneDirectionWithStats, appSock, upstream, procInfo.name, isTunneled);
    std::thread t2(PumpOneDirectionWithStats, upstream, appSock, procInfo.name, isTunneled);
    t1.join();
    t2.join();

    closesocket(appSock);
    closesocket(upstream);
    if (foundInTable && table != nullptr) table->erase(clientPort);
    if (relay) relay->DecrementActiveConnections();
}

} // namespace

void RunRelay(ConnTable* table, const Config& cfg, DnsTable* dnsTable) {
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) { std::fprintf(stderr, "[Relay] socket() failed\n"); return; }
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg.relayPort);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) { closesocket(listener); return; }
    if (listen(listener, SOMAXCONN) != 0) { closesocket(listener); return; }
    std::printf("[Relay] Listening on 127.0.0.1:%u\n", cfg.relayPort);
    for (;;) {
        SOCKET client = accept(listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) continue;
        std::thread(HandleConnection, client, table, &cfg, nullptr, dnsTable).detach();
    }
}

StoppableRelay::~StoppableRelay() {
    Stop();
}

void StoppableRelay::Start(ConnTable* table, const Config& cfg, DnsTable* dnsTable) {
    if (m_running.load()) return;
    m_stopping = false;
    m_activeConnections = 0;
    m_acceptThread = std::thread(&StoppableRelay::AcceptLoop, this, table, &cfg, dnsTable);
    m_running = true;
}

void StoppableRelay::Stop() {
    if (!m_running.load()) return;
    m_stopping = true;

    if (m_listener != INVALID_SOCKET) {
        closesocket(m_listener);
        m_listener = INVALID_SOCKET;
    }

    if (m_acceptThread.joinable()) m_acceptThread.join();

    for (int i = 0; i < 20 && m_activeConnections.load() > 0; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (m_activeConnections.load() > 0) {
        std::printf("[Relay] %d connections still active after drain timeout.\n", m_activeConnections.load());
    }

    m_running = false;
    std::printf("[Relay] Stopped.\n");
}

void StoppableRelay::AcceptLoop(ConnTable* table, const Config* cfg, DnsTable* dnsTable) {
    m_listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listener == INVALID_SOCKET) {
        std::fprintf(stderr, "[Relay] socket() failed\n");
        return;
    }

    int opt = 1;
    setsockopt(m_listener, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg->relayPort);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (bind(m_listener, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        std::fprintf(stderr, "[Relay] bind() failed on 127.0.0.1:%u\n", cfg->relayPort);
        closesocket(m_listener);
        m_listener = INVALID_SOCKET;
        return;
    }
    if (listen(m_listener, SOMAXCONN) != 0) {
        std::fprintf(stderr, "[Relay] listen() failed\n");
        closesocket(m_listener);
        m_listener = INVALID_SOCKET;
        return;
    }

    std::printf("[Relay] Started - listening on 127.0.0.1:%u (upstream %s:%u)\n",
                cfg->relayPort, cfg->proxyIp.c_str(), cfg->proxyPort);

    while (!m_stopping.load()) {
        SOCKET client = accept(m_listener, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            if (m_stopping.load()) break;
            continue;
        }
        std::thread(HandleConnection, client, table, cfg, this, dnsTable).detach();
    }
}
