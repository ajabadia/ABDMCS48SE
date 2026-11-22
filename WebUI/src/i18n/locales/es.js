/**
 * BDMCS48SE — es.js
 * Diccionario en Castellano (ES) Nivel 2 (Extiende @abdsynths/shared SHARED_ES).
 *
 * Reglas:
 * - Términos comunes viven en @abdsynths/shared (SHARED_ES).
 * - Acrónimos universales de síntesis (PC, A, PSW, Hz, dB, ms, etc.) NO se traducen.
 */

import { SHARED_ES, deepMergeLocales } from '@abdsynths/shared/i18n';

export const mcs48Es = {
  navbar: {
    savePreset: 'Guardar Preset...',
    loadPreset: 'Cargar Preset...',
    exportSysEx: 'Exportar Patch SysEx...',
    importSysEx: 'Importar Patch SysEx...',
    resetMachine: 'Reiniciar Máquina (Reset CPU)',
    panic: 'Panic (Apagar Todas las Notas)',
    undo: 'Deshacer (Undo)',
    redo: 'Rehacer (Redo)',
    randomizeMatrix: 'Sembrar Mutación en Matriz',
    resetMatrix: 'Reiniciar Matriz de Comandos',
    firmwareManager: 'Gestor de Firmwares & ROMs...',
    reloadFirmwares: 'Recargar Firmwares',
    workspaceSettings: 'Carpeta de Trabajo (Workspace)...',
    toggleKeyboard: 'Alternar Teclado Virtual',
    toggleTheme: 'Alternar Modo Oscuro / Claro',
    themeMenu: 'Tema',
    colorMode: 'Modo de Color',
    modeDark: 'Modo Oscuro',
    modeLight: 'Modo Claro',
    language: 'Idioma',
    langEn: 'English (EN)',
    langEs: 'Español (ES)',
    about: 'Acerca de BDMCS48SE...',

    aboutTitle: 'BDMCS48SE Instrument',
    aboutSubtitle: 'Sintetizador Monofónico MCS-48',
    aboutDescription: 'Sintetizador Monofónico Intel MCS-48 y Motor de Telemetría In-Circuit',

    engineStatus: 'MCS-48',
  },

  toolbar: {
    presetLabel: 'PRESET:',
    presetSave: 'Guardar',
    presetLoad: 'Cargar',
    presetSaveTitle: 'Guardar Preset',
    presetLoadTitle: 'Cargar Preset',
    presetNameTitle: 'Nombre del preset',
    firmwareLabel: 'FIRMWARE:',
    firmwareBadgeTitle: 'Clic para abrir Gestor de Firmware y ROMs',
    panicBtn: 'PANIC',
    panicTitle: 'Panic: Apagar todas las notas (Ctrl+Q)',
    resetCpuBtn: 'RESET CPU',
    resetCpuTitle: 'Reiniciar CPU MCS-48',
    defaultFwName: 'MCS-48 Enhanced Mono Voice',
  },

  voice: {
    title: 'Voz & Motor MCS-48',
    gate: 'GATE',
    note: 'NOTA:',
    freq: 'FREQ:',
    toneLabel: '440 Hz TONE',
    diagTone: 'DIAG TONE',
    diagToneTitle: 'Alternar tono senoidal de prueba acústica',
  },

  master: {
    title: 'Salida Master',
    badge: '8-BIT R2R DAC',
    scopeTitle: 'DAC WAVEFORM',
    scopeLive: 'LIVE',
  },

  matrix: {
    title: 'Matriz de comandos (8 Pasos)',
    hint: 'Disparo automático al recibir Note On · Mutación en tiempo real',
    rndBtn: 'RND SEED',
    rndTitle: 'Sembrar Mutación',
    resetBtn: 'RESET',
    resetTitle: 'Reiniciar Matriz',
    colStep: '#',
    colTrig: 'TRIG',
    colOn: 'ON',
    colNote: 'NOTA',
    colTarget: 'DESTINO',
    colValue: 'VALOR',
    colProb: 'PROB',
    colDelay: 'DELAY',
    colMutation: 'MUTACIÓN',
  },

  telemetry: {
    title: 'Telemetría MCS-48',
    bezelTitle: 'INTEL 8048 / 8049 MICROCONTROLLER IN-CIRCUIT DIAGNOSTICS',
    pc: 'PROGRAM CTR (PC)',
    acc: 'ACCUMULATOR (A)',
    psw: 'STATUS WORD (PSW)',
    bank: 'REGISTER BANK',
    cycles: 'TOTAL CPU CYCLES',
    note: 'ACTIVE MIDI NOTE',
    level: 'AUDIO LEVEL',
    command: 'LAST SOUND COMMAND',
    overruns: 'FIFO OVERRUNS',
    underruns: 'BUFFER UNDERRUNS',
    openDrawerBtn: 'Gestor de ROMs & Firmwares...',
    faultDefault: 'CPU detenida por fallo interno',
  },

  drawer: {
    title: 'Gestor de Firmware & ROMs MCS-48',
    badge: 'MCS-48',
    wsTitle: 'Carpeta de Trabajo',
    wsHint: 'Ubicación donde BDMCS48SE busca paquetes de firmware y ROMs compiladas.',
    wsPlaceholder: 'Detectando workspace...',
    wsBrowseBtn: 'Examinar...',
    fwTitle: 'Firmwares Disponibles',
    fwHint: 'Selecciona un paquete empaquetado para cargarlo en el emulador en tiempo real.',
    fwLoadBtn: 'Cargar en Motor',
    romConverterTitle: 'Conversor de ROM a Paquete',
    romConverterHint: 'Convierte un volcado binario .bin/.rom de Intel 8048/8049 en un paquete oficial para la suite.',
    romFileLabel: 'Archivo Binario ROM (*.bin, *.rom):',
    romPlaceholder: 'Ruta al binario ROM...',
    romBrowseBtn: 'Seleccionar...',
    romExistingLabel: 'O elegir de ROMs escaneadas en workspace:',
    romSelectPrompt: '— Seleccionar ROM escaneada... —',
    romSelectEmpty: '— No hay ROMs en el workspace —',
    pkgIdLabel: 'ID Paquete:',
    pkgIdPlaceholder: 'ej: bdmcs48-demo',
    pkgNameLabel: 'Nombre:',
    pkgNamePlaceholder: 'ej: MCS-48 Monosynth',
    pkgCpuLabel: 'Variante CPU:',
    pkgClockLabel: 'Reloj CPU (Hz):',
    pkgAuthorLabel: 'Autor / Licencia:',
    pkgAuthorPlaceholder: 'Tu nombre / CC0-1.0',
    pkgGenerateBtn: 'Generar e Instalar Paquete',
    errSelectRom: 'Selecciona un archivo binario ROM.',
    errPackageId: 'Define un ID de paquete único.',
    errPackageName: 'Define un nombre descriptivo.',
    msgPackageSuccess: 'Paquete generado e instalado correctamente.',
    msgFirmwareLoaded: "Firmware '{name}' cargado con éxito",
  },

  messages: {
    noUndo: 'Nada que deshacer',
    noRedo: 'Nada que rehacer',
    contractMismatch: 'Contrato incompatible: el backend usa la versión {backend} y esta interfaz espera la {expected}.',
    presetEmptyList: '— elegir preset —',
    presetNoList: '— sin presets —',
    fwEmptyList: '— elegir firmware —',
    fwNoList: '— sin firmwares —',
    sysexExportSuccess: 'Patch SysEx exportado con éxito.',
    sysexImportSuccess: 'Patch SysEx importado con éxito.',
    dropRomSuccess: 'Microcódigo ROM "{name}" instalado con éxito.',
    dropRomError: 'Error al instalar la ROM: {error}',
  }
};

export const es = deepMergeLocales(SHARED_ES, mcs48Es);
