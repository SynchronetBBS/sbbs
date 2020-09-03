/*	FileBrowser object
	echicken -at- bbs.electronicchicken.com

	Options:

	var fileBrowser = new FileBrowser(
		{	'path' : String,
			'top' : String (optional),
			'frame' : Frame (optional),
			'hide' : Array (optional),
			'colors' : Object (optional)
		}
	);

	- 'path' is the directory to start in
		- If 'top' is supplied, 'path' must be below 'top' in the filesystem

	- 'top' is the top-level directory that the browser will not ascend past
		- If not supplied, 'top' will default to the same value as 'path'

	-	'frame' is an optional instance of Frame (see frame.js) to place the
		browser inside of
			- If 'frame' is omitted, the browser will fill the terminal window

	-	'hide' is an optional array of wildcard-pattern strings; files
		matching these patterns (eg. "*.secret") will be omitted from
		directory listings
	  		- These patterns are not case-sensitive
	  		- The * and ? wildcards are supported
	  		- Patterns are applied to the filenames rather than full paths

	- 'colors' is an optional object with the following optional properties:
		- 'fg' - foreground color of a non-highlit item
		- 'bg' - background color of a non-highlit item
		- 'lfg' - foreground color of a highlit item
		- 'lbg' - background color of a highlit item
		- 'sfg' - foreground color of the path bar ("Browsing: ...")
		- 'sbg' - background color of the path bar ("Browsing: ...")
		- See sbbsdefs.js for colors

	Properties:

	- 'path' 	-	The directory currently being browsed
				-	You can set this value on the fly to cause the browser to
					jump to another directory.  (The new path must be at or
					under the 'top' directory.)

	Events:

	-	'load' is an event that fires after the directory listing has been
		read but before the file list has been displayed.  Its callback
		function will be given one argument, an array of strings of the
		complete paths of all files and subdirectories within the current
		directory.  It must return this array - or a modified version
		thereof - once complete.

	-	'file' is an event that fires once for each file in the list, just
		prior to the file being displayed. Its callback function will receive
		a string argument, the complete path to the file or subdirectory.
		The return value of	this function will be the text displayed in the
		list for this file.
			-	The default callback function strips the path from the
				filename, and wraps subdirectories in [ and ]

	-	'fileSelect' is an event that fires when a file is selected.  Its
		callback function will be given one string argument: the full path to
		the file.
			-	The .refresh() method will be called after this event's callback
				function returns

	Methods:

	- 'open'
		- Fetch the file list and display the browser

	- 'on(event, callback)'
		-	Register a callback function for an event.  See 'Events' above for
			a list of events, and what arguments their callback functions will
			receive.
		-	You can use the 'this' keyword within any callback function to
			reference to the FileBrowser object that called the function.

	- 'getcmd(str)'
		- Handle user input string 'str'

	- 'cycle'
		- Update the scrollbar
		- Update the display (if 'options.frame' was not provided)
			-	If 'options.frame' was given, it is assumed that the parent
				script is calling Frame.cycle() at regular intervals on
				'options.frame' or its parent frame.

	- 'refresh'
		- Force a redraw of the browser

	- 'close'
		- Close / remove the browser from the display

	Example:

	// Hit 'Q' to quit

	load("sbbsdefs.js");
	load("filebrowser.js");

	var fb = new FileBrowser(
		{	'path' : system.text_dir + "menu",
			'top' : system.text_dir,
			'colors' : {
				'lfg' : WHITE,
				'lbg' : BG_CYAN
			}
		}
	);
	fb.on(
		"fileSelect",
		function(fn) {
			console.clear();
			console.printfile(fn);
			console.pause();
			console.clear();
		}
	)
	fb.open();

	while(!js.terminated) {
		var userInput = console.inkey(K_NONE, 5).toUpperCase();
		if(userInput == "Q")
			break;
		fb.getcmd(userInput);
		fb.cycle();
	}
*/

load("sbbsdefs.js");
load("frame.js");
load("tree.js");
load("scrollbar.js");

var FileBrowser = function (options) {

	var self = this;
	var delimiter = backslash(system.exec_dir).substr(-1, 1);

	var properties = {
		path : "",
		top : null,
		parentFrame : null,
		frame : null,
		pathFrame : null,
		tree : null,
		treeFrame : null,
		scrollBar : null,
		colors : {
			sfg : WHITE,
			sbg : BG_BLUE,
			fg : WHITE,
			bg : BG_BLACK,
			lfg : WHITE,
			lbg : BG_CYAN
		},
		hide : [],
        hide_regexp : null,
		index : 0
	};

	var callbacks = {
		load : function (list) { return list; }, // wat?
		file : function (str) {
			if (file_isdir(str)) return "[" + file_getname(str) + "]";
			return file_getname(str);
		},
		fileSelect : function() {}
	};

	function initOptions(options) {

		if (typeof options.path != "string" ||
			!file_isdir(options.path)
		) {
			throw "FileBrowser: invalid or no 'path' argument: " + options.path;
		}

		if (typeof options.frame != "undefined") {
			properties.parentFrame = options.frame;
		}

		if (!file_isdir(options.path)) {
			throw "FileBrowser: 'path' argument must be an existing directory.";
		} else {
			properties.path = options.path;
		}

		if (typeof options.top == "string" &&
			(!file_isdir(options.top) || options.top.length > options.path.length)
		) {
			throw "FileBrowser: 'top' argument must be an existing directory above or equal to 'path'.";
		} else if (typeof options.top == "string") {
			properties.top = options.top;
		} else {
			properties.top = options.path;
		}

		if (properties.path.substr(-1) != delimiter) properties.path += delimiter;

		if (properties.top.substr(-1) != delimiter) properties.top += delimiter;

		if (typeof options.colors == "object") {
			for (var color in options.colors) {
				properties.colors[color] = options.colors[color];
			}
		}

		if (typeof options.hide != "undefined") {
			if (!Array.isArray(options.hide)) {
				throw "FileBrowser: 'hide' argument must be an array of strings.";
			}
			properties.hide = options.hide;
		}

        if (typeof options.hide_regexp != 'undefined') {
            properties.hide_regexp = new RegExp(options.hide_regexp);
            log(options.hide_regexp);
        }

	}

	function initList() {

		properties.pathFrame.putmsg(
			"Browsing: " + properties.path.replace(properties.top, delimiter)
		);

		// Would rather use GLOB_APPEND here, but it crashes my BBS. :|
		var files = [].concat(
			directory(properties.path + "*", GLOB_ONLYDIR|GLOB_PERIOD),
			directory(properties.path + "*").filter(
				function (e) { return (!file_isdir(e)); }
			)
		);
		files = callbacks.load.apply(self, [files]);

		files.forEach(
			function (item) {
				var fileString = callbacks.file.apply(self, [item]);
				if (fileString == "[.]") return;
				if (item.replace(/\.\.$/, "") == properties.top) return;
				var fn = file_getname(item);
				for (var h = 0; h < properties.hide.length; h++) {
					if (!wildmatch(fn, properties.hide[h])) continue;
					return;
				}
                if (properties.hide_regexp !== null && fn.search(properties.hide_regexp) > -1) return;
				properties.tree.addItem(
					fileString,
					file_isdir(item)
					?
					(function () {
						if (item.match(/\.\.$/) !== null) {
							item = item.split(delimiter).slice(0, -2).join(delimiter);
						}
						properties.path = backslash(item);
						self.refresh();
					})
					:
					(function () {
						properties.index = properties.tree.index;
						callbacks.fileSelect.apply(self, [item]);
						self.refresh();
						properties.tree.index = properties.index;
						properties.tree.refresh();
					})
				);
			}
		);

		properties.tree.index = 0;


	}

	function initDisplay() {

		properties.frame = new Frame(
			(properties.parentFrame === null) ? 1 : properties.parentFrame.x,
			(properties.parentFrame === null) ? 1 : properties.parentFrame.y,
			(properties.parentFrame === null) ? console.screen_columns : properties.parentFrame.width,
			(properties.parentFrame === null) ? console.screen_rows : properties.parentFrame.height,
			BG_BLACK|LIGHTGRAY,
			(properties.parentFrame === null) ? undefined : properties.parentFrame
		);

		properties.pathFrame = new Frame(
			properties.frame.x,
			properties.frame.y,
			properties.frame.width,
			1,
			properties.colors.sbg|properties.colors.sfg,
			properties.frame
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
		for (var color in properties.colors) {
			properties.tree.colors[color] = properties.colors[color];
		}
		initList();
		properties.scrollBar = new ScrollBar(properties.tree);
        properties.tree.first_letter_search = true;
		properties.frame.open();
		properties.tree.open();

	}

	function init() {
		initOptions(options);
		initDisplay();
	}

	this.__defineGetter__("path", function() { return properties.path; });
	this.__defineSetter__(
		"path", function (path) {
			if (typeof path != "string") {
				throw "FileBrowser.path: Invalid or no path supplied.";
			}
			path = backslash(fullpath(path));
			if (path.length < properties.top.length ||
				path.substr(0, properties.top.length) != properties.top
			) {
				throw "FileBrowser.path: Path must not be outside of the top-level directory.";
			}
			properties.path = path;
			self.refresh();
		}
	);

	this.open = function () {
		init();
		properties.frame.draw();
	}

	this.on = function (event, callback) {
		if (typeof event != "string" || typeof callbacks[event] == undefined) {
			throw "FileBrowser.on: Invalid or no event specified.";
		}
		if (typeof callback != "function") {
			throw "FileBrowser.on: Invalid callback function.";
		}
		callbacks[event] = callback;
	}

	this.cycle = function() {
		if (properties.parentFrame === null && properties.frame.cycle()) {
			console.gotoxy(console.screen_columns, console.screen_rows);
		}
		properties.scrollBar.cycle();
	}

	this.getcmd = function (cmd) {
		properties.tree.getcmd(cmd);
	}

	this.refresh = function () {
		this.close();
		options =  {
			path : properties.path,
			top : properties.top,
			parentFrame : properties.parentFrame
		};
		this.open();
	}

	this.close = function () {
		properties.tree.close();
		properties.scrollBar.close();
		//properties.frame.close();
		properties.frame.delete();
	}

}
