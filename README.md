# ABDMCS48SE / BDMCS48SE

**BDMCS48SE** (*Bus/Data MCS-48 Sound Engine*) es el nombre de trabajo de un proyecto para crear instrumentos musicales a partir de firmware MCS-48. El repositorio de GitHub es [ajabadia/ABDMCS48SE](https://github.com/ajabadia/ABDMCS48SE); la decisión final sobre el nombre del producto sigue abierta.

> **Estado:** núcleo headless, firmware propio, host de referencia, plugin JUCE con editor WebView (matriz de comandos, teclado, estado y presets) y CI multi-plataforma. Windows, Linux (GCC y Clang) y macOS compilan y pasan la suite; el plugin JUCE solo se ha verificado en Windows.

## Visión

El objetivo es un **core headless reutilizable** para construir sintetizadores basados en firmware, con máquinas/perfiles intercambiables y hosts o interfaces a medida. El subsistema de sonido de **Mario Bros. (1983)** es un posible perfil de compatibilidad, no una dependencia del core ni un requisito para empezar a crear instrumentos.

```text
Firmware/ROM + Machine Profile + Core MCS-48 + Host + Frontend = Instrumento
```

La intención es poder reutilizar el mismo motor para experimentos como percusión, wavetable, drones o máquinas glitch basadas en ROM, siempre dentro del alcance MCS-48 que se defina. Son direcciones de producto por explorar, no funciones implementadas.

## Arquitectura propuesta

```text
Frontend / WebView
        ↕ funciones nativas y eventos
Host JUCE (Editor + AudioProcessor)
        ↕ parámetros, colas acotadas y snapshots
SDK / Core MCS-48 headless
        ↕ interfaces de máquina
Machine Profiles y adaptadores
```

- **Core/SDK:** emulación, tiempo, memoria, I/O, audio y API C++ estándar. No debe depender de JUCE, MIDI, VST3, WebView ni JSON.
- **Machine Profile:** conecta una variante de CPU y firmware con su reloj, mapa de memoria/I/O, latches, periféricos y salida de audio. Una ROM por sí sola no define una máquina compatible.
- **Host JUCE:** traduce MIDI y automatización, maneja el audio del plugin, presets y operaciones de archivos, y media entre la UI y el core.
- **Frontend:** presenta controles y diagnóstico. La WebView nunca llama al core directamente; todas las solicitudes pasan por JUCE.

El hilo de audio no debe acceder al filesystem, bloquearse ni llamar a la GUI/WebView. Parámetros continuos, comandos discretos y estado/visualización se tratarán por canales distintos y seguros. La salida de diagnóstico y forma de onda se enviará de forma reducida, nunca muestra a muestra.

## Alcance exploratorio

### Objetivos

- Un motor MCS-48 headless que pueda probarse sin plugin ni interfaz.
- Perfiles de máquina y firmware intercambiables, sin convertir el proyecto en un emulador universal.
- Un CLI de render y tests como primera vía de validación.
- Un host JUCE VST3/Standalone y una UI WebView a medida en fases posteriores.
- Firmware propio de prueba para avanzar sin depender de una ROM comercial.
- Evaluar Mario Bros. como perfil de compatibilidad cuando se hayan identificado los requisitos técnicos y legales.

### Fuera del objetivo inicial

- Emular el juego completo, vídeo o CPU principal Z80.
- Soportar cualquier CPU o sustituir MAME como emulador universal.
- Dar por garantizada la polifonía antes de medir el coste de múltiples instancias.
- Distribuir ROMs comerciales.

## Interfaz prevista

La primera interfaz se plantea con cinco áreas principales:

- **Header:** estado de ROM, CPU, errores y master gain.
- **Command Matrix:** MIDI → comando, con probabilidad, delay y mutación.
- **Circuit Bend:** clock, latch, bus y address.
- **Output:** profundidad de bits, filtro, medidor y forma de onda.
- **Diagnostics:** ciclos por muestra, underruns y estado del core.

El secuenciador podrá ser una función secundaria. La GUI puede sustituirse por otra interfaz sin cambiar el core.

## Política estricta de ROMs

**No distribuir, no pedir, no enlazar ROMs comerciales.** Este repositorio no incluirá ni publicará dumps, ROMs comerciales ni enlaces a ellas. La aplicación podrá permitir que el usuario seleccione localmente archivos cuyo uso tenga autorizado; validar un hash no demuestra por sí mismo la procedencia ni concede derechos.

La ruta inicial preferida para desarrollar es crear firmware de prueba propio. Para cualquier firmware o ROM de terceros, hay que comprobar de forma expresa los permisos de uso y redistribución antes de incluirlo en el proyecto.

## MAME y licencias

La posible reutilización de componentes de MAME requiere revisar los **archivos concretos**, sus licencias, origen y obligaciones de distribución. La licencia global del proyecto MAME no permite inferir automáticamente la licencia de cada fichero. Si se reutiliza código GPL, habrá que cumplir las obligaciones aplicables.

**Este repositorio no declara aún una licencia definitiva para el producto.** Esa decisión queda pendiente de la auditoría de dependencias y firmware. No hay que añadir un badge de licencia hasta resolverla.

## Compilación, tests y CI

El core, el firmware, la voz y los tests no necesitan dependencias externas; solo CMake y un compilador C++20. El plugin JUCE es opcional y descarga JUCE en la primera configuración.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

- Con `-DABDMCS48SE_BUILD_PLUGIN=ON` se añaden el VST3 y el Standalone (editor WebView con recursos embebidos), más la prueba `plugin_smoke`. En Windows hace falta el paquete NuGet **Microsoft.Web.WebView2** y su runtime.
- `.github/workflows/ci.yml` compila y prueba en Linux/GCC, Linux/Clang, Windows/MSVC y macOS/Clang, y sube los logs de CTest como artefacto cuando algo falla.
- Estado por plataforma: CI en verde en las cuatro entradas de la matriz (CTest 2/2 en cada una) desde el primer run. En local, Windows está verificado en Debug y Release (119/119) con el plugin JUCE y el benchmark de humo (~160× tiempo real).

## Roadmap y documentación

- [Roadmap](ROADMAP.md): fases, hitos, casillas de seguimiento y criterios de salida.
- [Arquitectura](docs/ARCHITECTURE.md): límites entre core, perfiles, host y frontend.
- [Paquete de firmware](docs/FIRMWARE_PACKAGE.md): propuesta de manifest, validación, bancos MCS-48 y ciclo de vida/hot swap (aún no normativa).
- [Esqueleto del instrumento](docs/INSTRUMENT_SKELETON.md): cadena MIDI → voz → CPU → DAC, afinación, hilos y build del VST3/Standalone.
- [Decisiones y preguntas abiertas](docs/DECISIONS_AND_QUESTIONS.md): acuerdos, supuestos y temas que iremos resolviendo.

El roadmap es vivo: los hitos solo se marcan como completados cuando se cumple su criterio y se anota evidencia, incluida la que falta por verificar.

## Contribuciones y conversación

Por ahora, las aportaciones más útiles son información técnica contrastable, fuentes sobre MCS-48 y propuestas para reducir incertidumbres del roadmap. Antes de proponer código, consulta la arquitectura y el estado de planificación.

No publiques ROMs, dumps ni enlaces a ROMs en issues, pull requests o discusiones. Cualquier ejemplo de firmware que se proponga distribuir debe incluir su procedencia y licencia.

## Enlaces

- Repositorio: [github.com/ajabadia/ABDMCS48SE](https://github.com/ajabadia/ABDMCS48SE)
- [Roadmap](ROADMAP.md)
- [Arquitectura](docs/ARCHITECTURE.md)
- [Decisiones y preguntas](docs/DECISIONS_AND_QUESTIONS.md)
