# SPDX-License-Identifier: MIT
"""AES-128-CBC with an all-zero key and IV, the cipher of MT7996 firmware containers.

The ROM patch body and every encrypted WM region use it, with the IV reset per region.
Delegated to the openssl command line so the build needs no Python crypto package.
"""
import shutil
import subprocess

_ZERO = '00' * 16


def cbc_zero(data: bytes, encrypt: bool) -> bytes:
    """Encrypt or decrypt whole 16-byte blocks, without padding."""
    if len(data) % 16:
        raise ValueError(f'{len(data)} bytes is not a whole number of AES blocks')
    openssl = shutil.which('openssl')
    if not openssl:
        raise SystemExit('the firmware build needs the openssl command')
    mode = '-e' if encrypt else '-d'
    result = subprocess.run([openssl, 'enc', '-aes-128-cbc', mode, '-nopad', '-K', _ZERO, '-iv', _ZERO],
                            input=data, capture_output=True)
    if result.returncode or len(result.stdout) != len(data):
        raise SystemExit('openssl enc failed: ' + result.stderr.decode(errors='replace').strip())
    return result.stdout
