// Métricas de hardware: uso de CPU (Win32) y uso/temperatura de GPU
// (ADL en AMD, NVML en NVIDIA, contadores PDH como respaldo para el uso).
#pragma once
#include <memory>
#include <string>

class HwMetrics
{
public:
    HwMetrics();
    ~HwMetrics();
    HwMetrics(const HwMetrics&) = delete;
    HwMetrics& operator=(const HwMetrics&) = delete;

    void Init();                 // detecta qué fuentes hay disponibles
    void Poll();                 // toma una muestra nueva de todo

    bool HasGpuUse() const;
    bool HasGpuTemp() const;
    double CpuUsePercent() const;
    double GpuUsePercent() const;
    double GpuTempC() const;
    const wchar_t* GpuSource() const;   // "ADL", "NVML", "PDH" o "" para diagnóstico

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
