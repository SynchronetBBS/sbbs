(function () {

    load('http.js');

	function ECBBS() {
		const url = 'https://bbs.electronicchicken.com/api/ansi-gallery.ssjs';
		this.list = function (path) {
            if (typeof path !== 'string') path = '';
			try {
                var resp = (new HTTPRequest()).Get(format('%s?path=%s', url, encodeURIComponent(path)));
				return JSON.parse(resp);
			} catch (err) {
                log(err);
				return null;
			}
		}
		this.get_file = function (path) {
			if (typeof path != "string") return null;
			try {
                var http = new HTTPRequest();
				return base64_decode((new HTTPRequest()).Get(format('%s?path=%s', url, encodeURIComponent(path))));
			} catch(err) {
                log(err);
				return null;
			}
		}
	}

    function Browser(options) {

		var self = this;

		var properties = {
			path : '/',
			parentFrame : null,
			frame : null,
			pathFrame : null,
			tree : null,
			treeFrame : null,
			scrollbar : null,
			colors : {
				fg : WHITE,
				bg : BG_BLACK,
				lfg : WHITE,
				lbg : BG_CYAN,
				sfg : WHITE,
				sbg : BG_BLUE
			},
			index : 0,
			cache : false,
			cachettl : 0,
			cachePath : js.exec_dir + '.cache/ecbbs/',
			selectHook : function (item) { }
		}

		var api = new ECBBS();

		function error_item() {
			properties.tree.addItem(
				'An error was encountered.  Press \'Q\' to quit this module.',
				function () { }
			);
		}

		function init_settings() {
            properties.selectHook = options.selectHook;
            properties.parentFrame = browserFrame;
			if (typeof options.path == 'string') properties.path = options.path;
			if (typeof options.cache == 'boolean') properties.cache = options.cache;
			if (typeof options.cachettl == 'number') properties.cachettl = options.cachettl;
		}

		function init_cache() {
			if (!properties.cache) return;
			if (!file_isdir(properties.cachePath)) mkpath(properties.cachePath);
		}

		function init_list() {

			if (properties.cache) {
				var cache_dir = properties.cachePath + backslash(decodeURIComponent(properties.path));
				if (!file_isdir(cache_dir)) mkpath(cache_dir);
				if (!file_exists(cache_dir + 'list.json')
                    || time() - file_date(cache_dir + 'list.json') > properties.cachettl
				) {
					var list = api.list(properties.path);
					if (list === null) {
						error_item();
						return;
					}
					var f = new File(cache_dir + 'list.json');
					f.open('w');
					f.write(JSON.stringify(list));
					f.close();
				} else {
					var f = new File(cache_dir + 'list.json');
					f.open('r');
					var list = JSON.parse(f.read());
					f.close();
				}
			} else {
				var list = api.list(properties.path);
				if (list === null) {
					error_item();
					return;
				}
			}

            if (properties.path !== '' && properties.path !== '/') {
                properties.tree.addItem('[..]', function () {
                    var path = properties.path.split('/');
                    properties.path = '/' + path.slice(1, path.length - 1).join('/');
                    log(properties.path);
                    self.refresh();
                });
            }

            list.forEach(function (e) {
                if (e.type == 'F') { // File
                    properties.tree.addItem(file_getname(e.path), function () {
                        properties.index = properties.tree.index;
                        if (properties.cache) {
                            var cache_file = properties.cachePath + decodeURIComponent(e.path);
                            if (!file_exists(cache_file)) {
                                var data = api.get_file(e.path);
                                if (data !== null) {
                                    var f = new File(cache_file);
                                    if (f.open('wb')) {
                                        f.write(data);
                                        f.close();
                                    }
                                }
                            }
                            properties.selectHook(cache_file);
                        } else {
                            var temp_file = system.temp_dir + md5_calc(e.path, true);
                            if (!file_exists(temp_file)) {
                                var data = api.get_file(e.path);
                                if (data !== null) {
                                    var f = new File(temp_file);
                                    f.open('wb');
                                    f.write(data);
                                    f.close();
                                }
                            }
                            properties.selectHook(temp_file);
                        }
                        self.refresh();
                        properties.tree.index = properties.index;
                        properties.tree.refresh();
                    });
                } else if (e.type == 'D') { // Directory
                    properties.tree.addItem('[' + e.path.split('/').slice(-2).shift() + ']', function () {
                        properties.path = e.path.replace(/\/$/, '');
                        self.refresh();
                    });
                }
            });

		}

		function init_display() {

			properties.frame = new Frame(
				properties.parentFrame.x, properties.parentFrame.y,
				properties.parentFrame.width, properties.parentFrame.height,
				BG_BLACK|LIGHTGRAY,
                properties.parentFrame
			);

			properties.pathFrame = new Frame(
				properties.frame.x, properties.frame.y,
				properties.frame.width, 1,
				properties.colors.sbg|properties.colors.sfg,
				properties.frame
			);
			properties.pathFrame.putmsg('Browsing: ' + decodeURIComponent(properties.path).replace(/\/$/, ''));

			properties.treeFrame = new Frame(
				properties.frame.x, properties.frame.y + 1,
				properties.frame.width, properties.frame.height - 1,
				properties.frame.attr,
				properties.frame
			);

			properties.tree = new Tree(properties.treeFrame);
			for (var color in properties.colors) {
				properties.tree.colors[color] = properties.colors[color];
            }

			init_list();

            properties.scrollBar = new ScrollBar(properties.tree);

            properties.frame.open();
			properties.tree.open();

		}

		this.open = function () {
            init_settings();
			init_cache();
			init_display();
			properties.frame.draw();
		}

		this.cycle = function () {
			properties.scrollBar.cycle();
		}

		this.getcmd = function (cmd) {
			properties.tree.getcmd(cmd);
		}

		this.refresh = function () {
			this.close();
			options.path = properties.path;
			this.open();
		}

		this.close = function () {
			properties.tree.close();
			properties.scrollBar.close();
			properties.frame.close();
		}

	}

    var args = JSON.parse(argv[0]);
	return new Browser({
        path : '',
		frame : browserFrame,
		selectHook : printFile,
		colors : {
			fg : args.colors.fg,
			bg : args.colors.bg,
			lfg : args.colors.lfg,
			lbg : args.colors.lbg,
			sfg : args.colors.sfg,
			sbg : args.colors.sbg
		},
		cache : typeof args.cache == 'undefined' ? true : args.cache,
		cachettl : parseInt(args.cachettl)
	});

})();
