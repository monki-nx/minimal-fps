// minimal-fps - visor mínimo de FPS y uso de hardware para Windows.
// Un solo ejecutable, sin servicios ni dependencias instaladas.
//
// Atajos:  Ctrl+Alt+O  mostrar/ocultar      Ctrl+Alt+Q  salir
//
// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <string>

#include "fps_tracker.h"
#include "hw_metrics.h"

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")

namespace {

// ---- ajustes por defecto (todos se pueden cambiar por línea de comandos) ----
int          kRefreshMs   = 2000;         // --refresh ms
int          kFontPx      = 15;           // --size px
std::wstring kFontName    = L"Consolas";  // --font nombre
BYTE         kAlpha       = 190;          // --alpha 0-255
COLORREF     kTextColor   = RGB(255, 255, 255);  // --color RRGGBB
COLORREF     kBackColor   = RGB(0, 0, 0);        // --bg RRGGBB
int          kPosX        = 8;            // --pos x,y
int          kPosY        = 8;
bool         kStartHidden = false;        // --hidden
// -----------------------------------------------------------------------------

const wchar_t* kHelp =
    L"minimal-fps [opciones]\n\n"
    L"Filas (por defecto todas activas):\n"
    L"  --no-fps  --no-gpu-use  --no-gpu-temp  --no-cpu-use\n"
    L"  --only fps,gpu-use,gpu-temp,cpu-use   muestra solo las indicadas\n\n"
    L"Apariencia:\n"
    L"  --refresh <ms>      intervalo de actualizacion (defecto 2000)\n"
    L"  --size <px>         tamano de fuente (defecto 15)\n"
    L"  --font <nombre>     fuente (defecto Consolas)\n"
    L"  --alpha <0-255>     opacidad (defecto 190)\n"
    L"  --color <RRGGBB>    color del texto (defecto FFFFFF)\n"
    L"  --bg <RRGGBB>       color de fondo (defecto 000000)\n"
    L"  --pos <x,y>         posicion en pantalla (defecto 8,8)\n"
    L"  --hidden            iniciar oculto (Ctrl+Alt+O para mostrar)\n"
    L"  --help              esta ayuda\n\n"
    L"Requiere ejecutarse como administrador (captura de FPS por ETW).\n"
    L"Atajos: Ctrl+Alt+O mostrar/ocultar, Ctrl+Alt+Q salir";

enum { IDT_POLL = 1, HK_TOGGLE = 1, HK_QUIT = 2, kRows = 4 };
enum class Source { Fps, GpuUse, GpuTemp, CpuUse };

struct Row {
    const wchar_t* key;      // nombre en la línea de comandos
    const wchar_t* label;
    const wchar_t* unit;
    Source         source;
    bool           active;
    double         value;
};

Row g_rows[kRows] = {
    { L"fps",      L"FPS",      L"",  Source::Fps,     true, NAN },
    { L"gpu-use",  L"GPU USE",  L"%", Source::GpuUse,  true, NAN },
    { L"gpu-temp", L"GPU TEMP", L"C", Source::GpuTemp, true, NAN },
    { L"cpu-use",  L"CPU USE",  L"%", Source::CpuUse,  true, NAN },
};

FpsTracker g_fps;
HwMetrics  g_hw;
uint32_t   g_pid     = 0;
bool       g_visible = true;
HWND       g_hwnd    = nullptr;
HFONT      g_font    = nullptr;

COLORREF ParseHexColor(const wchar_t* s, COLORREF fallback)
{
    if (!s || wcslen(s) != 6) return fallback;
    unsigned v = wcstoul(s, nullptr, 16);
    return RGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

// Devuelve false si hay que terminar (ayuda o error de argumentos).
bool ParseArgs()
{
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (!argv) return true;
    auto next = [&](int& i) -> const wchar_t* { return (i + 1 < argc) ? argv[++i] : nullptr; };
    for (int i = 1; i < argc; ++i) {
        std::wstring a = argv[i];
        if (a == L"--help" || a == L"-h" || a == L"/?") {
            MessageBoxW(nullptr, kHelp, L"minimal-fps", MB_ICONINFORMATION);
            LocalFree(argv); return false;
        }
        bool handled = false;
        if (a.rfind(L"--no-", 0) == 0) {
            for (auto& r : g_rows) if (a.substr(5) == r.key) { r.active = false; handled = true; }
        }
        else if (a == L"--only") {
            for (auto& r : g_rows) r.active = false;
            std::wstring list = next(i) ? argv[i] : L"";
            size_t p = 0;
            while (p <= list.size()) {
                size_t q = list.find(L',', p); if (q == std::wstring::npos) q = list.size();
                std::wstring k = list.substr(p, q - p);
                for (auto& r : g_rows) if (k == r.key) r.active = true;
                p = q + 1;
            }
            handled = true;
        }
        else if (a == L"--refresh") { if (auto v = next(i)) kRefreshMs = (std::max)(200, _wtoi(v)); handled = true; }
        else if (a == L"--size")    { if (auto v = next(i)) kFontPx = (std::max)(6, _wtoi(v)); handled = true; }
        else if (a == L"--font")    { if (auto v = next(i)) kFontName = v; handled = true; }
        else if (a == L"--alpha")   { if (auto v = next(i)) kAlpha = BYTE((std::max)(0, (std::min)(255, _wtoi(v)))); handled = true; }
        else if (a == L"--color")   { kTextColor = ParseHexColor(next(i), kTextColor); handled = true; }
        else if (a == L"--bg")      { kBackColor = ParseHexColor(next(i), kBackColor); handled = true; }
        else if (a == L"--pos")     { if (auto v = next(i)) swscanf_s(v, L"%d,%d", &kPosX, &kPosY); handled = true; }
        else if (a == L"--hidden")  { kStartHidden = true; handled = true; }
        if (!handled) {
            std::wstring msg = L"Opcion desconocida: " + a + L"\n\n" + kHelp;
            MessageBoxW(nullptr, msg.c_str(), L"minimal-fps", MB_ICONERROR);
            LocalFree(argv); return false;
        }
    }
    LocalFree(argv);
    return true;
}

// Sigue a la ventana en primer plano (el juego). Ignora la propia ventana.
void UpdateTarget()
{
    HWND fg = GetForegroundWindow();
    if (!fg || fg == g_hwnd) return;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (pid == 0 || pid == GetCurrentProcessId()) return;
    g_pid = pid;
}

void Poll()
{
    g_hw.Poll();
    for (auto& r : g_rows) {
        if (!r.active) continue;
        switch (r.source) {
        case Source::Fps:     r.value = g_pid ? g_fps.GetFps(g_pid, kRefreshMs / 1000.0) : NAN; break;
        case Source::GpuUse:  r.value = g_hw.GpuUsePercent(); break;
        case Source::GpuTemp: r.value = g_hw.GpuTempC(); break;
        case Source::CpuUse:  r.value = g_hw.CpuUsePercent(); break;
        }
    }
}

void Paint(HDC dc, RECT rc)
{
    HBRUSH bg = CreateSolidBrush(kBackColor);
    FillRect(dc, &rc, bg);
    DeleteObject(bg);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, kTextColor);
    SelectObject(dc, g_font);
    int y = 4;
    for (auto& r : g_rows) {
        if (!r.active) continue;
        wchar_t line[64];
        const bool noData = std::isnan(r.value) || (r.source == Source::Fps && r.value <= 0.0);
        if (noData)
            swprintf_s(line, L"%-9s  --", r.label);
        else
            swprintf_s(line, L"%-9s%4.0f%s", r.label, r.value, r.unit);
        TextOutW(dc, 6, y, line, int(wcslen(line)));
        y += kFontPx + 3;
    }
}

LRESULT CALLBACK WndProc(HWND h, UINT m, WPARAM w, LPARAM l)
{
    switch (m) {
    case WM_TIMER:
        UpdateTarget();
        Poll();
        InvalidateRect(h, nullptr, FALSE);
        // mantenerse encima aunque el juego cree ventanas nuevas
        SetWindowPos(h, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(h, &ps);
        RECT rc; GetClientRect(h, &rc);
        Paint(dc, rc);
        EndPaint(h, &ps);
        return 0;
    }
    case WM_HOTKEY:
        if (w == HK_TOGGLE) { g_visible = !g_visible; ShowWindow(h, g_visible ? SW_SHOWNOACTIVATE : SW_HIDE); }
        else if (w == HK_QUIT) DestroyWindow(h);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(h, m, w, l);
}

} // namespace

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int)
{
    if (!ParseArgs()) return 0;

    // hardware: desactivar filas que esta máquina no puede ofrecer
    g_hw.Init();
    for (auto& r : g_rows) {
        if (r.source == Source::GpuUse && !g_hw.HasGpuUse()) r.active = false;
        if (r.source == Source::GpuTemp && !g_hw.HasGpuTemp()) r.active = false;
    }

    // FPS: sesión ETW (requiere administrador)
    bool wantFps = false; for (auto& r : g_rows) wantFps |= (r.source == Source::Fps && r.active);
    if (wantFps) {
        std::wstring err;
        if (!g_fps.Start(err)) {
            MessageBoxW(nullptr, err.c_str(), L"minimal-fps", MB_ICONERROR);
            return 1;
        }
    }

    bool any = false; for (auto& r : g_rows) any |= r.active;
    if (!any) { MessageBoxW(nullptr, L"No queda ninguna fila activa.", L"minimal-fps", MB_ICONERROR); return 1; }

    WNDCLASSW wc{};
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"minimal-fps";
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    RegisterClassW(&wc);

    g_font = CreateFontW(-kFontPx, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                         OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         FIXED_PITCH | FF_MODERN, kFontName.c_str());

    int rows = 0; for (auto& r : g_rows) rows += r.active ? 1 : 0;
    const int width  = 6 + int(kFontPx * 0.55 + 0.5) * 15 + 6;   // ~15 caracteres monoespaciados
    const int height = 8 + rows * (kFontPx + 3);

    g_hwnd = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName, L"minimal-fps", WS_POPUP,
        kPosX, kPosY, width, height, nullptr, nullptr, hInst, nullptr);
    SetLayeredWindowAttributes(g_hwnd, 0, kAlpha, LWA_ALPHA);
    g_visible = !kStartHidden;
    ShowWindow(g_hwnd, g_visible ? SW_SHOWNOACTIVATE : SW_HIDE);

    RegisterHotKey(g_hwnd, HK_TOGGLE, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'O');
    RegisterHotKey(g_hwnd, HK_QUIT,   MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'Q');
    SetTimer(g_hwnd, IDT_POLL, kRefreshMs, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }

    g_fps.Stop();
    DeleteObject(g_font);
    return 0;
}
