#include "packet_engine.h"
#include <winsock2.h>
#include <iostream>

#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }

    std::cout << "=== ProxyBridge Packet Engine Read-Only Capture (Milestone 7) ===" << std::endl;
    std::cout << "Initializing WinDivert driver..." << std::endl;

    RunPacketEngineCaptureOnly();

    WSACleanup();
    return 0;
}
