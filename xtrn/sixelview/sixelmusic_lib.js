/* sixelmusic_lib.js
 *
 * SixelMusic: a music player for SyncTERM, written entirely in Synchronet JavaScript.  It is part of
 * SixelView: sixelviewlibrary.js starts it when a music file is chosen, and sixelmusic.js runs it alone.
 *
 * How it works.  SyncTERM can play sound files that the BBS sends it (its "audio APCs", see cterm_lib.js):
 * the BBS uploads a file, SyncTERM decodes it (MP3, Ogg Vorbis, Opus, FLAC, WAV, AIFF, AU ...) and plays it, and the BBS
 * can change the volume or stop it at any time.  What SyncTERM cannot do is seek, pause, or report where
 * it is in a song.  So this file reads the file's tags, length and structure (MP3 frames, Ogg pages, WAV
 * chunks) itself and keeps its own clock for the playing position; a song that is stopped starts again from
 * the beginning.
 * Only Synchronet is needed to play.  An outside program (ffmpeg) is used only if the sysop asks for smaller
 * copies of big songs (music_kbps in settings.ini); see music_recode.
 *
 * Needs: a terminal with the audio APCs and libsndfile (SyncTERM).  MP3 needs libsndfile 1.1 or later.
 * Mouse reporting is used when the user has it (see mouse_start in sixelview_lib.js).
 *
 * Tab size is 4.
 */

var MUSIC_TYPES = ["mp3", "ogg", "oga", "opus", "wav", "flac", "aif", "aiff", "aifc", "au"];
/* Biggest file that will be loaded.  The BBS keeps it in memory, and sending it takes a few times that much, so it
 * is limited to 64 MB or an eighth of the JavaScript heap, whichever is less. */
var MUSIC_MAX_BYTES = Math.min(64 * 1024 * 1024, (typeof js !== "undefined" && js.max_bytes > 0) ? Math.floor(js.max_bytes / 8) : 64 * 1024 * 1024);

/* ---------- small binary helpers (data is a binary string: one byte per character) ---------- */

function mu_le16(d, p) { return d.charCodeAt(p) | (d.charCodeAt(p + 1) << 8); }
function mu_le32(d, p) { return (d.charCodeAt(p) | (d.charCodeAt(p + 1) << 8) | (d.charCodeAt(p + 2) << 16)) + d.charCodeAt(p + 3) * 16777216; }
function mu_be32(d, p) { return d.charCodeAt(p) * 16777216 + ((d.charCodeAt(p + 1) << 16) | (d.charCodeAt(p + 2) << 8) | d.charCodeAt(p + 3)); }

/* Text from a tag (a string of UTF-8 bytes) made safe to print on the user's terminal */
function mu_display(s)
{
	var r = null;
	try {
		if (typeof utf8_decode == "function")
			r = utf8_decode(s);
	}
	catch (e) { r = null; }
	if (r === null || r === undefined)
		r = String(s).replace(/[\x80-\xff]/g, "?");
	return String(r).replace(/[\x00-\x1f\x7f]/g, " ").replace(/^\s+|\s+$/g, "");
}

function mu_utf8(cp)
{
	if (cp < 0x80)
		return String.fromCharCode(cp);
	if (cp < 0x800)
		return String.fromCharCode(0xc0 | (cp >> 6), 0x80 | (cp & 63));
	if (cp < 0x10000)
		return String.fromCharCode(0xe0 | (cp >> 12), 0x80 | ((cp >> 6) & 63), 0x80 | (cp & 63));
	return String.fromCharCode(0xf0 | (cp >> 18), 0x80 | ((cp >> 12) & 63), 0x80 | ((cp >> 6) & 63), 0x80 | (cp & 63));
}

/* A string in one of the ID3 text encodings -> UTF-8 bytes */
function mu_id3_text(s)
{
	if (!s.length)
		return "";
	var enc = s.charCodeAt(0), t = s.substr(1), out = "", i;
	if (enc == 1 || enc == 2) {
		var be = enc == 2;
		if (t.length >= 2 && t.charCodeAt(0) == 0xff && t.charCodeAt(1) == 0xfe) {
			be = false;
			t = t.substr(2);
		}
		else if (t.length >= 2 && t.charCodeAt(0) == 0xfe && t.charCodeAt(1) == 0xff) {
			be = true;
			t = t.substr(2);
		}
		for (i = 0; i + 1 < t.length; i += 2) {
			var cu = be ? (t.charCodeAt(i) << 8) | t.charCodeAt(i + 1) : (t.charCodeAt(i + 1) << 8) | t.charCodeAt(i);
			if (cu == 0)
				break;
			if (cu >= 0xd800 && cu < 0xdc00 && i + 3 < t.length) {
				var lo = be ? (t.charCodeAt(i + 2) << 8) | t.charCodeAt(i + 3) : (t.charCodeAt(i + 3) << 8) | t.charCodeAt(i + 2);
				cu = 0x10000 + ((cu - 0xd800) << 10) + (lo - 0xdc00);
				i += 2;
			}
			out += mu_utf8(cu);
		}
		return out;
	}
	var z = t.indexOf("\x00");
	if (z >= 0)
		t = t.substr(0, z);
	if (enc == 3)
		return t;
	for (i = 0; i < t.length; i++)		/* ISO-8859-1 */
		out += mu_utf8(t.charCodeAt(i));
	return out;
}

/* ---------- MP3 ---------- */

var MP3_BITRATES = {
	"1:1": [0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448],
	"1:2": [0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384],
	"1:3": [0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320],
	"2:1": [0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256],
	"2:2": [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160],
	"2:3": [0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160]
};
var MP3_RATES = { 3: [44100, 48000, 32000], 2: [22050, 24000, 16000], 0: [11025, 12000, 8000] };

/* The frame header at p, or null.  ref (optional) is the first frame's header: later ones must match it. */
function mp3_header(d, p, ref)
{
	if (p + 4 > d.length)
		return null;
	var b0 = d.charCodeAt(p), b1 = d.charCodeAt(p + 1), b2 = d.charCodeAt(p + 2), b3 = d.charCodeAt(p + 3);
	if (b0 != 0xff || (b1 & 0xe0) != 0xe0)
		return null;
	var vv = (b1 >> 3) & 3, layer = 4 - ((b1 >> 1) & 3);		/* 1..3 */
	if (vv == 1 || layer == 4)
		return null;
	var bi = (b2 >> 4) & 15, si = (b2 >> 2) & 3, pad = (b2 >> 1) & 1;
	if (bi == 0 || bi == 15 || si == 3)
		return null;
	var table = MP3_BITRATES[(vv == 3 ? "1:" : "2:") + layer];
	var br = table[bi] * 1000, sr = MP3_RATES[vv][si];
	var len, spf;
	if (layer == 1) {
		len = (Math.floor(12 * br / sr) + pad) * 4;
		spf = 384;
	}
	else {
		spf = (layer == 3 && vv != 3) ? 576 : 1152;
		len = Math.floor((spf / 8) * br / sr) + pad;
	}
	if (ref && (ref.vv != vv || ref.layer != layer || ref.sr != sr))
		return null;
	return { vv: vv, layer: layer, br: br, sr: sr, len: len, spf: spf, mono: ((b3 >> 6) & 3) == 3 };
}

function mp3_id3v2(d, tags)
{
	if (d.substr(0, 3) != "ID3" || d.length < 10)
		return 0;
	var ver = d.charCodeAt(3), flags = d.charCodeAt(5);
	var size = (d.charCodeAt(6) & 127) * 2097152 + (d.charCodeAt(7) & 127) * 16384 + (d.charCodeAt(8) & 127) * 128 + (d.charCodeAt(9) & 127);
	var end = 10 + size + ((flags & 0x10) ? 10 : 0);
	var p = 10;
	if (ver >= 3 && (flags & 0x40) && p + 4 <= d.length) {		/* extended header */
		var es = ver == 4 ? ((d.charCodeAt(p) & 127) * 2097152 + (d.charCodeAt(p + 1) & 127) * 16384 + (d.charCodeAt(p + 2) & 127) * 128 + (d.charCodeAt(p + 3) & 127)) : mu_be32(d, p) + 4;
		p += es;
	}
	var stop = Math.min(end, d.length), hl = ver == 2 ? 6 : 10, names = ver == 2 ?
		{ TT2: "title", TP1: "artist", TAL: "album", TYE: "year" } : { TIT2: "title", TPE1: "artist", TALB: "album", TYER: "year", TDRC: "year" };
	while (p + hl <= stop) {
		var id = d.substr(p, ver == 2 ? 3 : 4), fs;
		if (!/^[A-Z0-9]{3,4}$/.test(id))
			break;
		if (ver == 2)
			fs = (d.charCodeAt(p + 3) << 16) | (d.charCodeAt(p + 4) << 8) | d.charCodeAt(p + 5);
		else if (ver == 4)
			fs = (d.charCodeAt(p + 4) & 127) * 2097152 + (d.charCodeAt(p + 5) & 127) * 16384 + (d.charCodeAt(p + 6) & 127) * 128 + (d.charCodeAt(p + 7) & 127);
		else
			fs = mu_be32(d, p + 4);
		if (fs < 0 || p + hl + fs > stop)
			break;
		if (names[id] && !tags[names[id]])
			tags[names[id]] = mu_display(mu_id3_text(d.substr(p + hl, fs)));
		p += hl + fs;
	}
	return end;
}

/* Everything the player needs to know about an MP3 file: tags, length, and where every frame starts */
function mp3_probe(d)
{
	var info = { kind: "mp3", tags: {} };
	var start = mp3_id3v2(d, info.tags);
	var limit = d.length;
	if (d.length >= 128 && d.substr(d.length - 128, 3) == "TAG") {
		var v1 = function (a, l) { return mu_display(mu_id3_text("\x00" + d.substr(d.length - 128 + a, l))); };
		if (!info.tags.title)
			info.tags.title = v1(3, 30);
		if (!info.tags.artist)
			info.tags.artist = v1(33, 30);
		if (!info.tags.album)
			info.tags.album = v1(63, 30);
		limit -= 128;
	}
	var offs = [], p = start, ref = null, h, n = 0;
	/* find the first frame: it must be followed by another one */
	while (p + 4 <= limit && p - start < 262144) {
		h = mp3_header(d, p);
		if (h && p + h.len + 4 <= limit && mp3_header(d, p + h.len, h)) {
			ref = h;
			break;
		}
		p++;
	}
	if (!ref)
		throw "No MP3 audio found in this file";
	while (p + 4 <= limit) {
		h = mp3_header(d, p, ref);
		if (!h || p + h.len > limit) {
			/* damaged or non-audio data: look for the next good frame, or stop */
			var q = p + 1, found = false;
			for (; q + 4 <= limit && q - p < 65536; q++) {
				var h2 = mp3_header(d, q, ref);
				if (h2 && q + h2.len + 4 <= limit && mp3_header(d, q + h2.len, ref)) {
					found = true;
					break;
				}
			}
			if (!found)
				break;
			p = q;
			continue;
		}
		if (n == 0) {
			/* a Xing/Info frame is a header, not sound */
			var chunk = d.substr(p + 4, 40);
			if (/Xing|Info/.test(chunk)) {
				p += h.len;
				n++;
				continue;
			}
		}
		offs.push(p);
		p += h.len;
		n++;
	}
	if (!offs.length)
		throw "No MP3 audio found in this file";
	info.sr = ref.sr;
	info.spf = ref.spf;
	info.channels = ref.mono ? 1 : 2;
	info.duration = offs.length * ref.spf / ref.sr;
	info.audio_bytes = limit - offs[0];
	info.bitrate = Math.round(info.audio_bytes * 8 / info.duration);
	info.label = "MP3";
	return info;
}

/* ---------- WAV ---------- */

function wav_probe(d)
{
	if (d.substr(0, 4) != "RIFF" || d.substr(8, 4) != "WAVE")
		throw "Not a WAV file";
	var p = 12, fmt = null, data = null;
	while (p + 8 <= d.length) {
		var id = d.substr(p, 4), sz = mu_le32(d, p + 4);
		if (id == "fmt ")
			fmt = { off: p, size: sz, tag: mu_le16(d, p + 8), channels: mu_le16(d, p + 10), rate: mu_le32(d, p + 12),
				byterate: mu_le32(d, p + 16), align: mu_le16(d, p + 20), bits: mu_le16(d, p + 22) };
		else if (id == "data") {
			data = { off: p + 8, size: Math.min(sz, d.length - (p + 8)) };
			break;
		}
		p += 8 + sz + (sz & 1);
	}
	if (!fmt || !data || !fmt.byterate || !fmt.align)
		throw "Unusable WAV file";
	return { kind: "wav", tags: {}, fmt: fmt, data: data, sr: fmt.rate, channels: fmt.channels,
		duration: data.size / fmt.byterate, bitrate: fmt.byterate * 8, label: "WAV" };
}

/* ---------- Ogg (Vorbis and Opus) ---------- */

function ogg_comments(body, off, info)
{
	var p = off;
	if (p + 4 > body.length)
		return;
	var vl = mu_le32(body, p);
	p += 4 + vl;
	if (p + 4 > body.length)
		return;
	var count = mu_le32(body, p);
	p += 4;
	for (var i = 0; i < count && p + 4 <= body.length; i++) {
		var l = mu_le32(body, p);
		p += 4;
		if (p + l > body.length)
			break;
		var m = /^([^=]+)=([\s\S]*)$/.exec(body.substr(p, l));
		p += l;
		if (m) {
			var k = m[1].toLowerCase();
			if ((k == "title" || k == "artist" || k == "album") && !info.tags[k])
				info.tags[k] = mu_display(m[2]);
			else if (k == "date" && !info.tags.year)
				info.tags.year = mu_display(m[2]).substr(0, 4);
		}
	}
}

function ogg_probe(d)
{
	var pages = [], p = 0, serial = null;
	while (p + 27 <= d.length && d.substr(p, 4) == "OggS") {
		var nseg = d.charCodeAt(p + 26);
		if (p + 27 + nseg > d.length)
			break;
		var body = 0;
		for (var i = 0; i < nseg; i++)
			body += d.charCodeAt(p + 27 + i);
		var len = 27 + nseg + body;
		if (p + len > d.length)
			break;
		var lo = mu_le32(d, p + 6), hi = mu_le32(d, p + 10);
		var gran = (hi == 4294967295 && lo == 4294967295) ? -1 : hi * 4294967296 + lo;
		var ser = mu_le32(d, p + 14);
		if (serial === null)
			serial = ser;
		else if (ser != serial)
			break;			/* a second stream: only the first is used */
		pages.push({ off: p, len: len, gran: gran, seq: mu_le32(d, p + 18), flags: d.charCodeAt(p + 5), body: p + 27 + nseg });
		p += len;
	}
	if (pages.length < 3)
		throw "Not a usable Ogg file";
	var first = d.substr(pages[0].body, 19);
	var info = { kind: "ogg", tags: {}, serial: serial };
	if (first.substr(0, 7) == "\x01vorbis") {
		info.codec = "vorbis";
		info.channels = first.charCodeAt(11);
		info.sr = mu_le32(first, 12);
		info.bitrate = mu_le32(first, 20);
		info.label = "Ogg Vorbis";
		info.preskip = 0;
		info.grate = info.sr;
	}
	else if (first.substr(0, 8) == "OpusHead") {
		info.codec = "opus";
		info.channels = first.charCodeAt(9);
		info.preskip = mu_le16(first, 10);
		info.sr = 48000;
		info.grate = 48000;
		info.label = "Opus";
	}
	else
		throw "This Ogg file is not Vorbis or Opus";
	var nh = 0;
	while (nh < pages.length && pages[nh].gran <= 0)
		nh++;
	if (nh < 2 || nh >= pages.length)
		throw "Not a usable Ogg file";
	info.nh = nh;
	var cbody = d.substr(pages[1].body, pages[1].off + pages[1].len - pages[1].body);
	if (info.codec == "vorbis" && cbody.substr(0, 7) == "\x03vorbis")
		ogg_comments(cbody, 7, info);
	else if (info.codec == "opus" && cbody.substr(0, 8) == "OpusTags")
		ogg_comments(cbody, 8, info);
	var lastg = 0;
	for (var k = pages.length - 1; k >= nh; k--) {
		if (pages[k].gran > 0) {
			lastg = pages[k].gran;
			break;
		}
	}
	info.duration = Math.max(0, (lastg - info.preskip) / info.grate);
	if (!info.duration)
		throw "Not a usable Ogg file";
	if (!info.bitrate)
		info.bitrate = Math.round((d.length - pages[nh].off) * 8 / info.duration);
	return info;
}

/* ---------- FLAC ---------- */

function flac_probe(d)
{
	var p = 0;
	if (d.substr(0, 3) == "ID3" && d.length > 10)		/* a tag in front of the stream */
		p = 10 + (((d.charCodeAt(6) & 0x7f) << 21) | ((d.charCodeAt(7) & 0x7f) << 14) | ((d.charCodeAt(8) & 0x7f) << 7) | (d.charCodeAt(9) & 0x7f));
	if (d.substr(p, 4) != "fLaC")
		throw "Not a FLAC file";
	var info = { kind: "flac", tags: {}, label: "FLAC" }, si = false;
	p += 4;
	while (p + 4 <= d.length) {
		var h = d.charCodeAt(p), type = h & 0x7f, len = (d.charCodeAt(p + 1) << 16) | (d.charCodeAt(p + 2) << 8) | d.charCodeAt(p + 3);
		var b = p + 4;
		if (b + len > d.length)
			break;
		if (type == 0 && len >= 34) {		/* STREAMINFO */
			info.sr = (d.charCodeAt(b + 10) << 12) | (d.charCodeAt(b + 11) << 4) | (d.charCodeAt(b + 12) >> 4);
			info.channels = ((d.charCodeAt(b + 12) >> 1) & 7) + 1;
			var bits = (((d.charCodeAt(b + 12) & 1) << 4) | (d.charCodeAt(b + 13) >> 4)) + 1;
			var total = (d.charCodeAt(b + 13) & 15) * 4294967296 + mu_be32(d, b + 14);
			info.duration = info.sr ? total / info.sr : 0;
			info.fmt = [23, bits <= 8 ? 1 : bits <= 16 ? 2 : bits <= 24 ? 3 : null];
			si = true;
		}
		else if (type == 4)			/* VORBIS_COMMENT: the same layout as in Ogg */
			ogg_comments(d.substr(b, len), 0, info);
		p = b + len;
		if (h & 0x80)
			break;
	}
	if (!si || !(info.duration > 0))
		throw "Unusable FLAC file (no length in its header)";
	info.bitrate = Math.round((d.length - p) * 8 / info.duration);
	return info;
}

/* ---------- AIFF / AIFF-C ---------- */

function aiff_probe(d)
{
	var form = d.substr(8, 4);
	if (d.substr(0, 4) != "FORM" || (form != "AIFF" && form != "AIFC"))
		throw "Not an AIFF file";
	var info = { kind: "aiff", tags: {}, label: form == "AIFC" ? "AIFF-C" : "AIFF" }, p = 12, comm = false;
	while (p + 8 <= d.length) {
		var id = d.substr(p, 4), sz = mu_be32(d, p + 4), b = p + 8;
		if (id == "COMM" && sz >= 18) {
			info.channels = (d.charCodeAt(b) << 8) | d.charCodeAt(b + 1);
			var frames = mu_be32(d, b + 2), bits = (d.charCodeAt(b + 6) << 8) | d.charCodeAt(b + 7);
			var ex = ((d.charCodeAt(b + 8) & 0x7f) << 8) | d.charCodeAt(b + 9);
			info.sr = Math.round((mu_be32(d, b + 10) * 4294967296 + mu_be32(d, b + 14)) * Math.pow(2, ex - 16383 - 63));
			var sub = bits <= 8 ? 1 : bits <= 16 ? 2 : bits <= 24 ? 3 : bits <= 32 ? 4 : null;
			if (form == "AIFC") {
				var comp = sz >= 22 ? d.substr(b + 18, 4) : "NONE";
				sub = comp == "NONE" ? sub : comp == "fl32" || comp == "FL32" ? 6 : comp == "fl64" || comp == "FL64" ? 7
					: comp == "ulaw" ? 0x10 : comp == "alaw" ? 0x11 : null;		/* others: don't second-guess the terminal */
			}
			info.fmt = [2, sub];
			info.duration = info.sr ? frames / info.sr : 0;
			info.bitrate = info.sr * info.channels * bits;
			comm = true;
		}
		else if ((id == "NAME" || id == "AUTH") && sz < 4096) {
			var t = mu_display(d.substr(b, sz).replace(/\0+$/, ""));
			if (t)
				info.tags[id == "NAME" ? "title" : "artist"] = t;
		}
		p = b + sz + (sz & 1);
	}
	if (!comm || !(info.duration > 0))
		throw "Unusable AIFF file";
	return info;
}

/* ---------- AU (Sun/NeXT) ---------- */

function au_probe(d)
{
	if (d.substr(0, 4) != ".snd" || d.length < 24)
		throw "Not an AU file";
	var off = mu_be32(d, 4), size = mu_be32(d, 8), enc = mu_be32(d, 12), sr = mu_be32(d, 16), ch = mu_be32(d, 20);
	var bytes = { 1: 1, 2: 1, 3: 2, 4: 3, 5: 4, 6: 4, 7: 8, 27: 1 }[enc];
	var subs = { 1: 0x10, 2: 1, 3: 2, 4: 3, 5: 4, 6: 6, 7: 7, 27: 0x11 };
	if (!bytes)
		throw "This AU file's encoding is not supported";
	if (!sr || !ch || off < 24)
		throw "Unusable AU file";
	if (size == 0xffffffff || off + size > d.length)
		size = d.length - off;
	var info = { kind: "au", tags: {}, label: "AU", sr: sr, channels: ch, fmt: [3, subs[enc]],
		duration: size / (sr * ch * bytes), bitrate: sr * ch * bytes * 8 };
	if (!(info.duration > 0))
		throw "Unusable AU file";
	return info;
}

/* ---------- common ---------- */

function mu_ext(name)
{
	var i = name.lastIndexOf(".");
	return i < 0 ? "" : name.substr(i + 1).toLowerCase();
}

/*
 * Optional: make a smaller copy of a song with ffmpeg (an Opus file at kbps), for slow connections.  This is the one
 * place an outside program is used, and only when the sysop sets music_kbps.  The copy is kept in .cache/music/ and
 * made once.  Returns the copy's file name, or "" when the song should be sent as it is (already small enough,
 * no ffmpeg, a file name the shell could misread, or ffmpeg failed).
 */
function music_recode(path, kbps, bitrate, ffmpeg)
{
	if (!(kbps > 0) || bitrate <= kbps * 1100)
		return "";
	if (!/^[A-Za-z0-9 _.,()\[\]+@=~\/:\\-]+$/.test(path))
		return "";
	var dir = BASE_DIR + ".cache/music/";		/* BASE_DIR: see sixelviewlibrary.js */
	var out = dir + md5_calc(path + "|" + file_date(path) + "|" + file_size(path) + "|" + kbps, true) + ".ogg";
	if (file_exists(out) && file_size(out) > 0)
		return out;
	mkpath(dir);
	var tmp = out + "." + (typeof bbs !== "undefined" ? bbs.node_num : 0) + "." + system.timer.toString().replace(/\D/g, "") + ".tmp";
	var ff = file_exists(system.exec_dir + "ffmpeg") ? system.exec_dir + "ffmpeg" : "ffmpeg";
	if (ffmpeg) {		/* the sysop named it (ffmpeg= in the settings) */
		if (!file_exists(ffmpeg) || !/^[A-Za-z0-9 _.,()\[\]+@=~\/:\\-]+$/.test(ffmpeg)) {
			log(LOG_ERR, "sixelmusic: ffmpeg setting is not a usable file: " + ffmpeg);
			return "";
		}
		ff = ffmpeg;
	}
	system.exec('"' + ff + '" -nostdin -hide_banner -loglevel error -y -i "' + path + '" -vn -map_metadata -1 -c:a libopus -b:a ' + kbps
		+ 'k -f ogg "' + tmp + '"');
	if (!file_exists(tmp) || file_size(tmp) < 1024) {
		file_remove(tmp);
		return "";
	}
	if (!file_rename(tmp, out))
		file_remove(tmp);
	return file_exists(out) ? out : "";
}

/* Read and analyze a music file.  Returns the track: { path, name, data, id, info } (or throws a message).
 * With kbps set, a song bigger than that is replaced by a smaller copy (see music_recode). */
function music_load_track(path, name, kbps, ffmpeg)
{
	var f = new File(path);
	if (!f.exists || !f.open("rb"))
		throw "Unable to open the file";
	var size = f.length;
	if (size > MUSIC_MAX_BYTES) {
		f.close();
		throw "The file is too big (limit " + Math.floor(MUSIC_MAX_BYTES / 1048576) + " MB)";
	}
	var d = f.read(size);
	f.close();
	var ext = mu_ext(name), info;
	if (d.substr(0, 4) == "fLaC" || (ext == "flac" && d.substr(0, 3) == "ID3"))
		info = flac_probe(d);
	else if (d.substr(0, 4) == "FORM")
		info = aiff_probe(d);
	else if (d.substr(0, 4) == ".snd")
		info = au_probe(d);
	else if (d.substr(0, 4) == "RIFF")
		info = wav_probe(d);
	else if (d.substr(0, 4) == "OggS")
		info = ogg_probe(d);
	else if (ext == "wav")
		info = wav_probe(d);
	else
		info = mp3_probe(d);
	if (kbps > 0 && info.bitrate) {
		var small = music_recode(path, kbps, info.bitrate, ffmpeg);
		var f2 = small ? new File(small) : null;
		if (f2 && f2.open("rb")) {
			var d2 = f2.read(f2.length);
			f2.close();
			try {
				var i2 = ogg_probe(d2);
				i2.tags = info.tags;
				i2.label = "Opus (from " + info.label + ")";
				d = d2;
				info = i2;
			}
			catch (e) {}
		}
	}
	return { path: path, name: name, data: d, info: info, id: md5_calc(path + ":" + d.length, true).substr(0, 12) };
}

function music_time(sec)
{
	sec = Math.max(0, Math.floor(sec));
	var m = Math.floor(sec / 60), s = sec % 60;
	return m + ":" + (s < 10 ? "0" : "") + s;
}

/* ---------- the player and its screen ---------- */

var MU_BAR_FULL = "\xdb", MU_BAR_EMPTY = "\xb0";

function music_fit(s, n)
{
	s = String(s).replace(/[\x00-\x1f\x7f]/g, " ");
	if (s.length > n)
		s = n > 3 ? s.substr(0, n - 3) + "..." : s.substr(0, n);
	return s;
}

function music_pad(s, n)
{
	while (s.length < n)
		s += " ";
	return s;
}

/*
 * Play playlist[index], then the files after it.  playlist is an array of { name, path }.  Returns when the user
 * quits (or the last file ends).
 */
function music_player(playlist, index, kbps, ffmpeg)
{
	var cterm = load({}, "cterm_lib.js");
	if (!cterm.supports_audio_files()) {
		console.print("\x01n\r\n\x01h\x01rSorry, your terminal can't play sound files from the BBS.\x01n\r\n");
		console.print("(SyncTERM with audio support is required.)\r\n");
		console.pause();
		return;
	}
	mouse_start();
	var clicks_were_on = MOUSE_STARTED && (console.status & CON_MOUSE_CLK_PASSTHRU) != 0;
	mouse_clicks(true);
	var CH = cterm.audio_music_chan;

	var cur = index, tr = null, err = "";
	var playing = false, base = 0, t0 = 0, volume = 70, muted = false, slot = 0;
	var autoplay = false;				/* go on to the next song by itself; off until the user turns it on */
	var note = "", note_until = 0, note_dirty = false;
	var buttons = [], regions = {};
	var cols = console.screen_columns, rows = console.screen_rows;
	var W = Math.min(cols, 80);

	function level() { return muted ? 0 : volume; }

	function position()
	{
		if (!tr)
			return 0;
		var p = playing ? base + (system.timer - t0) : base;
		return Math.max(0, Math.min(tr.info.duration, p));
	}

	function put(x, y, s)
	{
		console.gotoxy(x, y);
		console.print(s);
	}

	function say(text, secs)
	{
		note = text;
		note_until = system.timer + (secs || 3);
		note_dirty = true;
	}

	function stop(fade)
	{
		try { cterm.audio_flush(CH, fade || 0); } catch (e) {}
		playing = false;
	}

	var send_rate = 0;		/* bytes per second the last big song was sent at; 0 = not measured yet */
	var sent = {};			/* the songs SyncTERM already has (cterm_lib sends each one only once per session) */

	/* Play the current track from the beginning.  SyncTERM keeps the file for the rest of the session, so a
	 * song is sent only the first time it is played. */
	function start()
	{
		stop();
		if (!tr)
			return;
		var size = tr.data.length;
		var txt = size >= 1048576 ? (size / 1048576).toFixed(1) + " MB" : Math.max(1, Math.round(size / 1024)) + " KB";
		/* the speed is not known until a song has been sent; after that the last one is used for the estimate */
		var name = "sxm" + tr.id;
		var first = !sent[name];
		if (first)
			status_line(" Sending the song: " + txt + (send_rate ? ", about " + music_time(Math.max(1, Math.round(size / send_rate))) + " ..." : " ..."));
		var sent_at = system.timer;
		cterm.audio_prefetch(name, tr.data);
		slot = (slot + 1) % 200;
		cterm.audio_load(slot, name);
		cterm.audio_queue(CH, slot, { vol: level() });
		/* the terminal answers only once it has taken all that in, so the clock starts then */
		cterm.query_fb("\x1b[=7;" + CH + "n", "n");
		var took = system.timer - sent_at;
		if (first && size >= 100000 && took > 0.2)
			send_rate = size / took;
		sent[name] = true;
		base = 0;
		t0 = system.timer;
		playing = true;
	}

	function open_track(i)
	{
		stop();
		tr = null;
		err = "";
		cur = i;
		draw_all();
		status_line(kbps > 0 ? " Loading (making a smaller copy the first time) ..." : " Loading ...");
		try {
			var e = playlist[cur];
			tr = music_load_track(e.path, e.name, kbps, ffmpeg);
			var fm = tr.info.fmt ? tr.info.fmt : tr.info.kind == "mp3" ? [cterm.audio_major_mpeg, cterm.audio_sub_mp3]
				: tr.info.kind == "wav" ? [cterm.audio_major_wav, cterm.audio_sub_pcm_16]
				: [cterm.audio_major_ogg, tr.info.codec == "opus" ? cterm.audio_sub_opus : cterm.audio_sub_vorbis];
			if (fm[1] !== null && cterm.supports_audio_format(fm[0], fm[1]) === false)
				throw "Your terminal can't play " + tr.info.label + " files";
			base = 0;
			draw_all();
			start();
		}
		catch (ex) {
			tr = null;
			playing = false;
			err = String(ex).replace(/\x01/g, "");
		}
		draw_all();
	}

	function jump(i)
	{
		if (i < 0 || i >= playlist.length)
			return false;
		open_track(i);
		return true;
	}

	/* Stop, or play the song again from the beginning (SyncTERM can't pause) */
	function play_toggle()
	{
		if (!tr)
			return;
		if (playing) {
			stop();
			base = 0;
		}
		else
			start();
	}

	function set_volume(v)
	{
		volume = Math.max(0, Math.min(100, v));
		muted = false;
		if (playing)
			cterm.audio_volume(CH, level());
	}

	function mute_toggle()
	{
		muted = !muted;
		if (playing)
			cterm.audio_volume(CH, level());
	}

	/* ----- drawing ----- */

	function bar(x, y, w, frac)
	{
		frac = Math.max(0, Math.min(1, frac));
		var n = Math.round(frac * w), s = "\x01n\x01c";
		for (var i = 0; i < n; i++)
			s += MU_BAR_FULL;
		s += "\x01n\x01b";
		for (var j = n; j < w; j++)
			s += MU_BAR_EMPTY;
		put(x, y, s + "\x01n");
	}

	function draw_title()
	{
		var left = " SixelMusic", right = playlist.length > 1 ? "track " + (cur + 1) + " of " + playlist.length + " " : "";
		var mid = W - left.length - right.length;
		put(1, 1, "\x01n\x01h\x01w\x014" + left + music_pad("", Math.max(0, mid)) + right + "\x01n");
	}

	function draw_info()
	{
		var i = tr ? tr.info : null, tg = i ? i.tags : {};
		var title = tg.title || (playlist[cur] ? playlist[cur].name : ""), n = W - 4;
		put(3, 3, "\x01n\x01h\x01w" + music_pad(music_fit(title, n), n) + "\x01n");
		put(3, 4, "\x01n\x01c" + music_pad(music_fit(tg.artist || "", n), n) + "\x01n");
		put(3, 5, "\x01n\x01c" + music_pad(music_fit((tg.album || "") + (tg.year && tg.album ? "  (" + tg.year + ")" : ""), n), n) + "\x01n");
		var line = "";
		if (err)
			line = "\x01h\x01r" + music_fit(err, n);
		else if (i)
			line = "\x01n\x01w" + music_fit(i.label + "   " + Math.round(i.bitrate / 1000) + " kbps   " + (i.sr / 1000).toFixed(1).replace(/\.0$/, "") + " kHz   " +
				(i.channels == 1 ? "Mono" : i.channels == 2 ? "Stereo" : i.channels + " channels") + "   " + music_time(i.duration), n);
		put(3, 6, music_pad("", n) + "\x01n");
		put(3, 6, line + "\x01n");
	}

	var BAR_X = 3, BAR_Y = 8, VOL_Y = 10, BTN_Y = 12;

	function draw_progress()
	{
		var dur = tr ? tr.info.duration : 0;
		var pos = position();
		var tw = 13;
		var w = W - BAR_X - tw - 2;
		bar(BAR_X, BAR_Y, w, dur ? pos / dur : 0);
		var ts = music_time(pos) + " / " + music_time(dur);
		put(BAR_X + w + 1, BAR_Y, "\x01n\x01h\x01w" + music_pad(ts, tw) + "\x01n");
	}

	function draw_volume()
	{
		var w = 20, x = BAR_X + 8;
		put(BAR_X, VOL_Y, "\x01n\x01wVolume");
		regions.vol = { x1: x, x2: x + w - 1, y: VOL_Y };
		bar(x, VOL_Y, w, muted ? 0 : volume / 100);
		put(x + w + 1, VOL_Y, "\x01n\x01h\x01w" + music_pad(muted ? "muted" : volume + "%", 6) + "\x01n");
		var ax = x + w + 10, label = autoplay ? "Autoplay: On " : "Autoplay: Off";
		regions.auto = { x1: ax, x2: ax + label.length + 3, y: VOL_Y };
		put(ax, VOL_Y, "\x01n\x01h\x01y[ " + (autoplay ? "\x01g" : "\x01w") + label + " \x01y]\x01n");
	}

	function draw_buttons()
	{
		var defs = [
			["prev", "|<"], ["play", playing ? "Stop" : "Play"], ["next", ">|"], ["quit", "Quit"]
		];
		buttons = [];
		var x = BAR_X, s = "";
		for (var i = 0; i < defs.length; i++) {
			var label = "[ " + defs[i][1] + " ]";
			buttons.push({ x1: x, x2: x + label.length - 1, y: BTN_Y, id: defs[i][0] });
			put(x, BTN_Y, "\x01n\x01h\x01y[ \x01w" + defs[i][1] + " \x01y]\x01n");
			x += label.length + 1;
		}
	}

	/* The bottom line is the menu, made like the other screens' menus.  A short message, if there is one, goes on
	 * the line above it so that the menu can always be clicked. */
	function draw_help()
	{
		var msg = note && system.timer < note_until ? note : "";
		put(2, rows - 1, "\x01n\x01h\x01y" + music_pad(music_fit(msg, W - 2), W - 2) + "\x01n");
		status_menu(" ", [playing ? "(S)top" : "(S)tart", "(N/P) Track", "(Up/Dn) Volume", "(M)ute", "(A)utoplay " + (autoplay ? "On" : "Off"), "(Q)uit"], []);
	}

	function draw_all()
	{
		console.clear();
		draw_title();
		draw_info();
		draw_progress();
		draw_volume();
		draw_buttons();
		draw_help();
	}

	/* ----- input ----- */

	function click(m)
	{
		if (m.button !== 0)
			return null;
		for (var i = 0; i < buttons.length; i++) {
			var b = buttons[i];
			if (m.y == b.y && m.x >= b.x1 && m.x <= b.x2)
				return b.id;
		}
		var r = regions.auto;
		if (r && m.y == r.y && m.x >= r.x1 && m.x <= r.x2)
			return "auto";
		r = regions.vol;
		if (r && m.y == r.y && m.x >= r.x1 - 1 && m.x <= r.x2 + 1) {
			set_volume(Math.round((Math.max(r.x1, Math.min(r.x2, m.x)) - r.x1) / Math.max(1, r.x2 - r.x1) * 20) * 5);
			return "volume";
		}
		return null;
	}

	/* Do what a key (or a button's id) asks.  Returns true to quit. */
	function act(k)
	{
		var m;
		switch (k) {
			case "quit": case "q": case "Q": case "\x03":
				return true;
			case " ": case "play": case "s": case "S":
				play_toggle();
				break;
			case KEY_UP: case "+": case "=": case "vup":
				set_volume(volume + 5);
				break;
			case KEY_DOWN: case "-": case "_": case "vdown":
				set_volume(volume - 5);
				break;
			case "m": case "M":
				mute_toggle();
				break;
			case "a": case "A": case "auto":
				autoplay = !autoplay;
				say("Autoplay " + (autoplay ? "on: the next song starts by itself" : "off: the player stops after each song"));
				break;
			case ">": case KEY_PAGEDN: case "]": case "next": case "n": case "N":
				if (!jump(cur + 1))
					say("That is the last song");
				break;
			case "<": case KEY_PAGEUP: case "[": case "prev": case "p": case "P":
				if (tr && position() > 3)
					open_track(cur);				/* like a CD player: first go back to the start */
				else if (!jump(cur - 1))
					say("That is the first song");
				break;
			default:
				break;
		}
		return false;
	}

	/* ----- the main loop ----- */

	try {
		open_track(cur);
		var lastdraw = 0;
		while (bbs.online) {
			var k = console.inkey(K_NONE, 200);
			if (bbs.sys_status & SS_ABORT) {		/* right mouse button */
				bbs.sys_status &= ~SS_ABORT;
				break;
			}
			if (k == "\x1b") {
				var mk = read_mouse();
				if (mk === false)
					break;							/* the Esc key itself */
				var bk = mk ? bar_key(mk) : null;		/* a click on the bottom menu */
				k = bk !== null ? bk : mk ? click(mk) : null;
				if (!k)
					continue;
				if (k == "volume")
					k = "";
			}
			if (k && act(k))
				break;
			/* the song has finished: on to the next */
			if (tr && playing && position() >= tr.info.duration - 0.05) {
				if (autoplay && cur + 1 < playlist.length)
					jump(cur + 1);
				else {
					stop();
					base = 0;
					say(autoplay ? "End of the playlist" : "Song finished (A turns autoplay on)", 6);
				}
				continue;
			}
			var now = system.timer;
			if (k || now - lastdraw >= 0.5) {
				lastdraw = now;
				draw_progress();
				draw_volume();
				draw_buttons();
				if (k || note_dirty || (note && now > note_until)) {
					draw_help();
					note_dirty = false;
					if (now > note_until)
						note = "";
				}
			}
		}
	}
	finally {
		try { cterm.audio_flush(CH, 300); } catch (e) {}
		mouse_clicks(clicks_were_on);
		console.clear();
	}
}

/* The audio files beside entry (and entry's place among them), for a playlist */
function music_playlist_for(path)
{
	var dir = path.replace(/[^\/\\]*$/, ""), name = path.substr(dir.length);
	var list = [], files = directory(dir + "*"), idx = 0;
	files.sort(function (a, b) { var x = a.toLowerCase(), y = b.toLowerCase(); return x < y ? -1 : x > y ? 1 : 0; });
	for (var i = 0; i < files.length; i++) {
		var fn = files[i].substr(dir.length);
		if (file_isdir(files[i]) || MUSIC_TYPES.indexOf(mu_ext(fn)) < 0)
			continue;
		if (fn == name)
			idx = list.length;
		list.push({ name: fn, path: files[i] });
	}
	if (!list.length)
		list.push({ name: name, path: path });
	return { list: list, index: idx };
}

this;
