'use strict';

// TODO: More optimal horizontal lightbars
// TODO: Multiplayer interactions
// TODO: Save player after changes in case process crashes

js.yield_interval = 0;
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
var killfiles = [];
var enemy;
var saved_cursor = {x:0, y:0};
var progname = '';
var time_warnings = [];
var file_pos = {};

var items = [];
var other_players = {};
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

var UCASE = true;
var matchcase = true;
function getfname(str)
{
	str = str.replace(/\\/g,'/');
	var ec = file_getcase(js.exec_dir + str);

	if (ec !== undefined) {
		return ec;
	}
	if (UCASE) {
		return js.exec_dir + str.toUpperCase();
	}
	return str.toLowerCase();
}

function savetime()
{
	var n = new Date();

	return n.getHours()*60 + n.getMinutes();
}

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
	version:{type:'const', val:103},
	'nil':{type:'const', val:''},
	'`n':{type:'fn', get:function() { return player.name } },
	'`e':{type:'fn', get:function() { return getvar('enemy'); } },
	'`g':{type:'int', val:3}, // TODO: >= 3 is ANSI or something...
	'`x':{type:'const', val:' '},
	'`d':{type:'const', val:'\b'},
	'`\\':{type:'const', val:'\r\n'},
	'`*':{type:'const', val:dk.connection.node},
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
	blockpassable:{type:'fn', get:function() { return (map.mapinfo[getpoffset()].terrain === 1 ? 1 : 0); } },
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
	'&lmap':{type:'fn', get:function() { return player.lastmap } },
	'&time':{type:'fn', get:function() { return state.time }, set:function(x) { throw('attempt to set &time'); } },
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
var morestr = '`2<`0MORE`2>';
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
	if (typeof str !== 'string')
		return str;
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

function getsvar(name, vname)
{
	var v = getvar(name);
	var fv = getvar(vname);

	if (typeof fv === 'string' && typeof v !== 'string')
		return replace_vars(name);
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

function replace_vars(str)
{
	if (typeof str !== 'string')
		return str;
	str = str.replace(/([Ss]?&[A-Za-z]+)/g, function(m, r1) { return getvar(r1); });
	str = str.replace(/(`[Vv][0-4][0-9])/g, function(m, r1) { return getvar(r1); });
	str = str.replace(/(`[Ss][0-1][0-9])/g, function(m, r1) { return getvar(r1); });
	str = str.replace(/(`[Pp][0-9][0-9])/g, function(m, r1) { return getvar(r1); });
	str = str.replace(/(`[Tt][0-9][0-9])/g, function(m, r1) { return getvar(r1); });
	str = str.replace(/(`[Ii][0-9][0-9])/g, function(m, r1) { return getvar(r1); });
	str = str.replace(/(`[Ii][0-9][0-9])/g, function(m, r1) { return getvar(r1); });
	str = str.replace(/(`\+[0-9][0-9])/g, function(m, r1) { return getvar(r1); });
	str = str.replace(/(`[nexd\\\*])/g, function(m, r1) { return getvar(r1); });
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

function superclean(str)
{
	return remove_colour(expand_ticks(replace_vars(str)));
}

function lord_to_ansi(str)
{
	var ret = '';
	var i;

	for (i=0; i<str.length; i += 1) {
		if (str[i] === '`') {
			i += 1;
			switch(str[i]) {
				case 'c':
					ret += '\x1b[2J\x1b[H\r\n\r\n';
					break;
				case '1':
					ret += '\x1b[0;34m';
					break;
				case '*':
					ret += '\x1b[0;30;41m';
					break;
				case '2':
					ret += '\x1b[0;32m';
					break;
				case '3':
					ret += '\x1b[0;36m';
					break;
				case '4':
					ret += '\x1b[0;31m';
					break;
				case '5':
					ret += '\x1b[0;35m';
					break;
				case '6':
					ret += '\x1b[0;33m';
					break;
				case '7':
					ret += '\x1b[0;37m';
					break;
				case '8':
					ret += '\x1b[1;30m';
					break;
				case '9':
					ret += '\x1b[1;34m';
					break;
				case '0':
					ret += '\x1b[1;32m';
					break;
				case '!':
					ret += '\x1b[1;36m';
					break;
				case '@':
					ret += '\x1b[1;31m';
					break;
				case '#':
					ret += '\x1b[1;35m';
					break;
				case '$':
					ret += '\x1b[1;33m';
					break;
				case '%':
					ret += '\x1b[1;37m';
					break;
				case 'r':
					i += 1;
					switch(str[i]) {
						case 0:
							ret += '\x1b[40m';
							break;
						case 1:
							ret += '\x1b[44m';
							break;
						case 2:
							ret += '\x1b[42m';
							break;
						case 3:
							ret += '\x1b[43m';
							break;
						case 4:
							ret += '\x1b[41m';
							break;
						case 5:
							ret += '\x1b[45m';
							break;
						case 6:
							ret += '\x1b[47m';
							break;
						case 7:
							ret += '\x1b[46m';
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

	curlinenum = 1;
	lw('`r0`2  '+morestr);
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

function space_pad(str, len)
{
	var dl = displen(str);

	while (dl < len) {
		str += ' ';
		dl++;
	}

	return str;
}

var bar_timeout = 0;
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
 * UPDATE
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
 * MAIL\TEMP1.QWT is where the message to quote goes.
 * MAIL\TEM1.TMP is where the message is composed to.
 * MAIL\MAB1.DAT seems to be all mail displayed this session.
 * MAIL\MAT1.DAT is the currently RXed file (MAIL1.DAT is renamed to this I betcha)
 * 
 * Other interesting MAIL things...
 * MAIL\BAT
 * MAIL\CHAT
 * MAIL\GIV
 * MAIL\INF
 * MAIL\LOG *
 * MAIL\MAB *
 * MAIL\MAT *
 * MAIL\TALK *
 * MAIL\TCON
 * MAIL\TEM *
 * MAIL\TEMP *
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
 * -500 is running away
 * -1000 is yelling something
 * To BAT<them>.dat
 * 
 * Rankings are stored in three files...
 * TEMP0.DAT (LORDANSI)
 * SCORE.ANS
 * SCORE.TXT
 * 
 */

var current_saybar = spaces(76);
function redraw_bar(updstatus)
{
	var attr = dk.console.attr.value;
	var x = scr.pos.x;
	var y = scr.pos.y;

	if (updstatus && bar_timeout === undefined)
		status_bar();
	dk.console.gotoxy(2, 20);
	dk.console.attr.value = 31;
	lw(current_saybar);
	dk.console.gotoxy(x, y);
	dk.console.attr.value = attr;
}

function update_bar(str, msg, timeout)
{
	str = replace_vars(str);
	var dl = displen(str);
	var l;

	if (msg && str.indexOf(':') > -1) {
		if (!lfile.open('ab'))
			throw('Unable to open '+lfile.name);
		lfile.write(str+'\r\n');
		lfile.close();
	}
	if (msg && dl < 76) {
		l = parseInt((76 - dl) / 2, 10);
		str = spaces(l) + str;
		dl += l;
	}
	l = 76 - dl;
	str = str + spaces(l);
	if (timeout !== undefined) {
		// TODO: Apparently, the time is supposed to be relative to the message len...
		bar_timeout = new Date().valueOf() + parseInt(timeout * 1000, 10);
	}
	else
		bar_timeout = undefined;
	current_saybar = str;
	redraw_bar(false);
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

function tfile_append(str) {
	if (!tfile.open('ab'))
		throw('Unable to open '+tfile.name);
	tfile.write(str+'\r\n');
	tfile.close();
	timeout_bar();
}

function timeout_bar()
{
	var al;
	var tl;

	if (bar_timeout === undefined && file_size(tfile.name) === 0)
		return;
	if (bar_timeout === undefined || new Date().valueOf() > bar_timeout) {
		if (file_size(tfile.name) > 0) {
			if (!tfile.open('r+b'))
				throw('Unable to open '+tfile.name);
			al = tfile.readAll();
			tl = al.shift();
			tfile.position = 0;
			al.forEach(function(l) {
				tfile.write(l+'\r\n');
			});
			tfile.truncate(tfile.position);
			tfile.close();
			update_bar(tl, true, 4);
		}
		else
			status_bar();
	}
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

function get_inventory()
{
	var inv = [];
	for (i = 0; i < 99; i++) {
		if (player.i[i] > 0)
			inv.push(i+1);
	}
	return inv;
}

function chooseplayer()
{
	var i;
	var pl;
	var pn;
	var ch;

	lln('`r0`2  Who would you like to send a message to?');
	sln('');
	lln('  `2(`0full or `%PARTIAL`0 name`2).')
	lw('  `2NAME `8: `%');
	pn = superclean(dk.console.getstr({len:26}));
	sln('');

	for (i = 0; i < pfile.length; i++) {
		pl = pfile.get(i);
		if (pl.name.indexOf(pn) !== 0) {
			lw('\r');
			dk.console.cleareol();
			lw('  `2You mean `0'+pl.name+'`2?  [`0Y`2] `8: ');
			ch = getkey().toUpperCase();
			if (ch !== 'N')
				ch = 'Y';
			if (ch === 'Y')
				return i;
		}
	}
	lw('\r  `2Sorry - no one by that name lives \'round these parts.\r\n');
	return undefined;
}

function run_ref(sec, fname)
{
	var line;
	var m;
	var cl;
	var args;
	var ret;

	fname = fname.toLowerCase();
	sec = sec.toLowerCase();

	// TODO: Some things ignore comments and trailing whitespace, but ANSI certainly works for some things...
	function getlines() {
		var ret = [];

		while (line < files[fname].lines.length && files[fname].lines[line+1].search(/^\s*@/) === -1) {
			if (files[fname].lines[line+1].search(/^\s*;/) === -1)
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
			var to;
			var l = replace_vars(getvar(args[0]));

			if (args.length > 1)
				to = getvar(args[1]).toLowerCase();
			if (to === undefined || to === 'all') {
				players.forEach(function(p, i) {
					var f;

					if (p.deleted === 1)
						return;
					if (p.onnow) {
						f = new File(getfname(maildir+'talk'+(i+1)+'.tmp'));
						if (f.open('ab')) {
							f.write(l+'\r\n');
							f.close();
						}
					}
				});
			}
			else {
				// TODO: Test alla this.
				to = parseInt(to, 10);
				if (isNaN(to))
					throw('Invalid talk to at '+fname+':'+line);
				if (player.deleted === 1)
					// TODO: Do we tell the player or something?
					return;
				if (players[to-1].onnow) {
					f = new File(getfname(maildir+'talk'+to+'.tmp'));
					if (f.open('ab')) {
						f.write(l+'\r\n');
						f.close();
					}
				}
			}
		},
		'readspecial':function(args) {
			var ch;

			do {
				ch = getkey().toUpperCase();
				if (ch === '\r')
					ch = args[1].substr(0, 1);
			} while (args[1].indexOf(ch) === -1);
			setvar(args[0], ch);
			sln(ch);
		},
		'goto':function(args) {
			// NOTE: This doesn't use getvar() because GREEN.REF has 'do goto bank'
			args[0] = replace_vars(args[0]).toLowerCase();
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
			s = dk.console.getstr({crlf:false, len:len, edit:(args[1].toLowerCase() === 'nil' ? '' : getvar(args[1])), input_box:true, attr:new Attribute(31)});
			setvar(args[2], s);
			dk.console.gotoxy(x, y);
			while (remove_colour(s).length < len)
				s += ' ';
			dk.console.attr = 15;
			lw(s);
			dk.console.attr = attr;
		},
		'readnum':function(args) {
			var x = scr.pos.x;
			var y = scr.pos.y;
			var attr = scr.attr.value;
			var len = getvar(args.shift());
			var s;
			var fg = 15;
			var bg = 1;
			var val = '';

			if (args.length > 1)
				fg = getvar(args.shift());
			if (args.length > 1)
				fg = getvar(args.shift());
			if (args.length > 0) {
				val = parseInt(getvar(args[0]), 10);
				if (isNaN(val))
					throw('Invalid default "'+args[0]+'" for readnum at '+fname+':'+line);
			}
			s = dk.console.getstr({crlf:false, len:len, edit:val.toString(), input_box:true, attr:new Attribute((bg<<4) | fg), integer:true});
			setvar('`v40', s);
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
		'strip':function(args) {
			setvar(args[0], getvar(args[0]).trim());
		},
		'upcase':function(args) {
			setvar(args[0], getvar(args[0]).toUpperCase());
		},
		'stripall':function(args) {
			setvar(args[0], remove_colour(clean_str(getvar(args[0]))));
		},
		'stripcode':function(args) {
			// TODO: How is this different than STRIPALL?
			setvar(args[0], remove_colour(clean_str(getvar(args[0]))));
		},
		'replace':function(args) {
			var hs;
			var f = getvar(args[0]);
			var r = getvar(args[1]);

			hs = getvar(args[2]);
			hs = hs.replace(f, r);
			setvar(args[2], hs);
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
			tfile_append(files[fname].lines[line]);
		},
		'pad':function(args) {
			var str = getvar(args[0]).toString();
			var dl = displen(str);
			var l = parseInt(getvar(args[1]), 10);

			str += spaces(l - dl);
			setvar(args[0], str);
		},
		'frontpad':function(args) {
			var str = getvar(args[0]).toString();
			var dl = broken_displen(str);
			var l = parseInt(getvar(args[1]), 10);

			str = spaces(l - dl) + str;
			setvar(args[0], str);
		},
		'rename':function(args) {
			file_rename(getfname(getvar(args[0]).toLowerCase()), getfname(getvar(args[1]).toLowerCase()));
		},
		'addlog':function(args) {
			var f = new File(getfname('lognow.txt'));
			line++;
			if (line > files[fname].lines.length)
				return;
			if (f.open('ab')) {
				cl = files[fname].lines[line];
				f.write(replace_vars(cl)+'\r\n');
				f.close();
			}
		},
		'delete':function(args) {
			file_remove(getfname(getvar(args[0]).toLowerCase()));
		},
		'statbar':function(args) {
			status_bar();
		},
		'trim':function(args) {
			var f = new File(getfname(getvar(args[0])));
			var len = getvar(args[1]);
			var al;

			if (!file_exists(f.name))
				return;
			if (!f.open('r+b'))
				throw('Unable to open '+f.name+' at '+fname+':'+line);
			al = f.readAll();
			while (al.length > len);
				al.shift();
			f.position = 0;
			al.forEach(function(l) {
				f.write(l+'\r\n');
			});
			f.truncate(f.position);
			f.close();
		},
		'getkey':function(args) {
			if (!dk.console.waitkey(0))
				return '_';
			return dk.console.getkey();
		},
		'readchar':function(args) {
			setvar(args[0], getkey());
		}
	};
	var handlers = {
		'do':function(args) {
			var tmp;

			if (args.length < 1 || args.length > 0 && args[0].toLowerCase() === 'do') {
				if (line + 1 >= files[fname].lines.length)
					throw('do at end of file');
				// Trailing do is not fatal... see jump.ref:21 in cnw
				//if (files[fname].lines[line + 1].search(/^\s*@begin/i) === -1)
				//	throw('trailing do at '+fname+':'+line);
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
				else if (args[2].toLowerCase() === 'getname') {
					tmp = pfile.get(clamp_integer(getvar(args[3]), '8'));
					setvar(args[0], tmp.name);
				}
				else if (args[2].toLowerCase() === 'deleted') {
					tmp = pfile.get(clamp_integer(getvar(args[3]), '8'));
					setvar(args[0], tmp.deleted);
				}
				else
					setvar(args[0], getsvar(args[2], args[0]));
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
				setvar(args[0], getvar(args[0]) + getsvar(args[2], args[0]).toString());
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
				setvar(args[0], random(clamp_integer(getvar(args[2]), 's32')) + clamp_integer(getvar(args[3]), 's32'));
				return;
			}
			if (args.length === 3 && args[1].toLowerCase() === 'random') {
				setvar(args[0], random(clamp_integer(getvar(args[2]), 's32')));
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
				case 'equals':
				case 'is':
					if (args[2].toLowerCase() === 'length') {
						tmp = remove_colour(expand_ticks(replace_vars(getvar(args[3])))).length;
					}
					else if (args[2].toLowerCase() === 'reallength') {
						tmp = getvar(args[3]).length;
					}
					else
						tmp = getsvar(args[2], args[0]);
					if (getvar(args[0]).toString() === tmp.toString())
						handlers.do(args.slice(4));
					else if (args[4].toLowerCase() === 'begin')
						handlers.begin(args.slice(5));
					break;
				case 'exist':
				case 'exists':
					if (file_exists(getfname(args[0].toLowerCase())) === (args[2].toLowerCase() === 'true'))
						handlers.do(args.slice(4));
					else if (args[4].toLowerCase() === 'begin')
						handlers.begin(args.slice(5));
					break;
				case 'inside':
					if (getvar(args[2]).toLowerCase().indexOf(getvar(args[0]).toLowerCase()) !== -1)
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
			// Don't do this, trailing whitespace... we now delete trailing WS so do it again.
			// We *can't* delete trailing whitespace because of felicity.ref:779 (sigh)
			//if (args.length > 0)
			//	throw('Unexpected arguments to begin at '+fname+':'+line);
			while (depth > 0) {
				line++;
				if (files[fname].lines[line] === undefined)
					throw('Ran off end of script at '+fname+':'+line);
				if (files[fname].lines[line].search(/^\s*@#/i) !== -1)
					depth = 0;
				if (files[fname].lines[line].search(/^\s*@begin/i) !== -1)
					depth++;
				if (files[fname].lines[line].search(/^\s*@end/i) !== -1)
					depth--;
			}
		},
		'routine':function(args) {
			var s = replace_vars(args[0]);
			if (args.length === 1) {
				run_ref(s, fname);
				return;
			}
			if (args[1].toLowerCase() === 'in') {
				run_ref(s, args[2]);
				return;
			}
			throw('Unable to parse routine at '+fname+':'+line);
		},
		'run':function(args) {
			var f = fname;
			var s = replace_vars(args[0]);

			if (args.length > 2 && args[1].toLowerCase() === 'in') {
				f = getvar(args[2]);
			}
			if (files[f] === undefined)
				load_ref(f);
			if (files[f].section[s] === undefined)
				throw('Unable to find run section '+sec+' in '+f+' at '+fname+':'+line);
			fname = f;
			line = files[f].section[s].line;
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
					sclrscr();
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
			var f = new File(getfname(getvar(args[0]).toLowerCase()));
			if (!f.open('ab'))
				throw('Unable to open '+f.name+' at '+fname+':'+line);
			getlines().forEach(function(l) {
				f.write(replace_vars(l)+'\r\n');
			});
			f.close();
		},
		'readfile':function(args) {
			var vs = getlines();
			var f = new File(getfname(getvar(args[0])));
			var l;

			if (f.open('r')) {
				while(vs.length > 0) {
					l = f.readln();
					if (l === null)
						break;
					setvar(vs.shift(), l);
				}
				f.close();
			}
			// TODO: Documentation says it won't change variables if file too short...
			// By felicity.ref ends up displaying junk...
			//while (vs.length) {
			//	setvar(vs.shift(), '');
			//}
		},
		'end':function(args) {},
		'label':function(args) {},
		// Appears to be used to end a slow/etc block
		'':function(args) {},
		'displayfile':function(args) {
			var lines;
			if (args.length < 1)
				throw('No filename for displayfile at '+fname+':'+line);
			var f = new File(getfname(getvar(args[0]).toLowerCase()));
			if (!file_exists(f.name)) {
				lln('`0File '+getvar(args[0])+' missing - please inform sysop');
				return;
			}
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
			var cur = getvar('`v01') - 1;
			var attr = dk.console.attr.value;
			var choice;

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
			choice = vbar(choices, {cur:cur});
			setvar('responce', cmap[choice.cur] + 1);
			setvar('`v01', cmap[choice.cur] + 1);
		},
		'halt':function(args) {
			if (args.length > 0)
				exit(clamp_integer(args[0], 's32'));
			exit(0);
		},
		'key':function(args) {
			if (args.length > 0) {
				switch(args[0].toLowerCase()) {
					case 'nodisplay':
						getkey();
						return;
					case 'top':
						dk.console.gotoxy(0,0);
						break;
					case 'bottom':
						dk.console.gotoxy(0,23);
						break;
					default:
						throw('Unhandled key arg "'+args[0]+'" at '+fname+':'+line);
				}
			}
			else
				lw('\r');
			dk.console.cleareol();
			lw(spaces(40-(displen(morestr)/2))+morestr);
			getkey();
			lw('\r');
			dk.console.cleareol();
		},
		'addchar':function(args) {
			var tmp;

			if (player.Record === undefined) {
				tmp = pfile.new();
				if (tmp.Record >= 200) {
					pfile.file.position = pfile.RecordLength * 200;
					pfile.truncate(pfile.RecordLength * 200);
					run_ref('full','gametxt.ref');
					exit(0);
				}
				player.Record = tmp.Record;
				ufile.new();
			}
			player.lastsaved = savetime();
			player.put();
			update_rec = ufile.get(player.Record);
			while(update_rec === null) {
				ufile.new();
				update_rec = ufile.get(player.Record);
			}
		},
		'offmap':function(args) {
			// TODO: Disappear to other players... this toggles busy because
			// it looks like that's what it does...
			player.busy = 1;
			update_update();
			player.put();
		},
		'busy':function(args) {
			// Actually, it doesn't seem this touches UPDATE.TMP *or* TRADER.DAT
			// TODO: Turn red for other players, and run @#busy if other player interacts
			// this toggles battle...
			//update_rec.battle = 1;
			//update_rec.put();
		},
		'drawmap':function(args) {
			draw_map();
		},
		'update':function(args) {
			player.busy = 0;
			update();
			player.put();
		},
		'update_update':function(args) {
			player.busy = 0;
			update_update();
			player.put();
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
			map = load_map(parseInt(getvar(args[0]), 10));
		},
		'loadworld':function(args) {
			world = wfile.get(0);
		},
		'saveworld':function(args) {
			world.put();
		},
		'buymanager':function(args) {
			var itms = getlines();
			var itm;
			var i;
			var cur = 0;
			var ch;
			var choice;

			for (i = 0; i < itms.length; i++)
				itms[i] = parseInt(itms[i], 10);
			// Don't clear the screen first?  Interesting...
			dk.console.gotoxy(0, 9);
			lw('`r5`%  Item To Buy                         Price                                     ');
			dk.console.gotoxy(0, 23);
			lw('`r5                                                                               ');
			dk.console.gotoxy(2, 23);
			lw('`$Q `2to quit, `$ENTER `2to buy item.        You have `$&gold `2gold.`r0');

			if (items.length === 0) {
				dk.console.gotoxy(0, 10);
				lw('  `2They have nothing to sell.  (press `%Q `2to continue)');
				do {
					ch = getkey.toUpperCase();
				} while(ch !== 'Q');
			}

			while(1) {
				choice = items_menu(itms, cur, true, false, '', 10, 22)
				cur = choice.cur;
				switch(choice.ch) {
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
							// TODO: `* is node number... sometimes!
							lw('`r4`^ ITEM BOUGHT! `%You now have ' + player.i[itm.Record] + ' of \'em.  `2(`0press a key to continue`2)`r0');
						}
						else {
							dk.console.gotoxy(1, 23);
							lw('`r4`^ ITEM NOT BOUGHT! `%You don\'t have enough gold.  `2(`0press a key to continue`2)`r0');
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
			var l = getlines();

			function split_ref(str, prefix) {
				var l = str.split('|');

				if (l.length < 2 || l[0].toUpperCase() === 'NONE' || l[1].toUpperCase() === 'NONE')
					l = ['',''];

				enemy[prefix+'_reffile'] = getvar(l[0]);
				enemy[prefix+'_refname'] = getvar(l[1]);
			}

			function add_attack(str) {
				var l = str.split('|');

				if (l.length < 2 || l[0].toUpperCase() === 'NONE' || l[1].toUpperCase() === 'NONE')
					return;

				enemy.attacks.push({strength:parseInt(getvar(l[1]), 10), hitaction:getvar(l[0])});
			}

			enemy = {
				name:getvar(l[0]),
				see:getvar(l[1]),
				killstr:getvar(l[2]),
				sex:parseInt(getvar(l[3]), 10),
				defence:parseInt(getvar(l[9]), 10),
				gold:parseInt(getvar(l[10]), 10),
				experience:parseInt(getvar(l[11]), 10),
				hp:parseInt(getvar(l[12]), 10),
				maxhp:parseInt(getvar(l[12]), 10),
				attacks:[]
			};
			add_attack(l[4]);
			add_attack(l[5]);
			add_attack(l[6]);
			add_attack(l[7]);
			add_attack(l[8]);
			split_ref(l[13], 'win');
			split_ref(l[14], 'lose');
			split_ref(l[15], 'run');
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
			var choice;

			dk.console.gotoxy(0, 6);
			lw('`r5`%  Item To Sell                        Amount Owned                              `r0');
			dk.console.gotoxy(0, 23);
			lw('`r5  `$Q `2to quit, `$ENTER `2to sell item.                                               `r0');

rescan:
			while (1) {
				dk.console.gotoxy(39, 23);
				lw('`2`r5You have `$&money `2gold.`r0');
				inv = get_inventory();
				if (inv.length === 0) {
					dk.console.gotoxy(0, 6);
					lw('`r0  `2You have nothing to sell.  (press `%Q `2to continue)');
					do {
						ch = getkey().toUpperCase();
					} while (ch != 'Q');
					return;
				}

				if (cur >= inv.length)
					cur = 0;

				while(1) {
					choice = items_menu(inv, cur, false, true, '', 7, 22);
					cur = choice.cur;
					switch(choice.ch) {
						case 'Q':
							return;
						case '\r':
							itm = items[inv[cur] - 1];
							amt = 1;
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
		},
		'dataload':function(args) {
			var f = new File(getfname(getvar(args[0]).toLowerCase()));
			var rec = getvar(args[1]);
			var val;

			if (!file_exists(f.name)) {
				f.open('wb');
				f.writeBin(state.time, 4);
				for (i = 0; i < 250; i++) {
					if ((i + 1) === rec)
						f.writeBin(val, 4);
					else
						f.writeBin(0, 4);
				}
				f.close();
				setvar(args[2], 0);
				return;
			}
			if (!f.open('rb'))
				throw('Unable to open '+f.name+' at '+fname+':'+line);
			f.position = rec * 4;
			val = f.readBin(4);
			f.close();
			setvar(args[2], val);
		},
		'datasave':function(args) {
			var f = new File(getfname(getvar(args[0]).toLowerCase()));
			var rec = getvar(args[1]);
			var val = replace_vars(getvar(args[2]));

			if (!file_exists(f.name)) {
				f.open('wb');
				f.writeBin(state.time, 4);
				for (i = 0; i < 250; i++) {
					if ((i + 1) === rec)
						f.writeBin(val, 4);
					else
						f.writeBin(0, 4);
				}
				f.close();
				return;
			}
			if (!f.open('r+b'))
				throw('Unable to open '+f.name+' at '+fname+':'+line);
			f.position = rec * 4;
			f.writeBin(val, 4);
			f.close();
		},
		'datanewday':function(args) {
			var f = new File(getfname(getvar(args[0]).toLowerCase()));
			var i;
			var d;

			if (!file_exists(f.name)) {
				f.open('wb');
				f.writeBin(state.time, 4);
				for (i = 0; i < 250; i++)
					f.writeBin(0, 4);
				f.close();
				return;
			}
			if (!f.open('r+b'))
				throw('Unable to open '+f.name+' at '+fname+':'+line);
			d = f.readBin(4);
			if (d != state.time) {
				f.position = 0;
				f.writeBin(state.time, 4);
				for (i = 0; i < 250; i++)
					f.writeBin(0, 4);
			}
			f.close();
		},
		'progname':function(args) {
			line++;
			if (line > files[fname].lines.length)
				return;
			cl = files[fname].lines[line];
			progname = replace_vars(cl);
		},
		'moremap':function(args) {
			line++;
			if (line > files[fname].lines.length)
				return;
			cl = files[fname].lines[line];
			morestr = replace_vars(cl);
		},
		'clear':function(args) {
			if (args[0].toLowerCase() === 'screen') {
				sclrscr();
				return;
			}
			throw('Unknown clear type at '+fname+':'+line);
		},
		'chooseplayer':function(args) {
			var pl = chooseplayer();

			if (pl === undefined)
				setvar(args[0], 0);
			else
				setvar(args[0], pl + 1);
		},
		'drawpart':function(args) {
			var x = getvar(args[0]);
			var y = getvar(args[1]);

			erase(x - 1, y - 1);
			update_space(x - 1, y - 1);
		},
		'loadcursor':function(args) {
			dk.console.gotoxy(saved_cursor.x, saved_cursor.y);
		},
		'savecursor':function(args) {
			saved_cursor = {x:scr.pos.x, y:scr.pos.y};
		},
		'overheadmap':function(args) {
			var off;
			var x, y;

			sclrscr();
			dk.console.gotoxy(0, 0);
			for (y = 0; y < 20; y++) {
				dk.console.gotoxy(0, y);
				for (x = 0; x < 80; x++) {
					off = y * 80 + x;
					if (world.hideonmap[off] || world.mapdatindex[off] < 1)
						background(1);
					else
						background(2);
					lw(' ');
				}
			}
		},
		'copyfile':function(args) {
			file_copy(getfname(getvar(args[0]).toLowerCase()), getfname(getvar(args[1]).toLowerCase()));
		},
		'convert_file_to_ansi':function(args) {
			var inf = new File(getfname(getvar(args[0])));
			var out = new File(getfname(getvar(args[1])));
			var l;

			if (!inf.open('r'))
				return;
			if (!out.open('wb')) {
				inf.close();
				return;
			}
			while ((l = inf.readln()) !== null) {
				out.write(lord_to_ansi(l)+'\r\n');
			}
			inf.close();
			out.close();
		},
		'convert_file_to_ascii':function(args) {
			var inf = new File(getfname(getvar(args[0])));
			var out = new File(getfname(getvar(args[1])));
			var l;

			if (!inf.open('r'))
				return;
			if (!out.open('wb')) {
				inf.close();
				return;
			}
			while ((l = inf.readln()) !== null) {
				out.write(remove_colour(l).replace(/`c/g,'')+'\r\n');
			}
			inf.close();
			out.close();
		},
		'display':function(args) {
			if (args.length > 2 && args[1].toLowerCase() === 'in') {
			}
			throw('Unsupported display at '+fname+':'+line);
		},
		'rank':function(args) {
			// TODO: No real clue what the filename is for...
			var rp = ranked_players(args[1]);
			var op = player;

			rp.forEach(function(pl, i) {
				player = pl;
				run_ref(format('`p%02d', getvar(args[2])), fname);
			});
			player = op;
		},
		'lordrank':function(args) {
			var f = new File(getfname(getvar(args[0])));
			var rp = ranked_players(args[1]);

			if (!f.open('ab'))
				return;
			rp.forEach(function(pl, i) {
				f.write((pl.sexmale === 1 ? '`0M' : '`#F')+' `2'+space_pad(pl.name, 21)+'`2'+format('%14d', pl.p[0])+'`%'+format('%5d', pl.p[8])+'     '+space_pad((pl.dead === 1 ? '`4Dead' : '`%Alive'), 6)+(pl.p[6] >= 0 ? '`0' : '`4') + format('%7d',pl.p[6])+'`%     '+pl.p[18]+'\r\n');
			});
			f.close();
		},
		'whoison':function(args) {
			var pl;
			var mp;

			players.forEach(function(p, i) {
				if (p.onnow === 1) {
					if (i === player.Record)
						pl = player;
					else
						pl = pfile.get(i);
					mp = load_map(pl.lastmap);
					lw('  `2'+space_pad(pl.name, 29)+'`%'+space_pad(pl.p[8].toString(), 14)+'`0'+mp.name);
				}
			});
		},
		'checkmail':function(args) {
			mail_check(false);
		}
	};

  	function handle(args)
	{
		if (handlers[args[0].toLowerCase()] === undefined)
			throw('No handler for '+args[0]+' command at '+fname+':'+line);
		return handlers[args[0].toLowerCase()](args.slice(1));
	}

	function load_ref(fname) {
		var obj = {section:{}};
		fname = fname.toLowerCase();
		var f;
		var cs;
		var i;
		var enc = false;

		function decrypt(o) {
			o.forEach(function(l, i) {
				var off;
				var odd;
				var con;
				var ret = '';
				var ch;
				var ct = 1;

				con = ascii(l.substr(4, 1));
				odd = ascii(l.substr(2, 1));
				for (off = 5; off < l.length; off++) {
					ch = ascii(l.substr(off, 1));
					ch -= ct;
					ct++;
					if (ct == 10)
						ct = 0;
					ch -= con;
					if (ct & 1)
						ch -= odd;
					ret += ascii(ch);
				}
				o[i] = ret;
			});
		}

		f = new File(getfname(fname));
		if (!file_exists(f.name)) {
			f = new File(getfname(fname.replace(/\.ref$/, '.rec')));
			if (file_exists(f.name))
				enc = true;
		}
		if (!f.open('r'))
			throw('Unable to open '+f.name);
		if (enc) {
			obj.lines = [];
			while(1) {
				i = f.read(2);
				if (i === '')
					break;
				i += f.read(ascii(i.substr(1,1))-3);
				f.readln();
				obj.lines.push(i);
			}
			decrypt(obj.lines);
		}
		else
			obj.lines = f.readAll(4096);
		f.close();
		obj.lines.unshift(';secret line zero... shhhhh');
		obj.lines.forEach(function(l, n) {
			var m;
			var sc;

			if (l.search(/^\s*@/) !== -1) {
				sc = l.indexOf(';');
				if (sc > -1)
					l = l.substr(0, sc);
				// We can't delete trailing whitespace because of felicity.ref:779
				//l = l.trim();
				l = l.replace(/^\s+/,'');
			}
			obj.lines[n] = l;
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
	if (fname.indexOf('.') === -1)
		fname += '.ref';
	if (files[fname] === undefined)
		load_ref(fname);
	if (files[fname].section[sec] === undefined)
		throw('Unable to find section '+sec+' in '+fname);
	line = files[fname].section[sec].line;

	while (1) {
		line++;
//log(fname+':'+line);
		if (line >= files[fname].lines.length)
			return;
		cl = files[fname].lines[line].replace(/^\s*/,'');
		if (cl.search(/^@#/) !== -1)
			return;
		if (cl.search(/^\s*@closescript/i) !== -1)
			return;
		if (cl.search(/^\s*@itemexit/i) !== -1)
			return 'ITEMEXIT';
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
		ret = handle(args);
	}
}

function load_player()
{
	var i;
	var p;

	for (i = 0; i < pfile.length; i++) {
		p = pfile.get(i);
		if (p.deleted === 1)
			continue;
		if (p.realname === dk.user.full_name) {
			player = p;
			update_rec = ufile.get(p.Record);
			while(update_rec === null) {
				ufile.new();
				update_rec = ufile.get(player.Record);
			}
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

function update_update()
{
	update_rec.x = player.x;
	update_rec.y = player.y;
	update_rec.map = player.map;
	update_rec.busy = player.busy;
	update_rec.battle = player.battle;
	update_rec.onnow = 1;
	update_rec.put();
}

function update_space(x, y)
{
	x += 1;
	y += 1;

	players.forEach(function(u, i) {
		if (u.map !== player.map)
			return;
		if (u.x !== x || u.y !== y)
			return;
		// Note that 'busy' is what 'offmap' toggles, not what 'busy' does. *sigh*
		if (i === player.Record) {
			foreground(15);
			background(map.mapinfo[getoffset(u.x-1, u.y-1)].backcolour);
			dk.console.print('\x02');
		}
		else {
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
		}
	});
}

function mail_check(messenger)
{
	var fn = getfname(maildir+'mail'+(player.Record + 1)+'.dat');
	var f = new File(getfname(maildir+'mat'+(player.Record + 1)+'.dat'));
	var l;
	var m;
	var op;
	var ch;
	var reply = [];

	if (!file_exists(fn))
		return;

	if (messenger) {
		update_bar('`2A messenger stops you with the following news. (press `0R`2 to read it)', true);
		do {
			ch = getkey().toUpperCase();
		} while (ch !== 'R');
		lw('`r0`c');
	}

	// TODO: Not sure what happens on windows of someone else has the file open...
	// We likely need a file mutex here.
	file_rename(fn, f.name);
	if (!f.open('r'))
		throw('Unable to open '+f.name);
	while(1) {
		l = f.readln();
		if (l === null)
			break;
		m = l.match(/^@([A-Z]+)(?: (.*))?$/);
		if (m !== null) {
			switch (m[1]) {
				case 'KEY':
					more();
					break;
				case 'MESSAGE':
					lln('  `2"Message from `0'+m[2]+'`2,".');
					sln('');
					lln('`0  '+repeat_chars(ascii(0xc4), 77));
					reply = [];
					break;
				case 'REPLY':
					op = parseInt(m[2], 10);
					if (isNaN(op))
						throw('Invalid REPLY ID');
					op = pfile.get(op - 1);
					if (op === null)
						throw('Invalid record number '+op+' in REPLY');
					lw('  `2Reply to `0'+op.name+'`2? [`0Y`2] : `%');
					do {
						ch = getkey().toUpperCase();
						if (ch === '\r')
							ch = 'Y';
					} while('YN'.indexOf(ch) === -1);
					if (ch === 'Y') {
						if (op.deleted) {
							lln('\r  `2You decide answering someone who is dead would be useless.');
						}
						else {
							mail_to(op.Record, reply);
						}
					}
					break;
				case 'DONE':
					lln('`0  '+repeat_chars(ascii(0xc4), 77));
					sln('');
					break;
			}
		}
		else {
			if (l.substr(0, 4) === '  `2')
				reply.push(' `0>`3'+l.substr(4));
			if (l !== '')
				lln(l);
		}
	}
	if (messenger) {
		sln('');
		more();
		draw_map();
		update();
	}
}

function chat(op)
{
	var pos = 0;
	var ch = new File(getfname(maildir + 'chat'+(player.Record + 1)+'.tmp'));
	var chop = new File(getfname(maildir + 'chat'+(op.Record + 1)+'.tmp'));
	var l;

	lln('`r0`c`2  You sit down and talk with '+op.name+'`2.');
	sln('  (enter q or x to exit)');
	sln('');
	while(1) {
		if (ch.open('r')) {
			ch.position = pos;
			l = ch.readln();
			pos = ch.position;
			ch.close();
			if (l !== null) {
				if (l === 'EXIT') {
					lln('  `0'+op.name+'`2 has left.');
					file_remove(ch.name);
					more();
					return;
				}
				lln(l+'`r');
				continue;
			}
		}
		if (dk.console.waitkey(game.delay)) {
			sw('  ');
			l = clean_str(dk.console.getstr({len:72, attr:new Attribute(31), input_box:true, crlf:false}));
			lw('`r0`2`r');
			dk.console.cleareol();
			switch (l.toUpperCase()) {
				case 'X':
				case 'Q':
					if (!chop.open('ab'))
						throw('Unable to open '+chop.name);
					chop.write('EXIT\r\n');
					chop.close();
					update_update()
					file_remove(ch.name);
					return;
				case '':
					break;
				default:
					sw('\r');
					lln('  `0'+player.name+' `$: '+l+'`2');
					if (!chop.open('ab'))
						throw('Unable to open '+chop.name);
					chop.write('  `0'+player.name+' `2: '+l+'\r\n');
					chop.close();
					break;
			}
		}
	}
}

function hailed(pl)
{
	var op = pfile.get(pl - 1);
	var tx = new File(getfname(maildir + 'tx'+(op.Record + 1)+'.tmp'));
	var inn;
	var inf;
	var al;
	var pos = 0;
	var exit = false;

	if (!file_exists(tx.name))
		return;
	update_bar('`0'+op.name+' `2hails you.  Press (`0A`2) to leave.', true);
	lw('`r0`2');
	file_remove(tx.name);
	do {
		inn = getfname(maildir + 'inf'+(player.Record + 1)+'.tmp');
		inn = file_getcase(inn);
		if (inn !== undefined) {
			inf = new File(inn)
			if (!inf.open('r'))
				throw('Unable to open '+inf.name);
			inf.position = pos;
			al = inf.readAll();
			pos = inf.position;
			inf.close();
			al.forEach(function(l) {
				if (l === 'CHAT') {
					chat(op);
					exit = true;
				}
			});
		}
		if (dk.console.waitkey(game.delay)) {
			if (getkey().toUpperCase() === 'A')
				break;
		}
		op.reLoad();
	} while((!exit) && op.battle === 1);
	if (inf !== undefined)
		file_remove(inf.name);
	draw_map();
	redraw_bar(true);
	update();
}

function con_check()
{
	var al;

	if (file_pos.con === undefined)
		file_pos.con = 0;
	if (!cfile.open('r'))
		throw('Unable to open '+cfile.name);
	cfile.position = file_pos.con;
	al = cfile.readAll();
	file_pos.con = cfile.position;
	cfile.close();
	al.forEach(function(l) {
		var c = l.split('|');

		switch(c[0]) {
			case 'BUILDINDEX':
				// TODO: No idea...
				break;
			case 'CONNECT':
				player.battle = 1;
				update_update();
				player.put();
				hailed(parseInt(c[1], 10));
				player.battle = 0;
				update_update();
				player.put();
				break;
			case 'UPDATE':
				// TODO: No idea...
				update();
				break;
			case 'ADDGOLD':
				player.money += parseInt(c[1], 10);
				break;
			case 'ADDITEM':
				player.i[parseInt(c[1], 10) - 1] += parseInt(c[2], 10);
				break;
		}
	});
}

var next_update = -1;
function update(skip) {
	var i;
	var u;
	var nop = {};
	var op;
	var now = (new Date().valueOf());
	var done;

	if ((!skip) || now > next_update) {
		next_update = now + game.delay;
		// First, update player data
		for (i = 0; i < ufile.length; i++) {
			if (i === player.Record)
				continue;
			if (i >= players.length) {
				op = pfile.get(i);
				if (op === null)
					break;
				players.push({x:op.x, y:op.y, map:op.map, onnow:op.onnow, busy:op.busy, battle:op.battle, deleted:op.deleted});
			}
			if (players[i].deleted)
				continue;
			u = ufile.get(i);
			if (u.map !== 0 && u.x !== 0 && u.y !== 0)
				players[i] = u;
		}

		// First, erase any moved players and update other_players
		done = new Array(80*20);
		players.forEach(function(u, i) {
			if (i === player.Record)
				return;
			if (u.deleted)
				return;
			if (u.map === player.map) {
				nop[i] = {x:u.x, y:u.y, map:u.map, onnow:u.onnow, busy:u.busy, battle:u.battle}
				// Erase old player pos...
				if (other_players[i] !== undefined) {
					op = other_players[i];
					if (done[getoffset(u.x, u.y)] === undefined && (op.x !== u.x || op.y !== u.y || op.map !== u.map) && (op.x !== player.x || op.y !== player.y)) {
						erase(op.x - 1, op.y - 1);
						done[getoffset(u.x, u.y)] = true;
					}
				}
			}
		});

		// Now, draw all players on the map
		done = new Array(80*20);
		Object.keys(nop).forEach(function(k) {
			u = nop[k];
			// Note that 'busy' is what 'offmap' toggles, not what 'busy' does. *sigh*
			if (done[getoffset(u.x, u.y)] === undefined && u.busy === 0 && (u.x !== player.x || u.y !== player.y)) {
				dk.console.gotoxy(u.x - 1, u.y - 1);
					foreground(4);
				if (u.battle)
					foreground(4);
				else
					foreground(7);
				background(map.mapinfo[getoffset(u.x-1, u.y-1)].backcolour);
				dk.console.print('\x02');
				done[getoffset(u.x, u.y)] = true;
			}
		});
		other_players = nop;

		timeout_bar();
		mail_check(true);
		con_check();
	}
	dk.console.gotoxy(player.x - 1, player.y - 1);
	foreground(15);
	background(map.mapinfo[getpoffset()].backcolour);
	dk.console.print('\x02');
	dk.console.gotoxy(player.x - 1, player.y - 1);
	update_update();
}

function draw_map() {
	var x;
	var y;
	var off;
	var mi;
	var s;

	dk.console.attr.value = 7;
	sclrscr();
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
	redraw_bar(true);
	other_players = {};
}

function load_map(mapnum)
{
	return mfile.get(world.mapdatindex[mapnum - 1] - 1);
}

function get_timestr()
{
	var desc;
	var secleft = dk.user.seconds_remaining - (time() - dk.user.seconds_remaining_from);
	var minleft = Math.floor(secleft / 60);

	if (time_warnings.length < 1)
		return '';

	if (minleft < 0)
		minleft = 0;

	if (player.p[10] > time_warnings[0])
		desc = 'It is morning.';
	else if (player.p[10] > time_warnings[1])
		desc = 'It is noon.';
	else if (player.p[10] > time_warnings[2])
		desc = 'It is late afternoon.';
	else if (player.p[10] > time_warnings[3])
		desc = 'It is getting dark.';
	else if (player.p[10] > time_warnings[4])
		desc = 'You are getting very sleepy.';
	else if (player.p[10] > time_warnings[5])
		desc = 'You are about to faint.';
	else
		return '`2You have fainted.  You will not be able to move until tommorow.';

	return '`%Time: `2'+desc+'  You have `0'+pretty_int(player.p[10])+'`2 turns and `0'+pretty_int(minleft)+'`2 minutes left.';
}

function move_player(xoff, yoff) {
	var x = player.x + xoff;
	var y = player.y + yoff;
	var moved = false;
	var special = false;
	var newmap = false;
	var perday;

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
		map = load_map(player.map);
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
			update(true);
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
					map = load_map(player.map);
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
		perday = getvar('`v05');
		if (perday > 0) {
			if (time_warnings.indexOf(player.p[10]) !== -1)
				tfile_append(get_timestr());
		}
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
	var otxt = [];

	str = box_top(38, title);
	dk.console.gotoxy(x, y);
	lw(str);
	opts.forEach(function(o, i) {
		str = box_middle(38, o.txt, i === cur);
		dk.console.gotoxy(x, y+1+i);
		lw(str);
	});
	dk.console.gotoxy(x+3, y+1+cur);

	str = box_bottom(38);
	dk.console.gotoxy(x, y+opts.length+1);
	lw(str);

	dk.console.gotoxy(x+3, y+1+cur);
	opts.forEach(function(o) {
		otxt.push(o.txt);
	});

	ch = vbar(otxt, {drawall:false, x:x + 3, y:y + 1, highlight:'`r5`0', norm:'`r1`0'});
	return ch.cur;
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
	var it;
	var ch;
	var cur = 0;
	var str;
	var attr = dk.console.attr.value;
	var use_opts;
	var desc;
	var ret;
	var choice;

rescan:
	while(1) {
		run_ref('stats', 'gametxt.ref');
		inv = get_inventory();
		if (inv.length === 0) {
			dk.console.gotoxy(0, 12);
			lw('`2  You are carrying nothing!  (press `%Q`2 to continue)');
			do {
				ch = getkey().toUpperCase();
			} while (ch != 'Q');
			return;
		}
		else {
newpage:
			while (1) {
				choice = items_menu(inv, cur, false, false, 'D', 12, 22);
				cur = choice.cur;
				switch(choice.ch) {
					case 'D':
						dk.console.gotoxy(17, 12);
						lw(box_top(42, items[inv[cur] - 1].name));
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
						ch = dk.console.getstr({edit:player.i[inv[cur] - 1].toString(), integer:true, input_box:true, attr:new Attribute(47), len:11});
						lw('`r1`0');
						ch = parseInt(ch, 10);
						if (!isNaN(ch) && ch <= player.i[inv[cur] - 1]) {
							if (items[inv[cur] - 1].questitem) {
								dk.console.gotoxy(21, 14);
								lw('`$Naw, it might be useful later...');
							}
							else {
								player.i[inv[cur] - 1] -= ch;
								if (player.i[inv[cur] - 1] === 0) {
									if (player.weaponnumber === inv[cur])
										player.weaponnumber = 0;
									if (player.armournumber === inv[cur])
										player.armournumber = 0;
								}
								dk.console.gotoxy(21, 14);
								if (ch === 1)
									lw('`$You go ahead and throw it away.`0');
								else
									lw('`$You drop the offending items!`0');
							}
							getkey();
						}
						continue rescan;
					case '\r':
						use_opts = [];
						if (items[inv[cur] - 1].weapon) {
							if (player.weaponnumber !== inv[cur])
								use_opts.push({txt:'Arm as weapon', ret:'A'});
							else
								use_opts.push({txt:'Unarm as weapon', ret:'U'});
						}
						if (items[inv[cur] - 1].armour) {
							if (player.armournumber !== inv[cur])
								use_opts.push({txt:'Wear as armour', ret:'W'});
							else
								use_opts.push({txt:'Take it off', ret:'T'});
						}
						if (items[inv[cur] - 1].refsection.length > 0 && items[inv[cur] - 1].useaction.length > 0) {
							use_opts.push({txt:items[inv[cur] - 1].useaction, ret:'S'});
						}
						if (use_opts.length === 0)
							// TODO: Test this... it's almost certainly broken.
							use_opts.push({txt:'You can\'t think of any way to use this item', ret:'N'});
						else
							use_opts.push({txt:'Nevermind', ret:'N'});
						ch = popup_menu(items[inv[cur] - 1].name, use_opts);
						ret = undefined;
						switch(ch) {
							case 'A':
								player.weaponnumber = inv[cur];
								break;
							case 'U':
								player.weaponnumber = 0;
								break;
							case 'W':
								player.armournumber = inv[cur];
								break;
							case 'T':
								player.armournumber = 0;
								break;
							case 'S':
								ret = run_ref(items[inv[cur] - 1].refsection, 'items.ref');
								if (items[inv[cur] - 1].useonce) {
									player.i[inv[cur] - 1]--;
									if (player.i[inv[cur] - 1] === 0) {
										if (player.weaponnumber === inv[cur])
											player.weaponnumber = 0;
										if (player.armournumber === inv[cur])
											player.armournumber = 0;
									}
								}
								break;
						}
						if (ret === undefined || ret !== 'ITEMEXIT')
							continue rescan;
						// Fallthrough
					case 'Q':
						dk.console.attr.value = attr;
						return;
				}
			}
		}
	}
}

function show_player_names()
{
	var pl;
	var sattr = dk.console.attr.value;

	players.forEach(function(p, i) {
		if (p.deleted === 1)
			return;
		if (p.map === player.map) {
			pl = pfile.get(i);
			dk.console.gotoxy(p.x, p.y-1);
			lw('`r1`2'+pl.name);
		}
	});
	dk.console.attr.value = sattr;
	dk.console.gotoxy(2, 20);
	lw('`r1`%                          Press a key to continue                           `r0');
	getkey();
}

function hbar(x, y, opts)
{
	var cur = 0;
	var ch;

	while (1) {
		dk.console.gotoxy(x, y);
		opts.forEach(function(o, i) {
			dk.console.movex(2);
			if (i === cur)
				lw('`r1');
			lw('`%');
			lw(o);
			if (i === cur)
				lw('`r0');
		});

		ch = getkey();
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
				return cur;
		}
	}
}

function disp_hp(cur, max)
{
	if (cur < 1)
		return '`bDead';
	return '(`0'+cur+'`2/`0'+max+'`2)';
}

function your_hp()
{
	dk.console.gotoxy(2, 20);
	lw(space_pad('`r1`2Your hitpoints: '+disp_hp(player.p[1], player.p[2]), 32));
}

function enemy_hp(enm)
{
	dk.console.gotoxy(34, 20);
	lw(space_pad('`r1`0`e\'s `2hitpoints: '+disp_hp(enm.hp, enm.maxhp), 44));
}

function offline_battle()
{
	var ch;
	var dmg;
	var str;
	var hstr;
	var ehstr;
	var def;
	var wep;
	var astr;
	var him;
	var atk;
	var enm = enemy;
	var supr;

	enemy = undefined;
	switch(enm.sex) {
		case 1:
			him = 'him';
			break;
		case 2:
			him = 'her';
			break;
		default:
			him = 'it';
			break;
	}
	if (player.weaponnumber !== 0)
		wep = items[player.weaponnumber - 1];
	str = (player.weaponnumber === 0 ? 0 : items[player.weaponnumber - 1].strength) + player.p[3];
	hstr = str >> 1;
	def = (player.armournumber === 0 ? 0 : items[player.armournumber - 1].defence) + player.p[4]
	setvar('enemy', enm.name);
	your_hp();
	enemy_hp(enm);
	dk.console.gotoxy(2,21);
	lw('`r0`2'+enm.see);
	while(1) {
		ch = hbar(2, 23, ['Attack', 'Run For it']);
		dk.console.gotoxy(2,21);
		dk.console.cleareol();
		if (ch === 1) {
			if (random(10) === 0) {
				dk.console.gotoxy(2, 21);
				dk.console.cleareol();
				lw('`r0`2`e  `4blocks your path!');
			}
			else {
				if (enm.run_reffile !== '' && enm.run_refname !== '')
					run_ref(enm.run_refname, enm.run_reffile);
				break;
			}
		}
		else {
			supr = (random(10) === 0);
			dmg = hstr + random(hstr) + 1 - enm.defence;
			if (supr)
				dmg *= 2;
			if (dmg < 1) {
				if (supr)
					lw('`4Your `%SUPER STRIKE misses!');
				else
					lw('`4You completely miss!');
			}
			else {
				if (supr)
					astr = '`2You `%SUPER STRIKE`2';
				else if (wep === undefined) {
					switch(random(5)) {
						case 0:
							astr = '`2You punch `0`e`2 in the jimmy';//Him,it
							break;
						case 1:
							astr = 'You kick `0`e`2 hard as you can';// Wild Boar
							break;
						case 2:
							astr = '`2You slap `0`e`2 a good one';//Rabid dog
							break;
						case 3:
							astr = '`2You headbutt `0`e';//Rabid dog
							break;
						case 4:
							astr = '`2You trip `0`e';//Rabid dog
							break;
					}
				}
				else
					astr = wep.hitaction
				lw('`2' + astr + '`2 for `0'+dmg+'`2 damage!');
				enm.hp -= dmg;
				enemy_hp(enm);
			}
		}
		lw('`r0`2');
		dk.console.gotoxy(2, 22);
		dk.console.cleareol();
		if (enm.hp < 1) {
			lw('`r0`0You killed '+him+'!')
			dk.console.gotoxy(2, 23);
			dk.console.cleareol();
			lw('`2You find `$$'+enm.gold+'`2 and gain `%'+enm.experience+' `2experience in this battle.  `2<`0MORE`2>')
			player.money += enm.gold;
			player.p[0] += enm.experience;
			if (supr)
				update_bar(enm.killstr, true, 0);
			getkey();
			if (enm.win_reffile !== '' && enm.win_refname !== '')
				run_ref(enm.win_refname, enm.win_reffile);
			break;
		}
		else {
			atk = enm.attacks[random(enm.attacks.length)];
			ehstr = atk.strength >> 1;
			dmg = ehstr + random(ehstr) + 1 - def;
			if (dmg < 1) {
				switch(random(5)) {
					case 0:
						lw('`0`e `2misses as you jump to one side!');//Farmer Joe
						break;
					case 1:
						lw('`2You dodge `0`e\'s`2 attack easily.');//Farmer Joe
						break;
					case 2:
						lw('`2You laugh as `0`e `2misses and strikes the ground.');//Farmer Joe
						break;
					case 3:
						lw('`2Your armour absorbs `0`e\'s`2 blow!');//Armored Hedge Hog
						break;
					case 4:
						lw('`2You are forced to grin as `e\'s puny strike bounces off you.');//Armored Hedge Hog
						break;
				}
			}
			else {
				lw('`r0`0'+enm.name+' `2'+atk.hitaction+'`2 for `4'+dmg+'`2 damage!');
				player.p[1] -= dmg;
				your_hp();
				if (player.p[1] < 1) {
					if (enm.lose_reffile !== '' && enm.lose_refname !== '')
						run_ref(enm.lose_refname, enm.lose_reffile);
				}
			}
		}
	}
	dk.console.gotoxy(0, 21);
	lw('`r0`2');
	dk.console.cleareol();
	dk.console.gotoxy(0, 22);
	dk.console.cleareol();
	dk.console.gotoxy(0, 23);
	dk.console.cleareol();
	redraw_bar(true);
}

function mail_to(pl, quotes)
{
	var wrap = '';
	var l;
	var ch;
	var bt = 0;
	var msg = [];
	var f;
	var op = pfile.get(pl);

	if (op === null || op.deleted)
		throw('mail_to() illegal user');

	lln(space_pad('\r`r1                           `%COMPOSING YOUR MASTERPIECE', 78)+'`r0');
	sln('');
	if (quotes !== undefined) {
		quotes.forEach(function(l) {
			lln(l);
			msg.push(l);
		});
		sln('');
	}

	do {
		l = wrap;
		wrap = '';
		foreground(10);
		sw(' >');
		foreground(2);
		sw(l);
		do {
			ch = getkey();
			if (bt === 1) {
				bt = 0;
				if ('0123456789!@#$%'.indexOf(ch) > -1) {
					l += ch;
					lw('\r`0 >`2'+l+' \b');
					continue;
				}
				else {
					lw('\b \b');
					l = l.slice(0, -1);
					continue;
				}
			}
			else {
				if (ch === '`')
					bt = 1;
			}
			if (ch === '\x08') {
				if (l.length > 0) {
					l = l.slice(0, -1);
					if (l.substr(l.length-1) === '`') {
						bt = 1;
						lw('\r`0 >`2'+l);
						sw('` \b');
					}
					else {
						sw(ch);
						sw(' \x08');
					}
				}
			}
			else if (ch !== '\x1b') {
				sw(ch);
				if (ch !== '\r') {
					l += ch;
				}
				if (displen(clean_str(l)) > 77) {
					t = l.lastIndexOf(' ');
					if (t !== -1) {
						wrap = l.slice(t+1);
						l = l.slice(0, t);
						sw(bs.substr(0, wrap.length));
						sw(ws.substr(0, wrap.length));
					}
					ch = '\r';
				}
			}
		} while (ch !== '\r');
		if (l.length === 0 && msg.length === 0) {
			sln('');
			lln('  `2Mail aborted');
			return;
		}
		l = clean_str('  `2' + l);
		if (l.length >= 5)
			msg.push(l);
		sln('');
	} while (l.length >= 5);
	msg.unshift('@MESSAGE '+player.name);
	msg.push('@DONE');
	msg.push('@REPLY '+(player.Record + 1));
	f = new File(getfname(maildir + 'mail' + (pl + 1) + '.dat'));
	if (!f.open('ab'))
		throw('Unable to open file '+f.name);
	msg.forEach(function(l) {
		f.write(l+'\r\n');
	});
	f.close();
	sln('');
	lln('  `2Mail sent to `0'+op.name+'`2');
}

function vbar(choices, args)
{
	var ch;
	var ret = {ch:undefined, wrap:false, cur:undefined};
	var opt = {cur:0, drawall:true, extras:'', x:scr.pos.x, y:scr.pos.y, return_on_wrap:false, highlight:'`r1`%', norm:'`r0`%'};
	var oldcur;

	if (args !== undefined) {
		Object.keys(opt).forEach(function(o) {
			if (args[o] !== undefined)
				opt[o] = args[o];
		});
	}

	ret.cur = opt.cur;
	oldcur = ret.cur;
	opt.extras = opt.extras.toUpperCase();

	function draw_choice(c) {
		dk.console.gotoxy(opt.x, opt.y + c);
		if (ret.cur === c)
			lw(opt.highlight);
		else
			lw(opt.norm);
		lw(choices[c]);
		lw(opt.norm);
	}

	if (opt.drawall) {
		choices.forEach(function(c, i) {
			draw_choice(i);
		});
	}
	else {
		draw_choice(ret.cur);
	}
	dk.console.gotoxy(opt.x, opt.y + ret.cur);

	while(1) {
		if (oldcur !== ret.cur) {
			draw_choice(oldcur);
			draw_choice(ret.cur);
			oldcur = ret.cur;
			dk.console.gotoxy(opt.x, opt.y + ret.cur);
		}
		ch = getkey().toUpperCase();
		ret.ch = ch;
		if (opt.extras.indexOf(ch) > -1)
			return ret;
		switch(ch) {
			case '8':
			case 'KEY_UP':
			case '4':
			case 'KEY_LEFT':
				ret.cur--;
				if (ret.cur < 0) {
					if (opt.return_on_wrap) {
						draw_choice(oldcur);
						ret.wrap = true;
						return ret;
					}
					ret.cur = choices.length - 1;
				}
				break;
			case '2':
			case 'KEY_DOWN':
			case '6':
			case 'KEY_RIGHT':
				ret.cur++;
				if (ret.cur >= choices.length) {
					if (opt.return_on_wrap) {
						draw_choice(oldcur);
						ret.wrap = true;
						return ret;
					}
					ret.cur = 0;
				}
				break;
			case '\r':
				return ret;
		}
	}
}

function items_menu(itms, cur, buying, selling, extras, starty, endy)
{
	var cnt = endy - starty + 1;
	var i;
	var it;
	var idx;
	var choices = [];
	var choice;
	var oldcur;
	var off;
	var desc;
	var str;

	function draw_page() {
		choices = [];
		off = cnt * (Math.floor(cur / cnt));
		lw('`r0`2');
		for (i = 0; i < cnt; i++) {
			idx = i + off;
			if (idx >= itms.length) {
				str = '';
			}
			else {
				it = itms[idx] - 1;
				desc = items[it].description;
				choices.push(((selling && (items[it].sell === false)) ? '`8  ' : '`2  ')+items[it].name);
				if (cur === idx)
					str = '`r1`2';
				else
					str = '`r0`2';
				if (selling && items[it].sell === false)
					str += '`8';
				str = '  '+items[it].name;
				if (cur === idx)
					str += '`r0';
				str += decorate_item(items[it]);
				if (buying) {
					str += spaces(32 - displen(str));
					str += '`2$`$'+items[it].value+'`2';
				}
				else {
					str += spaces(37 - displen(str));
					if (items[it].armour) {
						if (player.armournumber === it + 1)
							desc = 'Currently wearing as armour.';
					}
					if (items[it].weapon) {
						if (player.weaponnumber === it + 1)
							desc = 'Currently armed as weapon.';
					}
					str += '`2 (`0';
					if (selling && items[it].sell === false)
						str += '`8';
					str += player.i[it]+'`2)';
				}
				str += spaces(48 - displen(str));
				str += '`2 ';
				if (selling && items[it].sell === false)
					str += '`8';
				str += desc;
				str += spaces(79 - displen(str));
			}
			dk.console.gotoxy(0, starty + i);
			dk.console.cleareol();
			lw(str);
		}
	}

	draw_page();
	while(1) {
		choice = vbar(choices, {cur:cur % cnt, drawall:false, extras:'QNP'+extras, x:0, y:starty, return_on_wrap:true, highlight:'`r1`2', norm:'`r0`2'});
		if (choice.wrap) {
			oldcur = cur;
			cur = off + choice.cur;
			if (cur < 0)
				cur = itms.length - 1;
			if (cur >= itms.length)
				cur = 0;
			if (Math.floor(oldcur / cnt) !== Math.floor(cur / cnt))
				draw_page();
		}
		else {
			switch(choice.ch) {
				case 'N':
					oldcur = cur;
					cur += cnt;
					if (cur >= itms.length)
						cur = itms.length - 1;
					if (Math.floor(oldcur / cnt) !== Math.floor(cur / cnt))
						draw_page();
					cur = oldcur;
					break;
				case 'P':
					oldcur = cur;
					cur -= cnt;
					if (cur < 0) {
						cur = oldcur;
						break;
					}
					draw_page();
					break;
				default:
					choice.cur += off;
					return choice;
			}
		}
	}
}

function hail()
{
	var op;
	var pl = [];
	var page;
	var cur;
	var pages;
	var i;
	var ch;
	var f;
	var fn;
	var inv;
	var choice;

	function draw_menu()
	{
		var x, y;
		page = Math.floor(cur / 9);
		if (pages > 1) {
			dk.console.gotoxy(0, 22);
			if (page === 0)
				lw('  ');
			else
				lw('`3<<`2');
			dk.console.gotoxy(78, 22);
			if (page < (pages - 1))
				lw('`3>>`2');
			else
				lw('  ');
		}
		for (i = 0; i < 9; i++) {
			dk.console.gotoxy(4 + (24 * (i % 3)), 21 + Math.floor(i / 3));
			if (i + page * 9 >= pl.length)
				break;
			if (i + page * 9 === cur)
				lw('`r1');
			lw('`2'+pl[i + page * 9].name);
			if (i + page * 9 === cur) {
				x = scr.pos.x;
				y = scr.pos.y;
				lw('`r0');
			}
		}
		dk.console.gotoxy(x,y);
	}

	function erase_menu() {
		dk.console.gotoxy(0, 21);
		dk.console.cleareol();
		dk.console.gotoxy(0, 22);
		dk.console.cleareol();
		dk.console.gotoxy(0, 23);
		dk.console.cleareol();
	}

	update();
	players.forEach(function(p, i) {
		var op;

		if (i === player.Record)
			return;
		if (p.map === player.map &&
		    p.x === player.x &&
		    p.y === player.y &&
		    p.busy === 0) {
			op = pfile.get(i);
			if (op.dead)
				return;
			if (op.deleted)
				return;
			pl.push(op);
		}
	});
	if (pl.length > 1) {
		page = 0;
		cur = 0;
		pages = Math.ceil(pl.length / 9);

		dk.console.gotoxy(2, 20);
		lw(space_pad('`r1`2                             Hail which person?', 76)+'`r0');
		do {
			draw_menu();
			ch = getkey().toUpperCase();
			switch(ch) {
				case '6':
				case 'KEY_RIGHT':
					cur++;
					if (cur >= pl.length)
						cur--;
					else if ((cur % 3) === 0) {
						cur = 9 * (page + 1) + 3 * (Math.floor((cur - 1) / 3) % 3);
						if (cur >= pl.length)
							cur = pl.length - 1;
						erase_menu();
					}
					break;
				case '4':
				case 'KEY_LEFT':
					cur--;
					if (cur < 0)
						cur = 0;
					else if ((cur % 3) === 2) {
						if (page === 0)
							cur++;
						else {
							cur = 9 * (page - 1) + 3 * (Math.floor((cur + 1) / 3) % 3) + 2;
							erase_menu();
						}
					}
					break;
				case '8':
				case 'KEY_UP':
					if ((cur % 9) > 2)
						cur -= 3;
					break;
				case '2':
				case 'KEY_DOWN':
					if ((cur % 9) < 6)
						cur += 3;
					break;
				case '\r':
					break;
			}
		} while (ch !== '\r');
		erase_menu();
		op = pfile.get(pl[cur].Record, true);
	}
	else if (pl.length === 0) {
		tfile_append('You look around, but don\'t see anyone.');
		return;
	}
	else {
		op = pfile.get(pl[0].Record, true);
	}
	if (op.battle || op.busy) {
		op.unLock();
		tfile_append(op.name+' `2is busy and doesn\'t hear you. (try later)');
		return;
	}
	player.battle = 1;
	update_update();
	player.put();
	if (op.onnow === 0) {
		op.battle = 1;
		op.put(false);
		update_bar('You find `0'+op.name+'`2 sleeping like a baby. (hit a key)', true);
		switch(hbar(2, 23, ['Leave', 'Attack', 'Give Item', 'Transfer Gold', 'Write Mail'])) {
		}
		// TODO: Offline battles, giving things, etc...
	}
	else {
		op.unLock();
		update_bar('`2Hailing `0'+op.name+'`2... (hit a key to abort)', true);

		f = new File(getfname(maildir+'tx'+(player.Record + 1)+'.tmp'))
		if (!f.open('ab'))
			throw('Unable to open '+f.name);
		f.write('*\r\n');
		f.close();

		f = new File(getfname(maildir+'con'+(op.Record + 1)+'.tmp'))
		if (!f.open('ab'))
			throw('Unable to open '+f.name);
		f.write('CONNECT|'+(player.Record + 1)+'\r\n');
		f.close();

		while (file_exists(getfname(maildir+'tx'+(player.Record + 1)+'.tmp'))) {
			if (dk.console.waitkey(game.delay)) {
				getkey();
				player.battle = 0;
				update_update();
				player.put();
				return;
			}
		}

		switch(hbar(2, 23, ['Leave', 'Attack', 'Give Item', 'Chat'])) {
			case 0:
				break;
			case 1:	// TODO: Attack...
				break;
			case 2: // TODO: Give Item
				llnw('`c`r0`2                                `r1`%  GIVING.  `r0');
				sln('');
				sln('');
				lw('`r5Item To Give                 Amount Owned');
				dk.console.cleareol();
				dk.console.gotoxy(0, 23);
				lw('  `$Q `2to quit, `$ENTER `2to give item.       You have `$'+player.money+' gold.');
				dk.console.cleareol();
				dk.console.gotoxy(0, 7);
				inv = get_inventory();
				if (inv.length === 0) {
					dk.console.gotoxy(0, 7);
					lw('`r0  `2You have nothing to give, loser.  (press `%Q `2to continue)');
					do {
						ch = getkey().toUpperCase();
					} while (ch !== 'Q');
				}
				else {
					// TODO: I'm not sure this is how it actually looks...
					choice = items_menu(inv, 0, false, false, '', 7, 22);
					draw_map();
					update();
					dk.console.gotoxy(0, 21);
					if (items[inv[choice.cur] - 1].questitem) {
						lw('  `$You don\'t think it would be wise to give that away.`2');
					}
					else {
						lw('  `$Give how many?`2');
						ch = parseInt(dk.console.getstr({len:6, crlf:false, integer:true}));
						if ((!isNaN(ch)) && ch > 0 && ch <= player.i[inv[choice.cur] - 1]) {
							player.i[inv[choice.cur] - 1] -= ch;
							f = new File(getfname(maildir+'con'+(op.Record + 1)+'.tmp'))
							if (!f.open('ab'))
								throw('Unable to open '+f.name);
							f.write('ADDITEM|'+inv[choice.cur]+'|'+ch+'\r\n');
							f.close();

							// TODO: And a mail message or something?
						}
					}
				}
				// TODO: Write amount to the thingie
				break;
			case 3:
				fn = file_getcase(maildir+'chat'+(player.Record + 1)+'.tmp');
				if (fn !== undefined)
					file_remove(fn);
				fn = file_getcase(maildir+'chat'+(op.Record + 1)+'.tmp');
				if (fn !== undefined)
					file_remove(fn);
				f = new File(getfname(maildir + 'inf'+(op.Record + 1)+'.tmp'));
				if (!f.open('ab'))
					throw('Unable to open '+f.name);
				
				f.write('CHAT\r\n');
				f.close();
				chat(op);
				draw_map();
				break;
		}
	}
	player.battle = 0;
	update_update();
	player.put();
	update();

	redraw_bar(true);
}

function do_map()
{
	var ch;
	var al;
	var oldmore;

	if (map === undefined || map.Record !== world.mapdatindex[player.map - 1] - 1)
		map = load_map(player.map);
	draw_map();
	update();

	ch = ''
	while (ch != 'Q') {
		if (enemy !== undefined) {
			offline_battle();
		}
		while (!dk.console.waitkey(game.delay)) {
			update();
		};
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
			case 'B':
				if (!lfile.open('r'))
					throw('Unable to open '+lfile.name);
				al = lfile.readAll();
				lfile.close();
				while (al.length > 19)
					al.shift();
				if (al.length === 0) {
					dk.console.gotoxy(2, 20);
					lw('`r1`%               Backbuffer is EMPTY - Press a key to continue.               `r0');
					getkey();
					redraw_bar(false);
					break;
				}
				oldmore = morechk;
				morechk = false;
				lln('`c`r1                                 BACK BUFFER                                   `r0`7');
				al.forEach(function(l) {
					sln('  '+superclean(l));
				});
				dk.console.gotoxy(3, 23);
				lw('`r1`%                     Press a key to return to the game.                     `r0');
				getkey();
				morechk = oldmore;
				draw_map();
				update();
				break;
			case 'D':
				run_ref('readlog', 'logstuff.ref');
				draw_map();
				update();
				break;
			case 'F':
				if (!lfile.open('r'))
					throw('Unable to open '+lfile.name);
				al = lfile.readAll();
				lfile.close();
				while (al.length > 5)
					al.shift();
				if (al.length === 0) {
					dk.console.gotoxy(2, 20);
					lw('`r1`%               Backbuffer is EMPTY - Press a key to continue.               `r0');
					getkey();
					redraw_bar(false);
				}
				else {
					lw('`r0`7');
					dk.console.gotoxy(0, 21);
					// First two, more, last three, prompt
					if (al.length < 4) {
						sln('  '+superclean(al[0]));
						if (al.length > 1)
							sln('  '+superclean(al[1]));
						if (al.length > 2)
							sw('  '+superclean(al[2]));
					}
					else {
						sln('  '+superclean(al[0]));
						sln('  '+superclean(al[1]));
						more();
						dk.console.gotoxy(0, 21);
						dk.console.cleareol();
						sln('  '+superclean(al[2]));
						dk.console.gotoxy(0, 22);
						dk.console.cleareol();
						if (al.length > 3)
							sln('  '+superclean(al[3]));
						dk.console.gotoxy(0, 23);
						dk.console.cleareol();
						if (al.length > 4)
							sw('  '+superclean(al[4]));
					}
					dk.console.gotoxy(2, 20);
					lw('`r1`%Fast Backbuffer - Press a key to continue                                   `r0`2');
					getkey();
					for (al = 21; al < 24; al++) {
						dk.console.gotoxy(0, al);
						dk.console.cleareol();
					}
					redraw_bar(false);
				}
				break;
			case 'H':
				hail();
				break;
			case 'L':
				run_ref('listplayers', 'help.ref');
				break;
			case 'M':
				run_ref('map', 'help.ref');
				break;
			case 'P':
				run_ref('whoison', 'help.ref');
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
			case 'R':
				draw_map();
				update();
				break;
			case 'S':
				show_player_names();
				draw_map();
				update();
				break;
			case 'T':
				run_ref('talk', 'help.ref');
				break;
			case 'V':
				view_inventory();
				run_ref('closestats', 'gametxt.ref');
				break;
			case 'W':
				sclrscr();
				ch = chooseplayer();
				if (ch !== undefined) {
					sln('');
					sln('');
					mail_to(ch);
				}
				draw_map();
				update();
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
		players.push({x:p.x, y:p.y, map:p.map, onnow:p.onnow, busy:p.busy, battle:p.battle, deleted:p.deleted});
	}
}

function load_time()
{
	var newday = false;
	var sday;
	var f = new File(getfname('stime.dat'));
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
	f = new File(getfname('time.dat'));
	if (!file_exists(f.name)) {
		state.time = 0;
		file_mutex(f.name, state.time+'\n');
		newday = true;
	}
	else {
		if (!f.open('r'))
			throw('Unable to open '+f.name);
		state.time = parseInt(f.readln(), 10);
		f.close();
	}

	if (newday || argv.indexOf('/MAINT') !== -1) {
		f = new File(getfname('stime.dat'));
		if (!f.open('r+b'))
			throw('Unable to open '+f.name);
		f.truncate(0);
		f.write(tday+'\r\n');
		f.close;
		state.time++;
		f = new File(getfname('time.dat'));
		if (!f.open('r+b'))
			throw('Unable to open '+f.name);
		f.truncate(0);
		f.write(state.time+'\r\n');
		f.close;
		run_ref('maint', 'maint.ref');
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

function pick_deleted()
{
	var i;
	var del = [];
	var pl;

	for (i = 0; i < pfile.length; i++) {
		pl = pfile.get(i);
		if (pl.deleted === 1)
			del.push(pl);
	}

	del.sort(function(a,b) { return b.lastdayon - a.lastdayon });
	for (i = 0; i < del.length; i++) {
		del[i].reLoad(true);
		if (del[i].deleted !== 1) {
			del.unLock();
			continue;
		}
		del[i].deleted = 0;
		del[i].lastdayon = -32768;
		del[i].put(false);
		player.Record = del[i].Record;
		break;
	}
}

function setup_time_warnings()
{
	var perday = getvar('`v05');

	time_warnings = [];
	if (perday === 0)
		return;
	time_warnings.push(Math.floor(perday * 0.75));
	time_warnings.push(Math.floor(perday * 0.5));
	time_warnings.push(Math.floor(perday * 0.25));
	time_warnings.push(Math.floor(perday * 0.1));
	time_warnings.push(Math.floor(perday * 0.05));
	time_warnings.push(0);
}

var pfile = new RecordFile(getfname('trader.dat'), Player_Def);
var mfile = new RecordFile(getfname('map.dat'), Map_Def);
var wfile = new RecordFile(getfname('world.dat'), World_Def);
var ifile = new RecordFile(getfname('items.dat'), Item_Def);
var ufile = new RecordFile(getfname('update.tmp'), Update_Def);
var gfile = new RecordFile(getfname('game.dat'), Game_Def);
var maildir = getfname('mail');

// TODO: Actually send this font to SyncTERM too.
if (file_exists(getfname('FONTS/LORD2.FNT'))) {
	if (dk.system.mode === 'local')
		conio.loadfont(getfname('FONTS/LORD2.FNT'));
}

if (!file_isdir(maildir)) {
	mkdir(maildir);
	if (!file_isdir(maildir))
		throw('Unable to create mail directory');
}
maildir = backslash(maildir).replace(js.exec_dir, '');
world = wfile.get(0);
load_player();
load_players();
load_items();
load_time();
load_game();

run_ref('rules', 'rules.ref');

setup_time_warnings();

if (player.Record === undefined) {
	if (pfile.length >= 200) {
		pick_deleted();
		if (player.Record === undefined) {
			run_ref('full', 'gametxt.ref');
			exit(0);
		}
	}
	run_ref('newplayer', 'gametxt.ref');
}

if (player.Record === undefined)
	exit(0);

js.on_exit('killfiles.forEach(function(f) { if (f.is_open) { f.close(); } file_remove(f.name); });');

var tfile = new File(getfname(maildir + 'talk'+(player.Record + 1)+'.tmp'));
if (!tfile.open('w+b'))
	throw('Unable to open '+tfile.name);
killfiles.push(tfile);
tfile.close();

var lfile = new File(getfname(maildir + 'log'+(player.Record + 1)+'.tmp'));
if (!lfile.open('w+b'))
	throw('Unable to open '+lfile.name);
killfiles.push(lfile);
lfile.close();

var cfile = new File(getfname(maildir + 'con'+(player.Record + 1)+'.tmp'));
if (!cfile.open('w+b'))
	throw('Unable to open '+cfile.name);
killfiles.push(cfile);
cfile.close();

run_ref('startgame', 'gametxt.ref');
js.on_exit('if (player !== undefined) { update_rec.onnow = 0; update_rec.busy = 0; update_rec.battle = 0; update_rec.map = player.map; update_rec.x = player.x; update_rec.y = player.y; update_rec.put(); ufile.file.close(); player.onnow = 0; player.busy = 0; player.battle = 0; player.lastsaved = savetime(); player.put(); pfile.file.close() }');
players[player.Record] = update_rec;
player.onnow = 1;
player.busy = 0;
player.battle = 0;
player.lastdayon = state.time;
player.lastdayplayed = state.time;
player.lastsaved = savetime();
player.put();
do_map();
