// paramStore.js - Central Parameter Store and UI Control Registry for ABDMCS48SE
// Inspired by the ABDMS2000 paramStore pattern. Decouples DOM widgets from the native bridge.

import { PARAMETERS_SPEC, PARAM_LOOKUP } from './registry.gen.js';

class ParamStore {
  constructor() {
    this.controls = new Map();
    this.values = new Map();
    this.listeners = new Map();
    this.bridge = null;
    this.undoStack = [];
    this.redoStack = [];
    this._suppressHistory = false;
    this._currentRows = null;
    this._gestureInitialSnapshot = null;
    this.onStateRestore = null;

    // Initialize with default values
    for (const p of PARAMETERS_SPEC.parameters) {
      this.values.set(p.id, p.default !== undefined ? p.default : 0);
    }
  }

  setBridge(bridge) {
    this.bridge = bridge;
  }

  setRows(rows) {
    if (Array.isArray(rows)) {
      this._currentRows = JSON.parse(JSON.stringify(rows));
    }
  }

  normalize(paramId, physicalValue) {
    const spec = PARAM_LOOKUP.get(paramId);
    if (!spec) return physicalValue;
    const min = spec.min ?? 0;
    const max = spec.max ?? 1;
    if (max <= min) return 0;
    return Math.max(0, Math.min(1, (physicalValue - min) / (max - min)));
  }

  denormalize(paramId, normValue) {
    const spec = PARAM_LOOKUP.get(paramId);
    if (!spec) return normValue;
    const min = spec.min ?? 0;
    const max = spec.max ?? 1;
    return min + Math.max(0, Math.min(1, normValue)) * (max - min);
  }

  get(paramId) {
    return this.values.get(paramId);
  }

  /**
   * Set parameter value programmatically or from UI.
   * @param {string} paramId
   * @param {number} physicalValue
   * @param {object} [options]
   * @param {boolean} [options.notifyBridge=true] Send to native C++ APVTS
   * @param {object} [options.sourceControl=null] The control originating the change (to avoid echo)
   * @param {boolean} [options.recordUndo=true]
   */
  setParam(paramId, physicalValue, { notifyBridge = true, sourceControl = null, recordUndo = true } = {}) {
    const spec = PARAM_LOOKUP.get(paramId);
    const clamped = spec
      ? Math.max(spec.min, Math.min(spec.max, physicalValue))
      : physicalValue;

    const prev = this.values.get(paramId);
    if (prev === clamped) return;

    if (recordUndo && !this._suppressHistory && !this._gestureInitialSnapshot) {
      this.recordStateSnapshot(`Param ${paramId}`);
    }

    this.values.set(paramId, clamped);

    // Notify registered controls (silent update to prevent echo)
    const set = this.controls.get(paramId);
    if (set) {
      for (const ctrl of set) {
        if (ctrl !== sourceControl) {
          this.applyToControl(paramId, ctrl, clamped);
        }
      }
    }

    // Notify listeners
    const listeners = this.listeners.get(paramId);
    if (listeners) {
      for (const cb of listeners) {
        try { cb(clamped, paramId); } catch (e) { console.error(e); }
      }
    }

    // Send to native bridge
    if (notifyBridge && this.bridge && typeof this.bridge.callNative === 'function') {
      const norm = this.normalize(paramId, clamped);
      this.bridge.callNative('setParameter', paramId, norm).catch((err) => {
        console.warn(`Error setting parameter ${paramId}:`, err);
      });
    }
  }

  beginGesture(paramId) {
    if (!this._suppressHistory) {
      this._gestureInitialSnapshot = this.captureSnapshot(`Gesture ${paramId}`);
    }
    if (this.bridge && typeof this.bridge.callNative === 'function') {
      this.bridge.callNative('beginParameterGesture', paramId).catch(() => {});
    }
  }

  endGesture(paramId) {
    if (this._gestureInitialSnapshot) {
      const initialVal = this._gestureInitialSnapshot.values[paramId];
      const currentVal = this.values.get(paramId);
      if (initialVal !== undefined && initialVal !== currentVal && !this._suppressHistory) {
        this.undoStack.push(this._gestureInitialSnapshot);
        if (this.undoStack.length > 50) this.undoStack.shift();
        this.redoStack.length = 0;
      }
      this._gestureInitialSnapshot = null;
    }
    if (this.bridge && typeof this.bridge.callNative === 'function') {
      this.bridge.callNative('endParameterGesture', paramId).catch(() => {});
    }
  }

  register(paramId, control) {
    if (!this.controls.has(paramId)) {
      this.controls.set(paramId, new Set());
    }
    this.controls.get(paramId).add(control);

    if (this.values.has(paramId)) {
      this.applyToControl(paramId, control, this.values.get(paramId));
    }
  }

  unregister(paramId, control) {
    const set = this.controls.get(paramId);
    if (set) set.delete(control);
  }

  applyToControl(paramId, control, physicalValue) {
    if (!control) return;
    if (typeof control.setValue === 'function') {
      // ABDShared Knob / Slider contract: setValue(normalized, notify=false)
      const norm = this.normalize(paramId, physicalValue);
      control.setValue(norm, false);
    } else if (control.tagName === 'INPUT') {
      if (control.type === 'checkbox') {
        control.checked = Boolean(physicalValue);
      } else {
        control.value = String(physicalValue);
      }
    } else if (control.tagName === 'SELECT') {
      control.value = String(physicalValue);
    }
  }

  onChange(paramId, cb) {
    if (!this.listeners.has(paramId)) {
      this.listeners.set(paramId, new Set());
    }
    this.listeners.get(paramId).add(cb);
  }

  offChange(paramId, cb) {
    const set = this.listeners.get(paramId);
    if (set) set.delete(cb);
  }

  /**
   * Sync from incoming C++ state snapshot without re-emitting to bridge.
   */
  syncFromState(state) {
    if (!state) return;
    if (Array.isArray(state.rows)) {
      this._currentRows = JSON.parse(JSON.stringify(state.rows));
    }
    for (const [id] of PARAM_LOOKUP.entries()) {
      if (state[id] !== undefined) {
        const val = Number(state[id]);
        this.setParam(id, val, { notifyBridge: false, recordUndo: false });
      }
    }
  }

  captureSnapshot(label = 'Edit') {
    return {
      label,
      values: Object.fromEntries(this.values),
      rows: this._currentRows ? JSON.parse(JSON.stringify(this._currentRows)) : null
    };
  }

  recordStateSnapshot(label = 'Edit') {
    if (this._suppressHistory) return;
    this.undoStack.push(this.captureSnapshot(label));
    if (this.undoStack.length > 50) this.undoStack.shift();
    this.redoStack.length = 0;
  }

  recordSnapshot(label = 'Edit') {
    this.recordStateSnapshot(label);
  }

  canUndo() {
    return this.undoStack.length > 0;
  }

  canRedo() {
    return this.redoStack.length > 0;
  }

  async undo() {
    if (!this.canUndo()) return false;
    const current = this.captureSnapshot('Current');
    const prev = this.undoStack.pop();
    this.redoStack.push(current);
    this._suppressHistory = true;
    try {
      // Restore voice parameters
      for (const [id, val] of Object.entries(prev.values)) {
        this.setParam(id, val, { notifyBridge: true, recordUndo: false });
      }
      // Restore step matrix rows if present
      if (prev.rows && Array.isArray(prev.rows) && this.bridge && typeof this.bridge.callNative === 'function') {
        for (let i = 0; i < prev.rows.length; i++) {
          await this.bridge.callNative('setRow', i, JSON.stringify(prev.rows[i]));
        }
      }
      // Refresh engine and UI state
      if (this.bridge && typeof this.bridge.callNative === 'function') {
        const json = await this.bridge.callNative('getState');
        const state = typeof json === 'string' ? JSON.parse(json) : json;
        if (this.onStateRestore) this.onStateRestore(state);
      }
    } finally {
      this._suppressHistory = false;
    }
    return true;
  }

  async redo() {
    if (!this.canRedo()) return false;
    const current = this.captureSnapshot('Current');
    const next = this.redoStack.pop();
    this.undoStack.push(current);
    this._suppressHistory = true;
    try {
      // Restore voice parameters
      for (const [id, val] of Object.entries(next.values)) {
        this.setParam(id, val, { notifyBridge: true, recordUndo: false });
      }
      // Restore step matrix rows if present
      if (next.rows && Array.isArray(next.rows) && this.bridge && typeof this.bridge.callNative === 'function') {
        for (let i = 0; i < next.rows.length; i++) {
          await this.bridge.callNative('setRow', i, JSON.stringify(next.rows[i]));
        }
      }
      // Refresh engine and UI state
      if (this.bridge && typeof this.bridge.callNative === 'function') {
        const json = await this.bridge.callNative('getState');
        const state = typeof json === 'string' ? JSON.parse(json) : json;
        if (this.onStateRestore) this.onStateRestore(state);
      }
    } finally {
      this._suppressHistory = false;
    }
    return true;
  }
}

export const paramStore = new ParamStore();
