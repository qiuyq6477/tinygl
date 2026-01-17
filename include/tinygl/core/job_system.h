#pragma once

#include <vector>
#include <thread>
#include <atomic>
#include <functional>
#include <condition_variable>
#include <mutex>

namespace tinygl {

class JobSystem {
public:
    JobSystem();
    ~JobSystem();

    void Init();
    void Shutdown();

    // ParallelFor: Executes func(i) for i in [start, end)
    // The range is split into small chunks and processed by worker threads.
    // This function blocks until all tasks are complete.
    void ParallelFor(int start, int end, std::function<void(int)> func);

private:
    void WorkerLoop(int threadIndex);

    std::vector<std::thread> m_workers;
    
    // Synchronization
    std::mutex m_mutex;
    std::condition_variable m_cvWake;
    std::condition_variable m_cvDone;
    
    // Current Job State
    std::atomic<int> m_nextIndex{0};
    std::atomic<int> m_endIndex{0};
    std::atomic<int> m_activeWorkers{0};
    std::function<void(int)> m_currentFunc;
    
    bool m_shutdown = false;
    bool m_jobActive = false;
};

} // namespace tinygl
