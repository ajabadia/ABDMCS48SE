# BDMCS48SE — Decisiones, supuestos y preguntas abiertas

Este registro distingue decisiones de producto confirmadas, hechos de implementación y preguntas que siguen abiertas. El contrato inicial de producto se acordó el 2026-10-23; la aceptación técnica y legal se mantiene condicionada a las pruebas y revisiones indicadas en el [roadmap](../ROADMAP.md).

## Decisiones confirmadas por el usuario

1. **Producto inicial:** instrumento creativo monofónico, tocable por MIDI, con matriz de comandos y presets; no un emulador universal ni una promesa inicial de fidelidad a un hardware concreto.
2. **Firmware de demo:** usar el firmware MCS-48 propio de prueba que ya existe. La demo no depende de ROM comercial.
3. **Alcance de CPU:** mantener el subconjunto implementado; no anunciar compatibilidad completa con 8048/8049 ni con variantes hasta completar pruebas de conformidad.
4. **Plataformas:** Windows primero. Linux y macOS serán objetivos del plugin cuando el propio plugin JUCE se integre y pase CI allí; el CI actual del core no cuenta como verificación del plugin.
5. **Objetivo de audio:** 44,1/48 kHz, salida mono o estéreo, bloques de 64–1024 muestras. Son objetivos de aceptación, no cobertura ya probada en todas las combinaciones.
6. **Presets portables:** el objetivo es guardar el estado junto con el ID del firmware y su SHA-256, sin incrustar ROM. La implementación actual también expone el hash en la UI, pero la serialización de presets persiste la ruta del firmware; completar la identidad portable, versionado y recuperación queda pendiente.
7. **Licencia:** no se elige licencia final todavía. Resolverla después de auditar JUCE, código de terceros y firmware, y antes de cualquier release público.
8. **Orden de trabajo:** primero sanear documentación y cerrar el contrato de producto; después validar el plugin en REAPER sobre Windows con la configuración de audio que el DAW acepte y registre; luego completar presets portables y ampliar pruebas de estrés. El primer test registrará CPU/dropouts sin umbral numérico y usará captura WAV de REAPER con análisis por script reproducible.
9. **Arquitectura:** core reutilizable/headless; el core no depende de JUCE, WebView, MIDI, VST3 ni JSON, y la UI solo se comunica a través del host.
10. **Audio thread:** sin acceso al filesystem, bloqueos, logging ni llamadas de GUI/WebView desde el callback.
11. **ROMs comerciales:** no distribuir, solicitar ni enlazar; comprobar expresamente derechos y condiciones de cualquier material de terceros.

## Estado de implementación (no confundir con aceptación)

Ya existen el core MCS-48 headless, firmware demo propio, CLI/host, plugin JUCE VST3/Standalone, WebView con teclado/matriz/presets y CI general de cuatro plataformas más build WebUI. La suite y el plugin se han probado en Windows; no hay validación en DAW real y el plugin JUCE no está incluido en la matriz multiplataforma. Véanse [INSTRUMENT_SKELETON.md](INSTRUMENT_SKELETON.md) y [ROADMAP.md](../ROADMAP.md).

## Propuestas aún revisables, no compromisos

- Mantener el primer SDK especializado en MCS-48, sin perseguir un emulador universal.
- Separar contrato SDK, core, machine profiles/adaptadores, CLI y hosts JUCE.
- Dejar polifonía por varias instancias para después de medir coste y acordar semántica.
- Considerar el diseño de UI más amplio (Header, Command Matrix, Circuit Bend, Output y Diagnostics) como evolución; la demo existente tiene un subconjunto funcional.

Estas decisiones y opciones deben leerse junto con [ARCHITECTURE.md](ARCHITECTURE.md), [FIRMWARE_PACKAGE.md](FIRMWARE_PACKAGE.md) y [ROADMAP.md](../ROADMAP.md).

## Información técnica contrastada (fuentes)

- En el MCS-48 clásico el direccionamiento de programa puede abarcar hasta 12 bits/4 KiB, con comportamiento de bancos de 2 KiB; `SEL MB0/MB1` selecciona el banco que se aplica en `JMP`/`CALL`, no una traducción plana de cada lectura. Los detalles de `RET`/`RETR`, interrupciones, ROM interna y fetch externo dependen de la variante; UPI-41/42 no deben darse por equivalentes opcode a opcode. Véase [docs/FIRMWARE_PACKAGE.md](FIRMWARE_PACKAGE.md#nota-de-precisión-memoria-mcs-48-y-bancos) y sus fuentes Intel/MAME.
- En JSON Schema, `default` es una anotación, no una garantía de insertar valores ausentes. La validación estructural no sustituye comprobaciones semánticas de solapamiento, ruta, banco y perfil.
- La especialización `std::atomic<std::shared_ptr<T>>` no garantiza lock-free y no resuelve dónde se destruye el objeto retirado. El diseño realtime necesita un protocolo explícito de lifetime/reclamation.
- No debe suponerse que el host VST3 acepta cambios arbitrarios al conjunto de parámetros durante una instancia activa; IDs y significados automatizables se deberían conservar o validar host por host.

## Supuestos por validar

- El SDK inicial será una API C++ estándar consumible desde CLI y JUCE; no se ha decidido ABI estable.
- Un perfil podría combinar datos declarativos con un adaptador compilado; faltan los límites del formato de firmware/profile.
- La carga de un paquete fallida conservará la máquina activa; la política y su experiencia de usuario requieren prueba.
- Los parámetros host-visible de VST3 deberían permanecer estables entre firmwares, sujeto a decisión y pruebas en hosts.
- La identidad portable de firmware en presets debe concretar versionado/migración y el comportamiento cuando el archivo falta o su contenido cambia.

## Preguntas abiertas después del cierre de Fase 0

### Validación del producto

- ¿Qué versión exacta de REAPER y Windows se usará? Registrar al ejecutar `BDMCS48SE-DAW-01`; la decisión de usar REAPER/Windows y de tomar la última versión estable disponible ya está fijada.
- ¿Qué umbrales cuantitativos de CPU/dropouts se adoptarán después de obtener una línea base? Para la primera pasada se registran métricas sin umbral.
- ¿Qué script/herramientas concretas se usarán para medir finitud, no-constancia, duración y RMS del WAV y comparar renders? El flujo acordado es REAPER + WAV + análisis por script reproducible; ABDAudioLab queda como posible opción futura, no dependencia mientras esté en desarrollo.

### CPU y firmware

- ¿Qué variantes exactas y capacidades se añadirán después de probar el subconjunto actual? No se promete compatibilidad completa con 8048/8049.
- ¿Qué ensamblador/simulador se usará para generar y verificar futuros firmware propios?
- ¿Qué partes del firmware demo pueden redistribuirse junto al producto y bajo qué términos?

### SDK y profiles

- ¿Se distribuirá el SDK como fuente C++ embebible o necesitará ABI/paquete estable?
- ¿Los profiles serán datos más adaptador compilado, o se necesitarán extensiones externas dinámicas?
- ¿Cuándo se considerará polifonía con varias instancias, y qué semántica tendrá?

### Paquete, compatibilidad y licencias

- ¿El paquete inicial será carpeta o ZIP, qué extensión tendrá y qué campos del manifest serán normativos?
- ¿Qué esquema/versionado usará el preset para conservar estado, ID y SHA-256 de firmware, y qué recuperación habrá si falta o cambia el archivo?
- ¿Cuál será el nombre público definitivo: BDMCS48SE, ABDMCS48SE u otro?
- ¿Qué componentes concretos de MAME u otros terceros se prevé reutilizar y en qué versiones/licencias?
- ¿Qué evidencia se exigirá para aceptar firmware/ROM de terceros y sus permisos de redistribución?
- ¿Cuál será la licencia final del producto? Resolverla antes del primer release público tras auditar JUCE, terceros y firmware.

## Historial

| Fecha | Cambio |
|---|---|
| 2026-10-21 | Se registró el cambio de visión hacia un core MCS-48 headless reutilizable. La nota de no programar era una restricción temporal de aquella etapa, no una decisión vigente; se recontextualiza aquí y se sustituye por el control actual de alcance: no ampliar el producto acordado sin autorización. |
| 2026-10-23 | Cierre del contrato inicial de producto: demo monofónica MIDI con firmware propio, matriz y presets; subconjunto MCS-48 sin prometer 8048/8049 completo; propósito creativo; Windows primero; objetivo 44,1/48 kHz, mono/estéreo, bloques 64–1024; presets portables con firmware ID + SHA-256; licencia final antes de release público. Se prioriza DAW real después de fijar este alcance. |
