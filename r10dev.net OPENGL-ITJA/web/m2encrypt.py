import os, sys, glob, hashlib
from cryptography.hazmat.primitives.ciphers.aead import AESGCM
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
from cryptography.hazmat.primitives import hashes

pw = sys.argv[1].encode()
kdf = PBKDF2HMAC(algorithm=hashes.SHA256(), length=32, salt=b'm2gate-v1', iterations=100000)
key = kdf.derive(pw)
a = AESGCM(key)
src, dst = sys.argv[2], sys.argv[3]
for p in sorted(glob.glob(os.path.join(src, '*.m2pack'))):
    data = open(p, 'rb').read()
    nonce = os.urandom(12)
    ct = a.encrypt(nonce, data, None)
    out = os.path.join(dst, os.path.basename(p))
    open(out, 'wb').write(b'M2E1' + nonce + ct)
    print(os.path.basename(p), len(data), '->', len(ct) + 16)
