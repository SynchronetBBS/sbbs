// Replaces the old (v3.18 and earlier) hard-coded temp/archive file menu

require("sbbsdefs.js", 'UFLAG_D');
require("file_size.js", 'file_size_str');

"use strict";

var options = {};

if(user.security.restrictions & UFLAG_D) {
	console.putmsg(bbs.text(bbs.text.R_Download));
	exit();
}

function delfiles(dir, spec)
{
	if(typeof js.global.rmfiles == "function")
		return js.global.rmfiles(dir, spec);
	var count = 0;
	var list = directory(dir + spec);
	for(var i = 0; i < list.length; i++) {
		if(!file_remove(list[i]))
			throw new Error("Removing " + list[i]);
		count++;
	}
	return count;
}

function checkfname(spec)
{
	if(!spec)
		return false;
	if(spec.indexOf("..") >= 0)
		return false;
	return true;
}

function checkspace()
{
	var space = dir_freespace(system.temp_dir);

	if(space < file_area.min_diskspace) {
		console.putmsg(bbs.text(bbs.text.LowDiskSpace));
		log(LOG_ERR, format("Disk space is low: %s (%s bytes)", system.temp_dir, file_size_float(space, 1, 1)));
		return false;
	}
	console.putmsg(format(bbs.text(bbs.text.DiskNBytesFree), file_size_float(space, 1, 1)));
	return true;
}

function checktemp()
{
	var list = directory(system.temp_dir + "*");
	var files = 0;
	for(var i = 0; i < list.length; i++) {
		if(!file_isdir(list[i])) {
			files++;
			break;
		}
	}
	if(files < 1) {
		writeln("\r\nNo files in temp directory.");
		writeln("Use 'E' to extract from file or Create File List with 'N' or 'F' commands.");
		return false;
	}
	return true;
}

function cleanup(spec)
{
	console.print("Cleaning up temp dir ...");
	var result = delfiles(system.temp_dir, spec || "*");
	console.clearline();
	return result;
}

var temp_fname = system.temp_dir + system.qwk_id + "." + (user.temp_file_ext || "zip");
var file_fmt = "\x01c\x01h%-*s \x01n\x01c%10lu  \x01h\x01w%s";
var file = {};
var dir_code;

// Clear out the temp directory to begin with
cleanup();

menu:
while(bbs.online) {
	
	console.aborted = false;
	if(!(user.settings & USER_EXPERT))
		bbs.menu("tempxfer");
	console.putmsg(bbs.text(bbs.text.TempDirPrompt));
	var keys = "ACDEFNILQRVX?";
	switch(console.getkeys(keys, 0, K_UPPER)) {
		case 'A': // Add (legacy)
		case 'C': // Create temp archive
			if(!checktemp())
				break;
			if(!checkspace())
				continue;
			var spec = bbs.get_filespec();
			if(!checkfname(spec))
				break;
			try {
				console.newline();
				console.print("Archiving ...");
				var files = Archive(temp_fname).create(directory(system.temp_dir + spec));
				console.clearline();
				if (files)
					console.writeln(format("\r%u files archived into %s (%s bytes)"
						, files
						, file_getname(temp_fname)
						, file_size_float(file_size(temp_fname), 1, 1)));
				else
					console.writeln("No files matched/archived");
			} catch(e) {
				console.clearline();
				alert(log(LOG_INFO, e));
			}
			break;
		case 'D': // Download temp archive
			if(!checktemp())
				break;
			var fpath = temp_fname;
			if(options.download_archive_only) {
				if(!file_exists(fpath)) {
					console.putmsg(format(bbs.text(bbs.text.TempFileNotCreatedYet), file_getname(fpath)));
					break;
				}
			} else {
				console.putmsg(bbs.text(bbs.text.Filename));
				var fname = console.getstr(file_getname(temp_fname), 64, K_EDIT|K_AUTODEL);
				if(!checkfname(fname))
					break;
				fpath = system.temp_dir + fname;
				if(!file_exists(fpath)) {
					console.putmsg(format(bbs.text(bbs.text.FileDoesNotExist), fname));
					break;
				}
			}
			if(console.aborted) {
				console.aborted = false;
				break;
			}
			if(bbs.send_file(fpath, user.download_protocol) && dir_code)
				user.downloaded_file(dir_code, file.name, file_size(fpath));
			break;
		case 'I': // Information on extracted file
			if(!checktemp())
				break;
			console.putmsg(format(bbs.text(bbs.text.TempFileInfo), file.from, file.name));
			break;
		case 'L': // List files in temp dir
			if(!checktemp())
				break;
			var spec = bbs.get_filespec();
			if(!checkfname(spec))
				break;
			console.newline();
			var bytes = 0;
			var files = 0;
			var list = directory(system.temp_dir + spec);
			var longest = 0;
			for(var i = 0; i < list.length; i++)
				longest = Math.max(longest, file_getname(list[i]).length);
			for(var i = 0; i < list.length && !console.aborted; i++) {
				if(file_isdir(list[i]))
					continue;
				var size = file_size(list[i]);
				writeln(format(file_fmt
					,longest
					,file_getname(list[i])
					,size
					,system.timestr(file_date(list[i])).slice(4)
					));
				bytes += size;
				files++;
			}
			if(console.aborted) {
				console.aborted = false;
				console.putmsg(bbs.text(bbs.text.Aborted));
			}
			else if(!files)
				console.putmsg(bbs.text(bbs.text.EmptyDir));
			else if(files > 1)
				console.putmsg(format(bbs.text(bbs.text.TempDirTotal), file_size_str(bytes), files));
			break;
		case 'R': // Remove files from temp dir
			if(!checktemp())
				break;
			var spec = bbs.get_filespec();
			if(!checkfname(spec))
				break;
			console.putmsg(format(bbs.text(bbs.text.NFilesRemoved), cleanup(spec)));
			break;
		case 'V': // View files in temp dir
			if(!checktemp())
				break;
			var spec = bbs.get_filespec();
			if(!checkfname(spec))
				break;
			var list = directory(system.temp_dir + spec);
			for(var i = 0; i < list.length; i++) {
				writeln(file_getname(list[i]));
				bbs.view_file(list[i]);
			}
			break;
		case 'E': // Extract from file in file libraries
			if(!checkspace())
				continue;
			cleanup();
			console.putmsg(bbs.text(bbs.text.ExtractFrom));
			var fname = console.getstr(64, K_TRIM);
			if(!checkfname(fname)) {
				if(fname)
					console.putmsg(bbs.text(bbs.text.BadFilename));
				break;
			}
			console.putmsg(bbs.text(bbs.text.SearchingAllLibs));
			var found = false;
			for(var i in file_area.dir) {
				if(console.aborted)
					break;
				if(!file_area.dir[i].can_download)
					continue;
				var base = new FileBase(i);
				if(!base.open())
					continue;
				if(!base.get(fname)) {
					base.close();
					continue;
				}
				var path = base.get_path(fname);
				if(!path) {
					base.close();
					continue;
				}
				found = true;
				var count = 0;
				console.print("Extracting " + fname + " ...");
				try { 
					count = new Archive(path).extract(system.temp_dir);
					console.clearline();
				} catch(e) {
					console.clearline();
					alert(log(LOG_INFO, e));
					base.close();
					break;
				}
				base.close();
				if(count) {
					dir_code = i;
					writeln(format("Extracted %lu files from %s", count, fname));
					break;
				}
			}
			if(console.aborted) {
				console.aborted = false;
				console.putmsg(bbs.text(bbs.text.Aborted));
			} else {
				if(!found)
					console.putmsg(bbs.text(bbs.text.FileNotFound));
			}
			break;
		case 'X': // Extract from file in temp dir
			if(!checktemp())
				break;
			if(!checkspace())
				continue;
			console.putmsg(bbs.text(bbs.text.ExtractFrom));
			var fname = console.getstr(64, K_TRIM);
			if(!checkfname(fname)) {
				if(fname)
					console.putmsg(bbs.text(bbs.text.BadFilename));
				break;
			}
			var path = system.temp_dir + fname;
			var count = 0;
			console.writeln("Extracting " + fname + " ...");
			try { 
				count = new Archive(path).extract(system.temp_dir);
				console.clearline();
			} catch(e) {
				console.clearline();
				alert(log(LOG_INFO, e));
				break;
			}
			writeln(format("Extracted %lu files from %s", count, fname));
			break;
		case 'F': // Create list of all files
			cleanup();
			dir_code = undefined;
			bbs.export_filelist(file.name = "FILELIST.TXT");
			file.from = "File List";
			break;
		case 'N': // Create new file list
			cleanup();
			dir_code = undefined;
			bbs.export_filelist(file.name = "NEWFILES.TXT", FL_ULTIME);
			file.from = "File List";
			break;
		case '?':
			if(user.settings & USER_EXPERT)
				bbs.menu("tempxfer");
			break;
		default:
			break menu;
	}
}

// Clear out the temp directory when done
cleanup();
