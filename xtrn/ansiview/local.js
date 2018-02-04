(function() {

	function onLoad(files) {

		if (files.length < 1) return;

		files = files.filter(
			function (file) {
				var ext = file_getext(file);
				if (typeof ext != "undefined" &&
					ext.toLowerCase() == ".zip" &&
					file_isdir(file.replace(ext, ""))
				) {
					return false;
				} else {
					return true;
				}
			}
		);

		var path = files[0].replace(file_getname(files[0]), "");

		if (file_exists(path + "ansiview.ini")) {
			var f = new File(path + "ansiview.ini");
			f.open("r");
			this.descriptions = f.iniGetObject("descriptions");
			f.close();
		}

		return files;

	}

	function onFile(file) {

		var fn = file_getname(file);
		if (typeof this.descriptions == "undefined") this.descriptions = {};
		var ret = format(
			"%-25s%s",
			file_isdir(file) ? ("[" + fn + "]") : fn,
			typeof this.descriptions[fn.toLowerCase()] == "undefined"
			? ""
			: (" " + this.descriptions[fn.toLowerCase()])
		);
		return ret;

	}

	function onSelect(file) {
		var ext = file_getext(file);
		if (typeof ext != "undefined" && ext.toLowerCase() == ".zip") {
			var destDir = file.replace(ext, "");
			if (!file_isdir(destDir)) {
				system.exec(
					system.exec_dir + 'unzip -o -qq "' + file + '" -d "' + destDir + '"'
				);
			}
			if (file_isdir(destDir)) this.path = destDir;
		} else {
			printFile(file);
		}
	}

	var args = JSON.parse(argv[0]);

	var hide = [".", "ansiview.ini", "ANSIVIEW.INI"];

	var fileBrowser = new FileBrowser(
		{	path : args.path,
			top : args.path,
			frame : browserFrame,
			colors : {
				lfg : args.colors.lfg,
				lbg : args.colors.lbg,
				fg : args.colors.fg,
				bg : args.colors.bg,
				sfg : args.colors.sfg,
				sbg : args.colors.sbg
			},
			hide : (
				typeof args.hide == "undefined")
				? hide
				: hide.concat(args.hide.split(",")
			),
            hide_regexp : (args.hide_regexp || null)
		}
	);
	fileBrowser.on("load", onLoad);
	fileBrowser.on("file", onFile);
	fileBrowser.on("fileSelect", onSelect);

	return fileBrowser;

})();
