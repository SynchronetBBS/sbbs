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

function read_input_seq(timeout)
{
	var seq = '';
	var ch;

	ch = console.inkey(0, timeout);
	if (ch === '' || ch === null || ch === undefined)
		return null;
	seq += ch;

	// Single non-ESC byte (e.g. Backspace 0x08 or 0x7f)
	if (ch !== '\x1b')
		return seq;

	// ESC received — read continuation bytes
	for (;;) {
		ch = console.inkey(0, 200);
		if (ch === '' || ch === null || ch === undefined)
			break;
		seq += ch;

		// CSI sequence: ESC [ ... final_byte
		if (seq.length >= 3 && seq.charAt(1) === '[') {
			var last = seq.charCodeAt(seq.length - 1);
			// Final byte is 0x40-0x7E (@-~) but not in parameter range
			if (last >= 0x40 && last <= 0x7E)
				return seq;
			continue;
		}

		// SS3 sequence: ESC O <final>
		if (seq.length >= 3 && seq.charAt(1) === 'O') {
			var last = seq.charCodeAt(seq.length - 1);
			if (last >= 0x40 && last <= 0x7E)
				return seq;
			continue;
		}

		// VT-52 style: ESC <letter>
		if (seq.length === 2) {
			var c2 = seq.charCodeAt(1);
			// If it's [ or O, keep reading (CSI/SS3 introducer)
			if (c2 === 0x5B || c2 === 0x4F)
				continue;
			// Otherwise it's a 2-byte VT-52 sequence
			if (c2 >= 0x40 && c2 <= 0x7E)
				return seq;
			continue;
		}
	}

	// Drain any remaining bytes
	while (console.inkey(100));
	// Bare ESC (user pressed ESC to skip)
	if (seq.length === 1)
		return seq;
	return seq;
}

function seq_to_hex(seq)
{
	var parts = [];
	for (var i = 0; i < seq.length; i++) {
		var c = seq.charCodeAt(i);
		if (c >= 0x20 && c < 0x7f)
			parts.push(seq.charAt(i));
		else
			parts.push('\\x' + ('0' + c.toString(16)).slice(-2));
	}
	return parts.join('');
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
		console.putbyte(9);
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
		console.putbyte(9);
		return check_xy(3,1);
	}},
	{'name':"RI", 'func':function() {
		console.gotoxy(5, 2);
		console.write("\x1bM");
		return check_xy(5, 1);
	}},
	{'name':'APC', 'func':function() {
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
	{'name':'SOS', 'func':function() {
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
		console.putbyte(9);
		var pos1 = fast_getxy();
		console.putbyte(9);
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
		console.write("\x1b[MFCDEFG\x0e\x0f\x08 \x08");
		var ret = console.yesno("Did you hear an ascending set of five notes");
		if (ret)
			return false;
		console.write("\x1b[=2M");
		console.write("\x1b[MBCDEFG\x0e\x0f\x08 \x08");
		console.write("\x1b[MFCDEFG\x0e\x0f\x08 \x08");
		var ret = console.yesno("Did you hear two ascending sets of five notes");
		if (!ret)
			return false;
		console.write("\x1b[=1M");
		return true;
	}},
	{'name':'BCAM', 'func':function() {
		if (!interactive)
			return null;
		console.write("\x1b[NCDEFG\x0e\x0f\x08 \x08");
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
		console.putbyte(9);
		var pos1 = fast_getxy();
		console.putbyte(9);
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
		console.putbyte(9);
		var pos1 = fast_getxy();
		console.putbyte(9);
		var pos2 = fast_getxy();
		console.putbyte(9);
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
		console.putbyte(9);
		var pos = fast_getxy();
		console.write("\x1b["+pos.x+" d");
		console.gotoxy(1, 1);
		console.putbyte(9);
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
		console.putbyte(9);
		var pos = fast_getxy();
		console.write("\x1b[0g");
		console.gotoxy(1, 1);
		console.putbyte(9);
		if (check_xy(pos.x, pos.y))
			return false;
		console.write("\x1bc");
		console.write("\x1b[3g");
		console.gotoxy(console.screen_columns, 1);
		console.write("\x1bH");
		console.gotoxy(1,1);
		console.putbyte(9);
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
	{'name':'DECSC', 'func':function() {
		console.gotoxy(2,2);
		console.write("\x1b7");
		console.gotoxy(1,1);
		console.write('     ');
		console.write("\x1b8");
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
	{'name':'DECRC', 'func':function() {
		console.gotoxy(2,2);
		console.write("\x1b7");
		console.gotoxy(1,1);
		console.write('     ');
		console.write("\x1b8");
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
		if (ras !== null && ras.search(/^\x1bP1!~[A-Z0-9]{4}\x1b\\$/) !== -1)
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
	{'name':'CTELCF', 'func':function() {
		// Enable LCF mode: cursor should stick at last column
		console.write("\x1b[=4h");
		console.gotoxy(console.screen_columns, 1);
		console.write("X");
		// In LCF mode, cursor should stick at last column
		if (!check_xy(console.screen_columns, 1)) {
			console.write("\x1b[=4l");
			return false;
		}
		// Writing another char should wrap then advance past it
		console.write("Y");
		if (!check_xy(2, 2)) {
			console.write("\x1b[=4l");
			return false;
		}
		console.write("\x1b[=4l");
		return true;
	}},
	{'name':'CTDLCF', 'func':function() {
		// Default mode (LCF off): writing to last col wraps immediately
		console.write("\x1b[=4l");
		console.gotoxy(console.screen_columns, 1);
		console.write("X");
		// Without LCF, cursor wraps to next line immediately
		if (!check_xy(1, 2))
			return false;
		return true;
	}},
	{'name':'OriginMode', 'func':function() {
		// Set scroll region to rows 5-10, enable origin mode
		// DSR 6n reports relative to scroll region in origin mode
		console.write("\x1b[5;10r");
		console.write("\x1b[?6h");
		// CUP 1;1 should report as (1,1) relative to region
		console.write("\x1b[1;1H");
		var pos = fast_getxy();
		if (pos.x !== 1 || pos.y !== 1) {
			console.write("\x1b[?6l\x1b[r");
			return false;
		}
		// CUP to row 3, col 5 -> relative (5, 3)
		console.write("\x1b[3;5H");
		pos = fast_getxy();
		if (pos.x !== 5 || pos.y !== 3) {
			console.write("\x1b[?6l\x1b[r");
			return false;
		}
		// Cursor should be clamped to bottom of region (row 6 relative)
		console.write("\x1b[20;1H");
		pos = fast_getxy();
		if (pos.y !== 6) {
			console.write("\x1b[?6l\x1b[r");
			return false;
		}
		console.write("\x1b[?6l\x1b[r");
		return true;
	}},
	{'name':'CUUClamp', 'func':function() {
		// CUU should clamp at row 1
		console.gotoxy(5, 3);
		console.write("\x1b[99A");
		return check_xy(5, 1);
	}},
	{'name':'CUDClamp', 'func':function() {
		// CUD should clamp at bottom row
		console.gotoxy(5, 1);
		console.write(format("\x1b[%dB", console.screen_rows + 10));
		return check_xy(5, console.screen_rows);
	}},
	{'name':'CUFClamp', 'func':function() {
		// CUF should clamp at last column
		console.gotoxy(1, 1);
		console.write(format("\x1b[%dC", console.screen_columns + 10));
		return check_xy(console.screen_columns, 1);
	}},
	{'name':'CUBClamp', 'func':function() {
		// CUB should clamp at column 1
		console.gotoxy(5, 1);
		console.write("\x1b[99D");
		return check_xy(1, 1);
	}},
	{'name':'HPBClamp', 'func':function() {
		// HPB should clamp at column 1
		console.gotoxy(5, 1);
		console.write("\x1b[99j");
		return check_xy(1, 1);
	}},
	{'name':'VPBClamp', 'func':function() {
		// VPB should clamp at row 1
		console.gotoxy(5, 3);
		console.write("\x1b[99k");
		return check_xy(5, 1);
	}},
	{'name':'DECRQSS_m', 'func':function() {
		// Query SGR state after setting red foreground
		console.write("\x1b[0;31m");
		console.write("\x1bP$qm\x1b\\");
		var seq = read_ansi_string(500);
		console.write("\x1b[0m");
		if (seq === null)
			return false;
		// Response: DCS 1 $ r <SGR params> m ST
		if (seq.search(/^\x1bP1\$r.*m\x1b\\$/) === -1)
			return false;
		// Should contain 31 (red fg) somewhere in the params
		if (seq.search(/31/) === -1)
			return false;
		return true;
	}},
	{'name':'DECRQSS_r', 'func':function() {
		// Query top/bottom margins
		console.write("\x1b[5;15r");
		console.write("\x1bP$qr\x1b\\");
		var seq = read_ansi_string(500);
		console.write("\x1b[r");
		if (seq === null)
			return false;
		// Response: DCS 1 $ r 5;15 r ST
		if (seq.search(/^\x1bP1\$r5;15r\x1b\\$/) === -1)
			return false;
		return true;
	}},
	{'name':'DECRQSS_s', 'func':function() {
		// Query left/right margins
		console.write("\x1b[?69h");
		console.write("\x1b[5;75s");
		console.write("\x1bP$qs\x1b\\");
		var seq = read_ansi_string(500);
		console.write("\x1b[s\x1b[?69l");
		if (seq === null)
			return false;
		// Response: DCS 1 $ r 5;75 s ST
		if (seq.search(/^\x1bP1\$r5;75s\x1b\\$/) === -1)
			return false;
		return true;
	}},
	{'name':'CTSMRR_4', 'func':function() {
		// Query LCF mode status - should be 0 (disabled) by default
		console.write("\x1b[=4l");
		console.write("\x1b[=4n");
		var seq = read_ansi_seq(500);
		if (seq === null)
			return false;
		if (seq.search(/^\x1b\[=4;0n$/) === -1)
			return false;
		// Enable LCF, query again
		console.write("\x1b[=4h");
		console.write("\x1b[=4n");
		seq = read_ansi_seq(500);
		console.write("\x1b[=4l");
		if (seq === null)
			return false;
		if (seq.search(/^\x1b\[=4;1n$/) === -1)
			return false;
		return true;
	}},
	{'name':'CTSMRR_5', 'func':function() {
		// Query LCF forced status - should be 0 by default
		console.write("\x1b[=5n");
		var seq = read_ansi_seq(500);
		if (seq === null)
			return false;
		if (seq.search(/^\x1b\[=5;[01]n$/) === -1)
			return false;
		return true;
	}},
	{'name':'CTSV', 'func':function() {
		// Query SyncTERM version via APC
		console.write("\x1b_SyncTERM:VER\x1b\\");
		var seq = read_ansi_string(500);
		if (seq === null)
			return false;
		// Response: APC SyncTERM:VER;SyncTERM <version> ST
		if (seq.search(/^\x1b_SyncTERM:VER;SyncTERM /) === -1)
			return false;
		return true;
	}},
	{'name':'CTQJS', 'func':function() {
		// Query JXL support
		console.write("\x1b_SyncTERM:Q;JXL\x1b\\");
		var seq = read_ansi_seq(500);
		if (seq === null)
			return false;
		// Response: CSI = 1 ; pR - n  (pR is 0 or 1, note '-' intermediate)
		if (seq.search(/^\x1b\[=1;[01]-n$/) === -1)
			return false;
		return true;
	}},
	{'name':'SGR256FG', 'func':function() {
		// Test 256-color palette foreground (SGR 38;5;N)
		if (!has_cksum)
			return null;
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[0m#");
		var cs1 = get_cs(1, 1, 1, 1);
		console.gotoxy(1, 1);
		console.write("\x1b[38;5;196m#");
		var cs2 = get_cs(1, 1, 1, 1);
		console.write("\x1b[0m");
		if (cs1 === null || cs2 === null)
			return null;
		if (cs1 === cs2)
			return false;
		return true;
	}},
	{'name':'SGR256BG', 'func':function() {
		// Test 256-color palette background (SGR 48;5;N)
		if (!has_cksum)
			return null;
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[0m ");
		var cs1 = get_cs(1, 1, 1, 1);
		console.gotoxy(1, 1);
		console.write("\x1b[48;5;21m ");
		var cs2 = get_cs(1, 1, 1, 1);
		console.write("\x1b[0m");
		if (cs1 === null || cs2 === null)
			return null;
		if (cs1 === cs2)
			return false;
		return true;
	}},
	{'name':'SGRRGBFG', 'func':function() {
		// Test 24-bit direct color foreground (SGR 38;2;R;G;B)
		if (!has_cksum)
			return null;
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[0m#");
		var cs1 = get_cs(1, 1, 1, 1);
		console.gotoxy(1, 1);
		console.write("\x1b[38;2;255;128;0m#");
		var cs2 = get_cs(1, 1, 1, 1);
		console.write("\x1b[0m");
		if (cs1 === null || cs2 === null)
			return null;
		if (cs1 === cs2)
			return false;
		return true;
	}},
	{'name':'SGRRGBBG', 'func':function() {
		// Test 24-bit direct color background (SGR 48;2;R;G;B)
		if (!has_cksum)
			return null;
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[0m ");
		var cs1 = get_cs(1, 1, 1, 1);
		console.gotoxy(1, 1);
		console.write("\x1b[48;2;0;255;128m ");
		var cs2 = get_cs(1, 1, 1, 1);
		console.write("\x1b[0m");
		if (cs1 === null || cs2 === null)
			return null;
		if (cs1 === cs2)
			return false;
		return true;
	}},
	{'name':'SGRBright', 'func':function() {
		// Test bright foreground colors 91-97 and bright background 100-107
		// Per spec, 100-107 require bright bg enabled (DECSET ?33)
		if (!has_cksum)
			return null;
		console.clear();
		// Normal red vs bright red foreground
		console.gotoxy(1, 1);
		console.write("\x1b[31m#");
		var cs_norm = get_cs(1, 1, 1, 1);
		console.gotoxy(1, 1);
		console.write("\x1b[91m#");
		var cs_bright = get_cs(1, 1, 1, 1);
		if (cs_norm === null || cs_bright === null) {
			console.write("\x1b[0m");
			return null;
		}
		if (cs_norm === cs_bright) {
			console.write("\x1b[0m");
			return false;
		}
		// Normal red vs bright red background (needs DECSET ?33)
		console.write("\x1b[?33h");
		console.gotoxy(1, 1);
		console.write("\x1b[41m ");
		cs_norm = get_cs(1, 1, 1, 1);
		console.gotoxy(1, 1);
		console.write("\x1b[101m ");
		cs_bright = get_cs(1, 1, 1, 1);
		console.write("\x1b[?33l\x1b[0m");
		if (cs_norm === null || cs_bright === null)
			return null;
		if (cs_norm === cs_bright)
			return false;
		return true;
	}},
	{'name':'SGRNeg', 'func':function() {
		// Test negative image (SGR 7) and positive image (SGR 27) roundtrip
		if (!has_cksum)
			return null;
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[0;37;40m#");
		var cs_normal = get_cs(1, 1, 1, 1);
		console.gotoxy(1, 1);
		console.write("\x1b[0;37;40;7m#");
		var cs_neg = get_cs(1, 1, 1, 1);
		// Negative image should look different
		if (cs_normal === null || cs_neg === null) {
			console.write("\x1b[0m");
			return null;
		}
		if (cs_normal === cs_neg) {
			console.write("\x1b[0m");
			return false;
		}
		// Positive image (27) should restore
		console.gotoxy(1, 1);
		console.write("\x1b[0;37;40;7;27m#");
		var cs_pos = get_cs(1, 1, 1, 1);
		console.write("\x1b[0m");
		if (cs_pos === null)
			return null;
		if (cs_pos !== cs_normal)
			return false;
		return true;
	}},
	{'name':'SGRConceal', 'func':function() {
		// Test concealed text (SGR 8) - fg set to bg color
		if (!has_cksum)
			return null;
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[0m#");
		var cs_visible = get_cs(1, 1, 1, 1);
		console.gotoxy(1, 1);
		console.write("\x1b[0;8m#");
		var cs_hidden = get_cs(1, 1, 1, 1);
		console.write("\x1b[0m");
		if (cs_visible === null || cs_hidden === null)
			return null;
		// Concealed should look different (fg becomes bg)
		if (cs_visible === cs_hidden)
			return false;
		return true;
	}},
	{'name':'OSC4', 'func':function() {
		// Test palette redefinition and reset
		// DECRQCRA checksums attribute indices, not rendered colors,
		// so palette changes are invisible to it. Use interactive.
		if (!interactive)
			return null;
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[0;31m################\x1b[0m  <-- This should be RED");
		console.gotoxy(1, 3);
		var ret = console.yesno("Is the text above red");
		if (!ret)
			return false;
		// Redefine palette entry 1 (red) to green
		console.write("\x1b]4;1;rgb:00/FF/00\x1b\\");
		console.gotoxy(1, 3);
		console.cleartoeol();
		ret = console.yesno("Did the text above change to green");
		if (!ret) {
			console.write("\x1b]104;1\x1b\\");
			return false;
		}
		// Reset palette
		console.write("\x1b]104;1\x1b\\");
		console.gotoxy(1, 3);
		console.cleartoeol();
		ret = console.yesno("Did the text above change back to red");
		if (!ret)
			return false;
		console.write("\x1b[0m");
		return true;
	}},
	{'name':'OSC104All', 'func':function() {
		// Test OSC 104 with no args resets all palette entries
		if (!interactive)
			return null;
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[0;34m################\x1b[0m  <-- This should be BLUE");
		// Redefine blue to yellow
		console.write("\x1b]4;4;rgb:FF/FF/00\x1b\\");
		console.gotoxy(1, 3);
		var ret = console.yesno("Did the text above change to yellow");
		if (!ret) {
			console.write("\x1b]104\x1b\\");
			return false;
		}
		// Reset ALL palette entries
		console.write("\x1b]104\x1b\\");
		console.gotoxy(1, 3);
		console.cleartoeol();
		ret = console.yesno("Did the text above change back to blue");
		if (!ret)
			return false;
		console.write("\x1b[0m");
		return true;
	}},
	{'name':'DECSCUSR', 'func':function() {
		if (!interactive)
			return null;
		console.clear();
		console.gotoxy(1, 2);
		console.write("\x1b[1 q");
		console.write("Blinking block cursor");
		console.gotoxy(1, 3);
		var ret = console.yesno("Is the cursor a blinking block");
		if (!ret)
			return false;
		console.gotoxy(1, 2);
		console.cleartoeol();
		console.write("\x1b[4 q");
		console.write("Steady underline cursor");
		console.gotoxy(1, 3);
		ret = console.yesno("Is the cursor a steady underline");
		console.write("\x1b[1 q");
		if (!ret)
			return false;
		return true;
	}},
	{'name':'AutoWrap', 'func':function() {
		// Test that autowrap disabled (CSI ?7l) prevents wrapping
		console.write("\x1b[?7l");
		console.gotoxy(console.screen_columns, 1);
		console.write("ABCDEF");
		// Should still be at last column
		if (!check_xy(console.screen_columns, 1)) {
			console.write("\x1b[?7h");
			return false;
		}
		// Re-enable autowrap
		console.write("\x1b[?7h");
		// Verify it works: write to last column, should wrap
		console.gotoxy(console.screen_columns, 1);
		console.write("AB");
		// With default (non-LCF) mode, first char wraps, second advances
		var pos = fast_getxy();
		if (pos.y < 2) {
			return false;
		}
		return true;
	}},
	{'name':'RIScroll', 'func':function() {
		// RI at top of scroll region should scroll down
		console.clear();
		console.gotoxy(1, 1);
		console.write("TopLine");
		console.gotoxy(1, 1);
		console.write("\x1bM");
		// Cursor should still be at row 1
		if (!check_xy(1, 1))
			return false;
		// "TopLine" should have moved to row 2
		return interactive_or_string("TopLine", 1, 2);
	}},
	{'name':'LFScroll', 'func':function() {
		// LF at bottom should scroll up
		console.clear();
		console.gotoxy(1, 1);
		console.write("ScrollMe");
		console.gotoxy(1, console.screen_rows);
		console.write("\n");
		// "ScrollMe" should have moved up (now on row 0, gone)
		// But we can verify cursor is still at bottom
		var pos = fast_getxy();
		if (pos.y !== console.screen_rows)
			return false;
		return true;
	}},
	{'name':'DECSETBlinkBG', 'func':function() {
		// DECSET ?33: blink bit controls background intensity
		if (!has_cksum)
			return null;
		console.clear();
		// Normal blink
		console.gotoxy(1, 1);
		console.write("\x1b[0;5;41m ");
		var cs_blink = get_cs(1, 1, 1, 1);
		// Enable blink-to-bright-bg mode
		console.write("\x1b[?33h");
		console.gotoxy(1, 1);
		console.write("\x1b[0;5;41m ");
		var cs_brightbg = get_cs(1, 1, 1, 1);
		console.write("\x1b[?33l\x1b[0m");
		if (cs_blink === null || cs_brightbg === null)
			return null;
		// With blink-to-bright-bg, the appearance should differ
		if (cs_blink === cs_brightbg)
			return false;
		return true;
	}},
	{'name':'CopyPaste', 'func':function() {
		// Test Copy screen region + Paste it elsewhere
		// Copy/Paste operates on the pixel framebuffer, not the text
		// cell buffer, so DECRQCRA can't verify it. Use interactive.
		if (!interactive)
			return null;
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[0;1;33mCopyTest123\x1b[0m");
		// Copy entire screen to buffer
		console.write("\x1b_SyncTERM:P;Copy\x1b\\");
		// Clear and paste at an offset
		console.clear();
		console.write("\x1b_SyncTERM:P;Paste\x1b\\");
		mswait(200);
		console.gotoxy(1, 3);
		return console.yesno("Do you see 'CopyTest123' on the first line");
	}},
	{'name':'DECDMAC', 'func':function() {
		// Define a macro that moves cursor to 3,3
		// Hex-encoded: 1B 5B 33 3B 33 48 -> "ESC [ 3 ; 3 H"
		// Note: ASCII-mode (p3=0) cannot contain ESC (not in 0x20-0x7e),
		// so only hex-mode (p3=1) can encode escape sequences.
		console.write("\x1bP2;0;1!z1B5B333B3348\x1b\\");
		console.gotoxy(1, 1);
		// Invoke macro 2
		console.write("\x1b[2*z");
		var pos = fast_getxy();
		if (!check_xy(3, 3))
			return false;
		// Define a second macro (hex) that moves to 5,5
		// Hex-encoded: 1B 5B 35 3B 35 48 -> "ESC [ 5 ; 5 H"
		console.write("\x1bP3;0;1!z1B5B353B3548\x1b\\");
		console.gotoxy(1, 1);
		console.write("\x1b[3*z");
		pos = fast_getxy();
		if (!check_xy(5, 5))
			return false;
		return true;
	}},
	{'name':'DECMSR', 'func':function() {
		// DECDSR ?62: Macro Space Report
		console.write("\x1b[?62n");
		var seq = read_ansi_seq(500);
		if (seq === null)
			return false;
		// Response: CSI 32767 * {
		if (seq.search(/^\x1b\[[0-9]+\*\{$/) === -1)
			return false;
		return true;
	}},
	{'name':'DECCKSR', 'func':function() {
		// DECDSR ?63: Macro Checksum Report
		console.write("\x1b[?63;1n");
		var seq = read_ansi_string(500);
		if (seq === null)
			return false;
		// Response: DCS 1 ! ~ xxxx ST
		if (seq.search(/^\x1bP1!~[A-Z0-9]{4}\x1b\\$/) === -1)
			return false;
		return true;
	}},
	{'name':'DECSTBMScroll', 'func':function() {
		// Test that scrolling is confined to the scroll region
		console.clear();
		console.gotoxy(1, 1);
		console.write("StayHere");
		// Set scroll region to rows 3-6
		console.write("\x1b[3;6r");
		console.gotoxy(1, 3);
		console.write("Line3\r\nLine4\r\nLine5\r\nLine6");
		// One more LF should scroll within the region
		console.write("\r\n");
		// "StayHere" on row 1 should be unaffected
		var ret = interactive_or_string("StayHere", 1, 1);
		console.write("\x1b[r");
		if (!ret)
			return ret;
		// Line3 should have scrolled off, Line4 now at row 3
		ret = interactive_or_string("Line4", 1, 3);
		console.write("\x1b[r");
		return ret;
	}},
	{'name':'DECSLRMScroll', 'func':function() {
		// Test left/right margin scrolling
		console.write("\x1b[?69h");
		console.write("\x1b[5;15s");
		console.clear();
		console.gotoxy(5, 1);
		console.write("LeftRight!");
		// ICH inside margins should shift within them
		console.gotoxy(5, 1);
		console.write("\x1b[@");
		var ret = interactive_or_string(" LeftRight", 5, 1);
		console.write("\x1b[s\x1b[?69l");
		return ret;
	}},
	{'name':'SGRDim', 'func':function() {
		// SGR 2 (dim) clears the bold/bright bit in PC text mode.
		// There is no separate "dim" attribute — it just means "not bright".
		// Verify that SGR 2 clears a previously set SGR 1 (bright).
		console.write("\x1b[0;1;2m");
		console.write("\x1bP$qm\x1b\\");
		var seq = read_ansi_string(500);
		console.write("\x1b[0m");
		if (seq === null)
			return false;
		// Response should be a valid DECRQSS response
		if (seq.search(/^\x1bP1\$r/) === -1)
			return false;
		// SGR 1 (bold) should NOT be present — dim cleared it
		if (seq.search(/;1[;m]/) !== -1)
			return false;
		return true;
	}},
	{'name':'CR', 'func':function() {
		// CR moves cursor to column 1 of the current line
		console.gotoxy(10, 5);
		console.write("\r");
		if (!check_xy(1, 5))
			return false;
		// Already at column 1, should stay
		console.write("\r");
		if (!check_xy(1, 5))
			return false;
		return true;
	}},
	{'name':'LF', 'func':function() {
		// LF moves cursor to same column of the next row
		console.gotoxy(5, 3);
		console.write("\n");
		if (!check_xy(5, 4))
			return false;
		// LF on last row should scroll and stay on last row
		console.gotoxy(5, console.screen_rows);
		console.write("\n");
		if (!check_xy(5, console.screen_rows))
			return false;
		return true;
	}},
	{'name':'BS', 'func':function() {
		// BS is non-destructive backspace; moves left unless at column 1
		console.gotoxy(5, 1);
		console.write("\x08");
		if (!check_xy(4, 1))
			return false;
		// At column 1, BS should not move
		console.gotoxy(1, 1);
		console.write("\x08");
		if (!check_xy(1, 1))
			return false;
		return true;
	}},
	{'name':'HT', 'func':function() {
		// HT moves to next horizontal tab stop without overwriting
		// Default tab stops are at every 8th column (9, 17, 25, ...)
		console.gotoxy(1, 1);
		console.write("\t");
		if (!check_xy(9, 1))
			return false;
		console.write("\t");
		if (!check_xy(17, 1))
			return false;
		return true;
	}},
	{'name':'NEL', 'func':function() {
		// ESC E (Next Line) = same as CR + LF
		console.gotoxy(10, 3);
		console.write("\x1bE");
		if (!check_xy(1, 4))
			return false;
		return true;
	}},
	{'name':'RI', 'func':function() {
		// ESC M (Reverse Index) moves up one line
		console.gotoxy(5, 5);
		console.write("\x1bM");
		if (!check_xy(5, 4))
			return false;
		return true;
	}},
	{'name':'HVP', 'func':function() {
		// CSI Pn1 ; Pn2 f — same as CUP but with 'f' final byte
		console.write("\x1b[5;10f");
		if (!check_xy(10, 5))
			return false;
		// Defaults: Pn1=1, Pn2=1
		console.write("\x1b[f");
		if (!check_xy(1, 1))
			return false;
		return true;
	}},
	{'name':'HTS', 'func':function() {
		// ESC H sets a tab stop at the current column
		// Clear all tab stops first
		console.write("\x1b[3g");
		// Set a tab stop at column 5
		console.gotoxy(5, 1);
		console.write("\x1bH");
		// Set a tab stop at column 20
		console.gotoxy(20, 1);
		console.write("\x1bH");
		// Verify via DECTABSR
		console.write("\x1b[2$w");
		var seq = read_ansi_string(500);
		if (seq === null)
			return false;
		// Response should list stops at 5 and 20
		if (seq.search(/^\x1bP2\$u5\/20\x1b\\$/) === -1)
			return false;
		// Restore default tab stops (set every 8 columns)
		console.write("\x1b[3g");
		for (var i = 9; i <= console.screen_columns; i += 8) {
			console.gotoxy(i, 1);
			console.write("\x1bH");
		}
		return true;
	}},
	{'name':'LCFFullLine', 'func':function() {
		// Write a full line (screen_columns chars) followed by CRLF.
		// In LCF mode, cursor sticks at last column, so CR goes to
		// col 1 of same row, LF goes to next row — no blank line.
		// In default mode, writing the last char wraps immediately
		// to the next row, so CR+LF advances one more — blank line.
		var w = console.screen_columns;
		var line = "";
		for (var i = 0; i < w; i++)
			line += "A";

		// Test with LCF enabled — should end up on the row right after
		console.write("\x1b[=4h");
		console.clear();
		console.gotoxy(1, 1);
		console.write(line);
		// Cursor should be stuck at last column of row 1
		if (!check_xy(w, 1)) {
			console.write("\x1b[=4l");
			return false;
		}
		console.write("\r\n");
		// Should be at col 1, row 2 (no blank line)
		if (!check_xy(1, 2)) {
			console.write("\x1b[=4l");
			return false;
		}
		console.write("\x1b[=4l");

		// Test with LCF disabled (default) — should skip a line
		console.clear();
		console.gotoxy(1, 1);
		console.write(line);
		// Last char wraps immediately, cursor is at col 1, row 2
		if (!check_xy(1, 2))
			return false;
		console.write("\r\n");
		// CR is no-op (already col 1), LF goes to row 3 — blank line
		if (!check_xy(1, 3))
			return false;
		return true;
	}},
	{'name':'LCFDSR', 'func':function() {
		// Test what DSR 6n reports when LCF is set.
		// LCF is set when a printable char advances cursor to right margin.
		var w = console.screen_columns;
		console.write("\x1b[=4h");
		console.gotoxy(w - 1, 1);
		// Write 2 chars: first fills col w-1, second fills col w and sets LCF
		console.write("AB");
		var pos = fast_getxy();
		console.write("\x1b[=4l");
		// The cursor should report the last column, not last+1
		if (pos.x !== w || pos.y !== 1)
			return false;
		return true;
	}},
	{'name':'Regressions', 'func':function() {
		// Fixed by 3dfa12a6cac
		// Deleting lines could move rows that are off the screen
		// in a box that has a negative height
		console.clear();
		console.gotoxy(1, 2);
		console.write(format("\x1b[2;%dr", console.screen_rows - 1));
		console.gotoxy(1, 2);
		console.write(format("\x1b[%dM", console.screen_rows - 1));
		return true;
	}},
	/* --- DEC Rectangular Area Operations --- */
	{'name':'DECERA', 'func':function() {
		console.clear();
		console.gotoxy(1, 1);
		console.write("ABCDEFGHIJ");
		console.gotoxy(1, 2);
		console.write("KLMNOPQRST");
		// Erase cols 3-8, rows 1-2
		console.write("\x1b[1;3;2;8$z");
		return interactive_or_string("AB      IJ", 1, 1);
	}},
	{'name':'DECFRA', 'func':function() {
		console.clear();
		console.gotoxy(1, 1);
		console.write("ABCDEFGHIJ");
		// Fill cols 3-7, rows 1-1 with '#' (35)
		console.write("\x1b[35;1;3;1;7$x");
		return interactive_or_string("AB#####HIJ", 1, 1);
	}},
	{'name':'DECCRA', 'func':function() {
		console.clear();
		console.gotoxy(1, 1);
		console.write("HELLO");
		// Copy row 1 cols 1-5 to row 3 col 10
		console.write("\x1b[1;1;1;5;1;3;10;1$v");
		return interactive_or_string("HELLO", 10, 3);
	}},
	{'name':'DECIC', 'func':function() {
		// Enable left/right margins
		console.clear();
		console.write("\x1b[?69h\x1b[1;20s");
		console.gotoxy(1, 1);
		console.write("ABCDEFGHIJ");
		console.gotoxy(3, 1);
		console.write("\x1b[2'}");  // Insert 2 columns
		var ret = interactive_or_string("AB  CDEFGH", 1, 1);
		console.write("\x1b[s\x1b[?69l");
		return ret;
	}},
	{'name':'DECDC', 'func':function() {
		console.clear();
		console.write("\x1b[?69h\x1b[1;20s");
		console.gotoxy(1, 1);
		console.write("ABCDEFGHIJ");
		console.gotoxy(3, 1);
		console.write("\x1b[2'~");  // Delete 2 columns
		var ret = interactive_or_string("ABEFGHIJ", 1, 1);
		console.write("\x1b[s\x1b[?69l");
		return ret;
	}},
	/* --- DECCARA / DECRARA / DECSACE --- */
	{'name':'DECCARA', 'func':function() {
		if (!has_cksum) return null;
		console.clear();
		console.write("\x1b[0m");
		console.gotoxy(1, 1);
		console.write("ABCDE");
		var cs1 = get_cs(1, 1, 5, 1);
		// Apply bold via DECCARA
		console.write("\x1b[1;1;1;5;1$r");
		var cs2 = get_cs(1, 1, 5, 1);
		if (cs1 === cs2)
			return false;
		// Undo bold via DECCARA SGR 22
		console.write("\x1b[1;1;1;5;22$r");
		var cs3 = get_cs(1, 1, 5, 1);
		return (cs1 === cs3);
	}},
	{'name':'DECRARA', 'func':function() {
		if (!has_cksum) return null;
		console.clear();
		console.write("\x1b[0m");
		console.gotoxy(1, 1);
		console.write("ABCDE");
		var cs1 = get_cs(1, 1, 5, 1);
		// Toggle bold via DECRARA
		console.write("\x1b[1;1;1;5;1$t");
		var cs2 = get_cs(1, 1, 5, 1);
		if (cs1 === cs2)
			return false;
		// Toggle again — should revert
		console.write("\x1b[1;1;1;5;1$t");
		var cs3 = get_cs(1, 1, 5, 1);
		return (cs1 === cs3);
	}},
	{'name':'DECSACE', 'func':function() {
		if (!has_cksum) return null;
		console.clear();
		console.write("\x1b[0m");
		// Set stream mode
		console.write("\x1b[1*x");
		console.gotoxy(1, 1);
		console.write("LINE1TEXT.");
		console.gotoxy(1, 2);
		console.write("LINE2TEXT.");
		var cs1 = get_cs(6, 1, 10, 1);
		// DECCARA stream: row 1 col 6 to row 2 col 5, bold
		console.write("\x1b[1;6;2;5;1$r");
		var cs2 = get_cs(6, 1, 10, 1);
		console.write("\x1b[0*x");  // reset
		return (cs1 !== cs2);
	}},
	/* --- CT24BC (24-bit color) --- */
	{'name':'CT24BC', 'func':function() {
		if (!has_cksum) return null;
		console.clear();
		console.write("\x1b[0m");
		console.gotoxy(1, 1);
		console.write("X");
		var cs1 = get_cs(1, 1, 1, 1);
		console.gotoxy(1, 1);
		console.write("\x1b[1;255;128;0t");  // fg orange
		console.write("X");
		var cs2 = get_cs(1, 1, 1, 1);
		console.write("\x1b[0m");
		return (cs1 !== cs2);
	}},
	/* --- FETM/TTM --- */
	{'name':'FETM', 'func':function() {
		// Set FETM=EXCLUDE, verify via DECRQM
		console.write("\x1b[14h");
		console.write("\x1b[14$p");
		var seq = read_ansi_seq(500);
		console.write("\x1b[14l");  // reset
		if (seq === null) return null;
		// Response should indicate mode 14 is set (Pm=1 or 3)
		var m = seq.match(/\x1b\[14;([0-9]+)\$y/);
		return (m !== null && (m[1] === '1' || m[1] === '3'));
	}},
	{'name':'TTM', 'func':function() {
		console.write("\x1b[16h");
		console.write("\x1b[16$p");
		var seq = read_ansi_seq(500);
		console.write("\x1b[16l");
		if (seq === null) return null;
		var m = seq.match(/\x1b\[16;([0-9]+)\$y/);
		return (m !== null && (m[1] === '1' || m[1] === '3'));
	}},
	/* --- Vertical Line Tabulation --- */
	{'name':'CVTwithVTS', 'func':function() {
		console.clear();
		// Set a line tab stop at row 10
		console.gotoxy(3, 10);
		console.write("\x1bJ");  // VTS
		console.gotoxy(5, 1);
		console.write("\x1b[Y");  // CVT
		var pos = fast_getxy();
		// Clear line tabs
		console.write("\x1b[4g");
		return (pos.x === 5 && pos.y === 10);
	}},
	/* --- Origin Mode --- */
	{'name':'OriginMode', 'func':function() {
		console.clear();
		console.write("\x1b[5;20r");  // DECSTBM 5-20
		console.write("\x1b[?6h");    // origin on
		console.gotoxy(1, 1);         // should be row 5 abs
		console.write("X");
		console.write("\x1b[?6l");    // origin off
		console.write("\x1b[r");      // reset margins
		return interactive_or_string("X", 1, 5);
	}},
	/* --- SL/SR --- */
	{'name':'SLMargins', 'func':function() {
		console.clear();
		console.write("\x1b[?69h\x1b[5;15s");
		console.gotoxy(5, 1);
		console.write("ABCDEFGHIJK");
		console.write("\x1b[2 @");  // SL 2
		var ret = interactive_or_string("CDE", 5, 1);
		console.write("\x1b[s\x1b[?69l");
		return ret;
	}},
	{'name':'SRMargins', 'func':function() {
		console.clear();
		console.write("\x1b[?69h\x1b[5;15s");
		console.gotoxy(5, 1);
		console.write("ABCDEFGHIJK");
		console.write("\x1b[2 A");  // SR 2
		var ret = interactive_or_string("ABC", 7, 1);
		console.write("\x1b[s\x1b[?69l");
		return ret;
	}},
	/* --- Save/Restore Mode --- */
	{'name':'CTSMS_CTRMS', 'func':function() {
		// Save autowrap (on), disable, verify, restore, verify
		console.write("\x1b[?7s");   // save
		console.write("\x1b[?7l");   // disable
		console.write("\x1b[?7$p");  // query
		var seq1 = read_ansi_seq(500);
		console.write("\x1b[?7u");   // restore
		console.write("\x1b[?7$p");  // query
		var seq2 = read_ansi_seq(500);
		if (seq1 === null || seq2 === null) return null;
		// First query: should be reset (Pm=2)
		var m1 = seq1.match(/\x1b\[\?7;([0-9]+)\$y/);
		var m2 = seq2.match(/\x1b\[\?7;([0-9]+)\$y/);
		if (m1 === null || m2 === null) return null;
		return (m1[1] === '2' && (m2[1] === '1' || m2[1] === '3'));
	}},
	/* --- SGR Extended --- */
	{'name':'SGRNoBlink', 'func':function() {
		if (!has_cksum) return null;
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[0m#");
		var cs1 = get_cs(1, 1, 1, 1);
		console.gotoxy(1, 1);
		console.write("\x1b[0;5;25m#");
		var cs2 = get_cs(1, 1, 1, 1);
		console.write("\x1b[0m");
		return (cs1 === cs2);
	}},
	{'name':'SGRNormInt', 'func':function() {
		if (!has_cksum) return null;
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[0m#");
		var cs1 = get_cs(1, 1, 1, 1);
		console.gotoxy(1, 1);
		console.write("\x1b[0;1;22m#");
		var cs2 = get_cs(1, 1, 1, 1);
		console.write("\x1b[0m");
		return (cs1 === cs2);
	}},
	{'name':'SGRDefFG', 'func':function() {
		if (!has_cksum) return null;
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[0;37m#");
		var cs1 = get_cs(1, 1, 1, 1);
		console.gotoxy(1, 1);
		console.write("\x1b[0;39m#");
		var cs2 = get_cs(1, 1, 1, 1);
		console.write("\x1b[0m");
		return (cs1 === cs2);
	}},
	{'name':'SGRDefBG', 'func':function() {
		if (!has_cksum) return null;
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[0;40m ");
		var cs1 = get_cs(1, 1, 1, 1);
		console.gotoxy(1, 1);
		console.write("\x1b[0;49m ");
		var cs2 = get_cs(1, 1, 1, 1);
		console.write("\x1b[0m");
		return (cs1 === cs2);
	}},
	{'name':'SGRBrightBG', 'func':function() {
		if (!has_cksum) return null;
		console.write("\x1b[?33h");
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b[0;41m ");
		var cs1 = get_cs(1, 1, 1, 1);
		console.gotoxy(1, 1);
		console.write("\x1b[0;101m ");
		var cs2 = get_cs(1, 1, 1, 1);
		console.write("\x1b[0m\x1b[?33l");
		return (cs1 !== cs2);
	}},
	/* --- DECRQSS extensions --- */
	{'name':'DECRQSS_*x', 'func':function() {
		console.write("\x1bP$q*x\x1b\\");
		var seq = read_ansi_string(500);
		if (seq === null) return null;
		return (seq.indexOf('*x') >= 0);
	}},
	{'name':'DECRQSS_*r', 'func':function() {
		console.write("\x1bP$q*r\x1b\\");
		var seq = read_ansi_string(500);
		if (seq === null) return null;
		return (seq.indexOf('*r') >= 0);
	}},
	/* --- OSC 8 Hyperlink --- */
	{'name':'OSC8', 'func':function() {
		if (!interactive)
			return null;
		console.clear();
		console.gotoxy(1, 1);
		console.write("\x1b]8;id=test;https://example.com\x1b\\");
		console.write("LINK");
		console.write("\x1b]8;;\x1b\\");
		return console.yesno("Do you see the text 'LINK' (possibly underlined or colored as a hyperlink)?");
	}},
];

var input_keys = [
	{name: 'Up Arrow',    key: 'Up',    seqs: ['\x1b[A', '\x1bOA', '\x1bA']},
	{name: 'Down Arrow',  key: 'Down',  seqs: ['\x1b[B', '\x1bOB', '\x1bB']},
	{name: 'Right Arrow', key: 'Right', seqs: ['\x1b[C', '\x1bOC', '\x1bC']},
	{name: 'Left Arrow',  key: 'Left',  seqs: ['\x1b[D', '\x1bOD', '\x1bD']},
	{name: 'Home',        key: 'Home',  seqs: ['\x1b[H', '\x1bOH', '\x1b[1~', '\x1b[L', '\x1bH']},
	{name: 'End',         key: 'End',   seqs: ['\x1b[K', '\x1b[F', '\x1bOK', '\x1b[4~', '\x1bK']},
	{name: 'Page Up',     key: 'PgUp',  seqs: ['\x1b[V', '\x1b[5~']},
	{name: 'Page Down',   key: 'PgDn',  seqs: ['\x1b[U', '\x1b[6~']},
	{name: 'Insert',      key: 'Ins',   seqs: ['\x1b[@', '\x1b[2~']},
	{name: 'Delete',      key: 'Del',   seqs: ['\x7f', '\x1b[3~']},
	{name: 'Backspace',   key: 'BkSp',  seqs: ['\x08', '\x7f']},
	{name: 'Backtab',     key: 'S-Tab', seqs: ['\x1b[Z']},
	{name: 'F1',          key: 'F1',    seqs: ['\x1b[11~', '\x1bOP']},
	{name: 'F2',          key: 'F2',    seqs: ['\x1b[12~', '\x1bOQ']},
	{name: 'F3',          key: 'F3',    seqs: ['\x1b[13~', '\x1bOR']},
	{name: 'F4',          key: 'F4',    seqs: ['\x1b[14~', '\x1bOS']},
	{name: 'F5',          key: 'F5',    seqs: ['\x1b[15~']},
	{name: 'F6',          key: 'F6',    seqs: ['\x1b[17~']},
	{name: 'F7',          key: 'F7',    seqs: ['\x1b[18~']},
	{name: 'F8',          key: 'F8',    seqs: ['\x1b[19~']},
	{name: 'F9',          key: 'F9',    seqs: ['\x1b[20~']},
	{name: 'F10',         key: 'F10',   seqs: ['\x1b[21~']},
	{name: 'F11',         key: 'F11',   seqs: ['\x1b[23~']},
	{name: 'F12',         key: 'F12',   seqs: ['\x1b[24~']},
	{name: 'Shift+F1',    key: 'S-F1',  seqs: ['\x1b[11;2~']},
	{name: 'Shift+F2',    key: 'S-F2',  seqs: ['\x1b[12;2~']},
	{name: 'Shift+F3',    key: 'S-F3',  seqs: ['\x1b[13;2~']},
	{name: 'Shift+F4',    key: 'S-F4',  seqs: ['\x1b[14;2~']},
	{name: 'Shift+F5',    key: 'S-F5',  seqs: ['\x1b[15;2~']},
	{name: 'Shift+F6',    key: 'S-F6',  seqs: ['\x1b[17;2~']},
	{name: 'Shift+F7',    key: 'S-F7',  seqs: ['\x1b[18;2~']},
	{name: 'Shift+F8',    key: 'S-F8',  seqs: ['\x1b[19;2~']},
	{name: 'Shift+F9',    key: 'S-F9',  seqs: ['\x1b[20;2~']},
	{name: 'Shift+F10',   key: 'S-F10', seqs: ['\x1b[21;2~']},
	{name: 'Shift+F11',   key: 'S-F11', seqs: ['\x1b[23;2~']},
	{name: 'Shift+F12',   key: 'S-F12', seqs: ['\x1b[24;2~']},
	{name: 'Ctrl+F1',     key: 'C-F1',  seqs: ['\x1b[11;5~']},
	{name: 'Ctrl+F2',     key: 'C-F2',  seqs: ['\x1b[12;5~']},
	{name: 'Ctrl+F3',     key: 'C-F3',  seqs: ['\x1b[13;5~']},
	{name: 'Ctrl+F4',     key: 'C-F4',  seqs: ['\x1b[14;5~']},
	{name: 'Ctrl+F5',     key: 'C-F5',  seqs: ['\x1b[15;5~']},
	{name: 'Ctrl+F6',     key: 'C-F6',  seqs: ['\x1b[17;5~']},
	{name: 'Ctrl+F7',     key: 'C-F7',  seqs: ['\x1b[18;5~']},
	{name: 'Ctrl+F8',     key: 'C-F8',  seqs: ['\x1b[19;5~']},
	{name: 'Ctrl+F9',     key: 'C-F9',  seqs: ['\x1b[20;5~']},
	{name: 'Ctrl+F10',    key: 'C-F10', seqs: ['\x1b[21;5~']},
	{name: 'Ctrl+F11',    key: 'C-F11', seqs: ['\x1b[23;5~']},
	{name: 'Ctrl+F12',    key: 'C-F12', seqs: ['\x1b[24;5~']},
	{name: 'Alt+F1',      key: 'A-F1',  seqs: ['\x1b[11;3~']},
	{name: 'Alt+F2',      key: 'A-F2',  seqs: ['\x1b[12;3~']},
	{name: 'Alt+F3',      key: 'A-F3',  seqs: ['\x1b[13;3~']},
	// Alt+F4 skipped — OS intercepts on Windows
	{name: 'Alt+F5',      key: 'A-F5',  seqs: ['\x1b[15;3~']},
	{name: 'Alt+F6',      key: 'A-F6',  seqs: ['\x1b[17;3~']},
	{name: 'Alt+F7',      key: 'A-F7',  seqs: ['\x1b[18;3~']},
	{name: 'Alt+F8',      key: 'A-F8',  seqs: ['\x1b[19;3~']},
	{name: 'Alt+F9',      key: 'A-F9',  seqs: ['\x1b[20;3~']},
	{name: 'Alt+F10',     key: 'A-F10', seqs: ['\x1b[21;3~']},
	{name: 'Alt+F11',     key: 'A-F11', seqs: ['\x1b[23;3~']},
	{name: 'Alt+F12',     key: 'A-F12', seqs: ['\x1b[24;3~']},
];

input_keys.forEach(function(k) {
	tests.push({name: 'IN:' + k.key, func: function() {
		if (!run_input) return null;
		console.clear();
		console.write("Press " + k.name + " (Enter to skip): ");
		var seq = read_input_seq(15000);
		if (seq === null) return false;        // timeout
		if (seq === '\r' || seq === '\n') return null;  // user skipped
		for (var i = 0; i < k.seqs.length; i++) {
			if (seq === k.seqs[i]) return true;
		}
		// Unrecognized sequence — show it for debugging
		console.write("\r\nGot: " + seq_to_hex(seq) + " ");
		console.write("\r\nPress Enter to continue...");
		while (console.inkey(0, 15000) !== '\r');
		return false;
	}});
});


// --- ANSI Fuzz Testing ---

var fuzz_categories = [
	{name: 'CSI',       weight: 40, generate: gen_malformed_csi},
	{name: 'Interrupt', weight: 25, generate: gen_interrupted},
	{name: 'String',    weight: 20, generate: gen_string_seq},
	{name: 'Boundary',  weight: 10, generate: gen_boundary},
	{name: 'Random',    weight:  5, generate: gen_random_bytes},
];

function pick_weighted_category()
{
	var total = 0;
	for (var i = 0; i < fuzz_categories.length; i++)
		total += fuzz_categories[i].weight;
	var r = Math.floor(Math.random() * total);
	for (var i = 0; i < fuzz_categories.length; i++) {
		r -= fuzz_categories[i].weight;
		if (r < 0)
			return fuzz_categories[i];
	}
	return fuzz_categories[0];
}

function rand_int(lo, hi)
{
	return lo + Math.floor(Math.random() * (hi - lo));
}

function rand_chars(len, lo, hi)
{
	var s = '';
	for (var i = 0; i < len; i++)
		s += String.fromCharCode(rand_int(lo, hi));
	return s;
}

function gen_malformed_csi()
{
	var sub = Math.floor(Math.random() * 6);
	switch (sub) {
		case 0: {
			// Long parameter string
			var len = rand_int(100, 1500);
			var params = '';
			for (var i = 0; i < len; i++)
				params += String.fromCharCode(0x30 + Math.floor(Math.random() * 10));
			return {desc: 'CSI ' + len + ' param digits + H', data: '\x1b[' + params + 'H'};
		}
		case 1: {
			// Many semicolons
			var count = rand_int(50, 500);
			var s = '';
			for (var i = 0; i < count; i++)
				s += ';';
			return {desc: 'CSI ' + count + ' semicolons + m', data: '\x1b[' + s + 'm'};
		}
		case 2: {
			// Huge parameter values
			var vals = ['999999999', '4294967295', '2147483647', '65536', '2147483648'];
			var v = vals[Math.floor(Math.random() * vals.length)];
			var finals = 'ABCDHJKLMPSTXZm';
			var f = finals.charAt(Math.floor(Math.random() * finals.length));
			return {desc: 'CSI huge param ' + v + f, data: '\x1b[' + v + f};
		}
		case 3: {
			// Invalid intermediate bytes
			var inter = ' !"#$%&\'()*+,-./';
			var c1 = inter.charAt(Math.floor(Math.random() * inter.length));
			var c2 = inter.charAt(Math.floor(Math.random() * inter.length));
			var p = String(rand_int(0, 100));
			return {desc: 'CSI invalid intermediates ' + c1 + c2, data: '\x1b[' + p + c1 + c2 + 'm'};
		}
		case 4: {
			// Random final byte in full range with random params
			var final_byte = String.fromCharCode(rand_int(0x40, 0x7F));
			var nparams = rand_int(0, 10);
			var params = '';
			for (var i = 0; i < nparams; i++) {
				if (i > 0)
					params += ';';
				params += String(rand_int(0, 999));
			}
			return {desc: 'CSI params=' + nparams + ' final=0x' + final_byte.charCodeAt(0).toString(16), data: '\x1b[' + params + final_byte};
		}
		case 5: {
			// Private-mode prefix with garbage params
			var prefix = '?>=!';
			var p = prefix.charAt(Math.floor(Math.random() * prefix.length));
			var val = String(rand_int(0, 99999));
			var finals = 'hlsmJK';
			var f = finals.charAt(Math.floor(Math.random() * finals.length));
			return {desc: 'CSI ' + p + val + f, data: '\x1b[' + p + val + f};
		}
	}
	return {desc: 'CSI fallback', data: '\x1b[m'};
}

function gen_interrupted()
{
	var sub = Math.floor(Math.random() * 5);
	switch (sub) {
		case 0:
			// ESC inside CSI
			return {desc: 'ESC inside CSI', data: '\x1b[1;2\x1b[3;4H'};
		case 1: {
			// CAN/SUB mid-sequence
			var cancel = (Math.random() < 0.5) ? '\x18' : '\x1a';
			var cname = (cancel === '\x18') ? 'CAN' : 'SUB';
			return {desc: cname + ' mid-CSI', data: '\x1b[12;34' + cancel + 'Hello'};
		}
		case 2: {
			// Multiple ESCs in a row
			var count = rand_int(2, 10);
			var escs = '';
			for (var i = 0; i < count; i++)
				escs += '\x1b';
			return {desc: count + ' bare ESCs then [H', data: escs + '[H'};
		}
		case 3:
			// Bare ESC followed by text
			return {desc: 'bare ESC + text', data: '\x1bHello World'};
		case 4:
			// CSI interrupted by DCS
			return {desc: 'CSI interrupted by DCS', data: '\x1b[1;2\x1bPsome data\x1b\\'};
	}
	return {desc: 'interrupt fallback', data: '\x1b[m'};
}

function gen_string_seq()
{
	var sub = Math.floor(Math.random() * 5);
	var starters = ['\x1bP', '\x1b]', '\x1b_', '\x1b^', '\x1bX'];
	var snames = ['DCS', 'OSC', 'APC', 'PM', 'SOS'];
	var si = Math.floor(Math.random() * starters.length);
	switch (sub) {
		case 0: {
			// Unterminated string
			var len = rand_int(100, 2000);
			var body = rand_chars(len, 0x20, 0x7F);
			return {desc: snames[si] + ' unterminated ' + len + ' bytes', data: starters[si] + body};
		}
		case 1: {
			// Very long properly-terminated string
			var len = rand_int(500, 4096);
			var body = rand_chars(len, 0x20, 0x7F);
			return {desc: snames[si] + ' terminated ' + len + ' bytes', data: starters[si] + body + '\x1b\\'};
		}
		case 2:
			// ST at wrong time
			return {desc: 'spurious ST', data: '\x1b\\' + 'Normal text' + '\x1b\\'};
		case 3: {
			// Nested string initiators
			var si2 = Math.floor(Math.random() * starters.length);
			return {desc: snames[si] + ' nested ' + snames[si2], data: starters[si] + 'outer' + starters[si2] + 'inner' + '\x1b\\'};
		}
		case 4: {
			// String body with control characters
			var len = rand_int(50, 200);
			var body = '';
			for (var i = 0; i < len; i++) {
				if (Math.random() < 0.2)
					body += String.fromCharCode(rand_int(0x00, 0x1F));
				else
					body += String.fromCharCode(rand_int(0x20, 0x7F));
			}
			return {desc: snames[si] + ' with controls ' + len + ' bytes', data: starters[si] + body + '\x1b\\'};
		}
	}
	return {desc: 'string fallback', data: '\x1b[m'};
}

function gen_boundary()
{
	var sub = Math.floor(Math.random() * 5);
	switch (sub) {
		case 0:
			// Cursor to extreme position
			return {desc: 'CUP 99999;99999', data: '\x1b[99999;99999H'};
		case 1: {
			// Inverted/bad scroll region
			var subs = [
				{d: 'inverted 20;5', s: '\x1b[20;5r'},
				{d: 'zero-size 10;10', s: '\x1b[10;10r'},
				{d: 'off-screen 9999;99999', s: '\x1b[9999;99999r'},
				{d: 'zero bounds 0;0', s: '\x1b[0;0r'},
			];
			var s = subs[Math.floor(Math.random() * subs.length)];
			return {desc: 'DECSTBM ' + s.d, data: s.s + '\x1b[r'};
		}
		case 2: {
			// Huge count for ED/EL/ICH/DCH
			var ops = [
				{d: 'ED 99999', s: '\x1b[99999J'},
				{d: 'EL 99999', s: '\x1b[99999K'},
				{d: 'ICH 99999', s: '\x1b[99999@'},
				{d: 'DCH 99999', s: '\x1b[99999P'},
			];
			var o = ops[Math.floor(Math.random() * ops.length)];
			return {desc: o.d, data: o.s};
		}
		case 3: {
			// Tab stops at extreme positions
			return {desc: 'HTS at col 9999', data: '\x1b[9999G\x1bH\x1b[1G'};
		}
		case 4: {
			// Rapid scroll operations
			var count = rand_int(20, 100);
			var data = '';
			for (var i = 0; i < count; i++)
				data += (Math.random() < 0.5) ? '\x1b[S' : '\x1b[T';
			return {desc: count + ' rapid scrolls', data: data};
		}
	}
	return {desc: 'boundary fallback', data: '\x1b[m'};
}

function gen_random_bytes()
{
	var sub = Math.floor(Math.random() * 3);
	switch (sub) {
		case 0: {
			// Pure random bytes
			var len = rand_int(50, 500);
			return {desc: 'random ' + len + ' bytes', data: rand_chars(len, 0x00, 0x100)};
		}
		case 1: {
			// Random bytes interspersed with valid resets
			var len = rand_int(50, 200);
			var data = '';
			for (var i = 0; i < len; i++) {
				data += String.fromCharCode(rand_int(0x00, 0x100));
				if (i % 20 === 19)
					data += '\x1b[0m\x1b[H';
			}
			return {desc: 'random ' + len + ' bytes + resets', data: data};
		}
		case 2: {
			// Alternating valid/invalid
			var count = rand_int(10, 50);
			var data = '';
			for (var i = 0; i < count; i++) {
				if (Math.random() < 0.5)
					data += '\x1b[' + rand_int(0, 100) + 'm';
				else
					data += rand_chars(rand_int(1, 20), 0x00, 0x100);
			}
			return {desc: count + ' alternating valid/invalid', data: data};
		}
	}
	return {desc: 'random fallback', data: rand_chars(100, 0x00, 0x100)};
}

function fuzz_alive_check()
{
	console.write("\x1b[6n");
	// Use a hard deadline instead of read_ansi_seq, which can be kept alive
	// indefinitely by a stream of partial-match bytes from a spinning terminal
	var deadline = system.timer + 3;
	var seq = '';
	while (system.timer < deadline && bbs.online && !js.terminated) {
		var ch = console.inkey(0, 500);
		if (ch === '' || ch === null || ch === undefined)
			break;
		seq += ch;
		if (seq.search(/\x1b\[[0-9]+;[0-9]+R/) >= 0)
			return true;
	}
	// Drain any remaining junk
	var drained = 0;
	while (bbs.online && !js.terminated && console.inkey(100) !== '') {
		drained++;
		if (drained % 16384 === 0)
			log(LOG_WARNING, "FUZZ alive check still draining, " + drained + " bytes so far");
	}
	if (drained > 0)
		log(LOG_DEBUG, "FUZZ alive check drained " + drained + " bytes total");
	return false;
}

function fuzz_log_data(iteration, data)
{
	var hex = seq_to_hex(data);
	var maxlen = 512;
	for (var off = 0; off < hex.length; off += maxlen) {
		var chunk = hex.substring(off, off + maxlen);
		if (off === 0)
			log(LOG_DEBUG, "FUZZ #" + iteration + " data=" + chunk);
		else
			log(LOG_DEBUG, "FUZZ #" + iteration + " data(cont)=" + chunk);
	}
}

function run_fuzz_loop()
{
	// Drain any residual input before starting
	while (bbs.online && !js.terminated && console.inkey(200) !== '');
	var iteration = 0;
	while (bbs.online && !js.terminated) {
		// Check for user abort (key press)
		// If we get ESC, it's likely a terminal response to a fuzzed sequence
		// (e.g. DSR response), not a user keypress — drain and continue
		var abort = console.inkey(0, 0);
		if (typeof abort === 'string' && abort.length > 0) {
			if (abort === '\x1b') {
				while (bbs.online && !js.terminated) {
					var ch = console.inkey(0, 100);
					if (ch === '' || ch === null || ch === undefined)
						break;
				}
			}
			else {
				break;
			}
		}
		var cat = pick_weighted_category();
		var tc = cat.generate();
		log(LOG_DEBUG, "FUZZ #" + iteration + " [" + cat.name + "] " + tc.desc);
		fuzz_log_data(iteration, tc.data);
		console.write(tc.data);
		// Reset terminal state after each test case
		// ST (ESC \) terminates any open string sequence (DCS/OSC/APC/PM/SOS)
		// Send it twice: first to close SOS (which buffers ESC), second as actual ST
		console.write("\x1b\\\x1b\\");
		console.write("\x1b[0m");
		console.write("\x1b[r");
		console.write("\x1b[?25h");
		// Liveness check
		log(LOG_DEBUG, "FUZZ #" + iteration + " alive check...");
		if (!fuzz_alive_check()) {
			log(LOG_WARNING, "FUZZ #" + iteration + " — terminal stopped responding after [" + cat.name + "] " + tc.desc);
			fuzz_log_data(iteration, tc.data);
			break;
		}
		log(LOG_DEBUG, "FUZZ #" + iteration + " alive OK");
		// Status display
		console.gotoxy(1, 4);
		console.write("Iteration: " + iteration + "  Category: " + cat.name + "     ");
		iteration++;
		mswait(50);
	}
	// Drain any queued input
	while (bbs.online && !js.terminated && console.inkey(100));
	log(LOG_INFO, "FUZZ completed after " + iteration + " iterations");
}

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
		if (!run_output && tst.name.substring(0, 3) !== 'IN:')
			results.push(null);
		else
			results.push(tst.func());
	});

	return results;
}

console.mnemonics("Run which tests: ~Non-interactive, ~Interactive, In~put, ~Fuzz, ~All\r\n");
var menu_key = console.getkeys("NIPFA");
var interactive = (menu_key === 'I' || menu_key === 'A');
var run_input = (menu_key === 'P' || menu_key === 'A');
var run_output = (menu_key !== 'P' && menu_key !== 'F');
var run_fuzz = (menu_key === 'F');
var oldpt = console.ctrlkey_passthru;
console.ctrlkey_passthru = 0x7FFFFFFF;

if (run_fuzz) {
	console.clear();
	console.print("\x01R\x01HWARNING:\x01N ANSI fuzzing may crash your terminal.\r\n");
	console.print("All test cases are logged to the BBS log.\r\n");
	console.print("Press any key to begin (or Enter to skip)...\r\n");
	var ch = console.getkey();
	if (ch !== '\r' && ch !== '\n') {
		run_fuzz_loop();
	}
	console.write("\x1bc");
	console.ctrlkey_passthru = oldpt;
	if (bbs.online && !js.terminated) {
		console.print("Fuzz testing complete. Press any key to return to BBS. ");
		console.getkey();
	}
}
else {
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
}
