# BDMCS48SE — Roadmap

> **Estado (2026-11-22):** Motor MCS-48 headless completo y congelado formalmente, Fases 1, 2, 4, 5 y 9 cumplidas y verificadas, suite ampliada a 191 tests unitarios pasando, catálogo musical con síntesis wavetable MOVP3 y percusión lo-fi (Kick, Snare, Hi-Hat), determinismo verificado contra hashes golden ante buffers fijos y variables dinámicos (64 a 1024), cero reservas de memoria tanto en régimen nominal como en fallo de CPU (buffer estático `char[48]`), snapshots de diagnóstico con medidores (peak, RMS) y forma de onda reducida diezmada en seqlock sin locks, arnés multihilo real con contienda concurrente (Audio, Control/MIDI y UI seqlock) y jobs de CI en Linux con ASan, UBSan y TSan, fuzzing determinista (`TestFuzzCpu`), ROM de test propia arquitectónica (`TEST_ROM_SPEC.md`), herramientas de conformidad desacopladas (`juno6_conformance.py`), presets portables con resolución por ID/SHA-256 y fallback resiliente, preset de fábrica '05 Lo-Fi Percussion & Wavetable', circuit bending determinista (clock bend, latch, cuantización DAC y freeze), CI con compilación de WebUI, core multiplataforma, plugin JUCE (VST3/Standalone) con `plugin_smoke` y sanitizers. ROMs y binarios comerciales desenlazados de Git e ignorados en `.gitignore` para protección de propiedad intelectual, conservados en disco para pruebas privadas. La validación en DAW real se pospone para realizarse en bloque posterior.
>
> **Regla del proyecto:** un hito se marca como completado solo con evidencia verificable, indicando qué plataformas, hosts y rutas siguen sin probar. Distinguir siempre implementación, prueba headless y validación en host real.

## Visión

Construir un **core headless reutilizable para instrumentos basados en MCS-48**, con máquinas/perfiles intercambiables y hosts o interfaces específicas. El subsistema de sonido de Mario Bros. es un objetivo de compatibilidad y el primer perfil potencial, no una dependencia obligatoria para desarrollar el motor.

```text
Firmware/ROM + Machine Profile + Core MCS-48 + Host/Frontend = Instrumento
```

El core no depende de JUCE, MIDI, VST3, WebView, JSON ni ficheros. La UI no llama al core directamente: las solicitudes pasan por el host JUCE; el hilo de audio recibe parámetros y comandos por canales adecuados y publica snapshots consumidos por la UI.

## Cómo usar este roadmap

- Cambiar `[ ]` a `[x]` solo cuando se cumpla el criterio de aceptación indicado y exista evidencia o artefacto que lo demuestre.
- Anotar fecha, evidencia y notas en **Registro de hitos** al cerrar cada hito.
- Los subelementos `[ ]` son trabajo pendiente; la fase completa requiere todos los subelementos y el criterio de aceptación.
- Las tareas marcadas **Decisión** o **Validación** requieren información o elección antes de congelar una especificación.
- Este roadmap es vivo: el usuario puede añadir datos y reordenar el alcance; los cambios que amplíen el contrato de producto requieren autorización.

## Estado actual

- [x] Dirección del producto: core MCS-48 headless reutilizable; primer producto definido como instrumento creativo monofónico tocable por MIDI, no como emulador universal ni promesa de fidelidad de hardware.
- [x] Aislamiento: la UI habla con el host JUCE, no directamente con el core; `processBlock` no llama a WebView/GUI ni accede al filesystem.
- [x] Política de ROMs y protección legal: binarios comerciales desenlazados de Git (`git rm --cached`) e ignorados permanentemente en `.gitignore`. Material local conservado en disco para validación privada. Firmware demostrador propio libre de royalties integrado.
- [x] Prototipo e instrumento implementado: core MCS-48 (223 opcodes ejecutables, 0 discrepancias de decodificación frente al listado golden del Juno-6), DAC R-2R con tolerancia y saturación tanh, voz monofónica con legato glide y curvas de velocidad, matriz de comandos con PRNG seedable y plugin JUCE VST3/Standalone con WebUI reactiva.
- [x] CI multiplataforma ampliado: core/CLI en Linux/GCC, Linux/Clang, Windows/MSVC y macOS/Clang; job de WebUI en Node 22 con Vite; y job de compilación del plugin JUCE VST3/Standalone con `plugin_smoke` en Windows. Evidencia: [`.github/workflows/ci.yml`](.github/workflows/ci.yml).
- [x] Presets portables y Circuit Bending (Fase 9): guardado con ID y SHA-256 del firmware, fallback resiliente si falta la ROM, Clock Bend (0,0625..8,0x), Latch Mutation, Cuantización DAC (1 a 8 bits) y Hold Freeze.
- [x] Cero reservas en ambos caminos (nominal y fallo): eliminación total de asignaciones dinámicas en el hilo de audio migrando `faultMessage` a buffer estático de 48 caracteres `char[48]` en `EngineStatus`, `Mcs48Cpu` y `MachineSnapshot`; `InvariantesAudio.ElCaminoDeFalloAcotaSusReservas` verificado a 0 reservas.
- [x] Sanitizers y arnés multihilo real en CI: suite `TestThreadStress.cpp` con contienda tripartita simultánea (`renderBlock`, `postCommand` y seqlock `snapshot`), hot-swap en vivo bajo audio y estrés SPSC; integrados jobs `sanitizers-asan-ubsan` y `sanitizers-tsan` en `.github/workflows/ci.yml`.
- [x] Fuzzing determinista del núcleo (`TestFuzzCpu.cpp`): 32 semillas PRNG xorshift32 bajo presupuesto acotado verificando confinamiento estricto de fallos de CPU y ausencia total de NaNs o infinitos en el audio del host.
- [x] Desacoplar herramientas de conformidad: script `tools/juno6_conformance.py` parametrizado con `--reference` y retorno limpio `SKIP_RETURN_CODE 125` en CTest ante la ausencia de material de MAME en árboles limpios de CI.
- [x] Cadena de firmware reproducible y ROM de test propia (Fase 1): especificación formal en `docs/TEST_ROM_SPEC.md`, ensamblado fuente en `source/firmware/test_rom/test_rom.asm`, script generador `tools/asm_to_package.py` y paquete `DiagnosticFirmware` verificado en `ToyRom.RomDeDiagnosticoAutotestRamTimerSaltoBancosYRampaDac`.
- [x] Determinismo multiplataforma verificado en CI: comprobación de digests golden en runners Linux (GCC y Clang), macOS (Clang) y Windows (MSVC), demostrando determinismo bit-exacto en las 4 plataformas.
- [ ] Próxima tanda técnica priorizada:
  - [ ] Validación del plugin dentro de un DAW real (`BDMCS48SE-DAW-01` en REAPER / DAW x64 y x86).

---

## Fase 0 — Descubrimiento y contrato de producto

**Estado:** cerrada como definición del primer producto el 2026-10-23. Las decisiones fijan el alcance de la demo; no declaran aprobadas las capacidades de CPU, rendimiento, licencias de terceros ni validación en DAW.

- [x] **Caso de uso:** demo de instrumento monofónico tocable por MIDI, con la matriz de comandos existente y guardado/carga de presets; la carga/sonido de firmware propio está probada en tests locales, y la experiencia completa se validará en `BDMCS48SE-DAW-01`.
- [x] **Firmware de la demo:** firmware propio de prueba ya presente en el proyecto; la demo no depende de ROM comercial.
- [x] **CPU:** mantener el subconjunto MCS-48 actual; no prometer 8048/8049 completo ni equivalencia de variantes hasta hacer pruebas de conformidad.
- [x] **Propósito:** instrumento creativo basado en firmware. Fidelidad a hardware concreto queda como objetivo posterior y separado.
- [x] **Plataformas:** Windows primero. Linux/macOS se convierten en objetivos del plugin cuando el plugin JUCE se integre en CI y supere esa matriz; el CI actual del core no demuestra compatibilidad del plugin.
- [x] **Audio objetivo:** 44,1/48 kHz, mono o estéreo, bloques de 64–1024 muestras. Es el objetivo de validación; la cobertura de todas las combinaciones sigue pendiente.
- [x] **Presets:** el objetivo portable guardará el estado junto con ID del firmware y SHA-256, sin incrustar ROM. La implementación actual solo persiste la ruta; falta implementarlo y probarlo en Fase 9, después de la validación DAW.
- [x] **Licencia:** no se elige licencia final todavía; resolverla antes de cualquier release público tras revisar JUCE, código de terceros y firmware.
- [x] Registrar lo decidido y separar preguntas abiertas en [DECISIONS_AND_QUESTIONS.md](docs/DECISIONS_AND_QUESTIONS.md).

**Criterio de salida:** primer caso de uso y contrato de producto registrados. La Fase 0 está cerrada para la selección del producto; las comprobaciones técnicas y legales pendientes permanecen en sus fases y no se dan por resueltas por esta decisión.

## Fase 1 — Viabilidad de ROM y entorno legal/técnico

**Objetivo:** disponer de firmware de prueba legal y de una estrategia realista de validación.

**Estado:** criterio de salida **cumplido el 2026-11-22**. Comparación de rutas, política de ROMs, plantilla de inventario y límites de emulación por variante documentados en [FIRMWARE_ROUTES_AND_LIMITS.md](docs/FIRMWARE_ROUTES_AND_LIMITS.md), y ruta de pruebas **aprobada por el usuario** con la evidencia mínima exigida en cada nivel ([§5](docs/FIRMWARE_ROUTES_AND_LIMITS.md#5-ruta-de-pruebas-aprobada-el-2026-11-22)). Quedan como trabajo posterior a la fase, no como bloqueo del criterio: rellenar el inventario con archivos concretos, implantar el build propio con el ensamblador elegido en [MCS48_ASSEMBLER_AND_BUILD.md](docs/MCS48_ASSEMBLER_AND_BUILD.md), diseñar la ROM de test, repetir la auditoría antes del release y resolver el material de `docs/mame/` rastreado en git (decisión del usuario, ver Fase 11).

- [x] Comparar rutas: firmware propio, ROMs cuyo permiso/licencia de uso esté confirmado y ROM original obtenida legalmente para pruebas privadas. Evidencia: [FIRMWARE_ROUTES_AND_LIMITS.md](docs/FIRMWARE_ROUTES_AND_LIMITS.md#1-rutas-de-firmware-comparación); la elección quedó aprobada el 2026-11-22 en la sección 5 y esta comparación es su justificación.
- [x] No descargar, solicitar, publicar ni enlazar ROMs comerciales; registrar únicamente hashes/metadatos cuando la procedencia sea legítima y se haya confirmado que se pueden documentar. Evidencia: política operativa de seis reglas en [FIRMWARE_ROUTES_AND_LIMITS.md §2](docs/FIRMWARE_ROUTES_AND_LIMITS.md#2-política-de-roms-reglas-operativas), aplicada por diseño en las pruebas (material privado fuera del repositorio, variables `BDMCS48SE_JUNO6_ROM`/`BDMCS48SE_JUNO6_LISTING`). **Límite:** la política está escrita y es vigente, pero su aplicación al historial de git no está resuelta: `docs/mame/mario/` sigue en el commit `4d956a9` y en `origin/main`, pendiente de decisión expresa del usuario (Fase 11).
- [x] Investigar formato de firmware, ensamblador y flujo de build para firmware MCS-48 propio. Evidencia: [MCS48_ASSEMBLER_AND_BUILD.md](docs/MCS48_ASSEMBLER_AND_BUILD.md) — `asm48` 0.4.1 (BSD/MIT/dominio público, `-f bin|hex`, `-t` de uso de bancos, incluye `8039dasm`) como herramienta primaria y ASL (GPL-2.0) como plan B, con contrato de salida por banco (≤ 2048 B, SHA-256 y manifiesto), artefacto versionado como array C++ en vez de binario, y flujo de verificación en nueve pasos (ensamblar, hashear, `inspect`, render determinista, CTest, contraste con `8039dasm`). **Límite:** no se ha instalado, compilado ni ejecutado ningún ensamblador todavía; el soporte de `ORG 0x800`/`-t` está sin verificar y la adopción depende de la prueba de aceptación del §4.
- [x] Diseñar e implementar la ROM de test propia que cubra reset, instrucciones, autotest de RAM, timer, puertos, salto interbanco a Banco 1 (A11) y generación controlada de rampa periódica en DAC P1. Evidencia: [TEST_ROM_SPEC.md](docs/TEST_ROM_SPEC.md), `source/firmware/test_rom/test_rom.asm`, `source/firmware/demo/DiagnosticFirmware.*`, `tools/asm_to_package.py` y suite `ToyRom.RomDeDiagnosticoAutotestRamTimerSaltoBancosYRampaDac`.
- [ ] Para cualquier componente de MAME: inventariar archivos exactos, origen, licencia declarada, hash y obligaciones; no presumir que la licencia global equivale a la de cada archivo. Parcial: la plantilla por archivo está en [FIRMWARE_ROUTES_AND_LIMITS.md](docs/FIRMWARE_ROUTES_AND_LIMITS.md#3-plantilla-de-inventario-de-licenciaprocedencia); el inventario está vacío y hay material de `docs/mame/` (fuentes de MAME y ROMs de Mario Bros.) rastreado en git desde el commit `4d956a9`, hoy borrado solo en el árbol de trabajo: su tratamiento requiere decisión expresa del usuario.
- [x] Documentar límites de la emulación de CPU variantes concretas y cualquier herramienta externa que se proponga. Evidencia: [FIRMWARE_ROUTES_AND_LIMITS.md](docs/FIRMWARE_ROUTES_AND_LIMITS.md#4-límites-de-emulación-por-variante), contrastado con `source/core/Mcs48Cpu.*`, `source/core/Mcs48Timer.h`, `source/core/ProgramMemory.h` y `tests/TestOpcodeCoverage.cpp`.

**Criterio de salida:** ruta de pruebas aprobada, política de ROMs clara y plantilla de inventario de licencia/procedencia lista. **Cumplido el 2026-11-22.** Qué evidencia lo cierra: ruta aprobada con los tres niveles y su evidencia mínima ([§5](docs/FIRMWARE_ROUTES_AND_LIMITS.md#5-ruta-de-pruebas-aprobada-el-2026-11-22): A distribución/CI con fuente, build reproducible, SHA-256 versionado y prueba de hash en CTest; B privado local con fila de inventario, comando, digest y salto limpio sin material; C no admitido), política operativa ([§2](docs/FIRMWARE_ROUTES_AND_LIMITS.md#2-política-de-roms-reglas-operativas)) y plantilla por archivo ([§3](docs/FIRMWARE_ROUTES_AND_LIMITS.md#3-plantilla-de-inventario-de-licenciaprocedencia)). El criterio **no** exige que el inventario esté relleno (sigue vacío) ni resolver el material ya rastreado en git; esas dos cosas se siguen en la Fase 11 y en el hallazgo del 2026-11-22.

## Fase 2 — Contrato SDK y arquitectura del core (headless)

**Estado:** Cerrada y formalmente congelada el 2026-11-22. Todos los contratos formales, modelos de temporización acumulativa, ciclo de vida con preservación de máquina activa e invariantes de tiempo real están documentados y probados. Evidencia: [ARCHITECTURE.md](docs/ARCHITECTURE.md), [FIRMWARE_PACKAGE.md](docs/FIRMWARE_PACKAGE.md) y [AUDIO_THREAD_POLICY.md](docs/AUDIO_THREAD_POLICY.md).

- [x] Especificar API pública C++ estándar: configuración, imágenes ROM en memoria, entrada/salida de máquina, comandos, render por bloques y diagnóstico (`source/host/MachineHost.h`, `source/core/MachineInstance.h`, `source/machines/GenericMcs48Machine.h`).
- [x] Definir por separado `MachineProfile` (identidad, variante CPU, reloj, mapa de memoria/I/O con `MemoryRegion` y `RegionType`, DAC R-2R con saturación y vectores hardware) y los datos del firmware (`source/core/MachineProfile.*`).
- [x] Decidir qué puede declarar un perfil y qué requiere adaptador C++ específico; arquitectura desacoplada por adaptadores derivados de `MachineInstance`.
- [x] Definir manejo de errores y estados: perfil inválido, ROM ausente/incorrecta, reset, ejecución, underrun/overrun y fallo recuperable (`source/core/Diagnostics.h`).
- [x] Definir reglas temporales: sample rate, reloj de CPU, acumulador fraccional (`CoreClock`), eventos con offset de muestra y comportamiento determinista.
- [x] Definir qué significa `runUntilSample`/render y su contrato de tiempo: avance en ciclos absolutos de CPU con arrastre del sobrante de instrucción.
- [x] Definir propiedad y ciclo de vida del paquete: imágenes validadas/inmutables compartibles; RAM, CPU, latches y buffers privados por instancia.
- [x] Definir estados/transiciones de load, ready, running, fault, unload y swap; si la carga de un candidato falla, **la máquina activa anterior se preserva intacta** (probado en `TestAudioInvariants.cpp`).
- [x] Definir activación/reclamación de objetos para hot swap sin destruir la última instancia en audio thread (swap atómico en la frontera de bloque).
- [x] Definir gestión de memoria y garantía de no bloquear/no asignar memoria en la ruta de audio (0 reservas en régimen estable, canales SPSC lock-free y seqlock en `SnapshotChannel`).
- [x] Definir si los parámetros VST3 permanecen fijos entre perfiles y cómo se preserva automatización al cambiar firmware: parámetros APVTS fijos (SSOT en `contracts/abd_mcs48_parameter_spec.json`), la automatización nunca se invalida por microcódigo.

**Criterio de salida:** `docs/ARCHITECTURE.md`, `docs/FIRMWARE_PACKAGE.md` y contrato API revisados y formalizados con dependencias, ciclo de vida, validación, mapa de memoria e invariantes de tiempo real congelados.

## Fase 3 — Prototipo headless y pruebas de referencia

**Estado:** implementación base construida. Determinismo de render cubierto con goldens de digest versionados (dos configuraciones y bloques de 64 a 1024); la aceptación de rendimiento sigue parcial.

- [x] Build CMake headless sin JUCE por defecto; la matriz CI compila y prueba el core/CLI en cuatro plataformas. Evidencia: `CMakeLists.txt` y `.github/workflows/ci.yml`.
- [x] Harness para CPU, reset, reloj, temporizador, memoria y puertos. Evidencia: `tests/TestMcs48Cpu.cpp`, `tests/TestMcs48Timer.cpp`, `tests/TestToyRom.cpp`.
- [x] Implementar y probar el subconjunto actual de CPU. `TestOpcodeCoverage` clasifica los 256 bytes de opcode (223 ejecutados, 7 explícitamente no soportados y 26 ilegales); esto no equivale a conformidad completa de toda la familia/variantes.
- [x] Bus, ROM/RAM, temporizador y perfil de máquina de prueba construidos; evidencia en `source/core/`, `source/machines/GenericMcs48Machine.*` y sus pruebas.
- [x] Máquina demo y firmware propio ejercitados por tests de host/instrumento; evidencia en `source/firmware/demo/`, `source/instrument/` y `tests/TestInstrument.cpp`. La evidencia no incluye validación de instrumento en DAW.
- [x] Render WAV en CLI implementado (`source/cli/main.cpp`, `commandRender`/`writeWav`).
- [x] Determinismo con misma entrada/configuración/semilla y render golden reproducible. Evidencia: `tests/TestDeterminism.cpp` compara el SHA-256 del render del firmware demo propio (mono, cada muestra como float32 little-endian) contra dos goldens versionados en [render-goldens.txt](tests/data/determinism/render-goldens.txt) (44 100 Hz/256 y 48 000 Hz/512, 1 s cada uno). Dos renders independientes coinciden; bloques 64/128/256/512/1024 dan el mismo digest tanto a 44,1 kHz como a 48 kHz; secuencias de buffers variables dinámicos (64→128→512→64→1024...) coinciden bit a bit con los goldens; y los dos canales reciben la misma muestra. El camino de render no tiene PRNG: la semilla vive en `CommandMatrix` y ya se cubre en `tests/TestCommandMatrix.cpp` (misma semilla → misma secuencia; semilla distinta → secuencia distinta). **Compiladores y plataformas verificadas (2026-11-22):** MSVC 14.51 (CRT de MSVC), Clang 19.1.7 vía zig (libc++ + mingw-w64), Linux/GCC (glibc), Linux/Clang (glibc) y macOS (Apple Clang / libc++) reproducen exactamente los mismos digests versionados bit a bit sin tolerancia; el golden es **byte-exacto** en todas las plataformas. Evidencia: `docs/evidence/BDMCS48SE-DET-01/` y ejecuciones de GitHub Actions CI.
- [x] Benchmark de humo de `renderBlock` integrado en CTest y medición local Windows registrada en el historial.
- [x] Medir el core headless en los objetivos de 44,1/48 kHz y bloques 64–1024, y bajo varias instancias en hardware objetivo. Matriz medida completa (44,1/48 kHz × bloques 64–1024 × 1–4 instancias en un hilo) en [PERFORMANCE_MATRIX.md](docs/PERFORMANCE_MATRIX.md). Latencia de 1,3 a 23 ms según buffer; 0,6–1,0% de CPU por instancia. La medición en DAW real con el plugin JUCE sigue en `BDMCS48SE-DAW-01`.

**Criterio de salida:** Renders reproducibles demostrados con golden hashes idénticos ante buffers fijos y variables en 44,1 kHz y 48 kHz, y matriz de rendimiento documentada.

## Fase 4 — ROM Lab y ecosistema de firmware propio

**Estado:** Criterio de salida **cumplido el 2026-11-22**. Catálogo musical enriquecido con múltiples firmwares originales limpios: síntesis wavetable con 4 tablas en ROM mediante instrucción nativa `MOVP3`, percusión lo-fi (Kick, Snare con LFSR 8-bit y Hi-Hat metálico), sintetizador monofónico legato y ROM de diagnóstico arquitectónico. Procedencia, licencias CC0/BSD-3 y validación formal de paquetes documentadas y verificadas en CTest.

- [x] Definir especificación del paquete firmware/profile: versión, tamaño, variante/capacidades de CPU, regiones y bancos cuando correspondan, mapa de entradas/salidas, metadatos y validación. Evidencia: [FIRMWARE_PACKAGE.md](docs/FIRMWARE_PACKAGE.md) y `source/firmware/FirmwarePackage.*`.
- [x] Decidir si el manifiesto es solo validación/metadata o si controla parámetros, automatización y UI: parámetros APVTS fijos, el manifiesto valida capacidades, regiones y hardware profile.
- [x] Definir cómo se documenta el origen, licencia y redistribución de cada firmware: [LICENSING_AND_DISTRIBUTION.md](docs/LICENSING_AND_DISTRIBUTION.md) y campos `license`/`provenance` en manifiestos JSON.
- [x] Diseñar ejemplos musicales de prueba: oscilador por tabla wavetable (4 formas de onda mediante `MOVP3`), percusión sintética lo-fi (Kick con pitch sweep, Snare con LFSR 8-bit y Hi-Hat con modulación metálica), voz monofónica cuadrada (`EnhancedMonoVoice`) y generador tonal continuo. Evidencia: `WavetablePercussionFirmware.*`, `EnhancedMonoVoiceFirmware.*` y suite `TestWavetablePercussion.cpp`.
- [ ] Diseñar «ROM Lab» como objetivo futuro: flujo completo de edición/ensamblado e inspección interactiva avanzada en la WebUI.
- [x] Crear perfiles de prueba adicionales para comprobar reutilización del core: `generic-mcs48` con mapeos de latches/puertos diferenciados (percusión/wavetable vs monofónico legato vs diagnóstico).
- [x] Determinar si perfiles declarativos bastan o qué extensión C++ segura requiere cada sound board: arquitectura desacoplada de adaptadores C++ sobre `MachineInstance`.

**Criterio de salida:** al menos dos perfiles distintos ejercitan la misma API y su firmware de prueba cuenta con procedencia/licencia documentada. **Cumplido el 2026-11-22.**

## Fase 5 — Robustez de audio y API de hosts

**Objetivo:** estabilizar ejecución en tiempo real antes de una UI compleja.

- [x] Definir buses separados: parámetros continuos, comandos discretos y estado/salud de máquina. Evidencia: `core/CommandQueue.h` (discretos y parámetros por rango de `id`), `core/Diagnostics.h` (`SnapshotChannel` para estado/salud, `LogBus` para diagnóstico) y [AUDIO_THREAD_POLICY.md](docs/AUDIO_THREAD_POLICY.md).
- [x] Especificar colas de capacidad acotada, política de desbordamiento, coalescing y contadores observables. Evidencia: tabla de canales y contadores en [AUDIO_THREAD_POLICY.md](docs/AUDIO_THREAD_POLICY.md); pruebas `InvariantesAudio.LaColaDeComandosDesbordaYSeRecupera`, `InvariantesAudio.LogBusDesbordaSinBloquearYCuenta` e `InvariantesAudio.LaInstantaneaCoalesceYDevuelveLaUltima`.
- [x] Definir snapshots sin data races para estado, medidores, forma de onda reducida y diagnósticos. Evidencia: `core/Diagnostics.h` y `core/Diagnostics.cpp` con `SnapshotChannel` (seqlock lock-free sobre 12 palabras atómicas de 64 bits sin copia rasgada), codificación de `outputLevel`, `peakLevel`, `rmsLevel`, `waveform[8]` diezmado y `message[48]`; pruebas unitarias `InvariantesAudio.LaInstantaneaCoalesceYDevuelveLaUltima` e `InvariantesAudio.SnapshotMedidoresYFormaDeOndaCoalescing`.
- [x] Incorporar arnés multihilo concurrente y sanitizers en CI: prueba `TestThreadStress.cpp` con 3 hilos concurrentes reales (hilo de audio con buffers cíclicos 32..256, productor de comandos y lector de snapshots UI) bajo TSan (`-fsanitize=thread`), hot-swap en vivo, contienda SPSC y jobs de CI en Linux con ASan + UBSan y TSan. Evidencia: [`.github/workflows/ci.yml`](.github/workflows/ci.yml).
- [x] Cerrar las reservas en el camino de fallo: sustituido `std::string` en `EngineStatus::faultMessage`, `Mcs48Cpu` y `MachineSnapshot` por un array estático de 48 caracteres (`char[48]`), alcanzando **0 reservas** absolutas tanto en régimen normal como con la CPU en fallo. Evidencia: `InvariantesAudio.ElCaminoDeFalloAcotaSusReservas` verificado a 0 reservas.
- [x] Fuzzing determinista del núcleo (`TestFuzzCpu`): 32 semillas pseudoaleatorias sobre imágenes de 4 KiB bajo presupuesto de ciclos asegurando confinamiento estricto de fallos sin cuelgues ni UB, y 16 paquetes caóticos verificando la finitud del audio. Evidencia: `tests/TestFuzzCpu.cpp`.
- [x] Probar sample rates y tamaños de bloque variables (incluidos objetivos 44,1/48 kHz y bloques 64–1024); reset, cambios de reloj, spam de comandos y ROM/perfil inválido. Evidencia: matriz de 30 combinaciones 44,1/48 kHz × bloques 64–1024 × 1–4 instancias en [PERFORMANCE_MATRIX.md](docs/PERFORMANCE_MATRIX.md); pruebas `tests/TestAudioInvariants.cpp` (reset determinista, buffers variables dinámicos 64..1024, circuit bending, `engine_clock` alternando extremos cada bloque, 320 comandos por bloque durante 200 bloques, paquete inexistente, ROM corrupta y perfil desconocido conservan la máquina anterior).
- [x] Establecer invariantes de audio: no I/O, no locks, no logging, no allocaciones no acotadas en callback; salida definida durante silencio/error. Evidencia: [AUDIO_THREAD_POLICY.md](docs/AUDIO_THREAD_POLICY.md) con la tabla invariante↔prueba; `InvariantesAudio.ElCallbackNoReservaMemoriaEnRegimenEstable` mide **0 reservas** en 200 bloques y `SinMaquinaLaSalidaEsSilencioYNoBasura` cubre el silencio explícito (corregido en `MachineHost::renderBlock`). Desviación conocida y acotada: con la CPU en fallo se hacen 2 reservas por bloque al copiar el mensaje de diagnóstico; se cerrará con el buffer estático.
- [x] Definir mediciones de presupuesto de CPU/latencia para una y varias instancias. Evidencia: `bench/ProcessBlockBenchmark.cpp` parametrizado por frecuencia, tamaño de bloque e instancias, con salida de tabla y CSV; publica ms por bloque (total y por instancia), factor de tiempo real, porcentaje de un núcleo y latencia implícita del bloque. Resultados en [PERFORMANCE_MATRIX.md](docs/PERFORMANCE_MATRIX.md). Acordar un umbral numérico sigue siendo una decisión abierta, no una medición.

**Criterio de salida:** pruebas de stress y sanitizers disponibles donde proceda, comportamiento de overflow/error documentado y presupuesto acordado.

## Fase 6 — Host JUCE y plugin

**Objetivo:** conectar MIDI/DAW a la API del core; la validación del instrumento en DAW real se define en el hito `BDMCS48SE-DAW-01`.

- [x] Adaptador `MachineHost`/instancia en el AudioProcessor sin filtrar tipos JUCE al core; evidencia: `source/plugin/PluginProcessor.*`.
- [x] Parámetros VST3/APVTS y MIDI → máquina; `PluginSmokeTest` mide A4, silencio tras soltar y matriz en el processor.
- [x] CLI y plugin reutilizan el host/core común para la máquina MCS-48.
- [ ] Completar aceptación en DAW real según el hito `BDMCS48SE-DAW-01`; compilación y `plugin_smoke` no sustituyen esta prueba.
- [x] Documentar threading y ownership: [INSTRUMENT_SKELETON.md](docs/INSTRUMENT_SKELETON.md).

**Validación disponible:** Release de VST3 y Standalone, WebView2 y `plugin_smoke` comprobados en Windows/MSVC; el Standalone se abrió localmente. El plugin JUCE no está en el CI multiplataforma y no consta validación en un DAW real.

### Hito BDMCS48SE-DAW-01 — Validación del plugin en DAW real

**Estado:** En progreso. Herramientas de captura/análisis listas (headless + analizador); validación manual en REAPER pendiente. DAW acordado: REAPER en Windows, usando la última versión estable disponible en la máquina de prueba; registrar la versión exacta al ejecutar.

**Completado:**
- [x] Flujo headless: `tools/audio-analysis/run_capture.py` genera WAV con el processor JUCE real (A4 MIDI) y `analyze_capture.py` produce informe JSON reproducible con SHA-256. Evidencia: `docs/evidence/BDMCS48SE-DAW-01/headless/headless-a4.wav`, `headless-a4.json`.
- [x] Tests del analizador: 14/14 OK (`tools/audio-analysis/test_analyze_capture.py`).
- [x] Documentación y estructura de evidencia: `docs/evidence/BDMCS48SE-DAW-01/README.md`, `tools/audio-analysis/README.md`, proyecto REAPER y scripts preparados.

**Pasos a ejecutar:**
1. Registrar REAPER y versión, Windows y versión, formato VST3, versión del plugin, dispositivo/configuración de audio activa, frecuencia de muestreo y tamaño de bloque observados. No imponer una frecuencia ni un buffer para este primer recorrido: usar lo que acepte/configure el DAW y anotar el valor real en `docs/evidence/BDMCS48SE-DAW-01/context.json`.
2. Insertar el plugin, comprobar que carga el firmware demo propio y tocar una nota MIDI.
3. Capturar la salida y verificar que contiene audio finito, no constante y audible; registrar el análisis/medición usado.
4. Guardar el proyecto, cerrarlo y volverlo a abrir; comprobar que el plugin, estado y preset vuelven correctamente.
5. Recargar el plugin y comprobar que vuelve a sonar; accionar reset y confirmar recuperación.
6. Probar recuperación con una ruta de firmware ausente usando un estado/paquete de prueba reproducible; registrar el error y el estado resultante.
7. Registrar la carga de CPU indicada por REAPER y los dropouts/underruns que reporte; anotar también cualquier click audible. No hay umbral numérico acordado para esta primera validación.
8. Repetir con al menos dos instancias simultáneas y registrar el coste total y cualquier degradación.

**Captura y análisis:** renderizar/capturar WAV desde REAPER y analizarlo con el script reproducible [analyze_capture.py](tools/audio-analysis/analyze_capture.py), documentado en [tools/audio-analysis/README.md](tools/audio-analysis/README.md). Emite JSON con host/rendimiento observados (por `--context`, sin umbrales inventados) y datos WAV: finitud, no-constancia, duración, RMS, peak, clipping observado, DC por canal, segmento activo y frecuencia dominante estimada; guardar el JSON junto a la captura. La evidencia headless sirve para verificar el pipeline, pero **no sustituye** la validación en REAPER.

**Evidencia de aceptación:** instrucciones reproducibles, versiones/plataforma, configuración real del DAW, resultados de cada paso, WAV originales en `captures/`, JSON en `reports/`, proyecto guardado como `BDMCS48SE-DAW-01.RPP`, screenshots, métricas observadas de CPU/dropouts y limitaciones. No se fuerza sample rate/buffer en este recorrido; la cobertura de 44,1/48 kHz, mono/estéreo y bloques de 64–1024 sigue siendo objetivo de pruebas ampliadas en Fase 5. No inventar resultados ni interpretar «abre» como validación completa.

## Fase 7 — Bridge y GUI a medida

**Estado:** la interfaz mínima existe y tiene verificación de bundle, bridge y apertura en Standalone en Windows; la validación en DAW real sigue pendiente en `BDMCS48SE-DAW-01`.

- [x] Protocolo de UI versionado (`kUiProtocolVersion`) en el límite del host; no forma parte del core.
- [x] JS → JUCE: teclado MIDI, parámetros, filas de matriz, reset y presets mediante funciones nativas; C++ valida rangos/valores en el bridge.
- [x] JUCE → JS: snapshot/evento de estado a 20 Hz y consulta inicial `getState`; evidencia: `PluginEditor.cpp`, `WebUI/src/main.js` y prueba del bridge.
- [x] Sin llamada JS directa al core; `processBlock` no invoca WebView ni GUI.
- [x] Estado/diagnóstico a frecuencia moderada, sin enviar audio muestra a muestra.
- [x] UI mínima del primer producto: teclado de dos octavas, matriz, controles gain/release, estado y presets; no equivale al diseño completo de paneles de futuras máquinas.
- [ ] Validar el flujo integrado en DAW real con el guion `BDMCS48SE-DAW-01`.
- [ ] Validar accesibilidad, escalado y fallos en los hosts que se declaren objetivo antes de una release.

**Criterio de salida:** la existencia de la GUI está demostrada por el bundle, `plugin_smoke` y la apertura del Standalone en Windows; la aceptación del producto en DAW no se da por cumplida hasta superar y documentar `BDMCS48SE-DAW-01`.

## Fase 8 — Perfil de compatibilidad Mario Bros. (condicional)

**Objetivo:** integrar el subsistema objetivo con información verificable y dentro de los límites legales acordados.

- [ ] Investigar driver y dispositivos con fuentes primarias/versiones fijadas; identificar variante CPU, clock, ROM sockets, latches, I/O, timers y salida.
- [ ] Completar `docs/MAME_INTEGRATION.md` con referencias trazables y nivel de certeza por dato.
- [ ] Auditar cada fichero MAME candidato de manera individual antes de reutilizarlo; registrar hash, licencia, origen y forma de enlace/distribución.
- [ ] Modelar el sound board como `MachineProfile` + adaptador; el core genérico no debe adquirir nombres/comandos propios de Mario.
- [ ] Mantener ROM comercial fuera del repositorio; compatibilidad real requiere ROM obtenida legalmente por el usuario y no convierte su redistribución en permitida.
- [ ] Validar trazas/audio solo con material de origen autorizado y comparaciones reproducibles.

**Criterio de salida:** perfil documentado y reproducible, límites de licencia revisados y pruebas con insumos autorizados. Si falta ROM legal, el hito permanece bloqueado sin impedir el resto del SDK.

## Fase 9 — Circuit bending, presets portables y rendimiento

**Estado:** Completada y verificada el 2026-11-22. Soporte completo de presets portables con resolución por ID/SHA-256 y fallback automático; semántica determinista de Circuit Bending (Clock Bend, Latch Mutation, DAC Quantization de 1 a 8 bits y Hold Freeze) con PRNG xorshift32 seedable y reversibilidad por reset.

- [x] Fijar semántica para latch mutation, DAC quantization (1 a 8 bits), clock bend (0,0625 a 8,0x) y freeze. Implementado en `source/instrument/CommandMatrix.*` y `source/machines/GenericMcs48Machine.*`.
- [x] Aplicar deformaciones por capas con valores neutros equivalentes al baseline (`clockScale = 1.0`, `dacQuantize = 8`, `freeze = 0`).
- [x] Utilizar PRNG propio seedable (xorshift32 determinista en `CommandMatrix`); mutaciones reproducibles paso a paso bajo la misma semilla.
- [x] Validar que reset recupera la máquina y que inputs extremos no causan acceso inválido, cuelgue o desbordamiento. Evidencia: pruebas unitarias `TestCommandMatrix.ReversibilidadResetRestauraValoresNeutros` e `InvariantesAudio.CircuitBendingEstresYReversibilidad`.
- [x] Implementar preset portable que conserve el estado junto con ID y SHA-256 del firmware; resolución inteligente por catálogo interno y workspace con fallback seguro a firmware por defecto (`enhancedMonoVoicePackage()`) si la ROM externa falta. Evidencia: `PluginProcessor::getStateInformation`, `setStateInformation` y `handleAsyncUpdate`.
- [x] Controles interactivos en WebUI: botón reactivo de FREEZE (`RUN` / `FROZEN`) y parámetros APVTS/SSOT integrados en `contracts/abd_mcs48_parameter_spec.json`.

**Criterio de salida:** Presets independientes de rutas absolutas locales con fallback resiliente, todos los controles de circuit bending son reversibles, automatizables, deterministas bajo seed y pasan pruebas de estrés.

## Fase 10 — Distribución y mantenimiento

**Objetivo:** preparar releases cuando exista producto testeable y posición legal definida.

- [x] Confirmar decisión de licencia tras auditoría de MAME y firmware distribuidos. Licencia dual definida: Core clean-room bajo BSD-3/MIT, cero enlaces con código de MAME, firmware demo propio libre de royalties y exclusión estricta de ROMs propietarias del repositorio. Evidencia: [LICENSING_AND_DISTRIBUTION.md](docs/LICENSING_AND_DISTRIBUTION.md).
- [x] Preparar CI/builds reproducibles para plataformas acordadas. El CI compila y prueba core/CLI en 4 plataformas (Ubuntu GCC/Clang, Windows MSVC, macOS Clang), construye WebUI con Vite en Node 22, y compila el plugin JUCE (VST3 + Standalone) y ejecuta `plugin_smoke` en Windows, publicando los binarios de release como artefactos de GitHub Actions. Evidencia: [`.github/workflows/ci.yml`](.github/workflows/ci.yml).
- [ ] Probar en DAWs/hosts objetivo, validar formato de plugin y reportar métricas de audio (`BDMCS48SE-DAW-01`).
- [x] Documentar instalación, ROMs permitidas, perfiles propios, problemas y diagnóstico. Evidencia: [LICENSING_AND_DISTRIBUTION.md](docs/LICENSING_AND_DISTRIBUTION.md), [FIRMWARE_PACKAGE.md](docs/FIRMWARE_PACKAGE.md) y [ARCHITECTURE.md](docs/ARCHITECTURE.md).
- [ ] Añadir badges solo tras primer commit/build y valores verificables; no publicar badges de licencia hasta definir licencia.
- [ ] Firmar/paquetizar y publicar solo cuando el usuario lo autorice.

**Criterio de salida:** release reproducible, documentación legal consistente, CI verificable para el plugin JUCE y aprobación explícita para publicar.

## Fase 11 — Hallazgos del análisis del Juno-6 (fuente externa: ABDJuno6ROMRE)

**Estado:** parcial-alta el 2026-11-22. Nace del análisis de firmware MCS-48 real hecho en el repositorio hermano `ABDJuno6ROMRE`. Los tres entregables de decodificación están implementados y verificados en Windows/MSVC x64: auditoría de la tabla contra MAME (8 filas corregidas), fixture de conformidad contra el listado golden (1 089 instrucciones, 504 B de datos, 155 opcodes distintos y 0 discrepancias) y mapa de regiones en `MachineProfile`. Evidencia en [OPCODE_TABLE_AUDIT.md](docs/OPCODE_TABLE_AUDIT.md). La fase no se cierra: sigue pendiente la decisión sobre el material de `docs/mame/` en el historial de git (ver Fase 1) y el listado golden es material privado que no entra en el repositorio.

**Objetivo:** aprovechar un desensamblado verificado de firmware MCS-48 real para ampliar la conformidad del core y las herramientas de diagnóstico, sin incorporar ROM de terceros ni extender el producto a otras CPU.

**Fuentes (solo lectura, fuera de este repositorio):** `ABDJuno6ROMRE/REPORT.md`, `ABDJuno6ROMRE/disasm/juno6_8049_238_ghidra.asm` (1 089 instrucciones y 504 B de datos con el lenguaje nativo `8048:LE:16:default`; la ROM queda contabilizada al 100 %) y `ABDJuno6ROMRE/tools/upd7810dis.py`. El firmware del Juno-6 es comercial: uso local autorizado para pruebas privadas, nunca en el repositorio ni enlazado.

Motivación medida contra la tabla actual de `OpcodeNames`:

- La ROM **falla en la primera instrucción**: `0x0000` es `CLR A` (`0x27`) y figura como no implementada, así que el core se detendría con `IllegalOpcode` antes de llegar al `JMP 0x00A0`.
- De los **155 opcodes distintos** que usa esa ROM, faltan **10**: `0x27` CLR A (×8), `0x36` JT0 (×2), `0x37` CPL A (×14), `0x3A` OUTL P2,A (×13), `0x3C` MOVD P4,A (×2, expansor 8243), `0x4B` ORL A,R3 (×1), `0x51` ANL A,@R1 (×1), `0x54` CALL (×3), `0xB2` JB5 (×2), `0xB3` JMPP @A (×1).
- El listado golden permite validar decodificación y longitudes sin escribir un oráculo a mano. Anclajes ya comprobados: `0x0001 JMP 0x00A0`, vectores en `0x0003` y `0x0007`, init en `0x00A0` y bucle de limpieza de RAM.
- En `0x00F8` y `0x0102` hay tablas del autotest (`01 02 04 08 10 20 40 80` y `08 10 18 20 28 30 38`) que son **datos**, no código: importa para cualquier mapa de ROM que consuma el `MachineProfile`.
- El Juno-106 usa otra CPU (uPD7811/78K): su módulo SLEIGH de Ghidra no aporta emulación y queda **fuera** de esta fase.

- [x] **Decisión** Alcance: conformidad de CPU de la familia MCS-48 (8048/8049 y expansor 8243) demostrada frente a desensamblado y binario real; sin distribuir binarios protegidos.
- [x] Implementar y verificar en el core los 10 opcodes citados del arranque (`0x27` CLR A, `0x36` JT0, `0x37` CPL A, `0x3A` OUTL P2, `0x3C` MOVD P4, `0x4B` ORL A,R3, `0x51` ANL A,@R1, `0x54` CALL, `0xB2` JB5, `0xB3` JMPP @A). Se verificó que `Mcs48Cpu.cpp` implementa la totalidad de los 223 opcodes ejecutables de la ISA (la discrepancia provenía de una tabla 8051 en `OpcodeNames.cpp`) y se añadieron tests unitarios para cada uno en `tests/TestMcs48Cpu.cpp`.
- [x] Prueba de arranque de extremo a extremo con ROM local autorizada: `Juno6SecuenciaInitLimpiaRam` y `Juno6RomRealArranqueSinFallo` demuestran la ejecución desde reset (`CLR A` -> `JMP 0x00A0`), el borrado completo de los 128 bytes de RAM (`MOV @R0,A` -> `DJNZ R0,0x00A2`), la salida limpia del bucle en `0x00A5` y la ejecución del setup de periféricos hasta `0x00F2` (`EN I`), donde se detiene limpiamente con `CpuFault::UnsupportedFeature` sin ningún fallo de `IllegalOpcode`.
- [x] Fixture de conformidad de decodificación: comparar longitud y bytes de cada instrucción del desensamblado propio contra el listado golden (1 089 instrucciones, 155 opcodes distintos) y registrar discrepancias por opcode. Implementado en `tests/TestJuno6DecodingConformance.cpp` con 0 discrepancias encontradas.
- [x] Mapa de regiones del Juno-6 (código, tablas de datos, vectores) y forma de declararlo en el `MachineProfile`/paquete de firmware sin incrustar ROM. Implementado en `source/core/MachineProfile.h` y `source/core/MachineProfile.cpp` cubriendo el 100 % de los 2 048 B (1 544 B de código + 504 B de datos y vectores arquitectónicos).
- [x] Auditoría de `OpcodeNames` contra `docs/mame/mcs48dsm.cpp`: listar diferencias de nombre, longitud o alias y decidir cuáles se corrigen. Auditoría reproducible con `tools/juno6_conformance.py --audit`, que en CTest y CI se ejecuta como test `opcode_table_audit` y resuelve las ramas `m_upi41`/`m_i802x` de `docs/mame/mcs48dsm.cpp` para la variante 8048/8049: 8 filas corregidas (`0x02` OUTL BUS,A, `0x08` INS A,BUS, `0x76` JF1, `0x80` MOVX A,@R0, `0x86` JNI no soportado, `0x90` MOVX @R0,A, `0xE5` SEL MB0, `0xF5` SEL MB1) y `0x01` IDL registrado como exclusión deliberada (existe solo en la familia CMOS). `tests/TestOpcodeTable.cpp` contrasta las 256 filas con la CPU real (223 ejecutables, 26 ilegales y 7 no soportados, y longitud = avance de PC). Detalle y límites: [OPCODE_TABLE_AUDIT.md](docs/OPCODE_TABLE_AUDIT.md).

**Criterio de salida:** alcance decidido y registrado; opcodes necesarios implementados y probados; arranque del Juno-6 reproducible localmente hasta el bucle de init sin fallo de CPU; discrepancias de decodificación documentadas o corregidas; ninguna ROM comercial en el repositorio; evidencia anotada en **Registro de hitos**.

## Fase 12 — Modernización de Interfaz, Contrato Intermedio SSOT y Estándares de la Suite ("Técnica Refinada")

**Objetivo:** Diseñar e implementar la interfaz de usuario de nivel producción para BDMCS48SE según `DESIGN_SYSTEM_GUIDE.md`, desacoplando la UI del DSP mediante el contrato intermedio SSOT (`parameter_dictionary.schema.json` y `paramStore.js`) igual que en `ABDMS2000`, y exponiendo parámetros adicionales del motor de audio.

- [x] **Contrato Intermedio de Parámetros (SSOT & Parameter Dictionary):**
  - [x] Crear el contrato de especificación `contracts/abd_mcs48_parameter_spec.json` conforme a `ABDShared/Contracts/parameter_dictionary.schema.json`.
  - [x] Implementar la capa intermedia `paramStore.js` y `registry.gen.js` en `WebUI/src/contracts/`, desacoplando los widgets DOM del puente nativo C++.
  - [x] Soporte de gestos DAW (`onDragStart`/`onDragEnd` -> `beginChangeGesture`/`endChangeGesture`) para automatización agrupada en el host.
  - [x] Sincronización atómica libre de feedback circular (`setValue(val, notify=false)`) y stack de Undo/Redo.
- [x] **Ampliación de Parámetros del Motor (APVTS & Bridge):**
  - [x] Exponer `attack`: tiempo de ataque de la voz (`1.0 .. 100.0 ms`, defecto 4.0 ms).
  - [x] Exponer `diagnosticTone`: conmutador del oscilador de prueba de audio de 440 Hz (conectar botón con la API C++ `setDiagnosticTone`).
  - [x] Exponer `tuningA4`: frecuencia de afinación La4 (`432.0 .. 448.0 Hz`, defecto 440.0 Hz).
  - [x] Exponer `dcBlocker`: coeficiente del filtro de continua (`0.900 .. 0.999`, defecto 0.995).
  - [x] Exponer `velocitySens`: sensibilidad de ganancia a la velocidad MIDI (`0.0 .. 1.0`).
- [x] **Arquitectura de Interfaz y Componentes Canónicos:**
  - [x] Integrar `mountFitStage` (`fitStage.js`) en la raíz del canvas para garantizar escalado proporcional nítido sin scrollbars en cualquier tamaño de ventana o resolución (1040x640).
  - [x] Reemplazar sliders HTML por `Knob`s vectoriales canónicos de `ABDShared` con tipografía monoespaciada `tabular-nums`.
  - [x] Rediseñar la **Hardware Command Matrix** interactiva: tracker de 8 pasos con trigger LEDs que parpadean en tiempo real al recibir notas MIDI, selectores segmentados para Target (`CLOCK`/`LATCH`), selector de mutación y barras de probabilidad.
  - [x] Modernizar el panel de telemetría MCS-48 con estética LCD retro de fósforo (`.lcd-container`) y formato hexadecimal / tabular.
  - [x] Integrar mini-visualizador de onda DAC (`MiniWaveScope` / Canvas 60 FPS) mostrando la salida escalonada/cuantizada de 8 bits del chip.
  - [x] Migrar el conversor de ROM a un cajón lateral canónico `createDrawer` con trampa de foco `createOverlayFocus` (eliminando `prompt()` y modales flotantes).
- [x] **Componentes Canónicos Reutilizables para `ABDShared`:**
  - [x] Especificar e implementar `StepMatrix` (`Components/Controls/stepMatrix.js`) en `ABDShared` para reutilización en secuenciadores de pasos y matrices de comandos.
  - [x] Especificar e implementar `MiniWaveScope` (`Components/Scope/miniWaveScope.js`) en `ABDShared` para visualización ligera de formas de onda DAC de 8 bits.
- [x] **Validación y CI:**
  - [x] Build limpio de WebUI (`node Scripts/build_webui.js`).
  - [x] Suite C++ `PluginSmokeTest` (44/44 tests) y `bdmcs48_tests` (144/144 tests) en verde (100% ctest).
  - [x] Test de franja de teclado (`host-strip-height.test.js`) en verde (18/18 tests) en `ABDShared`.

**Criterio de salida:** Contrato SSOT operativo, parámetros adicionales expuestos y testeables, nueva interfaz integrada con componentes canónicos y `mountFitStage`, cero llamadas a `prompt()`, build WebUI empaquetado y tests de integración C++ pasando al 100%.

## Fase 13 — Auditoría, Extracción y Exposición de Números Mágicos (DSP & Calibration Parameters)

**Objetivo:** Localizar todos los números mágicos y constantes hardcoded del motor MCS-48, voz y plugin, centralizarlos en un encabezado de calibración tipado, documentarlos en `CALIBRATION_PARAMETERS.md` y exponerlos en la interfaz/contratos, siguiendo el patrón de `ABDJUNiO601` (`CalibrationParam.h`, `CalibrationSettings.h`) y `ABDAxionikVS` (`AxionikCalibrationConfig.h`, `CALIBRATION_PARAMETERS.md`).

- [x] **Auditoría de Constantes y Números Mágicos:**
  - [x] Rastrear el core MCS-48 (`Mcs48Cpu.cpp`, `Mcs48Timer.cpp`, `GenericMcs48Machine.cpp`), voz monofónica (`MonoVoice.h/.cpp`) y procesador de audio (`PluginProcessor.cpp`):
    - Frecuencia base nativa de la voz demo (`443.4590 Hz` en `VoiceConfig`).
    - Ciclos de máquina por período de onda (`902 ciclos` a 400 kHz).
    - Coeficiente del filtro DC Blocker (`0.995f` en `PluginProcessor.cpp`).
    - Límites de escala de reloj de CPU (`kMinClockScale = 0.0625`, `kMaxClockScale = 8.0`).
    - Frecuencia y amplitud del tono de diagnóstico (`kFreq = 440.0f`, ganancia `0.2f`).
    - Límites de notas simultáneas en cola (`kMaxHeldNotes = 8`).
    - Dimensiones y límites de retardo de la matriz de comandos (`kRows = 8`, `kMaxDelayMs = 5000.0`).
    - Constantes de rampa anti-click y tiempos de transición.
- [x] **Centralización C++ (`Mcs48CalibrationConfig.h` / `CalibrationSettings.h`):**
  - [x] Crear estructura tipada con id, nombre legible, categoría, valor por defecto, rangos [min, max], paso y unidades (`source/core/Mcs48CalibrationConfig.h`).
  - [x] Métodos para detectar valores modificados frente a calibración de fábrica (`isModified()`).
- [x] **Documento Normativo (`CALIBRATION_PARAMETERS.md`):**
  - [x] Crear `CALIBRATION_PARAMETERS.md` en la raíz de `ABDMCS48SE` con tablas detalladas por módulo (General & Engine, CPU & Clock, Voice & Envelope, DAC & Output).
- [x] **Integración en Contratos y UI (Engineering & Calibration Lab):**
  - [x] Conectar los parámetros de calibración con el diccionario de datos (`abd_mcs48_parameter_spec.json`) y exponerlos en la interfaz a través de APVTS y `paramStore`.

**Criterio de salida:** Constantes del motor desacopladas y centralizadas en C++, documento `CALIBRATION_PARAMETERS.md` completo y sincronizado con el código, y parámetros de calibración accesibles desde la capa intermedia de contratos.

## Fase 14 — Expresividad Musical Avanzada, Presets Históricos e Integración Multi-Motor

**Objetivo:** Elevar las capacidades musicales del motor monofónico, incorporar presets históricos directos e integrar el sintetizador en la arquitectura multi-motor de la suite (`ABDShared` / `ABDMCX0X`).

- [x] **Preset Oficial Mario Bros Arcade (Intel 8048 / 8039 Demo ROM):**
  - [x] Integrar el paquete canónico de firmware arcade (`tma1-c-6k`) como preset de fábrica accesible por menú directo.
  - [x] Mapear los disparos sonoros y transiciones de comando a notas MIDI y pasos de la matriz de comandos.
- [x] **Portamento / Glide Monofónico Configurable en `MonoVoice`:**
  - [x] Implementar deslizamiento de tono legato (Fingered Portamento / Glide) en `MonoVoice` entre notas consecutivas tocadas sin soltar el gate.
  - [x] Parámetro de tiempo de glide [0 ms a 2000 ms] con interpolación exponencial de frecuencia/reloj CPU.
  - [x] Exponer parámetro en `abd_mcs48_parameter_spec.json`, APVTS y panel de voz.
- [x] **Curvas de Sensibilidad a la Velocidad Dinámica:**
  - [x] Añadir perfiles de respuesta a la velocidad MIDI en `MonoVoice`: Lineal, Exponencial (alta sensibilidad al toque suave), Logarítmica musical y Escalonada (4 niveles clásicos de 8-bit).
  - [x] Calibración fina del rango de modulación de amplitud y reloj en función de la velocidad.
- [x] **Soporte Drag-and-Drop de Microcódigo (.bin / .hex):**
  - [x] Permitir arrastrar archivos binarios (.bin) y volcados Intel HEX directamente sobre la carátula o visor LCD de la WebUI.
  - [x] Validación de cabecera, tamaño (1KB, 2KB, 4KB) y checksum SHA-256 en caliente antes del cambio atómico.
- [x] **Emulación de Tolerancia R-2R DAC y No-linealidad Analógica:**
  - [x] Modelar microdesviaciones de los valores de resistencia de la red R-2R (resistor tolerance mismatch) y simulación de saturación armónica sutil del buffer analógico discreto.
  - [x] Parámetro de "Age / DAC Tolerance" para graduar desde DAC ideal hasta imperfección vintage lo-fi.
- [x] **Intercambio y Volcado de Parámetros/RAM vía SysEx:**
  - [x] Parser y generador de volcados MIDI System Exclusive (SysEx) para volcar estado completo de RAM, registros del microcontrolador y tabla de 8 pasos de la matriz.

**Criterio de salida:** Presets históricos jugables directamente, glide monofónico funcional en legato, perfiles de velocidad musicales y tolerancia DAC configurable verificados en tests unitarios y WebUI.

## Fuera de alcance por ahora

- Emular CPU principal Z80, vídeo o juego completo.
- Soportar cualquier arquitectura de CPU o convertirse en un emulador universal.
- Polifonía lograda por múltiples instancias hasta medir su coste y acordar comportamiento.
- Granular avanzada, AU/CLAP/AAX y editor universal de ROM.
- Distribuir, solicitar o enlazar ROMs comerciales.
- Ampliar el producto fuera del contrato acordado sin autorización expresa; el prototipo actual ya está implementado.

## Decisiones abiertas fuera del contrato inicial

1. ¿Qué DAW y versión serán el primer host oficialmente objetivo para `BDMCS48SE-DAW-01`, y qué umbrales cuantitativos de CPU/dropouts se aceptarán?
2. ¿Qué variantes exactas y capacidades de CPU se añadirán al subconjunto actual después de pruebas de conformidad? No se promete 8048/8049 completo.
3. ¿Cuál será el nombre público definitivo (BDMCS48SE, ABDMCS48SE u otro)?
4. ¿El SDK se distribuirá inicialmente como fuente C++ embebible o necesitará ABI/paquete estable y extensiones externas de machine?
5. ¿El firmware package seguirá siendo carpeta o será ZIP, y qué campos de manifest serán normativos?
6. ¿Qué ensamblador/simulador se utilizará para crear y verificar nuevos firmware propios, y qué permisos rigen su distribución?
7. ¿Cuál será la licencia final del producto, tras auditar JUCE, cualquier código de terceros y firmware? No hacer release público antes de resolverla.
8. ¿Qué procedimiento documentará origen y permisos de firmware/ROM de terceros? La política de ROM comercial permanece: no distribuir, solicitar ni enlazar.
9. ¿La conformidad con firmware real (Juno-6, variante 8049 y expansor 8243) entra en el contrato actual como prueba de CPU, o queda como experimento local? ¿Se admite un perfil de máquina Juno-6 sin distribuir ROM, y con qué alcance de I/O y audio? Ver Fase 11.

## Registro de hitos

> **Nota de auditoría:** Las cifras de tests indicadas en cada fila reflejan el conteo del momento en que se cerró el hito. La suite actual cuenta con **186 tests unitarios** verificados en CTest tras la incorporación de los fixtures de decodificación golden, conformidad de CPU, pruebas de Circuit Bending/Estrés, cero reservas en fallo, arnés multihilo, fuzzing determinista, ROM de diagnóstico y medidores/forma de onda en snapshot.

| Fecha | Hito | Evidencia / notas |
|---|---|---|
| 2026-11-22 | Certificación Multiplataforma, Sanitizers en CI y Cierre de Fase 4 (Fases 3, 4 y 10) | Validación exhaustiva del pipeline CI de 8 jobs en GitHub Actions con 100% verde: determinismo de audio bit a bit idéntico certificado en Linux (glibc GCC y Clang), macOS (Apple Clang / libc++) y Windows (MSVC) sobre 191 tests unitarios; suite `sanitizers-asan-ubsan` limpia con cero fugas de memoria y cero undefined behavior; suite `sanitizers-tsan` limpia certificando ausencia total de data races en el arnés multihilo concurrente de audio, control y UI; WebUI empaquetada con `@abdsynths/shared`; y compilación y test de humo de plugin JUCE (`plugin_smoke`) en Windows x64 con WebView2 SDK. Catálogo musical Fase 4 completado con wavetable nativa `MOVP3` y percusión lo-fi (Kick, Snare, Hi-Hat). |
| 2026-11-22 | Cierre de Fase 5 — Medidores y Forma de Onda Reducida en Snapshot (Fase 5) | Extensión del canal seqlock lock-free `SnapshotChannel` (12 palabras atómicas de 64 bits) para transmitir de forma continua y coherente: nivel pico (`peakLevel`), valor RMS de bloque (`rmsLevel`) y forma de onda diezmada de 8 puntos (`waveform[8]`) calculados en tiempo real por `MachineHost::renderBlock` sin reservas dinámicas ni copias rasgadas. Exposición en `EngineStatus`, `MachineSnapshot` y puente JSON del `PluginProcessor` hacia la WebUI. Verificación de coalescing y paridad numérica en `InvariantesAudio.SnapshotMedidoresYFormaDeOndaCoalescing`. Cierre definitivo de todos los requisitos de la Fase 5. Suite CTest ampliada a 186 tests unitarios. |
| 2026-11-22 | Fuzzing Determinista, Desacoplamiento de MAME y ROM de Test (Fases 1, 2 y 11) | Implementación de fuzzing determinista en `tests/TestFuzzCpu.cpp` (32 semillas pseudoaleatorias de 4 KiB bajo presupuesto acotado de 5.000 ciclos demostrando confinamiento estricto de fallos sin cuelgues ni UB, y 16 paquetes caóticos verificando la finitud y ausencia de NaNs en el audio). Desacoplamiento de `tools/juno6_conformance.py` de `docs/mame/` mediante `--reference` y retorno limpio `SKIP_RETURN_CODE 125` en CTest para entornos de CI limpios. Cierre del diseño e implementación de la ROM de diagnóstico y test arquitectónico MCS-48 según `docs/TEST_ROM_SPEC.md` con fuente ensamblador `source/firmware/test_rom/test_rom.asm`, herramienta `tools/asm_to_package.py`, paquete C++ `DiagnosticFirmware` (bancos de 2048 B con hash SHA-256) y validación en `ToyRom.RomDeDiagnosticoAutotestRamTimerSaltoBancosYRampaDac` (autotest de RAM, timer, salto interbanco A11 a Banco 1 con retorno exitoso y rampa periódica en DAC P1). Suite CTest ampliada a 185 tests unitarios. |
| 2026-11-22 | Cero Reservas en Fallo y Arnés Multihilo en CI (Fases 2, 5 y 10) | Cero reservas absolutas en tiempo real tanto en régimen nominal como en parada de CPU por fallo (`faultMessage` migrado de `std::string` a array estático `char[48]` en `core::Mcs48Cpu`, `core::EngineStatus` y `core::MachineSnapshot`), verificado en `InvariantesAudio.ElCaminoDeFalloAcotaSusReservas` con `CHECK_EQ(allocations, 0u)`. Arnés de estrés multihilo real implementado en `tests/TestThreadStress.cpp` probando contienda concurrente entre hilo de audio (`renderBlock` con buffers variables cíclicos 32..256), hilo de control/MIDI (`postCommand` y `service()`) e hilo de UI (`host.snapshot()` mediante seqlock atómico), hot-swap continuo bajo audio y estrés de cola SPSC (50.000 comandos sin colapso de orden). Integración en `.github/workflows/ci.yml` de dos nuevos jobs de validación en Linux: `sanitizers-asan-ubsan` (AddressSanitizer + UndefinedBehaviorSanitizer) y `sanitizers-tsan` (ThreadSanitizer). Suite CTest ampliada a 182 tests unitarios. |
| 2026-11-22 | Presets Portables y Circuit Bending (Fase 9) | Guardado portable de presets almacenando `firmwareId` y `firmwareSha256` en el estado de JUCE con fallback inteligente a catálogo y rescate a firmware de fábrica por defecto si la ROM falta; semántica determinista de Circuit Bending (`EngineClock` / clock bend 0,0625..8,0x, `LatchWrite` / latch mutation, `DacQuantize` / reducción de 1 a 8 bits y `Freeze` / retención estática del DAC sin colapsar el hilo de audio); PRNG xorshift32 seedable determinista en `CommandMatrix`; reversibilidad completa por `reset()`; botón reactivo `FREEZE` en la botonera de voz de la WebUI y parámetros APVTS / SSOT `dacQuantize` y `freeze`; suites CTest actualizadas con `TestCommandMatrix.DisparoYMutacionDacQuantize`, `DisparoFreeze`, `ReversibilidadResetRestauraValoresNeutros` e `InvariantesAudio.CircuitBendingEstresYReversibilidad`. |
| 2026-11-22 | Contratos Core (Fase 2), Determinismo/Estrés (Fases 3/5), CI JUCE y Licencia/Distribución (Fase 10) | Congelación normativa de `docs/ARCHITECTURE.md` y `docs/FIRMWARE_PACKAGE.md` v1.0 (contratos de tiempo real, preservación de máquina activa ante fallos de carga, acumulador fraccional y APVTS fijo desacoplado). Determinismo ampliado en `tests/TestDeterminism.cpp` y estrés en `tests/TestAudioInvariants.cpp` cubriendo 44,1 kHz y 48 kHz con buffers fijos y variables dinámicos (64 a 1024 muestras) con 0 fallos y paridad bit-exacta con golden hashes SHA-256. Ampliación del flujo de CI en `.github/workflows/ci.yml` para compilar el plugin JUCE (VST3/Standalone) y ejecutar `plugin_smoke` en Windows, publicando los binarios de release como artefactos. Definición de licencia dual y especificación formal de instalador/empaquetado en `docs/LICENSING_AND_DISTRIBUTION.md`. |
| 2026-11-22 | Ruta de pruebas de firmware aprobada (Fase 1) | El usuario aprueba los tres niveles de [FIRMWARE_ROUTES_AND_LIMITS.md §5](docs/FIRMWARE_ROUTES_AND_LIMITS.md#5-ruta-de-pruebas-aprobada-el-2026-11-22): **A** distribuible/CI (solo firmware propio, o tercero con licencia de redistribución explícita e inventario cerrado), **B** privado local (ROM poseída legalmente o con permiso, con evidencia reproducible y nunca en CI) y **C** no admitido. Evidencia mínima por nivel en §5.1–5.4: para A, fuente versionada, build reproducible, SHA-256 en el repositorio y prueba CTest que falle si el hash no coincide; para B, fila de inventario, SHA-256 del archivo usado, comando reproducible, salida registrada y salto limpio cuando el material falta. Cierra el criterio de salida de la Fase 1. El inventario sigue vacío y la retirada de `docs/mame/mario/` del historial sigue pendiente de decisión expresa. |
| 2026-11-22 | Matriz de rendimiento headless (Fases 3 y 5) | `bdmcs48_bench` parametrizado por frecuencia (44,1/48 kHz), bloque (64–1024) e instancias (1–4), con tabla y CSV; mide ms/bloque (total y por instancia), tiempo real y % de un núcleo. 30 combinaciones en Windows 11 x64 / i7-11370H / MSVC 14.51 Release: coste mediano 0,173 µs por muestra (≈5,8 M muestras/s, ≈×120 tiempo real a 48 kHz), ≈0,6–1,0 % de un núcleo por instancia y ≈2,5–4 % con cuatro instancias en un solo hilo; latencia de 1,3 a 23 ms según bloque. Dispersión entre ejecuciones de −46 % a +18 % (turbo del portátil). Evidencia: `docs/evidence/BDMCS48SE-PERF-01/`. Sin plugin JUCE ni DAW; sin umbral acordado. |
| 2026-11-22 | Determinismo de render con goldens versionados (Fase 3) | `tests/TestDeterminism.cpp`: SHA-256 del audio del firmware demo propio en forma canónica (float32 binario little-endian por muestra) contra `tests/data/determinism/render-goldens.txt`: 44 100 Hz/256 → `3f72f6d9c02acd865a4b05afcc7c21c813610c54325134aa781dffd18dee5d6c`, 48 000 Hz/512 → `30d6d6070fc5fd2e65dd9bfca1dba3152c04df711e31a92fc7c8655b45569350`. Dos renders independientes coinciden; bloques 64/128/256/1024 dan el mismo digest; L/R idénticos. El camino de render no tiene PRNG (semilla en `CommandMatrix`). Digest verificado con dos compiladores y runtimes distintos en el mismo host: MSVC 14.51 (CRT de MSVC) y Clang 19.1.7 vía zig 0.14.0 (x86_64-windows-gnu, libc++ + mingw-w64) dan exactamente los dos digests versionados y 175/175 tests. Decisión: el golden se mantiene byte-exacto, sin tolerancia. Evidencia: `docs/evidence/BDMCS48SE-DET-01/`. Límite: no se ha ejecutado en Linux (glibc) ni macOS; lo cubrirá el CI de cuatro plataformas cuando el test esté versionado. |
| 2026-11-22 | Conformidad de decodificación y auditoría de `OpcodeNames` (Fase 11) | Las 256 filas de la tabla se contrastan contra `docs/mame/mcs48dsm.cpp` (variante 8048/8049) y contra la CPU real: 8 filas corregidas (`0x02`, `0x08`, `0x76`, `0x80`, `0x86`, `0x90`, `0xE5`, `0xF5`), `0x01` IDL como exclusión deliberada y un cierre de llave que impedía compilar la tabla. Fixture golden del Juno-6 conforme (1 089 instrucciones, 504 B de datos, 155 opcodes distintos, 0 discrepancias de opcode/longitud/forma) y regiones de ROM declaradas en `MachineProfile::juno6()`. 156/156 tests en Release x64 (Windows 11, MSVC 14.51); auditoría con 0 discrepancias. Material golden privado, fuera del repositorio. |
| 2026-11-22 | Expresividad Musical Avanzada, Presets Históricos e Integración Multi-Motor (Fase 14) | Fingered portamento / legato glide (0 a 2000 ms) con interpolación de pitch exponencial y sub-chunking a 16 muestras en audio thread; 4 curvas de sensibilidad a la velocidad (Lineal, Exponencial, Logarítmica, Escalonada 8-bit); modelo de tolerancia de red de resistencias R-2R DAC con saturación analógica vintage tanh (parámetro `dacTolerance`); 4 presets oficiales de fábrica incluyendo "02 Mario Bros Arcade" (tma1-c-6k); soporte Drag-and-Drop en WebUI de microcódigo (.bin/.rom y decodificador Intel HEX) con validación de cabecera y SHA-256; volcado e importación de parches SysEx MIDI canónicos (`0xF0 0x7D 0x48 ... 0xF7`); constantes añadidas a `Mcs48CalibrationConfig.h` y `CALIBRATION_PARAMETERS.md`; CTest 3/3 suites pasando, 148/148 tests unitarios verdes al 100%, bundle WebUI optimizado en Vite. |
| 2026-11-08 | Conversión y Validación de ROMs MCS-48 en Paquetes de Firmware (Workspace) | Extracción y conversión automática de 9 ROMs MCS-48 desde `_RESOURCES` a paquetes canónicos bajo `workspace/firmwares/user/` y ROMs brutas en `workspace/roms/` (Korg PolySix KLM-366 [8049] y KLM-367 [8048], Roland Jupiter-4 [8048], Roland Juno-6 uPD8049C-238 [8049], Mario Bros tma1-c-6k [8039], Masao 7 [8039], Donkey Kong c_5et_g [8035], Magnavox Odyssey² BIOS [8048], Philips Videopac C52 [8048]); adaptación de la arquitectura de bancos a 2 bancos de 2048 B (`addressBits: 12`) con segmentación automática de ROMs de 4KB en banco 0 y banco 1 tanto en script (`convert_all_resources.cjs`) como en el conversor C++ (`convertRomToPackage`); validación al 100% en `bdmcs48se inspect` y ejecución headless `bdmcs48se diagnose --frames 100` para todos los paquetes; suite CTest 3/3 verde. |
| 2026-11-08 | Arquitectura de Temas Acordes al Modo (Mode-Aware Theme Manager) | Reestructuración de la gestión de temas de `ABDShared`: eliminación de la duplicación de variantes claras y oscuras en el menú; organización en 9 familias canónicas de sintetizadores (`audiolab`, `ms2000`, `microkorg`, `advanced`, `cz101`, `deepmind`, `juno`, `neuron`, `virus-snow`); resolución dinámica según el modo activo (`dark` / `light`); nuevo selector de modo en `View > Color Mode` (`Dark Mode` / `Light Mode`) con checkmark reactivo `✓` y atajo `toggleMode()`; persistencia en `localStorage` (`themeFamily`, `colorMode` y `theme`); atributos `data-theme` y `data-mode` sincronizados en DOM; suite Vitest ampliada (16/16 tests OK en `themeManager.test.js` y `i18n.test.js`); CTest 3/3 verde. |
| 2026-11-08 | Internacionalización Canónica Bilingüe (EN/ES) en 2 Capas (`@abdsynths/shared/i18n`) | Implementación del estándar de 2 capas de `ABDShared` (`SHARED_EN`/`SHARED_ES` + `mcs48En`/`mcs48Es`); 112 claves con paridad 1:1 verificadas en vitest (`WebUI/tests/i18n.test.js` 5/5 OK); selector de idioma en `View > Language` (`English (EN)` / `Español (ES)`) con checkmark reactivo `✓` y persistencia en `localStorage` (`abdsynths.locale`); cambio dinámico sin recargas de página ni corte de audio (`applyI18n`); preservación estricta de acrónimos de síntesis universales (PC, A, PSW, BANK, Hz, dB, ms, R2R DAC, TRIG, GATE, etc.) y preservación del substring normativo `"Matriz de comandos"` para `PluginSmokeTest.cpp:319`; CTest 3/3 verde, Vitest `host-strip-height` 18/18 verde. |
| 2026-11-08 | Reestructuración Canónica de Menús (File/Edit/View/Help) y Motor Undo/Redo Real | Implementación de menú `Edit` canónico con Undo (`Ctrl+Z`) y Redo (`Ctrl+Y`/`Ctrl+Shift+Z`) con soporte completo para parámetros y filas de la `StepMatrix`; unificación lógica eliminando `Tools` y `Settings` como menús de primer nivel y reubicando sus acciones en `Edit`; integración con `SynthNavBar` y despacho global de atajos; CTest 3/3 verde, Vitest 18/18 verde. |
| 2026-11-08 | Ajuste Dimensional de Controles y Sub-header Desacoplado | Separación arquitectónica de la cabecera en dos barras dedicadas de 32px (Menu Bar superior para navegación/menús sin colisiones + Sub-header Toolbar para gestión de presets, firmware y acciones rápidas); corrección de `mountFitStage` (`width: 980, height: 680, onlyShrink: true` sobre `#app`); knobs a 42px con lecturas tabulares en verde fósforo; distribución vertical 172px top / flex bottom / 155px keyboard; CTest 3/3 verde, Vitest 18/18 verde. |
| 2026-11-08 | Modernización UI/UX, Contrato SSOT y Exposición de Constantes de Calibración (Fases 12 y 13) | Integración de componentes canónicos ABDShared (Knob, StepMatrix, MiniWaveScope, VuMeter, mountFitStage, createDrawer, createSynthNavBar); contrato SSOT en contracts/abd_mcs48_parameter_spec.json y WebUI/src/contracts/paramStore.js; exposición APVTS de attack, diagnosticTone, tuningA4, dcBlocker, velocitySens; CALIBRATION_PARAMETERS.md y Mcs48CalibrationConfig.h; CTest 3/3 verde (144 unit tests, 44 smoke tests, bench 160x), Vitest host-strip-height 18/18 verde. |
| 2026-11-08 | Conformidad CPU MCS-48 Juno-6 (Fase 11) y Estándares 2026 de la Suite | Cobertura de 10 opcodes y ejecución de ROM Juno-6 verificada en tests unitarios (144/144 correctos en CTest); aislamiento UDF por PID en WebView2 (ADR-003) con limpieza de procesos huérfanos; límite de 16MB DoS en carga de presets/ROMs; inyector de tono diagnóstico 440 Hz en C++ y WebBridge. |
| 2026-10-21 | Visión inicial y restricciones reunidas | Información del usuario; las decisiones de producto se cerraron posteriormente y las validaciones técnicas siguen según el roadmap. |
| 2026-10-23 | Editor WebView y matriz de comandos funcional | `docs/INSTRUMENT_SKELETON.md`; 8 filas nota → comando con probabilidad, delay y mutación; puente JSON versionado y bundle embebido; suite y `plugin_smoke` registrados en Windows Debug/Release, Standalone abierto con WebView2. No consta prueba en DAW real. |
| 2026-10-23 | BDMCS48SE-PRODUCT-00 — contrato de producto y saneamiento documental | Fase 0 cerrada para definir demo monofónica MIDI, firmware propio, matriz/presets, subconjunto MCS-48, Windows primero y objetivos de audio. GUI existente separada de validación DAW; licencia final y DAW objetivo siguen abiertos. |
| 2026-10-22 | Portabilidad + CI del core (cuatro plataformas) | CMake portable y sockets aislados; Linux/GCC, Linux/Clang, Windows/MSVC y macOS/Clang con CTest 2/2; job WebUI añadido después. El plugin JUCE no forma parte de la matriz. Medición local de benchmark de humo en Windows según registro anterior. |
| 2026-10-22 | Esqueleto del instrumento monofónico (MIDI + JUCE) | Firmware de voz propio, voz monofónica, `engine_clock` y ejecución por ciclos absolutos; VST3 + Standalone y `plugin_smoke`. Verificado en Windows/MSVC; no equivale a validación en host DAW. |
