@echo off

rem Si no estamos ya dentro de una ventana persistente, relanzarnos con cmd /k
if not defined BUILD_BAT_PERSISTENT (
    set BUILD_BAT_PERSISTENT=1
    start "" cmd /k "%~f0" %*
    exit /b
)

setlocal enabledelayedexpansion

rem ============================================================================
rem build.bat - ABDMCS48SE (build ALL versions: x64 + x86 / 32-bit V-Machine)
rem ============================================================================

set "CONFIG=Release"
set "FORCE_WEBUI=0"
set "DO_X64=0"
set "DO_X86=0"
set "EXPLICIT_ARCH=0"

for %%A in (%*) do (
    if /i "%%~A"=="Debug" set "CONFIG=Debug"
    if /i "%%~A"=="Release" set "CONFIG=Release"
    if /i "%%~A"=="--webui" set "FORCE_WEBUI=1"
    if /i "%%~A"=="-w" set "FORCE_WEBUI=1"
    if /i "%%~A"=="--rebuild-webui" set "FORCE_WEBUI=1"
    if /i "%%~A"=="x64" ( set "DO_X64=1" & set "EXPLICIT_ARCH=1" )
    if /i "%%~A"=="64" ( set "DO_X64=1" & set "EXPLICIT_ARCH=1" )
    if /i "%%~A"=="x86" ( set "DO_X86=1" & set "EXPLICIT_ARCH=1" )
    if /i "%%~A"=="32" ( set "DO_X86=1" & set "EXPLICIT_ARCH=1" )
    if /i "%%~A"=="win32" ( set "DO_X86=1" & set "EXPLICIT_ARCH=1" )
    if /i "%%~A"=="all" ( set "DO_X64=1" & set "DO_X86=1" & set "EXPLICIT_ARCH=1" )
)

if "%EXPLICIT_ARCH%"=="0" (
    set "DO_X64=1"
    set "DO_X86=1"
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

:vs_carga_fallo
echo [ERROR] Fallo al invocar vcvarsall.bat.
goto error

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

set "BUILD_DIR_64=build"
set "BUILD_DIR_32=build32"

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

rem Cerrar instancias previas para liberar locks de enlace
taskkill /f /im "BDMCS48SE Instrument.exe" >nul 2>&1
taskkill /f /im "bdmcs48se.exe" >nul 2>&1

rem ============================================================================
rem BLOQUE X64 (Arquitectura 64 bits para DAWs modernos)
rem ============================================================================
if "%DO_X64%"=="1" (
    echo.
    echo ============================================================================
    echo  [BUILD X64] Targets x64 (Standalone, VST3, VST2, CLI, Tests)
    echo  Configuracion: %CONFIG%
    echo  Directorio:    %BUILD_DIR_64%
    echo ============================================================================

    "%CMAKE_PATH%" -S . -B "%BUILD_DIR_64%" -G "Visual Studio 18 2026" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DABDMCS48SE_BUILD_PLUGIN=ON -DABDMCS48SE_BUILD_CLI=ON -DABDMCS48SE_BUILD_TESTS=ON -DABDMCS48SE_BUILD_BENCHMARKS=ON
    if errorlevel 1 (
        echo [WARNING] Fallo de configuracion CMake x64. Limpiando cache y reintentando...
        if exist "%BUILD_DIR_64%\CMakeCache.txt" del /q "%BUILD_DIR_64%\CMakeCache.txt"
        if exist "%BUILD_DIR_64%\CMakeFiles" rmdir /s /q "%BUILD_DIR_64%\CMakeFiles"
        "%CMAKE_PATH%" -S . -B "%BUILD_DIR_64%" -G "Visual Studio 18 2026" -A x64 -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DABDMCS48SE_BUILD_PLUGIN=ON -DABDMCS48SE_BUILD_CLI=ON -DABDMCS48SE_BUILD_TESTS=ON -DABDMCS48SE_BUILD_BENCHMARKS=ON
        if errorlevel 1 goto error
    )

    echo [INFO] Compilando targets x64...
    "%CMAKE_PATH%" --build "%BUILD_DIR_64%" --config %CONFIG% --parallel
    if errorlevel 1 goto error

    echo [INFO] Ejecutando tests x64...
    "%CMAKE_PATH%" --build "%BUILD_DIR_64%" --config %CONFIG% --target run_tests
    if errorlevel 1 goto error

    rem Staging de artefactos x64 en dist\x64
    if not exist "%~dp0dist\x64\Standalone" mkdir "%~dp0dist\x64\Standalone"
    if not exist "%~dp0dist\x64\VST3" mkdir "%~dp0dist\x64\VST3"
    if not exist "%~dp0dist\x64\VST" mkdir "%~dp0dist\x64\VST"
    if not exist "%~dp0dist\x64\CLI" mkdir "%~dp0dist\x64\CLI"

    if exist "%BUILD_DIR_64%\bdmcs48se_instrument_artefacts\%CONFIG%\Standalone\BDMCS48SE Instrument.exe" (
        copy /y "%BUILD_DIR_64%\bdmcs48se_instrument_artefacts\%CONFIG%\Standalone\BDMCS48SE Instrument.exe" "%~dp0dist\x64\Standalone\" >nul
    )
    if exist "%BUILD_DIR_64%\bdmcs48se_instrument_artefacts\%CONFIG%\VST3\BDMCS48SE Instrument.vst3" (
        xcopy /e /i /y "%BUILD_DIR_64%\bdmcs48se_instrument_artefacts\%CONFIG%\VST3\BDMCS48SE Instrument.vst3" "%~dp0dist\x64\VST3\BDMCS48SE Instrument.vst3" >nul
    )
    if exist "%BUILD_DIR_64%\bdmcs48se_instrument_artefacts\%CONFIG%\VST\BDMCS48SE Instrument.dll" (
        copy /y "%BUILD_DIR_64%\bdmcs48se_instrument_artefacts\%CONFIG%\VST\BDMCS48SE Instrument.dll" "%~dp0dist\x64\VST\" >nul
    )
    if exist "%BUILD_DIR_64%\Release\bdmcs48se.exe" (
        copy /y "%BUILD_DIR_64%\Release\bdmcs48se.exe" "%~dp0dist\x64\CLI\" >nul
    ) else if exist "%BUILD_DIR_64%\%CONFIG%\bdmcs48se.exe" (
        copy /y "%BUILD_DIR_64%\%CONFIG%\bdmcs48se.exe" "%~dp0dist\x64\CLI\" >nul
    )
)

rem ============================================================================
rem BLOQUE X86 / 32 BITS (Salida propia para V-Machine y DAWs x86)
rem ============================================================================
if "%DO_X86%"=="1" (
    echo.
    echo ============================================================================
    echo  [BUILD X86 / 32-BIT] Targets Win32 (V-Machine / IA32 + VST2 + Standalone)
    echo  Configuracion: %CONFIG%
    echo  Directorio:    %BUILD_DIR_32%
    echo ============================================================================

    "%CMAKE_PATH%" -S . -B "%BUILD_DIR_32%" -G "Visual Studio 18 2026" -A Win32 -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DABDMCS48SE_BUILD_PLUGIN=ON -DABDMCS48SE_BUILD_CLI=ON -DABDMCS48SE_BUILD_TESTS=ON -DABDMCS48SE_BUILD_BENCHMARKS=ON
    if errorlevel 1 (
        echo [WARNING] Fallo de configuracion CMake x86. Limpiando cache y reintentando...
        if exist "%BUILD_DIR_32%\CMakeCache.txt" del /q "%BUILD_DIR_32%\CMakeCache.txt"
        if exist "%BUILD_DIR_32%\CMakeFiles" rmdir /s /q "%BUILD_DIR_32%\CMakeFiles"
        "%CMAKE_PATH%" -S . -B "%BUILD_DIR_32%" -G "Visual Studio 18 2026" -A Win32 -DCMAKE_SYSTEM_VERSION=10.0.26100.0 -DABDMCS48SE_BUILD_PLUGIN=ON -DABDMCS48SE_BUILD_CLI=ON -DABDMCS48SE_BUILD_TESTS=ON -DABDMCS48SE_BUILD_BENCHMARKS=ON
        if errorlevel 1 goto error
    )

    echo [INFO] Compilando targets x86 (32 bits con /arch:IA32)...
    "%CMAKE_PATH%" --build "%BUILD_DIR_32%" --config %CONFIG% --parallel
    if errorlevel 1 goto error

    echo [INFO] Ejecutando tests x86 (32 bits)...
    "%CMAKE_PATH%" --build "%BUILD_DIR_32%" --config %CONFIG% --target run_tests
    if errorlevel 1 goto error

    rem Staging de artefactos x86 en dist\x86
    if not exist "%~dp0dist\x86\Standalone" mkdir "%~dp0dist\x86\Standalone"
    if not exist "%~dp0dist\x86\VST3" mkdir "%~dp0dist\x86\VST3"
    if not exist "%~dp0dist\x86\VST" mkdir "%~dp0dist\x86\VST"
    if not exist "%~dp0dist\x86\CLI" mkdir "%~dp0dist\x86\CLI"

    if exist "%BUILD_DIR_32%\bdmcs48se_instrument_artefacts\%CONFIG%\Standalone\BDMCS48SE Instrument.exe" (
        copy /y "%BUILD_DIR_32%\bdmcs48se_instrument_artefacts\%CONFIG%\Standalone\BDMCS48SE Instrument.exe" "%~dp0dist\x86\Standalone\" >nul
    )
    if exist "%BUILD_DIR_32%\bdmcs48se_instrument_artefacts\%CONFIG%\VST3\BDMCS48SE Instrument.vst3" (
        xcopy /e /i /y "%BUILD_DIR_32%\bdmcs48se_instrument_artefacts\%CONFIG%\VST3\BDMCS48SE Instrument.vst3" "%~dp0dist\x86\VST3\BDMCS48SE Instrument.vst3" >nul
    )
    if exist "%BUILD_DIR_32%\bdmcs48se_instrument_artefacts\%CONFIG%\VST\BDMCS48SE Instrument.dll" (
        copy /y "%BUILD_DIR_32%\bdmcs48se_instrument_artefacts\%CONFIG%\VST\BDMCS48SE Instrument.dll" "%~dp0dist\x86\VST\" >nul
    )
    if exist "%BUILD_DIR_32%\Release\bdmcs48se.exe" (
        copy /y "%BUILD_DIR_32%\Release\bdmcs48se.exe" "%~dp0dist\x86\CLI\" >nul
    ) else if exist "%BUILD_DIR_32%\%CONFIG%\bdmcs48se.exe" (
        copy /y "%BUILD_DIR_32%\%CONFIG%\bdmcs48se.exe" "%~dp0dist\x86\CLI\" >nul
    )
)

echo.
echo ============================================================================
echo  [SUCCESS] Compilacion completada y pruebas superadas.
echo ============================================================================
if "%DO_X64%"=="1" (
    echo  [Artefactos 64-bit - DAWs modernos]:
    echo    Carpeta de distribucion: %~dp0dist\x64\
    echo    VST2 [.dll 64-bit]:      %~dp0dist\x64\VST\BDMCS48SE Instrument.dll
    echo    VST3 [.vst3 64-bit]:     %~dp0dist\x64\VST3\BDMCS48SE Instrument.vst3
    echo    Standalone [.exe]:       %~dp0dist\x64\Standalone\BDMCS48SE Instrument.exe
    echo    CLI [.exe]:              %~dp0dist\x64\CLI\bdmcs48se.exe
    echo.
)
if "%DO_X86%"=="1" (
    echo  [Artefactos 32-bit - SM Pro Audio V-Machine / DAWs x86]:
    echo    Carpeta de distribucion: %~dp0dist\x86\
    echo    VST2 [.dll 32-bit IA32]: %~dp0dist\x86\VST\BDMCS48SE Instrument.dll
    echo    VST3 [.vst3 32-bit]:     %~dp0dist\x86\VST3\BDMCS48SE Instrument.vst3
    echo    Standalone [.exe]:       %~dp0dist\x86\Standalone\BDMCS48SE Instrument.exe
    echo    CLI [.exe]:              %~dp0dist\x86\CLI\bdmcs48se.exe
    echo.
)
echo ============================================================================
echo.
pause
exit /b 0

:error
echo.
echo ============================================================================
echo  [ERROR] La compilacion o ejecucion de tests ha fallado.
echo ============================================================================
echo.
pause
exit /b 1
