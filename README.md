# minimal-fps

Visor mínimo de FPS y uso de hardware para Windows. Un solo ejecutable, texto
plano siempre encima de la ventana activa, sin interfaz ni configuración.

```
FPS        91
GPU USE   16%
GPU TEMP  63C
CPU USE   41%
```

Es una versión ligera basada en [Intel PresentMon](https://github.com/GameTechDev/PresentMon):
usa su servicio como fuente de datos y reemplaza la interfaz por esta ventana.

## Requisitos

- Intel PresentMon instalado (`winget install Intel.PresentMon`). Su servicio
  arranca solo con Windows; la aplicación de PresentMon no hace falta abrirla.
- Para compilar: Visual Studio 2022 Build Tools con el workload de C++.

## Uso

Ejecuta `minimal-fps.exe`. Sigue a la ventana que tenga el foco, así que basta
con abrir el juego.

| Atajo      | Acción          |
|------------|-----------------|
| Ctrl+Alt+O | mostrar/ocultar |
| Ctrl+Alt+Q | salir           |

Para que arranque con Windows, crea un acceso directo en `shell:startup`.

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

## Compilar

```
build.cmd
```

Genera `out\minimal-fps.exe` junto con `PresentMonAPI2Loader.dll`, que debe
quedar en la misma carpeta que el ejecutable.

`diag.cpp` lista los dispositivos y las métricas que el servicio ofrece en tu
equipo. Útil si quieres agregar filas en `g_rows`.

## Licencia

MIT.
