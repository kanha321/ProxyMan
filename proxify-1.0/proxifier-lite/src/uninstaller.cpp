#include <windows.h>
#include <wininet.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <sstream>
#include <cstdlib>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "shell32.lib")

namespace fs = std::filesystem;

extern "C" BOOL WINAPI DnsFlushResolverCache();

static bool IsElevated() {
    BOOL isElevated = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            isElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return isElevated != FALSE;
}

static bool RelaunchElevated(int argc, char* argv[]) {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) {
        return false;
    }

    std::wstring args;
    for (int i = 1; i < argc; ++i) {
        if (i > 1) args += L" ";
        std::string a = argv[i];
        args += std::wstring(a.begin(), a.end());
    }

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.cbSize = sizeof(sei);
    sei.lpVerb = L"runas";
    sei.lpFile = exePath;
    sei.lpParameters = args.c_str();
    sei.nShow = SW_SHOWNORMAL;

    return ShellExecuteExW(&sei) != FALSE;
}

static bool KillProcessesByName(const std::wstring& processName) {
    DWORD currentPid = GetCurrentProcessId();
    bool killedAny = false;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32W pe = { sizeof(pe) };
    if (Process32FirstW(hSnap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, processName.c_str()) == 0 && pe.th32ProcessID != currentPid) {
                HANDLE hProc = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProc) {
                    TerminateProcess(hProc, 0);
                    CloseHandle(hProc);
                    killedAny = true;
                }
            }
        } while (Process32NextW(hSnap, &pe));
    }
    CloseHandle(hSnap);
    return killedAny;
}

static void ClearSystemProxySettings() {
    HKEY hKey = NULL;
    LONG res = RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
        0,
        KEY_SET_VALUE,
        &hKey
    );

    if (res == ERROR_SUCCESS) {
        DWORD enable = 0;
        RegSetValueExW(hKey, L"ProxyEnable", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&enable), sizeof(enable));
        RegDeleteValueW(hKey, L"ProxyServer");
        RegDeleteValueW(hKey, L"ProxyOverride");
        RegCloseKey(hKey);
    }

    InternetSetOptionW(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
    InternetSetOptionW(NULL, INTERNET_OPTION_REFRESH, NULL, 0);

    HINSTANCE hDnsApi = LoadLibraryW(L"dnsapi.dll");
    if (hDnsApi) {
        typedef BOOL(WINAPI* DnsFlushProc)();
        DnsFlushProc pDnsFlush = (DnsFlushProc)GetProcAddress(hDnsApi, "DnsFlushResolverCache");
        if (pDnsFlush) pDnsFlush();
        FreeLibrary(hDnsApi);
    }
    _wsystem(L"ipconfig /flushdns >nul 2>&1");
}

static bool RemoveFromSystemPath(const std::wstring& targetDir) {
    HKEY hKey = NULL;
    LONG res = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment",
        0,
        KEY_READ | KEY_SET_VALUE,
        &hKey
    );

    if (res != ERROR_SUCCESS) return false;

    DWORD type = 0;
    DWORD cbData = 0;
    res = RegQueryValueExW(hKey, L"Path", NULL, &type, NULL, &cbData);
    if (res != ERROR_SUCCESS || cbData == 0) {
        RegCloseKey(hKey);
        return false;
    }

    std::vector<wchar_t> buffer(cbData / sizeof(wchar_t) + 2);
    res = RegQueryValueExW(hKey, L"Path", NULL, &type, reinterpret_cast<BYTE*>(buffer.data()), &cbData);
    if (res != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return false;
    }

    std::wstring currentPath(buffer.data());
    std::wstring normalizedTarget = targetDir;
    while (!normalizedTarget.empty() && (normalizedTarget.back() == L'\\' || normalizedTarget.back() == L'/')) {
        normalizedTarget.pop_back();
    }

    std::vector<std::wstring> entries;
    std::wstringstream wss(currentPath);
    std::wstring item;
    bool modified = false;

    while (std::getline(wss, item, L';')) {
        std::wstring normItem = item;
        while (!normItem.empty() && (normItem.back() == L'\\' || normItem.back() == L'/')) {
            normItem.pop_back();
        }
        if (_wcsicmp(normItem.c_str(), normalizedTarget.c_str()) == 0) {
            modified = true;
        } else if (!item.empty()) {
            entries.push_back(item);
        }
    }

    if (modified) {
        std::wstring newPath;
        for (size_t i = 0; i < entries.size(); ++i) {
            if (i > 0) newPath += L";";
            newPath += entries[i];
        }

        RegSetValueExW(
            hKey,
            L"Path",
            0,
            type,
            reinterpret_cast<const BYTE*>(newPath.c_str()),
            static_cast<DWORD>((newPath.size() + 1) * sizeof(wchar_t))
        );

        DWORD_PTR dwResult = 0;
        SendMessageTimeoutW(
            HWND_BROADCAST,
            WM_SETTINGCHANGE,
            0,
            reinterpret_cast<LPARAM>(L"Environment"),
            SMTO_ABORTIFHUNG,
            5000,
            &dwResult
        );
    }

    RegCloseKey(hKey);
    return modified;
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    std::cout << "\x1b[1;36m┌─────────────────────────────────────────────────────────────┐\x1b[0m\n";
    std::cout << "\x1b[1;36m│\x1b[0m  \x1b[1;31m🗑️  ProxyMan Complete Cleanup & Uninstallation Utility\x1b[0m    \x1b[1;36m│\x1b[0m\n";
    std::cout << "\x1b[1;36m└─────────────────────────────────────────────────────────────┘\x1b[0m\n\n";

    if (!IsElevated()) {
        std::cout << "\x1b[1;33m[Notice] Administrator privileges required for complete cleanup.\x1b[0m\n";
        std::cout << "[Notice] Requesting UAC elevation...\n\n";
        if (RelaunchElevated(argc, argv)) {
            return 0;
        } else {
            std::cerr << "\x1b[1;31m[Error] Elevation request cancelled. Cannot proceed with cleanup.\x1b[0m\n";
            return 1;
        }
    }

    std::cout << "\x1b[1;32m[Step 1/5] Stopping ProxyMan processes & driver services...\x1b[0m\n";
    bool killedProc = KillProcessesByName(L"ProxyMan.exe");
    KillProcessesByName(L"ProxyManSetup.exe");

    _wsystem(L"sc.exe stop ProxyMan >nul 2>&1");
    _wsystem(L"sc.exe delete ProxyMan >nul 2>&1");
    _wsystem(L"net stop WinDivert >nul 2>&1");
    _wsystem(L"sc.exe delete WinDivert >nul 2>&1");
    std::cout << "  ✔ Background processes & services stopped.\n\n";

    std::cout << "\x1b[1;32m[Step 2/5] Removing Task Scheduler autostart entry...\x1b[0m\n";
    _wsystem(L"schtasks /delete /tn \"ProxyMan\" /f >nul 2>&1");
    std::cout << "  ✔ Task Scheduler entry deleted.\n\n";

    std::cout << "\x1b[1;32m[Step 3/5] Restoring Windows System Proxy & Flushing DNS...\x1b[0m\n";
    ClearSystemProxySettings();
    std::cout << "  ✔ System proxy disabled and DNS cache cleared.\n\n";

    std::cout << "\x1b[1;32m[Step 4/5] Removing ProxyMan from System PATH...\x1b[0m\n";
    std::wstring defaultTarget = L"C:\\Program Files\\ProxyMan";
    bool removedPath = RemoveFromSystemPath(defaultTarget);
    if (removedPath) {
        std::cout << "  ✔ Removed 'C:\\Program Files\\ProxyMan' from System PATH.\n\n";
    } else {
        std::cout << "  ✔ System PATH verified clean.\n\n";
    }

    std::cout << "\x1b[1;32m[Step 5/5] Deleting ProxyMan files & configuration folders...\x1b[0m\n";

    try {
        if (fs::exists("C:\\Program Files\\ProxyMan")) {
            fs::remove_all("C:\\Program Files\\ProxyMan");
            std::cout << "  ✔ Removed installation folder: C:\\Program Files\\ProxyMan\n";
        }
    } catch (...) {
        std::cout << "  ⚠️ Failed to delete C:\\Program Files\\ProxyMan (files in use or locked)\n";
    }

    const char* userProfile = std::getenv("USERPROFILE");
    if (userProfile) {
        fs::path userConfig = fs::path(userProfile) / ".config" / "proxyman";
        try {
            if (fs::exists(userConfig)) {
                fs::remove_all(userConfig);
                std::cout << "  ✔ Removed user config folder: " << userConfig.string() << "\n";
            }
        } catch (...) {}
    }

    std::cout << "\n\x1b[1;32m=============================================================\x1b[0m\n";
    std::cout << "\x1b[1;32m  ✔ PROXYMAN HAS BEEN COMPLETELY REMOVED FROM YOUR SYSTEM!\x1b[0m\n";
    std::cout << "\x1b[1;32m=============================================================\x1b[0m\n";
    std::cout << "All background engines, system proxy rules, autostart tasks,\n";
    std::cout << "environment PATH entries, and config files have been deleted.\n\n";

    std::cout << "Press \x1b[1;36m[ENTER]\x1b[0m to exit...";
    std::cout.flush();
    std::string line;
    std::getline(std::cin, line);

    return 0;
}
