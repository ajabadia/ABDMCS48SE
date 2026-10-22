# BDMCS48SE — Paquete de firmware y ciclo de vida

> **Estado:** propuesta para discutir. No define todavía un formato v1, no es un JSON Schema normativo y no autoriza a implementar. Contrástese con el [roadmap](../ROADMAP.md), la [arquitectura](ARCHITECTURE.md) y el [registro de decisiones](DECISIONS_AND_QUESTIONS.md).

## Propósito

El objetivo no es cargar bytes sueltos y esperar que funcionen, sino preparar una descripción validada de firmware y máquina:

```text
Firmware Package = manifest + ROM image(s) + machine profile + interface metadata
```

El loader se encarga de adquirir y validar el paquete fuera del hilo de audio. Una fábrica construye una instancia ya configurada; solo una instancia que completó validación, mapeo y reset puede pasar a ser la máquina activa.

Una ROM no determina por sí sola variante de CPU, reloj, memoria, E/S ni circuito de audio. La compatibilidad necesita un perfil de máquina explícito.

## Límites del paquete

El contenedor podrá ser una carpeta o un archivo empaquetado, decisión pendiente. Conceptualmente incluiría:

```text
ExampleFirmware/
├── manifest.json
├── roms/
│   ├── program-bank0.rom
│   └── program-bank1.rom
├── presets/            # opcional; datos, nunca ROM comercial
└── assets/             # opcional; referencias UI limitadas
```

La extensión del manifiesto, empaquetado, instalación y distribución no están decididos. Evitar incrustar ROMs en presets de usuario. Las reglas de ROM del proyecto prevalecen: **no distribuir, no pedir, no enlazar ROMs comerciales**.

## Manifest conceptual

Un manifiesto versionado podría describir:

- Formato e ID de versión del formato, e ID/nombre/versión del firmware.
- Autor, descripción, licencia y procedencia cuando se conozcan y se puedan publicar.
- Tipo de máquina/perfil y variante exacta de CPU; reloj y características disponibles.
- Memoria de programa y, solo si el perfil lo requiere, bancos/regiones de datos y RAM.
- Cada imagen: ID, ruta relativa, región, banco cuando aplique, offset dentro de región, tamaño y hash esperado.
- Entradas y salidas públicas del perfil: ID, tipo, ancho, dirección lógica y etiqueta para presentación.
- Parámetros configurables, tipo, rango, valor predeterminado y si el host puede automatizarlos.
- Capacidades de circuit bending permitidas y metadatos declarativos de UI limitados.

Ejemplo mínimo **ilustrativo**, incompleto y no normativo:

```json
{
  "format": "bdmcs48se-firmware",
  "formatVersion": 1,
  "id": "example.wavetable",
  "name": "Example Wavetable",
  "version": "0.1.0",
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
      "bankCount": 2
    }
  },
  "images": [
    {
      "id": "program0",
      "file": "roms/program-bank0.rom",
      "region": "program",
      "bank": 0,
      "offset": 0,
      "size": 2048,
      "sha256": "<64 hex characters>"
    }
  ],
  "inputs": [
    { "id": "note", "type": "latch", "index": 0, "bits": 8 }
  ],
  "outputs": [
    { "id": "audio", "type": "mono_stream" }
  ],
  "parameters": [
    {
      "id": "engine_clock",
      "type": "float",
      "minimum": 0.25,
      "maximum": 4.0,
      "initial": 1.0,
      "hostAutomatable": true
    }
  ]
}
```

`default` en JSON Schema es una anotación descriptiva: un validador no tiene por qué insertar el valor cuando falta. El loader debe aplicar defaults de forma explícita o exigirlos. El chequeo de `format: uri` también puede depender de la configuración del validador. La versión inicial deberá fijar dialecto y comportamiento del validator elegido; aún no se elige parser ni dependencia.

## Carga y validación

Flujo conceptual, completamente fuera del callback de audio:

1. Elegir el paquete y resolver su raíz autorizada.
2. Leer y parsear el manifest con límites de bytes/profundidad.
3. Validar versión del formato y estructura del documento.
4. Resolver solo rutas relativas dentro de la raíz del paquete; rechazar rutas absolutas, segmentos de traversal (`..`) y enlaces simbólicos que escapen del paquete.
5. Aplicar límites de cantidad/tamaño de archivos antes de reservar memoria.
6. Cargar cada imagen, validar tamaño y hash declarado.
7. Validar IDs únicos, referencias, rangos, solapamientos, bancos, overflow de `offset + size`, regiones y tipo de máquina soportado.
8. Validar compatibilidad semántica entre perfil, variante CPU, capacidades de banco, mapa de memoria, entradas y salidas. El JSON Schema no reemplaza estas reglas.
9. Construir candidato nuevo mediante una fábrica registrada para ese perfil.
10. Instalar ROM/regiones, conectar E/S y salida, aplicar configuración inicial y resetear.
11. Publicar el candidato solo cuando esté listo; un error de carga debe conservar el firmware activo si la política de producto así lo decide.
12. Informar el resultado y nuevo estado al host/UI por el canal de estado, no desde audio thread.

Hash SHA-256 compara bytes con una referencia; **no prueba autoría, procedencia, licencia ni permiso de redistribución**. El manifiesto debe evitar afirmaciones legales inferidas del hash.

## Nota de precisión: memoria MCS-48 y bancos

No generalizar el resumen «PC de 11 bits y banco activo sumado a la dirección» como el comportamiento completo del procesador.

Para la familia MCS-48 clásica con bank switching:

- El espacio de programa direccionable alcanza 12 bits (hasta 4 KiB), descrito como dos bancos de 2 KiB.
- La ejecución secuencial avanza dentro del banco de 2 KiB actual.
- `SEL MB0` / `SEL MB1` cambia un flip-flop de selección; la selección se aplica a la dirección alta al ejecutar `JMP` o `CALL`, no como una suma física para cada lectura secuencial.
- `CALL`/`RET` e interrupciones tienen reglas propias: `RET` restaura la dirección guardada, pero no el flip-flop de selección; los vectores de interrupción usan banco 0 y se debe respetar el comportamiento de `RETR`.
- Los modelos sin ROM interna (p. ej. 8035/8039) requieren fetch de programa externo; las capacidades internas varían entre modelos.
- UPI-41/42 son derivados con cambios de instrucciones: en esos modelos `E5`/`F5` corresponden a funciones UPI y no deben interpretarse automáticamente como `SEL MB0`/`SEL MB1`.

La implementación del perfil/CPU necesitará tests de conformidad por variante, ROM interna/external fetch, `JMP`/`CALL`, `RET`/`RETR` e interrupciones antes de afirmar soporte de bancos.

Fuentes consultadas:

- [Intel MCS-48 User’s Manual — bank switching](https://auto.manualsonline.com/manuals/mfg/intel/mcs48.html?p=61)
- [Intel MCS-48 User’s Manual — external fetch and EA](https://auto.manualsonline.com/manuals/mfg/intel/mcs48.html?p=60)
- [MAME MCS-48 implementation and variant table](https://github.com/mamedev/mame/blob/master/src/devices/cpu/mcs48/mcs48.cpp)
- [MAME MCS-48 device header](https://raw.githubusercontent.com/mamedev/mame/master/src/devices/cpu/mcs48/mcs48.h)

MAME es fuente para revisar su modelo, no sustituto del manual ni garantía de que cada variante/clon comparte exactamente todas las propiedades.

## Máquina: estados del ciclo de vida

Estados conceptuales del **proceso de carga/instancia activa**, no necesariamente API pública final:

| Estado | Significado | Acciones conceptualmente válidas |
|---|---|---|
| `Unloaded` | No hay máquina activa | solicitar carga |
| `Validating` | manifest e imágenes están siendo inspeccionados fuera de audio | cancelar/esperar; no hay órdenes musicales al candidato |
| `Constructing` | se crea un candidato nuevo fuera de audio | cancelar o esperar |
| `Mapping` | se instalan regiones, puertos, latches y salida en candidato | esperar |
| `Ready` | candidato válido y reseteado, aún no ejecutándose | iniciar, dejar preparado o descartar |
| `Running` | instancia activa procesa audio | comandos, parámetros, reset seguro, solicitar reemplazo |
| `Swapping` | reemplazo preparado y pendiente de activación en frontera segura | mantener estado coherente y no bloquear audio |
| `Faulted` | la instancia reportó fallo recuperable/no recuperable | reset, descargar o intentar reemplazo según política |

Transiciones esperadas:

```text
Unloaded → Validating → Constructing → Mapping → Ready → Running
Running → Swapping → Running (candidate activado)
Running → Faulted → Ready/Unloaded (tras reset/recovery)
Ready/Running → Unloaded (descarga ordenada)
```

No confundir error de un **candidato nuevo** con fallo de la **máquina ya activa**. Un manifiesto inválido no debería tirar abajo el audio que está sonando, salvo decisión explícita.

## Hot swap: ownership real-time

Requisito de producto: validar, leer archivos, parsear manifest, reservar/configurar memoria y construir la máquina nueva fuera del audio thread. Cambiar de máquina solo en un punto definido (normalmente límite de bloque o evento de host coordinado), sin bloquear la reproducción.

`std::atomic<std::shared_ptr<MachineInstance>>` hace atómica la publicación de ownership compartido, pero no garantiza operación lock-free, wait-free, ausencia de refcount/destrucción en audio ni destrucción en hilo de control. La documentación del estándar deja `is_always_lock_free` como implementation-defined y permite que decrementos/eliminación ocurran después de la actualización atómica. Por tanto, no tratar ese patrón como automáticamente apto para tiempo real.

Dirección de diseño bajo la regla estricta de no allocations/locks/destrucción en callback:

1. El hilo de control prepara el candidato completo y publica un handle mediante handoff acotado preasignado.
2. El audio thread recoge el reemplazo en la frontera permitida y usa una referencia no propietaria cuyo lifetime está garantizado por protocolo externo.
3. La instancia anterior se pasa a una cola de retiro preasignada; la destruye un hilo no real-time cuando se confirme que ya no se usa.
4. Si no hay capacidad de retiro, se aplaza la sustitución; el callback no espera, no asigna y no deja caer el último owner.
5. Al apagar el plugin, detener/reclamar el callback antes de destruir máquina activa y retiradas.

Alternativas (epochs/quiescent-state, buffers de slots con handshake u otras) requieren análisis de ciclo de vida y pruebas. Un puntero atómico sin reclamación no evita use-after-free. El mecanismo exacto queda abierto a elección e implementación posterior.

Fuentes:

- [C++ draft — atomic smart pointers](https://eel.is/c++draft/util.smartptr.atomic)
- [PortAudio — callback guidance](https://portaudio.com/docs/v19-doxydocs/writing_a_callback.html)
- [Steinberg VST3 — audio processor call sequence](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical+Documentation/Workflow+Diagrams/Audio+Processor+Call+Sequence.html)

## Parámetros y automatización VST3

No prometer que cambiar el manifest durante una sesión puede añadir/quitar parámetros VST3 host-visible sin consecuencias. IDs y significados de parámetros deberían ser estables para automatización y restauración. Los parámetros del firmware podrían limitarse a un superset fijo o residir en controles/preset del plugin; el contrato público final queda por decidir.

Si cambiar el conjunto de parámetros es requisito, investigar cambios de versión, remapeo de IDs, comportamiento de hosts compatibles y migración de proyectos. No confundir notificar cambio de títulos/valores con cambiar libremente la lista de parámetros. Probar en cada host soportado antes de prometerlo.

Fuentes:

- [Steinberg — Parameters and Automation](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical+Documentation/Parameters+Automation/Index.html)
- [Steinberg — IRemapParamID](https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical+Documentation/Change+History/3.7.11/IRemapParamID.html)
- [JUCE — AudioProcessorValueTreeState](https://docs.juce.com/master/classjuce_1_1AudioProcessorValueTreeState.html)

## Diagnóstico y exposición a la UI

La UI recibe solo un snapshot de diagnóstico copiado/estable: estado de carga, ROM/profile IDs, fallos, conteos de samples/ciclos, underruns, nivel de salida y, si se acuerda, un subconjunto de registros como PC lógico, acumulador, PSW, banco/flip-flop, puertos e instrucciones ejecutadas.

- No publicar punteros ni referencias mutables a CPU, RAM, latch o estructuras internas.
- No mostrar todos los registros por defecto si no son musicales/diagnósticamente útiles; hacer selectivo el nivel de debug.
- El productor y lector del snapshot necesitan protocolo de propiedad libre de data races. Doble/triple buffer por sí solo no prueba seguridad si el escritor puede reutilizar el slot que está leyendo la UI.
- UI consulta a frecuencia moderada desde timer y notifica mediante eventos; audio thread nunca llama a WebView.
- El esquema `ui.debug` puede sugerir visibilidad, pero no concede capacidades de edición de memoria o ejecución arbitraria.

## Decisiones todavía abiertas

- Nombre/extensión del paquete, empaquetado, firma y distribución.
- Si el manifest v1 incluirá parámetros/MIDI/UI o solo perfil + imágenes + puertos.
- Dialecto JSON Schema, parser, límites y política de defaults.
- Qué perfiles son datos declarativos y cuáles requieren adaptador compilado/registrado.
- Variantes CPU soportadas y cómo se expresa capacidad efectiva de memoria/bancos.
- Si los datos/wavetables comparten espacio de programa o residen en regiones propias del perfil.
- Semántica del ciclo de vida, cancelación, mensajes de error y preservación de la máquina activa al fallar un reemplazo.
- Protocolo de hot-swap/reclamación de objetos y cola de retirements acotada.
- Qué diagnóstico de CPU exponer y a qué frecuencia.
- Si parámetros VST3 serán un conjunto fijo o cómo tratar controles específicos por firmware.
