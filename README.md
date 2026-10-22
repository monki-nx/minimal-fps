# minimal-fps

An ultra-lightweight FPS and hardware usage overlay for Windows. A single tiny
executable with no UI, no config files and no background services: plain text
that stays on top of the active window and nothing else.

```
FPS        91
GPU USE   16%
GPU TEMP  63C
CPU USE   41%
```

It is a stripped-down take on [Intel PresentMon](https://github.com/GameTechDev/PresentMon):
it embeds the PresentData library to count frames via ETW and pairs it with its
own window and direct GPU (ADL on AMD, NVML on NVIDIA) and CPU readings. No
runtime, no framework, no dependencies beyond Windows itself.

## Usage

Run `minimal-fps.exe`. It asks for administrator rights because FPS capture
uses an ETW session. It follows whichever window has focus, so just launch
your game.

| Shortcut   | Action      |
|------------|-------------|
| Ctrl+Alt+O | show / hide |
| Ctrl+Alt+Q | quit        |

To start it with Windows, create a scheduled task with elevated privileges or
drop a shortcut into `shell:startup`.

## Options

```
minimal-fps [options]

Rows (all enabled by default):
  --no-fps  --no-gpu-use  --no-gpu-temp  --no-cpu-use
  --only fps,gpu-use,gpu-temp,cpu-use   show only the listed rows

Appearance:
  --refresh <ms>      update interval (default 2000)
  --size <px>         font size (default 15)
  --font <name>       font (default Consolas)
  --alpha <0-255>     opacity (default 190)
  --color <RRGGBB>    text color (default FFFFFF)
  --bg <RRGGBB>       background color (default 000000)
  --pos <x,y>         screen position (default 8,8)
  --hidden            start hidden
  --help              help
```

Examples:

```
minimal-fps --only fps
minimal-fps --no-cpu-use --size 18 --alpha 230
minimal-fps --only fps,gpu-temp --color 00FF66 --pos 20,20 --refresh 1000
```

GPU rows hide themselves when the driver does not expose the value. Without an
AMD or NVIDIA driver, GPU usage falls back to Windows performance counters and
temperature is not shown.

## Building

Requirements: Visual Studio 2022 (Build Tools or full) with the "Desktop
development with C++" workload and CMake (bundled in the "C++ CMake tools for
Windows" component).

```
git clone https://github.com/monki-nx/minimal-fps.git
cd minimal-fps
build.cmd
```

Output: `build\Release\minimal-fps.exe`, with no dependencies outside Windows.

## Layout

```
src/            window, command line, ETW-based FPS, hardware metrics
third_party/    PresentData from Intel PresentMon and AMD ADL headers (see NOTICE.md)
CMakeLists.txt  CMake + MSVC build
build.cmd       wrapper that locates Visual Studio and builds
```

## License

MIT. Third-party code keeps its own licenses, see `third_party/NOTICE.md`.
