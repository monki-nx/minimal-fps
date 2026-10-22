#include "hw_metrics.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <adl_sdk.h>

#include <cstdlib>
#include <cmath>
#include <vector>

#pragma comment(lib, "pdh.lib")

namespace {

// ---------------------------------------------------------------- CPU
struct CpuSampler
{
    ULONGLONG lastIdle = 0, lastKernel = 0, lastUser = 0;
    double percent = 0.0;
    bool primed = false;

    static ULONGLONG ToU64(FILETIME ft) { return (ULONGLONG(ft.dwHighDateTime) << 32) | ft.dwLowDateTime; }

    void Poll()
    {
        FILETIME idle, kernel, user;
        if (!GetSystemTimes(&idle, &kernel, &user)) return;
        const auto i = ToU64(idle), k = ToU64(kernel), u = ToU64(user);
        if (primed) {
            const auto total = (k - lastKernel) + (u - lastUser);   // kernel incluye idle
            const auto busy = total - (i - lastIdle);
            percent = total ? 100.0 * double(busy) / double(total) : 0.0;
        }
        lastIdle = i; lastKernel = k; lastUser = u; primed = true;
    }
};

// ---------------------------------------------------------------- AMD (ADL)
void* __stdcall AdlAlloc(int size) { return malloc(size_t(size)); }

struct AdlGpu
{
    HMODULE dll = nullptr;
    ADL_CONTEXT_HANDLE ctx = nullptr;
    int adapter = -1;
    bool hasUse = false, hasTemp = false;
    double use = 0.0, temp = 0.0;

    int (*Create)(ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE*) = nullptr;
    int (*Destroy)(ADL_CONTEXT_HANDLE) = nullptr;
    int (*NumAdapters)(ADL_CONTEXT_HANDLE, int*) = nullptr;
    int (*AdapterInfoGet)(ADL_CONTEXT_HANDLE, LPAdapterInfo, int) = nullptr;
    int (*AdapterActive)(ADL_CONTEXT_HANDLE, int, int*) = nullptr;
    int (*PMLogGet)(ADL_CONTEXT_HANDLE, int, ADLPMLogDataOutput*) = nullptr;
    int (*OdnStatus)(ADL_CONTEXT_HANDLE, int, ADLODNPerformanceStatus*) = nullptr;
    int (*OdnTemp)(ADL_CONTEXT_HANDLE, int, int, int*) = nullptr;
    int (*Od6Status)(ADL_CONTEXT_HANDLE, int, ADLOD6CurrentStatus*) = nullptr;
    int (*Od6Temp)(ADL_CONTEXT_HANDLE, int, int*) = nullptr;
    int (*Od5Activity)(ADL_CONTEXT_HANDLE, int, ADLPMActivity*) = nullptr;
    int (*Od5Temp)(ADL_CONTEXT_HANDLE, int, int, ADLTemperature*) = nullptr;

    template<class F> void Load(F& f, const char* name) { f = reinterpret_cast<F>(GetProcAddress(dll, name)); }

    bool Init()
    {
        dll = LoadLibraryA("atiadlxx.dll");
        if (!dll) dll = LoadLibraryA("atiadlxy.dll");
        if (!dll) return false;
        Load(Create, "ADL2_Main_Control_Create");
        Load(Destroy, "ADL2_Main_Control_Destroy");
        Load(NumAdapters, "ADL2_Adapter_NumberOfAdapters_Get");
        Load(AdapterInfoGet, "ADL2_Adapter_AdapterInfo_Get");
        Load(AdapterActive, "ADL2_Adapter_Active_Get");
        Load(PMLogGet, "ADL2_New_QueryPMLogData_Get");
        Load(OdnStatus, "ADL2_OverdriveN_PerformanceStatus_Get");
        Load(OdnTemp, "ADL2_OverdriveN_Temperature_Get");
        Load(Od6Status, "ADL2_Overdrive6_CurrentStatus_Get");
        Load(Od6Temp, "ADL2_Overdrive6_Temperature_Get");
        Load(Od5Activity, "ADL2_Overdrive5_CurrentActivity_Get");
        Load(Od5Temp, "ADL2_Overdrive5_Temperature_Get");
        if (!Create || !NumAdapters || !AdapterInfoGet) return false;
        if (Create(AdlAlloc, 1, &ctx) != ADL_OK) return false;

        int count = 0;
        if (NumAdapters(ctx, &count) != ADL_OK || count <= 0) return false;
        std::vector<AdapterInfo> infos(static_cast<size_t>(count));
        if (AdapterInfoGet(ctx, infos.data(), int(sizeof(AdapterInfo) * count)) != ADL_OK) return false;
        for (auto& a : infos) {
            if (a.iVendorID != 1002) continue;
            int active = 0;
            if (AdapterActive && AdapterActive(ctx, a.iAdapterIndex, &active) == ADL_OK && !active) continue;
            adapter = a.iAdapterIndex;
            break;
        }
        if (adapter < 0) {
            for (auto& a : infos) if (a.iVendorID == 1002) { adapter = a.iAdapterIndex; break; }
        }
        if (adapter < 0) return false;

        Poll();
        return hasUse || hasTemp;
    }

    void Poll()
    {
        hasUse = hasTemp = false;
        // 1) PMLog (RDNA y posteriores)
        if (PMLogGet) {
            ADLPMLogDataOutput out{};
            if (PMLogGet(ctx, adapter, &out) == ADL_OK) {
                if (out.sensors[PMLOG_INFO_ACTIVITY_GFX].supported) { use = out.sensors[PMLOG_INFO_ACTIVITY_GFX].value; hasUse = true; }
                if (out.sensors[PMLOG_TEMPERATURE_EDGE].supported) { temp = out.sensors[PMLOG_TEMPERATURE_EDGE].value; hasTemp = true; }
            }
        }
        // 2) OverdriveN (GCN 4/5: Polaris, Vega)
        if (!hasUse && OdnStatus) {
            ADLODNPerformanceStatus st{};
            if (OdnStatus(ctx, adapter, &st) == ADL_OK) { use = st.iGPUActivityPercent; hasUse = true; }
        }
        if (!hasTemp && OdnTemp) {
            int t = 0;
            if (OdnTemp(ctx, adapter, 1, &t) == ADL_OK && t > 0) { temp = t / 1000.0; hasTemp = true; }
        }
        // 3) Overdrive6
        if (!hasUse && Od6Status) {
            ADLOD6CurrentStatus st{};
            if (Od6Status(ctx, adapter, &st) == ADL_OK) { use = st.iActivityPercent; hasUse = true; }
        }
        if (!hasTemp && Od6Temp) {
            int t = 0;
            if (Od6Temp(ctx, adapter, &t) == ADL_OK && t > 0) { temp = t / 1000.0; hasTemp = true; }
        }
        // 4) Overdrive5
        if (!hasUse && Od5Activity) {
            ADLPMActivity act{}; act.iSize = sizeof(act);
            if (Od5Activity(ctx, adapter, &act) == ADL_OK) { use = act.iActivityPercent; hasUse = true; }
        }
        if (!hasTemp && Od5Temp) {
            ADLTemperature t{}; t.iSize = sizeof(t);
            if (Od5Temp(ctx, adapter, 0, &t) == ADL_OK && t.iTemperature > 0) { temp = t.iTemperature / 1000.0; hasTemp = true; }
        }
    }

    ~AdlGpu()
    {
        if (Destroy && ctx) Destroy(ctx);
        if (dll) FreeLibrary(dll);
    }
};

// ---------------------------------------------------------------- NVIDIA (NVML)
struct NvmlGpu
{
    using Device = void*;
    struct Utilization { unsigned gpu, memory; };
    HMODULE dll = nullptr;
    Device dev = nullptr;
    bool hasUse = false, hasTemp = false;
    double use = 0.0, temp = 0.0;

    int (*Init)() = nullptr;
    int (*Shutdown)() = nullptr;
    int (*GetHandle)(unsigned, Device*) = nullptr;
    int (*GetTemp)(Device, int, unsigned*) = nullptr;
    int (*GetUtil)(Device, Utilization*) = nullptr;

    template<class F> void Load(F& f, const char* name) { f = reinterpret_cast<F>(GetProcAddress(dll, name)); }

    bool Open()
    {
        dll = LoadLibraryA("nvml.dll");
        if (!dll) {
            char path[MAX_PATH]{};
            if (ExpandEnvironmentStringsA("%ProgramW6432%\\NVIDIA Corporation\\NVSMI\\nvml.dll", path, MAX_PATH))
                dll = LoadLibraryA(path);
        }
        if (!dll) return false;
        Load(Init, "nvmlInit_v2");
        Load(Shutdown, "nvmlShutdown");
        Load(GetHandle, "nvmlDeviceGetHandleByIndex_v2");
        Load(GetTemp, "nvmlDeviceGetTemperature");
        Load(GetUtil, "nvmlDeviceGetUtilizationRates");
        if (!Init || !GetHandle) return false;
        if (Init() != 0) return false;
        if (GetHandle(0, &dev) != 0) return false;
        Poll();
        return hasUse || hasTemp;
    }

    void Poll()
    {
        hasUse = hasTemp = false;
        if (!dev) return;
        if (GetUtil) { Utilization u{}; if (GetUtil(dev, &u) == 0) { use = u.gpu; hasUse = true; } }
        if (GetTemp) { unsigned t = 0; if (GetTemp(dev, 0 /*NVML_TEMPERATURE_GPU*/, &t) == 0) { temp = t; hasTemp = true; } }
    }

    ~NvmlGpu()
    {
        if (Shutdown && dev) Shutdown();
        if (dll) FreeLibrary(dll);
    }
};

// ---------------------------------------------------------------- PDH (uso de GPU genÃ©rico)
struct PdhGpu
{
    PDH_HQUERY query = nullptr;
    PDH_HCOUNTER counter = nullptr;
    bool hasUse = false;
    double use = 0.0;

    bool Open()
    {
        if (PdhOpenQueryW(nullptr, 0, &query) != ERROR_SUCCESS) return false;
        if (PdhAddEnglishCounterW(query, L"\\GPU Engine(*engtype_3D)\\Utilization Percentage", 0, &counter) != ERROR_SUCCESS) return false;
        PdhCollectQueryData(query);
        return true;
    }

    void Poll()
    {
        hasUse = false;
        if (!query || PdhCollectQueryData(query) != ERROR_SUCCESS) return;
        DWORD size = 0, count = 0;
        PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &size, &count, nullptr);
        if (size == 0) return;
        std::vector<unsigned char> buf(size);
        auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buf.data());
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &size, &count, items) != ERROR_SUCCESS) return;
        double sum = 0.0;
        for (DWORD i = 0; i < count; ++i) if (items[i].FmtValue.CStatus == PDH_CSTATUS_VALID_DATA) sum += items[i].FmtValue.doubleValue;
        use = sum > 100.0 ? 100.0 : sum;
        hasUse = true;
    }

    ~PdhGpu() { if (query) PdhCloseQuery(query); }
};

} // namespace

struct HwMetrics::Impl
{
    CpuSampler cpu;
    std::unique_ptr<AdlGpu> adl;
    std::unique_ptr<NvmlGpu> nvml;
    std::unique_ptr<PdhGpu> pdh;
    const wchar_t* source = L"";
};

HwMetrics::HwMetrics() : impl_{ std::make_unique<Impl>() } {}
HwMetrics::~HwMetrics() = default;

void HwMetrics::Init()
{
    auto& im = *impl_;
    im.cpu.Poll();
    auto nv = std::make_unique<NvmlGpu>();
    if (nv->Open()) { im.nvml = std::move(nv); im.source = L"NVML"; }
    else {
        auto amd = std::make_unique<AdlGpu>();
        if (amd->Init()) { im.adl = std::move(amd); im.source = L"ADL"; }
    }
    const bool haveVendorUse = (im.nvml && im.nvml->hasUse) || (im.adl && im.adl->hasUse);
    if (!haveVendorUse) {
        auto p = std::make_unique<PdhGpu>();
        if (p->Open()) { im.pdh = std::move(p); if (!im.nvml && !im.adl) im.source = L"PDH"; }
    }
}

void HwMetrics::Poll()
{
    auto& im = *impl_;
    im.cpu.Poll();
    if (im.nvml) im.nvml->Poll();
    if (im.adl) im.adl->Poll();
    if (im.pdh) im.pdh->Poll();
}

bool HwMetrics::HasGpuUse() const
{
    auto& im = *impl_;
    return (im.nvml && im.nvml->hasUse) || (im.adl && im.adl->hasUse) || (im.pdh != nullptr);
}
bool HwMetrics::HasGpuTemp() const
{
    auto& im = *impl_;
    return (im.nvml && im.nvml->hasTemp) || (im.adl && im.adl->hasTemp);
}
double HwMetrics::CpuUsePercent() const { return impl_->cpu.percent; }
double HwMetrics::GpuUsePercent() const
{
    auto& im = *impl_;
    if (im.nvml && im.nvml->hasUse) return im.nvml->use;
    if (im.adl && im.adl->hasUse) return im.adl->use;
    if (im.pdh && im.pdh->hasUse) return im.pdh->use;
    return NAN;
}
double HwMetrics::GpuTempC() const
{
    auto& im = *impl_;
    if (im.nvml && im.nvml->hasTemp) return im.nvml->temp;
    if (im.adl && im.adl->hasTemp) return im.adl->temp;
    return NAN;
}
const wchar_t* HwMetrics::GpuSource() const { return impl_->source; }
