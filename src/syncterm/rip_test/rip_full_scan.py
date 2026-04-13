#!/usr/bin/env python3
"""Scan all RIP files for pixel-perfect rendering diffs.

Sends each .rip file to both SyncTERM and RIPterm via the harness,
captures and compares.  Stops on the first file with diffs and prints
the filename and diff summary.  Exits 0 if all files pass, 1 on first
diff, 2 on error.

Usage:
    python3 rip_full_scan.py [start_file]

If start_file is given, scanning begins at that filename (inclusive).
"""
import subprocess, os, sys

RIP_DIR = '/synchronet/tmp/ripterm/rips'
HCTL = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'hctl')

files = sorted([f for f in os.listdir(RIP_DIR) if f.endswith('.rip')], key=str.lower)

start_idx = 0
if len(sys.argv) > 1:
    target = sys.argv[1]
    for i, f in enumerate(files):
        if f.lower() == target.lower():
            start_idx = i
            break
    else:
        print(f"ERROR: {target} not found in {RIP_DIR}", file=sys.stderr)
        sys.exit(2)

for i in range(start_idx, len(files)):
    f = files[i]
    if '!' in f:
        continue
    path = os.path.join(RIP_DIR, f)
    try:
        result = subprocess.run(
            ['python3', HCTL, 'reset', ',', f'sendfile {path}', ',', 'capture'],
            capture_output=True, text=True, timeout=180
        )
    except subprocess.TimeoutExpired:
        print(f'TIMEOUT at file {i+1}/{len(files)}: {f}')
        sys.exit(2)

    if result.returncode != 0:
        print(f'ERROR at file {i+1}/{len(files)}: {f}')
        print(result.stderr[:200] if result.stderr else result.stdout[:200])
        sys.exit(2)

    for line in result.stdout.split('\n'):
        if 'comparison:' in line:
            if '0 differ' not in line:
                print(f'DIFFS at file {i+1}/{len(files)}: {f}')
                print(line)
                sys.exit(1)
            break

    if (i - start_idx + 1) % 100 == 0:
        print(f'  ...scanned {i+1}/{len(files)} ({f}) - all clean', flush=True)

print(f'All {len(files) - start_idx} files clean!')
sys.exit(0)
