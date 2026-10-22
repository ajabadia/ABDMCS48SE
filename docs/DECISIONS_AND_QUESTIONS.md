# BDMCS48SE — Decisiones, supuestos y preguntas abiertas

Este registro separa lo expresado explícitamente por el usuario de propuestas o temas que aún necesitan confirmación. Actualizarlo conforme llegue nueva información y enlazar la evidencia cuando proceda.

## Decisiones confirmadas por el usuario

1. **No programar de momento.** La etapa actual es recopilar y analizar información, documentar el proyecto y crear un roadmap que se pueda seguir y actualizar. No iniciar implementación hasta recibir autorización expresa.
2. **Explorar un core reutilizable y headless.** La meta planteada es que el motor pueda servir para otros sintetizadores con firmware/ROM distintos e interfaces a medida.
3. **Separación de capas.** El core no debe depender de JUCE, WebView, MIDI, VST3 ni JSON; la UI tampoco accede al core.
4. **JUCE como intermediario del host inicial.** La WebView habla con el editor/host JUCE; la comunicación hacia procesamiento se organiza con parámetros, colas y snapshots.
5. **Sin llamadas de UI desde el audio thread.** No invocar WebView, GUI ni filesystem dentro del callback de audio.
6. **Evitar el acoplamiento exclusivo a Mario Bros.** Mario es caso/perfil de compatibilidad, no razón para que el SDK conozca sus nombres, comandos o GUI.
7. **No distribuir, solicitar ni enlazar ROMs comerciales.** El firmware propio es una ruta inicial preferible; cualquier ROM ajena requiere verificación de derechos/condiciones.

## Propuestas de diseño aún revisables

- Especializar el primer SDK en MCS-48 y variantes elegidas, en lugar de intentar un emulador universal.
- Separar el contrato SDK, el core, los machine profiles/adaptadores, el CLI y los hosts JUCE.
- Usar firmware de prueba propio para desarrollar sin bloquearse por ROM de Mario Bros.
- Separar comunicaciones en parámetros continuos, comandos discretos y estado/visualización.
- Mantener la UI principal organizada en Header, Command Matrix, Circuit Bend, Output y Diagnostics; secuenciador como función secundaria.
- Dejar polifonía por múltiples instancias para después de medir coste y decidir semántica.

Estas opciones se reflejan en [ARCHITECTURE.md](ARCHITECTURE.md) y [ROADMAP.md](../ROADMAP.md); no se consideran compromisos irreversibles.

## Información técnica contrastada (fuentes)

- En el MCS-48 clásico el direccionamiento de programa puede abarcar hasta 12 bits/4 KiB, con comportamiento de bancos de 2 KiB; `SEL MB0/MB1` selecciona el banco que se aplica en `JMP`/`CALL`, no una traducción plana de cada lectura. Los detalles de `RET`/`RETR`, interrupciones, ROM interna y fetch externo dependen de la variante; UPI-41/42 no deben darse por equivalentes opcode a opcode. Véase [docs/FIRMWARE_PACKAGE.md](FIRMWARE_PACKAGE.md#nota-de-precisión-memoria-mcs-48-y-bancos) y sus fuentes Intel/MAME.
- En JSON Schema, `default` es una anotación, no una garantía de insertar valores ausentes. La validación estructural no sustituye comprobaciones semánticas de solapamiento, ruta, banco y perfil.
- La especialización `std::atomic<std::shared_ptr<T>>` no garantiza lock-free y no resuelve dónde se destruye el objeto retirado. El diseño realtime necesita un protocolo explícito de lifetime/reclamation.
- No debe suponerse que el host VST3 acepta cambios arbitrarios al conjunto de parámetros durante una instancia activa; IDs y significados automatizables se deberían conservar o validar host por host.

## Supuestos por validar

- El SDK inicial será una API C++ estándar consumible desde un CLI y desde JUCE.
- Una máquina podría describirse parcialmente con un profile declarativo y completar detalles con adaptador compilado.
- El flujo de audio objetivo es por bloques/muestras con temporalidad determinista y presupuestos en tiempo real.
- ROM e imagen de firmware se compartirán inmutablemente, mientras CPU/RAM/I/O/PRNG/buffers serán privados de cada instancia.
- JUCE WebView se mantendrá como interfaz inicial aunque otros frontends puedan implementarse después.
- La carga de un nuevo paquete fallida conservará la máquina actualmente activa; falta confirmar y precisar la experiencia deseada.
- Se prefiere un esquema host-visible de parámetros VST3 estable entre firmwares, salvo que se acuerde y pruebe una estrategia de migración.

## Preguntas para siguientes rondas de análisis

### Producto y primera demo

- ¿Qué resultado quieres poder escuchar o enseñar primero: test de CPU, oscilador, percusión, máquina glitch o compatibilidad con un firmware existente?
- ¿La primera entrega de audio debe sonar con firmware propio o la prioridad es Mario Bros. original?
- ¿El proyecto público se llamará BDMCS48SE o ABDMCS48SE? Hay discrepancia entre documentos actuales.

### CPU y firmware

- ¿Qué variante MCS-48 se toma como baseline? ¿Se necesita M58715 desde el comienzo?
- ¿Qué comportamiento de reloj, ROM interna/externa, RAM, timers, puertos e interrupciones es imprescindible?
- ¿Qué ensamblador, simulador o placas/hardware están disponibles para generar/verificar firmware propio?
- ¿Se pueden distribuir los ejemplos de firmware/ROM propios junto con plugin y SDK, bajo qué licencia?

### SDK y perfiles

- ¿La primera API será solo fuente C++ estable o también ABI binaria/publicada como paquete?
- ¿Los profiles describirán datos básicos o soportarán lógica de sound board escrita en C++?
- ¿Querrás permitir perfiles externos de terceros como plugins dinámicos en el futuro, o basta recompilar/adaptar el SDK?
- ¿Se considera polifonía por instancias, una sola máquina monofónica o las dos como productos/hosts distintos?

### Paquete de firmware y ciclo de vida

- ¿El paquete inicial será carpeta, ZIP u otro contenedor, y qué extensión/ubicación tendrá el manifest?
- ¿Qué campos deben ser obligatorios desde v1 y cuáles son perfiles de UI opcionales?
- ¿Qué variantes exactas MCS-48 soportarás primero? ¿Incluimos UPI-41/42 como otra familia con capacidades/opcodes propios o se excluyen?
- ¿Quieres soportar bancos reales en v1, o un subconjunto inicial explícito sin prometer banking completo?
- ¿Al fallar el firmware nuevo debemos conservar siempre el activo y el audio, o existe un modo de carga que pare el motor?
- ¿Qué registros CPU tienen valor de usuario/diagnóstico y cuáles deben quedar solo en modo debug?
- ¿Los controles de cada firmware deben exponerse como parámetros automatizables VST3, o deben permanecer como UI/preset para mantener estable la lista host-visible?
- ¿El hot swap puede diferirse hasta disponer de capacidad de retirar la instancia vieja sin interrumpir audio?

### Plataforma, audio y licencias

- ¿Plataformas de plugin y hosts prioritarios? ¿Windows primero, también macOS/Linux?
- ¿Presupuestos deseados de latencia/CPU y número de instancias simultáneas?
- ¿Qué fuentes concretas y condiciones de licencia se aceptan para firmware de prueba?
- ¿Qué componentes concretos de MAME se prevé reutilizar y en qué versión?
- ¿Qué evidencia será aceptable para declarar una ROM «libre» y redistribuible?

## Historial

| Fecha | Cambio |
|---|---|
| 2026-10-21 | Se registró el cambio de visión hacia un core MCS-48 headless reutilizable y la restricción de no programar durante la etapa actual. |
