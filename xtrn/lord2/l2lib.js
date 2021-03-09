require("recordfile.js", "RecordFile");
var scr;
if (dk.console.local_io !== undefined)
	scr = dk.console.local_io.screen;
if (scr === undefined && dk.console.local_screen !== undefined)
	scr = dk.console.local_screen;
if (scr === undefined && dk.console.remote_screen !== undefined)
	scr = dk.console.remote_screen;
if (scr === undefined)
	throw new Error('No usable screen!');

var UCASE = false;
var matchcase = true;
function getfname(str)
{
	str = str.replace(/\\/g,'/');
	if (matchcase) {
		var ec = file_getcase(js.exec_dir + str);

		if (ec !== undefined) {
			return ec;
		}
	}
	if (UCASE) {
		return js.exec_dir + str.toUpperCase();
	}
	if (str.indexOf(maildir) === 0) {
		str = str.toLowerCase().replace(/([\\\/])([^\/\\]+)$/, function(m, p1, p2) { return p1 + p2.toUpperCase(); });
		return js.exec_dir + str;
	}
	return js.exec_dir + str.toLowerCase();
}

var Player_Def = [
	{	// 0
		prop:'name',
		type:'PString:25',
		def:''
	},
	{	// 26 (1a)
		prop:'realname',
		type:'PString:40',
		def:''
	},
	{	// 67 (43)
		prop:'money',
		type:'SignedInteger',
		def:0
	},
	{	// 71 (47)
		prop:'bank',
		type:'SignedInteger',
		def:0
	},
	{	// 75 (4b)
		prop:'experience',
		type:'SignedInteger',
		def:0
	},
	{	// 79 (4f)
		prop:'lastdayon',
		type:'SignedInteger16',
		def:-1
	},
	{	// 81 (51)
		prop:'love',		
		type:'SignedInteger16',
		def:0
	},
	{	// 83 (53)
		prop:'weaponnumber',
		type:'SignedInteger8',
		def:0
	},
	{	// 84 (54)
		prop:'armournumber',
		type:'SignedInteger8',
		def:0
	},
	{	// 85 (55)
		prop:'race',
		type:'PString:30',
		def:''
	},
	{	// 116 (74)
		prop:'sexmale',
		type:'SignedInteger16',
		def:0
	},
	{	// 118 (76)
		prop:'onnow',
		type:'Integer8',
		def:0
	},
	{	// 119 (77)
		prop:'battle',
		type:'Integer8',
		def:0
	},
	{	// 120 (78)
		prop:'dead',
		type:'SignedInteger16',
		def:0
	},
	{	// 122 (7a)
		prop:'busy',
		type:'SignedInteger16',
		def:0
	},
	{	// 124 (7c)
		prop:'deleted',
		type:'SignedInteger16',
		def:0
	},
	{	// 126 (7e)
		prop:'nice',
		type:'SignedInteger16',
		def:0
	},
	{	// 128 (80)
		prop:'map',
		type:'SignedInteger16',
		def:0
	},
	{	// 130 (82)
		prop:'e6',
		type:'SignedInteger16',
		def:0
	},
	{	// 132 (84)
		prop:'x',
		type:'SignedInteger16',
		def:0
	},
	{	// 134 (86)
		prop:'y',
		type:'SignedInteger16',
		def:0
	},
	{	// 136 (88)
		prop:'i',
		type:'Array:99:SignedInteger16',
		def:eval('var aret = []; while(aret.length < 99) aret.push(0); aret;')
	},
	{	// 334 (14e)
		prop:'p',
		type:'Array:99:SignedInteger',
		def:eval('var aret = []; while(aret.length < 99) aret.push(0); aret;')
	},
	{	// 720 (2d0)
		prop:'t',
		type:'Array:99:Integer8',
		def:eval('var aret = []; while(aret.length < 99) aret.push(0); aret;')
	},
	{	// 819 (333)
		prop:'lastsaved',
		type:'SignedInteger',
		def:-1
	},
	{	// 823 (337)
		prop:'lastdayplayed',
		type:'SignedInteger',
		def:-1
	},
	{	// 827 (33b)
		prop:'lastmap',
		type:'SignedInteger16',
		def:-1
	},
	{	// 829 (33d)
		prop:'extra',
		type:'String:354',
		def:''
	}
];

var Map_Def = [
	{
		prop:'name',
		type:'PString:30',
		def:''
	},
	{
		prop:'mapinfo',
		type:{
			array:80*20,
			recordDef:[
				{
					prop:'forecolour',
					type:'SignedInteger8',
					def:3
				},
				{
					prop:'backcolour',
					type:'SignedInteger8',
					def:0
				},
				{
					prop:'ch',
					type:'String:1',
					def:ascii(176)
				},
				{
					prop:'t',
					type:'SignedInteger16',
					def:0
				},
				{
					prop:'terrain',
					type:'SignedInteger8',
					def:0
				},
			]
		},
	},
	{
		prop:'hotspots',
		type:{
			array:10,
			recordDef:[
				{
					prop:'warptomap',
					type:'SignedInteger16',
					def:0
				},
				{
					prop:'hotspotx',
					type:'SignedInteger8',
					def:0
				},
				{
					prop:'hotspoty',
					type:'SignedInteger8',
					def:0
				},
				{
					prop:'warptox',
					type:'SignedInteger8',
					def:0
				},
				{
					prop:'warptoy',
					type:'SignedInteger8',
					def:0
				},
				{
					prop:'refsection',
					type:'PString:12',
					def:''
				},
				{
					prop:'reffile',
					type:'PString:12',
					def:''
				},
				{
					prop:'extra',
					type:'String:100',
					def:''
				},
			]
		}
	},
	{
		prop:'battleodds',
		type:'SignedInteger',
		def:0
	},
	{
		prop:'reffile',
		type:'PString:12',
		def:''
	},
	{
		prop:'refsection',
		type:'PString:12',
		def:''
	},
	{
		prop:'nofighting',
		type:'Boolean',
		def:true
	},
	{
		prop:'extra',
		type:'String:469',
		def:''
	}
];

var World_Def = [
	{
		prop:'name',
		type:'PString:60',
		def:''
	},
	{
		prop:'mapdatindex',
		type:'Array:1600:SignedInteger16',
		def:eval('var aret = []; while(aret.length < 1600) aret.push(0); aret;')
	},
	{
		prop:'v',
		type:'Array:40:SignedInteger',
		def:new Array(40)
	},
	{
		prop:'s',
		type:'Array:10:PString:80',
		def:eval('var aret = []; while(aret.length < 10) aret.push(""); aret;')
	},
	{
		prop:'time',
		type:'SignedInteger',
		def:0
	},
	{
		prop:'hideonmap',
		type:'Array:1600:Integer8',
		def:eval('var aret = []; while(aret.length < 1600) aret.push(0); aret;')
	},
	{
		prop:'extra',
		type:'String:396',
		def:''
	}
]

var Item_Def = [
	{
		prop:'name',
		type:'PString:30',
		def:''
	},
	{
		prop:'hitaction',
		type:'PString:40',
		def:''
	},
	{
		prop:'useonce',
		type:'Boolean',
		def:false
	},
	{
		prop:'armour',
		type:'Boolean',
		def:false
	},
	{
		prop:'weapon',
		type:'Boolean',
		def:false
	},
	{
		prop:'sell',
		type:'Boolean',
		def:false
	},
	{
		prop:'used',
		type:'Boolean',
		def:false
	},
	{
		prop:'value',
		type:'SignedInteger',
		def:false
	},
	{
		prop:'breakage',
		type:'SignedInteger16',
		def:false
	},
	{
		prop:'maxbuy',
		type:'SignedInteger16',
		def:false
	},
	{
		prop:'defence',
		type:'SignedInteger16',
		def:false
	},
	{
		prop:'strength',
		type:'SignedInteger16',
		def:false
	},
	{
		prop:'eat',
		type:'SignedInteger16',
		def:false
	},
	{
		prop:'refsection',
		type:'PString:12',
		def:false
	},
	{
		prop:'useaction',
		type:'PString:30',
		def:false
	},
	{
		prop:'description',
		type:'PString:30',
		def:false
	},
	{
		prop:'questitem',
		type:'Boolean',
		def:false
	},
	{
		prop:'extra',
		type:'String:37',
		def:false
	},
];

var Update_Def = [
	{
		prop:'x',
		type:'SignedInteger8',
		def:0
	},
	{
		prop:'y',
		type:'SignedInteger8',
		def:0
	},
	{
		prop:'map',
		type:'SignedInteger16',
		def:155
	},
	{
		prop:'onnow',
		type:'Integer8',
		def:0
	},
	{
		prop:'busy',
		type:'Integer8',
		def:0
	},
	{
		prop:'battle',
		type:'Integer8',
		def:0
	},
];

var Game_Def = [
	{
		prop:'pad0',
		type:'String:8',
		def:''
	},
	// Offset 8 is days to deletion
	{
		prop:'deldays',
		type:'Integer',
		def:15
	},
	
	{
		prop:'pad1',
		type:'String:150',
		def:''
	},
	// Offset 162 is path to editor
	{
		prop:'editor',
		type:'PString:40',
		def:''
	},
	{
		prop:'pad4',
		type:'String:20',
		def:''
	},
	// Offset 223 is 32-bit delay
	{
		prop:'delay',
		type:'Integer',
		def:100
	},
	// Offset 223 and 227 seem to be the version maybe?
	{
		prop:'pad2',
		type:'String:4',
		def:''
	},
	// Offset 231 is buffer strokes
	{
		prop:'buffer',
		type:'Integer8',
		def:1
	},
	{
		prop:'pad3',
		type:'String:1990',
		def:''
	}
];

var lastkey = time();
var time_callback;
var idle_timeout = 60 * 5;	// Seconds
function getkeyw()
{
	var timeout = lastkey + idle_timeout;
	var tl;
	var now;

	do {
		if (time_callback !== undefined) {
			now = time();
			// TODO: dk.console.getstr() doesn't support this stuff... (yet)
			tl = (dk.user.seconds_remaining + dk.user.seconds_remaining_from - 30) - now;
			if (tl < 1) {
				time_callback('BBS_NO_TIME');
				return 'BBS_NO_TIME';
			}
			if (now >= timeout) {
				time_callback('IDLE');
				return 'IDLE';
			}
		}
	} while(!dk.console.waitkey(1000));
	lastkey = time();
	return dk.console.getkey();
}

var curlinenum = 1;
var curcolnum = 1;
var morechk = true;
var morestr = '`r0`2<`0MORE`2>';
// Reads a key
function getkey()
{
	var ch;
	var a;

	curlinenum = 1;
	ch = '\x00';
	do {
		ch = getkeyw();
		if (ch === undefined || ch === null || ch.length < 1) {
			ch = '\x00';
		}
	} while (ch === '\x00');

	return ch;
}

function remove_colour(str)
{
	if (typeof str !== 'string')
		return str;
	str = str.replace(/`[0-9\!-^]/g, '');
	str = str.replace(/`r[0-7]/g, '');
	return str;
}

function clean_str(str)
{
	var ret = '';
	var i;

	for (i = 0; i < str.length; i += 1) {
		if (str[i] !== '`') {
			ret += str[i];
		}
		else {
			switch (str[i+1]) {
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
				case '!':
				case '@':
				case '#':
				case '$':
				case '%':
					ret += str[i];
					break;
				default:
					i += 1;
			}
		}
	}

	// TODO: This is where badwords replacements should happen...

	return ret;
}

function superclean(str)
{
	return remove_colour(expand_ticks(replace_vars(str)));
}

function lord_to_ansi(str)
{
	var ret = '\x1b[0m';
	var i;
	var bg = '';
	var bright = false;

	for (i=0; i<str.length; i += 1) {
		if (str[i] === '`') {
			i += 1;
			switch(str[i]) {
				case 'c':
					ret += '\x1b[2J\x1b[H\r\n\r\n';
					break;
				case '1':
					if (bright)
						ret += '\x1b[0;34'+bg+'m';
					else
						ret += '\x1b[34m';
					bright = false;
					break;
				case '^':
					if (bright)
						ret += '\x1b[0;30'+bg+'m';
					else
						ret += '\x1b[30m';
					bright = false;
					break;
				case '*':
					// TODO: This may not do 41...
					if (bright) {
						ret += '\x1b[0;30;41m';
					}
					else
						ret += '\x1b[30;41m';
					bg = ';41';
					bright = false;
					break;
				case '2':
					if (bright)
						ret += '\x1b[0;32'+bg+'m';
					else
						ret += '\x1b[32m';
					bright = false;
					break;
				case '3':
					if (bright)
						ret += '\x1b[0;36'+bg+'m';
					else
						ret += '\x1b[36m';
					bright = false;
					break;
				case '4':
					if (bright)
						ret += '\x1b[0;31'+bg+'m';
					else
						ret += '\x1b[31m';
					bright = false;
					break;
				case '5':
					if (bright)
						ret += '\x1b[0;35'+bg+'m';
					else
						ret += '\x1b[35m';
					bright = false;
					break;
				case '6':
					if (bright)
						ret += '\x1b[0;33;'+bg+'m';
					else
						ret += '\x1b[33m';
					bright = false;
					break;
				case '7':
					if (bright)
						ret += '\x1b[0;37'+bg+'m';
					else
						ret += '\x1b[37m';
					bright = false;
					break;
				case '8':
					if (bright)
						ret += '\x1b[30m';
					else
						ret += '\x1b[1;30m';
					bright = true;
					break;
				case '9':
					if (bright)
						ret += '\x1b[34m';
					else
						ret += '\x1b[1;34m';
					bright = true;
					break;
				case '0':
					if (bright)
						ret += '\x1b[32m';
					else
						ret += '\x1b[1;32m';
					bright = true;
					break;
				case '!':
					if (bright)
						ret += '\x1b[36m';
					else
						ret += '\x1b[1;36m';
					bright = true;
					break;
				case '@':
					if (bright)
						ret += '\x1b[31m';
					else
						ret += '\x1b[1;31m';
					bright = true;
					break;
				case '#':
					if (bright)
						ret += '\x1b[35m';
					else
						ret += '\x1b[1;35m';
					bright = true;
					break;
				case '$':
					if (bright)
						ret += '\x1b[33m';
					else
						ret += '\x1b[1;33m';
					bright = true;
					break;
				case '%':
					if (bright)
						ret += '\x1b[37m';
					else
						ret += '\x1b[1;37m';
					bright = true;
					break;
				case 'r':
					i += 1;
					switch(str[i]) {
						case '0':
							ret += '\x1b[40m';
							bg = '';
							break;
						case '1':
							ret += '\x1b[44m';
							bg = ';44';
							break;
						case '2':
							ret += '\x1b[42m';
							bg = ';42'
							break;
						case '3':
							ret += '\x1b[43m';
							bg = ';43';
							break;
						case '4':
							ret += '\x1b[41m';
							bg = ';41';
							break;
						case '5':
							ret += '\x1b[45m';
							bg = ';45';
							break;
						case '6':
							ret += '\x1b[47m';
							bg = ';47';
							break;
						case '7':
							ret += '\x1b[46m';
							bg = ';46';
							break;
					}
					break;
			}
		}
		else {
				ret += str[i];
		}
	}
	return ret;
}

function displen(str)
{
	return superclean(str).length;
}

// FRONTPAD is broken on colour codes.
// It doesn't ignore colour codes.
function broken_displen(str)
{
	var i;

	str = expand_ticks(replace_vars(str));
	return str.length;
}

function sw(str) {
	if (str === '') {
		return;
	}
	if (str === '\r\n') {
		curcolnum = 1;
	}
	dk.console.print(str);
	if (str !== '\r\n') {
		curcolnum += str.length;
	}
}

function sln(str)
{
	// *ALL* CRLFs should be sent from here!
	if (str !== '') {
		sw(str);
	}
	sw('\r\n');
	curlinenum += 1;
	if (morechk) {
		if (curlinenum > 24) {
			more();
			curlinenum = 1;
		}
	}
	else {
		curlinenum = 1;
	}
}

function sclrscr()
{
	var oa = dk.console.attr.value;

	dk.console.clear();
	dk.console.attr.value = oa;
	curlinenum = 1;
}

function clearrows(start, end)
{
	var row;

	if (end === undefined)
		end = start;

	dk.console.attr.value = 2;
	for (row = start; row <= end; row++) {
		dk.console.gotoxy(0, row);
		dk.console.cleareol();
	}
}

function foreground(col)
{
	if (col > 15) {
		col = 0x80 | (col & 0x0f);
	}
	dk.console.attr.value = (dk.console.attr.value & 0x70) | col;
}

function background(col)
{
	// PukeWorld uses a background colour of 13...
	col &= 0x07;
	if (col > 7 || col < 0) {
		return;
	}
	dk.console.attr.value = (dk.console.attr.value & 0x8f) | (col << 4);
}

function more()
{
	var oa = dk.console.attr.value;

	curlinenum = 1;
	lw('`r0`2  '+morestr);
	getkey();
	dk.console.print('\r');
	dk.console.cleareol();
	dk.console.attr.value = oa;
}

function handle_lordcodes(str)
{
	var i;
	var to;
	var oop;
	var snip = '';
	var lwch;

	for (i=0; i<str.length; i += 1) {
		if (str[i] === '`') {
			sw(snip);
			snip = '';
			i += 1;
			if (i >= str.length) {
				break;
			}
			switch(str[i]) {
				case 'k':
				case 'K':
					more();
					break;
				case 'w':
				case 'W':
					mswait(100);
					break;
				case 'l':
				case 'L':
					mswait(500);
					break;
				case '1':		// BLUE
					foreground(1);
					break;
				case '2':		// GREEN
					foreground(2);
					break;
				case '3':		// CYAN
					foreground(3);
					break;
				case '4':		// RED
					foreground(4);
					break;
				case '5':		// MAGENTA
					foreground(5);
					break;
				case '6':		// YELLOW
					foreground(6);
					break;
				case '7':		// WHITE/GRAY
					foreground(7);
					break;
				case '8':
					foreground(8);
					break;
				case '9':
					foreground(9);
					break;
				case '0':
					foreground(10);
					break;
				case '!':
					foreground(11);
					break;
				case '@':
					foreground(12);
					break;
				case '#':
					foreground(13);
					break;
				case '$':
					foreground(14);
					break;
				case '%':
					foreground(15);
					break;
				case '^':
				case '*':
					foreground(0);
					break;
				case 'b':
				case 'B':
					foreground(20);	// Blinking red
					break;
				case 'c':
				case 'C':
					sclrscr();
					sln('');
					sln('');
					break;
				case 'y':
				case 'Y':
					foreground(30);	// Blinking yellow
					break;
				case 'r':
				case 'R':
					i += 1;
					if (i >= str.length) {
						break;
					}
					switch (str[i]) {
						case '0':
							background(0);
							break;
						case '1':
							background(1);
							break;
						case '2':
							background(2);
							break;
						case '3':
							background(3);
							break;
						case '4':
							background(4);
							break;
						case '5':
							background(5);
							break;
						case '6':
							background(6);
							break;
						case '7':
							background(7);
							break;
					}
					break;
			}
		}
		else {
			snip += str[i];
		}
	}
	sw(snip);
}

function lw(str) {
	str = replace_vars_only(str);
	handle_lordcodes(str);
}

function lln(str)
{
	lw(str);
	sln('');
}

function repeat_chars(ch, len)
{
	var r = '';
	var i;

	for (i = 0; i < len; i++)
		r += ch;
	return r;
}

function spaces(len)
{
	return repeat_chars(' ', len);
}

function space_pad(str, len)
{
	var dl = displen(str);
	var alen = Math.abs(len);

	while (dl < alen) {
		if (len < 0)
			str = ' ' + str;
		else
			str += ' ';
		dl++;
	}

	return str;
}

function pretty_int(int, rpad)
{
	var ret = parseInt(Math.abs(int), 10).toString();
	var i;
	if (rpad === undefined)
		rpad = 0;

	for (i = ret.length - 3; i > 0; i-= 3) {
		ret = ret.substr(0, i)+','+ret.substr(i);
	}
	if (int < 0)
		ret = '-' + ret;
	ret = space_pad(ret, rpad);
	return ret;
}

function clamp_integer(v, s)
{
	var i = parseInt(v, 10);
	if (isNaN(i))
		throw new Error('Invalid integer (' + s + '): ' + v);

	switch(s) {
		case 's8':
			if (i > 127)
				i = 127;
			else if (i < -128)
				i = -128;
			break;
		case '8':
			if (i >255)
				i = 255;
			else if (i < 0)
				i = 0;
			break;
		case 's16':
			if (i > 32767)
				i = 32767;
			else if (i < -32768)
				i = -32768
			break;
		case '16':
			if (i > 65535)
				i = 65535;
			else if (i < 0)
				i = 0;
			break;
		case 's32':
			if (i > 2147483647)
				i = 2147483647;
			else if (i < -2147483648)
				i = -2147483648;
			break;
		case '32':
			if (i > 4294967295)
				i = 4294967295;
			else if (i < 0)
				i = 0;
			break;
		default:
			throw new Error('Invalid clamp style '+s);
	}
	return i;
}

var state = {
	time:0
};
var files = {};
var vars = {
	version:{type:'const', val:103},
	'nil':{type:'const', val:''},
	'`n':{type:'fn', get:function() { return player.name } },
	'`e':{type:'fn', get:function() { return getvar('enemy'); } },
	'`g':{type:'int', val:3}, // TODO: >= 3 is ANSI or something...
	'`x':{type:'const', val:' '},
	'`d':{type:'const', val:'\b'},
	'`\\':{type:'const', val:'\r\n'},
	'`*':{type:'const', val:dk.connection.node},
	x:{type:'fn', get:function() { return player.x }, set:function(x) { player.x = clamp_integer(x, 's8'); } },
	y:{type:'fn', get:function() { return player.y }, set:function(y) { player.y = clamp_integer(y, 's8'); } },
	map:{type:'fn', get:function() { return player.map }, set:function(map) { player.map = clamp_integer(map, 's16'); if (world.hideonmap[player.map] === 0) player.lastmap = player.map; player.put(); } },
	dead:{type:'fn', get:function() { return player.dead }, set:function(dead) { player.dead = clamp_integer(dead, 's8') } },
	sexmale:{type:'fn', get:function() { return player.sexmale }, set:function(sexmale) { player.sexmale = clamp_integer(sexmale, 's16') } },
	narm:{type:'fn', get:function() { return player.armournumber }, set:function(narm) { player.armournumber = clamp_integer(narm, 's8') } },
	nwep:{type:'fn', get:function() { return player.weaponnumber }, set:function(nwep) { player.weaponnumber = clamp_integer(nwep, 's8') } },
	money:{type:'fn', get:function() { return player.money }, set:function(money) { player.money = clamp_integer(money, 's32') } },
	gold:{type:'fn', get:function() { return player.money }, set:function(money) { player.money = clamp_integer(money, 's32') } },
	bank:{type:'fn', get:function() { return player.bank }, set:function(bank) { player.bank = clamp_integer(bank, 's32') } },
	enemy:{type:'str', val:''},
	local:{type:'fn', get:function() { return (dk.system.mode === 'local' ? 5 : 0) } },
	blockpassable:{type:'fn', get:function() { return (map.mapinfo[getpoffset()].terrain === 1 ? 1 : 0); } },
	'&realname':{type:'const', val:dk.user.full_name},
	'&date':{type:'fn', get:function() { var d = new Date(); return format('%02d/%02d/%02d', d.getMonth()+1, d.getDate(), d.getYear()%100); }, set:function(x) { throw new Error('Attempt to set date at '+fname+':'+line); } },
	// DOCUMENTED in LORD 2 but non-functional.
	//'&nicedate':{type:'fn', get:function() { var d = new Date(); return format('%d:%02d on %02d/%02d/%02d', d.getHours() % 12, d.getMinutes(), d.getMonth()+1, d.getDate(), d.getYear()%100); }, set:function(x) { throw new Error('Attempt to set nicedate at '+fname+':'+line); } },
	// Implemented in LORD 2 (but not documented)
	'&quickdate':{type:'fn', get:function() { var d = new Date(); return format('%d:%02d on %02d/%02d/%02d', d.getHours() % 12, d.getMinutes(), d.getMonth()+1, d.getDate(), d.getYear()%100); }, set:function(x) { throw new Error('Attempt to set nicedate at '+fname+':'+line); } },
	's&armour':{type:'fn', get:function() { if (player.armournumber === 0) return ''; return items[player.armournumber - 1].name; } },
	's&arm_num':{type:'fn', get:function() { if (player.armournumber === 0) return 0; return items[player.armournumber - 1].defence; } },
	's&weapon':{type:'fn', get:function() { if (player.weaponnumber === 0) return ''; return items[player.weaponnumber - 1].name; } },
	's&wep_num':{type:'fn', get:function() { if (player.weaponnumber === 0) return 0; return items[player.weaponnumber - 1].strength; } },
	's&son':{type:'fn', get:function() { return player.sexmale === 1 ? 'son' : 'daughter' }},
	's&boy':{type:'fn', get:function() { return player.sexmale === 1 ? 'boy' : 'girl' }},
	's&man':{type:'fn', get:function() { return player.sexmale === 1 ? 'man' : 'lady' }},
	's&sir':{type:'fn', get:function() { return player.sexmale === 1 ? 'sir' : 'ma\'am' }},
	's&him':{type:'fn', get:function() { return player.sexmale === 1 ? 'him' : 'her' }},
	's&his':{type:'fn', get:function() { return player.sexmale === 1 ? 'his' : 'her' }},
	's&he':{type:'fn', get:function() { return player.sexmale === 1 ? 'he' : 'she' }},
	'&money':{type:'fn', get:function() { return player.money }, set:function(money) { player.money = clamp_integer(money, 's32') } },
	'&gold':{type:'fn', get:function() { return player.money }, set:function(money) { player.money = clamp_integer(money, 's32') } },
	'&bank':{type:'fn', get:function() { return player.bank }, set:function(bank) { player.bank = clamp_integer(bank, 's32') } },
	'&lastx':{type:'fn', get:function() { return player.lastx }, set:function(bank) { player.lastx = clamp_integer(bank, 's8') } },
	'&lasty':{type:'fn', get:function() { return player.lasty }, set:function(bank) { player.lasty = clamp_integer(bank, 's8') } },
	'&map':{type:'fn', get:function() { return player.map } },
	'&lmap':{type:'fn', get:function() { return player.lastmap } },
	'&time':{type:'fn', get:function() { return state.time }, set:function(x) { throw new Error('attempt to set &time'); } },
	'&timeleft':{type:'fn', get:function() { return parseInt((dk.user.seconds_remaining + dk.user.seconds_remaining_from - time()) / 60, 10) } },
	'&sex':{type:'fn', get:function() { return player.sexmale }, set:function(sexmale) { player.sexmale = clamp_integer(sexmale, 's16') } },
	'&playernum':{type:'fn', get:function() { return player.Record + 1 } },
	'&totalaccounts':{type:'fn', get:function() { return pfile.length } },
	responce:{type:'int', val:0},
	response:{type:'fn', get:function() { return vars.responce.val; }, set:function(val) { vars.responce.val = clamp_integer(val, 's32')  } },
};
var i;
for (i = 0; i < 40; i++) {
	vars[format('`v%02d', i+1)] = {type:'fn', get:eval('function() { return world.v['+i+'] }'), set:eval('function(val) { world.v['+i+'] = clamp_integer(val, "s32"); }')};
}
for (i = 0; i < 10; i++) {
	vars[format('`s%02d', i+1)] = {type:'fn', get:eval('function() { return world.s['+i+'] }'), set:eval('function(val) { world.s['+i+'] = val.substr(0, 80); }')};
}
for (i = 0; i < 99; i++) {
	vars[format('`p%02d', i+1)] = {type:'fn', get:eval('function() { return player.p['+i+'] }'), set:eval('function(val) { player.p['+i+'] = clamp_integer(val, "s32"); }')};
	vars[format('`t%02d', i+1)] = {type:'fn', get:eval('function() { return player.t['+i+'] }'), set:eval('function(val) { player.t['+i+'] = clamp_integer(val, "8"); }')};
	vars[format('`i%02d', i+1)] = {type:'fn', get:eval('function() { return player.i['+i+'] }'), set:eval('function(val) { player.i['+i+'] = clamp_integer(val, "s16"); }')};
	vars[format('`+%02d', i+1)] = {type:'fn', get:eval('function() { return items['+i+'].name }'), set:eval('function(val) { throw new Error("Attempt to set item '+i+' name"); }')};
}

function setvar(name, val) {
	var t;

	name = name.toLowerCase();
	if (vars[name] === undefined)
		throw new Error('Attempt to set invalid variable '+name);
	switch(vars[name].type) {
		case 'int':
			t = parseInt(val);
			if (isNaN(t))
				throw new Error('Invalid value '+val+' assigned to '+name);
			if (t > 2147483647)
				t = 2147483648;
			if (t < -2147483648)
				t = -2147483648;
			vars[name].val = t;
			break;
		case 'str':
			vars[name].val = val.toString();
			break;
		case 'const':
			throw new Error('Attempt to set const value '+name);
		case 'bool':
			if (val)
				vars[name].val = true;
			else
				vars[name].val = false;
			break;
		case 'fn':
			vars[name].set(val);
			break;
		default:
			throw new Error('Unhandled var type '+vars[name].type);
	}
}

function getvar(name, replacing) {
	var uc = false;
	var lc = false;
	var ret;

	if (vars[name.toLowerCase()] === undefined) {
		if (replacing === true)
			return name;
		return replace_vars(name);
	}
	if (name.substr(0, 2) === 'S&')
		uc = true;
	if (name.substr(0, 2) === 's&')
		lc = true;
	name = name.toLowerCase();
	switch(vars[name].type) {
		case 'int':
		case 'str':
		case 'const':
			ret = vars[name].val;
			break;
		case 'bool':
			if (vars[name].val)
				ret = 1;
			ret = 0;
			break;
		case 'fn':
			ret = vars[name].get();
			break;
		default:
			throw new Error('Unhandled var type '+vars[name].type);
	}
	if (uc || lc)
		ret = ret.toString();
	if (uc)
		ret = ret.substr(0, 1).toUpperCase() + ret.substr(1);
	if (lc)
		ret = ret.substr(0, 1).toLowerCase() + ret.substr(1);
	return ret;
}

/*
 * Returns a string if variable name is a string
 */
function getsvar(args, offset, vname)
{
	var v = getvar(args[offset]);
	var fv = getvar(vname);

	if (typeof fv === 'string') {
		if (typeof v !== 'string')
			return replace_vars(args.slice(offset).join(' '));
		// TODO: Not really sure how variable replacements with spces work.
		// DAMN YOU SETH!!!
		args[offset] = v;
		return replace_vars(args.slice(offset).join(' '));
	}
	return v;
}

/*
 * Returns a string if variable name is a string, but only takes one arg.
 */
function getsvar1(name, vname)
{
	var v = getvar(name);
	var fv = getvar(vname);

	if (typeof fv === 'string') {
		if (typeof v !== 'string')
			return replace_vars(name);
	}
	return v;
}

function expand_ticks(str)
{
	if (typeof str !== 'string')
		return str;
	str = str.replace(/`w/ig, '');
	str = str.replace(/`l/ig, '');
	str = str.replace(/`c/ig, '');
	return str;
}

function replace_vars_only(str)
{
	if (typeof str !== 'string')
		return str;
	str = str.replace(/(`[Vv][0-4][0-9])/g, function(m, r1) { return getvar(r1, true); });
	str = str.replace(/(`[Ss][0-1][0-9])/g, function(m, r1) { return getvar(r1, true); });
	str = str.replace(/(`[Pp][0-9][0-9])/g, function(m, r1) { return getvar(r1, true); });
	str = str.replace(/(`[Tt][0-9][0-9])/g, function(m, r1) { return getvar(r1, true); });
	str = str.replace(/(`[Ii][0-9][0-9])/g, function(m, r1) { return getvar(r1, true); });
	str = str.replace(/(`[Ii][0-9][0-9])/g, function(m, r1) { return getvar(r1, true); });
	str = str.replace(/(`\+[0-9][0-9])/g, function(m, r1) { return getvar(r1, true); });
	str = str.replace(/(`[nexdNEXD\\\*])/g, function(m, r1) { return getvar(r1, true); });
	return str;
}

function replace_vars(str)
{
	if (typeof str !== 'string')
		return str;
	str = str.replace(/([Ss]?&[A-Za-z]+)/g, function(m, r1) { return getvar(r1, true); });
	return replace_vars_only(str);
}

function getoffset(x, y) {
	return (x * 20 + y);
}

function draw_map(opt) {
	var x;
	var y;
	var off;
	var mi;
	var s;
	var aster;

	// We can't auto-load the players map here because of ORACLE2.REF in CNW
	if (map === null || map === undefined)
		map = load_map(player.map);

	dk.console.attr.value = 7;
	// No need to clear screen since we're overwriting the whole thing.
	// TODO: If dk.console had a function to clear to end of screen, that would help.
	last_draw = undefined;
	for (y = 0; y < 20; y++) {
		for (x = 0; x < 80; x++) {
			off = getoffset(x,y);
			mi = map.mapinfo[off];
			dk.console.gotoxy(x, y);
			if (x == 79) {
				dk.console.attr.value = 2;
				dk.console.cleareol();
			}
			aster = false;
			if (opt === 'HOT') {
				map.hotspots.forEach(function(spot) {
					if (spot.hotspotx === x + 1 && spot.hotspoty === y + 1) {
						aster = true;
					}
				});
				dk.console.attr.value = 148;
			}
			else if (opt === 'HARD') {
				if (mi.terrain !== 1) {
					aster = true;
					dk.console.attr.value = 20;
				}
			}
			if (aster) {
				dk.console.print('*');
			}
			else {
				foreground(mi.forecolour);
				background(mi.backcolour);
				dk.console.print(mi.ch === '' ? ' ' : mi.ch);
			}
		}
	}
	clearrows(21, dk.console.rows - 1);
	other_players = {};
}

function load_map(mapnum)
{
	return mfile.get(world.mapdatindex[mapnum - 1] - 1);
}

function box_top(width, title)
{
	var str;

	str = '`r1`0'+ascii(218) + repeat_chars(ascii(196), parseInt(((width - 2) - displen(title)) / 2, 10)) + title + '`r1`0';
	str += repeat_chars(ascii(196), (width - 1) - displen(str));
	str += ascii(191);
	return str;
}

function box_bottom(width, footer)
{
	if (footer === undefined)
		return '`r1`0'+ascii(192)+repeat_chars(ascii(196), (width - 2))+ascii(217);
	return '`r1`0'+ascii(192)+ascii(196)+footer+'`r1`0'+repeat_chars(ascii(196), (width - 3 - displen(footer)))+ascii(217);
}

function box_middle(width, text, highlight)
{
	var str = '`r1`0'+ascii(179)+'  ';
	if (highlight)
		str += '`r5';
	str += text;
	str += '`r1`0';
	str += spaces((width - 1) - displen(str));
	str += ascii(179);
	return str;
}

function draw_box(y, title, lines, width, x, footer)
{
	if (width === undefined) {
		width = displen(title) + 6;
		lines.forEach(function(l) {
			var len = displen(l) + 6;
			if (len > width)
				width = len;
		});
	}

	if (x === undefined)
		x = Math.floor((80-width) / 2);

	dk.console.gotoxy(x, y);
	lw(box_top(width, title));
	lines.forEach(function(l, i) {
		dk.console.gotoxy(x, y + i + 1);
		lw(box_middle(width, l));
	});
	dk.console.gotoxy(x, y + lines.length + 1);
	lw(box_bottom(width, footer));
	return {width:width, y:y, x:x};
}

function load_items()
{
	var i;

	for (i = 0; i < ifile.length; i++)
		items.push(ifile.get(i));
}

function load_players()
{
	var i;
	var p;

	players = [];
	for (i = 0; i < pfile.length; i++) {
		p = pfile.get(i);
		players.push({x:p.x, y:p.y, map:p.map, onnow:p.onnow, busy:p.busy, battle:p.battle, deleted:p.deleted});
	}
}

function load_game()
{
	if (gfile.length > 0)
		game = gfile.get(0);
	else {
		game = gfile.new();
		game.put();
	}
}

function build_index()
{
	var i;
	var p;
	var u;

	for (i = 0; i < pfile.length; i++) {
		u = ufile.get(i);
		if (u === null) {
			u = ufile.new();
			p = pfile.Get(i);
			u.x = p.x;
			u.y = p.y;
			u.map = p.map;
			u.onnow = p.onnow;
			u.busy = p.busy;
			u.battle = p.battle;
			u.deleted = p.deleted;
			u.put();
		}
	}
}

function erase(x, y, opt) {
	var off = getoffset(x,y);
	var mi;
	var attr = dk.console.attr.value;
	var aster = false;

	if (map !== undefined) {
		mi = map.mapinfo[off];
		dk.console.gotoxy(x, y);
		if (opt === 'HOT') {
			map.hotspots.forEach(function(spot) {
				if (spot.hotspotx === x + 1 && spot.hotspoty === y + 1) {
					aster = true;
				}
			});
		}
		if (aster) {
			dk.console.attr.value = 148;
			dk.console.print('*');
		}
		else {
			foreground(mi.forecolour);
			background(mi.backcolour);
			dk.console.print(mi.ch === '' ? ' ' : mi.ch);
		}
		dk.console.attr.value = attr;
	}
}

function get_inventory()
{
	var inv = [];
	for (i = 0; i < 99; i++) {
		if (player.i[i] > 0)
			inv.push(i+1);
	}
	return inv;
}

function ranked_players(prop)
{
	var i;
	var ret = [];
	var pl;

	for (i = 0; i < pfile.length; i++) {
		pl = pfile.get(i);
		if (pl.deleted === 1)
			continue;
		ret.push(pfile.get(i));
	}

	function sortfunc(a, b) {
		function getprop(pl) {
			var ret;
			var op = player;

			player = pl;
			ret = getvar(prop);
			player = op;
		}

		var ap, bp;

		if (prop !== undefined) {
			ap = getprop(a);
			bp = getprop(b);
			if (ap !== bp)
				return bp - ap;
		}
		if (a.p[0] !== b.p[0])
			return b.p[0] - a.p[0];
		if (a.p[8] !== b.p[8])
			return b.p[8] - a.p[8];
		if (a.p[17] !== b.p[17])
			return b.p[17] - a.p[17];
		if ((a.money + a.bank) !== (b.money + b.bank))
			return (b.money + b.bank) - (a.money + a.bank);
		if (a.p[18] !== b.p[18])
			return b.p[18] - a.p[18];
		if (a.p[2] !== b.p[2])
			return b.p[2] - a.p[2];
		if (a.p[33] !== b.p[33])
			return b.p[33] - a.p[33];
		if (a.p[35] !== b.p[35])
			return b.p[35] - a.p[35];
	}

	return ret.sort(sortfunc);
}

function overheadmap(showhidden)
{
	var off;
	var x, y;

	dk.console.gotoxy(0, 0);
	for (y = 0; y < 20; y++) {
		dk.console.gotoxy(0, y);
		for (x = 0; x < 80; x++) {
			off = y * 80 + x;
			if (world.mapdatindex[off] < 1)
				background(1);
			else if (world.hideonmap[off]) {
				if (showhidden)
					background(3);
				else
					background(1);
			}
			else
				background(2);
			lw(' ');
		}
	}
	clearrows(20, 25);
}

function getpoffset() {
	return (player.x - 1)*20+(player.y - 1);
}

function copy_map(from, to)
{
	Map_Def.forEach(function(prop) {
		to[prop.prop] = from[prop.prop];
	});
}

var player;
var players = [];
var map;
var world;
var game;
var items = [];
var other_players = {};
var pfile = new RecordFile(getfname('trader.dat'), Player_Def);
var mfile = new RecordFile(getfname('map.dat'), Map_Def);
var wfile = new RecordFile(getfname('world.dat'), World_Def);
var ifile = new RecordFile(getfname('items.dat'), Item_Def);
var ufile = new RecordFile(getfname('update.tmp'), Update_Def);
if (ufile.length < pfile.length) {
	build_index();
}
var gfile = new RecordFile(getfname('game.dat'), Game_Def);
var maildir = getfname('mail');

if (!file_isdir(maildir)) {
	mkdir(maildir);
	if (!file_isdir(maildir))
		throw new Error('Unable to create mail directory');
}
maildir = backslash(maildir).replace(js.exec_dir, '');
world = wfile.get(0);
if (world === null) {
	wfile.new();
	world = wfile.get(0);
}
load_players();
load_items();
load_game();
