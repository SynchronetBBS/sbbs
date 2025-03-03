require("key_defs.js", "KEY_UP");

const a = "\x01";
var msg = 1;
var tnames = ["None"];
var last_entry = 0;
var pos = 0;
var len = 0;
var msglens = [];
var displaywith = 0;

var details = {
	'AutoMsg': {
		'displaywith': 1,
	},
	'DirLibOrAll': {
		'displaywith': 1,
	},
	'JoinWhichDir': {
		'displaywith': 1,
	},
	'JoinWhichGrp': {
		'displaywith': 1,
	},
	'JoinWhichLib': {
		'displaywith': 1,
	},
	'JoinWhichSub': {
		'displaywith': 1,
	},
	'MsgSubj': {
		'args': ['Example message subject (up to 70 characters).........................'],
	},
	'NodeToPrivateChat': {
		'displaywith': 1,
	},
	'NScanCfgWhichGrp': {
		'displaywith': 1,
	},
	'NScanCfgWhichSub': {
		'displaywith': 1,
	},
	'PrivateMsgPrompt': {
		'displaywith': 1,
	},
	'ProtocolBatchOrQuit': {
		'displaywith': 1,
	},
	'ProtocolBatchQuitOrNext': {
		'displaywith': 1,
	},
	'ProtocolOrQuit': {
		'displaywith': 1,
	},
	'QuitOrNext': {
		'displaywith': 1,
	},
	'RExemptRemoveFilePrompt': {
		'displaywith': 1,
	},
	'SetMsgPtrPrompt': {
		'displaywith': 1,
	},
	'SScanCfgWhichGrp': {
		'displaywith': 1,
	},
	'SScanCfgWhichSub': {
		'displaywith': 1,
	},
	'SubGroupOrAll': {
		'displaywith': 1,
	},
	'SysopRemoveFilePrompt': {
		'displaywith': 1,
	},
	'TelnetGatewayPrompt': {
		'displaywith': 1,
	},
	'UserRemoveFilePrompt': {
		'displaywith': 1,
	},
	'VoteInThisPollNow': {
		'displaywith': 1,
	},
	'VoteMsgUpDownOrQuit': {
		'displaywith': 1,
	},
	'WhichOrAll': {
		'displaywith': 1,
	},
	'WhichTextFile': {
		'displaywith': 1,
	},
	'WhichTextFileSysop': {
		'displaywith': 1,
	},
	'WhichTextSection': {
		'displaywith': 1,
	},
}

console.cleartoeos = function(attr)
{
	if (attr !== undefined)
		console.attributes = attr;
	console.write("\x1b[J");
}

function format_entry(str)
{
	// bbs.command_str = '@';
	// .replace(/@/g, "@U+40:@@")
	return str.replace(/[\x00-\x1F]/g, function(match) {
		switch(match) {
			case '\n':
				return "\\n";
			case '\r':
				return "\\r";
			case '\v':
				return "\\v";
			case '\t':
				return "\\v";
			case '\b':
				return "\\b";
			case '\f':
				return "\\f";
			default:
				return '\x01'+'7\x01'+'B^' + String.fromCharCode(match.charCodeAt(0)+64) + "\x01"+'0\x01'+'w';
		}
	});
}

function scanargs(str)
{
	var arg = 0;
	var ret = [];
	// No patchAll or iterators in for... *sigh*
	var re = /%(?:([1-9][0-9]*)$)?([-#0 +]*)(\*|[0-9]+)?(?:.(\*|[0-9]+))?(hh|h|l|ll|j|t|z|L)?([diouxXDOUeEfFgGaAcCsSPnm%])/g;
	var match;

	while ((match = re.exec(str)) !== null) {
		if (arg >= ret.length)
			ret.push(undefined);
		if (match[1] != undefined)
			arg = parseInt(m[1], 10) - 1;
		if (match[3] == '*') {
			ret[arg] = 'minwidth';
			arg++;
			if (arg >= ret.length)
				ret.push(undefined);
		}
		if (match[3] == '*') {
			ret[arg] = 'precision';
			arg++;
			if (arg >= ret.length)
				ret.push(undefined);
		}
		ret[arg] = match[6];
		arg++;
	}
	return ret;
}

function formatted(str, num)
{
	var args = [str];
	if (details[tnames[num]] !== undefined && details[tnames[num]].args !== undefined)
		args.push.apply(args, details[tnames[num]].args);
	else {
		var types = scanargs(str);
		var type;
		var strs = 0;
		for (type in types) {
			switch (types[type]) {
				case 'D':
				case 'd':
				case 'i':
					args.push(-2147483648);
					break;
				case 'o':
				case 'u':
				case 'x':
				case 'X':
				case 'O':
				case 'U':
					args.push(4294967295);
					break;
				case 'e':
				case 'E':
				case 'f':
				case 'F':
				case 'g':
				case 'G':
				case 'a':
				case 'A':
					args.push("3.14159");
					break;
				case 'c':
				case 'C':
					args.push(88);
					break;
				case 's':
				case 'S':
					strs++;
					args.push("StringVal" + strs);
					break;
				case 'p':
					args.push(0x6734531);
					break;
				case 'n':
					break;
				case 'm':
					break;
				case '%':
					break;
				case 'minwidth':
					args.push(4);
					break;
				case 'precision':
					args.push(5);
					break;
			}
		}
	}
	return format.apply(js.global, args).replace(/\x07/g, '');
}

// TODO: Message 765 is too big
// TODO: Some way to "run" things like 559 (YesNoQuestion)
function redraw(str, num)
{
	var df = 'unknown';
	console.clear();
	console.gotoxy(0,0);
	console.attributes = 7;
	console.print("Example text\r\nbefore message.\r\n");
	console.question = "Example question";
	// TODO: mnemonics() vs. putmsg() vs. print()
	//       different messages have different requirements.
	console.ungetkeys("\x03", true);
	switch (displaywith) {
		case 0:
			console.putmsg(formatted(a + 'Q' + str, num));
			df = 'putmsg()   ';
			break;
		case 1:
			console.mnemonics(formatted(a + 'Q' + str, num));
			df = 'mnemonics()';
			break;
		case 2:
			console.print(formatted(a + 'Q' + str, num));
			df = 'print()    ';
			break;
	}
	console.inkey();
	console.print("Example text after message.\r\n");
	console.gotoxy(0,16);
	console.print(a + "h" + a + "w" + a + "4" + " " + df + " ^^ As seen on BBS ^^" + "     vv " + tnames[num] + " (" + num + ") vv" + a + ">");
	console.gotoxy(0,17);
	console.cleartoeos(7);
	console.print(format_entry(bbs.text(msg)));
	place_cursor();
}

function get_tvals() {
	var tvals = {};
	load(tvals, "text.js");
	var i;
	for (i in tvals) {
		tnames[tvals[i]] = i;
	}
	// TODO: Why is this one higher than the last?
	last_entry = tvals.TOTAL_TEXT - 1;
}

function newmsg()
{
	var rmsg = bbs.text(msg);
	var tpos = 0;
	var spos = 0;
	var i;
	msglens = [];
	for (i = 0; i < rmsg.length; i++) {
		msglens.push({'tpos': tpos, 'spos': spos});
		if (rmsg[i] < ' ')
			tpos++;
		tpos++;
		spos++;
	}
	msglens.push({'tpos': tpos, 'spos': spos});
	len = msglens.length;
	pos = len - 1;
	displaywith = 0;
	if (details[tnames[msg]] !== undefined) {
		if (details[tnames[msg]].displaywith !== undefined)
			displaywith = details[tnames[msg]].displaywith;
	}
}

function place_cursor()
{
	console.gotoxy(0, 17);
	var np = msglens[pos].tpos;
	while (np >= console.screen_columns) {
		console.linefeed();
		np -= console.screen_columns;
	}
	console.right(np);
	console.attributes = 15;
}

function get_msgnum()
{
	console.gotoxy(42, 16);
	console.attributes = 0x1F;
	console.cleartoeol();
	var ret = console.getnum(last_entry, msg);
	if (typeof ret === 'number') {
		msg = ret;
		newmsg();
	}
	console.attributes = 7;
}

get_tvals();
newmsg();
var done = false;
while (!done) {
	redraw(bbs.text(msg), msg);
	switch (console.getkey()) {
		case KEY_UP:
			if (msg > 1) {
				msg--;
				newmsg();
			}
			break;
		case KEY_DOWN:
			if (bbs.text(msg + 1) != null) {
				msg++;
				newmsg();
			}
			break;
		case KEY_LEFT:
			if (pos > 0)
				pos--;
			break;
		case KEY_RIGHT:
			if (pos < len - 1)
				pos++;
			break;
		case KEY_PAGEUP:
			displaywith--;
			if (displaywith < 0)
				displaywith = 2;
			break;
		case KEY_PAGEDN:
			displaywith++;
			if (displaywith > 2)
				displaywith = 0;
			break;
		case ctrl('G'):
			get_msgnum();
			break;
		case 'q':
		case 'Q':
			done = true;
			break;
	}
}
