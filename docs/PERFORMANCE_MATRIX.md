# BDMCS48SE — Matriz de rendimiento headless (Fases 3 y 5)

> **Estado:** medición realizada el 2026-11-22 en Windows x64 (MSVC Release). Cubre los objetivos de 44,1/48 kHz, bloques de 64 a 1024 muestras y 1–4 instancias en un solo hilo, con el core y la máquina MCS-48 reales. **No hay presupuesto numérico acordado**: las cifras son indicativas y sirven para detectar regresiones de orden de magnitud. No sustituyen a la medición del plugin JUCE dentro de un DAW (sigue pendiente en `BDMCS48SE-DAW-01`).
>
> Contrastar con el [roadmap](../ROADMAP.md), la [arquitectura](ARCHITECTURE.md) y la evidencia en [docs/evidence/BDMCS48SE-PERF-01](evidence/BDMCS48SE-PERF-01/).

## 1. Qué mide

`bench/ProcessBlockBenchmark.cpp` mide el camino completo de audio de una instancia headless: `MachineHost::renderBlock` → `GenericMcs48Machine::runBlock` → reloj → CPU MCS-48 → DAC. Sin JUCE, sin DAW y sin ficheros temporales: el paquete de firmware se construye en memoria (`firmware::demo::demoPackage()`).

Parámetros de la matriz (todos por línea de comandos):

| Eje | Valores medidos |
|---|---|
| Frecuencia de muestreo | 44 100 y 48 000 Hz |
| Tamaño de bloque | 64, 128, 256, 512 y 1024 muestras |
| Instancias simultáneas | 1, 2 y 4 |
| Audio por medida | 2 s (por instancia) |
| Repeticiones | 5 (se publica el mínimo) |

Los hosts de las N instancias se renderizan **secuencialmente en un solo hilo**, que es el caso de un callback de audio que atiende varias instancias. No mide escalado entre núcleos.

## 2. Tabla medida

Latencia = tamaño de bloque / frecuencia de muestreo. `ms/bloque tot` es el coste de renderizar ese bloque en todas las instancias; `tiempo real` y `CPU %` son **por instancia**.

### 44 100 Hz

| Bloque | Latencia (ms) | Inst | ms/bloque tot | Tiempo real ×inst | CPU % inst |
|---:|---:|---:|---:|---:|---:|
| 64 | 1,45 | 1 | 0,0139 | 104,6 | 0,96 |
| 64 | 1,45 | 2 | 0,0218 | 133,4 | 0,75 |
| 64 | 1,45 | 4 | 0,0434 | 133,7 | 0,75 |
| 128 | 2,90 | 1 | 0,0220 | 131,7 | 0,76 |
| 128 | 2,90 | 2 | 0,0436 | 133,0 | 0,75 |
| 128 | 2,90 | 4 | 0,0999 | 116,2 | 0,86 |
| 256 | 5,80 | 1 | 0,0391 | 148,6 | 0,67 |
| 256 | 5,80 | 2 | 0,0771 | 150,6 | 0,66 |
| 256 | 5,80 | 4 | 0,1811 | 128,2 | 0,78 |
| 512 | 11,61 | 1 | 0,0843 | 137,8 | 0,73 |
| 512 | 11,61 | 2 | 0,1655 | 140,3 | 0,71 |
| 512 | 11,61 | 4 | 0,4622 | 100,5 | 1,00 |
| 1024 | 23,22 | 1 | 0,2254 | 103,0 | 0,97 |
| 1024 | 23,22 | 2 | 0,4431 | 104,8 | 0,95 |
| 1024 | 23,22 | 4 | 0,8733 | 106,4 | 0,94 |

### 48 000 Hz

| Bloque | Latencia (ms) | Inst | ms/bloque tot | Tiempo real ×inst | CPU % inst |
|---:|---:|---:|---:|---:|---:|
| 64 | 1,33 | 1 | 0,0131 | 101,5 | 0,99 |
| 64 | 1,33 | 2 | 0,0267 | 99,7 | 1,00 |
| 64 | 1,33 | 4 | 0,0480 | 111,2 | 0,90 |
| 128 | 2,67 | 1 | 0,0211 | 126,6 | 0,79 |
| 128 | 2,67 | 2 | 0,0413 | 129,2 | 0,77 |
| 128 | 2,67 | 4 | 0,0849 | 125,6 | 0,80 |
| 256 | 5,33 | 1 | 0,0401 | 132,8 | 0,75 |
| 256 | 5,33 | 2 | 0,0697 | 153,1 | 0,65 |
| 256 | 5,33 | 4 | 0,1351 | 157,9 | 0,63 |
| 512 | 10,67 | 1 | 0,0610 | 175,0 | 0,57 |
| 512 | 10,67 | 2 | 0,1593 | 133,9 | 0,75 |
| 512 | 10,67 | 4 | 0,3507 | 121,7 | 0,82 |
| 1024 | 21,33 | 1 | 0,1568 | 136,0 | 0,74 |
| 1024 | 21,33 | 2 | 0,3382 | 126,2 | 0,79 |
| 1024 | 21,33 | 4 | 0,7279 | 117,2 | 0,85 |

Datos crudos: [performance-matrix.csv](evidence/BDMCS48SE-PERF-01/performance-matrix.csv) (tabla) y [performance-matrix.txt](evidence/BDMCS48SE-PERF-01/performance-matrix.txt) (informe del benchmark).

## 3. Lectura de los resultados

- **Coste por muestra estable:** al normalizar por muestra, las 60 celdas de dos ejecuciones quedan entre 0,108 y 0,213 µs por muestra (mediana 0,173 µs), es decir **4,7–9,3 M muestras/s** por instancia. A 48 kHz eso es entre ~98 y ~194 veces tiempo real; a 44,1 kHz, entre ~106 y ~210 (mismo coste por muestra, menos muestras por segundo de audio).
- **Coste por instancia:** la mediana por instancia apenas cambia con 1, 2 o 4 instancias (0,163 / 0,178 / 0,171 µs por muestra), lo que indica que el coste es aditivo y no hay contención más allá del ruido de medida.
- **Presupuesto observado:** una instancia consume del orden de **0,6–1,0 % de un núcleo**; cuatro instancias en un solo hilo, **≈ 2,5–4 %**.
- **Latencia:** viene del tamaño de bloque elegido por el host, de 1,3 ms (64 muestras) a 23 ms (1024 muestras). El motor no añade latencia propia.
- **Dispersión entre ejecuciones:** la segunda pasada ([run2](evidence/BDMCS48SE-PERF-01/performance-matrix-run2.csv)) dio diferencias de −46 % a +18 % por celda sobre las mismas combinaciones. Es el portátil de pruebas (turbo y gestión de energía), no el motor: por eso las cifras se publican por orden de magnitud y no como umbral.

## 4. Cómo reproducir

```bash
# Matriz completa (tabla legible)
./build/Release/bdmcs48_bench.exe

# La misma matriz en CSV, con una medida mas larga y mas repeticiones
./build/Release/bdmcs48_bench.exe --csv --seconds 2 --repeats 5

# Subconjunto con umbral (lo que ejecuta CTest)
ctest --test-dir build -C Release -R process_block_benchmark --output-on-failure

# Combinaciones concretas
./build/Release/bdmcs48_bench.exe --sample-rates 48000 --blocks 64,1024 --instances 1,8
```

`--smoke` ejecuta 48 kHz × {64…1024} × 1 instancia, 0,5 s por medida y falla si alguna combinación baja de **×4 tiempo real**. Es un detector de bloqueos o de recorridos múltiples del bloque, no un presupuesto.

## 5. Límites

- **Plataforma:** medido solo en Windows 11 x64 con MSVC 14.51, Release. No hay medición en Linux/macOS ni en la build de 32 bits.
- **Alcance:** core + máquina + host headless. **No** incluye el plugin JUCE (editor, WebView, APVTS), ni un DAW, ni drivers ASIO, ni el coste de MIDI/parámetros. La validación en DAW sigue siendo `BDMCS48SE-DAW-01`.
- **Instancias:** secuenciales en un hilo; no mide paralelismo entre núcleos ni el escalado de un host multihilo.
- **Firmware:** siempre el paquete de demostración propio. Un firmware con más trabajo por muestra cambiaría el coste; la matriz no barre perfiles.
- **Umbral:** no hay presupuesto acordado. Cualquier compromiso de CPU/dropouts requiere una decisión explícita (ver las decisiones abiertas del roadmap).
