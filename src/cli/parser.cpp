#include "cli/parser.h"
#include "cli/help.h"
#include "cli/status.h"
#include "cli/logs.h"
#include "cli/stop.h"
#include "cli/stats.h"
#include "cli/diagnostics.h"
#include "cli/autostart.h"
#include "version.h"
#include "platform/elevation.h"
#include <iostream>

int ParseAndDispatch(int argc, char* argv[], const Config& cfg) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help" || arg == "/?") {
            PrintHelp();
            return 0;
        } else if (arg == "-v" || arg == "--version") {
            std::cout << PROXYMAN_VERSION_FULL << "\n";
            return 0;
        } else if (arg == "--status") {
            ShowStatus(cfg);
            return 0;
        } else if (arg == "--logs") {
            bool follow = (i + 1 < argc && (std::string(argv[i + 1]) == "-f" || std::string(argv[i + 1]) == "--follow"));
            ShowLogs(follow);
            return 0;
        } else if (arg == "--stop") {
            return HandleStop();
        } else if (arg == "--stats") {
            return HandleStats();
        }
    }
    return -1;
}

int ParseElevatedCommands(int argc, char* argv[], Config& cfg, const std::string& configPath) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--install-startup") {
            InstallStartupTask();
            return 0;
        } else if (arg == "--uninstall-startup") {
            UninstallStartupTask();
            return 0;
        } else if (arg == "--install-service") {
            InstallWindowsService();
            return 0;
        } else if (arg == "--uninstall-service") {
            UninstallWindowsService();
            return 0;
        } else if (arg == "--check-proxies" || arg == "--speedtest") {
            RunSpeedTest(cfg);
            return 0;
        } else if (arg == "--set-user" && i + 2 < argc) {
            cfg.proxyUser = argv[i + 1];
            cfg.proxyPass = argv[i + 2];
            SaveConfigToFile(configPath, cfg);
            std::cout << "[Config] EDC credentials updated successfully: user=" << cfg.proxyUser << "\n";
            return 0;
        }
    }
    return -1;
}
