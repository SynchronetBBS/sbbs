/*
 * Pass a directory code and get back an array of objects for the files
 * in the specified directory.
 *
 * This is file is now just an unnecessary shim for the new FileBase class
 * and should be deprecated.
 *
 * Similar to filedir.js.
 *
 * Each element in the array is an object with the following properties:
 *
 * base:       Base filename.
 * ext:        Filename extension.
 * datoffset:  Offset in .dat file of file data. - N/A
 * uldate:     Date uploaded
 * dldate:     Date downloaded(?)
 * credits:    Number of credits(?)
 * desc:       Short description (58 chars max)
 * uploader:   Name of user that uploaded the file.
 * downloaded: Number of times the file has been downloaded.
 * opencount:  I have no idea what this is. - N/A
 * misc:       Misc flags... see FM_* below. - N/A
 * anonymous:  Anonymous upload
 * altpath:    Index into the internal array of alt paths (not visible to JavaScript) - N/A
 * extdesc:    May be undefined... extended description.
 * size:       Size in bytes: the file's actual size when the directory checks
 *             for files, otherwise the size recorded in the file base.
 * path:       Full path to file.  Undefined if the file doesn't exist or
 *             is in an alt path (since they're not visible to JS).
 */

function OldFileBase(dir) {
	var ret = [];
	var filebase = new FileBase(dir);
	if(!filebase.open())
		return ret;

	/* Build each path here rather than with filebase.get_path(), which looks
	   the file up by name (a linear index scan) and so made this function
	   O(n^2) in the directory's file count.  A file's path is its
	   directory's path plus its name; file_getcase() does the same
	   case-correcting existence check as get_path() and returns undefined
	   when the file is missing. */
	var dir_obj = file_area.dir[dir] || file_area.dir[String(dir).toLowerCase()];
	var file_list = filebase.get_list(FileBase.DETAIL.EXTENDED);
	for(var i = 0; i < file_list.length; i++) {
		var file = file_list[i];
		ret.push({
			name: file.name,
			uldate: file.added,
			dldate: file.last_downloaded,
			credits: file.cost,
			desc: file.desc,
			uploader: file.from,
			downloaded: file.times_downloaded,
			extdesc: file.extdesc,
			anonymous: file.anon,
			size: file.size,
			path: dir_obj ? file_getcase(dir_obj.path + file.name)
				: filebase.get_path(file.name)
		});
	}

	filebase.close();

	return ret;
}
