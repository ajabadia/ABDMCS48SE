cmake_minimum_required(VERSION 3.22)

project(ABDMCS48SE
    VERSION 0.1.0
    DESCRIPTION "Headless MCS-48 core for sound machines"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Register the two MSVC toolset versions this project runs against so the
# Windows/MSVC generator can synthesize a consistent identification without
# spawning cl.exe during the first configure phase. These are only used by the
# generator to build a self-consistent compiler identification; the project is
# MSVC-only and the real compile flags come from the target rules below.
set(CMAKE_CXX_CL_19_38_VERSION "19.38.33807")
set(CMAKE_CXX_CL_14_36_VERSION "14.36.32526")

# Keep this project MSVC-only while the toolchain is resolved through
# vcvarsall.bat.
set(CMAKE_CXX_COMPILER_FORCED ON)
set(CMAKE_C_COMPILER_FORCED ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

option(ABDMCS48SE_BUILD_TESTS "Build the unit tests" ON)
option(ABDMCS48SE_BUILD_CLI "Build the reference command line host" ON)

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
    source/firmware/demo/DemoFirmware.cpp)
target_link_libraries(bdmcs48_firmware PUBLIC bdmcs48_core)

# ---------------------------------------------------------------------------
# machines: sound machine profiles (adapters over the core)
# ---------------------------------------------------------------------------
add_library(bdmcs48_machines STATIC
    source/machines/GenericMcs48Machine.cpp)
target_link_libraries(bdmcs48_machines PUBLIC bdmcs48_core bdmcs48_firmware)

# ---------------------------------------------------------------------------
# host: headless reference host + diagnostic protocol
# ---------------------------------------------------------------------------
add_library(bdmcs48_host STATIC
    source/host/MachineHost.cpp
    source/host/DiagnosticsProtocol.cpp
    source/host/HttpServer.cpp)
target_link_libraries(bdmcs48_host PRIVATE bdmcs48_machines)

if(ABDMCS48SE_BUILD_CLI)
    add_executable(bdmcs48se source/cli/main.cpp)
    target_link_libraries(bdmcs48se PRIVATE bdmcs48_host bdmcs48_machines)
    target_include_directories(bdmcs48se PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/source)

    # The WebView client travels alongside the executable: it is copied to the
    # output directory so the host can serve it without depending on the source
    # tree.
    add_custom_command(TARGET bdmcs48se POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_CURRENT_SOURCE_DIR}/source/webview
                $<TARGET_FILE_DIR:bdmcs48se>/webview
        COMMENT "Copying the WebView client next to the executable")
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
        tests/TestEndToEnd.cpp)
    target_link_libraries(bdmcs48_tests PRIVATE bdmcs48_host)
    target_include_directories(bdmcs48_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/tests
        ${CMAKE_CURRENT_SOURCE_DIR}/source)

    add_test(NAME bdmcs48_tests COMMAND bdmcs48_tests)
endif()
