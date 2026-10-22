# minimal-fps

Visor mínimo de FPS y uso de hardware para Windows. Un solo ejecutable, sin
instalar nada más: texto plano siempre encima de la ventana activa, sin
interfaz ni archivos de configuración.

```
FPS        91
GPU USE   16%
GPU TEMP  63C
CPU USE   41%
```

Es una versión ligera basada en [Intel PresentMon](https://github.com/GameTechDev/PresentMon):
incorpora su librería PresentData para contar frames por ETW y la combina con
una ventana propia y lecturas directas de GPU (ADL en AMD, NVML en NVIDIA) y CPU.

## Uso

Ejecuta `minimal-fps.exe`. Pide permisos de administrador porque la captura de
FPS usa una sesión ETW. Sigue a la ventana que tenga el foco, así que basta con
abrir el juego.

| Atajo      | Acción          |
|------------|-----------------|
| Ctrl+Alt+O | mostrar/ocultar |
| Ctrl+Alt+Q | salir           |

Para que arranque con Windows, crea una tarea programada con privilegios
elevados o un acceso directo en `shell:startup`.

## Opciones

```
minimal-fps [opciones]

Filas (por defecto todas activas):
  --no-fps  --no-gpu-use  --no-gpu-temp  --no-cpu-use
  --only fps,gpu-use,gpu-temp,cpu-use   muestra solo las indicadas

Apariencia:
  --refresh <ms>      intervalo de actualización (defecto 2000)
  --size <px>         tamaño de fuente (defecto 15)
  --font <nombre>     fuente (defecto Consolas)
  --alpha <0-255>     opacidad (defecto 190)
  --color <RRGGBB>    color del texto (defecto FFFFFF)
  --bg <RRGGBB>       color de fondo (defecto 000000)
  --pos <x,y>         posición en pantalla (defecto 8,8)
  --hidden            iniciar oculto
  --help              ayuda
```

Ejemplos:

```
minimal-fps --only fps
minimal-fps --no-cpu-use --size 18 --alpha 230
minimal-fps --only fps,gpu-temp --color 00FF66 --pos 20,20 --refresh 1000
```

Las filas de GPU se ocultan solas si el driver no expone el dato. Si no hay
driver AMD ni NVIDIA, el uso de GPU se lee de los contadores de rendimiento de
Windows y la temperatura no se muestra.

## Compilar

Requisitos: Visual Studio 2022 (Build Tools o completo) con el workload
"Desarrollo para el escritorio con C++" y CMake (incluido en el componente
"C++ CMake tools for Windows").

```
git clone https://github.com/monki-nx/minimal-fps.git
cd minimal-fps
build.cmd
```

Resultado: `build\Release\minimal-fps.exe`, sin dependencias fuera de Windows.

## Estructura

```
src/            ventana, línea de comandos, FPS por ETW, métricas de hardware
third_party/    PresentData de Intel PresentMon y headers ADL de AMD (ver NOTICE.md)
CMakeLists.txt  build con CMake + MSVC
build.cmd       envoltorio que localiza Visual Studio y compila
```

## Licencia

MIT. El código de terceros conserva sus licencias, ver `third_party/NOTICE.md`.
