#ifndef HTTP_PROXY_CLIENT_H
#define HTTP_PROXY_CLIENT_H

#include <string>
#include <cstdint>
#include <winsock2.h>

SOCKET HttpProxyConnect(const std::string& proxyIp, uint16_t proxyPort,
                         const std::string& username, const std::string& password,
                         uint32_t targetAddrNet, uint16_t targetPortNet);

SOCKET HttpProxyConnectHost(const std::string& proxyIp, uint16_t proxyPort,
                             const std::string& username, const std::string& password,
                             const std::string& targetHost, uint16_t targetPort);

#endif // HTTP_PROXY_CLIENT_H
