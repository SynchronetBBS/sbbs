(function() {

	load("http.js");

	var TheScene = function() {
		var apiUrl = "http://thescene.electronicchicken.com/api";
		this.list = function(path) {
			try {
				return JSON.parse(
					new HTTPRequest().Get(
						apiUrl + "/list/" + (typeof path == "undefined" ? "" : path)
					)
				);
			} catch(err) {
				return null;
				log(err);
			}
		}
		this.getANSI = function(path) {
			if(typeof path != "string")
				throw "TheScene.getANSI: Invalid 'path' argument: " + path;
			try {
				return (new HTTPRequest().Get(apiUrl + "/ansi/" + path));
			} catch(err) {
				return null;
				log(err);
			}
		}
	}

	var Browser = function(options) {

		var self = this;

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
			'cachePath' : root + ".cache/thescene/",
			'selectHook' : function(item) {}
		}

		var api = new TheScene();

		var errorItem = function() {
			properties.tree.addItem(
				"An error was encountered.  Press 'Q' to quit this module.",
				function() {}
			);
		}

		var initSettings = function() {

			if(typeof options.frame == "undefined")
				throw "TheScene Browser: No 'frame' argument provided.";
			else
				properties.parentFrame = options.frame;

			if(typeof options.colors != "object")
				throw "TheScene Browser: Invalid or no 'colors' argument provided.";
			else
				properties.colors = options.colors;

			if(	typeof options.selectHook != "undefined"
				&&
				typeof options.selectHook != "function"
			) {
				throw "TheScene Browser: 'selectHook' argument is not a function.";
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

		var initList = function() {
			
			if(properties.cache) {
				var cacheDir = properties.cachePath + backslash(decodeURIComponent(properties.path));
				if(!file_isdir(cacheDir))
					mkpath(cacheDir);
				if(	!file_exists(cacheDir + "list.json")
					||
					time() - file_date(cacheDir + "list.json") > properties.cachettl
				) {
					var list = api.list(properties.path);
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
				var list = api.list(properties.path);
				if(list === null) {
					errorItem();
					return;
				}
			}

			for(var l in list) {
				(function(item) {
					if(item.type == "file") {
						properties.tree.addItem(
							item.filename,
							function() {
								properties.index = properties.tree.index;
								if(properties.cache) {
									var cacheItem = properties.cachePath + decodeURIComponent(item.path);
									if(!file_exists(cacheItem)) {
										var ansi = api.getANSI(item.path);
										if(ansi === null)
											return;
										var f = new File(cacheItem);
										f.open("w");
										f.write(ansi);
										f.close();
									}
									properties.selectHook(cacheItem);
								} else {
									var tempItem = system.temp_dir + md5_calc(item.path, true);
									if(!file_exists(tempItem)) {
										var ansi = api.getANSI(item.path);
										if(ansi === null)
											return;
										var f = new File(tempItem);
										f.open("w");
										f.write(ansi);
										f.close();
									}
									properties.selectHook(tempItem);
								}
								self.refresh();
								properties.tree.index = properties.index;
								properties.tree.refresh();
							}
						);
					} else {
						properties.tree.addItem(
							"[" + item.filename + "]",
							function() {
								properties.path = item.path;
								self.refresh();
							}
						);
					}
				})(list[l]);
			}
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
				"Browsing: /" + decodeURIComponent(properties.path)
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
		{	'path' : "",
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