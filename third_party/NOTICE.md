# Código de terceros

## PresentData

`third_party/PresentData/` proviene de [Intel PresentMon](https://github.com/GameTechDev/PresentMon)
(tag v2.6.0), licencia MIT (ver `LICENSE.txt` en esa carpeta). Es la librería que
consume los eventos ETW de DXGI, D3D9, DxgKrnl, etc. y produce eventos de
present. Se copió tal cual.

`third_party/IntelPresentMon/CommonUtilities/` contiene los pocos headers de
PresentMon que PresentData necesita (`Hash.h`, `Meta.h`, `SampleStatistics.h`,
`PrecisionWaiter.*`) y shims propios de minimal-fps (`Exception.h`, `Qpc.h`,
`log/Log.h`, `win/*`, `str/*`, `shim.cpp`) que reemplazan al resto de las
utilidades de PresentMon por implementaciones mínimas.

## ADL

`third_party/adl/` son los headers públicos de AMD Display Library, licencia
MIT (ver encabezado de cada archivo), tomados de la misma copia de PresentMon.
La librería `atiadlxx.dll` se carga en tiempo de ejecución desde el driver de
AMD si está presente.

## NVML

No se incluye ningún header de NVIDIA. minimal-fps declara a mano las tres
funciones de `nvml.dll` que usa y la carga en tiempo de ejecución desde el
driver si está presente.
