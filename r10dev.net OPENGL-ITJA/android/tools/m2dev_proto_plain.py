#!/usr/bin/env python3
"""Re-encode m2dev-client item_proto/mob_proto for this client.

m2dev-client stores the protos as LZO blocks encrypted with libsodium XChaCha20
(key derived from the classic 4-DWORD proto key). This client reads the older
layout, so the payload is decrypted and written back as an unencrypted MCOZ block.

usage: m2dev_proto_plain.py <in> <out> item|mob
"""
import ctypes
import ctypes.util
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


def convert(src_path, out_path, kind):
    sod = load_sodium()
    d = open(src_path, 'rb').read()
    hdr_len = 20 if d[:4] == b'MIPX' else 12
    outer = d[:hdr_len - 4]
    lz = d[hdr_len:]
    lfcc, enc, comp, real = struct.unpack_from('<4sIII', lz)
    if lfcc != b'MCOZ':
        sys.exit('%s: not an MCOZ proto' % src_path)
    if enc == 0:
        open(out_path, 'wb').write(d)
        return
    size = (enc + 7) // 8 * 8
    src = lz[16:16 + enc].ljust(size, b'\0')
    dk, nonce = derive(sod, KEYS[kind])
    dst = ctypes.create_string_buffer(size)
    sod.crypto_stream_xchacha20_xor(dst, src, ctypes.c_ulonglong(size), nonce, dk)
    plain = dst.raw
    if plain[:4] != b'MCOZ':
        sys.exit('%s: decryption failed (wrong key or format)' % src_path)
    body = struct.pack('<4sIII', b'MCOZ', 0, comp, real) + b'MCOZ' + plain[4:4 + comp]
    open(out_path, 'wb').write(outer + struct.pack('<I', len(body)) + body)


if __name__ == '__main__':
    if len(sys.argv) != 4 or sys.argv[3] not in KEYS:
        sys.exit(__doc__)
    convert(*sys.argv[1:4])
