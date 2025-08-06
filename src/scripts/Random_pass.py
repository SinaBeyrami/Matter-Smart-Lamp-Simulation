import os, binascii
salt     = os.urandom(16)
verifier = os.urandom(40)
print("salt_hex    =", binascii.hexlify(salt).decode())
print("verifier_hex=", binascii.hexlify(verifier).decode())