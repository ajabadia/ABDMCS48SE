#!/usr/bin/env python3
"""asm_to_package.py - Herramienta para convertir ensamblado o binarios en paquetes de firmware C++.

Toma imágenes binarias o volcados por banco, los rellena a 2048 bytes exactos,
calcula los hashes SHA-256 canónicos, y genera:
  1. Archivo fuente C++ con los arrays de bytes (`kBank0Code`, `kBank1Code`).
  2. Manifiesto JSON formal conforme a docs/FIRMWARE_PACKAGE.md.
  3. Paquete exportable (.json o directorio).

Uso:
  python tools/asm_to_package.py --id <id> --name <nombre> --bank0 <bin0> [--bank1 <bin1>] --out-cpp <archivo.cpp>
"""

import argparse
import hashlib
import json
import os
import sys

BANK_SIZE = 2048


def sha256_of(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def pad_bank(data: bytes, fill: int = 0x00) -> bytes:
    if len(data) > BANK_SIZE:
        raise ValueError(f"El banco excede los {BANK_SIZE} bytes (longitud: {len(data)})")
    return data + bytes([fill] * (BANK_SIZE - len(data)))


def format_cpp_array(name: str, data: bytes) -> str:
    lines = [f"constexpr std::uint8_t {name}[{len(data)}] = {{"]
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_values = ", ".join(f"0x{b:02x}" for b in chunk)
        comma = "," if i + 16 < len(data) else ""
        lines.append(f"    {hex_values}{comma}")
    lines.append("};")
    return "\n".join(lines)


def generate_package_json(firmware_id: str, name: str, bank0_hash: str, bank1_hash: str = None) -> dict:
    pkg = {
        "id": firmware_id,
        "name": name,
        "version": "1.0.0",
        "targetMachine": "generic_mcs48",
        "author": "ABDMCS48SE",
        "description": f"Paquete de firmware generado automáticamente para {name}",
        "machine": {
            "programMemory": {
                "addressBits": 12,
                "bankSizeBytes": BANK_SIZE,
                "bankCount": 2 if bank1_hash else 1
            }
        },
        "images": [
            {
                "region": "program",
                "bank": 0,
                "offset": 0,
                "size": BANK_SIZE,
                "sha256": bank0_hash
            }
        ]
    }
    if bank1_hash:
        pkg["images"].append({
            "region": "program",
            "bank": 1,
            "offset": 0,
            "size": BANK_SIZE,
            "sha256": bank1_hash
        })
    return pkg


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--id", required=True, help="ID canónico del firmware (ej. mcs48.test.diagnostic)")
    ap.add_argument("--name", required=True, help="Nombre legible del firmware")
    ap.add_argument("--bank0", required=True, help="Ruta al binario del banco 0")
    ap.add_argument("--bank1", help="Ruta al binario del banco 1 (opcional)")
    ap.add_argument("--out-cpp", help="Ruta del archivo C++ de salida")
    ap.add_argument("--out-json", help="Ruta del firmware.json de salida")

    args = ap.parse_args()

    with open(args.bank0, "rb") as f:
        raw_b0 = f.read()
    bank0 = pad_bank(raw_b0)
    hash_b0 = sha256_of(bank0)

    hash_b1 = None
    bank1 = None
    if args.bank1 and os.path.exists(args.bank1):
        with open(args.bank1, "rb") as f:
            raw_b1 = f.read()
        bank1 = pad_bank(raw_b1)
        hash_b1 = sha256_of(bank1)

    pkg_dict = generate_package_json(args.id, args.name, hash_b0, hash_b1)

    if args.out_json:
        with open(args.out_json, "w", encoding="utf-8") as f:
            json.dump(pkg_dict, f, indent=2)
        print(f"Generado manifiesto JSON en {args.out_json}")

    if args.out_cpp:
        manifest_json_str = json.dumps(pkg_dict, indent=2)
        cpp_content = f"""// Generado automaticamente por tools/asm_to_package.py
#include <cstdint>
#include <string_view>

namespace bdmcs48::firmware::generated {{

{format_cpp_array("kBank0Code", bank0)}

"""
        if bank1:
            cpp_content += f"""{format_cpp_array("kBank1Code", bank1)}

"""
        cpp_content += f"""constexpr std::string_view kManifestJson = R"json(
{manifest_json_str}
)json";

}} // namespace bdmcs48::firmware::generated
"""
        with open(args.out_cpp, "w", encoding="utf-8") as f:
            f.write(cpp_content)
        print(f"Generado C++ en {args.out_cpp}")

    print(f"Banco 0 SHA-256: {hash_b0}")
    if hash_b1:
        print(f"Banco 1 SHA-256: {hash_b1}")


if __name__ == "__main__":
    main()
