# Proyecto REAPER

La versión observada es REAPER 7.82. El starter original contenía los tokens `MASTERANALRANGE`, `MASTERFADER` y `MASTERTRACK`, que REAPER marcó como no reconocidos. Una copia temporal saneada abrió sin ese aviso; el `.RPP` de evidencia no se modificó para ese test. La captura de pantalla facilitada muestra las tres pistas y el MIDI, pero no acredita FX insertado, proyecto guardado ni audio generado.

El VST3 Release se copió a una ruta de usuario local y REAPER lo reportó en su caché. El script `DAW01-create-project.lua` selecciona la pista del instrumento antes de insertar el FX, crea los sends MIDI→instrumento y estéreo audio→captura, y escribe en la consola el nombre del FX reportado por la API. Confirma allí la inserción efectiva en REAPER.

El WAV `DAW01-test.wav` existente dura 6 segundos, PCM estéreo 24-bit a 44,1 kHz, pero se midió casi totalmente en silencio (pico de los primeros 2.000 samples cercano a −97 dBFS); **no usar como captura del plugin ni como evidencia DAW-01**. Las carpetas `captures/` y `reports/` no deben llenarse con ese WAV ni con un reporte derivado de él. Se dejó una copia con el nombre `captures/DAW01-a4-candidate.wav` solamente para inspección; sigue siendo inválida como evidencia y no tiene JSON asociado. Bórrala después de revisar si prefieres que `captures/` quede reservada exclusivamente a audio validado.

Las preferencias guardadas de dispositivos WaveOut/WASAPI (44,1 kHz/1024 y 48 kHz/512 respectivamente) **no son evidencia del dispositivo/conexión activa**; registra lo que muestre Options > Preferences > Audio > Device durante la ejecución real.
