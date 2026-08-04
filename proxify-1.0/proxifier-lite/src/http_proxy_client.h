#ifndef HTTP_PROXY_CLIENT_H
#define HTTP_PROXY_CLIENT_H

#include <string>
#include <cstdint>
#include <winsock2.h>

struct ProxyHealthResult {
    std::string ip;
    uint16_t port = 3128;
    bool isHealthy = false;
    long latencyMs = -1;
    std::string errorMsg;
};

SOCKET HttpProxyConnect(const std::string& proxyIp, uint16_t proxyPort,
                         const std::string& username, const std::string& password,
                         uint32_t targetAddrNet, uint16_t targetPortNet);

SOCKET HttpProxyConnectHost(const std::string& proxyIp, uint16_t proxyPort,
                             const std::string& username, const std::string& password,
                             const std::string& targetHost, uint16_t targetPort);

ProxyHealthResult TestProxyHealth(const std::string& proxyIp, uint16_t proxyPort,
                                   const std::string& username, const std::string& password,
                                   int timeoutMs = 2000);

#endif // HTTP_PROXY_CLIENT_H
