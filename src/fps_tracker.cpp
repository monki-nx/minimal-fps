#include "fps_tracker.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <evntrace.h>
#include <PresentMonTraceConsumer.hpp>
#include <PresentMonTraceSession.hpp>

#include <atomic>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {
const wchar_t* kSessionName = L"minimal-fps-etw";
}

struct FpsTracker::Impl
{
    PMTraceConsumer consumer;
    PMTraceSession session;
    std::thread processThread;   // bloquea en ProcessTrace()
    std::thread dequeueThread;   // vacía el ring buffer del consumidor
    std::atomic<bool> running{ false };
    std::mutex mtx;
    std::unordered_map<uint32_t, std::deque<uint64_t>> presents; // pid -> QPC de cada present
    uint64_t qpcFrequency = 1;

    void Drain()
    {
        std::vector<std::shared_ptr<PresentEvent>> events;
        consumer.DequeuePresentEvents(events);
        if (events.empty()) return;
        std::lock_guard lock{ mtx };
        for (auto& e : events) {
            presents[e->ProcessId].push_back(e->PresentStartTime);
        }
    }

    void Prune(uint64_t cutoff)
    {
        for (auto it = presents.begin(); it != presents.end();) {
            auto& q = it->second;
            while (!q.empty() && q.front() < cutoff) q.pop_front();
            if (q.empty()) it = presents.erase(it); else ++it;
        }
    }
};

FpsTracker::FpsTracker() : impl_{ std::make_unique<Impl>() } {}
FpsTracker::~FpsTracker() { Stop(); }

bool FpsTracker::Start(std::wstring& error)
{
    auto& im = *impl_;
    // Solo nos interesa contar presents: sin seguimiento de display, GPU ni input
    // para habilitar la menor cantidad de proveedores ETW posible.
    im.consumer.mTrackDisplay = false;
    im.consumer.mTrackGPU = false;
    im.consumer.mTrackGPUVideo = false;
    im.consumer.mTrackInput = false;
    im.consumer.mTrackFrameType = false;
    im.session.mPMConsumer = &im.consumer;

    ULONG status = im.session.Start(nullptr, kSessionName);
    if (status == ERROR_ALREADY_EXISTS) {
        // quedó una sesión huérfana de una ejecución anterior
        StopNamedTraceSession(kSessionName);
        status = im.session.Start(nullptr, kSessionName);
    }
    if (status != ERROR_SUCCESS) {
        if (status == ERROR_ACCESS_DENIED)
            error = L"No se pudo iniciar la sesión ETW: acceso denegado.\nEjecuta minimal-fps como administrador.";
        else
            error = L"No se pudo iniciar la sesión ETW (código " + std::to_wstring(status) + L").";
        return false;
    }
    im.qpcFrequency = uint64_t(im.session.mTimestampFrequency.QuadPart);
    if (im.qpcFrequency == 0) im.qpcFrequency = 1;
    im.running = true;

    im.processThread = std::thread([this] {
        SetThreadDescription(GetCurrentThread(), L"minimal-fps etw");
        auto handle = impl_->session.mTraceHandle;
        ProcessTrace(&handle, 1, nullptr, nullptr);
    });
    im.dequeueThread = std::thread([this] {
        while (impl_->running) {
            impl_->Drain();
            Sleep(50);
        }
    });
    return true;
}

void FpsTracker::Stop()
{
    auto& im = *impl_;
    if (!im.running) return;
    im.running = false;
    im.session.Stop();          // hace que ProcessTrace() retorne
    if (im.processThread.joinable()) im.processThread.join();
    if (im.dequeueThread.joinable()) im.dequeueThread.join();
}

double FpsTracker::GetFps(uint32_t pid, double windowSec)
{
    auto& im = *impl_;
    if (!im.running || windowSec <= 0.0) return 0.0;
    LARGE_INTEGER now; QueryPerformanceCounter(&now);
    const uint64_t cutoff = uint64_t(now.QuadPart) - uint64_t(windowSec * double(im.qpcFrequency));
    std::lock_guard lock{ im.mtx };
    im.Prune(cutoff);
    auto it = im.presents.find(pid);
    if (it == im.presents.end()) return 0.0;
    return double(it->second.size()) / windowSec;
}
