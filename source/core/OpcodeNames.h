// OpcodeNames.h - MCS-48 opcode names and assembler-style aliases.
//
// The MCS-48 (Intel 8048/8049/8039/8035) encodes each instruction as a single
// opcode byte plus a possible 8-bit argument. This header provides a canonical,
// machine-written table of the opcodes the project core actually executes:
//   - name:          canonical machine name (e.g. "add a, #data")
//   - aliases:       assembler-style spellings used by tests and tooling
//   - mnemonic:      short instruction class (ADD, ORL, DJNZ, ...)
//   - format:        operand pattern (IMM, INDIRECT, REG, JMP, COND, ...)
//   - implemented:   the core has a handler for this opcode
//   - fault:         if false the opcode is unknown and raises IllegalOpcode
//
// The table is generated from the canonical Intel MCS-48 table. Every opcode
// byte 0x00..0xFF is present exactly once, so callers can map opcode -> name,
// name -> mnemonic, or iterate all implemented opcodes without string
// comparisons anywhere in the core.
//
// Sources: Intel MCS-48 User's Manual; MAME src/devices/cpu/mcs48.
#pragma once
