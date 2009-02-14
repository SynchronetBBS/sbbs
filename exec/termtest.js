load("sbbsdefs.js");

function run_tests(results,section,keys)
{
	var key;
	var tested;

	for(key in keys) {
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
		if(keys[key].printable) {
			tested=true;
			write_raw('"'+keys[key].char+'" '+keys[key].desc+"\r\n");
			if(!console.yesno("Does the character in quotes match the description")) {
				results[section][key]=false;
				continue;
			}
		}
		if(tested)
			results[section][key]=true;
		else
			results[section][key]=null;
	}
}

function test_ctrl(results)
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
	run_tests(results, "ctrl", ctrl_keys);
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

function test_ctrl_seqs(results)
{
	var ctrl_seqs={
		/* No intermediate bytes */
		"ICH":{
			char:"@",
			test:function(results) {
				if(results.ctrl.BS) {
					console.writeln("InsertCharTest\b\b\b\b\033[@\b\b\b\b\033[@");
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
				console.writeln(" \033[2ALine 3");
				console.writeln(" Line 4");
				return(console.yesno('Does "Line 1" to "Line 4" line up with no gaps'));
			}
		},
		"CUD":{
			char:"B",
			test:function(results) {
				if(results.ctrl_seqs.CUU) {
				}
				else if(results.ctrl.FF) {
				}
				return(null);
			}
		},
		"CUF":{
			char:"C",
		},
		"CUB":{
			char:"D",
		},
		"CNL":{
			char:"E",
		},
		"CPL":{
			char:"F",
		},
		"CHA":{
			char:"G",
		},
		"CUP":{
			char:"H",
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
		},
		"INT":{
			char:"a",
		},
		"EMI":{
			char:"b",
		},
		"RIS":{
			char:"c",
		},
		"CMD":{
			char:"d",
		},
		"Undef-e":{
			char:"e",
		},
		"Undef-f":{
			char:"f",
		},
		"Undef-g":{
			char:"g",
		},
		"Undef-h":{
			char:"h",
		},
		"Undef-i":{
			char:"i",
		},
		"Undef-j":{
			char:"j",
		},
		"Undef-k":{
			char:"k",
		},
		"Undef-l":{
			char:"l",
		},
		"Undef-m":{
			char:"m",
		},
		"LS2":{
			char:"n",
			desc:"Locking-Shift Two",
		},
		"LS3":{
			char:"o",
			desc:"Locking-Shift Three",
		},
		"Undef-p":{
			char:"p",
		},
		"Undef-q":{
			char:"q",
		},
		"Undef-r":{
			char:"r",
		},
		"Undef-s":{
			char:"s",
		},
		"Undef-t":{
			char:"t",
		},
		"Undef-u":{
			char:"u",
		},
		"Undef-v":{
			char:"v",
		},
		"Undef-w":{
			char:"w",
		},
		"Undef-x":{
			char:"x",
		},
		"Undef-y":{
			char:"y",
		},
		"Undef-z":{
			char:"z",
		},
		"Undef-{":{
			char:"{",
		},
		"LS3R":{
			char:"|",
			desc:"Locking-Shift Three Right",
		},
		"LS2R":{
			char:"}",
			desc:"Locking-Shift Two Right",
		},
		"LS1R":{
			char:"~",
			desc:"Locking-Shift One Right",
		},
		"Undef-DEL":{
			char:"\x7f",
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

	/* Disable parsing */
	console.ctrlkey_passthru="+@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_";

	test_ctrl(results);
	test_c1(results);
	test_ctrl_seqs(results);
	test_ind_ctrl_funcs(results);

	/* Restore CTRL parsing */
	console.ctrlkey_passthru=orig_pt;

	console.crlf();
	print_results(results);
}

run_all_tests();
