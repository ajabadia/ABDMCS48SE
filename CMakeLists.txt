cmake_minimum_required(VERSION 3.22)

project(ABDMCS48SE
    VERSION 0.1.0
    DESCRIPTION "Headless MCS-48 core for sound machines"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Solo para MSVC: registrar las versiones de toolset con las que se compila
# permite al generador de Visual Studio construir una identificacion
# coherente sin lanzar cl.exe en la primera configuracion. En otros
# compiladores estas variables no significan nada.
if(MSVC)
    set(CMAKE_CXX_CL_19_38_VERSION "19.38.33807")
    set(CMAKE_CXX_CL_14_36_VERSION "14.36.32526")
endif()

# Nada de CMAKE_CXX_COMPILER_FORCED: en Linux y macOS la deteccion normal del
# compilador (y sus pruebas de ABI) tiene que ejecutarse.
if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(ABDMCS48SE_BUILD_TESTS "Build the unit tests" ON)
option(ABDMCS48SE_BUILD_CLI "Build the reference command line host" ON)
option(ABDMCS48SE_BUILD_BENCHMARKS "Build the processBlock benchmark" ON)
# Desactivado por defecto: JUCE no es dependencia del core y la primera
# configuracion lo descarga. El resto del proyecto compila sin red.
option(ABDMCS48SE_BUILD_PLUGIN "Build the JUCE instrument (downloads JUCE 9)" OFF)

# ---------------------------------------------------------------------------
# core: headless, without external dependencies (neither JUCE, nor JSON, nor
# filesystem). Implemented as an interface library whose sources are compiled
# by the libraries that link it.
# ---------------------------------------------------------------------------
add_library(bdmcs48_core INTERFACE)
target_include_directories(bdmcs48_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/source)
target_compile_features(bdmcs48_core INTERFACE cxx_std_20)

add_library(bdmcs48_core_impl STATIC
    source/core/ProgramMemory.cpp
    source/core/Mcs48Cpu.cpp
    source/core/CoreClock.cpp
    source/core/Mcs48Timer.cpp
    source/core/MachineExchange.cpp
    source/core/Diagnostics.cpp)
target_link_libraries(bdmcs48_core_impl PUBLIC bdmcs48_core)

# ---------------------------------------------------------------------------
# firmware: firmware package (manifest, JSON, SHA-256). Lives OUTSIDE the core
# because the core must not depend on the package format.
# ---------------------------------------------------------------------------
add_library(bdmcs48_firmware STATIC
    source/firmware/Json.cpp
    source/firmware/Sha256.cpp
    source/firmware/FirmwarePackage.cpp
    source/firmware/PackageLoader.cpp
    source/firmware/demo/DemoFirmware.cpp
    source/firmware/demo/MonoVoiceFirmware.cpp)
target_link_libraries(bdmcs48_firmware PUBLIC bdmcs48_core)

# ---------------------------------------------------------------------------
# machines: sound machine profiles (adapters over the core)
# ---------------------------------------------------------------------------
add_library(bdmcs48_machines STATIC
    source/machines/GenericMcs48Machine.cpp
    source/machines/MachineFactory.cpp)
target_link_libraries(bdmcs48_machines PUBLIC bdmcs48_core bdmcs48_core_impl bdmcs48_firmware)

# ---------------------------------------------------------------------------
# instrument: voz monofonica MIDI -> maquina. Sigue siendo JUCE-free: el
# plugin solo aporta el AudioProcessor y el editor.
# ---------------------------------------------------------------------------
add_library(bdmcs48_instrument STATIC
    source/instrument/MonoVoice.cpp)
target_link_libraries(bdmcs48_instrument PUBLIC bdmcs48_core)

# ---------------------------------------------------------------------------
# host: headless reference host + diagnostic protocol
# ---------------------------------------------------------------------------
add_library(bdmcs48_host STATIC
    source/host/MachineHost.cpp
    source/host/DiagnosticsProtocol.cpp
    source/host/HttpServer.cpp
    source/host/HttpClient.cpp)
target_link_libraries(bdmcs48_host PRIVATE bdmcs48_machines bdmcs48_core_impl)
if(WIN32)
    # HttpServer habla con winsock2 directamente: sin ws2_32 el enlace falla
    # con los __imp_socket/__imp_select sin resolver.
    target_link_libraries(bdmcs48_host PUBLIC ws2_32)
endif()

if(ABDMCS48SE_BUILD_CLI)
    add_executable(bdmcs48se source/cli/main.cpp)
    target_link_libraries(bdmcs48se PRIVATE bdmcs48_host bdmcs48_machines)
    target_include_directories(bdmcs48se PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

    # The WebView client travels alongside the executable: it is copied to the
    # output directory so the host can serve it without depending on the source
    # tree. El cliente es opcional: sin el, el host sigue sirviendo el
    # diagnostico y `serve --webview <dir>` permite apuntar a otro directorio.
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/source/webview)
        add_custom_command(TARGET bdmcs48se POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${CMAKE_CURRENT_SOURCE_DIR}/source/webview
                    $<TARGET_FILE_DIR:bdmcs48se>/webview
            COMMENT "Copying the WebView client next to the executable")
    endif()
endif()

# ---------------------------------------------------------------------------
# plugin: instrumento monofonico (VST3 + Standalone) sobre el host headless.
# Solo este objetivo conoce JUCE; el core, el firmware y la voz siguen sin
# depender de el. Requiere red en la primera configuracion (FetchContent).
# ---------------------------------------------------------------------------
if(ABDMCS48SE_BUILD_PLUGIN)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 9.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(bdmcs48se_instrument
        COMPANY_NAME "ABDMCS48SE"
        BUNDLE_ID "com.abdmcs48se.instrument"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        PLUGIN_MANUFACTURER_CODE Abms
        PLUGIN_CODE Bd48
        FORMATS Standalone VST3
        PRODUCT_NAME "BDMCS48SE Instrument")

    target_sources(bdmcs48se_instrument PRIVATE
        source/plugin/PluginProcessor.cpp)

    target_compile_definitions(bdmcs48se_instrument PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(bdmcs48se_instrument
        PRIVATE
            bdmcs48_host
            bdmcs48_instrument
            bdmcs48_machines
            bdmcs48_firmware
            juce::juce_audio_utils)

    target_include_directories(bdmcs48se_instrument PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)
endif()

if(ABDMCS48SE_BUILD_TESTS)
    enable_testing()

    add_executable(bdmcs48_tests
        tests/TestMain.cpp
        tests/TestProgramMemory.cpp
        tests/TestMcs48Cpu.cpp
        tests/TestClockAndQueue.cpp
        tests/TestMachineExchange.cpp
        tests/TestJsonAndSha256.cpp
        tests/TestFirmwarePackage.cpp
        tests/TestMonoVoice.cpp
        tests/TestInstrument.cpp
        tests/TestEndToEnd.cpp
        tests/TestToyRom.cpp
        tests/TestMcs48Faults.cpp
        tests/TestMcs48Timer.cpp
        tests/TestOpcodeCoverage.cpp)
    target_link_libraries(bdmcs48_tests PRIVATE bdmcs48_host bdmcs48_instrument)
    target_include_directories(bdmcs48_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/source)

    # Con enable_testing() el generador de Visual Studio crea el objetivo
    # RUN_TESTS, que es el que build.bat invoca como `--target run_tests`.
    add_test(NAME bdmcs48_tests COMMAND bdmcs48_tests)
    # Un timeout evita que un fallo de la CPU deje colgada la ejecucion de
    # CTest (la suite tarda menos de un segundo).
    set_tests_properties(bdmcs48_tests PROPERTIES TIMEOUT 120 LABELS "unit")

    # Prueba headless del esqueleto JUCE: MIDI -> processor -> audio, sin
    # abrir ventanas ni dispositivos. Solo existe si el plugin esta activado.
    if(ABDMCS48SE_BUILD_PLUGIN)
        juce_add_console_app(bdmcs48se_plugin_smoke
            PRODUCT_NAME "bdmcs48se_plugin_smoke")
        target_sources(bdmcs48se_plugin_smoke PRIVATE
            tests/PluginSmokeTest.cpp
            source/plugin/PluginProcessor.cpp)
        target_compile_definitions(bdmcs48se_plugin_smoke PRIVATE
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0)
        target_include_directories(bdmcs48se_plugin_smoke PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/source)
        target_link_libraries(bdmcs48se_plugin_smoke PRIVATE
            bdmcs48_host
            bdmcs48_instrument
            bdmcs48_machines
            bdmcs48_firmware
            juce::juce_audio_utils)
        add_test(NAME plugin_smoke COMMAND bdmcs48se_plugin_smoke)
        set_tests_properties(plugin_smoke PROPERTIES TIMEOUT 120 LABELS "plugin")
    endif()
endif()

# ---------------------------------------------------------------------------
# bench: coste de renderBlock por bloque. Sin dependencias externas: mide con
# std::chrono y comprueba un umbral de tiempo real en modo --smoke.
# ---------------------------------------------------------------------------
if(ABDMCS48SE_BUILD_BENCHMARKS)
    add_executable(bdmcs48_bench bench/ProcessBlockBenchmark.cpp)
    target_link_libraries(bdmcs48_bench PRIVATE bdmcs48_host bdmcs48_machines bdmcs48_firmware)
    target_include_directories(bdmcs48_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

    if(ABDMCS48SE_BUILD_TESTS)
        add_test(NAME process_block_benchmark COMMAND bdmcs48_bench --smoke)
        set_tests_properties(process_block_benchmark PROPERTIES
            TIMEOUT 120 LABELS "benchmark")
    endif()
endif()
