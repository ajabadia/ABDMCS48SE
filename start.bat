@echo off
setlocal EnableDelayedExpansion
REM ============================================================
REM start.bat - Servidor de desarrollo local para ABDMCS48SE WebUI
REM (Intel MCS-48 8048/8049 Synthesizer - Modern Web Edition)
REM
REM Uso: start.bat           (abre navegador por defecto en http://localhost:8048)
REM      start.bat nobrowser (solo inicia el servidor Vite sin abrir navegador)
REM ============================================================

set "PORT=8048"
set "URL=http://localhost:%PORT%"

where node >nul 2>nul
if %ERRORLEVEL% neq 0 (
    if exist "%LOCALAPPDATA%\nvm\v24.21.0\node.exe" (
        set "PATH=%LOCALAPPDATA%\nvm\v24.21.0;%PATH%"
    ) else if exist "C:\Program Files\nodejs\node.exe" (
        set "PATH=C:\Program Files\nodejs;%PATH%"
    )
)

echo ============================================================
echo   ABDMCS48SE -- MCS-48 Synthesizer (Web Edition)
echo   Puerto: %PORT%
echo ============================================================

REM 1. Cerrar procesos previos que ocupen el puerto local %PORT%
echo [1/3] Verificando y liberando puerto %PORT%...
set "KILLED_ANY=0"
for /f "tokens=2,5" %%a in ('netstat -aon ^| findstr ":%PORT% "') do (
    echo %%a | findstr /R ":%PORT%$" >nul 2>&1
    if !ERRORLEVEL! EQU 0 (
        if not "%%b"=="0" (
            echo [INFO] Terminando proceso previo en puerto %PORT% [PID %%b]...
            taskkill /F /T /PID %%b >nul 2>&1
            set "KILLED_ANY=1"
        )
    )
)

powershell -NoProfile -Command "Get-NetTCPConnection -LocalPort %PORT% -ErrorAction SilentlyContinue | Select-Object -ExpandProperty OwningProcess -Unique | Where-Object { $_ -gt 4 } | ForEach-Object { taskkill /F /T /PID $_ }" >nul 2>&1

if "%KILLED_ANY%"=="1" (
    timeout /t 1 /nobreak >nul
)

REM 2. Abrir navegador en la WebUI (omitir con 'start.bat nobrowser')
if /I "%1"=="nobrowser" (
    echo [2/3] Modo nobrowser: omitiendo apertura de navegador.
) else (
    echo [2/3] Abriendo navegador en %URL%...
    start "" "%URL%"
)

REM 3. Iniciar Vite dev server
echo [3/3] Iniciando Vite Dev Server...
echo ============================================================
echo   URL: %URL%
echo   Directorio WebUI: WebUI\index.html
echo   Pulsa Ctrl+C para detener el servidor
echo ============================================================

if not exist "WebUI\node_modules" (
    echo [INFO] Instalando dependencias de WebUI...
    npm --prefix WebUI install
)

npm --prefix WebUI run dev -- --port %PORT% --strictPort
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [ERROR] El servidor Vite se ha detenido o no se pudo iniciar.
    pause
)
