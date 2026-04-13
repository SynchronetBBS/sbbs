#!/usr/bin/env python3
"""
RIP 1.54 and ANSI (CTerm) sequence validator.

Validates byte streams against:
  - RIPscrip Graphics Protocol v1.54 (RIPSCRIP.DOC)
  - CTerm ANSI-BBS emulation (cterm.adoc)

Usage as CLI:
    python3 rip_ansi_validator.py <filename> [--ansi-only] [--rip-only] [--quiet]

Usage as library:
    from rip_ansi_validator import validate_file, validate_bytes, Issue
"""

import sys
import re
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import Optional


class Severity(Enum):
	ERROR = auto()
	WARNING = auto()


@dataclass
class Issue:
	"""A single validation issue found in the input."""
	offset: int
	severity: Severity
	category: str  # "RIP" or "ANSI"
	message: str
	raw: bytes = b""

	def __str__(self):
		sev = "ERROR" if self.severity == Severity.ERROR else "WARNING"
		return f"[{sev}] offset {self.offset}: [{self.category}] {self.message}"


# ---------------------------------------------------------------------------
# MegaNum helpers
# ---------------------------------------------------------------------------

MEGANUM_CHARS = set(b"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ")

# NUL (0x00) is used as a sentinel to mark continuation join points.
NUL_SENTINEL = 0x00


def is_meganum(b):
	"""Check if a byte value is a valid MegaNum digit (0-9, A-Z)."""
	return b in MEGANUM_CHARS


def is_meganum_or_nul(b):
	"""Check if a byte is a valid MegaNum digit or a NUL join sentinel."""
	return b in MEGANUM_CHARS or b == NUL_SENTINEL


def parse_meganum(data, width):
	"""Parse a fixed-width MegaNum from bytes, skipping NUL sentinels.
	Returns (value, ok).  NUL sentinels are transparently skipped."""
	# Collect 'width' actual MegaNum digits, skipping NULs
	digits = []
	i = 0
	while len(digits) < width and i < len(data):
		if data[i] == NUL_SENTINEL:
			i += 1
			continue
		if not is_meganum(data[i]):
			return 0, False
		digits.append(data[i])
		i += 1
	if len(digits) < width:
		return 0, False
	val = 0
	for ch in digits:
		if 0x30 <= ch <= 0x39:
			d = ch - 0x30
		elif 0x41 <= ch <= 0x5A:
			d = ch - 0x41 + 10
		else:
			return 0, False
		val = val * 36 + d
	return val, True


# ---------------------------------------------------------------------------
# RIP 1.54 command definitions
# ---------------------------------------------------------------------------
# Each entry: (name, fixed_arg_width, has_text_tail, has_variable_points,
#              last_param_width)
# fixed_arg_width: total MegaNum digits of fixed arguments
# has_text_tail: the last parameter is a variable-length text string
# has_variable_points: npoints:2 prefix then npoints*(x:2 y:2) pairs
# last_param_width: width of the last fixed parameter (for relaxed mode)

RIP_COMMANDS_L0 = {
	ord('w'): ("RIP_TEXT_WINDOW", 10, False, False, 1),      # x0:2 y0:2 x1:2 y1:2 wrap:1 size:1
	ord('v'): ("RIP_VIEWPORT", 8, False, False, 2),           # x0:2 y0:2 x1:2 y1:2
	ord('*'): ("RIP_RESET_WINDOWS", 0, False, False, 0),
	ord('e'): ("RIP_ERASE_WINDOW", 0, False, False, 0),
	ord('E'): ("RIP_ERASE_VIEW", 0, False, False, 0),
	ord('g'): ("RIP_GOTOXY", 4, False, False, 2),             # x:2 y:2
	ord('H'): ("RIP_HOME", 0, False, False, 0),
	ord('>'): ("RIP_ERASE_EOL", 0, False, False, 0),
	ord('c'): ("RIP_COLOR", 2, False, False, 2),              # color:2
	ord('Q'): ("RIP_SET_PALETTE", 32, False, False, 2),       # c1:2 ... c16:2
	ord('a'): ("RIP_ONE_PALETTE", 4, False, False, 2),        # color:2 value:2
	ord('W'): ("RIP_WRITE_MODE", 2, False, False, 2),         # mode:2
	ord('m'): ("RIP_MOVE", 4, False, False, 2),               # x:2 y:2
	ord('T'): ("RIP_TEXT", 0, True, False, 0),
	ord('@'): ("RIP_TEXT_XY", 4, True, False, 2),             # x:2 y:2 + text
	ord('Y'): ("RIP_FONT_STYLE", 8, False, False, 2),         # font:2 dir:2 size:2 res:2
	ord('X'): ("RIP_PIXEL", 4, False, False, 2),              # x:2 y:2
	ord('L'): ("RIP_LINE", 8, False, False, 2),               # x0:2 y0:2 x1:2 y1:2
	ord('R'): ("RIP_RECTANGLE", 8, False, False, 2),          # x0:2 y0:2 x1:2 y1:2
	ord('B'): ("RIP_BAR", 8, False, False, 2),                # x0:2 y0:2 x1:2 y1:2
	ord('C'): ("RIP_CIRCLE", 6, False, False, 2),             # x:2 y:2 radius:2
	ord('O'): ("RIP_OVAL", 12, False, False, 2),              # x:2 y:2 sa:2 ea:2 xr:2 yr:2
	ord('o'): ("RIP_FILLED_OVAL", 8, False, False, 2),        # x:2 y:2 xr:2 yr:2
	ord('A'): ("RIP_ARC", 10, False, False, 2),               # x:2 y:2 sa:2 ea:2 r:2
	ord('V'): ("RIP_OVAL_ARC", 12, False, False, 2),          # x:2 y:2 sa:2 ea:2 rx:2 ry:2
	ord('I'): ("RIP_PIE_SLICE", 10, False, False, 2),         # x:2 y:2 sa:2 ea:2 r:2
	ord('i'): ("RIP_OVAL_PIE_SLICE", 12, False, False, 2),    # x:2 y:2 sa:2 ea:2 rx:2 ry:2
	ord('Z'): ("RIP_BEZIER", 18, False, False, 2),            # x1:2 y1:2 ... x4:2 y4:2 cnt:2
	ord('P'): ("RIP_POLYGON", 0, False, True, 0),
	ord('p'): ("RIP_FILL_POLYGON", 0, False, True, 0),
	ord('l'): ("RIP_POLYLINE", 0, False, True, 0),
	ord('F'): ("RIP_FILL", 6, False, False, 2),               # x:2 y:2 border:2
	ord('='): ("RIP_LINE_STYLE", 8, False, False, 2),         # style:2 user_pat:4 thick:2
	ord('S'): ("RIP_FILL_STYLE", 4, False, False, 2),         # pattern:2 color:2
	ord('s'): ("RIP_FILL_PATTERN", 18, False, False, 2),      # c1:2..c8:2 col:2
	ord('#'): ("RIP_NO_MORE", 0, False, False, 0),
}

RIP_COMMANDS_L1 = {
	ord('M'): ("RIP_MOUSE", 19, True, False, 5),              # num:2 x0:2 y0:2 x1:2 y1:2 clk:1 clr:1 res:5 + text
	ord('K'): ("RIP_KILL_MOUSE_FIELDS", 0, False, False, 0),
	ord('T'): ("RIP_BEGIN_TEXT", 10, False, False, 2),         # x1:2 y1:2 x2:2 y2:2 res:2
	ord('t'): ("RIP_REGION_TEXT", 1, True, False, 1),          # justify:1 + text
	ord('E'): ("RIP_END_TEXT", 0, False, False, 0),
	ord('C'): ("RIP_GET_IMAGE", 9, False, False, 1),           # x0:2 y0:2 x1:2 y1:2 res:1
	ord('P'): ("RIP_PUT_IMAGE", 7, False, False, 1),           # x:2 y:2 mode:2 res:1
	ord('W'): ("RIP_WRITE_ICON", 1, True, False, 1),           # res:1 + filename
	ord('I'): ("RIP_LOAD_ICON", 11, True, False, 2),           # x:2 y:2 mode:2 clip:1 res:2 + filename
	ord('B'): ("RIP_BUTTON_STYLE", 36, False, False, 6),       # ... res:6
	ord('U'): ("RIP_BUTTON", 12, True, False, 1),              # x0:2 y0:2 x1:2 y1:2 hotkey:2 flags:1 res:1 + text
	ord('D'): ("RIP_DEFINE", 5, True, False, 2),               # flags:3 res:2 + text
	0x1B:     ("RIP_QUERY", 4, True, False, 3),                # mode:1 res:3 + text
	ord('G'): ("RIP_COPY_REGION", 12, False, False, 2),        # x0:2 y0:2 x1:2 y1:2 res:2 dest:2
	ord('R'): ("RIP_READ_SCENE", 8, True, False, 8),           # res:8 + filename
	ord('F'): ("RIP_FILE_QUERY", 6, True, False, 4),           # mode:2 res:4 + filename
}

RIP_COMMANDS_L9 = {
	0x1B: ("RIP_ENTER_BLOCK_MODE", 8, True, False, 4),        # mode:1 proto:1 file_type:2 res:4 + filename<>
}


# ---------------------------------------------------------------------------
# Parameter range specifications
# ---------------------------------------------------------------------------
# Each entry maps (level, cmd_char) to a list of (field_name, width, min, max)
# tuples.  Only fields with meaningful range constraints are listed.
# Coordinates: x 0..639, y 0..349.  Colors: 0..15.  Modes per spec.

_X = (0, 639)
_Y = (0, 349)
_C = (0, 15)
_C64 = (0, 63)

PARAM_RANGES = {
	(0, ord('w')): [("x0", 2, 0, 90), ("y0", 2, 0, 42), ("x1", 2, 0, 90), ("y1", 2, 0, 42), ("wrap", 1, 0, 1), ("size", 1, 0, 4)],
	(0, ord('v')): [("x0", 2, *_X), ("y0", 2, *_Y), ("x1", 2, *_X), ("y1", 2, *_Y)],
	(0, ord('g')): [("x", 2, 0, 90), ("y", 2, 0, 42)],
	(0, ord('c')): [("color", 2, *_C)],
	(0, ord('Q')): [("c%d" % i, 2, *_C64) for i in range(16)],
	(0, ord('a')): [("color", 2, *_C), ("value", 2, *_C64)],
	(0, ord('W')): [("mode", 2, 0, 1)],
	(0, ord('m')): [("x", 2, *_X), ("y", 2, *_Y)],
	(0, ord('@')): [("x", 2, *_X), ("y", 2, *_Y)],
	(0, ord('Y')): [("font", 2, 0, 10), ("dir", 2, 0, 1), ("size", 2, 1, 10)],
	(0, ord('X')): [("x", 2, *_X), ("y", 2, *_Y)],
	(0, ord('L')): [("x0", 2, *_X), ("y0", 2, *_Y), ("x1", 2, *_X), ("y1", 2, *_Y)],
	(0, ord('R')): [("x0", 2, *_X), ("y0", 2, *_Y), ("x1", 2, *_X), ("y1", 2, *_Y)],
	(0, ord('B')): [("x0", 2, *_X), ("y0", 2, *_Y), ("x1", 2, *_X), ("y1", 2, *_Y)],
	(0, ord('C')): [("x", 2, *_X), ("y", 2, *_Y), ("radius", 2, 0, 1295)],
	(0, ord('O')): [("x", 2, *_X), ("y", 2, *_Y), ("sa", 2, 0, 360), ("ea", 2, 0, 360), ("xr", 2, 0, 1295), ("yr", 2, 0, 1295)],
	(0, ord('o')): [("x", 2, *_X), ("y", 2, *_Y), ("xr", 2, 0, 1295), ("yr", 2, 0, 1295)],
	(0, ord('A')): [("x", 2, *_X), ("y", 2, *_Y), ("sa", 2, 0, 360), ("ea", 2, 0, 360), ("r", 2, 0, 1295)],
	(0, ord('V')): [("x", 2, *_X), ("y", 2, *_Y), ("sa", 2, 0, 360), ("ea", 2, 0, 360), ("rx", 2, 0, 1295), ("ry", 2, 0, 1295)],
	(0, ord('I')): [("x", 2, *_X), ("y", 2, *_Y), ("sa", 2, 0, 360), ("ea", 2, 0, 360), ("r", 2, 0, 1295)],
	(0, ord('i')): [("x", 2, *_X), ("y", 2, *_Y), ("sa", 2, 0, 360), ("ea", 2, 0, 360), ("rx", 2, 0, 1295), ("ry", 2, 0, 1295)],
	(0, ord('Z')): [("x1", 2, *_X), ("y1", 2, *_Y), ("x2", 2, *_X), ("y2", 2, *_Y), ("x3", 2, *_X), ("y3", 2, *_Y), ("x4", 2, *_X), ("y4", 2, *_Y), ("cnt", 2, 1, 1295)],
	(0, ord('F')): [("x", 2, *_X), ("y", 2, *_Y), ("border", 2, *_C)],
	(0, ord('=')): [("style", 2, 0, 4), None, ("thick", 2, 0, 3)],
	(0, ord('S')): [("pattern", 2, 0, 11), ("color", 2, *_C)],
	(0, ord('s')): [None, None, None, None, None, None, None, None, ("color", 2, *_C)],
	(1, ord('C')): [("x0", 2, *_X), ("y0", 2, *_Y), ("x1", 2, *_X), ("y1", 2, *_Y)],
	(1, ord('P')): [("x", 2, *_X), ("y", 2, *_Y), ("mode", 2, 0, 4)],
	(1, ord('I')): [("x", 2, *_X), ("y", 2, *_Y), ("mode", 2, 0, 4), ("clip", 1, 0, 1)],
	(1, ord('G')): [("x0", 2, *_X), ("y0", 2, *_Y), ("x1", 2, *_X), ("y1", 2, *_Y)],
	(1, ord('U')): [("x0", 2, *_X), ("y0", 2, *_Y), ("x1", 2, *_X), ("y1", 2, *_Y)],
}


def validate_param_ranges(data, ranges, issues, base_offset, name):
	"""Parse fixed-width MegaNum fields and check value ranges.

	Args:
		data: bytes containing MegaNum digits (NUL sentinels already stripped)
		ranges: list from PARAM_RANGES — each entry is (field_name, width, min, max)
		        or None to skip a field (consume width=2 without checking)
		issues: list to append Issue objects to
		base_offset: file offset of the start of data
		name: command name for error messages
	"""
	pos = 0
	for spec in ranges:
		if spec is None:
			# Skip a 2-digit field
			pos += 2
			continue
		field_name, width, lo, hi = spec
		if pos + width > len(data):
			break
		val, ok = parse_meganum(data[pos:pos + width], width)
		if ok and (val < lo or val > hi):
			issues.append(Issue(
				offset=base_offset + pos,
				severity=Severity.ERROR,
				category="RIP",
				message=f"{name}: {field_name}={val} out of range ({lo}..{hi})",
			))
		pos += width


# ---------------------------------------------------------------------------
# ANSI/CTerm CSI final bytes from cterm.adoc
# ---------------------------------------------------------------------------
# Standard CSI sequences: final byte in '@'..'~' (0x40..0x7E)
# With optional intermediate bytes in ' '..'/' (0x20..0x2F)
# With optional parameter bytes in '0'..'?' (0x30..0x3F)

# Valid CSI final bytes (no intermediate), from cterm.adoc
CSI_STANDARD_FINALS = {
	ord('@'),  # ICH
	ord('A'),  # CUU
	ord('B'),  # CUD
	ord('C'),  # CUF
	ord('D'),  # CUB
	ord('E'),  # CNL
	ord('F'),  # CPL
	ord('G'),  # CHA
	ord('H'),  # CUP
	ord('I'),  # CHT
	ord('J'),  # ED
	ord('K'),  # EL
	ord('L'),  # IL
	ord('M'),  # DL / ANSI music
	ord('N'),  # BCAM (ANSI music)
	ord('P'),  # DCH
	ord('S'),  # SU
	ord('T'),  # SD
	ord('X'),  # ECH
	ord('Y'),  # CVT
	ord('Z'),  # CBT
	ord('`'),  # HPA
	ord('a'),  # HPR
	ord('b'),  # REP
	ord('c'),  # DA
	ord('d'),  # VPA
	ord('e'),  # VPR
	ord('f'),  # HVP
	ord('g'),  # TBC
	ord('h'),  # SM
	ord('j'),  # HPB
	ord('k'),  # VPB
	ord('l'),  # RM
	ord('m'),  # SGR
	ord('n'),  # DSR
	ord('p'),  # DECRQM (with $ intermediate)
	ord('q'),  # DECSCUSR (with SP intermediate)
	ord('r'),  # DECSTBM / DECCARA (with $ intermediate) / DECSCS (with * intermediate)
	ord('s'),  # DECSLRM / SCOSC
	ord('t'),  # CT24BC / DECRARA (with $ intermediate)
	ord('u'),  # SCORC
	ord('v'),  # DECCRA (with $ intermediate)
	ord('w'),  # DECTABSR (with $ intermediate)
	ord('x'),  # DECFRA (with $ intermediate) / DECSACE (with * intermediate)
	ord('y'),  # DECRQCRA (with * intermediate) / DECRPM (with $ intermediate)
	ord('z'),  # DECERA (with $ intermediate) / DECINVM (with * intermediate)
	ord('|'),  # ANSI music introducer
	ord('}'),  # DECIC (with ' intermediate)
	ord('~'),  # DECDC (with ' intermediate)
}

# CSI sequences with SP (0x20) intermediate
CSI_SP_FINALS = {
	ord('@'),  # SL
	ord('A'),  # SR
	ord('D'),  # FNT (note: listed as "sp D")
	ord('d'),  # TSR
	ord('q'),  # DECSCUSR
}

# CSI sequences with $ intermediate
CSI_DOLLAR_FINALS = {
	ord('p'),  # DECRQM
	ord('r'),  # DECCARA
	ord('t'),  # DECRARA
	ord('v'),  # DECCRA
	ord('w'),  # DECTABSR
	ord('x'),  # DECFRA
	ord('y'),  # DECRPM response
	ord('z'),  # DECERA
}

# CSI sequences with * intermediate
CSI_STAR_FINALS = {
	ord('r'),  # DECSCS
	ord('x'),  # DECSACE
	ord('y'),  # DECRQCRA
	ord('z'),  # DECINVM
	ord('{'),  # DECMSR response
}

# CSI sequences with ' intermediate
CSI_QUOTE_FINALS = {
	ord('}'),  # DECIC
	ord('~'),  # DECDC
}

# CSI with = prefix (CTerm extensions)
CSI_EQ_FINALS = {
	ord('h'),  # BCSET (=255h), CTELCF (=4h), CTFLCF (=5h)
	ord('l'),  # BCRST (=255l), CTDLCF (=4l)
	ord('M'),  # CTSAM
	ord('n'),  # CTSMRR
	ord('{'),  # CTOSF (deprecated)
	ord('$'),  # with $ p: DECRQM CTerm extension
}

# CSI with ? prefix (DEC private)
CSI_QUESTION_FINALS = {
	ord('h'),  # DECSET
	ord('l'),  # DECRST
	ord('S'),  # XTSRGA
	ord('n'),  # DECDSR
	ord('s'),  # CTSMS
	ord('u'),  # CTRMS
	ord('$'),  # with $ p: DECRQM private
}

# CSI with < prefix
CSI_LT_FINALS = {
	ord('c'),  # CTDA
}

# Valid SGR parameters
VALID_SGR_PARAMS = {
	0, 1, 2, 5, 6, 7, 8, 22, 25, 27,
	30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
	40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
	91, 92, 93, 94, 95, 96, 97,
	100, 101, 102, 103, 104, 105, 106, 107,
}

# Valid DECSET/DECRST mode numbers
VALID_DEC_MODES = {
	6, 7, 9, 25, 31, 32, 33, 34, 35, 67, 69, 80,
	1000, 1001, 1002, 1003, 1004, 1005, 1006, 1007, 1015, 2004,
}

# Valid Fe escape sequences (ESC + 0x40..0x5F)
VALID_FE_SEQUENCES = {
	ord('E'),  # NEL
	ord('F'),  # SSA
	ord('G'),  # ESA
	ord('H'),  # HTS
	ord('J'),  # VTS
	ord('M'),  # RI
	ord('P'),  # DCS
	ord('S'),  # STS
	ord('X'),  # SOS
	ord('['),  # CSI
	ord('\\'), # ST
	ord(']'),  # OSC
	ord('^'),  # PM
	ord('_'),  # APC
}

# Valid Fp escape sequences (ESC + 0x30..0x3F)
VALID_FP_SEQUENCES = {
	ord(':'),  # ESCST (reserved for STS)
	ord('7'),  # DECSC
	ord('8'),  # DECRC
}

# Valid Fs escape sequences (ESC + 0x60..0x7E)
VALID_FS_SEQUENCES = {
	ord('c'),  # RIS
}

# Valid C0 control characters in ANSI-BBS mode
VALID_C0 = {
	0x00,  # NUL (doorway mode)
	0x07,  # BEL
	0x08,  # BS
	0x09,  # HT
	0x0A,  # LF
	0x0D,  # CR
	0x1B,  # ESC
}

# Valid OSC numbers
VALID_OSC_NUMBERS = {4, 8, 10, 11, 104}


# ---------------------------------------------------------------------------
# RIP Validator
# ---------------------------------------------------------------------------

class RIPValidator:
	"""Validates RIP 1.54 command sequences."""

	def __init__(self, data: bytes, relaxed: bool = False):
		self.data = data
		self.issues: list[Issue] = []
		self.pos = 0
		self.relaxed = relaxed
		self.clipboard_initialized = False

	def validate(self) -> list[Issue]:
		"""Scan data for RIP sequences and validate them."""
		# Split into raw lines, then join continuation lines.
		# Per RIPscrip spec rule #5, a backslash (\) just before a
		# line break joins the next line.  A double backslash (\\)
		# is a literal backslash and does NOT continue.
		raw_lines = self.data.split(b'\n')

		# Build (joined_line, base_offset) pairs.
		# base_offset is the file offset of the first byte of the
		# (first constituent) line — good enough for error reporting.
		joined: list[tuple[bytes, int]] = []
		offset = 0
		i = 0
		while i < len(raw_lines):
			line = raw_lines[i]
			base_offset = offset
			offset += len(line) + 1  # +1 for the \n

			# Strip trailing CR for continuation check
			stripped = line
			if stripped.endswith(b'\r'):
				stripped = stripped[:-1]

			# Count trailing backslashes to decide continuation.
			# Insert a NUL sentinel at each join point.
			while self._is_continuation(stripped) and i + 1 < len(raw_lines):
				# Remove the trailing backslash and CR, append next line
				if stripped.endswith(b'\r'):
					stripped = stripped[:-1]
				stripped = stripped[:-1]  # remove the continuation backslash
				stripped = stripped + b'\x00'  # NUL sentinel at join point
				i += 1
				next_line = raw_lines[i]
				offset += len(next_line) + 1
				# Strip CR from next line too
				next_stripped = next_line
				if next_stripped.endswith(b'\r'):
					next_stripped = next_stripped[:-1]
				stripped = stripped + next_stripped

			joined.append((stripped, base_offset))
			i += 1

		for line, base_offset in joined:
			# Strip trailing CR (in case it wasn't already)
			if line.endswith(b'\r'):
				line = line[:-1]
			self._validate_rip_line(line, base_offset)

		return self.issues

	@staticmethod
	def _is_continuation(line: bytes) -> bool:
		"""Check if a line ends with a continuation backslash.

		An odd number of trailing backslashes means continuation;
		an even number means escaped literal backslashes.
		"""
		if not line.endswith(b'\\'):
			return False
		count = 0
		i = len(line) - 1
		while i >= 0 and line[i:i+1] == b'\\':
			count += 1
			i -= 1
		return count % 2 == 1

	def _validate_rip_line(self, line: bytes, base_offset: int):
		"""Validate a single line for RIP commands."""
		# Check for RIP line start: !| at beginning, or SOH/STX | anywhere
		pos = 0
		while pos < len(line):
			rip_start = -1
			if pos == 0 and len(line) >= 2 and line[0:1] == b'!' and line[1:2] == b'|':
				rip_start = 0
				pos = 2
			elif line[pos:pos+1] in (b'\x01', b'\x02') and pos + 1 < len(line) and line[pos+1:pos+2] == b'|':
				rip_start = pos
				pos += 2
			else:
				pos += 1
				continue

			# We found a RIP command introducer, now parse commands
			pos = self._parse_rip_commands(line, pos, base_offset + rip_start)
			break  # rest of line is part of RIP parsing

	def _handle_continuation(self, line: bytes, pos: int):
		"""Check if line ends with continuation backslash."""
		if len(line) > 0 and line[-1:] == b'\\':
			# Could be continuation — count trailing backslashes
			count = 0
			i = len(line) - 1
			while i >= 0 and line[i:i+1] == b'\\':
				count += 1
				i -= 1
			return count % 2 == 1  # Odd number = continuation
		return False

	def _parse_rip_commands(self, line: bytes, pos: int, base_offset: int) -> int:
		"""Parse one or more RIP commands on a line starting after !|"""
		while pos < len(line):
			cmd_start = pos
			# Determine level
			level = 0
			level_digits = []
			while pos < len(line) and 0x31 <= line[pos] <= 0x39:  # '1'-'9'
				level_digits.append(line[pos] - 0x30)
				pos += 1

			if level_digits:
				level = level_digits[0]  # Primary level

			if pos >= len(line):
				break

			cmd_char = line[pos]
			pos += 1

			# Look up command
			cmd_table = None
			if level == 0:
				cmd_table = RIP_COMMANDS_L0
			elif level == 1:
				cmd_table = RIP_COMMANDS_L1
			elif level == 9:
				cmd_table = RIP_COMMANDS_L9

			if cmd_table is None or cmd_char not in cmd_table:
				level_str = "".join(str(d) for d in level_digits) if level_digits else "0"
				if cmd_char == 0x1B:
					cmd_repr = "<ESC>"
				elif 0x20 <= cmd_char <= 0x7E:
					cmd_repr = chr(cmd_char)
				else:
					cmd_repr = f"0x{cmd_char:02X}"
				self.issues.append(Issue(
					offset=base_offset + cmd_start,
					severity=Severity.ERROR,
					category="RIP",
					message=f"Unknown RIP command '{cmd_repr}' at level {level_str}",
					raw=line[cmd_start:min(cmd_start + 20, len(line))],
				))
				# Skip to next | delimiter or end of line
				while pos < len(line) and line[pos:pos+1] != b'|':
					if line[pos:pos+1] == b'\\' and pos + 1 < len(line):
						pos += 2  # Skip escaped char
					else:
						pos += 1
				if pos < len(line) and line[pos:pos+1] == b'|':
					pos += 1
				continue

			name, fixed_width, has_text, has_var_points, last_param_width = cmd_table[cmd_char]
			fixed_start = pos

			if has_var_points:
				# Parse npoints:2 then npoints * (x:2 y:2)
				# Skip any NUL sentinels to find the npoints digits
				while pos < len(line) and line[pos] == NUL_SENTINEL:
					pos += 1
				if pos + 2 > len(line):
					self.issues.append(Issue(
						offset=base_offset + pos,
						severity=Severity.ERROR,
						category="RIP",
						message=f"{name}: missing npoints parameter",
					))
					break
				npoints, ok = parse_meganum(line[pos:pos+4], 2)  # up to 4 to allow NUL
				if not ok:
					self.issues.append(Issue(
						offset=base_offset + pos,
						severity=Severity.ERROR,
						category="RIP",
						message=f"{name}: invalid MegaNum in npoints",
						raw=line[pos:pos+2],
					))
					break
				# Advance past npoints digits + any NULs
				digits_seen = 0
				while digits_seen < 2 and pos < len(line):
					if line[pos] == NUL_SENTINEL:
						pos += 1
					elif is_meganum(line[pos]):
						digits_seen += 1
						pos += 1
					else:
						break
				if npoints < 2 or npoints > 512:
					self.issues.append(Issue(
						offset=base_offset + pos - 2,
						severity=Severity.ERROR,
						category="RIP",
						message=f"{name}: npoints={npoints} out of range (2-512)",
					))
				elif npoints >= 255 and not self.relaxed:
					self.issues.append(Issue(
						offset=base_offset + pos - 2,
						severity=Severity.WARNING,
						category="RIP",
						message=f"{name}: npoints={npoints} exceeds RIPterm's "
						         f"255-vertex limit (undefined behavior in RIPterm)",
					))
				# Collect the vertex data region (MegaNums + NULs)
				vdata_start = pos
				vdata_end = pos
				vdigits = 0
				needed = npoints * 4  # each point is x:2 y:2
				while vdata_end < len(line) and vdigits < needed:
					if line[vdata_end] == NUL_SENTINEL:
						vdata_end += 1
					elif is_meganum(line[vdata_end]):
						vdigits += 1
						vdata_end += 1
					else:
						break
				if vdigits < needed:
					self.issues.append(Issue(
						offset=base_offset + vdata_start,
						severity=Severity.ERROR,
						category="RIP",
						message=f"{name}: expected {needed} MegaNum vertex digits, got {vdigits}",
					))
				else:
					# Validate vertex coordinate ranges
					stripped = bytes(b for b in line[vdata_start:vdata_end] if b != NUL_SENTINEL)
					for vi in range(npoints):
						voff = vi * 4
						if voff + 4 > len(stripped):
							break
						vx, xok = parse_meganum(stripped[voff:voff+2], 2)
						vy, yok = parse_meganum(stripped[voff+2:voff+4], 2)
						if xok and (vx < 0 or vx > 639):
							self.issues.append(Issue(
								offset=base_offset + vdata_start + voff,
								severity=Severity.ERROR,
								category="RIP",
								message=f"{name}: vertex {vi} x={vx} out of range (0..639)",
							))
						if yok and (vy < 0 or vy > 349):
							self.issues.append(Issue(
								offset=base_offset + vdata_start + voff + 2,
								severity=Severity.ERROR,
								category="RIP",
								message=f"{name}: vertex {vi} y={vy} out of range (0..349)",
							))
				pos = vdata_end

			elif fixed_width > 0:
				# Validate fixed-width MegaNum arguments.
				# NUL sentinels mark continuation join points and are
				# skipped when counting digits.
				fixed_start = pos
				avail = len(line) - pos
				# Count MegaNum digits (skipping NULs) and track total
				# bytes consumed
				n_digits = 0
				n_bytes = 0
				while n_bytes < avail:
					b = line[pos + n_bytes]
					if b == NUL_SENTINEL:
						n_bytes += 1
						continue
					if not is_meganum(b):
						break
					n_digits += 1
					n_bytes += 1

				# In relaxed mode, the last parameter may be shorter
				if self.relaxed and last_param_width > 0:
					min_width = fixed_width - last_param_width + 1
				else:
					min_width = fixed_width

				if n_digits < min_width:
					if n_digits < fixed_width:
						self.issues.append(Issue(
							offset=base_offset + pos,
							severity=Severity.ERROR,
							category="RIP",
							message=f"{name}: expected {fixed_width} MegaNum digits, got {n_digits}",
						))
					pos += n_bytes
				else:
					# Consume up to fixed_width digits (plus NULs)
					end = pos
					digits_seen = 0
					while digits_seen < fixed_width and end < pos + n_bytes:
						if line[end] == NUL_SENTINEL:
							end += 1
						elif is_meganum(line[end]):
							digits_seen += 1
							end += 1
						else:
							break
					# Validate parameter value ranges
					range_key = (level, cmd_char)
					if range_key in PARAM_RANGES:
						# Strip NUL sentinels for range checking
						stripped = bytes(b for b in line[pos:end] if b != NUL_SENTINEL)
						validate_param_ranges(
							stripped, PARAM_RANGES[range_key],
							self.issues, base_offset + pos, name)
					pos = end

			# If has text tail, consume rest until | or end (with backslash escaping)
			if has_text:
				while pos < len(line):
					ch = line[pos:pos+1]
					if ch == b'|':
						break
					if ch == b'\\' and pos + 1 < len(line):
						pos += 2  # skip escaped char
					else:
						pos += 1
			else:
				# No text tail — skip any trailing text until | (unrecognized trailing data)
				trail_start = pos
				while pos < len(line) and line[pos:pos+1] != b'|':
					if line[pos:pos+1] == b'\\' and pos + 1 < len(line):
						pos += 2
					else:
						pos += 1
				# Don't warn about trailing data — spec says unrecognized text is ignored

			# Track clipboard state for |1C / |1P dependency
			if level == 1 and cmd_char == ord('C'):
				# Check BGI imagesize limit: ((w+7)/8)*4*h + 6 <= 65535
				stripped = bytes(b for b in line[fixed_start:pos]
				                 if b != NUL_SENTINEL)
				if len(stripped) >= 8:
					x0, x0ok = parse_meganum(stripped[0:2], 2)
					y0, y0ok = parse_meganum(stripped[2:4], 2)
					x1, x1ok = parse_meganum(stripped[4:6], 2)
					y1, y1ok = parse_meganum(stripped[6:8], 2)
					if x0ok and y0ok and x1ok and y1ok:
						w = abs(x1 - x0) + 1
						h = abs(y1 - y0) + 1
						imgsz = ((w + 7) // 8) * 4 * h + 6
						if imgsz > 65535:
							self.issues.append(Issue(
								offset=base_offset + cmd_start,
								severity=Severity.ERROR,
								category="RIP",
								message=f"{name}: image too large ({w}x{h}, "
								        f"{imgsz} bytes > 65535 BGI limit)",
								raw=line[cmd_start:min(cmd_start + 20, len(line))],
							))
						else:
							self.clipboard_initialized = True
				else:
					self.clipboard_initialized = True
			elif level == 1 and cmd_char == ord('P'):
				if not self.clipboard_initialized:
					self.issues.append(Issue(
						offset=base_offset + cmd_start,
						severity=Severity.WARNING if self.relaxed else Severity.ERROR,
						category="RIP",
						message=f"{name}: |1P used before any |1C (clipboard not initialized)",
						raw=line[cmd_start:min(cmd_start + 20, len(line))],
					))

			# Skip | delimiter
			if pos < len(line) and line[pos:pos+1] == b'|':
				pos += 1

		return pos


# ---------------------------------------------------------------------------
# ANSI/CTerm Validator
# ---------------------------------------------------------------------------

class ANSIValidator:
	"""Validates ANSI/CTerm escape sequences."""

	def __init__(self, data: bytes):
		self.data = data
		self.issues: list[Issue] = []

	def _in_rip_command(self, pos):
		"""Check if pos is inside a RIP command sequence."""
		# Scan backward looking for an introducer (! at BOL, or SOH/STX
		# anywhere) followed by |.  Stop at CR/LF.
		j = pos - 1
		while j >= 0 and self.data[j] not in (0x0A, 0x0D):
			# SOH| or STX| can appear mid-line
			if self.data[j] in (0x01, 0x02) and j + 1 < len(self.data) and self.data[j + 1] == 0x7C:
				return True
			j -= 1
		# Check if line starts with !|
		line_start = j + 1
		if line_start + 1 < len(self.data):
			if self.data[line_start] == 0x21 and self.data[line_start + 1] == 0x7C:
				return True
		return False

	def validate(self) -> list[Issue]:
		"""Scan data for ANSI escape sequences and validate them."""
		data = self.data
		i = 0
		while i < len(data):
			b = data[i]

			if b == 0x1B:
				if self._in_rip_command(i):
					i += 1
					continue
				i = self._validate_escape(i)
			elif b < 0x20 and b not in VALID_C0:
				# C0 control not in the valid set — check if it's a
				# music terminator or shift character
				if b in (0x0E, 0x0F):
					pass  # SO/SI used with ANSI music
				else:
					self.issues.append(Issue(
						offset=i,
						severity=Severity.WARNING,
						category="ANSI",
						message=f"Non-standard C0 control character 0x{b:02X}",
					))
				i += 1
			else:
				i += 1

		return self.issues

	def _validate_escape(self, start: int) -> int:
		"""Validate an escape sequence starting at position start."""
		data = self.data
		i = start + 1

		if i >= len(data):
			self.issues.append(Issue(
				offset=start,
				severity=Severity.ERROR,
				category="ANSI",
				message="Truncated ESC sequence at end of data",
			))
			return i

		b = data[i]

		# CSI: ESC [
		if b == ord('['):
			return self._validate_csi(start)

		# Fe sequences: ESC + 0x40..0x5F
		if 0x40 <= b <= 0x5F:
			if b in VALID_FE_SEQUENCES:
				# DCS, OSC, PM, APC, SOS need string parsing
				if b == ord('P'):  # DCS
					return self._skip_string_body(i + 1, start, "DCS")
				elif b == ord(']'):  # OSC
					return self._validate_osc(start, i + 1)
				elif b in (ord('^'), ord('_'), ord('X')):  # PM, APC, SOS
					return self._skip_string_body(i + 1, start,
						{ord('^'): "PM", ord('_'): "APC", ord('X'): "SOS"}[b])
				elif b == ord('\\'):  # ST — valid on its own
					return i + 1
				else:
					return i + 1
			else:
				# Legal Fe combinations not handled are silently dropped per spec
				# but we still report as warning
				self.issues.append(Issue(
					offset=start,
					severity=Severity.WARNING,
					category="ANSI",
					message=f"Unhandled Fe escape sequence ESC {chr(b)} (0x{b:02X})",
				))
				return i + 1

		# Fp sequences: ESC + 0x30..0x3F
		if 0x30 <= b <= 0x3F:
			if b in VALID_FP_SEQUENCES:
				return i + 1
			else:
				# Legal per ECMA-35, silently dropped per cterm.adoc
				self.issues.append(Issue(
					offset=start,
					severity=Severity.WARNING,
					category="ANSI",
					message=f"Unhandled Fp escape sequence ESC {chr(b)} (0x{b:02X})",
				))
				return i + 1

		# Fs sequences: ESC + 0x60..0x7E
		if 0x60 <= b <= 0x7E:
			if b in VALID_FS_SEQUENCES:
				return i + 1
			else:
				self.issues.append(Issue(
					offset=start,
					severity=Severity.WARNING,
					category="ANSI",
					message=f"Unhandled Fs escape sequence ESC {chr(b)} (0x{b:02X})",
				))
				return i + 1

		# nF sequences: ESC + 0x20..0x2F then more intermediates then final 0x30..0x7E
		if 0x20 <= b <= 0x2F:
			j = i + 1
			while j < len(data) and 0x20 <= data[j] <= 0x2F:
				j += 1
			if j < len(data) and 0x30 <= data[j] <= 0x7E:
				# Valid nF structure but CTerm doesn't support any
				self.issues.append(Issue(
					offset=start,
					severity=Severity.WARNING,
					category="ANSI",
					message=f"nF escape sequence (not supported by CTerm)",
					raw=data[start:j+1],
				))
				return j + 1
			else:
				self.issues.append(Issue(
					offset=start,
					severity=Severity.ERROR,
					category="ANSI",
					message="Malformed nF escape sequence",
					raw=data[start:min(start + 10, len(data))],
				))
				return j

		# ESC followed by something unexpected
		self.issues.append(Issue(
			offset=start,
			severity=Severity.ERROR,
			category="ANSI",
			message=f"Invalid byte 0x{b:02X} after ESC",
		))
		return i + 1

	def _validate_csi(self, start: int) -> int:
		"""Validate a CSI sequence: ESC [ params intermediate final"""
		data = self.data
		i = start + 2  # skip ESC [

		if i >= len(data):
			self.issues.append(Issue(
				offset=start,
				severity=Severity.ERROR,
				category="ANSI",
				message="Truncated CSI sequence",
			))
			return i

		# Collect parameter bytes (0x30..0x3F)
		param_start = i
		prefix = None
		if i < len(data) and data[i] in (ord('<'), ord('='), ord('>'), ord('?')):
			prefix = chr(data[i])
			i += 1

		# Collect parameter string (digits and ;)
		while i < len(data) and 0x30 <= data[i] <= 0x3F:
			i += 1
		param_end = i

		# Collect intermediate bytes (0x20..0x2F)
		inter_start = i
		while i < len(data) and 0x20 <= data[i] <= 0x2F:
			i += 1
		inter_end = i
		intermediates = data[inter_start:inter_end]

		# Final byte (0x40..0x7E)
		if i >= len(data):
			self.issues.append(Issue(
				offset=start,
				severity=Severity.ERROR,
				category="ANSI",
				message="Truncated CSI sequence (no final byte)",
				raw=data[start:i],
			))
			return i

		final = data[i]
		if not (0x40 <= final <= 0x7E):
			self.issues.append(Issue(
				offset=start,
				severity=Severity.ERROR,
				category="ANSI",
				message=f"Invalid CSI final byte 0x{final:02X} (expected 0x40..0x7E)",
				raw=data[start:i+1],
			))
			return i + 1

		i += 1  # past final byte

		# Now validate the specific combination
		param_bytes = data[param_start:param_end]
		# Strip prefix from param_bytes if present
		if prefix:
			param_bytes = param_bytes[1:]

		# Parse numeric parameters
		params = self._parse_csi_params(param_bytes)

		if prefix == '?':
			if len(intermediates) == 0:
				if final not in CSI_QUESTION_FINALS:
					self.issues.append(Issue(
						offset=start,
						severity=Severity.ERROR,
						category="ANSI",
						message=f"Unknown CSI ? sequence with final '{chr(final)}'",
						raw=data[start:i],
					))
				elif final in (ord('h'), ord('l')):
					# Validate mode numbers
					for p in params:
						if p is not None and p not in VALID_DEC_MODES:
							self.issues.append(Issue(
								offset=start,
								severity=Severity.WARNING,
								category="ANSI",
								message=f"Unrecognized DECSET/DECRST mode {p}",
								raw=data[start:i],
							))
			elif intermediates == b'$' and final == ord('p'):
				pass  # DECRQM private — valid
			elif intermediates == b'$' and final == ord('y'):
				pass  # DECRPM response — valid
			else:
				self.issues.append(Issue(
					offset=start,
					severity=Severity.ERROR,
					category="ANSI",
					message=f"Unknown CSI ? sequence with intermediate '{intermediates.decode(errors='replace')}' final '{chr(final)}'",
					raw=data[start:i],
				))

		elif prefix == '=':
			if len(intermediates) == 0:
				if final not in CSI_EQ_FINALS:
					self.issues.append(Issue(
						offset=start,
						severity=Severity.ERROR,
						category="ANSI",
						message=f"Unknown CSI = sequence with final '{chr(final)}'",
						raw=data[start:i],
					))
			elif intermediates == b'$' and final == ord('p'):
				pass  # DECRQM CTerm extension
			elif intermediates == b'$' and final == ord('y'):
				pass  # DECRPM CTerm response
			else:
				self.issues.append(Issue(
					offset=start,
					severity=Severity.ERROR,
					category="ANSI",
					message=f"Unknown CSI = sequence with intermediate",
					raw=data[start:i],
				))

		elif prefix == '<':
			if len(intermediates) == 0:
				if final not in CSI_LT_FINALS:
					self.issues.append(Issue(
						offset=start,
						severity=Severity.ERROR,
						category="ANSI",
						message=f"Unknown CSI < sequence with final '{chr(final)}'",
						raw=data[start:i],
					))
			elif intermediates == b'<' and final == ord('M'):
				pass  # SGR mouse report press — valid extension
			elif intermediates == b'<' and final == ord('m'):
				pass  # SGR mouse report release — valid extension
			else:
				self.issues.append(Issue(
					offset=start,
					severity=Severity.ERROR,
					category="ANSI",
					message=f"Unknown CSI < sequence",
					raw=data[start:i],
				))

		elif prefix is None:
			# Standard CSI sequence
			if len(intermediates) == 0:
				if final == ord('!'):
					# RIP detection: CSI ! / CSI 0 ! / CSI 1 ! / CSI 2 !
					# These are actually RIP-specific ANSI extensions
					if params in ([None], [0], [1], [2]):
						pass  # valid RIP ANSI sequences
					else:
						self.issues.append(Issue(
							offset=start,
							severity=Severity.ERROR,
							category="ANSI",
							message=f"Unknown CSI Ps ! sequence with params {params}",
							raw=data[start:i],
						))
				elif final not in CSI_STANDARD_FINALS:
					self.issues.append(Issue(
						offset=start,
						severity=Severity.ERROR,
						category="ANSI",
						message=f"Unknown CSI final byte '{chr(final)}' (0x{final:02X})",
						raw=data[start:i],
					))
				elif final == ord('m'):
					# Validate SGR parameters
					self._validate_sgr(params, start, data[start:i])
			elif len(intermediates) == 1:
				inter = intermediates[0]
				if inter == 0x20:  # SP
					if final not in CSI_SP_FINALS:
						self.issues.append(Issue(
							offset=start,
							severity=Severity.ERROR,
							category="ANSI",
							message=f"Unknown CSI Pn SP {chr(final)} sequence",
							raw=data[start:i],
						))
				elif inter == ord('$'):
					if final not in CSI_DOLLAR_FINALS:
						self.issues.append(Issue(
							offset=start,
							severity=Severity.ERROR,
							category="ANSI",
							message=f"Unknown CSI $ {chr(final)} sequence",
							raw=data[start:i],
						))
				elif inter == ord('*'):
					if final not in CSI_STAR_FINALS:
						self.issues.append(Issue(
							offset=start,
							severity=Severity.ERROR,
							category="ANSI",
							message=f"Unknown CSI * {chr(final)} sequence",
							raw=data[start:i],
						))
				elif inter == ord("'"):
					if final not in CSI_QUOTE_FINALS:
						self.issues.append(Issue(
							offset=start,
							severity=Severity.ERROR,
							category="ANSI",
							message=f"Unknown CSI ' {chr(final)} sequence",
							raw=data[start:i],
						))
				else:
					self.issues.append(Issue(
						offset=start,
						severity=Severity.ERROR,
						category="ANSI",
						message=f"Unknown intermediate byte 0x{inter:02X} in CSI sequence",
						raw=data[start:i],
					))
			else:
				self.issues.append(Issue(
					offset=start,
					severity=Severity.ERROR,
					category="ANSI",
					message=f"Multiple intermediate bytes in CSI sequence",
					raw=data[start:i],
				))

		else:
			# prefix is > — not used in cterm.adoc
			self.issues.append(Issue(
				offset=start,
				severity=Severity.ERROR,
				category="ANSI",
				message=f"Unknown CSI prefix '{prefix}'",
				raw=data[start:i],
			))

		return i

	def _parse_csi_params(self, param_bytes: bytes) -> list[Optional[int]]:
		"""Parse semicolon-separated decimal parameters from CSI param bytes."""
		if not param_bytes:
			return [None]
		parts = param_bytes.split(b';')
		result = []
		for p in parts:
			if not p:
				result.append(None)
			else:
				try:
					result.append(int(p))
				except ValueError:
					result.append(None)
		return result

	def _validate_sgr(self, params: list[Optional[int]], offset: int, raw: bytes):
		"""Validate SGR (Select Graphic Rendition) parameters."""
		i = 0
		while i < len(params):
			p = params[i]
			if p is None:
				p = 0  # default
			if p in (38, 48):
				# Extended color — need sub-parameters
				if i + 1 < len(params) and params[i + 1] == 5:
					# 256-color: 38;5;N or 48;5;N
					if i + 2 < len(params) and params[i + 2] is not None:
						idx = params[i + 2]
						if idx < 0 or idx > 255:
							self.issues.append(Issue(
								offset=offset,
								severity=Severity.ERROR,
								category="ANSI",
								message=f"SGR {p};5;{idx}: palette index out of range (0-255)",
								raw=raw,
							))
						i += 3
					else:
						self.issues.append(Issue(
							offset=offset,
							severity=Severity.ERROR,
							category="ANSI",
							message=f"SGR {p};5 missing palette index",
							raw=raw,
						))
						i += 2
				elif i + 1 < len(params) and params[i + 1] == 2:
					# Direct color: 38;2;R;G;B or 48;2;R;G;B
					if i + 4 < len(params):
						for j, comp in enumerate(("R", "G", "B")):
							v = params[i + 2 + j]
							if v is not None and (v < 0 or v > 255):
								self.issues.append(Issue(
									offset=offset,
									severity=Severity.ERROR,
									category="ANSI",
									message=f"SGR {p};2;...{comp}={v} out of range (0-255)",
									raw=raw,
								))
						i += 5
					else:
						self.issues.append(Issue(
							offset=offset,
							severity=Severity.ERROR,
							category="ANSI",
							message=f"SGR {p};2 missing R;G;B components",
							raw=raw,
						))
						i = len(params)
				else:
					self.issues.append(Issue(
						offset=offset,
						severity=Severity.ERROR,
						category="ANSI",
						message=f"SGR {p} missing color-space selector (2 or 5)",
						raw=raw,
					))
					i += 1
			elif p not in VALID_SGR_PARAMS:
				self.issues.append(Issue(
					offset=offset,
					severity=Severity.WARNING,
					category="ANSI",
					message=f"Unrecognized SGR parameter {p}",
					raw=raw,
				))
				i += 1
			else:
				i += 1

	def _validate_osc(self, esc_start: int, body_start: int) -> int:
		"""Validate an OSC sequence, starting at the byte after ESC ]."""
		data = self.data
		i = body_start
		# Find ST (ESC \) or BEL (0x07)
		osc_body_start = i
		while i < len(data):
			if data[i] == 0x1B and i + 1 < len(data) and data[i + 1] == ord('\\'):
				osc_body = data[osc_body_start:i]
				i += 2  # skip ST
				break
			elif data[i] == 0x07:
				osc_body = data[osc_body_start:i]
				i += 1
				break
			# Check for valid string content (0x08-0x0d, 0x20-0x7e)
			b = data[i]
			if not ((0x08 <= b <= 0x0D) or (0x20 <= b <= 0x7E)):
				self.issues.append(Issue(
					offset=i,
					severity=Severity.ERROR,
					category="ANSI",
					message=f"Invalid byte 0x{b:02X} in OSC string body",
				))
				return i + 1
			i += 1
		else:
			self.issues.append(Issue(
				offset=esc_start,
				severity=Severity.ERROR,
				category="ANSI",
				message="Unterminated OSC sequence",
			))
			return i

		# Parse OSC number
		semi = osc_body.find(b';')
		if semi >= 0:
			num_str = osc_body[:semi]
		else:
			num_str = osc_body

		try:
			osc_num = int(num_str)
		except ValueError:
			self.issues.append(Issue(
				offset=esc_start,
				severity=Severity.ERROR,
				category="ANSI",
				message=f"Invalid OSC number '{num_str.decode(errors='replace')}'",
				raw=data[esc_start:i],
			))
			return i

		if osc_num not in VALID_OSC_NUMBERS:
			self.issues.append(Issue(
				offset=esc_start,
				severity=Severity.ERROR,
				category="ANSI",
				message=f"Unknown OSC {osc_num} (valid: {sorted(VALID_OSC_NUMBERS)})",
				raw=data[esc_start:i],
			))

		return i

	def _skip_string_body(self, start: int, esc_start: int, kind: str) -> int:
		"""Skip a DCS/PM/APC/SOS string body until ST (ESC \\)."""
		data = self.data
		i = start
		while i < len(data):
			if data[i] == 0x1B and i + 1 < len(data) and data[i + 1] == ord('\\'):
				return i + 2  # past ST

			# SOS can contain anything except SOS and ST
			if kind == "SOS":
				i += 1
				continue

			# DCS/PM/APC: valid bytes are 0x08-0x0d, 0x20-0x7e
			b = data[i]
			if b == 0x1B:
				# Could be start of ST — but we already checked ESC \
				# Or could be invalid
				i += 1
				continue
			if not ((0x08 <= b <= 0x0D) or (0x20 <= b <= 0x7E)):
				self.issues.append(Issue(
					offset=i,
					severity=Severity.ERROR,
					category="ANSI",
					message=f"Invalid byte 0x{b:02X} in {kind} string body",
				))
				return i + 1  # abort
			i += 1

		self.issues.append(Issue(
			offset=esc_start,
			severity=Severity.ERROR,
			category="ANSI",
			message=f"Unterminated {kind} string",
		))
		return i


# ---------------------------------------------------------------------------
# SAUCE record stripping
# ---------------------------------------------------------------------------

# SAUCE record layout (from saucedefs.h):
#   Last 128 bytes of file:
#     id[5] "SAUCE" + ver[2] "00" + title[35] + author[20] + group[20]
#     + date[8] + filesize[4] + datatype[1] + filetype[1]
#     + tinfo1[2] + tinfo2[2] + tinfo3[2] + tinfo4[2]
#     + comments[1] + tflags[1] + tinfos[22]   = 128 bytes total
#   Optional comment block preceding SAUCE record:
#     "COMNT" (5 bytes) + comments * 64 bytes
#   A 0x1A (Ctrl-Z / EOF separator) typically precedes the COMNT/SAUCE block

SAUCE_RECORD_LEN = 128
SAUCE_ID = b"SAUCE"
SAUCE_VER = b"00"
SAUCE_COMNT_ID = b"COMNT"


def strip_sauce(data: bytes) -> bytes:
	"""Strip a SAUCE record (and optional COMNT block and EOF separator)
	from the end of data. Returns the content portion only.

	If no valid SAUCE record is found, returns data unchanged.
	"""
	if len(data) < SAUCE_RECORD_LEN:
		return data

	sauce = data[-SAUCE_RECORD_LEN:]
	if sauce[:5] != SAUCE_ID or sauce[5:7] != SAUCE_VER:
		return data

	# Number of comment lines
	num_comments = sauce[104]  # comments field offset within SAUCE record

	end = len(data) - SAUCE_RECORD_LEN

	# Check for COMNT block
	if num_comments > 0:
		comnt_len = 5 + num_comments * 64
		comnt_start = end - comnt_len
		if comnt_start >= 0 and data[comnt_start:comnt_start + 5] == SAUCE_COMNT_ID:
			end = comnt_start

	# Strip preceding Ctrl-Z EOF separator if present
	if end > 0 and data[end - 1] == 0x1A:
		end -= 1

	return data[:end]


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

@dataclass
class ValidationResult:
	"""Result of validating a byte stream."""
	issues: list[Issue] = field(default_factory=list)

	@property
	def errors(self) -> list[Issue]:
		return [i for i in self.issues if i.severity == Severity.ERROR]

	@property
	def warnings(self) -> list[Issue]:
		return [i for i in self.issues if i.severity == Severity.WARNING]

	@property
	def is_valid(self) -> bool:
		return len(self.errors) == 0

	def __str__(self):
		if not self.issues:
			return "No issues found."
		return "\n".join(str(i) for i in self.issues)


def validate_bytes(data: bytes, *, check_ansi: bool = True,
                   check_rip: bool = True,
                   relaxed: bool = False) -> ValidationResult:
	"""Validate a byte stream for RIP 1.54 and/or ANSI sequence errors.

	SAUCE records (and optional COMNT blocks) are automatically stripped
	before validation so that trailing metadata is not flagged as errors.

	Args:
		data: Raw byte content to validate.
		check_ansi: If True, validate ANSI/CTerm escape sequences.
		check_rip: If True, validate RIPscrip 1.54 commands.
		relaxed: If True, allow the last parameter of each RIP command
		         to be shorter than its spec width (minimum 1 digit).

	Returns:
		ValidationResult with any issues found.
	"""
	data = strip_sauce(data)
	result = ValidationResult()

	if check_rip:
		rip = RIPValidator(data, relaxed=relaxed)
		result.issues.extend(rip.validate())

	if check_ansi:
		ansi = ANSIValidator(data)
		result.issues.extend(ansi.validate())

	# Sort by offset
	result.issues.sort(key=lambda i: i.offset)
	return result


def validate_file(path: str, *, check_ansi: bool = True,
                  check_rip: bool = True,
                  relaxed: bool = False) -> ValidationResult:
	"""Validate a file for RIP 1.54 and/or ANSI sequence errors.

	Args:
		path: Path to the file to validate.
		check_ansi: If True, validate ANSI/CTerm escape sequences.
		check_rip: If True, validate RIPscrip 1.54 commands.
		relaxed: If True, allow the last parameter of each RIP command
		         to be shorter than its spec width (minimum 1 digit).

	Returns:
		ValidationResult with any issues found.
	"""
	with open(path, "rb") as f:
		data = f.read()
	return validate_bytes(data, check_ansi=check_ansi, check_rip=check_rip,
	                      relaxed=relaxed)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
	import argparse

	parser = argparse.ArgumentParser(
		description="Validate RIP 1.54 and ANSI sequences in a file.",
		epilog="Based strictly on RIPSCRIP.DOC v1.54 and CTerm cterm.adoc.",
	)
	parser.add_argument("filename", nargs="+", help="File(s) to validate")
	parser.add_argument("--ansi-only", action="store_true",
	                    help="Only check ANSI sequences")
	parser.add_argument("--rip-only", action="store_true",
	                    help="Only check RIP sequences")
	parser.add_argument("--quiet", "-q", action="store_true",
	                    help="Only print errors, not warnings")
	parser.add_argument("--errors-only", action="store_true",
	                    help="Only show errors (no warnings)")
	parser.add_argument("--relaxed", action="store_true",
	                    help="Allow the last parameter of RIP commands to be "
	                         "shorter than spec width (minimum 1 digit)")

	args = parser.parse_args()

	check_ansi = not args.rip_only
	check_rip = not args.ansi_only

	total_errors = 0
	total_warnings = 0

	for filename in args.filename:
		try:
			result = validate_file(filename, check_ansi=check_ansi,
			                       check_rip=check_rip, relaxed=args.relaxed)
		except FileNotFoundError:
			print(f"Error: file not found: {filename}", file=sys.stderr)
			total_errors += 1
			continue
		except Exception as e:
			print(f"Error reading {filename}: {e}", file=sys.stderr)
			total_errors += 1
			continue

		issues = result.issues
		if args.errors_only or args.quiet:
			issues = result.errors

		if not issues:
			if not args.quiet and len(args.filename) == 1:
				print(f"{filename}: OK (no issues found)")
			continue

		for issue in issues:
			print(f"{filename}:{issue}")

		total_errors += len(result.errors)
		total_warnings += len(result.warnings)

	if not args.quiet and len(args.filename) > 1:
		print(f"\n{total_errors} error(s), {total_warnings} warning(s) across {len(args.filename)} file(s)")

	sys.exit(1 if total_errors > 0 else 0)


if __name__ == "__main__":
	main()
