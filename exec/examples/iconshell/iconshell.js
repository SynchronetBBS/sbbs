/*
	Demo using frame.js to make a cursor-navigable user interface with icons.
	This is not a command shell (I'd put more thought into the planning and
	customizability of one) but shows some methods that could be used to create
	such a shell.
	Probably requires Synchronet 3.15.
	echicken@bbs.electronicchicken.com
	(For Lesser Keys re: 'bbs' thread in Synchronet Sysops on DoveNet, Sep 2013)
*/

load("sbbsdefs.js");
load("frame.js");

// If you want to edit the icon .bin files and modify their sizes, update these
var iconWidth = 5;
var iconHeight = 3;

// Declare some "global" variables that we'll be using later from within functions
// (This could lead to a whole conversation about variable scope in JS.)
var frame; // This will be our parent frame, containing all other frames
var header; // This will be a frame displaying some info text
var iconBox; // This will be a frame containing our icons
var cursor; // This frame will be slightly larger than an icon, and sit in the background
var output; // This will be a frame that we can push output to
var activeIcon; // We'll use this to track which icon is currently selected
var userInput; // We'll use this to store the most recent keypress
var icons = []; // This is an array; we'll store icon objects in here

// Create an icon object and add it to the icons array
// "name" is whatever we want, "filename" is the .bin graphic to load for the icon
var makeIcon = function(name, filename) {
	icons.push(
		{	'name' : name,
			// Frame instantiation: var f = new Frame(x, y, width, height, attributes, parentFrame)
			'frame' : new Frame(
				// Don't worry too much about this x, y position calculation for now
				(icons.length == 0) ? 3 : icons[icons.length - 1].frame.x + (iconWidth * 2),
				3,
				iconWidth, // This is the width we specified at the beginning
				iconHeight, // This is the height we specified at the beginning
				BG_BLACK|WHITE, // See sbbsdefs.js for colour attributes
				iconBox // We want iconBox to be the parent frame for all icon frames
			)
		}
	);
	// So, we've pushed an object into the icons array, and we know that it is
	// the last element of the array right now.  Therefore we can access it as
	// icons[icons.length - 1].  If we want to see its name, we can look at:
	// icons[icons.length - 1].name.  If we want to access its frame, we can act
	// upon icons[icons.length - 1].frame.
	icons[icons.length - 1].frame.load(js.exec_dir + filename, iconWidth, iconHeight);
}

// We'll call this function after receiving certain kinds of user input.
// Basically just sets the activeIcon (wrapping around as needed) and moves the
// cursor frame to the required position.
var navigate = function(direction) {
	switch(direction) {
		case "left":
			// This is a faster way of doing the following:
			// if(activeIcon == 0)
			// 	activeIcon = icons.length - 1;
			// else
			// 	activeIcon--;
			activeIcon = (activeIcon == 0) ? icons.length - 1 : activeIcon - 1;
			break;
		case "right":
			activeIcon = (activeIcon == icons.length - 1) ? 0 : activeIcon + 1;
			break;
		default:
			break;
	}
	// The "cursor" frame is hard coded to be four columns wider and two rows
	// taller than an icon.  We want to move it so that its top left corner is
	// two cells to the left of the icon and one cell above the icon.  The
	// Frame.moveTo method accepts new coordinates for the frame's top left
	// corner and then relocates the frame to that position.
	cursor.moveTo(icons[activeIcon].frame.x - 2, icons[activeIcon].frame.y - 1);
}

// We'll call this function before all others, in order to set things up
var init = function() {

	console.clear(); // Clear the terminal window
	activeIcon = 0; // Set a default "active icon", 0 being the first element in the icons array
	userInput = ""; // Set userInput to an empty string so that it's not 'undefined'

	// frame will fill the entire terminal window, and has no parent
	frame = new Frame(1, 1, console.screen_columns, console.screen_rows);
	// header will be at 1, 1, fill the width of the window, be one row high, and be a child of frame
	header = new Frame(1, 1, console.screen_columns, 1, BG_BLACK|WHITE, frame);
	// et cetera
	iconBox = new Frame(1, 2, console.screen_columns, iconHeight + 2, BG_BLACK|WHITE, frame);
	cursor = new Frame(1, 2, iconWidth + 4, iconBox.height, BG_CYAN, iconBox);
	output = new Frame(1, 7, console.screen_columns, 1, BG_BLACK|WHITE, frame);

	// Dump some text into the header frame
	header.putmsg("Press [enter] to select an item, hit [esc] to exit.");

	// Call that handy makeIcon function we made earlier, to load our icons
	makeIcon("Messages", "msg.bin");
	makeIcon("Files", "files.bin");
	makeIcon("Games", "games.bin");

	// Open frame and all of its children
	frame.open();
	// Frame.cycle ensures that the frames current contents are displayed in the user's terminal
	frame.cycle();

}

// This will be our main loop, accepting and reacting to user input
var main = function() {

	// While the user has not hit escape ...
	while(ascii(userInput) != 27) {

		// Call frame.cycle once each loop; don't worry about console.gotoxy for now
		if(frame.cycle())
			console.gotoxy(console.screen_columns, console.screen_rows);

		// Wait for a keypress from the user, store it in userInput when it arrives
		var userInput = console.getkey();

		// Evaluate the input we received from the user
		switch(userInput) {
			// If they hit the left arrow, call our navigate function with the appropriate argument
			case KEY_LEFT:
				navigate("left");
				break;
			// "" for a right arrow
			case KEY_RIGHT:
				navigate("right");
				break;
			// If they hit enter, just say which icon they selected
			case "\r":
				output.clear();
				output.putmsg("Selected: " + icons[activeIcon].name);
				break;
			// If input was none of the above, do nothing
			default:
				break;
		}

	}

}

// We'll call this when the script exits, just to tidy up
var cleanUp = function() {

	frame.close(); // Close the frame
	console.clear(); // Clear the terminal

}

init(); // Initialize
main(); // Run the main loop
cleanUp(); // Tidy up