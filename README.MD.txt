# BDMCS48SE

> Archivo de trabajo y especificación técnica. Para la portada pública de GitHub, consulta [`README.md`](README.md).

**BDMCS48SE** (*Bus/Data MCS-48 Sound Engine*) es un instrumento VST3/Standalone experimental basado en la emulación del subsistema de sonido del arcade **Nintendo Mario Bros. (1983)**.

No es un soundboard ni un reproductor de samples: es un **sintetizador de comandos, ROM y circuito**. MIDI, automatización del DAW, una matriz de comandos y circuit bending virtual controlan el firmware MCS-48/M58715, sus latches, su bus de datos y el circuito de salida modelado por MAME.

> Estado actual: **diseño y desarrollo inicial**.  
> La primera versión utilizará componentes de MAME como motor de emulación; una fase final prevista contempla migrar a un motor propio sin dependencia de MAME.

---

## ✨ Concepto

BDMCS48SE emula únicamente el **subsistema de sonido** de Mario Bros., no el juego completo:

```text
MIDI / GUI / DAW
       │
       ▼
JUCE AudioProcessor
       │
       ▼
BDMCS48SE Core
       │
       ├── Latches de comandos
       ├── ROM de sonido
       ├── CPU MCS-48 / M58715
       ├── Timers e I/O necesarios
       ├── DAC / circuito de salida
       ▼
Buffer circular de audio
       │
       ▼
JUCE processBlock → DAW
```

La CPU principal Z80 del arcade no se emula. En su lugar, el plugin la sustituye por MIDI, automatización y una matriz de comandos: cada nota, controlador o paso de secuenciador puede escribir un comando en el subsistema de sonido.

---

## 🎛️ Qué puedes hacer

BDMCS48SE está pensado como un híbrido entre:

- **Módulo de sonido retro:** disparar efectos y rutinas originales del firmware.
- **Instrumento MIDI:** asignar comandos a notas, controladores y zonas del teclado.
- **Caja de ritmos experimental:** secuenciar comandos cortos como percusión.
- **Sintetizador glitch basado en ROM:** alterar datos, direcciones y rutinas del firmware.
- **Generador de drones y texturas:** congelar estados, mutar el bus y manipular el reloj.
- **Motor granular futuro:** procesar la salida del core con capas posteriores de granulación y efectos.

No es un sintetizador polifónico convencional. Su interés está en generar **eventos sonoros no lineales**: rutinas completas, estados internos, corrupción de memoria y texturas difíciles de obtener con osciladores tradicionales.

---

## 🧩 Funcionalidades previstas

### Motor

- Emulación del subsistema de sonido de Mario Bros.
- Carga externa de ROMs (*Bring Your Own ROM*).
- Ejecución temporal del firmware MCS-48/M58715.
- Comunicación mediante latches de comandos.
- Salida mono hacia el DAW.
- Core headless, independiente de JUCE y de la GUI.

### Musicalidad

- Entrada MIDI.
- Matriz de comandos configurable.
- Mapeo de notas MIDI a comandos.
- Velocity, probabilidad, retardo y repetición.
- Modos *Single Shot*, *Retrigger*, *Latch Hold*, *Sequence Step* y *Random Command*.
- Secuenciador interno de comandos.
- Parámetros VST3 automatizables.

### Circuit bending virtual

- Clock bend.
- Mutación de latches.
- Corrupción de datos.
- Mutación de direcciones.
- DAC bit crush.
- Freeze / hold.
- Reset injection.
- PRNG determinista con semilla reproducible.

### Interfaz

- GUI moderna basada en WebView.
- Comunicación bidireccional C++ ↔ JavaScript.
- Matriz de comandos.
- Controles de circuit bending.
- Estado del motor, diagnóstico y visualización.
- Presets de usuario.

---

## 🚧 Roadmap

### Fase 0 — Investigación

- Identificar los ficheros y dispositivos MAME necesarios.
- Documentar CPU de sonido, ROMs, latches, I/O, timers y circuito de salida.
- Definir el conjunto mínimo de componentes reutilizables.
- Crear `docs/MAME_INTEGRATION.md`.

### Fase 1 — Core headless

- Proyecto CMake independiente de JUCE.
- Interfaz `ISoundEngine`.
- Implementación `MameSoundEngine`.
- Carga y validación de ROMs.
- Reloj, acumulador de ciclos y ejecución temporal.
- Buffer circular de audio.
- Exportación WAV desde consola.

### Fase 2 — Integración JUCE

- `AudioProcessor` básico.
- Integración del core.
- `processBlock` basado en tiempo, no en “una instrucción por muestra”.
- Parámetros básicos.
- MIDI básico.
- Builds Standalone y VST3.

### Fase 3 — Matriz de comandos

- Traducción MIDI → comandos.
- Matriz nota → comando.
- Probabilidad, delay, repetición y hold.
- Secuenciador interno.
- Documentación de comandos descubiertos.

### Fase 4 — Circuit bending

- Latch mutation.
- DAC bit crush.
- Clock bend.
- Bus mutation.
- Address mutation.
- Freeze / hold.
- Reset injection.
- Presets de bending.

### Fase 5 — WebView

- Integración con `juce::WebBrowserComponent`.
- Contrato JSON C++ ↔ JavaScript.
- Funciones nativas y eventos.
- Panel de comandos y circuit bending.
- Visualización de estado.

### Fase 6 — Estabilidad

- Tests automatizados.
- Presets de fábrica.
- Builds multiplataforma.
- Documentación de usuario.
- Validación en DAWs.

### Fase final — Migración fuera de MAME

El objetivo a largo plazo es sustituir progresivamente MAME por un motor propio:

```text
v1: BDMCS48SE Core → MameSoundEngine
v2: BDMCS48SE Core → NativeMCS48Engine
v3: BDMCS48SE Core → NativeSoundBoard + NativeNetlist
```

La migración se hará por capas:

1. Sound board propio: ROM, latches e I/O.
2. CPU MCS-48 propia.
3. Timers e interrupciones necesarias.
4. DAC propio.
5. Modelo propio del circuito analógico / netlist.
6. Comparación A/B contra la versión basada en MAME.

---

## 🏗️ Estructura prevista

```text
/bdmcs48se/
├── CMakeLists.txt
├── README.md
├── LICENSES.md
├── /docs/
│   ├── ARCHITECTURE.md
│   ├── MAME_INTEGRATION.md
│   ├── ROM_FORMAT.md
│   ├── PARAMETERS.md
│   ├── CIRCUIT_BENDING.md
│   └── MAME_TO_NATIVE_MIGRATION.md
├── /source/
│   ├── /core/
│   │   ├── BDMCS48SECore.h/.cpp
│   │   ├── ISoundEngine.h
│   │   ├── MameSoundEngine.h/.cpp
│   │   ├── CommandQueue.h
│   │   ├── SampleRingBuffer.h
│   │   ├── RomLoader.h/.cpp
│   │   └── CoreTypes.h
│   ├── /plugin/
│   │   ├── PluginProcessor.h/.cpp
│   │   ├── PluginEditor.h/.cpp
│   │   ├── Parameters.h/.cpp
│   │   ├── MidiCommandMapper.h/.cpp
│   │   └── WebViewBridge.h
│   └── /tests/
├── /webview/
│   ├── index.html
│   ├── app.js
│   ├── style.css
│   └── /assets/
└── /roms/
    └── README.md
```

La carpeta `/roms/` solo contendrá documentación y validación. **Este repositorio no distribuye ROMs.**

---

## ⚙️ Principios técnicos

- El core debe poder compilarse y probarse sin JUCE.
- El core no conoce MIDI, VST3, WebView ni el sistema de ficheros.
- El hilo de audio no realiza I/O, allocations no acotadas ni llamadas a la GUI.
- La emulación se ejecuta por tiempo, con acumulador fraccional de ciclos.
- La salida se genera en un buffer circular monofónico.
- Todos los efectos de circuit bending deben ser reversibles, automatizables y reproducibles.
- La misma ROM, semilla y secuencia de comandos deben producir la misma salida.

---

## 🎚️ Parámetros previstos

### Motor

| Parámetro | Tipo | Descripción |
|---|---|---|
| `master_gain` | Float | Ganancia final |
| `engine_clock` | Float | Multiplicador del reloj base |
| `output_filter` | Float | Filtro / post-procesado |
| `mono_width` | Float | Imagen estéreo artificial |

### Circuit bending

| Parámetro | Tipo | Descripción |
|---|---|---|
| `latch_xor_mask` | Integer | XOR aplicado al valor del latch |
| `latch_and_mask` | Integer | AND aplicado al valor del latch |
| `bus_xor_mask` | Integer | XOR sobre datos leídos |
| `bus_mutation_amount` | Float | Probabilidad de corrupción |
| `address_offset` | Integer | Desplazamiento de lectura |
| `address_xor_mask` | Integer | XOR sobre direcciones |
| `dac_bit_depth` | Integer | Cuantización de salida |
| `dac_bit_crush` | Float | Corrupción del DAC |
| `freeze_address` | Bool | Congelar dirección |
| `freeze_value` | Bool | Congelar valor |
| `reset_injection` | Button | Reinicio del motor |
| `random_seed` | Integer | Semilla determinista |

---

## 🌐 Interfaz WebView

La GUI utilizará `juce::WebBrowserComponent`. Las acciones de la WebView se expondrán como funciones nativas registradas desde C++ mediante `withNativeFunction` y se invocarán desde JavaScript con `getNativeFunction`. Para comunicar cambios de estado desde C++ al frontend se utilizará `emitEventIfBrowserIsVisible`. `ResourceProvider` servirá principalmente para recursos y datos binarios, no como mecanismo general de mensajería.

### Contrato inicial

**JavaScript → funciones nativas C++**

```js
const setParameter = window.__JUCE__.backend.getNativeFunction("setParameter");
await setParameter("engine_clock", 1.25);

const sendCommand = window.__JUCE__.backend.getNativeFunction("sendCommand");
await sendCommand(0, 0x12, 1.0);

const loadRoms = window.__JUCE__.backend.getNativeFunction("loadRoms");
await loadRoms(); // C++ abre el selector; la interfaz no envía rutas arbitrarias.
```

Las funciones nativas previstas son `setParameter(id, value)`, `sendCommand(latch, value, probability)`, `loadRoms()` y `resetEngine()`. Los callbacks validan la solicitud y encolan el trabajo; la carga de ficheros no se ejecuta en el hilo de audio.

**C++ → JavaScript**

```js
window.__JUCE__.backend.addEventListener("bd:engineState", (state) => {
  updateLeds(state);
});

window.__JUCE__.backend.addEventListener("bd:diagnostics", (diagnostics) => {
  updateDiagnostics(diagnostics);
});
```

### Funciones y eventos previstos

| Nombre | Dirección | Mecanismo | Uso |
|---|---|---|---|
| `setParameter(id, value)` | JS → C++ | Función nativa | Solicitar cambio de parámetro |
| `sendCommand(latch, value, probability)` | JS → C++ | Función nativa | Enviar comando |
| `loadRoms()` | JS → C++ | Función nativa | Abrir selector y validar ROMs |
| `resetEngine()` | JS → C++ | Función nativa | Solicitar reset del motor |
| `bd:engineState` | C++ → JS | Evento | Estado y LEDs |
| `bd:parameterChanged` | C++ → JS | Evento | Reflejar automatización del DAW |
| `bd:romStatus` | C++ → JS | Evento | Validación de ROMs |
| `bd:diagnostics` | C++ → JS | Evento | Ciclos por muestra, underruns y estado del core |

### Distribución visual propuesta

- **Header:** estado de ROM, CPU, errores y master gain.
- **Command Matrix:** tabla MIDI → comando, con probabilidad, delay y mutación.
- **Circuit Bend:** controles de clock, latch, bus y address.
- **Output:** profundidad de bits, filtro, medidor y forma de onda.
- **Diagnostics:** ciclos por muestra, underruns y estado del core.

El secuenciador interno previsto en el producto podrá mostrarse como vista o control secundario; no desplazará Diagnostics de la disposición principal.

---

## 🔧 Compilación

> Las instrucciones definitivas se añadirán cuando exista el sistema de build inicial.

Requisitos previstos:

- C++20 o superior.
- CMake 3.24 o superior.
- JUCE 8.
- Compilador compatible: MSVC, Clang o GCC.
- ROMs del arcade aportadas por el usuario.

Compilación prevista:

```bash
git clone https://github.com/ajabadia/ABDMCS48SE.git
cd ABDMCS48SE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

---

## 💾 ROMs

BDMCS48SE no distribuye ROMs.

**Política de ROMs: no distribuir, no pedir, no enlazar.** No solicites, publiques ni enlaces ROMs en los issues, pull requests, discusiones, documentación o materiales de este repositorio. La aplicación podrá permitir al usuario seleccionar localmente archivos que ya posea y validar su tamaño y hash, sin que el proyecto los proporcione.

---

## ⚖️ Licencias

Este proyecto puede reutilizar componentes de MAME durante su primera fase. MAME, como proyecto global, se distribuye bajo **GPL-2.0-or-later**, aunque gran parte de sus ficheros —incluidos muchos del core— están bajo **BSD-3-Clause**. Cada fichero reutilizado deberá revisarse individualmente. [81]

**Este README no declara todavía una licencia definitiva para BDMCS48SE.** Antes de elegirla, habrá que auditar los ficheros concretos de MAME reutilizados, sus licencias y las obligaciones de distribución aplicables.

Consecuencias prácticas:

- Si BDMCS48SE incluye o enlaza código GPL de MAME, deberá respetar los términos de la GPL.
- La migración final a un motor propio permitirá reconsiderar la licencia del proyecto.
- Las ROMs de Mario Bros. no se distribuyen, solicitan ni enlazan desde este repositorio.

Consulta `LICENSES.md` cuando esté disponible para ver el origen, licencia y hash de cada componente reutilizado.

---

## 🤝 Contribuciones

Las contribuciones son bienvenidas, especialmente en:

- Investigación del firmware MCS-48.
- Análisis de ROM y trazas de MAME.
- Implementación de la CPU MCS-48 propia.
- Modelado del DAC y circuito de salida.
- Tests de regresión y comparación de audio.
- Documentación técnica.

Antes de abrir un pull request:

1. Discute primero la propuesta en un issue.
2. No incluyas ROMs, dumps ni enlaces a ROMs.
3. Mantén el core independiente de JUCE.
4. No introduzcas allocations ni bloqueos en el hilo de audio.
5. Añade tests para cualquier cambio en el core.

---

## 📌 Nombre del proyecto

**BDMCS48SE** = **B**us/**D**ata **MCS-48** **S**ound **E**ngine.

El nombre refleja las tres ideas centrales del instrumento:

- Manipulación del **bus y los datos** del firmware.
- Emulación del microcontrolador **MCS-48/M58715**.
- Conversión del subsistema de sonido en un **motor musical**.