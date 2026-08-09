#include "process_utils.h"

#include <windows.h>
#include <iphlpapi.h>
#include <psapi.h>
#include <filesystem>
#include <vector>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "psapi.lib")

namespace fs = std::filesystem;

ProcessInfo GetProcessInfoFromLocalPort(uint16_t localPort) {
    ProcessInfo info;
    info.pid = 0;
    info.name = "System";

    DWORD size = 0;
    GetExtendedTcpTable(NULL, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0);
    if (size == 0) return info;

    std::vector<BYTE> buffer(size);
    PMIB_TCPTABLE_OWNER_PID pTcpTable = reinterpret_cast<PMIB_TCPTABLE_OWNER_PID>(buffer.data());

    if (GetExtendedTcpTable(pTcpTable, &size, FALSE, AF_INET, TCP_TABLE_OWNER_PID_ALL, 0) == NO_ERROR) {
        for (DWORD i = 0; i < pTcpTable->dwNumEntries; ++i) {
            uint16_t port = ntohs(static_cast<u_short>(pTcpTable->table[i].dwLocalPort));
            if (port == localPort) {
                info.pid = pTcpTable->table[i].dwOwningPid;
                break;
            }
        }
    }

    if (info.pid == 0) return info;
    if (info.pid == 4) { info.name = "System"; return info; }

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, info.pid);
    if (hProcess) {
        char path[MAX_PATH] = {0};
        DWORD pathSize = MAX_PATH;
        if (QueryFullProcessImageNameA(hProcess, 0, path, &pathSize)) {
            info.name = fs::path(path).filename().string();
        }
        CloseHandle(hProcess);
    }

    return info;
}
