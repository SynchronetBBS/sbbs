// Terminal test utility...

function check_xy(x, y)
{
	var pos = console.getxy();
	if (pos.x !== x || pos.y !== y)
		return false;
	return true;
}

function ask_user(str)
{
	if (!interactive)
		return null;
	console.clear();
	return(console.yesno(str));
}

function read_ansi_seq(timeout)
{
	var seq = '';
	var ch;

	while (seq.search(/^(?:\x1b(?:\[(?:[0-\?]*(?:[ -/]*(?:[@-~])?)?)?)?)?$/) == 0) {
		ch = console.inkey(0, timeout);
		if (ch === '' || ch === null || ch === undefined)
			break;
		seq += ch;
		if (seq.search(/[@-~]$/) > 1)
			return seq;
	}
	mswait(timeout);
	while(console.inkey());
	return null;
}

var tests = [
	{'name':"NUL", 'func':function() {
		console.gotoxy(1,1);
		console.write("\x00\x00\x00");
		return check_xy(1, 1);
	}},
	{'name':"BEL", 'func':function() {
		if (!interactive)
			return null;
		console.write("\x07");
		return ask_user("Did you hear a beep?");
	}},
	{'name':"BS", 'func':function() {
		console.gotoxy(1,2);
		console.write("1234\b");
		if (!check_xy(4, 2))
			return false;
		console.write("\b\b\b\b\b\b\b");
		return check_xy(1, 2);
	}},
	{'name':"HT", 'func':function() {
		console.gotoxy(1,1);
		console.write("\t");
		// TODO: XTerm fails this test... ???
		if (check_xy(1, 1))
			return false;
		console.gotoxy(console.screen_columns, 1);
		console.write("\t");
		return check_xy(console.screen_columns, 1);
		// TODO: Check scroll... somehow.
	}},
	{'name':"LF", 'func':function() {
		console.gotoxy(1,1);
		console.write("1234\n");
		return check_xy(5, 2);
	}},
	{'name':"CR", 'func':function() {
		console.gotoxy(1,1);
		console.write("1234\r");
		return check_xy(1, 1);
	}},
	{'name':"NEL", 'func':function() {
		console.gotoxy(3,1);
		console.write("\x1bE");
		return check_xy(1,2);
	}},
	{'name':"HTS", 'func':function() {
		console.gotoxy(3,1);
		console.write("\x1bH");
		console.gotoxy(1,1);
		console.write("\t");
		return check_xy(3,1);
	}},
	{'name':"RI", 'func':function() {
		console.gotoxy(5, 2);
		console.write("\x1bM");
		return check_xy(5, 1);
	}},
	{'name':'APS', 'func':function() {
		console.gotoxy(1,1);
		console.write("\x1b_This is an application string\x1b\\");
		return check_xy(1, 1);
	}},
	{'name':'DCS', 'func':function() {
		console.gotoxy(1,1);
		console.write("\x1bPThis is a device control string\x1b\\");
		return check_xy(1, 1);
		// TODO: Loadable font
		// TODO: Sixel
		// TODO: DECRQSS
		// TODO: DECDMAC
	}},
	{'name':'PM', 'func':function() {
		console.gotoxy(1,1);
		console.write("\x1b^This is a privacy message\x1b\\");
		return check_xy(1, 1);
	}},
	{'name':'OSC', 'func':function() {
		console.gotoxy(1,1);
		console.write("\x1b]echo I hope this doesn't do anything...\x1b\\");
		return check_xy(1, 1);
		// TODO: Palette redefinitions...
	}},
	{'name':'APS', 'func':function() {
		console.gotoxy(1,1);
		console.write("\x1bXThis is just a string\x1b\\");
		return check_xy(1, 1);
	}},
	{'name':'RIS', 'func':function() {
		console.gotoxy(2,2);
		console.write("\x1bc");
		return check_xy(1, 1);
		// TODO: So much stuff here... basically verify that things get reset
	}},
	{'name':'ICH', 'func':function() {
		return null;
		// TODO: Interactive...
	}},
	{'name':'SL', 'func':function() {
		return null;
		// TODO: Interactive...
	}},
	{'name':'CUU', 'func':function() {
		console.gotoxy(20, 10);
		console.write("\x1b[A");
		if (!check_xy(20, 9))
			return false;
		console.write("\x1b[1A");
		if (!check_xy(20, 8))
			return false;
		console.write("\x1b[2A");
		if (!check_xy(20, 6))
			return false;
		return true;
	}},
	{'name':'SR', 'func':function() {
		return null;
		// TODO: Interactive...
	}},
	{'name':'CUD', 'func':function() {
		console.gotoxy(20, 1);
		console.write("\x1b[B");
		if (!check_xy(20, 2))
			return false;
		console.write("\x1b[1B");
		if (!check_xy(20, 3))
			return false;
		console.write("\x1b[2B");
		if (!check_xy(20, 5))
			return false;
		return true;
	}},
	{'name':'CUF', 'func':function() {
		console.gotoxy(1, 1);
		console.write("\x1b[C");
		if (!check_xy(2, 1))
			return false;
		console.write("\x1b[1C");
		if (!check_xy(3, 1))
			return false;
		console.write("\x1b[2C");
		if (!check_xy(5, 1))
			return false;
		return true;
	}},
	{'name':'CUB', 'func':function() {
		console.gotoxy(5, 1);
		console.write("\x1b[D");
		if (!check_xy(4, 1))
			return false;
		console.write("\x1b[1D");
		if (!check_xy(3, 1))
			return false;
		console.write("\x1b[2D");
		if (!check_xy(1, 1))
			return false;
		return true;
	}},
	{'name':'FNT', 'func':function() {
		return null;
		// TODO: Interactive...
	}},
	{'name':'CNL', 'func':function() {
		console.gotoxy(20, 1);
		console.write("\x1b[E");
		if (!check_xy(1, 2))
			return false;
		console.gotoxy(20, 1);
		console.write("\x1b[1E");
		if (!check_xy(1, 2))
			return false;
		console.gotoxy(20, 1);
		console.write("\x1b[2E");
		if (!check_xy(1, 3))
			return false;
		return true;
		// TODO: Scrolling
	}},
	{'name':'CPL', 'func':function() {
		console.gotoxy(20, 3);
		console.write("\x1b[F");
		if (!check_xy(1, 2))
			return false;
		console.gotoxy(20, 3);
		console.write("\x1b[1F");
		if (!check_xy(1, 2))
			return false;
		console.gotoxy(20, 5);
		console.write("\x1b[2F");
		if (!check_xy(1, 3))
			return false;
		return true;
	}},
	{'name':'CHA', 'func':function() {
		console.gotoxy(20, 1);
		console.write("\x1b[G");
		if (!check_xy(1, 1))
			return false;
		console.write("\x1b["+console.screen_columns+"G");
		if (!check_xy(console.screen_columns, 1))
			return false;
		console.write("\x1b[1G");
		if (!check_xy(1, 1))
			return false;
		return true;
	}},
	{'name':'CUP', 'func':function() {
		console.gotoxy(20, 3);
		console.write("\x1b[H");
		if (!check_xy(1, 1))
			return false;
		console.gotoxy(20, 3);
		console.write("\x1b[;H");
		if (!check_xy(1, 1))
			return false;
		console.gotoxy(20, 3);
		console.write("\x1b[2H");
		if (!check_xy(1, 2))
			return false;
		console.gotoxy(20, 3);
		console.write("\x1b[2;H");
		if (!check_xy(1, 2))
			return false;
		console.gotoxy(20, 3);
		console.write("\x1b[2;2H");
		if (!check_xy(2, 2))
			return false;
		console.gotoxy(20, 3);
		console.write("\x1b[;2H");
		if (!check_xy(2, 1))
			return false;
		return true;
	}},
	{'name':'CHT', 'func':function() {
		console.gotoxy(1,1);
		console.write("\t");
		var pos1 = console.getxy();
		console.write("\t");
		var pos2 = console.getxy();
		console.write("\t");
		var pos3 = console.getxy();
		console.gotoxy(pos3.x,pos3.y);
		console.write("\x1b[Z");
		if (!check_xy(pos2.x, pos2.y))
			return false;
		console.gotoxy(pos3.x,pos3.y);
		console.write("\x1b[1Z");
		if (!check_xy(pos2.x, pos2.y))
			return false;
		console.gotoxy(pos3.x,pos3.y);
		console.write("\x1b[2Z");
		if (!check_xy(pos1.x, pos1.y))
			return false;
		return true;
	}},
	{'name':'ED', 'func':function() {
		return null;
		// TODO: Interactive...
	}},
	{'name':'EL', 'func':function() {
		return null;
		// TODO: Interactive...
	}},
	{'name':'IL', 'func':function() {
		return null;
		// TODO: Interactive...
	}},
	{'name':'DL', 'func':function() {
		return null;
		// TODO: Interactive...
	}},
	{'name':'CTAM', 'func':function() {
		return null;
		// TODO: Interactive...
	}},
	{'name':'BCAM', 'func':function() {
		return null;
		// TODO: Interactive...
	}},
	{'name':'DCH', 'func':function() {
		return null;
		// TODO: Interactive...
	}},
	{'name':'SU', 'func':function() {
		return null;
		// TODO: Interactive...
	}},
	{'name':'XTSRGA', 'func':function() {
		// TODO: XTerm, even with "-ti vt340" fails this test (never a response)
		console.clear();
		console.write("NOTE: If this test hangs, press enter to abort.");
		console.write("\x1b[?2;1S");
		var seq = read_ansi_seq(500);
		console.clear();
		if (seq === null)
			return false;
		var m = seq.match(/^\x1b\[\?([0-9]+);([0-9]+)(?:;([0-9]+)(?:;([0-9]+))?)?S$/);
		if (m === null)
			return false;
		if (m[1] !== '2' || m[2] !== '0' || (m[3] == null) || (m[4] == null))
			return false;
		return true;
		// TODO: Many other options...
	}},
	{'name':'SD', 'func':function() {
		return null;
		// TODO: Interactive...
	}},
	{'name':'ECH', 'func':function() {
		return null;
		// TODO: Interactive...
	}},
	{'name':'CVT', 'func':function() {
		console.gotoxy(1,1);
		console.write("\t");
		var pos1 = console.getxy();
		console.write("\t");
		var pos2 = console.getxy();
		console.gotoxy(1,1);
		console.write("\x1b[Y");
		if (!check_xy(pos1.x, pos1.y))
			return false;
		console.gotoxy(1,1);
		console.write("\x1b[1Y");
		if (!check_xy(pos1.x, pos1.y))
			return false;
		console.gotoxy(1,1);
		console.write("\x1b[2Y");
		if (!check_xy(pos2.x, pos2.y))
			return false;
		return true;
	}},
	{'name':'CBT', 'func':function() {
		console.gotoxy(1,1);
		console.write("\t");
		var pos1 = console.getxy();
		console.write("\t");
		var pos2 = console.getxy();
		console.write("\t");
		var pos3 = console.getxy();
		console.gotoxy(pos3.x,pos3.y);
		console.write("\x1b[Z");
		if (!check_xy(pos2.x, pos2.y))
			return false;
		console.gotoxy(pos3.x,pos3.y);
		console.write("\x1b[1Z");
		if (!check_xy(pos2.x, pos2.y))
			return false;
		console.gotoxy(pos3.x,pos3.y);
		console.write("\x1b[2Z");
		if (!check_xy(pos1.x, pos1.y))
			return false;
		return true;
	}},
	{'name':'HPA', 'func':function() {
		console.gotoxy(20, 1);
		console.write("\x1b[`");
		if (!check_xy(1, 1))
			return false;
		console.write("\x1b["+console.screen_columns+"`");
		if (!check_xy(console.screen_columns, 1))
			return false;
		console.write("\x1b[1`");
		if (!check_xy(1, 1))
			return false;
		return true;
	}},
	{'name':'HPR', 'func':function() {
		console.gotoxy(1, 1);
		console.write("\x1b[a");
		if (!check_xy(2, 1))
			return false;
		console.write("\x1b[1a");
		if (!check_xy(3, 1))
			return false;
		console.write("\x1b[2a");
		if (!check_xy(5, 1))
			return false;
		return true;
	}},
	{'name':'REP', 'func':function() {
		console.gotoxy(1, 1);
		console.write(" \x1b[b");
		if (!check_xy(3, 1))
			return false;
		console.gotoxy(1, 1);
		console.write(" \x1b[1b");
		if (!check_xy(3, 1))
			return false;
		console.gotoxy(1, 1);
		console.write(" \x1b[10b");
		if (!check_xy(12, 1))
			return false;
		return true;
	}},
	{'name':'DA', 'func':function() {
		console.write("\x1b[c");
		var seq = read_ansi_seq(500);
		if (seq === null)
			return false;
		if (seq.search(/^\x1b\[.*c$/) === -1)
			return false;
		return true;
	}},
	{'name':'CTDA', 'func':function() {
		console.write("\x1b[<0c");
		var seq = read_ansi_seq(500);
		if (seq === null)
			return false;
		if (seq.search(/^\x1b\[\<.*c$/) === -1)
			return false;
		return true;
	}},
	{'name':'VPA', 'func':function() {
		console.gotoxy(20, 2);
		console.write("\x1b[d");
		if (!check_xy(20, 1))
			return false;
		console.write("\x1b[2d");
		return check_xy(20,2);
	}},
	{'name':'TSR', 'func':function() {
		console.gotoxy(1, 1);
		console.write("\t");
		var pos = console.getxy();
		console.write("\x1b["+pos.x+" d");
		console.gotoxy(1, 1);
		console.write("\t");
		if (check_xy(pos.x, pos.y))
			return false;
		return true;
	}},
	{'name':'VPR', 'func':function() {
		console.gotoxy(20, 1);
		console.write("\x1b[e");
		if (!check_xy(20, 2))
			return false;
		console.write("\x1b[1e");
		if (!check_xy(20, 3))
			return false;
		console.write("\x1b[2e");
		if (!check_xy(20, 5))
			return false;
		return true;
	}},
	{'name':'CUP', 'func':function() {
		console.gotoxy(20, 3);
		console.write("\x1b[f");
		if (!check_xy(1, 1))
			return false;
		console.gotoxy(20, 3);
		console.write("\x1b[;f");
		if (!check_xy(1, 1))
			return false;
		console.gotoxy(20, 3);
		console.write("\x1b[2f");
		if (!check_xy(1, 2))
			return false;
		console.gotoxy(20, 3);
		console.write("\x1b[2;f");
		if (!check_xy(1, 2))
			return false;
		console.gotoxy(20, 3);
		console.write("\x1b[2;2f");
		if (!check_xy(2, 2))
			return false;
		console.gotoxy(20, 3);
		console.write("\x1b[;2f");
		if (!check_xy(2, 1))
			return false;
		return true;
	}},
	{'name':'TBC', 'func':function() {
		console.gotoxy(1, 1);
		console.write("\t");
		var pos = console.getxy();
		console.write("\x1b[0g");
		console.gotoxy(1, 1);
		console.write("\t");
		if (check_xy(pos.x, pos.y))
			return false;
		console.write("\x1bc");
		console.write("\x1b[3g");
		console.gotoxy(console.screen_columns, 1);
		console.write("\x1bH");
		console.gotoxy(1,1);
		console.write("\t");
		if (!check_xy(80,1)) {
			console.write("\x1bc");
			return false;
		}
		console.write("\x1bc");
		return true;
	}},
];

function main()
{
	var results = [];

	/*
	 * First, ensure that console.clear(), console.gotoxy(), and
	 * console.getxy() work...
	 * 
	 * These are use by many of the movement tests.
	 */

	console.clear();
	var pos = console.getxy();
	if (pos.x !== 1 || pos.y !== 1) {
		// Note: this is not required by the spec...
		throw("Clear screen doesn't home cursor");
	}
	console.writeln("Just some text");
	console.write("To move the cursor");
	pos = console.getxy();
	if (pos.x !== 19 || pos.y !== 2) {
		throw("console.getxy() error! " + JSON.stringify(pos));
	}
	console.gotoxy(3, 2);
	pos = console.getxy();
	if (pos.x !== 3 || pos.y !== 2) {
		throw("console.gotoxy() error!");
	}

	tests.forEach(function(tst, idx) {
		results.push(tst.func());
	});

	return results;
}

var interactive = console.yesno("Run interactive tests");
console.write("\x1b[E");
var res = main();
var longest = 0;
var i;
var col = 0;
tests.forEach(function(tst) {
	if (tst.name.length > longest)
		longest = tst.name.length;
});
var cols = Math.floor(console.screen_columns / (longest + 1 + 7 + 2));
console.clear();
res.forEach(function(r, idx) {
	console.print(tests[idx].name);
	for (i=tests[idx].name.length; i<longest; i++)
		console.print('.');
	console.print('.');
	if (r === null)
		console.print("Skipped\x01N");
	else if (r)
		console.print(".\x01GPassed\x01N");
	else
		console.print(".\x01RFailed\x01N");
	col++;
	if (col >= cols) {
		console.print("\r\n");
		col = 0;
	}
	else
		console.print("  ");
});
if (col > 0)
	console.print("\r\n");
console.print("Press any key to return to BBS. ");
console.getkey();
