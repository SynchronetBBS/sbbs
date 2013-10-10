var orig_exec_dir = js.exec_dir;
var game_dir = orig_exec_dir;
/*
    Solomoriah's WAR!

    warupd.c -- main procedure file for war update

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


// Based on:
/*
    Solomoriah's WAR!

    warupd.c -- main procedure file for war update

    Copyright 1993, 1994, 2001, 2013 Chris Gonnerman
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
load(game_dir+'/warcommon.js');

var mfile;

var nlist=[];
var nlcnt;
function shuffle()
{
    var i, n1, n2, tmp;

    nlcnt = 0;

    for(i = 1; i < nationcnt; i++)
        nlist[nlcnt++] = i;

    for(i = 0; i < nlcnt * nlcnt; i++) {
        n1 = random(nlcnt);
        n2 = random(nlcnt);
        if(n1 != n2) {
            tmp = nlist[n1];
            nlist[n1] = nlist[n2];
            nlist[n2] = tmp;
        }
    }
}

var usertable = {
    "move-army":function(argc, argv) {
		var a, b;

		a = parseInt(argv[1], 10);

		if(armies[a].nation == -1)
			return 0;

		if(argc > 5) {
			b = parseInt(argv[5], 10);

			if(a != b && armies[b].nation == -1)
				return 0;
		}

		b = armies[a].move_left;
		b -= parseInt(argv[2], 10);
		if(b < 0)
			b = 0;
		armies[a].move_left = b;
		armies[a].r = parseInt(argv[3], 10);
		armies[a].c = parseInt(argv[4], 10);

		if(argc > 5)
			mfile.printf("move-army %s %s %s %s %s\n",
				argv[1], argv[2], argv[3], argv[4], argv[5]);
		else
			mfile.printf("move-army %s %s %s %s\n",
				argv[1], argv[2], argv[3], argv[4]);

		return 0;
	},
    "make-army":function(argc, argv) {
		/* no error, just don't do it. */

		if(armycnt >= MAXARMIES)
			return 0;

		armies[armycnt].name = argv[1].substr(0, ARMYNAMLEN);
		armies[armycnt].nation = parseInt(argv[2], 10);
		armies[armycnt].r = parseInt(argv[3], 10);
		armies[armycnt].c = parseInt(argv[4], 10);
		armies[armycnt].combat = parseInt(argv[5], 10);
		armies[armycnt].hero = parseInt(argv[6], 10);
		armies[armycnt].move_rate = parseInt(argv[7], 10);
		armies[armycnt].move_left = parseInt(argv[7], 10);
		armies[armycnt].move_tbl = parseInt(argv[8], 10);
		armies[armycnt].special_mv = parseInt(argv[9], 10);
		armies[armycnt].eparm1 = 0;

		armycnt++;

		mfile.printf("make-army '%s' %s %s %s %s %s %s %s %s\n",
			argv[1], argv[2], argv[3],
			argv[4], argv[5], argv[6],
			argv[7], argv[8], argv[9]);

		return 0;
	},
    "name-army":function(argc, argv) {
		var a;

		a = parseInt(argv[1], 10);

		if(armies[a].nation == -1)
			return 0;

		armies[a].name = argv[2].substr(0, ARMYNAMLEN);

		mfile.printf("name-army %s '%s'\n", argv[1], argv[2]);

		return 0;
	},
    "set-produce":function(argc, argv) {
		var c;

		c = parseInt(argv[1], 10);

		cities[c].prod_type = parseInt(argv[2], 10);
		cities[c].turns_left = cities[c].times[cities[c].prod_type];

		mfile.printf("set-produce %s %s\n", argv[1], argv[2]);

		return 0;
	}
};

function execuser(line)
{
    var args;
    var n;

    args = parseline(line);

    if(args.length == 0)
        return 5;

	if(usertable[args[0]] != undefined)
		return usertable[args[0]](args.length, args);
	return 6;
}

var mail_gen = 0;
function mail_line(text, ntn)
{
	var fp;
	
    if(ntn <= 0 || ntn == 27) /* ignore god and rogue */
        return;

	fname = game_dir+'/'+format(MAILFL, ntn);
	fp = new File(fname);
	if(fp.open('ab')) {
		fp.write(text);
		fp.close();
	}
	return;
}

function message_out(text, n1, n2, do_news)
{
    if(n1)
        mail_line(text, n1);

    if(n2)
        mail_line(text, n2);

    if(do_news)
        news.write(text);

    print(text);
}

function capture(n, c)
{
	var buff;
	var n2;

	n2 = cities[c].nation;

	buff = nationcity(n)+" captures "+cities[c].name;
	message_out(buff, n, n2, 1);

	if(cities[c].nation > 0) {
		buff = " from "+nationcity(n2)+"\n";
		message_out(buff, n, n2, 1);
	} else
		message_out("\n", n, n2, 1);

	buff = "control-city "+n+" "+c+" 1\n";
	mfile.write(buff);
	execpriv(buff);
}

/*
	make_stack forms the stack of armies which includes army a.
*/

function make_stack(stack, a)
{
	var i, n, r, c, h;

	n = armies[a].nation;
	r = armies[a].r;
	c = armies[a].c;

	stack.count = 0;
	stack.ctot = 0;
	stack.hero = -1;
	h = 0;

	for(i = 0; stack.count < 12 && i < armycnt; i++)
		if(armies[i].nation == n
		&& armies[i].r == r
		&& armies[i].c == c) {
			stack.index[stack.count] = i;
			if(armies[i].hero > h) {
				stack.hero = stack.count;
				h = armies[i].hero;
			}
			stack.ctot += armies[i].combat;
			stack.count++;
		}

	stack.ctot += stack.count * h;

	stack.city = city_at(r, c);

	if(cities[stack.city].nation != n)
		stack.city = -1;

	if(stack.city != -1)
		stack.ctot += stack.count * cities[stack.city].defense;
}

function least(stack)
{
	var i, n;

	n = 0;

	for(i = 0; i < stack.count; i++)
		if(isgreater(stack.index[n], stack.index[i]))
			n = i;

	return n;
}

function stack_remove(stack, idx)
{
	var i, h;

	if(stack.count < 1)
		return;

	stack.count--;

	if(idx < stack.count)
		stack.index[idx] = stack.index[stack.count];
	
	stack.ctot = 0;
	stack.hero = -1;
	h = 0;

	for(i = 0; i < stack.count; i++) { 
		stack.ctot += armies[stack.index[i]].combat;
		if(armies[stack.index[i]].hero > h) {
			stack.hero = i;
			h = armies[stack.index[i]].hero;
		}
	}
	
	stack.ctot += stack.count * h;

	if(stack.city != -1)
		stack.ctot += stack.count * cities[stack.city].defense;
}

function advance(hero)
{
    var res;
	var buff;

    print("advance "+hero+"\n");

    res = random(9) + 1;

    if(res <= (armies[hero].hero + armies[hero].combat))
        return;

    if(armies[hero].combat < 7 
    && random(8) >= armies[hero].combat) {
        print("advance "+hero+" combat\n");
		buff = "change-army "+hero+" "+armies[hero].combat + 1+" "+armies[hero].hero+"\n";
		mfile.write(buff);
		execpriv(buff);
        return;
    }

    if(armies[hero].hero < 3
    && random(4) >= armies[hero].hero) {
        print("advance "+hero+" hero\n");
		buff = "change-army "+hero+" "+armies[hero].combat+" "+armies[hero].hero + 1+"\n";
		mfile.write(buff);
		execpriv(buff);
    }
}

function battle(a, b)
{
	var stacks = [
		{ctot:0, hero:0, count:0, city:0, index:[0,0,0,0,0,0,0,0,0,0,0,0]},
		{ctot:0, hero:0, count:0, city:0, index:[0,0,0,0,0,0,0,0,0,0,0,0]}
	];
	var ntns=[0,0];
	var r, c, city, hi, total, rlose, rwin, l, i, d, dn;
	var names=['',''], buff;

	ntns[0] = armies[a].nation;
	ntns[1] = armies[b].nation;

	names[0] = nationcity(ntns[0]);
	names[1] = nationcity(ntns[1]);

	r = armies[a].r;
	c = armies[a].c;
	city = city_at(r, c);

	buff = "Battle between "+names[0]+" and "+names[1];
	message_out(buff, ntns[0], ntns[1], 1);

	if(city != -1) {
		buff = " in "+cities[city].name;
		message_out(buff, ntns[0], ntns[1], 1);
	}

	buff = " (Turn "+gen+")";
	message_out(buff, ntns[0], ntns[1], 0);

	message_out("\n", ntns[0], ntns[1], 1);

	make_stack(stacks[0], a);
	make_stack(stacks[1], b);

	buff = "(Odds:  "+stacks[0].ctot+" to "+stacks[1].ctot+")\n";
	message_out(buff, ntns[0], ntns[1], 0);

	for(i = 0; i < 2; i++) {

		buff = names[i]+" has "+stacks[i].count+" armies";
		message_out(buff, ntns[0], ntns[1], 1);

		if((hi = stacks[i].hero) != -1) {
			buff = " led by " +
				(armies[stacks[i].index[hi]].name[0] == '\0'
					? "a nameless hero"
					: armies[stacks[i].index[hi]].name);
			buff += '\n';
			message_out(buff, ntns[0], ntns[1], 1);
		} else
			message_out("\n", ntns[0], ntns[1], 1);
	}

	while(stacks[0].count > 0 && stacks[1].count > 0) {

		total = stacks[0].ctot + stacks[1].ctot;

		/* roll dem bones */

		if(random(total) < stacks[0].ctot)
			rlose = 1;
		else
			rlose = 0;

		rwin = (rlose + 1) % 2;

		/* check for hero desertion */

		d = -1;

		if(stacks[rlose].ctot < parseInt(stacks[rwin].ctot / 2)
		&& (hi = stacks[rlose].hero) != -1
		&& armies[stacks[rlose].index[hi]].eparm1 != 1) {
			if(random(10) < 5) {
				dn = -1;
				if(stacks[rwin].count < 12 && random(10) < 5)
					dn = ntns[rwin];
				d = hi;
			} else {
				buff = "set-eparm "+stacks[d].index[hi]+" 1\n";
				mfile.write(buff);
				execpriv(buff);
			}
		}

		/* find the victim */

		if(d == -1) {
			l = least(stacks[rlose]);

			buff = armyname(stacks[rlose].index[l])+" ("+names[rlose]+") is destroyed.\n";
			message_out(buff, ntns[0], ntns[1], 0);

			if(armies[stacks[rlose].index[l]].hero > 0)
				news.write(buff);

			/* kill an army */

			buff = "kill-army "+stacks[rlose].index[l]+"\n";
			mfile.write(buff);
			execpriv(buff);

		} else {
			l = d;

			buff = armyname(stacks[rlose].index[l])+" ("+names[rlose]+") deserts";
			message_out(buff, ntns[0], ntns[1], 1);

			if(dn != -1) {
				buff = " and joins "+nationcity(dn)+"\n";
				message_out(buff, ntns[0], ntns[1], 1);
			} else
				message_out("\n", ntns[0], ntns[1], 1);

			/* hero deserts */

			buff = "kill-army -2 "+stacks[rlose].index[d]+" "+dn+"\n";
			mfile.write(buff);
			execpriv(buff);

			/* rebuild other stack around the deserter */

			if(dn != -1)
			    make_stack(stacks[rwin], d);
		}

		/* remove army from stack */

		stack_remove(stacks[rlose], l);
	}

	if(stacks[0].count < 1)
		rwin = 1;
	else
		rwin = 0;

	buff = names[rwin]+" wins!\n";
	message_out(buff, ntns[0], ntns[1], 1);

	if(city != -1
	&& cities[city].nation != ntns[rwin])
		capture(ntns[rwin], city);

	if((hi = stacks[rwin].hero) != -1)
        advance(stacks[rwin].index[hi]);
}

function combat(n)
{
	var i, j, c;

	for(i = 0; i < armycnt; i++)
		if(armies[i].nation == n)
			for(j = 0; j < armycnt; j++)
				if(armies[j].nation != n
				&& armies[j].r == armies[i].r
				&& armies[j].c == armies[i].c) {

					mail_line(HEADERMARK, armies[i].nation);
					mail_line(HEADERMARK, armies[j].nation);

					battle(i, j);
					print('\n');
					news.write('\n');
					break;
				}

	/* all battles are now over. */
	/* rescan the armies table for armies attacking undefended cities */
	/* must scan separately since armies may have died in battle */

	for(i = 0; i < armycnt; i++)
		if(armies[i].nation == n)
			if((c = city_at(armies[i].r, armies[i].c)) != -1
			&& cities[c].nation != n) {

				mail_line(HEADERMARK, armies[i].nation);
				mail_line(HEADERMARK, cities[c].nation);

				capture(armies[i].nation, c);
				print('\n');
				news.write('\n');
			}
}

/*
	deserter() causes a randomly-selected non-hero army to desert the 
	given nation.
*/

function deserter(n)
{
	var narmies, i, die;
	var buff;

	narmies = 0;

	for(i = 0; i < armycnt; i++)
		if(armies[i].nation == n
		&& armies[i].hero < 1)
			narmies++;

	if(narmies < 1)
		return;

	die = random(narmies);

	for(i = 0; i < armycnt; i++)
		if(armies[i].nation == n
		&& armies[i].hero < 1)
			if(die == 0)
				break;
			else
				die--;


	/* found one, kill it. */

	mail_line(" \b", n);
	buff = armyname(i)+" deserts "+nations[n].name+" of "+nationcity(n)+".";
	message_out(buff, n, 0, 1);

	buff = " (Turn "+gen+")";
	message_out(buff, n, 0, 0);

	message_out("\n\n", n, 0, 1);

	/* so kill it. */

	buff = "kill-army "+i+"\n";
	mfile.write(buff);
	execpriv(buff);
}

/*
	creator() constructs a hero for the given nation, *if* the nation
	needs one and if a roll indicates one is to be created.

	a nation "needs" a hero when it has more cities than heros.

	a roll is then made using (number of cities) - (number of heros)
	as a percent chance EXCEPT if the nation has NO heros; then the
    odds are 10 times normal.  Maximum odds are 90% in either case.
*/

function creator(n)
{
	var nheros, ncities, odds, counter, i, c, r, t, mtbl;
	var buff;

	ncities = 0;

	for(i = 0; i < citycnt; i++)
		if(cities[i].nation == n)
			ncities++;

	nheros = 0;

	for(i = 0; i < armycnt; i++)
		if(armies[i].nation == n
		&& armies[i].hero > 0)
			nheros++;

	odds = ncities - nheros;

	if(nheros == 0)
		odds *= 10;

    if(odds > 90)
        odds = 90;

	if(nheros >= ncities
	|| random(100) >= odds)
		return;

	r = random(ncities);

	for(i = 0; i < citycnt; i++)
		if(cities[i].nation == n)
			if(r-- < 1)
				break;

	c = i;

	/* too many armies there? */

	counter = 0;

	for(i = 0; i < armycnt; i++)
		if(armies[i].nation == n
		&& armies[i].r == cities[c].r
		&& armies[i].c == cities[c].c)
			counter++;

	if(counter >= 12)
		return;

	/* it's OK, do it. */

	mail_line(" \b", n);
	buff = "A hero arises in "+cities[c].name+" and joins "+nations[n].name+" of "+nationcity(n)+"!";
	message_out(buff, n, 0, 1);

	buff = " (Turn "+gen+")";
	message_out(buff, n, 0, 0);

	message_out("\n\n", n, 0, 1);

	t = cities[c].types[0];
	mtbl = ttypes[t].move_tbl;

	buff = format("make-army '' %d %d %d %d %d %d %d %d %d\n",
		   n, cities[c].r, cities[c].c,
		   random(4)+2, random(2)+1, 8, mtbl, 0, 0);

	mfile.write(buff);
	execpriv(buff);
}

function savegame(fp)
{
	var game={};

	game.cities=cities;
	game.ttypes=ttypes;
	game.nations=nations;
	game.armies=armies;
	game.move_table=move_table;
	game.world=world;
    game.cities.length=citycnt;
    game.ttypes.length=ttypecnt;
    game.ncnt=nationcnt;
    game.armies.length=armycnt;
    game.move_table.length=mvtcnt;
    game.gen=gen;
    fp.write(JSON.stringify(game, null, '\t'));
}

function main(argc, argv)
{
    var rc, i, j, k, n, u;
    var fp;
    var filename, inbuf, p;
    var arg;

    print(format("\nJSWARUPD Version %d.%d  \"Code by Solomoriah\"\n", 
        major_ver, minor_ver));
    print("Original Copyright 1993, 1994, 2001, 2013, Chris Gonnerman\n");
    print("JavaScript Version Copyright 2013, Stephen Hurd\n");
    print("All Rights Reserved.\n\n");

	for(arg in argv) {
		set_game(argv[arg]);

		/* load map file */

		rc = loadmap();

		if(rc != 0) {
			print("Error Loading Map ("+rc+")\n");
			exit(1);
		}

		/* load game save */

		rc = loadsave();

		if(rc != 0) {
			print("Error Loading Game Save ("+rc+")\n");
			exit(1);
		}

		if(world.length != 0)
			print("World:  "+world+"\n");

		/* open news output */

		if(!news.open('ab')) {
			print("Error Writing News File <"+news.name+">\n");
			exit(10);
		}
		maint_in_progress = true;

		print("Turn Update "+gen+"\n\n");

		if(world.length != 0) {
			news.write(" \b");
			news.write(world);
		} else
			news.write(" \bSolomoriah's WAR!");

		news.write(" News Report     Turn "+gen+"\n\n");

		/* execute master file */

		print("Reading Master Commands...\r\n");

		fp = new File(game_dir+'/'+MASTERFL);

		if(fp.open('rb')) {
			for(i = 0; (inbuf = fp.readln()) != null; i++) {
				rc = execpriv(inbuf);
				if(rc != 0) {
					print("Master Cmd Failed, Line "+(i+1)+", Code "+rc+"\n");
					exit(2);
				}
			}

			fp.close();
		}

		/* open master for writing. */

		mfile = new File(game_dir+'/'+MASTERFL);

		if(!mfile.open('ab')) {
			print("Can't Append to Master File");
			exit(5);
		}

		/* execute player commands */

		print("Reading Player Commands...\r\n");

		shuffle();

		for(k = 0; k < nlcnt; k++) {

			n = nlist[k];
			u = nations[n].uid;

			filename = format(PLAYERFL, u);

			fp = new File(game_dir+'/'+filename);

			if(fp.open("rb")) {

				print("\n"+nations[n].name+" of "+nationcity(n)+" moves...\n");

				nations[n].idle_turns = 0;

				for(i = 0; (inbuf = fp.readln()) != null; i++) {
					rc = execuser(inbuf);
					if(rc != 0) {
						printf("Player Cmd Failed, Line "+(i+1)+", Code "+rc+"  ");
						exit(2);
					}
				}

				fp.close();

				file_remove(fp.name);
			} else {
				print("\n"+nations[n].name+" of "+nationcity(n)+" is idle.\n");
				nations[n].idle_turns++;
			}

			combat(n);

			if(nations[n].idle_turns >= TIME_TO_DESERT)
				deserter(n);
			else
				creator(n);
		}

		/* do the update */

		print("Global Update...\n");

		execpriv("global-update");
		mfile.write("global-update\n");

		/* close up */

		news.close();
		mfile.close();

		mail_line(null, -1);

		/* consolidate */

		filename = format(MASTERBAK, gen);

		if(!file_rename(game_dir+'/'+MASTERFL, game_dir+'/'+filename)) {
			print("Can't Rename Master Cmd File\n");
			exit(9);
		}

		filename = format(GAMEBAK, gen);

		if(file_exists(game_dir+'/'+GAMESAVE) && !file_rename(game_dir+'/'+GAMESAVE, game_dir+'/'+filename)) {
			print("Can't Rename Game Save\n");
			exit(9);
		}

		fp = new File(game_dir+'/'+GAMESAVE);

		if(!fp.open('wb')) {
			print("Can't Create New Save File\n");
			exit(9);
		}

		/* clean up outdated files. */

		if(gen - 4 > 0) {
			filename = game_dir+'/'+format(MASTERBAK, gen - 4);
			file_remove(filename);

			filename = game_dir+'/'+format(GAMEBAK, gen - 4);
			file_remove(filename);
		}

		savegame(fp);

		fp.close();
	}

    exit(0);
}

main(argc, argv);
