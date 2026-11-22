# BDMCS48SE — Ensamblador y flujo de build para firmware propio

> **Estado:** documento de decisión de la **Fase 1** (2026-11-22). Elige herramienta, formato de salida y flujo de verificación para escribir firmware MCS-48 propio. **No implementa nada**: no se ha instalado, compilado ni ejecutado ningún ensamblador todavía, no hay scripts de build y la adopción queda condicionada a la [prueba de aceptación](#4-prueba-de-aceptación-antes-de-adoptarlo) de la sección 4.
>
> Contrastar con la [especificación del paquete](FIRMWARE_PACKAGE.md), las [rutas de firmware y límites](FIRMWARE_ROUTES_AND_LIMITS.md) y el [roadmap](../ROADMAP.md).

## 1. Qué tiene que cumplir la herramienta

El firmware propio es la única ruta distribuible (§1 de [FIRMWARE_ROUTES_AND_LIMITS.md](FIRMWARE_ROUTES_AND_LIMITS.md#1-rutas-de-firmware-comparación)) y la única que puede entrar en CI, presets de fábrica y releases. Por eso la herramienta de ensamblado se elige con criterios de cadena de build, no de comodidad:

1. **Target MCS-48 real** (8048/8049/8035/8039), no un subconjunto ni un dialecto de 8051.
2. **Salida binaria determinista** y, si es posible, Intel HEX para auditoría legible.
3. **Bancos de programa:** el core direcciona 4 KiB planos con banco = A11 (`0x000–0x7FF` / `0x800–0xFFF`) y el paquete declara imágenes por banco de hasta 2048 B. La herramienta debe poder ensamblar código en `ORG 0x800` o, como mínimo, producir una imagen plana que el build pueda partir por banco.
4. **Licencia libre y clara**, apta para una herramienta de build que **no se enlaza** con el producto ni se distribuye con él.
5. **Portable:** Windows para el desarrollo diario, Linux/macOS para CI. Sin instalador propietario ni entorno MS-DOS.
6. **Reproducible:** misma fuente y misma versión → mismos bytes; el hash entra en el manifiesto.
7. **Verificable contra nuestro decodificador**: símbolos y, si los hay, listado o desensamblador para comparar ida y vuelta con `OpcodeNames`/la CPU.

## 2. Candidatos

| Herramienta | Licencia | Target 8048 | Salida | Plataformas | Mantenimiento | Encaje |
|---|---|---|---|---|---|---|
| **asm48 0.4.1** (David Hovemeyer / Adventure Vision) | BSD/MIT/dominio público, según los ficheros fuente | Sí: 8021, 8035, 8038, 8039, 8040, 8048, 8049, 8050, 8748, 8749 | `-f bin` (por defecto) y `-f hex` (Intel HEX); `-o`, `-s` símbolos, `-t` tabla de uso de bancos | Windows, Linux, macOS (fuentes C + binarios precompilados) | Estancada: 0.4.1 es de 2013 | **Recomendada** |
| **Macroassembler AS (ASL)** | GPL-2.0 | Sí (target 8048, con soporte de campos desconocidos) | Binario + listado + símbolos; HEX según opciones | Windows, DOS, Linux, macOS, OS/2 | Activa (releases en GitHub) | Plan B |
| **ASxxxx (`as8048` + `aslink`)** | GPL | Sí (8048/8041/8022/8021) | Relocalizable + enlazador con salida Intel HEX / S-record / binario | Windows, DOS, Linux, Android | Activa | Viable pero de dos etapas |
| **Frank's AS48** (Kingswood) | MIT | Sí (i8048/49/50) | Binario/HEX | Windows, Linux | Activa | Alternativa simple |
| **Asm198x** | Sin verificar | Dialecto 8048 declarado | No verificado | Multiplataforma | Proyecto reciente | Solo si las anteriores fallan |
| **TASM** | Licencia poco clara | Sí (tabla `-48`) | HEX/binario | DOS/Linux | Antigua | **Descartada** por licencia |
| **ASM48 de Intel (histórico)** | Sin licencia libre | Sí | Intel HEX | Intellec/MS-DOS | Extinta | Solo como referencia del lenguaje |
| **MAME (`unidasm`, `mcs48dsm.cpp`)** | GPL | Solo **desensambla** | Texto | Multiplataforma | Activa | Herramienta de verificación, no de build |

## 3. Decisión propuesta

**Herramienta primaria: `asm48` 0.4.1.**

Motivos:

- Es la más pequeña y directa para lo que necesitamos: ~163 kB con fuentes, C portable, sin dependencias en ejecución, y **produce directamente los dos formatos que interesan** (`-f bin` para los bytes que se ejecutan, `-f hex` para auditar direcciones).
- `-t` imprime una **tabla de uso de bancos**, que es justo lo que hay que comprobar en un mapa de 4 KiB con A11 como banco.
- Incluye `8039dasm <fichero> <offset> <len>`, un desensamblador derivado de MAME/MESS que permite el **contraste ida y vuelta** entre el fuente y el binario sin escribir un oráculo a mano.
- Licencia permisiva (BSD/MIT/dominio público según fichero), lo que evita arrastrar obligaciones de copyleft a una cadena de build reproducible.

**Plan B: Macroassembler AS (ASL).** Si `asm48` falla en macros, expresiones o direccionamiento de banco, ASL cubre 8048 con más potencia y está mantenido. Es GPL-2.0: aceptable como herramienta externa, pero si algún día se empaqueta el build hay que distribuir sus fuentes; por eso no es la primera opción.

**Descartado por ahora:** ASxxxx (dos etapas con enlazador para un firmware de 2–4 KiB es complejidad innecesaria), TASM (licencia), Asm198x (sin madurez verificada) y el ASM48 histórico de Intel (sin licencia; solo sirve como referencia del lenguaje).

**Advertencia de mantenimiento:** `asm48` no se actualiza desde 2013 y sus binarios de Windows son antiguos. La mitigación es doble: (a) compilarlo desde fuente en CI cuando haga falta (`lex.l`/`parse.y` requieren flex y bison), y (b) no depender de él en tiempo de ejecución del producto: solo genera artefactos que se versionan como código.

## 4. Prueba de aceptación (antes de adoptarlo)

Checklist ejecutable; **ninguna casilla está verificada todavía**. Si falla un caso estructural (b, c o f), se repite el checklist con ASL y se registra el resultado en `docs/evidence/`.

| # | Caso | Qué se comprueba | Criterio |
|---|---|---|---|
| a | Programa mínimo | `MOV A,#imm`, `OUTL P1,A`, `JMP`, `NOP`, `DJNZ R0,rel` | Bytes idénticos a los del manual MCS-48 y a los del demo ya escrito a mano |
| b | Banco 1 | `ORG 0x800` + `SEL MB1` + `JMP` | `-t` reporta uso del banco 1 y los bytes caen en `0x800`; si no lo soporta, el build parte una imagen plana por banco |
| c | Opcodes del arranque del Juno-6 | `CLR A`, `CPL A`, `JT0`, `JB5`, `JMPP @A`, `CALL`, `OUTL P2,A`, `MOVD P4,A`, `ORL A,R3`, `ANL A,@R1` | Se ensamblan sin error y coinciden con `TestOpcodeTable` |
| d | Directivas | `EQU`, `DB`, `DS`, expresiones y etiquetas locales | Salida estable y símbolos exportables con `-s` |
| e | Reproducibilidad | Dos ejecuciones del mismo fuente | Mismo SHA-256 del binario |
| f | Intel HEX ↔ binario | `-f hex` y `-f bin` del mismo fuente | El HEX, decodificado, es byte a byte igual al binario |
| g | Ida y vuelta | `8039dasm` sobre el binario | Mismas instrucciones y longitudes que el fuente |
| h | Opcodes no soportados | `EN I`, `DIS I`, `JTF`, `EN/DIS TCNTI`, `ENT0 CLK`, `JNI` | El ensamblador los emite y la CPU los rechaza con `UnsupportedFeature` (comportamiento esperado, no un fallo del build) |

Si (b) falla pero (a), (c) y (f) pasan, `asm48` sigue siendo válido: el build ensambla **una imagen por banco** con `ORG 0` en cada una y las compone el paso de empaquetado.

## 5. Formato de salida y contrato con el paquete

**Fuente:** `source/firmware/<nombre>/<nombre>.asm` (texto, versionado), con el mismo estilo de comentarios que `source/firmware/demo/DemoFirmware.cpp`.

**Artefactos del build (locales, no versionados):**

| Artefacto | Formato | Para qué |
|---|---|---|
| `<nombre>.hex` | Intel HEX | Auditoría legible: direcciones y bytes por línea |
| `<nombre>.bin` | Binario crudo por banco | Bytes que se ejecutan; entrada del hash |

**Lo que se versiona no es el binario, sino un array C++ generado** (`constexpr std::uint8_t kBank0Code[] = { … }`) con el mismo patrón que `DemoFirmware`. Motivos: el repositorio queda sin binarios (no hay que abrir excepciones en `.gitignore`, que ignora `*.bin` y `*.hex`), la fuente de verdad sigue siendo el `.asm`, y el test puede comprobar el hash del array contra el valor esperado. El script que convierte `.asm` → array forma parte del trabajo de implementación, no de este documento.

**Paquete resultante** (especificación en [FIRMWARE_PACKAGE.md](FIRMWARE_PACKAGE.md)):

- `images[]` con `region: "program"`, `bank` 0 o 1, `offset` dentro del banco, `size` ≤ 2048 y `sha256` del binario **exacto** que se carga.
- `machine.programMemory`: `addressBits: 12`, `bankSizeBytes: 2048`, `bankCount` 1 o 2.
- Entrada en `0x000`; sin cabecera ni metadatos dentro de la imagen.
- **Relleno explícito** hasta `size` (bytes `0x00` o `0xFF`, decidido y documentado por firmware) para que el hash sea estable; el core no rellena huecos por su cuenta.
- Intel HEX es solo formato intermedio: el paquete y el manifiesto siempre referencian el binario.

**Reglas del modelo de CPU que el fuente debe respetar** (y que el build no puede “arreglar”):

- El banco cambia **solo** en el `JMP`/`CALL` siguiente a `SEL MB0`/`SEL MB1`; los saltos condicionales y `MOVP` se quedan en la página actual.
- No hay interrupciones: `0x003` y `0x007` no son vectores útiles y `EN I`/`DIS I`/`JTF`/`JNI` fallan con `UnsupportedFeature`.
- `0x01` (IDL, solo CMOS) y los 26 huecos de la ISA detienen la CPU con `IllegalOpcode`; el firmware no debe depender de ellos.
- Reloj: 15 oscilaciones por ciclo de máquina; el tiempo de audio se deriva de ciclos, no de instrucciones.

## 6. Flujo de verificación

Propuesto para cuando se implemente; hoy no existe ningún script de este flujo.

1. **Ensamblar para auditar**
   `asm48 -f hex -s build/<nombre>.sym -t -o build/<nombre>.hex source/firmware/<nombre>/<nombre>.asm`
   Evidencia: salida de `-t` (uso de bancos) y `.sym` para revisar direcciones de etiquetas.
2. **Ensamblar los bytes**
   `asm48 -f bin -o build/<nombre>.bin …` (o una pasada por banco si (b) no se cumple).
3. **Componer bancos y hashear**: partir/rellenar a 2048 B por banco y calcular SHA-256 con `sha256sum`/`certutil`; el valor se copia al array generado y al manifiesto.
4. **Generar el paquete**: array C++ + `firmware.json` con `images[]`, `size` y `sha256`.
5. **Validar el paquete**: `bdmcs48se inspect <paquete>` debe aceptar manifiesto y hashes, y rechazar una imagen manipulada (ya cubierto por `PackageLoader`).
6. **Ejecutar dos veces**: `bdmcs48se render <paquete> --frames N` (o `diagnose`) dos veces y comparar el digest del audio; el camino de render es determinista y no usa PRNG.
7. **Probar en CTest**: un test nuevo que instale el paquete, renderice N frames y compruebe la secuencia de puertos/DAC esperada —el comportamiento se documenta **antes** de escribir el firmware, según el ítem de la ROM de test— y el SHA-256 del binario contra `tests/data/`.
8. **Contraste de decodificación**: `8039dasm build/<nombre>.bin 0 <longitud>` y comparación instrucción a instrucción con el fuente; a medio plazo, un script del estilo `tools/juno6_conformance.py` que compare el listado del ensamblador con la tabla `OpcodeNames`.
9. **Registrar evidencia**: versión del ensamblador, comandos, SHA-256 del binario, digest del render y salida de `inspect`, en `docs/evidence/<id>/context.json` + salidas. Sin esa evidencia el firmware no entra en un preset de fábrica.

## 7. Integración futura en repositorio y CI (no implementada)

- **No se vendoriza el ensamblador** ni se commitean sus binarios: se documenta cómo obtenerlo (fuente + versión + SHA-256 del archivo descargado) y se compila o descarga fuera del repositorio.
- **Primera iteración de CI:** el array generado y el hash se versionan; los tests existentes verifican el hash y el comportamiento. CI **no** necesita el ensamblador para pasar.
- **Job opcional `rebuild-firmware`** (Linux): compilar `asm48` desde fuente (gcc + flex + bison), reensamblar el `.asm`, regenerar el array y fallar si el hash difiere del versionado. Detecta deriva entre fuente y artefacto sin convertirlo en dependencia obligatoria.
- **Windows:** los binarios precompilados de 2013 son de 32 bits; si no arrancan en la máquina de desarrollo, se compila desde fuente con MSYS2/MinGW y se registra la versión usada.
- Nada de esto cambia [FIRMWARE_PACKAGE.md](FIRMWARE_PACKAGE.md): el ensamblador es una herramienta de autoría, no parte del formato.

## 8. Riesgos y límites

- **Nada ejecutado.** Este documento se basa en la documentación publicada de las herramientas; `asm48` no se ha compilado ni ejecutado en esta máquina. El soporte de `ORG 0x800` y el comportamiento exacto de `-t` están **sin verificar** (caso b del checklist).
- **Herramienta estancada:** `asm48` 0.4.1 es de 2013. Si un caso estructural falla, ASL es el plan B y puede requerir adaptar el dialecto del fuente (los fuentes son nuestros, no hay migración de terceros).
- **`8039dasm` deriva de MAME/MESS:** se usa como herramienta externa de comparación, nunca se enlaza con el producto ni se distribuye. Copiar su tabla al proyecto no aporta nada: ya tenemos `OpcodeNames` auditado y `TestOpcodeTable`.
- **Dialecto propio:** la sintaxis de `asm48` no es idéntica al ASM48 de Intel; la documentación del proyecto debe fijar la del fuente elegido para que los ejemplos no mezclen dialectos.
- **Sin macros complejas garantizadas:** si el firmware crece y necesita macros o inclusión condicional, se evalúa ASL en ese momento, no antes.
- **El binario no garantiza el comportamiento:** el flujo de la sección 6 verifica bytes, hash, decodificación y salida de audio; no sustituye la validación en DAW (`BDMCS48SE-DAW-01`) ni la prueba en hardware real.

## 9. Pendiente tras este documento

1. Ejecutar la prueba de aceptación (§4) y registrar resultados y versión del ensamblador.
2. Escribir los scripts de split/relleno por banco, generación del array C++ y del manifiesto.
3. Diseñar la ROM de test pequeña (siguiente ítem de la Fase 1) con su comportamiento esperado documentado antes de implementarla.
4. Decidir si el job `rebuild-firmware` entra en CI y con qué plataforma.
