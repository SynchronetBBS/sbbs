#!/usr/bin/env python3
"""ZMODEM bench harness (pty model) — for serial-era rz/sz (Forsberg).

Each endpoint runs on its own pty slave (raw mode), which satisfies the
tcgetattr/tcsetattr the tools do. A relay bridges the two pty masters, with
optional one-way latency + bandwidth cap. Works for sexyz/lrzsz too.
"""
import argparse, os, pty, shlex, subprocess, sys, termios, threading, time, hashlib, tty, errno

CHUNK = 65536

def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for b in iter(lambda: f.read(1 << 20), b''):
            h.update(b)
    return h.hexdigest()

def make_raw(fd):
    a = termios.tcgetattr(fd)
    a[0] &= ~(termios.BRKINT|termios.ICRNL|termios.INPCK|termios.ISTRIP|termios.IXON|termios.INLCR|termios.IGNCR)
    a[0] &= ~(termios.IXOFF)
    a[1] &= ~termios.OPOST
    a[2] &= ~(termios.CSIZE|termios.PARENB); a[2] |= termios.CS8
    a[3] &= ~(termios.ECHO|termios.ICANON|termios.ISIG|termios.IEXTEN)
    a[6][termios.VMIN] = 1; a[6][termios.VTIME] = 0
    termios.tcsetattr(fd, termios.TCSANOW, a)

def relay(src_fd, dst_fd, latency_s, rate_bps, counter, name):
    tokens=0.0; last=time.monotonic(); total=0
    try:
        while True:
            try:
                data=os.read(src_fd, CHUNK)
            except OSError as e:
                if e.errno==errno.EIO: break
                raise
            if not data: break
            if latency_s>0: time.sleep(latency_s)
            off,n=0,len(data)
            while off<n:
                if rate_bps>0:
                    now=time.monotonic(); tokens+=(now-last)*rate_bps; last=now
                    if tokens>rate_bps: tokens=rate_bps
                    if tokens<1: time.sleep(min(0.05,(1-tokens)/rate_bps)); continue
                    take=max(1,int(min(n-off,tokens))); tokens-=take
                else: take=n-off
                try: w=os.write(dst_fd, data[off:off+take])
                except OSError: counter[name]=total; return
                off+=w; total+=w
    except OSError: pass
    finally: counter[name]=total

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument('--file',required=True); ap.add_argument('--outdir',required=True)
    ap.add_argument('--sender',required=True); ap.add_argument('--receiver',required=True)
    ap.add_argument('--latency-ms',type=float,default=0.0); ap.add_argument('--rate-bps',type=float,default=0.0)
    ap.add_argument('--label',default=''); ap.add_argument('--timeout',type=float,default=1800)
    args=ap.parse_args()
    os.makedirs(args.outdir,exist_ok=True)
    src_size=os.path.getsize(args.file)
    dst=os.path.join(args.outdir,os.path.basename(args.file))
    if os.path.exists(dst): os.remove(dst)
    lat=args.latency_ms/1000.0

    s_m,s_s=pty.openpty(); r_m,r_s=pty.openpty()
    for fd in (s_s,r_s,s_m,r_m): make_raw(fd)
    serr=open(os.path.join(args.outdir,'sender.stderr'),'wb')
    rerr=open(os.path.join(args.outdir,'recv.stderr'),'wb')
    t0=time.monotonic()
    sender=subprocess.Popen(shlex.split(args.sender),stdin=s_s,stdout=s_s,stderr=serr,
                            cwd=os.path.dirname(os.path.abspath(args.file)),close_fds=True)
    receiver=subprocess.Popen(shlex.split(args.receiver),stdin=r_s,stdout=r_s,stderr=rerr,
                              cwd=args.outdir,close_fds=True)
    os.close(s_s); os.close(r_s)
    counter={}
    fwd=threading.Thread(target=relay,args=(s_m,r_m,lat,args.rate_bps,counter,'fwd'))
    bwd=threading.Thread(target=relay,args=(r_m,s_m,lat,args.rate_bps,counter,'bwd'))
    fwd.start(); bwd.start()
    try:
        rc_s=sender.wait(timeout=args.timeout); rc_r=receiver.wait(timeout=args.timeout)
    except subprocess.TimeoutExpired:
        sender.kill(); receiver.kill()
        print(f"[{args.label}] TIMEOUT after {args.timeout}s")
        for fd in (s_m,r_m):
            try: os.close(fd)
            except OSError: pass
        sys.exit(2)
    t1=time.monotonic()
    fwd.join(timeout=3); bwd.join(timeout=3)
    for fd in (s_m,r_m):
        try: os.close(fd)
        except OSError: pass
    serr.close(); rerr.close()
    elapsed=t1-t0
    if not (os.path.exists(dst) and os.path.getsize(dst)==src_size):
        cand=[os.path.join(args.outdir,f) for f in os.listdir(args.outdir)
              if not f.endswith('.stderr') and os.path.isfile(os.path.join(args.outdir,f))
              and os.path.getsize(os.path.join(args.outdir,f))==src_size]
        if cand: dst=cand[0]
    ok=os.path.exists(dst) and os.path.getsize(dst)==src_size
    integrity=('MATCH' if sha256(args.file)==sha256(dst) else 'CORRUPT') if ok else 'N/A'
    dsz=os.path.getsize(dst) if os.path.exists(dst) else 0
    rname=os.path.basename(dst) if os.path.exists(dst) else '-'
    goodput=src_size/elapsed if elapsed>0 else 0
    wire=counter.get('fwd',0); ovh=(wire/src_size-1)*100 if src_size else 0
    print(f"[{args.label}] rc(s/r)={rc_s}/{rc_r} elapsed={elapsed:.2f}s size={src_size} "
          f"recv={dsz} name={rname} {integrity} goodput={goodput/1e6:.2f}MB/s wire={wire} overhead={ovh:+.2f}%")

if __name__=='__main__': main()
