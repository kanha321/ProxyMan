#include "platform/console.h"

BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    if (ctrlType == CTRL_C_EVENT || ctrlType == CTRL_BREAK_EVENT || ctrlType == CTRL_CLOSE_EVENT) {
        std::printf("\n[Main] Shutdown signal received...\n");
        g_shutdownRequested = true;
        return TRUE;
    }
    return FALSE;
}

void EnsureConsoleOutput() {
    if (AttachConsole(ATTACH_PARENT_PROCESS)) {
        FILE* fp = nullptr;
        freopen_s(&fp, "CONOUT$", "w", stdout);
        freopen_s(&fp, "CONOUT$", "w", stderr);
        freopen_s(&fp, "CONIN$", "r", stdin);
    } else {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut != NULL && hOut != INVALID_HANDLE_VALUE) {
            int fd = _open_osfhandle(reinterpret_cast<intptr_t>(hOut), _O_TEXT);
            if (fd != -1) {
                FILE* fp = _fdopen(fd, "w");
                if (fp) {
                    *stdout = *fp;
                    setvbuf(stdout, NULL, _IONBF, 0);
                }
            }
        }
    }
    std::cout.clear();
    std::cerr.clear();
}
