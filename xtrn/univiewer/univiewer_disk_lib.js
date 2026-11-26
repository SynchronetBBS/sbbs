/* univiewer_disk_lib.js - read-only access to FAT disk images (floppy and hard disk images: .img, .ima, .dsk, .vfd)
 *
 * Pure JavaScript, no external programs.  Understands FAT12, FAT16 and FAT32, long file names, and a hard disk image's
 * partition table (each FAT partition shows as a folder named "Partition 1" and so on).  Other file systems are not
 * recognized.
 *
 *   probe(path)               true if the file looks like a FAT disk image (reads only its first sectors)
 *   list(path)                every folder and file on the disk: [{ name (with / between folders), isdir, size, date }]
 *   extract(path, inner, out) copy the file called inner (a name from list()) to the file out; true if it worked
 *
 * The file is never read whole: sectors are read as they are needed.
 */
"use strict";
(function () {
	var MAX_ENTRIES = 20000;		/* the most files and folders listed from one disk */
	var MAX_DEPTH = 24;
	var PARTITION_FAT = { 0x01: 1, 0x04: 1, 0x06: 1, 0x0b: 1, 0x0c: 1, 0x0e: 1, 0x11: 1, 0x14: 1, 0x16: 1, 0x1b: 1, 0x1c: 1, 0x1e: 1 };

	function u16(s, o) { return s.charCodeAt(o) | (s.charCodeAt(o + 1) << 8); }
	function u32(s, o) { return (s.charCodeAt(o) | (s.charCodeAt(o + 1) << 8) | (s.charCodeAt(o + 2) << 16) | (s.charCodeAt(o + 3) << 24)) >>> 0; }

	function read_at(f, pos, len)
	{
		f.position = pos;
		var s = f.read(len);
		return s === undefined || s === null ? "" : s;
	}

	/* Is this boot sector a FAT volume's?  Returns its geometry or null. */
	function parse_bpb(s)
	{
		if (s.length < 512)
			return null;
		var j = s.charCodeAt(0);
		if (j != 0xeb && j != 0xe9)
			return null;
		var bps = u16(s, 11), spc = s.charCodeAt(13), res = u16(s, 14), nfat = s.charCodeAt(16), nroot = u16(s, 17);
		var tot = u16(s, 19) || u32(s, 32);
		var fatsz = u16(s, 22) || u32(s, 36);
		if ([512, 1024, 2048, 4096].indexOf(bps) < 0 || spc < 1 || (spc & (spc - 1)) != 0 || res < 1 || nfat < 1 || nfat > 4 ||
		    !fatsz || !tot)
			return null;
		var rootsec = Math.floor((nroot * 32 + bps - 1) / bps);
		var datastart = res + nfat * fatsz + rootsec;
		if (tot <= datastart)
			return null;
		var clusters = Math.floor((tot - datastart) / spc);
		var v = { bps: bps, spc: spc, res: res, nfat: nfat, nroot: nroot, fatsz: fatsz, rootsec: rootsec, datastart: datastart,
			clusters: clusters, total: tot, root32: 0 };
		v.kind = clusters < 4085 ? 12 : clusters < 65525 ? 16 : 32;
		if (v.kind == 32)
			v.root32 = u32(s, 44);
		return v;
	}

	/* The FAT volumes on a disk image: [{ vol, start (byte offset), label }] */
	function volumes(f)
	{
		var out = [];
		var s0 = read_at(f, 0, 512);
		var v = parse_bpb(s0);
		if (v) {
			out.push({ vol: v, start: 0 });
			return out;
		}
		if (s0.length < 512 || s0.charCodeAt(510) != 0x55 || s0.charCodeAt(511) != 0xaa)
			return out;
		for (var i = 0; i < 4; i++) {
			var o = 446 + i * 16;
			var type = s0.charCodeAt(o + 4), lba = u32(s0, o + 8);
			if (!PARTITION_FAT[type] || !lba)
				continue;
			var pv = parse_bpb(read_at(f, lba * 512, 512));
			if (pv)
				out.push({ vol: pv, start: lba * pv.bps, part: i + 1 });
		}
		return out;
	}

	function open_disk(path)
	{
		var f = new File(path);
		if (!f.open("rb"))
			return null;
		return f;
	}

	function probe(path)
	{
		var f = open_disk(path);
		if (!f)
			return false;
		try {
			return volumes(f).length > 0;
		}
		finally {
			f.close();
		}
	}

	/* A reader for one volume */
	function Volume(f, info)
	{
		this.f = f;
		this.v = info.vol;
		this.base = info.start;
		this.fat = {};		/* FAT sectors read so far */
	}

	Volume.prototype.fat_entry = function (n)
	{
		var v = this.v;
		var off = v.kind == 12 ? n + (n >> 1) : v.kind == 16 ? n * 2 : n * 4;
		var sec = Math.floor(off / v.bps), within = off % v.bps;
		/* a FAT12 entry can run over the end of a sector: read two sectors */
		var key = sec;
		var s = this.fat[key];
		if (s === undefined) {
			s = read_at(this.f, this.base + (v.res + sec) * v.bps, v.bps * 2);
			this.fat[key] = s;
			if (this.fat_count === undefined)
				this.fat_count = 0;
			if (++this.fat_count > 4096) {		/* keep the cache small */
				this.fat = {};
				this.fat[key] = s;
				this.fat_count = 1;
			}
		}
		if (within + (v.kind == 12 ? 2 : v.kind == 16 ? 2 : 4) > s.length)
			return 0xfffffff;
		if (v.kind == 12) {
			var w = u16(s, within);
			return (n & 1) ? (w >> 4) : (w & 0xfff);
		}
		if (v.kind == 16)
			return u16(s, within);
		return u32(s, within) & 0x0fffffff;
	};

	Volume.prototype.is_end = function (c)
	{
		var v = this.v;
		return c < 2 || c >= v.clusters + 2 || c >= (v.kind == 12 ? 0xff7 : v.kind == 16 ? 0xfff7 : 0x0ffffff7);
	};

	Volume.prototype.cluster_offset = function (c)
	{
		var v = this.v;
		return this.base + (v.datastart + (c - 2) * v.spc) * v.bps;
	};

	/* The clusters of a chain, in order (stopping at a loop or a bad link) */
	Volume.prototype.chain = function (first)
	{
		var out = [];
		var seen = {};
		var c = first;
		while (!this.is_end(c) && !seen[c] && out.length <= this.v.clusters) {
			seen[c] = true;
			out.push(c);
			c = this.fat_entry(c);
		}
		return out;
	};

	/* The bytes of a directory (a start cluster of 0 means the fixed root of FAT12 and FAT16) */
	Volume.prototype.dir_bytes = function (cluster)
	{
		var v = this.v;
		if (cluster == 0 && v.kind != 32)
			return read_at(this.f, this.base + (v.res + v.nfat * v.fatsz) * v.bps, v.rootsec * v.bps);
		var chain = this.chain(cluster == 0 ? v.root32 : cluster);
		var parts = [];
		var size = v.spc * v.bps;
		for (var i = 0; i < chain.length && i < 4096; i++)
			parts.push(read_at(this.f, this.cluster_offset(chain[i]), size));
		return parts.join("");
	};

	function dos_date(d, t)
	{
		var year = 1980 + (d >> 9), mon = (d >> 5) & 15, day = d & 31;
		if (!d || mon < 1 || mon > 12 || day < 1)
			return 0;
		var date = new Date(year, mon - 1, day, t >> 11, (t >> 5) & 63, (t & 31) * 2);
		return Math.floor(date.getTime() / 1000);
	}

	/* A character of a long file name (UCS-2) as one the BBS can show (CP437), "?" if there is none */
	function lfn_char(u)
	{
		if (u < 128)
			return String.fromCharCode(u);
		if (typeof unicode_to_cp437 == "function") {
			var cp = unicode_to_cp437(u);
			if (cp >= 0)
				return String.fromCharCode(cp);
		}
		return "?";
	}

	/* A name that is safe to use as a file name on the BBS's own disk */
	function safe_name(n)
	{
		n = String(n).replace(/[\\\/:*?"<>|\x00-\x1f]/g, "_").replace(/^\.+$/, "_");
		return n.length ? n : "_";
	}

	/* The entries of a directory's bytes: [{ name, isdir, size, date, cluster }] */
	function parse_dir(bytes)
	{
		var out = [];
		var lfn = [];
		var lfn_sum = -1;
		for (var o = 0; o + 32 <= bytes.length; o += 32) {
			var b0 = bytes.charCodeAt(o);
			if (b0 == 0)
				break;
			var attr = bytes.charCodeAt(o + 11);
			if (b0 == 0xe5) {
				lfn = [];
				continue;
			}
			if (attr == 0x0f) {		/* part of a long name; the last part comes first */
				var seq = b0 & 0x1f;
				if (b0 & 0x40)
					lfn = [];
				var part = "";
				var offs = [1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30];
				for (var k = 0; k < offs.length; k++) {
					var u = u16(bytes, o + offs[k]);
					if (u == 0 || u == 0xffff)
						break;
					part += lfn_char(u);
				}
				lfn[seq] = part;
				lfn_sum = bytes.charCodeAt(o + 13);
				continue;
			}
			if (attr & 0x08) {		/* the volume's label */
				lfn = [];
				continue;
			}
			var nm = bytes.substr(o, 8).replace(/ +$/, "");
			var ex = bytes.substr(o + 8, 3).replace(/ +$/, "");
			if (nm.charCodeAt(0) == 5)
				nm = "\xe5" + nm.substr(1);
			var nt = bytes.charCodeAt(o + 12);
			if (nt & 0x08)
				nm = nm.toLowerCase();
			if (nt & 0x10)
				ex = ex.toLowerCase();
			var name = ex.length ? nm + "." + ex : nm;
			/* use the long name if its parts are all there and belong to this entry */
			if (lfn.length) {
				var sum = 0;
				for (var q = 0; q < 11; q++)
					sum = (((sum & 1) << 7) + (sum >> 1) + bytes.charCodeAt(o + q)) & 0xff;
				var whole = "";
				var ok = sum == lfn_sum;
				for (var p = 1; p < lfn.length && ok; p++) {
					if (lfn[p] === undefined)
						ok = false;
					else
						whole += lfn[p];
				}
				if (ok && whole.length)
					name = whole;
			}
			lfn = [];
			if (name == "." || name == "..")
				continue;
			out.push({ name: name, isdir: (attr & 0x10) != 0, size: u32(bytes, o + 28),
				date: dos_date(u16(bytes, o + 24), u16(bytes, o + 22)),
				cluster: u16(bytes, o + 26) | (u16(bytes, o + 20) << 16) });
		}
		return out;
	}

	/* Every file and folder of a volume, with names like "dir/sub/file" (prefix goes before each) */
	Volume.prototype.walk = function (prefix, out, state)
	{
		var self = this;
		function visit(cluster, path, depth)
		{
			var entries = parse_dir(self.dir_bytes(cluster));
			for (var i = 0; i < entries.length; i++) {
				if (out.length >= MAX_ENTRIES)
					return;
				var e = entries[i];
				var full = path + safe_name(e.name);
				out.push({ name: prefix + full + (e.isdir ? "/" : ""), isdir: e.isdir, size: e.isdir ? 0 : e.size, date: e.date, cluster: e.cluster });
				if (e.isdir && depth < MAX_DEPTH && e.cluster >= 2 && !state.seen[e.cluster]) {
					state.seen[e.cluster] = true;
					visit(e.cluster, full + "/", depth + 1);
				}
			}
		}
		visit(0, "", 0);
	};

	function list(path)
	{
		var f = open_disk(path);
		if (!f)
			throw "Unable to open " + file_getname(path);
		try {
			var vols = volumes(f);
			if (!vols.length)
				throw "Not a FAT disk image";
			var out = [];
			for (var i = 0; i < vols.length; i++) {
				var vol = new Volume(f, vols[i]);
				vol.walk(vols.length > 1 || vols[i].part ? "Partition " + (vols[i].part || 1) + "/" : "", out, { seen: {} });
				if (vols.length > 1 || vols[i].part)
					out.push({ name: "Partition " + (vols[i].part || 1) + "/", isdir: true, size: 0, date: 0, cluster: 0 });
			}
			return out.map(function (e) {
				return { name: e.name.replace(/\/$/, ""), isdir: e.isdir, size: e.size, date: e.date };
			});
		}
		finally {
			f.close();
		}
	}

	/* Copy the file inner (a name from list()) to out */
	function extract(path, inner, out)
	{
		var f = open_disk(path);
		if (!f)
			return false;
		var o = new File(out);
		var ok = false;
		try {
			var vols = volumes(f);
			var multi = vols.length > 1 || (vols.length == 1 && vols[0].part);
			for (var i = 0; i < vols.length && !ok; i++) {
				var parts = inner.split("/");
				if (multi) {
					if (parts[0] != "Partition " + (vols[i].part || 1))
						continue;
					parts.shift();
				}
				var vol = new Volume(f, vols[i]);
				var cluster = 0, ent = null;
				for (var d = 0; d < parts.length; d++) {
					ent = null;
					var es = parse_dir(vol.dir_bytes(cluster));
					for (var k = 0; k < es.length; k++) {
						if (safe_name(es[k].name) == parts[d]) {
							ent = es[k];
							break;
						}
					}
					if (!ent)
						break;
					cluster = ent.cluster;
				}
				if (!ent || ent.isdir)
					continue;
				if (!o.open("wb"))
					return false;
				var left = ent.size;
				var chain = ent.size ? vol.chain(ent.cluster) : [];
				var csize = vol.v.spc * vol.v.bps;
				for (var c = 0; c < chain.length && left > 0; c++) {
					var chunk = read_at(f, vol.cluster_offset(chain[c]), Math.min(csize, left));
					if (!chunk.length)
						break;
					o.write(chunk);
					left -= chunk.length;
				}
				o.close();
				ok = left <= 0;
			}
		}
		finally {
			f.close();
			if (o.is_open)
				o.close();
		}
		if (!ok)
			file_remove(out);
		return ok;
	}

	return { probe: probe, list: list, extract: extract, safe_name: safe_name };
})();
