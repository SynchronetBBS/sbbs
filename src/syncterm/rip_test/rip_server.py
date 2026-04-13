#!/usr/bin/env python3
"""RIP rendering test harness — telnet server that captures and compares screenshots.

Serves .rip files over telnet, captures window screenshots via xwd, and compares
SyncTERM vs RIPterm rendering at 640x350 native resolution.

Usage:
    python3 rip_server.py <rip_directory> [--port 1513] [--output <output_dir>]
"""

import argparse
import glob
import os
import socket
import struct
import subprocess
import sys
import tempfile
import time

# Add parent directory so we can import ripdiff
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..'))
import ripdiff

# Telnet protocol constants
IAC  = 0xFF
DONT = 0xFE
DO   = 0xFD
WONT = 0xFC
WILL = 0xFB
SB   = 0xFA
SE   = 0xF0

# Telnet options
OPT_BINARY = 0x00
OPT_ECHO   = 0x01
OPT_SGA    = 0x03  # Suppress Go Ahead

# RIP sequences — SOH introducer, single |# terminator, \r\n to dispatch and BOL.
RIP_RESET       = b'\x01|*|#\r\n'
RIP_SBAROFF     = b'\x01|1\x1b0000$SBAROFF$|#\r\n'
RIP_HIDE_CURSOR = b'\x01|1\x1b0000$COFF$|#\r\n'


def telnet_cmd(*args):
	"""Build a telnet command sequence."""
	return bytes([IAC] + list(args))


def send_negotiation(sock):
	"""Send initial telnet negotiation."""
	sock.sendall(
		telnet_cmd(WILL, OPT_ECHO) +
		telnet_cmd(WILL, OPT_SGA) +
		telnet_cmd(DO, OPT_SGA) +
		telnet_cmd(WILL, OPT_BINARY) +
		telnet_cmd(DO, OPT_BINARY)
	)


def drain_negotiation(sock, timeout=0.5):
	"""Drain any pending telnet negotiation responses from the client."""
	sock.setblocking(False)
	deadline = time.monotonic() + timeout
	while time.monotonic() < deadline:
		try:
			data = sock.recv(256)
			if not data:
				break
		except BlockingIOError:
			# No data available — wait a bit and try once more
			remaining = deadline - time.monotonic()
			if remaining > 0:
				time.sleep(min(0.05, remaining))
				continue
			break
		except (ConnectionResetError, BrokenPipeError, OSError):
			break
	sock.setblocking(True)


def recv_line(sock):
	"""Read a line from the telnet client, stripping IAC sequences and handling CR."""
	buf = b''
	state = 'data'  # data, iac, iac_opt, sb, sb_iac
	while True:
		try:
			chunk = sock.recv(1)
		except (ConnectionResetError, BrokenPipeError, OSError):
			return None
		if not chunk:
			return None

		b = chunk[0]

		if state == 'iac':
			if b == IAC:
				# Escaped 0xFF
				buf += bytes([IAC])
				state = 'data'
			elif b in (WILL, WONT, DO, DONT):
				state = 'iac_opt'
			elif b == SB:
				state = 'sb'
			else:
				state = 'data'
		elif state == 'iac_opt':
			# Skip the option byte
			state = 'data'
		elif state == 'sb':
			if b == IAC:
				state = 'sb_iac'
		elif state == 'sb_iac':
			if b == SE:
				state = 'data'
			else:
				state = 'sb'
		else:  # data
			if b == IAC:
				state = 'iac'
			elif b == 0x0D:
				# CR — end of line (next byte is NUL or LF, consumed later)
				sock.sendall(b'\r\n')
				break
			elif b == 0x0A:
				# LF — end of line
				sock.sendall(b'\r\n')
				break
			elif b == 0x00:
				# NUL after CR — skip
				continue
			elif b == 0x08 or b == 0x7F:
				# Backspace / DEL
				if buf:
					buf = buf[:-1]
					sock.sendall(b'\x08 \x08')
			elif 0x20 <= b < 0x7F:
				buf += bytes([b])
				sock.sendall(bytes([b]))

	return buf.decode('ascii', errors='replace').strip()


def send_text(sock, text):
	"""Send text to the client."""
	sock.sendall(text.encode('ascii', errors='replace') if isinstance(text, str) else text)


def prompt(sock, msg):
	"""Send a prompt and read a line response."""
	send_text(sock, msg)
	return recv_line(sock)


# Sync queries: RIP_QUERY that sends back "S\r".
# Two forms depending on injection context:
#
#   RIP_SYNC_PIPE — used inside a RIP line at a | command boundary.
#     Inserted BEFORE the | in the data.  The pre-sync chunk ends in
#     the previous command's args (parser in ARGS state).  The leading |
#     of the sync dispatches that previous command.  The query runs.
#     The sync ends with |# which becomes the next command in the buffer.
#     Then the data resumes with its original |, which dispatches the
#     |# (NO_MORE) and starts the next command normally.
#     Sequence: [data...args] [|1<esc>0000S^m|#] [|nextcmd...]
#
#   RIP_SYNC_SOH — used at BOL or in non-RIP text.
#     SOH starts a new RIP sequence mid-line.  Ends with \r\n to return
#     the parser to BOL for the next line.
#
RIP_SYNC_PIPE = b'|1\x1b0000S^m|#'
RIP_SYNC_SOH  = b'\x01|1\x1b0000S^m|#\r\n'
RIP_SYNC = RIP_SYNC_SOH  # legacy alias
RIP_SYNC_THRESHOLD = 2048


def _is_continuation(line):
	"""Check if a line ends with an odd number of backslashes (continuation)."""
	if not line.endswith(b'\\'):
		return False
	count = 0
	i = len(line) - 1
	while i >= 0 and line[i] == 0x5c:
		count += 1
		i -= 1
	return count % 2 == 1


def _wait_for_sync(sock, timeout=60.0):
	"""Wait for the 'S\\r' response from a sync query.
	Uses a state machine: wait for S, then \\r.
	Any other bytes are silently discarded."""
	state = 0  # 0 = waiting for S, 1 = waiting for \r
	deadline = time.monotonic() + timeout
	while time.monotonic() < deadline:
		remaining = deadline - time.monotonic()
		if remaining <= 0:
			break
		sock.settimeout(remaining)
		try:
			b = sock.recv(1)
		except socket.timeout:
			break
		except (ConnectionResetError, BrokenPipeError, OSError):
			break
		if not b:
			break
		deadline = time.monotonic() + timeout
		if state == 0 and b == b'S':
			state = 1
		elif state == 1:
			if b == b'\r':
				break  # got S\r — done
			else:
				state = 0  # wasn't S\r, start over
	sock.settimeout(None)


def _find_sync_points(data):
	"""Parse a RIP data stream and return a list of (offset, context) tuples
	identifying safe sync injection points.

	Each tuple is (byte_offset, context) where context is:
	  'pipe'  — inside a RIP line at a | command boundary; use RIP_SYNC_PIPE
	  'bol'   — at beginning of line or non-RIP text; use RIP_SYNC_SOH

	The parser tracks enough state to avoid injecting inside:
	  - backslash continuations (\\<cr>[<lf>] or \\<lf>)
	  - variable-length command arguments (|l, |P, |p)
	  - backslash-escaped pipes (\\|)
	  - ANSI escape sequences in non-RIP text (see conio/cterm.adoc):
	      * CSI (ESC [ ... {@ to ~})
	      * Fe/Fs/Fp 2-byte escapes (ESC {char})
	      * String commands: DCS (ESC P), OSC (ESC ]), PM (ESC ^),
	        APC (ESC _), SOS (ESC X) — terminated by ST (ESC \\)

	Safe points are:
	  - At a | that separates two commands (not \\|, not inside continuation)
	  - At a \\r or \\n line boundary when not inside a continuation
	  - At any point in non-RIP text NOT inside an ESC sequence
	"""
	# States
	S_BOL = 0       # beginning of line, expecting ! or text
	S_TEXT = 1      # non-RIP text (safe to inject with SOH)
	S_BANG = 2      # saw ! at BOL, expecting |
	S_PIPE = 3      # saw |, expecting command char
	S_LEVEL = 4     # saw digit after |, reading level/sublevel
	S_CMD = 5       # reading command character
	S_ARGS = 6      # reading fixed-width arguments
	S_FREEFORM = 7  # reading freeform text argument (last param)
	S_CONT = 8      # inside \ continuation (waiting for next line)
	# ANSI sub-states for S_TEXT
	S_T_ESC = 10    # saw \x1b in text, next byte decides
	S_T_CSI = 11    # inside CSI, waiting for final byte (0x40-0x7E)
	S_T_STR = 12    # inside DCS/OSC/PM/APC/SOS, waiting for ST
	S_T_STR_ESC = 13  # saw ESC inside string, next byte might be '\\'
	S_T_MUSIC = 14  # inside ANSI music string, waiting for 0x0e (SI)

	# Variable-length commands: |P, |p, |l (level 0)
	VARLEN_CMDS = {ord('P'), ord('p'), ord('l')}

	points = []
	state = S_BOL
	i = 0
	n = len(data)
	in_rip_line = False  # are we inside a !|...  RIP line?
	varlen = False        # is current command variable-length?
	csi_has_params = False  # did current CSI sequence contain parameter bytes?

	while i < n:
		ch = data[i]

		if state == S_BOL:
			if ch == ord('!') or ch == 0x01 or ch == 0x02:
				state = S_BANG
				in_rip_line = True
			elif ch == ord('\r'):
				# Empty line
				points.append((i, 'bol'))
				i += 1
				if i < n and data[i] == ord('\n'):
					i += 1
				continue
			elif ch == ord('\n'):
				points.append((i, 'bol'))
				i += 1
				continue
			else:
				state = S_TEXT
				in_rip_line = False
				continue  # re-process this byte in TEXT state

		elif state == S_TEXT:
			# Non-RIP text — safe to inject with SOH sync except
			# while inside an ANSI ESC sequence.
			if ch == 0x1b:
				state = S_T_ESC
			elif ch == ord('\r'):
				points.append((i, 'bol'))
				state = S_BOL
				i += 1
				if i < n and data[i] == ord('\n'):
					i += 1
				continue
			elif ch == ord('\n'):
				points.append((i, 'bol'))
				state = S_BOL
				i += 1
				continue
			else:
				# Safe byte position in text — record sync point.
				points.append((i, 'bol'))

		elif state == S_T_ESC:
			# Saw ESC in text.  Next byte determines sequence type.
			if ch == ord('['):
				state = S_T_CSI       # CSI sequence
				csi_has_params = False
			elif ch in (ord('P'), ord(']'), ord('^'), ord('_'), ord('X')):
				# DCS, OSC, PM, APC, SOS — string command
				state = S_T_STR
			elif 0x20 <= ch <= 0x2F:
				# nF escape intermediate — stay in ESC until final byte
				# (ESC {SPACE to /}... {0 to ~})
				pass  # remain in S_T_ESC-like state, but we need finality
				# Simplest: treat as complete on next 0x30-0x7E
				# For safety, just exit ESC state on this byte.
				state = S_TEXT
			elif 0x30 <= ch <= 0x7E:
				# Fs/Fp/Fe 2-byte escape complete (ESC + this byte)
				state = S_TEXT
			else:
				# Invalid — cterm ignores the ESC, re-process byte
				state = S_TEXT
				continue

		elif state == S_T_CSI:
			# Inside CSI: parameter (0x30-0x3F), intermediate (0x20-0x2F),
			# or final (0x40-0x7E).
			if 0x40 <= ch <= 0x7E:
				# CSI complete on final byte.  If the final byte is
				# M, N, or | AND no parameters were seen, this is an
				# ANSI music introducer and the subsequent bytes form
				# a music string terminated by 0x0e (SI).  We must not
				# inject sync points inside a music string.
				if not csi_has_params and ch in (ord('M'), ord('N'), ord('|')):
					state = S_T_MUSIC
				else:
					state = S_TEXT
			elif 0x30 <= ch <= 0x3F:
				csi_has_params = True
			elif ch == ord('\r') or ch == ord('\n'):
				# Malformed CSI — cterm aborts.  Return to BOL.
				state = S_BOL
				i += 1
				if ch == ord('\r') and i < n and data[i] == ord('\n'):
					i += 1
				continue
			# else: intermediate byte, stay in CSI

		elif state == S_T_MUSIC:
			# Inside an ANSI music string.  Do not inject sync points
			# here.  Only the SI (0x0e) byte terminates the string.
			if ch == 0x0e:
				state = S_TEXT

		elif state == S_T_STR:
			# Inside DCS/OSC/PM/APC/SOS — accumulate until ST (ESC \).
			# A lone ESC starts a potential ST.
			if ch == 0x1b:
				state = S_T_STR_ESC

		elif state == S_T_STR_ESC:
			if ch == ord('\\'):
				state = S_TEXT  # ST completes string
			else:
				state = S_T_STR  # lone ESC inside string, keep going

		elif state == S_BANG:
			if ch == ord('|'):
				state = S_PIPE
			else:
				# ! not followed by | — not a RIP line
				state = S_TEXT
				in_rip_line = False
				continue  # re-process

		elif state == S_PIPE:
			# At a | boundary — this is a safe injection point.
			# Record the sync point AT the | so the pre-sync chunk
			# excludes it.  The sync query starts with | (which dispatches
			# the previous command) and ends with |# (the NO_MORE that
			# the data's original | will then dispatch).
			# Only record on the | itself, not on subsequent chars in PIPE.
			if in_rip_line and ch == ord('|'):
				points.append((i, 'pipe'))
			varlen = False
			if ch == ord('|'):
				# || — dispatch empty, stay in pipe
				pass
			elif ch == ord('\r'):
				state = S_BOL
				i += 1
				if i < n and data[i] == ord('\n'):
					i += 1
				in_rip_line = False
				continue
			elif ch == ord('\n'):
				state = S_BOL
				in_rip_line = False
				i += 1
				continue
			elif ord('1') <= ch <= ord('9'):
				state = S_LEVEL
			else:
				# Level-0 command character
				if ch in VARLEN_CMDS:
					varlen = True
				state = S_ARGS

		elif state == S_LEVEL:
			if ord('1') <= ch <= ord('9'):
				pass  # sublevel digit, stay in LEVEL
			elif ch == ord('|'):
				state = S_PIPE
				continue  # re-process | as pipe
			elif ch == ord('\r') or ch == ord('\n'):
				state = S_BOL
				in_rip_line = False
				if ch == ord('\r'):
					i += 1
					if i < n and data[i] == ord('\n'):
						i += 1
				else:
					i += 1
				continue
			else:
				# Command character after level digits
				state = S_ARGS

		elif state == S_ARGS:
			if ch == ord('|'):
				state = S_PIPE
				continue  # re-process | as pipe boundary
			elif ch == ord('\\') and i + 1 < n:
				nc = data[i + 1]
				if nc == ord('\r'):
					# \<cr>[<lf>] continuation
					state = S_CONT
					i += 2
					if i < n and data[i] == ord('\n'):
						i += 1
					continue
				elif nc == ord('\n'):
					# \<lf> continuation
					state = S_CONT
					i += 2
					continue
				elif not varlen:
					# Escaped character in fixed args — skip both
					i += 2
					continue
				# In varlen args, \ is just data (no escaping)
			elif ch == ord('\r'):
				# CR terminates command.  The `\<cr>` continuation
				# case was consumed above, so this `\r` definitively
				# ends the command — safe to inject a sync here.
				# A pipe-context sync (`|1...|#`) inserted at this
				# position terminates the current command cleanly
				# via the leading `|`, runs the query, and ends with
				# `|#` (NO_MORE) before the original `\r\n` takes us
				# back to BOL.
				points.append((i, 'pipe'))
				state = S_BOL
				in_rip_line = False
				i += 1
				if i < n and data[i] == ord('\n'):
					i += 1
				continue
			elif ch == ord('\n'):
				if varlen:
					pass  # \n is data in varlen commands
				else:
					# Bare \n terminates fixed-arg command — safe
					# sync point (see `\r` case above).
					points.append((i, 'pipe'))
					state = S_BOL
					in_rip_line = False
					i += 1
					continue
			# else: argument data byte, stay in ARGS

		elif state == S_CONT:
			# After \<cr><lf> or \<lf> — continuation line.
			# Keep accumulating in ARGS until we see the next
			# command boundary.
			state = S_ARGS
			continue  # re-process this byte in ARGS state

		i += 1

	return points


def send_rip_data(sock, data, threshold=RIP_SYNC_THRESHOLD):
	"""Send RIP file data with inline flow control.

	Parses the RIP command stream to find safe sync injection points,
	then sends data in chunks with sync queries at those points to
	prevent RIPterm's input buffer from overflowing.

	Safe injection points are:
	  - At | command boundaries inside RIP lines (use RIP_SYNC_PIPE)
	  - At line boundaries or in non-RIP text (use RIP_SYNC_SOH)

	Sync is never injected inside:
	  - Backslash continuations
	  - Variable-length command arguments (|l, |P, |p)
	  - Escaped \\| sequences
	"""
	sync_points = _find_sync_points(data)
	if not sync_points:
		sock.sendall(data)
		return

	last_sent = 0     # byte position of last data actually sent
	last_sync = 0     # byte position of last sync injection (or 0 at start)

	for offset, context in sync_points:
		if offset - last_sync >= threshold:
			# Send data up to this point
			sock.sendall(data[last_sent:offset])
			last_sent = offset
			# Insert appropriate sync query
			if context == 'pipe':
				sock.sendall(RIP_SYNC_PIPE)
			else:
				sock.sendall(RIP_SYNC_SOH)
			_wait_for_sync(sock)
			last_sync = offset

	# Send remaining data
	if last_sent < len(data):
		sock.sendall(data[last_sent:])


def strip_sauce(data):
	"""Strip SAUCE record (and optional comment block) from file data.

	SAUCE format: last 128 bytes are the record, starting with b'SAUCE'.
	If comments > 0, a comment block of (comments * 64 + 5) bytes precedes
	the SAUCE record, starting with b'COMNT'.
	A SUB (0x1A) EOF marker may precede the comment block or SAUCE record.
	"""
	if len(data) < 128:
		return data
	sauce = data[-128:]
	if sauce[:5] != b'SAUCE' or sauce[5:7] != b'00':
		return data
	comments = sauce[104]
	cut = 128
	if comments > 0:
		cblock_size = comments * 64 + 5
		cut += cblock_size
		if len(data) >= cut and data[-(cut):][:5] == b'COMNT':
			pass  # valid comment block
		else:
			cut = 128  # no valid comment block, strip SAUCE only
	result = data[:-cut]
	# Strip trailing SUB (0x1A) EOF marker
	if result and result[-1:] == b'\x1a':
		result = result[:-1]
	return result


def xwd_capture(window_id):
	"""Capture a window via xwd, return raw bytes or None on failure."""
	try:
		result = subprocess.run(
			['xwd', '-silent', '-id', str(window_id)],
			capture_output=True, timeout=10
		)
		if result.returncode == 0 and len(result.stdout) > 100:
			return result.stdout
		return None
	except (subprocess.TimeoutExpired, FileNotFoundError):
		return None


def xwd_dimensions(data):
	"""Extract width and height from XWD data."""
	if len(data) < 24:
		return None, None
	fields = struct.unpack_from('>6I', data, 0)
	return fields[4], fields[5]  # width, height


def wait_for_render(sock, window_id, timeout=60.0):
	"""Wait for rendering to complete using RIP_QUERY, then capture.

	Sends a RIP_QUERY that makes the terminal respond with a known string.
	The terminal must finish processing all prior commands before responding,
	so the response acts as a sync point.
	"""
	# RIP_QUERY mode 0 (immediate): send back "Z\r"
	# Format: !|1<ESC>0000<text>  where ^m = carriage return
	sock.sendall(b'\x01|1\x1b0000Z^m|#\r\n')

	# Wait for the "Z\r" response using a simple state machine.
	# Any received data resets the timeout since it proves RIPterm
	# is still alive and processing.
	state = 0  # 0 = waiting for Z, 1 = waiting for \r
	deadline = time.monotonic() + timeout
	while time.monotonic() < deadline:
		remaining = deadline - time.monotonic()
		if remaining <= 0:
			break
		sock.settimeout(remaining)
		try:
			b = sock.recv(1)
		except socket.timeout:
			break
		except (ConnectionResetError, BrokenPipeError, OSError):
			break
		if not b:
			break
		deadline = time.monotonic() + timeout
		if state == 0 and b == b'Z':
			state = 1
		elif state == 1:
			if b == b'\r':
				break  # got Z\r — done
			else:
				state = 0  # wasn't Z\r, start over
	sock.settimeout(None)

	# Now capture
	return xwd_capture(window_id)


def compare_captures(syncterm_path, ripterm_path, output_dir, basename, logfile):
	"""Compare a SyncTERM capture against a RIPterm capture."""
	try:
		img_s = ripdiff.load_xwd(syncterm_path)
		img_r = ripdiff.load_xwd(ripterm_path)
	except Exception as e:
		logfile.write(f"{basename}: ERROR loading captures: {e}\n")
		logfile.flush()
		return

	if img_s is None or img_r is None:
		logfile.write(f"{basename}: ERROR: could not parse XWD\n")
		logfile.flush()
		return

	ws, hs = img_s.size
	wr, hr = img_r.size

	if (ws, hs) != (wr, hr):
		logfile.write(f"{basename}: SKIP: dimension mismatch {ws}x{hs} vs {wr}x{hr}\n")
		logfile.flush()
		return

	# Snap both to EGA palette
	img_s = ripdiff.snap_image(img_s.copy())
	img_r = ripdiff.snap_image(img_r.copy())

	# Count diffs
	mismatches = ripdiff.count_diffs(img_s, img_r)
	total = ws * hs
	matching = total - mismatches

	if mismatches == 0:
		logfile.write(f"{basename}: MATCH ({total} pixels)\n")
	else:
		pct = matching / total * 100
		logfile.write(f"{basename}: DIFFER {mismatches}/{total} pixels ({pct:.2f}% match)\n")
	logfile.flush()


def gen_pixel_stress(min_bytes):
	"""Generate repeated RIP_PIXEL commands wrapped to 76-char continued lines.

	Each pixel command is |X0000 (6 bytes) drawing pixel at (0,0).
	Lines are wrapped at 76 chars with backslash continuation.
	Returns a complete RIP sequence starting with !| and ending with |#\\r\\n.
	"""
	# |X0000 = 6 bytes per pixel command
	cmd = b'|X0000'
	# First line starts with !
	# Subsequent continuation lines have no prefix
	# Each line is max 76 chars including the trailing backslash
	# So usable chars per line: 75 (76th is the backslash)
	# First line: "!" + commands + "\" = 76 chars, so 75 usable, "!" takes 1, leaves 74
	# Continuation lines: commands + "\" = 76 chars, so 75 usable
	result = bytearray(b'!')
	col = 1
	total = 1

	while total < min_bytes:
		if col + len(cmd) > 75:
			# Wrap: add backslash + CRLF
			result += b'\\\r\n'
			total += 3
			col = 0
		result += cmd
		col += len(cmd)
		total += len(cmd)

	# End with |# on current line or new line
	tail = b'|#\r\n'
	if col + len(tail) - 2 > 75:  # -2 because \r\n don't count for column
		result += b'\\\r\n'
	result += tail
	return bytes(result)


def gen_polygon_stress(min_bytes):
	"""Generate a RIP_POLYGON with many vertices wrapped to 76-char lines.

	The polygon alternates between two points (00,00) and (01,01),
	so it draws the same line segment over and over.
	Format: |P<npoints:2><x:2 y:2>...
	Each vertex is 4 bytes (x:2 y:2). npoints is 2 MegaNum digits.
	Lines wrapped at 76 chars with backslash continuation.
	Returns a complete RIP sequence starting with !| and ending with |#\\r\\n.
	"""
	# Calculate how many vertices we need to reach min_bytes
	# Header: !|P<nn> = 5 bytes, each vertex = 4 bytes
	# Plus line wrapping overhead
	overhead_per_line = 3  # \\r\n
	usable_per_line = 75
	# Rough estimate: each vertex is 4 bytes, ~18 per 75-char line
	npoints = max(2, (min_bytes - 10) // 4)
	if npoints > 512:
		npoints = 512  # spec max

	# Encode npoints as 2-digit MegaNum
	d1 = npoints // 36
	d0 = npoints % 36
	def mega_digit(n):
		if n < 10:
			return bytes([0x30 + n])
		return bytes([0x41 + n - 10])

	result = bytearray(b'!|P')
	result += mega_digit(d1) + mega_digit(d0)
	col = len(result)

	# Alternate between 0000 and 0101
	for i in range(npoints):
		vertex = b'0000' if i % 2 == 0 else b'0101'
		if col + len(vertex) > 75:
			result += b'\\\r\n'
			col = 0
		result += vertex
		col += len(vertex)

	# End with |#
	tail = b'|#\r\n'
	if col + len(tail) - 2 > 75:
		result += b'\\\r\n'
	result += tail
	return bytes(result)


def test_rip_commands(sock, rip_dir='.'):
	"""Interactive test mode for trying RIP commands."""
	commands = [
		("RIP Reset (!|*)", b'!|*'),
		("SBAROFF via query", b'!|1\x1b0000$SBAROFF$'),
		("SBARON via query", b'!|1\x1b0000$SBARON$'),
		("Cursor OFF via query", b'!|1\x1b0000$COFF$'),
		("Cursor ON via query", b'!|1\x1b0000$CON$'),
		("Cursor OFF via SOH", b'\x01|1\x1b0000$COFF$'),
		("Cursor ON via SOH", b'\x01|1\x1b0000$CON$'),
		("Disable text window (!|w0000000000)", b'!|w0000000000'),
		("DTW via query", b'!|1\x1b0000$DTW$'),
		("ANSI cursor hide (CSI ?25l)", b'\x1b[?25l'),
		("ANSI cursor show (CSI ?25h)", b'\x1b[?25h'),
		("SBAROFF via SOH+NO_MORE", b'\x01|1\x1b0000$SBAROFF$|#\r\n'),
		("SBAROFF+DTW combo", b'\x01|1\x1b0000$SBAROFF$|1\x1b0000$DTW$|#\r\n'),
		("Pixel stress test (continued lines)", 'pixel_stress'),
		("Polygon stress test (many vertices)", 'polygon_stress'),
		("File bisect (send N lines of a file)", 'file_bisect'),
	]

	send_text(sock, "\r\nRIP Command Test Mode\r\n")
	send_text(sock, "=====================\r\n")
	while True:
		send_text(sock, "\r\nCommands:\r\n")
		for i, (desc, _) in enumerate(commands):
			send_text(sock, f"  {i:2d}. {desc}\r\n")
		send_text(sock, "   q. Quit\r\n")
		resp = prompt(sock, "\r\nChoice: ")
		if resp is None or resp.lower() == 'q':
			return
		try:
			idx = int(resp.strip())
			if 0 <= idx < len(commands):
				desc, seq = commands[idx]
				if seq == 'pixel_stress':
					size_str = prompt(sock, "Minimum bytes: ")
					if size_str is None:
						return
					try:
						min_bytes = int(size_str.strip())
					except ValueError:
						send_text(sock, "Invalid number.\r\n")
						continue
					seq = gen_pixel_stress(min_bytes)
					send_text(sock, f"\r\nGenerating {len(seq)} bytes of pixel commands...\r\n")
					send_text(sock, b'\n' + seq)
					send_text(sock, f"\r\nSent: {len(seq)} bytes ({desc})\r\n")
				elif seq == 'polygon_stress':
					size_str = prompt(sock, "Minimum bytes (max ~2054 for 512 vertices): ")
					if size_str is None:
						return
					try:
						min_bytes = int(size_str.strip())
					except ValueError:
						send_text(sock, "Invalid number.\r\n")
						continue
					seq = gen_polygon_stress(min_bytes)
					send_text(sock, f"\r\nGenerating {len(seq)} bytes polygon...\r\n")
					send_text(sock, b'\n' + seq)
					send_text(sock, f"\r\nSent: {len(seq)} bytes ({desc})\r\n")
				elif seq == 'file_bisect':
					if not hasattr(test_rip_commands, '_last_bisect_file'):
						test_rip_commands._last_bisect_file = ''
					default = test_rip_commands._last_bisect_file
					p = f"Filename [{default}]: " if default else "Filename (in rip dir): "
					fname = prompt(sock, p)
					if fname is None:
						return
					fname = fname.strip()
					if not fname and default:
						fname = default
					test_rip_commands._last_bisect_file = fname
					fpath = os.path.join(rip_dir, fname)
					if not os.path.exists(fpath):
						send_text(sock, f"File not found: {fname}\r\n")
						continue
					fdata = strip_sauce(open(fpath, 'rb').read())
					flines = fdata.split(b'\n')
					# Strip trailing empty lines
					while flines and not flines[-1].strip(b'\r'):
						flines.pop()
					send_text(sock, f"File has {len(flines)} lines.\r\n")
					nlines_str = prompt(sock, "Lines to send: ")
					if nlines_str is None:
						return
					try:
						nlines = int(nlines_str.strip())
					except ValueError:
						send_text(sock, "Invalid number.\r\n")
						continue
					nlines = max(0, min(nlines, len(flines)))
					# Extend past any trailing continuation lines
					while nlines < len(flines):
						line = flines[nlines - 1]
						if line.endswith(b'\r'):
							line = line[:-1]
						# Check for odd number of trailing backslashes
						count = 0
						j = len(line) - 1
						while j >= 0 and line[j] == 0x5c:
							count += 1
							j -= 1
						if count % 2 == 1:
							nlines += 1
						else:
							break
					# Send a reset first
					send_text(sock, b'\n!|*\n')
					# Send the requested lines with flow control
					out = b'\n'.join(flines[:nlines]) + b'\n'
					send_rip_data(sock, out)
					# Terminate cleanly
					send_text(sock, b'!|#\r\n')
					total = len(out)
					send_text(sock, f"\r\nSent: {nlines} lines, {total} bytes\r\n")
				else:
					send_text(sock, b'\n' + seq + b'\n')
					send_text(sock, f"\r\nSent: {desc}\r\n")
			else:
				send_text(sock, "Invalid choice.\r\n")
		except ValueError:
			send_text(sock, "Invalid choice.\r\n")


def handle_connection(sock, addr, rip_dir, output_dir):
	"""Handle a single telnet connection."""
	print(f"Connection from {addr[0]}:{addr[1]}")

	send_negotiation(sock)
	# Drain client negotiation responses before prompting
	drain_negotiation(sock, timeout=0.5)

	# Ask terminal type
	resp = prompt(sock, "SyncTERM, RIPterm, or Test? [S/R/T]: ")
	if resp is None:
		print("  Client disconnected during terminal type prompt")
		return
	resp = resp.upper().strip()
	if resp.startswith('S'):
		terminal = 'syncterm'
	elif resp.startswith('R'):
		terminal = 'ripterm'
	elif resp.startswith('T'):
		test_rip_commands(sock, rip_dir)
		return
	else:
		send_text(sock, f"Unknown terminal type '{resp}'. Disconnecting.\r\n")
		return
	print(f"  Terminal: {terminal}")

	# Ask window ID
	resp = prompt(sock, "Window ID (hex, e.g. 0x431e083): ")
	if resp is None:
		print("  Client disconnected during window ID prompt")
		return
	try:
		window_id = int(resp.strip(), 16)
	except ValueError:
		send_text(sock, f"Invalid window ID '{resp}'. Disconnecting.\r\n")
		return
	print(f"  Window ID: 0x{window_id:x}")

	# Validate window capture
	send_text(sock, "Validating window capture...\r\n")
	test_cap = xwd_capture(window_id)
	if test_cap is None:
		send_text(sock, "ERROR: Cannot capture window. Check window ID and xwd.\r\n")
		return

	# Check dimensions
	w, h = xwd_dimensions(test_cap)
	print(f"  Capture dimensions: {w}x{h}")
	if h == 480:
		send_text(sock, "ERROR: Capture is 640x480. SyncTERM must be built without scaling (640x350).\r\n")
		send_text(sock, "Press Enter to disconnect.\r\n")
		recv_line(sock)
		return

	send_text(sock, f"OK - {w}x{h} capture working.\r\n")

	# Find .rip files
	rip_files = sorted(glob.glob(os.path.join(rip_dir, '*.rip')))
	if not rip_files:
		send_text(sock, f"No .rip files found in {rip_dir}\r\n")
		return

	send_text(sock, f"Found {len(rip_files)} RIP files. Starting captures...\r\n")
	time.sleep(0.5)

	os.makedirs(output_dir, exist_ok=True)
	log_path = os.path.join(output_dir, 'comparison.log')
	logfile = open(log_path, 'a')
	logfile.write(f"\n--- {terminal} session {time.strftime('%Y-%m-%d %H:%M:%S')} ---\n")
	logfile.flush()

	try:
		for i, rip_path in enumerate(rip_files):
			# Use raw bytes for the filename to avoid surrogate issues with CP437
			rip_path_b = os.fsencode(rip_path)
			basename_b = os.path.splitext(os.path.basename(rip_path_b))[0]
			# Safe printable version for console/telnet (replace non-ASCII)
			basename_safe = basename_b.decode('ascii', errors='replace')

			# Skip if capture already exists
			out_path_b = os.path.join(os.fsencode(output_dir), basename_b + b'_' + terminal.encode() + b'.xwd')
			if os.path.exists(out_path_b):
				print(f"  [{i+1}/{len(rip_files)}] {basename_safe} (skipped, capture exists)")
				continue

			print(f"  [{i+1}/{len(rip_files)}] {basename_safe}")
			send_text(sock, f"\r\n[{i+1}/{len(rip_files)}] {basename_safe}\r\n")

			# Send RIP preamble: reset + disable status bar + hide cursor
			send_text(sock, RIP_RESET + RIP_SBAROFF)

			# Small delay for reset to take effect
			time.sleep(0.3)

			# Send the RIP file contents (strip SAUCE record if present)
			with open(rip_path_b, 'rb') as f:
				rip_data = strip_sauce(f.read())
			send_rip_data(sock, rip_data)

			# If the file left the parser inside a text-tail parameter
			# (e.g. a continuation that swallowed |#), the LF
			# ends the line which terminates the text parameter.
			# The !|# on the new line then gives a clean NO_MORE.
			# In the normal case, the LF is just a blank line and
			# !|# is a redundant NO_MORE — harmless either way.
			send_text(sock, b'\n!|#\r\n')

			# Disable text window to hide cursor (prevents blink from blocking stabilization)
			send_text(sock, RIP_HIDE_CURSOR)

			# Wait for rendering to complete, then capture
			capture = wait_for_render(sock, window_id, timeout=60.0)

			if capture is None:
				print(f"  WARNING: No capture obtained for {basename_safe}")
				logfile.write(f"{basename_safe}: WARNING: capture failed\n")
				logfile.flush()
				continue

			# Save capture
			with open(out_path_b, 'wb') as f:
				f.write(capture)
			print(f"    Saved: {basename_safe}_{terminal}.xwd")

			# If this is SyncTERM and a RIPterm capture exists, compare
			if terminal == 'syncterm':
				ripterm_path_b = os.path.join(os.fsencode(output_dir), basename_b + b'_ripterm.xwd')
				if os.path.exists(ripterm_path_b):
					compare_captures(
						os.fsdecode(out_path_b),
						os.fsdecode(ripterm_path_b),
						output_dir, basename_safe, logfile
					)

	finally:
		logfile.close()

	send_text(sock, f"\r\nDone! {len(rip_files)} files processed.\r\n")
	send_text(sock, f"Results in: {output_dir}\r\n")
	send_text(sock, f"Log: {log_path}\r\n")
	print(f"  Session complete. Log: {log_path}")


def main():
	parser = argparse.ArgumentParser(
		description='RIP rendering test harness — telnet server')
	parser.add_argument('rip_directory',
		help='Directory containing .rip files to test')
	parser.add_argument('--port', type=int, default=1513,
		help='Listen port (default: 1513)')
	parser.add_argument('--output',
		help='Output directory for captures (default: rip_directory)')
	args = parser.parse_args()

	rip_dir = os.path.abspath(args.rip_directory)
	output_dir = os.path.abspath(args.output) if args.output else os.path.join(rip_dir, 'captures')

	if not os.path.isdir(rip_dir):
		print(f"Error: {rip_dir} is not a directory", file=sys.stderr)
		sys.exit(1)

	rip_count = len(glob.glob(os.path.join(rip_dir, '*.rip')))
	print(f"RIP directory: {rip_dir} ({rip_count} files)")
	print(f"Output: {output_dir}")

	srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	srv.bind(('127.0.0.1', args.port))
	srv.listen(1)
	print(f"Listening on 127.0.0.1:{args.port}")
	print("Connect with SyncTERM or RIPterm to begin testing.")

	try:
		while True:
			conn, addr = srv.accept()
			try:
				handle_connection(conn, addr, rip_dir, output_dir)
			except Exception as e:
				print(f"Error handling connection: {e}")
				import traceback
				traceback.print_exc()
			finally:
				try:
					conn.close()
				except Exception:
					pass
			print("Waiting for next connection...")
	except KeyboardInterrupt:
		print("\nShutting down.")
	finally:
		srv.close()


if __name__ == '__main__':
	main()
