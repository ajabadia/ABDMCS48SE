/**
 * BDMCS48SE — i18n/index.js
 * Instancia del motor reactivo de internacionalización (i18n) de `@abdsynths/shared/i18n`.
 *
 * Modelo en 2 Capas:
 * - Capa 1: Términos comunes transversales (`SHARED_EN` / `SHARED_ES`).
 * - Capa 2: Extensión específica de BDMCS48SE (`mcs48En` / `mcs48Es`).
 */

import { createSynthI18n, collectLocaleKeys } from '@abdsynths/shared/i18n';
import { mcs48En, en } from './locales/en.js';
import { mcs48Es, es } from './locales/es.js';

export { en, es, mcs48En, mcs48Es, collectLocaleKeys };

const i18n = createSynthI18n({
  locales: {
    en: mcs48En,
    es: mcs48Es,
  },
  defaultLocale: 'en',
  storageKey: 'abdsynths.locale',
  legacyStorageKeys: ['bdmcs48.locale'],
});

export const initI18n = i18n.initI18n;
export const getLocale = i18n.getLocale;
export const setLocale = i18n.setLocale;
export const onLocaleChange = i18n.onLocaleChange;
export const t = i18n.t;
export const applyI18n = i18n.applyI18n;
export const getSupportedLocales = i18n.getSupportedLocales;
export const getDictionary = i18n.getDictionary;
export const extendLocale = i18n.extendLocale;
