// Terminal test utility...

var has_cksum = false;

function check_xy(x, y)
{
	var pos = fast_getxy();
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
	while(console.inkey(100));
	return null;
}

function get_cs(sx, sy, ex, ey)
{
	console.write("\x1b[1;1;"+sy+";"+sx+";"+ey+";"+ex+"*y");
	return read_ansi_string(500);
}

function interactive_or_string(str, x, y)
{
	var oldcs;
	var newcs;

	if (has_cksum) {
		oldcs = get_cs(x, y, x + str.length - 1, y);
		console.gotoxy(x, y);
		console.write(str);
		newcs = get_cs(x, y, x + str.length - 1, y);
		if (newcs === oldcs)
			return true;
		// Check if spaces are actually erased...
		console.gotoxy(x, y);
		console.cleartoeol();
		str.split('').forEach(function(ch) {
			if (ch === ' ')
				console.write("\x1b[C");
			else
				console.write(ch);
		});
		newcs = get_cs(x, y, x + str.length - 1, y);
		if (newcs === oldcs)
			return true;
		return false;
	}
	if (!interactive)
		return null;
	if (x > 3) {
		console.gotoxy(x - 2, y);
		console.print("\x01H\x01Y->\x01N\b");
	}
	else {
		console.gotoxy(x+str.length, y);
		console.print("\x01H\x01Y<-\x01N\b");
	}
	if (y < console.screen_rows / 2)
		console.gotoxy(1, console.screen_rows - 2);
	else
		console.gotoxy(1, 1);
	console.cleartoeol();
	var ret = console.yesno("Do you see the string \""+str+"\" at the yellow arrow");
	if (x > 3) {
		console.gotoxy(x - 2, y);
		console.print("\x01N  \b");
	}
	else {
		console.gotoxy(x+str.length, y);
		console.print("\x01N  \b");
	}
	if (y < console.screen_rows / 2) {
		console.gotoxy(1, console.screen_rows - 2);
		console.write("\x1b[0J");
	}
	else {
		console.gotoxy(1, 3);
		console.write("\x1b[1J");
	}
	
	return ret;
}

function read_ansi_string(timeout)
{
	var seq = '';
	var ch;

	while (seq.search(/^(?:\x1b(?:[PX\]\^_](?:[\x08-\x0d\x20-\x7e]*(?:\x1b(?:\\)?)?)?)?)?$/) == 0) {
		ch = console.inkey(0, timeout);
		if (ch === '' || ch === null || ch === undefined)
			break;
		seq += ch;
		if (seq.search(/\\/) > 1)
			return seq;
	}
	mswait(timeout);
	while(console.inkey());
	return null;
}

function fast_getxy()
{
	console.write("\x1b[6n");
	var seq = read_ansi_seq(500);
	if (seq === null)
		return {x:0, y:0};
	var m = seq.match(/^\x1b\[([0-9]+);([0-9]+)R$/);
	if (m === null)
		return {x:0, y:0};
	return {x:parseInt(m[2], 10), y:parseInt(m[1], 10)};
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
		return ask_user("Did you hear a beep");
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
		if (check_xy(1, 1))
			return false;
		return true;
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
		var ret;
		console.gotoxy(1, 1);
		console.write("InsertCharacter");
		console.gotoxy(7, 1);
		console.write("\x1b[@");
		ret = interactive_or_string("Insert Character", 1, 1);
		if (!ret)
			return ret;
		console.gotoxy(1, 1);
		console.write("InsertCharacters");
		console.gotoxy(7, 1);
		console.write("\x1b[3@");
		console.write(" 3");
		return interactive_or_string("Insert 3 Characters", 1, 1);
	}},
	{'name':'SL', 'func':function() {
		var ret;
		console.gotoxy(1, 1);
		console.write(" Shift Left");
		console.write("\x1b[ @");
		ret = interactive_or_string("Shift Left", 1, 1);
		if (!ret)
			return ret;
		console.gotoxy(1, 1);
		console.write("   Shift Left 3");
		console.write("\x1b[3 @");
		return interactive_or_string("Shift Left 3", 1, 1);
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
		var ret;
		console.gotoxy(1, 1);
		console.write("Shift Right");
		console.write("\x1b[ A");
		console.gotoxy(1, 1);
		console.write('x');
		ret = interactive_or_string("xShift Right", 1, 1);
		if (!ret)
			return ret;
		console.gotoxy(1, 1);
		console.write("Shift Right 3");
		console.write("\x1b[3 A");
		console.gotoxy(1, 1);
		console.write('xxx');
		return interactive_or_string("xxxShift Right 3", 1, 1);
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
		console.clear();
		console.gotoxy(1, 1);
		console.write("This is in this font...\r\n");
		var oldcs = get_cs(1, 1, 23, 1);
		if (oldcs == null)
			return null;
		console.write("\x1b[0;1 D");
		console.gotoxy(1, 2);
		console.write("This is in this font...\r\n");
		var newcs = get_cs(1, 2, 23, 2);
		console.write("\x1b[0;0 D");
		if (newcs == null)
			return null;
		if (oldcs == newcs)
			return false;
		return true;
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
		var pos1 = fast_getxy();
		console.write("\t");
		var pos2 = fast_getxy();
		console.gotoxy(1, 1);
		console.write("\x1b[I");
		if (!check_xy(pos1.x, pos1.y))
			return false;
		console.gotoxy(1, 1);
		console.write("\x1b[1I");
		if (!check_xy(pos1.x, pos1.y))
			return false;
		console.gotoxy(1, 1);
		console.write("\x1b[2I");
		if (!check_xy(pos2.x, pos2.y))
			return false;
		return true;
	}},
	{'name':'ED', 'func':function() {
		console.clear();
		console.gotoxy(1, 1);
		console.write("This is a string that should be erased... except-for-this-bit.");
		console.gotoxy(42, 1);
		console.write("\x1b[1J");
		var ret = interactive_or_string("                                          except-for-this-bit.", 1, 1);
		if (!ret)
			return ret;
		console.gotoxy(console.screen_columns - 51, console.screen_rows);
		console.write("Except-for-this-bit, all of this should be erased.");
		console.gotoxy(console.screen_columns - 32, console.screen_rows);
		console.write("\x1b[0J");
		ret = interactive_or_string("Except-for-this-bit                               ", console.screen_columns - 51, console.screen_rows);
		if (!ret)
			return ret;
		console.gotoxy(1, 1);
		console.write("This is a string that should be erased...");
		console.gotoxy(console.screen_columns - 40, console.screen_rows);
		console.write("This string should be erased as well...");
		console.write("\x1b[2J");
		ret = interactive_or_string("                                         ", 1, 1);
		if (!ret)
			return ret;
		ret = interactive_or_string("                                       ", console.screen_columns - 40, console.screen_rows);
		return ret;
	}},
	{'name':'EL', 'func':function() {
		console.clear();
		console.gotoxy(1, 1);
		console.write("First Line\r\nSecond Line\r\nThird Line\r\nFourth Line");
		console.gotoxy(6, 1);
		console.write("\x1b[K");
		var ret = interactive_or_string("First     ", 1, 1);
		if (!ret)
			return ret;
		console.gotoxy(7, 2);
		console.write("\x1b[0K");
		ret = interactive_or_string("Second     ", 1, 2);
		if (!ret)
			return ret;
		console.gotoxy(6, 3);
		console.write("\x1b[1K");
		ret = interactive_or_string("      Line", 1, 3);
		if (!ret)
			return ret;
		console.gotoxy(7, 4);
		console.write("\x1b[2K");
		return interactive_or_string("           ", 1, 4);
	}},
	{'name':'IL', 'func':function() {
		console.clear();
		console.gotoxy(1, 1);
		console.write("First Line\r\nSecond Line\r\nThird Line\r\nFourth Line");
		console.gotoxy(1, 2);
		console.write("\x1b[L");
		var ret = interactive_or_string("           ", 1, 2);
		if (!ret)
			return ret;
		ret = interactive_or_string("Second Line", 1, 3);
		if (!ret)
			return ret;
		console.gotoxy(2, 4);
		console.write("\x1b[1L");
		ret = interactive_or_string("           ", 1, 4);
		if (!ret)
			return ret;
		ret = interactive_or_string("Third Line", 1, 5);
		if (!ret)
			return ret;
		console.gotoxy(3, 6);
		console.write("\x1b[2L");
		ret = interactive_or_string("           ", 1, 6);
		if (!ret)
			return ret;
		ret = interactive_or_string("           ", 1, 7);
		if (!ret)
			return ret;
		ret = interactive_or_string("Fourth Line", 1, 8);
		return ret;
	}},
	{'name':'DL', 'func':function() {
		console.clear();
		console.gotoxy(1, 1);
		console.write("First Line\r\nKillme\r\nSecond Line\r\nKillme too\r\nThird Line\r\nKillme also\r\nKillme last\r\nFourth Line\r\n");
		console.gotoxy(1, 2);
		console.write("\x1b[M");
		console.gotoxy(1, 3);
		console.write("\x1b[1M");
		console.gotoxy(1, 4);
		console.write("\x1b[2M");
		var ret = interactive_or_string("First Line", 1, 1);
		if (!ret)
			return ret;
		ret = interactive_or_string("Second Line", 1, 2);
		if (!ret)
			return ret;
		ret = interactive_or_string("Third Line", 1, 3);
		if (!ret)
			return ret;
		ret = interactive_or_string("Fourth Line", 1, 4);
		return ret;
	}},
	{'name':'CTSAM', 'func':function() {
		if (!interactive)
			return null;
		console.write("\x1b[=0M");
		console.write("\x1b[MFCDEFG\x0e\x0f");
		var ret = console.yesno("Did you hear an ascending set of five notes");
		if (ret)
			return false;
		console.write("\x1b[=2M");
		console.write("\x1b[MFCDEFG\x0e\x0f");
		var ret = console.yesno("Did you hear an ascending set of five notes");
		if (!ret)
			return false;
		console.write("\x1b[=1M");
		return true;
	}},
	{'name':'BCAM', 'func':function() {
		if (!interactive)
			return null;
		console.write("\x1b[NCDEFG\x0e\x0f");
		return ret = console.yesno("Did you hear an ascending set of five notes");
	}},
	{'name':'DCH', 'func':function() {
		console.gotoxy(1, 1);
		console.write("This  Line  Has   Single    Spaces");
		console.gotoxy(5, 1);
		console.write("\x1b[P");
		console.gotoxy(10, 1);
		console.write("\x1b[1P");
		console.gotoxy(14, 1);
		console.write("\x1b[2P");
		console.gotoxy(21, 1);
		console.write("\x1b[3P");
		return interactive_or_string("This Line Has Single Spaces", 1, 1);
	}},
	{'name':'SU', 'func':function() {
		console.clear();
		console.gotoxy(1, 1);
		console.write("First Line\r\nSecond Line\r\nThird Line\r\nFourth Line\r\nFifth Line\r\n");
		var ret = interactive_or_string("First Line", 1, 1);
		if (!ret)
			return ret;
		console.write("\x1b[S");
		ret = interactive_or_string("Second Line", 1, 1);
		if (!ret)
			return ret;
		console.write("\x1b[1S");
		ret = interactive_or_string("Third Line", 1, 1);
		if (!ret)
			return ret;
		console.write("\x1b[2S");
		ret = interactive_or_string("Fifth Line", 1, 1);
		return ret;
	}},
	{'name':'XTSRGA', 'func':function() {
		// TODO: XTerm, requires the second ; despite docs...
		console.write("\x1b[?2;1;S");
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
		console.clear();
		console.gotoxy(1, 1);
		console.write("First Line\r\n");
		var ret = interactive_or_string("First Line", 1, 1);
		if (!ret)
			return ret;
		console.write("\x1b[T");
		ret = interactive_or_string("First Line", 1, 2);
		if (!ret)
			return ret;
		console.write("\x1b[1T");
		ret = interactive_or_string("First Line", 1, 3);
		if (!ret)
			return ret;
		console.write("\x1b[2T");
		ret = interactive_or_string("First Line", 1, 5);
		return ret;
	}},
	{'name':'ECH', 'func':function() {
		console.clear();
		console.gotoxy(1, 1);
		console.write("One-Another-Two--Whee");
		console.gotoxy(4, 1);
		console.write("\x1b[X");
		console.gotoxy(12, 1);
		console.write("\x1b[1X");
		console.gotoxy(16, 1);
		console.write("\x1b[2X");
		return interactive_or_string("One Another Two  Whee", 1, 1);
	}},
	{'name':'CVT', 'func':function() {
		console.gotoxy(1,1);
		console.write("\t");
		var pos1 = fast_getxy();
		console.write("\t");
		var pos2 = fast_getxy();
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
		var pos1 = fast_getxy();
		console.write("\t");
		var pos2 = fast_getxy();
		console.write("\t");
		var pos3 = fast_getxy();
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
		var pos = fast_getxy();
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
		var pos = fast_getxy();
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
		if (!check_xy(console.screen_columns,1)) {
			console.write("\x1bc");
			return false;
		}
		console.write("\x1bc");
		return true;
	}},
	{'name':'BCDM', 'func':function() {
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[=255h\x00\x1b[1;1H\x1b[=255l");
		return check_xy(7, 1);
	}},
	{'name':'DECSET', 'func':function() {
		if (!interactive)
			return null;
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[?9h");
		console.write("Click the mouse anywhere on the screen (if that doesn't testing, press space)");
		var seq = read_ansi_seq(30000);
		console.gotoxy(1, 1);
		console.cleartoeol();
		console.write("\x1b[?9l");
		if (seq === null)
			return false;
		if (seq.search(/^\x1b\[M$/) === -1)
			return false;
		seq = console.inkey(500);
		if (seq.match(/^[ !"]$/) === -1)
			return false;
		seq = console.inkey(500);
		if (seq.match(/^[ -\xff]$/) === -1)
			return false;
		seq = console.inkey(500);
		if (seq.match(/^[ -\xff]$/) === -1)
			return false;
		return true;
		// TODO: Lots of stuff in here...
	}},
	{'name':'HPB', 'func':function() {
		console.gotoxy(5, 1);
		console.write("\x1b[j");
		if (!check_xy(4, 1))
			return false;
		console.write("\x1b[1j");
		if (!check_xy(3, 1))
			return false;
		console.write("\x1b[2j");
		if (!check_xy(1, 1))
			return false;
		return true;
	}},
	{'name':'VPB', 'func':function() {
		console.gotoxy(20, 10);
		console.write("\x1b[k");
		if (!check_xy(20, 9))
			return false;
		console.write("\x1b[1k");
		if (!check_xy(20, 8))
			return false;
		console.write("\x1b[2k");
		if (!check_xy(20, 6))
			return false;
		return true;
	}},
	{'name':'BCRST', 'func':function() {
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[=255h\x00\x1b[1;1H\x1b[=255l\x00\x1b[2;2H");
		return check_xy(2, 2);
	}},
	{'name':'DECRST', 'func':function() {
		console.gotoxy(console.screen_columns, 1);
		console.write("\x1b[?7l");
		console.write("This is all garbage here! ");
		console.write("\x1b[?7h");
		var ret = check_xy(console.screen_columns, 1);
		if (!ret)
			return false;
		return true;
		// TODO: Lots of stuff in here...
	}},
	{'name':'SGR', 'func':function() {
		if (!interactive)
			return null;
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[mDefault Attribute   \x1b[0mDefault Attribute   \x1b[0;1mBright              \x1b[0;2mDim                 ");
		console.gotoxy(1, 2);
		console.write("\x1b[0;5mSlow Blink          \x1b[0;6mFast Blink          \x1b[0;7mNegative Image      \x1b[0;8mConcealed           ");
		console.gotoxy(1, 3);
		console.write("\x1b[0;1;22mNormal Intensity    \x1b[0;5;25mNot Blinking        \x1b[0;47;30mBlack Foreground    \x1b[0;31mRed Foreground      ");
		console.gotoxy(1, 4);
		console.write("\x1b[0;32mGreen Foreground    \x1b[0;33mYellow Foreground   \x1b[0;34mBlue Foreground     \x1b[0;35mMagenta Foreground  ");
		console.gotoxy(1, 5);
		console.write("\x1b[0;36mCyan Foreground     \x1b[0;37mWhite Foreground    \x1b[0;40mBlack Background    \x1b[0;41mRed Background      ");
		console.gotoxy(1, 6);
		console.write("\x1b[0;42mGreen Background    \x1b[0;43mYellow Background   \x1b[0;44mBlue Background     \x1b[0;45mMagenta Background  ");
		console.gotoxy(1, 7);
		console.write("\x1b[0;46mCyan Background     \x1b[0;30;47mWhite Background    \x1b[39;49mDefault Attribute");
		console.gotoxy(1, 9);
		return console.yesno("Does the above look as described (fourth item in second row is concealed)");
		// TODO: Palette and direct-color selection
	}},
	{'name':'DSR', 'func':function() {
		console.write("\x1b[5n");
		var seq = read_ansi_seq(500);
		if (seq === null)
			return false;
		if (seq.search(/^\x1b\[[012]n$/) === -1)
			return false;
		console.gotoxy(1,1);
		if (!check_xy(1,1))
			return false;
		return true;
	}},
	{'name':'BCDSR', 'func':function() {
		console.write("\x1b[255n");
		var seq = read_ansi_seq(500);
		if (seq === null)
			return false;
		var m = seq.match(/^\x1b\[([0-9]+);([0-9]+)R$/);
		if (m === null)
			return false;
		if (parseInt(m[1], 10) !== console.screen_rows)
			return false;
		if (parseInt(m[2], 10) !== console.screen_columns)
			return false;
		return true;
	}},
	{'name':'CTSMRR', 'func':function() {
		console.write("\x1b[=1n");
		var seq = read_ansi_seq(500);
		if (seq === null)
			return false;
		if (seq.search(/^\x1b\[=1;[0-9]+;(?:0|1|99);[0-9]+;[0-9]+;[0-9]+;[0-9]+n$/) === -1)
			return false;
		console.write("\x1b[=2n");
		seq = read_ansi_seq(500);
		if (seq === null)
			return false;
		if (seq.search(/^\x1b\[=2;(?:[0-9]+;?)*n$/) === -1)
			return false;
		console.write("\x1b[=3n");
		seq = read_ansi_seq(500);
		if (seq === null)
			return false;
		if (seq.search(/^\x1b\[=3;[0-9]+;[0-9]+n$/) === -1)
			return false;
		return true;
	}},
	{'name':'DECDSR', 'func':function() {
		console.write("\x1b[?62n");
		var seq = read_ansi_seq(500);
		if (seq === null)
			return false;
		if (seq.search(/^\x1b\[[0-9]+\*\{$/) === -1)
			return false;
		console.write("\x1b[?63n");
		seq = read_ansi_string(500);
		if (seq === null)
			return false;
		if (seq.search(/^\x1bP[0-9]+!~[A-Z0-9]{4}\x1b\\$/) === -1)
			return false;
		console.write("\x1b[?63;2n");
		seq = read_ansi_string(500);
		if (seq === null)
			return false;
		if (seq.search(/^\x1bP2!~[A-Z0-9]{4}\x1b\\$/) === -1)
			return false;
		return true;
	}},
	{'name':'DECSTBM', 'func':function() {
		console.gotoxy(10, 10);
		console.write("\x1b[10;11r");
		if (!check_xy(1, 1)) {
			console.write("\x1b[r");
			return false;
		}
		console.gotoxy(10, 10);
		console.write("\r\n\r\n\r\n");
		if (!check_xy(1, 11)) {
			console.write("\x1b[r");
			return false;
		}
		console.write("\x1b[5A");
		if (!check_xy(1, 10)) {
			console.write("\x1b[r");
			return false;
		}
		console.write("\x1b[r");
		return true;
	}},
	{'name':'DECSCS', 'func':function() {
		var start = system.timer;
		console.write("\x1b[0;1*r\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x1b[0;0*r\x1b[6n");
		if (read_ansi_seq(2500) === null)
			return false;
		if ((system.timer - start) < 0.8)
			return false;
		return true;
	}},
	{'name':'CTSMS', 'func':function() {
		// TODO: Depends on other extensions...
		console.write("\x1bc\x1b[=2n");
		var seq = read_ansi_seq(500);
		if (seq === null)
			return null;
		var m = seq.match(/^\x1b\[=2;([0-9]+)[0-9;]*n$/);
		if (m === null)
			return null;
		console.write("\x1b[?s");
		console.write("\x1b[?"+m[1]+"l");
		console.write("\x1b[=2n");
		var seq2 = read_ansi_seq(500);
		if (seq2 === null) {
			console.write("\x1b[?"+m[1]+"h");
			return null;
		}
		if (seq2 === seq) {
			console.write("\x1b[?"+m[1]+"h");
			return null;
		}
		console.write("\x1b[?u");
		console.write("\x1b[=2n");
		console.write("\x1b[?"+m[1]+"h");
		seq2 = read_ansi_seq(500);
		if (seq2 === null) {
			console.write("\x1b[?"+m[1]+"h");
			return null;
		}
		if (seq2 === seq)
			return true;
		return false;
	}},
	{'name':'DECSLRM', 'func':function() {
		console.write("\x1b[?69h");
		console.write("\x1b[10;11s");
		if (!check_xy(1, 1)) {
			console.write("\x1bc");
			return false;
		}
		console.gotoxy(10, 10);
		console.write("\b\b\b");
		if (!check_xy(10, 10)) {
			console.write("\x1bc");
			return false;
		}
		console.write("\x1b[10C");
		if (!check_xy(11, 10)) {
			console.write("\x1bc");
			return false;
		}
		console.write("\x1b[s");
		console.write("\x1b[?69l");
		return true;
	}},
	{'name':'SCOSC', 'func':function() {
		console.gotoxy(2,2);
		console.write("\x1b[s");
		console.gotoxy(1,1);
		console.write('     ');
		console.write("\x1b[u");
		return check_xy(2,2);
	}},
	{'name':'CT24BC', 'func':function() {
		if (!interactive)
			return null;
		var i;
		console.clear();
		for (i = 0; i < console.screen_columns; i++) {
			console.gotoxy(1+i, 1)
			console.write("\x1b[0;"+Math.floor((255/console.screen_columns)*i)+";0;0t\x1b[1;"+(255-Math.floor((255/console.screen_columns)*i))+";0;0t=");
			console.gotoxy(1+i, 2)
			console.write("\x1b[0;0;"+Math.floor((255/console.screen_columns)*i)+";0t\x1b[1;0;"+(255-Math.floor((255/console.screen_columns)*i))+";0t=");
			console.gotoxy(1+i, 3)
			console.write("\x1b[0;0;0;"+Math.floor((255/console.screen_columns)*i)+"t\x1b[1;0;0;"+(255-Math.floor((255/console.screen_columns)*i))+"t=");
		}
		console.gotoxy(1, 5);
		return console.yesno("Do you see red, green, and blue gradients above");
	}},
	{'name':'CTRMS', 'func':function() {
		// TODO: Depends on other extensions...
		// TODO: Copy of CTSMS
		console.write("\x1bc\x1b[=2n");
		var seq = read_ansi_seq(500);
		if (seq === null)
			return null;
		var m = seq.match(/^\x1b\[=2;([0-9]+)[0-9;]*n$/);
		if (m === null)
			return null;
		console.write("\x1b[?s");
		console.write("\x1b[?"+m[1]+"l");
		console.write("\x1b[=2n");
		var seq2 = read_ansi_seq(500);
		if (seq2 === null) {
			console.write("\x1b[?"+m[1]+"h");
			return null;
		}
		if (seq2 === seq) {
			console.write("\x1b[?"+m[1]+"h");
			return null;
		}
		console.write("\x1b[?u");
		console.write("\x1b[=2n");
		console.write("\x1b[?"+m[1]+"h");
		seq2 = read_ansi_seq(500);
		if (seq2 === null) {
			console.write("\x1b[?"+m[1]+"h");
			return null;
		}
		if (seq2 === seq)
			return true;
		return false;
	}},
	{'name':'SCORC', 'func':function() {
		console.gotoxy(2,2);
		console.write("\x1b[s");
		console.gotoxy(1,1);
		console.write('     ');
		console.write("\x1b[u");
		return check_xy(2,2);
	}},
	{'name':'DECTABSR', 'func':function() {
		console.write("\x1b[2$w");
		var seq = read_ansi_string(500);
		if (seq === null)
			return false;
		if (seq.search(/^\x1bP2\$u(?:[0-9]\/?)*\x1b\\$/) === -1)
			return false;
		return true;
	}},
	{'name':'DECRQCRA', 'func':function() {
		console.write("\x1b[1;1;1;1;1;1*y");
		var ras = read_ansi_string(500);
		if (ras === null || ras.search(/^\x1bP1!~[A-Z0-9]{4}\x1b\\$/) !== -1)
			return true;
		return false;
	}},
	{'name':'DECINVM', 'func':function() {
		console.write("\x1bP1;0;1!z1B5B313B3148\x1b\\");
		console.gotoxy(2, 2);
		console.write("\x1b[1*z");
		return check_xy(1, 1);
	}},
	{'name':'CTOSF', 'func':function() {
		console.clear();
		console.gotoxy(1, 1);
		console.write("This is in this font...\r\n");
		var oldcs = get_cs(1, 1, 23, 1);
		if (oldcs == null)
			return null;
		var i;
		console.write("\x1b[=255;0{"+(Array(4097).join("\x10")));
		console.write("\x1b[=255;1{"+(Array(3585).join("\x10")));
		console.write("\x1b[=255;2{"+(Array(2049).join("\x10")));
		console.write("\x1b[0;255 D");
		console.gotoxy(1, 2);
		console.write("This is in this font...\r\n");
		var newcs = get_cs(1, 2, 23, 2);
		console.write("\x1b[0;0 D");
		if (newcs == null)
			return null;
		if (oldcs == newcs)
			return false;
		return true;
		// TODO: Interactive...
	}},
];

function main()
{
	var results = [];

	/*
	 * First, ensure that console.clear(), console.gotoxy(), and
	 * fast_getxy() work...
	 * 
	 * These are use by many of the movement tests.
	 */

	console.clear();
	var pos = fast_getxy();
	if (pos.x !== 1 || pos.y !== 1) {
		// Note: this is not required by the spec...
		throw("Clear screen doesn't home cursor");
	}
	console.writeln("Just some text");
	console.write("To move the cursor");
	pos = fast_getxy();
	if (pos.x !== 19 || pos.y !== 2) {
		throw("fast_getxy() error! " + JSON.stringify(pos));
	}
	console.gotoxy(3, 2);
	pos = fast_getxy();
	if (pos.x !== 3 || pos.y !== 2) {
		throw("console.gotoxy() error!");
	}

	tests.forEach(function(tst, idx) {
		results.push(tst.func());
	});

	return results;
}

var interactive = console.yesno("Run interactive tests");
var oldpt = console.ctrlkey_passthru;
console.ctrlkey_passthru = 0x7FFFFFFF;
console.write("\x1b[1;1;1;1;1;1*y");
var ras = read_ansi_string(500);
if (ras !== null && ras.search(/^\x1bP1!~[A-Z0-9]{4}\x1b\\$/) !== -1)
	has_cksum = true;
console.write("\x1bc");
var res = main();
console.write("\x1bc");
console.ctrlkey_passthru = oldpt;
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
