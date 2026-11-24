// Extended File Info loadable module: one file's extended information, as
// the built-in display shows it, with what a file search found highlighted
// in the file's short and extended descriptions
// for Synchronet v3.22+

// Set up in SCFG->System->Loadable Modules->Ext File Info: extfileinfo

// Arguments (from the Terminal Server):
//   <dir code> <filename> <1 to show the extended description, or 0>
//   [search expression, with any % and " encoded as %25 and %22]

// The highlight color is the List Files module's found_attr option (the
// [listfiles] sections of ctrl/modopts.ini), by default bright yellow.

"use strict";

require("sbbsdefs.js", "P_TRUNCATE");
load("listfiles_lib.js");
load("extfileinfo_lib.js");

// The found_attr option (Ctrl-A codes, or "") for directory 'dir'
function found_attr(dir)
{
	function section(name)
	{
		try {
			return load({}, "modopts.js", name);
		} catch (e) {
			return null;
		}
	}
	var opts = listfiles_options(listfiles_defaults()
		, section("listfiles")
		, section("listfiles:lib" + (dir.lib_number + 1))
		, section("listfiles:" + dir.code.toLowerCase()));
	return attr_option(opts.found_attr);
}

// 'text' with what the search found highlighted, in 'attr' (Ctrl-A codes)
// or, when that's empty, the color the text is in from 'start' (a number)
// on, brightened or dimmed
function highlight(text, terms, attr, start)
{
	if (!terms.length || !text)
		return text;
	return listfiles_highlight(text, terms, attr || start);
}

// As the built-in lister's time to download a file (at 'cps' characters per
// second): 0 for an empty or missing file
function time_to_download(size, cps)
{
	if (size < 1)
		return 0;
	if (size <= cps)
		return 1;
	if (cps < 1)
		cps = 100000;
	return Math.floor(size / cps);
}

function print_line(str)
{
	console.print(str, P_TRUNCATE);
}

// Print text.dat string 'id' with arguments 'args' (an array), as the
// built-in display does (see extfileinfo_text()), in print mode 'mode'
function print_text(id, args, mode)
{
	var text = extfileinfo_text(bbs.text(id), args || []);
	console.print(text.str, (mode === undefined ? P_TRUNCATE : mode) | (text.atcodes ? P_ATCODES : 0));
}

// The header fields, as the built-in display shows them when there's no
// fileinfo menu file
function show_header(dir, f, disk, terms, attr)
{
	// (a user-to-user transfer can be from a directory in a library the user
	// can't access: numbered past the user's last, as the built-in display
	// numbers it)
	var lib = file_area.lib[dir.lib_name];
	var lib_num = dir.lib_index >= 0 ? dir.lib_index + 1 : file_area.lib_list.length + 1;
	var dir_num = dir.index >= 0 ? dir.index + 1 : lib.dir_list.length + 1;
	print_text(bbs.text.FiLib, [lib_num, lib.description]);
	print_text(bbs.text.FiDir, [dir_num, dir.description]);
	print_text(bbs.text.FiFilename, [f.name]);
	if (disk.size >= 0)
		print_text(bbs.text.FiFileSize, [extfileinfo_commas(disk.size), extfileinfo_byte_estimate(disk.size)]);
	var cost = f.cost || 0;
	print_text(bbs.text.FiCredits, [((dir.settings & DIR_FREE) || (disk.size > 0 && cost <= 0))
			? bbs.text(bbs.text.FREE) : extfileinfo_commas(cost)]);
	if (disk.size > 0 && disk.size === disk.indexed_size) {
		if (f.crc32 !== undefined)
			print_text(bbs.text.FiChecksum, ["CRC-32", format("%08x", f.crc32)]);
		if (f.md5)
			print_text(bbs.text.FiChecksum, ["MD5", f.md5]);
		if (f.sha1)
			print_text(bbs.text.FiChecksum, ["SHA-1", f.sha1]);
	}
	if (f.desc) {
		// Its color before the description, as the terminal has it
		var fmt = bbs.text(bbs.text.FiDescription);
		var pos = fmt.indexOf("%");
		if (pos < 0)
			print_text(bbs.text.FiDescription);
		else {
			print_line(fmt.substr(0, pos));
			print_line(format(fmt.substr(pos), highlight(f.desc, terms, attr, console.attributes)));
		}
	}
	if (f.tags)
		print_text(bbs.text.FiTags, [f.tags]);
	if (f.author)
		print_text(bbs.text.FiAuthor, [f.author]);
	if (f.author_org)
		print_text(bbs.text.FiGroup, [f.author_org]);
	var from = f.anon ? bbs.text(bbs.text.UNKNOWN_USER) : f.from;
	if (from) {
		print_text(bbs.text.FiUploadedBy, [from]);
		if (f.from_protocol)
			print_line(format(" via %s ", f.from_protocol));
	}
	if (dir.is_operator) {
		var source = "";
		if (f.from_ip_addr)
			source += "[" + f.from_ip_addr + "] ";
		if (f.from_host_name)
			source += f.from_host_name + " ";
		if (source)
			print_text(bbs.text.FiUploadedBy, [source]);
	}
	if (f.to_list)
		print_text(bbs.text.FiUploadedTo, [f.to_list]);
	print_text(bbs.text.FiDateUled, [system.timestr(f.added || 0)]);
	if (disk.time > 0)
		print_text(bbs.text.FiFileDate, [system.timestr(disk.time)]);
	print_text(bbs.text.FiDateDled, [system.timestr(f.last_downloaded || 0)]);
	print_text(bbs.text.FiTimesDled, [f.times_downloaded || 0]);
	var seconds = time_to_download(disk.size, bbs.download_cps);
	if (seconds > 0)
		print_text(bbs.text.FiTransferTime, [system.secondstr(seconds, true), bbs.download_cps], P_NONE);
	print_text(bbs.text.FileHdrDescSeparator);
}

function main(args)
{
	var dir = file_area.dir[String(args[0] || "").toLowerCase()];
	if (!dir || args.length < 2)
		return 0;
	var fb = new FileBase(dir.code);
	if (!fb.open())
		return 0;
	var f = fb.get(args[1], FileBase.DETAIL.MAX);
	var disk = {};
	if (f) {
		// The file on disk, as the built-in display checks it (a file object
		// has the indexed values unless the directory checks the disk)
		disk.size = fb.get_size(f.name);
		disk.time = extfileinfo_file_time(fb.get_time(f.name));
		disk.indexed_size = fb.get(f.name, FileBase.DETAIL.MIN).size;
		disk.path = fb.get_path(f.name);
	}
	fb.close();
	if (!f)
		return 0;
	var terms = listfiles_search_terms(extfileinfo_decode_search(args.slice(3)));
	var attr = terms.length ? extfileinfo_found_attr(found_attr(dir)) : "";

	var header = !bbs.menu("fileinfo", P_NOCRLF | P_NOERROR);
	if (header)
		show_header(dir, f, disk, terms, attr);
	if (args[2] === "1" && f.extdesc) {
		var ext = f.extdesc.replace(/^[\r\n]+/, "").replace(/\s+$/, "");
		console.putmsg(highlight(ext, terms, attr, console.attributes), P_NOATCODES | P_CPM_EOF | P_AUTO_UTF8);
		console.newline();
	}
	if (header && disk.size === -1) {
		print_text(bbs.text.FileIsNotOnline, [f.name], P_NONE);
		if (user.is_sysop)
			console.print(disk.path + "\r\n");
	}
	return 0;
}

exit(main(argv));
