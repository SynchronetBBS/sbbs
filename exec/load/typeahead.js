/*	typeahead.js for Synchronet BBS 3.15+
	echicken -at- bbs.electronicchicken.com

	Provides the Typeahead object, a blocking or non-blocking string input
	prompt with optional autocomplete suggestions.  Somewhat inspired by
	Twitter's library of the same name.

	Usage:

		load('typeahead.js');
		var options = {
			// x-coordinate
			'x' : 0,
			// y-coordinate
			'y' : 0,
			// Length of input prompt
			'len' : 60,
			// Prompt (CTRL-A codes can be used)
			'prompt' : "Type something: ",
			// Input box foreground color
			'fg' : LIGHTGRAY,
			// Input box background color
			'bg' : BG_BLUE,
			// Autocomplete suggestion foreground color
			'sfg' : LIGHTGRAY,
			// Autocomplete suggestion background color
			'sbg' : BG_BLUE,
			// Highlighted autocomplete suggestion foreground color
			'hsfg' : WHITE,
			// Highlighted autocomplete suggestion background color
			'hsbg' : BG_CYAN,
			// Cursor character
			'cursor' : ascii(219),
			// Initial position of cursor within input string
			'position' : 0,
			// Default text of input string
			'text' : "",
			// An array of datasource functions (see below)
			'datasources' : [ myDataSource ],
			// Seconds of idle input to wait before calling datasources
			'delay' : 1
		};

		var typeahead = new Typeahead(options);

	Datasource functions:

		A datasource function will be called with one argument: the current
		string that is in the input box.  The datasource can examine that
		string, then return an array of strings - these being its autocomplete
		suggestions.  The datasource function must at minimum return an empty
		array.

	Examples:

		load('typeahead.js');

		// This will be our datasource

		var suggester = function(str) {

			var suggestions = [];

			var words = [
				"donuts",
				"coffee",
				"bacon",
				"waffles",
				"toast",
				"peanut butter",
				"buttered toast",
				"buttered waffles",
				"toasted donuts"
			];

			str = str.toUpperCase();
			suggestions = words.filter(
				function(word) {
					word = word.toUpperCase();
					if(word.search(str) > - 1)
						return true;
					if(str.search(word) > - 1)
						return true;
					return false;
				}
			);

			return suggestions;

		}

		// An example that blocks for input from the user

		console.clear(LIGHTGRAY);

		var typeahead = new Typeahead(
			{	'prompt' : "Type something: ",
				'datasources' : [suggester]
			}
		);

		var str = typeahead.getstr();
		console.putmsg(str);
		console.pause();

		// Now a non-blocking example inside of a parent frame

		console.clear(LIGHTGRAY);

		var frame = new Frame(
			1,
			1,
			console.screen_columns,
			console.screen_rows,
			LIGHTGRAY
		);
		frame.open();

		var typeahead = new Typeahead(
			{	'prompt' : "Type something: ",
				'len' : 60,
				'datasources' : [suggester],
				'frame' : frame
			}
		);

		var str = undefined;
		while(typeof str != "string") {
			var key = console.inkey(K_NONE, 5);
			str = typeahead.inkey(key);
			if(!str) {
				// Input wasn't handled by Typeahead
				// Do something else with it, if you want
			}
			typeahead.cycle();
			if(frame.cycle())
				typeahead.updateCursor();
		}

		typeahead.close();
		frame.close();

		console.clear(LIGHTGRAY);
		console.putmsg(str);
		console.pause();
*/

load('sbbsdefs.js');
load('frame.js');
load('tree.js');

var Typeahead = function(options) {

	var properties = {
		'x' : 0,
		'y' : 0,
		'len' : console.screen_columns,
		'prompt' : "",
		'fg' : LIGHTGRAY,
		'bg' : BG_BLUE,
		'sfg' : LIGHTGRAY,
		'sbg' : BG_BLUE,
		'hsfg' : WHITE,
		'hsbg' : BG_CYAN,
		'cursor' : ascii(219),
		'position' : 0,
		'text' : "",
		'datasources' : [],
		'delay' : 1,
		'lastKey' : system.timer,
		'suggested' : false,
		'attr' : console.attributes
	};

	var display = {
		'parentFrame' : undefined,
		'frame' : undefined,
		'inputFrame' : undefined,
		'cursor' : undefined,
		'treeFrame' : undefined,
		'tree' : undefined
	};

	var initSettings = function() {

		for(var p in properties) {
			if(p == "datasources")
				continue;
			if(typeof options[p] != typeof properties[p])
				continue;
			properties[p] = options[p];
		}

		if(properties.x == 0 || properties.y == 0) {
			var xy = console.getxy();
			properties.x = xy.x;
			properties.y = xy.y;
		}

		if(properties.len + properties.prompt.length > console.screen_columns)
			properties.len = console.screen_columns - properties.prompt.length;

		if(properties.text != "")
			properties.position = properties.text.length + 1;

		if(typeof options.datasources != "undefined" && Array.isArray(options.datasources)) {
			properties.datasources = options.datasources.filter(
				function(d) {
					return(typeof d == "function");
				}
			);	
		}

	}

	var initDisplay = function() {

		if(typeof options.frame != "undefined")
			display.parentFrame = options.frame;

		display.frame = new Frame(
			properties.x,
			properties.y,
			properties.prompt.length + properties.len,
			1,
			LIGHTGRAY,
			display.parentFrame
		);
		display.frame.checkbounds = false;
		display.frame.putmsg(properties.prompt);

		display.inputFrame = new Frame(
			display.frame.x + properties.prompt.length,
			display.frame.y,
			properties.len,
			1,
			properties.bg|properties.fg,
			display.frame
		);
		display.inputFrame.putmsg(properties.text);

		display.cursor = new Frame(
			display.inputFrame.x + properties.text.length,
			display.inputFrame.y,
			1,
			1,
			properties.bg|properties.fg,
			display.inputFrame
		);
		display.cursor.putmsg(properties.cursor);

		if(properties.datasources.length > 0) {
			display.treeFrame = new Frame(
				display.inputFrame.x,
				display.inputFrame.y + 1,
				display.inputFrame.width,
				console.screen_rows - display.inputFrame.y,
				properties.fg,
				display.frame
			);
			display.treeFrame.transparent = true;
		}

		display.frame.open();

	}

	var init = function() {
		initSettings();
		initDisplay();
	}

	var suggest = function() {

		var suggestions = [];
		properties.datasources.forEach(
			function(datasource) {
				suggestions = suggestions.concat(datasource(properties.text));
			}
		);

		if(typeof display.tree != "undefined") {
			display.tree.close();
			display.treeFrame.invalidate();
		}

		if(suggestions.length < 1) {
			display.tree = undefined;
			return;
		}

		display.tree = new Tree(display.treeFrame);
		display.tree.colors.fg = properties.sfg;
		display.tree.colors.bg = properties.sbg;
		display.tree.colors.lfg = properties.hsfg;
		display.tree.colors.lbg = properties.hsbg;

		display.tree.addItem("");
		suggestions.forEach(
			function(suggestion) {
				display.tree.addItem(suggestion);
			}
		);

		display.tree.open();

		properties.suggested = true;

	}

	this.inkey = function(key) {
		if(typeof key == "undefined" || key == "")
			return;
		var ret = true;
		var change = false;
		switch(key) {
			case '\x0c':	/* CTRL-L */
			case '\x00':	/* CTRL-@ (NULL) */
			case '\x03':	/* CTRL-C */
			case '\x04':	/* CTRL-D */
			case '\x0b':	/* CTRL-K */
			case '\x0e':	/* CTRL-N */
			case '\x0f':	/* CTRL-O */
			case '\x09': 	// TAB
			case '\x10':	/* CTRL-P */
			case '\x11':	/* CTRL-Q */
			case '\x12':	/* CTRL-R */
			case '\x13':	/* CTRL-S */
			case '\x14':	/* CTRL-T */
			case '\x15':	/* CTRL-U */
			case '\x16':	/* CTRL-V */
			case '\x17':	/* CTRL-W */
			case '\x18':	/* CTRL-X */
			case '\x19':	/* CTRL-Y */
			case '\x1a':	/* CTRL-Z */
			case '\x1c':	/* CTRL-\ */
			case '\x1f':	/* CTRL-_ */
				ret = false;
				break;
			case KEY_UP:
			case KEY_DOWN:
				if(typeof display.tree != "undefined")
					display.tree.getcmd(key);
				break;
			case KEY_HOME:
				properties.position = 0;
				break;
			case KEY_END:
				properties.position = display.inputFrame.x + properties.text.length;
				break;
			case KEY_LEFT:
				properties.position = (properties.position == 0) ? 0 : properties.position - 1;
				break;
			case KEY_RIGHT:
				properties.position = (properties.position >= properties.text.length) ? properties.text.length : properties.position + 1;
				break;
			case '\b':
				if(properties.position == 0)
					break;
				properties.text = properties.text.split("");
				properties.text.splice((properties.position - 1), 1);
				properties.text = properties.text.join("");
				properties.position--;
				change = true;
				break;
			case '\x7f':
				if(properties.position >= properties.text.length)
					break;
				properties.text = properties.text.split("");
				properties.text.splice((properties.position), 1);
				properties.text = properties.text.join("");
				change = true;
				break;
			case '\r':
			case '\n':
				if(typeof display.tree != "undefined" && display.tree.index > 0)
					ret = display.tree.currentItem.text;
				else
					ret = properties.text;
				break;
			default:
				key = strip_ctrl(key);
				if(properties.position != properties.text.length) {
					properties.text = properties.text.split("");
					properties.text.splice(properties.position, 0, key);
					properties.text = properties.text.join("");
				} else {
					properties.text += key;
				}
				properties.position++;
				change = true;
				break;
		}
		if(change) {
			display.inputFrame.clear();
			display.inputFrame.putmsg(properties.text);
			properties.lastKey = system.timer;
			properties.suggested = false;
		}
		return ret;
	}

	this.getstr = function() {
		var ret;
		while(typeof ret != "string") {
			ret = this.inkey(console.inkey(K_NONE, 5));
			this.cycle();
		}
		this.close(true);
		return ret;
	}

	this.cycle = function() {
		if(properties.text == "" && typeof display.tree != "undefined") {
			display.tree.close();
			display.tree = undefined;
			display.treeFrame.invalidate();
		} else if(
			properties.text != "" &&
			properties.datasources.length > 0 &&
			system.timer - properties.lastKey > properties.delay &&
			!properties.suggested
		) {
			suggest();
		}
		display.cursor.moveTo(display.inputFrame.x + properties.position, display.inputFrame.y);
		display.cursor[time() % 2 == 0 ? "top" : "bottom"]();
		if(typeof display.parentFrame == "undefined" && display.frame.cycle())
			this.updateCursor();
	}

	this.updateCursor = function() {
		console.gotoxy(display.inputFrame.x + properties.position, display.inputFrame.y);
	}

	this.close = function(cleanup) {
		if(typeof display.tree != "undefined")
			display.tree.close();
		display.frame.close();
		if(typeof cleanup == "boolean" && cleanup) {
			console.attributes = properties.attr;
			if(typeof display.tree != "undefined") {
				for(var i = 0; i < display.tree.items.length; i++) {
					console.crlf();
					console.clearline();
				}
			}
			console.gotoxy(1, display.frame.y + 1);
		}
		display.frame.delete();
	}

	init();

}