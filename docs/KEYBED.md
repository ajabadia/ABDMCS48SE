# Teclado compartido (ABDShared)

El keybed del editor no vive en este repo: la página monta
`@abdsynths/midi-keyb`, el componente de
`ABDShared/Code/MidiKeyboard`, que es el mismo que usan ABDMS2000, ABDEep y
ABDCZ101. Aquí solo hay cableado; el componente y su CSS son de ABDShared.

Versión con la que se verificó este cableado: **0.2.1**.

## Cómo se resuelve

Como este repo no es un workspace de ABDSynths, la dependencia no entra por
`npm install` sino por alias de Vite en `WebUI/vite.config.js`:

| Import | Destino |
|---|---|
| `@abdsynths/midi-keyb` | `<shared>/Code/MidiKeyboard/src/keyboard.js` |
| `@abdsynths/midi-keyb/keyboard.css` | `<shared>/Code/MidiKeyboard/src/keyboard.css` |
| `@abdsynths/shared/components/wheel.js` | `<shared>/Components/wheel.js` |
| `@abdsynths/shared/styles/…` | `<shared>/Assets/styles/…` |

Las dos últimas no son opcionales: `keyboard.js` importa la rueda compartida y
`keyboard.css` hace `@import` de `wheels.css` y `kbd-buttons.css`.

`<shared>` es `ABD_SHARED_DIR` si está definida (relativa a la **raíz del
repo**, no a `WebUI/`) o, por defecto, el checkout hermano `../ABDShared`. Si
no aparece `Code/MidiKeyboard/src/keyboard.js`, la configuración de Vite falla
con la ruta resuelta en el mensaje y el build no genera `dist`.

## Cómo se monta

- **HTML** (`WebUI/index.html`): `<div id="piano-keyboard"></div>` dentro de la
  sección `panel keyboard`, al final del documento.
- **JS** (`WebUI/src/main.js`): `setupKeyboard()` llama a `createKeyboard()` con
  `containerId`, `onNoteOn`, `onNoteOff` y `onPanic`, y esos callbacks van a las
  funciones nativas `noteOn` / `noteOff` / `allNotesOff`. C++ sigue siendo la
  autoridad de la nota; el keybed no guarda estado musical propio.

| Opción | Valor | Motivo |
|---|---|---|
| `numOctaves` / `startNote` | 2 / 48 | C3..B4, el rango que tenía el keybed anterior |
| `fixedOctaves` | `true` | el host fija el rango; no se reacomoda por ancho |
| `maxOctaveShift` | `0` | sin botones de octava en esta UI |
| `velocitySource` | `fixed` (0.9) | el motor no lee velocidad del puntero |
| `enableQwerty` | `true` | el componente ignora el teclado mientras se escribe en un input |
| `enableAccessibility` | `true` | teclas `role=button` con `aria-label` |
| `enableHold` / `enableRetrig` | `false` | sobre la CPU emulada dejan notas sonando sin pulsación |

Sin ids de ruedas, el componente no monta pitch/mod (el motor no las tiene).
Sí genera sus botones **ALL OFF** (→ `allNotesOff`) y **SUSTAIN**, que sostiene
notas retrasando el `noteOff`; el motor no tiene pedal propio.

**Contrato de altura**: `#piano-keyboard` lleva `height: 155px` en
`WebUI/src/main.css`. WebView2 no resuelve `height: 100%` a través de cadenas
flex y las teclas colapsan a una fracción de píxel si falta. La franja es
`flex-shrink: 0` y el scroll vive en `main`, así que el teclado queda fijo al
fondo.

## Cómo actualizarlo

El consumidor no fija un tag/commit: Vite lee el checkout local de ABDShared y
el job `webui` de CI clona su rama por defecto. Para poder repetir una
verificación, anota el commit de ABDShared (`git -C ../ABDShared rev-parse
HEAD`) junto con el resultado.

1. Antes de actualizar, revisa `git -C ../ABDShared status --short --branch`.
   Si el checkout está modificado o tiene commits propios, no hagas `pull`,
   `checkout` ni `reset`: usa un checkout limpio separado o deja la actualización
   para después.
2. En un checkout limpio: `git -C ../ABDShared fetch origin`; revisa
   `git -C ../ABDShared log --oneline HEAD..origin/main` y el diff de
   `Code/MidiKeyboard/`, `Components/wheel.js` y `Assets/styles/`. Actualiza
   solo tras revisar los cambios, con
   `git -C ../ABDShared merge --ff-only origin/main`.
3. Lee `Code/MidiKeyboard/CHANGELOG.md` y `package.json`: busca cambios de API,
   valores por defecto y nuevas rutas importadas. Si cambia una ruta, ajusta el
   alias correspondiente en `WebUI/vite.config.js`.
4. `node Scripts/build_webui.js`; comprueba que `WebUI/dist` contiene HTML, JS
   y CSS. Después recompila el plugin configurado con
   `-DABDMCS48SE_BUILD_PLUGIN=ON` y ejecuta CTest en ese directorio de build;
   `plugin_smoke` comprueba que el documento monta `#piano-keyboard` y que el
   CSS embebido trae `kbd-keys-wrapper`.
5. Abre el Standalone y toca: las teclas deben sonar y el estado (`Nota`)
   moverse. El árbol de accesibilidad del WebView debe incluir `Virtual piano
   keyboard` y `Note C3 (MIDI 48)`.

Señales de que la actualización rompió algo:

| Síntoma | Causa probable |
|---|---|
| `Failed to resolve import "@abdsynths/…"` | el paquete empezó a importar otra ruta compartida: añadir alias |
| Error de configuración con la ruta de `keyboard.js` | ABDShared no está donde se busca o movió `Code/MidiKeyboard` |
| `plugin_smoke` falla en «index.html monta el keybed compartido» o «el CSS incluye el keybed compartido de ABDShared» | el componente dejó de montarse o de empaquetarse |
| Teclas invisibles o negras | se perdió la altura definida del contenedor |

El job `webui` de CI está configurado para clonar `ajabadia/ABDShared` en
`.ci/ABDShared`, compilar la página y comprobar el bundle. Con eso, un cambio
en el keybed compartido que rompa esta UI se ve en el PR y no en el primer
build del plugin en Windows.
