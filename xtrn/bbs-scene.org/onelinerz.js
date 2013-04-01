// bbs-scene.org onelinerz for Synchronet 3.16+
// echicken -at- bbs.electronicchicken.com

load("sbbsdefs.js");
load("frame.js");
load("funclib.js");
load("inputline.js");
load(js.exec_dir + "client.js");

if(!http)
	exit;

var frame = new Frame(1, 1, console.screen_columns, console.screen_rows, BG_BLACK|WHITE);
var headerFrame = new Frame(1, 1, console.screen_columns, 7, BG_BLACK|LIGHTGRAY, frame);
var onelinerzFrame = new Frame(1, 8, console.screen_columns, console.screen_rows - 9, BG_BLACK|LIGHTGRAY, frame);
var helpFrame = new Frame(1, console.screen_rows - 1, console.screen_columns, 1, BG_BLACK|LIGHTGRAY, frame);
var inputNickFrame = new Frame(1, console.screen_rows, 23, 1, BG_BLACK|LIGHTGRAY, frame);
var inputFrame = new Frame(25, console.screen_rows, console.screen_columns - 25, 1, BG_BLACK|LIGHTGRAY, frame);
var inputLine = new InputLine(inputFrame);
var hideInputFrame = new Frame(1, console.screen_rows, console.screen_columns, 1, BG_BLACK|LIGHTGRAY, frame);

inputLine.max_buffer = 55;
inputLine.auto_clear = false;
inputLine.show_cursor = true;
inputLine.cursor_char = "\xB1";
inputLine.cursor_attr = BG_LIGHTGRAY|WHITE;
inputFrame.bottom();

function getOneLinerz() {

	onelinerzFrame.gotoxy(1, ((console.screen_rows - 9) / 2).toFixed(0));
	onelinerzFrame.center("\1H\1W\1ILoading onelinerz from bbs-scene.org...\1N");
	frame.cycle();

	try {
		var response = http.Get("http://bbs-scene.org/api/onelinerz.php?limit=" + onelinerzFrame.height + "&ansi=1");
	}
	catch(err) {
		log(LOG_INFO, "bbs-scene onelinerz http error: " + err);
		return false;
	}

	try {
		response = response.replace(/^<\?xml\s+version\s*=\s*(["'])[^\1]+\1[^?]*\?>/, "");
		var onelinerz = new XML(response);
	}
	catch(err) {
		log(LOG_INFO, "bbs-scene onelinerz XML error: " + err);
		return false;
	}

	onelinerzFrame.clear();
	for(var o in onelinerz) {
		var userAtBBS = "\1h\1k(\1h\1w" + onelinerz[o].alias.substr(0, 13) + "\1h\1c@\1n\1w" + onelinerz[o].bbsname.substr(0, 7).toLowerCase() + "\1h\1k)";
		while(console.strlen(userAtBBS) < 23) {
			userAtBBS = "\1h\1k." + userAtBBS;
		}
		onelinerzFrame.putmsg(userAtBBS);
		onelinerzFrame.putmsg(" \1n\1w" + pipeToCtrlA(onelinerz[o].oneliner.substr(0, console.screen_columns - 24)) + "\r\n");
	}	
	return true;

}

function postOneLiner() {

	var userInput = undefined;

	helpFrame.invalidate();
	helpFrame.putmsg(line);
	helpFrame.gotoxy(50, 1);
	helpFrame.putmsg(ascii(180) + "\1h\1w[\1nTAB\1h]\1n to change colors" + ascii(195));

	var userAtBBS = "\1h\1k(\1h\1w" + user.alias.substr(0, 13) + "\1h\1c@\1n\1w" + system.qwk_id.toLowerCase() + "\1h\1k)";
	while(console.strlen(userAtBBS) < 23) {
		userAtBBS = "\1h\1k." + userAtBBS;
	}
	inputNickFrame.putmsg(userAtBBS);
	hideInputFrame.bottom();

	while(userInput === undefined) {
		userInput = inputLine.getkey();
		if(frame.cycle())
			console.gotoxy(80, 24);
		if(ascii(userInput) == 9) {
			inputLine.frame.attr = colorPicker(20, 10, inputFrame);
			userInput = undefined;
			continue;
		}
	}

	if(!userInput || userInput.replace(/\s*/g, "").length < 1)
		return false;

	var fgmask = (1<<0)|(1<<1)|(1<<2)|(1<<3);
	var bgmask = (1<<4)|(1<<5)|(1<<6);
	var fgattr = LIGHTGRAY;
	var bgattr = BLACK;
	var piped = "";
	for(var c = 0; c < userInput.length; c++) {
		var x = inputLine.frame.getData(c, 0, false);
		if((x.attr&fgmask) != fgattr) {
			fgattr = (x.attr&fgmask);
			piped += "|" + format("%02s", fgattr);
		}
		if(((x.attr&bgmask)>>4) != bgattr) {
			bgattr = ((x.attr&bgmask)>>4);
			piped += "|" + (bgattr + 16);
		}
		piped += userInput[c];
	}
	try {
		var response = http.Post("http://bbs-scene.org/api/onelinerz.json", "&bbsname=" + system.qwk_id.toLowerCase() + "&alias=" + user.alias.replace(/\s/g, "+") + "&oneliner=" + piped.replace(/\s/g, "+") + "  ", "", "");
	}
	catch(err) {
		log(LOG_INFO, "bbs-scene onelinerz http error: " + err);
		return false;
	}

	response = JSON.parse(response);
	inputFrame.clear();
	hideInputFrame.top();
	if(!response || !response.success)
		hideInputFrame.center("\1h\1rThere was an error and your oneliner was not posted. Try again later.");
	else
		hideInputFrame.center("\1h\1wYour oneliner has been posted. \1h\1c[\1h\1wPress any key to continue\1h\1c]");
	frame.cycle();
	console.getkey();
}

var line = "";
for(var l = 1; l <= frame.width; l++)
	line += ascii(196);

console.clear();
frame.open();
headerFrame.load(js.exec_dir + "onelinerz.bin", 80, 6);
headerFrame.gotoxy(1, 7);
headerFrame.putmsg(line, LIGHTGRAY);
helpFrame.putmsg(line);

if(!getOneLinerz())
	exit();

frame.cycle();
console.gotoxy(((console.screen_columns - 48) / 2).toFixed(0), console.screen_rows - 1);
if(!console.noyes("Post a new oneliner to bbs-scene.org"))
	postOneLiner();
frame.close();