load('filebase.js');
load('file_size.js');

function listLibraries() {
	return file_area.lib_list.filter(
		function (library) { return library.dir_list.length > 0; }
	);
}

function listDirectories(library) {
	var dirs = [];
	file_area.lib_list[library].dir_list.forEach(
		function (dir) {
			var fd = new FileBase(dir.code);
			if (fd.length < 1) return;
			dirs.push({'dir' : dir, 'fileCount' : fd.length });
		}
	);
	return dirs;
}

function listFiles(dir) {
	var fd = new FileBase(file_area.dir[dir].code);
	var files = fd.map(
		function (df) {
			if (typeof df.path !== 'undefined') {
				df.size = file_size_str(file_size(df.path));
			} else {
				df.size = 'Unknown';
			}
			return df;
		}
	);
	return files;
}