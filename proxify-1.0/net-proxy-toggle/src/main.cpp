#include "network_watcher.h"
#include "proxy_settings.h"
#include <iostream>
#include <cstring>

void ApplyProxyForLinkType(LinkType type, const std::string& targetProxy) {
    if (type == LinkType::Ethernet) {
        SetSystemProxy(targetProxy);
    } else {
        ClearSystemProxy();
    }
}

int main(int argc, char* argv[]) {
    if (argc > 1 && strcmp(argv[1], "--test-set") == 0) {
        std::string target = (argc > 2) ? argv[2] : "127.0.0.1:55555";
        std::cout << "Testing SetSystemProxy(" << target << ")..." << std::endl;
        bool ok = SetSystemProxy(target);
        return ok ? 0 : 1;
    }

    if (argc > 1 && strcmp(argv[1], "--test-clear") == 0) {
        std::cout << "Testing ClearSystemProxy()..." << std::endl;
        bool ok = ClearSystemProxy();
        return ok ? 0 : 1;
    }

    std::string targetProxy = "127.0.0.1:55555";
    if (argc > 1) {
        targetProxy = argv[1];
    }

    std::cout << "=== ProxyBridge Net-Proxy-Toggle (Milestone 3) ===" << std::endl;
    std::cout << "Target Proxy: " << targetProxy << std::endl;

    LinkType initial = GetActiveLinkType();
    std::cout << "Initial Active Link Type: " << LinkTypeToString(initial) << std::endl;

    ApplyProxyForLinkType(initial, targetProxy);

    std::cout << "\nListening for network interface changes live... (Press Enter to exit)\n" << std::endl;

    NetworkWatcher watcher([targetProxy](LinkType newType) {
        std::cout << "[EVENT] Active Network Changed: " << LinkTypeToString(newType) << std::endl;
        ApplyProxyForLinkType(newType, targetProxy);
    });

    std::cin.get();

    std::cout << "\nExiting... Cleaning up system proxy settings." << std::endl;
    ClearSystemProxy();

    return 0;
}
