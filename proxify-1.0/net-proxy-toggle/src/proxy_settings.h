#ifndef PROXY_SETTINGS_H
#define PROXY_SETTINGS_H

#include <string>

bool SetSystemProxy(const std::string& proxyHostPort);
bool ClearSystemProxy();
bool GetSystemProxy(bool& enabled, std::string& proxyHostPort);

#endif // PROXY_SETTINGS_H
