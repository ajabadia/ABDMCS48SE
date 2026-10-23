# Instrumento monofónico — esqueleto técnico

> **Estado:** instrumento con editor **WebView**, a 2026-10-23. El plugin se
> construye como VST3 + Standalone, recibe MIDI, suena y expone la matriz de
> comandos y los parámetros desde una página embebida en el binario.

## Qué se ha construido

| Pieza | Fichero | Responsabilidad |
|---|---|---|
| Voz monofónica | `source/instrument/MonoVoice.*` | Nota MIDI → escala del reloj, prioridad de última nota, envolvente de puerta. Sin JUCE. |
| Firmware de voz | `source/firmware/demo/MonoVoiceFirmware.*` | ROM original de 16 bytes: onda cuadrada simétrica. Paquete en memoria, sin escribir en disco. |
| Processor | `source/plugin/PluginProcessor.*` | `juce::AudioProcessor`, APVTS (gain/release), MIDI con offsets, DC blocker, carga de paquetes y estado. |
| Matriz de comandos | `source/instrument/CommandMatrix.*` | 8 filas nota → comando con probabilidad, delay y mutación. Sin JUCE ni MIDI: recibe notas ya extraídas. |
| Editor WebView | `source/plugin/PluginEditor.*` | `WebBrowserComponent`, `ResourceProvider` sobre el bundle embebido, fallback a disco en desarrollo y despacho de funciones nativas. |
| Puente JSON | `source/plugin/WebViewBridge.*` | Contrato tipado C++ ↔ JS: estado, filas, parámetros, notas y presets. C++ valida y aplica. |
| Página | `WebUI/index.html`, `WebUI/src/` | Teclado de dos octavas, matriz de 8 filas, panel de estado y presets. Vite lo empaqueta en `WebUI/dist`, que se embebe con `juce_add_binary_data`. |
| Mapeo de recursos | `source/webui/WebUIResources.*` | Ruta HTTP → bytes + MIME sobre el bundle; sin JUCE para poder probarlo desde `plugin_smoke`. |
| Prueba headless | `tests/PluginSmokeTest.cpp` | MIDI → processor → audio, matriz (disparo, delay, probabilidad, mutación), recursos embebidos y roundtrip de estado. |
| Build | `CMakeLists.txt` | `ABDMCS48SE_BUILD_PLUGIN` (OFF por defecto), JUCE 9.0.4 por FetchContent, formatos Standalone y VST3. |

Cambios en el core que el instrumento necesitaba:

1. **`engine_clock` con el contrato correcto.** `MachineInstance::setParameter`
   documenta un valor normalizado (0..1), pero el perfil lo trataba como la
   escala final y lo recortaba a 0,25..4,0: una automatización al 100 % nunca
   pasaba de 1,0. Ahora se proyecta 0..1 → 0,0625..8,0
   (`GenericMcs48Machine::kMinClockScale/kMaxClockScale`), que da siete
   octavas de afinación.
2. **Ejecución hasta un límite absoluto de ciclos.** `runCycles()` redondea
   hacia arriba hasta terminar la instrucción en curso. El perfil descartaba
   ese sobrante en cada muestra, así que la CPU corría hasta un 10 % más
   rápido que su reloj y todo el instrumento sonaba alto. Ahora `runBlock`
   compara contra `cycleOffset_ + clock_.totalCycles()` y el sobrante se
   arrastra a la muestra siguiente: la media es exacta y la afinación deja de
   depender del reloj elegido.
3. **Carga en memoria.** `MachineHost::loadPackage(LoadedPackage)` instala un
   paquete ya validado, sin tocar el disco. El plugin usa
   `firmware::demo::monoVoicePackage()` y no escribe ficheros temporales.
4. **El parámetro de liberación no reabre la cola.** El processor escribe
   `release` en cada bloque; `MonoVoice::setReleaseMs` recalculaba la pendiente
   con la ganancia actual, así que la cola se alargaba indefinidamente (a 80 ms
   seguía sonando 0,3 s después de soltar). Ahora ignora valores iguales al
   vigente y solo recalcula si el tiempo cambia de verdad.

## Matriz de comandos

La inteligencia performable vive en el host; el core sigue recibiendo comandos
resueltos. Cada Note On evalúa las filas cuya nota coincide:

```text
Note On
  → probabilidad (se decide al recibir la nota)
  → programación por deadline (delay en ms → muestras)
  → CommandQueue → applyPendingCommands()
  → setParameter(engine_clock) o writeInput(latch)
```

- **Valor:** para `engine_clock` es el multiplicador de reloj (0,0625..8,0);
  para `latch_write`, el byte 0..255. `CommandMatrix::commandFor` lo convierte
  al entero que espera `MachineHost` (valor/32767).
- **Mutación:** `none`, `increment`, `decrement`, `random` (con PRNG xorshift
  sembrado) y `wrap`. Solo se aplica cuando la fila se ejecuta de verdad, no
  cuando falla la probabilidad, y afecta a la **siguiente** ejecución.
- **Tiempo:** el processor parte el bloque en cada deadline; un retardo de
  5 ms cae en su muestra, no al principio del bloque siguiente. Si no hay
  comandos, el bloque se renderiza de una vez.
- **Hilos:** la UI edita una copia en el hilo de mensajes; `publishRows` la
  publica con doble buffer y el hilo de audio la adopta al inicio del bloque.
  Nunca hay locks ni asignaciones en la ruta de audio.
- **Persistencia:** cada fila va al estado del plugin dentro de
  `<CommandMatrix>`; el roundtrip está cubierto por `plugin_smoke`.

## Editor

- La UI se escribe en `WebUI/` (HTML + `src/main.js` + `src/main.css`) y se
  empaqueta con Vite: `node Scripts/build_webui.js` genera `WebUI/dist`
  (`index.html`, `assets/index.js`, `assets/index.css`, sin hash en el nombre).
  Ese dist se embebe con `juce_add_binary_data`: el VST3 y el Standalone
  funcionan sin un directorio `WebUI/` al lado del binario.
- El teclado virtual **no es propio**: se monta el keybed compartido de
  ABDShared, `@abdsynths/midi-keyb`, el mismo que usa ABDMS2000. Vite lo
  resuelve contra el checkout hermano `../ABDShared` (o `ABD_SHARED_DIR`) y
  sus hojas de estilo entran en el bundle. La página solo aporta los
  callbacks `noteOn`/`noteOff`/`allNotesOff`; las notas siguen viniendo del
  backend y el keybed no guarda estado musical.
- Configuración del keybed: 2 octavas fijas desde C3 (`fixedOctaves`), sin
  ruedas de pitch/mod (el motor no las implementa) y con hold/latch y retrig
  apagados, porque sobre la CPU emulada dejarían notas sonando sin pulsación.
  La franja tiene altura definida (155 px): es el contrato del componente para
  que WebView2 no colapse las teclas.
- CMake falla en la configuración si falta `WebUI/dist/index.html`, en vez de
  producir un plugin sin editor. En desarrollo el `ResourceProvider` cae a
  `WebUI/dist` en disco después de mirar el bundle, así que regenerar el dist
  se ve al recargar sin recompilar.
- El provider resuelve rutas exactas o por sufijo, sirve el MIME por extensión
  y devuelve 404 para una ruta desconocida: un asset mal empaquetado se nota,
  no se disfraza con la página principal.
- La página pide el estado con `getState` y recibe actualizaciones por el
  evento `bdmcs48-state` a 20 Hz. No guarda su propia verdad: PC, PSW, ciclos,
  nivel y filas vienen del backend.
- El contrato está versionado: `protocol` viaja en el documento de estado y en
  `listPresets` (`kUiProtocolVersion` en `source/plugin/UiProtocol.h`). La
  página compara la versión al arrancar y muestra un error visible si no
  coincide; nunca pinta controles con un esquema que no entiende.
- Funciones nativas: `getState`, `listPresets`, `noteOn`, `noteOff`,
  `allNotesOff`, `setParameter`, `setRow`, `resetMachine`, `savePreset` y
  `loadPreset`. El teclado virtual entra por `MidiMessageCollector`, el mismo
  camino que el MIDI del DAW.
- Presets del Standalone: `%APPDATA%/ABDMCS48SE/presets/*.bdm48` (o el
  equivalente de cada sistema). Guardan parámetros, matriz, semilla y ruta de
  firmware.

## Cadena de señal

```text
MIDI (DAW / teclado virtual)
  │  note on/off, offsets de muestra
  ├── MonoVoice ── nota → escala de reloj ─┐
  └── CommandMatrix ── filas, delay ───────┤
                                           ▼
processor ── cola de comandos ──► GenericMcs48Machine
                                     │  CPU MCS-48 + firmware
                                     ▼
                              DAC P1 → envolvente → DC blocker → salida estéreo
```

## Afinación

- El firmware alterna el DAC cada 450 ciclos y añade 2 ciclos de `JMP` por
  periodo: **902 ciclos**. A 6 MHz (400 000 ciclos/s) son **443,33 Hz**.
- La nota se traduce a escala de reloj:
  `escala = frecuencia(nota) / 443,33`, con A4 = 440 Hz.
- Rango: escala 0,0625..8,0 → **27,7 Hz .. 3 546 Hz** (A0..A7). Fuera de ese
  rango la voz recorta.
- La escala se envía en el entero de la cola de comandos (÷32767), así que la
  resolución es de 1/32767 del rango: menos de 0,01 cents. La medición
  end-to-end en `TestInstrument` da A4 dentro de ±2 Hz y A2 dentro de ±1 Hz.

## Hilos y propiedad

| Contexto | Qué hace |
|---|---|
| `processBlock` | Renderiza segmentos entre eventos MIDI, aplica envolvente y DC blocker, encola el cambio de afinación. Sin allocaciones, ficheros ni UI. |
| `prepareToPlay` | Construye `MachineHost` y `MonoVoice` con la frecuencia de muestreo real. |
| Hilo de mensajes | `loadPackage()`, `resetMachine()`, lecturas de `statusLine()`/`firmwareLabel()`. La restauración de estado usa `AsyncUpdater`, no el hilo de audio. |

El `MachineHost` se sustituye solo en `prepareToPlay`, con el audio detenido;
el hilo de audio lo dereferencia sin bloqueo. Los cambios de firmware pasan
por `MachineExchange` (hot swap en el límite de bloque).

## Build

```bash
cmake -S . -B build-plugin -DABDMCS48SE_BUILD_PLUGIN=ON
cmake --build build-plugin --config Release --parallel
```

- La primera configuración **descarga JUCE 9.0.4** (FetchContent) y tarda uno o
  dos minutos; después queda en `build-plugin/_deps`.
- En Windows el editor necesita el paquete **Microsoft.Web.WebView2** de NuGet y
  el runtime de WebView2. CMake lo busca con `NEEDS_WEBVIEW2`; si falta, el
  mensaje indica qué instalar y dónde (`JUCE_WEBVIEW2_PACKAGE_LOCATION`).
- El proyecto declara `LANGUAGES C CXX` porque JUCE 9 exige compilador de C.
- Artefactos en `build-plugin/bdmcs48se_instrument_artefacts/Release/`:
  `Standalone/BDMCS48SE Instrument.exe` y `VST3/BDMCS48SE Instrument.vst3`.
- Con `ABDMCS48SE_BUILD_TESTS=ON` (por defecto) se añade el test
  `plugin_smoke` a CTest.

Sin `-DABDMCS48SE_BUILD_PLUGIN=ON` el proyecto sigue compilando sin red y sin
JUCE: el core, el firmware, la voz y los tests no dependen del plugin.

## Licencia de JUCE

JUCE 9 se ofrece bajo **AGPLv3 o licencia comercial**. Compilar y probar en
local no publica nada, pero **distribuir binarios** del plugin bajo AGPL
obliga a publicar el código bajo AGPL (o a comprar la licencia comercial). El
repositorio todavía no tiene `LICENSE`, así que la decisión sigue abierta y no
se ha elegido por ti.

## Verificación

- Suite headless del core: **132 tests** (CPU, paquetes, host, voz, matriz y
  audio), con la afinación de A2/A3/A4/A5 y el silencio tras soltar.
- `plugin_smoke`: MIDI → audio (440 Hz, silencio tras soltar), matriz (disparo,
  probabilidad 0, delay de 100 ms, mutación increment), recursos embebidos,
  contrato del puente y roundtrip de estado/matriz.
- Build Release de VST3 + Standalone correcto con JUCE 9.0.4, MSVC 19.51 y
  WebView2. El editor se ha arrancado en Standalone; falta probarlo dentro de un
  DAW real.

## Pendiente (no incluido a propósito)

- Pitch bend, glide y velocity con curva.
- Puerta dentro del firmware (leer un latch) en vez de solo envolvente.
- Presets que guarden hash/ID de firmware y no solo la ruta.
- Medición de varias instancias y estrés de colas en un DAW real.
