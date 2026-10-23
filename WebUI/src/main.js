// main.js - Frontend de BDMCS48SE.
//
// La UI no guarda su propia verdad: pide el estado a C++ al arrancar, recibe
// actualizaciones por eventos y envia intenciones (notas, filas, presets).
// C++ valida y aplica; lo que se pinta siempre viene del backend.

// Vite empaqueta este import junto al JavaScript y emite assets/index.css,
// que el index.html del dist referencia solo. El provider nativo no conoce
// rutas de fuentes: sirve lo que salio del build.
import './style.css';

// Componentes compartidos de ABDShared
import { createSynthNavBar, AboutDialog, createAboutDialog, VuMeter } from '@abdsynths/shared/components';
import { createDrawer } from '@abdsynths/shared/components';

// Keybed estandar de ABDSynths: el mismo componente que monta ABDMS2000.
// Vite lo resuelve contra el checkout hermano de ABDShared (ver vite.config.js).
import { createKeyboard } from '@abdsynths/midi-keyb';
import '@abdsynths/midi-keyb/keyboard.css';

(() => {
  const backend = window.__JUCE__.backend;

  // --- Puente con C++ -----------------------------------------------------
  // JUCE expone funciones nativas por el protocolo de promesas interno.
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

  // Debe coincidir con bdmcs48::plugin::kUiProtocolVersion. Si no coincide,
  // la pagina no pinta nada: es mejor un aviso que controles que mienten.
  const PROTOCOL_VERSION = 1;

  const MUTATIONS = ["none", "increment", "decrement", "random", "wrap"];
  const TARGETS = [
    { id: "engine_clock", label: "engine_clock" },
    { id: "latch_write", label: "latch_write" },
  ];
  const ROW_COUNT = 8;
  const CLOCK_MIN = 0.0625;
  const CLOCK_MAX = 8;
  const LATCH_MAX = 255;

  const rowElements = [];
  let lastState = null;

  const el = (id) => document.getElementById(id);

  function showFault(message) {
    const fault = el("fault");
    fault.textContent = message;
    fault.hidden = !message;
  }

  // --- Matriz -------------------------------------------------------------
  function targetRange(target) {
    return target === "latch_write"
      ? { min: 0, max: LATCH_MAX, step: 1, decimals: 0 }
      : { min: CLOCK_MIN, max: CLOCK_MAX, step: 0.0625, decimals: 3 };
  }

  function formatValue(target, value) {
    return target === "latch_write"
      ? String(Math.round(value))
      : `${Number(value).toFixed(3)}x`;
  }

  function buildRow(index) {
    const tr = document.createElement("tr");

    const enabledCell = document.createElement("td");
    const enabled = document.createElement("input");
    enabled.type = "checkbox";
    enabledCell.appendChild(enabled);

    const noteCell = document.createElement("td");
    const note = document.createElement("input");
    note.type = "number";
    note.min = "0";
    note.max = "127";
    noteCell.appendChild(note);

    const targetCell = document.createElement("td");
    const target = document.createElement("select");
    for (const option of TARGETS) {
      const node = document.createElement("option");
      node.value = option.id;
      node.textContent = option.label;
      target.appendChild(node);
    }
    targetCell.appendChild(target);

    const valueCell = document.createElement("td");
    valueCell.className = "value";
    const valueRow = document.createElement("div");
    valueRow.className = "range-row";
    const value = document.createElement("input");
    value.type = "range";
    const valueOut = document.createElement("output");
    valueRow.append(value, valueOut);
    valueCell.appendChild(valueRow);

    const probabilityCell = document.createElement("td");
    const probability = document.createElement("input");
    probability.type = "range";
    probability.min = "0";
    probability.max = "1";
    probability.step = "0.01";
    probabilityCell.appendChild(probability);

    const delayCell = document.createElement("td");
    const delay = document.createElement("input");
    delay.type = "number";
    delay.min = "0";
    delay.max = "5000";
    delay.step = "1";
    delayCell.appendChild(delay);

    const mutationCell = document.createElement("td");
    const mutation = document.createElement("select");
    for (const id of MUTATIONS) {
      const node = document.createElement("option");
      node.value = id;
      node.textContent = id;
      mutation.appendChild(node);
    }
    mutationCell.appendChild(mutation);

    const amountCell = document.createElement("td");
    const amount = document.createElement("input");
    amount.type = "number";
    amount.min = "0";
    amount.step = "0.001";
    amountCell.appendChild(amount);

    tr.append(enabledCell, noteCell, targetCell, valueCell, probabilityCell, delayCell,
              mutationCell, amountCell);
    el("matrix-body").appendChild(tr);

    const refs = { tr, enabled, note, target, value, valueOut, probability, delay, mutation, amount };
    rowElements.push(refs);

    const send = () => {
      const range = targetRange(target.value);
      value.min = String(range.min);
      value.max = String(range.max);
      value.step = String(range.step);
      const payload = {
        enabled: enabled.checked,
        note: Number(note.value),
        target: target.value,
        value: Number(value.value),
        probability: Number(probability.value),
        delayMs: Number(delay.value),
        mutation: mutation.value,
        mutationAmount: Number(amount.value),
      };
      valueOut.textContent = formatValue(payload.target, payload.value);
      callNative("setRow", index, JSON.stringify(payload)).then((error) => {
        if (error) showFault(String(error));
      });
    };

    enabled.addEventListener("change", send);
    note.addEventListener("change", send);
    target.addEventListener("change", () => {
      const range = targetRange(target.value);
      value.value = String(target.value === "latch_write" ? 18 : 1);
      amount.value = String(target.value === "latch_write" ? 1 : 0.0625);
      value.min = String(range.min);
      value.max = String(range.max);
      value.step = String(range.step);
      send();
    });
    for (const control of [value, probability, amount]) {
      control.addEventListener("input", send);
    }
    for (const control of [delay]) {
      control.addEventListener("change", send);
    }
    mutation.addEventListener("change", send);

    return refs;
  }

  function renderRows(rows) {
    rows.forEach((row, index) => {
      const refs = rowElements[index];
      if (!refs) return;
      const active = document.activeElement;

      refs.tr.classList.toggle("off", !row.enabled);
      if (active !== refs.enabled) refs.enabled.checked = Boolean(row.enabled);
      if (active !== refs.note) refs.note.value = String(row.note);
      if (active !== refs.target) refs.target.value = row.target;

      const range = targetRange(row.target);
      if (refs.value.min !== String(range.min)) refs.value.min = String(range.min);
      if (refs.value.max !== String(range.max)) refs.value.max = String(range.max);
      if (refs.value.step !== String(range.step)) refs.value.step = String(range.step);
      if (active !== refs.value) refs.value.value = String(row.value);
      refs.valueOut.textContent = formatValue(row.target, row.value);

      if (active !== refs.probability) refs.probability.value = String(row.probability);
      if (active !== refs.delay) refs.delay.value = String(Math.round(row.delayMs));
      if (active !== refs.mutation) refs.mutation.value = row.mutation;
      if (active !== refs.amount) refs.amount.value = String(row.mutationAmount);
    });
  }

  // --- Teclado ------------------------------------------------------------
  // Keybed compartido de ABDShared (@abdsynths/midi-keyb), el mismo que monta
  // ABDMS2000. La pagina solo aporta callbacks; C++ sigue siendo la autoridad
  // de la nota: el keybed no guarda estado musical propio.
  function setupKeyboard() {
    createKeyboard({
      containerId: "piano-keyboard",
      onNoteOn: (note, velocity) => callNative("noteOn", note, velocity),
      onNoteOff: (note) => callNative("noteOff", note),
      onPanic: () => callNative("allNotesOff"),
      config: {
        numOctaves: 2,
        startNote: 48, // C3..B4: el rango que tenia el teclado anterior
        fixedOctaves: true, // el host fija el rango; no se reacomoda por ancho
        maxOctaveShift: 0,
        velocitySource: "fixed",
        fixedVelocity: 0.9,
        velocityCurve: "linear",
        enableQwerty: true,
        showQwertyLabels: true,
        enableTouch: true,
        // Teclas con role=button y aria-label ("Note C3 (MIDI 48)"): sin esto
        // el keybed es una fila de divs que un lector de pantalla no ve.
        enableAccessibility: true,
        // Hold/latch y retrig quedan apagados: sobre la CPU emulada dejan notas
        // sonando sin pulsacion. El componente los activa por defecto.
        enableHold: false,
        enableRetrig: false,
      },
    });
  }

  // --- Parametros ---------------------------------------------------------
  const gainToNormalized = (db) => (db + 24) / 30;
  const releaseToNormalized = (ms) => (ms - 10) / 490;

  function bindParameters() {
    const gain = el("gain");
    const release = el("release");

    gain.addEventListener("input", () => {
      el("gain-value").textContent = `${Number(gain.value).toFixed(1)} dB`;
      callNative("setParameter", "gain", gainToNormalized(Number(gain.value)));
    });
    release.addEventListener("input", () => {
      el("release-value").textContent = `${Math.round(Number(release.value))} ms`;
      callNative("setParameter", "release", releaseToNormalized(Number(release.value)));
    });
  }

  // --- Estado -------------------------------------------------------------
  function renderState(state) {
    if (state.protocol !== PROTOCOL_VERSION) {
      showFault(
        `Contrato incompatible: el backend usa la version ${state.protocol ?? "?"} ` +
        `y esta pagina espera la ${PROTOCOL_VERSION}.`
      );
      return;
    }

    lastState = state;

    el("firmware").textContent = state.firmware.label || "sin firmware";
    el("firmware-hash").textContent = state.firmware.hash ? `#${state.firmware.hash}` : "";

    if (document.activeElement !== el("gain")) {
      el("gain").value = String(state.gain);
      el("gain-value").textContent = `${Number(state.gain).toFixed(1)} dB`;
    }
    if (document.activeElement !== el("release")) {
      el("release").value = String(Math.round(state.release));
      el("release-value").textContent = `${Math.round(state.release)} ms`;
    }

    renderRows(state.rows || []);

    const engine = state.engine || {};
    const hex = (value) => `0x${Number(value || 0).toString(16).padStart(3, "0")}`;
    el("st-pc").textContent = hex(engine.pc);
    el("st-a").textContent = hex(engine.a);
    el("st-psw").textContent = hex(engine.psw);
    el("st-bank").textContent = engine.bank === undefined ? "–" : String(engine.bank);
    el("st-cycles").textContent = Number(engine.cycles || 0).toLocaleString("es-ES");
    el("st-note").textContent = engine.note >= 0 ? String(engine.note) : "–";
    el("st-level").textContent = `${(100 * Number(engine.level || 0)).toFixed(0)} %`;
    el("st-overruns").textContent = String(engine.droppedCommands ?? 0);
    el("st-underruns").textContent = String(engine.underruns ?? 0);
    el("st-command").textContent =
      engine.lastCommandId >= 0 ? `${engine.lastCommandId} = ${engine.lastCommandValue}` : "–";

    if (engine.faulted) {
      showFault(engine.faultMessage || "CPU detenida");
    }

    renderPresets(state.presets || []);
  }

  let renderedPresets = "";
  function renderPresets(names) {
    const list = el("preset-list");
    const signature = names.join("\u0000");
    if (signature === renderedPresets) return;
    renderedPresets = signature;
    const selected = list.value;
    list.innerHTML = "";
    const empty = document.createElement("option");
    empty.value = "";
    empty.textContent = names.length ? "— elegir —" : "— sin presets —";
    list.appendChild(empty);
    for (const name of names) {
      const option = document.createElement("option");
      option.value = name;
      option.textContent = name;
      list.appendChild(option);
    }
    if (names.includes(selected)) list.value = selected;
  }

  // --- Presets y transporte ----------------------------------------------
  function bindTransport() {
    el("preset-save").addEventListener("click", () => {
      const name = el("preset-name").value.trim();
      callNative("savePreset", name).then((error) => {
        showFault(error ? String(error) : "");
        return callNative("getState");
      }).then((json) => renderState(JSON.parse(json)));
    });

    el("preset-load").addEventListener("click", () => {
      const name = el("preset-list").value;
      if (!name) return;
      callNative("loadPreset", name).then((error) => {
        showFault(error ? String(error) : "");
        el("preset-name").value = name;
        return callNative("getState");
      }).then((json) => renderState(JSON.parse(json)));
    });

    el("panic").addEventListener("click", () => {
      callNative("resetMachine").then(() => callNative("getState"))
        .then((json) => renderState(JSON.parse(json)));
    });
  }

  // --- Arranque -----------------------------------------------------------
  for (let index = 0; index < ROW_COUNT; index += 1) {
    buildRow(index);
  }
  setupKeyboard();
  bindParameters();

  // Montar componentes ABDShared
  try {
    const navContainer = document.getElementById('navBarContainer');
    if (navContainer) {
      const presetBarHtml = `
        <div id="presetBar" class="synth-preset-bar">
          <label class="preset-name-label">Preset
            <input id="preset-name" type="text" value="Init" spellcheck="false">
          </label>
          <select id="preset-list" aria-label="Presets guardados"></select>
          <button id="preset-save" type="button" class="btn btn-sm">
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"></path><polyline points="17 21 17 13 7 13 7 21"></polyline><polyline points="7 3 7 8 15 8"></polyline></svg>
            Guardar
          </button>
          <button id="preset-load" type="button" class="btn btn-sm">
            <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M3 15v4c0 1.1.9 2 2 2h14a2 2 0 0 0 2-2v-4M17 9l-5 5-5-5M12 12.8V2.5"></path></svg>
            Cargar
          </button>
        </div>
      `;

      createSynthNavBar({
        container: navContainer,
        brand: { title: 'BDMCS48SE', subtitle: 'MCS-48 Instrument' },
        centerSlot: presetBarHtml,
        menus: [
          {
            name: 'File',
            items: [
              { label: 'Panic (All Notes Off)', action: 'panic' },
              { label: 'Reset Machine', action: 'resetMachine' },
              { label: 'Save Preset', action: 'savePreset' },
              { label: 'Load Preset', action: 'loadPreset' },
            ]
          },
          {
            name: 'View',
            items: [
              { label: 'Toggle Theme', action: 'toggleTheme' },
              { label: 'Toggle Virtual Keyboard (Ctrl+K)', action: 'toggleKeyboardDrawer' },
            ]
          },
          {
            name: 'Help',
            items: [
              { label: 'About BDMCS48SE...', action: 'about' },
            ]
          }
        ],
        actions: {
          panic: () => {
            callNative('allNotesOff').catch(() => {});
            callNative('getState').then((json) => renderState(JSON.parse(json)));
          },
          resetMachine: () => {
            callNative('resetMachine').then(() => callNative('getState')).then((json) => renderState(JSON.parse(json)));
          },
          savePreset: () => {
            const name = document.getElementById('preset-name')?.value.trim() || '';
            callNative('savePreset', name).then((error) => {
              showFault(error ? String(error) : '');
              return callNative('getState');
            }).then((json) => renderState(JSON.parse(json)));
          },
          loadPreset: () => {
            const name = document.getElementById('preset-list')?.value || '';
            if (!name) return;
            callNative('loadPreset', name).then((error) => {
              showFault(error ? String(error) : '');
              const input = document.getElementById('preset-name');
              if (input) input.value = name;
              return callNative('getState');
            }).then((json) => renderState(JSON.parse(json)));
          },
          toggleTheme: () => {
            const html = document.documentElement;
            const current = html.getAttribute('data-theme') || 'audiolab';
            const next = current === 'audiolab-light' ? 'audiolab' : 'audiolab-light';
            html.setAttribute('data-theme', next);
            document.body.setAttribute('data-theme', next);
            document.body.className = `theme-${next} skin-${next} chassis`;
          },
          toggleKeyboardDrawer: () => {
            if (window.__bdmcs48_kbDrawer) {
              window.__bdmcs48_kbDrawer.toggle();
            }
          },
          about: () => {
            try {
              const dlg = createAboutDialog({
                name: 'BDMCS48SE Instrument',
                version: '0.1.0',
                build: 'pilot',
                description: 'MCS-48 based monophonic instrument'
              });
              dlg.open();
            } catch (e) {
              console.warn('About dialog failed:', e);
            }
          }
        },
        rightSlot: `<span id="firmwareStatusPill" class="navbar-pill">MCS-48</span>`
      });

      // Bind preset buttons after navbar injection
      setTimeout(() => {
        const saveBtn = document.getElementById('preset-save');
        const loadBtn = document.getElementById('preset-load');
        if (saveBtn) {
          saveBtn.addEventListener('click', () => {
            const name = document.getElementById('preset-name')?.value.trim() || '';
            callNative('savePreset', name).then((error) => {
              showFault(error ? String(error) : '');
              return callNative('getState');
            }).then((json) => renderState(JSON.parse(json)));
          });
        }
        if (loadBtn) {
          loadBtn.addEventListener('click', () => {
            const name = document.getElementById('preset-list')?.value || '';
            if (!name) return;
            callNative('loadPreset', name).then((error) => {
              showFault(error ? String(error) : '');
              const input = document.getElementById('preset-name');
              if (input) input.value = name;
              return callNative('getState');
            }).then((json) => renderState(JSON.parse(json)));
          });
        }
      }, 0);
    }
  } catch (e) {
    console.warn('SynthNavBar init failed:', e);
  }

  try {
    const vuContainer = document.getElementById('vuMeterMaster');
    if (vuContainer) {
      const vuMeter = new VuMeter(vuContainer, { channels: 2, orientation: 'vertical' });
      window.__bdmcs48_vuMeter = vuMeter;
    }
  } catch (e) {
    console.warn('VuMeter init failed:', e);
  }

  // VirtualKeyboardDrawer en la base
  try {
    const kbMount = document.getElementById('virtualKeyboardDrawerMount');
    if (kbMount) {
      const drawer = createDrawer({ id: 'virtualKeyboardDrawer', title: 'Teclado Virtual' });
      let kbDiv = document.getElementById('piano-keyboard');
      if (!kbDiv) {
        kbDiv = document.createElement('div');
        kbDiv.id = 'piano-keyboard';
      }
      kbDiv.style.height = '155px';
      drawer.body.appendChild(kbDiv);
      createKeyboard({
        containerId: 'piano-keyboard',
        onNoteOn: (note, velocity) => callNative('noteOn', note, velocity),
        onNoteOff: (note) => callNative('noteOff', note),
        onPanic: () => callNative('allNotesOff'),
        config: {
          numOctaves: 2,
          startNote: 48,
          fixedOctaves: true,
          maxOctaveShift: 0,
          enableQwerty: true,
          enableAccessibility: true,
          enableHold: false,
          enableRetrig: false,
        }
      });
      window.__bdmcs48_kbDrawer = drawer;
      // Abrir con Ctrl+K
      window.addEventListener('keydown', (e) => {
        if ((e.ctrlKey || e.metaKey) && e.key.toLowerCase() === 'k') {
          const tag = document.activeElement ? document.activeElement.tagName.toLowerCase() : '';
          if (tag === 'input' || tag === 'textarea') return;
          e.preventDefault();
          if (window.__bdmcs48_kbDrawer) {
            window.__bdmcs48_kbDrawer.toggle();
          }
        }
      });
    }
  } catch (e) {
    console.warn('Keyboard drawer init failed:', e);
  }

  backend.addEventListener("bdmcs48-state", (json) => {
    if (typeof json === "string") {
      renderState(JSON.parse(json));
    } else if (json) {
      renderState(json);
    }
    if (window.__bdmcs48_vuMeter && json) {
      try {
        const state = typeof json === 'string' ? JSON.parse(json) : json;
        const level = state.engine?.level || 0;
        window.__bdmcs48_vuMeter.setLevels({ left: level * 0.9, right: level * 1.0 });
      } catch (e) {}
    }
  });

  callNative("getState").then((json) => renderState(JSON.parse(json)))
    .catch((error) => showFault(String(error)));
})();
