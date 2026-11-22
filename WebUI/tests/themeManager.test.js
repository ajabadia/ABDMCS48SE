/**
 * BDMCS48SE — themeManager.test.js
 * Verification of mode-aware theme resolution and canonical theme families.
 */

import { describe, it, expect, beforeEach } from 'vitest';
import {
  themeManager,
  THEMES,
  COLOR_MODES,
  ALL_THEME_CSS_IDS,
  STORAGE_KEY_THEME,
  STORAGE_KEY_FAMILY,
  STORAGE_KEY_MODE,
} from '../src/utils/themeManager.js';

describe('BDMCS48SE Theme Manager (Mode-Aware)', () => {
  beforeEach(() => {
    // Reset to default audiolab dark
    themeManager.setMode('dark');
    themeManager.setTheme('audiolab');
  });

  it('contains 9 canonical theme families without light/dark clutter in menus', () => {
    expect(THEMES.length).toBe(9);
    const expectedFamilies = [
      'audiolab',
      'ms2000',
      'microkorg',
      'advanced',
      'cz101',
      'deepmind',
      'juno',
      'neuron',
      'virus-snow',
    ];

    const actualFamilies = THEMES.map((t) => t.id);
    expect(actualFamilies).toEqual(expectedFamilies);
  });

  it('covers all 15 underlying CSS theme sheets from ABDShared across families', () => {
    expect(ALL_THEME_CSS_IDS.length).toBe(15);
    for (const cssId of ALL_THEME_CSS_IDS) {
      const foundInFamilies = THEMES.some(
        (fam) => fam.darkTheme === cssId || fam.lightTheme === cssId
      );
      expect(foundInFamilies).toBe(true);
    }
  });

  it('has valid labels in both English and Spanish for all families and modes', () => {
    for (const theme of THEMES) {
      expect(typeof theme.id).toBe('string');
      expect(typeof theme.label).toBe('string');
      expect(theme.label.length).toBeGreaterThan(0);
      expect(typeof theme.labelEs).toBe('string');
      expect(theme.labelEs.length).toBeGreaterThan(0);
      expect(typeof theme.darkTheme).toBe('string');
      expect(typeof theme.lightTheme).toBe('string');
    }

    for (const mode of COLOR_MODES) {
      expect(typeof mode.id).toBe('string');
      expect(typeof mode.label).toBe('string');
      expect(typeof mode.labelEs).toBe('string');
    }
  });

  it('applies dark variant when mode is dark', () => {
    themeManager.setMode('dark');
    themeManager.setTheme('ms2000');

    expect(themeManager.getCurrentTheme()).toBe('ms2000');
    expect(themeManager.getMode()).toBe('dark');
    expect(themeManager.getAppliedTheme()).toBe('ms2000');
    expect(themeManager.isDark()).toBe(true);
  });

  it('automatically applies light variant of the active theme when mode switches to light', () => {
    themeManager.setMode('dark');
    themeManager.setTheme('audiolab');
    expect(themeManager.getAppliedTheme()).toBe('audiolab');

    themeManager.setMode('light');
    expect(themeManager.getCurrentTheme()).toBe('audiolab');
    expect(themeManager.getMode()).toBe('light');
    expect(themeManager.getAppliedTheme()).toBe('audiolab-light');
    expect(themeManager.isDark()).toBe(false);
  });

  it('applies light variant when selecting a new theme family while in light mode', () => {
    themeManager.setMode('light');
    themeManager.setTheme('ms2000');

    expect(themeManager.getCurrentTheme()).toBe('ms2000');
    expect(themeManager.getMode()).toBe('light');
    expect(themeManager.getAppliedTheme()).toBe('ms2000-light');

    themeManager.setTheme('cz101');
    expect(themeManager.getCurrentTheme()).toBe('cz101');
    expect(themeManager.getAppliedTheme()).toBe('cz101-light');
  });

  it('toggles mode cleanly with toggleMode() / toggleLightDark()', () => {
    themeManager.setMode('dark');
    themeManager.setTheme('advanced');
    expect(themeManager.getAppliedTheme()).toBe('advanced');

    const toggledToLight = themeManager.toggleMode();
    expect(toggledToLight).toBe('light');
    expect(themeManager.getAppliedTheme()).toBe('advanced-light');

    const toggledBackToDark = themeManager.toggleLightDark();
    expect(toggledBackToDark).toBe('dark');
    expect(themeManager.getAppliedTheme()).toBe('advanced');
  });

  it('supports legacy / explicit CSS ID in setTheme and infers family + mode', () => {
    themeManager.setTheme('neuron-light');
    expect(themeManager.getCurrentTheme()).toBe('neuron');
    expect(themeManager.getMode()).toBe('light');
    expect(themeManager.getAppliedTheme()).toBe('neuron-light');

    themeManager.setTheme('virus-snow-dark');
    expect(themeManager.getCurrentTheme()).toBe('virus-snow');
    expect(themeManager.getMode()).toBe('dark');
    expect(themeManager.getAppliedTheme()).toBe('virus-snow-dark');
  });

  it('persists theme family, color mode, and applied theme in localStorage', () => {
    themeManager.setMode('light');
    themeManager.setTheme('ms2000');

    if (typeof localStorage !== 'undefined') {
      expect(localStorage.getItem(STORAGE_KEY_FAMILY)).toBe('ms2000');
      expect(localStorage.getItem(STORAGE_KEY_MODE)).toBe('light');
      expect(localStorage.getItem(STORAGE_KEY_THEME)).toBe('ms2000-light');
    }
  });

  it('cycles through all theme families while maintaining current mode', () => {
    themeManager.setMode('light');
    themeManager.setTheme('audiolab');
    expect(themeManager.getAppliedTheme()).toBe('audiolab-light');

    const nextApplied = themeManager.cycleTheme();
    expect(themeManager.getCurrentTheme()).toBe('ms2000');
    expect(nextApplied).toBe('ms2000-light');
    expect(themeManager.getMode()).toBe('light');
  });

  it('notifies listeners when theme or mode changes', () => {
    let lastApplied = null;
    let lastMode = null;
    let lastFamily = null;

    const unsub = themeManager.onThemeChange((applied, mode, family) => {
      lastApplied = applied;
      lastMode = mode;
      lastFamily = family;
    });

    themeManager.setMode('dark');
    themeManager.setTheme('virus-snow');
    expect(lastApplied).toBe('virus-snow-dark');
    expect(lastMode).toBe('dark');
    expect(lastFamily).toBe('virus-snow');

    themeManager.toggleMode();
    expect(lastApplied).toBe('virus-snow');
    expect(lastMode).toBe('light');

    unsub();
    themeManager.setTheme('juno');
    expect(lastFamily).toBe('virus-snow'); // No further notification after unsub
  });
});
