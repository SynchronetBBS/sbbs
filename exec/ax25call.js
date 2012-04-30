/*
	ax25call.js - Connect to a remote AX.25 station via a KISS-mode TNC.
	echicken -at- bbs.electronicchicken.com (VE3XEC)

	This script takes three arguments: TNC, callsign, and SSID.
	
	'TNC' is the name of a section from ctrl/kiss.ini, the TNC you wish to
	use for this call. 'callsign' and 'SSID' are the callsign and SSID of the
	station you wish to call.
	
	This script assumes that the user's comment field is populated with their
	callsign.  The sysop should take steps to verify that the user is a
	licensed amateur radio operator before allowing them to run this script.
*/

if(argc < 3) exit();

load("frame.js");
load("inputline.js");
load("kissAX25lib.js");

var f = new File(system.ctrl_dir + "kiss.ini");
f.open("r");
var k = f.iniGetObject(argv[0]);
f.close();
var tnc = new kissTNC(argv[0], user.comment, 0, k.serialPort, k.baudRate);
var c = new ax25Client(argv[1], argv[2], tnc.callsign, tnc.ssid, tnc);

var frame = new Frame(1, 1, 80, 24, BG_BLACK|LIGHTGRAY);
var buffer = new Frame(1, 2, 80, 22, BG_BLACK|LIGHTGRAY, frame);
var inputPrompt = new Frame(1, 24, 7, 1, BG_BLUE|WHITE, frame);
var inputBox = new Frame(8, 24, 73, 1, BG_BLUE|WHITE, frame);
var statusBar = new Frame(1, 1, 62, 1, BG_BLUE|WHITE, frame);
var quitBar = new Frame(63, 1, 18, 1, BG_BLUE|WHITE, frame);
buffer.checkbounds = false;
buffer.v_scroll = true;
var inputLine = new InputLine(inputBox);
frame.open();
inputPrompt.putmsg("Input: ");
quitBar.putmsg("Type /QUIT to quit");
statusBar.putmsg(argv[1] + "-" + argv[2] + " de " + c.kissTNC.callsign + "-" + c.kissTNC.ssid + ", Connecting...");
frame.cycle();

c.connect();

function main() {
	var userInput;
	var ret;
	while(!js.terminated && c.connected) {
		if(frame.cycle()) console.gotoxy(80, 24);
		userInput = inputLine.getkey();
		if(userInput !== undefined) {
			switch(userInput.toUpperCase()) {
				case "/QUIT":
					c.disconnect();
					break;
				default:
					c.send(stringToByteArray(userInput));
					buffer.putmsg(userInput + "\r\n");
					break;
			}
		}
		ret = c.receive();
		if(!ret) continue;
		buffer.putmsg("\1h\1w" + byteArrayToString(ret) + "\r\n");
	}
}

if(c.connected) {
	statusBar.clear();
	statusBar.putmsg(argv[1] + "-" + argv[2] + " de " + c.kissTNC.callsign + "-" + c.kissTNC.ssid + ", Connected");
	main();
} else {
	// Failure message here
}
frame.close();
console.clear();
