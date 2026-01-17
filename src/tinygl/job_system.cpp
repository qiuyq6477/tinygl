#include <tinygl/core/job_system.h>
#include <tinygl/base/log.h>

namespace tinygl {

JobSystem::JobSystem() {}

JobSystem::~JobSystem() {
    Shutdown();
}

void JobSystem::Init() {
    int numThreads = std::thread::hardware_concurrency();
    if (numThreads == 0) numThreads = 4;
    
    // Reserve thread 0 for main thread participation if needed, but here we just spawn N-1 or N workers
    // Let's spawn N workers for simplicity, and main thread will wait.
    // Optimization: Main thread could also help work.
    
    for (int i = 0; i < numThreads; ++i) {
        m_workers.emplace_back(&JobSystem::WorkerLoop, this, i);
    }
    LOG_INFO("JobSystem initialized with " + std::to_string(numThreads) + " threads.");
}

void JobSystem::Shutdown() {
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_shutdown = true;
    }
    m_cvWake.notify_all();
    
    for (auto& t : m_workers) {
        if (t.joinable()) t.join();
    }
    m_workers.clear();
}

void JobSystem::ParallelFor(int start, int end, std::function<void(int)> func) {
    if (start >= end) return;

    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_nextIndex = start;
        m_endIndex = end;
        m_currentFunc = func;
        m_jobActive = true;
        m_activeWorkers = 0; // Reset counter
    }
    m_cvWake.notify_all();

    // Main thread waits (and optionally helps - TODO)
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cvDone.wait(lock, [this] { 
            return m_nextIndex >= m_endIndex && m_activeWorkers == 0; 
        });
        m_jobActive = false;
    }
}

void JobSystem::WorkerLoop(int threadIndex) {
    while (true) {
        std::function<void(int)> func;

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cvWake.wait(lock, [this] { return m_shutdown || (m_jobActive && m_nextIndex < m_endIndex); });
            
            if (m_shutdown) return;
            
            func = m_currentFunc;
            m_activeWorkers++;
        }

        // Loop until no more work
        while (true) {
            int idx = m_nextIndex++;
            if (idx >= m_endIndex) {
                break; // No more work
            }
            func(idx);
        }

        // Work done for this batch
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_activeWorkers--;
        }
        m_cvDone.notify_all();
    }
}

} // namespace tinygl
