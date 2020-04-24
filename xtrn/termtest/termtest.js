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
	console.clear();
	return(console.yesno(str));
}

var tests = [
	{'name':"NUL", 'func':function() {
		console.gotoxy(1,1);
		console.write("\x00\x00\x00");
		return check_xy(1, 1);
	}},
	{'name':"BEL", 'func':function() {
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
		if (check_xy(1, 1))
			return false;
		console.gotoxy(console.screen_columns, 1);
		console.write("\t");
		return check_xy(1,2);
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

var res = main();
console.clear();
res.forEach(function(r, idx) {
	console.print(format("%-15s\x01H", tests[idx].name))
	if (r)
		console.print("\x01GPassed\x01N\r\n");
	else
		console.print("\x01RFailed\x01N\r\n");
});
console.print("Press any key to return to BBS. ");
console.getkey();
