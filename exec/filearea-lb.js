// Lightbar file library/directory browser
load("sbbsdefs.js");
load("nodedefs.js");
load("cga_defs.js"); // For color definitions
load("file_size.js");
load("filedir.js");
load("frame.js");
load("tree.js");
load("scrollbar.js");

// File library index: bbs.curlib
// File directory index: bbs.curdir
// File directory internal code: bbs.curdir_code
var gStartupDirCode = "";
if (argv.length > 0)
	gStartupDirCode = argv[0];

// gFileCnf will contain the settings from file.cnf
var gFileCnf = null;

// See exec/load/cga_defs.js for color names
var colors = {
	'fg' : WHITE,      // Non-highlighted item foreground
	'bg' : BG_BLACK,   // Non-highlighted item background
	'lfg' : WHITE,     // Highlighted item foreground
	'lbg' : BG_CYAN,   // Highlighted item background
	'sfg' : WHITE,     // Path/status-bar foreground
	'sbg' : BG_BLUE,   // Path/status-bar background
	'hfg' : LIGHTGRAY, // Header foreground
	'hbg' : BG_BLACK   // Header background
};

var divider = ascii(179);

Frame.prototype.drawBorder = function(color) {
	var theColor = color;
	if(Array.isArray(color));
		var sectionLength = Math.round(this.width / color.length);
	this.pushxy();
	for(var y = 1; y <= this.height; y++) {
		for(var x = 1; x <= this.width; x++) {
			if(x > 1 && x < this.width && y > 1 && y < this.height)
				continue;
			var msg;
			this.gotoxy(x, y);
			if(y == 1 && x == 1)
				msg = ascii(218);
			else if(y == 1 && x == this.width)
				msg = ascii(191);
			else if(y == this.height && x == 1)
				msg = ascii(192);
			else if(y == this.height && x == this.width)
				msg = ascii(217);
			else if(x == 1 || x == this.width)
				msg = ascii(179);
			else
				msg = ascii(196);
			if(Array.isArray(color)) {
				if(x == 1)
					theColor = color[0];
				else if(x % sectionLength == 0 && x < this.width)
					theColor = color[x / sectionLength];
				else if(x == this.width)
					theColor = color[color.length - 1];
			}
			this.putmsg(msg, theColor);
		}
	}
	this.popxy();
}

var BROWSE_LIBS = 0,
	BROWSE_DIRS = 1,
	BROWSE_FILES = 2,
	BROWSE_BATCH = 3;

var state = {
	'chooser' : null,
	'lib' : 0,
	'dir' : "",
	'browse' : BROWSE_LIBS,
	'batch' : {}
};

var restore = {
	'sys_status' : bbs.sys_status,
	'attributes' : console.attributes
};

var frame;

var standardHelp = format(
	"B)atch DL %s Q)uit",
	divider, divider
);

var Batch = function(parentFrame) {

	var self = this,
		chooser = state.chooser,
		browse = state.browse;

	state.browse = BROWSE_BATCH;

	this.frame = new Frame(
		Math.ceil((console.screen_columns - 69) / 2),
		Math.ceil((console.screen_rows - 19) / 2),
		70,
		20,
		BG_BLACK|LIGHTGRAY,
		parentFrame
	);
	this.frame.drawBorder(colors.lfg); // meh
	this.frame.gotoxy(1, 2);
	this.frame.attr = colors.lbg|colors.lfg;
	this.frame.center("Batch Queue");
	this.frame.attr = BG_BLACK|LIGHTGRAY;

	var subFrame = new Frame(
		this.frame.x + 1,
		this.frame.y + 3,
		this.frame.width - 2,
		this.frame.height - 5,
		this.frame.attr,
		this.frame
	);

	var helpBar = new Frame(
		this.frame.x + 1,
		this.frame.y + this.frame.height - 2,
		this.frame.width - 2,
		1,
		colors.sbg|colors.sfg,
		this.frame
	);
	helpBar.putmsg("R)emove " + divider + " D)ownload " + divider + " G)o back");

	var scrollBar = new ScrollBar(subFrame);

	this.tree = null;
	var buildTree = function() {
		self.tree = new Tree(subFrame);
		for(var c in self.tree.colors) {
			if(typeof colors[c] != "undefined")
				self.tree.colors[c] = colors[c];
		}
		for(var b in state.batch) {
			var item = self.tree.addItem(
				format(
					"%-13s %7s %s",
					state.batch[b].name,
					file_size_str(file_size(state.batch[b].fullPath)),
					state.batch[b].description
				)
			);
			item.fileItem = state.batch[b];
		}
	}
	buildTree();

	this.frame.open();
	this.tree.open();

	var sendBatch = function() {
		var fn = system.temp_dir + format("%04d.dwn", user.number);
		var f = new File(fn);
		f.open("w");
		for(var b in state.batch) {
			var ifn = ixbFilenameFormat(state.batch[b].name);
			var ext = ifn.substr(8, 3);
			f.writeln(truncsp(ext) == "" ? ifn : (ifn.substr(0, 8) + "." + ext));
		}
		f.close();
		console.clear(BG_BLACK|LIGHTGRAY);
		bbs.batch_add_list(fn);
		bbs.batch_download();
		console.clear(BG_BLACK|LIGHTGRAY);
		frame.invalidate();
	}

	this.cycle = function() {
		scrollBar.cycle();
	}

	this.getcmd = function(cmd) {
		switch(cmd.toUpperCase()) {
			case "D":
				sendBatch();
				break;
			case "G":
				this.close();
				break;
			case "R":
				if(typeof this.tree.currentItem.fileItem != "undefined") {
					var key = md5_calc(this.tree.currentItem.fileItem.fullPath, true);
					delete state.batch[key];
					this.tree.close();
					buildTree();
					this.tree.open();
				}
				break;
			default:
				if(this.tree.items.length > 0)
					this.tree.getcmd(cmd);
				break;
		}
	}

	this.close = function() {
		this.tree.close();
		this.frame.close();
		this.frame.delete();
		state.chooser = chooser;
		state.browse = browse;
	}

}

var Details = function(parentFrame, dirFile) {

	var chooser = state.chooser;

	var frame = new Frame(
		Math.ceil((console.screen_columns - 74) / 2),
		Math.ceil((console.screen_rows - 15) / 2),
		73,
		16,
		BG_BLACK|LIGHTGRAY,
		parentFrame
	);
	frame.drawBorder(colors.lfg); // meh

	var subFrame = new Frame(
		frame.x + 1,
		frame.y + 1,
		frame.width - 2,
		frame.height - 2,
		frame.attr,
		frame
	);
	subFrame.word_wrap = true;

	var scrollBar = new ScrollBar(subFrame, { 'autohide' : true });

	subFrame.attr = colors.lbg|colors.lfg;
	subFrame.center(dirFile.name);
	subFrame.attr = BG_BLACK|LIGHTGRAY;
	subFrame.gotoxy(1, 3);
	subFrame.putmsg(
		"       Size: " + file_size_str(file_size(dirFile.fullPath)) + "\r\n" +
		"Uploaded by: " + dirFile.uploader + "\r\n" +
		"Uploaded on: " + strftime("%d/%m/%Y %H:%M") + "\r\n" +
		"  Downloads: " + dirFile.timesDownloaded + "\r\n" +
		"    Credits: " + dirFile.credit + "\r\n" +
		"Description: " + dirFile.description + "\r\n" +
		"   Extended: "
	);
	if(dirFile.extendedDescription == "") {
		subFrame.gotoxy(14, 9)
		subFrame.putmsg("Not available.");
	} else {
		subFrame.gotoxy(1, 11);
		subFrame.putmsg(dirFile.extendedDescription);
	}

	frame.open();

	this.cycle = function() {
		scrollBar.cycle();
	}

	this.getcmd = function(cmd) {
		if( (cmd == KEY_UP || cmd == KEY_DOWN)
			&&
			(subFrame.data_height > subFrame.height)
		) {
			subFrame.scroll(0, (cmd == KEY_UP) ? -1 : 1);
		} else if(cmd != "") {
			this.close();
		}
	}

	this.close = function() {
		frame.close();
		frame.delete();
		state.chooser = chooser;
	}

}

var Chooser = function(parentFrame, list, treeBuilder) {

	var self = this;

	this.frame = null;
	this.tree = null;

	var scrollBar;

	var initDisplay = function() {

		self.frame = new Frame(
			parentFrame.x,
			parentFrame.y,
			parentFrame.width,
			parentFrame.height - 1,
			parentFrame.attr,
			parentFrame
		);

		self.pathFrame = new Frame(
			self.frame.x,
			self.frame.y,
			self.frame.width,
			1,
			colors.sbg|colors.sfg,
			self.frame
		);

		self.headerFrame = new Frame(
			self.frame.x + 1,
			self.frame.y + 1,
			self.frame.width - 1,
			1,
			colors.hbg|colors.hfg,
			self.frame
		);

		self.treeFrame = new Frame(
			self.frame.x,
			self.frame.y + 2,
			self.frame.width,
			self.frame.height - 2,
			self.frame.attr,
			self.frame
		);

		self.footerFrame = new Frame(
			self.frame.x,
			self.frame.y + self.frame.height,
			self.frame.width,
			1,
			colors.sbg|colors.sfg,
			self.frame
		);

		self.tree = new Tree(self.treeFrame);
		for(c in self.tree.colors) {
			if(typeof colors[c] != "undefined")
				self.tree.colors[c] = colors[c];
		}

		list.forEach(function(item) { treeBuilder(self.tree, item); });

		scrollBar = new ScrollBar(self.tree, { 'autohide' : true });

		self.frame.open();
		self.tree.open();
		self.frame.draw();

	}

	this.open = function() {
		initDisplay();
	}

	this.getcmd = function(cmd) {
		if(this.tree.items.length > 0)
			this.tree.getcmd(cmd);
	}

	this.cycle = function() {
		scrollBar.cycle();
	}

	this.close = function() {
		this.tree.close();
		this.frame.close();
		this.frame.delete();
	}

}

var batchToggle = function() {
	var key = md5_calc(state.chooser.tree.currentItem.fileItem.fullPath, true);
	if(typeof state.batch[key] == "undefined") {
		state.chooser.tree.currentItem.text = state.chooser.tree.currentItem.text.replace(/^./, "*");
		state.batch[key] = state.chooser.tree.currentItem.fileItem;
	} else {
		state.chooser.tree.currentItem.text = state.chooser.tree.currentItem.text.replace(/^\*/, " ");
		delete state.batch[key];
	}
	state.chooser.tree.refresh();
}

var fileChooser = function(code) {

	var treeBuilder = function(tree, item) {

		var str = format(
			" %-13s%-7s%8s %s",
			item.name,
			file_size_str(file_size(item.fullPath)),
			system.datestr(item.uploadDate),
			item.description
		);

		var treeItem = tree.addItem(
			str,
			function() {
				state.chooser = new Details(frame, item);
			}
		);
		treeItem.fileItem = item;

		// Make the file list tree's 'select' key an I (for "file info")
		tree.__commands__.SELECT = "I";
	}

	var fd = new FileDir(file_area.dir[code]);

	state.chooser = new Chooser(
		frame,
		fd.files,
		treeBuilder
	);
	state.chooser.open();
	state.chooser.pathFrame.putmsg(
		format(
			" Browsing: File Libraries -> %s -> %s",
			file_area.dir[code].lib_name,
			file_area.dir[code].name
		)
	);
	state.chooser.headerFrame.putmsg(
		format(
			" %-13s%-7s%8s %s",
			"Filename", "Size", "Uploaded", "Description"
		)
	);
	state.chooser.footerFrame.putmsg(
		format("I)nfo %s T)ag %s V)iew %s D)load %s U)pload %s G)o back %s %s",
		       divider, divider, divider, divider, divider, divider, standardHelp)
	);
}

var dirChooser = function(index) {

	var treeBuilder = function(tree, item) {
		tree.addItem(
			format(
				"%-25s %-6s %s",
				item.name,
				directory(item.path + "*").length,
				item.description
			),
			function() {
				state.dir = item.code;
				state.browse = BROWSE_FILES;
				state.chooser.close();
				fileChooser(item.code);
			}
		);
	}

	state.chooser = new Chooser(
		frame,
		file_area.lib_list[index].dir_list,
		treeBuilder
	);
	state.chooser.open();
	state.chooser.pathFrame.putmsg(" Browsing: File Libraries -> " + file_area.lib_list[index].name);
	state.chooser.headerFrame.putmsg(format("%-25s %-6s Description", "Directory", "Files"));
	state.chooser.footerFrame.putmsg("G)o back " + divider + " " + standardHelp);
}

var libraryChooser = function() {

	var treeBuilder = function(tree, item) {
		tree.addItem(
			format(
				"%-25s %-6s %s",
				item.name,
				file_area.lib_list[item.index].dir_list.length,
				item.description
			),
			function() {
				state.lib = item.index;
				state.browse = BROWSE_DIRS;
				state.chooser.close();
				dirChooser(item.index);
			}
		);
	}

	state.chooser = new Chooser(
		frame,
		file_area.lib_list,
		treeBuilder
	);
	state.chooser.open();
	state.chooser.pathFrame.putmsg(" Browsing: File Libraries");
	state.chooser.headerFrame.putmsg(format("%-25s %-6s Description", "Library", "Dirs"));
	state.chooser.footerFrame.putmsg(standardHelp);

}

var canDownload = function() {
	if(	typeof state.chooser.tree.currentItem.fileItem != "undefined"
		&&
		file_area.dir[state.dir].can_download
		&&
		(	file_area.dir[state.dir].is_exempt
			||
			user.security.credits >= state.chooser.tree.currentItem.fileItem.credit
		)
	) {
		return true;
	}
	return false;
}

var inputHandler = function(userInput) {
	if(userInput == "")
		return;

	var ret = false;

	switch(userInput.toUpperCase()) {
		case "B": // Batch DL
			if(state.browse != BROWSE_BATCH) {
				ret = true;
				state.chooser = new Batch(state.chooser.frame);
			}
			break;
		case "D": // Download
			if(state.browse == BROWSE_FILES && canDownload()) {
				ret = true;
				console.clear(BG_BLACK|LIGHTGRAY);
				bbs.send_file(state.chooser.tree.currentItem.fileItem.fullPath);
				console.clear(BG_BLACK|LIGHTGRAY);
				frame.invalidate();
			}
			break;
		case "G": // Go back
			if(state.browse == BROWSE_FILES) {
				ret = true;
				state.browse = BROWSE_DIRS;
				state.chooser.close();
				dirChooser(state.lib);
			} else if(state.browse == BROWSE_DIRS) {
				ret = true;
				state.browse = BROWSE_LIBS;
				state.chooser.close();
				libraryChooser();
			}
			break;
		case "Q": // Quit
			if(state.browse != BROWSE_BATCH) {
				ret = true;
				cleanUp();
			}
			break;
		case "T": // Tag a file
			if(state.browse == BROWSE_FILES) {
				ret = true;
				if(canDownload())
					batchToggle();
			}
			break;
		case "U": // Upload
			if(state.browse == BROWSE_FILES && file_area.dir[state.dir].can_upload) {
				ret = true;
				console.clear(BG_BLACK|LIGHTGRAY);
				bbs.upload_file(state.dir);
				console.clear(BG_BLACK|LIGHTGRAY);
				frame.invalidate();
				fileChooser(state.dir);
			}
			break;
		case "V": // View file
			if(state.browse == BROWSE_FILES) {
				ret = true;
				// Populate gFileCnf if it hasn't been populated yet
				if (gFileCnf == null) {
					var cnflib = load({}, "cnflib.js");
					gFileCnf = cnflib.read("file.cnf");
				}
				if (gFileCnf) {
					// Get the filename extension
					var filenameExt = file_getext(state.chooser.tree.currentItem.fileItem.fullPath);
					if (typeof(filenameExt) == "string") {
						filenameExt = filenameExt.substr(1);
						// See if the filename extension is in the list of viewable file types
						var filenameExtUpper = filenameExt.toUpperCase();
						var fileViewCmd = "";
						for (var i = 0; i < gFileCnf.fview.length; ++i) {
							// Ensure the view command's ARS matches the user's access and
							// the file extension matches before using the view command
							if (user.compare_ars(gFileCnf.fview[i].ars) &&
							    (filenameExtUpper == gFileCnf.fview[i].extension.toUpperCase()))
							{
								// Get the file view command, and substitute %f or %s
								// with the filename.
								fileViewCmd = gFileCnf.fview[i].cmd;
								fileViewCmd = fileViewCmd.replace(/%f/i, state.chooser.tree.currentItem.fileItem.fullPath);
								fileViewCmd = fileViewCmd.replace(/%s/i, state.chooser.tree.currentItem.fileItem.fullPath);
								break;
							}
						}
						// View the file if we got a file view command, or an eror if not.
						console.print("\1n");
						console.crlf();
						if (fileViewCmd != "")
							bbs.exec(fileViewCmd);
						else {
							console.print("\1h\1yThat file is not viewable.\1n");
							mswait(2000);
						}
					}
				}
				else {
					console.print("\1n");
					console.crlf();
					console.print("\1y\1hUnable to read file.cnf!\1n");
					mswait(2000);
				}
				console.clear(BG_BLACK|LIGHTGRAY);
				frame.invalidate();
				fileChooser(state.dir);
			}
			break;
		default:
			break;
	}

	return ret;
}

var initDisplay = function() {

	console.clear(BG_BLACK|LIGHTGRAY);

	frame = new Frame(
		1,
		1,
		console.screen_columns,
		console.screen_rows,
		BG_BLACK|LIGHTGRAY
	);

	frame.open();
}

var init = function() {
	bbs.sys_status |= SS_MOFF;
	initDisplay();
	// If this script was given a directory code, and the code is valid, then
	// list the files in that directory.  Otherwise, start with the file library
	// chooser.
	if ((gStartupDirCode != "") && dirCodeIsValid(gStartupDirCode))
	{
		fileChooser(gStartupDirCode);
		state.dir = gStartupDirCode;
		state.lib = file_area.dir[gStartupDirCode].lib_index;
		state.browse = BROWSE_FILES;
	}
	else
		libraryChooser();
}

var main = function() {
	var previousNodeAction = bbs.node_action;
	bbs.node_action = NODE_LFIL;
	bbs.nodesync();
	while(!js.terminated) {
		var userInput = console.inkey(K_NONE, 5);
		if(!inputHandler(userInput)) {
			// A bit of a hack - If the user pressed "i" (lowercase I) while
			// browsing files, then make the user input a capital I so that
			// the "file info" command is not case-sensitive.
			if ((userInput == "i") && (state.browse == BROWSE_FILES))
				userInput = "I";
			state.chooser.getcmd(userInput);
			state.chooser.cycle();
		}
		if(frame.cycle())
			console.gotoxy(console.screen_columns, console.screen_rows);
	}
	bbs.node_action = previousNodeAction;
	bbs.nodesync();
}

var cleanUp = function() {
	state.chooser.close();
	frame.close();
	bbs.sys_status = restore.sys_status;
	console.attributes = restore.attributes;
	console.clear();
	exit(0);
}

// Returns whether or not an internal file directory code is valid.
function dirCodeIsValid(pDirCode) {
	if (typeof(pDirCode) != "string")
		return false;

	var dirCodeUpper = pDirCode.toUpperCase();
	var codeIsValid = false;
	for (var dirCode in file_area.dir)
	{
		if (dirCode.toUpperCase() == dirCodeUpper)
		{
			codeIsValid = true;
			break;
		}
	}
	return codeIsValid;
}

//////////////////////////////////////////////////
// Main script code

init();
main();
cleanUp();
