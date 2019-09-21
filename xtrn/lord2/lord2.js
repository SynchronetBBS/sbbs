'use strict';
js.load_path_list.unshift(js.exec_dir+"dorkit/");
load("dorkit.js");
dk.console.auto_pause = false;
if (js.global.console !== undefined) {
	console.ctrlkey_passthru = dk_old_ctrlkey_passthru;
	console.ctrlkey_passthru = '+[';
}

var scr;
if (dk.console.local_io !== undefined)
	scr = dk.console.local_io.screen;
if (scr === undefined && dk.console.local_screen !== undefined)
	scr = dk.console.local_screen;
if (scr === undefined && dk.console.remote_screen !== undefined)
	scr = dk.console.remote_screen;
if (scr === undefined)
	throw('No usable screen!');
require("recordfile.js", "RecordFile");

var player;
var map;
var world;
var items = [];
var Player_Def = [
	{
		prop:'name',
		type:'PString:25',
		def:''
	},
	{
		prop:'realname',
		type:'PString:40',
		def:''
	},
	{
		prop:'money',
		type:'SignedInteger',
		def:0
	},
	{
		prop:'bank',
		type:'SignedInteger',
		def:0
	},
	{
		prop:'experience',
		type:'SignedInteger',
		def:0
	},
	{
		prop:'lastdayon',
		type:'SignedInteger16',
		def:-1
	},
	{
		prop:'love',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'weaponnumber',
		type:'SignedInteger8',
		def:0
	},
	{
		prop:'armournumber',
		type:'SignedInteger8',
		def:0
	},
	{
		prop:'race',
		type:'PString:30',
		def:''
	},
	{
		prop:'sexmale',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'onnow',
		type:'Integer8',
		def:0
	},
	{
		prop:'battle',
		type:'Integer8',
		def:0
	},
	{
		prop:'dead',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'busy',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'deleted',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'nice',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'map',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'e6',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'x',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'y',
		type:'SignedInteger16',
		def:0
	},
	{
		prop:'i',
		type:'Array:99:SignedInteger16',
		def:eval('var aret = []; while(aret.length < 99) aret.push(0); aret;')
	},
	{
		prop:'p',
		type:'Array:99:SignedInteger',
		def:eval('var aret = []; while(aret.length < 99) aret.push(0); aret;')
	},
	{
		prop:'t',
		type:'Array:99:Integer8',
		def:eval('var aret = []; while(aret.length < 99) aret.push(0); aret;')
	},
	{
		prop:'lastsaved',
		type:'SignedInteger',
		def:-1
	},
	{
		prop:'lastdayplayed',
		type:'SignedInteger',
		def:-1
	},
	{
		prop:'lastmap',
		type:'SignedInteger16',
		def:-1
	},
	{
		prop:'extra',
		type:'String:353',
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
					def:7
				},
				{
					prop:'backcolour',
					type:'SignedInteger8',
					def:0
				},
				{
					prop:'ch',
					type:'String:1',
					def:' '
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
					def:0
				},
				{
					prop:'reffile',
					type:'PString:12',
					def:0
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
		def:new Array(1600)
	},
	{
		prop:'v',
		type:'Array:40:SignedInteger',
		def:new Array(40)
	},
	{
		prop:'s',
		type:'Array:10:PString:80',
		def:new Array(10)
	},
	{
		prop:'time',
		type:'SignedInteger',
		def:0
	},
	{
		prop:'hideonmap',
		type:'Array:1600:Integer8',
		def:new Array(1600)
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

function clamp_integer(v, s)
{
	var i = parseInt(v, 10);
	if (isNaN(i))
		throw('Invalid integer '+v);

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
			throw('Invalid clamp style '+s);
	}
	return i;
}

var state = {
	time:0
};
var files = {};
var vars = {
	version:{type:'const', val:99},
	x:{type:'fn', get:function() { return player.x }, set:function(x) { player.x = clamp_integer(x, 's8') } },
	y:{type:'fn', get:function() { return player.y }, set:function(y) { player.y = clamp_integer(y, 's8') } },
	map:{type:'fn', get:function() { return player.map }, set:function(map) { player.map = clamp_integer(map, 's16') } },
	dead:{type:'fn', get:function() { return player.dead }, set:function(dead) { player.dead = clamp_integer(dead, 's8') } },
	sexmale:{type:'fn', get:function() { return player.sexmale }, set:function(sexmale) { player.sexmale = clamp_integer(sexmale, 's16') } },
	narm:{type:'fn', get:function() { return player.armournumber }, set:function(narm) { player.armournumber = clamp_integer(narm, 's8') } },
	nwep:{type:'fn', get:function() { return player.weaponnumber }, set:function(nwep) { player.weaponnumber = clamp_integer(nwep, 's8') } },
	money:{type:'fn', get:function() { return player.money }, set:function(money) { player.money = clamp_integer(money, 's32') } },
	gold:{type:'fn', get:function() { return player.money }, set:function(money) { player.money = clamp_integer(money, 's32') } },
	bank:{type:'fn', get:function() { return player.bank }, set:function(bank) { player.bank = clamp_integer(bank, 's32') } },
	enemy:{type:'str', val:''},
	'&realname':{type:'const', val:dk.user.full_name},
	'&date':{type:'fn', get:function() { var d = new Date(); return format('%02d/%02d/%02d', d.getMonth()+1, d.getDate(), d.getYear()%100); }, set:function(x) { throw('Attempt to set date at '+fname+':'+line); } },
	'&nicedate':{type:'fn', get:function() { var d = new Date(); return format('%d:%02d on %02d/%02d/%02d', d.getHours() % 12, d.getMinutes(), d.getMonth()+1, d.getDate(), d.getYear()%100); }, set:function(x) { throw('Attempt to set nicedate at '+fname+':'+line); } },
	's&armour':{type:'fn', get:function() { if (player.armournumber === 0) return ''; return items[player.armournumber].name; } },
	's&arm_num':{type:'fn', get:function() { if (player.armournumber === 0) return 0; return items[player.armournumber].defence; } },
	's&weapon':{type:'fn', get:function() { if (player.weaponnumber === 0) return ''; return items[player.weaponnumber].name; } },
	's&wep_num':{type:'fn', get:function() { if (player.weaponnumber === 0) return 0; return items[player.weaponnumber].strength; } },
	's&son':{type:'fn', get:function() { return player.sexmale === 1 ? 'son' : 'daughter' }},
	's&boy':{type:'fn', get:function() { return player.sexmale === 1 ? 'boy' : 'girl' }},
	's&man':{type:'fn', get:function() { return player.sexmale === 1 ? 'man' : 'lady' }},
	's&sir':{type:'fn', get:function() { return player.sexmale === 1 ? 'sir' : 'ma\'am' }},
	's&him':{type:'fn', get:function() { return player.sexmale === 1 ? 'him' : 'her' }},
	's&his':{type:'fn', get:function() { return player.sexmale === 1 ? 'his' : 'her' }},
	'&money':{type:'fn', get:function() { return player.money }, set:function(money) { player.money = clamp_integer(money, 's32') } },
	'&gold':{type:'fn', get:function() { return player.money }, set:function(money) { player.money = clamp_integer(money, 's32') } },
	'&bank':{type:'fn', get:function() { return player.bank }, set:function(bank) { player.bank = clamp_integer(bank, 's32') } },
	'&lastx':{type:'fn', get:function() { return player.lastx }, set:function(bank) { player.lastx = clamp_integer(bank, 's8') } },
	'&lasty':{type:'fn', get:function() { return player.lasty }, set:function(bank) { player.lasty = clamp_integer(bank, 's8') } },
	'&map':{type:'fn', get:function() { return player.map } },
	'&time':{type:'fn', get:function() { return state.time }, set:function(x) { throw('attempt to set &time'); } },
	'&timeleft':{type:'fn', get:function() { return parseInt((dk.user.seconds_remaining + dk.user.seconds_remaining_from - time()) / 60, 10) } },
	'&sex':{type:'fn', get:function() { return player.sexmale }, set:function(sexmale) { player.sexmale = clamp_integer(sexmale, 's16') } },
	'&playernum':{type:'fn', get:function() { return player.Record } },
	'&totalaccounts':{type:'fn', get:function() { return pfile.length } },
	responce:{type:'int', val:0},
	response:{type:'fn', get:function() { return vars.responce.val; }, set:function(val) { vars.responce.val = clamp_integer(val, 's32')  } },
};
var i;
for (i = 1; i < 41; i++) {
	vars[format('`v%02d', i)] = {type:'int', val:0};
}
for (i = 1; i < 11; i++) {
	vars[format('`s%02d', i)] = {type:'str', val:''};
}
for (i = 0; i < 99; i++) {
	vars[format('`p%02d', i+1)] = {type:'fn', get:eval('function() { return player.p['+i+'] }'), set:eval('function(val) { player.p['+i+'] = clamp_integer(val, "s32"); }')};
	vars[format('`t%02d', i+1)] = {type:'fn', get:eval('function() { return player.t['+i+'] }'), set:eval('function(val) { player.t['+i+'] = clamp_integer(val, "8"); }')};
	vars[format('`i%02d', i+1)] = {type:'fn', get:eval('function() { return player.i['+i+'] }'), set:eval('function(val) { player.i['+i+'] = clamp_integer(val, "s8"); }')};
	vars[format('`+%02d', i+1)] = {type:'fn', get:eval('function() { return items['+i+'].name }'), set:eval('function(val) { throw("Attempt to set item '+i+' name"); }')};
}

function getkeyw()
{
	var timeout = time() + 60 * 5;
	var tl;
	var now;

	do {
		now = time();
		// TODO: dk.console.getstr() doesn't support this stuff... (yet)
		tl = (dk.user.seconds_remaining + dk.user.seconds_remaining_from - 30) - now;
		if (tl < 1) {
			// TODO message etc
			exit(0);
		}
		if (now >= timeout) {
			// TODO message etc
			exit(0);
		}
	} while(!dk.console.waitkey(1000));
	return dk.console.getkey();
}

var curlinenum = 1;
var curcolnum = 1;
var morechk = true;
// Reads a key
function getkey()
{
	var ch;
	var a;

	curlinenum = 1;
	ch = '\x00';
	do {
		ch = getkeyw();
		if (ch === null || ch.length < 1) {
			ch = '\x00';
		}
	} while (ch === '\x00');

	return ch;
}

function remove_colour(str)
{
	str = str.replace(/`[0-9\!-^]/g, '');
	str = str.replace(/`r[0-7]/g, '');
	return str;
}

function setvar(name, val) {
	var t;

	name = name.toLowerCase();
	if (vars[name] === undefined)
		throw('Attempt to set invalid variable '+name);
	switch(vars[name].type) {
		case 'int':
			t = parseInt(val);
			if (isNaN(t))
				throw('Invalid value '+val+' assigned to '+name);
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
			throw('Attempt to set const value '+name);
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
			throw('Unhandled var type '+vars[name].type);
	}
}

function getvar(name) {
	var uc = false;
	var lc = false;
	var ret;

	if (vars[name.toLowerCase()] === undefined)
		return name;
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
			throw('Unhandled var type '+vars[name].type);
	}
	if (uc || lc)
		ret = ret.toString();
	if (uc)
		ret = ret.substr(0, 1).toUpperCase() + ret.substr(1);
	if (lc)
		ret = ret.substr(0, 1).toLowerCase() + ret.substr(1);
	return ret;
}

function expand_ticks(str)
{
	str = str.replace(/`n/ig, player.name);
	str = str.replace(/`e/ig, getvar('enemy'));
	str = str.replace(/`x/ig, ' ');
	str = str.replace(/`w/ig, '');
	str = str.replace(/`l/ig, '');
	str = str.replace(/`\\/ig, '\n');
	str = str.replace(/`c/ig, '');
	str = str.replace(/.`d/ig, '');
	// TODO: Not sure how to handle `c and `d is icky.
	return str;
}

function replace_vars(str)
{
	str = str.replace(/([Ss]?&[A-Za-z]+)/g, function(m, r1) { return getvar(r1); });
	str = str.replace(/(`[Vv][0-4][0-9])/g, function(m, r1) { return getvar(r1); });
	str = str.replace(/(`[Ss][0-1][0-9])/g, function(m, r1) { return getvar(r1); });
	str = str.replace(/(`[Pp][0-9][0-9])/g, function(m, r1) { return getvar(r1); });
	str = str.replace(/(`[Tt][0-9][0-9])/g, function(m, r1) { return getvar(r1); });
	str = str.replace(/(`[Ii][0-9][0-9])/g, function(m, r1) { return getvar(r1); });
	str = str.replace(/(`[Ii][0-9][0-9])/g, function(m, r1) { return getvar(r1); });
	str = str.replace(/(`\+[0-9][0-9])/g, function(m, r1) { return getvar(r1); });
	return str;
}

function clean_str(str)
{
	var ret = '';
	var i;

	for (i = 0; i < str.length; i += 1) {
		if (str[i] === '$') {
			// Just skip it.
		}
		else if (str[i] !== '`') {
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

function displen(str)
{
	return remove_colour(expand_ticks(replace_vars(str))).length;
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
		if (curlinenum > dk.console.rows - 1) {
			more_nomail();
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

function foreground(col)
{
	if (col > 15) {
		col = 0x80 | (col & 0x0f);
	}
	dk.console.attr.value = (dk.console.attr.value & 0x70) | col;
}

function background(col)
{
	if (col > 7 || col < 0) {
		return;
	}
	dk.console.attr.value = (dk.console.attr.value & 0x8f) | (col << 4);
}

function more()
{
	var oa = dk.console.attr.value;

	// TODO: Should be *centered*
	curlinenum = 1;
	lw('  `2<`0MORE`2>');
	getkey();
	dk.console.print('\r');
	dk.console.cleareol();
	dk.console.attr.value = oa;
}

function lw(str) {
	var i;
	var to;
	var oop;
	var snip = '';
	var lwch;

	str = replace_vars(str);
	for (i=0; i<str.length; i += 1) {
		if (str[i] === '`') {
			sw(snip);
			snip = '';
			i += 1;
			if (i > str.length) {
				break;
			}
			switch(str[i]) {
				case 'n':
				case 'N':
					lw(player.name);
					break;
				case 'e':
				case 'E':
					lw(getvar('enemy'));
					break;
				case 'g':
				case 'G':
					throw('TODO: Graphics level');
					break;
				case 'k':
				case 'K':
					more();
					break;
				case 'x':
				case 'X':
					sw(' ');
					break;
				case 'd':
				case 'D':
					sw('\b');
					break;
				case 'w':
				case 'W':
					mswait(100);
					break;
				case 'l':
				case 'L':
					mswait(500);
					break;
				case '\\':
					sln('');
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
					foreground(0);
					break;
				case 'c':
				case 'C':
					sclrscr();
					sln('');
					sln('');
					break;
				case 'r':
				case 'R':
					i += 1;
					if (i > str.length) {
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

function lln(str)
{
	lw(str);
	sln('');
}

function spaces(len)
{
	var r = '';
	var i;

	for (i = 0; i < len; i++)
		r += ' ';
	return r;
}

var bar_timeout;
var bar_queue = [];
function update_bar(str, timeout)
{
	str = replace_vars(str);
	var dl = displen(str);
	var l;
	var attr = dk.console.attr.value;

	dk.console.gotoxy(2, 20);
	dk.console.attr.value = 31;
	if (dl < 76) {
		l = parseInt((76 - dl) / 2, 10);
		str = spaces(l) + str;
		l = 76 - (dl + l);
		str = str + spaces(l);
	}
	lw(str);
	dk.console.gotoxy(player.x-1, player.y-1);
	dk.console.attr.value = attr;
	if (timeout !== undefined)
		bar_timeout = new Date().valueOf() + parseInt(timeout * 1000, 10);
	else
		bar_timeout = undefined;
}

function status_bar()
{
	var b;

	b = '`r1`2Location is `0'+map.name+'`2. HP: (`%'+player.p[1]+'`2 of `%'+player.p[2]+'`2).  Press `%?`2 for help.';
	update_bar(b);
}

function timeout_bar()
{
	if (bar_timeout === undefined && bar_queue.length === 0)
		return;
	if (bar_timeout === undefined || new Date().valueOf() > bar_timeout) {
		if (bar_queue.length)
			update_bar(bar_queue.shift(), 4);
		else
			status_bar();
	}
}

function run_ref(sec, fname)
{
	var line;
	var m;
	var cl;
	var args;

	fname = fname.toLowerCase();
	sec = sec.toLowerCase();

	function getlines() {
		var ret = [];

		while (line < files[fname].lines.length && files[fname].lines[line+1].search(/^\s*@/) === -1) {
			ret.push(files[fname].lines[line+1]);
			line++;
		}
		return ret;
	}

	var do_handlers = {
		'write':function(args) {
			line++;
			if (line > files[fname].lines.length)
				return;
			cl = files[fname].lines[line];
			lw(cl);
		},
		'talk':function(args) {
			// TODO: Send message to other players (mail stuff?)
		},
		'readspecial':function(args) {
			var ch;

			do {
				ch = getkey().toUpperCase();
				if (ch === '\r')
					ch = args[1].substr(0, 1);
			} while (args[1].indexOf(ch) === -1);
			setvar(args[0], ch);
		},
		'goto':function(args) {
			args[0] = args[0].toLowerCase();
			if (files[fname].section[args[0]] === undefined)
				throw('Goto undefined label '+args[0]+' at '+fname+':'+line);
			line = files[fname].section[args[0]].line;
		},
		'move':function(args) {
			if (args.length > 1) {
				dk.console.gotoxy(clamp_integer(getvar(args[0]), '8') - 1, clamp_integer(getvar(args[1]), '8') - 1);
				return;
			}
			throw('Invalid move at '+fname+':'+line);
		},
		'readstring':function(args) {
			var x = scr.pos.x;
			var y = scr.pos.y;
			var attr = scr.attr.value;
			var len = getvar(args[0]);
			var s;

			if (args.length === 2)
				args.push('`s10');
			s = dk.console.getstr({len:len, edit:(args[1].toLowerCase() === 'nil' ? '' : getvar(args[1])), input_box:true, attr:new Attribute(31)});
			setvar(args[2], s);
			dk.console.gotoxy(x, y);
			while (remove_colour(s).length < len)
				s += ' ';
			dk.console.attr = 15;
			lw(s);
			dk.console.attr = attr;
		},
		'stripbad':function(args) {
			setvar(args[0], clean_str(getvar(args[0])));
		},
		'replaceall':function(args) {
			var hs;
			var f = getvar(args[0]);
			var r = getvar(args[1]);

			hs = getvar(args[2]);
			while (hs.indexOf(f) !== -1) {
				hs = hs.replace(f, r);
			}
			setvar(args[2], hs);
		},
		'copytoname':function(args) {
			player.name = getvar('`s10');
		},
		'moveback':function(args) {
			erase(player.x - 1, player.y - 1);
			player.x = player.lastx;
			player.y = player.lasty;
			update();
		},
		'saybar':function(args) {
			line++;
			if (line >= files[fname].lines.length)
				throw('Trailing saybar at '+fname+':'+line);
			update_bar(files[fname].lines[line], 5);
		},
		'quebar':function(args) {
			line++;
			if (line >= files[fname].lines.length)
				throw('Trailing quebar at '+fname+':'+line);
			bar_queue.push(files[fname].lines[line]);
			timeout_bar();
		},
		'pad':function(args) {
			var str = getvar(args[0]).toString();
			var dl = displen(str);
			var l = parseInt(getvar(args[1]), 10);

			str += spaces(l - dl);
			setvar(args[0], str);
		}
	};
	var handlers = {
		'do':function(args) {
			var tmp;

			if (args.length < 1 || args.length > 0 && args[0].toLowerCase() === 'do') {
				if (line + 1 >= files[fname].lines.length)
					throw('do at end of file');
				if (files[fname].lines[line + 1].search(/^\s*@begin/i) === -1)
					throw('trailing do');
				line++;
				return;
			}
			if (do_handlers[args[0].toLowerCase()] !== undefined) {
				do_handlers[args[0].toLowerCase()](args.slice(1));
				return;
			}
			if (args.length > 2 && (args[1].toLowerCase() === 'is' || args[1] === '=')) {
				if (args[2].toLowerCase() === 'length') {
					tmp = getvar(args[3]);
					tmp = remove_colour(expand_ticks(replace_vars(tmp)));
					setvar(args[0], tmp.length);
				}
				else if (args[2].toLowerCase() === 'reallength') {
					setvar(args[0], getvar(args[3]).length);
				}
				else
					setvar(args[0], getvar(args[2]));
				return;
			}
			if (args.length > 2 && args[1] == '-') {
				setvar(args[0], getvar(args[0]) - getvar(args[2]));
				return;
			}
			if (args.length > 2 && (args[1] === '+' || args[1].toLowerCase() === 'add')) {
				setvar(args[0], getvar(args[0]) + getvar(args[2]));
				return;
			}
			if (args.length > 2 && args[1] == '/') {
				setvar(args[0], getvar(args[0]) / getvar(args[2]));
				return;
			}
			if (args.length > 2 && args[1] == '*') {
				setvar(args[0], getvar(args[0]) * getvar(args[2]));
				return;
			}
			if (args.length > 3 && args[1].toLowerCase() === 'random') {
				setvar(args[0], random(clamp_integer(getvar(args[2]), 's32')) + clamp_integer(args[3], 's32'));
				return;
			}
			throw('Unhandled do at '+fname+':'+line);
		},
		'version':function(args) {
			if (args.length < 1)
				throw('Invalid version at '+fname+':'+line);
			if (parseInt(args[0] > vars.version.val))
				throw('lord2.js version too old!');
		},
		'if':function(args) {
			var tmp;
			var tmp2;
			var tmp3;
			var tmp4;

			if (args[0].toLowerCase() === 'checkdupe') {
				tmp2 = remove_colour(getvar(args[1]));
				tmp4 = false;
				for (tmp = 0; tmp < pfile.length; tmp++) {
					tmp3 = pfile.get(tmp);
					if (tmp3.deleted)
						continue;
					if (remove_colour(tmp3.name) === tmp2) {
						tmp4 = true;
						break;
					}
				}
				if (tmp4 === (getvar(args[2]).toLowerCase() === 'true'))
					handlers.do(args.slice(4));
				else if (args[4].toLowerCase() === 'begin')
					handlers.begin(args.slice(5));
				return;
			}
			else if (args[0].toLowerCase() === 'bitcheck') {
				tmp = getvar(args[1]);
				tmp2 = 1 << parseInt(getvar(args[2]), 10);
				tmp3 = parseInt(getvar(args[3]), 10);
				if (tmp3 === 0)
					tmp3 = false;
				else
					tmp3 = true;
				if ((!!(tmp & tmp2)) === tmp3)
					handlers.do(args.slice(5));
				else if (args[5].toLowerCase() === 'begin')
					handlers.begin(args.slice(6));
				return;
			}
			switch(args[1].toLowerCase()) {
				case 'more':
				case '>':
					if (parseInt(getvar(args[0]), 10) > parseInt(getvar(args[2]), 10))
						handlers.do(args.slice(4));
					else if (args[4].toLowerCase() === 'begin')
						handlers.begin(args.slice(5));
					break;
				case '<':
				case 'less':
					if (parseInt(getvar(args[0]), 10) < parseInt(getvar(args[2]), 10))
						handlers.do(args.slice(4));
					else if (args[4].toLowerCase() === 'begin')
						handlers.begin(args.slice(5));
					break;
				case '!':
				case 'not':
					if (getvar(args[0]).toString() !== getvar(args[2]).toString())
						handlers.do(args.slice(4));
					else if (args[4].toLowerCase() === 'begin')
						handlers.begin(args.slice(5));
					break;
				case '=':
				case 'is':
					if (args[2].toLowerCase() === 'length') {
						tmp = remove_colour(expand_ticks(replace_vars(getvar(args[3])))).length;
					}
					else if (args[2].toLowerCase() === 'reallength') {
						tmp = getvar(args[3]).length;
					}
					else
						tmp = getvar(args[2]);
					if (getvar(args[0]).toString() === tmp.toString())
						handlers.do(args.slice(4));
					else if (args[4].toLowerCase() === 'begin')
						handlers.begin(args.slice(5));
					break;
				case 'exist':
					if (file_exists(js.exec_dir + args[0]) === (args[2].toLowerCase() === 'true'))
						handlers.do(args.slice(5));
					else if (args[4].toLowerCase() === 'begin')
						handlers.begin(args.slice(6));
					break;
				default:
					throw('Unhandled condition '+args[1]+' at '+fname+':'+line);
			}
		},
		'begin':function(args) {
			var depth = 1;
			if (args.length > 0)
				throw('Unexpected arguments to begin');
			while (depth > 0) {
				line++;
				if (files[fname].lines[line].search(/^\s*@begin/i) !== -1)
					depth++;
				if (files[fname].lines[line].search(/^\s*@end/i) !== -1)
					depth--;
			}
		},
		'routine':function(args) {
			if (args.length === 1) {
				run_ref(args[0], fname);
				return;
			}
			if (args[1].toLowerCase() === 'in') {
				run_ref(args[0], args[2]);
				return;
			}
			throw('Unable to parse routine at '+fname+':'+line);
		},
		'pauseon':function(args) {
			morechk = true;
		},
		'pauseoff':function(args) {
			morechk = false;
		},
		'show':function(args) {
			var l = getlines();
			var ch;
			var i;
			var p;
			var pages;
			var sattr = 2;

			if (args.length === 0) {
				l.forEach(function(l) {
					lln(l);
				});
			}
			else if (args[0].toLowerCase() === 'scroll') {
				p = 0;
				pages = l.length / 22;
				if (pages > parseInt(pages, 10))
					pages = parseInt(pages, 10) + 1;

				while(1) {
					dk.console.clear();
					dk.console.attr.value = sattr;
					for (i = 0; i < 22; i++) {
						if (p*22+i >= l.length)
							continue;
						lln(l[p*22+i]);
					}
					sattr = dk.console.attr.value;
					dk.console.gotoxy(0, 22);
					lw('`r1`%  (`$'+(p+1)+'`%/`$'+pages+'`%)`$');
					dk.console.cleareol();
					dk.console.gotoxy(12, 22);
					lw('`%[`$N`%]`$ext Page, `%[`$P`%]`$revious Page, `%[`$Q`%]`$uit, `%[`$S`%]`$tart, `%[`$E`%]`$nd');
					ch = getkey().toUpperCase();
					switch (ch) {
						case 'E':
							p = pages - 1;
							break;
						case 'N':
							p++;
							if (p >= pages)
								p = pages - 1;
							break;
						case 'P':
							p--;
							if (p < 0)
								p = 0;
							break;
						case 'S':
							p = 0;
							break;
						case 'Q':
							dk.console.attr.value = sattr;
							return;
					}
				}
			}
			else
				throw('Unsupported SHOW command at '+fname+':'+line);
		},
		'writefile':function(args) {
			if (args.length < 1)
				throw('No filename for writefile at '+fname+':'+line);
			var f = new File(js.exec_dir + args[0].toLowerCase());
			if (!f.open('a'))
				throw('Unable to open '+f.name+' at '+fname+':'+line);
			getlines().forEach(function(l) {
				f.writeln(l);
			});
			f.close();
		},
		'end':function(args) {},
		'label':function(args) {},
		// Appears to be used to end a slow/etc block
		'':function(args) {},
		'displayfile':function(args) {
			var lines;
			if (args.length < 1)
				throw('No filename for displayfile at '+fname+':'+line);
			var f = new File(js.exec_dir + args[0].toLowerCase());
			if (!f.open('r'))
				throw('Unable to open '+f.name+' at '+fname+':'+line);
			lines = f.readAll();
			f.close();
			lines.forEach(function(l) {
				lln(l);
			});
		},
		'choice':function(args) {
			var allchoices = getlines();
			var choices = [];
			var cmap = [];
			var x = scr.pos.x;
			var y = scr.pos.y;
			var cur = getvar('`v01') - 1;
			var cx = scr.pos.x;
			var ch;
			var attr = dk.console.attr.value;

			function draw_menu() {
				choices.forEach(function(c, i) {
					dk.console.gotoxy(x, y+i);
					if (i === cur)
						c = '`r1'+c+'`r0';
					lw(c);
					if (i === cur)
						cx = scr.pos.x;
				});
				dk.console.gotoxy(cx, y + cur);
			}

			function filter_choices() {
				choices = [];
				allchoices.forEach(function(l, i) {
					var m;

					do {
						m = l.match(/^([-\+=\!><])([`&0-9a-zA-Z]+) ([^ ]+) /)
						if (m !== null) {
							l = l.substr(m[0].length);
							switch(m[1]) {
								case '=':
									if (getvar(m[2]).toString() !== m[3])
										return;
									break;
								case '!':
									if (getvar(m[2]).toString() === m[3])
										return;
									break;
								case '<':
									if (getvar(m[2]) >= parseInt(m[3], 10))
										return;
									break;
								case '>':
									if (getvar(m[2]) <= parseInt(m[3], 10))
										return;
									break;
								case '+':
									if (!(getvar(m[2]) & (1 << parseInt(m[3], 10))))
										return;
									break;
								case '-':
									if (getvar(m[2]) & (1 << parseInt(m[3], 10)))
										return;
									break;
								default:
									throw('Unhandled filter choice!');
							}
						}
					} while (m != null);
					choices.push(l);
					cmap.push(i);
				});
			}

			filter_choices();
			dk.console.attr.value = 15;
			while(1) {
				draw_menu();
				ch = getkey();
				switch(ch) {
					case '8':
					case 'KEY_UP':
					case '4':
					case 'KEY_LEFT':
						cur--;
						if (cur < 0)
							cur = choices.length - 1;
						break;
					case '2':
					case 'KEY_DOWN':
					case '6':
					case 'KEY_RIGHT':
						cur++;
						if (cur >= choices.length)
							cur = 0;
						break;
					case '\r':
						setvar('responce', cmap[cur] + 1);
						setvar('`v01', cmap[cur] + 1);
						dk.console.gotoxy(x, y+choices.length-1);
						dk.console.attr.value = attr;
						return;
				}
			}
		},
		'halt':function(args) {
			if (args.length > 0)
				exit(clamp_integer(args[0], 's32'));
			exit(0);
		},
		'key':function(args) {
			if (args.length > 0 && args[0].toLowerCase() === 'nodisplay')
				getkey();
			else
				more();
		},
		'addchar':function(args) {
			var tmp = pfile.new();
			player.Record = tmp.Record;
			player.put();
			player.reLoad();
		},
		'offmap':function(args) {
			// TODO: Disappear to other players.
		},
		'busy':function(args) {
			// TODO: Turn red for other players, and run @#busy if other player interacts
		},
		'drawmap':function(args) {
			draw_map();
		},
		'update':function(args) {
			update();
		},
		'clearblock':function(args) {
			var start = parseInt(getvar(args[0]), 10) - 1;
			var end = parseInt(getvar(args[1]), 10);
			var i;

			for (i = start; i < end; i++) {
				dk.console.gotoxy(0, start);
				dk.console.cleareol();
			}
		},
		'loadmap':function(args) {
			load_map(parseInt(getvar(args[0]), 10));
		}
	};

	function handle(args)
	{
		if (handlers[args[0].toLowerCase()] === undefined)
			throw('No handler for '+args[0]+' command at '+fname+':'+line);
		handlers[args[0].toLowerCase()](args.slice(1));
	}

	function load_ref(fname) {
		var obj = {section:{}};
		fname = fname.toLowerCase();
		var f = new File(js.exec_dir + fname);
		var cs;

		if (!f.open('r'))
			throw('Unable to open '+f.name);
		obj.lines = f.readAll(4096);
		obj.lines.unshift(';secret line zero... shhhhh');
		f.close();
		obj.lines.forEach(function(l, n) {
			var m;
			m = l.match(/^\s*@#([^\s;]+)/);
			if (m !== null) {
				cs = m[1].toLowerCase();
				// SIGH... duplicates are allowed... see Stonebridge.
				//if (obj.section[cs] !== undefined)
				//	throw('Duplicate section name '+cs+' in '+fname);
				obj.section[cs] = {line:n};
				return;
			}
			m = l.match(/^\s*@label\s+([^\s;]+)/i);
			if (m !== null) {
				var lab = m[1].toLowerCase();
				// SIGH... duplicates are allowed... see Stonebridge.
				//if (obj.section[lab] !== undefined)
				//	throw('Duplicate label name '+lab+' in section '+cs+' in '+fname);
				obj.section[lab] = {line:n};
				return;
			}
		});
		files[fname] = obj;
	}
	if (files[fname] === undefined)
		load_ref(fname);
	if (files[fname].section[sec] === undefined)
		throw('Unable to find section '+sec+' in '+fname);
	line = files[fname].section[sec].line;

	while (1) {
		line++;
		if (line > files[fname].lines.length)
			return;
		cl = files[fname].lines[line].replace(/^\s*/,'');
		if (cl.search(/^@#/) !== -1)
			return;
		if (cl.search(/^\s*@closescript/) !== -1)
			return;
		if (cl.search(/^;/) !== -1)
			continue;
		if (cl.search(/^$/) === 0)
			continue;
		if (cl.search(/^@/) === -1)
			continue;	// Sigh... if...write etc.
			//throw('Invalid line '+line+' in '+fname+' "'+cl+'"');
		args = cl.substr(1).split(/\s+/);
		handle(args);
	}
}

function load_player()
{
	var i;
	var p;

	for (i = 0; i < pfile.length; i++) {
		p = pfile.get(i);
		if (p.realname === dk.user.full_name) {
			player = p;
			return;
		}
	}
	player = new RecordFileRecord(pfile);
	player.reInit();
	player.realname = dk.user.full_name;
	player.onnow = 1;
	player.busy = 0;
	player.battle = 0;
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
	if (col > 7 || col < 0) {
		return;
	}
	dk.console.attr.value = (dk.console.attr.value & 0x8f) | (col << 4);
}

function getoffset(x, y) {
	return (x * 20 + y);
}

function getpoffset() {
	return (player.x - 1)*20+(player.y - 1);
}

function erase(x, y) {
	var off = getoffset(x,y);
	var mi = map.mapinfo[off];

	foreground(mi.forecolour);
	background(mi.backcolour);
	dk.console.gotoxy(x, y);
	dk.console.print(mi.ch === '' ? ' ' : mi.ch);
}

function update() {
	// TODO: Draw other players
	timeout_bar();
	dk.console.gotoxy(player.x - 1, player.y - 1);
	foreground(15);
	background(map.mapinfo[getpoffset()].backcolour);
	dk.console.print('\x02');
	dk.console.gotoxy(player.x - 1, player.y - 1);
}

function draw_map() {
	var x;
	var y;
	var off;
	var mi;
	var s;

	dk.console.attr.value = 7;
	dk.console.clear();
	for (y = 0; y < 20; y++) {
		for (x = 0; x < 80; x++) {
			off = getoffset(x,y);
			mi = map.mapinfo[off];
			foreground(mi.forecolour);
			background(mi.backcolour);
			dk.console.gotoxy(x, y);
			dk.console.print(mi.ch === '' ? ' ' : mi.ch);
		}
	}
	status_bar();
}

function load_map(mapnum)
{
	map = mfile.get(world.mapdatindex[mapnum - 1] - 1);
}

function move_player(xoff, yoff) {
	var x = player.x + xoff;
	var y = player.y + yoff;
	var moved = false;
	var special = false;
	var newmap = false;

	if (x === 0) {
		player.map -= 1;
		player.x = 80;
		newmap = true;
	}
	if (x === 81) {
		player.map += 1;
		player.x = 1;
		newmap = true;
	}
	if (y === 0) {
		player.map -= 80;
		player.y = 20;
		newmap = true;
	}
	if (y === 21) {
		player.map += 80;
		player.y = 1;
		newmap = true;
	}
	if (newmap) {
		if (world.hideonmap[player.map] === 0)
			player.lastmap = player.map;
		load_map(player.map);
		draw_map();
		update();
	}
	else {
		player.lastx = player.x;
		player.lasty = player.y;
		if (map.mapinfo[getoffset(x-1, y-1)].terrain === 1) {
			erase(player.x-1, player.y-1);
			player.x = x;
			player.y = y;
			update();
			moved = true;
		}
	}
	map.hotspots.forEach(function(s) {
		if (s.hotspotx === x && s.hotspoty === y) {
			special = true;
			if (s.warptomap > 0 && s.warptox > 0 && s.warptoy > 0) {
				if (player.map !== s.warptomap)
					newmap = true;
				player.map = s.warptomap;
				if (world.hideonmap[player.map] === 0)
					player.lastmap = player.map;
				if (newmap) {
					load_map(player.map);
					draw_map();
				}
				else {
					erase(player.x - 1, player.y - 1);
				}
				player.x = s.warptox;
				player.y = s.warptoy;
				update();
			}
			else if (s.reffile !== '' && s.refsection !== '') {
				run_ref(s.refsection, s.reffile);
			}
		}
	});
	if (moved && !special && map.battleodds > 0 && map.reffile !== '' && map.refsection !== '') {
		if (random(map.battleodds === 0))
			run_ref(map.refsection, map.reffile);
	}
}

function view_inventory()
{
	var inv = [];
	var lb = [];
	var choices = [];
	var i;
	var ch;
	var cur = 0;
	var str;
	var attr = dk.console.attr.value;
	var cx;

	function draw_menu() {
		choices.forEach(function(c, i) {
			dk.console.gotoxy(0, 12+i);
			if (i === cur)
				c = '`r1'+c+'`r0';
			lw(c);
			if (i === cur)
				cx = scr.pos.x;
		});
		dk.console.gotoxy(cx, 12+i);
	}

	function draw_inv() {
		lb.forEach(function(c, i) {
			dk.console.gotoxy(0, 12+i);
			lln(c);
		});
		dk.console.gotoxy(0, 12+cur);
	}

	for (i = 0; i < 99; i++) {
		if (player.i[i] > 0)
			inv.push(i);
	}
	if (inv.length === 0) {
		dk.console.gotoxy(0, 12);
		lw('`2  You are carrying nothing!  (press `%Q`2 to continue)');
		do {
			ch = getkey().toUpperCase();
		} while (ch != 'Q');
	}
	else {
		inv.forEach(function(i) {
			str = '`2  '+items[i].name;
			choices.push(str);
			if (items[i].armour)
				str += ' `0A`2';
			if (items[i].weapon)
				str += ' `4W`2';
			if (items[i].useonce)
				str += ' `51`2';
			str += spaces(37 - displen(str));
			str += '`2 (`0'+player.i[i]+'`2)';
			str += spaces(47 - displen(str));
			str += '`2 ' + items[i].description;
			str += spaces(79 - displen(str));
			lb.push(str);
		});

		draw_inv();
		while(1) {
			draw_menu();
			dk.console.gotoxy(0, 23);
			ch = getkey();
			switch(ch) {
				case '8':
				case 'KEY_UP':
				case '4':
				case 'KEY_LEFT':
					cur--;
					if (cur < 0)
						cur = lb.length - 1;
					break;
				case '2':
				case 'KEY_DOWN':
				case '6':
				case 'KEY_RIGHT':
					cur++;
					if (cur >= lb.length)
						cur = 0;
					break;
				case '\r':
					setvar('responce', cur + 1);
					dk.console.gotoxy(0, 12+lb.length-1);
					dk.console.attr.value = attr;
					return;
			}
		}
	}
}

function do_map()
{
	var ch;

	if (map === undefined || map.Record !== world.mapdatindex[player.map - 1] - 1)
		load_map(player.map);
	draw_map();
	update();

	ch = ''
	while (ch != 'Q') {
		do {
			timeout_bar();
		} while (!dk.console.waitkey(500));
		ch = getkey().toUpperCase();
		switch(ch) {
			case '8':
			case 'KEY_UP':
				move_player(0, -1);
				break;
			case '4':
			case 'KEY_LEFT':
				move_player(-1, 0);
				break;
			case '6':
			case 'KEY_RIGHT':
				move_player(1, 0);
				break
			case '2':
			case 'KEY_DOWN':
				move_player(0, 1);
				break;
			case '?':
				run_ref('help', 'help.ref');
				break;
			case 'R':
				draw_map();
				update();
				break;
			case 'D':
				run_ref('readlog', 'logstuff.ref');
				draw_map();
				update();
				break;
			case 'L':
				run_ref('listplayers', 'help.ref');
				break;
			case 'M':
				run_ref('map', 'help.ref');
				break;
			case 'Q':
				dk.console.gotoxy(0, 22);
				lw('`r0`2  Are you sure you want to quit back to the BBS? [`%Y`2] : ');
				do {
					ch = getkey().toUpperCase();
				} while('YN\r'.indexOf(ch) === -1);
				if (ch === 'N') {
					dk.console.gotoxy(0, 22);
					dk.console.cleareol();
					dh.console.gotoxy(player.x - 1, player.y - 1);
				}
				else
					ch = 'Q';
				break;
			case 'T':
				run_ref('talk', 'help.ref');
				break;
			case 'V':
				run_ref('stats', 'gametxt.ref');
				view_inventory();
				run_ref('closestats', 'gametxt.ref');
				break;
			case 'Y':
				run_ref('yell', 'help.ref');
				break;
			case 'Z':
				run_ref('z', 'help.ref');
				break;
		}
	}
	run_ref('endgame', 'gametxt.ref');
	mswait(2500);
}

function load_items()
{
	var i;

	for (i = 0; i < ifile.length; i++)
		items.push(ifile.get(i));
}

var pfile = new RecordFile(js.exec_dir + 'player.dat', Player_Def);
var mfile = new RecordFile(js.exec_dir + 'map.dat', Map_Def);
var wfile = new RecordFile(js.exec_dir + 'world.dat', World_Def);
var ifile = new RecordFile(js.exec_dir + 'items.dat', Item_Def);
world = wfile.get(0);
load_player();
load_items();

if (player.Record === undefined)
	run_ref('newplayer', 'gametxt.ref');

run_ref('startgame', 'gametxt.ref');
do_map();
