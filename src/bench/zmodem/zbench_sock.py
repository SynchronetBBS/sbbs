#!/usr/bin/env python3
"""ZMODEM bench harness (socket model).

Each endpoint gets ONE bidirectional socket, duped to both stdin and stdout.
This works for tools that use separate stdin/stdout (sexyz, lrzsz) AND for
traditional rz/sz that read+write a single fd (Forsberg, zmtx/zmrx). A relay
bridges the two middle sockets, optionally injecting one-way latency + a
bandwidth cap.

Repeat --file to send a BATCH of files in one session: goodput alone divides a
per-file cost away, so anything suspected of costing per *file* rather than per
byte needs several files in one transfer to be visible at all.

Both endpoints are reaped with wait4(), so every run reports the sender's and
receiver's CPU seconds beside its goodput. A number with no CPU figure next to
it cannot tell "fast code" from "idle code" -- that distinction is what this
harness exists to make.

Usage: zbench_sock.py --file F [--file F2 ...] --outdir D
       --sender 'CMD' --receiver 'CMD'
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

def wait_rusage(p):
    """Reap p, returning (exitcode, rusage).  Popen.wait() throws the rusage
    away; the CPU split between the two endpoints is half the measurement."""
    try:
        _pid, status, ru = os.wait4(p.pid, 0)
    except ChildProcessError:
        return p.poll(), None
    rc = os.waitstatus_to_exitcode(status)
    p.returncode = rc          # keep Popen from reaping (and warning) again
    return rc, ru

def cpu_str(ru):
    if ru is None:
        return 'cpu=?'
    return (f"cpu={ru.ru_utime + ru.ru_stime:.2f}s"
            f"(u{ru.ru_utime:.2f}/s{ru.ru_stime:.2f}) "
            f"csw={ru.ru_nvcsw + ru.ru_nivcsw}")

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--file', required=True, action='append',
                    help='file to send; repeat for a multi-file batch')
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
    files = args.file
    sizes = [os.path.getsize(f) for f in files]
    src_size = sum(sizes)
    for f in files:
        d = os.path.join(args.outdir, os.path.basename(f))
        if os.path.exists(d): os.remove(d)
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
                              cwd=os.path.dirname(os.path.abspath(files[0])))
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

    # wait4() has no timeout, so a watchdog does the killing.
    timed_out = threading.Event()
    def reaper():
        timed_out.set()
        for p in (sender, receiver):
            try: p.kill()
            except OSError: pass
    watchdog = threading.Timer(args.timeout, reaper)
    watchdog.start()
    rc_s, ru_s = wait_rusage(sender)
    rc_r, ru_r = wait_rusage(receiver)
    watchdog.cancel()
    t1 = time.monotonic()
    if timed_out.is_set():
        print(f"[{args.label}] TIMEOUT after {args.timeout}s")
        try: s_relay.close(); r_relay.close()
        except OSError: pass
        sys.exit(2)
    for s in (s_relay, r_relay):
        try: s.shutdown(socket.SHUT_RDWR)
        except OSError: pass
    fwd.join(timeout=5); bwd.join(timeout=5)
    try: s_relay.close(); r_relay.close()
    except OSError: pass
    serr.close(); rerr.close()
    for t in (tap_f, tap_b):
        if t is not None: t.close()

    elapsed = t1 - t0
    # Verify every file: same name in outdir, else any same-sized leftover
    # (receivers rename on collision, and sexyz has been seen to '.'-prefix).
    verdicts, recvd = [], 0
    used = set()
    for f, sz in zip(files, sizes):
        dst = os.path.join(args.outdir, os.path.basename(f))
        if not (os.path.exists(dst) and os.path.getsize(dst) == sz):
            cand = [os.path.join(args.outdir, c) for c in sorted(os.listdir(args.outdir))
                    if not c.endswith('.stderr') and not c.startswith('wire.')
                    and os.path.join(args.outdir, c) not in used
                    and os.path.isfile(os.path.join(args.outdir, c))
                    and os.path.getsize(os.path.join(args.outdir, c)) == sz]
            dst = cand[0] if cand else None
        if dst is None:
            verdicts.append('MISSING'); continue
        used.add(dst)
        recvd += os.path.getsize(dst)
        verdicts.append('MATCH' if sha256(f) == sha256(dst) else 'CORRUPT')
    integrity = 'MATCH' if verdicts and all(v == 'MATCH' for v in verdicts) else '/'.join(verdicts)
    goodput = src_size / elapsed if elapsed > 0 else 0
    wire = counter.get('fwd', 0)
    ovh = (wire / src_size - 1) * 100 if src_size else 0
    nf = f"files={len(files)} " if len(files) > 1 else ""
    print(f"[{args.label}] rc(s/r)={rc_s}/{rc_r} elapsed={elapsed:.2f}s "
          f"{nf}size={src_size} recv={recvd} {integrity} "
          f"goodput={goodput/1e6:.2f}MB/s wire={wire} overhead={ovh:+.2f}% "
          f"tx[{cpu_str(ru_s)}] rx[{cpu_str(ru_r)}]")

if __name__ == '__main__':
    main()
