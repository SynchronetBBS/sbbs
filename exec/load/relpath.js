// $Id: relpath.js,v 1.1 2020/04/26 04:41:58 rswindell Exp $

// Given 2 absolute directories: a working directory (cwd) and a target (dir),
// return the relative path to reach the target directory.
// The given directories do not have to actually exist.
// Returns a slash-separated path, terminated with a trailing slash.
function get(cwd, dir)
{
	var cwd = backslash(fullpath(cwd)).replace('\\', '/', 'g').split('/');
	var dir = backslash(fullpath(dir)).replace('\\', '/', 'g').split('/');
	var common;
	for(common = 0; common < cwd.length; common++) {
		if(cwd[common] != dir[common])
			break;
	}
	var result = [];
	if(common) {
		cwd.splice(0, common);
		dir.splice(0, common);
		while(result.length < cwd.length - 1)
			result.push("..");
	}
	return result.concat(dir).join('/');
}

this;
