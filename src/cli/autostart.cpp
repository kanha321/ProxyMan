#include "cli/autostart.h"
#include <windows.h>
#include <iostream>
#include <string>

bool InstallStartupTask() {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) return false;

    std::wstring cmd = L"schtasks /create /tn \"ProxyMan\" /tr \"\\\"";
    cmd += exePath;
    cmd += L"\\\"\" /sc ONLOGON /rl HIGHEST /f";

    int ret = _wsystem(cmd.c_str());
    if (ret == 0) {
        std::cout << "\n==========================================================\n";
        std::cout << "  ProxyMan Task Scheduler Autostart Registered! ✨\n";
        std::cout << "==========================================================\n";
        std::cout << "Method:          Task Scheduler (/sc ONLOGON /rl HIGHEST)\n";
        std::cout << "Execution:       Runs automatically at user logon\n";
        std::cout << "Privileges:      Highest Administrator (No UAC Prompts!)\n\n";
        return true;
    } else {
        std::cerr << "[Main] schtasks failed with error code: " << ret << std::endl;
        return false;
    }
}

bool UninstallStartupTask() {
    int ret = _wsystem(L"schtasks /delete /tn \"ProxyMan\" /f");
    if (ret == 0) {
        std::cout << "[Main] ProxyMan Task Scheduler autostart removed successfully.\n";
        return true;
    } else {
        std::cerr << "[Main] Failed to remove Task Scheduler autostart.\n";
        return false;
    }
}

bool InstallWindowsService() {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) return false;

    std::wstring cmd = L"sc.exe create ProxyMan binPath= \"\\\"";
    cmd += exePath;
    cmd += L"\\\"\" start= auto DisplayName= \"ProxyMan Transparent Proxy Engine\"";

    int ret = _wsystem(cmd.c_str());
    if (ret == 0) {
        _wsystem(L"sc.exe description ProxyMan \"ProxyMan Network-Aware Transparent Proxy Engine\"");
        std::cout << "\n==========================================================\n";
        std::cout << "  ProxyMan Windows System Service Registered! 🛡️\n";
        std::cout << "==========================================================\n";
        std::cout << "Method:          Windows System Service (NT AUTHORITY\\SYSTEM)\n";
        std::cout << "Execution:       Starts at System Boot (Before User Logon Screen)\n\n";
        return true;
    } else {
        std::cerr << "[Main] sc.exe create failed with error code: " << ret << std::endl;
        return false;
    }
}

bool UninstallWindowsService() {
    _wsystem(L"sc.exe stop ProxyMan");
    int ret = _wsystem(L"sc.exe delete ProxyMan");
    if (ret == 0) {
        std::cout << "[Main] ProxyMan Windows System Service removed successfully.\n";
        return true;
    } else {
        std::cerr << "[Main] Failed to remove Windows System Service.\n";
        return false;
    }
}
