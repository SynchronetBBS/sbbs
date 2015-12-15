load('filedir.js');
load('file_size.js');

function listLibraries() {
	var libraries = [];
	file_area.lib_list.forEach(
		function (library) {
			if (library.dir_list.length > 0) libraries.push(library);
		}
	);
	return libraries;
}

function listDirectories(library) {
	var dirs = [];
	file_area.lib_list[library].dir_list.forEach(
		function (dir) {
			var fd = new FileDir(dir);
			if (fd.files.length < 1) return;
			dirs.push({'dir' : dir, 'fileCount' : fd.files.length });
		}
	);
	return dirs;
}

function listFiles(dir) {
	var files = [];
	var fd = new FileDir(file_area.dir[dir]);
	fd.files.forEach(
		function (dirFile) {
			dirFile.size = file_size_str(file_size(dirFile.fullPath));
			files.push(dirFile);
		}
	);
	return files;
}