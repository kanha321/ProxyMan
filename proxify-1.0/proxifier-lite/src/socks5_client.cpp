#include "socks5_client.h"
#include <cstdio>
#include <cstring>
#include <ws2tcpip.h>

namespace {

bool RecvAll(SOCKET s, char* buf, int len) {
    int got = 0;
    while (got < len) {
        int n = recv(s, buf + got, len - got, 0);
        if (n <= 0) return false;
        got += n;
    }
    return true;
}

bool SendAll(SOCKET s, const char* buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(s, buf + sent, len - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }
    return true;
}

} // namespace

SOCKET Socks5Connect(const char* proxyIp, uint16_t proxyPortHost,
                      uint32_t targetAddrNet, uint16_t targetPortNet) {
    SOCKET s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;

    sockaddr_in proxyAddr{};
    proxyAddr.sin_family = AF_INET;
    proxyAddr.sin_port = htons(proxyPortHost);
    if (inet_pton(AF_INET, proxyIp, &proxyAddr.sin_addr) != 1) {
        closesocket(s);
        return INVALID_SOCKET;
    }

    if (connect(s, reinterpret_cast<sockaddr*>(&proxyAddr), sizeof(proxyAddr)) != 0) {
        closesocket(s);
        return INVALID_SOCKET;
    }

    char greeting[3] = { 0x05, 0x01, 0x00 };
    if (!SendAll(s, greeting, sizeof(greeting))) { closesocket(s); return INVALID_SOCKET; }

    char methodReply[2];
    if (!RecvAll(s, methodReply, 2)) { closesocket(s); return INVALID_SOCKET; }
    if (methodReply[0] != 0x05 || methodReply[1] != 0x00) {
        closesocket(s);
        return INVALID_SOCKET;
    }

    char req[10];
    req[0] = 0x05;              // VER
    req[1] = 0x01;              // CMD = CONNECT
    req[2] = 0x00;              // RSV
    req[3] = 0x01;              // ATYP = IPv4
    memcpy(&req[4], &targetAddrNet, 4);
    memcpy(&req[8], &targetPortNet, 2);
    if (!SendAll(s, req, sizeof(req))) { closesocket(s); return INVALID_SOCKET; }

    char replyHead[4];
    if (!RecvAll(s, replyHead, 4)) { closesocket(s); return INVALID_SOCKET; }
    if (replyHead[0] != 0x05 || replyHead[1] != 0x00) {
        closesocket(s);
        return INVALID_SOCKET;
    }

    int addrLen = 0;
    switch (replyHead[3]) {
        case 0x01: addrLen = 4;  break; // IPv4
        case 0x04: addrLen = 16; break; // IPv6
        case 0x03: {                    // domain
            unsigned char lenByte;
            if (!RecvAll(s, reinterpret_cast<char*>(&lenByte), 1)) { closesocket(s); return INVALID_SOCKET; }
            addrLen = lenByte;
            break;
        }
        default:
            closesocket(s);
            return INVALID_SOCKET;
    }
    char discard[256];
    if (!RecvAll(s, discard, addrLen + 2)) { closesocket(s); return INVALID_SOCKET; }

    return s;
}
