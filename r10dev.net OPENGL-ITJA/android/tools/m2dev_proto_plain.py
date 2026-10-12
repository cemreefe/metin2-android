#!/usr/bin/env python3
"""Re-encode m2dev-client item_proto/mob_proto for this client.

m2dev-client stores the protos as LZO blocks encrypted with libsodium XChaCha20
(key derived from the classic 4-DWORD proto key). This client reads the older
layout, so the payload is decrypted and written back as an unencrypted MCOZ block.

usage: m2dev_proto_plain.py <in> <out> item|mob
"""
import ctypes
import ctypes.util
from pathlib import Path
import struct
import sys

KEYS = {
    'item': (173217, 72619434, 408587239, 27973291),
    'mob': (4813894, 18955, 552631, 6822045),
}


def load_sodium():
    name = ctypes.util.find_library('sodium')
    if not name:
        sys.exit('libsodium not found (apt install libsodium23)')
    lib = ctypes.CDLL(name)
    assert lib.sodium_init() >= 0
    return lib


def derive(sod, key):
    kb = struct.pack('<4I', *key)
    dk = ctypes.create_string_buffer(32)
    sod.crypto_generichash(dk, 32, kb, 16, b'M2DevPackEncrypt', 16)
    ns = ctypes.create_string_buffer(32)
    sod.crypto_generichash(ns, 32, kb, 16, b'M2DevNonce', 10)
    return dk.raw, ns.raw[:24]


def decrypt_tea(data, key):
    output = bytearray()
    for offset in range(0, len(data), 8):
        y, z = struct.unpack_from('<II', data, offset)
        total = 0x9E3779B9 * 32 & 0xFFFFFFFF
        for _ in range(32):
            z = (z - ((((y << 4) ^ (y >> 5)) + y) ^
                      (total + key[(total >> 11) & 3]))) & 0xFFFFFFFF
            total = (total - 0x9E3779B9) & 0xFFFFFFFF
            y = (y - ((((z << 4) ^ (z >> 5)) + z) ^
                      (total + key[total & 3]))) & 0xFFFFFFFF
        output.extend(struct.pack('<II', y, z))
    return bytes(output)


def lzo_decompress(payload, size):
    lib = ctypes.CDLL(ctypes.util.find_library('lzo2'))
    output = ctypes.create_string_buffer(size)
    length = ctypes.c_ulong(size)
    if lib.lzo1x_decompress_safe(payload, len(payload), output, ctypes.byref(length), None) != 0:
        raise ValueError('invalid LZO proto payload')
    if length.value != size:
        raise ValueError('invalid LZO proto size')
    return output.raw


def lzo_compress(data):
    lib = ctypes.CDLL(ctypes.util.find_library('lzo2'))
    output = ctypes.create_string_buffer(len(data) + len(data) // 16 + 67)
    length = ctypes.c_ulong(len(output))
    work = ctypes.create_string_buffer(16384 * ctypes.sizeof(ctypes.c_void_p))
    if lib.lzo1x_1_compress(data, len(data), output, ctypes.byref(length), work) != 0:
        raise ValueError('LZO compression failed')
    return output.raw[:length.value]


def encode_names(header, data, encoding):
    count = struct.unpack_from('<I', header, len(header) - 4)[0]
    if not count or len(data) % count:
        raise ValueError('invalid proto row count')
    stride = len(data) // count
    start = 8 if header[:4] == b'MIPX' else 4
    width = 65
    if stride < start + 2 * width:
        raise ValueError('unsupported proto name layout')
    output = bytearray(data)
    for row in range(count):
        for offset in (start, start + width):
            begin = row * stride + offset
            field = data[begin:begin + width]
            name = field.split(b'\0', 1)[0]
            try:
                encoded = name.decode('utf-8').encode(encoding)
            except UnicodeDecodeError:
                continue
            if len(encoded) >= width:
                raise ValueError('encoded proto name is too long')
            output[begin:begin + width] = encoded.ljust(width, b'\0')
    return bytes(output)


def convert(src_path, out_path, kind, encoding=None):
    sod = load_sodium()
    d = Path(src_path).read_bytes()
    hdr_len = 20 if d[:4] == b'MIPX' else 12
    outer = d[:hdr_len - 4]
    lz = d[hdr_len:]
    lfcc, enc, comp, real = struct.unpack_from('<4sIII', lz)
    if lfcc != b'MCOZ':
        sys.exit('%s: not an MCOZ proto' % src_path)
    if enc == 0 and encoding is None:
        Path(out_path).write_bytes(d)
        return
    if enc:
        size = (enc + 7) // 8 * 8
        src = lz[16:16 + enc].ljust(size, b'\0')
        dk, nonce = derive(sod, KEYS[kind])
        dst = ctypes.create_string_buffer(size)
        sod.crypto_stream_xchacha20_xor(dst, src, ctypes.c_ulonglong(size), nonce, dk)
        plain = dst.raw
        if plain[:4] != b'MCOZ':
            if decrypt_tea(src[:8], KEYS[kind])[:4] != b'MCOZ':
                sys.exit('%s: decryption failed (wrong key or format)' % src_path)
            plain = decrypt_tea(src, KEYS[kind])
    else:
        plain = lz[16:]
    payload = plain[4:4 + comp]
    if encoding:
        decoded = lzo_decompress(payload, real)
        encoded = encode_names(outer, decoded, encoding)
        if encoded != decoded:
            payload = lzo_compress(encoded)
    body = struct.pack('<4sIII', b'MCOZ', 0, len(payload), real) + b'MCOZ' + payload
    Path(out_path).write_bytes(outer + struct.pack('<I', len(body)) + body)


if __name__ == '__main__':
    if len(sys.argv) != 4 or sys.argv[3] not in KEYS:
        sys.exit(__doc__)
    convert(*sys.argv[1:4])
