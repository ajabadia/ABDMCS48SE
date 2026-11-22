const fs = require('fs');
const path = require('path');
const crypto = require('crypto');
const { execSync } = require('child_process');

const workspaceDir = path.resolve('workspace');
const romsDestDir = path.join(workspaceDir, 'roms');
const firmwaresUserDir = path.join(workspaceDir, 'firmwares', 'user');

fs.mkdirSync(romsDestDir, { recursive: true });
fs.mkdirSync(firmwaresUserDir, { recursive: true });

function sha256(buffer) {
  return crypto.createHash('sha256').update(buffer).digest('hex');
}

// 1. Extraer archivos temporales si están en ZIP
const tmpExtractDir = path.join(workspaceDir, '.tmp_extract');
fs.mkdirSync(tmpExtractDir, { recursive: true });

// Extraer Juno-6 de juno6.zip
const junoZip = 'D:/desarrollos/ABDSynths/ABDJuno6ROMRE/juno6.zip';
const junoRomOut = path.join(tmpExtractDir, 'upd8049c-238.ic56');
if (fs.existsSync(junoZip)) {
  try {
    execSync(`tar -xf "${junoZip}" -C "${tmpExtractDir}"`);
  } catch (e) {
    console.warn('Could not extract juno6.zip with tar:', e.message);
  }
}

// Extraer Odyssey 2 BIOS
const o2Zip = 'D:/desarrollos/ABDSynths/_RESOURCES/ABDMCS48se/Magnavox_Odyssey_2_TOSEC_2012_04_23/Magnavox Odyssey 2 [TOSEC]/Magnavox Odyssey2 - Firmware (TOSEC-v2011-02-22_CM)/Magnavox Odyssey2 BIOS (1978)(Philips)(EU-US).zip';
if (fs.existsSync(o2Zip)) {
  try {
    execSync(`tar -xf "${o2Zip}" -C "${tmpExtractDir}"`);
  } catch (e) {
    console.warn('Could not extract o2Zip with tar:', e.message);
  }
}

// Extraer Videopac C52 BIOS
const c52Zip = 'D:/desarrollos/ABDSynths/_RESOURCES/ABDMCS48se/Magnavox_Odyssey_2_TOSEC_2012_04_23/Magnavox Odyssey 2 [TOSEC]/Magnavox Odyssey2 - Firmware (TOSEC-v2011-02-22_CM)/Philips C52 BIOS (19xx)(Philips)(FR).zip';
if (fs.existsSync(c52Zip)) {
  try {
    execSync(`tar -xf "${c52Zip}" -C "${tmpExtractDir}"`);
  } catch (e) {
    console.warn('Could not extract c52Zip with tar:', e.message);
  }
}

// Lista de ROMs a procesar
const romCatalog = [
  {
    packageId: 'korg-polysix-klm8049',
    packageName: 'Korg PolySix Key Assigner (KLM-366)',
    sourceFile: 'D:/desarrollos/ABDSynths/_RESOURCES/ABDMCS48se/Korg PolySix (1981)/Korg PolySix - (klm8049-klm8048)/Korg PolySix - klm8049.bin',
    targetRomName: 'korg-polysix-klm8049.bin',
    cpuVariant: '8049',
    clockHz: 6000000,
    author: 'Korg Inc. (1981)',
    description: 'Intel 8049 Voice & Key Assigner ROM for Korg PolySix (KLM-366)',
    license: 'Proprietary / Archival Preserved',
    provenance: 'DBWBP Synth ROM Archive'
  },
  {
    packageId: 'korg-polysix-klm8048',
    packageName: 'Korg PolySix Programmer (KLM-367)',
    sourceFile: 'D:/desarrollos/ABDSynths/_RESOURCES/ABDMCS48se/Korg PolySix (1981)/Korg PolySix - (klm8049-klm8048)/Korg PolySix - klm8048.bin',
    targetRomName: 'korg-polysix-klm8048.bin',
    cpuVariant: '8048',
    clockHz: 6000000,
    author: 'Korg Inc. (1981)',
    description: 'Intel 8048 Programmer & Memory Control ROM for Korg PolySix (KLM-367)',
    license: 'Proprietary / Archival Preserved',
    provenance: 'DBWBP Synth ROM Archive'
  },
  {
    packageId: 'roland-jupiter4-cpu',
    packageName: 'Roland Jupiter-4 Key Assigner & Arpeggiator',
    sourceFile: 'D:/desarrollos/ABDSynths/_RESOURCES/ABDMCS48se/Roland Jupiter 4 8048-012 firmware (main board CPU)/JP4_D8048C-012_5996.bin',
    targetRomName: 'roland-jupiter4-8048.bin',
    cpuVariant: '8048',
    clockHz: 6000000,
    author: 'Roland Corp. (1978)',
    description: 'NEC uPD8048C-012 Main Board Key Assigner and Arpeggiator firmware for Roland Jupiter-4',
    license: 'Proprietary / Archival Preserved',
    provenance: 'DBWBP Synth ROM Archive'
  },
  {
    packageId: 'roland-juno6-assigner',
    packageName: 'Roland Juno-6 Voice Assigner (uPD8049C-238)',
    sourceFile: path.join(tmpExtractDir, 'upd8049c-238.ic56'),
    targetRomName: 'roland-juno6-upd8049c-238.bin',
    cpuVariant: '8049',
    clockHz: 6000000,
    author: 'Roland Corp. (1982)',
    description: 'NEC uPD8049C-238 Key Assigner & Arpeggiator firmware for Roland Juno-6',
    license: 'Proprietary / Archival Preserved',
    provenance: 'ABDJuno6ROMRE'
  },
  {
    packageId: 'mario-bros-sound-board',
    packageName: 'Nintendo Mario Bros. Sound Board (tma1-c-6k)',
    sourceFile: 'D:/desarrollos/ABDSynths/_RESOURCES/ABDMCS48se/mario/tma1-c-6k_e.6k',
    targetRomName: 'mario-bros-tma1-c-6k.rom',
    cpuVariant: '8039',
    clockHz: 7372800,
    author: 'Nintendo Co., Ltd. (1983)',
    description: 'Intel 8039 audio processor ROM for Nintendo Mario Bros. Arcade Sound Board',
    license: 'Proprietary / Archival Preserved',
    provenance: 'MAME Mario Bros. Romset'
  },
  {
    packageId: 'mario-masao-sound-board',
    packageName: 'Masao Sound Board (masao-7)',
    sourceFile: 'D:/desarrollos/ABDSynths/_RESOURCES/ABDMCS48se/mario/masao/masao-7.rom',
    targetRomName: 'mario-masao-7.rom',
    cpuVariant: '8039',
    clockHz: 6000000,
    author: 'Masao Bootleg (1983)',
    description: 'Intel 8039 audio processor ROM for Masao (Mario Bros. bootleg)',
    license: 'Archival Bootleg',
    provenance: 'MAME Masao Romset'
  },
  {
    packageId: 'donkey-kong-sound-board',
    packageName: 'Nintendo Donkey Kong Sound CPU (c_5et_g)',
    sourceFile: 'D:/desarrollos/ABDSynths/_RESOURCES/ABDMCS48se/dkong/c_5et_g.bin',
    targetRomName: 'donkey-kong-c_5et_g.bin',
    cpuVariant: '8035',
    clockHz: 6000000,
    author: 'Nintendo Co., Ltd. (1981)',
    description: 'Intel 8035 audio processor ROM for Nintendo Donkey Kong Arcade',
    license: 'Proprietary / Archival Preserved',
    provenance: 'MAME Donkey Kong Romset'
  },
  {
    packageId: 'magnavox-odyssey2-bios',
    packageName: 'Magnavox Odyssey² Console BIOS',
    sourceFile: path.join(tmpExtractDir, 'Magnavox Odyssey2 BIOS (1978)(Philips)(EU-US).bin'),
    targetRomName: 'magnavox-odyssey2-bios.bin',
    cpuVariant: '8048',
    clockHz: 5370000,
    author: 'Magnavox / Philips (1978)',
    description: 'Intel 8048 internal BIOS ROM for Magnavox Odyssey² console',
    license: 'Proprietary / Archival Preserved',
    provenance: 'TOSEC Magnavox Odyssey 2'
  },
  {
    packageId: 'philips-videopac-c52-bios',
    packageName: 'Philips Videopac C52 BIOS (PAL)',
    sourceFile: path.join(tmpExtractDir, 'Philips C52 BIOS (19xx)(Philips)(FR).bin'),
    targetRomName: 'philips-videopac-c52-bios.bin',
    cpuVariant: '8048',
    clockHz: 5910000,
    author: 'Philips (1979)',
    description: 'Intel 8048 internal BIOS ROM for Philips Videopac C52 (PAL)',
    license: 'Proprietary / Archival Preserved',
    provenance: 'TOSEC Philips Videopac'
  }
];

console.log('--- Converting ROMs to BDMCS48SE Packages ---');

let successCount = 0;

for (const entry of romCatalog) {
  if (!fs.existsSync(entry.sourceFile)) {
    console.warn(`[SKIP] Source file not found: ${entry.sourceFile}`);
    continue;
  }

  const romBuffer = fs.readFileSync(entry.sourceFile);
  const size = romBuffer.length;
  const hash = sha256(romBuffer);

  // 1. Copiar a workspace/roms/
  const destRomInWorkspace = path.join(romsDestDir, entry.targetRomName);
  fs.writeFileSync(destRomInWorkspace, romBuffer);

  // 2. Crear paquete en workspace/firmwares/user/<packageId>/
  const pkgDir = path.join(firmwaresUserDir, entry.packageId);
  const pkgRomsDir = path.join(pkgDir, 'roms');
  fs.mkdirSync(pkgRomsDir, { recursive: true });

  // Determinar parámetros de memoria de programa según la arquitectura fija del core MCS-48
  // (core::ProgramMemory: 12 bits = 4096 bytes, 2 bancos de 2048 bytes)
  const addressBits = 12;
  const bankSizeBytes = 2048;
  const bankCount = 2;

  let images = [];
  if (size <= 2048) {
    const pkgRomFile = path.join(pkgRomsDir, 'program.rom');
    fs.writeFileSync(pkgRomFile, romBuffer);
    images.push({
      id: 'program',
      file: 'roms/program.rom',
      region: 'program',
      bank: 0,
      offset: 0,
      size,
      sha256: hash
    });
  } else {
    // ROM de 4KB (por ejemplo 8039 o 8035 con 2 bancos de 2KB)
    const chunk0 = romBuffer.subarray(0, 2048);
    const chunk1 = romBuffer.subarray(2048, Math.min(size, 4096));

    const pkgRomFile0 = path.join(pkgRomsDir, 'program-bank0.rom');
    fs.writeFileSync(pkgRomFile0, chunk0);
    images.push({
      id: 'program-bank0',
      file: 'roms/program-bank0.rom',
      region: 'program',
      bank: 0,
      offset: 0,
      size: chunk0.length,
      sha256: sha256(chunk0)
    });

    const pkgRomFile1 = path.join(pkgRomsDir, 'program-bank1.rom');
    fs.writeFileSync(pkgRomFile1, chunk1);
    images.push({
      id: 'program-bank1',
      file: 'roms/program-bank1.rom',
      region: 'program',
      bank: 1,
      offset: 0,
      size: chunk1.length,
      sha256: sha256(chunk1)
    });
  }

  const manifest = {
    format: 'bdmcs48se-firmware',
    formatVersion: 1,
    id: entry.packageId,
    name: entry.packageName,
    version: '1.0.0',
    author: entry.author,
    description: entry.description,
    license: entry.license,
    provenance: entry.provenance,
    machine: {
      profile: 'generic-mcs48',
      cpu: {
        family: 'mcs48',
        variant: entry.cpuVariant,
        clockHz: entry.clockHz
      },
      programMemory: {
        addressBits,
        bankSizeBytes,
        bankCount
      }
    },
    images,
    inputs: [],
    outputs: [
      { id: 'audio', type: 'mono_stream' }
    ],
    parameters: [
      { id: 'engine_clock', type: 'float', minimum: 0.0625, maximum: 8.0, initial: 1.0, hostAutomatable: true }
    ]
  };

  const manifestPath = path.join(pkgDir, 'firmware.json');
  fs.writeFileSync(manifestPath, JSON.stringify(manifest, null, 2), 'utf8');

  console.log(`[OK] Converted: ${entry.packageName}`);
  console.log(`     Package ID: ${entry.packageId}`);
  console.log(`     Size: ${size} B | CPU: ${entry.cpuVariant} | Clock: ${entry.clockHz} Hz | SHA-256: ${hash.slice(0, 16)}...`);
  successCount++;
}

// Limpiar directorio temporal
try {
  fs.rmSync(tmpExtractDir, { recursive: true, force: true });
} catch {}

console.log(`\nSuccessfully converted ${successCount} ROMs into ${firmwaresUserDir}!`);
