#!/usr/bin/env python3
"""Derive sntrup rejection answers from the official corpus and specification.

No DeuceSSH implementation or output is used. Sections 2.3.8, 3.1 and 3.2 of
the round-three NTRU Prime submission define the expected fallback as
SHA512(0 || SHA512(3 || rho)[:32] || ciphertext)[:32].
"""

import argparse
import hashlib
from pathlib import Path


def records(path):
	text = path.read_text(encoding="ascii")
	if not text.startswith("# kem/sntrup761\n\n"):
		raise ValueError("unexpected response header")
	result = []
	for block in text.split("\n\n")[1:]:
		if not block:
			continue
		lines = block.splitlines()
		row = dict(line.split(" = ", 1) for line in lines)
		if len(lines) != 6 or set(row) != {"count", "seed", "pk", "sk", "ct", "ss"}:
			raise ValueError("unexpected response fields")
		if row["count"] != str(len(result)):
			raise ValueError("response record out of order")
		decoded = {key: bytes.fromhex(row[key]) for key in ("seed", "pk", "sk", "ct", "ss")}
		if {key: len(value) for key, value in decoded.items()} != {
			"seed": 48, "pk": 1158, "sk": 1763, "ct": 1039, "ss": 32
		}:
			raise ValueError("unexpected response lengths")
		if decoded["sk"][382:1540] != decoded["pk"]:
			raise ValueError("embedded public key mismatch")
		result.append(decoded)
	if len(result) != 100:
		raise ValueError("expected 100 response records")
	return result


def variants(record, index):
	changed = bytearray(record["ct"])
	for kind, position, mask in (("first", 0, 1), ("last", 1038, 128), ("zero", 0, 0)):
		if kind == "zero":
			changed[:] = bytes(1039)
		changed[position] ^= mask
		yield f"{index}:{kind}:0", changed
		changed[:] = record["ct"]
	if index == 0:
		for position in range(1007):
			changed[position] ^= 1 << (position % 8)
			yield f"0:body:{position}", changed
			changed[position] ^= 1 << (position % 8)
		for bit in range(256):
			changed[1007 + bit // 8] ^= 1 << (bit % 8)
			yield f"0:confirm:{bit}", changed
			changed[1007 + bit // 8] ^= 1 << (bit % 8)


def generate(path):
	output = ["# DeuceSSH sntrup761 rejection answers v1\n\n"]
	count = 0
	for index, record in enumerate(records(path)):
		rho_hash = hashlib.sha512(b"\x03" + record["sk"][1540:1731]).digest()[:32]
		for label, ciphertext in variants(record, index):
			expected = hashlib.sha512(b"\x00" + rho_hash + ciphertext).digest()[:32]
			if expected == record["ss"]:
				raise ValueError("fallback unexpectedly equals positive answer")
			output.append(f"case = {label}\nfallback = {expected.hex().upper()}\n")
			count += 1
	if count != 1563:
		raise ValueError("unexpected rejection case count")
	return "".join(output).encode("ascii")


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("--check", action="store_true", help="verify the checked-in answers")
	options = parser.parse_args()
	data = Path(__file__).resolve().parent / "vectors"
	expected = generate(data / "sntrup761.rsp")
	output = data / "sntrup761.reject"
	if options.check:
		if output.read_bytes() != expected:
			raise ValueError("rejection corpus differs from independent derivation")
	else:
		output.write_bytes(expected)
	print("sntrup rejection corpus: 1563 cases across 100 official records")


if __name__ == "__main__":
	main()
