(function() {

	var onLoad = function(files) {

		if(files.length < 1)
			return;

		var path = files[0].replace(file_getname(files[0]), "");

		if(!file_exists(path + "ansiview.ini"))
			return;
		var f = new File(path + "ansiview.ini");
		f.open("r");
		this.descriptions = f.iniGetObject("descriptions");
		f.close();

	}

	var onFile = function(file) {

		var fn = file_getname(file);

		if(typeof this.descriptions == "undefined")
			this.descriptions = {};

		var ret = format(
			"%-25s%s",
			file_isdir(file) ? ("[" + fn + "]") : fn,
			typeof this.descriptions[fn.toLowerCase()] == "undefined" ? "" : (" " + this.descriptions[fn.toLowerCase()])
		);
		return ret;

	}

	var args = JSON.parse(argv[0]);

	var hide = [".", "ansiview.ini", "ANSIVIEW.INI"];

	var fileBrowser = new FileBrowser(
		{	'path' : args.path,
			'top' : args.path,
			'frame' : browserFrame,
			'colors' : {
				'lfg' : args.colors.lfg,
				'lbg' : args.colors.lbg,
				'fg' : args.colors.fg,
				'bg' : args.colors.bg,
				'sfg' : args.colors.sfg,
				'sbg' : args.colors.sbg
			},
			'hide' : (typeof args.hide == "undefined") ? hide : hide.concat(args.hide.split(","))
		}
	);
	fileBrowser.on("load", onLoad);
	fileBrowser.on("file", onFile);
	fileBrowser.on("fileSelect", printFile);

	return fileBrowser;

})();