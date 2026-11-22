# BDMCS48SE — Especificación del paquete de firmware y ciclo de vida

> **Estado:** Especificación canónica y normativa v1.0 (Fase 2).
> Define la estructura física, el esquema del manifiesto, las reglas de validación criptográfica y la máquina de estados del ciclo de vida de firmwares para el motor MCS-48.
> Contrastar con [ARCHITECTURE.md](ARCHITECTURE.md) y el código en `source/firmware/`.

## 1. Propósito

El motor BDMCS48SE no carga volcados de bytes huérfanos sin contexto de hardware. Un microcódigo MCS-48 depende intrínsecamente del mapa de puertos, reloj de oscilador, arquitectura de memoria y circuito de salida. 

Por ello, la unidad básica de ejecución es el **Paquete de Firmware**:

```text
Firmware Package = manifest.json + binarios ROM + Machine Profile + Metadatos de interfaz
```

El cargador (`PackageLoader`) adquiere y valida el paquete íntegramente fuera del hilo de audio. Si el paquete resulta inválido o corrupto, el proceso se aborta de forma segura y **la máquina activa anterior se mantiene en funcionamiento**, protegiendo la sesión musical contra caídas o ruidos.

## 2. Estructura física del paquete

El paquete se organiza como un directorio autónomo con la siguiente disposición:

```text
<PaqueteNombre>/
├── manifest.json            # Descriptor canónico (JSON)
├── roms/                    # Imágenes binarias de microcódigo
│   ├── program-bank0.rom    # Banco 0 (hasta 2048 bytes)
│   └── program-bank1.rom    # Banco 1 opcional (hasta 2048 bytes)
├── presets/                 # Opcional: configuraciones de usuario (solo datos)
└── README.md                # Metadatos de procedencia, autoría y licencia
```

### Reglas de rutas y seguridad:
- Todas las rutas dentro de `images[].file` deben ser relativas a la carpeta raíz del paquete.
- **Prohibido el Directory Traversal:** Se rechaza de forma estricta cualquier ruta que contenga `..`, barras invertidas o que resuelva fuera del directorio del paquete.
- **Límite DoS:** El tamaño total acumulado de las imágenes binarias y del manifest no puede exceder los **16 MB**.
- **Profundidad de JSON acotada:** El parser JSON propio (`source/firmware/Json.cpp`) impone un límite de anidamiento de 32 niveles para prevenir desbordamientos de pila.

## 3. Esquema normativo del Manifest (`manifest.json`)

El manifiesto es un documento UTF-8 con la siguiente estructura:

```json
{
  "format": "bdmcs48se-firmware",
  "formatVersion": 1,
  "id": "canon.mario_arcade.sound",
  "name": "Mario Bros Arcade Sound Subsystem",
  "version": "1.0.0",
  "author": "Nintendo / Ikegami Tsushinki (Microcode analysis)",
  "license": "Proprietary ROM / Analysis & Clean-room Host",
  "machine": {
    "profile": "generic-mcs48",
    "cpu": {
      "family": "mcs48",
      "variant": "8039",
      "clockHz": 6000000
    },
    "programMemory": {
      "addressBits": 12,
      "bankSizeBytes": 2048,
      "bankCount": 1
    }
  },
  "images": [
    {
      "id": "program0",
      "file": "roms/tma1-c-6k.bin",
      "region": "program",
      "bank": 0,
      "offset": 0,
      "size": 2048,
      "sha256": "4c45a7663e26ff2c0a9611dbdd0bb2d37c95a0928ff67bd4a3ea380ddfc023e6"
    }
  ],
  "inputs": [
    { "id": "command_latch", "type": "latch", "index": 0, "bits": 8 }
  ],
  "outputs": [
    { "id": "dac_audio", "type": "mono_stream" }
  ]
}
```

### Especificación de campos:
| Campo | Tipo | Requerido | Descripción |
|---|---|---|---|
| `format` | string | Sí | Debe ser exactamente `"bdmcs48se-firmware"`. |
| `formatVersion` | integer | Sí | Versión de la especificación. Actualmente `1`. |
| `id` | string | Sí | Identificador alfanumérico único en formato punto (`a-z`, `0-9`, `.` o `-`). |
| `name` | string | Sí | Nombre legible para humanos en la interfaz gráfica. |
| `version` | string | Sí | Versión del paquete según semver (ej. `"1.0.0"`). |
| `machine.profile` | string | Sí | Identificador del perfil de hardware (`"generic-mcs48"` o derivado). |
| `machine.cpu.variant` | string | Sí | Variante del microcontrolador: `"8048"`, `"8049"`, `"8039"` o `"8035"`. |
| `machine.cpu.clockHz` | integer | Sí | Frecuencia de reloj nominal de la CPU en hercios. |
| `machine.programMemory.addressBits` | integer | Sí | Bits de dirección de programa: 11 (2 KB) o 12 (4 KB). |
| `machine.programMemory.bankCount` | integer | Sí | Número de bancos de memoria: 1 o 2. |
| `images` | array | Sí | Lista de descriptores de binarios ROM a cargar. |
| `images[].file` | string | Sí | Ruta relativa al archivo binario dentro del paquete. |
| `images[].size` | integer | Sí | Tamaño exacto en bytes (no puede superar 2048 por banco). |
| `images[].sha256` | string | Sí | Digest SHA-256 en minúsculas (64 caracteres hexadecimales). |

## 4. Memoria de programa y arquitectura de bancos (MCS-48)

La memoria de programa de la arquitectura MCS-48 opera con las siguientes reglas canónicas:
- **Espacio direccionable:** Hasta 12 bits de memoria de programa (4 096 bytes), dividido en **Banco 0** (`0x0000..0x07FF`) y **Banco 1** (`0x0800..0x0FFF`).
- **Conmutación de bancos:**
  - `SEL MB0` (`0xE5`) y `SEL MB1` (`0xF5`) modifican el bit de selección en el registro de control de la CPU.
  - El cambio de banco efectivo solo toma lugar tras ejecutar una instrucción `JMP` o `CALL`.
  - Las lecturas de datos en tabla con `MOVP A,@A` operan siempre sobre la página actual de 256 bytes donde reside el PC.
  - La instrucción `MOVP3 A,@A` lee específicamente en la página 3 del banco actual (`0x0300..0x03FF`).

## 5. Máquina de estados del ciclo de vida

```text
 ┌──────────┐
 │ Unloaded │
 └────┬─────┘
      │ loadPackage()
      ▼
 ┌────────────┐     Error / Fallo
 │ Validating ├──────────────────────────┐
 └────┬───────┘                          │
      │ Manifiesto y SHA-256 válidos     │
      ▼                                  │
 ┌──────────────┐                        │
 │ Constructing │                        │
 └────┬─────────┘                        │
      │ Fábrica instancia máquina        │
      ▼                                  │
 ┌─────────┐                             │
 │ Mapping │                             │
 └────┬────┘                             │
      │ Memoria y DAC conectados         │
      ▼                                  │
 ┌───────┐                               │
 │ Ready │                               │
 └────┬──┘                               │
      │ Atomic swap en audio boundary    │
      ▼                                  │
 ┌─────────┐                             │
 │ Running │                             │
 └────┬────┘                             │
      │                                  │
      ├── Opcode ilegal / fallo CPU ──► ┌┴───────┐
      │                                 │ Faulted│
      │ Carga de nuevo paquete          └────────┘
      ▼
 ┌──────────┐
 │ Swapping │ (La máquina previa continúa sonando hasta la activación del candidato)
 └──────────┘
```

### Garantía de preservación:
Si la fase `Validating`, `Constructing` o `Mapping` detecta un error (archivo ausente, SHA-256 discordante, JSON malformado o variante no soportada), el estado del candidato se destruye y el motor permanece en su estado `Running` anterior sin emitir ruidos extraños ni interrumpir la reproducción.

## 6. Procedencia, legalidad y propiedad intelectual

- **Principio de exclusión:** Ninguna ROM comercial ni microcódigo propietario de terceros se incluye en el repositorio de código fuente ni en los instaladores binarios de release.
- **Firmwares de demostración de fábrica:** El plugin y los tests incluyen microcódigo original propio (`DemoFirmware`, `MonoVoiceFirmware`, `EnhancedMonoVoiceFirmware`) creado expresamente para este proyecto bajo la misma licencia libre del software.
- **Función de los hashes SHA-256:** Los hashes listados en la documentación o en paquetes de usuario sirven exclusivamente como sumas de verificación técnica de integridad de archivo (detección de corrupción o volcado incompleto). Un hash **no confiere ni prueba derechos de distribución legal**.
