# BDMCS48SE — Licenciamiento, Auditoría Legal y Distribución (Fase 10)

> **Estado:** Especificación normativa de release (Fase 10). Define la posición legal del proyecto tras las auditorías de código, MAME y microcódigo, y detalla la estrategia de empaquetado e instalador.
> Contrastar con [ARCHITECTURE.md](ARCHITECTURE.md) y [FIRMWARE_PACKAGE.md](FIRMWARE_PACKAGE.md).

---

## 1. Posición Legal y Marco de Licenciamiento

El proyecto BDMCS48SE está diseñado con una arquitectura en capas estrictamente desacoplada, lo que permite una estrategia de licenciamiento modular y limpia:

```text
┌────────────────────────────────────────────────────────┐
│ GUI / WebUI (TypeScript, HTML, CSS, Vite)              │
│ Licencia: MIT / Apache 2.0 (código propio de UI)      │
├────────────────────────────────────────────────────────┤
│ Host / Plugin Adapter (JUCE 9.0.4)                     │
│ Licencia dual: GPLv3 (Open Source) o Comercial (JUCE)  │
├────────────────────────────────────────────────────────┤
│ Core MCS-48, Machines y Engine Host (C++20 headless)   │
│ Licencia: BSD-3-Clause o MIT (Clean-room implementation)│
├────────────────────────────────────────────────────────┤
│ Microcódigo / ROMs incluidos de fábrica               │
│ Licencia: Microcódigo de dominio público / propio      │
└────────────────────────────────────────────────────────┘
```

### 1.1. Core MCS-48 (`bdmcs48_core`, `bdmcs48_machines`, `bdmcs48_host`)
- **Naturaleza:** Implementación original en C++20 de la ISA Intel 8048/8049/8039/8035, temporización de reloj, puertos y convertidor DAC R-2R.
- **Licencia asignada:** **BSD 3-Clause** o **MIT**. Permisiva para permitir su reutilización tanto en proyectos de código abierto como en productos comerciales integrados sin copyleft vírico.
- **Auditoría clean-room:** El código fue escrito a partir de las hojas de datos oficiales de Intel (*Intel MCS-48 User's Manual*). Cero líneas de código han sido copiadas de emuladores con licencias restrictivas.

### 1.2. Relación con MAME y herramientas de auditoría
- **Alcance del uso:** Los ficheros de desensamblado de MAME (`docs/mame/mcs48dsm.cpp`) se han empleado **única y exclusivamente como referencia técnica externa y oráculo de verificación** para auditar nemotécnicos de opcodes y discrepancias de decodificación (reportadas en [OPCODE_TABLE_AUDIT.md](OPCODE_TABLE_AUDIT.md)).
- **Aislamiento binario:** Ningún fichero de MAME, cabecera o librería con licencia BSD-3-Clause / GPL de MAME está enlazado ni se compila dentro de los binarios del plugin (`bdmcs48se_instrument`), CLI (`bdmcs48se`) o librerías del core.

### 1.3. Marco de JUCE Framework
- **Licencia Comunitaria (Open Source):** Si el plugin se compila y distribuye de forma abierta, queda regido bajo la licencia **GNU GPLv3** en cumplimiento de los términos de JUCE Open Source.
- **Licencia Comercial:** Para distribución comercial sin obligaciones de código abierto (código propietario cerrado), el proyecto se compila bajo una suscripción válida de **JUCE Indie o JUCE Pro**.

### 1.4. Política de Microcódigo y ROMs de Terceros
- **Principio de exclusión:** Ninguna ROM propietaria de terceros (e.g. Roland Juno-6, Nintendo Mario Bros, Magnavox Odyssey², Korg PolySix) se distribuye con el instalador ni se aloja en el repositorio público.
- **Firmwares de fábrica:** Se distribuyen exclusivamente paquetes con microcódigo demostrador propio (`DemoFirmware`, `MonoVoiceFirmware`, `EnhancedMonoVoiceFirmware`) creado expresamente para este producto.
- **Carga por el usuario:** La importación de ROMs externas (.bin / .hex / paquetes de usuario) se realiza en tiempo de ejecución bajo la responsabilidad y tenencia legal exclusiva del usuario final.

---

## 2. Estrategia de Empaquetado y Distribución (Release)

### 2.1. Formatos de Salida por Plataforma
| Plataforma | Formato de Plugin | Standalone | Arquitectura |
|---|---|---|---|
| **Windows 10 / 11** | VST3 (`.vst3`) | Ejecutable Win32/x64 (`.exe`) | x86_64 (con soporte IA32 opcional) |
| **macOS (12+)** | VST3 / AU (`.component`) | App Bundle (`.app`) | Universal Binary (Apple Silicon + x86_64) |
| **Linux** | VST3 | Binario ELF | x86_64 |

### 2.2. Empaquetado e Instalador para Windows (Inno Setup / NSIS)
Para la versión de lanzamiento de Windows, se define un script de instalador unificado (`installer/windows/setup.iss`):

1. **Rutas de instalación estándar:**
   - **VST3:** `{commoncf}\VST3\ABDMCS48SE.vst3` (usualmente `C:\Program Files\Common Files\VST3\ABDMCS48SE.vst3`).
   - **Standalone:** `{autopf}\ABDSynths\ABDMCS48SE\ABDMCS48SE.exe`.
   - **Presets y Datos de Fábrica:** `{userappdata}\ABDSynths\ABDMCS48SE\Presets\`.
   - **Documentación y Licencia:** `{autopf}\ABDSynths\ABDMCS48SE\Docs\`.
2. **Dependencias de ejecución (Runtime Zero-Config):**
   - **Interfaz Gráfica Embebida:** `WebUI/dist` viaja comprimido en la sección de datos binarios del ejecutable (`juce_add_binary_data`). No se requieren carpetas web sueltas en el sistema.
   - **Microsoft Edge WebView2:** El plugin enlaza estáticamente `WebView2LoaderStatic.lib`. Requiere el Microsoft Edge WebView2 Evergreen Runtime (presente por defecto en Windows 10 versión 2004+ y en todo Windows 11).
   - **Aislamiento de Perfil Web (UDF):** El plugin inicializa WebView2 con una carpeta de datos de usuario aislada por PID en `%TEMP%\ABDMCS48SE_WebView2_<PID>`, eliminando cualquier bloqueo o conflicto al abrir múltiples instancias en una sesión DAW.

### 2.3. Checklist de Verificación para Release (Pre-flight Gate)
Antes de etiquetar una versión de lanzamiento (`v1.0.0`):
- [ ] Todas las suites de CTest pasan al 100% en Release (`bdmcs48_tests`, `plugin_smoke`, `process_block_benchmark`).
- [ ] Render determinista verificado contra los digests SHA-256 de referencia.
- [ ] Auditoría de licencias libre de inclusiones accidentales de código MAME o volcados de ROM propietaria.
- [ ] Binarios de Release libres de símbolos de depuración pesados (`/DEBUG:NONE` o símbolos extraídos a ficheros `.pdb` separados).
- [ ] Firmado digital del instalador y del ejecutable (`signtool` con certificado de firma de código).
