"""Independent PKCS#8 field fixtures; every child uses a fresh native registry."""
import base64
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile


def tlv(data, pos, expected):
    if pos >= len(data) or data[pos] != expected or pos + 1 >= len(data):
        raise ValueError("fixture ASN.1 tag")
    length = data[pos + 1]
    head = pos + 2
    if length & 128:
        width = length & 127
        if not 1 <= width <= 4 or head + width > len(data):
            raise ValueError("fixture ASN.1 length")
        length = int.from_bytes(data[head:head + width], "big")
        head += width
    end = head + length
    if end > len(data):
        raise ValueError("fixture ASN.1 extent")
    return head, end


def rsa_fields(pem):
    match = re.fullmatch(rb"-----BEGIN PRIVATE KEY-----\s+([A-Za-z0-9+/=\s]+)-----END PRIVATE KEY-----\s*", pem)
    if match is None:
        raise ValueError("expected cached PKCS#8 PEM")
    der = base64.b64decode(re.sub(rb"\s", b"", match[1]), validate=True)
    seq, end = tlv(der, 0, 48)
    if end != len(der):
        raise ValueError("outer fixture extent")
    _, version_end = tlv(der, seq, 2)
    _, algorithm_end = tlv(der, version_end, 48)
    inner, inner_end = tlv(der, algorithm_end, 4)
    values, values_end = tlv(der, inner, 48)
    if inner_end != end or values_end != inner_end:
        raise ValueError("inner fixture extent")
    _, pos = tlv(der, values, 2)
    fields = {}
    for name in ["n", "e", "d", "p", "q", "dp", "dq", "coefficient"]:
        first, last = tlv(der, pos, 2)
        if last <= first or der[first] & 128:
            raise ValueError("positive fixture integer")
        fields[name] = last - 1
        pos = last
    if pos != values_end:
        raise ValueError("RSA fixture extent")
    return der, fields


def pem_encode(der):
    body = base64.b64encode(der)
    return b"-----BEGIN PRIVATE KEY-----\n" + b"\n".join(body[i:i + 64] for i in range(0, len(body), 64)) + b"\n-----END PRIVATE KEY-----\n"


def main():
    if len(sys.argv) != 3 or sys.argv[2] not in ["valid", "invalid"]:
        raise ValueError("usage: key_validation.py executable valid|invalid")
    der, fields = rsa_fields(Path(os.environ["DSSH_TEST_RSA_KEY"]).read_bytes())
    cases = []
    if sys.argv[2] == "valid":
        for algorithm, variable in [("ssh-ed25519", "DSSH_TEST_ED25519_KEY"),
                                    ("rsa-sha2-256", "DSSH_TEST_RSA_KEY"),
                                    ("rsa-sha2-512", "DSSH_TEST_RSA_KEY")]:
            cases.append((algorithm, "valid", Path(os.environ[variable]).read_bytes(), "accept"))
    else:
        for algorithm in ["rsa-sha2-256", "rsa-sha2-512"]:
            for name, offset in fields.items():
                changed = bytearray(der)
                changed[offset] ^= 2
                cases.append((algorithm, name, pem_encode(changed), "reject"))
    failures = []
    with tempfile.TemporaryDirectory(prefix="dssh-key-validation-") as directory:
        for index, (algorithm, label, contents, expectation) in enumerate(cases):
            path = Path(directory) / str(index)
            with open(path, "xb", opener=lambda name, flags: os.open(name, flags, 0o600)) as stream:
                if stream.write(contents) != len(contents):
                    raise OSError("short fixture write")
            environment = os.environ.copy()
            environment.update(DSSH_VALIDATION_ALGORITHM=algorithm,
                               DSSH_VALIDATION_CANDIDATE=str(path), DSSH_VALIDATION_EXPECT=expectation)
            child = subprocess.run([sys.argv[1], "--exact", "transaction"], env=environment,
                                   text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=120)
            print(f"algorithm={algorithm} case={label} exit={child.returncode}", flush=True)
            print(child.stdout, end="", flush=True)
            if child.returncode != 0 or "1 tests: 1 passed, 0 failed, 0 skipped" not in child.stdout:
                failures.append((algorithm, label, child.returncode))
    print(f"key validation: mode={sys.argv[2]} cases={len(cases)} failed={len(failures)}", flush=True)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
