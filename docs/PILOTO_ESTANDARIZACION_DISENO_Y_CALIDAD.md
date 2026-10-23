# BDMCS48SE — Plan y Checklist del Piloto de Estandarización de Diseño y Calidad

> **Objetivo:** Ejecutar la estandarización completa de la interfaz y experiencia de usuario de **BDMCS48SE** (`ABDMCS48SE`) como banco de pruebas y piloto de referencia del Sistema de Diseño Visual de **ABDSynths** ("Técnica Refinada"), consolidando las directrices de `ABDShared/Docs/ui-ux/DESIGN_SYSTEM_GUIDE.md`.
> 
> **Fecha:** 2026-10-24  
> **Alcance:** `WebUI/index.html`, `WebUI/src/main.js`, `WebUI/src/main.css`, `WebUI/src/style.css`, empaquetado Vite y verificación con `PluginSmokeTest`.

---

## 1. Diagnóstico del Estado Actual en `ABDMCS48SE/WebUI`

Una auditoría del código frontend actual revela las siguientes deficiencias y duplicidades a resolver:

1. **Doble Cabecera Redundante:**
   - Coexisten `<div id="navBarContainer"></div>` (donde se monta `SynthNavBar`) y un `<header class="topbar">` casero con inputs de preset, botones Guardar/Cargar con SVGs inline dispersos y botón Panic.
   - **Solución:** Eliminar por completo `<header class="topbar">` e integrar toda la gestión de presets, pánico y badges en `SynthNavBar` (`centerSlot` y `menus`).

2. **Disposición Espacial del Lienzo (`main`):**
   - Actualmente las tres secciones (`params`, `matrix`, `status`) fluyen en bloque vertical simple o flex desordenado.
   - **Solución:** Reestructurar en dos columnas operativas según la arquitectura de 4 áreas canónicas:
     - **Columna Izquierda:** Controles del motor de síntesis (`Gain`, `Release`) y la **Matriz de Comandos** (8 filas interactivas).
     - **Columna Derecha:** Visualización de telemetría y estado de la CPU MCS-48 en formato LCD retro, más la torre de medición master `VuMeter`.

3. **Panel de Estado de Microprocesador:**
   - Se muestra mediante una lista cruda `<dl class="status-grid">`.
   - **Solución:** Aplicar el estilo de display fosforescente retro (`LCD_GUIDE.md`) con fondo oscuro de cuarzo/fósforo, tipografía monospace `tabular-nums` y resaltado cromático de registros (`PC`, `A`, `PSW`, `Bank`, `Cycles`) y contadores de buffer (`Overruns`, `Underruns`).

4. **Monitorización Master (`VuMeter`):**
   - El contenedor `#vuMeterMaster` está montado pero orientado en horizontal de forma rudimentaria bajo los faders.
   - **Solución:** Integrar un `VuMeter` vertical estéreo calibrado con escala en dBFS (-60 a 0 dBFS), pico numérico monoespaciado y LED de clipping a -0.5 dBFS.

5. **Teclado Virtual y Contrato WebView2:**
   - El cajón `#virtualKeyboardDrawerMount` existe pero debe garantizar el cumplimiento estricto del contrato de altura fija (`height: 155px; min-height: 155px;`) en CSS para blindar Microsoft WebView2 contra colapsos de teclas.
   - Atajo de teclado global `Ctrl+K` para desplegar/ocultar.

---

## 2. Checklist Operativo y Secuencial de Implementación

### Fase 1: Limpieza Estructural del DOM (`WebUI/index.html`)
- [ ] Eliminar el elemento `<header class="topbar">` y su contenido inline redundante.
- [ ] Reestructurar `<main>` con el layout canónico de dos columnas:
  ```html
  <main class="synth-canvas">
    <!-- Columna Izquierda: Motor de Síntesis y Matriz de Comandos -->
    <div class="canvas-col-left">
      <section class="panel params" aria-labelledby="params-title">
        <h2 id="params-title">Controles del Motor</h2>
        <!-- Sliders Gain y Release estilizados con tokens de controles -->
      </section>
      <section class="panel matrix" aria-labelledby="matrix-title">
        <h2 id="matrix-title">Matriz de Comandos (8 Pasos)</h2>
        <!-- Tabla de matriz con inputs, selects y estados -->
      </section>
    </div>

    <!-- Columna Derecha: Diagnóstico LCD y Medición Master -->
    <div class="canvas-col-right">
      <section class="panel lcd-panel" aria-labelledby="status-title">
        <h2 id="status-title">Telemetría MCS-48</h2>
        <div class="lcd-container">
          <!-- Grid fosforescente de registros de CPU -->
        </div>
      </section>
      <section class="panel master-panel" aria-labelledby="master-title">
        <h2 id="master-title">Master</h2>
        <div id="vuMeterMaster" class="vumeter-tower"></div>
      </section>
    </div>
  </main>
  ```
- [ ] Ubicar al pie del documento el punto de montaje del cajón:
  ```html
  <div id="virtualKeyboardDrawerMount"></div>
  ```

### Fase 2: Unificación de la Barra Superior (`SynthNavBar` en `WebUI/src/main.js`)
- [ ] Configurar `createSynthNavBar` para que gobierne toda la barra superior:
  - **Identidad:** Título `BDMCS48SE`, subtítulo `MCS-48 Engine`, pill de versión `v0.1.0`.
  - **Slot Central (`centerSlot`):**
    - Contenedor `#presetBar` con selector desplegable de presets, campo de texto para renombrar el parche activo y botones vectoriales Guardar y Cargar.
  - **Menús Nativos Desktop:**
    - `File`: Panic (All Notes Off), Guardar Preset, Cargar Preset, Reset Machine.
    - `View`: Conmutar Tema (Oscuro / Claro), Conmutar Teclado Virtual (`Ctrl+K`).
    - `Help`: About BDMCS48SE... (con modal `createAboutDialog`).
  - **Slot Derecho (`rightSlot`):**
    - Botón de cambio de tema (`#themeToggleBtn`) y badge de estado de firmware (`#firmwareStatusPill`).

### Fase 3: Pantalla LCD Retro y Diagnóstico (`WebUI/src/main.css` y `src/main.js`)
- [ ] Diseñar el panel LCD retro con tokens:
  - Contenedor con borde biselado, fondo ámbar o verde fósforo retro (`background: #0f1810; border: 1px solid #1a3820;`).
  - Celdas para: `PC` (Program Counter en hex), `A` (Acumulador), `PSW` (Flags de estado), `Bank` (Banco de memoria 0/1), `Cycles` (Ciclos totales de CPU), `Overruns` y `Underruns`.
  - Tipografía digital / monoespaciada (`font-family: var(--font-mono); font-variant-numeric: tabular-nums;`).
  - Conectar `renderState(state)` para actualizar las celdas directamente a partir del snapshot recibido por el evento `bdmcs48-state`.

### Fase 4: Medición Master (`VuMeter`)
- [ ] Instanciar el componente `VuMeter` de `@abdsynths/shared/components`:
  - Modo vertical estéreo con dos canales.
  - Rango de calibración acústica de `-60 dBFS` a `0 dBFS`.
  - Conectar el nivel de motor en decibelios a partir del valor normalizado de telemetría emitido a 20 Hz por C++.

### Fase 5: Teclado Virtual y Contrato WebView2
- [ ] En `src/main.css`, asegurar el contrato de altura para `#piano-keyboard-drawer`:
  ```css
  #piano-keyboard-drawer {
    height: 155px !important;
    min-height: 155px !important;
    max-height: 155px !important;
    flex-shrink: 0;
  }
  ```
- [ ] En `src/main.js`, verificar la instanciación de `createKeyboard`:
  - `startNote: 48` (C3), `numOctaves: 2` (o 3 según ancho de ventana).
  - `fixedOctaves: true`.
  - `enableHold: false` y `enableRetrig: false` (estrictamente desactivados).
  - Conectar atajo de teclado `Ctrl+K` para abrir/cerrar el cajón retráctil.

### Fase 6: Auditoría Anti-Slop (Iconos y Modales)
- [ ] Eliminar emojis de toda la aplicación.
- [ ] Emplear exclusivamente iconos vectoriales SVG monocromáticos con `currentColor` y trazo de 2px según `ICONS_GUIDE.md`.
- [ ] Asegurar que el diálogo About utilice `createAboutDialog` y el gestor de modales unificado de `ABDShared` (cero doble modal).

### Fase 7: Validación de Build y Smoke Tests
- [ ] Ejecutar el bundler de WebUI (`node Scripts/build_webui.js` en `ABDMCS48SE`).
- [ ] Verificar que los archivos empaquetados en `WebUI/dist` existan (`index.html`, `assets/index.js`, `assets/index.css`).
- [ ] Indicar al usuario la compilación y ejecución de `PluginSmokeTest.exe` para certificar que el bundle embebido binario y las funciones nativas responden correctamente.
