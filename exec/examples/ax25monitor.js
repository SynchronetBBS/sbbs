/*	ax25monitor.js
	echicken -at- bbs.electronicchicken.com (VE3XEC) */

/*	A simple AX.25 packet monitoring script.
	(Try using it to monitor APRS traffic on 144.390 MHz) */

load("sbbsdefs.js");
load("kissAX25lib.js");

/*	This script requires one argument, the name of a TNC that's defined in
	ctrl/kiss.ini. (Eg., if you have a PK88 TNC defined in your kiss.ini
	file, its section name might be [PK88], so pass "PK88" as an argument
	to this script. */
if(argc < 1) exit();

/*	This is where we load the section of kiss.ini that was specified as
	an argument. */
var f = new File(system.ctrl_dir + "kiss.ini");
f.open("r");
var k = f.iniGetObject(argv[0]);
f.close();

/*	Having loaded the TNC's configuration details from kiss.ini, we now create
	a kissTNC object based on that information: */
var tnc = new kissTNC(argv[0], user.comment, 0, k.serialPort, k.baudRate);

var kissFrame;
var msg;

console.clear();
while(!js.terminated) {

	// Hit [ESC] to exit :D :D :D
	if(ascii(console.inkey(K_NONE, 5)) == 27) break;

	/*	kissTNC.getKISSFrame() is kind of a misnomer.  While it does read a
		KISS frame from the TNC, what it actually returns is an AX.25 frame
		(less the start/stop flags and frame check sequence) in the form of
		an array of bytes, or false if there was no frame to be read. */
	kissFrame = tnc.getKISSFrame();
	if(!kissFrame) continue; // See what I said re: 'false' above.

	/*	So, if we've gotten this far, then we have an AX.25 frame to decode.
		We'll create a new (empty) ax25packet object, then disassemble the
		array that tnc.getKISSFrame() returned to us in order to populate
		the object with properties. */
	var p = new ax25packet();
	p.disassemble(kissFrame);

	/*	So now our ax25packet object 'p' has a bunch of properties.  We won't
		print all of them out since most of them won't be useful to you as a
		dev or as a user simply watching traffic.  Instead we'll just print
		the source and destination callsigns and SSIDs, plus the digipeater
		path taken by the packet (if any).  If the packet has an information
		field (ie. it is an I frame, a UI frame, or a U FRMR frame) we'll
		print that out as well. */
	msg = p.source.replace(/\s/, "");
	if(p.sourceSSID > 0) msg += "-" + p.sourceSSID;
	msg += " -> " + p.destination.replace(/\s/, "");
	if(p.destinationSSID > 0) msg += "-" + p.destinationSSID;
	if(p.repeaters[0] != 0) msg += ", repeated by: " + p.repeaters.join(", ");
	if(p.hasOwnProperty("information")) msg += "\r\n" + byteArrayToString(p.information);
	console.line_counter = 0;
	console.putmsg("\1h\1w" + msg + "\r\n\r\n");
}
