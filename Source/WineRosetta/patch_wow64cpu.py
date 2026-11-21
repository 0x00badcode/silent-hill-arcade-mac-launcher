#!/usr/bin/env python3
"""Apply the Rosetta far-call/far-return workaround to Gcenx Wine Staging 11.21.

The instruction sequence follows CodeWeavers' workaround 20760, as published in
https://github.com/dttdrv/wiage/blob/3f52c197d9fc9569a09d224b861e3010b397cb03/dlls/wow64cpu/cpu.c.
The original runtime's exports, thunk addresses, context layout, and imports stay
compatible. The added leaf stubs occupy verified padding in the existing text page.
"""
from hashlib import sha256
from pathlib import Path
import struct
import sys

ORIGINAL_SHA256 = '261da16d45a5196eb64a1173387120f8b0422ac4e9c74b3a4edb234385cb5fa5'
SYSCALL_ENTRY = 0x110c
UNIX_ENTRY = 0x1210
SYSCALL_GATE = 0x1e00
UNIX_GATE = 0x1e10
RETURN_GATE = 0x1e20


def relative_jump(source, target):
    return b'\xe9' + struct.pack('<i', target - source - 5)


def patch(source, destination):
    data = bytearray(source.read_bytes())
    if sha256(data).hexdigest() != ORIGINAL_SHA256:
        raise ValueError('Unexpected wow64cpu.dll build')

    pe = struct.unpack_from('<I', data, 0x3c)[0]
    count = struct.unpack_from('<H', data, pe + 6)[0]
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    table = pe + 24 + optional_size
    sections = []
    for index in range(count):
        header = table + index * 40
        virtual_size, rva, raw_size, raw_offset = struct.unpack_from('<IIII', data, header + 8)
        sections.append((rva, raw_size, raw_offset, header, virtual_size))

    def file_offset(rva):
        for start, size, offset, *_ in sections:
            if start <= rva < start + size:
                return offset + rva - start
        raise ValueError(f'Unmapped RVA {rva:#x}')

    def replace(rva, expected, replacement):
        offset = file_offset(rva)
        expected = bytes.fromhex(expected) if isinstance(expected, str) else expected
        if len(expected) != len(replacement) or data[offset:offset + len(expected)] != expected:
            raise ValueError(f'Unexpected instructions at {rva:#x}')
        data[offset:offset + len(expected)] = replacement

    # 32 -> 64. A far call switches CS and pushes a return frame. Discard that
    # eight-byte frame before entering the original 64-bit syscall/unix routines.
    syscall_gate = b'\x83\xc4\x08' + relative_jump(SYSCALL_GATE + 3, SYSCALL_ENTRY)
    unix_gate = b'\x83\xc4\x08' + relative_jump(UNIX_GATE + 3, UNIX_ENTRY)

    # 64 -> 32. Recreate the saved far target as two 64-bit stack entries and
    # use lretq, matching CodeWeavers' SIGUSR1/Rosetta workaround.
    return_gate = bytes.fromhex(
        '49 87 e6'           # xchg r14,rsp
        '48 83 ec 10'        # sub rsp,16
        '41 8b 56 04'        # mov edx,[r14+4]   saved CS
        '48 89 54 24 08'     # mov [rsp+8],rdx
        '41 8b 16'           # mov edx,[r14]     saved EIP
        '48 89 14 24'        # mov [rsp],rdx
        '48 cb'              # lretq
    )
    for rva, code in ((SYSCALL_GATE, syscall_gate), (UNIX_GATE, unix_gate), (RETURN_GATE, return_gate)):
        replace(rva, b'\xcc' * len(code), code)

    # Generated 32-bit gates keep their original twelve-byte layout and addresses.
    replace(0x1402, '66 c7 00 ff 2d', bytes.fromhex('66 c7 00 ff 1d'))
    replace(0x140e, '48 8d 0d f7 fc ff ff',
            bytes.fromhex('48 8d 0d') + struct.pack('<i', SYSCALL_GATE - 0x1415))
    replace(0x1423, '66 c7 40 0c ff 2d', bytes.fromhex('66 c7 40 0c ff 1d'))
    replace(0x1430, '48 8d 15 d9 fd ff ff',
            bytes.fromhex('48 8d 15') + struct.pack('<i', UNIX_GATE - 0x1437))

    # Both return paths use the same leaf gate. The reset-state iretq path
    # continues to restore the complete CPU context using the original routine.
    for rva in (0x11a9, 0x1292):
        replace(rva, '49 87 e6 41 ff 2e', relative_jump(rva, RETURN_GATE) + b'\x90')

    text = next(section for section in sections if section[0] == 0x1000)
    new_size = RETURN_GATE + len(return_gate) - text[0]
    struct.pack_into('<I', data, text[3] + 8, max(text[4], new_size))
    destination.write_bytes(data)
    print(f'Original {ORIGINAL_SHA256}')
    print(f'Patched  {sha256(data).hexdigest()}')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('Usage: patch_wow64cpu.py ORIGINAL_DLL PATCHED_DLL')
    patch(Path(sys.argv[1]), Path(sys.argv[2]))
