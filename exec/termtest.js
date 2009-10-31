load("sbbsdefs.js");

function run_tests(results,section,keys,skip_printable)
{
	var key;
	var tested;

	for(key in keys) {
		if(console.aborted) {
			console.aborted=false;
			break;
		}
		tested=false;
		console.line_counter=0;
		console.crlf();
		console.line_counter=0;
		console.writeln("Testing "+key);
		if(keys[key].untestable) {
			results[section][key]=null;
			continue;
		}
		if(keys[key].test != undefined) {
			tested=true;
			if(!keys[key].test(results)) {
				results[section][key]=false;
				continue;
			}
		}
		if(!skip_printable) {
			if(keys[key].printable) {
				tested=true;
				write_raw('"'+keys[key].char+'" '+keys[key].desc+"\r\n");
				if(!console.yesno("Does the character in quotes match the description")) {
					results[section][key]=false;
					continue;
				}
			}
		}
		if(tested)
			results[section][key]=true;
		else
			results[section][key]=null;
	}
}

function test_ctrl(results,skip)
{

	var ctrl_keys={
		"NUL":{
			char:"\x00",
			desc:"Zero width",
			type:"control",
			printable:true,
			source:"ECMA-48",
		},
		"SOH":{
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
					return("ENQ Response: '"+retstr+"'");
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
				do {
					var ret;

					console.write(this.char);
					ret=console.yesno("Did your terminal beep");
					if(!ret) {
						if(!console.yesno("Do you want to try again"))
							return(false);
					}
					else {
						return(true);
					}
				} while(1);
			},
		},
		"BS":{
			char:"\x08",
			desc:"Backspace",
			type:"control",
			test:function(results) {
				var ret;

				console.writeln("Backspace Test");
				console.writeln("Back     \b\b\b\b\bspace Test");
				console.writeln("Back      Test\b\b\b\b\b\b\b\b\b\bspace");
				console.writeln("\b\b\bBackspace Test");
				ret=console.yesno("Are the previous four lines identical");
				if(!ret) {
					ret='';

					console.writeln("Backspace Test");
					console.writeln("Back     \b\b\b\b\bspace Test");
					ret=console.yesno("Are the previous two lines identical");
					if(!ret) {
						// Completely non-functional
						return(false);
					}
					console.writeln("Backspace Test");
					console.writeln("Back      Test\b\b\b\b\b\b\b\b\b\bspace");
					if(!console.yesno("Are the previous two lines identical")) {
						// Destructive
						ret += "Destructive Backspace";
					}
					console.writeln("Backspace Test");
					console.writeln("\b\b\bBackspace Test");
					if(!console.yesno("Are the previous two lines identical")) {
						// Wraps
						if(ret.length)
							ret += "\r\n";
						ret += "Backspace Wraps";
					}
					if(ret=='') {
						console.writeln("I thought you said they didn't line up!");
						return(true);
					}
				}
				return(ret);
			},
			source:"ECMA-48",
		},
		"HT":{
			char:"\x09",
			desc:"TAB",
			type:"control",
			test:function(results) {
				var lines=0;
				console.writeln("|       ||      |||     ||||    |||||   ||||||  ||||||| ||||||||        |");
				lines++;
				console.writeln("|\t||\t|||\t||||\t|||||\t||||||\t|||||||\t||||||||\t|\t|\t||\t|||\t||||\t|||||\t||||||\t|||||||\t||||||||\t|");
				lines+=2;
				if(results.ctrl.BS) {
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
				if(results.ctrl.BS) {
					console.write("Line Feed Test\b\b\b\b\b\n Test\b\b\b\b\b\b\b\b\b\b\b\b\b\bLine Feed\r\n");
					return(console.yesno("Are the previous two lines identical"));
				}
				else {
					console.writeln("First Half\nSecond Half");
					console.writeln("First Half");
					console.writeln("          Second Half");
					return(console.yesno("Are the previous four lines two identical pairs of lines"));
				}
			},
			source:"ECMA-48",
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
				if(results.ctrl.BS) {
					lines++;
					console.writeln("         Return Test\b\b\b\b\b\rCarriage");
				}
				return(console.yesno("Are the previous "+lines+" lines identical"));
			},
			source:"ECMA-48",
		},
		"SO - LS1":{
			char:"\x0e",
			desc:"Beamed Sixteenth Notes",
			type:"control",
			printable:true,
			test:function(results) {
				if(results.ctrl.BS) {
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
		"SI - LS0":{
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
			desc:"Double Exclamation",
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
			untestable:true,
			source:"ECMA-48",
		},
		"IS4 - FS":{
			char:"\x1c",
			desc:"Turned Not/Right Angle",
			normal:true,
			type:"control",
			printable:true,
		},
		"IS3 - GS":{
			char:"\x1d",
			desc:"Left/Right arrow",
			normal:true,
			type:"control",
			printable:true,
		},
		"IS2 - RS":{
			char:"\x1e",
			desc:"Up pointing triangle",
			normal:true,
			type:"control",
			printable:true,
		},
		"IS1 - US":{
			char:"\x1f",
			desc:"Down pointing triangle",
			normal:true,
			type:"control",
			printable:true,
		},
	};

	if(results.ctrl==undefined)
		results.ctrl={};
	run_tests(results, "ctrl", ctrl_keys,skip);
}

function test_c1(results)
{
	var c1_keys = {
		"Undef-@":{
			char:"@",
			untestable:true,
		},
		"Undef-A":{
			char:"A",
			untestable:true,
		},
		"BPH":{
			char:"B",
			desc:"Break Permitted Here",
			untestable:true,
		},
		"NBH":{
			char:"C",
			desc:"No Break Here",
			untestable:true,
		},
		"Undef-D":{
			char:"D",
			untestable:true,
		},
		"NEL":{
			char:"E",
			desc:"Next Line",
			untestable:true,
		},
		"SSA":{
			char:"F",
			desc:"Start Of Selected Area",
			untestable:true,
		},
		"ESA":{
			char:"G",
			desc:"End Of Selected Area",
			untestable:true,
		},
		"HTS":{
			char:"H",
			desc:"Character Tabulation Set",
			untestable:true,
		},
		"HTJ":{
			char:"I",
			desc:"Character Tabulation With Justification",
			untestable:true,
		},
		"VTS":{
			char:"J",
			desc:"Line Tabulation Set",
			untestable:true,
		},
		"PLD":{
			char:"K",
			desc:"Partial Line Forward",
			untestable:true,
		},
		"PLU":{
			char:"L",
			desc:"Partial Line Backward",
			untestable:true,
		},
		"RI":{
			char:"M",
			desc:"Reverse Line Feed",
			untestable:true,
		},
		"SS2":{
			char:"N",
			desc:"Single-Shift Two",
			untestable:true,
		},
		"SS3":{
			char:"O",
			desc:"Single-Shift Three",
			untestable:true,
		},
		"DCS":{
			char:"P",
			desc:"Device Control String",
			untestable:true,
		},
		"PU1":{
			char:"Q",
			desc:"Private Use 1",
			untestable:true,
		},
		"PU2":{
			char:"R",
			desc:"Private Use 2",
			untestable:true,
		},
		"STS":{
			char:"S",
			desc:"Set Transmit State",
			untestable:true,
		},
		"CCH":{
			char:"T",
			desc:"Cancel Character",
			untestable:true,
		},
		"MW":{
			char:"U",
			desc:"Message Waiting",
			untestable:true,
		},
		"SPA":{
			char:"V",
			desc:"Start Of Guarded Area",
			untestable:true,
		},
		"EPA":{
			char:"W",
			desc:"End Of Guarded Area",
			untestable:true,
		},
		"SOS":{
			char:"X",
			desc:"Start Of String",
			untestable:true,
		},
		"Undef-Y":{
			char:"Y",
			untestable:true,
		},
		"SCI":{
			char:"Z",
			desc:"Single Character Introducer",
			untestable:true,
		},
		"CSI":{
			char:"[",
			desc:"Control Sequence Intoducer",
			untestable:true,
		},
		"ST":{
			char:"\\",
			desc:"String Terminator",
			untestable:true,
		},
		"OSC":{
			char:"]",
			desc:"Operating system Command",
			untestable:true,
		},
		"PM":{
			char:"^",
			desc:"Privacy Message",
			untestable:true,
		},
		"APC":{
			char:"_",
			desc:"Application Program Command",
			untestable:true,
		},
	};

	if(results.c1==undefined)
		results.c1={};
	run_tests(results, "c1", c1_keys);
}

function get_curr_pos()
{
	var buf='';
	var state=0;
	var row=0;
	var col=0;
	var ret=null;

	console.write("\033[6n");
	for(i=0; i<3; i++) {
		tmp=console.inkey(K_NONE, 1000);
		if(tmp != '') {
			switch(state) {
				case 0:
					if(tmp=='\033') {
						i=0;
						state++;
					}
					break;
				case 1:
					if(tmp=='[') {
						i=0;
						state++;
					}
					else {
						state=0;
					}
					break;
				case 2:
					if(tmp >= '0' && tmp <= '9') {
						buf+=tmp;
					}
					else if(tmp==';') {
						if((row=parseInt(buf,10))!=NaN) {
							i=0;
							buf='';
							state++;
						}
						else {
							state=0;
						}
					}
					else {
						state=0;
					}
					break;
				case 3:
					if(tmp >= '0' && tmp <= '9') {
						buf+=tmp;
					}
					else if(tmp=='R') {
						if((col=parseInt(buf,10))!=NaN) {
							ret={
								row:row,
								col:col,
							};
							return(ret);
						}
						else {
							state=0;
						}
					}
					else {
						state=0;
					}
					break;
			}
		}
	}
	return(null);	
}

function test_ctrl_seqs(results)
{
	var ctrl_seqs={
		/* No intermediate bytes */
		"ICH":{
			char:"@",
			test:function(results) {
				if(results.ctrl.BS) {
					console.writeln("InsertCharTest\b\b\b\b\033[@\b\b\b\b\033[01@");
					console.write("\033[99@");
					return(console.yesno('Are there spaces in "Insert Char Test"'));
				}
				return(null);
			},
		},
		"CUU":{
			char:"A",
			test:function(results) {
				console.writeln(" Line 1");
				console.crlf();
				console.writeln(" \033[ALine 2");
				console.crlf();
				console.crlf();
				console.writeln(" \033[02ALine 3");
				console.writeln(" Line 4");
				return(console.yesno('Does "Line 1" to "Line 4" line up with no gaps'));
			},
		},
		"CUD":{
			char:"B",
			test:function(results) {
				if(results.ctrl_seqs.CUU && results.ctrl.BS) {
					console.crlf();
					console.crlf();
					console.crlf();
					console.crlf();
					console.write("\033[4A");
					console.write(" Line 1\b\b\b\b\b\b");
					console.write("\033[BLine 2\b\b\b\b\b\b");
					console.write("\033[1BLine 3\b\b\b\b\b\b");
					console.write("\033[A\033[02B");
					console.write("Line 4");
					console.write("\033[99B");
					console.writeln("\b\b\b\b\b\bLine 5");
					return(console.yesno('Does "Line 1" to "Line 5" line up with no gaps'));
				}
				return(null);
			},
		},
		"CUF":{
			char:"C",
			test:function(results) {
				if(results.ctrl.CR) {
					console.writeln("C\r\033[Cu\r\033[2Cr\r\033[3Cs\r\033[4Co\r\033[5Cr\r\033[7CF\r\033[8Co\r\033[9Cr\r\033[10Cw\r\033[011Ca\r\033[012Cr\r\033[13Cd");
					console.writeln("Cursor Forward");
					return(console.yesno("Are the two previous lines identical"));
				}
				return(null);
			},
		},
		"CUB":{
			char:"D",
			test:function(results) {
				var ret;

				console.writeln("Cursor Backwards Test");
				console.writeln("Cursor           \033[D\033[2D\033[03D\033[4DBackwards Test");
				console.writeln("Cursor           Test\033[014DBackwards");
				console.writeln("\033[D\033[2DCursor Backwards Test");
				console.write("\033[99D");
				ret=console.yesno("Are the previous four lines identical");
				return(ret);
			},
		},
		"CNL":{
			char:"\x0a",
			desc:"Cursor Next Line",
			test:function(results) {
				if(results.ctrl_seqs.CUU) {
					console.crlf();
					console.crlf();
					console.writeln("\033[2ACursor Next Line Test\033[ECursor Next Line Test\033[A\033[2ECursor Next Line Test");
					return(console.yesno("Are the previous three lines identical"));
				}
				return(null);
			},
			char:"E",
		},
		"CPL":{
			char:"F",
			desc:"Cursor Preceding Line",
			test:function(results) {
				if(results.ctrl.LF) {
					console.crlf();
					console.crlf();
					console.crlf();
					console.write("\033[3F");
					console.writeln(" Line 1\n\n");
					console.writeln("\033[2F Line 2");
					console.crlf();
					console.writeln("\033[F Line 3");
					return(console.yesno('Does "Line 1" to "Line 3" line up with no gaps'));
				}
			}
		},
		"CHA":{
			char:"G",
			test:function(results) {
				var ret;

				console.writeln("Cursor Character Absolute Test");
				console.writeln("Cursor           \033[8GCharacter Absolute Test");
				console.write("Cursor           Absolute Test\033[08GCharacter\n");
				console.writeln("\033[GCursor Character Absolute Test");
				console.write("\033[99G\033[0G");
				ret=console.yesno("Are the previous four lines identical");
				return(ret);
			},
		},
		"CUP":{
			char:"H",
			test:function(results) {
				
			},
		},
		"CHT":{
			char:"I",
		},
		"ED":{
			char:"J",
		},
		"EL":{
			char:"K",
		},
		"IL":{
			char:"L",
		},
		"DL":{
			char:"M",
		},
		"EF":{
			char:"N",
		},
		"EA":{
			char:"O",
		},
		"DCH":{
			char:"P",
		},
		"SSE":{
			char:"Q",
		},
		"CPR":{
			char:"R",
		},
		"SU":{
			char:"S",
		},
		"SD":{
			char:"T",
		},
		"NP":{
			char:"U",
		},
		"PP":{
			char:"V",
		},
		"CTC":{
			char:"W",
		},
		"ECH":{
			char:"X",
		},
		"CVT":{
			char:"Y",
		},
		"CBT":{
			char:"Z",
		},
		"SRS":{
			char:"[",
		},
		"PTX":{
			char:"\\",
		},
		"SDS":{
			char:"]",
		},
		"SIMD":{
			char:"^",
		},
		"":{
			char:"_",
		},
		"HPA":{
			char:"`",
		},
		"HPR":{
			char:"a",
		},
		"REP":{
			char:"b",
		},
		"DA":{
			char:"c",
		},
		"VPA":{
			char:"d",
		},
		"VPR":{
			char:"e",
		},
		"HVP":{
			char:"f",
		},
		"TBC":{
			char:"g",
		},
		"SM":{
			char:"h",
		},
		"MC":{
			char:"i",
		},
		"HPB":{
			char:"j",
		},
		"VPB":{
			char:"k",
		},
		"RM":{
			char:"l",
		},
		"SGR":{
			char:"m",
		},
		"DSR":{
			char:"n",
			test:function(results) {
			},
		},
		"DAQ":{
			char:"o",
		},
		/* SPACE Intermediate Byte */
		"SL":{
			char:"@",
			IB:" ",
		},
		"SR":{
			char:"A",
			IB:" ",
		},
		"GSM":{
			char:"B",
			IB:" ",
		},
		"GSS":{
			char:"C",
			IB:" ",
		},
		"FNT":{
			char:"D",
			IB:" ",
		},
		"TSS":{
			char:"E",
			IB:" ",
		},
		"JFY":{
			char:"F",
			IB:" ",
		},
		"SPI":{
			char:"G",
			IB:" ",
		},
		"QUAD":{
			char:"H",
			IB:" ",
		},
		"SSU":{
			char:"I",
			IB:" ",
		},
		"PFS":{
			char:"J",
			IB:" ",
		},
		"SHS":{
			char:"K",
			IB:" ",
		},
		"SVS":{
			char:"L",
			IB:" ",
		},
		"IGS":{
			char:"M",
			IB:" ",
		},
		"Undef-N":{
			char:"N",
			IB:" ",
		},
		"IDCS":{
			char:"O",
			IB:" ",
		},
		"PPA":{
			char:"P",
			IB:" ",
		},
		"PPR":{
			char:"Q",
			IB:" ",
		},
		"PPB":{
			char:"R",
			IB:" ",
		},
		"SPD":{
			char:"S",
			IB:" ",
		},
		"DTA":{
			char:"T",
			IB:" ",
		},
		"SHL":{
			char:"U",
			IB:" ",
		},
		"SLL":{
			char:"V",
			IB:" ",
		},
		"FNK":{
			char:"W",
			IB:" ",
		},
		"SPQR":{
			char:"X",
			IB:" ",
		},
		"SEF":{
			char:"Y",
			IB:" ",
		},
		"PEC":{
			char:"Z",
			IB:" ",
		},
		"SSW":{
			char:"[",
			IB:" ",
		},
		"SACS":{
			char:"\\",
			IB:" ",
		},
		"SAPV":{
			char:"]",
			IB:" ",
		},
		"STAB":{
			char:"^",
			IB:" ",
		},
		"GCC":{
			char:"_",
			IB:" ",
		},
		"TATE":{
			char:"`",
			IB:" ",
		},
		"TALE":{
			char:"a",
			IB:" ",
		},
		"TAC":{
			char:"b",
			IB:" ",
		},
		"TCC":{
			char:"c",
			IB:" ",
		},
		"TSR":{
			char:"d",
			IB:" ",
		},
		"SCO":{
			char:"e",
			IB:" ",
		},
		"SRCS":{
			char:"f",
			IB:" ",
		},
		"SCS":{
			char:"g",
			IB:" ",
		},
		"SLS":{
			char:"h",
			IB:" ",
		},
		"Undef-i":{
			char:"i",
			IB:" ",
		},
		"Undef-j":{
			char:"j",
			IB:" ",
		},
		"SCP":{
			char:"k",
			IB:" ",
		},
		"Undef-l":{
			char:"l",
			IB:" ",
		},
		"Undef-m":{
			char:"m",
			IB:" ",
		},
		"Undef-n":{
			char:"n",
			IB:" ",
		},
		"Undef-o":{
			char:"o",
			IB:" ",
		},
	};

	if(results.ctrl_seqs==undefined)
		results.ctrl_seqs={};
	run_tests(results, "ctrl_seqs", ctrl_seqs);
}

function test_ind_ctrl_funcs(results)
{
	var ind_ctrl_funcs={
		"DMI":{
			char:"`",
			untestable:true,
		},
		"INT":{
			char:"a",
			untestable:true,
		},
		"EMI":{
			char:"b",
			untestable:true,
		},
		"RIS":{
			char:"c",
			untestable:true,
		},
		"CMD":{
			char:"d",
			untestable:true,
		},
		"Undef-e":{
			char:"e",
			untestable:true,
		},
		"Undef-f":{
			char:"f",
			untestable:true,
		},
		"Undef-g":{
			char:"g",
			untestable:true,
		},
		"Undef-h":{
			char:"h",
			untestable:true,
		},
		"Undef-i":{
			char:"i",
			untestable:true,
		},
		"Undef-j":{
			char:"j",
			untestable:true,
		},
		"Undef-k":{
			char:"k",
			untestable:true,
		},
		"Undef-l":{
			char:"l",
			untestable:true,
		},
		"Undef-m":{
			char:"m",
			untestable:true,
		},
		"LS2":{
			char:"n",
			desc:"Locking-Shift Two",
			untestable:true,
		},
		"LS3":{
			char:"o",
			desc:"Locking-Shift Three",
			untestable:true,
		},
		"Undef-p":{
			char:"p",
			untestable:true,
		},
		"Undef-q":{
			char:"q",
			untestable:true,
		},
		"Undef-r":{
			char:"r",
			untestable:true,
		},
		"Undef-s":{
			char:"s",
			untestable:true,
		},
		"Undef-t":{
			char:"t",
			untestable:true,
		},
		"Undef-u":{
			char:"u",
			untestable:true,
		},
		"Undef-v":{
			char:"v",
			untestable:true,
		},
		"Undef-w":{
			char:"w",
			untestable:true,
		},
		"Undef-x":{
			char:"x",
			untestable:true,
		},
		"Undef-y":{
			char:"y",
			untestable:true,
		},
		"Undef-z":{
			char:"z",
			untestable:true,
		},
		"Undef-{":{
			char:"{",
			untestable:true,
		},
		"LS3R":{
			char:"|",
			desc:"Locking-Shift Three Right",
			untestable:true,
		},
		"LS2R":{
			char:"}",
			desc:"Locking-Shift Two Right",
			untestable:true,
		},
		"LS1R":{
			char:"~",
			desc:"Locking-Shift One Right",
			untestable:true,
		},
		"Undef-DEL":{
			char:"\x7f",
			untestable:true,
		},
	};

	if(results.ind_ctrl_funcs==undefined)
		results.ind_ctrl_funcs={};
	run_tests(results, "ind_ctrl_funcs", ind_ctrl_funcs);
}

function print_results(results)
{
	var total=0;
	var passed=0;
	var skipped=0;
	var section_total;
	var section_passed;
	var section_skipped;
	var i,j;

	for(i in results) {
		section_total=section_passed=section_skipped=0;
		for(j in results[i]) {
			section_total++;
			if(results[i][j]===null)
				section_skipped++;
			if(results[i][j]===true)
				section_passed++;
		}
		total+=section_total;
		passed+=section_passed;
		skipped+=section_skipped;
		console.writeln(format("%s: Passed %d of %d (%f%%) Skipped %d",i,section_passed,section_total,(section_passed/section_total*100),section_skipped));
	}
	console.writeln(format("Total: Passed %d of %d (%f%%) Skipped %d",passed,total,(passed/total*100),skipped));
	console.pause();
}

function run_all_tests() {
	var results={};
	var orig_pt=console.ctrlkey_passthru;
	var skip;

	skip=console.yesno("Skip glyph validation");
	/* Disable parsing */
	console.ctrlkey_passthru="+@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_";

	test_ctrl(results,skip);
	test_c1(results,skip);
	test_ctrl_seqs(results,skip);
	test_ind_ctrl_funcs(results,skip);

	/* Restore CTRL parsing */
	console.ctrlkey_passthru=orig_pt;

	console.crlf();
	print_results(results);
}

function run_deuces_tests() {
	var results={};
	var orig_pt=console.ctrlkey_passthru;

	/* Disable parsing */
	console.ctrlkey_passthru="+@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_";

	var curr_pos=get_curr_pos();
	for(i in curr_pos) {
		console.writeln(i+" = "+curr_pos[i]);
	}

	/* Restore CTRL parsing */
	console.ctrlkey_passthru=orig_pt;
}

run_all_tests();
