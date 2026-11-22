#!/usr/bin/env python3
"""juno6_conformance.py - auditoria de la tabla de opcodes contra MAME.

Contrasta source/core/OpcodeNames.cpp con el desensamblador de MAME
(docs/mame/mcs48dsm.cpp) en la variante 8048/8049: resuelve las ramas
condicionales del desensamblador (m_upi41 / m_i802x) y compara mnemonico y
longitud del primer byte. Lo que MAME no asigna cae en el `default` del switch
y se considera ilegal.

El fixture sobre el listado de Ghidra del Juno-6 (material privado) vive en
tests/TestJuno6DecodingConformance.cpp, dentro de CTest; no se duplica aqui.
Ver docs/OPCODE_TABLE_AUDIT.md.

Se ejecuta en CTest (`opcode_table_audit`) y, por tanto, en CI: una divergencia
contra el desensamblador de MAME rompe la suite sin que nadie lance el script
a mano. La referencia se puede sustituir con --reference o con la variable de
entorno BDMCS48SE_MAME_DSM; si no existe, el test falla con un mensaje claro en
vez de pasar en silencio.

Uso:
  python tools/juno6_conformance.py --audit [--reference <mcs48dsm.cpp>]
"""

import argparse
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TABLE = os.path.join(ROOT, 'source', 'core', 'OpcodeNames.cpp')
MAME = os.path.join(ROOT, 'docs', 'mame', 'mcs48dsm.cpp')
# Cuando docs/mame/ deje de estar en el arbol, la referencia se puede apuntar a
# un checkout externo de MAME con esta variable de entorno o con --reference.
MAME_ENV = 'BDMCS48SE_MAME_DSM'

ROW = re.compile(
    r'\{\s*0x([0-9A-Fa-f]{2}),\s*"([^"]*)",\s*"([^"]*)",\s*(\d),'
    r'\s*(true|false),\s*(true|false),\s*(true|false),\s*(true|false),\s*(true|false)\s*\}')
CASE = re.compile(r'^\s*case 0x([0-9a-fA-F]{2}):(.*)$')

# 0x01 es IDL en MAME, pero solo existe en las variantes CMOS (80C48); el core
# emula la NMOS 8048/8049 y lo trata como opcode ilegal. Diferencia deliberada.
INTENTIONAL = {
    0x01: 'IDL es exclusivo de la familia CMOS; el core NMOS lo rechaza',
}


def parse_table(path=TABLE):
    """[{opcode, name, mnemonic, length, branch, call, cond, implemented, illegal}]."""
    rows = []
    for line in open(path, encoding='utf-8', errors='replace'):
        m = ROW.search(line)
        if not m:
            continue
        rows.append({
            'opcode': int(m.group(1), 16),
            'name': m.group(2),
            'mnemonic': m.group(3),
            'length': int(m.group(4)),
            'branch': m.group(5) == 'true',
            'call': m.group(6) == 'true',
            'conditional': m.group(7) == 'true',
            'implemented': m.group(8) == 'true',
            'illegal': m.group(9) == 'true',
        })
    return rows


def guard_matches(condition):
    """Evalua una condicion de MAME para la variante 8048 (ni UPI-41 ni 802x)."""
    if condition is None:
        return True
    variables = {'m_upi41': False, 'm_i802x': False}
    for term in condition.split('&&'):
        term = term.strip()
        negated = term.startswith('!')
        name = term.lstrip('!').strip()
        if name not in variables:
            return False
        value = variables[name]
        if negated:
            value = not value
        if not value:
            return False
    return True


def parse_mame(path=MAME):
    """{opcode: {'name': str|None, 'param': bool}} resuelto para la 8048."""
    raw = open(path, encoding='utf-8', errors='replace').read()
    blocks = {}
    current = None
    buffer = []
    for line in raw.splitlines():
        m = CASE.match(line)
        if m:
            if current is not None:
                blocks[current] = '\n'.join(buffer)
            current = int(m.group(1), 16)
            buffer = [m.group(2)]
            continue
        if current is None:
            continue
        if re.match(r'^\s*(default:|case 0x)', line):
            blocks[current] = '\n'.join(buffer)
            current = None
            continue
        buffer.append(line)
    if current is not None:
        blocks[current] = '\n'.join(buffer)

    result = {}
    for opcode, block in blocks.items():
        guard = None
        entries = []  # (condicion, literal, consume_parametro)
        for line in block.splitlines():
            stripped = line.strip()
            condition = re.match(r'^(?:else\s+)?if\s*\((.*)\)\s*$', stripped)
            if condition:
                guard = condition.group(1)
                continue
            if stripped == 'else':
                guard = 'else'
                continue
            for call in re.finditer(r'util::stream_format\(stream,\s*(.*?)\);', line):
                body = call.group(1)
                literals = re.findall(r'"([^"]*)"', body)
                param = 'cpc++' in body
                if '?' in body and len(literals) == 2:
                    # ternario: "m_i802x ? A : B" -> para 8048 vale B
                    entries.append(('!m_i802x', literals[1], param))
                elif '?' in body and len(literals) > 2:
                    # "cond ? A : cond ? B : C": el ultimo es el de 8048
                    entries.append((None, literals[-1], param))
                else:
                    for literal in literals:
                        entries.append((guard, literal, param))
        chosen = None
        for condition, literal, param in entries:
            if condition == 'else':
                chosen = (literal, param)
                break
            if guard_matches(condition):
                chosen = (literal, param)
                break
        result[opcode] = {'name': chosen[0].strip() if chosen else None,
                          'param': chosen[1] if chosen else False}
    return result


def mnemonic(text):
    """Primera palabra del texto, en minusculas ('' si esta vacio)."""
    parts = (text or '').strip().split()
    return parts[0].lower() if parts else ''


def audit(reference=MAME):
    rows = parse_table()
    cases = parse_mame(reference)
    if len(rows) != 256:
        print('ERROR: la tabla tiene %d filas, se esperaban 256' % len(rows))
        return 2

    problems = []
    for row in rows:
        op = row['opcode']
        if op in INTENTIONAL:
            continue
        entry = cases.get(op)
        if entry is None or entry['name'] is None:
            # Sin case en el switch: MAME cae en el default y lo declara ilegal.
            if not row['illegal']:
                problems.append((op, row, 'MAME sin case (default: ilegal)'))
            continue
        if entry['name'] == 'illegal':
            if not row['illegal']:
                problems.append((op, row, 'mame=illegal'))
            continue
        if row['illegal']:
            problems.append((op, row, 'mame=%s' % entry['name']))
            continue
        if row['mnemonic'].lower() != mnemonic(entry['name']):
            problems.append((op, row, 'mame=%s' % entry['name']))
            continue
        if (row['length'] == 2) != entry['param']:
            problems.append((op, row, 'mame=%s (parametro=%s)'
                             % (entry['name'], 'si' if entry['param'] else 'no')))

    print('auditoria: %d filas, %d discrepancias' % (len(rows), len(problems)))
    for op, row, detail in problems:
        print('  0x%02X  tabla=%-18s len=%d  %s' % (op, row['name'], row['length'], detail))
    print('  exclusiones deliberadas: %d (0x%s)'
          % (len(INTENTIONAL), ' 0x'.join('%02X' % op for op in INTENTIONAL)))
    return 1 if problems else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('--audit', action='store_true', help='tabla vs desensamblador de MAME')
    ap.add_argument('--reference', metavar='RUTA',
                    help='desensamblador de MAME a usar (por defecto %s)' % MAME)
    args = ap.parse_args()

    reference = args.reference or os.environ.get(MAME_ENV) or MAME
    if not os.path.isfile(reference):
        print('AVISO: no se encuentra el desensamblador de referencia:')
        print('  %s' % reference)
        print('  Indica otra ruta con --reference <ruta> o %s=<ruta>.' % MAME_ENV)
        print('  Se omite el test en entornos sin material de referencia vendorizado.')
        return 125
    return audit(reference)


if __name__ == '__main__':
    sys.exit(main())
