/**
 * BDMCS48SE — i18n.test.js
 * Verificación de paridad de claves y funcionamiento del motor reactivo i18n.
 */

import { describe, it, expect } from 'vitest';
import { mcs48En } from '../src/i18n/locales/en.js';
import { mcs48Es } from '../src/i18n/locales/es.js';
import {
  initI18n,
  getLocale,
  setLocale,
  onLocaleChange,
  t,
  collectLocaleKeys
} from '../src/i18n/index.js';

describe('BDMCS48SE i18n Suite', () => {
  it('has 1:1 key parity between English and Spanish dictionaries', () => {
    const enKeys = collectLocaleKeys(mcs48En).sort();
    const esKeys = collectLocaleKeys(mcs48Es).sort();

    expect(enKeys.length).toBe(esKeys.length);
    for (let i = 0; i < enKeys.length; i++) {
      expect(enKeys[i]).toBe(esKeys[i]);
    }
  });

  it('has non-empty string values for all synth-specific keys', () => {
    const enKeys = collectLocaleKeys(mcs48En);
    for (const key of enKeys) {
      const parts = key.split('.');
      let enVal = mcs48En;
      let esVal = mcs48Es;
      for (const p of parts) {
        enVal = enVal?.[p];
        esVal = esVal?.[p];
      }
      expect(typeof enVal).toBe('string');
      expect(enVal.length).toBeGreaterThan(0);
      expect(typeof esVal).toBe('string');
      expect(esVal.length).toBeGreaterThan(0);
    }
  });

  it('translates correctly and falls back to Tier-1 shared dictionaries', () => {
    initI18n();
    setLocale('en');
    expect(getLocale()).toBe('en');
    expect(t('navbar.savePreset')).toBe('Save Sound Preset...');
    expect(t('voice.title')).toBe('MCS-48 Voice & Engine');
    expect(t('master.badge')).toBe('8-BIT R2R DAC');

    // Tier 1 shared fallback
    expect(t('common.close')).toBe('Close');
    expect(t('common.cancel')).toBe('Cancel');
  });

  it('switches dynamically to Spanish and notifies subscribers', () => {
    let notifiedLocale = null;
    const unsub = onLocaleChange((loc) => {
      notifiedLocale = loc;
    });

    setLocale('es');
    expect(getLocale()).toBe('es');
    expect(notifiedLocale).toBe('es');
    expect(t('navbar.savePreset')).toBe('Guardar Preset...');
    expect(t('voice.title')).toBe('Voz & Motor MCS-48');
    expect(t('common.close')).toBe('Cerrar');
    expect(t('common.cancel')).toBe('Cancelar');

    unsub();
  });

  it('handles variable interpolation in messages', () => {
    setLocale('es');
    const msgEs = t('drawer.msgFirmwareLoaded', { name: 'DemoROM' });
    expect(msgEs).toBe("Firmware 'DemoROM' cargado con éxito");

    setLocale('en');
    const msgEn = t('drawer.msgFirmwareLoaded', { name: 'DemoROM' });
    expect(msgEn).toBe("Firmware 'DemoROM' loaded successfully");
  });
});
