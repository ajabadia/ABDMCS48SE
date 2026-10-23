# BDMCS48SE — Arquitectura del proyecto

> Documento de dirección y diseño en evolución; no es una implementación ni congela las decisiones abiertas del [roadmap](../ROADMAP.md).

## Propósito

BDMCS48SE evoluciona hacia un **SDK/core headless especializado en MCS-48** para crear instrumentos basados en firmware. Mario Bros. puede ser un perfil de compatibilidad importante, pero el motor no debe quedar acoplado a su ROM, mapa de comandos, MIDI o interfaz.

La composición conceptual es:

```text
Firmware/ROM + Machine Profile + Core MCS-48 + Host + Frontend específico
```

El primer resultado de ingeniería debería poder probarse sin JUCE, DAW ni UI. Un CLI de render y tests deben poder cargar una máquina de pruebas, enviar entradas y producir audio verificable.

## Capas y dependencias

```text
┌──────────────────────────────────────────────┐
│ Frontend / UI                                │
│ WebView u otra UI; controles y visualización │
└──────────────────────┬───────────────────────┘
                       │ funciones nativas/eventos
┌──────────────────────▼───────────────────────┐
│ Host (JUCE para el plugin inicial)            │
│ MIDI, VST3/APVTS, audio, presets, archivos    │
│ Editor/bridge: valida requests y publica estado│
└──────────────────────┬───────────────────────┘
                       │ atomics, colas y snapshots
┌──────────────────────▼───────────────────────┐
│ SDK / Core headless                           │
│ API C++ estándar, CPU MCS-48, memoria, I/O,   │
│ temporización, perfiles y audio                │
└──────────────────────┬───────────────────────┘
                       │ interfaces de máquina
┌──────────────────────▼───────────────────────┐
│ Machine Profiles / adaptadores                │
│ Variante CPU, mapa de memoria, latches, I/O,  │
│ DAC y circuito de una máquina concreta        │
└──────────────────────────────────────────────┘
```

| Capa | Puede depender de | No debe depender de |
|---|---|---|
| SDK público | C++ estándar | JUCE, WebView, MIDI, VST3, MAME interno, filesystem |
| Core | SDK y componentes internos propios | JUCE, WebView, MIDI, JSON, DAW |
| Machine profile/adaptador | SDK y core | GUI/host; lógica específica de una máquina fuera de su adaptador |
| CLI/tests | SDK, core y perfiles | WebView; JUCE salvo test de integración aislado |
| Host JUCE / processor | SDK/core, JUCE | tipos privados de perfiles que no necesite |
| Editor/bridge | contrato público del host, JUCE WebView | estructuras internas de CPU/memoria |
| Frontend JS | protocolo versionado | punteros, objetos JUCE/C++, implementación del core |

La frontera importante es por responsabilidad y dependencia, no por el nombre de las carpetas. El core nunca conoce JUCE, MIDI, VST3, JSON, JavaScript ni WebView; la WebView nunca llama al core.

## Firmware Package y ciclo de vida

El firmware se considera un paquete validado (manifest + imagen(es) + perfil + metadatos), no una ruta suelta que el core deba interpretar. El loader valida fuera del audio thread; la fábrica construye y mapea una máquina candidata, la inicializa y solo después se puede activar. Un error al cargar un candidato no tiene por qué interrumpir la máquina que ya está activa.

El formato, los estados de carga, las capacidades de bancos y el detalle de hot swap siguen provisionales. El documento [Firmware Package](FIRMWARE_PACKAGE.md) recoge la propuesta, reglas de validación, estados, límites VST3 y fuentes técnicas. Es importante no asumir que `std::atomic<std::shared_ptr<T>>` es automáticamente lock-free o apropiado para destrucción del objeto desde el audio thread.

## Machine Profile

Un perfil define cómo una máquina concreta usa el core MCS-48. En términos conceptuales puede declarar o conectar:

- ID y versión del perfil.
- Variante de CPU, reloj y requisitos de memoria.
- Imágenes necesarias: identidad, tamaño, región/offset/banco si lo soporta la variante y hash esperado cuando exista una referencia legítima.
- Mapa de memoria, puertos, latches y señales externas.
- Procedencia, versión y capacidades del firmware/CPU; validar semánticamente además del esquema del manifest.
- Timers/interrupciones/periféricos requeridos.
- Ruta de salida de audio: DAC, stream, canales y tratamiento del circuito.
- Entradas externas con nombre/anchura para que un host las traduzca.
- Metadatos y procedencia/licencia de perfil y firmware.

El límite v1 es deliberadamente estrecho: MCS-48 y recursos necesarios para las primeras máquinas seleccionadas. No se pretende reemplazar MAME ni describir cualquier chip o computadora. Queda por decidir qué detalles serán datos declarativos y cuáles necesitarán adaptador C++ compilado.

Una ROM no equivale a una máquina completa: el firmware depende de mapa de E/S, reloj, variante CPU y hardware de salida correctos. Un perfil no debe fingir compatibilidad solo porque el tamaño de ROM encaja.

## API pública: conceptos, no contrato congelado

La API pública debe expresar operaciones neutrales que puedan consumir CLI, tests o JUCE. Los candidatos conceptuales incluyen:

- `MachineHost`: crea instancias desde un ID/configuración validada.
- `MachineInstance`: reset, configuración/carga de imágenes, envío de entradas/comandos, ejecución/render por muestras y lectura de diagnóstico.
- `MachineProfile`/`MachineConfig`: identidad, requisitos e interfaz de la máquina.
- `RomImage`: bytes/metadatos ya cargados por el host; el core no abre rutas.
- `SoundCommand` o `MachineInput`: evento discreto dirigido a una entrada tipada, con valor y tiempo/offset.
- `EngineStatus`/snapshot: estado estable y datos de diagnóstico sin exponer punteros internos.

Las firmas incluidas en notas de conversación se tratan como bocetos. Antes de implementar habrá que resolver semántica de tiempo, propiedad de memoria, errores, parámetros, entrada externa y compatibilidad ABI. Evitar APIs basadas en strings libres en el tramo de audio si un ID enumerado/tipado puede servir.

## Modelo de audio y concurrencia

### Flujo de control y audio

```text
WebView / MIDI / DAW
        │
        ▼
JUCE Editor / AudioProcessor
        ├── Parámetros continuos ── atomic/snapshot ─┐
        ├── Eventos discretos ─── cola acotada ─────┤
        └── carga/selección de archivos en message thread
                                                   ▼
                                      AudioProcessor / audio thread
                                                   │
                                                   ▼
                                         Core + Machine Instance
                                                   │
                                                   ▼
                                       audio → DAW y status snapshot
                                                   │
                                                   ▼
                                       JUCE UI timer → eventos WebView
```

- **Parámetros continuos:** valores persistentes/automatizables (por ejemplo, clock o ganancia) fluyen desde DAW/host al motor por el mecanismo seguro acordado; el editor refleja el valor confirmado.
- **Comandos discretos:** notes, latch writes, reset y triggers llevan timestamp/offset si el contrato temporal lo necesita. Se transportan en cola de capacidad acotada, con política explícita al llenarse.
- **Estado:** el core/processor publica un snapshot seguro. El editor lo consulta a frecuencia moderada y emite eventos a la página ya cargada y visible.
- **Visualización:** forma de onda/espectro se reduce/precalcula en host/core; nunca se envían muestras de audio crudas una por una a JavaScript.
- **Archivos:** el diálogo y lectura/hash de ROM se realizan fuera del audio thread. La UI no proporciona rutas arbitrarias como autoridad de acceso; el host elige y valida el archivo.

El hilo de audio no realiza operaciones de filesystem, bloqueos, logging, allocations no acotadas ni llamadas a WebView/GUI. La estrategia concreta de cola, snapshot, buffers y desbordamiento debe probarse y documentarse antes de declararse lock-free o thread-safe.

### Contrato temporal (implementado)

`MachineInstance::runBlock` avanza la máquina hasta un límite **absoluto** de
ciclos de CPU (`cycleOffset_ + CoreClock::totalCycles()`), no "una
instrucción por muestra". `Mcs48Cpu::runCycles` redondea hacia arriba hasta
acabar la instrucción en curso; comparar contra el contador del CPU arrastra
esa diferencia a la muestra siguiente y mantiene la media exacta. Sin esto la
CPU corría hasta un 10 % más rápido que su reloj y la afinación dependía de
la escala de reloj elegida.

Los parámetros y las entradas se consumen de la cola al principio de cada
llamada a `renderBlock`; el host que necesita precisión sub-bloque parte el
bloque en los offsets de sus eventos (lo hace el processor JUCE con el MIDI).
El resultado es que un evento a mitad de bloque se aplica en el límite de
segmento, no al principio del bloque entero.

### Contrato JUCE WebView

- Acciones frontend → C++: funciones nativas concretas registradas con `withNativeFunction`, invocadas mediante la API JUCE de frontend; C++ valida tipos/rangos/estado y despacha por el canal de host adecuado.
- Notificaciones C++ → frontend: eventos del host, por ejemplo `emitEventIfBrowserIsVisible`, emitidos desde el contexto de UI después de que la página esté lista.
- `ResourceProvider`: servir recursos web y datos binarios, no un bus de mensajes de propósito general. La página que sirve es el bundle de Vite (`WebUI/dist`, sin hash en los nombres) embebido con `juce_add_binary_data`; el mapeo vive en `source/webui/WebUIResources.*`, resuelve por ruta exacta o sufijo, declara el MIME por extensión y devuelve 404 (no `index.html`) cuando la ruta no existe. En desarrollo cae a `WebUI/dist` en disco para no recompilar por un cambio de UI.
- El bridge no filtra `juce::var`, `juce::File`, `MidiBuffer` ni `AudioBuffer` al SDK.
- JSON puede ser formato/protocolo de configuración o UI, pero no representación interna obligatoria del core.
- Cualquier ejemplo de callback o API del documento es ilustrativo hasta validarse contra la versión JUCE fijada.

### Protocolo frontend

El protocolo de UI es tipado y versionado. Debe separar:

| Canal | Ejemplos | Mecanismo conceptual | Frecuencia |
|---|---|---|---|
| Parámetros | clock, gain, filter | API de parámetros host/automatización | al cambio/bloque de audio |
| Comandos | latch write, reset, trigger | llamada nativa → cola acotada | por evento |
| Estado | ROM status, CPU, errors, underruns | snapshot → evento | baja/moderada |
| Visualización | nivel, forma de onda reducida | buffer reducido → evento | moderada, configurable |
| Archivo/preset | seleccionar ROM, guardar/restaurar estado | host/message thread | bajo demanda |

Nunca enviar audio muestra a muestra, punteros internos del core ni archivos de ROM al frontend.

### Versión del contrato (implementada)

La versión vive en `source/plugin/UiProtocol.h` (`kUiProtocolVersion`) y viaja
en el propio documento:

```json
{
  "protocol": 1,
  "gain": -6.0,
  "release": 80.0,
  "rows": [ { "note": 60, "target": "engine_clock", "value": 1.0,
              "probability": 1.0, "delayMs": 0,
              "mutation": "none", "mutationAmount": 0.0 } ],
  "firmware": { "label": "…", "id": "…", "hash": "…" },
  "engine": { "pc": 0, "a": 0, "psw": 0, "bank": 0, "cycles": 0 },
  "presets": [ "Init" ]
}
```

- `stateJson()` (respuesta a `getState`) y `presetsJson()` (respuesta a
  `listPresets`) declaran `protocol`.
- La página compara el valor con su propia constante al recibir el primer
  estado; si no coincide muestra el error y no pinta controles. Así una
  página vieja contra un backend nuevo falla de forma visible, no silenciosa.
- Subir la versión solo ante un cambio incompatible (campos renombrados o
  eliminados, significado distinto). Añadir campos opcionales no obliga;
  ambos lados ignoran lo que no conocen.

## ROMs, firmware y licencias

- No distribuir, pedir ni enlazar ROMs comerciales en el proyecto.
- Para desarrollar sin ROM de Mario, priorizar firmware propio de prueba y archivos cuyo derecho de uso/distribución esté comprobado.
- ROMs libres o de uso no comercial no son automáticamente redistribuibles en cualquier contexto: comprobar términos específicos y atribuciones.
- La posibilidad de leer un chip de una placa propia depende del chip; un chip de máscara ROM puede no ser legible con un programador EPROM convencional.
- Tratar MAME como dependencia que requiere auditoría fichero por fichero; guardar procedencia, licencia, hash y obligaciones para cada archivo reutilizado.
- No elegir una licencia final del proyecto hasta que la auditoría de código y firmware esté hecha.
- Los hashes de ROM solo se guardan cuando el origen y uso de ese metadato son aceptables; el hash no prueba por sí mismo procedencia legal.

## Reutilización y extensiones

El SDK debe permitir construir:

1. Core headless + CLI para render/test.
2. Host JUCE genérico para plugin VST3/Standalone.
3. Machine profiles intercambiables, con UI personalizada por producto.
4. Eventualmente, varias instancias para polifonía, tras medición del presupuesto de CPU y una decisión explícita de sincronización/mezcla.

No garantizar desde v1 un formato estable de plugins dinámicos de terceros ni compatibilidad universal de profiles. Primero demostrar que dos máquinas distintas pueden usar el contrato común sin especializar el core por nombre de producto.

## Interfaces visuales previstas

La GUI del primer host tendrá estos grupos principales:

- **Header:** estado de ROM, CPU, errores y master gain.
- **Command Matrix:** MIDI → comando, probabilidad, delay y mutación.
- **Circuit Bend:** clock, latch, bus y address.
- **Output:** profundidad de bits, filtro, medidor y forma de onda.
- **Diagnostics:** ciclos por muestra, underruns y estado del core.

El secuenciador es una función secundaria; no sustituye Diagnostics en la vista principal. El frontend se puede reemplazar sin alterar core ni SDK.

## Calidad y testabilidad

La arquitectura debe permitir verificar sin plugin:

- Instrucciones/variantes CPU contra pruebas de referencia.
- Reset, temporización, memoria y puertos.
- Entrada discreta con timestamp y orden determinista.
- Render de cantidad exacta de muestras y condiciones de underrun.
- Mismo input/config/seed produce mismo output.
- Parámetros de bending neutros preservan baseline.
- ROM/perfil ausente, corrupto, tamaño inválido y hash incorrecto se reportan sin crash.
- Tasa/bloque variables, desbordamiento de cola, reset durante ejecución y actualización de estado.

Los criterios concretos y métricas pendientes están en [ROADMAP.md](../ROADMAP.md).

## Preguntas abiertas

- Variante(s) de MCS-48 soportadas inicialmente y pruebas de conformidad disponibles.
- Semántica exacta de `render`/tiempo, sample-rate conversion y offsets MIDI.
- Declaración del perfil: JSON/TOML limitado, estructura C++ compilada, o ambos.
- Requisitos mínimos y formato del paquete de firmware; tamaño, segmentos, hash y procedencia.
- API fuente estable frente a ABI binaria estable y distribución del SDK.
- Polifonía y compartición de ROM/buffers.
- Estrategia segura de activación y reclamación de máquina en hot swap, fuera del audio thread.
- Política para mantener IDs/significados VST3 estables cuando cambia el firmware.
- Protocolo probado para publicar snapshots de diagnóstico/registros sin data races.
- Política de compatibilidad y licensing de MAME, ensambladores y firmware de ejemplo.
