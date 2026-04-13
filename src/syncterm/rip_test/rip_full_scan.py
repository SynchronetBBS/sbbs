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
import socket, os, sys

RIP_DIR = '/synchronet/tmp/ripterm/rips'
HARNESS_HOST = 'portable'
HARNESS_PORT = 1516

files = sorted(
    [f for f in os.listdir(RIP_DIR) if f.lower().endswith('.rip')],
    key=str.lower
)

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

# Connect to harness control port directly (avoids shell escaping issues
# with ! and other special characters in filenames).
try:
    s = socket.create_connection((HARNESS_HOST, HARNESS_PORT), timeout=120)
except ConnectionRefusedError:
    print(f"ERROR: cannot connect to harness at {HARNESS_HOST}:{HARNESS_PORT}",
          file=sys.stderr)
    sys.exit(2)

buf = b''
# Read and discard banner
while b'\n' not in buf:
    buf += s.recv(4096)
buf = buf.split(b'\n', 1)[1]


def send_cmd(cmd):
    """Send a command and return all output lines up to and including OK/ERROR."""
    global buf
    s.sendall((cmd + '\n').encode())
    output_lines = []
    while True:
        buf += s.recv(4096)
        while b'\n' in buf:
            line, buf = buf.split(b'\n', 1)
            text = line.decode('utf-8', errors='replace').rstrip('\r')
            output_lines.append(text)
            if text == 'OK' or text.startswith('ERROR:'):
                return output_lines


for i in range(start_idx, len(files)):
    f = files[i]
    path = os.path.join(RIP_DIR, f)

    send_cmd('reset')
    sf = send_cmd(f'sendfile {f}')

    # Check for sendfile errors
    if any(l.startswith('ERROR:') for l in sf):
        print(f'ERROR at file {i+1}/{len(files)}: {f}')
        for l in sf:
            print(f'  {l}')
        sys.exit(2)

    cap_lines = send_cmd('capture')

    clean = False
    comp_line = ''
    for line in cap_lines:
        if 'comparison:' in line:
            comp_line = line
            if '0 differ' in line or '100.00%' in line:
                clean = True
            break

    if not clean:
        print(f'DIFFS at file {i+1}/{len(files)}: {f}')
        if comp_line:
            print(comp_line)
        sys.exit(1)

    if (i - start_idx + 1) % 100 == 0:
        print(f'  ...scanned {i+1}/{len(files)} ({f}) - all clean', flush=True)

s.close()
total = len(files) - start_idx
print(f'All {total} files clean!')
sys.exit(0)
