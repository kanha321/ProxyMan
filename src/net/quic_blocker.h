#pragma once

#include <thread>
#include <atomic>

void RunQuicBlocker();

class StoppableQuicBlocker {
public:
    StoppableQuicBlocker() = default;
    ~StoppableQuicBlocker();

    StoppableQuicBlocker(const StoppableQuicBlocker&) = delete;
    StoppableQuicBlocker& operator=(const StoppableQuicBlocker&) = delete;

    void Start();
    void Stop();
    bool IsRunning() const { return m_running.load(); }

private:
    void DropLoop();

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopping{false};
    void* m_handle = nullptr;
    std::thread m_thread;
};
