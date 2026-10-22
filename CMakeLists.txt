cmake_minimum_required(VERSION 3.21)
project(minimal-fps LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# runtime de C++ estatico: el exe no depende del redistribuible de Visual C++
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# --- PresentData (Intel PresentMon, MIT) + shims ------------------------------
file(GLOB PRESENTDATA_SOURCES CONFIGURE_DEPENDS
    third_party/PresentData/*.cpp
    third_party/IntelPresentMon/CommonUtilities/*.cpp)
add_library(presentdata STATIC ${PRESENTDATA_SOURCES})
target_include_directories(presentdata PUBLIC
    third_party/PresentData
    third_party/IntelPresentMon/CommonUtilities)
target_compile_definitions(presentdata PUBLIC
    _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A000000 WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_compile_options(presentdata PRIVATE /W3 /utf-8 /EHsc)
target_link_libraries(presentdata PUBLIC tdh advapi32)

# --- aplicacion -----------------------------------------------------------------
add_executable(minimal-fps WIN32
    src/main.cpp
    src/fps_tracker.cpp
    src/hw_metrics.cpp)
target_include_directories(minimal-fps PRIVATE src third_party/adl)
target_compile_options(minimal-fps PRIVATE /W4 /utf-8 /EHsc)
target_link_libraries(minimal-fps PRIVATE presentdata user32 gdi32 shell32 pdh)
# La captura ETW requiere privilegios de administrador.
target_link_options(minimal-fps PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")
