@echo off
setlocal enabledelayedexpansion

rem ============================================================================
rem build.bat - ABDMCS48SE (core MCS-48 headless)
rem ============================================================================
rem
rem CODIFICACION
rem cmd.exe lee este fichero en la codificacion del SISTEMA, no en UTF-8: un
rem acento se parte en dos bytes y de ahi sale un comando que no existe en
rem mitad de un echo. Por eso todo lo que hay aqui es ASCII puro.
rem
rem EL COMPILADOR
rem Se busca con vswhere porque una ruta escrita a fuego solo funciona en la
rem maquina de quien la escribio. Lo que decide es vcvarsall.bat: el
rem directorio de Visual Studio existe tambien en una instalacion sin la
rem carga de trabajo de C++, y una deteccion que mirara el directorio daria
rem por buena esa instalacion.
rem
rem ARGS
rem   build.bat [destino]
rem     destino  Release (por defecto) o Debug
rem ============================================================================

set "CONFIG=Release"
if not "%~1"=="" set "CONFIG=%~1"

set "PF86=%ProgramFiles(x86)%"
set "VSWHERE=%PF86%\Microsoft Visual Studio\Installer\vswhere.exe"
set "VS_LIST=%TEMP%\abdmcs48se-vs.txt"
del /q "%VS_LIST%" >nul 2>&1
if exist "%VSWHERE%" "%VSWHERE%" -all -prerelease -products * -property installationPath > "%VS_LIST%"

set "VS_ROOT="
if not exist "%VS_LIST%" goto vs_lista_vacia
for /f "usebackq delims=" %%I in ("%VS_LIST%") do (
    if not defined VS_ROOT if exist "%%~I\VC\Auxiliary\Build\vcvarsall.bat" set "VS_ROOT=%%~I"
)
:vs_lista_vacia

if defined VS_ROOT goto vs_encontrado
for %%I in (
    "%ProgramFiles%\Microsoft Visual Studio\18\Insiders"
    "%ProgramFiles%\Microsoft Visual Studio\18\Community"
    "%ProgramFiles%\Microsoft Visual Studio\18\Professional"
    "%ProgramFiles%\Microsoft Visual Studio\18\Enterprise"
    "%PF86%\Microsoft Visual Studio\18\BuildTools"
    "%ProgramFiles%\Microsoft Visual Studio\17\Community"
    "%ProgramFiles%\Microsoft Visual Studio\17\Professional"
    "%ProgramFiles%\Microsoft Visual Studio\17\Enterprise"
    "%PF86%\Microsoft Visual Studio\17\BuildTools"
) do (
    if not defined VS_ROOT if exist "%%~I\VC\Auxiliary\Build\vcvarsall.bat" set "VS_ROOT=%%~I"
)
:vs_encontrado

if not defined VS_ROOT goto vs_ausente
if exist "%VS_ROOT%\VC\Auxiliary\Build\vcvarsall.bat" goto vs_listo

:vs_ausente
echo [ERROR] No hay Visual Studio con el compilador de C++ en esta maquina.
echo         Falta vcvarsall.bat, que es lo unico que dice que hay toolchain.
echo         Instala la carga de trabajo "Desarrollo para el escritorio con C++".
goto error

:vs_listo
set "VC_VARS=%VS_ROOT%\VC\Auxiliary\Build\vcvarsall.bat"
echo [INFO] Visual Studio: %VS_ROOT%
call "%VC_VARS%" x64
if errorlevel 1 goto vs_carga_fallo
goto vs_cargado

:vs_cargado
where cl >nul 2>&1
if errorlevel 1 goto cl_ausente
goto cl_ok

:cl_ausente
echo [ERROR] vcvars cargado pero 'cl' no esta en PATH. No hay toolchain de C++.
goto error

:cl_ok

rem --- CMake ---------------------------------------------------------------
rem Primero el que trae Visual Studio, que ya sabemos que existe. Si no esta,
rem se busca en la instalacion propia del proyecto y en el PATH. El mensaje
rem de error Nombra la ruta en la que se ha buscado: si no, con
rem CMAKE_PATH vacio el error diria "CMake not found at " y a continuacion
rem nada, senalando el aire.
set "CMAKE_PATH=%VS_ROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
set "CMAKE_EN_VS=%CMAKE_PATH%"
if exist "%CMAKE_PATH%" goto cmake_ok
if exist "D:\desarrollos\cmake\bin\cmake.exe" (
    set "CMAKE_PATH=D:\desarrollos\cmake\bin\cmake.exe"
    goto cmake_ok
)
for %%C in (cmake.exe) do set "CMAKE_PATH=%%~$PATH:C"
if not "%CMAKE_PATH%"=="" goto cmake_ok
echo [ERROR] CMake not found.
echo         Se ha buscado en la instalacion de Visual Studio:
echo         %CMAKE_EN_VS%
echo         y tambien en D:\desarrollos\cmake\bin y en el PATH.
goto error
:cmake_ok

set "BUILD_DIR=build"

echo ========================================
echo  ABDMCS48SE - core MCS-48 headless
echo  Configuracion: %CONFIG%
echo  Directorio:    %BUILD_DIR%
echo  Visual Studio: %VS_ROOT%
echo  cmake:         %CMAKE_PATH%
echo ========================================

rem --- Configurar -----------------------------------------------------------
"%CMAKE_PATH%" -S . -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0
if not errorlevel 1 goto cmake_configurado
echo [WARNING] CMake configuration failed. Clearing CMakeCache.txt and retrying...
if exist "%BUILD_DIR%\CMakeCache.txt" del /q "%BUILD_DIR%\CMakeCache.txt"
if exist "%BUILD_DIR%\CMakeFiles" rmdir /s /q "%BUILD_DIR%\CMakeFiles"
"%CMAKE_PATH%" -S . -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0
if not errorlevel 1 goto cmake_configurado
echo [ERROR] CMake configuration failed again with code %ERRORLEVEL%
goto error
:cmake_configurado

rem --- Compilar -------------------------------------------------------------
echo [INFO] Building core, firmware loader, machines, host and tests...
"%CMAKE_PATH%" --build "%BUILD_DIR%" --config %CONFIG% --parallel
if not errorlevel 1 goto compilado
echo [ERROR] Build failed with code %ERRORLEVEL%
goto error
:compilado

rem --- Tests ----------------------------------------------------------------
echo [INFO] Running tests...
"%CMAKE_PATH%" --build "%BUILD_DIR%" --config %CONFIG% --target run_tests
if errorlevel 1 goto tests_fallo
goto tests_ok

:tests_fallo
echo [ERROR] The test suite failed.
goto error

:tests_ok
echo ========================================
echo  [SUCCESS] Build and tests finished.
echo  CLI: %BUILD_DIR%\bdmcs48se\Release\bdmcs48se.exe
echo ========================================
exit /b 0

:error
echo.
echo [ERROR] Build failed.
exit /b 1
