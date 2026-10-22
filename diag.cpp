// diag - lista dispositivos y disponibilidad de las metricas que usa minimal-fps
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cstdio>
#include "PresentMonAPI.h"
#pragma comment(lib, "PresentMonAPI2Loader.lib")

int main()
{
    PM_SESSION_HANDLE s = nullptr;
    if (pmOpenSession(&s) != PM_STATUS_SUCCESS) { puts("open failed"); return 1; }
    const PM_INTROSPECTION_ROOT* root = nullptr;
    pmGetIntrospectionRoot(s, &root);
    for (size_t i = 0; i < root->pDevices->size; ++i) {
        auto d = (const PM_INTROSPECTION_DEVICE*)root->pDevices->pData[i];
        printf("device id=%u type=%d vendor=%d\n", d->id, (int)d->type, (int)d->vendor);
    }
    PM_METRIC want[] = { PM_METRIC_PRESENTED_FPS, PM_METRIC_GPU_UTILIZATION, PM_METRIC_GPU_TEMPERATURE,
                         PM_METRIC_CPU_UTILIZATION, PM_METRIC_CPU_TEMPERATURE, PM_METRIC_CPU_FREQUENCY,
                         PM_METRIC_GPU_MEM_USED, PM_METRIC_GPU_POWER };
    for (size_t i = 0; i < root->pMetrics->size; ++i) {
        auto m = (const PM_INTROSPECTION_METRIC*)root->pMetrics->pData[i];
        for (auto w : want) if (m->id == w) {
            printf("metric %d:", (int)m->id);
            for (size_t j = 0; j < m->pDeviceMetricInfo->size; ++j) {
                auto di = (const PM_INTROSPECTION_DEVICE_METRIC_INFO*)m->pDeviceMetricInfo->pData[j];
                printf("  [dev %u avail=%d n=%u]", di->deviceId, (int)di->availability, di->arraySize);
            }
            printf("  stats:");
            for (size_t j = 0; j < m->pStatInfo->size; ++j) printf(" %d", (int)*(const int*)m->pStatInfo->pData[j]);
            puts("");
        }
    }
    pmFreeIntrospectionRoot(root);
    pmCloseSession(s);
    return 0;
}
