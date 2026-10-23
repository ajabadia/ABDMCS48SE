# Utilitario de captura y análisis — BDMCS48SE-DAW-01

Este flujo sí crea el audio: ejecuta el processor JUCE real en modo headless, le inyecta un A4 MIDI, escribe la salida directa del instrumento como WAV y después genera un informe JSON reproducible con el hash del WAV. No depende de REAPER ni abre la interfaz/dispositivo de audio; no es evidencia de la configuración física del DAW ni de su carga de CPU/dropouts. Para reutilizar los runners y analizadores existentes de ABDMS2000, véase la nota de reutilización más abajo.

## Requisitos e instalación

Python 3.11 o posterior, pip, NumPy/SoundFile y el binario C++ `bdmcs48se_plugin_smoke` compilado en Release con `ABDMCS48SE_BUILD_PLUGIN=ON`. En Windows, desde la raíz:

```powershell
python -m venv .venv-audio-analysis
.venv-audio-analysis\Scripts\Activate.ps1
python -m pip install -r tools/audio-analysis/requirements.txt
cmake --build build-plugin --config Release --target bdmcs48se_plugin_smoke
python -m unittest discover -s tools/audio-analysis -p 'test_*.py' -v
```

En otra configuración CMake con el plugin JUCE habilitado, cambia `build-plugin` por su directorio. El motor C++ usa el processor/plugin real y el firmware demo propio; Python solo inspecciona el WAV y escribe el informe.

## Uso: captura headless seguida de análisis

Un comando compila el runner JUCE headless si hace falta, renderiza seis segundos de audio estéreo PCM 24-bit con A4 MIDI entre 0,5 y 5,5 s y genera el JSON con el SHA-256 del WAV. Sus artefactos se guardan en `docs/evidence/BDMCS48SE-DAW-01/headless/`, separados de las capturas reales de REAPER en `captures/`:

```powershell
python tools/audio-analysis/run_capture.py --build-if-needed
```

También acepta `--plugin-smoke <ruta-al-exe>` para usar un ejecutable existente, o `--build-dir <dir> --build-if-needed` para compilar `bdmcs48se_plugin_smoke` con el generador/configuración ya creados. Puedes ajustar `--sample-rate`, `--duration`, `--note`, `--note-on`, `--note-off` y `--velocity`; `--output-wav` y `--output-json` cambian rutas. El contexto headless se describe sin atribuir falsamente REAPER ni ajustes del host. La aplicación no abre ventana ni dispositivo físico.

Para analizar por separado una exportación original de REAPER:

```powershell
python tools/audio-analysis/analyze_capture.py `
  --input docs/evidence/BDMCS48SE-DAW-01/captures/mi-captura.wav `
  --output docs/evidence/BDMCS48SE-DAW-01/reports/mi-captura.json `
  --context docs/evidence/BDMCS48SE-DAW-01/context.json
```

`--context` es opcional. El WAV de entrada no se normaliza ni se modifica. Para anotar los ajustes observados, parte de [`context.example.json`](context.example.json) y deja en `null` lo que no hayas medido; el analizador no infiere versión del host, tasa ni buffer. `null` significa «no registrado», no cero. El contexto acepta solo `host` y `performance`; consulta la plantilla para los campos disponibles.

Si el WAV no existe, está truncado o no se puede decodificar, el comando devuelve código distinto de cero y no genera un informe que parezca válido. Directorios de salida inexistentes se crean automáticamente.

## Reutilización parcial de ABDMS2000

Gracias a la referencia a `lfo_rate_sweep.py` y `lfo_rate_analyze.py` de ABDMS2000. Se conserva su patrón útil: generar/renderizar casos de audio repetibles por separado del análisis posterior. No se invocan directamente ni se copian, porque su sweep y sus formatos de salida están ligados a los ejecutables, ROM/tablas, parámetros LFO y estructura de evidencia del MS2000. Para BDMCS48SE se usa el runner headless C++ JUCE `bdmcs48se_plugin_smoke`, que ejecuta el processor real y su firmware demo, acepta MIDI reproducible y produce WAV PCM para el analizador común. `lfo_rate_analyze.py` sirve como referencia metodológica para un análisis de modulación/LFO si ese objetivo se añade en otro momento; el informe actual mide audio general y estima frecuencia dominante, no frecuencia LFO.

## Captura en REAPER

1. Registra las versiones exactas de Windows, REAPER y del plugin; el formato de plugin es VST3.
2. Usa el dispositivo y los ajustes de audio configurados/aceptados por REAPER. No cambies sample rate ni tamaño de bloque para satisfacer este script: apunta los valores que muestre el DAW/dispositivo durante la ejecución.
3. Crea una pista MIDI con BDMCS48SE Instrument y carga el firmware demo propio. Configura una nota MIDI A4 mantenida dentro de la región de captura y luego su Note Off, dejando silencio antes/después para identificar el segmento activo.
4. Renderiza un WAV sin normalización ni procesamiento adicional. Conserva la exportación original, anota profundidad de bits/formato, frecuencia y canales seleccionados, y registra duración de la selección.
5. Repite primero con una instancia y después con dos instancias simultáneas, guardando WAV separados.
6. Ejecuta el analizador para cada captura. Guarda cada JSON junto a su WAV y registra manualmente CPU, dropouts/underruns reportados por REAPER y clicks audibles. Esta primera pasada no impone umbrales numéricos de CPU/dropouts.
7. Conserva el proyecto REAPER, capturas, informes y notas del entorno de prueba dentro del paquete de evidencia autorizado. No incluyas ROM comercial.

## Campos del informe JSON (schemaVersion 1)

- `source`: nombre de archivo, SHA-256 de los bytes originales, contenedor y subtipo leídos por libsndfile.
- `host`: DAW/formato por defecto REAPER/VST3, OS por defecto del ejecutor, y campos editables con versiones/configuración observada. Tasa, bloque e instancias quedan `null` si no se proporcionan.
- `performance`: CPU/dropouts observados o `null`; `threshold` siempre indica que no se definió umbral para la línea base.
- `audio`: finitud de muestras, igualdad exacta/no-constancia, duración, sample rate, canales, frames, RMS lineal y dBFS, peak sample y dBFS, cantidad de muestras con magnitud >= 1.0, observación de clipping, DC offset por canal, frames/muestras por encima de -90 dBFS, primer segmento contiguo cuya ventana RMS centrada de 10 ms supera -60 dBFS, y frecuencia dominante estimada.
- `truePeak` se deja `null`: el script no sobremuestrea y no presenta el máximo de muestras como true peak.
- `dominantFrequencyHz` es una estimación FFT del primer segmento activo, sumando energía espectral por canal para evitar cancelación entre canales en contrafase, con ventana Hann e interpolación local. No garantiza fundamental (un armónico puede dominar), precisión de afinación ni estabilidad temporal; el largo del segmento limita la resolución.
- RMS/peak/clipping se calculan sobre todas las muestras decodificadas; `nonConstant` compara valores exactos y no equivale a audibilidad. El valor de -90 dBFS es una convención descriptiva para contar frames silenciosos, no un umbral de aceptación de producto.

Pasa `--context` para incorporar al JSON los datos de host y rendimiento observados; verifica que reflejan lo que indica REAPER y no los valores ilustrativos de la plantilla. Mono/estéreo se obtiene del WAV, y el reporte no inventa umbrales CPU/dropout.

## Pruebas

La suite sintética cubre silencio, señal constante, seno A4 con silencio inicial/final, WAV truncado, estéreo, DC offset, nota breve, canales estéreo en contrafase y escritura de informe JSON por CLI. Ejecución:

```bash
python -m unittest discover -s tools/audio-analysis -p 'test_*.py' -v
```
