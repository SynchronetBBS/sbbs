/*
    Solomoriah's WAR!

    warcommon.js -- Shared globals and functions

    Copyright 2013 Stephen Hurd
    All rights reserved.

    3 Clause BSD License

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    Redistributions of source code must retain the above copyright
    notice, self list of conditions and the following disclaimer.

    Redistributions in binary form must reproduce the above copyright
    notice, self list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

    Neither the name of the author nor the names of any contributors
    may be used to endorse or promote products derived from self software
    without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
    FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
    AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

const TIME_TO_DESERT = 7;
const NATIONNMLEN = 14;
const major_ver  = 1;
const minor_ver  = 0;
const MAXCITIES  = 150;
const MAXMAPSZ   = 160;
const MAXARMIES  = 2000;
const ARMYNAMLEN = 14;
const TRANS_SELF = 0;
const TRANS_HERO = 1;
const TRANS_ONE  = 2;
const TRANS_ALL  = 3;
const MAILFL     = "mail.%03d";
const TMPMAILFL	 = "%04d.tmpmsg";
const NEWSFL     = "news";
const HEADERMARK = " \b";
const MAILLOG    = "mail.log";
const GAMEBAK    = "game.%03d";
const MASTERFL   = "master.cmd";
const MASTERBAK  = "master.%03d";
const GAMESAVE   = "game.save.json";
const PLAYERFL   = "%04d.cmd";
const TURNFL	 = "%04d.turn";
const marks = [
	"*ABCDEFGHIJKLMNOPQRSTUVWXYZ",
	"@abcdefghijklmnopqrstuvwxyz"
];

// defined by loadmap()
var map_width;
var map_height;
var map=[];
var mapovl=[];
function loadmap()
{
	var fp;
	var inbuf='';
	var rows, cols, i, j, m;

	/* initialize map arrays */
	fp = new File(game_dir+'/map');
	if(!fp.open('rb'))
		return 'Open Error';
	if((inbuf = fp.readln())==null) {
		fp.close();
		return 'Read Error';
	}
	if(inbuf.substr(0, 2) != 'M ') {
		fp.close();
		return 'Invalid Header';
	}
	if((m=inbuf.match(/^M ([0-9]+) ([0-9]+)$/))==null || m[1] < 1 || m[2] < 1 || m[1] > MAXMAPSZ || m[2] > MAXMAPSZ) {
		fp.close();
		return 'Invalid Header';
	}
	map_height=rows=parseInt(m[1], 10);
	map_width=cols=parseInt(m[2], 10);
	fp.readln();
	for(i = 0; i < rows; i++) {
		map[i]=[];
		mapovl[i]=[];
		for(j = 0; j < cols; j++) {
			map[i][j] = '~';
			mapovl[i][j] = ' ';
		}
	}
	for(i = 0; i < rows; i++) {
		if((inbuf = fp.readln())==null) {
			fp.close();
			return 'Unexpected EOF';
		}
		for(j = 0; j<cols && j<inbuf.length; j++)
			map[i][j] = inbuf.substr(j, 1) == ' ' ? '~' : inbuf.substr(j, 1);
	}
	fp.close();
	return 0;
}

// defined by loadsave()
var cities;
var ttypes;
var armies;
var move_table;
var world='';
var nations;
var gen=0;
var was_polled;
function loadsave() {
	var game;
	var i;
	var fp=new File(game_dir+'/game.save.json');

	if(!file_exists(fp.name))
		fp=new File(game_dir+'/game.orig.json');
	if(!fp.open('rb'))
		return 'Open Error';
	game=JSON.parse(fp.readAll().join('\n'));
	fp.close();

	/* add cities to map overlay */
	for(i = 0; i < game.cities.length; i++) {
		if(mapovl[game.cities[i].r][game.cities[i].c] == ' '
		|| marks[1].indexOf(mapovl[game.cities[i].r][game.cities[i].c]) != -1)
			mapovl[game.cities[i].r][game.cities[i].c] = game.nations[game.cities[i].nation].mark;
		else
			mapovl[game.cities[i].r][game.cities[i].c] = '!';
	}

	/* Setup the globals */
	cities=game.cities;
	ttypes=game.ttypes;
	nations=game.nations;
	armies=game.armies;
	move_table=game.move_table;
	world=game.world;
	gen = game.gen;

	/* clean up */
	was_polled = false;
	if(game.polled !== undefined)
		was_polled = game.polled;
	fp.close();
	return 0;
}

var maint_in_progress = false;
var news = new File(game_dir+'/news');
function nation_news(name, city) {
	if(maint_in_progress && news.is_open)
		news.write(name+' becomes the ruler of '+cities[city].name+'!\r\n\r\n');
}

function instance(n)
{
	var buff;
	
	if(n > 10 && n < 20) {
		buff = format("%dth", n);
		return buff;
	}
	switch(n % 10) {
	case 1 :
		buff = format("%dst", n);
		break;
	case 2 :
		buff = format("%dnd", n);
		break;
	case 3 :
		buff = format("%drd", n);
		break;
	default :
		buff = format("%dth", n);
		break;
	}
	return buff;
}

function event(text, r, c, ntn)
{
	var fp = new File(game_dir+'/'+format(TURNFL, ntn));

	if(fp.open('ab')) {
		fp.write(r+' '+c+' '+text+'\n');
		fp.close();
	}
}

function armyname(a)
{
	var c, t, i, m;
	var buff;

	if(armies[a].name.substr(0, 1) != '.')
		return armies[a].name;
	c = 0;
	t = 0;
	i = -1;
	m=armies[a].name.match(/([0-9]+)\/([0-9]+)\/([0-9]+)/);
	if(m==null)
		return '(Unnamed Army)';
	c=parseInt(m[1], 10);
	t=parseInt(m[2], 10);
	i=parseInt(m[3], 10);
	buff = cities[c].name+' '+instance(i)+' '+ttypes[t].name;
	return buff;
}

var turn_done = false;
var privtable = {
	"global-update":function(argc, argv) {
		var i, k, n, t, cnt, xch, max, an;

		/* pack army list */
		for(k = 0; k < armies.length; k++) {
			while(armies[k] !== undefined && armies[k].nation == -1)
					armies.splice(k, 1);
		}

		/* create new armies */
		for(i = 0; i < cities.length; i++) {
			cnt = 0;
			for(k = 0; k < armies.length; k++) {
				if(armies[k].nation == cities[i].nation
						&& armies[k].r == cities[i].r
						&& armies[k].c == cities[i].c)
					cnt++;
			}
			max = cities[i].defense * 2;
			if(max > 10)
				max = 10;
			if(cities[i].nation != 0) {
				max = 12;
				k = cities[i].nation;
				if(nations[k].idle_turns >= TIME_TO_DESERT)
					max = 0;
			}
			if(cnt < max) {
				n = cities[i].prod_type;
				if(n >= 0 && n < 4) {
					cities[i].turns_left--;
					if(cities[i].turns_left < 1) {
						if(armies.length < MAXARMIES - 1) {
							t = cities[i].types[n];
							armies.push({
								name: format(".%03d/%03d/%03d", i, t, ++cities[i].instance[n]),
								nation: cities[i].nation,
								r: cities[i].r,
								c: cities[i].c,
								combat: ttypes[t].combat,
								hero: 0,
								move_rate: ttypes[t].move_rate,
								move_tbl: ttypes[t].move_tbl,
								special_mv: ttypes[t].special_mv,
								eparm1: 0
							});
							an = armyname(armies.length-1);
							if(an.substr(0, cities[i].name.length) == cities[i].name)
								event(an+' created', armies[armies.length-1].r, armies[armies.length-1].c, armies[armies.length-1].nation);
							else
								event(an+' created in '+cities[i].name, armies[armies.length-1].r, armies[armies.length-1].c, armies[armies.length-1].nation);

							cities[i].turns_left = cities[i].times[n];

						}
						else {
							cities[i].turns_left = 0;
						}
					}
				}
			}
		}

		/* update all armies' movement. */
		for(i = 0; i < armies.length; i++)
			armies[i].move_left = armies[i].move_rate;
		gen++;
		return 0;
	},
	"new-nation":function(argc, argv) {
		var i, r, c;

		/* if in war update, add news entry */
		nation_news(argv[1], parseInt(argv[3],10));

		/* create nation entry */
		nations.push({
			name: argv[1],
			uid: parseInt(argv[2], 10),
			city: parseInt(argv[3], 10),
			mark: argv[4].substr(0, 1),
			idle_turns: 0
		});

		/* transfer armies */
		r = cities[nations[nations.length-1].city].r;
		c = cities[nations[nations.length-1].city].c;
		for(i = 0; i < armies.length; i++) {
			if(armies[i].r == r
			&& armies[i].c == c
			&& armies[i].nation == 0)
				armies[i].nation = nations.length-1;
		}

		/* clean up */
		return 0;
	},
	"control-city":function(argc, argv) {
		var c;

		cities[c = parseInt(argv[2], 10)].nation = parseInt(argv[1], 10);
		if(argv[3] != undefined && parseInt(argv[3], 10) > 0)
			cities[c].turns_left = cities[c].times[cities[c].prod_type] + 1;
		return 0;
	},
	"kill-army":function(argc, argv) {
		var a, i, n;

		a = parseInt(argv[1], 10);

		/* Hero starts batter for the other team */
		if(a == -2) {
			i = parseInt(argv[2], 10);
			n = parseInt(argv[3], 10);
			if(n > 0) {
				armies[i].nation = n;
				if(armies[i].hero > 1)
					armies[i].hero--;
				return 0;
			}
			a = i;
		}

		/* All armies for a nation destroyed (not used) */
		if(a == -1) {
			n = parseInt(argv[2], 10);
			if(n < 1 || n > nations.length)
				return 0;
			for(i = 0; i < armies.length; i++) {
				while(armies[i].nation == n) {
					armies[i].nation = -1;
					armies[i].r = -1;
					armies[i].c = -1;
				}
			}
			return 0;
		}

		/* Just the one army */
		armies[a].nation = -1;
		armies[a].r = -1;
		armies[a].c = -1;
		return 0;
	},
	"move-army":function(argc, argv) {
		var a, b;

		a = parseInt(argv[1], 10);
		if(armies[a].nation == -1)
			return 0;
		b = armies[a].move_left;
		b -= parseInt(argv[2], 10);
		if(b < 0)
			b = 0;
		armies[a].move_left = b;
		armies[a].r = parseInt(argv[3], 10);
		armies[a].c = parseInt(argv[4], 10);
		return 0;
	},
	"make-army":function(argc, argv) {
		/* no error, just don't do it. */
		if(armies.length >= MAXARMIES)
			return 0;
		armies.push({
			name:argv[1].substr(0, ARMYNAMLEN),
			nation:parseInt(argv[2], 10),
			r:parseInt(argv[3], 10),
			c:parseInt(argv[4], 10),
			combat:parseInt(argv[5], 10),
			hero:parseInt(argv[6], 10),
			move_rate:parseInt(argv[7], 10),
			move_left:parseInt(argv[7], 10),
			move_tbl:parseInt(argv[8], 10),
			special_mv:parseInt(argv[9], 10),
			eparm1:parseInt(argv[10], 10)
		});
		return 0;
	},
	"name-army":function(argc, argv) {
		var a;

		a = parseInt(argv[1], 10);
		if(armies[a].nation == -1)
			return 0;
		armies[a].name = argv[2].substr(0, ARMYNAMLEN);
		return 0;
	},
	"change-army":function(argc, argv) {
		var a, c, h;

		a = parseInt(argv[1], 10);
		c = parseInt(argv[2], 10);
		h = parseInt(argv[3], 10);
		if(armies[a].nation == -1)
			return 0;
		if(c >= 0) armies[a].combat = c;
		if(h >= 0) armies[a].hero = h;
		return 0;
	},
	"set-eparm":function(argc, argv) {
		var a;

		a = parseInt(argv[1], 10);
		if(armies[a].nation == -1)
			return 0;
		armies[a].eparm1 = parseInt(argv[2], 10);
		return 0;
	},
	"set-produce":function(argc, argv) {
		var c;

		c = parseInt(argv[1], 10);
		cities[c].prod_type = parseInt(argv[2], 10);
		cities[c].turns_left = cities[c].times[cities[c].prod_type];
		return 0;
	},
	'end-turn':function(argc, argv) {
		turn_done = true;
		return 0;
	}
};

/*
 * Splits a line into an argv array
 */
function parseline(line) {
	var i;
	var s;
	var args=[];

	i = 0;
	while(line.length > 0 && i++ < 20) {
		line=line.replace(/^\s*((?:'.*?')|(?:[^'\s][^\s]*))\s*/, function(m, str) {
			str=str.replace(/^'(.*)'$/,"$1");
			args.push(str);
			return '';
		});
	}
	return args;
}

function execpriv(line)
{
	var args=[];
	var n;

	args = parseline(line);
	if(args.length == 0)
		return 'No Text on Line';
	if(privtable[args[0]] == undefined)
		return 'No Such Command';
	return privtable[args[0]](args.length, args);
}

function my_city_at(pos, n)
{
	var i;

	for(i = 0; i < cities.length; i++) {
		if((n < 0 || cities[i].nation == n)
				&& cities[i].r == pos.r && cities[i].c == pos.c)
			return i;
	}
	return -1;
}

function city_at(pos)
{
	return my_city_at(pos, -1);
}

function nationcity(n)
{
	if(n == 0)
		return "Militia";
	return cities[nations[n].city].name;
}

function isgreater(a1, a2)
{
	var c1, c2, t1, t2, i1, i2;
	var buf1='';
	var buf2='';
	var m;

	if(armies[a1].hero > armies[a2].hero)
		return true;
	if(armies[a1].hero < armies[a2].hero)
		return false;

	if(armies[a1].special_mv > armies[a2].special_mv)
		return true;
	if(armies[a1].special_mv < armies[a2].special_mv)
		return false;

	if(armies[a1].combat > armies[a2].combat)
		return true;
	if(armies[a1].combat < armies[a2].combat)
		return false;

	if(armies[a1].move_rate > armies[a2].move_rate)
		return true;
	if(armies[a1].move_rate < armies[a2].move_rate)
		return false;

	if(armies[a1].name.substr(0,1) != '.' && armies[a2].name.substr(0,1) == '.')
		return true;
	if(armies[a1].name.substr(0,1) == '.' && armies[a2].name.substr(0,1) != '.')
		return false;
	if(armies[a1].name.substr(0,1) != '.' && armies[a2].name.substr(0,1) != '.') {
		if(armies[a1].name > armies[a2].name)
			return true;
		else
			return false;
	}

	if((m=armies[a1].name.match(/^\.([0-9]+)\/([0-9]+)\/([0-9]+)$/))!=null)
		buf1=format("%03d%03d%03d", parseInt(m[1], 10),parseInt(m[2], 10),parseInt(m[3], 10));
	if((m=armies[a2].name.match(/^\.([0-9]+)\/([0-9]+)\/([0-9]+)$/))!=null)
		buf2=format("%03d%03d%03d", parseInt(m[1], 10),parseInt(m[2], 10),parseInt(m[3], 10));
	if(buf1 > buf2)
		return true;

	return false;
}

function set_game(path)
{
	if(path.substr(0, 1)=='/' || (path.substr(1,1)==':' && path.substr(2,1)=='\\'))
		game_dir = path;
	else
		game_dir = fullpath(orig_exec_dir+'/'+path);
	news = new File(game_dir+'/news');
}

function hero_name(hero)
{
	var hname = armies[hero].name;

	if(hname == '')
		hname = '(Nameless Hero)';
	return hname;
}

function Position(r, c)
{
	if(r==undefined)
		this.__r = -1;
	else
		this.__r = (r + map_height) % map_height;
	if(c==undefined)
		this.__c = -1;
	else
		this.__c = (c + map_width) % map_width;
}
Object.defineProperty(Position.prototype, 'r', {
	get: function() {
		return this.__r;
	},
	set: function(r) {
		this.__r = (r + map_height) % map_height;
	}
});
Object.defineProperty(Position.prototype, 'c', {
	get: function() {
		return this.__c;
	},
	set: function(c) {
		this.__c = (c + map_width) % map_width;
	}
});
