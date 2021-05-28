// Replaces the old (v3.18 and earlier) hard-coded temp/archive file menu

require("sbbsdefs.js", 'UFLAG_D');
require("text.js", 'TempDirPrompt');
require("file_size.js", 'file_size_str');

if(user.security.restrictions & UFLAG_D) {
	console.putmsg(bbs.text(R_Download));
	exit();
}

function delfiles(dir, spec)
{
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
	var space = dir_freespace(system.temp_dir, 1024);
	
	if(space < file_area.min_diskspace) {
		console.putmsg(bbs.text(LowDiskSpace));
		log(LOG_ERR, format("Disk space is low: %s (%lu kilobytes)", system.temp_dir, space));
		return false;
	}
	console.putmsg(format(bbs.text(DiskNBytesFree), file_size_float(space, 1024, 1)));
	return true;
}

var temp_fname = system.temp_dir + system.qwk_id + "." + (user.temp_file_ext || "zip");
var file_fmt = "\x01c\x01h%-*s \x01n\x01c%10lu  \x01h\x01w%s";
var file = {};
var dir_code;

// Clear out the temp directory to begin with
delfiles(system.temp_dir, "*");

menu:
while(bbs.online && !console.aborted) {
	
	// Display TEXT\MENU\CHAT.* if not in expert mode
	if(!(user.settings & USER_EXPERT)) {
		bbs.menu("tempxfer");
	}
	
	console.putmsg(bbs.text(TempDirPrompt));
	var keys = "ADEFNILQRVX?";
	switch(console.getkeys(keys, K_UPPER)) {
		case 'A': // Add to temp archive
			if(!checkspace())
				continue;
			var spec = bbs.get_filespec();
			if(!checkfname(spec))
				break;
			try {
				print(Archive(temp_fname).create(directory(system.temp_dir + spec)) + " files archived");
			} catch(e) {
				log(LOG_INFO, e);
			}
			break;
		case 'D': // Download temp archive
			if(!file_exists(temp_fname)) {
				console.putmsg(format(bbs.text(TempFileNotCreatedYet), file_getname(temp_fname)));
				break;
			}
			if(bbs.send_file(temp_fname, user.download_protocol) && dir_code)
				user.downloaded_file(dir_code, file.name, file_size(temp_fname));
			break;
		case 'I': // Information on extracted file
			console.putmsg(format(bbs.text(TempFileInfo), file.from, file.name));
			break;
		case 'L': // List files in temp dir
			var spec = bbs.get_filespec();
			if(!checkfname(spec))
				break;
			var bytes = 0;
			var files = 0;
			var list = directory(system.temp_dir + spec);
			var longest = 0;
			for(var i = 0; i < list.length; i++)
				longest = Math.max(longest, file_getname(list[i]).length);
			for(var i = 0; i < list.length && !console.aborted; i++) {
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
			if(!files)
				console.putmsg(bbs.text(EmptyDir));
			else if(files > 1)
				console.print(format(bbs.text(TempDirTotal), file_size_str(bytes), files));
			console.aborted = false;
			break;
		case 'R': // Remove files from temp dir
			var spec = bbs.get_filespec();
			if(!checkfname(spec))
				break;
			console.putmsg(format(bbs.text(NFilesRemoved), delfiles(system.temp_dir, spec)));
			break;
		case 'V': // View files in temp dir
			var spec = bbs.get_filespec();
			if(!checkfname(spec))
				break;
			var list = directory(system.temp_dir + spec);
			for(var i = 0; i < list.length; i++) {
				writeln(file_getname(list[i]));
				bbs.view_file(list[i]);
			}
			console.aborted = false;
			break;
		case 'E': // Extract from file in file libraries
			if(!checkspace())
				continue;
			delfiles(system.temp_dir, "*");
			console.putmsg(bbs.text(ExtractFrom));
			var fname = console.getstr();
			if(!checkfname(fname)) {
				if(fname)
					console.putmsg(bbs.text(BadFilename));
				break;
			}
			var found = false;
			for(var i in file_area.dir) {
				if(!file_area.dir[i].can_download)
					continue;
				var base = new FileBase(i);
				if(!base.open())
					continue;
				file = base.get(fname);
				var path = base.get_path(fname);
				if(!path || !file) {
					base.close();
					continue;
				}
				found = true;
				var count = 0;
				try { 
					count = new Archive(path).extract(system.temp_dir);
				} catch(e) {
					log(LOG_INFO, e);
				}
				base.close();
				if(count) {
					dir_code = i;
					writeln(format("\r\nExtracted %lu files from %s", count, fname));
					break;
				}
			}
			if(!found)
				console.putmsg(bbs.text(FileNotFound));
			break;
		case 'X': // Extract from file in temp dir
			if(!checkspace())
				continue;
			console.putmsg(bbs.text(ExtractFrom));
			var fname = console.getstr();
			if(!checkfname(fname)) {
				if(fname)
					console.putmsg(bbs.text(BadFilename));
				break;
			}
			var path = system.temp_dir + fname;
			var count = 0;
			try { 
				count = new Archive(path).extract(system.temp_dir);
			} catch(e) {
				log(LOG_INFO, e);
				break;
			}
			writeln(format("\r\nExtracted %lu files from %s", count, fname));
			break;
		case 'F': // Create list of all files
			delfiles(system.temp_dir, "*");
			dir_code = undefined;
			bbs.export_filelist(file.name = "FILELIST.TXT");
			file.from = "File List";
			console.aborted = false;
			break;
		case 'N': // Create new file list
			delfiles(system.temp_dir, "*");
			dir_code = undefined;
			bbs.export_filelist(file.name = "NEWFILES.TXT", FL_ULTIME);
			file.from = "File List";
			console.aborted = false;
			break;
		case '?':
			if(user.settings & USER_EXPERT)
				bbs.menu("tempxfer");
			break;
		default:
			break menu;
	}
}
