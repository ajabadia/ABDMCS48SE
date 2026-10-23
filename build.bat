@echo off

rem Si no estamos ya dentro de una ventana persistente, relanzarnos con cmd /k
if not defined BUILD_BAT_PERSISTENT (
    set BUILD_BAT_PERSISTENT=1
    start "" cmd /k "%~f0" %*
    exit /b
)

setlocal enabledelayedexpansion

rem ============================================================================
rem build.bat - ABDMCS48SE (build ALL versions)
rem ============================================================================

set "CONFIG=Release"
set "FORCE_WEBUI=0"

for %%A in (%*) do (
    if /i "%%~A"=="Debug" set "CONFIG=Debug"
    if /i "%%~A"=="Release" set "CONFIG=Release"
    if /i "%%~A"=="--webui" set "FORCE_WEBUI=1"
    if /i "%%~A"=="-w" set "FORCE_WEBUI=1"
    if /i "%%~A"=="--rebuild-webui" set "FORCE_WEBUI=1"
)

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

rem --- Build WebUI if needed (required for JUCE plugin embed) ------------
set "NEED_WEBUI=0"
if not exist "%~dp0WebUI\dist\index.html" set "NEED_WEBUI=1"
if "%FORCE_WEBUI%"=="1" set "NEED_WEBUI=1"

if "!NEED_WEBUI!"=="0" (
    powershell -NoProfile -Command "if ((Get-ChildItem -Path '%~dp0WebUI\src', '%~dp0WebUI\index.html' -Recurse | Measure-Object -Property LastWriteTime -Maximum).Maximum -gt (Get-Item '%~dp0WebUI\dist\index.html').LastWriteTime) { exit 1 } else { exit 0 }" >nul 2>&1
    if errorlevel 1 (
        echo [INFO] Detected changes in WebUI sources. Rebuilding bundle...
        set "NEED_WEBUI=1"
    )
)

if "!NEED_WEBUI!"=="1" (
    echo [INFO] Building WebUI bundle with Vite...
    where node >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] Node.js not found in PATH. Install Node.js or build WebUI manually:
        echo         npm --prefix WebUI install
        echo         node Scripts/build_webui.js
        goto error
    )
    pushd "%~dp0"
    node Scripts\build_webui.js
    if errorlevel 1 (
        popd
        echo [ERROR] WebUI build failed. Check ../ABDShared checkout or ABD_SHARED_DIR.
        goto error
    )
    popd
    echo [INFO] WebUI built successfully.
) else (
    echo [INFO] WebUI/dist/index.html is up to date. Using existing bundle.
)

echo ========================================
echo  ABDMCS48SE - build ALL versions
echo  Configuracion: %CONFIG%
echo  Directorio:    %BUILD_DIR%
echo  Visual Studio: %VS_ROOT%
echo  cmake:         %CMAKE_PATH%
echo  Plugin:        ON (VST3 + Standalone)
echo  CLI/Tests/Bench: ON
echo ========================================

rem --- Configurar -----------------------------------------------------------
"%CMAKE_PATH%" -S . -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DABDMCS48SE_BUILD_PLUGIN=ON -DABDMCS48SE_BUILD_CLI=ON -DABDMCS48SE_BUILD_TESTS=ON -DABDMCS48SE_BUILD_BENCHMARKS=ON
if not errorlevel 1 goto cmake_configurado
echo [WARNING] CMake configuration failed. Clearing CMakeCache.txt and retrying...
if exist "%BUILD_DIR%\CMakeCache.txt" del /q "%BUILD_DIR%\CMakeCache.txt"
if exist "%BUILD_DIR%\CMakeFiles" rmdir /s /q "%BUILD_DIR%\CMakeFiles"
"%CMAKE_PATH%" -S . -B "%BUILD_DIR%" -G "Visual Studio 18 2026" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DABDMCS48SE_BUILD_PLUGIN=ON -DABDMCS48SE_BUILD_CLI=ON -DABDMCS48SE_BUILD_TESTS=ON -DABDMCS48SE_BUILD_BENCHMARKS=ON
if not errorlevel 1 goto cmake_configurado
echo [ERROR] CMake configuration failed again with code %ERRORLEVEL%
goto error
:cmake_configurado

rem --- Compilar TODO --------------------------------------------------------
echo [INFO] Building ALL targets (core, firmware, host, CLI, tests, benchmarks, plugin VST3+Standalone)...
"%CMAKE_PATH%" --build "%BUILD_DIR%" --config %CONFIG% --parallel
if not errorlevel 1 goto compilado
echo [ERROR] Build failed with code %ERRORLEVEL%
goto error
:compilado

rem --- Tests ----------------------------------------------------------------
echo [INFO] Running tests (core + plugin_smoke if available)...
"%CMAKE_PATH%" --build "%BUILD_DIR%" --config %CONFIG% --target run_tests
if errorlevel 1 goto tests_fallo
goto tests_ok

:tests_fallo
echo.
echo [ERROR] The test suite failed.
echo [INFO] View full test output with:
echo        ctest --test-dir build --rerun-failed --output-on-failure
goto error

:tests_ok
echo ========================================
echo  [SUCCESS] All versions built and tests passed.
echo  CLI:        %BUILD_DIR%\bdmcs48se\%CONFIG%\bdmcs48se.exe
echo  Standalone: %BUILD_DIR%\bdmcs48se_instrument_artefacts\%CONFIG%\Standalone\BDMCS48SE Instrument.exe
echo  VST3:       %BUILD_DIR%\bdmcs48se_instrument_artefacts\%CONFIG%\VST3\BDMCS48SE Instrument.vst3
echo ========================================
echo.
pause
exit /b 0

:error
echo.
echo [ERROR] Build failed.
echo.
pause
exit /b 1
