// Rastreador de FPS: sesión ETW en tiempo real consumida con PresentData.
#pragma once
#include <cstdint>
#include <memory>
#include <string>

class FpsTracker
{
public:
    FpsTracker();
    ~FpsTracker();
    FpsTracker(const FpsTracker&) = delete;
    FpsTracker& operator=(const FpsTracker&) = delete;

    // Inicia la sesión ETW y los hilos de consumo. Devuelve false y un
    // mensaje si falla (por ejemplo, sin permisos de administrador).
    bool Start(std::wstring& error);
    void Stop();

    // Presents por segundo del proceso en los últimos windowSec segundos.
    // Devuelve 0 si no hay datos.
    double GetFps(uint32_t pid, double windowSec);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
