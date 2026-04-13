#!/usr/bin/env python3
"""Persistent RIP rendering comparison harness.

Multi-port telnet server that keeps SyncTERM and RIPterm connected
simultaneously, with a control port for sending data and comparing
captures in real time.

Ports (defaults):
    1514  SyncTERM
    1515  RIPterm
    1516  Control (commands)

Usage:
    python3 rip_harness.py [--syncterm-port 1514] [--ripterm-port 1515] \
        [--control-port 1516] [--output /tmp/rip_harness] [--rip-dir .]
"""

import argparse
import os
import select
import struct
import sys
import threading
import time

# Add script directory and parent for imports
_script_dir = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, _script_dir)
sys.path.insert(0, os.path.join(_script_dir, '..'))

from rip_server import (
	IAC, DONT, DO, WONT, WILL, SB, SE,
	OPT_BINARY, OPT_ECHO, OPT_SGA,
	telnet_cmd, send_negotiation, drain_negotiation,
	recv_line, send_text, prompt,
	RIP_RESET, RIP_SBAROFF, RIP_HIDE_CURSOR,
	RIP_SYNC, RIP_SYNC_THRESHOLD,
	send_rip_data, _wait_for_sync, _is_continuation,
	wait_for_render, strip_sauce,
	xwd_capture, xwd_dimensions,
)

import socket as _socket_mod_for_timeout


# ---------- EGA palette + XWD parsing ----------

EGA_PALETTE = [
	(0, 0, 0),       (0, 0, 170),     (0, 170, 0),     (0, 170, 170),
	(170, 0, 0),     (170, 0, 170),   (170, 85, 0),    (170, 170, 170),
	(85, 85, 85),    (85, 85, 255),   (85, 255, 85),   (85, 255, 255),
	(255, 85, 85),   (255, 85, 255),  (255, 255, 85),  (255, 255, 255),
]

EGA_NAMES = [
	"black", "blue", "green", "cyan",
	"red", "magenta", "brown", "ltgray",
	"dkgray", "ltblue", "ltgreen", "ltcyan",
	"ltred", "ltmagenta", "yellow", "white",
]

# Pre-compute lookup table: (r, g, b) -> EGA index for all palette colors
_EGA_RGB_TO_IDX = {rgb: i for i, rgb in enumerate(EGA_PALETTE)}


def _snap_rgb_to_ega(r, g, b):
	"""Return EGA palette index (0-15) for an RGB pixel."""
	idx = _EGA_RGB_TO_IDX.get((r, g, b))
	if idx is not None:
		return idx
	best = 0
	best_dist = 999999
	for i, (er, eg, eb) in enumerate(EGA_PALETTE):
		d = (r - er)**2 + (g - eg)**2 + (b - eb)**2
		if d < best_dist:
			best_dist = d
			best = i
	return best


def parse_xwd_to_ega(data):
	"""Parse raw XWD bytes into (width, height, ega_indices).

	ega_indices is a bytes object of length width*height, one byte per
	pixel, each valued 0-15 (EGA palette index).  Snapping to the nearest
	EGA color is done during parse — single pass, no intermediate image.
	"""
	if len(data) < 100:
		return None, None, None

	header_size, file_version = struct.unpack_from('>II', data, 0)
	if file_version != 7:
		return None, None, None

	fields = struct.unpack_from('>13I', data, 0)
	pixmap_depth = fields[3]
	width = fields[4]
	height = fields[5]
	byte_order = fields[7]  # 0=LSBFirst, 1=MSBFirst
	bits_per_pixel = fields[11]
	bytes_per_line = fields[12]

	more = struct.unpack_from('>7I', data, 52)
	visual_class = more[0]
	red_mask = more[1]
	green_mask = more[2]
	blue_mask = more[3]
	ncolors = more[6]

	# Colormap
	cmap_offset = header_size
	colormap = []
	for i in range(ncolors):
		off = cmap_offset + i * 12
		pixel, r, g, b = struct.unpack_from('>IHHH', data, off)
		colormap.append((r >> 8, g >> 8, b >> 8))

	pixel_offset = cmap_offset + ncolors * 12
	bpp = bits_per_pixel // 8
	is_pseudocolor = visual_class in (3, 4) and pixmap_depth <= 8
	depth_mask = (1 << pixmap_depth) - 1
	unpack_pixel = struct.Struct('<I').unpack_from if byte_order == 0 else struct.Struct('>I').unpack_from

	buf = bytearray(width * height)
	pos = 0

	for y in range(height):
		row_off = pixel_offset + y * bytes_per_line
		for x in range(width):
			val = unpack_pixel(data, row_off + x * bpp)[0]
			if is_pseudocolor:
				idx = val & depth_mask
				if idx < len(colormap):
					r, g, b = colormap[idx]
				else:
					r, g, b = 0, 0, 0
			else:
				r = (val & red_mask) >> 16
				g = (val & green_mask) >> 8
				b = val & blue_mask
			buf[pos] = _snap_rgb_to_ega(r, g, b)
			pos += 1

	return width, height, bytes(buf)


def ega_label_idx(idx):
	"""Return 'name(index)' string for an EGA index."""
	return f"{EGA_NAMES[idx]}({idx})"


# ---------- Helpers ----------

def _wait_for_sync_char(sock, char, timeout=60.0):
	"""Wait for char + \\r response from a RIP_QUERY."""
	state = 0
	deadline = time.monotonic() + timeout
	while time.monotonic() < deadline:
		remaining = deadline - time.monotonic()
		if remaining <= 0:
			break
		sock.settimeout(remaining)
		try:
			b = sock.recv(1)
		except _socket_mod_for_timeout.timeout:
			break
		except (ConnectionResetError, BrokenPipeError, OSError):
			break
		if not b:
			break
		if state == 0 and b == char:
			state = 1
		elif state == 1:
			if b == b'\r':
				break
			else:
				state = 0
	sock.settimeout(None)


def decode_escapes(s):
	"""Decode \\x, \\r, \\n, \\t, \\\\ escape sequences in a string."""
	result = bytearray()
	i = 0
	while i < len(s):
		if s[i] == '\\' and i + 1 < len(s):
			c = s[i + 1]
			if c == 'x' and i + 3 < len(s):
				try:
					result.append(int(s[i+2:i+4], 16))
					i += 4
					continue
				except ValueError:
					pass
			elif c == 'r':
				result.append(0x0D)
				i += 2
				continue
			elif c == 'n':
				result.append(0x0A)
				i += 2
				continue
			elif c == 't':
				result.append(0x09)
				i += 2
				continue
			elif c == '\\':
				result.append(0x5C)
				i += 2
				continue
		result.append(ord(s[i]) if isinstance(s[i], str) else s[i])
		i += 1
	return bytes(result)


# ---------- Terminal connection ----------

class TerminalConnection:
	"""A persistent connection to a terminal (SyncTERM or RIPterm)."""

	def __init__(self, name):
		self.name = name
		self.sock = None
		self.addr = None
		self.window_id = None
		self.connected = False
		self.lock = threading.Lock()
		self.last_capture_time = None
		self.ega_pixels = None         # bytes, one per pixel (0-15)
		self.dimensions = None         # (width, height)

	def send(self, data):
		"""Send raw data. Returns True on success."""
		with self.lock:
			if not self.connected or self.sock is None:
				return False
			try:
				self.sock.sendall(data)
				return True
			except (ConnectionResetError, BrokenPipeError, OSError):
				self.connected = False
				return False

	def send_rip_file(self, path):
		"""Send a .rip file with SAUCE stripping and flow control."""
		with self.lock:
			if not self.connected or self.sock is None:
				return False
			try:
				with open(path, 'rb') as f:
					data = strip_sauce(f.read())
				send_rip_data(self.sock, data)
				# Clean termination
				self.sock.sendall(b'\n!|#\r\n')
				return True
			except (ConnectionResetError, BrokenPipeError, OSError):
				self.connected = False
				return False
			except FileNotFoundError:
				return False

	def send_rip_lines(self, path, n):
		"""Send first N lines of a .rip file with reset prefix."""
		with self.lock:
			if not self.connected or self.sock is None:
				return False, "not connected"
			try:
				with open(path, 'rb') as f:
					data = strip_sauce(f.read())
			except FileNotFoundError:
				return False, "file not found"

			lines = data.split(b'\n')
			# Strip trailing empty lines
			while lines and not lines[-1].strip(b'\r'):
				lines.pop()

			nlines = max(0, min(n, len(lines)))
			# Extend past continuation lines
			while nlines < len(lines):
				line = lines[nlines - 1]
				if line.endswith(b'\r'):
					line = line[:-1]
				if _is_continuation(line):
					nlines += 1
				else:
					break

			try:
				# Reset first
				self.sock.sendall(b'\n' + RIP_RESET + b'\n')
				time.sleep(0.3)
				# Send lines with flow control
				out = b'\n'.join(lines[:nlines]) + b'\n'
				send_rip_data(self.sock, out)
				self.sock.sendall(b'!|#\r\n')
				return True, f"{nlines} lines, {len(out)} bytes"
			except (ConnectionResetError, BrokenPipeError, OSError):
				self.connected = False
				return False, "connection lost"

	def sync_capture(self):
		"""RIP_QUERY sync + XWD capture. Returns (ok, message)."""
		with self.lock:
			if not self.connected or self.sock is None:
				return False, "not connected"
			try:
				self.sock.sendall(RIP_HIDE_CURSOR)
				self.sock.sendall(b'\x01|1\x1b0000Z^m|#\r\n')
				_wait_for_sync_char(self.sock, b'Z')
				time.sleep(0.01)
				cap = xwd_capture(self.window_id)
			except (ConnectionResetError, BrokenPipeError, OSError):
				self.connected = False
				return False, "connection lost during sync"
			return self._process_capture(cap)

	def raw_capture(self):
		"""XWD capture without sync query."""
		cap = xwd_capture(self.window_id)
		return self._process_capture(cap)

	def _process_capture(self, cap):
		if cap is None:
			return False, "xwd capture failed"
		w, h, pixels = parse_xwd_to_ega(cap)
		if pixels is None:
			return False, "xwd parse failed"
		self.ega_pixels = pixels
		self.dimensions = (w, h)
		self.last_capture_time = time.strftime('%H:%M:%S')
		return True, f"captured {w}x{h}"

	def reset(self):
		"""Send RIP reset + SBAROFF + hide cursor."""
		return self.send(RIP_RESET + RIP_SBAROFF + RIP_HIDE_CURSOR)

	def is_alive(self):
		return self.connected and self.sock is not None

	def disconnect(self):
		with self.lock:
			self.connected = False
			if self.sock:
				try:
					self.sock.close()
				except Exception:
					pass
				self.sock = None


class HarnessState:
	"""Thread-safe shared state for the harness."""

	def __init__(self, capture_dir, rip_dir):
		self.lock = threading.Lock()
		self.terminals = {
			'syncterm': TerminalConnection('syncterm'),
			'ripterm': TerminalConnection('ripterm'),
		}
		self.capture_dir = capture_dir
		self.rip_dir = rip_dir
		os.makedirs(capture_dir, exist_ok=True)

	def get_terminal(self, name):
		return self.terminals.get(name)

	def both_connected(self):
		return (self.terminals['syncterm'].is_alive() and
				self.terminals['ripterm'].is_alive())

	def resolve_path(self, path):
		"""Resolve a file path relative to rip_dir."""
		if os.path.isabs(path):
			return path
		return os.path.join(self.rip_dir, path)


def handle_terminal_connect(sock, addr, name, state):
	"""Handle a terminal connection: negotiate, get window ID, register."""
	term = state.get_terminal(name)
	label = name.upper()
	print(f"[{label}] Connection from {addr[0]}:{addr[1]}")

	send_negotiation(sock)
	drain_negotiation(sock, timeout=0.5)

	# Get window ID
	resp = prompt(sock, f"Window ID (hex, e.g. 0x431e083): ")
	if resp is None:
		print(f"  [{label}] Disconnected during prompt")
		sock.close()
		return
	try:
		window_id = int(resp.strip(), 16)
	except ValueError:
		send_text(sock, f"Invalid window ID '{resp}'. Disconnecting.\r\n")
		sock.close()
		return
	print(f"  [{label}] Window ID: 0x{window_id:x}")

	# Validate capture
	send_text(sock, "Validating window capture...\r\n")
	test_cap = xwd_capture(window_id)
	if test_cap is None:
		send_text(sock, "ERROR: Cannot capture window. Check window ID and xwd.\r\n")
		sock.close()
		return

	w, h = xwd_dimensions(test_cap)
	print(f"  [{label}] Capture dimensions: {w}x{h}")
	if h != 350:
		send_text(sock, f"ERROR: Capture is {w}x{h}. Must be 640x350 (no scaling).\r\n")
		send_text(sock, "SyncTERM must be built without aspect ratio scaling.\r\n")
		sock.close()
		return

	send_text(sock, f"OK - {w}x{h} capture working. Connection held open.\r\n")
	send_text(sock, "You can now use the control port to send commands.\r\n")

	# Close old connection if any
	if term.is_alive():
		print(f"  [{label}] Replacing existing connection")
		term.disconnect()

	# Register
	term.sock = sock
	term.addr = addr
	term.window_id = window_id
	term.connected = True
	term.dimensions = (w, h)
	print(f"  [{label}] Registered and ready")


def compare_captures(state, respond):
	"""Compare last captures from both terminals."""
	st = state.terminals['syncterm']
	rt = state.terminals['ripterm']

	if st.ega_pixels is None:
		respond("ERROR: no syncterm capture")
		return
	if rt.ega_pixels is None:
		respond("ERROR: no ripterm capture")
		return

	ws, hs = st.dimensions
	wr, hr = rt.dimensions
	if (ws, hs) != (wr, hr):
		respond(f"ERROR: dimension mismatch {ws}x{hs} vs {wr}x{hr}")
		return

	total = ws * hs
	if st.ega_pixels == rt.ega_pixels:
		respond(f"comparison: {total} total, {total} match, 0 differ (100.00%)")
	else:
		mismatches = sum(a != b for a, b in zip(st.ega_pixels, rt.ega_pixels))
		matching = total - mismatches
		pct = matching / total * 100
		respond(f"comparison: {total} total, {matching} match, {mismatches} differ ({pct:.2f}%)")


# ---------- Control port ----------

def handle_control(sock, addr, state):
	"""Handle a control port connection — raw line protocol, no telnet."""
	print(f"[CTRL] Connection from {addr[0]}:{addr[1]}")

	# Raw socket I/O — no telnet negotiation
	rfile = sock.makefile('r', encoding='ascii', errors='replace')

	def respond(text):
		try:
			sock.sendall((text + "\n").encode('ascii', errors='replace'))
		except (ConnectionResetError, BrokenPipeError, OSError):
			pass

	respond("RIP Harness ready. Type 'help' for commands.")

	while True:
		try:
			line = rfile.readline()
		except (ConnectionResetError, BrokenPipeError, OSError):
			break
		if not line:
			break

		line = line.strip()
		if not line:
			continue

		parts = line.split(None, 1)
		cmd = parts[0].lower()
		args = parts[1] if len(parts) > 1 else ''

		try:
			if cmd == 'help':
				cmd_help(respond)
			elif cmd == 'status':
				cmd_status(state, respond)
			elif cmd == 'send':
				cmd_send(state, args, respond)
			elif cmd == 'sendfile':
				cmd_sendfile(state, args, respond)
			elif cmd == 'sendlines':
				cmd_sendlines(state, args, respond)
			elif cmd == 'reset':
				cmd_reset(state, args, respond)
			elif cmd == 'capture':
				cmd_capture(state, args, respond)
			elif cmd == 'snap':
				cmd_snap(state, args, respond)
			elif cmd == 'diff':
				cmd_diff(state, respond)
			elif cmd == 'pixels':
				cmd_pixels(state, args, respond)
			elif cmd == 'diffpixels':
				cmd_diffpixels(state, args, respond)
			elif cmd == 'ripdir':
				cmd_ripdir(state, args, respond)
			elif cmd == 'quit' or cmd == 'exit':
				respond("Goodbye.")
				break
			else:
				respond(f"Unknown command: {cmd}")
				respond("OK")
		except Exception as e:
			respond(f"ERROR: {e}")

	print(f"[CTRL] Disconnected {addr[0]}:{addr[1]}")
	try:
		rfile.close()
	except Exception:
		pass
	try:
		sock.close()
	except Exception:
		pass


def _parse_target(args):
	"""Parse optional terminal target from args. Returns (target_name_or_None, remaining_args)."""
	parts = args.split(None, 1)
	if parts and parts[0].lower() in ('syncterm', 'ripterm'):
		return parts[0].lower(), parts[1] if len(parts) > 1 else ''
	return None, args


def _get_targets(state, target_name):
	"""Return list of TerminalConnection objects for a target (or both)."""
	if target_name:
		t = state.get_terminal(target_name)
		return [t] if t else []
	return [state.terminals['syncterm'], state.terminals['ripterm']]


def _parallel_targets(targets, fn):
	"""Run fn(terminal) on each target in parallel, return list of (terminal, result)."""
	if len(targets) <= 1:
		return [(t, fn(t)) for t in targets]
	results = [None] * len(targets)
	def worker(idx, t):
		results[idx] = (t, fn(t))
	threads = []
	for i, t in enumerate(targets):
		th = threading.Thread(target=worker, args=(i, t))
		th.start()
		threads.append(th)
	for th in threads:
		th.join()
	return [r for r in results if r is not None]


# ---------- Commands ----------

def cmd_help(respond):
	respond("Commands:")
	respond("  status                        Show terminal connections")
	respond("  send [target] <data>          Send raw data (\\x \\r \\n \\t \\\\ escapes)")
	respond("  sendfile [target] <path>      Send .rip file with flow control")
	respond("  sendlines [target] <path> <n> Reset + send first N lines")
	respond("  reset [target]                Send RIP reset + SBAROFF + hide cursor")
	respond("  capture [target]              Sync + XWD capture, compare if both")
	respond("  snap [target]                 XWD capture without sync query")
	respond("  diff                          Re-compare last two captures")
	respond("  pixels [x,y] or [x1,y1,x2,y2]  Show EGA colors at coordinates")
	respond("  diffpixels [limit]            List differing pixels (default 500)")
	respond("  ripdir [path]                 Show or set the .rip file base directory")
	respond("  quit                          Close control session")
	respond("  [target] = syncterm | ripterm (omit for both)")
	respond("OK")


def cmd_status(state, respond):
	for name in ('syncterm', 'ripterm'):
		t = state.get_terminal(name)
		if t.is_alive():
			cap_info = f", last_capture={t.last_capture_time}" if t.last_capture_time else ""
			respond(f"{name}: connected, window=0x{t.window_id:x}, {t.dimensions[0]}x{t.dimensions[1]}{cap_info}")
		else:
			respond(f"{name}: disconnected")
	respond("OK")


def cmd_send(state, args, respond):
	target, data_str = _parse_target(args)
	if not data_str:
		respond("ERROR: no data specified")
		return
	data = decode_escapes(data_str)
	targets = _get_targets(state, target)
	sent = []
	for t in targets:
		if t.send(data):
			sent.append(t.name)
		elif not t.is_alive():
			respond(f"{t.name}: not connected")
	if sent:
		respond(f"Sent {len(data)} bytes to {', '.join(sent)}")
	respond("OK")


def cmd_sendfile(state, args, respond):
	target, path_str = _parse_target(args)
	if not path_str:
		respond("ERROR: no path specified")
		return
	path = state.resolve_path(path_str.strip())
	if not os.path.exists(path):
		respond(f"ERROR: file not found: {path_str}")
		return
	targets = _get_targets(state, target)
	def do_send(t):
		if not t.is_alive():
			return "not connected"
		return "sent " + os.path.basename(path) if t.send_rip_file(path) else "send failed"
	for t, msg in _parallel_targets(targets, do_send):
		respond(f"{t.name}: {msg}")
	respond("OK")


def cmd_sendlines(state, args, respond):
	target, rest = _parse_target(args)
	parts = rest.rsplit(None, 1)
	if len(parts) < 2:
		respond("ERROR: usage: sendlines [target] <path> <n>")
		return
	path_str, n_str = parts
	try:
		n = int(n_str)
	except ValueError:
		respond(f"ERROR: invalid line count: {n_str}")
		return
	path = state.resolve_path(path_str.strip())
	if not os.path.exists(path):
		respond(f"ERROR: file not found: {path_str}")
		return
	targets = _get_targets(state, target)
	def do_send(t):
		if not t.is_alive():
			return "not connected"
		ok, msg = t.send_rip_lines(path, n)
		return msg
	for t, msg in _parallel_targets(targets, do_send):
		respond(f"{t.name}: {msg}")
	respond("OK")


def cmd_reset(state, args, respond):
	target = args.strip().lower() if args.strip() else None
	if target and target not in ('syncterm', 'ripterm'):
		respond(f"ERROR: unknown target: {target}")
		return
	targets = _get_targets(state, target)
	def do_reset(t):
		if not t.is_alive():
			return "not connected"
		return "reset sent" if t.reset() else "send failed"
	for t, msg in _parallel_targets(targets, do_reset):
		respond(f"{t.name}: {msg}")
	respond("OK")


def cmd_capture(state, args, respond):
	target = args.strip().lower() if args.strip() else None
	if target and target not in ('syncterm', 'ripterm'):
		respond(f"ERROR: unknown target: {target}")
		return
	targets = _get_targets(state, target)
	def do_cap(t):
		if not t.is_alive():
			return False, "not connected"
		return t.sync_capture()
	for t, (ok, msg) in _parallel_targets(targets, do_cap):
		respond(f"{t.name}: {msg}")
	if target is None and state.terminals['syncterm'].ega_pixels and state.terminals['ripterm'].ega_pixels:
		compare_captures(state, respond)
	respond("OK")


def cmd_snap(state, args, respond):
	target = args.strip().lower() if args.strip() else None
	if target and target not in ('syncterm', 'ripterm'):
		respond(f"ERROR: unknown target: {target}")
		return
	targets = _get_targets(state, target)
	def do_snap(t):
		if not t.is_alive():
			return False, "not connected"
		return t.raw_capture()
	for t, (ok, msg) in _parallel_targets(targets, do_snap):
		respond(f"{t.name}: {msg}")
	if target is None and state.terminals['syncterm'].ega_pixels and state.terminals['ripterm'].ega_pixels:
		compare_captures(state, respond)
	respond("OK")


def cmd_diff(state, respond):
	compare_captures(state, respond)
	respond("OK")


def cmd_pixels(state, args, respond):
	st = state.terminals['syncterm']
	rt = state.terminals['ripterm']
	if st.ega_pixels is None and rt.ega_pixels is None:
		respond("ERROR: no captures available")
		return

	args = args.strip()
	if not args:
		respond("ERROR: usage: pixels x,y  or  pixels x1,y1,x2,y2")
		return

	coords = [int(c) for c in args.replace(' ', '').split(',')]
	if len(coords) == 2:
		x, y = coords
		for name, t in [('syncterm', st), ('ripterm', rt)]:
			if t.ega_pixels:
				w, h = t.dimensions
				if 0 <= x < w and 0 <= y < h:
					idx = t.ega_pixels[y * w + x]
					respond(f"{name}: {ega_label_idx(idx)}")
				else:
					respond(f"{name}: out of bounds")
			else:
				respond(f"{name}: no capture")
	elif len(coords) == 4:
		x1, y1, x2, y2 = coords
		# Clamp to 50x50
		if x2 - x1 > 50:
			x2 = x1 + 50
		if y2 - y1 > 50:
			y2 = y1 + 50
		for y in range(y1, y2 + 1):
			for x in range(x1, x2 + 1):
				parts = [f"({x},{y}):"]
				for name, t in [('syncterm', st), ('ripterm', rt)]:
					if t.ega_pixels:
						w, h = t.dimensions
						if 0 <= x < w and 0 <= y < h:
							idx = t.ega_pixels[y * w + x]
							parts.append(f"{name}={ega_label_idx(idx)}")
				respond(' '.join(parts))
	else:
		respond("ERROR: usage: pixels x,y  or  pixels x1,y1,x2,y2")
		return
	respond("OK")


def cmd_ripdir(state, args, respond):
	args = args.strip()
	if not args:
		respond(f"ripdir: {state.rip_dir}")
	else:
		path = os.path.abspath(args)
		if not os.path.isdir(path):
			respond(f"ERROR: not a directory: {args}")
			return
		state.rip_dir = path
		respond(f"ripdir: {state.rip_dir}")
	respond("OK")


def cmd_diffpixels(state, args, respond):
	st = state.terminals['syncterm']
	rt = state.terminals['ripterm']
	if st.ega_pixels is None or rt.ega_pixels is None:
		respond("ERROR: need captures from both terminals")
		return

	limit = 500
	if args.strip():
		try:
			limit = int(args.strip())
		except ValueError:
			respond("ERROR: invalid limit")
			return

	ws, hs = st.dimensions
	wr, hr = rt.dimensions
	if (ws, hs) != (wr, hr):
		respond(f"ERROR: dimension mismatch {ws}x{hs} vs {wr}x{hr}")
		return

	diffs = []
	for i, (a, b) in enumerate(zip(st.ega_pixels, rt.ega_pixels)):
		if a != b:
			x = i % ws
			y = i // ws
			diffs.append((x, y, a, b))

	total_diffs = len(diffs)
	shown = min(limit, total_diffs)
	respond(f"{total_diffs} differing pixels (showing {'all' if shown >= total_diffs else f'first {shown}'}):")
	for i in range(shown):
		x, y, s, r = diffs[i]
		respond(f"({x},{y}): syncterm={ega_label_idx(s)} ripterm={ega_label_idx(r)}")
	respond("OK")


# ---------- Main ----------

import socket as socket_mod


def main():
	parser = argparse.ArgumentParser(
		description='Persistent RIP rendering comparison harness')
	parser.add_argument('--syncterm-port', type=int, default=1514,
		help='SyncTERM listen port (default: 1514)')
	parser.add_argument('--ripterm-port', type=int, default=1515,
		help='RIPterm listen port (default: 1515)')
	parser.add_argument('--control-port', type=int, default=1516,
		help='Control port (default: 1516)')
	parser.add_argument('--output', default='/tmp/rip_harness',
		help='Capture output directory (default: /tmp/rip_harness)')
	parser.add_argument('--rip-dir', default=_script_dir,
		help='Base directory for .rip files (default: script dir)')
	parser.add_argument('--control-bind', default=None, metavar='ADDR',
		help='Bind address for control port only (e.g. machine hostname for remote access)')
	args = parser.parse_args()

	state = HarnessState(
		capture_dir=os.path.abspath(args.output),
		rip_dir=os.path.abspath(args.rip_dir),
	)

	# Create listening sockets
	# Terminal ports always bind to localhost; control port optionally elsewhere
	control_bind = args.control_bind or '127.0.0.1'
	binds = {
		'syncterm': ('127.0.0.1', args.syncterm_port),
		'ripterm': ('127.0.0.1', args.ripterm_port),
		'control': (control_bind, args.control_port),
	}
	listeners = {}
	for name, (bind_addr, port) in binds.items():
		srv = socket_mod.socket(socket_mod.AF_INET, socket_mod.SOCK_STREAM)
		srv.setsockopt(socket_mod.SOL_SOCKET, socket_mod.SO_REUSEADDR, 1)
		srv.bind((bind_addr, port))
		srv.listen(2)
		srv.setblocking(False)
		listeners[name] = srv
		print(f"Listening: {name} on {bind_addr}:{port}")

	print("\nConnect SyncTERM to port {}, RIPterm to port {}, control to port {}.".format(
		args.syncterm_port, args.ripterm_port, args.control_port))

	try:
		while True:
			readable, _, _ = select.select(list(listeners.values()), [], [], 1.0)
			for srv in readable:
				# Find which listener this is
				for name, listener in listeners.items():
					if srv is listener:
						conn, addr = srv.accept()
						conn.setblocking(True)
						if name in ('syncterm', 'ripterm'):
							t = threading.Thread(
								target=handle_terminal_connect,
								args=(conn, addr, name, state),
								daemon=True,
							)
							t.start()
						else:  # control
							t = threading.Thread(
								target=handle_control,
								args=(conn, addr, state),
								daemon=True,
							)
							t.start()
						break
	except KeyboardInterrupt:
		print("\nShutting down.")
	finally:
		for srv in listeners.values():
			srv.close()


if __name__ == '__main__':
	main()
