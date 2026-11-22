# BDMCS48SE — Rutas de firmware, licencias y límites de emulación

> **Estado:** **ruta de pruebas aprobada por el usuario el 2026-11-22** ([sección 5](#5-ruta-de-pruebas-aprobada-el-2026-11-22)), con lo que se cierra el criterio de salida de la **Fase 1** junto con la política de ROMs ([sección 2](#2-política-de-roms-reglas-operativas)) y la plantilla de inventario ([sección 3](#3-plantilla-de-inventario-de-licenciaprocedencia)). La aprobación es de la **ruta y de la evidencia exigida por nivel**, no de archivos concretos: ningún firmware de tercero queda admitido hasta tener su fila de inventario. Siguen abiertos, fuera de ese criterio, rellenar el inventario, el flujo de build propio, la ROM de test y la decisión sobre el material de `docs/mame/` ya rastreado en git ([sección 6](#6-pendiente-en-esta-fase)). No amplía el alcance del producto.
>
> Contrastar con el [roadmap](../ROADMAP.md), la [arquitectura](ARCHITECTURE.md), el [paquete de firmware](FIRMWARE_PACKAGE.md) y el [registro de decisiones](DECISIONS_AND_QUESTIONS.md).

## 1. Rutas de firmware: comparación

| Ruta | Qué aporta | Verificabilidad | Requisitos | Riesgo | Uso previsto |
|---|---|---|---|---|---|
| **Firmware propio** | Control total del contenido musical y del comportamiento; ya existe en `source/firmware/demo/` (voz monofónica, variante enriquecida) y se empaqueta con manifiesto validado | Alta: el binario y su fuente son del proyecto; hash reproducible | Ensamblador/flujo de build propio (pendiente de investigar) | Bajo: no hay material de terceros | Única ruta **distribuible** y admisible en CI, presets de fábrica y releases |
| **ROM con permiso/licencia de uso confirmada** | Permite validar contra firmware real de un tercero que autoriza el uso | Media: exige conservar la evidencia de la autorización por archivo | Inventario completo (sección 3) y copia de la licencia en el momento de la obtención | Medio: una autorización mal documentada contamina el release | Uso en tests locales y, solo si la licencia lo permite expresamente, en artefactos |
| **ROM original obtenida legalmente para pruebas privadas** | Caso del firmware del Juno-6 (8049 + 8243) analizado en `ABDJuno6ROMRE`; ya se usó para el arranque de Fase 11 | Media: se verifica ejecución y decodificación contra un desensamblado propio, no contra el binario de otro | El usuario debe poseer el material legítimamente; nunca se versiona ni se comparte | Alto si se filtra al repositorio, a un preset, a un log o a un artefacto de CI | Solo validación privada en la máquina de pruebas |
| **ROM comercial descargada o set de MAME** | Ninguno que el proyecto necesite | Nula | — | **Inaceptable** | Excluida por política (sección 2) |

**Criterio para elegir ruta** (propuesta, no decisión): una ruta es admisible en el repositorio o en CI solo si existe una fuente redistribuible demostrable; cualquier otra se limita a prueba privada y queda fuera de presets, capturas y artefactos.

**Riesgo transversal:** el material de prueba local que hoy existe en el disco (por ejemplo `workspace/roms/` y `workspace/firmwares/user/`, ver el registro de hitos de 2026-11-08) es un insumo privado. Debe permanecer fuera de control de versiones y de cualquier paquete que se publique; su presencia en disco no lo convierte en distribuible.

## 2. Política de ROMs (reglas operativas)

1. **No descargar, no solicitar, no publicar y no enlazar** ROMs comerciales, ni directa ni indirectamente (incluidos sets de MAME).
2. El binario de una ROM comercial **nunca** entra en el repositorio, en un preset, en un fixture, en una captura, en un log, en un issue ni en un artefacto de CI.
3. Un **SHA-256** identifica un firmware, no lo autoriza: publicar el hash no legitima el binario ni lo hace distribuible.
4. Registrar metadatos (nombre, variante de CPU, tamaño, hash) **solo** cuando la procedencia sea legítima y exista confirmación de que se pueden documentar.
5. Si falta cualquier dato de la sección 3, el material **no se usa**: la ausencia de evidencia se trata como ausencia de permiso.
6. La revisión de licencias se repite antes de cada release, no una sola vez.

**Hallazgo (2026-11-22):** `git status` muestra rastreados en git y borrados solo en el árbol de trabajo los ficheros `docs/mame/mcs48.cpp`, `docs/mame/mcs48.h`, `docs/mame/mcs48dsm.cpp`, `docs/mame/mcs48dsm.h` y varios binarios de ROM bajo `docs/mame/mario/` (`tma1-c-*`, `tma1-v-*`). Si esos binarios se llegaron a commitear, siguen en el historial aunque desaparezcan del árbol, y la regla 2 queda comprometida. Requiere decisión expresa del usuario: no se reescribe historial ni se publica nada mientras exista esa duda. El inventario de la sección 3 debe registrar esta situación archivo por archivo.

## 3. Plantilla de inventario de licencia/procedencia

Se rellena **una fila por archivo**, no por proyecto ni por repositorio de origen. Sirve para componentes de MAME, desensamblados, herramientas, fuentes, iconos, datos y cualquier binario de terceros.

| # | Archivo / ruta exacta | Origen (repo + commit o versión) | Autor / titular | Licencia declarada (texto exacto) | Fichero de licencia en el origen | SHA-256 | Uso previsto | Forma de distribución | Obligaciones | Decisión | Fecha / responsable |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 1 |  |  |  |  |  |  | compilado / enlazado / datos / solo prueba | fuente / objeto / ninguna | atribución / copyleft / aviso / patentes | usar / no usar / pendiente |  |
| 2 |  |  |  |  |  |  |  |  |  |  |  |
| 3 |  |  |  |  |  |  |  |  |  |  |  |

**Ejemplo ilustrativo (ficticio, no inventariado):**

| # | Archivo / ruta exacta | Origen | Autor / titular | Licencia declarada | Fichero de licencia | SHA-256 | Uso previsto | Distribución | Obligaciones | Decisión |
|---|---|---|---|---|---|---|---|---|---|---|
| 0 | `example/disasm.c` | repo `example/example`, commit `abc1234` | Example Org | BSD-3-Clause (texto en `LICENSE`) | `LICENSE` | `0000…` | referencia de decodificación | ninguna (solo lectura) | conservar aviso; no reclamar autoría | usar |

**Cómo usar la plantilla:**

- Una fila por archivo exacto: un directorio con licencia global no autoriza a asumir la licencia de cada fichero (regla explícita del proyecto).
- Copiar el texto de licencia tal como estaba en el momento de la obtención; no resumirlo ni interpretarlo de memoria.
- Registrar el hash del archivo **efectivamente usado**, no el del release de origen.
- Toda fila con `Decisión = pendiente` bloquea el uso del archivo y, si el archivo ya se usa, bloquea el release.
- Para componentes MAME: además del archivo, anotar si se compila, se enlaza o solo inspira una implementación propia.

**Estado actual del inventario:** vacío en el repositorio (la ruta de pruebas quedó aprobada el 2026-11-22, pero sin filas auditadas no se admite ningún archivo de tercero). Existe material de MAME y binarios de ROM que estaban en el árbol bajo `docs/mame/` (fuentes `mcs48.cpp/.h` y `mcs48dsm.cpp/.h`, más ROMs de Mario Bros. en `docs/mame/mario/`); hoy aparecen borrados en el árbol de trabajo pero siguen rastreados en git. No hay ninguna fila con archivos concretos auditados.

## 4. Límites de emulación por variante

### 4.1 Qué implementa el core hoy

Evidencia: `source/core/Mcs48Cpu.cpp/.h`, `source/core/Mcs48Timer.h`, `source/core/ProgramMemory.h`, `tests/TestOpcodeCoverage.cpp`.

- **ISA:** de los 256 bytes de opcode, **223 se ejecutan**, **26 son huecos de la ISA** y provocan `CpuFault::IllegalOpcode`, y **7 son `CpuFault::UnsupportedFeature`** (`0x05` EN I, `0x15` DIS I, `0x16` JTF, `0x25` EN TCNTI, `0x35` DIS TCNTI, `0x75` ENT0 CLK, `0x86` JNI).
- **Memoria de programa:** 4 KiB planos (12 bits), banco = bit A11. `SEL MB0/MB1` fija el flip-flop y el bit se aplica **solo en el `JMP`/`CALL` siguiente**; el fetch secuencial conserva A11 y los saltos condicionales no lo tocan.
- **Memoria de datos:** `ramSize` configurable entre 1 y 256, por defecto 128; `reset()` la pone a cero. Bancos de registros en `0x00` y `0x18`.
- **Pila:** 8 niveles de 2 bytes en `0x08`–`0x17`; el core cuenta la profundidad y falla con `StackOverflow`/`StackUnderflow` en lugar de dar la vuelta en silencio como el puntero de 3 bits del hardware.
- **Puertos:** P1, P2 y BUS (`IN`, `OUTL`, `ORL`, `ANL`) más P4–P7 genéricos (`MOVD`, `MOV`, `ORLD`, `ANLD`) sobre el `PortBus` que implementa el perfil.
- **Temporizador/contador:** contador de 8 bits con prescaler de 32, modos Timer y EventCounter (`externalPulse()` para T1), `STRT T` reinicia el prescaler, `STRT CNT` no lo hace, `STOP TCNT` conserva el valor, y el flag TF es observable.
- **Tiempo:** 15 oscilaciones por ciclo de máquina (configurable); `runCycles` redondea hasta acabar la instrucción en curso. No se modelan las fases internas del ciclo ni las señales de bus (ALE, RD/WR).
- **Fallos:** se conserva el primer fallo, la CPU se detiene y deja un mensaje; no hay ejecución "como si" la instrucción hubiera funcionado.

### 4.2 Tabla por variante

| Variante | Programa | RAM | Puertos | Configuración necesaria | Estado en este core | Límite conocido |
|---|---|---|---|---|---|---|
| **8048** | 1 KiB interno + hasta 4 KiB externo | 64 B | P1, P2, BUS | `ramSize = 64` | ISA común soportada; RAM configurable | No distingue ROM interna de memoria externa: todo es memoria de programa plana de 4 KiB |
| **8049** | 2 KiB interno | 128 B | P1, P2, BUS | `ramSize = 128` (por defecto) | ISA común soportada | Igual que 8048; variante por defecto del demo |
| **8035** | Sin ROM interna (solo externa) | 64 B | P1, P2, BUS | `ramSize = 64` | ISA común soportada | No modela el arranque "sin ROM interna" ni el tiempo de acceso externo |
| **8039** | Sin ROM interna (solo externa) | 128 B | P1, P2, BUS | por defecto | ISA común soportada | Igual que 8035 |
| **8748 / 8749** | EPROM equivalente a 8048/8049 | 64 / 128 B | P1, P2, BUS | como su equivalente | ISA común soportada | No modela borrado/programación EPROM (fuera de alcance) |
| **8243 (expansor)** | — | — | P4–P7 (nibble) con PROG/CS | perfil que traduzca `readPort`/`writePort` 4–7 | Puertos expuestos como valores de 8 bits genéricos | **No se modela el protocolo del 8243**: ni la dirección en P2 baja + PROG, ni el handshake de nibble. El firmware que use P4–P7 "funciona" a nivel de puerto, no de protocolo |
| **8041A / 8042 (UPI-41/42)** | — | — | — | — | **Fuera de alcance** | Repertorio de opcode y periféricos distintos; no se asume equivalencia opcode a opcode |
| **Familia 8051** | — | — | — | — | **Fuera de alcance** | No es MCS-48. Una tabla 8051 fue el origen de discrepancias de nombres ya corregidas (Fase 11) |

### 4.3 Lo que no está modelado

- **Interrupciones:** no hay pin INT, ni vectores en `0x003`/`0x007`, ni latencia, ni estado "en interrupción". Por eso `EN I`, `DIS I`, `JTF`, `EN TCNTI`, `DIS TCNTI`, `ENT0 CLK` y `JNI` fallan explícitamente.
- **Protocolo del 8243** (ver tabla) y cualquier periférico externo que exija temporización de bus.
- **PSW completo:** se modelan CY, AC, F0 y BS; el nibble bajo (puntero de pila en el hardware real) no se usa, y DV/DA pertenecen a la familia UPI y no se implementan.
- **Electricalidad y arranque:** niveles de tensión, flancos de T1 (solo se inyecta el pulso), tiempo de reset y estado de power-on más allá de `PC = 0` y `RAM = 0`.
- **Variante como comportamiento:** `FirmwarePackage::isKnownCpuVariant()` acepta `8048`, `8049`, `8039` y `8035` y rechaza el resto, y el plugin propaga `cpuVariant` al manifiesto, pero el core **no consume la variante**: recibe `ramSize`, `clockHz` y `clocksPerCycle`. Hoy la variante es una etiqueta validada, no un conmutador de comportamiento; la diferencia de RAM solo se respeta si el perfil o el host la traducen a `ramSize`.

## 5. Ruta de pruebas aprobada el 2026-11-22

**Decisión del usuario (2026-11-22):** se aprueban los tres niveles siguientes **y la evidencia mínima exigida en cada uno**. La aprobación cubre la ruta, no archivos concretos: ninguna ROM de tercero queda admitida hasta que su fila de inventario ([sección 3](#3-plantilla-de-inventario-de-licenciaprocedencia)) esté completa, y solo pasa a nivel A si su licencia permite redistribuir el binario.

| Nivel | Material admitido | Dónde se ejecuta | Evidencia exigida (resumen) |
|---|---|---|---|
| **A. Distribuible / CI** | Firmware propio del proyecto; firmware de tercero solo con licencia de redistribución explícita e inventario cerrado | Repositorio y CI, en toda la matriz de plataformas | Fuente versionada + build reproducible + SHA-256 del binario en el repositorio + prueba en CTest que falle si el hash no coincide |
| **B. Privado local** | ROM original poseída legalmente; ROM con permiso de uso confirmado | Máquina de pruebas del usuario; **nunca** en CI | Fila de inventario + SHA-256 del archivo usado + comando reproducible + salida/digest registrado + salto limpio de la prueba cuando el material no está |
| **C. No admitido** | ROM comercial descargada, sets de MAME, material de procedencia desconocida | — | No existe evidencia admisible: la ausencia de permiso se trata como ausencia de derecho |

### 5.1 Nivel A — qué evidencia exige

1. **Fuente versionada** del firmware en el repositorio (hoy `source/firmware/demo/`), escrita por el proyecto o con licencia compatible documentada.
2. **Build reproducible**: comando exacto, toolchain y versión; el binario que se prueba se genera desde esa fuente, no se copia a mano.
3. **SHA-256 del binario** en el repositorio (manifiesto del paquete y/o `tests/data/`), calculado sobre los bytes que la prueba ejecuta.
4. **Prueba automática en CTest** que ejecuta el firmware y falla si el hash o el comportamiento esperado no coinciden; la suite debe pasar en la matriz de CI completa.
5. **Licencia y procedencia** registradas en el inventario, con decisión `usar` y forma de distribución `fuente`/`objeto` según corresponda.
6. **Sin material de terceros** dentro del paquete: ni ROMs, ni desensamblados, ni binarios de MAME.

Un firmware de tercero que aspire a este nivel necesita además el texto de licencia citado en el origen y la confirmación de que permite redistribuir el binario; hasta entonces se queda en nivel B y no entra en presets, capturas ni artefactos.

### 5.2 Nivel B — qué evidencia exige

1. **Declaración de posesión legal** o de permiso de uso por parte del usuario; no se documenta en el repositorio la procedencia de una ROM comercial.
2. **Fila de inventario** ([sección 3](#3-plantilla-de-inventario-de-licenciaprocedencia)) con nombre, variante de CPU, tamaño, SHA-256 del archivo efectivamente usado y uso previsto. La fila puede vivir en el registro privado del usuario si la procedencia no se puede publicar.
3. **Comando reproducible**: binario de test, argumentos y variables de entorno (`BDMCS48SE_JUNO6_ROM`, `BDMCS48SE_JUNO6_LISTING`), sin rutas absolutas incrustadas.
4. **Salida o digest obtenido** registrado en la evidencia (el hash de la salida, nunca el binario).
5. **Salto limpio sin material**: las pruebas locales deben omitirse cuando la ROM o el listado no están, de modo que CI y terceros no fallen por su ausencia.
6. **Revisión antes de cada release**: si la evidencia no se puede reproducir o la autorización no está vigente, el material deja de usarse.

### 5.3 Nivel C — no admitido

No requiere evidencia porque no hay forma de acreditarlo: entrar por esta vía es una violación de la política de la sección 2, no un caso a documentar. Si apareciera material así en el árbol o en el historial, se trata como incidente (ver el hallazgo de 2026-11-22 en la sección 2) y no como material de prueba.

### 5.4 Regla común a todos los niveles

Ningún nivel autoriza a versionar el binario de un tercero. Lo que se versiona es la **evidencia verificable** —comando, hash y salida esperada—; el binario lo aporta el usuario en su máquina. Esta regla es la que hace que las pruebas sean reproducibles sin distribuir material con derechos.

## 6. Pendiente en esta fase

- [x] Aprobar o corregir la ruta de pruebas de la sección 5 (decisión del usuario; es el criterio de salida de la fase). **Aprobada el 2026-11-22** sin cambios de fondo: tres niveles (A distribuible/CI, B privado local, C no admitido) y la evidencia mínima por nivel detallada en las secciones 5.1–5.4.
- [ ] Rellenar la plantilla con los archivos concretos antes de reutilizar cualquier componente MAME.
- [ ] Investigar formato de firmware, ensamblador y flujo de build propio.
- [ ] Diseñar la ROM de test pequeña (documentar comportamiento esperado antes de implementarla).
- [ ] Repetir la auditoría de licencias antes del primer release.
