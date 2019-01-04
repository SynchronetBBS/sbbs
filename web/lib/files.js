load('filebase.js');
load('file_size.js');

function count_files(dir) {
    var n = 0;
    const fn = format("%s%s.ixb", file_area.dir[dir].data_dir, dir);
    if (!file_exists(fn)) return n;
    return Math.floor(file_size(fn) / 22);
}

function listLibraries() {
	return file_area.lib_list.filter(function (library) {
        return library.dir_list.length >= 1;
    });
}

function listDirectories(library) {
	var dirs = [];
	file_area.lib_list[library].dir_list.forEach(function (dir) {
        const fc = count_files(dir.code);
        if (fc < 1) return;
		dirs.push({'dir' : dir, 'fileCount' : fc });
	});
	return dirs;
}

function listFiles(dir) {
	var fd = new FileBase(file_area.dir[dir].code);
	return fd.map(function (df) {
        df.size = df.path ? file_size_str(file_size(df.path)) : 'Unknown';
		return df;
	});
}
