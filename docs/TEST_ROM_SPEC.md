# BDMCS48SE — Especificación de la ROM de Test Propia

> **Estado (2026-11-22):** Documento normativo de la **Fase 1**.
> Define el comportamiento esperado, vectores de prueba y contrato de audio
> de la ROM de diagnóstico y test arquitectónico para MCS-48 (8048/8049/8039).
> Redactado y congelado **antes** de la implementación del ensamblado.

---

## 1. Propósito y Alcance

La ROM de test (`mcs48.test.diagnostic`) sustituye el código de demostración ad-hoc
por una imagen de firmware reproducible con los siguientes objetivos:
1. **Verificación exhaustiva de reset y arranque:** PC en `0x000`, banderas limpias, registro SP a cero.
2. **Validación de la memoria RAM interna:** escritura y lectura de patrones (patrón alterno `0x55`, `0xAA`, `0x00`) en los 128 bytes de RAM.
3. **Validación del temporizador/contador interno:** arranque del timer con `MOV A, #imm`, `MOV T, A`, `STRT T`, avance de cuentas y lectura con `MOV A, T`.
4. **Prueba de conmutación de bancos (A11 / 4 KiB):**
   - Ejecución en Banco 0 (`0x000..0x7FF`).
   - Selección de Banco 1 (`SEL MB1`).
   - Salto interbanco (`JMP 0x820` en Banco 1).
   - Verificación de ejecución en Banco 1 (escritura testigo en RAM o puerto).
   - Selección de Banco 0 (`SEL MB0`) y retorno (`JMP 0x040` en Banco 0).
5. **Generador de señal analógica determinista en P1 (DAC):**
   - Salida escalonada periódica de 16 pasos (0x00, 0x10, 0x20 ... 0xF0) que produce una rampa determinista en el audio del DAC.
   - Oráculo exacto para comprobar el DAC y las lecturas de salida con cero dispersión.

---

## 2. Mapa de Memoria de la ROM de Test

| Dirección | Banco | Propósito |
|---|---|---|
| `0x0000` | Banco 0 | Vector de Reset: inicialización y salto a `MAIN` |
| `0x0003` | Banco 0 | Vector de interrupción externa (relleno con `NOP` / `RETR`) |
| `0x0007` | Banco 0 | Vector de interrupción de timer (relleno con `NOP` / `RETR`) |
| `0x0010` | Banco 0 | Rutina de autotest de RAM interna (`0x00..0x7F`) |
| `0x0028` | Banco 0 | Rutina de inicialización y avance del Temporizador |
| `0x0038` | Banco 0 | Conmutación hacia Banco 1 (`SEL MB1` -> `JMP 0x820`) |
| `0x0040` | Banco 0 | Punto de reentrada desde Banco 1 |
| `0x0050` | Banco 0 | Bucle de audio: generación de onda escalonada en P1 |
| `0x0800` | Banco 1 | Cabecera del Banco 1 (identificador y firma) |
| `0x0820` | Banco 1 | Subrutina de Banco 1: marca testigo en R7, conmuta a `SEL MB0` y salta a `0x0040` |

---

## 3. Secuencia y Criterio de Aceptación Paso a Paso

### Paso 1: Reset y Registros
- **Entrada:** Reset de la CPU.
- **Esperado:** `PC == 0x0000`, `PSW == 0x08` (Stack Pointer 0, Bank 0), `A == 0x00`.
- **Acción:** `CLR A`, `MOV R0, #0x7F`, `MOV R1, #0x00`.

### Paso 2: Limpieza y Prueba de RAM
- **Acción:** Bucle que escribe `0x55` en las direcciones `0x08..0x7F`, comprueba lectura, luego escribe `0x00`.
- **Esperado:** Toda la RAM interna en `0x00`. En caso de fallo de lectura, la CPU escribe `0xEE` en P2 y se detiene en bucle infinito `JMP $`.

### Paso 3: Inicialización del Temporizador
- **Acción:** `MOV A, #0x80`, `MOV T, A`, `STRT T`.
- **Esperado:** Temporizador corriendo en modo Timer con valor inicial `128`.

### Paso 4: Salto Interbanco (Banco 0 -> Banco 1 -> Banco 0)
- **Acción:**
  ```asm
  SEL MB1
  JMP 0x820   ; Salto a Banco 1
  ```
- **En Banco 1 (`0x820`):**
  ```asm
  MOV R7, #0xA5 ; Testigo de ejecución en Banco 1
  SEL MB0
  JMP 0x0040   ; Retorno a Banco 0
  ```
- **Esperado en `0x0040`:** `R7 == 0xA5`, `Bank == 0`.

### Paso 5: Generación de Audio en Puerto 1 (DAC)
- **Acción:** Bucle cerrado que incrementa el acumulador en saltos de `0x10`:
  ```asm
  audio_loop:
      OUTL P1, A
      ADD A, #0x10
      JMP audio_loop
  ```
- **Esperado:** La salida del DAC produce una rampa periódica determinista de 16 niveles. Cada ciclo completo del bucle consume exactamente 4 ciclos de máquina (`OUTL` = 2, `ADD` = 1, `JMP` = 2, total = 5 ciclos).

---

## 4. Manifiesto y Empaquetado

El manifiesto canónico `firmware.json` generado para esta ROM es:
```json
{
  "id": "mcs48.test.diagnostic",
  "name": "MCS-48 Diagnostic Test ROM",
  "version": "1.0.0",
  "targetMachine": "generic_mcs48",
  "author": "ABDMCS48SE Clean-Room Team",
  "description": "ROM propia de autotest de RAM, timer, conmutación de bancos A11 y rampa DAC",
  "machine": {
    "programMemory": {
      "addressBits": 12,
      "bankSizeBytes": 2048,
      "bankCount": 2
    }
  },
  "images": [
    {
      "region": "program",
      "bank": 0,
      "offset": 0,
      "size": 2048,
      "sha256": "<hash-banco-0>"
    },
    {
      "region": "program",
      "bank": 1,
      "offset": 0,
      "size": 2048,
      "sha256": "<hash-banco-1>"
    }
  ]
}
```
Ambos bancos van rellenados con `0x00` hasta 2048 bytes exactos.
