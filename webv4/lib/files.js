require('filebase.js', 'FileBase');
require('file_size.js', 'file_size_str');

function count_files(dir) {
    const fn = format("%s%s.ixb", file_area.dir[dir].data_dir, dir);
    if (!file_exists(fn)) return 0;
    return Math.floor(file_size(fn) / 22); // ixb record length is 22 bytes
}

function libHasFiles(lib) {
    return lib.dir_list.some(function (e) {
        return count_files(e.code) > 0;
    });
}

function listLibraries() {
	return file_area.lib_list.filter(function (lib) {
        return lib.dir_list.length >= 1 && libHasFiles(lib);
    });
}

function listDirectories(lib) {
    return file_area.lib_list[lib].dir_list.reduce(function (a, c) {
        const fc = count_files(c.code);
        if (fc < 1) return a;
        a.push({ dir: c, fileCount: fc });
        return a;
    }, []);
}

function listFiles(dir) {
	return (new FileBase(file_area.dir[dir].code)).map(function (df) {
        df.size = df.path ? file_size_str(file_size(df.path)) : 'Unknown';
        df._size = df.path ? file_size(df.path) : 0;
		return df;
	});
}
