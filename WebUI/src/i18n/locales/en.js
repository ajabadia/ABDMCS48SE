/**
 * BDMCS48SE — en.js
 * Tier-2 Synthesizer-Specific English Dictionary (Extends @abdsynths/shared SHARED_EN).
 *
 * Rules:
 * - Common UI terms (common.*, standard navbar.*, etc.) live in @abdsynths/shared (SHARED_EN).
 * - Universal synth acronyms and descriptors (PC, A, PSW, Hz, dB, ms, etc.) MUST NOT be translated.
 */

import { SHARED_EN, deepMergeLocales } from '@abdsynths/shared/i18n';

export const mcs48En = {
  navbar: {
    savePreset: 'Save Sound Preset...',
    loadPreset: 'Load Sound Preset...',
    exportSysEx: 'Export SysEx Patch...',
    importSysEx: 'Import SysEx Patch...',
    resetMachine: 'Reset Machine (Reset CPU)',
    panic: 'Panic (All Notes Off)',
    undo: 'Undo',
    redo: 'Redo',
    randomizeMatrix: 'Randomize Command Matrix',
    resetMatrix: 'Reset Command Matrix',
    firmwareManager: 'Firmware & ROMs Manager...',
    reloadFirmwares: 'Reload Firmwares',
    workspaceSettings: 'Working Directory (Workspace)...',
    toggleKeyboard: 'Toggle Virtual Keyboard',
    toggleTheme: 'Toggle Dark / Light Mode',
    themeMenu: 'Theme',
    colorMode: 'Color Mode',
    modeDark: 'Dark Mode',
    modeLight: 'Light Mode',
    language: 'Language',
    langEn: 'English (EN)',
    langEs: 'Español (ES)',
    about: 'About BDMCS48SE...',

    aboutTitle: 'BDMCS48SE Instrument',
    aboutSubtitle: 'MCS-48 Monophonic Synthesizer',
    aboutDescription: 'Intel MCS-48 Monophonic Synthesizer & In-Circuit Telemetry Engine',

    engineStatus: 'MCS-48',
  },

  toolbar: {
    presetLabel: 'PRESET:',
    presetSave: 'Save',
    presetLoad: 'Load',
    presetSaveTitle: 'Save Preset',
    presetLoadTitle: 'Load Preset',
    presetNameTitle: 'Preset Name',
    firmwareLabel: 'FIRMWARE:',
    firmwareBadgeTitle: 'Click to open Firmware and ROM Manager',
    panicBtn: 'PANIC',
    panicTitle: 'Panic: Turn off all notes (Ctrl+Q)',
    resetCpuBtn: 'RESET CPU',
    resetCpuTitle: 'Reset MCS-48 CPU',
    defaultFwName: 'MCS-48 Enhanced Mono Voice',
  },

  voice: {
    title: 'MCS-48 Voice & Engine',
    gate: 'GATE',
    note: 'NOTE:',
    freq: 'FREQ:',
    toneLabel: '440 Hz TONE',
    diagTone: 'DIAG TONE',
    diagToneTitle: 'Toggle acoustic diagnostic sine tone',
  },

  master: {
    title: 'Master Output',
    badge: '8-BIT R2R DAC',
    scopeTitle: 'DAC WAVEFORM',
    scopeLive: 'LIVE',
  },

  matrix: {
    title: 'Command Matrix (8 Steps)',
    hint: 'Automatic trigger on Note On · Real-time mutation',
    rndBtn: 'RND SEED',
    rndTitle: 'Seed Mutation',
    resetBtn: 'RESET',
    resetTitle: 'Reset Matrix',
    colStep: '#',
    colTrig: 'TRIG',
    colOn: 'ON',
    colNote: 'NOTE',
    colTarget: 'TARGET',
    colValue: 'VALUE',
    colProb: 'PROB',
    colDelay: 'DELAY',
    colMutation: 'MUTATION',
  },

  telemetry: {
    title: 'MCS-48 Telemetry',
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
    openDrawerBtn: 'ROMs & Firmware Manager...',
    faultDefault: 'CPU stopped due to internal fault',
  },

  drawer: {
    title: 'MCS-48 Firmware & ROM Manager',
    badge: 'MCS-48',
    wsTitle: 'Working Directory',
    wsHint: 'Location where BDMCS48SE searches for firmware packages and compiled ROMs.',
    wsPlaceholder: 'Detecting workspace...',
    wsBrowseBtn: 'Browse...',
    fwTitle: 'Available Firmwares',
    fwHint: 'Select a packaged bundle to load into the emulator in real time.',
    fwLoadBtn: 'Load into Engine',
    romConverterTitle: 'ROM to Package Converter',
    romConverterHint: 'Convert a binary Intel 8048/8049 .bin/.rom dump into an official suite package.',
    romFileLabel: 'Binary ROM File (*.bin, *.rom):',
    romPlaceholder: 'Path to binary ROM...',
    romBrowseBtn: 'Select...',
    romExistingLabel: 'Or select from scanned ROMs in workspace:',
    romSelectPrompt: '— Select scanned ROM... —',
    romSelectEmpty: '— No ROMs in workspace —',
    pkgIdLabel: 'Package ID:',
    pkgIdPlaceholder: 'e.g. bdmcs48-demo',
    pkgNameLabel: 'Name:',
    pkgNamePlaceholder: 'e.g. MCS-48 Monosynth',
    pkgCpuLabel: 'CPU Variant:',
    pkgClockLabel: 'CPU Clock (Hz):',
    pkgAuthorLabel: 'Author / License:',
    pkgAuthorPlaceholder: 'Your name / CC0-1.0',
    pkgGenerateBtn: 'Generate & Install Package',
    errSelectRom: 'Select a binary ROM file.',
    errPackageId: 'Define a unique package ID.',
    errPackageName: 'Define a descriptive name.',
    msgPackageSuccess: 'Package generated and installed successfully.',
    msgFirmwareLoaded: "Firmware '{name}' loaded successfully",
  },

  messages: {
    noUndo: 'Nothing to undo',
    noRedo: 'Nothing to redo',
    contractMismatch: 'Incompatible contract: backend uses version {backend} and this interface expects {expected}.',
    presetEmptyList: '— choose preset —',
    presetNoList: '— no presets —',
    fwEmptyList: '— choose firmware —',
    fwNoList: '— no firmwares —',
    sysexExportSuccess: 'SysEx patch exported successfully.',
    sysexImportSuccess: 'SysEx patch imported successfully.',
    dropRomSuccess: 'ROM microcode "{name}" installed successfully.',
    dropRomError: 'Failed to install ROM: {error}',
  }
};

export const en = deepMergeLocales(SHARED_EN, mcs48En);
