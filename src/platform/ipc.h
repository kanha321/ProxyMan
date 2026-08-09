#pragma once

#include <windows.h>
#include <tlhelp32.h>

HANDLE CreateGlobalShutdownEvent();
HANDLE CreateGlobalMutex();
bool KillRunningProxyManProcesses();
