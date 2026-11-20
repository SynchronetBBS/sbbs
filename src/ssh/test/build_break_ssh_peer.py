#!/usr/bin/env python3
"""Build the optional independent SSH peer from existing offline Go modules.

This builds a foreign test application, not the C API. No module sources are
copied or fetched. Output, module metadata and compiler cache live in one
caller-supplied directory, normally below an existing build/test directory.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def build(args):
    directory = Path(args.build_dir).resolve()
    directory.mkdir(parents=True, exist_ok=True)
    crypto = Path(args.crypto).resolve(strict=True)
    system = Path(args.system).resolve(strict=True)
    if not (crypto / "ssh/channel.go").is_file() or not (system / "go.mod").is_file():
        raise RuntimeError("expected existing offline crypto/sys module sources")
    # JSON quoted strings are valid Go module string literals; no shell use.
    module = ('module deucessh-audit-break-peer\n\ngo 1.26.0\n\n'
              'require golang.org/x/crypto v0.58.0\n'
              f'replace golang.org/x/crypto => {json.dumps(str(crypto))}\n'
              f'replace golang.org/x/sys => {json.dumps(str(system))}\n')
    (directory / "go.mod").write_text(module)
    source = Path(__file__).resolve().with_name("break_ssh_peer.go")
    binary = directory / "break-ssh-peer"
    env = os.environ | dict(GOTOOLCHAIN="local", GOPROXY="off", GOSUMDB="off",
                            GOCACHE=str(directory / "cache"), GOMODCACHE=str(directory / "modules"),
                            GOFLAGS="", GOWORK="off", CGO_ENABLED="0", GOMAXPROCS="3")
    commands = [[args.go, "version"],
                [args.go, "build", "-mod=mod", "-p", "4", "-o", str(binary), str(source)],
                [args.go, "version", "-m", str(binary)],
                [args.go, "list", "-mod=mod", "-deps", "-json", str(source)]]
    record = dict(scope="Independent Go SSH implementation; not C API compilation or a library dependency",
                  commands=[], source_sha256={str(source): digest(source)}, binary=str(binary))
    attempt = 0
    while (directory / f"build-{attempt}-result.json").exists():
        attempt += 1
    for index, command in enumerate(commands):
        log = directory / f"build-{attempt}-{index}.log"
        with log.open("wb") as output:
            result = subprocess.run(command, cwd=directory, env=env,
                                    stdout=output, stderr=subprocess.STDOUT)
        record["commands"].append(dict(command=command, exit=result.returncode,
                                        log=str(log), sha256=digest(log)))
        if result.returncode != 0:
            (directory / f"build-{attempt}-result.json").write_text(json.dumps(record, indent=2) + "\n")
            (directory / "result.json").write_text(json.dumps(record, indent=2) + "\n")
            raise RuntimeError(f"foreign peer build failed: {log}")
    text = Path(record["commands"][-1]["log"]).read_text()
    decoder = json.JSONDecoder()
    inputs = {str(source): digest(source), str(crypto / "go.mod"): digest(crypto / "go.mod"),
              str(system / "go.mod"): digest(system / "go.mod")}
    while text.strip():
        entry, end = decoder.raw_decode(text.lstrip())
        text = text.lstrip()[end:]
        if entry.get("Standard"):
            continue
        root = Path(entry["Dir"])
        for category in ["GoFiles", "CgoFiles", "SFiles", "HFiles", "EmbedFiles"]:
            for name in entry.get(category, []):
                path = root / name
                inputs[str(path)] = digest(path)
    record.update(inputs_sha256=inputs, binary_sha256=digest(binary),
                  module_sha256=digest(directory / "go.mod"), built=True)
    (directory / f"build-{attempt}-result.json").write_text(json.dumps(record, indent=2) + "\n")
    (directory / "result.json").write_text(json.dumps(record, indent=2) + "\n")
    print(f"independent_break_peer built=1 source_inputs={len(inputs)} binary={binary}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--go", required=True)
    parser.add_argument("--crypto", required=True)
    parser.add_argument("--system", required=True)
    parser.add_argument("--build-dir", required=True)
    build(parser.parse_args())
