# BDMCS48SE — Roadmap

> **Estado (2026-10-23):** prototipo de instrumento funcional: core MCS-48 headless, firmware demo propio, CLI/host, plugin JUCE VST3/Standalone y WebView con teclado, matriz y presets. El CI general cubre Linux/GCC, Linux/Clang, Windows/MSVC, macOS/Clang y build de WebUI; el plugin JUCE solo se ha compilado y probado en Windows. La validación dentro de un DAW real sigue pendiente.
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
- [x] Política de ROMs: no distribuir, solicitar ni enlazar ROMs comerciales; el firmware de prueba inicial es propio.
- [x] Prototipo implementado: core, subconjunto CPU, firmware demo, CLI/host y plugin VST3/Standalone con WebView, teclado, matriz y presets. Evidencia: `source/`, `WebUI/`, `tests/PluginSmokeTest.cpp` y [INSTRUMENT_SKELETON.md](docs/INSTRUMENT_SKELETON.md).
- [x] CI general: core/CLI en Linux/GCC, Linux/Clang, Windows/MSVC y macOS/Clang; job separado compila y comprueba WebUI. Evidencia: [`.github/workflows/ci.yml`](.github/workflows/ci.yml). Este CI no compila el plugin JUCE.
- [x] Verificación local del plugin en Windows/MSVC: `plugin_smoke`, build VST3/Standalone y apertura del Standalone con WebView2. Evidencia y límites en [INSTRUMENT_SKELETON.md](docs/INSTRUMENT_SKELETON.md).
- [ ] Validación del plugin dentro de un DAW real: pendiente; la compilación, `plugin_smoke` y abrir Standalone no la sustituyen.
- [ ] Portabilidad de presets, pruebas de audio/estrés ampliadas, licencia final y distribución/release siguen pendientes según sus fases.

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

- [ ] Comparar rutas: firmware propio, ROMs cuyo permiso/licencia de uso esté confirmado y ROM original obtenida legalmente para pruebas privadas.
- [ ] No descargar, solicitar, publicar ni enlazar ROMs comerciales; registrar únicamente hashes/metadatos cuando la procedencia sea legítima y se haya confirmado que se pueden documentar.
- [ ] Investigar formato de firmware, ensamblador y flujo de build para firmware MCS-48 propio.
- [ ] Diseñar una ROM de test pequeña que cubra reset, instrucciones, puertos y generación controlada de una señal; primero documentar comportamiento esperado, sin implementarla aún.
- [ ] Para cualquier componente de MAME: inventariar archivos exactos, origen, licencia declarada, hash y obligaciones; no presumir que la licencia global equivale a la de cada archivo.
- [ ] Documentar límites de la emulación de CPU variantes concretas y cualquier herramienta externa que se proponga.

**Criterio de salida:** ruta de pruebas aprobada, política de ROMs clara y plantilla de inventario de licencia/procedencia lista.

## Fase 2 — Contrato SDK y arquitectura del core (headless)

**Objetivo:** congelar una frontera pequeña y estable que permita reutilizar el motor desde CLI, tests, JUCE u otro host.

- [ ] Especificar API pública C++ estándar: configuración, imágenes ROM en memoria, entrada/salida de máquina, comandos, render por bloques y diagnóstico.
- [ ] Definir por separado `MachineProfile` (identidad, variante CPU, reloj, mapa de memoria/I/O, periféricos/audio y requisitos ROM) y los datos del firmware.
- [ ] Decidir qué puede declarar un perfil y qué requiere adaptador C++ específico; no perseguir configuración universal ni plugins arbitrarios en v1.
- [ ] Definir manejo de errores y estados: perfil inválido, ROM ausente/incorrecta, reset, ejecución, underrun/overrun y fallo recuperable.
- [ ] Definir reglas temporales: sample rate, reloj de CPU, acumulador fraccional, eventos con offset de muestra y comportamiento determinista.
- [ ] Definir qué significa `runUntilSample`/render y su contrato de tiempo; evitar métodos ambiguos que dependan del estado interno del host.
- [ ] Definir propiedad y ciclo de vida del paquete: imágenes validadas/inmutables compartibles; RAM, CPU, latches, PRNG y buffers privados por instancia.
- [ ] Definir estados/transiciones de load, ready, running, fault, unload y swap; decidir si un load fallido conserva la máquina anterior.
- [ ] Definir activación/reclamación de objetos para hot swap sin destruir la última instancia en audio thread.
- [ ] Definir gestión de memoria y garantía de no bloquear/no asignar memoria en la ruta de audio.
- [ ] Definir si los parámetros VST3 permanecen fijos entre perfiles y cómo se preserva automatización al cambiar firmware.

**Criterio de salida:** `docs/ARCHITECTURE.md`, `docs/FIRMWARE_PACKAGE.md` y contrato API revisados, con dependencias, ciclo de vida, validación y propiedad/hot-swap documentados; los puntos no resueltos quedan expresamente abiertos.

## Fase 3 — Prototipo headless y pruebas de referencia

**Estado:** implementación base construida; aceptación de determinismo y rendimiento todavía parcial.

- [x] Build CMake headless sin JUCE por defecto; la matriz CI compila y prueba el core/CLI en cuatro plataformas. Evidencia: `CMakeLists.txt` y `.github/workflows/ci.yml`.
- [x] Harness para CPU, reset, reloj, temporizador, memoria y puertos. Evidencia: `tests/TestMcs48Cpu.cpp`, `tests/TestMcs48Timer.cpp`, `tests/TestToyRom.cpp`.
- [x] Implementar y probar el subconjunto actual de CPU. `TestOpcodeCoverage` clasifica los 256 bytes de opcode (223 ejecutados, 7 explícitamente no soportados y 26 ilegales); esto no equivale a conformidad completa de toda la familia/variantes.
- [x] Bus, ROM/RAM, temporizador y perfil de máquina de prueba construidos; evidencia en `source/core/`, `source/machines/GenericMcs48Machine.*` y sus pruebas.
- [x] Máquina demo y firmware propio ejercitados por tests de host/instrumento; evidencia en `source/firmware/demo/`, `source/instrument/` y `tests/TestInstrument.cpp`. La evidencia no incluye validación de instrumento en DAW.
- [x] Render WAV en CLI implementado (`source/cli/main.cpp`, `commandRender`/`writeWav`).
- [ ] Añadir prueba de determinismo con misma entrada/configuración/semilla y comparar render golden/hash reproducible; el CLI por sí solo no demuestra esta propiedad.
- [x] Benchmark de humo de `renderBlock` integrado en CTest y medición local Windows registrada en el historial.
- [ ] Medir el plugin/core en los objetivos de 44,1/48 kHz y bloques 64–1024, y bajo varias instancias en hardware objetivo; la medición histórica de humo no cubre esta matriz.

**Criterio de salida:** aún no cumplido: faltan comparación de renders deterministas y las mediciones de rendimiento en el perfil/plataformas acordadas. El código headless y el render CLI existen, pero no se marca toda la fase como validada.

## Fase 4 — ROM Lab y ecosistema de firmware propio

**Objetivo:** convertir firmware propio en contenido musical y probar la API con más de una máquina.

- [ ] Definir especificación del paquete firmware/profile: versión, tamaño, variante/capacidades de CPU, regiones y bancos cuando correspondan, mapa de entradas/salidas, metadatos y validación.
- [ ] Decidir si el manifiesto es solo validación/metadata o si controla parámetros, automatización y UI; acordar qué es declarativo y qué requiere adaptador compilado.
- [ ] Definir cómo se documenta el origen, licencia y redistribución de cada firmware.
- [ ] Diseñar ejemplos musicales de prueba: oscilador por tabla, percusión sintética, secuenciador simple y material propenso a mutación controlable.
- [ ] Diseñar «ROM Lab» como objetivo futuro: ensamblado/importación, inspección, metadatos y validación, sin editor visual de ROM en v1 salvo decisión expresa.
- [ ] Crear perfiles de prueba adicionales para comprobar reutilización del core; elegirlos según resultados de Fase 0.
- [ ] Determinar si perfiles declarativos bastan o qué extensión C++ segura requiere cada sound board.

**Criterio de salida:** al menos dos perfiles distintos ejercitan la misma API y su firmware de prueba cuenta con procedencia/licencia documentada.

## Fase 5 — Robustez de audio y API de hosts

**Objetivo:** estabilizar ejecución en tiempo real antes de una UI compleja.

- [ ] Definir buses separados: parámetros continuos, comandos discretos y estado/salud de máquina.
- [ ] Especificar colas de capacidad acotada, política de desbordamiento, coalescing y contadores observables.
- [ ] Definir snapshots sin data races para estado, medidores, forma de onda reducida y diagnósticos.
- [ ] Probar sample rates y tamaños de bloque variables (incluidos objetivos 44,1/48 kHz y bloques 64–1024); reset, cambios de reloj, spam de comandos y ROM/perfil inválido.
- [ ] Establecer invariantes de audio: no I/O, no locks, no logging, no allocaciones no acotadas en callback; salida definida durante silencio/error.
- [ ] Definir mediciones de presupuesto de CPU/latencia para una y varias instancias.

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

**Objetivo:** después de validar el flujo base en DAW real, completar portabilidad de presets, estrés y rendimiento sin poner en riesgo estabilidad o determinismo.

- [ ] Fijar semántica para latch mutation, DAC quantization, clock bend, bus/address mutation y freeze.
- [ ] Aplicar deformaciones por capas con valores neutros equivalentes al baseline.
- [ ] Utilizar PRNG propio seedable; separar mutación reproducible de aleatoriedad no determinista.
- [ ] Validar que reset recupera la máquina y que inputs extremos no causan acceso inválido, cuelgue o desbordamiento.
- [ ] Medir cada efecto y número de instancias, documentando límites recomendados.
- [ ] Implementar preset portable que conserve el estado junto con ID y SHA-256 del firmware; evitar depender solo de rutas absolutas y no incrustar ROM. Definir versionado/migración y recuperación ante firmware ausente o cambiado.

**Criterio de salida:** todos los controles son reversibles, automatizables, deterministas bajo seed y pasan pruebas de fuzz/valores límite.

## Fase 10 — Distribución y mantenimiento

**Objetivo:** preparar releases cuando exista producto testeable y posición legal definida.

- [ ] Confirmar decisión de licencia tras auditoría de MAME y firmware distribuidos.
- [ ] Preparar CI/builds reproducibles para plataformas acordadas. Parcial: el CI general compila/prueba el core y CLI en cuatro plataformas y construye/comprueba WebUI; no compila el plugin JUCE. Faltan CI del plugin por plataforma, builds de release y artefactos instalables.
- [ ] Probar en DAWs/hosts objetivo, validar formato de plugin y reportar métricas de audio.
- [ ] Documentar instalación, ROMs permitidas, perfiles propios, problemas y diagnóstico.
- [ ] Añadir badges solo tras primer commit/build y valores verificables; no publicar badges de licencia hasta definir licencia.
- [ ] Firmar/paquetizar y publicar solo cuando el usuario lo autorice.

**Criterio de salida:** release reproducible, documentación legal consistente, CI verificable y aprobación explícita para publicar.

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

## Registro de hitos

| Fecha | Hito | Evidencia / notas |
|---|---|---|
| 2026-10-21 | Visión inicial y restricciones reunidas | Información del usuario; las decisiones de producto se cerraron posteriormente y las validaciones técnicas siguen según el roadmap. |
| 2026-10-23 | Editor WebView y matriz de comandos funcional | `docs/INSTRUMENT_SKELETON.md`; 8 filas nota → comando con probabilidad, delay y mutación; puente JSON versionado y bundle embebido; suite y `plugin_smoke` registrados en Windows Debug/Release, Standalone abierto con WebView2. No consta prueba en DAW real. |
| 2026-10-23 | BDMCS48SE-PRODUCT-00 — contrato de producto y saneamiento documental | Fase 0 cerrada para definir demo monofónica MIDI, firmware propio, matriz/presets, subconjunto MCS-48, Windows primero y objetivos de audio. GUI existente separada de validación DAW; licencia final y DAW objetivo siguen abiertos. |
| 2026-10-22 | Portabilidad + CI del core (cuatro plataformas) | CMake portable y sockets aislados; Linux/GCC, Linux/Clang, Windows/MSVC y macOS/Clang con CTest 2/2; job WebUI añadido después. El plugin JUCE no forma parte de la matriz. Medición local de benchmark de humo en Windows según registro anterior. |
| 2026-10-22 | Esqueleto del instrumento monofónico (MIDI + JUCE) | Firmware de voz propio, voz monofónica, `engine_clock` y ejecución por ciclos absolutos; VST3 + Standalone y `plugin_smoke`. Verificado en Windows/MSVC; no equivale a validación en host DAW. |
