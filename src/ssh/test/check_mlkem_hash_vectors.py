#!/usr/bin/env python3
"""Verify pinned NIST hash responses, optionally compare a native test driver.

The native driver uses the bundled implementation. Standard answers use
Python's separate hashlib provider; one private-branch model follows FIPS202. It neither implements nor replaces production crypto.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / 'test/data/mlkem_hash_sources.json'
MODES = {
    'SHA3_256': ('sha3-256', hashlib.sha3_256, 32),
    'SHA3_512': ('sha3-512', hashlib.sha3_512, 64),
    'SHAKE128': ('shake128', hashlib.shake_128, None),
    'SHAKE256': ('shake256', hashlib.shake_256, None),
}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def octets(text):
    require(re.fullmatch('[0-9a-f]+', text) is not None and len(text) % 2 == 0,
            'noncanonical hex')
    return bytes.fromhex(text)


def bit_count(text):
    require(re.fullmatch('[0-9]+', text) is not None, 'nondecimal length')
    value = int(text)
    require(value % 8 == 0, 'non-byte input/output')
    return value // 8


def rc_bit(index):
    """FIPS202 Algorithm5, with explicit bit positions rather than a table."""
    register = [1, 0, 0, 0, 0, 0, 0, 0]
    for _ in range(index % 255):
        register = [0] + register
        for position in (0, 4, 5, 6):
            register[position] ^= register[8]
        register = register[:8]
    return register[0]


def raw72_reference(message, length):
    """Test-only rate72/delimiter06 sponge beyond the fixed SHA3 digest extent.

    FIPS202 §§3.1.2,3.2,3.3,4,5 and AppendixB. This model uses literal
    coordinate formulas and generated constants, not the bundled C schedule.
    Its first64 bytes must agree with independent hashlib SHA3-512.
    """
    mask = (1 << 64) - 1
    rotations = {(0, 0): 0}
    x, y = 1, 0
    for step in range(24):
        rotations[x, y] = ((step + 1) * (step + 2) // 2) % 64
        x, y = y, (2 * x + 3 * y) % 5
    constants = [sum(rc_bit(j + 7 * rnd) << ((1 << j) - 1) for j in range(7))
                 for rnd in range(24)]

    def rotate(value, shift):
        return ((value << shift) | (value >> (64 - shift))) & mask if shift else value

    def permute(state):
        for constant in constants:
            columns = [state[x] ^ state[x + 5] ^ state[x + 10] ^ state[x + 15] ^ state[x + 20]
                       for x in range(5)]
            theta = [state[x + 5 * y] ^ columns[(x - 1) % 5] ^ rotate(columns[(x + 1) % 5], 1)
                     for y in range(5) for x in range(5)]
            rho = [rotate(theta[x + 5 * y], rotations[x, y]) for y in range(5) for x in range(5)]
            pi = [rho[(x + 3 * y) % 5 + 5 * x] for y in range(5) for x in range(5)]
            state = [pi[x + 5 * y] ^ ((pi[(x + 1) % 5 + 5 * y] ^ mask) & pi[(x + 2) % 5 + 5 * y])
                     for y in range(5) for x in range(5)]
            state[0] ^= constant
        return state

    padded = bytearray(message) + bytearray(72 - len(message) % 72)
    padded[len(message)] = 0x06
    padded[-1] |= 0x80
    state = [0] * 25
    for offset in range(0, len(padded), 72):
        for word in range(9):
            state[word] ^= int.from_bytes(padded[offset + 8 * word:offset + 8 * (word + 1)], 'little')
        state = permute(state)
    output = bytearray()
    while len(output) < length:
        output.extend(b''.join(word.to_bytes(8, 'little') for word in state[:9]))
        if len(output) < length:
            state = permute(state)
    return bytes(output[:length])


def reference(mode, message, length):
    for name, constructor, fixed in MODES.values():
        if name == mode:
            require(fixed is None or length == fixed, 'fixed output extent')
            return constructor(message).digest() if fixed else constructor(message).digest(length)
    raise ValueError('unknown mode: ' + mode)


def verify_file(row):
    raw = (ROOT / row['path']).read_bytes()
    require(len(raw) == row['bytes'] and hashlib.sha256(raw).hexdigest() == row['sha256'],
            'checksum: ' + row['path'])
    prefix = row['member'].split('Short')[0].split('Long')[0].split('Variable')[0]
    mode, _, fixed = MODES[prefix]
    metadata = {}
    record = {}
    count = 0
    for line in raw.decode('ascii').splitlines():
        if not line or line.startswith('#'):
            continue
        if line.startswith('[') and line.endswith(']'):
            if line == '[Tested for Output of byte-oriented messages]':
                continue
            key, value = line[1:-1].split(' = ')
            require(key not in metadata, 'duplicate metadata')
            metadata[key] = value
            continue
        key, value = line.split(' = ')
        require(key not in record, 'duplicate field')
        record[key] = value
        if key not in ('MD', 'Output'):
            continue
        variable = 'VariableOut' in row['member']
        expected_keys = {'COUNT', 'Outputlen', 'Msg', 'Output'} if variable else {'Len', 'Msg', key}
        require(set(record) == expected_keys, 'record fields')
        if variable:
            require(int(record['COUNT']) == count, 'record ordinal')
        length = bit_count(metadata['Input Length'] if variable else record['Len'])
        encoded = octets(record['Msg'])
        if length == 0:
            require(encoded == b'\0', 'zero-message convention')
        else:
            require(len(encoded) == length, 'message extent')
        message = encoded[:length]
        output_length = fixed if fixed else bit_count(record['Outputlen'] if variable else metadata['Outputlen'])
        answer = octets(record[key])
        require(len(answer) == output_length and reference(mode, message, output_length) == answer,
                'reference answer: ' + row['member'] + ' record ' + str(count))
        require(length <= row['maximum_input_bytes'], 'input bound')
        count += 1
        record = {}
    require(not record and count == row['cases'], 'record count')
    return count


def boundary_requests():
    inputs = [0, 1, 33, 34, 71, 72, 73, 135, 136, 137, 167, 168, 169,
              335, 336, 337, 1120, 1184, 4096]
    outputs = [0, 1, 31, 32, 63, 64, 135, 136, 137, 167, 168, 169,
               503, 504, 505, 671, 672, 673, 1024]
    requests = []
    for size in inputs:
        for pattern in range(2):
            message = bytes(size) if pattern == 0 else bytes((197 * i + size + 7) % 256 for i in range(size))
            for mode, lengths in [('sha3-256', [32]), ('sha3-512', [64]),
                                  ('shake128', outputs), ('shake256', outputs)]:
                for length in lengths:
                    requests.append((mode, message, length, reference(mode, message, length)))
    for seed in range(16):
        message = bytes((29 * i + 17 * seed) % 256 for i in range(102))
        expected = b''.join(hashlib.shake_128(message[34 * i:34 * (i + 1)]).digest(672) for i in range(3))
        requests.append(('matrix128', message, len(expected), expected))
    for size in [0, 1, 71, 72, 73, 143, 144, 145]:
        for pattern in range(2):
            message = bytes(size) if pattern == 0 else bytes((197 * i + size + 7) % 256 for i in range(size))
            stream = raw72_reference(message, 217)
            require(stream[:64] == hashlib.sha3_512(message).digest(), 'independent raw72 model prefix')
            for length in [0, 64, 71, 72, 73, 144, 145, 217]:
                requests.append(('raw72', message, length, stream[:length]))
    require(len(requests) == 1664, 'boundary extent')
    return requests


def compare_program(program, retirement):
    requests = boundary_requests()
    wire = ''.join(f'{mode} {length} {message.hex() or "-"}\n'
                   for mode, message, length, _ in requests)
    # One process and one reused work area for the complete boundary matrix.
    completed = subprocess.run([str(Path(program).resolve()), '--oracle'], input=wire,
                               text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    require(completed.returncode == 0, 'native oracle exit: ' + str(completed.returncode)
            + '\n' + completed.stderr)
    marker = f'ML-KEM hash oracle: cases={len(requests)} valid=1\n'
    if retirement:
        match = re.fullmatch(re.escape(marker) + r'ML-KEM hash retirement: cases=(\d+) sites=14 clears=(\d+) seen=16383 active=0 valid=1\n', completed.stderr)
        require(match is not None and int(match[1]) == len(requests) and int(match[2]) > 0,
                'native retirement diagnostics: ' + completed.stderr)
    else:
        require(completed.stderr == marker, 'native oracle diagnostics: ' + completed.stderr)
    lines = completed.stdout.splitlines()
    require(len(lines) == len(requests), 'native oracle output count')
    for index, (line, request) in enumerate(zip(lines, requests)):
        require(line == request[3].hex(), 'native boundary answer: ' + str(index))
    print(f'ML-KEM hash boundary: requests={len(requests)} passed={len(requests)}')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--retirement', action='store_true', help='require live hash cleanup observations')
    parser.add_argument('--program', help='built dssh_test_mlkem_hash executable')
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text())
    rows = manifest['selected_files']
    require(len(rows) == 10 and len({row['path'] for row in rows}) == 10,
            'selected file extent')
    total = sum(verify_file(row) for row in rows)
    require(total == manifest['total_cases'] == 3592, 'complete corpus count')
    print(f'ML-KEM hash Python: files=10 cases={total} valid=1')
    if args.program:
        compare_program(args.program, args.retirement)
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (ValueError, KeyError, OSError, UnicodeError) as error:
        print('ML-KEM hash check failed: ' + str(error), file=sys.stderr)
        sys.exit(1)
