#include "cli/help.h"
#include "version.h"
#include <iostream>

void PrintHelp() {
    std::cout << "\x1b[1;36m";
    std::cout << "===================================================================\n";
    std::cout << "  ⚡ ProxyMan - Network-Aware Transparent Proxy Engine (v1.0.0)\n";
    std::cout << "===================================================================\n";
    std::cout << "\x1b[0m";
    std::cout << "\x1b[1;32mUSAGE:\x1b[0m\n";
    std::cout << "    ProxyMan.exe [FLAGS] [COMMANDS] [CONFIG_PATH]\n\n";

    std::cout << "\x1b[1;32mINFORMATIONAL & POOL COMMANDS:\x1b[0m\n";
    std::cout << "    \x1b[1;36m-h, --help\x1b[0m               Display this help & usage guide\n";
    std::cout << "    \x1b[1;36m-v, --version\x1b[0m            Display ProxyMan version details\n";
    std::cout << "    \x1b[1;36m--status\x1b[0m                 Display ProxyMan engine & system proxy status\n";
    std::cout << "    \x1b[1;36m--logs [-f]\x1b[0m              Tail connection logs (use -f to follow live stream)\n";
    std::cout << "    \x1b[1;36m--stop\x1b[0m                   Stop active background ProxyMan engine & restore system proxy\n";
    std::cout << "    \x1b[1;36m--stats\x1b[0m                  Display session data usage & application traffic summary\n";
    std::cout << "    \x1b[1;36m--speedtest\x1b[0m              Run MNNIT proxy speed & latency benchmark\n";
    std::cout << "    \x1b[1;36m--check-proxies\x1b[0m          Test health & latency across MNNIT proxy pool\n";
    std::cout << "    \x1b[1;36m--set-user <u0> <p0>\x1b[0m     Update proxy credentials in config\n\n";

    std::cout << "\x1b[1;32mAUTOSTART MANAGEMENT:\x1b[0m\n";
    std::cout << "    \x1b[1;36m--install-startup\x1b[0m        Install Task Scheduler autostart (Zero UAC Prompts)\n";
    std::cout << "    \x1b[1;36m--uninstall-startup\x1b[0m      Remove Task Scheduler autostart\n";
    std::cout << "    \x1b[1;36m--install-service\x1b[0m        Install Windows System Service (Starts at Boot)\n";
    std::cout << "    \x1b[1;36m--uninstall-service\x1b[0m      Remove Windows System Service\n\n";

    std::cout << "\x1b[1;32mCONFIGURATION FILE (TOML):\x1b[0m\n";
    std::cout << "    Default Path: \x1b[1;33m%USERPROFILE%\\.config\\proxyman\\config.toml\x1b[0m\n\n";

    std::cout << "\x1b[1;32mMNNIT DEFAULT PROXY POOL:\x1b[0m\n";
    std::cout << "    172.31.100.25:3128   (Primary Gateway)\n";
    std::cout << "    172.31.100.27:3128   (Secondary Failover)\n";
    std::cout << "    172.31.102.29:3128   (Secondary Failover)\n";
    std::cout << "    172.31.103.29:3128   (Hostel Failover)\n";
    std::cout << "    172.31.100.14:3128   (EDC Failover)\n";
    std::cout << "===================================================================\n";
}
