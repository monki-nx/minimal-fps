// minimal-fps - visor minimo de FPS y uso de hardware.
// Usa el servicio de Intel PresentMon (ya instalado) como fuente de datos
// y dibuja una ventana de texto plano siempre encima. Sin UI, sin config.
//
// Atajos:  Ctrl+Alt+O  mostrar/ocultar      Ctrl+Alt+Q  salir
//
// SPDX-License-Identifier: MIT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <cstdint>
#include <cstdio>
#include <cmath>
#include <string>
#include <vector>
#include "PresentMonAPI.h"

#pragma comment(lib, "PresentMonAPI2Loader.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "shell32.lib")

namespace {

// ---- ajustes por defecto (todos se pueden cambiar por linea de comandos) ----
int          kRefreshMs = 2000;      // --refresh ms
int          kFontPx    = 15;        // --size px
std::wstring kFontName  = L"Consolas"; // --font nombre
BYTE         kAlpha     = 190;       // --alpha 0-255
COLORREF     kTextColor = RGB(255, 255, 255);  // --color RRGGBB
COLORREF     kBackColor = RGB(0, 0, 0);        // --bg RRGGBB
int          kPosX      = 8;         // --pos x,y
int          kPosY      = 8;
bool         kStartHidden = false;   // --hidden
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
    L"Atajos: Ctrl+Alt+O mostrar/ocultar, Ctrl+Alt+Q salir";

enum { IDT_POLL = 1, HK_TOGGLE = 1, HK_QUIT = 2, kRows = 4 };

struct Row {
    const wchar_t* key;      // nombre en la linea de comandos
    const wchar_t* label;
    const wchar_t* unit;
    PM_METRIC      metric;
    PM_DEVICE_TYPE dev;      // dispositivo que expone la metrica
    bool           active;   // false si el servicio no ofrece la metrica
    double         value;
};

Row g_rows[kRows] = {
    { L"fps",      L"FPS",      L"",   PM_METRIC_PRESENTED_FPS,   PM_DEVICE_TYPE_INDEPENDENT,      true, NAN },
    { L"gpu-use",  L"GPU USE",  L"%",  PM_METRIC_GPU_UTILIZATION, PM_DEVICE_TYPE_GRAPHICS_ADAPTER, true, NAN },
    { L"gpu-temp", L"GPU TEMP", L"C",  PM_METRIC_GPU_TEMPERATURE, PM_DEVICE_TYPE_GRAPHICS_ADAPTER, true, NAN },
    { L"cpu-use",  L"CPU USE",  L"%",  PM_METRIC_CPU_UTILIZATION, PM_DEVICE_TYPE_SYSTEM,           true, NAN },
};

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
        else if (a == L"--refresh") { if (auto v = next(i)) kRefreshMs = max(200, _wtoi(v)); handled = true; }
        else if (a == L"--size")    { if (auto v = next(i)) kFontPx = max(6, _wtoi(v)); handled = true; }
        else if (a == L"--font")    { if (auto v = next(i)) kFontName = v; handled = true; }
        else if (a == L"--alpha")   { if (auto v = next(i)) kAlpha = BYTE(max(0, min(255, _wtoi(v)))); handled = true; }
        else if (a == L"--color")   { kTextColor = ParseHexColor(next(i), kTextColor); handled = true; }
        else if (a == L"--bg")      { kBackColor = ParseHexColor(next(i), kBackColor); handled = true; }
        else if (a == L"--pos")     { if (auto v = next(i)) swscanf(v, L"%d,%d", &kPosX, &kPosY); handled = true; }
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

PM_SESSION_HANDLE       g_session = nullptr;
PM_DYNAMIC_QUERY_HANDLE g_query   = nullptr;
std::vector<PM_QUERY_ELEMENT> g_elements;   // solo las filas activas, en orden
std::vector<uint8_t>    g_blob;
uint32_t                g_gpuDevice = 0;
uint32_t                g_sysDevice = 0;
uint32_t                g_pid       = 0;
bool                    g_visible   = true;
HWND                    g_hwnd      = nullptr;
HFONT                   g_font      = nullptr;

// Busca el primer adaptador grafico y el dispositivo "sistema" (CPU).
void FindDevices()
{
    const PM_INTROSPECTION_ROOT* root = nullptr;
    if (pmGetIntrospectionRoot(g_session, &root) != PM_STATUS_SUCCESS || !root) return;
    for (size_t i = 0; i < root->pDevices->size; ++i) {
        auto d = static_cast<const PM_INTROSPECTION_DEVICE*>(root->pDevices->pData[i]);
        if (d->type == PM_DEVICE_TYPE_GRAPHICS_ADAPTER && g_gpuDevice == 0) g_gpuDevice = d->id;
        if (d->type == PM_DEVICE_TYPE_SYSTEM && g_sysDevice == 0) g_sysDevice = d->id;
    }
    pmFreeIntrospectionRoot(root);
}

uint32_t DeviceFor(PM_DEVICE_TYPE t)
{
    if (t == PM_DEVICE_TYPE_GRAPHICS_ADAPTER) return g_gpuDevice;
    if (t == PM_DEVICE_TYPE_SYSTEM) return g_sysDevice;
    return 0;
}

// Registra la consulta con las filas activas. Si el servicio la rechaza,
// desactiva la ultima fila de hardware y reintenta (FPS siempre queda).
bool RegisterQuery()
{
    for (;;) {
        g_elements.clear();
        for (auto& r : g_rows) {
            if (!r.active) continue;
            PM_QUERY_ELEMENT e{};
            e.metric     = r.metric;
            e.stat       = PM_STAT_AVG;
            e.deviceId   = DeviceFor(r.dev);
            e.arrayIndex = 0;
            g_elements.push_back(e);
        }
        if (g_query) { pmFreeDynamicQuery(g_query); g_query = nullptr; }
        auto st = pmRegisterDynamicQuery(g_session, &g_query, g_elements.data(),
                                         g_elements.size(), double(kRefreshMs), 0.0);
        if (st == PM_STATUS_SUCCESS) {
            size_t size = 0;
            for (auto& e : g_elements) size = max(size, size_t(e.dataOffset + e.dataSize));
            g_blob.assign(size, 0);
            return true;
        }
        // quitar la ultima fila de hardware activa y reintentar
        bool dropped = false;
        for (int i = kRows - 1; i >= 1; --i) {
            if (g_rows[i].active) { g_rows[i].active = false; dropped = true; break; }
        }
        if (!dropped) return false;
    }
}

bool InitPresentMon()
{
    if (pmOpenSession(&g_session) != PM_STATUS_SUCCESS) return false;
    FindDevices();
    for (auto& r : g_rows) {
        if (r.dev != PM_DEVICE_TYPE_INDEPENDENT && DeviceFor(r.dev) == 0) r.active = false;
    }
    pmSetTelemetryPollingPeriod(g_session, 0, 500);
    return RegisterQuery();
}

// Sigue a la ventana en primer plano (el juego). Ignora la propia ventana.
void UpdateTarget()
{
    HWND fg = GetForegroundWindow();
    if (!fg || fg == g_hwnd) return;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (pid == 0 || pid == GetCurrentProcessId() || pid == g_pid) return;
    if (g_pid) pmStopTrackingProcess(g_session, g_pid);
    g_pid = (pmStartTrackingProcess(g_session, pid) == PM_STATUS_SUCCESS) ? pid : 0;
}

void Poll()
{
    for (auto& r : g_rows) r.value = NAN;
    if (!g_pid || !g_query) return;
    uint32_t swapChains = 1;
    if (pmPollDynamicQuery(g_query, g_pid, g_blob.data(), &swapChains) != PM_STATUS_SUCCESS || swapChains == 0)
        return;
    size_t k = 0;
    for (auto& r : g_rows) {
        if (!r.active) continue;
        double v = 0;
        memcpy(&v, g_blob.data() + g_elements[k].dataOffset, sizeof v);
        r.value = v;
        ++k;
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
        if (std::isnan(r.value) || r.value <= 0.0)
            swprintf(line, 64, L"%-9s  --", r.label);
        else
            swprintf(line, 64, L"%-9s%4.0f%s", r.label, r.value, r.unit);
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
    bool any = false; for (auto& r : g_rows) any |= r.active;
    if (!any) { MessageBoxW(nullptr, L"No queda ninguna fila activa.", L"minimal-fps", MB_ICONERROR); return 1; }
    if (!InitPresentMon()) {
        MessageBoxW(nullptr,
            L"No se pudo conectar al servicio de PresentMon.\n"
            L"Verifica que Intel PresentMon este instalado y el servicio PresentMonService corriendo.",
            L"minimal-fps", MB_ICONERROR);
        return 1;
    }

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
        kPosX, kPosY, int(width), height, nullptr, nullptr, hInst, nullptr);
    SetLayeredWindowAttributes(g_hwnd, 0, kAlpha, LWA_ALPHA);
    g_visible = !kStartHidden;
    ShowWindow(g_hwnd, g_visible ? SW_SHOWNOACTIVATE : SW_HIDE);

    RegisterHotKey(g_hwnd, HK_TOGGLE, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'O');
    RegisterHotKey(g_hwnd, HK_QUIT,   MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'Q');
    SetTimer(g_hwnd, IDT_POLL, kRefreshMs, nullptr);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) { TranslateMessage(&msg); DispatchMessageW(&msg); }

    if (g_pid) pmStopTrackingProcess(g_session, g_pid);
    if (g_query) pmFreeDynamicQuery(g_query);
    pmCloseSession(g_session);
    DeleteObject(g_font);
    return 0;
}
