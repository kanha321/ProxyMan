#ifndef SOCKS5_CLIENT_H
#define SOCKS5_CLIENT_H

#include <winsock2.h>
#include <cstdint>

SOCKET Socks5Connect(const char* proxyIp, uint16_t proxyPortHost,
                      uint32_t targetAddrNet, uint16_t targetPortNet);

#endif // SOCKS5_CLIENT_H
