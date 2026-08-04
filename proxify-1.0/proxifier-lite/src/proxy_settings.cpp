#include "proxy_settings.h"

#include <windows.h>
#include <wininet.h>
#include <iostream>
#include <vector>

#pragma comment(lib, "wininet.lib")

static const wchar_t* kInternetSettingsSubKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings";
static const wchar_t* kEnvironmentSubKey = L"Environment";

typedef BOOL(WINAPI* DnsFlushResolverCacheFunc)(VOID);

static void FlushWindowsDnsCache() {
    HMODULE hDns = LoadLibraryW(L"dnsapi.dll");
    if (hDns) {
        auto pFlush = reinterpret_cast<DnsFlushResolverCacheFunc>(GetProcAddress(hDns, "DnsFlushResolverCache"));
        if (pFlush) {
            pFlush();
            std::cout << "[ProxySettings] Windows DNS Cache flushed." << std::endl;
        }
        FreeLibrary(hDns);
    }
}

static std::string WStringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), NULL, 0, NULL, NULL);
    std::string str(sizeNeeded, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), static_cast<int>(wstr.size()), &str[0], sizeNeeded, NULL, NULL);
    return str;
}

static std::wstring StringToWString(const std::string& str) {
    if (str.empty()) return L"";
    int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), NULL, 0);
    std::wstring wstr(sizeNeeded, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), static_cast<int>(str.size()), &wstr[0], sizeNeeded);
    return wstr;
}

static void SetUserEnvVar(const wchar_t* name, const wchar_t* value) {
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kEnvironmentSubKey, 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        if (value && wcslen(value) > 0) {
            RegSetValueExW(hKey, name, 0, REG_SZ, reinterpret_cast<const BYTE*>(value), static_cast<DWORD>((wcslen(value) + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(hKey, name);
        }
        RegCloseKey(hKey);
    }
}

bool SetSystemProxy(const std::string& proxyHostPort) {
    HKEY hKey;
    LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, kInternetSettingsSubKey, 0, KEY_SET_VALUE, &hKey);
    if (lRes != ERROR_SUCCESS) {
        std::cerr << "[ProxySettings] Failed to open registry key for proxy settings. Error: " << lRes << std::endl;
        return false;
    }

    DWORD dwEnable = 1;
    RegSetValueExW(hKey, L"ProxyEnable", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwEnable), sizeof(dwEnable));

    std::wstring wProxyHostPort = StringToWString(proxyHostPort);
    RegSetValueExW(hKey, L"ProxyServer", 0, REG_SZ, reinterpret_cast<const BYTE*>(wProxyHostPort.c_str()), static_cast<DWORD>((wProxyHostPort.size() + 1) * sizeof(wchar_t)));

    const wchar_t* wOverride = L"localhost;127.*;10.*;172.16.*;172.17.*;172.18.*;172.19.*;172.20.*;172.21.*;172.22.*;172.23.*;172.24.*;172.25.*;172.26.*;172.27.*;172.28.*;172.29.*;172.30.*;172.31.*;192.168.*;<local>";
    RegSetValueExW(hKey, L"ProxyOverride", 0, REG_SZ, reinterpret_cast<const BYTE*>(wOverride), static_cast<DWORD>((wcslen(wOverride) + 1) * sizeof(wchar_t)));

    RegCloseKey(hKey);

    // Also set HTTP_PROXY and HTTPS_PROXY environment variables for Go/Antigravity/Node/Python apps
    std::string proxyUrl = "http://" + proxyHostPort;
    std::wstring wProxyUrl = StringToWString(proxyUrl);

    SetUserEnvVar(L"HTTP_PROXY", wProxyUrl.c_str());
    SetUserEnvVar(L"HTTPS_PROXY", wProxyUrl.c_str());
    SetUserEnvVar(L"http_proxy", wProxyUrl.c_str());
    SetUserEnvVar(L"https_proxy", wProxyUrl.c_str());

    SetEnvironmentVariableA("HTTP_PROXY", proxyUrl.c_str());
    SetEnvironmentVariableA("HTTPS_PROXY", proxyUrl.c_str());
    SetEnvironmentVariableA("http_proxy", proxyUrl.c_str());
    SetEnvironmentVariableA("https_proxy", proxyUrl.c_str());

    // Flush Windows DNS Resolver Cache to force apps to resolve DNS via ProxyMan Interceptor
    FlushWindowsDnsCache();

    // Refresh WinINet settings globally
    InternetSetOptionW(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
    InternetSetOptionW(NULL, INTERNET_OPTION_REFRESH, NULL, 0);

    DWORD_PTR dwResult = 0;
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_ABORTIFHUNG, 1000, &dwResult);

    std::cout << "[ProxySettings] System proxy ENABLED -> " << proxyHostPort << std::endl;
    return true;
}

bool ClearSystemProxy() {
    HKEY hKey;
    LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, kInternetSettingsSubKey, 0, KEY_SET_VALUE, &hKey);
    if (lRes != ERROR_SUCCESS) {
        std::cerr << "[ProxySettings] Failed to open registry key for proxy settings. Error: " << lRes << std::endl;
        return false;
    }

    DWORD dwEnable = 0;
    RegSetValueExW(hKey, L"ProxyEnable", 0, REG_DWORD, reinterpret_cast<const BYTE*>(&dwEnable), sizeof(dwEnable));

    RegCloseKey(hKey);

    // Clear environment variables
    SetUserEnvVar(L"HTTP_PROXY", NULL);
    SetUserEnvVar(L"HTTPS_PROXY", NULL);
    SetUserEnvVar(L"http_proxy", NULL);
    SetUserEnvVar(L"https_proxy", NULL);

    SetEnvironmentVariableA("HTTP_PROXY", NULL);
    SetEnvironmentVariableA("HTTPS_PROXY", NULL);
    SetEnvironmentVariableA("http_proxy", NULL);
    SetEnvironmentVariableA("https_proxy", NULL);

    // Flush Windows DNS Resolver Cache
    FlushWindowsDnsCache();

    // Refresh WinINet settings globally
    InternetSetOptionW(NULL, INTERNET_OPTION_SETTINGS_CHANGED, NULL, 0);
    InternetSetOptionW(NULL, INTERNET_OPTION_REFRESH, NULL, 0);

    DWORD_PTR dwResult = 0;
    SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_ABORTIFHUNG, 1000, &dwResult);

    std::cout << "[ProxySettings] System proxy DISABLED." << std::endl;
    return true;
}

bool GetSystemProxy(bool& enabled, std::string& proxyHostPort) {
    HKEY hKey;
    LONG lRes = RegOpenKeyExW(HKEY_CURRENT_USER, kInternetSettingsSubKey, 0, KEY_QUERY_VALUE, &hKey);
    if (lRes != ERROR_SUCCESS) {
        return false;
    }

    DWORD dwEnable = 0;
    DWORD dwSize = sizeof(dwEnable);
    if (RegQueryValueExW(hKey, L"ProxyEnable", NULL, NULL, reinterpret_cast<BYTE*>(&dwEnable), &dwSize) == ERROR_SUCCESS) {
        enabled = (dwEnable != 0);
    } else {
        enabled = false;
    }

    wchar_t buf[512] = {0};
    dwSize = sizeof(buf);
    if (RegQueryValueExW(hKey, L"ProxyServer", NULL, NULL, reinterpret_cast<BYTE*>(buf), &dwSize) == ERROR_SUCCESS) {
        proxyHostPort = WStringToString(std::wstring(buf));
    } else {
        proxyHostPort.clear();
    }

    RegCloseKey(hKey);
    return true;
}
