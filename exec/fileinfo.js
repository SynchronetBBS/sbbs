// Display information about a media file (audio, video, or image), identified
// by its content rather than its filename, plus any SAUCE record it carries.
//
// Reads only the file's headers and trailers, never the whole file (with the
// exception of MIDI files, which are small), so it's fast for files of any
// size or location.
//
// Usage, as a Viewable File Type command-line (SCFG->File Options):
//     ?fileinfo %f
// or stand-alone:
//     jsexec fileinfo.js <path/filename> [...]

"use strict";

require("sbbsdefs.js", "P_UTF8");

var MAX_HEADER = 65536;
var MAX_TEXT = 512;
var MAX_MIDI = 2 * 1024 * 1024;

var file;
var printed = 0;

// ---------------------------------------------------------------------------
// Binary helpers: a "buffer" is a string of 8-bit character codes
// ---------------------------------------------------------------------------

function rd(pos, len)
{
	if (pos < 0 || pos >= file.length || len < 1)
		return "";
	file.position = pos;
	if (file.position != pos) // seek failed (e.g. offset not supported)
		return "";
	var s = file.read(len);
	return (typeof s == "string") ? s : "";
}

function u8(s, o) { return s.charCodeAt(o) & 0xff; }
function le16(s, o) { return u8(s, o) + u8(s, o + 1) * 256; }
function be16(s, o) { return u8(s, o) * 256 + u8(s, o + 1); }
function be24(s, o) { return u8(s, o) * 65536 + be16(s, o + 1); }
function le32(s, o) { return le16(s, o) + le16(s, o + 2) * 65536; }
function be32(s, o) { return be16(s, o) * 65536 + be16(s, o + 2); }
function le64(s, o) { return le32(s, o) + le32(s, o + 4) * 4294967296; }
function be64(s, o) { return be32(s, o) * 4294967296 + be32(s, o + 4); }
function synchsafe(s, o)
{
	return (u8(s, o) & 0x7f) * 2097152 + (u8(s, o + 1) & 0x7f) * 16384
	       + (u8(s, o + 2) & 0x7f) * 128 + (u8(s, o + 3) & 0x7f);
}

// ---------------------------------------------------------------------------
// Text helpers: tag text is carried as a string of UTF-8 bytes
// ---------------------------------------------------------------------------

function utf8_from_codes(codes)
{
	var out = "";
	for (var i = 0; i < codes.length; i++) {
		var c = codes[i];
		if (c >= 0xd800 && c < 0xdc00 && i + 1 < codes.length
		    && codes[i + 1] >= 0xdc00 && codes[i + 1] < 0xe000) {
			c = 0x10000 + ((c - 0xd800) << 10) + (codes[i + 1] - 0xdc00);
			i++;
		}
		if (c < 0x80)
			out += String.fromCharCode(c);
		else if (c < 0x800)
			out += String.fromCharCode(0xc0 | (c >> 6), 0x80 | (c & 0x3f));
		else if (c < 0x10000)
			out += String.fromCharCode(0xe0 | (c >> 12), 0x80 | ((c >> 6) & 0x3f), 0x80 | (c & 0x3f));
		else
			out += String.fromCharCode(0xf0 | (c >> 18), 0x80 | ((c >> 12) & 0x3f)
			                           , 0x80 | ((c >> 6) & 0x3f), 0x80 | (c & 0x3f));
	}
	return out;
}

function latin1_text(s)
{
	var codes = [];
	for (var i = 0; i < s.length; i++) {
		var c = u8(s, i);
		if (c == 0)
			break;
		codes.push(c);
	}
	return utf8_from_codes(codes);
}

function utf16_text(s, big_endian)
{
	var codes = [];
	for (var i = 0; i + 1 < s.length; i += 2) {
		var c = big_endian ? be16(s, i) : le16(s, i);
		if (c == 0)
			break;
		codes.push(c);
	}
	return utf8_from_codes(codes);
}

function utf8_text(s)
{
	var end = s.indexOf("\0");
	return end < 0 ? s : s.substr(0, end);
}

// Remove control characters (Ctrl-A codes included) and surrounding white-space
function clean(s)
{
	return String(s).replace(/[\x00-\x1f\x7f]+/g, " ").replace(/^\s+|\s+$/g, "");
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

function out(label, value, utf8)
{
	if (value === undefined || value === null)
		return;
	value = clean(value);
	if (!value.length)
		return;
	var line = format("\x01n\x01c%-13s\x01h%s\x01n", label ? label + ":" : "", value);
	if (js.global.console)
		console.print(line + "\r\n", P_NOATCODES | (utf8 ? P_UTF8 : 0));
	else
		print(strip_ctrl(line));
	printed++;
}

function out_tags(tags)
{
	var order = ["Title", "Artist", "Album", "Year", "Track", "Genre", "Comment", "Copyright"];
	for (var i = 0; i < order.length; i++)
		out(order[i], tags[order[i]], /* utf8: */true);
}

function blank_line()
{
	if (js.global.console)
		console.newline();
	else
		print("");
}

function duration(seconds, approximate)
{
	if (!(seconds >= 0) || !isFinite(seconds))
		return undefined;
	var t = Math.round(seconds);
	var h = Math.floor(t / 3600);
	var m = Math.floor((t % 3600) / 60);
	var s = t % 60;
	var str = h ? format("%u:%02u:%02u", h, m, s) : format("%u:%02u", m, s);
	return approximate ? str + " (estimated)" : str;
}

function kbps(bits_per_second)
{
	if (!(bits_per_second > 0) || !isFinite(bits_per_second))
		return undefined;
	return Math.round(bits_per_second / 1000) + " kbps";
}

function hertz(rate)
{
	return rate > 0 ? rate + " Hz" : undefined;
}

function channels(count)
{
	if (!(count > 0))
		return undefined;
	if (count == 1)
		return "Mono";
	if (count == 2)
		return "Stereo";
	return count + " channels";
}

function pixels(width, height)
{
	if (!(width > 0 && height > 0))
		return undefined;
	return width + " x " + height + " pixels";
}

function join(list)
{
	var result = [];
	for (var i = 0; i < list.length; i++)
		if (list[i] !== undefined && list[i] !== null && list[i] !== "")
			result.push(list[i]);
	return result.join(", ");
}

function fourcc(s)
{
	return clean(s.replace(/[^\x20-\x7e]/g, ""));
}

// ---------------------------------------------------------------------------
// Shared tables
// ---------------------------------------------------------------------------

var wave_formats = {
	0x0001: "PCM",
	0x0002: "MS ADPCM",
	0x0003: "IEEE float",
	0x0006: "A-law",
	0x0007: "mu-law",
	0x000a: "WMA Voice",
	0x0011: "IMA ADPCM",
	0x0031: "GSM 6.10",
	0x0050: "MPEG",
	0x0055: "MP3",
	0x00ff: "AAC",
	0x0160: "WMA v1",
	0x0161: "WMA",
	0x0162: "WMA Pro",
	0x0163: "WMA Lossless",
	0x2000: "AC-3",
	0x2001: "DTS",
	0xf1ac: "FLAC"
};

function wave_format(tag)
{
	return wave_formats[tag] || format("format 0x%04X", tag);
}

var id3_genres = [
	"Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge", "Hip-Hop", "Jazz", "Metal",
	"New Age", "Oldies", "Other", "Pop", "R&B", "Rap", "Reggae", "Rock", "Techno", "Industrial",
	"Alternative", "Ska", "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", "Trip-Hop",
	"Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical", "Instrumental", "Acid", "House", "Game",
	"Sound Clip", "Gospel", "Noise", "AlternRock", "Bass", "Soul", "Punk", "Space", "Meditative",
	"Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic", "Darkwave", "Techno-Industrial",
	"Electronic", "Pop-Folk", "Eurodance", "Dream", "Southern Rock", "Comedy", "Cult", "Gangsta",
	"Top 40", "Christian Rap", "Pop/Funk", "Jungle", "Native American", "Cabaret", "New Wave",
	"Psychedelic", "Rave", "Showtunes", "Trailer", "Lo-Fi", "Tribal", "Acid Punk", "Acid Jazz", "Polka",
	"Retro", "Musical", "Rock & Roll", "Hard Rock"
];

// Vorbis comments (Ogg Vorbis/Opus, FLAC): returns tags found in 's' at offset 'o'
function vorbis_comments(s, o, tags)
{
	var map = { TITLE: "Title", ARTIST: "Artist", ALBUM: "Album", DATE: "Year", YEAR: "Year"
		        , TRACKNUMBER: "Track", GENRE: "Genre", COMMENT: "Comment", DESCRIPTION: "Comment"
		        , COPYRIGHT: "Copyright" };
	if (o + 8 > s.length)
		return;
	o += 4 + le32(s, o); // vendor string
	if (o + 4 > s.length)
		return;
	var count = le32(s, o);
	o += 4;
	for (var i = 0; i < count && i < 200 && o + 4 <= s.length; i++) {
		var len = le32(s, o);
		o += 4;
		if (o + len > s.length)
			break;
		var entry = s.substr(o, Math.min(len, MAX_TEXT));
		o += len;
		var eq = entry.indexOf("=");
		if (eq < 1)
			continue;
		var key = map[entry.substr(0, eq).toUpperCase()];
		if (key && tags[key] === undefined)
			tags[key] = utf8_text(entry.substr(eq + 1));
	}
}

// ---------------------------------------------------------------------------
// MPEG audio (MP3) and ID3 tags
// ---------------------------------------------------------------------------

var mpeg_bitrates = {
	v1l1: [32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448],
	v1l2: [32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384],
	v1l3: [32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320],
	v2l1: [32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256],
	v2l2: [8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160]
};

function mpeg_frame(s, o)
{
	if (o + 4 > s.length || u8(s, o) != 0xff || (u8(s, o + 1) & 0xe0) != 0xe0)
		return null;
	var b1 = u8(s, o + 1);
	var b2 = u8(s, o + 2);
	var version = (b1 >> 3) & 3;  // 0: 2.5, 2: 2, 3: 1
	var layer = 4 - ((b1 >> 1) & 3); // 1, 2, or 3
	var bitrate_index = b2 >> 4;
	var rate_index = (b2 >> 2) & 3;
	if (version == 1 || layer == 4 || bitrate_index == 0 || bitrate_index == 15 || rate_index == 3)
		return null;
	var frame = {};
	frame.version = version == 3 ? "1" : (version == 2 ? "2" : "2.5");
	frame.layer = layer;
	var table;
	if (version == 3)
		table = mpeg_bitrates["v1l" + layer];
	else
		table = layer == 1 ? mpeg_bitrates.v2l1 : mpeg_bitrates.v2l2;
	frame.bitrate = table[bitrate_index - 1] * 1000;
	frame.rate = [44100, 48000, 32000][rate_index];
	if (version == 2)
		frame.rate /= 2;
	else if (version == 0)
		frame.rate /= 4;
	var padding = (b2 >> 1) & 1;
	frame.mode = u8(s, o + 3) >> 6;
	frame.mono = frame.mode == 3;
	if (layer == 1) {
		frame.samples = 384;
		frame.length = (Math.floor(12 * frame.bitrate / frame.rate) + padding) * 4;
	} else {
		frame.samples = (layer == 3 && version != 3) ? 576 : 1152;
		frame.length = Math.floor(frame.samples / 8 * frame.bitrate / frame.rate) + padding;
	}
	if (version == 3)
		frame.side_info = frame.mono ? 17 : 32;
	else
		frame.side_info = frame.mono ? 9 : 17;
	return frame;
}

function id3v2_text(data)
{
	if (data.length < 2)
		return undefined;
	var encoding = u8(data, 0);
	var text = data.substr(1);
	switch (encoding) {
		case 1:
			if (u8(text, 0) == 0xfe && u8(text, 1) == 0xff)
				return utf16_text(text.substr(2), /* big_endian: */true);
			if (u8(text, 0) == 0xff && u8(text, 1) == 0xfe)
				return utf16_text(text.substr(2), false);
			return utf16_text(text, false);
		case 2:
			return utf16_text(text, /* big_endian: */true);
		case 3:
			return utf8_text(text);
	}
	return latin1_text(text);
}

function id3_genre(text)
{
	var m = /^\(?(\d{1,3})\)?$/.exec(clean(text));
	if (m && id3_genres[parseInt(m[1], 10)])
		return id3_genres[parseInt(m[1], 10)];
	return text;
}

// Returns the total size of an ID3v2 tag at the start of the file (0 if none)
function id3v2_size(head)
{
	if (head.substr(0, 3) != "ID3" || u8(head, 3) == 0xff)
		return 0;
	return 10 + synchsafe(head, 6) + ((u8(head, 5) & 0x10) ? 10 : 0);
}

function id3v2_read(head, tags)
{
	var names = { TIT2: "Title", TPE1: "Artist", TALB: "Album", TYER: "Year", TDRC: "Year"
		          , TRCK: "Track", TCON: "Genre", TCOP: "Copyright"
		          , TT2: "Title", TP1: "Artist", TAL: "Album", TYE: "Year", TRK: "Track", TCO: "Genre" };
	var version = u8(head, 3);
	if (version < 2 || version > 4)
		return undefined;
	var end = 10 + synchsafe(head, 6);
	var pos = 10;
	if (u8(head, 5) & 0x40) { // extended header
		var ext = rd(pos, 4);
		pos += version == 4 ? synchsafe(ext, 0) : 4 + be32(ext, 0);
	}
	var header_len = version == 2 ? 6 : 10;
	for (var count = 0; count < 200 && pos + header_len <= end; count++) {
		var hdr = rd(pos, header_len);
		if (hdr.length < header_len)
			break;
		var id = hdr.substr(0, version == 2 ? 3 : 4);
		if (!/^[A-Z0-9]+$/.test(id))
			break; // padding
		var size;
		if (version == 2)
			size = be24(hdr, 3);
		else if (version == 4)
			size = synchsafe(hdr, 4);
		else
			size = be32(hdr, 4);
		if (names[id] && size > 1 && tags[names[id]] === undefined) {
			var text = id3v2_text(rd(pos + header_len, Math.min(size, MAX_TEXT)));
			if (names[id] == "Genre" && text !== undefined)
				text = id3_genre(text);
			tags[names[id]] = text;
		}
		pos += header_len + size;
	}
	return "ID3v2." + version;
}

function id3v1_read(tags)
{
	if (file.length < 128)
		return false;
	var tag = rd(file.length - 128, 128);
	if (tag.substr(0, 3) != "TAG")
		return false;
	var fields = { Title: tag.substr(3, 30), Artist: tag.substr(33, 30), Album: tag.substr(63, 30)
		           , Year: tag.substr(93, 4) };
	for (var name in fields) {
		var text = latin1_text(fields[name]);
		if (tags[name] === undefined && clean(text).length)
			tags[name] = text;
	}
	if (tags.Track === undefined && u8(tag, 125) == 0 && u8(tag, 126) != 0)
		tags.Track = String(u8(tag, 126));
	if (tags.Genre === undefined && id3_genres[u8(tag, 127)])
		tags.Genre = id3_genres[u8(tag, 127)];
	return true;
}

// Returns the offset of the first MPEG audio frame in 's' (-1 if none)
function mpeg_sync(s)
{
	for (var o = 0; o + 4 <= s.length; o++) {
		if (u8(s, o) != 0xff)
			continue;
		var frame = mpeg_frame(s, o);
		if (frame === null)
			continue;
		var next = o + frame.length;
		if (next + 4 > s.length)
			return o;
		var following = mpeg_frame(s, next);
		if (following !== null && following.rate == frame.rate && following.layer == frame.layer)
			return o;
	}
	return -1;
}

function mp3_info(head, tag_size)
{
	var tags = {};
	var tag_versions = [];
	if (tag_size) {
		var v2 = id3v2_read(head, tags);
		if (v2)
			tag_versions.push(v2);
	}
	var buf = rd(tag_size, MAX_HEADER);
	var sync = mpeg_sync(buf);
	if (sync < 0)
		return false;
	var frame = mpeg_frame(buf, sync);
	var audio_start = tag_size + sync;
	var has_v1 = id3v1_read(tags);
	if (has_v1)
		tag_versions.push("ID3v1");
	var audio_bytes = file.length - audio_start - (has_v1 ? 128 : 0);

	var total_frames;
	var total_bytes;
	var vbr = false;
	if (frame.layer == 3) {
		var x = sync + 4 + frame.side_info;
		var id = buf.substr(x, 4);
		if (id == "Xing" || id == "Info") {
			vbr = (id == "Xing");
			var flags = be32(buf, x + 4);
			var o = x + 8;
			if (flags & 1) {
				total_frames = be32(buf, o);
				o += 4;
			}
			if (flags & 2)
				total_bytes = be32(buf, o);
		} else if (buf.substr(sync + 36, 4) == "VBRI") {
			vbr = true;
			total_bytes = be32(buf, sync + 46);
			total_frames = be32(buf, sync + 50);
		}
	}
	var seconds;
	var estimated = false;
	var bitrate = frame.bitrate;
	if (total_frames > 0) {
		seconds = total_frames * frame.samples / frame.rate;
		bitrate = (total_bytes > 0 ? total_bytes : audio_bytes) * 8 / seconds;
	} else {
		// No length header: sample the frames at hand for a varying bitrate
		var o = sync;
		var sum = 0;
		var frames = 0;
		while (frames < 64) {
			var f = mpeg_frame(buf, o);
			if (f === null)
				break;
			if (f.bitrate != frame.bitrate)
				vbr = true;
			sum += f.bitrate;
			frames++;
			o += f.length;
		}
		if (vbr) {
			bitrate = sum / frames;
			estimated = true;
		}
		seconds = audio_bytes * 8 / bitrate;
	}

	var names = { 1: "MP1", 2: "MP2", 3: "MP3" };
	out("Type", format("MPEG-%s Layer %s audio (%s)", frame.version
	                   , ["I", "II", "III"][frame.layer - 1], names[frame.layer]));
	out("Bitrate", kbps(bitrate) + (vbr ? (estimated ? " (variable, estimated)" : " average (variable)") : ""));
	out("Sample rate", hertz(frame.rate));
	out("Channels", ["Stereo", "Joint stereo", "Dual channel", "Mono"][frame.mode]);
	out("Length", duration(seconds, estimated));
	out_tags(tags);
	out("Tag format", tag_versions.join(", "));
	return true;
}

// ---------------------------------------------------------------------------
// RIFF: WAVE audio and AVI video
// ---------------------------------------------------------------------------

function riff_chunks(pos, end, callback)
{
	for (var count = 0; count < 128 && pos + 8 <= end; count++) {
		var hdr = rd(pos, 12);
		if (hdr.length < 8)
			break;
		var size = le32(hdr, 4);
		if (callback(hdr.substr(0, 4), pos + 8, size, hdr.substr(8, 4)) === false)
			break;
		pos += 8 + size + (size & 1);
	}
}

function wav_info(head)
{
	var fmt;
	var data_size;
	var data_size64;
	var tags = {};
	var info = { INAM: "Title", IART: "Artist", IPRD: "Album", ICRD: "Year", IGNR: "Genre"
		         , ICMT: "Comment", ICOP: "Copyright" };

	riff_chunks(12, file.length, function (id, pos, size, type) {
		if (id == "fmt ")
			fmt = rd(pos, Math.min(size, 40));
		else if (id == "ds64")
			data_size64 = le64(rd(pos, 16), 8);
		else if (id == "data") {
			data_size = size;
			if (size == 0xffffffff && data_size64 !== undefined) {
				data_size = data_size64;
				return false;
			}
		}
		else if (id == "LIST" && type == "INFO") {
			riff_chunks(pos + 4, pos + size, function (id, pos, size) {
				if (info[id] && size > 0)
					tags[info[id]] = latin1_text(rd(pos, Math.min(size, MAX_TEXT)));
			});
		}
	});
	if (fmt === undefined || fmt.length < 16)
		return false;
	var tag = le16(fmt, 0);
	if (tag == 0xfffe && fmt.length >= 26)
		tag = le16(fmt, 24);
	var byte_rate = le32(fmt, 8);
	var bits = le16(fmt, 14);
	out("Type", "WAVE audio (" + wave_format(tag) + ")");
	if (bits)
		out("Sample size", bits + "-bit");
	out("Sample rate", hertz(le32(fmt, 4)));
	out("Channels", channels(le16(fmt, 2)));
	out("Bitrate", kbps(byte_rate * 8));
	if (byte_rate && data_size !== undefined)
		out("Length", duration(Math.min(data_size, file.length) / byte_rate));
	out_tags(tags);
	return true;
}

function avi_info(head)
{
	var avih;
	var streams = [];

	riff_chunks(12, file.length, function (id, pos, size, type) {
		if (id != "LIST" || type != "hdrl")
			return id != "LIST" || type != "movi";
		riff_chunks(pos + 4, pos + size, function (id, pos, size, type) {
			if (id == "avih")
				avih = rd(pos, Math.min(size, 56));
			else if (id == "LIST" && type == "strl") {
				var stream = {};
				riff_chunks(pos + 4, pos + size, function (id, pos, size) {
					if (id == "strh")
						stream.strh = rd(pos, Math.min(size, 56));
					else if (id == "strf")
						stream.strf = rd(pos, Math.min(size, 40));
				});
				streams.push(stream);
			}
		});
		return false;
	});
	if (avih === undefined || avih.length < 40)
		return false;
	var usec_per_frame = le32(avih, 0);
	var total_frames = le32(avih, 16);
	var video;
	var audio;
	for (var i = 0; i < streams.length; i++) {
		var strh = streams[i].strh || "";
		var strf = streams[i].strf || "";
		var kind = strh.substr(0, 4);
		if (kind == "vids" && video === undefined) {
			var codec = fourcc(strf.substr(16, 4)) || fourcc(strh.substr(4, 4));
			var scale = le32(strh, 20);
			var fps = scale ? le32(strh, 24) / scale : 0;
			video = join([codec, pixels(le32(avih, 32), le32(avih, 36))
				          , fps ? (Math.round(fps * 100) / 100) + " fps" : undefined]);
		}
		else if (kind == "auds" && audio === undefined && strf.length >= 16)
			audio = join([wave_format(le16(strf, 0)), hertz(le32(strf, 4)), channels(le16(strf, 2))
				          , kbps(le32(strf, 8) * 8)]);
	}
	out("Type", "AVI video");
	out("Video", video);
	out("Audio", audio);
	if (usec_per_frame && total_frames)
		out("Length", duration(total_frames * usec_per_frame / 1000000));
	return true;
}

// ---------------------------------------------------------------------------
// Ogg (Vorbis, Opus, FLAC, Speex, Theora) and Rock Band MOGG
// ---------------------------------------------------------------------------

function ogg_info(base, container)
{
	var head = rd(base, MAX_HEADER);
	if (head.substr(0, 4) != "OggS" || head.length < 28)
		return false;
	var serial = head.substr(14, 4);
	var packet = 27 + u8(head, 26);
	var codec;
	var rate = 0;
	var count = 0;
	var nominal = 0;
	var preskip = 0;
	var video = false;
	var tags = {};

	if (head.substr(packet, 7) == "\x01vorbis") {
		codec = "Vorbis";
		count = u8(head, packet + 11);
		rate = le32(head, packet + 12);
		nominal = le32(head, packet + 20);
		if (nominal >= 0x80000000)
			nominal = 0;
		var comments = head.indexOf("\x03vorbis", packet);
		if (comments > 0)
			vorbis_comments(head, comments + 7, tags);
	} else if (head.substr(packet, 8) == "OpusHead") {
		codec = "Opus";
		count = u8(head, packet + 9);
		preskip = le16(head, packet + 10);
		rate = 48000;
		var comments = head.indexOf("OpusTags", packet);
		if (comments > 0)
			vorbis_comments(head, comments + 8, tags);
	} else if (head.substr(packet, 5) == "\x7fFLAC") {
		codec = "FLAC";
		var info = packet + 17; // STREAMINFO block data
		rate = (u8(head, info + 10) << 12) | (u8(head, info + 11) << 4) | (u8(head, info + 12) >> 4);
		count = ((u8(head, info + 12) >> 1) & 7) + 1;
	} else if (head.substr(packet, 8) == "Speex   ") {
		codec = "Speex";
		rate = le32(head, packet + 36);
		count = le32(head, packet + 48);
	} else if (head.substr(packet, 7) == "\x80theora") {
		codec = "Theora";
		video = true;
	}

	// The last page of the stream gives the total number of samples
	var seconds;
	if (rate && !video) {
		var tail_len = Math.min(MAX_HEADER, file.length - base);
		var tail = rd(file.length - tail_len, tail_len);
		for (var o = tail.lastIndexOf("OggS"); o >= 0; o = o ? tail.lastIndexOf("OggS", o - 1) : -1) {
			if (o + 27 > tail.length || tail.substr(o + 14, 4) != serial)
				continue;
			if (le32(tail, o + 6) == 0xffffffff && le32(tail, o + 10) == 0xffffffff)
				continue;
			seconds = Math.max(0, le64(tail, o + 6) - preskip) / rate;
			break;
		}
	}

	var type = "Ogg " + (codec || "stream") + (video ? " video" : " audio");
	out("Type", container ? container + " (" + type + ")" : type);
	if (!video) {
		out("Channels", channels(count));
		out("Sample rate", hertz(rate));
		if (seconds > 0)
			out("Bitrate", kbps((file.length - base) * 8 / seconds) + " average");
		else if (nominal)
			out("Bitrate", kbps(nominal) + " nominal");
		out("Length", duration(seconds));
	} else
		out("Video", pixels(be24(head, packet + 14), be24(head, packet + 17)));
	out_tags(tags);
	return true;
}

function mogg_info(head)
{
	var version = le32(head, 0);
	var offset = le32(head, 4);
	if (version == 10 && rd(offset, 4) == "OggS")
		return ogg_info(offset, "Multitrack Ogg (MOGG)");
	out("Type", format("Multitrack Ogg (MOGG), encrypted (version %u)", version));
	return true;
}

// ---------------------------------------------------------------------------
// FLAC
// ---------------------------------------------------------------------------

function flac_info(base)
{
	var tags = {};
	var info;
	var pos = base + 4;
	for (var count = 0; count < 64; count++) {
		var hdr = rd(pos, 4);
		if (hdr.length < 4)
			break;
		var type = u8(hdr, 0) & 0x7f;
		var len = be24(hdr, 1);
		if (type == 0)
			info = rd(pos + 4, 34);
		else if (type == 4)
			vorbis_comments(rd(pos + 4, Math.min(len, MAX_HEADER)), 0, tags);
		if (u8(hdr, 0) & 0x80)
			break;
		pos += 4 + len;
	}
	if (info === undefined || info.length < 18)
		return false;
	var rate = (u8(info, 10) << 12) | (u8(info, 11) << 4) | (u8(info, 12) >> 4);
	var bits = (((u8(info, 12) & 1) << 4) | (u8(info, 13) >> 4)) + 1;
	var samples = (u8(info, 13) & 0x0f) * 4294967296 + be32(info, 14);
	var seconds = rate ? samples / rate : 0;
	out("Type", "FLAC audio (lossless)");
	out("Sample size", bits + "-bit");
	out("Sample rate", hertz(rate));
	out("Channels", channels(((u8(info, 12) >> 1) & 7) + 1));
	if (seconds > 0) {
		out("Bitrate", kbps((file.length - base) * 8 / seconds) + " average");
		out("Length", duration(seconds));
	}
	out_tags(tags);
	return true;
}

// ---------------------------------------------------------------------------
// ISO base media: MP4, M4A, QuickTime, 3GPP
// ---------------------------------------------------------------------------

var mp4_codecs = {
	"mp4a": "AAC", "alac": "Apple Lossless", "ac-3": "AC-3", "ec-3": "E-AC-3", "Opus": "Opus"
	, "fLaC": "FLAC", "samr": "AMR", "sawb": "AMR-WB", ".mp3": "MP3", "sowt": "PCM", "twos": "PCM"
	, "avc1": "H.264", "avc3": "H.264", "hvc1": "H.265", "hev1": "H.265", "mp4v": "MPEG-4 Visual"
	, "av01": "AV1", "vp09": "VP9", "vp08": "VP8", "s263": "H.263", "jpeg": "Motion JPEG"
	, "mjpa": "Motion JPEG", "SVQ3": "Sorenson 3", "SVQ1": "Sorenson 1"
};

function mp4_atoms(pos, end, callback)
{
	for (var count = 0; count < 512 && pos + 8 <= end; count++) {
		var hdr = rd(pos, 16);
		if (hdr.length < 8)
			break;
		var size = be32(hdr, 0);
		var hdr_len = 8;
		if (size == 1) {
			size = be64(hdr, 8);
			hdr_len = 16;
		} else if (size == 0)
			size = end - pos;
		if (size < hdr_len)
			break;
		if (callback(hdr.substr(4, 4), pos + hdr_len, Math.min(pos + size, end)) === false)
			break;
		pos += size;
	}
}

function mp4_info(head)
{
	var brand = head.substr(8, 4);
	var seconds;
	var tracks = [];
	var tags = {};
	var items = { "\xa9nam": "Title", "\xa9ART": "Artist", "\xa9alb": "Album", "\xa9day": "Year"
		          , "\xa9gen": "Genre", "\xa9cmt": "Comment", "cprt": "Copyright" };
	var found = false;

	mp4_atoms(0, file.length, function (type, pos, end) {
		if (type != "moov")
			return true;
		found = true;
		mp4_atoms(pos, end, function (type, pos, end) {
			if (type == "mvhd") {
				var mvhd = rd(pos, 32);
				var scale = u8(mvhd, 0) == 1 ? be32(mvhd, 20) : be32(mvhd, 12);
				var length = u8(mvhd, 0) == 1 ? be64(mvhd, 24) : be32(mvhd, 16);
				if (scale)
					seconds = length / scale;
			}
			else if (type == "trak") {
				var track = {};
				mp4_atoms(pos, end, function (type, pos, end) {
					if (type == "tkhd") {
						var size = rd(end - 8, 8);
						track.width = be16(size, 0);
						track.height = be16(size, 4);
					}
					else if (type == "mdia") {
						mp4_atoms(pos, end, function (type, pos, end) {
							if (type == "hdlr")
								track.handler = rd(pos + 8, 4);
							else if (type == "minf") {
								mp4_atoms(pos, end, function (type, pos, end) {
									if (type != "stbl")
										return true;
									mp4_atoms(pos, end, function (type, pos, end) {
										if (type != "stsd")
											return true;
										track.stsd = rd(pos, 64);
										return false;
									});
									return false;
								});
							}
						});
					}
				});
				tracks.push(track);
			}
			else if (type == "udta") {
				mp4_atoms(pos, end, function (type, pos, end) {
					if (type != "meta")
						return true;
					if (rd(pos + 4, 4) != "hdlr")
						pos += 4; // version and flags
					mp4_atoms(pos, end, function (type, pos, end) {
						if (type != "ilst")
							return true;
						mp4_atoms(pos, end, function (type, pos, end) {
							var name = items[type];
							if (!name)
								return true;
							mp4_atoms(pos, end, function (type, pos, end) {
								if (type != "data")
									return true;
								if (be32(rd(pos, 4), 0) == 1) // UTF-8 text
									tags[name] = utf8_text(rd(pos + 8, Math.min(end - (pos + 8), MAX_TEXT)));
								return false;
							});
							return true;
						});
						return false;
					});
					return false;
				});
			}
		});
		return false;
	});
	if (!found && head.substr(4, 4) != "ftyp")
		return false;

	var video;
	var audio;
	for (var i = 0; i < tracks.length; i++) {
		var stsd = tracks[i].stsd || "";
		var format_id = stsd.substr(12, 4);
		var codec = mp4_codecs[format_id] || fourcc(format_id);
		if (tracks[i].handler == "vide" && video === undefined)
			video = join([codec, pixels(be16(stsd, 40) || tracks[i].width, be16(stsd, 42) || tracks[i].height)]);
		else if (tracks[i].handler == "soun" && audio === undefined)
			audio = join([codec, hertz(be16(stsd, 40)), channels(be16(stsd, 32))]);
	}
	var type;
	if (brand == "qt  ")
		type = "QuickTime movie";
	else if (brand.substr(0, 2) == "3g")
		type = "3GPP media";
	else if (video !== undefined)
		type = "MPEG-4 video";
	else if (audio !== undefined)
		type = "MPEG-4 audio";
	else
		type = "MPEG-4 media";
	out("Type", type + " (" + fourcc(brand) + ")");
	out("Video", video);
	out("Audio", audio);
	if (seconds > 0) {
		out("Bitrate", kbps(file.length * 8 / seconds) + " average");
		out("Length", duration(seconds));
	}
	out_tags(tags);
	return true;
}

// ---------------------------------------------------------------------------
// ASF: Windows Media Audio and Video
// ---------------------------------------------------------------------------

var asf_header_guid = "\x30\x26\xb2\x75\x8e\x66\xcf\x11\xa6\xd9\x00\xaa\x00\x62\xce\x6c";
var asf_file_guid = "\xa1\xdc\xab\x8c\x47\xa9\xcf\x11\x8e\xe4\x00\xc0\x0c\x20\x53\x65";
var asf_stream_guid = "\x91\x07\xdc\xb7\xb7\xa9\xcf\x11\x8e\xe6\x00\xc0\x0c\x20\x53\x65";
var asf_content_guid = "\x33\x26\xb2\x75\x8e\x66\xcf\x11\xa6\xd9\x00\xaa\x00\x62\xce\x6c";
var asf_extended_guid = "\x40\xa4\xd0\xd2\x07\xe3\xd2\x11\x97\xf0\x00\xa0\xc9\x5e\xa8\x50";
var asf_audio_guid = "\x40\x9e\x69\xf8\x4d\x5b\xcf\x11\xa8\xfd\x00\x80\x5f\x5c\x44\x2b";
var asf_video_guid = "\xc0\xef\x19\xbc\x4d\x5b\xcf\x11\xa8\xfd\x00\x80\x5f\x5c\x44\x2b";

function asf_info(head)
{
	var size = Math.min(le64(head, 16), 4 * MAX_HEADER);
	var hdr = rd(0, size);
	var seconds;
	var audio;
	var video;
	var bitrate;
	var tags = {};
	var extended = { "WM/AlbumTitle": "Album", "WM/Year": "Year", "WM/Genre": "Genre"
		             , "WM/TrackNumber": "Track" };

	var pos = 30;
	for (var count = 0; count < 64 && pos + 24 <= hdr.length; count++) {
		var guid = hdr.substr(pos, 16);
		var len = le64(hdr, pos + 16);
		if (len < 24)
			break;
		var o = pos + 24;
		if (guid == asf_file_guid) {
			seconds = le64(hdr, o + 40) / 10000000 - le64(hdr, o + 56) / 1000;
			bitrate = le32(hdr, o + 76);
		}
		else if (guid == asf_stream_guid) {
			var kind = hdr.substr(o, 16);
			var data = o + 54;
			if (kind == asf_audio_guid && audio === undefined)
				audio = join([wave_format(le16(hdr, data)), hertz(le32(hdr, data + 4))
					          , channels(le16(hdr, data + 2)), kbps(le32(hdr, data + 8) * 8)]);
			else if (kind == asf_video_guid && video === undefined)
				video = join([fourcc(hdr.substr(data + 27, 4)), pixels(le32(hdr, data), le32(hdr, data + 4))]);
		}
		else if (guid == asf_content_guid) {
			var lengths = [le16(hdr, o), le16(hdr, o + 2), le16(hdr, o + 4)];
			var names = ["Title", "Artist", "Copyright"];
			var text = o + 10;
			for (var i = 0; i < names.length; i++) {
				if (lengths[i] > 2)
					tags[names[i]] = utf16_text(hdr.substr(text, Math.min(lengths[i], MAX_TEXT)), false);
				text += lengths[i];
			}
		}
		else if (guid == asf_extended_guid) {
			var total = le16(hdr, o);
			var p = o + 2;
			for (var i = 0; i < total && i < 200 && p + 6 <= hdr.length; i++) {
				var name_len = le16(hdr, p);
				var name = utf16_text(hdr.substr(p + 2, name_len), false);
				p += 2 + name_len;
				var value_type = le16(hdr, p);
				var value_len = le16(hdr, p + 2);
				p += 4;
				if (extended[name] && tags[extended[name]] === undefined) {
					if (value_type == 0)
						tags[extended[name]] = utf16_text(hdr.substr(p, Math.min(value_len, MAX_TEXT)), false);
					else if (value_type == 3)
						tags[extended[name]] = String(le32(hdr, p));
				}
				p += value_len;
			}
		}
		pos += len;
	}
	out("Type", video !== undefined ? "Windows Media video (ASF)" : "Windows Media audio (ASF)");
	out("Video", video);
	out("Audio", audio);
	if (bitrate)
		out("Bitrate", kbps(bitrate) + " maximum");
	if (seconds > 0)
		out("Length", duration(seconds));
	out_tags(tags);
	return true;
}

// ---------------------------------------------------------------------------
// Standard MIDI files
// ---------------------------------------------------------------------------

function midi_info(head)
{
	var format_type = be16(head, 8);
	var track_count = be16(head, 10);
	var division = be16(head, 12);
	var title;
	var seconds;
	var bpm;

	if (file.length <= MAX_MIDI && format_type < 2) {
		var data = rd(0, file.length);
		var pos = 8 + be32(head, 4);
		var tempos = [];
		var last_tick = 0;
		for (var t = 0; t < track_count && pos + 8 <= data.length; t++) {
			var end = Math.min(pos + 8 + be32(data, pos + 4), data.length);
			var is_track = data.substr(pos, 4) == "MTrk";
			pos += 8;
			var tick = 0;
			var status = 0;
			while (is_track && pos < end) {
				var delta = 0;
				var b;
				do {
					b = u8(data, pos++);
					delta = delta * 128 + (b & 0x7f);
				} while ((b & 0x80) && pos < end);
				tick += delta;
				b = u8(data, pos);
				if (b == 0xff) {
					var meta = u8(data, pos + 1);
					pos += 2;
					var len = 0;
					do {
						b = u8(data, pos++);
						len = len * 128 + (b & 0x7f);
					} while ((b & 0x80) && pos < end);
					if (meta == 0x51 && len == 3)
						tempos.push({ tick: tick, usec: be24(data, pos) });
					else if (meta == 0x03 && t == 0 && title === undefined)
						title = latin1_text(data.substr(pos, Math.min(len, MAX_TEXT)));
					pos += len;
					if (meta == 0x2f)
						break;
				} else if (b == 0xf0 || b == 0xf7) {
					pos++;
					var len = 0;
					do {
						b = u8(data, pos++);
						len = len * 128 + (b & 0x7f);
					} while ((b & 0x80) && pos < end);
					pos += len;
				} else {
					if (b & 0x80) {
						status = b;
						pos++;
					}
					var kind = status & 0xf0;
					if (kind < 0x80)
						break; // invalid running status
					pos += (kind == 0xc0 || kind == 0xd0) ? 1 : 2;
				}
			}
			if (tick > last_tick)
				last_tick = tick;
			pos = end;
		}
		if (division & 0x8000) {
			var fps = 256 - (division >> 8);
			var per_frame = division & 0xff;
			if (fps * per_frame)
				seconds = last_tick / (fps * per_frame);
		} else if (division) {
			tempos.sort(function (a, b) { return a.tick - b.tick; });
			var usec = 500000;
			var tick = 0;
			seconds = 0;
			for (var i = 0; i < tempos.length && tempos[i].tick < last_tick; i++) {
				seconds += (tempos[i].tick - tick) * usec / division / 1000000;
				tick = tempos[i].tick;
				usec = tempos[i].usec;
			}
			seconds += (last_tick - tick) * usec / division / 1000000;
			var first = tempos.length ? tempos[0].usec : 500000;
			if (first)
				bpm = Math.round(60000000 / first);
		}
	}
	out("Type", format("Standard MIDI file (format %u)", format_type));
	out("Tracks", String(track_count));
	if (bpm)
		out("Tempo", bpm + " BPM" + (tempos.length > 1 ? " (initial)" : ""));
	if (seconds > 0)
		out("Length", duration(seconds));
	out("Title", title, /* utf8: */true);
	return true;
}

// ---------------------------------------------------------------------------
// Images
// ---------------------------------------------------------------------------

function png_info(head)
{
	var color_types = { 0: "grayscale", 2: "RGB", 3: "palette", 4: "grayscale with alpha", 6: "RGB with alpha" };
	var depth = u8(head, 24);
	var type = u8(head, 25);
	var samples = { 0: 1, 2: 3, 3: 1, 4: 2, 6: 4 }[type] || 1;
	out("Type", "PNG image" + (u8(head, 28) ? " (interlaced)" : ""));
	out("Dimensions", pixels(be32(head, 16), be32(head, 20)));
	out("Color", (depth * samples) + "-bit " + (color_types[type] || "color"));
	return true;
}

function gif_info(head)
{
	var flags = u8(head, 10);
	out("Type", "GIF image (" + head.substr(3, 3) + ")");
	out("Dimensions", pixels(le16(head, 6), le16(head, 8)));
	if (flags & 0x80)
		out("Color", (1 << ((flags & 7) + 1)) + " colors");
	return true;
}

function jpeg_info(head)
{
	var pos = 2;
	for (var count = 0; count < 512; count++) {
		var hdr = rd(pos, 10);
		if (hdr.length < 4 || u8(hdr, 0) != 0xff)
			break;
		var marker = u8(hdr, 1);
		if (marker == 0xff) { // fill byte
			pos++;
			continue;
		}
		if (marker == 0x01 || (marker >= 0xd0 && marker <= 0xd8)) {
			pos += 2;
			continue;
		}
		if (marker == 0xd9 || marker == 0xda)
			break;
		if (marker >= 0xc0 && marker <= 0xcf && marker != 0xc4 && marker != 0xc8 && marker != 0xcc) {
			var components = u8(hdr, 9);
			out("Type", "JPEG image" + (marker == 0xc2 ? " (progressive)" : ""));
			out("Dimensions", pixels(be16(hdr, 7), be16(hdr, 5)));
			out("Color", (u8(hdr, 4) * components) + "-bit "
			    + (components == 1 ? "grayscale" : (components == 4 ? "CMYK" : "color")));
			return true;
		}
		pos += 2 + be16(hdr, 2);
	}
	out("Type", "JPEG image");
	return true;
}

function bmp_info(head)
{
	var width;
	var height;
	var bits;
	if (le32(head, 14) == 12) {
		width = le16(head, 18);
		height = le16(head, 20);
		bits = le16(head, 24);
	} else {
		width = le32(head, 18);
		height = le32(head, 22);
		if (height >= 0x80000000)
			height = 4294967296 - height; // top-down
		bits = le16(head, 28);
	}
	out("Type", "BMP image");
	out("Dimensions", pixels(width, height));
	if (bits)
		out("Color", bits + "-bit");
	return true;
}

// ---------------------------------------------------------------------------
// SAUCE (Standard Architecture for Universal Comment Extensions)
// ---------------------------------------------------------------------------

function sauce_info()
{
	var lib = load({}, "sauce_lib.js");
	var sauce = lib.read(file);
	if (!sauce)
		return false;
	var datatypes = ["None", "Character", "Bitmap", "Vector", "Audio", "BinaryText", "XBin"
		             , "Archive", "Executable"];
	var filetypes = ["ASCII", "ANSI", "ANSiMation", "RIP script", "PCBoard", "Avatar", "HTML"
		             , "Source", "TundraDraw"];
	var type = datatypes[sauce.datatype] || format("type %u", sauce.datatype);
	if (sauce.datatype == lib.defs.datatype.character && filetypes[sauce.filetype])
		type += " / " + filetypes[sauce.filetype];
	if (printed)
		blank_line();
	out("SAUCE", type);
	out("Title", sauce.title);
	out("Author", sauce.author);
	out("Group", sauce.group);
	if (sauce.date instanceof Date && !isNaN(sauce.date.getTime()))
		out("Date", strftime("%Y-%m-%d", sauce.date.getTime() / 1000));
	if (sauce.cols)
		out("Size", sauce.cols + " x " + sauce.rows + (sauce.ice_color ? " (iCE colors)" : ""));
	if (sauce.datatype == lib.defs.datatype.character || sauce.datatype == lib.defs.datatype.bin)
		out("Font", sauce.tinfos);
	for (var i = 0; i < sauce.comment.length; i++)
		out(i ? "" : "Comment", sauce.comment[i]);
	return true;
}

// ---------------------------------------------------------------------------

function media_info(path)
{
	var head = rd(0, 64);
	var tag_size = id3v2_size(head);
	var magic = tag_size ? rd(tag_size, 4) : head.substr(0, 4);

	if (magic == "fLaC")
		return flac_info(tag_size);
	if (tag_size)
		return mp3_info(head, tag_size);
	if (magic == "RIFF" || magic == "RF64") {
		var form = head.substr(8, 4);
		if (form == "WAVE")
			return wav_info(head);
		if (form == "AVI ")
			return avi_info(head);
		return false;
	}
	if (magic == "OggS")
		return ogg_info(0);
	if (head.substr(4, 4) == "ftyp" || head.substr(4, 4) == "moov" || head.substr(4, 4) == "mdat"
	    || head.substr(4, 4) == "wide")
		return mp4_info(head);
	if (head.substr(0, 16) == asf_header_guid)
		return asf_info(head);
	if (magic == "MThd")
		return midi_info(head);
	if (head.substr(0, 8) == "\x89PNG\r\n\x1a\n")
		return png_info(head);
	if (head.substr(0, 4) == "GIF8")
		return gif_info(head);
	if (u8(head, 0) == 0xff && u8(head, 1) == 0xd8 && u8(head, 2) == 0xff)
		return jpeg_info(head);
	if (head.substr(0, 2) == "BM" && le32(head, 6) == 0)
		return bmp_info(head);
	if (head.substr(0, 5) == "%PDF-") {
		out("Type", "PDF document (version " + clean(head.substr(5, 3)) + ")");
		return true;
	}
	var archives = { "PK\x03\x04": "ZIP archive", "Rar!": "RAR archive", "7z\xbc\xaf": "7-Zip archive" };
	if (archives[magic]) {
		out("Type", archives[magic]);
		return true;
	}
	if (/\.mogg$/i.test(path) && le32(head, 0) >= 10 && le32(head, 0) <= 17 && le32(head, 4) < file.length)
		return mogg_info(head);
	return mp3_info(head, 0);
}

function fileinfo(path)
{
	file = new File(path);
	if (!file.open("rb")) {
		alert("Error " + file.error + " opening " + path);
		return false;
	}
	printed = 0;
	try {
		var recognized = false;
		try {
			recognized = media_info(path);
		} catch (e) {
			log(LOG_WARNING, "fileinfo: " + path + ": " + e);
		}
		if (!sauce_info() && !recognized && !printed)
			out("Type", "Unrecognized file format");
	} finally {
		file.close();
	}
	return true;
}

var fileinfo_result = true;
for (var fileinfo_arg = 0; fileinfo_arg < argc; fileinfo_arg++) {
	if (argc > 1) {
		if (fileinfo_arg)
			blank_line();
		out("File", file_getname(argv[fileinfo_arg]));
	}
	if (!fileinfo(argv[fileinfo_arg]))
		fileinfo_result = false;
}
if (argc < 1)
	alert("usage: fileinfo <path/filename> [...]");
fileinfo_result;
