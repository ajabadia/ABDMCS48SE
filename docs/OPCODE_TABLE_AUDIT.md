# BDMCS48SE — Auditoría de `OpcodeNames` y conformidad de decodificación (Fase 11)

> **Estado:** los tres entregables de decodificación de la Fase 11 están implementados y verificados en Windows/MSVC x64 (2026-11-22): auditoría de la tabla contra el desensamblador de MAME, fixture de conformidad contra el listado golden del Juno-6 (1 089 instrucciones, 504 B de datos, 155 opcodes distintos) y mapa de regiones en `MachineProfile`. La fase **no se cierra**: sigue pendiente la decisión sobre el material de `docs/mame/` en el historial de git (ver Fase 1) y el listado golden es material privado que no entra en el repositorio.
>
> Contrastar con el [roadmap](../ROADMAP.md), los [límites de emulación](FIRMWARE_ROUTES_AND_LIMITS.md) y el [paquete de firmware](FIRMWARE_PACKAGE.md).

## 1. Qué se ha comprobado

| Comprobación | Fuente de verdad | Dónde se ejecuta | Material externo |
|---|---|---|---|
| Nombre, mnemónico y longitud de los 256 opcodes | `docs/mame/mcs48dsm.cpp`, variante 8048/8049 | test `opcode_table_audit` de CTest (CI) → `tools/juno6_conformance.py --audit` | No |
| Clasificación (implementado / no soportado / ilegal) y longitud de la tabla | La CPU real (`Mcs48Cpu`), ejecutando cada opcode | `tests/TestOpcodeTable.cpp` (CI) | No |
| Longitud, opcode y forma de operando instrucción a instrucción | Listado de Ghidra del Juno-6 | `tests/TestJuno6DecodingConformance.cpp` (CTest; se omite sin el listado) | Sí (privado) |
| Mapa de regiones de la ROM (código, vectores, tablas) | Desensamblado y listado golden | `MachineProfile::juno6()` + test | No (solo metadatos) |

## 2. Método

### 2.1. Contraste con la CPU (sin material externo, en CI)

`tests/TestOpcodeTable.cpp` no compara la tabla con una lista escrita a mano: ejecuta los 256 opcodes en una `Mcs48Cpu` real y exige que la tabla diga lo mismo.

- **Clasificación:** `isIllegal` ⇔ `CpuFault::IllegalOpcode`; `!isImplemented` ⇔ `CpuFault::UnsupportedFeature`; `isImplemented` ⇔ sin fallo. El recuento queda fijado en 223 ejecutables, 7 no soportados y 26 ilegales, los mismos números de `TestOpcodeCoverage`.
- **Longitud:** para toda instrucción que no desvía el flujo (ni salto ni llamada), el PC avanzado tiene que ser exactamente `length`.
- **Saltos condicionales:** se ejecutan con argumento `0x77` y se exige que el PC no acabe en `dirección + 1`; si la tabla declarase 1 byte, el argumento se ejecutaría como instrucción.

### 2.2. Listado golden (material privado)

El **único** fixture del listado es `tests/TestJuno6DecodingConformance.cpp`. Recorre el listado de Ghidra del firmware del Juno-6 y comprueba, por instrucción: que el opcode tenga fila válida (no ilegal), que la longitud declarada coincida con los bytes, que la forma del operando coincida (mnemónico y estructura, con los literales numéricos normalizados a `<num>`), que la dirección sea código según `MachineProfile` y, si la ROM está disponible, que los bytes coincidan. Además fija las cifras del hito: 1 089 instrucciones, 504 B de datos y 155 opcodes distintos.

No hay una segunda implementación de este fixture en `tools/juno6_conformance.py`: ese script se limita a la auditoría de la tabla contra MAME (`--audit`), que es un oráculo distinto (nombres y longitudes del desensamblador oficial, no del firmware).

El listado y la ROM **no se copian ni se versionan**: el test los toma de `BDMCS48SE_JUNO6_LISTING` y `BDMCS48SE_JUNO6_ROM` y se omite limpiamente si no existen, de modo que el CI no falla por ausencia de material privado.

### 2.3. Auditoría contra MAME

`tools/juno6_conformance.py --audit` parsea los `case 0xNN:` de `docs/mame/mcs48dsm.cpp`, **resuelve las ramas condicionales** (`m_upi41`, `m_i802x` y los ternarios) para quedarse con la variante 8048/8049 y compara mnemónico y longitud (si el `case` consume un byte de parámetro). Los opcodes sin `case` caen en el `default` del switch y se consideran ilegales, igual que en la CPU.

## 3. Correcciones aplicadas a `OpcodeNames`

El primer estado de la tabla (256 filas, ya ordenada por opcode) tenía 8 filas incorrectas: nombres de las variantes 8021/8022 o UPI-41, o huecos marcados como ilegales que la CPU sí ejecuta. Contrastado con `mcs48dsm.cpp` y con `Mcs48Cpu.cpp`:

| Opcode | Tabla anterior | Tabla corregida | Referencia |
|---|---|---|---|
| `0x02` | `illegal` | `outl bus,a`, 1 B, implementado | `mcs48dsm.cpp` → `outl bus,a`; `Mcs48Cpu.cpp` → `// OUTL BUS,A` |
| `0x08` | `in a,p0` (8021/8022) | `ins a,bus`, 1 B | `mcs48dsm.cpp` → `ins a,bus`; `Mcs48Cpu.cpp` → `// INS A,BUS` |
| `0x76` | `illegal` | `jf1 $%03X`, 2 B, salto condicional | `mcs48dsm.cpp` → `jf1`; `Mcs48Cpu.cpp` → `// JF1` |
| `0x80` | `rad` (UPI-41) | `movx a,@r0`, 1 B | `mcs48dsm.cpp` → `movx a,@r0`; `Mcs48Cpu.cpp` → `// MOVX A,@R0` |
| `0x86` | `illegal` | `jni $%03X`, 2 B, **no implementado** | `mcs48dsm.cpp` → `jni`; la CPU falla con `UnsupportedFeature` |
| `0x90` | `outl p0,a` (8021/8022) | `movx @r0,a`, 1 B | `mcs48dsm.cpp` → `movx @r0,a`; `Mcs48Cpu.cpp` → `// MOVX @R0,A` |
| `0xE5` | `illegal` | `sel mb0`, 1 B | `mcs48dsm.cpp` → `sel mb0`; `Mcs48Cpu.cpp` → `// SEL MB0` |
| `0xF5` | `illegal` | `sel mb1`, 1 B | `mcs48dsm.cpp` → `sel mb1`; `Mcs48Cpu.cpp` → `// SEL MB1` |

Además se corrigió un fallo de compilación que la tabla arrastraba por no estar en el build: el inicializador `std::array<OpcodeInfo, 256> kTable = {{ … }}` cerraba con `};` en vez de `}};`.

**Nota sobre `0x76`:** la fila anterior lo marcaba ilegal, pero la ROM del Juno-6 lo usa 4 veces (`JF1`). Con la fila antigua el fixture habría contado 9 discrepancias (5 de longitud) y el arranque del firmware podía desviarse.

## 4. Exclusión deliberada: `0x01`

MAME desensambla `0x01` como `idl` con el comentario «CMOS only; called HALT by NEC and Toshiba». La familia NMOS 8048/8049 (la que emula este core y la que monta el Juno-6) no tiene `IDL`: el core lo trata como `IllegalOpcode`. La tabla mantiene `illegal` y la herramienta de auditoría lo registra como **exclusión deliberada**, no como discrepancia. Si algún día se emula una variante CMOS, habrá que cambiarlo en los dos sitios a la vez.

## 5. Resultados (2026-11-22, Windows x64, MSVC 14.51, Release)

| Comprobación | Resultado |
|---|---|
| `python tools/juno6_conformance.py --audit` | 256 filas, **0 discrepancias**, 1 exclusión deliberada |
| `ctest -R opcode_table_audit` | igual, ejecutado por CTest en la suite: 1 test, 0,10 s |
| `bdmcs48_tests` con `BDMCS48SE_JUNO6_LISTING` y `BDMCS48SE_JUNO6_ROM` | 1 089 instrucciones, 504 B de datos, 155 opcodes distintos, **0 discrepancias** (opcode, longitud y forma) |
| `tests/TestOpcodeTable.cpp` | 4/4 tests, tabla vs CPU: 223 / 7 / 26, longitudes y accesores correctos |
| `TestJuno6DecodingConformance.cpp` | regiones 1 544 B de código + 504 B de datos = 2 048 B; cero discrepancias |
| Suite completa `bdmcs48_tests` | **156 correctos, 0 fallidos** |

## 6. Cómo reproducir

```bash
# En CI o en cualquier máquina sin material privado:
ctest --test-dir build -C Release -R opcode_table_audit --output-on-failure
python tools/juno6_conformance.py --audit          # mismo test, a mano
./build/Release/bdmcs48_tests.exe

# Con el repositorio hermano y el listado golden a mano:
export BDMCS48SE_JUNO6_LISTING=/ruta/a/juno6_8049_238_ghidra.asm
export BDMCS48SE_JUNO6_ROM=/ruta/a/upd8049c-238.ic56   # opcional
./build/Release/bdmcs48_tests.exe    # el fixture se ejecuta si el listado existe
```

## 7. Límites de la comprobación

- **Referencia de la auditoría:** el test `opcode_table_audit` depende de `docs/mame/mcs48dsm.cpp`. Si ese fichero deja de estar en el árbol (decisión pendiente sobre el material MAME), hay que apuntar `BDMCS48SE_MAME_DSM` a un checkout externo o retirar el test en la misma decisión; sin referencia el test **falla** con un mensaje explícito en vez de pasar en silencio. La auditoría compara mnemónico y longitud, no la forma del operando: un cambio solo de operandos en MAME no la rompe.
- **Material privado:** el fixture golden solo se ejecuta donde exista el listado. En CI se ejecutan las comprobaciones sin material externo, que cubren clasificación y longitud pero **no** la forma de operando contra un desensamblado independiente.
- **Normalización de operandos:** los literales numéricos se comparan como `<num>`; el prefijo de página de `jmp $Nxx` (`$0%02X`…`$7%02X`) no se contrasta con el valor absoluto del listado. La semántica de página/A11 la cubren `tests/TestToyRom.cpp` y `tests/TestMcs48Cpu.cpp`.
- **Longitud de saltos y llamadas:** no se puede medir por avance de PC; se valida con el listado golden (todas las de 2 bytes del firmware) y con la auditoría de MAME.
- **Protocolo del 8243:** los opcodes `MOVD`/`ORLD`/`ANLD` se decodifican bien, pero el protocolo del expansor sigue sin modelarse (ver [FIRMWARE_ROUTES_AND_LIMITS.md](FIRMWARE_ROUTES_AND_LIMITS.md#42-tabla-por-variante)).
- **`tests/OpcodeNamesTest.cpp` eliminado (2026-11-22):** probaba la API anterior (`OpcodeName::find`, `byName`, `byMnemonic`), que ya no existe en `OpcodeNames.h`, y nunca estuvo en el target de tests. Su parte útil (la API de consulta de la tabla) se conserva como `OpcodeTable.LosAccesoresCoincidenConLaFila` dentro de `tests/TestOpcodeTable.cpp`, que además contrasta las 256 filas con la CPU real.

## 8. Pendiente

- [x] Decidir qué se hace con `tests/OpcodeNamesTest.cpp` (reescribir sobre `OpcodeTable` o eliminar). **Decidido y hecho el 2026-11-22: eliminado.** La cobertura útil que tenía (accesores `nameOf`/`mnemonicOf`/`lengthOf`/`isTwoBytes`/`isIllegal`) vive ahora en `OpcodeTable.LosAccesoresCoincidenConLaFila`; no queda ningún fichero muerto junto a `tests/TestOpcodeTable.cpp`.
- [ ] Repetir el fixture sobre otros listados golden (p. ej. PolySix 8049) cuando exista autorización para su material.
- [ ] Decidir el tratamiento del material de `docs/mame/` en el historial de git (Fase 1).
