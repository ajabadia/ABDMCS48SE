# BDMCS48SE — Arquitectura del proyecto

> **Estado:** Contrato arquitectónico formal del Core congelado (Fase 2). Define las garantías canónicas de ejecución, fronteras de aislamiento, modelo de audio en tiempo real y ciclo de vida de máquinas MCS-48.
> Contrastar con el [roadmap](../ROADMAP.md), [FIRMWARE_PACKAGE.md](FIRMWARE_PACKAGE.md) y [AUDIO_THREAD_POLICY.md](AUDIO_THREAD_POLICY.md).

## Propósito

BDMCS48SE es un **SDK y core headless especializado en la familia Intel MCS-48** (8048, 8049, 8039, 8035 y expansores 8243) para construir instrumentos musicales y motores de síntesis o sonido arcade basados en microcódigo.

La composición arquitectónica se rige por la fórmula:

```text
Firmware/ROM + Machine Profile + Core MCS-48 + Host + Frontend específico = Instrumento
```

El Core opera de manera completamente desacoplada: puede instanciarse, configurarse, alimentarse de comandos y renderizar bloques de audio de forma determinista y reproducible desde herramientas de línea de comandos (CLI), suites de tests automatizados (CTest) o plugins de audio (VST3 / Standalone vía JUCE).

## Capas y dependencias

```text
┌──────────────────────────────────────────────┐
│ Frontend / UI                                │
│ WebView u otra UI; controles y visualización │
└──────────────────────┬───────────────────────┘
                       │ funciones nativas/eventos tipados
┌──────────────────────▼───────────────────────┐
│ Host (JUCE para el plugin / CLI para test)   │
│ MIDI, VST3/APVTS, audio, presets, archivos    │
│ Editor/bridge: valida requests y publica estado│
└──────────────────────┬───────────────────────┘
                       │ atomics, colas SPSC y snapshots
┌──────────────────────▼───────────────────────┐
│ SDK / Core headless                           │
│ API C++20 estándar, CPU MCS-48, memoria, I/O, │
│ temporización acumulativa, perfiles y audio   │
└──────────────────────┬───────────────────────┘
                       │ interfaces de máquina (MachineInstance)
┌──────────────────────▼───────────────────────┐
│ Machine Profiles / adaptadores                │
│ Variante CPU, mapa de memoria, latches, I/O,  │
│ DAC R-2R con saturación y circuito analógico │
└──────────────────────────────────────────────┘
```

| Capa | Puede depender de | No debe depender de |
|---|---|---|
| **SDK / Core público** | C++20 estándar (`source/core/`) | JUCE, WebView, MIDI, VST3, MAME interno, filesystem |
| **Core Impl** | SDK y componentes internos propios | JUCE, WebView, MIDI, JSON, DAW |
| **Machine Profile/Adaptador** | SDK y core (`source/machines/`) | GUI, host, lógica de plugins de audio |
| **Firmware Package** | SDK, core, parser JSON propio, SHA-256 | JUCE, WebView |
| **CLI / Tests** | SDK, core, firmware y máquinas | WebView; JUCE salvo test de integración aislado (`PluginSmokeTest`) |
| **Host JUCE / Processor** | SDK/core, JUCE APVTS, colas atómicas | Estructuras privadas de CPU o registros internos directos |
| **Editor / Bridge** | Contrato público del host, JUCE WebView | Estructuras internas de CPU/memoria o llamadas directas al core |
| **Frontend WebUI** | Protocolo versionado (`kUiProtocolVersion`) | Punteros nativos, objetos JUCE/C++, implementación de bajo nivel |

El core nunca conoce JUCE, MIDI, VST3, JSON, JavaScript ni WebView; la interfaz web nunca invoca directamente al core.

## Firmware Package y ciclo de vida formal

El firmware no se suministra como una ruta de archivo al audio thread, sino como un paquete canónico validado (`manifest.json` + binarios en `roms/`).

### Invariantes del ciclo de vida:
1. **Validación fuera del hilo de audio:** La lectura de disco, cálculo SHA-256 y verificación de estructura se realizan en el hilo de control o GUI.
2. **Reemplazo atómico (Atomic Swap):** El candidato se construye y se resetea por completo fuera de audio. Una vez listo, se sustituye la referencia activa en la frontera de bloque.
3. **Preservación ante fallos:** Si un paquete es inexistente, corrupto o de perfil desconocido, la carga se aborta devolviendo un mensaje de error legible y **la máquina activa anterior se preserva intacta**, garantizando la continuidad del audio sin chasquidos ni caídas.

## Machine Profile y mapa de memoria

Un `MachineProfile` formaliza el hardware en el que corre el microcódigo:
- **Variante de CPU:** 8048 (ROM interna 1 KB), 8049 (ROM interna 2 KB), 8039/8035 (fetch externo).
- **Reloj base:** Frecuencia nominal de oscilador (ej. 6.0 MHz para Juno-6 / arcade, 11 MHz para toy machines).
- **Mapa formal de regiones de memoria:**
  - Código ejecutable (`RegionType::Code`).
  - Tablas de datos constantes y calibración (`RegionType::Data`).
  - Vectores de interrupción y reinicio (`RegionType::Vector`: Reset `0x0000`, Ext Int `0x0003`, Timer `0x0007`).
  - Relleno de alineación (`RegionType::Padding`).
- **Banco de memoria:** Hasta 2 bancos de 2 KiB (12 bits de direccionamiento con `SEL MB0`/`SEL MB1`).
- **Circuito DAC:** Red R-2R con tabla LUT de 256 entradas, calibrada para modelar tolerancia física de resistencias (DNL/INL) y saturación analógica suave `tanh`.

## API pública C++: Contrato formal

El contrato del SDK reside en `source/`:

- `MachineHost` (`source/host/MachineHost.h`):
  Orquestador de alto nivel para hosts y DAWs. Expone `renderBlock(frameCount, channelOutputs, numChannels)`, `service()` para procesar eventos del bus fuera de audio, `loadPackage()` para carga segura y `postCommand()` para inyectar comandos discretos o notas MIDI.
- `MachineInstance` (`source/core/MachineInstance.h`):
  Interfaz abstracta que toda máquina emulada implementa: `reset()`, `runBlock(frameCount, outputs, numChannels)` y lectura de estado.
- `GenericMcs48Machine` (`source/machines/GenericMcs48Machine.h`):
  Implementación canónica de máquina MCS-48. Conecta CPU, memoria de programa, timer, bus de puertos P1/P2/BUS y la etapa DAC.
- `CoreClock` (`source/core/CoreClock.h`):
  Contrato de tiempo basado en acumulador fraccional de ciclos absolutos. Garantiza cero deriva temporal y sample rate conversion exacto para 44.1 kHz, 48 kHz y superiores.
- `CommandQueue` (`source/core/CommandQueue.h`):
  Cola SPSC de capacidad acotada para comunicación lock-free entre el host y el hilo de audio.
- `Diagnostics` (`source/core/Diagnostics.h`):
  - `SnapshotChannel`: lectura coherente de estado y registros vía seqlock con memoria fija (40 chars para mensajes, cero asignaciones de memoria).
  - `LogBus`: bus circular de eventos con contador de descartes por desbordamiento.

## Modelo de audio y concurrencia en tiempo real

### Garantías estrictas en el callback de audio (`processBlock` / `renderBlock`):
1. **0 Reservas de memoria:** Cero llamadas a `operator new`, `malloc` o redimensionamiento de contenedores en régimen estable (verificado formalmente en `TestAudioInvariants.cpp`).
2. **0 Bloqueos y 0 Mutexes:** Comunicación unidireccional exclusiva mediante operaciones atómicas (`std::atomic`) con semántica de memoria `acquire/release` y colas lock-free.
3. **0 Entrada/Salida (I/O):** Prohibido el acceso al sistema de archivos, sockets o llamadas de logging en el hilo de audio.
4. **Salida acotada y finita:** Si no hay máquina activa o la CPU entra en fallo por instrucción ilegal, el motor conmuta a silencio estricto (0.0f) o retiene la última muestra finita; jamás emite `NaN`, infinito ni ruido de desbordamiento.
5. **Invarianza de tamaño de bloque:** El render produce exactamente el mismo flujo de audio byte a byte tanto con bloques fijos (64, 128, 256, 512, 1024) como con secuencias de bloques variables.

### Parámetros y automatización de la DAW
Para garantizar compatibilidad absoluta con proyectos de DAWs (REAPER, Bitwig, Cubase, Ableton):
- Los parámetros del APVTS son **fijos y universales** (SSOT definido en `contracts/abd_mcs48_parameter_spec.json`).
- Cambiar de preset o microcódigo **no añade ni retira parámetros VST3 en tiempo de ejecución**, preservando las pistas de automatización y las curvas del usuario.

## Protocolo frontend / WebUI

- **Versión del contrato:** `kUiProtocolVersion = 1` en `source/plugin/UiProtocol.h`.
- **Desacoplamiento:** El frontend solo recibe snapshots periódicos a 20 Hz con estado de registros (`PC`, `A`, `PSW`, `BANK`), contadores de ciclo y niveles.
- **Validación bidireccional:** Todo comando enviado desde JavaScript es validado en tipos, rangos y sanidad en C++ (`WebViewBridge.cpp`) antes de enviarse a la cola de audio.
