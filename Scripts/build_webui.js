// build_webui.js - Empaqueta el editor WebView antes de configurar CMake.
//
// CMake no compila JavaScript: embebe WebUI/dist tal cual. Si el dist no
// existe, la configuracion falla a proposito en vez de generar un plugin sin
// interfaz, y si existe pero esta viejo, el binario mostraría una UI que ya no
// coincide con las fuentes. Por eso este script es el unico camino soportado:
// `npm install` una vez en WebUI/ y `node Scripts/build_webui.js` en cada build.
import { spawnSync } from 'child_process';
import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const rootDir = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const webUiDir = path.join(rootDir, 'WebUI');
const distIndex = path.join(webUiDir, 'dist', 'index.html');

if (!fs.existsSync(path.join(webUiDir, 'node_modules', 'vite'))) {
  console.error(
    '[ERROR] Falta Vite en WebUI/node_modules. Ejecuta primero:\n' +
      '        npm --prefix WebUI install',
  );
  process.exit(1);
}

// `shell: true` es lo que hace que npm.cmd funcione igual en Windows que en
// POSIX; sin el, spawn busca un ejecutable llamado "npm" y en Windows no existe.
const result = spawnSync('npm', ['run', 'build'], {
  cwd: webUiDir,
  stdio: 'inherit',
  shell: true,
});

if (result.status !== 0) {
  console.error('[ERROR] Vite fallo; no se genera WebUI/dist y CMake no podra embeber la UI.');
  process.exit(result.status ?? 1);
}

if (!fs.existsSync(distIndex)) {
  console.error(`[ERROR] Vite termino sin producir ${distIndex}`);
  process.exit(1);
}

console.log('[OK] WebUI/dist listo para embeber en el binario.');
