# Política del hilo de audio

Este documento fija el contrato entre el hilo de audio y el resto del
instrumento (UI, hilo de control, WebView) y la forma en que cada invariante
se vigila en CTest. No describe el *render* ni el DSP: eso está en
[ARCHITECTURE.md](ARCHITECTURE.md) y [INSTRUMENT_SKELETON.md](INSTRUMENT_SKELETON.md).

**Alcance:** core, host headless (`MachineHost`) e instrumento; el plugin JUCE
usa exactamente este mismo host y no añade canales de comunicación propios
hacia el audio. Verificado en Windows 11 x64 / MSVC 14.51 Release; no hay
ejecución con sanitizers de hilos en CI (ver [Límites](#8-límites-y-desviaciones)).

## 1. Reparto de responsabilidades

| Hilo | Puede | No puede |
|---|---|---|
| Audio (`renderBlock`) | Leer/escribir la máquina activa, consumir comandos, publicar instantáneas, consumir el intercambio | Reservar memoria, bloquear, hacer E/S, escribir logs, destruir máquinas |
| Control / UI | Construir paquetes y máquinas, publicarlas, escribir comandos y logs, drenar máquinas retiradas, leer instantáneas | Tocar la máquina activa del hilo de audio |
| WebView | Leer instantáneas y logs | Escribir en colas del audio sin pasar por el hilo de control |

La regla de oro: **el hilo de audio nunca espera a nadie**. Cuando un
productor se adelanta, se pierde información y se cuenta la pérdida; el audio
sigue sonando.

## 2. Canales y política de desbordamiento

| Canal | Tipo | Capacidad | Productor → consumidor | Si se llena | Contador |
|---|---|---|---|---|---|
| `CommandQueue` | Anillo SPSC, `id`/`value` | 256 | Control → audio | **Se descarta el comando nuevo** | `dropped()` |
| `LogBus` | Anillo SPSC, registro de 48 caracteres | 128 | Control → WebView | Se descarta el registro nuevo | `dropped()` |
| `SnapshotChannel` | Seqlock de 12 palabras atómicas | 1 (coalescing) | Audio → UI | El estado nuevo **sustituye** al anterior | `publishedCount()` |
| `MachineExchange` | Puntero pendiente + cola de retiro | 1 pendiente / 8 retiradas | Control ↔ audio | Publicación: se rechaza la nueva. Retiro lleno: **se difiere el cambio de bloque** | `rejectedCount()`, `swapCount()`, `retiredPending()` |

Notas de cada política:

- **Comandos: se descarta el nuevo, no el viejo.** Si la UI se adelanta, se
  pierde la última pulsación, no la primera; el estado final puede quedar
  desfasado, pero nunca hay un comando a medias ni un audio bloqueado. El
  contador permite que la UI avise («N comandos descartados»).
- **Rangos de `id`:** `id < 0x1000` es entrada (latch/trigger/gate);
  `id >= 0x1000` es parámetro del perfil (`id - 0x1000`). Ambos se normalizan
  a `[-1, 1]` dividiendo por 32767.
- **Instantáneas: coalescing por diseño.** No hay cola de estados: la UI ve el
  último, no los 100 intermedios. El escritor marca la secuencia en impar
  mientras escribe; el lector comprueba que no cambió y, si cambió, devuelve
  `false` (no hay copia rasgada). El mensaje de diagnóstico viaja truncado a
  40 caracteres.
- **Intercambio de máquina: sin destructores en el audio.** El audio cambia
  el puntero en el límite de bloque, empuja la máquina anterior a la cola de
  retiro y sigue; el hilo de control la destruye, **una por llamada** a
  `drainRetired()`. Si la cola está llena, la máquina nueva queda pendiente y
  el cambio se reintenta en el siguiente bloque: preferimos un bloque de
  retraso a destruir en el audio.

## 3. Invariantes del callback

`MachineHost::renderBlock` cumple, en este orden:

1. **Sin reservas de memoria** en régimen estable: 0 asignaciones medidas en
   200 bloques de 64 frames con la máquina sonando
   (`InvariantesAudio.ElCallbackNoReservaMemoriaEnRegimenEstable`).
2. **Sin destructores**: las máquinas retiradas se destruyen en el hilo de
   control (`MachineExchange`).
3. **Sin locks**: SPSC con `memory_order` release/acquire y seqlock; no hay
   `mutex`, `condition_variable` ni esperas.
4. **Sin E/S**: no hay ficheros, red, `printf` ni reloj de sistema en el
   camino del audio.
5. **Sin logs desde el audio**: `LogBus` lo escribe el hilo de control. El
   diagnóstico del audio (PC, PSW, banco, fallo) sale por la instantánea, que
   es de tamaño fijo.
6. **Trabajo acotado**: el coste por bloque depende del bloque, no del número
   de comandos pendientes ni del estado de la UI (`applyPendingCommands` drena
   hasta vaciar lo aceptado, con tope de 256 por construcción).

## 4. Salida definida en silencio y en error

| Situación | Salida | Estado publicado |
|---|---|---|
| Sin máquina instalada | **Silencio explícito**: el host escribe ceros en todos los canales | `frameCounter = 0`, sin fallo |
| Paquete inexistente, ROM corrupta o perfil desconocido al cargar | La máquina anterior **sigue sonando**; la carga devuelve `false` con el motivo | Sin cambios |
| Opcode no implementado / fallo de CPU | Último nivel de DAC mantenido, **constante y finito**; la CPU deja de avanzar | `faulted = 1` y mensaje con el motivo |
| Cambio de máquina pendiente con la cola de retiro llena | La máquina activa sigue sonando hasta que haya hueco | Sin cambios |

Nunca se propaga `NaN` ni `Inf`: el DAC es una tabla de niveles y el
mecanismo de fallo detiene la CPU en vez de inventar actividad.

El silencio sin máquina se escribe explícitamente porque el buffer que entrega
un DAW puede traer restos del ciclo anterior; dejarlo sin tocar produciría
clic o ruido en la primera reproducción
(`InvariantesAudio.SinMaquinaLaSalidaEsSilencioYNoBasura`).

## 5. Reset

`MachineInstance::reset()` reinicia CPU, DAC, latches, contador de frames y
reloj de la máquina. La prueba de determinismo compara 512 frames de una
máquina reiniciada con los de una máquina recién construida: misma salida,
mismos ciclos, mismos frames
(`InvariantesAudio.ResetDevuelveElEstadoInicialDeterminista`).

No hay comando de reset en la cola. Un reset desde la UI se hace **cargando
otra vez el paquete**: se construye una máquina nueva y se publica en el
intercambio, de modo que el reinicio ocurre en el límite de bloque y nunca
sobre la máquina que está sonando en mitad de un bloque. El plugin usa esta
vía en `resetMachine()`.

## 6. Contadores observables

| Contador | Dónde | Qué mide |
|---|---|---|
| `droppedCommands()` | `MachineHost` | Comandos descartados por cola llena |
| `dropped()` | `LogBus` | Registros de log descartados por cola llena |
| `publishedCount()` | `SnapshotChannel` | Instantáneas publicadas (la UI sabe si se quedó atrás) |
| `swapCount()` / `rejectedCount()` | `MachineExchange` | Cambios instalados / publicaciones rechazadas por haber una pendiente |
| `retiredPending()` | `MachineExchange` | Máquinas esperando destrucción en el hilo de control |
| `framesRendered()` | `MachineHost` | Frames generados (no avanza sin máquina) |

## 7. Qué prueba cada invariante

| Prueba (CTest `bdmcs48_tests`) | Invariante |
|---|---|
| `InvariantesAudio.LogBusDesbordaSinBloquearYCuenta` | Overflow del log y recuperación del hueco |
| `InvariantesAudio.LaColaDeComandosDesbordaYSeRecupera` | Overflow de comandos, FIFO y contador |
| `InvariantesAudio.LaInstantaneaCoalesceYDevuelveLaUltima` | Coalescing de la instantánea (última gana) |
| `InvariantesAudio.SinMaquinaLaSalidaEsSilencioYNoBasura` | Salida definida sin máquina |
| `InvariantesAudio.UnPaqueteInexistenteConservaLaMaquinaAnterior` | Carga fallida no altera el audio |
| `InvariantesAudio.UnaRomCorruptaSeRechazaYNoTocaElAudio` | SHA-256 de la imagen verificado antes de instalar |
| `InvariantesAudio.UnPerfilDesconocidoSeRechaza` | Perfil rechazado en la validación del manifiesto |
| `InvariantesAudio.UnFirmwareConOpcodeIlegalDaSalidaConstanteYFinito` | Salida definida con la CPU detenida |
| `InvariantesAudio.ElSpamDeComandosNoRompeElAudio` | 320 comandos por bloque durante 200 bloques |
| `InvariantesAudio.LosCambiosDeRelojRepetidosMantienenLaSalida` | `engine_clock` alternando extremos cada bloque |
| `InvariantesAudio.ResetDevuelveElEstadoInicialDeterminista` | Reset determinista |
| `InvariantesAudio.ElCallbackNoReservaMemoriaEnRegimenEstable` | 0 reservas en 200 bloques |
| `InvariantesAudio.ElCaminoDeFalloAcotaSusReservas` | 0 reservas con la máquina en fallo (buffer fijo `char[48]`) |
| `MachineExchange.ColaDeRetiroLlenaDifiereElCambio` | Retiro lleno aplaza el cambio |
| `CommandQueue.ColaLlenaDescartaYLoCuenta` | Overflow de comandos (prueba original) |
| `ClockAndQueue.*` | Reloj sin deriva y cola FIFO |

## 8. Límites y desviaciones

- **Cero reservas en ambos caminos (nominal y fallo)**: Con la CPU detenida, el
  mensaje de diagnóstico viaja en arrays estáticos de caracteres `char[48]`
  (`Mcs48Cpu`, `EngineStatus` y `MachineSnapshot`), eliminando cualquier
  llamada a `operator new`/`malloc`. Medido en exactamente 0 reservas / 100 bloques.
- **El contador de reservas sustituye `operator new` en el ejecutable de
  tests**, así que mide ese binario (host + core + firmware), no el plugin
  dentro de un DAW. Es una prueba de regresión, no una medida de latencia.
- **Concurrencia multihilo verificada**: Arnés multihilo real (`TestThreadStress.cpp`)
  probando contienda simultánea entre hilo de audio (`renderBlock`), hilo de control/MIDI
  (`postCommand`) e hilo de UI (`snapshot`). Integrado con ASan/UBSan y TSan en CI.
- **No se mide el comportamiento del planificador**: prioridad del hilo de
  audio, xruns del driver y latencia real del DAW quedan fuera de esta suite
  (ver `docs/PERFORMANCE_MATRIX.md` para el coste por bloque y el hito
  `BDMCS48SE-DAW-01` para la validación en DAW).
- **Cambiar la frecuencia de muestreo no es una operación en caliente del
  host**: se construye con su frecuencia y el plugin lo reconstruye en
  `prepareToPlay`. La matriz 44,1/48 kHz del rendimiento se mide por
  configuración, no por cambio en vivo.
