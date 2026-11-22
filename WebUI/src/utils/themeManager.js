/**
 * ABDMCS48SE — Theme Manager
 * Centralized theme management and application logic using ABDShared canonical themes.
 *
 * Mode-aware theme resolution:
 * - Themes are organized into canonical synth families (AudioLab, MS2000, microKORG, etc.).
 * - Themes DO NOT clutter the menu with separate light/dark entries.
 * - The active theme resolves dynamically according to the selected mode ('dark' | 'light').
 * - Switching mode seamlessly shifts the active theme to its corresponding variant.
 */

export const THEMES = [
  {
    id: 'audiolab',
    label: 'AudioLab (Scientific)',
    labelEs: 'AudioLab (Científico)',
    darkTheme: 'audiolab',
    lightTheme: 'audiolab-light',
  },
  {
    id: 'ms2000',
    label: 'MS2000 (Teal Heritage)',
    labelEs: 'MS2000 (Turquesa Heritage)',
    darkTheme: 'ms2000',
    lightTheme: 'ms2000-light',
  },
  {
    id: 'microkorg',
    label: 'microKORG (Amber Heritage)',
    labelEs: 'microKORG (Ámbar Heritage)',
    darkTheme: 'microkorg',
    lightTheme: 'ms2000-light',
  },
  {
    id: 'advanced',
    label: 'Advanced (High Contrast)',
    labelEs: 'Advanced (Alto Contraste)',
    darkTheme: 'advanced',
    lightTheme: 'advanced-light',
  },
  {
    id: 'cz101',
    label: 'CZ-101 (Vintage Slate)',
    labelEs: 'CZ-101 (Pizarra y Carmesí)',
    darkTheme: 'cz101',
    lightTheme: 'cz101-light',
  },
  {
    id: 'deepmind',
    label: 'DeepMind (Charcoal & Amber)',
    labelEs: 'DeepMind (Carbón y Ámbar)',
    darkTheme: 'deepmind',
    lightTheme: 'audiolab-light',
  },
  {
    id: 'juno',
    label: 'Juno (Multi-Color Heritage)',
    labelEs: 'Juno (Multicolor Heritage)',
    darkTheme: 'juno',
    lightTheme: 'audiolab-light',
  },
  {
    id: 'neuron',
    label: 'Neuron (Anthracite & Neon)',
    labelEs: 'Neuron (Antracita y Neón)',
    darkTheme: 'neuron-dark',
    lightTheme: 'neuron-light',
  },
  {
    id: 'virus-snow',
    label: 'Virus Snow (Polar Studio)',
    labelEs: 'Virus Snow (Estudio Polar)',
    darkTheme: 'virus-snow-dark',
    lightTheme: 'virus-snow',
  },
];

export const COLOR_MODES = [
  { id: 'dark', label: 'Dark Mode', labelEs: 'Modo Oscuro' },
  { id: 'light', label: 'Light Mode', labelEs: 'Modo Claro' },
];

export const ALL_THEME_CSS_IDS = [
  'audiolab',
  'audiolab-light',
  'ms2000',
  'ms2000-light',
  'microkorg',
  'advanced',
  'advanced-light',
  'cz101',
  'cz101-light',
  'deepmind',
  'juno',
  'neuron-dark',
  'neuron-light',
  'virus-snow',
  'virus-snow-dark',
];

export const STORAGE_KEY = 'abdsynths.theme';
export const STORAGE_KEY_THEME = 'abdsynths.theme';
export const STORAGE_KEY_FAMILY = 'abdsynths.themeFamily';
export const STORAGE_KEY_MODE = 'abdsynths.colorMode';

export const LEGACY_STORAGE_KEY = 'bdmcs48.theme';
export const LEGACY_STORAGE_KEY_THEME = 'bdmcs48.theme';
export const LEGACY_STORAGE_KEY_FAMILY = 'bdmcs48.themeFamily';
export const LEGACY_STORAGE_KEY_MODE = 'bdmcs48.colorMode';

export class ThemeManager {
  constructor() {
    this._themes = THEMES;
    this._listeners = new Set();

    let initialMode = 'dark';
    let initialFamily = 'audiolab';

    try {
      if (typeof localStorage !== 'undefined') {
        const savedMode = localStorage.getItem(STORAGE_KEY_MODE) || localStorage.getItem(LEGACY_STORAGE_KEY_MODE);
        if (savedMode === 'light' || savedMode === 'dark') {
          initialMode = savedMode;
        }

        const savedFamily = localStorage.getItem(STORAGE_KEY_FAMILY) || localStorage.getItem(LEGACY_STORAGE_KEY_FAMILY);
        if (savedFamily && this._themes.some((t) => t.id === savedFamily)) {
          initialFamily = savedFamily;
        } else {
          // Check if an explicit CSS theme ID was saved previously
          const legacyTheme = localStorage.getItem(STORAGE_KEY_THEME) || localStorage.getItem(LEGACY_STORAGE_KEY_THEME);
          if (legacyTheme) {
            const resolved = this._inferFamilyAndMode(legacyTheme);
            if (resolved) {
              initialFamily = resolved.family;
              if (!savedMode) initialMode = resolved.mode;
            }
          }
        }
      }
    } catch {}

    this._currentFamily = initialFamily;
    this._mode = initialMode;
    this._appliedTheme = this.resolveAppliedTheme(this._currentFamily, this._mode);
  }

  _inferFamilyAndMode(cssId) {
    for (const fam of this._themes) {
      if (fam.darkTheme === cssId) return { family: fam.id, mode: 'dark' };
      if (fam.lightTheme === cssId) return { family: fam.id, mode: 'light' };
      if (fam.id === cssId) return { family: fam.id, mode: 'dark' };
    }
    return null;
  }

  resolveAppliedTheme(familyId, mode = this._mode) {
    const family = this._themes.find((t) => t.id === familyId) || this._themes[0];
    return mode === 'light' ? family.lightTheme : family.darkTheme;
  }

  init() {
    this.applyCurrent();
    return this._appliedTheme;
  }

  applyCurrent() {
    this._appliedTheme = this.resolveAppliedTheme(this._currentFamily, this._mode);

    if (typeof document !== 'undefined') {
      document.documentElement.setAttribute('data-theme', this._appliedTheme);
      document.body.setAttribute('data-theme', this._appliedTheme);
      document.documentElement.setAttribute('data-mode', this._mode);
      document.body.setAttribute('data-mode', this._mode);
      document.body.className = `theme-${this._appliedTheme} skin-${this._appliedTheme} mode-${this._mode} chassis`;
    }

    try {
      if (typeof localStorage !== 'undefined') {
        localStorage.setItem(STORAGE_KEY_THEME, this._appliedTheme);
        localStorage.setItem(STORAGE_KEY_FAMILY, this._currentFamily);
        localStorage.setItem(STORAGE_KEY_MODE, this._mode);
        localStorage.setItem(LEGACY_STORAGE_KEY_THEME, this._appliedTheme);
        localStorage.setItem(LEGACY_STORAGE_KEY_FAMILY, this._currentFamily);
        localStorage.setItem(LEGACY_STORAGE_KEY_MODE, this._mode);
      }
    } catch {}

    this._notify(this._appliedTheme, this._mode, this._currentFamily);
  }

  setTheme(familyOrThemeId) {
    const directFamily = this._themes.find((t) => t.id === familyOrThemeId);
    if (directFamily) {
      this._currentFamily = directFamily.id;
      this.applyCurrent();
      return;
    }

    // Check if an explicit CSS theme id was passed
    const inferred = this._inferFamilyAndMode(familyOrThemeId);
    if (inferred) {
      this._currentFamily = inferred.family;
      this._mode = inferred.mode;
      this.applyCurrent();
    }
  }

  setMode(mode) {
    if (mode === 'dark' || mode === 'light') {
      if (this._mode !== mode) {
        this._mode = mode;
        this.applyCurrent();
      }
    }
  }

  toggleMode() {
    const nextMode = this._mode === 'dark' ? 'light' : 'dark';
    this.setMode(nextMode);
    return nextMode;
  }

  toggleLightDark() {
    return this.toggleMode();
  }

  getCurrentTheme() {
    return this._currentFamily;
  }

  getCurrentFamily() {
    return this._currentFamily;
  }

  getAppliedTheme() {
    return this._appliedTheme;
  }

  getMode() {
    return this._mode;
  }

  isDark() {
    return this._mode === 'dark';
  }

  getCurrentThemeInfo() {
    const fam = this._themes.find((t) => t.id === this._currentFamily) || this._themes[0];
    return {
      id: fam.id,
      label: fam.label,
      labelEs: fam.labelEs,
      mode: this._mode,
      appliedTheme: this._appliedTheme,
      isDark: this._mode === 'dark',
      darkTheme: fam.darkTheme,
      lightTheme: fam.lightTheme,
    };
  }

  cycleTheme() {
    const idx = this._themes.findIndex((t) => t.id === this._currentFamily);
    const nextIdx = (idx + 1) % this._themes.length;
    this._currentFamily = this._themes[nextIdx].id;
    this.applyCurrent();
    return this._appliedTheme;
  }

  getThemes() {
    return [...this._themes];
  }

  onThemeChange(callback) {
    if (typeof callback === 'function') {
      this._listeners.add(callback);
      return () => this._listeners.delete(callback);
    }
    return () => {};
  }

  _notify(appliedTheme, mode, family) {
    for (const listener of this._listeners) {
      try {
        listener(appliedTheme, mode, family);
      } catch (e) {
        console.warn('Error in theme change listener:', e);
      }
    }
  }
}

export const themeManager = new ThemeManager();
