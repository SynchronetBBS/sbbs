// Extract the archive content listings of a directory and save them to each file's auxdata property

"use strict";

var dir_code = argv[0];

function inventory_archives(dir_code, filespec, hash) {
	var filebase = new FileBase(dir_code);
	if(!filebase.open()) {
		alert(filebase.error);
		return false;
	}
	
	var list = filebase.get_list(filespec, FileBase.DETAIL.AUXDATA );
	for(var i in list) {
		var file = list[i];
		if(file.auxdata !== undefined) {
			print(file.vpath + " already has auxdata");
			continue;
		}
		var auxdata = {};
		try {
			auxdata.archive_contents = Archive(filebase.get_path(file)).list(hash);
		} catch(e) {
			print(file.vpath + " is not a supported archive");
			continue;
		}
		file.auxdata = JSON.stringify(auxdata);
		if(!filebase.update(file.name, file, /* use_diz_always: */false, /* readd_always: */true))
			alert("Error updating " + file.vpath);
		print(file.vpath + " auxdata updated");
	}
	filebase.close();
	return true;
}

inventory_archives(dir_code, "*", argv.indexOf('-hash') >= 0);
