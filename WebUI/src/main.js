// main.js - Frontend de BDMCS48SE.
//
// Arquitectura de Interfaz Canónica de la suite ABDSynths:
// - SSOT de parámetros: paramStore.js desacopla los controles DOM del DSP APVTS.
// - Contrato de parámetros: contracts/abd_mcs48_parameter_spec.json.
// - Componentes canónicos de ABDShared: Knob, StepMatrix, MiniWaveScope, VuMeter,
//   mountFitStage, createSynthNavBar, createAboutDialog, createDrawer.
// - Keybed canónico: @abdsynths/midi-keyb.
// - La UI no guarda su propia verdad: pide el estado a C++ al arrancar, recibe
//   actualizaciones a 20 Hz (bdmcs48-state) y envía intenciones a través del puente nativo.

import './style.css';

// Componentes canónicos de ABDShared
import {
  mountFitStage,
  createSynthNavBar,
  createAboutDialog,
  createDrawer,
  midiNoteToName
} from '@abdsynths/shared/components';
import { createMcs48EnginePanel } from '@abdsynths/shared/components/synth-engines';

// Keybed estándar de ABDSynths
import { createKeyboard } from '@abdsynths/midi-keyb';
import '@abdsynths/midi-keyb/keyboard.css';

// SSOT y Contrato de Parámetros
import { paramStore } from './contracts/paramStore.js';
import { PARAM_LOOKUP } from './contracts/registry.gen.js';

// Gestor de temas canónicos de ABDShared
import { themeManager, THEMES } from './utils/themeManager.js';

// i18n reactivo canónico de 2 capas de ABDShared
import {
  initI18n,
  getLocale,
  setLocale,
  onLocaleChange,
  t,
  applyI18n
} from './i18n/index.js';

(() => {
  const backend = (window.__JUCE__ && window.__JUCE__.backend) || (() => {
    const listeners = new Map();
    return {
      addEventListener: (evt, cb) => {
        if (!listeners.has(evt)) listeners.set(evt, []);
        listeners.get(evt).push(cb);
      },
      emitEvent: (evt, data) => {
        if (evt === '__juce__invoke') {
          setTimeout(() => {
            const handlers = listeners.get('__juce__complete') || [];
            handlers.forEach((h) =>
              h({
                promiseId: data.resultId,
                result: data.name === 'getState'
                  ? JSON.stringify({
                      protocol: 1,
                      firmware: { label: 'MCS-48 Dev Mode' },
                      gain: -6,
                      release: 80,
                      rows: [],
                      presets: ['Init', 'Bass 8048', 'Bell Lead'],
                      firmwares: ['demo-mono-voice.pkg']
                    })
                  : null
              })
            );
          }, 0);
        }
      }
    };
  })();

  // Inicializar motor reactivo i18n y temas canónicos de ABDShared
  initI18n();
  themeManager.init();

  // --- Puente con C++ -----------------------------------------------------
  let nextPromiseId = 1;
  const pending = new Map();

  backend.addEventListener("__juce__complete", (payload) => {
    const resolve = pending.get(payload.promiseId);
    if (resolve) {
      pending.delete(payload.promiseId);
      resolve(payload.result);
    }
  });

  function callNative(name, ...args) {
    const promiseId = nextPromiseId++;
    return new Promise((resolve) => {
      pending.set(promiseId, resolve);
      backend.emitEvent("__juce__invoke", { name, params: args, resultId: promiseId });
    });
  }

  // Vincular el store de parámetros al puente C++
  paramStore.setBridge({ callNative });

  // Protocolo con C++ (bdmcs48::plugin::kUiProtocolVersion)
  const PROTOCOL_VERSION = 1;
  let lastState = null;

  const el = (id) => document.getElementById(id);

  function showFault(message) {
    const fault = el("fault");
    if (fault) {
      fault.textContent = message;
      fault.hidden = !message;
    }
  }

  // --- FitStage: Escalado Proporcional Sin Barras de Scroll ---------------
  try {
    const appEl = el("app");
    if (appEl) {
      mountFitStage(appEl, {
        width: 980,
        height: 680,
        onlyShrink: true
      });
    }
  } catch (err) {
    console.warn("FitStage init error:", err);
  }

  // --- Montaje Canónico del Escenario MCS-48 desde ABDShared (100% DRY) ---
  let mcs48Panel = null;
  const synthStageEl = el('synthStage');
  if (synthStageEl) {
    const nativeBridge = {
      getDescriptor: () => ({ typeId: 'mcs48_synth', displayName: 'MCS-48 Micro-Synth' }),
      getParam: (key) => paramStore.get(key),
      setParam: (key, val) => {
        paramStore.recordStateSnapshot(`Set ${key}`);
        paramStore.setParam(key, val, { recordUndo: false });
      },
      beginGesture: (key) => paramStore.beginGesture(key),
      endGesture: (key) => paramStore.endGesture(key),
      listFactoryPresets: () => [],
      loadFactoryPreset: (idx) => callNative('loadPreset', idx),
      setMatrixRow: (rowIndex, payload) => {
        paramStore.recordStateSnapshot(`Step Row ${rowIndex + 1}`);
        callNative('setRow', rowIndex, JSON.stringify(payload)).then((error) => {
          if (error) showFault(String(error));
        });
      },
      getState: () => {
        const eng = lastState?.engine || {};
        const p = {};
        for (const [k] of PARAM_LOOKUP.entries()) {
          p[k] = paramStore.get(k);
        }
        return {
          ...eng,
          ...p,
          rows: lastState?.rows || [],
          tuningA4: paramStore.get('tuningA4') || 440.0,
        };
      },
      onStateChange: (cb) => {
        paramStore.onStateRestore = (state) => {
          renderState(state);
          cb();
        };
        return () => { paramStore.onStateRestore = null; };
      },
      onRequestBankBrowser: () => {
        if (firmwareDrawer) {
          if (typeof refreshWorkspaceData === 'function') refreshWorkspaceData();
          firmwareDrawer.open();
        }
      }
    };

    mcs48Panel = createMcs48EnginePanel(synthStageEl, nativeBridge, {
      locale: getLocale(),
      hideHeader: true, // El sub-header de presets lo gestiona synthToolbar
    });

    onLocaleChange((locale) => {
      mcs48Panel?.setLocale(locale);
    });
  }

  function randomizeMatrix() {
    paramStore.recordStateSnapshot('Randomize Matrix');
    mcs48Panel?.randomizeMatrix();
  }

  function resetMatrix() {
    paramStore.recordStateSnapshot('Reset Matrix');
    mcs48Panel?.resetMatrix();
  }

  // --- Drawer Lateral para Gestión de Firmware & ROMs --------------------
  let firmwareDrawer = null;
  let refreshWorkspaceData = null;
  try {
    firmwareDrawer = createDrawer({
      id: 'firmware-drawer',
      title: t('drawer.title'),
      badge: 'MCS-48'
    });

    const body = firmwareDrawer.body;
    body.innerHTML = `
      <div class="drawer-section">
        <h3 class="drawer-section-title" data-i18n="drawer.wsTitle">${t('drawer.wsTitle')}</h3>
        <p class="drawer-hint" data-i18n="drawer.wsHint">${t('drawer.wsHint')}</p>
        <div class="drawer-field-row">
          <input type="text" id="dw-ws-path" class="drawer-input" readonly placeholder="${t('drawer.wsPlaceholder')}" data-i18n-placeholder="drawer.wsPlaceholder">
          <button type="button" id="dw-ws-browse" class="btn btn-sm" data-i18n="drawer.wsBrowseBtn">${t('drawer.wsBrowseBtn')}</button>
        </div>
      </div>

      <div class="drawer-section">
        <h3 class="drawer-section-title" data-i18n="drawer.fwTitle">${t('drawer.fwTitle')}</h3>
        <p class="drawer-hint" data-i18n="drawer.fwHint">${t('drawer.fwHint')}</p>
        <div class="drawer-field-row">
          <select id="dw-fw-select" class="drawer-select"></select>
          <button type="button" id="dw-fw-load" class="btn btn-sm btn-primary" data-i18n="drawer.fwLoadBtn">${t('drawer.fwLoadBtn')}</button>
        </div>
      </div>

      <div class="drawer-section">
        <h3 class="drawer-section-title" data-i18n="drawer.romConverterTitle">${t('drawer.romConverterTitle')}</h3>
        <p class="drawer-hint" data-i18n="drawer.romConverterHint">${t('drawer.romConverterHint')}</p>
        
        <div class="drawer-field">
          <label for="dw-rom-path" class="drawer-label" data-i18n="drawer.romFileLabel">${t('drawer.romFileLabel')}</label>
          <div class="drawer-field-row">
            <input type="text" id="dw-rom-path" class="drawer-input" placeholder="${t('drawer.romPlaceholder')}" data-i18n-placeholder="drawer.romPlaceholder">
            <button type="button" id="dw-rom-browse" class="btn btn-sm" data-i18n="drawer.romBrowseBtn">${t('drawer.romBrowseBtn')}</button>
          </div>
        </div>

        <div class="drawer-field">
          <label for="dw-rom-select-existing" class="drawer-label" data-i18n="drawer.romExistingLabel">${t('drawer.romExistingLabel')}</label>
          <select id="dw-rom-select-existing" class="drawer-select">
            <option value="">${t('drawer.romSelectPrompt')}</option>
          </select>
        </div>

        <div class="drawer-field-grid">
          <div class="drawer-field">
            <label for="dw-pkg-id" class="drawer-label" data-i18n="drawer.pkgIdLabel">${t('drawer.pkgIdLabel')}</label>
            <input type="text" id="dw-pkg-id" class="drawer-input" placeholder="${t('drawer.pkgIdPlaceholder')}" data-i18n-placeholder="drawer.pkgIdPlaceholder">
          </div>
          <div class="drawer-field">
            <label for="dw-pkg-name" class="drawer-label" data-i18n="drawer.pkgNameLabel">${t('drawer.pkgNameLabel')}</label>
            <input type="text" id="dw-pkg-name" class="drawer-input" placeholder="${t('drawer.pkgNamePlaceholder')}" data-i18n-placeholder="drawer.pkgNamePlaceholder">
          </div>
        </div>

        <div class="drawer-field-grid">
          <div class="drawer-field">
            <label for="dw-pkg-cpu" class="drawer-label" data-i18n="drawer.pkgCpuLabel">${t('drawer.pkgCpuLabel')}</label>
            <select id="dw-pkg-cpu" class="drawer-select">
              <option value="8048">Intel 8048 (1K ROM / 64B RAM)</option>
              <option value="8049" selected>Intel 8049 (2K ROM / 128B RAM)</option>
              <option value="8748">Intel 8748 (1K EPROM)</option>
              <option value="8035">Intel 8035 (ROM externa)</option>
            </select>
          </div>
          <div class="drawer-field">
            <label for="dw-pkg-clock" class="drawer-label" data-i18n="drawer.pkgClockLabel">${t('drawer.pkgClockLabel')}</label>
            <input type="number" id="dw-pkg-clock" class="drawer-input" value="6000000" step="100000">
          </div>
        </div>

        <div class="drawer-field">
          <label for="dw-pkg-author" class="drawer-label" data-i18n="drawer.pkgAuthorLabel">${t('drawer.pkgAuthorLabel')}</label>
          <input type="text" id="dw-pkg-author" class="drawer-input" placeholder="${t('drawer.pkgAuthorPlaceholder')}" data-i18n-placeholder="drawer.pkgAuthorPlaceholder">
        </div>

        <div class="drawer-actions">
          <button type="button" id="dw-pkg-generate" class="btn btn-primary btn-block" data-i18n="drawer.pkgGenerateBtn">${t('drawer.pkgGenerateBtn')}</button>
        </div>

        <div id="dw-status" class="drawer-status" hidden></div>
      </div>
    `;

    // Conectar eventos del Drawer
    const wsBrowseBtn = el('dw-ws-browse');
    if (wsBrowseBtn) {
      wsBrowseBtn.addEventListener('click', async () => {
        try {
          const res = await callNative('browseWorkspaceFolder');
          if (res !== 'cancelado') {
            const data = typeof res === 'string' ? JSON.parse(res) : res;
            if (el('dw-ws-path')) el('dw-ws-path').value = data.path || '';
            refreshWorkspaceData();
          }
        } catch (e) {
          console.error('Error browsing workspace:', e);
        }
      });
    }

    const fwLoadBtn = el('dw-fw-load');
    if (fwLoadBtn) {
      fwLoadBtn.addEventListener('click', () => {
        const selected = el('dw-fw-select')?.value;
        if (!selected) return;
        callNative('loadFirmware', selected).then((err) => {
          if (err) {
            showDrawerStatus(String(err), true);
          } else {
            showDrawerStatus(t('drawer.msgFirmwareLoaded', { name: selected }), false);
            callNative('getState').then((json) => renderState(JSON.parse(json)));
          }
        });
      });
    }

    const romBrowseBtn = el('dw-rom-browse');
    if (romBrowseBtn) {
      romBrowseBtn.addEventListener('click', async () => {
        try {
          const res = await callNative('pickRomFile');
          if (res !== 'cancelado') {
            const data = typeof res === 'string' ? JSON.parse(res) : res;
            if (data.path) {
              if (el('dw-rom-path')) el('dw-rom-path').value = data.path;
              const baseName = data.name.replace(/\.[^/.]+$/, '');
              if (el('dw-pkg-name') && !el('dw-pkg-name').value) el('dw-pkg-name').value = baseName;
              if (el('dw-pkg-id') && !el('dw-pkg-id').value) {
                el('dw-pkg-id').value = baseName.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
              }
            }
          }
        } catch (e) {
          showDrawerStatus(String(e), true);
        }
      });
    }

    const romSelectExisting = el('dw-rom-select-existing');
    if (romSelectExisting) {
      romSelectExisting.addEventListener('change', () => {
        const val = romSelectExisting.value;
        if (!val) return;
        if (el('dw-rom-path')) el('dw-rom-path').value = val;
        const baseName = val.split(/[\\/]/).pop().replace(/\.[^/.]+$/, '');
        if (el('dw-pkg-name') && !el('dw-pkg-name').value) el('dw-pkg-name').value = baseName;
        if (el('dw-pkg-id') && !el('dw-pkg-id').value) {
          el('dw-pkg-id').value = baseName.toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
        }
      });
    }

    const pkgGenBtn = el('dw-pkg-generate');
    if (pkgGenBtn) {
      pkgGenBtn.addEventListener('click', async () => {
        try {
          const romPath = el('dw-rom-path')?.value.trim();
          const packageId = el('dw-pkg-id')?.value.trim();
          const packageName = el('dw-pkg-name')?.value.trim();
          const cpuVariant = el('dw-pkg-cpu')?.value || '8049';
          const clockHz = parseInt(el('dw-pkg-clock')?.value || '6000000') || 6000000;
          const author = el('dw-pkg-author')?.value.trim() || 'BDMCS48SE User';

          if (!romPath) throw new Error(t('drawer.errSelectRom'));
          if (!packageId) throw new Error(t('drawer.errPackageId'));
          if (!packageName) throw new Error(t('drawer.errPackageName'));

          const params = {
            romPath,
            packageId,
            packageName,
            packageVersion: '1.0.0',
            author,
            description: `Paquete compilado desde ${packageName}`,
            license: 'CC0-1.0',
            cpuVariant,
            clockHz,
            profile: 'generic-mcs48',
            bankCount: 1,
            bankSizeBytes: cpuVariant === '8048' ? 1024 : 2048,
            addressBits: cpuVariant === '8048' ? 11 : 12
          };

          const res = await callNative('convertRomToPackage', JSON.stringify(params));
          if (typeof res === 'string' && res.length > 0 && res !== 'null') {
            throw new Error(res);
          }

          showDrawerStatus(t('drawer.msgPackageSuccess'), false);
          refreshWorkspaceData();
          callNative('getState').then((json) => renderState(JSON.parse(json)));
        } catch (err) {
          showDrawerStatus(String(err.message || err), true);
        }
      });
    }

    function showDrawerStatus(msg, isError) {
      const st = el('dw-status');
      if (!st) return;
      st.textContent = msg;
      st.className = `drawer-status ${isError ? 'drawer-status-error' : 'drawer-status-success'}`;
      st.hidden = !msg;
      if (!isError && msg) {
        setTimeout(() => { if (st) st.hidden = true; }, 4000);
      }
    }

    refreshWorkspaceData = async function() {
      try {
        const ws = await callNative('getWorkspaceFolder');
        const wsData = typeof ws === 'string' ? JSON.parse(ws) : ws;
        if (el('dw-ws-path')) el('dw-ws-path').value = wsData.path || '';

        const roms = await callNative('listRomFiles');
        const romData = typeof roms === 'string' ? JSON.parse(roms) : roms;
        const files = romData.files || [];
        const romSelect = el('dw-rom-select-existing');
        if (romSelect) {
          romSelect.innerHTML = files.length
            ? `<option value="">${t('drawer.romSelectPrompt')}</option>`
            : `<option value="">${t('drawer.romSelectEmpty')}</option>`;
          files.forEach((f) => {
            const opt = document.createElement('option');
            opt.value = f;
            opt.textContent = f.split(/[\\/]/).pop();
            romSelect.appendChild(opt);
          });
        }
      } catch (e) {
        console.warn('Error refreshing workspace data:', e);
      }
    };

    // Botón en panel de telemetría para abrir el Drawer
    const openDrawerBtn = el('openFirmwareDrawerBtn');
    if (openDrawerBtn) {
      openDrawerBtn.addEventListener('click', () => {
        refreshWorkspaceData();
        firmwareDrawer.open();
      });
    }

  } catch (err) {
    console.warn('Drawer init failed:', err);
  }

  // --- Keybed Estándar ABDSynths (@abdsynths/midi-keyb) -------------------
  let keyboardInstance = null;
  function setupKeyboard() {
    keyboardInstance = createKeyboard({
      containerId: "piano-keyboard",
      wheelPitchId: null,
      wheelModId: null,
      octUpId: "oct-up",
      octDownId: "oct-down",
      ledUpId: "led-up",
      ledDownId: "led-down",
      sustainBtnId: "sustain-btn",
      panicBtnId: "panic-btn",
      onNoteOn: (note, velocity) => callNative("noteOn", note, velocity),
      onNoteOff: (note) => callNative("noteOff", note),
      onPanic: () => callNative("allNotesOff"),
      onCollapseChange: (collapsed) => {
        const kbdFooter = el("synth-keyboard-footer");
        if (kbdFooter) {
          kbdFooter.classList.toggle("collapsed", collapsed);
        }
      },
      config: {
        numOctaves: 2,
        startNote: 48, // C3..B4
        fixedOctaves: true,
        maxOctaveShift: 2,
        velocitySource: "fixed",
        fixedVelocity: 0.9,
        velocityCurve: "linear",
        enableQwerty: true,
        showQwertyLabels: true,
        enableTouch: true,
        enableAccessibility: true,
        enableHold: false,
        enableRetrig: false,
        enableCollapse: true,
      },
    });

    const kbdFooter = el("synth-keyboard-footer");
    if (kbdFooter) {
      kbdFooter.addEventListener("click", (e) => {
        if (e.target.closest('.kbd-collapse-btn')) return;
        if (kbdFooter.classList.contains("collapsed")) {
          if (keyboardInstance && typeof keyboardInstance.expand === "function") {
            keyboardInstance.expand();
          } else {
            kbdFooter.classList.remove("collapsed");
          }
        }
      });
    }
  }
  setupKeyboard();

  // Hook para refrescar la interfaz completa tras deshacer/rehacer en paramStore
  paramStore.onStateRestore = (state) => {
    renderState(state);
  };

  // --- Barra de Navegación Universal SynthNavBar --------------------------
  let navBarInstance = null;
  try {
    const navContainer = el('navBarContainer');
    if (navContainer) {
      navBarInstance = createSynthNavBar({
        container: navContainer,
        brand: { title: 'BDMCS48SE', subtitle: 'MCS-48 Instrument' },
        locale: getLocale(),
        centerSlot: null,
        menus: [
          {
            name: 'File',
            nameEs: 'Archivo',
            nameEn: 'File',
            i18nKey: 'navbar.menuFile',
            items: [
              { label: 'Save Sound Preset...', labelEs: 'Guardar Preset...', labelEn: 'Save Sound Preset...', action: 'savePreset', shortcut: 'Ctrl+S' },
              { label: 'Load Sound Preset...', labelEs: 'Cargar Preset...', labelEn: 'Load Sound Preset...', action: 'loadPreset', shortcut: 'Ctrl+O' },
              { separator: true },
              { label: 'Export SysEx Patch...', labelEs: 'Exportar Patch SysEx...', labelEn: 'Export SysEx Patch...', action: 'exportSysEx' },
              { label: 'Import SysEx Patch...', labelEs: 'Importar Patch SysEx...', labelEn: 'Import SysEx Patch...', action: 'importSysEx' },
              { separator: true },
              { label: 'Reset Machine (Reset CPU)', labelEs: 'Reiniciar Máquina (Reset CPU)', labelEn: 'Reset Machine (Reset CPU)', action: 'resetMachine' },
              { label: 'Panic (All Notes Off)', labelEs: 'Panic (Apagar Todas las Notas)', labelEn: 'Panic (All Notes Off)', action: 'panic', shortcut: 'Ctrl+Q' },
            ]
          },
          {
            name: 'Edit',
            nameEs: 'Edición',
            nameEn: 'Edit',
            i18nKey: 'navbar.menuEdit',
            items: [
              { label: 'Undo', labelEs: 'Deshacer (Undo)', labelEn: 'Undo', action: 'undo', shortcut: 'Ctrl+Z' },
              { label: 'Redo', labelEs: 'Rehacer (Redo)', labelEn: 'Redo', action: 'redo', shortcut: 'Ctrl+Y' },
              { separator: true },
              { label: 'Randomize Command Matrix', labelEs: 'Sembrar Mutación en Matriz', labelEn: 'Randomize Command Matrix', action: 'randomizeMatrix' },
              { label: 'Reset Command Matrix', labelEs: 'Reiniciar Matriz de Comandos', labelEn: 'Reset Command Matrix', action: 'resetMatrix' },
              { separator: true },
              { label: 'Firmware & ROMs Manager...', labelEs: 'Gestor de Firmwares & ROMs...', labelEn: 'Firmware & ROMs Manager...', action: 'openFirmwareDrawer' },
              { label: 'Reload Firmwares', labelEs: 'Recargar Firmwares', labelEn: 'Reload Firmwares', action: 'refreshFirmwares' },
              { separator: true },
              { label: 'Working Directory (Workspace)...', labelEs: 'Carpeta de Trabajo (Workspace)...', labelEn: 'Working Directory (Workspace)...', action: 'openWorkspaceSettings' },
            ]
          },
          {
            name: 'View',
            nameEs: 'Ver',
            nameEn: 'View',
            i18nKey: 'navbar.menuView',
            items: [
              { label: 'Toggle Virtual Keyboard', labelEs: 'Alternar Teclado Virtual', labelEn: 'Toggle Virtual Keyboard', action: 'toggleKeyboard', shortcut: 'Ctrl+K' },
              { separator: true },
              {
                label: 'Theme',
                labelEs: 'Tema',
                labelEn: 'Theme',
                submenu: THEMES.map((theme) => ({
                  label: theme.label,
                  labelEs: theme.labelEs,
                  labelEn: theme.label,
                  action: 'setTheme',
                  value: theme.id,
                }))
              },
              {
                label: 'Color Mode',
                labelEs: 'Modo de Color',
                labelEn: 'Color Mode',
                submenu: [
                  { label: 'Dark Mode', labelEs: 'Modo Oscuro', labelEn: 'Dark Mode', action: 'setColorMode', value: 'dark' },
                  { label: 'Light Mode', labelEs: 'Modo Claro', labelEn: 'Light Mode', action: 'setColorMode', value: 'light' },
                ]
              },
              { label: 'Toggle Dark / Light Mode', labelEs: 'Alternar Modo Oscuro / Claro', labelEn: 'Toggle Dark / Light Mode', action: 'toggleMode' },
              { separator: true },
              {
                label: 'Language',
                labelEs: 'Idioma',
                labelEn: 'Language',
                submenu: [
                  { label: 'English (EN)', labelEs: 'English (EN)', labelEn: 'English (EN)', action: 'setLocale', value: 'en' },
                  { label: 'Español (ES)', labelEs: 'Español (ES)', labelEn: 'Español (ES)', action: 'setLocale', value: 'es' },
                ]
              }
            ]
          },
          {
            name: 'Help',
            nameEs: 'Ayuda',
            nameEn: 'Help',
            i18nKey: 'navbar.menuHelp',
            items: [
              { label: 'About BDMCS48SE...', labelEs: 'Acerca de BDMCS48SE...', labelEn: 'About BDMCS48SE...', action: 'about' },
            ]
          }
        ],
        activeValues: (action) => {
          if (action === 'setLocale') return getLocale();
          if (action === 'setTheme') return themeManager.getCurrentTheme();
          if (action === 'setColorMode') return themeManager.getMode();
          return undefined;
        },
        actions: {
          setLocale: (lang) => {
            setLocale(lang);
          },
          undo: async () => {
            const ok = await paramStore.undo();
            if (!ok) {
              showFault(t('messages.noUndo'));
              setTimeout(() => showFault(''), 1500);
            }
          },
          redo: async () => {
            const ok = await paramStore.redo();
            if (!ok) {
              showFault(t('messages.noRedo'));
              setTimeout(() => showFault(''), 1500);
            }
          },
          savePreset: () => {
            const name = el('preset-name')?.value.trim() || '';
            callNative('savePreset', name).then((error) => {
              showFault(error ? String(error) : '');
              return callNative('getState');
            }).then((json) => renderState(JSON.parse(json)));
          },
          loadPreset: () => {
            const name = el('preset-list')?.value || '';
            if (!name) return;
            callNative('loadPreset', name).then((error) => {
              showFault(error ? String(error) : '');
              const input = el('preset-name');
              if (input) input.value = name;
              return callNative('getState');
            }).then((json) => renderState(JSON.parse(json)));
          },
          exportSysEx: async () => {
            try {
              const hex = await callNative('exportSysExDump');
              if (!hex || hex.startsWith('error') || hex.startsWith('Error')) {
                showFault(hex || 'Error exporting SysEx');
                return;
              }
              const presetName = el('preset-name')?.value.trim() || 'patch';
              downloadSysExFile(hex, `${presetName.toLowerCase().replace(/[^a-z0-9_-]+/g, '_')}.syx`);
              showFault(t('messages.sysexExportSuccess') || 'SysEx patch exported');
              setTimeout(() => showFault(''), 2500);
            } catch (e) {
              showFault(String(e));
            }
          },
          importSysEx: () => {
            triggerSysExImport();
          },
          resetMachine: () => {
            callNative('resetMachine').then(() => callNative('getState')).then((json) => renderState(JSON.parse(json)));
          },
          panic: () => {
            callNative('allNotesOff').catch(() => {});
            callNative('getState').then((json) => renderState(JSON.parse(json)));
          },
          randomizeMatrix: () => {
            randomizeMatrix();
          },
          resetMatrix: () => {
            resetMatrix();
          },
          setTheme: (themeId) => {
            themeManager.setTheme(themeId);
          },
          setColorMode: (mode) => {
            themeManager.setMode(mode);
          },
          toggleMode: () => {
            themeManager.toggleMode();
          },
          toggleTheme: () => {
            themeManager.toggleMode();
          },
          toggleKeyboard: () => {
            if (keyboardInstance && typeof keyboardInstance.toggleCollapse === 'function') {
              keyboardInstance.toggleCollapse();
            } else {
              const kbd = el('synth-keyboard-footer');
              if (kbd) kbd.classList.toggle('collapsed');
            }
          },
          about: () => {
            try {
              const dlg = createAboutDialog({
                name: t('navbar.aboutTitle'),
                version: '0.2.0',
                build: 'calibrated-release',
                description: t('navbar.aboutDescription')
              });
              dlg.open();
            } catch (e) {
              console.warn('About dialog failed:', e);
            }
          },
          openWorkspaceSettings: async () => {
            try {
              const res = await callNative('browseWorkspaceFolder');
              if (res !== 'cancelado') {
                callNative('getState').then((json) => renderState(JSON.parse(json)));
              }
            } catch (e) {
              showFault(String(e));
            }
          },
          refreshFirmwares: () => {
            callNative('getState').then((json) => renderState(JSON.parse(json))).catch((e) => showFault(String(e)));
          },
          openFirmwareDrawer: () => {
            if (firmwareDrawer) {
              if (typeof refreshWorkspaceData === 'function') refreshWorkspaceData();
              firmwareDrawer.open();
            }
          }
        },
        rightSlot: `
          <div class="navbar-engine-status">
            <span class="engine-badge-led"></span>
            <button id="firmwareStatusPill" type="button" class="navbar-pill" title="${t('navbar.firmwareManager')}" style="cursor: pointer; background: transparent;">MCS-48</button>
          </div>
        `
      });

      // Vincular eventos de la barra de herramientas y presets
      setTimeout(() => {
        const saveBtn = el('preset-save');
        const loadBtn = el('preset-load');
        const presetListEl = el('preset-list');
        const fwPill = el('firmwareStatusPill');
        const tbFwBadge = el('toolbarFwBadge');
        const panicBtn = el('toolbarPanicBtn');
        const resetCpuBtn = el('toolbarResetCpuBtn');

        if (saveBtn) {
          saveBtn.addEventListener('click', () => {
            const name = el('preset-name')?.value.trim() || '';
            callNative('savePreset', name).then((error) => {
              showFault(error ? String(error) : '');
              return callNative('getState');
            }).then((json) => renderState(JSON.parse(json)));
          });
        }
        if (loadBtn) {
          loadBtn.addEventListener('click', () => {
            const name = el('preset-list')?.value || '';
            if (!name) return;
            callNative('loadPreset', name).then((error) => {
              showFault(error ? String(error) : '');
              const input = el('preset-name');
              if (input) input.value = name;
              return callNative('getState');
            }).then((json) => renderState(JSON.parse(json)));
          });
        }
        if (presetListEl) {
          presetListEl.addEventListener('change', () => {
            const sel = presetListEl.value;
            if (sel && el('preset-name')) {
              el('preset-name').value = sel;
            }
          });
        }

        const openFwDrawer = () => {
          if (firmwareDrawer) {
            if (typeof refreshWorkspaceData === 'function') refreshWorkspaceData();
            firmwareDrawer.open();
          }
        };

        if (fwPill) fwPill.addEventListener('click', openFwDrawer);
        if (tbFwBadge) tbFwBadge.addEventListener('click', openFwDrawer);

        if (panicBtn) {
          panicBtn.addEventListener('click', () => {
            callNative('allNotesOff').catch(() => {});
            callNative('getState').then((json) => renderState(JSON.parse(json)));
          });
        }

        if (resetCpuBtn) {
          resetCpuBtn.addEventListener('click', () => {
            callNative('resetMachine')
              .then(() => callNative('getState'))
              .then((json) => renderState(JSON.parse(json)));
          });
        }
      }, 0);
    }
  } catch (e) {
    console.warn('SynthNavBar init failed:', e);
  }

  // --- Renderizado de Estado (20 Hz Snapshot) -----------------------------
  let renderedPresets = '';
  let renderedFirmwares = '';

  function renderPresets(names) {
    const list = el("preset-list");
    if (!list) return;
    const signature = `${names.join("\u0000")}#${getLocale()}`;
    if (signature === renderedPresets) return;
    renderedPresets = signature;
    const selected = list.value;
    list.innerHTML = "";
    const empty = document.createElement("option");
    empty.value = "";
    empty.textContent = names.length ? t('messages.presetEmptyList') : t('messages.presetNoList');
    list.appendChild(empty);
    for (const name of names) {
      const option = document.createElement("option");
      option.value = name;
      option.textContent = name;
      list.appendChild(option);
    }
    if (names.includes(selected)) list.value = selected;
  }

  function renderFirmwares(names) {
    const list = el("firmware-list");
    const drawerList = el("dw-fw-select");
    const signature = `${names.join("\u0000")}#${getLocale()}`;
    if (signature === renderedFirmwares) return;
    renderedFirmwares = signature;

    const populate = (selEl, promptText) => {
      if (!selEl) return;
      const prev = selEl.value;
      selEl.innerHTML = "";
      const empty = document.createElement("option");
      empty.value = "";
      empty.textContent = names.length ? (promptText || t('messages.fwEmptyList')) : t('messages.fwNoList');
      selEl.appendChild(empty);
      for (const name of names) {
        const opt = document.createElement("option");
        opt.value = name;
        opt.textContent = name;
        selEl.appendChild(opt);
      }
      if (names.includes(prev)) selEl.value = prev;
    };

    populate(list, t('messages.fwEmptyList'));
    populate(drawerList, t('messages.fwEmptyList'));
  }

  function renderState(state) {
    if (!state) return;
    if (state.protocol !== PROTOCOL_VERSION) {
      showFault(
        t('messages.contractMismatch', { backend: state.protocol ?? "?", expected: PROTOCOL_VERSION })
      );
      return;
    }

    lastState = state;

    // Sincronizar parámetros en paramStore
    paramStore.syncFromState(state);

    // Firmware Pill & Toolbar Name
    const fwPill = el("firmwareStatusPill");
    if (fwPill && state.firmware) {
      fwPill.textContent = "MCS-48";
      if (state.firmware.hash) fwPill.title = `#${state.firmware.hash}`;
    }

    const tbFwName = el("toolbarFwName");
    if (tbFwName && state.firmware) {
      tbFwName.textContent = state.firmware.label || "MCS-48 Enhanced Mono Voice";
      if (state.firmware.hash) tbFwName.title = `#${state.firmware.hash}`;
    }

    // Sincronizar Escenario Canónico desde ABDShared (Scope, VU, Matriz, LCD, Controles)
    if (mcs48Panel) {
      mcs48Panel.syncFromBridge();
    }

    renderPresets(state.presets || []);
    renderFirmwares(state.firmwares || []);
  }

  // --- Recepción de Eventos Push del Backend JUCE -------------------------
  backend.addEventListener("bdmcs48-state", (json) => {
    try {
      const data = typeof json === "string" ? JSON.parse(json) : json;
      renderState(data);
    } catch (e) {
      console.warn("Error parsing bdmcs48-state:", e);
    }
  });

  // Atajos de Teclado Globales (DAW / Desktop)
  window.addEventListener('keydown', (e) => {
    const tag = document.activeElement ? document.activeElement.tagName.toLowerCase() : '';
    const isInput = tag === 'input' || tag === 'textarea' || tag === 'select';

    // Deshacer: Ctrl+Z (Cmd+Z)
    if ((e.ctrlKey || e.metaKey) && !e.shiftKey && e.key.toLowerCase() === 'z') {
      if (isInput) return;
      e.preventDefault();
      paramStore.undo();
      return;
    }

    // Rehacer: Ctrl+Y o Ctrl+Shift+Z (Cmd+Shift+Z)
    if (((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'y') ||
        ((e.ctrlKey || e.metaKey) && e.shiftKey && e.key.toLowerCase() === 'z')) {
      if (isInput) return;
      e.preventDefault();
      paramStore.redo();
      return;
    }

    // Alternar Teclado Virtual: Ctrl+K
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'k') {
      if (isInput) return;
      e.preventDefault();
      if (keyboardInstance && typeof keyboardInstance.toggleCollapse === 'function') {
        keyboardInstance.toggleCollapse();
      } else {
        const kbd = el('synth-keyboard-footer');
        if (kbd) kbd.classList.toggle('collapsed');
      }
      return;
    }

    // Guardar Preset: Ctrl+S
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 's') {
      e.preventDefault();
      const saveBtn = el('preset-save');
      if (saveBtn) saveBtn.click();
      return;
    }

    // Panic: Ctrl+Q
    if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'q') {
      e.preventDefault();
      callNative('allNotesOff').catch(() => {});
      callNative('getState').then((json) => renderState(JSON.parse(json)));
      return;
    }
  });

  // --- Utilidades SysEx y Decodificadores ROM (.bin / .hex) -------------
  function parseIntelHex(text) {
    const lines = text.split(/\r?\n/);
    const memory = new Map();
    let maxAddr = 0;
    let baseAddr = 0;

    for (const rawLine of lines) {
      const line = rawLine.trim();
      if (!line.startsWith(':')) continue;
      const byteCount = parseInt(line.substring(1, 3), 16);
      const address = parseInt(line.substring(3, 7), 16);
      const recordType = parseInt(line.substring(7, 9), 16);

      if (recordType === 0) {
        const fullAddr = baseAddr + address;
        for (let i = 0; i < byteCount; i++) {
          const byte = parseInt(line.substring(9 + i * 2, 11 + i * 2), 16);
          memory.set(fullAddr + i, byte);
          if (fullAddr + i > maxAddr) maxAddr = fullAddr + i;
        }
      } else if (recordType === 1) {
        break;
      } else if (recordType === 2) {
        baseAddr = parseInt(line.substring(9, 13), 16) << 4;
      } else if (recordType === 4) {
        baseAddr = parseInt(line.substring(9, 13), 16) << 16;
      }
    }

    if (memory.size === 0) return new Uint8Array(0);

    let size = 1024;
    if (maxAddr >= 2048) size = 4096;
    else if (maxAddr >= 1024) size = 2048;

    const result = new Uint8Array(size);
    result.fill(0);
    for (const [addr, val] of memory.entries()) {
      if (addr < size) {
        result[addr] = val;
      }
    }
    return result;
  }

  function uint8ArrayToBase64(bytes) {
    let binary = '';
    const len = bytes.byteLength;
    for (let i = 0; i < len; i++) {
      binary += String.fromCharCode(bytes[i]);
    }
    return btoa(binary);
  }

  function downloadSysExFile(hexStr, filename = 'patch.syx') {
    const clean = hexStr.replace(/[^0-9A-Fa-f]/g, '');
    const bytes = new Uint8Array(clean.length / 2);
    for (let i = 0; i < clean.length; i += 2) {
      bytes[i / 2] = parseInt(clean.substring(i, i + 2), 16);
    }
    const blob = new Blob([bytes], { type: 'application/octet-stream' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  function triggerSysExImport() {
    const input = document.createElement('input');
    input.type = 'file';
    input.accept = '.syx,.bin';
    input.onchange = async (e) => {
      const file = e.target.files && e.target.files[0];
      if (!file) return;
      try {
        const buffer = await file.arrayBuffer();
        const bytes = new Uint8Array(buffer);
        let hex = '';
        for (let i = 0; i < bytes.length; i++) {
          hex += bytes[i].toString(16).padStart(2, '0');
        }
        const err = await callNative('importSysExDump', hex);
        if (err && typeof err === 'string' && err.length > 0) {
          showFault(String(err));
        } else {
          showFault(t('messages.sysexImportSuccess') || 'SysEx patch importado correctamente');
          setTimeout(() => showFault(''), 2500);
          callNative('getState').then((json) => renderState(JSON.parse(json)));
        }
      } catch (err) {
        showFault(String(err));
      }
    };
    input.click();
  }

  async function handleDroppedFile(file) {
    if (!file) return;
    const lowerName = file.name.toLowerCase();

    try {
      if (lowerName.endsWith('.syx')) {
        const buffer = await file.arrayBuffer();
        const bytes = new Uint8Array(buffer);
        let hex = '';
        for (let i = 0; i < bytes.length; i++) {
          hex += bytes[i].toString(16).padStart(2, '0');
        }
        const err = await callNative('importSysExDump', hex);
        if (err && typeof err === 'string' && err.length > 0) {
          showFault(String(err));
        } else {
          showFault(t('messages.sysexImportSuccess') || 'SysEx patch importado');
          setTimeout(() => showFault(''), 2500);
          callNative('getState').then((json) => renderState(JSON.parse(json)));
        }
      } else if (lowerName.endsWith('.hex')) {
        const text = await file.text();
        const bytes = parseIntelHex(text);
        if (bytes.length === 0) {
          throw new Error('Archivo Intel HEX inválido o sin datos');
        }
        const b64 = uint8ArrayToBase64(bytes);
        const err = await callNative('loadRawRomData', file.name, b64);
        if (err && typeof err === 'string' && err.length > 0) {
          showFault(String(err));
        } else {
          showFault(t('messages.dropRomSuccess', { name: file.name }));
          setTimeout(() => showFault(''), 3000);
          callNative('getState').then((json) => renderState(JSON.parse(json)));
        }
      } else if (lowerName.endsWith('.bin') || lowerName.endsWith('.rom')) {
        const buffer = await file.arrayBuffer();
        const bytes = new Uint8Array(buffer);
        const b64 = uint8ArrayToBase64(bytes);
        const err = await callNative('loadRawRomData', file.name, b64);
        if (err && typeof err === 'string' && err.length > 0) {
          showFault(String(err));
        } else {
          showFault(t('messages.dropRomSuccess', { name: file.name }));
          setTimeout(() => showFault(''), 3000);
          callNative('getState').then((json) => renderState(JSON.parse(json)));
        }
      } else {
        showFault(`Extensión no soportada (.bin, .rom, .hex, .syx): ${file.name}`);
        setTimeout(() => showFault(''), 3000);
      }
    } catch (err) {
      showFault(String(err.message || err));
      setTimeout(() => showFault(''), 3500);
    }
  }

  // Soporte Drag-and-Drop Global en la Ventana
  const appContainer = el('app') || document.body;
  window.addEventListener('dragover', (e) => {
    e.preventDefault();
    if (appContainer) appContainer.classList.add('drag-active');
  });

  window.addEventListener('dragleave', (e) => {
    e.preventDefault();
    if (e.relatedTarget === null && appContainer) {
      appContainer.classList.remove('drag-active');
    }
  });

  window.addEventListener('drop', (e) => {
    e.preventDefault();
    if (appContainer) appContainer.classList.remove('drag-active');
    if (e.dataTransfer && e.dataTransfer.files && e.dataTransfer.files.length > 0) {
      handleDroppedFile(e.dataTransfer.files[0]);
    }
  });

  // Suscripción a cambios de idioma
  onLocaleChange((locale) => {
    applyI18n();
    if (navBarInstance && typeof navBarInstance.setLocale === 'function') {
      navBarInstance.setLocale(locale);
    }
    if (firmwareDrawer && typeof firmwareDrawer.setHeader === 'function') {
      firmwareDrawer.setHeader({ title: t('drawer.title'), badge: 'MCS-48' });
    }
    renderedPresets = '';
    renderedFirmwares = '';
    if (lastState) {
      renderPresets(lastState.presets || []);
      renderFirmwares(lastState.firmwares || []);
    }
  });

  // Aplicar traducción inicial
  applyI18n();

  // Solicitar estado inicial al arrancar
  callNative("getState")
    .then((json) => renderState(JSON.parse(json)))
    .catch((error) => showFault(String(error)));
})();
