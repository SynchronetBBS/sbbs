#!/usr/bin/env python3
"""ZMODEM bench harness (socket model).

Each endpoint gets ONE bidirectional socket, duped to both stdin and stdout.
This works for tools that use separate stdin/stdout (sexyz, lrzsz) AND for
traditional rz/sz that read+write a single fd (Forsberg). A relay bridges the
two middle sockets, optionally injecting one-way latency + a bandwidth cap.

Usage: zbench_sock.py --file F --outdir D --sender 'CMD' --receiver 'CMD'
       [--latency-ms N] [--rate-bps N] [--label L] [--timeout S]
"""
import argparse, os, shlex, socket, subprocess, sys, threading, time, hashlib, random

CHUNK = 65536

def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()

def relay(src, dst, latency_s, rate_bps, counter, name, corrupt_rate=0.0, rng=None, tap=None):
    tokens = 0.0
    last = time.monotonic()
    total = 0
    try:
        while True:
            data = src.recv(CHUNK)
            if not data:
                break
            if corrupt_rate > 0 and rng is not None:
                ba = bytearray(data)
                for i in range(len(ba)):
                    if rng.random() < corrupt_rate:
                        ba[i] ^= (1 << rng.randrange(8))
                data = bytes(ba)
            if tap is not None:
                tap.write(data)
            if latency_s > 0:
                time.sleep(latency_s)
            off, n = 0, len(data)
            while off < n:
                if rate_bps > 0:
                    now = time.monotonic()
                    tokens += (now - last) * rate_bps; last = now
                    if tokens > rate_bps: tokens = rate_bps
                    if tokens < 1:
                        time.sleep(min(0.05, (1 - tokens) / rate_bps)); continue
                    take = max(1, int(min(n - off, tokens))); tokens -= take
                else:
                    take = n - off
                try:
                    w = dst.send(data[off:off+take])
                except OSError:
                    counter[name] = total; return
                off += w; total += w
    except OSError:
        pass
    finally:
        counter[name] = total
        try: dst.shutdown(socket.SHUT_WR)
        except OSError: pass

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--file', required=True)
    ap.add_argument('--outdir', required=True)
    ap.add_argument('--sender', required=True)
    ap.add_argument('--receiver', required=True)
    ap.add_argument('--latency-ms', type=float, default=0.0)
    ap.add_argument('--rate-bps', type=float, default=0.0)
    ap.add_argument('--rate-back-bps', type=float, default=0.0)  # asymmetric back-channel cap
    ap.add_argument('--corrupt-rate', type=float, default=0.0)   # per-byte bit-flip prob (fwd only)
    ap.add_argument('--label', default='')
    ap.add_argument('--sockbuf', type=int, default=0,
                    help='SO_SNDBUF/SO_RCVBUF on the socketpairs, to bound in-flight backlog')
    ap.add_argument('--tap', action='store_true',
                    help='write post-corruption wire captures to <outdir>/wire.fwd and wire.bwd')
    ap.add_argument('--timeout', type=float, default=1800)
    args = ap.parse_args()

    os.makedirs(args.outdir, exist_ok=True)
    src_size = os.path.getsize(args.file)
    dst = os.path.join(args.outdir, os.path.basename(args.file))
    if os.path.exists(dst): os.remove(dst)
    lat = args.latency_ms / 1000.0

    s_end, s_relay = socket.socketpair()
    r_end, r_relay = socket.socketpair()
    if args.sockbuf:
        for sk in (s_end, s_relay, r_end, r_relay):
            sk.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, args.sockbuf)
            sk.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, args.sockbuf)
    serr = open(os.path.join(args.outdir, 'sender.stderr'), 'wb')
    rerr = open(os.path.join(args.outdir, 'recv.stderr'), 'wb')

    t0 = time.monotonic()
    sender = subprocess.Popen(shlex.split(args.sender), stdin=s_end.fileno(),
                              stdout=s_end.fileno(), stderr=serr,
                              cwd=os.path.dirname(os.path.abspath(args.file)))
    receiver = subprocess.Popen(shlex.split(args.receiver), stdin=r_end.fileno(),
                                stdout=r_end.fileno(), stderr=rerr, cwd=args.outdir)
    s_end.close(); r_end.close()

    counter = {}
    rback = args.rate_back_bps if args.rate_back_bps > 0 else args.rate_bps
    tap_f = open(os.path.join(args.outdir, 'wire.fwd'), 'wb') if args.tap else None
    tap_b = open(os.path.join(args.outdir, 'wire.bwd'), 'wb') if args.tap else None
    fwd = threading.Thread(target=relay, args=(s_relay, r_relay, lat, args.rate_bps, counter, 'fwd',
                                               args.corrupt_rate, random.Random(1234), tap_f))
    bwd = threading.Thread(target=relay, args=(r_relay, s_relay, lat, rback, counter, 'bwd',
                                               0.0, None, tap_b))
    fwd.start(); bwd.start()

    try:
        rc_s = sender.wait(timeout=args.timeout)
        rc_r = receiver.wait(timeout=args.timeout)
    except subprocess.TimeoutExpired:
        sender.kill(); receiver.kill()
        print(f"[{args.label}] TIMEOUT after {args.timeout}s");
        try: s_relay.close(); r_relay.close()
        except OSError: pass
        sys.exit(2)
    t1 = time.monotonic()
    for s in (s_relay, r_relay):
        try: s.shutdown(socket.SHUT_RDWR)
        except OSError: pass
    fwd.join(timeout=5); bwd.join(timeout=5)
    try: s_relay.close(); r_relay.close()
    except OSError: pass
    serr.close(); rerr.close()

    elapsed = t1 - t0
    if not (os.path.exists(dst) and os.path.getsize(dst) == src_size):
        cand = [os.path.join(args.outdir, f) for f in os.listdir(args.outdir)
                if not f.endswith('.stderr')
                and os.path.isfile(os.path.join(args.outdir, f))
                and os.path.getsize(os.path.join(args.outdir, f)) == src_size]
        if cand: dst = cand[0]
    ok = os.path.exists(dst) and os.path.getsize(dst) == src_size
    integrity = ('MATCH' if sha256(args.file) == sha256(dst) else 'CORRUPT') if ok else 'N/A'
    dsz = os.path.getsize(dst) if os.path.exists(dst) else 0
    rname = os.path.basename(dst) if os.path.exists(dst) else '-'
    goodput = src_size / elapsed if elapsed > 0 else 0
    wire = counter.get('fwd', 0)
    ovh = (wire / src_size - 1) * 100 if src_size else 0
    print(f"[{args.label}] rc(s/r)={rc_s}/{rc_r} elapsed={elapsed:.2f}s "
          f"size={src_size} recv={dsz} name={rname} {integrity} "
          f"goodput={goodput/1e6:.2f}MB/s wire={wire} overhead={ovh:+.2f}%")

if __name__ == '__main__':
    main()
