# ABDMCS48SE — Parámetros de Calibración DSP y Valores Mágicos (`Mcs48CalibrationConfig.h`)

Este documento cataloga todos los valores mágicos, constantes de temporización, límites de hardware y coeficientes de filtrado del motor de sonido **BDMCS48SE** (Bus/Data MCS-48 Sound Engine) hacia la estructura de configuración centralizada [`source/core/Mcs48CalibrationConfig.h`](source/core/Mcs48CalibrationConfig.h).

Inspirado en los estándares de ingeniería de la suite ABDSynths (`ABDJUNiO601/CALIBRATION_PARAMETERS.md` y `ABDAxionikVS/CALIBRATION_PARAMETERS.md`), este catálogo permite desacoplar la física del emulador de valores "hardcoded", facilitando su inspección, calibración y exposición en el contrato de datos.

---

## 1. GENERAL & MOTOR (Engine & Clock)

| ID | Nombre Legible | Defecto | Rango (Min..Max) | Unidad | Código Fuente / Justificación |
| :--- | :--- | :---: | :---: | :---: | :--- |
| `minClockScale` | **Min Clock Scale Factor** | `0.0625` | `0.0100 .. 0.5000` | `x` | `GenericMcs48Machine::kMinClockScale`. Factor de escala mínima de reloj (7 octavas abajo). |
| `maxClockScale` | **Max Clock Scale Factor** | `8.0` | `1.0 .. 16.0` | `x` | `GenericMcs48Machine::kMaxClockScale`. Factor de escala máxima de reloj (3 octavas arriba). |
| `defaultClockHz` | **Default CPU Crystal Clock** | `6000000` | `1000000 .. 12000000` | `Hz` | Reloj de cuarzo típico del Intel 8048 / 8049 (6 MHz = 400 kHz ciclos de instrucción). |
| `commandQueueCapacity` | **Command Queue Capacity** | `256` | `32 .. 1024` | `slots` | `CommandQueue::kCapacity`. Límite de comandos en cola SPSC lock-free. |
| `maxRomFileSize` | **Max ROM / Package Size** | `16777216` | `65536 .. 67108864` | `bytes` | Límite de seguridad DoS (16 MB) para carga y conversión de paquetes de firmware. |

---

## 2. VOZ & AFINACIÓN (Voice & Tuning)

| ID | Nombre Legible | Defecto | Rango (Min..Max) | Unidad | Código Fuente / Justificación |
| :--- | :--- | :---: | :---: | :---: | :--- |
| `demoBaseFreqHz` | **Demo Firmware Base Frequency** | `443.4590` | `100.0 .. 1000.0` | `Hz` | `VoiceConfig::baseFrequencyHz`. Frecuencia nativa de la ROM de voz: 902 ciclos a 400 kHz = 443.459 Hz. |
| `referenceNote` | **Reference Tuning MIDI Note** | `69.0` | `0.0 .. 127.0` | `midi` | Nota central La4 (A4 = 69) utilizada como pivote temperado. |
| `referenceHz` | **Master A4 Tuning Frequency** | `440.0` | `432.0 .. 448.0` | `Hz` | Frecuencia de referencia estándar de concierto. |
| `maxHeldNotes` | **Voice Note Memory Stack** | `8` | `1 .. 16` | `notes` | `MonoVoice::kMaxHeldNotes`. Pila de notas mantenidas para prioridad de última nota. |
| `defaultAttackMs` | **Default Voice Attack Time** | `4.0` | `0.1 .. 100.0` | `ms` | Rampa lineal anti-clics de apertura de puerta de voz. |
| `defaultReleaseMs` | **Default Voice Release Time** | `80.0` | `10.0 .. 500.0` | `ms` | Caída lineal al soltar la nota hasta llegar al silencio absoluto. |
| `glideTime` | **Fingered Portamento Glide Time** | `0.0` | `0.0 .. 2000.0` | `ms` | `MonoVoice::glideStepCoeff_`. Transición continua exponencial entre notas al tocar legato. |
| `velocityCurve` | **Velocity Dynamic Curve Profile** | `0` | `0 .. 3` | `index` | `MonoVoice::velocityCurve_`. 0: Lineal, 1: Exponencial, 2: Logarítmica, 3: Escalonada 8-bit. |

---

## 3. FILTRADO, DAC & SALIDA (Output & Circuit)

| ID | Nombre Legible | Defecto | Rango (Min..Max) | Unidad | Código Fuente / Justificación |
| :--- | :--- | :---: | :---: | :---: | :--- |
| `dacTolerance` | **R-2R DAC Network Tolerance** | `0.0` | `0.000 .. 1.000` | `ratio` | `GenericMcs48Machine::dacTolerance_`. Desajuste de resistencias R-2R (DNL/INL) y saturación vintage $\tanh$. |
| `dcBlockerFeedback` | **DC Blocker Pole Feedback** | `0.995` | `0.900 .. 0.999` | `coeff` | `PluginProcessor.cpp:274`. Polo del filtro de paso alto $y[n] = x[n] - x[n-1] + R \cdot y[n-1]$ para centrar la salida del DAC en 0 V. |
| `diagToneFreqHz` | **Diagnostic Tone Frequency** | `440.0` | `100.0 .. 1000.0` | `Hz` | Tono senoidal de prueba acústica para verificación rápida del canal de audio. |
| `diagToneAmplitude` | **Diagnostic Tone Headroom Gain** | `0.2` | `0.05 .. 1.0` | `x` | Nivel relativo (-14 dBFS) del oscilador de prueba para evitar saturación en monitores. |
| `defaultGainDb` | **Default Master Gain** | `-6.0` | `-24.0 .. +6.0` | `dB` | Headroom inicial estándar para prevenir clipping inter-sample con ondas cuadradas. |

---

## 4. MATRIZ DE COMANDOS (Command Matrix)

| ID | Nombre Legible | Defecto | Rango (Min..Max) | Unidad | Código Fuente / Justificación |
| :--- | :--- | :---: | :---: | :---: | :--- |
| `matrixRows` | **Command Matrix Total Slots** | `8` | `4 .. 16` | `rows` | `CommandMatrix::kRows`. Número de ranuras interactivas de asignación MIDI -> instrucción. |
| `matrixMaxDelayMs` | **Max Step Trigger Delay** | `5000.0` | `100.0 .. 10000.0`| `ms` | Tiempo máximo de retardo antes de despachar el comando al bus de la CPU. |
| `clockCommandScale` | **Clock Normalization Integer Span**| `32767` | `16384 .. 65535` | `steps` | Escala de cuantización de punto fijo para `MachineHost::postCommand`. |
