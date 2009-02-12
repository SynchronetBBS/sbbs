load("sbbsdefs.js");

var orig_pt=console.ctrlkey_passthru;
/* Disable parsing */
console.ctrlkey_passthru="+@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_";

function test_ctrl(results)
{
	var key;
	var tests=0;

	var ctrl_keys={
		"NUL":{
			char:"\x00",
			desc:"Zero width",
			type:"control",
			printable:true,
		},
		"SOL":{
			char:"\x01",
			desc:"Empty smiling face",
			type:"control",
			printable:true,
		},
		"STX":{
			char:"\x02",
			desc:"Filled smiling face",
			type:"control",
			printable:true,
		},
		"ETX":{
			char:"\x03",
			desc:"Heart",
			type:"control",
			printable:true,
		},
		"EOT":{
			char:"\x04",
			desc:"Diamond",
			type:"control",
			printable:true,
		},
		"ENQ":{
			char:"\x05",
			desc:"Club",
			type:"control",
			printable:true,
			test:function(results) {
				var i;
				var retstr='';
				var tmp;

				for(i=0; i<3; i++) {
					tmp=console.inkey(K_NONE, 1000);
					console.write(".");
					if(tmp!='') {
						retstr += tmp;
						i=0;
					}
				}
				console.crlf();
				if(retstr != '') {
					console.writeln('FAILED!  Returned "'+retstr+'"');
					return(false);
				}
				console.writeln("Passed");
				return(true);
			},
		},
		"ACK":{
			char:"\x06",
			desc:"Spade",
			type:"control",
			printable:true,
		},
		"BEL":{
			char:"\x07",
			desc:"Beeps",
			type:"control",
			test:function(results) {
				console.write(this.char);
				return(console.yesno("Did your terminal beep"));
			},
		},
		"BS":{
			char:"\x08",
			desc:"Backspace",
			type:"control",
			test:function(results) {
				console.writeln("Backspace Test");
				console.writeln("Back     \b\b\b\b\bspace Test");
				console.writeln("Back      Test\b\b\b\b\b\b\b\b\b\bspace");
				console.writeln("\b\b\bBackspace Test");
				return(console.yesno("Are the previous four lines identical"));
			}
		},
		"TAB":{
			char:"\x09",
			desc:"TAB",
			type:"control",
			test:function(results) {
				var lines=0;
				console.writeln("|      ||      |||     ||||    |||||   ||||||  ||||||| ||||||||        |");
				lines++
				console.writeln("|\t||\t|||\t||||\t|||||\t||||||\t|||||||\t||||||||\t|\t\t|\t||\t|||\t||||\t|||||\t||||||\t|||||||\t||||||||\t|");
				lines+=2;
				if(results["BS"]) {
					console.writeln("|\t||\t|||\t||||\t|||||\t||||||\t|||||||\t||||||||\b\t\t|");
					lines++;
					console.writeln("|\t||\t|||\t||||\t|||||\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\t\t\t\t\t||||||\t|||||||\t||||||||\t|");
					lines++;
				}
				return(console.yesno("Do the previous "+lines+" lines line up"));
			}
		},
		"LF":{
			char:"\x0a",
			desc:"Linefeed",
			type:"control",
			test:function(results) {
				if(results["BS"]) {
					console.write("Line Feed Test\b\b\b\b\b\n Test\b\b\b\b\b\b\b\b\b\b\b\b\b\bLine Feed\r\n");
					return(console.yesno("Are the previous two lines identical"));
				}
				else {
					console.writeln("First Half\nSecond Half");
					console.writeln("First Half");
					console.writeln("          Second Half");
					return(console.yesno("Are the previous four lines two identical pairs of lines"));
				}
			}
		},
		"VT":{
			char:"\x0b",
			desc:"Male Sign",
			type:"control",
			printable:true,
		},
		"FF":{
			char:"\x0c",
			desc:"Clears Screen",
			type:"control",
			test:function(results) {
				console.line_counter=0;
				console.write(this.char);
				return(console.yesno("Is this the only thing on the screen and in the top left corner"));
			}
		},
		"CR":{
			char:"\x0d",
			desc:"Carriage Return",
			type:"control",
			test:function(results) {
				var lines=2;
			
				console.writeln("Carriage Return Test");
				console.writeln("         Return Test\rCarriage");
				if(results["BS"]) {
					lines++;
					console.writeln("         Return Test\b\b\b\b\b\rCarriage");
				}
				return(console.yesno("Are the previous "+lines+" lines identical"));
			}
		},
		"SO":{
			char:"\x0e",
			desc:"Beamed Eighth Notes",
			type:"control",
			printable:true,
			test:function(results) {
				if(results["BS"]) {
					console.writeln("Testing SO/SI");
					console.writeln('"This is a test"');
					console.writeln(this.char+'\b"This is a test\x0f\b"');
					return(console.yesno("Are the previous two lines identical"));
				}
				else {
					console.writeln('\x0e"This is a test"\x0f');
					return(console.yesno('Does the previous line contain "This is a test"'));
				}
			},
		},
		"SI":{
			char:"\x0f",
			desc:"Empty circle with rays",
			normal:true,
			type:"control",
			printable:true,
		},
		"DLE":{
			char:"\x10",
			desc:"Right pointing triangle",
			normal:true,
			type:"control",
			printable:true,
		},
		"DC1":{
			char:"\x11",
			desc:"Left pointing triangle",
			normal:true,
			type:"control",
			printable:true,
		},
		"DC2":{
			char:"\x12",
			desc:"Up/Down arrow",
			normal:true,
			type:"control",
			printable:true,
		},
		"DC3":{
			char:"\x13",
			desc:"Double Exclimation",
			normal:true,
			type:"control",
			printable:true,
		},
		"DC4":{
			char:"\x14",
			desc:"Pilcrow",
			normal:true,
			type:"control",
			printable:true,
		},
		"NAK":{
			char:"\x15",
			desc:"Section sign",
			normal:true,
			type:"control",
			printable:true,
		},
		"SYN":{
			char:"\x16",
			desc:"Rectangle",
			normal:true,
			type:"control",
			printable:true,
		},
		"ETB":{
			char:"\x17",
			desc:"Up/Down Arrow with base",
			normal:true,
			type:"control",
			printable:true,
		},
		"CAN":{
			char:"\x18",
			desc:"Up Arrow",
			normal:true,
			type:"control",
			printable:true,
		},
		"EM":{
			char:"\x19",
			desc:"Down Arrow",
			normal:true,
			type:"control",
			printable:true,
		},
		"SUB":{
			char:"\x1a",
			desc:"Right Arrow",
			normal:true,
			type:"control",
			printable:true,
		},
		"ESC":{
			char:"\x1b",
			desc:"Escape",
			normal:true,
			type:"control",
			test:function(results) {
				console.writeln("ESC Character is not testable!");
				return(true);
			},
		},
		"FS":{
			char:"\x1c",
			desc:"Turned Not/Right Angle",
			normal:true,
			type:"control",
			printable:true,
		},
		"GS":{
			char:"\x1d",
			desc:"Left/Right arrow",
			normal:true,
			type:"control",
			printable:true,
		},
		"RS":{
			char:"\x1e",
			desc:"Up pointing triangle",
			normal:true,
			type:"control",
			printable:true,
		},
		"US":{
			char:"\x1f",
			desc:"Down pointing triangle",
			normal:true,
			type:"control",
			printable:true,
		},
	}

	for(key in ctrl_keys) {
		tests++;
		console.crlf();
		console.writeln("Testing "+key);
		if(ctrl_keys[key].test != undefined) {
			if(!ctrl_keys[key].test(results)) {
				results[key]=false;
				continue;
			}
		}
		if(ctrl_keys[key].printable) {
			write_raw('"'+ctrl_keys[key].char+'" '+ctrl_keys[key].desc+"\r\n");
			if(!console.yesno("Does the character in quotes match the description")) {
				results[key]=false;
			}
		}
		results[key]=true;
	}
}

function print_results(results)
{
	var total=0;
	var passed=0;
	var i;

	for(i in results) {
		total++;
		if(results[i])
			passed++;
	}
	console.writeln(format("Passed %d of %d (%f%%)",passed,total,(passed/total*100)));
	console.pause();
}

var results={};

test_ctrl(results);

console.crlf();
print_results(results);
