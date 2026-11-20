#!/usr/bin/env python3
"""Reproduce the checked-in ML-KEM-768 fixture from pinned NIST ACVP JSON."""
import argparse
from collections import Counter
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parent
COMMIT = "975de31eb83d87039ec88934fdc47d8c312b892d"
BASE = f"https://raw.githubusercontent.com/usnistgov/ACVP-Server/{COMMIT}"
PROFILES = ("ML-KEM-keyGen-FIPS203", "ML-KEM-encapDecap-FIPS203", "ML-KEM-encapDecap-FIPS203-tr1")
FIELDS = {"d": 32, "z": 32, "m": 32, "ek": 1184, "dk": 2400, "c": 1088, "k": 32}
KINDS = {"keyGen": "KEYGEN", "encapsulation": "ENCAP", "decapsulation": "DECAP", "seed": "DECAP_SEED", "encapsulationKeyCheck": "PUBLIC_CHECK", "decapsulationKeyCheck": "PRIVATE_CHECK"}


def require(condition, detail="invalid pinned reference data"):
	if not condition:
		raise ValueError(f"ACVP corpus check failed: {detail}")


def main():
	parser = argparse.ArgumentParser(description=__doc__)
	parser.add_argument("inputs", type=Path, help="directory containing the pinned upstream JSON files")
	parser.add_argument("--check", action="store_true", help="verify existing generated files without writing")
	args = parser.parse_args()
	manifest = json.loads((ROOT / "data/mlkem_acvp_sources.json").read_text())
	require(manifest["commit"] == COMMIT)
	for name, entry in manifest["inputs"].items():
		raw = (args.inputs / name).read_bytes()
		require(len(raw) == entry["bytes"] and hashlib.sha256(raw).hexdigest() == entry["sha256"], name)
	records = []
	for profile in PROFILES:
		def load(which):
			data = json.loads((args.inputs / f"{profile}-{which}.json").read_text())
			require(data["algorithm"] == "ML-KEM" and data["vsId"] == 42)
			return data
		prompt, expected = load("prompt"), load("expectedResults")
		require(all(prompt[k] == expected[k] for k in ("algorithm", "mode", "revision", "vsId", "isSample")))
		answers = {g["tgId"]: {t["tcId"]: t for t in g["tests"]} for g in expected["testGroups"]}
		projected = {} if "keyGen" in profile else {g["tgId"]: {t["tcId"]: t for t in g["tests"]} for g in load("internalProjection")["testGroups"]}
		for group in prompt["testGroups"]:
			if group["parameterSet"] != "ML-KEM-768":
				continue
			function = group.get("function", "keyGen")
			kind = KINDS["seed" if function == "decapsulation" and group.get("keyFormat") == "seed" else function]
			require(len(group["tests"]) == len(answers[group["tgId"]]))
			for question in group["tests"]:
				answer = answers[group["tgId"]][question["tcId"]]
				merged = dict(question)
				for key, value in answer.items():
					require(key not in merged or merged[key] == value)
					merged[key] = value
				projection = projected.get(group["tgId"], {}).get(question["tcId"], {})
				require(not projection.get("deferred", False))
				for key, value in merged.items():
					require(not projection or projection[key] == value, (profile, question["tcId"], key))
				if kind == "DECAP_SEED":
					merged.update(ek=projection["ek"], dk=projection["dk"])
				values = {}
				for field, length in FIELDS.items():
					if field in merged:
						value = merged[field]
						require(isinstance(value, str) and len(value) == 2 * length and re.fullmatch(r"[0-9A-Fa-f]+", value))
						values[field] = bytes.fromhex(value)
				required = {"KEYGEN": {"d", "z", "ek", "dk"}, "ENCAP": {"ek", "m", "c", "k"}, "DECAP": {"dk", "c", "k"}, "DECAP_SEED": {"d", "z", "ek", "dk", "c", "k"}, "PUBLIC_CHECK": {"ek"}, "PRIVATE_CHECK": {"dk"}}[kind]
				require(set(values) == required)
				valid = merged.get("testPassed", True)
				require(isinstance(valid, bool))
				implicit = projection.get("reason") == "modified ciphertext"
				if kind in ("KEYGEN", "DECAP_SEED"):
					require(values["dk"][1152:2336] == values["ek"])
					require(values["dk"][2336:2368] == hashlib.sha3_256(values["ek"]).digest())
					require(values["dk"][2368:] == values["z"])
				if kind in ("DECAP", "DECAP_SEED"):
					z = values["z"] if kind == "DECAP_SEED" else values["dk"][-32:]
					require((hashlib.shake_256(z + values["c"]).digest(32) == values["k"]) == implicit)
				records.append(dict(profile=profile, group=group["tgId"], case=question["tcId"], kind=kind, valid=valid, implicit=implicit, values=values))
	counts = Counter(record["kind"] for record in records)
	require(counts == {"KEYGEN": 25, "ENCAP": 50, "DECAP": 20, "DECAP_SEED": 10, "PUBLIC_CHECK": 20, "PRIVATE_CHECK": 20})
	require(len({(r["profile"], r["group"], r["case"]) for r in records}) == 145)
	require(sum(r["implicit"] for r in records) == 15)
	lines = ["/* NIST ACVP reference data, pinned at " + COMMIT + ".",
			 " * Changed 2026-11-07: selected every ML-KEM-768 case and encoded as C17 fixtures.",
			 " * See data/mlkem_acvp_sources.json and data/NIST-ACVP-NOTICE.txt.",
			 " * Regenerate with: python3 test/import_mlkem_acvp.py INPUT_DIRECTORY", " */"]
	for index, record in enumerate(records):
		lines += [f"static const struct dssh_acvp_case acvp_case_{index} = {{", f'\t.profile = "{record["profile"]}",', f'\t.group = {record["group"]}, .id = {record["case"]}, .kind = DSSH_ACVP_{record["kind"]},', f'\t.valid = {str(record["valid"]).lower()}, .implicit_rejection = {str(record["implicit"]).lower()},']
		for field, value in record["values"].items():
			encoded = value.hex().upper()
			chunks = [encoded[i:i + 2048] for i in range(0, len(encoded), 2048)]
			lines.append(f"\t.{field} = {{{{")
			for chunk in chunks:
				pieces = [chunk[i:i + 128] for i in range(0, len(chunk), 128)]
				lines += ['\t\t"' + piece + '"' + ("," if i + 1 == len(pieces) else "") for i, piece in enumerate(pieces)]
			lines.append(f"\t}}, {len(value)}}},")
		lines.append("};")
	lines += ["static const struct dssh_acvp_case *const acvp_cases[] = {"]
	lines += [f"\t&acvp_case_{i}," for i in range(len(records))]
	lines += ["};", ""]
	output = "\n".join(lines)
	destination = ROOT / "mlkem_acvp_vectors.h"
	notice = (args.inputs / "README.md").read_text().split("## License", 1)[1].split("## Contributions", 1)[0].strip() + "\n"
	notice_path = ROOT / "data/NIST-ACVP-NOTICE.txt"
	if args.check:
		require(destination.read_text() == output, "Generated vector header differs")
		require(notice_path.read_text() == notice, "NIST notice differs")
	else:
		destination.write_text(output)
		notice_path.write_text(notice)
	print("ML-KEM-768 cases:", dict(counts), "implicit rejection: 15; total: 145")


if __name__ == "__main__":
	main()
