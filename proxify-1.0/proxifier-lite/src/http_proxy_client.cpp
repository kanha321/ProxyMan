#include "http_proxy_client.h"
#include <ws2tcpip.h>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <chrono>

namespace {

std::string Base64Encode(const std::string& input) {
    static const char* kTable =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);

    size_t i = 0;
    while (i + 3 <= input.size()) {
        uint32_t n = (static_cast<unsigned char>(input[i]) << 16)
                   | (static_cast<unsigned char>(input[i + 1]) << 8)
                   | (static_cast<unsigned char>(input[i + 2]));
        out += kTable[(n >> 18) & 0x3F];
        out += kTable[(n >> 12) & 0x3F];
        out += kTable[(n >> 6) & 0x3F];
        out += kTable[n & 0x3F];
        i += 3;
    }
    size_t rem = input.size() - i;
    if (rem == 1) {
        uint32_t n = static_cast<unsigned char>(input[i]) << 16;
        out += kTable[(n >> 18) & 0x3F];
        out += kTable[(n >> 12) & 0x3F];
        out += "==";
    } else if (rem == 2) {
        uint32_t n = (static_cast<unsigned char>(input[i]) << 16)
                   | (static_cast<unsigned char>(input[i + 1]) << 8);
        out += kTable[(n >> 18) & 0x3F];
        out += kTable[(n >> 12) & 0x3F];
        out += kTable[(n >> 6) & 0x3F];
        out += "=";
    }
    return out;
}

bool ReadLine(SOCKET s, std::string& outLine) {
    outLine.clear();
    char c;
    for (;;) {
        int n = recv(s, &c, 1, 0);
        if (n <= 0) return false;
        if (c == '\n') {
            if (!outLine.empty() && outLine.back() == '\r') outLine.pop_back();
            return true;
        }
        outLine += c;
        if (outLine.size() > 8192) return false;
    }
}

} // namespace

SOCKET HttpProxyConnect(const std::string& proxyIp, uint16_t proxyPort,
                         const std::string& username, const std::string& password,
                         uint32_t targetAddrNet, uint16_t targetPortNet) {
    char targetIpStr[INET_ADDRSTRLEN];
    in_addr targetAddr{};
    targetAddr.s_addr = targetAddrNet;
    inet_ntop(AF_INET, &targetAddr, targetIpStr, sizeof(targetIpStr));
    uint16_t targetPortHost = ntohs(targetPortNet);

    return HttpProxyConnectHost(proxyIp, proxyPort, username, password, targetIpStr, targetPortHost);
}

SOCKET HttpProxyConnectHost(const std::string& proxyIp, uint16_t proxyPort,
                             const std::string& username, const std::string& password,
                             const std::string& targetHost, uint16_t targetPort) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;

    sockaddr_in proxyAddr{};
    proxyAddr.sin_family = AF_INET;
    proxyAddr.sin_port = htons(proxyPort);
    if (inet_pton(AF_INET, proxyIp.c_str(), &proxyAddr.sin_addr) != 1) {
        closesocket(s);
        return INVALID_SOCKET;
    }

    // Set 3 second socket connection timeout
    DWORD timeout = 3000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    if (connect(s, reinterpret_cast<sockaddr*>(&proxyAddr), sizeof(proxyAddr)) != 0) {
        closesocket(s);
        return INVALID_SOCKET;
    }

    std::string auth = Base64Encode(username + ":" + password);

    char request[2048];
    int len = std::snprintf(request, sizeof(request),
        "CONNECT %s:%u HTTP/1.1\r\n"
        "Host: %s:%u\r\n"
        "Proxy-Authorization: Basic %s\r\n"
        "Proxy-Connection: Keep-Alive\r\n"
        "Connection: Keep-Alive\r\n"
        "\r\n",
        targetHost.c_str(), targetPort, targetHost.c_str(), targetPort, auth.c_str());

    if (len <= 0 || send(s, request, len, 0) != len) {
        closesocket(s);
        return INVALID_SOCKET;
    }

    std::string statusLine;
    if (!ReadLine(s, statusLine)) { closesocket(s); return INVALID_SOCKET; }

    int statusCode = 0;
    size_t firstSpace = statusLine.find(' ');
    if (firstSpace != std::string::npos) {
        statusCode = std::atoi(statusLine.c_str() + firstSpace + 1);
    }

    std::string line;
    do {
        if (!ReadLine(s, line)) { closesocket(s); return INVALID_SOCKET; }
    } while (!line.empty());

    if (statusCode != 200) {
        closesocket(s);
        return INVALID_SOCKET;
    }

    return s;
}

ProxyHealthResult TestProxyHealth(const std::string& proxyIp, uint16_t proxyPort,
                                   const std::string& username, const std::string& password,
                                   int timeoutMs) {
    ProxyHealthResult result;
    result.ip = proxyIp;
    result.port = proxyPort;

    auto tStart = std::chrono::high_resolution_clock::now();
    SOCKET s = HttpProxyConnectHost(proxyIp, proxyPort, username, password, "1.1.1.1", 80);
    auto tEnd = std::chrono::high_resolution_clock::now();

    if (s != INVALID_SOCKET) {
        result.isHealthy = true;
        result.latencyMs = static_cast<long>(std::chrono::duration_cast<std::chrono::milliseconds>(tEnd - tStart).count());
        closesocket(s);
    } else {
        result.isHealthy = false;
        result.latencyMs = -1;
        result.errorMsg = "Connection failed or authentication rejected";
    }

    return result;
}
