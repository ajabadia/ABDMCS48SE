# Instrumento monofónico — esqueleto técnico

> **Estado:** esqueleto funcional **sin interfaz**, a 2026-10-22. El plugin se
> construye como VST3 + Standalone, recibe MIDI y suena; el editor queda
> pendiente de instrucciones.

## Qué se ha construido

| Pieza | Fichero | Responsabilidad |
|---|---|---|
| Voz monofónica | `source/instrument/MonoVoice.*` | Nota MIDI → escala del reloj, prioridad de última nota, envolvente de puerta. Sin JUCE. |
| Firmware de voz | `source/firmware/demo/MonoVoiceFirmware.*` | ROM original de 16 bytes: onda cuadrada simétrica. Paquete en memoria, sin escribir en disco. |
| Processor | `source/plugin/PluginProcessor.*` | `juce::AudioProcessor`, APVTS (gain/release), MIDI con offsets, DC blocker, carga de paquetes y estado. |
| Prueba headless | `tests/PluginSmokeTest.cpp` | MIDI → processor → audio: afinación de A4 y silencio al soltar. |
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

## Cadena de señal

```text
MIDI (host)
  │  note on/off, offsets de muestra
  ▼
MonoVoice ── nota → escala de reloj ──┐
  │  envolvente ataque/liberación      │
  ▼                                    ▼
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

- Suite headless del core: **119 tests** (CPU, paquetes, host, voz y audio),
  con la afinación de A2/A3/A4/A5 y el silencio tras soltar.
- `plugin_smoke`: construye el processor, envía un `noteOn` y mide 440 Hz en la
  salida; comprueba el canal derecho y el silencio tras `noteOff`.
- Build Release de VST3 + Standalone correcto con JUCE 9.0.4 y MSVC 19.51.

## Pendiente (no incluido a propósito)

- **Interfaz**: `hasEditor()` devuelve `false` y `createEditor()` `nullptr`.
  El processor ya expone `currentNote()`, `currentFrequency()`, `sounding()`,
  `level()`, `firmwareLabel()`, `statusLine()`, `loadPackage()`,
  `resetMachine()` y `parameters()` para engancharla.
- Pitch bend, glide y velocity con curva.
- Puerta dentro del firmware (leer un latch) en vez de solo envolvente.
- Presets que guarden hash/ID de firmware y no solo la ruta.
- Medición de varias instancias y estrés de colas en un DAW real.
