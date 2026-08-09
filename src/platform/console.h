#pragma once

#include <windows.h>
#include <atomic>
#include <cstdio>
#include <io.h>
#include <fcntl.h>
#include <iostream>

extern std::atomic<bool> g_shutdownRequested;

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType);
void EnsureConsoleOutput();
