require('filebase.js', 'OldFileBase');
require('file_size.js', 'file_size_str');

function count_files(dir) {
    return file_area.dir[dir].files;
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
	return (new OldFileBase(file_area.dir[dir].code)).map(function (df) {
        df.size = df.path ? file_size_str(file_size(df.path)) : 'Unknown';
        df._size = df.path ? file_size(df.path) : 0;
		return df;
	});
}

// Where file is a FileBase file record
function getMimeType(file) {
    if (file.ext) {
        const f = new File(system.ctrl_dir + 'mime_types.ini');
        if (f.open('r')) {
            const mimes = f.iniGetObject();
            f.close();
            if (mimes[file.ext] !== undefined) return mimes[file.ext];
        }
    }
    return 'application/octet-stream';
}
