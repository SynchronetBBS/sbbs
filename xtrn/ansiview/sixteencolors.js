(function() {

	load(root + "sixteencolors-api.js");

	var Browser = function(options) {

		var self = this;

		var badExtensions = [
			"exe",
			"com",
			"zip",
			"gif",
			"jpg",
			"png"
		];

		var properties = {
			'path' : "",
			'parentFrame' : null,
			'frame' : null,
			'pathFrame' : null,
			'tree' : null,
			'treeFrame' : null,
			'scrollbar' : null,
			'colors' : {
				'fg' : WHITE,
				'bg' : BG_BLACK,
				'lfg' : WHITE,
				'lbg' : BG_CYAN,
				'sfg' : WHITE,
				'sbg' : BG_BLUE
			},
			'index' : 0,
			'cache' : false,
			'cachettl' : 0,
			'cachePath' : root + ".cache/sixteencolors/",
			'selectHook' : function(item) {}
		}

		var api = new SixteenColors();

		var errorItem = function() {
			properties.tree.addItem(
				"An error was encountered.  Press 'Q' to quit this module.",
				function() {}
			);
		}

		var initSettings = function() {

			if(typeof options.frame == "undefined")
				throw "SixteenColors Browser: No 'frame' argument provided.";
			else
				properties.parentFrame = options.frame;

			if(typeof options.colors != "object")
				throw "SixteenColors Browser: Invalid or no 'colors' argument provided.";
			else
				properties.colors = options.colors;

			if(	typeof options.selectHook != "undefined"
				&&
				typeof options.selectHook != "function"
			) {
				throw "SixteenColors Browser: 'selectHook' argument is not a function.";
			} else {
				properties.selectHook = options.selectHook;
			}

			if(typeof options.path == "string")
				properties.path = options.path;

			if(typeof options.cache == "boolean")
				properties.cache = options.cache;

			if(typeof options.cachettl == "number")
				properties.cachettl = options.cachettl;

		}

		var initCache = function() {
			if(!properties.cache)
				return;
			if(!file_isdir(properties.cachePath))
				mkpath(properties.cachePath);
		}

		var listYears = function() {

			if(properties.cache) {
				var cacheDir = properties.cachePath + "years/";
				if(!file_isdir(cacheDir))
					mkpath(cacheDir);
				if(	!file_exists(cacheDir + "list.json")
					||
					time() - file_date(cacheDir + "list.json") > properties.cachettl
				) {
					var list = api.years();
					if(list === null) {
						errorItem();
						return;
					}
					var f = new File(cacheDir + "list.json");
					f.open("w");
					f.write(JSON.stringify(list));
					f.close();
				} else {
					var f = new File(cacheDir + "list.json");
					f.open("r");
					var list = JSON.parse(f.read());
					f.close();
				}
			} else {
				var list = api.years();
				if(list === null) {
					errorItem();
					return;
				}
			}

			list.forEach(
				function(year) {
					properties.tree.addItem(
						format("[%s]...%s packs", year.year, year.packs),
						function() {
							properties.path = "/years/" + year.year;
							self.refresh();
						}
					);
				}
			);

		}

		var listYear = function(year) {

			if(properties.cache) {
				var cacheDir = properties.cachePath + "years/" + year + "/";
				if(!file_isdir(cacheDir))
					mkpath(cacheDir);
				if(	!file_exists(cacheDir + "list.json")
					||
					time() - file_date(cacheDir + "list.json") > properties.cachettl
				) {
					var list = api.year(year);
					if(list === null) {
						errorItem();
						return;
					}
					var f = new File(cacheDir + "list.json");
					f.open("w");
					f.write(JSON.stringify(list));
					f.close();
				} else {
					var f = new File(cacheDir + "list.json");
					f.open("r");
					var list = JSON.parse(f.read());
					f.close();
				}
			} else {
				var list = api.year(year);
				if(list === null) {
					errorItem();
					return;
				}
			}

			properties.tree.addItem(
				"[..]",
				function() {
					properties.path = "/years";
					self.refresh();
				}
			);

			list.forEach(
				function(item) {
					properties.tree.addItem(
						format("[%s]", item.name),
						function() {
							properties.path = "/pack/" + item.name;
							self.refresh();
						}
					);
				}
			);

		}

		var listPack = function(pack) {

			if(properties.cache) {
				var cacheDir = properties.cachePath + "packs/" + pack + "/";
				if(!file_isdir(cacheDir))
					mkpath(cacheDir);
				if(	!file_exists(cacheDir + "pack.json")
					||
					time() - file_date(cacheDir + "pack.json") > properties.cachettl
				) {
					var pack = api.pack(pack);
					if(pack === null) {
						errorItem();
						return;
					}
					var f = new File(cacheDir + "pack.json");
					f.open("w");
					f.write(JSON.stringify(pack));
					f.close();
				} else {
					var f = new File(cacheDir + "pack.json");
					f.open("r");
					var pack = JSON.parse(f.read());
					f.close();
				}
			} else {
				var pack = api.pack(pack);
				if(pack === null) {
					errorItem();
					return;
				}
			}

			properties.tree.addItem(
				"[..]",
				function() {
					properties.path = "/years/" + pack.year;
					self.refresh();
				}
			);

			pack.files.forEach(
				function(item) {
					for(var b = 0; b < badExtensions.length; b++) {
						var re = new RegExp(badExtensions[b] + "$", "i");
						if(item.filename.match(re) !== null)
							return;
					}
					properties.tree.addItem(
						format("%s", item.filename),
						function() {
							properties.index = properties.tree.index;
							if(properties.cache) {
								if(!file_exists(cacheDir + item.filename)) {
									var ansi = api.getFile(item.file_location);
									if(ansi === null)
										return;
									var f = new File(cacheDir + item.filename);
									f.open("w");
									f.write(ansi);
									f.close();
								}
								properties.selectHook(cacheDir + item.filename);
							} else {
								if(!file_exists(system.temp_dir + item.filename)) {
									var ansi = api.getFile(item.file_location);
									if(ansi === null)
										return;
									var f = new File(system.temp_dir + item.filename);
									f.open("w");
									f.write(ansi);
									f.close();
								}
								properties.selectHook(system.temp_dir + item.filename);
							}
							self.refresh();
							properties.tree.index = properties.index;
							properties.tree.refresh();
						}
					);
				}
			);

		}

		var initList = function() {
			if(properties.path == "/years")
				listYears();
			else if(properties.path.match(/^\/years\/\d\d\d\d$/) !== null)
				listYear(properties.path.split("/")[2]);
			else if(properties.path.match(/^\/pack\/.*$/) !== null)
				listPack(properties.path.split("/")[2]);
		}

		var initDisplay = function() {

			properties.frame = new Frame(
				properties.parentFrame.x,
				properties.parentFrame.y,
				properties.parentFrame.width,
				properties.parentFrame.height,
				BG_BLACK|LIGHTGRAY,
				properties.parentFrame
			);

			properties.pathFrame = new Frame(
				properties.frame.x,
				properties.frame.y,
				properties.frame.width,
				1,
				properties.colors.sbg|properties.colors.sfg,
				properties.frame
			);
			properties.pathFrame.putmsg(
				"Browsing: " + decodeURIComponent(properties.path)
			);

			properties.treeFrame = new Frame(
				properties.frame.x,
				properties.frame.y + 1,
				properties.frame.width,
				properties.frame.height - 1,
				properties.frame.attr,
				properties.frame
			);

			properties.tree = new Tree(properties.treeFrame);
			for(var color in properties.colors)
				properties.tree.colors[color] = properties.colors[color];
			initList();
			properties.scrollBar = new ScrollBar(properties.tree);
			properties.frame.open();
			properties.tree.open();

		}

		var init = function() {
			initSettings();
			initCache();
			initDisplay();
		}

		this.open = function() {
			init();
			properties.frame.draw();
		}

		this.cycle = function() {
			properties.scrollBar.cycle();
		}

		this.getcmd = function(cmd) {
			properties.tree.getcmd(cmd);
		}

		this.refresh = function() {
			this.close();
			options.path = properties.path;
			this.open();
		}

		this.close = function() {
			properties.tree.close();
			properties.scrollBar.close();
			properties.frame.close();
		}

	}

	var args = JSON.parse(argv[0]);
	return new Browser(
		{	'path' : "/years",
			'frame' : browserFrame,
			'selectHook' : printFile,
			'colors' : {
				'fg' : args.colors.fg,
				'bg' : args.colors.bg,
				'lfg' : args.colors.lfg,
				'lbg' : args.colors.lbg,
				'sfg' : args.colors.sfg,
				'sbg' : args.colors.sbg
			},
			'cache' : (typeof args.cache == "undefined") ? true : args.cache,
			'cachettl' : parseInt(args.cachettl)
		}
	);

})();