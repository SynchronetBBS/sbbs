// SyncWall InterBBS Graffiti Wall
// by echicken -at- bbs.electronicchicken.com

load("ansiedit.js");
load("json-client.js");

js.branch_limit = 0; // Who wants to loop forever?

var chDelay = 15; // Milliseconds between characters when loading the buffer
var server = "bbs.electronicchicken.com"; // IP or hostname of the JSON server
var port = 10088; // Port that "server" is listening on

// Most people won't need to edit this file, especially not past this point

/*	'monthYear' will be the name of the property in the 'syncwall' JSON DB on
	'server' for this month's canvas. */
var monthYear = strftime("%m%y", time());

var ch; // Reused during the main loop to hold ansiEdit's return value
var lastUser = ""; // Updated only when the alias of whoever's drawing changes
var lastSystem = ""; // As 'lastUser', but for the BBS name instead
var userInput = ""; // Reused wherever user input is taken

// Possible bg attrs of the "who's drawing" bar
var bgColours = [ BG_BLUE, BG_CYAN, BG_GREEN, BG_MAGENTA, BG_RED, BG_BROWN ];

/*	This will be our JSON service client, connected to 'server' on 'port'.
	We'll exit quietly if the connection fails, otherwise we will proceed and
	assign 'drawstuff()' as the function to run when updates are received,
	then subscribe to this month's canvas property of the 'canvas' property in
	the 'syncwall' DB. */
var ansiClient = new JSONClient(server, port);
if(!ansiClient.socket.is_connected) exit();
ansiClient.callback = drawStuff;
ansiClient.subscribe("syncwall", "canvas." + monthYear);

/*	Establish parent frame 'frame' with subframes 'topBar' and 'bottomBar'.
	Frames make it nice and easy to output and organize things, and make it
	easy for us to update the status / "who's drawing" bar (bottomBar) later
	on. */
var frame = new Frame(1, 1, 80, 24, BG_BLACK);
var topBar = new Frame(1, 1, 80, 1, BG_BLUE|WHITE, frame);
var bottomBar = new Frame(1, 24, 80, 1, BG_BLUE|WHITE, frame);
topBar.putmsg(".: SyncWall InterBBS Graffiti Wall : Hit [ESC] to Exit :.");
topBar.gotoxy(69, 1);
topBar.putmsg("\1h\1bby echicken");
frame.open();
frame.cycle();

// Create ANSI Editor 'ansi' at x,y coordinates 1,2, 80 columns by 22 rows
var ansi = new ansiEdit(1, 2, 80, 22, BG_BLACK|LIGHTGRAY);

/*	We previously established that 'drawStuff' would be the callback function
	for our JSON client 'ansiClient', ie. the function that is called if we do
	'ansiClient.cycle()' and an update is returned. Now we create drawStuff(),
	which will act upon the update passed as 'update'. */
function drawStuff(update) {
	if(update.oper.toUpperCase() == "SUBSCRIBE" || update.oper.toUpperCase() == "UNSUBSCRIBE") return;
	var location = update.location.split(".").shift().toUpperCase();
	if(location != "CANVAS") return;
	putCh(update.data);
	return;
}

/*	Tell ANSI editor 'ansi' to draw character object 'ch';
	Update the "who's drawing" bar (bottomBar) if necessary.
	See the comments in ansiedit.js for details on a "character object". */
function putCh(ch) {
	ansi.putChar(ch);
	if(ch.userAlias != lastUser || ch.system != lastSystem) {
		var randomAttr = bgColours[Math.floor(Math.random()*bgColours.length)]|WHITE;
		bottomBar.attr = randomAttr;
		bottomBar.clear();
		bottomBar.putmsg(ch.userAlias + "@" + ch.system + " is drawing");
		bottomBar.cycle();	
		lastUser = ch.userAlias;
		lastSystem = ch.system;
	}
	return;
}

//	Call this function on exit to close our frames, ANSI editor & JSON client
function cleanUp() {
	frame.close();
	ansi.close();
	ansiClient.unsubscribe("syncwall", "canvas." + monthYear);
	ansiClient.disconnect();
	console.clear();
}

/*	Buffer in this month's entire canvas drawing history and push it into the
	ANSI editor one character at a time so that we can watch the progression
	from a blank slate to whatever it looks like now. */
/*	The canvas history is an array in the JSON DB (canvas[monthYear].history).
	We may not be able to read the entire array in one shot, so we will page
	through it instead, starting with elements 0 - 1000, 'index' & 'endex' */
var index = 0; 
var endex = 1000;
// Find the length of the history array so we'll know where to stop paging
var canvasLength = ansiClient.read("syncwall", "canvas." + monthYear + ".history.length", 1);
var canvas = []; // We'll push each partial array onto this one
if(canvasLength === undefined) {
	/*	There's no canvas history; we should create a property for this month
		and create its history array. */
	ansiClient.write("syncwall", "canvas." + monthYear, { history : [] }, 2);
} else {
	// Here's where we page through each 1000 elements in the history array
	while(index < canvasLength) {
		/* Use the JSON client's slice method to grab elements 'index' through
		'endex', then advance 'index' and 'endex' for the next pass through
		this loop. */
		canvas.push(ansiClient.slice("syncwall", "canvas." + monthYear + ".history", index, endex, 1));
		index = index + 1000;
		endex = index + 1000;
	}
	// Loop through each returned array of (up to) 1000 character objects
	for(var c = 0; c < canvas.length; c++) {
		for(var cc = 0; cc < canvas[c].length; cc++) {
			/*	Loop through an array of character objects, calling putCh() on
				each one, allowing the user to quit if they wish. */
			putCh(canvas[c][cc]);
			if(ascii(console.inkey(K_NONE, chDelay)) == 27) {
				cleanUp();
				exit();
			}
		}
	}
	console.gotoxy(80, 24);
}

// The main loop.  Exit when the user hits 'esc'.
while(ascii(userInput) != 27) {
	ansiClient.cycle(); // Check for updates to the JSON DB
	userInput = console.inkey(K_NONE, 5);
	if(userInput == "") continue;
	// If there was user input, pass it to the ANSI editor 'ansi'
	ch = ansi.cycle(userInput);
	if(!ch.ch) continue;
	/*	If a character was drawn, update bottomBar to reflect that this user
		is drawing, modify lastUser and lastSystem accordingly. */
	if(lastUser != user.alias || lastSystem != system.name) {
		bottomBar.attr = BG_BLUE|WHITE;
		bottomBar.clear();
		bottomBar.putmsg("You are drawing");
		bottomBar.cycle();
		lastUser = user.alias;
		lastSystem = system.name;
	}
	/*	Pass the character object 'ch' as returned from ANSI editor 'ansi' up
		to the JSON DB, with additional properties to show who drew it. */
	ch.userAlias = user.alias;
	ch.system = system.name;
	/*	Clients (like us) watch the 'ch' property of this month's canvas
		property for updates, which is constantly overwritten. Each character
		is also pushed onto the history array so that it can be buffered in
		when this script starts (see above.) */
	ansiClient.write("syncwall", "canvas." + monthYear + ".ch", ch, 2);
	ansiClient.push("syncwall", "canvas." + monthYear + ".history", ch, 2);
}

cleanUp(); // You disgusting slob.
