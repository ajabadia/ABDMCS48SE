# BDMCS48SE — Roadmap

> **Estado:** planificación inicial. Ningún hito de implementación se marca como completado todavía.
>
> **Regla del proyecto:** durante la etapa actual recopilamos, contrastamos y documentamos información. No comenzamos a programar hasta que el usuario lo indique expresamente.

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
- Este roadmap es vivo: el usuario puede añadir datos y reordenar el alcance antes de que empiece la implementación.

## Estado actual

- [x] Decisión de producto: orientar el diseño a un core MCS-48 headless reutilizable, no a un plugin rígidamente ligado a Mario Bros.
- [x] Principio de aislamiento: la UI habla con el host JUCE, nunca directamente con el core; el hilo de audio no accede a la WebView.
- [x] Política de distribución: el repositorio no incluye ROMs comerciales, no las solicita ni enlaza.
- [ ] Ingeniería de requisitos y arquitectura: por completar con las decisiones de este roadmap.
- [ ] Código, compilación, tests y prototipo: **no iniciados**.

---

## Fase 0 — Descubrimiento y requisitos

**Objetivo:** reducir supuestos y decidir qué problema debe resolver primero el framework.

- [ ] Definir el primer caso de uso demostrable: ROM de prueba propia, perfil de terceros con licencia verificada o perfil Mario Bros. condicionado a adquisición lícita de ROM.
- [ ] Acordar alcance de v1: emulación de CPU MCS-48, perfiles, ejecución de firmware y audio; aplazar funciones no esenciales.
- [ ] Aclarar si el objetivo inicial es fidelidad a hardware, instrumento creativo basado en ROM, o ambos con criterios separados.
- [ ] Identificar familias/variantes MCS-48 objetivo y diferencias de ROM, RAM, reloj, timers, interrupciones y puertos.
- [ ] Definir plataformas objetivo, compiladores, sample rates, latencia y presupuesto de CPU por instancia.
- [ ] Mantener un registro de supuestos, preguntas abiertas y fuentes; separar hechos comprobados de hipótesis.

**Criterio de salida:** documento de requisitos aprobado por el usuario, primer caso de uso escogido y preguntas críticas de CPU/ROM/licencias identificadas.

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

**Objetivo:** demostrar el core sin JUCE ni interfaz mediante un ejecutable de consola y render verificable.

- [ ] Preparar build CMake headless, solo después de autorización para programar.
- [ ] Implementar primero harness de tests de instrucciones, reset, reloj y puertos.
- [ ] Implementar CPU MCS-48 incrementalmente usando pruebas de referencia y ROM de test propia/autorizada.
- [ ] Implementar bus, ROM/RAM y periféricos mínimos requeridos por el primer perfil, sin trasladar al core dependencias de GUI/MAME.
- [ ] Implementar interfaz de perfil y una máquina de prueba que traduzca entradas a comportamiento de firmware documentado.
- [ ] Añadir render WAV CLI, pruebas de determinismo y comparación golden/hash de renders controlados.
- [ ] Medir tiempo de ejecución por bloque y por instancia en hardware/plataforma objetivo.

**Criterio de salida:** CLI renderiza salida reproducible de una ROM de prueba autorizada, con tests de CPU, temporización y E/S pasando; core construye y prueba sin JUCE.

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
- [ ] Probar sample rates y tamaños de bloque variables; reset, cambios de reloj, spam de comandos y ROM/perfil inválido.
- [ ] Establecer invariantes de audio: no I/O, no locks, no logging, no allocaciones no acotadas en callback; salida definida durante silencio/error.
- [ ] Definir mediciones de presupuesto de CPU/latencia para una y varias instancias.

**Criterio de salida:** pruebas de stress y sanitizers disponibles donde proceda, comportamiento de overflow/error documentado y presupuesto acordado.

## Fase 6 — Host JUCE headless y plugin mínimo

**Objetivo:** conectar MIDI/DAW a la API del core sin depender todavía de WebView.

- [ ] Definir adaptador `MachineHost`/instancia en el AudioProcessor sin filtrar tipos JUCE al SDK.
- [ ] Implementar parámetros VST3/APVTS y traducción MIDI a eventos de máquina.
- [ ] Integrar CLI/core en una ruta de render consistente con el plugin.
- [ ] Establecer serialización de estado/presets con referencia y hash de ROM/profile, nunca con datos de ROM comercial incrustados.
- [ ] Verificar carga, reset, audio y recuperación ante ROM ausente en Standalone y VST3.
- [ ] Documentar requisitos de threading y ownership entre Processor, editor y máquina.

**Criterio de salida:** plugin mínimo y CLI usan el mismo core, audio estable y parámetros/MIDI probados sin WebView.

## Fase 7 — Bridge y GUI a medida

**Objetivo:** añadir UI reemplazable que no rompa el aislamiento ni comprometa el hilo de audio.

- [ ] Definir protocolo tipado y versionado de UI; JSON solo en la frontera del host, no dentro del core/SDK.
- [ ] JS → JUCE: funciones nativas concretas para parámetros, comandos, reset y operaciones de archivos; validar tipos, rangos y permisos en C++.
- [ ] JUCE → JS: `emitEventIfBrowserIsVisible` para estado/snapshots en timer de UI; esperar a que la página esté lista antes de emitir.
- [ ] Mantener selección y lectura de ROM en C++ en hilo apropiado; no aceptar rutas arbitrarias del navegador ni tocar archivos desde audio thread.
- [ ] Ninguna llamada JS directa al core; `processBlock` no invoca WebView ni callbacks de UI.
- [ ] Separar status de alta frecuencia de visualización reducida; no transferir audio muestra a muestra.
- [ ] Diseñar panel principal: Header (ROM/CPU/errors/gain), Command Matrix, Circuit Bend, Output y Diagnostics; el secuenciador queda secundario.
- [ ] Diseñar accesibilidad, escalado, controles alternativos y visualización legible de fallos.

**Criterio de salida:** GUI controla funciones autorizadas, refleja estado en vivo sin acceso directo al core y pasa pruebas de estrés de mensajes y frecuencia de actualización.

## Fase 8 — Perfil de compatibilidad Mario Bros. (condicional)

**Objetivo:** integrar el subsistema objetivo con información verificable y dentro de los límites legales acordados.

- [ ] Investigar driver y dispositivos con fuentes primarias/versiones fijadas; identificar variante CPU, clock, ROM sockets, latches, I/O, timers y salida.
- [ ] Completar `docs/MAME_INTEGRATION.md` con referencias trazables y nivel de certeza por dato.
- [ ] Auditar cada fichero MAME candidato de manera individual antes de reutilizarlo; registrar hash, licencia, origen y forma de enlace/distribución.
- [ ] Modelar el sound board como `MachineProfile` + adaptador; el core genérico no debe adquirir nombres/comandos propios de Mario.
- [ ] Mantener ROM comercial fuera del repositorio; compatibilidad real requiere ROM obtenida legalmente por el usuario y no convierte su redistribución en permitida.
- [ ] Validar trazas/audio solo con material de origen autorizado y comparaciones reproducibles.

**Criterio de salida:** perfil documentado y reproducible, límites de licencia revisados y pruebas con insumos autorizados. Si falta ROM legal, el hito permanece bloqueado sin impedir el resto del SDK.

## Fase 9 — Circuit bending, presets y rendimiento

**Objetivo:** añadir manipulación controlable sin poner en riesgo estabilidad o determinismo.

- [ ] Fijar semántica para latch mutation, DAC quantization, clock bend, bus/address mutation y freeze.
- [ ] Aplicar deformaciones por capas con valores neutros equivalentes al baseline.
- [ ] Utilizar PRNG propio seedable; separar mutación reproducible de aleatoriedad no determinista.
- [ ] Validar que reset recupera la máquina y que inputs extremos no causan acceso inválido, cuelgue o desbordamiento.
- [ ] Medir cada efecto y número de instancias, documentando límites recomendados.
- [ ] Definir preset portable (perfil/ROM id+hash, parámetros y versión), cuidando rutas absolutas y datos propietarios.

**Criterio de salida:** todos los controles son reversibles, automatizables, deterministas bajo seed y pasan pruebas de fuzz/valores límite.

## Fase 10 — Distribución y mantenimiento

**Objetivo:** preparar releases cuando exista producto testeable y posición legal definida.

- [ ] Confirmar decisión de licencia tras auditoría de MAME y firmware distribuidos.
- [ ] Preparar CI/builds reproducibles para plataformas acordadas.
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
- Implementar código durante la etapa de recopilación/definición sin autorización expresa.

## Decisiones abiertas

1. ¿La primera demo será una ROM MCS-48 propia, un firmware libre con permiso comprobado, o ambas?
2. ¿Qué variante(s) MCS-48 y condiciones de reloj se consideran objetivo inicial?
3. ¿Qué target de CPU, sample rates y tamaños de bloque son prioritarios?
4. ¿El SDK será solo librería C++ embebible al inicio, o también API estable de plugins de machine externos?
5. ¿Machine Profile describe solo datos y un adaptador compilado, o debe soportar extensiones dinámicas desde v1?
6. ¿Qué estructura de archivos/repo y nombre público se prefieren: BDMCS48SE, ABDMCS48SE o SDK + producto?
7. ¿Qué herramientas/IDE/ensamblador MCS-48 se tienen disponibles y cuáles permiten distribuir firmware creado?
8. ¿Qué condiciones deben cumplir las ROMs «libres» antes de usarlas (licencia explícita, no comercial, atribución)?
9. ¿La UI inicial seguirá siendo JUCE WebView o se reserva la opción de otro frontend para futuros productos?

## Registro de hitos

| Fecha | Hito | Evidencia / notas |
|---|---|---|
| 2026-10-21 | Visión inicial y restricciones reunidas | Información del usuario; pendientes de validación externa y decisiones abiertas. |
