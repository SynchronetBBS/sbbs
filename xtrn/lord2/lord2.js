'use strict';

// TODO: More optimal lightbars
// TODO: Multiplayer interactions
// TODO: Save player after changes in case process crashes
// TODO: Optimize lightbars a lot

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
var players = [];
var update_rec;
var map;
var world;
var game;

var items = [];
var other_players = {};
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
		type:'String:211',
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
	'`d':{type:'const', val:'\b'},
	'`x':{type:'const', val:' '},
	'`\\':{type:'const', val:'\r\n'},
	'nil':{type:'const', val:''},
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
	local:{type:'fn', get:function() { return (dk.system.mode === 'local' ? 5 : 0) } },
	'&realname':{type:'const', val:dk.user.full_name},
	'&date':{type:'fn', get:function() { var d = new Date(); return format('%02d/%02d/%02d', d.getMonth()+1, d.getDate(), d.getYear()%100); }, set:function(x) { throw('Attempt to set date at '+fname+':'+line); } },
	'&nicedate':{type:'fn', get:function() { var d = new Date(); return format('%d:%02d on %02d/%02d/%02d', d.getHours() % 12, d.getMinutes(), d.getMonth()+1, d.getDate(), d.getYear()%100); }, set:function(x) { throw('Attempt to set nicedate at '+fname+':'+line); } },
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
for (i = 0; i < 40; i++) {
	vars[format('`v%02d', i+1)] = {type:'fn', get:eval('function() { return world.v['+i+'] }'), set:eval('function(val) { world.v['+i+'] = clamp_integer(val, "s32"); }')};
}
for (i = 0; i < 10; i++) {
	vars[format('`s%02d', i+1)] = {type:'fn', get:eval('function() { return world.s['+i+'] }'), set:eval('function(val) { world.s['+i+'] = val; }')};
}
for (i = 0; i < 99; i++) {
	vars[format('`p%02d', i+1)] = {type:'fn', get:eval('function() { return player.p['+i+'] }'), set:eval('function(val) { player.p['+i+'] = clamp_integer(val, "s32"); }')};
	vars[format('`t%02d', i+1)] = {type:'fn', get:eval('function() { return player.t['+i+'] }'), set:eval('function(val) { player.t['+i+'] = clamp_integer(val, "8"); }')};
	vars[format('`i%02d', i+1)] = {type:'fn', get:eval('function() { return player.i['+i+'] }'), set:eval('function(val) { player.i['+i+'] = clamp_integer(val, "s16"); }')};
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

	if (vars[name.toLowerCase()] === undefined) {
		if (name.toLowerCase() === 'nil')
			return '';
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
	str = str.replace(/`w/ig, '');
	str = str.replace(/`l/ig, '');
	str = str.replace(/`c/ig, '');
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
	str = str.replace(/(`[xd\\])/g, function(m, r1) { return getvar(r1); });
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

var bar_timeout;
var bar_queue = [];
// TODO: How this works in the original...
/*
 * The buffer is in mail/talkX.tmp (where X is the 1-based player number)
 * The game pulls the top line out of there and displays it.
 * As soon as a line is displayed, it is appended to mail/logX.tmp
 * which is what is used when displaying the backlog/fastbacklog
 * 
 * Only the last 19 are displayed, but the file keeps growing.
 * 
 * Various notifications also go into mail/talkX.tmp (enter/exit messages etc)
 * This is where 'T' global messages go too.
 * 
 * mail/conX.tmp is where
 * ADDITEM|4|1 (itemnum, count) goes.
 * ADDGOLD|1
 * CONNECT|1
 * 
 * mail/mailX.tmp is actual messages..
 * @MESSAGE Mee
 * <txt>
 * <txt>
 * @DONE
 * @REPLY 1
 * 
 * Only @DONE is required.
 * 
 * Other interesting MAIL things...
 * MAIL\BAT
 * MAIL\CHAT
 * MAIL\GIV
 * MAIL\INF
 * MAIL\LOG
 * MAIL\MAB
 * MAIL\MAT
 * MAIL\TALK
 * MAIL\TCON
 * MAIL\TEM
 * MAIL\TEMP
 * MAIL\TX
 * 
 * Requires \r\n
 * 
 * HAIL... Appends CONNECT|<them> to conX, creates tx<me>.tmp, and sets battle bit
 * Delete the tx<me>.tmp and the connection is established.
 * 
 * Chat...
 * Puts "CHAT" into INF<them>.TMP
 * Chat lines go into CHAT<them>.TMP '  `0Mee : `2Chat line.'
 * Single line 'EXIT' indicates that was terminated.
 * 
 * Battle...
 * Appends `r0`0Mee `2hits you with his `0Dagger`2 for `b7`2 damage!|7
 * To BAT<them>.dat
 * 
 */
function update_bar(str, centre, timeout)
{
	str = replace_vars(str);
	var dl = displen(str);
	var l;
	var attr = dk.console.attr.value;

	dk.console.gotoxy(2, 20);
	dk.console.attr.value = 31;
	if (centre && dl < 76) {
		l = parseInt((76 - dl) / 2, 10);
		str = spaces(l) + str;
		dl += l;
	}
	l = 76 - dl;
	str = str + spaces(l);
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
	update_bar(b, false);
}

function pretty_int(int)
{
	var ret = parseInt(int, 10).toString();
	var i;

	for (i = ret.length - 3; i > 0; i-= 3) {
		ret = ret.substr(0, i)+','+ret.substr(i);
	}
	return ret;
}

function timeout_bar()
{
	if (bar_timeout === undefined && bar_queue.length === 0)
		return;
	if (bar_timeout === undefined || new Date().valueOf() > bar_timeout) {
		if (bar_queue.length)
			update_bar(bar_queue.shift(), true, 4);
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
			lw(replace_vars(cl));
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
			args[0] = getvar(args[0]);
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
		},
		'saybar':function(args) {
			line++;
			if (line >= files[fname].lines.length)
				throw('Trailing saybar at '+fname+':'+line);
			update_bar(files[fname].lines[line], true, 5);
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
		},
		'rename':function(args) {
			file_rename(js.exec_dir + getvar(args[0]), js.exec_dir + getvar(args[1]));
		},
		'addlog':function(args) {
			var f = new File(js.exec_dir + 'lognow.txt');
			line++;
			if (line > files[fname].lines.length)
				return;
			if (f.open('a')) {
				cl = files[fname].lines[line];
				f.writeln(replace_vars(cl));
				f.close();
			}
		},
		'delete':function(args) {
			file_remove(js.exec_dir + getvar(args[0]));
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
				setvar(args[0], getvar(args[0]) - parseInt(getvar(args[2]), 10));
				return;
			}
			if (args.length > 2 && args[1] === '+') {
				setvar(args[0], getvar(args[0]) + parseInt(getvar(args[2]), 10));
				return;
			}
			if (args.length > 2 && args[1].toLowerCase() === 'add') {
				setvar(args[0], getvar(args[0]) + getvar(args[2]).toString());
				return;
			}
			if (args.length > 2 && args[1] == '/') {
				setvar(args[0], getvar(args[0]) / parseInt(getvar(args[2]), 10));
				return;
			}
			if (args.length > 2 && args[1] == '*') {
				setvar(args[0], getvar(args[0]) * parseInt(getvar(args[2]), 10));
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
						handlers.do(args.slice(4));
					else if (args[4].toLowerCase() === 'begin')
						handlers.begin(args.slice(5));
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
					lln(replace_vars(l));
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
				f.writeln(replace_vars(l));
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
				ch = getkey().toUpperCase();
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
			ufile.new();
			player.put();
			update_rec = ufile.get(player.Record);
		},
		'offmap':function(args) {
			// TODO: Disappear to other players... this toggles busy because
			// it looks like that's what it does...
			update_rec.busy = 1;
			update_rec.put();
		},
		'busy':function(args) {
			// TODO: Turn red for other players, and run @#busy if other player interacts
			// this toggles battle...
			update_rec.battle = 1;
			update_rec.put();
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
			var sattr = dk.console.attr.value;

			dk.console.attr.value = 2;
			for (i = start; i < end; i++) {
				dk.console.gotoxy(0, start);
				dk.console.cleareol();
			}
			dk.console.attr.value = sattr;
		},
		'loadmap':function(args) {
			load_map(parseInt(getvar(args[0]), 10));
		},
		'buymanager':function(args) {
			var itms = getlines();
			var itm;
			var i;
			var cur = 0;
			var ch;

			for (i = 0; i < itms.length; i++)
				itms[i] = parseInt(itms[i], 10);
			// Don't clear the screen first?  Interesting...
			dk.console.gotoxy(0, 9);
			lw('`r5`%  Item To Buy                         Price                                     ');
			dk.console.gotoxy(0, 23);
			lw('`r5                                                                               ');
			dk.console.gotoxy(2, 23);
			lw('`$Q `2to quit, `$ENTER `2to buy item.        You have `$&gold `2gold.`r0');

			itms.forEach(function(it, i) {
				var str;

				itm = items[it - 1];
				dk.console.gotoxy(0, 10 + i);
				str = '';
				if (i === cur)
					str += '`r1';
				str += '`2  ';
				str += itm.name;
				if (i === cur)
					str += '`r0';
				str += decorate_item(itm);
				str += spaces(32 - displen(str));
				str += '`2$`$'+itm.value+'`2';
				str += spaces(48 - displen(str));
				str += itm.description;
				lw(str);
			});

			function draw_menu() {
				itms.forEach(function(it, i) {
					var str;
					var itm = items[it - 1];

					dk.console.gotoxy(0, 10 + i);
					str = '';
					if (i === cur)
						str += '`r1';
					else
						str += '`r0';
					str += '`2  ';
					str += itm.name;
					lw(str);
				});
				dk.console.gotoxy(0, 10 + cur);
			}

			while(1) {
				ch = getkey().toUpperCase();
				switch(ch) {
					case '8':
					case 'KEY_UP':
					case '4':
					case 'KEY_LEFT':
						cur--;
						if (cur < 0)
							cur = itms.length - 1;
						break;
					case '2':
					case 'KEY_DOWN':
					case '6':
					case 'KEY_RIGHT':
						cur++;
						if (cur >= itms.length)
							cur = 0;
						break;
					case 'Q':
						return;
					case '\r':
						itm = items[itms[cur] - 1];
						dk.console.gotoxy(0, 23);
						lw('`r4                                                                               ');
						if (player.money >= itm.value) {
							player.i[itm.Record]++;
							player.money -= itm.value;
							dk.console.gotoxy(1, 23);
							lw('`* ITEM BOUGHT! `%You now have ' + player.i[itm.Record] + ' of \'em.  `2(`0press a key to continue`2)`r0');
						}
						else {
							dk.console.gotoxy(1, 23);
							lw('`* ITEM NOT BOUGHT! `%You don\'t have enough gold.  `2(`0press a key to continue`2)`r0');
						}
						getkey();
						dk.console.gotoxy(0, 23);
						lw('`r5                                                                               ');
						dk.console.gotoxy(2, 23);
						lw('`$Q `2to quit, `$ENTER `2to buy item.        You have `$&gold `2gold.`r0');
						break;
				}
				draw_menu();
			}
		},
		'saveglobals':function(args) {
			world.put();
		},
		'loadglobals':function(args) {
			world.reLoad();
		},
		'bitset':function(args) {
			var bit = clamp_integer(getvar(args[1]), '32');
			var s = clamp_integer(getvar(args[2]), '8');
			var v = clamp_integer(getvar(args[0]), '32');

			if (s !== 0 && s !== 1)
				throw('Setting bit to value other than zero or one');
			if ((!!(v & (1 << bit))) !== (!!s))
				v ^= (1<<bit);
			setvar(args[0], v);
		},
		'fight':function(args) {
			// TODO
		},
		'sellmanager':function(args) {
			var i;
			var inv;
			var lb;
			var choices;
			var cur = 0;
			var amt;
			var price;
			var yn;
			var itm;
			var ch;

			function draw_menu() {
				inv.forEach(function(it, i) {
					var str;
					var itm = items[it - 1];

					dk.console.gotoxy(0, 7 + i);
					str = '';
					if (i === cur)
						str += '`r1';
					else
						str += '`r0';
					str += '`2  ';
					if (!itm.sell)
						str += '`8';
					str += itm.name;
					lw(str);
				});
				dk.console.gotoxy(0, 7 + cur);
			}

			dk.console.gotoxy(0, 6);
			lw('`r5`%  Item To Sell                        Amount Owned                              `r0');
			dk.console.gotoxy(0, 23);
			lw('`r5  `$Q `2to quit, `$ENTER `2to sell item.                                               `r0');

rescan:
			while (1) {
				dk.console.gotoxy(39, 23);
				lw('`2`r5You have `$&money `2gold.`r0');
				for (i = 7; i < 23; i++) {
					dk.console.gotoxy(0, i);
					dk.console.cleareol();
				}
				inv = [];
				for (i = 0; i < 99; i++) {
					if (player.i[i] > 0)
						inv.push(i+1);
				}
				if (inv.length === 0) {
					dk.console.gotoxy(0, 6);
					lw('`r0  `2You have nothing to sell.  (press `%Q `2to continue)');
					do {
						ch = getkey().toUpperCase();
					} while (ch != 'Q');
					return;
				}

				inv.forEach(function(it, i) {
					var str;

					itm = items[it - 1];
					dk.console.gotoxy(0, 7 + i);
					str = '';
					if (i === cur)
						str += '`r1';
					str += '`2  ';
					if (!itm.sell)
						str += '`8';
					str += itm.name;
					if (i === cur)
						str += '`r0';
					str += decorate_item(itm);
					str += spaces(37 - displen(str));
					str += '`2(';
					if (itm.sell)
						str += '`0';
					else
						str += '`8';
					str += player.i[itm.Record];
					str += '`2)';
					str += spaces(48 - displen(str));
					if (itm.sell)
						str += '`2';
					else
						str += '`8';
					str += itm.description;
					lw(str);
				});
				dk.console.gotoxy(0, 7);

				while(1) {
					ch = getkey().toUpperCase();
					switch(ch) {
						case '8':
						case 'KEY_UP':
						case '4':
						case 'KEY_LEFT':
							cur--;
							if (cur < 0)
								cur = inv.length - 1;
							break;
						case '2':
						case 'KEY_DOWN':
						case '6':
						case 'KEY_RIGHT':
							cur++;
							if (cur >= inv.length)
								cur = 0;
							break;
						case 'Q':
							return;
						case '\r':
							itm = items[inv[cur] - 1];
							amt = 1;
							if (!itm.sell) {
								
							}
							price = parseInt(itm.value / 2, 10);
							if (!itm.sell) {
								dk.console.gotoxy(21, 14);
								lw(box_top(41, itm.name));
								dk.console.gotoxy(21, 15);
								lw(box_middle(41, ''));
								dk.console.gotoxy(21, 16);
								lw(box_middle(41, '`$They don\'t seem interested in that.'));
								dk.console.gotoxy(21, 17);
								lw(box_middle(41, ''));
								dk.console.gotoxy(21, 18);
								lw(box_bottom(41));
								dk.console.gotoxy(59, 16);
								getkey();
								continue rescan;
							}
							if (player.i[itm.Record] > 1) {
								dk.console.gotoxy(17, 14);
								lw(box_top(42, items[itm.Record].name));
								dk.console.gotoxy(17, 15);
								lw(box_middle(42, ''));
								dk.console.gotoxy(17, 16);
								lw(box_middle(42, '   `$Sell how many? '));
								dk.console.gotoxy(17, 17);
								lw(box_middle(42, ''));
								dk.console.gotoxy(17, 18);
								lw(box_bottom(42, ''));
								dk.console.gotoxy(38, 16);
								// TODO: This isn't exactly right... cursor is in wrong position, and selected colour is used.
								ch = dk.console.getstr({edit:player.i[itm.Record].toString(), integer:true, input_box:true, attr:new Attribute(47), len:11});
								lw('`r1`0');
								amt = parseInt(ch, 10);
								if (isNaN(amt) || amt <= 0 || amt > player.i[itm.Record]) {
									continue rescan;
								}
							}
							dk.console.gotoxy(20, 16);
							lw(box_top(36, itm.name));
							dk.console.gotoxy(20, 17);
							lw(box_middle(36, ''));
							dk.console.gotoxy(20, 18);
							lw(box_middle(36, '`$Sell '+amt+' of \'em for '+(amt * price)+' gold?'));
							dk.console.gotoxy(20, 19);
							lw(box_middle(36, ''));
							dk.console.gotoxy(20, 20);
							lw(box_middle(36, '`r5`$Yes'));
							dk.console.gotoxy(20, 21);
							lw(box_middle(36, '`$No'));
							dk.console.gotoxy(20, 22);
							lw(box_bottom(36));
							dk.console.gotoxy(25,21);
							yn = true;
							do {
								ch = getkey().toUpperCase();
								switch (ch) {
									case '8':
									case 'KEY_UP':
									case '4':
									case 'KEY_LEFT':
									case '2':
									case 'KEY_DOWN':
									case '6':
									case 'KEY_RIGHT':
										yn = !yn;
										dk.console.gotoxy(23,20);
										if (yn)
											lw('`r5');
										lw('`$Yes`r1');
										dk.console.gotoxy(23,21);
										if (!yn)
											lw('`r5');
										lw('`$No`r1');
										break;
								}
							} while (ch !== '\r');
							if (yn) {
								player.i[itm.Record] -= amt;
								if (player.i[itm.Record] <= 0) {
									if (player.weaponnumber === inv[cur])
										player.weaponnumber = 0;
									if (player.armournumber === inv[cur])
										player.armournumber = 0;
								}
								setvar('money', getvar('money') + amt * price);
							}
							lw('`r0');
							continue rescan;
					}
					draw_menu();
				}
			}
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
//log(fname+':'+line);
		if (line > files[fname].lines.length)
			return;
		cl = files[fname].lines[line].replace(/^\s*/,'');
		if (cl.search(/^@#/) !== -1)
			return;
		if (cl.search(/^\s*@closescript/) !== -1)
			return;
		if (cl.search(/^;/) !== -1)
			continue;
		if (cl.search(/^@/) === -1) {
			// It appears from home.ref:747 that non-comment lines
			// are printed.
			//lln(cl);
			// Nope, that's just Chuck Testa with another
			// realistic bug in the REF file.
			continue;
		}
		// A bare @ is used to terminate things sometimes...
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
			update_rec = ufile.get(p.Record);
			return;
		}
	}
	player = new RecordFileRecord(pfile);
	player.reInit();
	player.realname = dk.user.full_name;
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
	var i;
	var u;
	var nop = {};
	var op;

	// First, update player data
	for (i = 0; i < ufile.length; i++) {
		if (i === player.Record)
			continue;
		if (i > players.length) {
			op = pfile.get(i);
			players.push({x:op.x, y:op.y, map:op.map, onnow:op.onnow, busy:op.busy, battle:op.battle});
		}
		u = ufile.get(i);
		if (u.map !== 0 && u.x !== 0 && u.y !== 0)
			players[i] = u;
	}

	// First, erase any moved players and update other_players
	players.forEach(function(u, i) {
		if (i === player.Record)
			return;
		if (u.map === player.map) {
			nop[i] = {x:u.x, y:u.y, map:u.map, onnow:u.onnow, busy:u.busy, battle:u.battle}
			// Erase old player pos...
			if (other_players[i] !== undefined) {
				op = other_players[i];
				if (op.x !== u.x || op.y !== u.y)
					erase(op.x - 1, op.y - 1);
			}
		}
	});

	// Now, draw all players on the map
	Object.keys(nop).forEach(function(k) {
		u = nop[k];
		// Note that 'busy' is what 'offmap' toggles, not what 'busy' does. *sigh*
		if (u.busy === 0) {
			dk.console.gotoxy(u.x - 1, u.y - 1);
				foreground(4);
			if (u.battle)
				foreground(4);
			else
				foreground(7);
			background(map.mapinfo[getoffset(u.x-1, u.y-1)].backcolour);
			dk.console.print('\x02');
		}
	});
	other_players = nop;

	timeout_bar();
	dk.console.gotoxy(player.x - 1, player.y - 1);
	foreground(15);
	background(map.mapinfo[getpoffset()].backcolour);
	dk.console.print('\x02');
	dk.console.gotoxy(player.x - 1, player.y - 1);
	update_rec.x = player.x;
	update_rec.y = player.y;
	update_rec.map = player.map;
	update_rec.busy = 0;
	update_rec.onnow = 1;
	update_rec.put();
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
	other_players = {};
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

	if (getvar('`v05') > 0) {
		if (player.p[10] <= 0) {
			update_bar('`5You are exhaused - maybe tomorrow, after you get some sleep...', true, 5);
			return;
		}
	}
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
	if (moved && getvar('`v05') > 0) {
		player.p[10]--;
		// It seems there's checks at 750 "It is getting dark", 300 "You are getting very sleepy", and 150 "You are about to faint"
	}
	if (moved && !special && map.battleodds > 0 && map.reffile !== '' && map.refsection !== '') {
		if (random(map.battleodds) === 0)
			run_ref(map.refsection, map.reffile);
	}
}

function box_top(width, title)
{
	var str;

	str = '`r1`0'+ascii(218) + repeat_chars(ascii(196), parseInt(((width - 2) - displen(title)) / 2, 10)) + title + '`r1`0';
	str += repeat_chars(ascii(196), (width - 1) - displen(str));
	str += ascii(191);
	return str;
}

function box_bottom(width)
{
	return '`r1`0'+ascii(192)+repeat_chars(ascii(196), (width - 2))+ascii(217);
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

// Assume width of 36
// Assume position centered in inventory window thing
function popup_menu(title, opts)
{
	var str;
	var x = 21;
	var y = 12 + ((9 - opts.length)/2);
	var cur = 0;
	var ch;
	var sattr = dk.console.attr.value;

	function show_opts() {
		opts.forEach(function(o, i) {
			str = box_middle(38, o.txt, i === cur);
			dk.console.gotoxy(x, y+1+i);
			lw(str);
		});
		dk.console.gotoxy(x+3, y+1+cur);
	}

	function cleanup() {
		dk.console.attr.value = sattr;
	}

	str = box_top(38, title);
	dk.console.gotoxy(x, y);
	lw(str);
	show_opts();
	str = box_bottom(38);
	dk.console.gotoxy(x, y+opts.length+1);
	lw(str);
	dk.console.gotoxy(x+3, y+1+cur);

	while(1) {
		ch = getkey().toUpperCase();
		switch(ch) {
			case '8':
			case 'KEY_UP':
			case '4':
			case 'KEY_LEFT':
				cur--;
				if (cur < 0)
					cur = opts.length - 1;
				break;
			case '2':
			case 'KEY_DOWN':
			case '6':
			case 'KEY_RIGHT':
				cur++;
				if (cur >= opts.length)
					cur = 0;
				break;
			case '\r':
				return opts[cur].ret;
		}
		show_opts();
	}
}

function decorate_item(it)
{
	var str = '';
	if (it.armour)
		str += ' `r2`^A`2`r0';
	if (it.weapon)
		str += ' `r4`^W`2`r0';
	if (it.useonce)
		str += ' `r5`^1`2`r0';
	return str;
}

function view_inventory()
{
	var inv;
	var lb;
	var choices;
	var i;
	var ch;
	var cur = 0;
	var str;
	var attr = dk.console.attr.value;
	var use_opts;
	var desc;

	function draw_menu() {
		choices.forEach(function(c, i) {
			dk.console.gotoxy(0, 12+i);
			if (i === cur)
				c = '`r1'+c+'`r0';
			lw(c);
		});
		dk.console.gotoxy(0, 12+i);
	}

	function draw_inv() {
		lb.forEach(function(c, i) {
			dk.console.gotoxy(0, 12+i);
			lln(c);
		});
		dk.console.gotoxy(0, 12+cur);
	}

	function clear_block() {
		var i;

		dk.console.attr.value = 2;
		for (i = 0; i < 11; i++) {
			dk.console.gotoxy(0, 12+i);
			dk.console.cleareol();
		}
		dk.console.gotoxy(0, 12);
	}

rescan:
	while(1) {
		run_ref('stats', 'gametxt.ref');
		inv = [];
		lb = [];
		choices = [];
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
			return;
		}
		else {
			inv.forEach(function(i) {
				desc = items[i].description;
				str = '`2  '+items[i].name;
				choices.push(str);
				str += decorate_item(items[i]);
				if (items[i].armour) {
					if (player.armournumber === i + 1)
						desc = 'Currently wearing as armour.';
				}
				if (items[i].weapon) {
					if (player.weaponnumber === i + 1)
						desc = 'Currently armed as weapon.';
				}
				str += spaces(37 - displen(str));
				str += '`2 (`0'+player.i[i]+'`2)';
				str += spaces(47 - displen(str));
				str += '`2 ' + desc;
				str += spaces(79 - displen(str));
				lb.push(str);
			});

			draw_inv();
			while(1) {
				draw_menu();
				dk.console.gotoxy(0, 23);
				ch = getkey().toUpperCase();
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
					case 'D':
						dk.console.gotoxy(17, 12);
						lw(box_top(42, items[inv[cur]].name));
						dk.console.gotoxy(17, 13);
						lw(box_middle(42, ''));
						dk.console.gotoxy(17, 14);
						lw(box_middle(42, '   `$Drop how many? '));
						dk.console.gotoxy(17, 15);
						lw(box_middle(42, ''));
						dk.console.gotoxy(17, 16);
						lw(box_bottom(42, ''));
						dk.console.gotoxy(38, 14);
						// TODO: This isn't exactly right... cursor is in wrong position, and selected colour is used.
						ch = dk.console.getstr({edit:player.i[inv[cur]].toString(), integer:true, input_box:true, attr:new Attribute(47), len:11});
						lw('`r1`0');
						ch = parseInt(ch, 10);
						if (!isNaN(ch) && ch <= player.i[inv[cur]]) {
							player.i[inv[cur]] -= ch;
							if (player.i[inv[cur]] === 0) {
								if (player.weaponnumber - 1 === inv[cur])
									player.weaponnumber = 0;
								if (player.armournumber - 1 === inv[cur])
									player.armournumber = 0;
							}
							dk.console.gotoxy(21, 14);
							if (ch === 1)
								lw('`$You go ahead and throw it away.`0');
							else
								lw('`$You drop the offending items!`0');
							getkey();
						}
						clear_block();
						continue rescan;
					case '\r':
						use_opts = [];
						if (items[inv[cur]].weapon) {
							if (player.weaponnumber - 1 !== inv[cur])
								use_opts.push({txt:'Arm as weapon', ret:'A'});
							else
								use_opts.push({txt:'Unarm as weapon', ret:'U'});
						}
						if (items[inv[cur]].armour) {
							if (player.armournumber - 1 !== inv[cur])
								use_opts.push({txt:'Wear as armour', ret:'W'});
							else
								use_opts.push({txt:'Take it off', ret:'T'});
						}
						if (items[inv[cur]].refsection.length > 0 && items[inv[cur]].useaction.length > 0) {
							use_opts.push({txt:items[inv[cur]].useaction, ret:'S'});
						}
						if (use_opts.length === 0)
							// TODO: Test this... it's almost certainly broken.
							use_opts.push({txt:'You can\'t think of any way to use this item', ret:'N'});
						else
							use_opts.push({txt:'Nevermind', ret:'N'});
						ch = popup_menu(items[inv[cur]].name, use_opts);
						clear_block();
						switch(ch) {
							case 'A':
								player.weaponnumber = inv[cur] + 1;
								break;
							case 'U':
								player.weaponnumber = 0;
								break;
							case 'W':
								player.armournumber = inv[cur] + 1;
								break;
							case 'T':
								player.armournumber = 0;
								break;
							case 'S':
								run_ref(items[inv[cur]].refsection, 'items.ref');
								if (items[inv[cur]].useonce) {
									player.i[inv[cur]]--;
									if (player.i[inv[cur]] === 0) {
										if (player.weaponnumber - 1 === inv[cur])
											player.weaponnumber = 0;
										if (player.armournumber - 1 === inv[cur])
											player.armournumber = 0;
									}
								}
								break;
						}
						continue rescan;
					case 'Q':
						dk.console.gotoxy(0, 12+lb.length-1);
						dk.console.attr.value = attr;
						return;
				}
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
			update();
		} while (!dk.console.waitkey(game.delay));
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
				break;
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
					dk.console.gotoxy(player.x - 1, player.y - 1);
				}
				else
					ch = 'Q';
				break;
			case 'T':
				run_ref('talk', 'help.ref');
				break;
			case 'V':
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

function dump_items()
{
	items.forEach(function(i) {
		Item_Def.forEach(function(d) {
			log(d.prop+': '+i[d.prop]);
		});
		log('=============');
	});
}

function dump_player()
{
	Player_Def.forEach(function(d) {
		log(d.prop+': '+player[d.prop]);
	});
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
		players.push({x:p.x, y:p.y, map:p.map, onnow:p.onnow, busy:p.busy, battle:p.battle});
		items.push(ifile.get(i));
	}
}

function load_time()
{
	var newday = false;
	var sday;
	var f = new File(js.exec_dir + 'stime.dat');
	var d = new Date();
	var tday = d.getFullYear()+(d.getMonth()+1)+d.getDate(); // Yes, really.

	if (!file_exists(f.name)) {
		file_mutex(f.name, tday+'\n');
		newday = true;
		state.time = 0;
	}
	else {
		if (!f.open('r'))
			throw('Unable to open '+f.name);
		sday = parseInt(f.readln(), 10);
		if (sday !== tday)
			newday = true;
		f.close();
	}
	f = new File(js.exec_dir + 'time.dat');
	if (!file_exists(f.name)) {
		state.time = 0;
		file_mutex(f.name, state.time+'\n');
		newday = true;
	}
	else {
		if (!f.open('r'))
			throw('Unable to open '+f.name);
		state.time = parseInt(f.readln(), 10);
	}

	if (newday) {
		f = new File(js.exec_dir + 'stime.dat');
		if (!f.open('r+'))
			throw('Unable to open '+f.name);
		f.truncate(0);
		f.writeln(tday);
		f.close;
		state.time++;
		f = new File(js.exec_dir + 'time.dat');
		if (!f.open('r+'))
			throw('Unable to open '+f.name);
		f.truncate(0);
		f.writeln(state.time);
		f.close;
		run_ref('maint', 'maint.ref');
	}
}

function load_game()
{
	game = gfile.get(0);
}

var pfile = new RecordFile(js.exec_dir + 'trader.dat', Player_Def);
var mfile = new RecordFile(js.exec_dir + 'map.dat', Map_Def);
var wfile = new RecordFile(js.exec_dir + 'world.dat', World_Def);
var ifile = new RecordFile(js.exec_dir + 'items.dat', Item_Def);
var ufile = new RecordFile(js.exec_dir + 'update.tmp', Update_Def);
var gfile = new RecordFile(js.exec_dir + 'game.dat', Game_Def);
world = wfile.get(0);
load_player();
load_players();
load_items();
load_time();
load_game();

run_ref('rules', 'rules.ref');
if (player.Record === undefined)
	run_ref('newplayer', 'gametxt.ref');

run_ref('startgame', 'gametxt.ref');
js.on_exit('if (player !== undefined) { update_rec.onnow = 0; update_rec.busy = 0; update_rec.battle = 0; update_rec.map = player.map; update_rec.x = player.x; update_rec.y = player.y; update_rec.put(); player.onnow = 0; player.busy = 0; player.battle = 0; player.put(); }');
player.onnow = 1;
player.busy = 0;
player.battle = 0;
player.put();
do_map();
