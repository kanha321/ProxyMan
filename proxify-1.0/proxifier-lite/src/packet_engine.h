#ifndef PACKET_ENGINE_H
#define PACKET_ENGINE_H

#include "config.h"
#include "conn_table.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

void RunPacketEngineCaptureOnly();
void RunPacketEngine(ConnTable& table, const Config& cfg);

class StoppablePacketEngine {
public:
    StoppablePacketEngine() = default;
    ~StoppablePacketEngine();

    StoppablePacketEngine(const StoppablePacketEngine&) = delete;
    StoppablePacketEngine& operator=(const StoppablePacketEngine&) = delete;

    void Start(ConnTable& table, const Config& cfg);
    void Stop();
    bool IsRunning() const { return m_running.load(); }

private:
    void CaptureLoop(ConnTable* table, const Config* cfg);
    void SweepLoop(ConnTable* table);

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_stopping{false};
    void* m_handle = nullptr;  // HANDLE (WinDivert)
    std::thread m_captureThread;
    std::thread m_sweepThread;
    std::mutex m_sweepMutex;
    std::condition_variable m_sweepCv;
};

#endif // PACKET_ENGINE_H
