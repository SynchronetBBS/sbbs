#!/usr/bin/env python3
"""ZMODEM head-to-head bench harness.

Wires a sender and receiver (each speaking ZMODEM on stdin/stdout) through a
userspace relay that can inject fixed one-way latency and a bandwidth cap
(token bucket) in each direction. No root / netem required.

Usage:
  zbench.py --file F --outdir D --sender 'CMD...' --receiver 'CMD...'
            [--latency-ms N] [--rate-bps N] [--label L]
"""
import argparse, os, shlex, subprocess, sys, threading, time, hashlib

CHUNK = 65536

def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()

def relay(src_fd, dst_fd, latency_s, rate_bps, counter, name, stop):
    """Copy src->dst applying one-way latency + token-bucket rate limit."""
    tokens = 0.0
    last = time.monotonic()
    total = 0
    try:
        while True:
            data = os.read(src_fd, CHUNK)
            if not data:
                break
            if latency_s > 0:
                time.sleep(latency_s)
            off = 0
            n = len(data)
            while off < n:
                if rate_bps > 0:
                    now = time.monotonic()
                    tokens += (now - last) * rate_bps
                    last = now
                    cap = rate_bps  # 1s burst ceiling
                    if tokens > cap:
                        tokens = cap
                    if tokens < 1:
                        time.sleep(min(0.05, (1 - tokens) / rate_bps))
                        continue
                    take = int(min(n - off, tokens))
                    if take <= 0:
                        take = 1
                    tokens -= take
                else:
                    take = n - off
                try:
                    w = os.write(dst_fd, data[off:off+take])
                except (BrokenPipeError, OSError):
                    stop.set()
                    return
                off += w
                total += w
    except OSError:
        pass
    finally:
        counter[name] = total
        try:
            os.close(dst_fd)
        except OSError:
            pass

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--file', required=True)
    ap.add_argument('--outdir', required=True)
    ap.add_argument('--sender', required=True)
    ap.add_argument('--receiver', required=True)
    ap.add_argument('--latency-ms', type=float, default=0.0)
    ap.add_argument('--rate-bps', type=float, default=0.0)   # bytes/s each direction
    ap.add_argument('--label', default='')
    ap.add_argument('--timeout', type=float, default=1800)
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    fname = os.path.basename(args.file)
    dst = os.path.join(args.outdir, fname)
    if os.path.exists(dst):
        os.remove(dst)
    src_size = os.path.getsize(args.file)
    lat = args.latency_ms / 1000.0

    # pipes: s_out (sender->relay), r_in (relay->receiver),
    #        r_out (receiver->relay), s_in (relay->sender)
    s_out_r, s_out_w = os.pipe()
    r_in_r,  r_in_w  = os.pipe()
    r_out_r, r_out_w = os.pipe()
    s_in_r,  s_in_w  = os.pipe()

    serr = open(os.path.join(args.outdir, 'sender.stderr'), 'wb')
    rerr = open(os.path.join(args.outdir, 'recv.stderr'), 'wb')

    scmd = shlex.split(args.sender)
    rcmd = shlex.split(args.receiver)

    t0 = time.monotonic()
    sender = subprocess.Popen(scmd, stdin=s_in_r, stdout=s_out_w, stderr=serr,
                              cwd=os.path.dirname(os.path.abspath(args.file)))
    receiver = subprocess.Popen(rcmd, stdin=r_in_r, stdout=r_out_w, stderr=rerr,
                                cwd=args.outdir)
    for fd in (s_out_w, r_in_r, r_out_w, s_in_r):
        os.close(fd)

    counter, stop = {}, threading.Event()
    fwd = threading.Thread(target=relay, args=(s_out_r, r_in_w, lat, args.rate_bps, counter, 'fwd', stop))
    bwd = threading.Thread(target=relay, args=(r_out_r, s_in_w, lat, args.rate_bps, counter, 'bwd', stop))
    fwd.start(); bwd.start()

    try:
        rc_s = sender.wait(timeout=args.timeout)
        rc_r = receiver.wait(timeout=args.timeout)
    except subprocess.TimeoutExpired:
        sender.kill(); receiver.kill()
        print(f"TIMEOUT after {args.timeout}s"); sys.exit(2)
    t1 = time.monotonic()

    fwd.join(timeout=5); bwd.join(timeout=5)
    for fd in (s_out_r, r_out_r, r_in_w, s_in_w):
        try: os.close(fd)
        except OSError: pass
    serr.close(); rerr.close()

    elapsed = t1 - t0
    # Locate received file by size (robust to receiver naming quirks)
    if not (os.path.exists(dst) and os.path.getsize(dst) == src_size):
        cand = [os.path.join(args.outdir, f) for f in os.listdir(args.outdir)
                if not f.endswith(('.stderr',)) and os.path.isfile(os.path.join(args.outdir, f))
                and os.path.getsize(os.path.join(args.outdir, f)) == src_size]
        if cand:
            dst = cand[0]
    ok = os.path.exists(dst) and os.path.getsize(dst) == src_size
    integrity = 'N/A'
    if ok:
        integrity = 'MATCH' if sha256(args.file) == sha256(dst) else 'CORRUPT'
    dsz = os.path.getsize(dst) if os.path.exists(dst) else 0
    rname = os.path.basename(dst) if os.path.exists(dst) else '-'
    goodput = src_size / elapsed if elapsed > 0 else 0
    wire_fwd = counter.get('fwd', 0)
    overhead = (wire_fwd / src_size - 1) * 100 if src_size else 0

    print(f"[{args.label}] rc(s/r)={rc_s}/{rc_r} elapsed={elapsed:.2f}s "
          f"size={src_size} recv={dsz} name={rname} {integrity} "
          f"goodput={goodput/1e6:.2f}MB/s wire_fwd={wire_fwd} overhead={overhead:+.2f}%")

if __name__ == '__main__':
    main()
