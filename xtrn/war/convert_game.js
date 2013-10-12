var game={
	nations:[],
	cities:[],
	ttypes:[],
	armies:[],
	move_table:[]
};

var fp;
var inbuf='';
var p='';
var cnt, ttcnt, acnt, mcnt, nt, i, j;
var nbytes;
var a_nation, a_r, a_c, a_combat, a_hero,
	a_move_rate, a_move_tbl, a_special_mv, a_eparm1;

/* initialize armies table */
game.armies=[];

/* open the game save */

fp = new File(js.exec_dir+'/'+'game.orig');
if(!fp.open("rb")) {
	alert("Unable to open "+fp.name);
	exit(1);
}

if((inbuf = fp.readln())==null) {
	fp.close();
	alert("Unable to read first line of "+fp.name);
	exit(1);
}

if(inbuf.substr(0,2) != 'G ') {
	fp.close();
	alert("Invalid Header in "+fp.name);
	exit(1);
}

cnt = ttcnt = acnt = mcnt = -1;

gen = 0;

game.world = '';

m=inbuf.match(/^G ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) g([0-9]+) ([^\s]{0,40})\s*$/);
if(m==null) {
	fp.close();
	alert("Invalid header in "+fp.name);
	exit(1);
}
ncnt=parseInt(m[1],10);
cnt=parseInt(m[2],10);
ttcnt=parseInt(m[3],10);
acnt=parseInt(m[4],10);
mcnt=parseInt(m[5],10);
game.gen=parseInt(m[6],10);
game.world=m[7];

game.world=game.world.replace(/_/g, ' ');

/* load nations table */

if(ncnt == 0) {
	game.nations[0] = {
		name:'God',
		uid:0,
		city:0,
		mark:0,
		idle_turns:0
	};
} else {
	if(fp.readln()==null) {
		fp.close();
		alert("Unable to read nation header");
		exit(1);
	}

	for(i=0; i<game.ncnt; i++) {
		if((inbuf = fp.readln())==null) {
			fp.close();
			alert("Unable to read nation "+(game.nations.length+1)+" from "+fp.name);
			exit(1);
		}
		if((m=inbuf.match(/^(.*?): U([0-9]+) C([0-9]+) M(.) I([0-9]+)\s*$/))==null) {
			alert("Unable to parse nation "+(game.nations.length+1)+" from "+fp.name);
			exit(1);
		}
		game.nations[i]={
			name:m[1],
			uid:parseInt(m[2], 10),
			city:parseInt(m[3], 10),
			mark:"*ABCDEFGHIJKLMNOPQRSTUVWXYZ?".substr(parseInt(m[4], 10), 1),
			idle_turns:parseInt(m[5], 10)
		};
	}
}

/* load cities table */

for(i = 0; i < cnt; i++) {
	if(fp.readln()==null) {
		fp.close();
		alert("Unable to read city header");
		exit(1);
	}

	/* city id line */
	if((inbuf = fp.readln())==null) {
		fp.close();
		alert("Unable to read city "+(game.cities.length+1)+" from "+fp.name);
		exit(1);
	}

	if((m=inbuf.match(/^(.*?): C([0-9]+) N([0-9]+) @([0-9]+):([0-9]+) P([0-9]+):([0-9]+) D([0-9]+) T([0-9]+)\s*$/))==null) {
		fp.close();
		alert("Unable to parse city "+(game.cities.length+1)+" from "+fp.name);
		exit(1);
	}

	game.cities[i] = {
		name:m[1],
		cluster:parseInt(m[2], 10),
		nation:parseInt(m[3], 10),
		r:parseInt(m[4], 10),
		c:parseInt(m[5], 10),
		prod_type:parseInt(m[6], 10),
		turns_left:parseInt(m[7], 10),
		defense:parseInt(m[8], 10),
		ntypes:parseInt(m[9], 10),
		types:[],
		times:[],
		instance:[]
	};

	/* troop types */

	nt = 0;

	for(j = 0; j < 4; j++) {
		if((inbuf = fp.readln())==null) {
			fp.close();
			alert("Unable to read city "+(game.cities.length+1)+" troop "+(j)+" from "+fp.name);
			exit(1);
		}
		if((m=inbuf.match(/^([-0-9]+) ([-0-9]+) ([-0-9]+)\s*$/))==null) {
			fp.close();
			alert("Unable to parse city "+(game.cities.length+1)+" troop "+(j)+" from "+fp.name);
			exit(1);
		}
		game.cities[i].types[j]=parseInt(m[1], 10);
		game.cities[i].times[j]=parseInt(m[2], 10);
		game.cities[i].instance[j]=parseInt(m[3], 10);

		if(game.cities[i].types[j] != -1)
			nt = j + 1;
	}

	if(game.cities[i].ntypes == -1)
		game.cities[i].ntypes = nt;
}

/* load troop types table */

if(fp.readln()==null) {
	fp.close();
	alert("Unable to read troop type header");
	exit(1);
}

for(i = 0; i < ttcnt; i++) {
	if((inbuf = fp.readln())==null) {
		fp.close();
		alert("Unable to read troop type "+(game.ttypes.length+1)+" from "+fp.name);
		exit(1);
	}
	if((m=inbuf.match(/^(.*?): C([0-9]+) M([0-9]+):([0-9]+):([0-9]+)\s*$/))==null) {
		fp.close();
		alert("Unable to parse troop type "+(game.ttypes.length+1)+" from "+fp.name);
		exit(1);
	}
	game.ttypes[i]={
		name:m[1],
		combat:parseInt(m[2], 10),
		move_rate:parseInt(m[3], 10),
		move_tbl:parseInt(m[4], 10),
		special_mv:parseInt(m[5], 10),
	};
}

/* load armies table */

if(acnt > 0) {
	if(fp.readln()==null) {
		fp.close();
		alert("Unable to read armies header");
		exit(1);
	}

	for(i = 0; i < acnt; i++) {

		if((inbuf = fp.readln())==null) {
			fp.close();
			alert("Unable to read army type "+(game.armies.length+1)+" from "+fp.name);
			exit(1);
		}
		if((m=inbuf.match(/^(.*?): N([0-9]+) @([0-9]+):([0-9]+) C([0-9]+):([0-9]+) M([0-9]+):([0-9]+) S([0-9]+) L([0-9]+)\s*$/))==null) {
			fp.close();
			alert("Unable to parse army type "+(game.armies.length+1)+" from "+fp.name);
			exit(1);
		}
		game.armies[i] = {
			name:m[1],
			nation:parseInt(m[2], 10),
			r:parseInt(m[3], 10),
			c:parseInt(m[4], 10),
			combat:parseInt(m[5], 10),
			hero:parseInt(m[6], 10),
			move_rate:parseInt(m[7], 10),
			move_tbl:parseInt(m[8], 10),
			special_mv:parseInt(m[9], 10),
			eparm1:parseInt(m[10], 10),
			move_left:parseInt(m[7], 10),
		};
	}
}

/* load move table */

if(fp.readln()==null) {
	fp.close();
	alert("Unable to read move table header");
	exit(1);
}


for(i = 0; i < mcnt; i++) {

	if((inbuf = fp.readln())==null) {
		fp.close();
		alert("Unable to read move table "+(game.move_table.length+1)+" from "+fp.name);
		exit(1);
	}
	if((m=inbuf.match(/^([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+) ([0-9]+)\s*$/))==null) {
		fp.close();
		alert("Unable to parse move table "+(game.move_table.length+1)+" from "+fp.name);
		exit(1);
	}

	game.move_table[i] = {};
	game.move_table[i].cost=[];
	for(j = 0; j < 5; j++)
		game.move_table[i].cost[j] = parseInt(m[j+1], 10);
}

fp.close();
fp=new File(js.exec_dir+'/'+'game.orig.json');
if(fp.open('wb')) {
	fp.write(JSON.stringify(game, null, '\t'));
}
fp.close();
