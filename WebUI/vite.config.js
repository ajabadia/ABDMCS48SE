// vite.config.js - Build de produccion del editor WebView.
//
// El binario no sirve el arbol de fuentes: CMake embebe WebUI/dist y el
// ResourceProvider resuelve rutas exactas dentro de ese dist. Por eso el
// output NO lleva hash en el nombre: assets/index.js y assets/index.css son
// estables entre builds, y el provider puede encontrarlos por ruta sin un
// manifiesto de hashes embebido.
//
// `base: './'` mantiene las referencias relativas: el documento se sirve desde
// la raiz del provider (juce://juce.backend/), asi que ./assets/index.js
// resuelve contra esa misma raiz.
import { defineConfig } from 'vite';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const webUiRoot = path.dirname(fileURLToPath(import.meta.url));

// El keybed NO se copia aqui: se monta el paquete compartido de ABDShared, el
// mismo que usa ABDMS2000 (`@abdsynths/midi-keyb`). Como este repo no es un
// workspace de ABDSynths, la resolucion se hace con alias sobre el checkout
// hermano. ABD_SHARED_DIR permite apuntar a otra copia (CI, otra maquina).
const sharedRoot = process.env.ABD_SHARED_DIR
  ? path.resolve(process.env.ABD_SHARED_DIR)
  : path.resolve(webUiRoot, '../../ABDShared');

const keyboardSource = path.join(sharedRoot, 'Code', 'MidiKeyboard', 'src', 'keyboard.js');

if (!fs.existsSync(keyboardSource)) {
  throw new Error(
    `No se encontro el keybed compartido en ${keyboardSource}.\n` +
      'Clona ABDShared junto a este repo (D:/desarrollos/ABDSynths/ABDShared) ' +
      'o define ABD_SHARED_DIR apuntando a su raiz.',
  );
}

export default defineConfig({
  root: webUiRoot,
  base: './',
  resolve: {
    alias: [
      {
        find: /^@abdsynths\/midi-keyb\/keyboard\.css$/,
        replacement: path.join(sharedRoot, 'Code', 'MidiKeyboard', 'src', 'keyboard.css'),
      },
      {
        find: /^@abdsynths\/midi-keyb$/,
        replacement: keyboardSource,
      },
      {
        find: /^@abdsynths\/shared\/components\/wheel\.js$/,
        replacement: path.join(sharedRoot, 'Components', 'wheel.js'),
      },
      {
        // @import de keyboard.css: styles/components/*.css
        find: /^@abdsynths\/shared\/styles\//,
        replacement: path.join(sharedRoot, 'Assets', 'styles') + path.sep,
      },
    ],
  },
  build: {
    outDir: 'dist',
    emptyOutDir: true,
    // Sin inlining en base64: cada fichero se pide al provider por su ruta.
    assetsInlineLimit: 0,
    rollupOptions: {
      output: {
        entryFileNames: 'assets/[name].js',
        chunkFileNames: 'assets/[name].js',
        assetFileNames: 'assets/[name][extname]',
      },
    },
  },
});
