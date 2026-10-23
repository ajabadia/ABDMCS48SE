# Evidencia de validación DAW — BDMCS48SE-DAW-01

**Estado:** En progreso. Herramientas y evidencia headless listas; validación manual en REAPER pendiente. Ya existe evidencia automatizada: `headless/headless-a4.wav` y `headless/headless-a4.json` (render JUCE headless). Las capturas REAPER, informes, lecturas de CPU/dropouts y screenshots reales aún están por generar.

## Entorno detectado (solo identificación, no configuración negociada)

- REAPER 7.82 (ejecutable instalado en `C:\Program Files\REAPER (x64)\reaper.exe`).
- Windows 11 Pro Insider Preview, versión 10.0.26220, build 26220, 64 bits.
- Plugin Release VST3 0.1.0 creado por el proyecto en `build-plugin/bdmcs48se_instrument_artefacts/Release/VST3/BDMCS48SE Instrument.vst3`.
- El bundle de build Release se copió a una ruta de plugin local de usuario, `C:\\Users\\ajaba\\AppData\\Local\\Programs\\REAPER-VST3`, y REAPER reportó/escaneó `VST3i: BDMCS48SE Instrument (ABDMCS48SE)`.
- Se observó que REAPER 7.82 abre el starter de tres pistas y una región MIDI A4 de seis segundos. El script de inserción actual selecciona la pista de instrumento antes de insertar, crea un send MIDI desde la pista de notas y otro send estéreo de audio a la pista de captura. Queda pendiente observar en REAPER que el FX se insertó y guardar/reabrir el proyecto.
- Al abrir el `.RPP` original aparecieron avisos de tokens no reconocidos (`MASTERANALRANGE`, `MASTERFADER`, `MASTERTRACK`). Se probó una copia aislada en `%TEMP%` con esos tokens removidos; REAPER 7.82 registró su apertura sin repetir esos avisos. Esto confirma únicamente la corrección del parser, no que se haya insertado el VST3, guardado/restaurado el proyecto, ni generado audio válido. La captura del usuario muestra las tres pistas y el MIDI, no prueba esas acciones.
- Se observó un proceso REAPER preexistente y no se cerró.

Sample rate, tamaño de bloque, dispositivo real, CPU y dropouts quedan sin registrar hasta la comprobación en el dispositivo/host; `context.json` los deja en `null`. El recuento de una instancia describe el caso de prueba actual, no dos instancias activas.

## Contenido

- `context.json`: metadatos de host detectados y espacios para completar valores medidos.
- `reaper-project/`: guardar aquí el proyecto `.RPP` creado y guardado desde REAPER.
- `captures/`: colocar aquí únicamente WAV originales de REAPER, sin normalización ni procesamiento añadido. La carpeta no tiene todavía una captura válida.
- `reports/`: JSON generado con `tools/audio-analysis/analyze_capture.py` y notas de ejecución.
- `headless/`: render del processor JUCE sin DAW, más su informe; es una comprobación automatizada y no sustituye la evidencia grabada en REAPER.
- `screenshots/`: capturas de la UI de REAPER que documenten los estados relevantes.

## Preparación del proyecto REAPER

1. El VST3 Release ya está copiado en una ubicación de usuario local y REAPER lo detecta en su caché. Para un equipo distinto, copia el bundle de Release a la carpeta VST3 de usuario o añade la ruta en Preferences > Plug-ins > VST.
2. Abre `reaper-project/DAW01-test.RPP` (o una copia de prueba saneada del starter). La pista MIDI contiene A4 durante una región de seis segundos de proyecto.
3. Ejecuta `reaper-project/DAW01-create-project.lua` desde Actions > Show action list > New action > Load ReaScript. El script intenta insertar el VST3 en la pista del instrumento, la arma/monitoriza y prepara sends separados para MIDI→instrumento y audio estéreo→captura. Comprueba la consola, el FX y las conexiones visibles en REAPER antes de grabar.
4. Inserta una pista nueva `Captura — instrumento 1`; configura su entrada como salida de audio estéreo de la pista del instrumento (pre-FX o post-FX, anotar cuál), arma para grabación y monitoriza. Si la ruta del host no permite grabar una salida interna, usa File > Render al WAV a partir del instrumento; describe la ruta real en las notas. No grabes el track MIDI como si fuera una captura de audio.
5. Para el caso de dos instancias, duplica la pista de instrumento, envía MIDI a ambas y añade una pista de captura con ambas salidas sumadas; anota claramente el ruteo y el número de instancias.
6. Deja el sample rate y block size que REAPER/dispositivo estén usando. Abre Options > Preferences > Audio > Device y registra el dispositivo, sample rate, block size y número de bloques visibles; no cambies nada solo para alcanzar un valor deseado.
7. Usa `reaper-project/DAW01-test.RPP` como candidato (no el starter con tokens rechazados). Una vez inspeccionado en REAPER, usa File > Save Project As para guardarlo como `reaper-project/BDMCS48SE-DAW-01.RPP`; opcionalmente ejecuta `DAW01-save-request.lua` en la instancia/candidato. Confirma el nuevo nombre, que ya no aparece `[modified]`, y que el archivo abre tras cerrar el proyecto de prueba. Captura screenshots del ruteo y ajustes del dispositivo.

> El FX de la instancia queda serializado por REAPER, no fabricado a mano. `DAW01-save-request.lua` debe correrse solo sobre la instancia de prueba que se desee guardar: solicita Save As mediante la API de REAPER; comprobar el nombre, archivo y estado `[modified]` en el DAW. El proyecto DAW-01 no se considera guardado hasta verificar que abre tras reabrir.

## Ejecución controlada (por escenario)

1. Abre el VST3 y captura la UI con el firmware demo mostrado. Registra versiones y configuración negociada en `context.json` o en una copia por escenario.
2. Ejecuta A4 durante al menos cinco segundos y renderiza la salida a WAV PCM, stereo, 48 kHz si así está configurado/negociado por el proyecto, sin normalización, limitador, FX de master ni otro procesamiento (si la configuración observada es distinta, conserva la real y anótala). Guarda en `captures/`.
3. Mide la carga de CPU indicada por REAPER mientras el transporte reproduce con el plugin activo; registra el valor observado y el contador real de dropouts/underruns que muestre el host/dispositivo. Anota si escuchas clicks. No infieras dropouts del contador interno de underruns del plugin.
4. Analiza cada WAV válido:

```powershell
python tools/audio-analysis/analyze_capture.py `
  --input docs/evidence/BDMCS48SE-DAW-01/captures/single-instance-a4.wav `
  --context docs/evidence/BDMCS48SE-DAW-01/context.json `
  --output docs/evidence/BDMCS48SE-DAW-01/reports/single-instance-a4.json
```

5. Guarda el proyecto, cierra el proyecto de prueba, vuelve a abrirlo y documenta si plugin, estado y preset reaparecen correctamente; incluye screenshot. No etiquetes el estado/preset como recuperado si no lo comprobaste.
6. Prueba Reset y vuelve a tocar A4; guarda resultados.
7. Prueba firmware ausente con una ruta de paquete de prueba que el usuario pueda restaurar, y documenta el error/estado resultante. No muevas ni renombres firmware de otros proyectos o ubicaciones compartidas.
8. Repite la captura/medición con dos instancias simultáneas. Mantén archivos y notas separados de la ejecución de una instancia.

## Registro de ejecución

Completar esta tabla desde la observación real; escribir `no medido` en vez de inferir.

| Escenario | WAV / informe | Proyecto reabierto y estado/preset | Reset | Firmware ausente | Instancias | CPU REAPER | Dropouts host | Clicks / notas |
|---|---|---|---|---|---:|---:|---:|---|
| Una instancia, A4 | pendiente | pendiente | pendiente | pendiente | — | no medido | no medido | pendiente |
| Dos instancias, A4 | pendiente | pendiente | pendiente | pendiente | — | no medido | no medido | pendiente |

**No cerrar DAW-01** hasta que exista evidencia de los pasos anteriores, WAV y JSON con hashes, proyecto probado tras reabrir, capturas de pantalla y métricas observadas sin umbrales inventados. Una instancia de REAPER abierta o el build de VST3 no cuenta como validación.
