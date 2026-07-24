#!/bin/bash
# Comprehensive ZMODEM benchmark matrix driver.
BENCH="$(cd "$(dirname "$0")" && pwd)"; SP="${DATA:-$BENCH/run}"; mkdir -p "$SP"
cd $SP
F32=$(realpath data/test32.bin); F8=$(realpath data/test8.bin)
LSZ=~/lrzsz-0.12.21rc/src/lsz; LRZ=~/lrzsz-0.12.21rc/src/lrz
SX=$(realpath ./sexyz); FSZ=$(realpath ./fsz2)
H="python3 zbench_sock.py"
run(){ # label file sender receiver [extra-args...]
  local lbl="$1" file="$2" snd="$3" rcv="$4"; shift 4
  timeout 120 $H --file "$file" --outdir "out/m_$(echo $lbl|tr ' /:,' '____')" \
     --sender "$snd" --receiver "$rcv" --label "$lbl" --timeout 110 "$@"
}
sec(){ echo; echo "########## $1 ##########"; }

case "$1" in
 block)
  sec "Block-size sweep (sender->lrz, 32MB clean)"
  for b in 2 4 8; do run "sexyz -$b ->lrz" "$F32" "$SX -$b sz $F32" "$LRZ -y"; done
  for b in 2 4 8; do run "lsz -$b ->lrz"   "$F32" "$LSZ -$b $F32"   "$LRZ -y"; done
  run "forsberg(1K) ->lrz" "$F32" "$FSZ $F32" "$LRZ -y"
  ;;
 crc)
  sec "CRC-16 (-o) vs CRC-32 (32MB clean, sender->lrz)"
  run "sexyz CRC32 ->lrz" "$F32" "$SX -8 sz $F32"    "$LRZ -y"
  run "sexyz CRC16 ->lrz" "$F32" "$SX -8 -o sz $F32" "$LRZ -y"
  run "lsz CRC32 ->lrz"   "$F32" "$LSZ -8 $F32"      "$LRZ -y"
  run "lsz CRC16 ->lrz"   "$F32" "$LSZ -8 -o $F32"   "$LRZ -y"
  ;;
 lat)
  sec "Latency sweep (8MB): streaming vs windowed"
  for ms in 5 25 100; do
    run "lrzsz stream ${ms}ms" "$F8" "$LSZ -8 $F8"          "$LRZ -y" --latency-ms $ms
    run "lrzsz win32k ${ms}ms" "$F8" "$LSZ -8 -w 32768 $F8" "$LRZ -y" --latency-ms $ms
    run "sexyz stream ${ms}ms" "$F8" "$SX -8 sz $F8"        "$LRZ -y" --latency-ms $ms
    run "sexyz win32k ${ms}ms" "$F8" "$SX -8 -w32768 sz $F8" "$LRZ -y" --latency-ms $ms
  done
  ;;
 bw)
  sec "Bandwidth asymmetry (8MB): fwd 4MB/s, back-channel 16KB/s"
  run "lrzsz asym"  "$F8" "$LSZ -8 $F8"   "$LRZ -y" --rate-bps 4000000 --rate-back-bps 16000
  run "sexyz asym"  "$F8" "$SX -8 sz $F8" "$LRZ -y" --rate-bps 4000000 --rate-back-bps 16000
  run "forsberg asym" "$F8" "$FSZ $F8"    "$LRZ -y" --rate-bps 4000000 --rate-back-bps 16000
  sec "Symmetric 1MB/s cap (8MB) for reference"
  run "lrzsz 1MBps" "$F8" "$LSZ -8 $F8"   "$LRZ -y" --rate-bps 1000000
  run "sexyz 1MBps" "$F8" "$SX -8 sz $F8" "$LRZ -y" --rate-bps 1000000
  ;;
 err)
  sec "Error injection (8MB, fwd bit-flips): completion + time"
  for r in 0.000003 0.00001 0.00003; do
    run "lrzsz err=$r"    "$F8" "$LSZ -8 $F8"   "$LRZ -y" --corrupt-rate $r
    run "sexyz err=$r"    "$F8" "$SX -8 sz $F8" "$LRZ -y" --corrupt-rate $r
    run "forsberg err=$r" "$F8" "$FSZ $F8"      "$LRZ -y" --corrupt-rate $r
  done
  ;;
esac
