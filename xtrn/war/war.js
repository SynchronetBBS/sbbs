var orig_exec_dir = js.exec_dir;
var game_dir = orig_exec_dir;
/*
    Solomoriah's WAR!

    war.js -- main procedure file for war user interface

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

    war.c -- main procedure file for war user interface

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
load("sbbsdefs.js");
load(game_dir+'/warcommon.js');

// From display.c
const gran = 2;
const terminate = "\033q ";
var avcnt = 0;
var avpnt = 0;
var enemies = [
	{nation:0, armies:0, heros:0},
	{nation:0, armies:0, heros:0}
];
var armyview = [
	{id:0, mark:' '},
	{id:0, mark:' '},
	{id:0, mark:' '},
	{id:0, mark:' '},
	{id:0, mark:' '},
	{id:0, mark:' '},
	{id:0, mark:' '},
	{id:0, mark:' '},
	{id:0, mark:' '},
	{id:0, mark:' '},
	{id:0, mark:' '},
	{id:0, mark:' '}
];
var mvcnt = 0;
var movestack = [
	{ id:0, moved:0, dep:0 },
	{ id:0, moved:0, dep:0 },
	{ id:0, moved:0, dep:0 },
	{ id:0, moved:0, dep:0 },
	{ id:0, moved:0, dep:0 },
	{ id:0, moved:0, dep:0 },
	{ id:0, moved:0, dep:0 },
	{ id:0, moved:0, dep:0 },
	{ id:0, moved:0, dep:0 },
	{ id:0, moved:0, dep:0 }
];

// These were extern...
var trans;
var pfile;

// From data.c
const terrain = "~.%#^";        /* standard types only. */
const terr_attr = {
	' ':'HB4',
	'~':'HB4',
	'.':'HY3',
	'%':'HG2',
	'#':'HW1',
	'^':'HW7',
	'*':'HW',
	'!':'HR'
};
const genattrs = {
	'help':'N',
	'prompt':'N',
	'border':'NW',
	'header':'N',
	'title':'N'
};

const attrs = {
	'main_screen':'NW',
	'status_area':'N',
	'world_bar':'NK7',
	'reader_pointer':'N',
	'reader_topbottom':'N',
	'reader_deleted':'N',
	'viewer_deleted':'N',
	'viewer_text':'N',
	'army_area':'NW',
	'army_list':'NW',
	'army_total':'NW',
	'city_name':'NW',
	'nation_status':'N',
	'army_types':'N',
	'title_text':'N',
	'title_copyright':'N',
	'title_world':'N',
	'title_newcity':'N',
	'title_retry':'N',
	'title_symbollist':'N',
	'title_symboltitle':'N',
	'title_welcome':'N',
	'title_anykey':'N',
	'title_nameprompt':'N',
	'mailer':'N'
};

const terr_names = [
    "Water",
    "Plains",
    "Forest",
    "Hills",
    "Mountains",
];

const special_desc = [
	"None",
	"Transport Hero",
	"Transport One",
	"Transport Stack"
];

// From reader.c
var r_index = [];
var killed=[];
var index_cnt = 0;
var pages = [];
var page_cnt = 0;

function nationname(n)
{
    return nations[n].name;
}

function turn()
{
    return gen;
}

function mainscreen()
{
    var i;

    console.clear(attrs.main_screen);

	console.attributes = genattrs.border;
	console.gotoxy(2, 1);
	console.print(ascii(201));
	console.gotoxy(2, 18);
	console.print(ascii(200));
	console.gotoxy(35, 1);
	console.print(ascii(205)+ascii(187));
	console.gotoxy(35, 18);
	console.print(ascii(205)+ascii(188));

    for(i = 0; i < 16; i++) {
		console.gotoxy(2, i+2);
		console.print(ascii(186));
		console.gotoxy(35, i+2);
		console.print(' '+ascii(186));
		console.gotoxy(i*2+3, 1);
		console.print(ascii(205)+ascii(205));
		console.gotoxy(i*2+3, 18);
		console.print(ascii(205)+ascii(205));
    }

    console.gotoxy(1, 20);
	console.attributes = genattrs.border;
    console.print((new Array(81)).join(ascii(196)));

	console.attributes = attrs.status_area;
	for(i=21; i<=console.screen_rows; i++) {
    	console.gotoxy(1, i);
		console.cleartoeol();
	}

    console.gotoxy(41, 1);
    console.attributes = attrs.world_bar;
    console.print(format("   %-20s    Turn %d   ", world, gen));
}

function saystat(msg)
{
    console.gotoxy(2, 21);
	console.attributes = attrs.status_area;
    console.print(msg);
    console.cleartoeol();
}

function mailer(from, to)
{
	var heading;
	var logf;
	var fp;
	var f;
	var fname;
	var inbuf;

	if(to > 0) {
		heading = format("From %s of %s to %s of %s  (Turn %d)",
			nationname(from), nationcity(from),
			nationname(to), nationcity(to), turn());
	} else {
		heading = format("From %s of %s  (Turn %d)",
			nationname(from), nationcity(from), turn());
	}

	console.clear(attrs.mailer);
	console.gotoxy(1,1);

	f = new File(format("%s/%04d.tmpmsg", game_dir, nations[from].uid));
	if(!f.open("wb")) {
    	mainscreen();
		saystat("Message Creation Failed (System Error)");
		return;
	}
	f.close();
	if(console.editfile(f.name)) {
		if(file_size(f.name) > 0) {
			if(to > 0)
				fname = game_dir+'/'+format(MAILFL, to);
			else
				fname = game_dir + '/' + NEWSFL;

			if(!f.open('rb')) {
				file_remove(f.name);
    			mainscreen();
				saystat("Mail Could Not Be Read...  Cancelled.");
				return;
			}

			fp = new File(fname);

			if(!fp.open('ab')) {
				file_remove(f.name);
    			mainscreen();
				saystat("Mail Could Not Be Sent...  Cancelled.");
				return;
			}

			fp.write(HEADERMARK);
			fp.write(heading);
			fp.write("\n\n");

			while((inbuf = f.readln()) != null)
				fp.writeln(inbuf);

			f.close();
			fp.close();

			logf = new File(game_dir+'/'+MAILLOG);
			if(logf.open('ab')) {
				logf.write(HEADERMARK);
				logf.write(heading);
				logf.write("\n\n");
			}
		}
	}
	file_remove(f.name);
    mainscreen();
	saystat("Mail Sent.");

}

/*
    showpage() shows a page of text from the current file, starting at
    the file position in pages[pg], for 19 lines.
*/

function showpage(fp, pg)
{
    var i, len;
    var inbuf;

	fp.position = pages[pg];

	len = HEADERMARK.length;

	console.attributes = attrs.viewer_text;
    for(i = 0; i < 19; i++) {
        if((inbuf = fp.readln(1024)) == null)
            break;
		inbuf = inbuf.substr(0, 77);

        if(inbuf.substr(0, 2) == HEADERMARK)
            break;

        console.gotoxy(3, i+1);
        console.print(inbuf);
        console.cleartoeol();
    }

    for(; i < 19; i++) {
        console.gotoxy(3, i+1);
        console.cleartoeol();
    }

    console.gotoxy(56, 22);
	console.attributes = genattrs.help;
    console.print(format("Page %d of %d", pg + 1, page_cnt));
    console.cleartoeol();

    console.gotoxy(71, 22);
	console.attribtes = attrs.reader_prompt;
    console.print("< >");
}

function show_killed(pos)
{
    console.gotoxy(41, 22);

    if(killed[pos]) {
		console.attributes = attrs.viewer_deleted;
        console.print("[Deleted]");
	}
    else {
		console.attributes = genattrs.help;
        console.print("         ");
	}
}

function viewerscr(mode)
{
    console.clear(genattrs.header);

    console.gotoxy(3, 21);

	console.attributes = genattrs.help;
    if(mode)
        console.print("Mail:  (d)elete  ");
    else
        console.print("News:  ");

    console.print("(z)down  (a)up");

    console.gotoxy(3, 22);
    console.print("Press SPACE to Exit.");

    console.gotoxy(71, 22);
	console.attributes = attrs.viewer_prompt;
    console.print("< >");
}

/*
    delete_msgs() deletes messages from the named file, using the
    information in the r_index[] and killed[] arrays.
*/

function delete_msgs(fn)
{
    var i, done, len;
    var tmp, inbuf;
    var inf, outf;

    len = HEADERMARK.length;

	tmp = 'tmp'+fn;

	inf = new File(game_dir+'/'+fn);
	outf = new File(game_dir+'/'+tmp);
	if(!inf.open("rb")) {
		log("Message Deletion Failed (System Error)");
		saystat("Message Deletion Failed (System Error)");
		return;
	}
	if(!outf.open("wb")) {
		inf.close();
		log("Message Deletion Failed (System Error)");
		saystat("Message Deletion Failed (System Error)");
		return;
	}

    for(i = 0; i < index_cnt; i++) {
        if(!killed[i]) {
			outf.write(HEADERMARK);
			inf.position = r_index[i];
			done = 0;
			while((inbuf = inf.readln(1024)) != null && !done) {
				inbuf = inbuf.substr(0, 77);
				if(inbuf.substr(0, len) == HEADERMARK)
					done = 1;
				else
					outf.write(inbuf+'\n');
			}
        }
	}

	inf.close();
	outf.close();

    file_remove(inf.name);
    file_rename(outf.name, inf.name);
}

function readerscr(mode)
{
    console.clear(genattrs.header);

    console.gotoxy(3, 21);

	console.attributes = genattrs.help;
    if(mode)
        console.print("Mail:  (v)iew current  (d)elete  ");
    else
        console.print("News:  (v)iew current  ");

    console.print("(z)down  (a)up  (]) next  ([) previous");

    console.gotoxy(3, 22);
    console.print("Press SPACE to Exit.");
}

/*
    viewer() displays text from the given file, from the given
    index offset to the end of file or until a line beginning with
    a HEADERMARK is encountered.
*/

function viewer(fp, pos, mode)
{
    var ch, i, len, done, pg, opg;
    var dummy;

    len = HEADERMARK.length;

	fp.position = r_index[pos];

    viewerscr(mode);
    show_killed(pos);

    done = 0;

    page_cnt = 0;

    do {
        pages[page_cnt++] = fp.position;

        for(i = 0; i < 15; i++) {
            if((dummy = fp.readln(1024)) == null) {
                done = 1;
                break;
            }
			dummy = dummy.substr(0, 77);
            if(dummy.substr(0, len) == HEADERMARK) {
                done = 1;
                break;
            }
        }

    } while(!done);

    pg = 0;

    showpage(fp, pg);

    do {
        console.gotoxy(72, 22);
        ch = console.getkey();

        opg = pg;

        switch(ch) {

        case 'z' :
        case KEY_DOWN:
            pg++;
            break;

        case 'a' :
        case KEY_UP:
            pg--;
            break;

        case 'd' :
            killed[pos] = killed[pos] ? 0 : 1;
            show_killed(pos);
            break;

        case ' ' :
        case 'q' :
            ch = '\033';
            break;

        }

        if(pg < 0)         pg = 0;
        if(pg >= page_cnt) pg = page_cnt - 1;

        if(pg != opg)
            showpage(fp, pg);

    } while(ch != '\033');

    readerscr(mode);
}

/*
    show_screen() shows headers from the given file, centering at the
    header number given.
*/

function show_screen(pos, fp)
{
    var i;
    var inbuf;

    pos -= 8;

    for(i = 0; i < 18; i++) {
        console.gotoxy(3, i+1);

        if(pos + i < index_cnt && pos + i >= 0) {
            if(killed[pos+i]) {
				console.attributes = attrs.reader_deleted;
                console.print('*');
			}
			else {
				console.attributes = genattrs.header;
                console.print(' ');
			}
			fp.position = r_index[pos+i];
			inbuf = fp.readln(1024);
			if(inbuf != null) {
				inbuf = inbuf.substr(0, 77);
           		console.print(inbuf);
			}
        } else {
			console.attributes = attrs.reader_topbottom;
			if(pos + i == -1)
           		console.print("*** Top of List ***");
        	else if(pos + i == index_cnt)
            	console.print("*** End of List ***");
		}

        console.cleartoeol();
    }
}

/*
    indexer() reads the given file, scanning for headings.  headings
    are flagged with a space-backspace combination (" \b") which is
    not shown on most printouts but is easily recognized.

    the array r_index[] contains the seek positions of each heading.
    it is limited by index_cnt.
*/

function indexer(fp)
{
	var inbuf, len, pos;

    index_cnt = 0;

    len = HEADERMARK.length;

    fp.rewind();

    for(pos = fp.position; (inbuf = fp.readln(1024)) != null; pos = fp.position) {
		inbuf=inbuf.substr(0, 77);
		if(inbuf.substr(0, len)==HEADERMARK) {
			killed[index_cnt] = 0;
			r_index[index_cnt++] = pos + len;
		}
	}
}

/*
    reader() displays a list of headings from the given file.  the
    user may page or "arrow" up or down to view long listings.  when
    a heading is selected with the (v)iew command, viewer() is called.

    mode is 0 for news, 1 for mail.  mode 1 enables deletion of headings.
*/

function reader(fname, mode)
{
    var top, pos, ch, killcnt;
    var fp = new File(game_dir+'/'+fname);

    if(!fp.open("rb")) {
        if(mode)
            saystat("No Mail in your box.");
        else
            saystat("No News to Read.");
        return;
    }

    readerscr(mode);

    indexer(fp);

    top = pos = index_cnt - 1;

    show_screen(top, fp);

    do {
        console.gotoxy(2, pos - top + 9);
		console.attributes = attrs.reader_pointer;
        console.print('>');

        ch = console.getkey();

        console.gotoxy(2, pos - top + 9);
		console.attributes = genattrs.header;
        console.print(' ');

        switch(ch) {

        case 'z' :
        case KEY_DOWN:
            pos++;
            break;

        case 'a' :
        case KEY_UP:
            pos--;
            break;

        case ']' :
        //case KEY_PAGEDOWN:
            pos += 11;
            break;

        case '[' :
        //case KEY_PAGEUP:
            pos -= 10;
            break;

        case 'd' :
            if(!mode)
                break;

            killed[pos]++;
            killed[pos] %= 2;

            show_screen(top, fp);

            break;

        case 'v' :
	case '\r' :
            viewer(fp, pos, mode);
            show_screen(top, fp);
            break;

        case ' ' :
        case 'q' :
            ch = '\033';
            break;

        }

        if(pos < 0)      pos = 0;
        if(pos >= index_cnt) pos = index_cnt - 1;

        if(pos > top + 8 || pos < top - 7) {
            top = pos;
            show_screen(top, fp);
        }

    } while(ch != '\033');

    fp.close();

    mainscreen();

    /* handle deletion. */

    if(mode) {
        for(killcnt = 0, pos = 0; pos < index_cnt; pos++)
            killcnt += killed[pos];

        if(killcnt != 0) { /* deleted some... */

            if(killcnt == index_cnt) /* deleted ALL */
                file_remove(game_dir+'/'+fname);
            else
                delete_msgs(fname);
        }
    }
}

function analyze_stack(ms, mc)
{
    var i, j, a, b, mmode;

    mmode = 0;

    for(i = 0; i < mc; i++)
        if(armies[ms[i].id].special_mv > mmode)
            mmode = armies[ms[i].id].special_mv;

    switch(mmode) {

    case TRANS_ALL :

        /* locate fastest transporter */
        j = -1;
        for(i = 0; i < mc; i++) {
            a = ms[i].id;
            if(armies[a].special_mv == TRANS_ALL) {
                if(j == -1 || armies[a].move_left > armies[j].move_left)
                    j = a;
                ms[i].dep = a;
            }
        }

        /* set (almost) all */
        for(i = 0; i < mc; i++) {
            a = ms[i].id;
            if(armies[a].special_mv == TRANS_ALL)
                ms[i].dep = a;
            else if(ms[i].dep == -1)
                ms[i].dep = j;
        }

        break;

    case TRANS_SELF :

        /* easy... all move by themselves */
        for(i = 0; i < mc; i++)
            ms[i].dep = ms[i].id;

        break;

    default :
        /* transport hero and/or one army */

        /* set all heros for transportation */
        for(i = 0; i < mc; i++) {
            a = ms[i].id;
            if(armies[a].hero > 0)
                for(j = 0; j < mc; j++) {
                    b = ms[j].id;
                    if(armies[b].special_mv == TRANS_HERO
                    && ms[j].dep == -1) {
                        ms[j].dep = b;
                        ms[i].dep = b;
                        break;
                    }
                }
        }

        /* set remaining for transportation */
        for(i = 0; i < mc; i++) {
            a = ms[i].id;
            if(ms[i].dep == -1
            && armies[a].special_mv == TRANS_SELF) {
                for(j = 0; j < mc; j++) {
                    b = ms[j].id;
                    if(armies[b].special_mv == TRANS_ONE
                    && ms[j].dep == -1) {
                        ms[j].dep = b;
                        ms[i].dep = b;
                        break;
                    }
                }
            }
        }

        /* leftovers transport self. */
        for(i = 0; i < mc; i++)
            if(ms[i].dep == -1)
                ms[i].dep = ms[i].id;

        break;
    }
}

function movecost(a, r, c)
{
    var t, tbl, mv, city, cost, a2;

    city = city_at(r, c);

    if(city == -1) {
        /* not a city */

        /* search for a TRANS_ALL unit there */
        if(!(armies[a].r == r && armies[a].c == c)
        && armies[a].special_mv != TRANS_ALL)
            for(a2 = 0; a2 < armycnt; a2++)
                if(a != a2
                && armies[a2].r == r
                && armies[a2].c == c
                && armies[a2].nation == armies[a].nation
                && armies[a2].special_mv == TRANS_ALL)
                    return 1;

        /* else do this: */
		t = terrain.indexOf(map[r][c]);
        if(t != -1) {
            tbl = armies[a].move_tbl;
            mv = move_table[tbl].cost[t];
            
            if(mv == 0
                /* otherwise can't move, and */
            && map[r][c] != '~' 
                /* target space is not water, and */
            && map[armies[a].r][armies[a].c] == '~'
                /* current space is water, and */
            && (cost = move_table[tbl].cost[0]) > 0)
                /* this unit moves on water, then */
                mv = cost * 2; /* beaching is allowed */
        } else {
            if(map[r][c] == '+')
                mv = 1;
            else
                mv = 3;
		}
    } else {

        /* city */

        if(armies[a].nation == cities[city].nation)
            mv = 1;
        else
            mv = 2;
    }

    return mv;
}

function clearstat(mode)
{
	console.attributes = attrs.status_area;
    if(mode == -1) {
        console.gotoxy(2, 21); console.cleartoeol();
        console.gotoxy(2, 22); console.cleartoeol();
        console.gotoxy(2, 23); console.cleartoeol();
        console.gotoxy(2, 24); console.cleartoeol();
    } else {
        console.gotoxy(2, 21+mode); console.cleartoeol();
    }
}

function mark_of(a)
{
    var i;

    if(mvcnt < 1)
        return 0;

    for(i = 0; i < mvcnt; i++)
        if(movestack[i].id == a)
            return 1;

    return 0;
}

function sortview()
{
    var gap, i, j, temp;

    avpnt = 0;

    for(gap = parseInt(avcnt / 2); gap > 0; gap = parseInt(gap / 2)) {
        for(i = gap; i < avcnt; i++) {
            for(j = i - gap; j >= 0 && isgreater(armyview[j+gap].id, armyview[j].id); j -= gap) {
                temp = armyview[j].id;
                armyview[j].id = armyview[j+gap].id;
                armyview[j+gap].id = temp;
                temp = armyview[j].mark;
                armyview[j].mark = armyview[j+gap].mark;
                armyview[j+gap].mark = temp;
            }
		}
	}
}

function setfocus(ntn, r, c)
{
    var i, j, e;

    avcnt = 0;

    /* clear the enemies list */

    for(e = 0; e < 2; e++) {
        enemies[e].nation = -1;
        enemies[e].armies = 0;
        enemies[e].heros = 0;
    }

    /* clear the map overlay */

    for(i = 0; i < map_height; i++)
        for(j = 0; j < map_height; j++)
            mapovl[i][j] = ' ';

    /* find the player's armies */

    for(i = 0; i < armycnt; i++) {
        if(r == armies[i].r
        && c == armies[i].c)
            if(armies[i].nation == ntn || ntn == -1) {
                armyview[avcnt].id = i;
                armyview[avcnt++].mark = mark_of(i);
            } else {

                e = 0;

                if(enemies[0].nation != armies[i].nation
                && enemies[0].nation != -1)
                    e = 1;

                enemies[e].nation = armies[i].nation;

                if(armies[i].hero > 0)
                    enemies[e].heros++;
                else
                    enemies[e].armies++;
            }

        if(armies[i].nation >= 0) {
            if(mapovl[armies[i].r][armies[i].c] == ' '
            || mapovl[armies[i].r][armies[i].c] ==
               marks[1].substr(marks[0].indexOf([nations[armies[i].nation].mark]), 1)) {
                mapovl[armies[i].r][armies[i].c] =
                    marks[1].substr(marks[0].indexOf([nations[armies[i].nation].mark]), 1);
			}
            else {
                mapovl[armies[i].r][armies[i].c] = '!';
			}
        }
    }

    sortview();

    /* update map overlay with cities */

    for(i = 0; i < citycnt; i++) {
        if(mapovl[cities[i].r][cities[i].c] == ' '
        || marks[1].indexOf(mapovl[cities[i].r][cities[i].c]) != -1) {
            mapovl[cities[i].r][cities[i].c] =
                nations[cities[i].nation].mark;
        }
        else {
            mapovl[cities[i].r][cities[i].c] = '!';
		}
    }
}

function showarmies()
{
    var i, a;
    var buff;

	console.attributes = attrs.army_area;
    for(i = 0; i < 12; i++) {

        console.gotoxy(38, i + 3);

        if(i < avcnt) {
            a = armyview[i].id;
            console.print(format("%s %c ",
                i == avpnt ? "=>" : "  ",
                armyview[i].mark ? '*' : ' '));
            buff = armyname(a);

            if(armies[a].name.length == 0 && armies[a].hero > 0)
                buff = "(Nameless Hero)";
            else if(armies[a].hero > 0)
                buff += " (Hero)";

            console.print(format("%-31.31s %d:%d", buff,
                armies[a].move_rate, armies[a].move_left));
        }

        console.cleartoeol();
    }

    console.gotoxy(38, 14);
	console.attributes = attrs.army_total;
    if(avcnt > 0)
        console.print(format("     Total %d Armies", avcnt));
    console.cleartoeol();

	console.attributes = attrs.army_area;
    for(i = 0; i < 2; i++) {
        console.gotoxy(43, i + 16);

        if(enemies[i].nation != -1)
            console.print(format("%-15.15s %3d Armies, %3d Heros",
                i == 1
                    ? "(Other Nations)"
                    : nationcity(enemies[i].nation),
                enemies[i].armies, enemies[i].heros));

        console.cleartoeol();
    }
}

function gmapspot(r, c, terr, mark, focus, extra_attr)
{
	var attr;
    if(mark == ' ')
        mark = terr;

    if(terr == '~')
        terr = ' ';

    if(mark == '~')
        mark = ' ';

    console.gotoxy(c * 2 + 4, r + 2);
	if(terr_attr[mark] == undefined)
		attr = 'N';
	else {
   		attr = terr_attr[terr];
		if(terr_attr[mark] != undefined)
    		attr += terr_attr[mark];
	}
	console.attributes = attr+extra_attr;
    console.print(mark);
    console.attributes = terr_attr[terr]+extra_attr;
    console.print(terr);

    console.gotoxy(c * 2 + 4, r + 2);

    return 0;
}

function cityowner(c)
{
    var n;

    n = cities[c].nation;

    if(n == 0)  return "Neutral";
    if(n == 27) return "Rogue";

    return cities[nations[n].city].name;
}

function showcity(r, c)
{
    var i;
    var buff = '';

    console.gotoxy(4, 19);

    i = city_at(r, c);

    if(i != -1) {
        buff = format("City:  %s (%s)",
            cities[i].name, cityowner(i));
    }

	console.attributes = attrs.city_name;
    console.print(format("%-40.40s", buff));
}

var old_ul_r = -1;
var old_ul_c = -1;
function showmap(r, c, force)
{
    var ul_r, ul_c, f_r, f_c;
    var i, j, zr, zc;
    var rem;

    showarmies();

    rem = parseInt((16 - gran) / 2);
    f_r = r % gran;
    f_c = c % gran;

    ul_r = ((parseInt(r / gran) * gran - rem) + map_height) % map_height;
    ul_c = ((parseInt(c / gran) * gran - rem) + map_width) % map_width;

    if(old_ul_r != ul_r || old_ul_c != ul_c || force) {
        for(i = 0; i < 16; i++) {
            for(j = 0; j < 16; j++) {
                zr = (ul_r+i) % map_height;
                zc = (ul_c+j) % map_width;
                gmapspot(i, j, map[zr][zc], mapovl[zr][zc], 0, '');
            }
		}
	}

	console.attributes='N';
    old_ul_r = ul_r;
    old_ul_c = ul_c;

    if(mapovl[r][c] != ' ')
        showcity(r, c);

    console.gotoxy(f_c * 2 + 16, f_r + 8);
}

function showfocus(r, c)
{
    var f_r, f_c, rem;

    rem = parseInt((16 - gran) / 2);

    f_r = (r % gran) + rem;
    f_c = (c % gran) + rem;

    gmapspot(f_r, f_c, map[r][c], mapovl[r][c], 1, '');
    console.attributes='N';
}

function fixrow(r)
{
    r += map_height;
    r %= map_height;

    return r;
}

function fixcol(c)
{
    c += map_width;
    c %= map_width;

    return c;
}

function move_mode(ntn, rp, cp)
{
    var i, j, mv, ch, city, army, t_r, t_c, a, b, ok, cnt, max, flag;
    var ac, hc, mmode;
    var mvc = new Array(TRANS_ALL+1);
    var gap, temp;
    var unmarked = [
		{ id:0, moved:0, dep:0 },
		{ id:0, moved:0, dep:0 },
		{ id:0, moved:0, dep:0 },
		{ id:0, moved:0, dep:0 },
		{ id:0, moved:0, dep:0 },
		{ id:0, moved:0, dep:0 },
		{ id:0, moved:0, dep:0 },
		{ id:0, moved:0, dep:0 },
		{ id:0, moved:0, dep:0 },
		{ id:0, moved:0, dep:0 },
		{ id:0, moved:0, dep:0 },
		{ id:0, moved:0, dep:0 }
    ];
    var uncnt;

    var buff;

    if(avcnt < 1) {
        saystat("No Army to Move.");
        return {r:rp,c:cp};
    }

    mvcnt = 0;
    flag = 0;

    for(i = 0; i < avcnt; i++) {
        if(armyview[i].mark)
            flag = 1;
        if(armyview[i].mark
        && (armies[armyview[i].id].move_left > 0 || ntn == -1)) {
            if(mvcnt >= 10 && ntn >= 0) {
                saystat("Too Many Armies for Group.");
        		return {r:rp,c:cp};
            }
            movestack[mvcnt].moved = 0;
            movestack[mvcnt].dep = -1;
            movestack[mvcnt++].id = armyview[i].id;
        }
    }

    if(mvcnt < 1 && !flag
    && (armies[armyview[avpnt].id].move_left > 0 || ntn == -1)) {
        movestack[mvcnt].moved = 0;
        movestack[mvcnt].dep = -1;
        movestack[mvcnt++].id = armyview[avpnt].id;
        armyview[avpnt].mark = 1;
    }

    if(mvcnt < 1 && ntn > -1) {
        saystat("Armies Have No Movement Left.");
        return {r:rp,c:cp};
    }

    /* 
        prevent stranding:  
            analyze the unmarked armies
    
        create a "movestack" of the unmarked.
        if they can move, stranding can't occur.
    */

    if(ntn > -1) {

        uncnt = 0;

        for(i = 0; i < avcnt; i++) {
            if(!armyview[i].mark) {
                unmarked[uncnt].moved = 0;
                unmarked[uncnt].dep = -1;
                unmarked[uncnt++].id = armyview[i].id;
            }
        }

        analyze_stack(unmarked, uncnt);

        for(i = 0; i < uncnt; i++) {
            a = unmarked[i].id;
            if(unmarked[i].dep == a
            && armies[a].special_mv != TRANS_ALL
            && movecost(a, armies[a].r, armies[a].c) == 0) {
                saystat("Armies Would Be Stranded...  Movement Cancelled.");
                return {r:rp,c:cp};
            }
        }
    }

    /* analyze move stack */

    if(ntn > -1)
        analyze_stack(movestack, mvcnt);

    /* perform movement */

    clearstat(-1);

    console.gotoxy(2, 22);
	console.attributes = attrs.status_area;
    console.print(format("Move %s", mvcnt > 1 ? "Armies" : "Army"));

    console.gotoxy(21, 23);
    console.print("4   6  or  d   g");

    console.gotoxy(21, 24);
    console.print("1 2 3      c v b");

    console.gotoxy(21, 22);
    console.print("7 8 9      e r t    SPACE to Stop.  ");

    setfocus(ntn, rp, cp);
    showmap(rp, cp, false);
    showfocus(rp, cp);

    while(mvcnt > 0 && (ch = console.getkey()) != ' ' 
    && terminate.indexOf(ch) == -1) {

        showfocus(rp, cp);
        clearstat(0);

        t_r = rp;
        t_c = cp;

        switch(ch) { /* directions */

        case '7' :
        case 'e' :
            t_r--;
            t_c--;
          break;

        case '8' :
        case 'r' :
            t_r--;
            break;

        case '9' :
        case 't' :
            t_r--;
            t_c++;
            break;

        case '4' :
        case 'd' :
            t_c--;
            break;

        case '6' :
        case 'g' :
            t_c++;
            break;

        case '1' :
        case 'c' :
            t_r++;
            t_c--;
          break;

        case '2' :
        case 'v' :
            t_r++;
            break;

        case '3' :
        case 'b' :
            t_r++;
            t_c++;
            break;

        case '\f' :
			// TODO: Refresh
            break;
        }

        t_r = fixrow(t_r);
        t_c = fixcol(t_c);

        if(t_r != rp || t_c != cp) {
            /* actual move code... */

            ok = 1;

            /* verify that all units can make the move. */

            if(ntn > -1) {

                for(i = 0; i < mvcnt; i++) {

                    a = movestack[i].id;

                    if(movestack[i].dep == a) {

                        mv = movecost(a, t_r, t_c);

                        if(mv > armies[a].move_left
                        || mv == 0) {
                            ok = 0;
                            buff = format("%s Can't Move There.",
                                armyname(a));
                            saystat(buff);
                            break;
                        }
                    }
                }
            }

            /* prevent overstacking. */

            if(ok && ntn > -1) {
                cnt = 0;

                for(i = 0; i < armycnt; i++)
                if(armies[i].nation == ntn
                    && armies[i].r == t_r
                    && armies[i].c == t_c)
                        cnt++;

                city = city_at(t_r, t_c);

                max = 10;

                if(city != -1
                && cities[city].nation == ntn)
                    max = 12;

                if(mvcnt + cnt > max) {
                    ok = 0;
                    saystat("Too Many Armies There.");
                }
            }

            /* can't leave combat. */

            if(ok && ntn > -1)
                for(i = 0; i < armycnt; i++)
                    if(armies[i].nation != ntn
                    && armies[i].r == rp
                    && armies[i].c == cp) {
                        ok = 0;
                    saystat("Can't Leave Combat.");
                        break;
                    }

            /* do it! */

            if(ok) {
                for(i = 0; i < mvcnt; i++) {
                    a = movestack[i].id;

                    if(ntn > -1) {
                        if(movestack[i].dep == a)
                            mv = movecost(a, t_r, t_c);
                        else
                            mv = armies[a].move_left ? 1 : 0;
                    } else
                        mv = 0;

                    movestack[i].moved += mv;
                    armies[a].move_left -= mv;
                    armies[a].r = t_r;
                    armies[a].c = t_c;
                }
                rp = t_r;
                cp = t_c;
            }

            /* redo screen. */

            setfocus(ntn, rp, cp);
            showmap(rp, cp, ok);
        }

        showfocus(rp, cp);
    }

    if(ntn > -1)
        for(i = 0; i < mvcnt; i++)
            if(movestack[i].moved > 0 || ntn == -1)
                pfile.write(format("move-army %d %d %d %d %d\n",
                    movestack[i].id, movestack[i].moved,
                    rp, cp, movestack[i].dep));

    clearstat(-1);

    mvcnt = 0;

    return {r:rp,c:cp};
}

function my_army_at(r, c, n)
{
    var i;

    for(i = 0; i < armycnt; i++)
        if(armies[i].nation == n
        && armies[i].r == r
        && armies[i].c == c)
            return i;

    return -1;
}

function show_info(r, c)
{
    var buff, buf2;
    var i, ch, city, ntn, ac, hc;

    buff = '';
    city = city_at(r, c);

    if(city != -1) {
        buff = format("City:  %s (%s)", cities[city].name,
            nationcity(cities[city].nation));
    } else {
        ch = map[r][c];

	i=terrain.indexOf(ch);
	if(i==-1 && ch==' ')
		buff = "Ocean";
	else
		buff = terr_names[i];
    }

    ac = 0;
    hc = 0;

    for(i = 0; i < armycnt; i++)
        if(armies[i].r == r && armies[i].c == c)
            if(armies[i].hero > 0)
                hc++;
            else
                ac++;

    buf2 = format("  %d Armies, %d Heros", ac, hc);
    buff += buf2;

    saystat(buff);
}

function info_mode(rp, cp, n, ch)
{
    var done, r, c, ul_r, ul_c, f_r, f_c, t_r, t_c, a_r, a_c;
    var city, army, i, focus;
    var rem;

    rem = parseInt((16 - gran) / 2);

    showfocus(rp, cp);

    clearstat(-1);

    console.gotoxy(21,23);
	console.attributes = attrs.status_area;	
    console.print("4   6  or  d   g");

    console.gotoxy(21,24);
    console.print("1 2 3      c v b");

    console.gotoxy(2,22);
    console.print("Info Mode:         7 8 9      e r t      ESC to Stop.  ");

    r = a_r = rp;
    c = a_c = cp;

    ul_r = ((parseInt(r / gran) * gran - rem) + map_height) % map_height;
    ul_c = ((parseInt(c / gran) * gran - rem) + map_width) % map_width;

    f_r = (r % gran) + rem;
    f_c = (c % gran) + rem;

    t_r = (ul_r + f_r + map_height) % map_height;
    t_c = (ul_c + f_c + map_width) % map_width;

    done = 0;

    do {
    	gmapspot(f_r, f_c, map[t_r][t_c], mapovl[t_r][t_c], 0, '');
		console.attributes='N';
        switch(ch) {

        case '7' :
        case 'e' :
            f_r--;
            f_c--;
            break;

        case '8' :
        case 'r' :
            f_r--;
            break;

        case '9' :
        case 't' :
            f_r--;
            f_c++;
            break;

        case '4' :
        case 'd' :
            f_c--;
            break;

        case '6' :
        case 'g' :
            f_c++;
            break;

        case '3' :
        case 'b' :
            f_r++;
            f_c++;
            break;

        case '2' :
        case 'v' :
            f_r++;
            break;

        case '1' :
        case 'c' :
            f_r++;
            f_c--;
            break;

		case 'i' :
			break;

        case '\f' :
	    // TODO: refresh
            break;

        default :
            done = 1;
        }

        if(f_r < gran-1) {
			f_r += gran;
			ul_r -= gran;
			rp = (rp - gran + map_height) % map_height;
			focus = true;
		}
        if(f_c < gran-1) {
			f_c += gran;
			ul_c -= gran;
			cp = (cp - gran + map_width) % map_width;
			focus = true;
		}

        if(f_r > 15 - gran + 1) {
			f_r -= gran;
			ul_r += gran;
			rp = (rp + gran + map_height) % map_height;
			focus = true;
		}
        if(f_c > 15 - gran + 1) {
			f_c -= gran;
			ul_c += gran
			cp = (cp + gran + map_width) % map_width;
			focus = true;
		}

        t_r = (ul_r + f_r + map_height) % map_height;
        t_c = (ul_c + f_c + map_width) % map_width;

        city = my_city_at(t_r, t_c, n);
        army = my_army_at(t_r, t_c, n);

        if(focus || army >= 0 || city >= 0) {
			if(army >= 0|| city >= 0) {
				a_r = t_r;
				a_c = t_c;
				setfocus(n, t_r, t_c);
			}
			showmap(rp, cp, false);
			focus = false;
		}

		show_info(t_r, t_c);

    	gmapspot(f_r, f_c, map[t_r][t_c], mapovl[t_r][t_c], 1, 'I');
		console.attributes='N';
        console.gotoxy(f_c * 2 + 4, f_r + 2);

        if(!done)
            ch = console.getkey();

    } while(!done);

   	gmapspot(f_r, f_c, map[t_r][t_c], mapovl[t_r][t_c], 0, '');
	console.attributes='N';

    return {r:a_r,c:a_c,ch:ch};
}

function groupcmp(r1, c1, r2, c2)
{
    /* quadrant check */

    if(parseInt(r1 / 16) > parseInt(r2 / 16))
        return 1;

    if(parseInt(r1 / 16) < parseInt(r2 / 16))
        return -1;

    if(parseInt(c1 / 16) > parseInt(c2 / 16))
        return 1;

    if(parseInt(c1 / 16) < parseInt(c2 / 16))
        return -1;

    /* exact check */

    if(r1 > r2)
        return 1;

    if(r1 < r2)
        return -1;

    if(c1 > c2)
        return 1;

    if(c1 < c2)
        return -1;

    return 0;
}

function prevgroup(ntn, rp, cp)
{
    var i, t_r, t_c;

    t_r = -1;
    t_c = -1;

    for(i = 0; i < citycnt; i++) {
        if(cities[i].nation == ntn) {
            if(groupcmp(cities[i].r, cities[i].c, rp, cp) < 0
            && (t_r == -1
            || groupcmp(cities[i].r, cities[i].c, t_r, t_c) > 0)) {
                t_r = cities[i].r;
                t_c = cities[i].c;
			}
		}
	}

    for(i = 0; i < armycnt; i++) {
        if(armies[i].nation == ntn) {
            if(groupcmp(armies[i].r, armies[i].c, rp, cp) < 0
            && (t_r == -1
            || groupcmp(armies[i].r, armies[i].c, t_r, t_c) > 0)) {
                t_r = armies[i].r;
                t_c = armies[i].c;
			}
		}
	}

    if(t_r != -1)
		return {r:t_r,c:t_c,ret:true};
	else
        return {r:rp,c:cp,ret:false};
}

function prevarmy(ntn, rp, cp)
{
    var i, t_r, t_c;

    t_r = -1;
    t_c = -1;

    for(i = 0; i < armycnt; i++) {
        if(armies[i].nation == ntn && armies[i].move_left > 0) {
            if(groupcmp(armies[i].r, armies[i].c, rp, cp) < 0
            && (t_r == -1
            || groupcmp(armies[i].r, armies[i].c, t_r, t_c) > 0)) {
                t_r = armies[i].r;
                t_c = armies[i].c;
			}
		}
	}

    if(t_r != -1)
		return {r:t_r,c:t_c,ret:true};
	else
        return {r:rp,c:cp,ret:false};
}

function nextgroup(ntn, rp, cp)
{
    var i, t_r, t_c;

    t_r = -1;
    t_c = -1;

    for(i = 0; i < citycnt; i++) {
        if(cities[i].nation == ntn) {
            if(groupcmp(cities[i].r, cities[i].c, rp, cp) > 0
            && (t_r == -1
            || groupcmp(cities[i].r, cities[i].c, t_r, t_c) < 0)) {
                t_r = cities[i].r;
                t_c = cities[i].c;
			}
		}
	}

    for(i = 0; i < armycnt; i++) {
        if(armies[i].nation == ntn) {
            if(groupcmp(armies[i].r, armies[i].c, rp, cp) > 0
            && (t_r == -1
            || groupcmp(armies[i].r, armies[i].c, t_r, t_c) < 0)) {
                t_r = armies[i].r;
                t_c = armies[i].c;
			}
		}
	}

    if(t_r != -1)
		return {r:t_r,c:t_c,ret:true};
    else
        return {r:rp,c:cp,ret:false};
}

function nextarmy(ntn, rp, cp)
{
    var i, t_r, t_c;

    t_r = -1;
    t_c = -1;

    for(i = 0; i < armycnt; i++) {
        if(armies[i].nation == ntn && armies[i].move_left > 0) {
            if(groupcmp(armies[i].r, armies[i].c, rp, cp) > 0
            && (t_r == -1
            || groupcmp(armies[i].r, armies[i].c, t_r, t_c) < 0)) {
                t_r = armies[i].r;
                t_c = armies[i].c;
			}
		}
	}

    if(t_r != -1)
		return {r:t_r,c:t_c,ret:true};
    else
		return {r:rp,c:cp,ret:false};
}

var help_mode = 0;
function help()
{
    console.gotoxy(2, 21);
	console.attributes = attrs.status_area;
    console.print(format("Commands, Page %d  (Press ? for More Help)", help_mode + 1));
    console.cleartoeol();

    switch(help_mode) {

    case 0 :
        console.gotoxy(2, 22);
        console.print("  (])next group  ([)previous group  (})last group  ({)first group  (q)uit game");
        console.cleartoeol();

        console.gotoxy(2, 23);
        console.print("  (CTRL-]) Next movable army  (CTRL-[) Previous movable army  (s)tatus display");
        console.cleartoeol();

        console.gotoxy(2, 24);
        console.print("  (A)rmy capabilities   army (p)roduction  (?) toggle quick help  (h)elp");
        console.cleartoeol();

        break;

    case 1 :
        console.gotoxy(2, 22);
        console.print("  (z/Down)pointer down  (a/Up)pointer up  (SPACE)mark/unmark  (*)mark all");
        console.cleartoeol();

        console.gotoxy(2, 23);
        console.print("  (/)unmark all  (m)ove marked armies  (f/5)mark all and move  (n)ame hero");
        console.cleartoeol();

        console.gotoxy(2, 24);
        console.print("  (I)nformation about current army    Read (N)ews  (S)end message  read (M)ail");
        console.cleartoeol();
        break;

    }

    help_mode++;
    help_mode %= 2;
}

function status()
{
    var ch, pos, i, j, hc, ac, cc;

    /* set up screen */

    console.clear(attrs.nation_status);

    console.gotoxy(21, 2);
	console.attributes = genattrs.title;
    console.print("Nation Status Display");

	console.attributes = genattrs.header;
    console.gotoxy(2, 4);    console.print("Nation:");
    console.gotoxy(2, 5);    console.print("Ruler:");
    console.gotoxy(2, 6);    console.print("Mark:");
    console.gotoxy(2, 8);    console.print("Cities:");
    console.gotoxy(2, 9);    console.print("Heros:");
    console.gotoxy(2, 10);   console.print("Armies:");

	console.attributes = genattrs.help;
    console.gotoxy(2, 12);   console.print("<]> next page  <[> previous page  (SPACE) to exit");

    /* display loop */

    pos = 0;

    do {
        /* show the nations. */

        for(i = 4; i <= 11; i++) {
            console.gotoxy(17, i);
            console.cleartoeol();
        }

		console.attributes = attrs.nation_status;
        for(i = 0; i < 4; i++)
            if(pos + 1 + i < nationcnt) {
                console.gotoxy(i * 16 + 17, 4);
                console.print(nationcity(pos+1+i));

                console.gotoxy(i * 16 + 17, 5);
                console.print(nations[pos+1+i].name);

                console.gotoxy(i * 16 + 20, 6);
                console.print(nations[pos+1+i].mark);

                cc = 0;

                for(j = 0; j < citycnt; j++)
                    if(cities[j].nation == pos + 1 + i)
                        cc++;

                hc = 0;
                ac = 0;

                for(j = 0; j < armycnt; j++)
                    if(armies[j].nation == pos + 1 + i)
                        if(armies[j].hero > 0)
                            hc++;
                        else
                            ac++;

                console.gotoxy(i * 16 + 17, 8);
                console.print(format("%5d", cc));

                console.gotoxy(i * 16 + 17, 9);
                console.print(format("%5d", hc));

                console.gotoxy(i * 16 + 17, 10);
                console.print(format("%5d", ac));
            }

        /* handle input. */

        console.gotoxy(71, 12);
		console.attributes = attrs.nstatus_prompt;
        console.print("< >\b\b");

        switch((ch = console.getkey())) {

        case ' ' :
        case 'q' :
            ch = '\033';
            break;

        case ']' :
            if(pos + 5 < nationcnt)
                pos += 4;
            break;

        case '[' :
            if(pos - 3 > 0)
                pos -= 4;
            break;
        }

    } while(ch != '\033');

    /* clean up */

    mainscreen();
}

function armytypes()
{
    var ch, pos, i, j, hc, ac, cc;

    /* set up screen */

    console.clear(attrs.army_types);

    console.gotoxy(21, 2);
	console.attributes = genattrs.title;
    console.print("Army Type Display");

	console.attributes = genattrs.header;
    console.gotoxy(2, 4);    console.print("Name:");
    console.gotoxy(2, 5);    console.print("Combat:");
    console.gotoxy(2, 6);    console.print("Move Rate:");
    for(i = 0; terr_names[i] != undefined; i++) {
		console.gotoxy(2, 7+i);   console.print(terr_names[i]+":");
	}
    console.gotoxy(2, 7+i);   console.print("Special:");

	console.attributes = genattrs.help;
    console.gotoxy(2, 9+i);   console.print("<]> next page  <[> previous page  (SPACE) to exit");

    /* display loop */

    pos = 0;

    do {
        /* show the types. */

		console.attributes = attrs.army_types;
        for(i = 4; i <= 8+terr_names.length; i++) {
            console.gotoxy(17, i);
            console.cleartoeol();
        }

        for(i = 0; i < 4; i++)
            if(pos + i < ttypecnt) {
                console.gotoxy(i * 16 + 17, 4);
                console.print(ttypes[pos+i].name);

                console.gotoxy(i * 16 + 17, 5);
                console.print(format("%5d",ttypes[pos+i].combat));

                console.gotoxy(i * 16 + 17, 6);
                console.print(format("%5d",ttypes[pos+i].move_rate));

                cc = 0;

                for(j = 0; j < terr_names.length; j++) {
					console.gotoxy(i * 16 + 17, 7+j);
					if(move_table[ttypes[pos+i].move_tbl].cost[j] == 0)
						console.print("Impassable");
					else
						console.print(format("%5d", move_table[ttypes[pos+i].move_tbl].cost[j]));
				}
                console.gotoxy(i * 16 + 17, 7+j);
                console.print(special_desc[ttypes[pos+i].special_mv]);
            }

        /* handle input. */

        console.gotoxy(71, 9+j);
		console.attributes = attrs.atype_prompt;
        console.print("< >\b\b");

        switch((ch = console.getkey())) {

        case ' ' :
        case 'q' :
            ch = '\033';
            break;

        case ']' :
            if(pos + 5 < ttypecnt)
                pos += 4;
            break;

        case '[' :
            if(pos - 3 > 0)
                pos -= 4;
            break;
        }

    } while(ch != '\033');

    /* clean up */

    mainscreen();
}

function produce(city)
{
    var i, t, ch;
    var buff='';
    var okstring='';

	console.attributes = attrs.status_area;
    console.gotoxy(2, 22);  console.cleartoeol();
    console.gotoxy(2, 23);  console.cleartoeol();

    for(i = 0; i < cities[city].ntypes; i++) {
        if(cities[city].types[i] != -1) {
            console.gotoxy(parseInt(i / 2) * 30 + 2, (i % 2) + 22);
            console.print(format("%d) %-14.14s (%d)", i + 1,
                ttypes[cities[city].types[i]].name,
                cities[city].times[i]));
            okstring += (i+1);
        }
    }

	console.gotoxy(2, 24);
    t = cities[city].types[cities[city].prod_type];
	console.print(format("Current:  %s (%d turns left)    ESC to Cancel",
        ttypes[t].name,
        cities[city].turns_left));
    console.cleartoeol();

    saystat(format("Set Army Production for %s:  ", cities[city].name));

    if(okstring.indexOf(ch = console.getkeys(okstring+'\x1b', 0)) != -1) {
        buff = format("set-produce %d %d\n", city, ch - 1);
        pfile.write(buff);
        execpriv(buff);
    }

    clearstat(-1);
}

function mainloop(ntn)
{
    var ch, r, c, i, n, city, army, force, obj;
    var inbuf, buff;
	var keep_ch = false;

    r = -1;
    c = -1;

    army = -1;

    /* find the player's capitol city */

    if(cities[nations[ntn].city].nation == ntn) {
        i = nations[ntn].city;
        r = cities[i].r;
        c = cities[i].c;
       setfocus(ntn, r, c);
    }

    /* find the player's first city */

    city = city_at(r, c);

    if(city != -1 && cities[city].nation != ntn)
        city = -1;

    if(city == -1)
        for(i = 0; i < citycnt; i++)
            if(cities[i].nation == ntn) {
                r = cities[i].r;
                c = cities[i].c;
                setfocus(ntn, r, c);
                break;
            }

    /* find the player's first army */

    if(city == -1)
        for(i = 0; i < armycnt; i++)
            if(armies[i].nation == ntn) {
                r = armies[i].r;
                c = armies[i].c;
                army = i;
             setfocus(ntn, r, c);
                break;
            }

    if(army == -1 && city == -1) {
        saystat("Nation is Destroyed.  Press Any Key:  ");
        console.getkey();
        return;
    }

	/* Check for messages */
	
	inbuf = format(MAILFL, ntn);
	reader(inbuf, 1);

    /* enter the loop */

    saystat("Welcome to Solomoriah's WAR!  Press <h> for Help.");

    force = false;

    for(;;) {
        showmap(r, c, force);
        force = false;

        showfocus(r, c);
		if(!keep_ch)
        	ch = console.getkey();
		keep_ch = false;
        showfocus(r, c);

        clearstat(-1);

        switch(ch) {

        case 'p' : /* production */
            city = city_at(r, c);
            if(city != -1 && cities[city].nation == ntn)
                produce(city);
            else
                saystat("Must View Your Own City First.");
            break;

		case 'A' : /* army types */
			armytypes();
			force = true;
			break;

        case ctrl(']') : /* next army */
			obj = nextarmy(ntn, r, c);
			r = obj.r;
			c = obj.c;
            if(!obj.ret)
                saystat("No Next Army with Movement Found");
            else
                setfocus(ntn, r, c);
            break;

        case ']' : /* next group */
			obj = nextgroup(ntn, r, c);
			r = obj.r;
			c = obj.c;
            if(!obj.ret) {
                saystat("No More Groups Remain.");
            } else
                setfocus(ntn, r, c);
            break;

        case '}' : /* last group */
            while((obj = nextgroup(ntn, r, c)).ret) {
				r = obj.r;
				c = obj.c;
			}
			r = obj.r;
			c = obj.c;
            setfocus(ntn, r, c);
            break;

        case ctrl('[') : /* prev army */
			obj = prevarmy(ntn, r, c);
			r = obj.r;
			c = obj.c;
            if(!obj.ret)
                saystat("No Previous Army with Movement Found");
            else
                setfocus(ntn, r, c);
            break;

        case '[' : /* previous group */
			obj = prevgroup(ntn, r, c);
			r = obj.r;
			c = obj.c;
            if(!obj.ret) {
                saystat("No Previous Groups Remain.");
            } else
                setfocus(ntn, r, c);
          break;

        case '{' : /* first group */
            while((obj = prevgroup(ntn, r, c)).ret) {
				r = obj.r;
				c = obj.c;
			}
			r = obj.r;
			c = obj.c;
            setfocus(ntn, r, c);
            break;

        case 'n' : /* name hero */
            if(avcnt > 0 && armies[armyview[avpnt].id].name.length == 0) {
                saystat("Enter Hero's Name:  ");
                inbuf = console.getstr(16);

                buff = format("name-army %d '%s'\n", armyview[avpnt].id, inbuf);
                pfile.write(buff);
                execpriv(buff);

                setfocus(ntn, r, c);
                clearstat(-1);
            } else
                saystat("Can Only Rename Heros.");
            break;

        case 'z' : /* next army */
        case KEY_DOWN:
            if(avcnt > 0) {
                avpnt++;
             avpnt += avcnt;
                avpnt %= avcnt;
            } else
                saystat("No Armies!");
            break;

        case 'a' : /* previous army */
        case KEY_UP:
            if(avcnt > 0) {
                avpnt--;
                avpnt += avcnt;
                avpnt %= avcnt;
            } else
                saystat("No Armies!");
            break;

        case ' ' : /* mark */
            if(avcnt > 0)
                armyview[avpnt].mark =
                    armyview[avpnt].mark ? 0 : 1;
            break;

        case 'I' : /* army information */
            if(avcnt > 0) {
                var id = armyview[avpnt].id;
                buff = format("%s: Combat %d / Hero %d %s",
                    armyname(id), armies[id].combat, armies[id].hero,
                    ((armies[id].hero > 0 && armies[id].eparm1) ? "(Loyal)" : ""));
                saystat(buff);
            } else
                saystat("No Armies!");
            break;

        case '*' : /* mark all */
        case 'f' : /* mark all and move */
        case '5' : /* mark all and move */
            for(i = 0; i < avcnt; i++)
                if(armies[armyview[i].id].move_left > 0)
                    armyview[i].mark = 1;
            if(ch == '*')
                break;
            /* fall through */

        case 'm' : /* move */
            obj = move_mode(ntn, r, c);
            r = obj.r;
            c = obj.c;
            setfocus(ntn, r, c);
            break;

        case '/' : /* unmark all */
            for(i = 0; i < avcnt; i++)
                armyview[i].mark = 0;
            break;

        case 'i' : /* information */
        case '1' :
        case '2' :
        case '3' :
        case '4' :
        case '6' :
        case '7' :
        case '8' :
        case '9' :
        case 'e' :
        case 'r' :
        case 't' :
        case 'd' :
        case 'g' :
        case 'c' :
        case 'v' :
        case 'b' :
            obj = info_mode(r, c, ntn, ch);
            r = obj.r;
            c = obj.c;
			ch = obj.ch;
			if(ch != '')
				keep_ch = true;
            break;

        case 'N' : /* read news... */
            reader(NEWSFL, 0);
			force = true;
            break;

        case 'M' : /* mail... */
            inbuf = format(MAILFL, ntn);
            reader(inbuf, 1);
            force = true;
            break;

        case 'S' : /* send mail */
            saystat("Send Mail -- Enter Ruler's Name, or All to Post News:  ");
            inbuf = console.getstr(16);

            n = -1;
            for(i = 0; i < nationcnt; i++)
                if(nations[i].name.toUpperCase() == inbuf.toUpperCase())
                    n = i;

            if(n == -1 && "all" == inbuf.toLowerCase())
                n = 0;

            if(n == -1) {
                saystat("No Such Ruler.");
                break;
            }

            mailer(ntn, n);

            force = true;
            clearstat(-1);
            break;

       case 's' : /* status */
            status();
            force = true;
            break;

        case '?' : /* help */
            help();
            break;

        case 'h' : /* help */
		case 'H' : /* help */
			reader('help.news', 0);
			force = true;
			break;

        case '\f' :
			force = true;
            break;

        case 'q' : /* quit */
            saystat("Quit?  (Y/N)  ");
            if(console.getkey().toLowerCase() == 'y')
                return;
            clearstat(-1);
            break;
       }
    }
}

function titlescreen()
{
    var i;
	var title = [
		"             S O L O M O R I A H ' S",
		"",
		"           #    #    ##    #####    ###",
		"           #    #   #  #   #    #   ###",
		"           #    #  #    #  #    #   ###",
		"           #    #  ######  #####     # ",
		"           # ## #  #    #  #   #       ",
		"           ##  ##  #    #  #    #   ###",
		"            #  #   #    #  #    #   ###",
	];

    console.clear(attrs.title_text);

    for(i=0; i<title.length; i++) {
        console.gotoxy(11, i+3);
        console.print(title[i]);
    }

	console.attributes = attrs.title_copyright;
    console.gotoxy(11, i+3);
    console.print('JSWAR Version '+(major_ver)+'.'+(minor_ver)+'             "Code by Solomoriah"');

    console.gotoxy(11, i+5);
    console.print("Original Copyright 1993, 1994, 2001, 2013, Chris Gonnerman");

    console.gotoxy(11, i+6);
    console.print("JavaScript Version Copyright 2013, Stephen Hurd");

    console.gotoxy(11, i+7);
    console.print("All Rights Reserved.");
}

function newcity()
{
	var i;
	var clusters = [];
	var cc;
	var c;
	var cl;

	for(i = 0; i<MAXCITIES; i++)
		clusters[i] = -1;

	for(i=0; i<citycnt; i++) {
		if(cities[i].nation > 0)
			clusters[cities[i].cluster % MAXCITIES] = 1;
		else if(clusters[cities[i].cluster % MAXCITIES] == -1)
			clusters[cities[i].cluster % MAXCITIES] = 0;
	}

    cc = 0;

    for(i = 0; i < MAXCITIES; i++)
        if(clusters[i] == 0)
            cc++;

    if(cc == 0) {   /* any city will do... */
        cc = 0;

        for(i = 0; i < citycnt; i++)
            if(cities[i].nation == 0)
                cc++;

        if(cc == 0)
            return -1;

        c = random(cc);

        for(i = 0; i < citycnt; i++)
            if(cities[i].nation == 0)
                if(c == 0)
                    return i;
                else
                    c--;

        /* should not get here... */

        return -1;

    } else {        /* find an empty cluster... */

        c = random(cc);

        for(i = 0; i < MAXCITIES; i++)
            if(clusters[i] == 0)
                if(c == 0)
                    break;
                else
                    c--;

        if(i < MAXCITIES) {
            cl = i;
            cc = 0;
            for(i = 0; i < citycnt; i++)
                if(cities[i].cluster == cl)
                    cc++;
            c = random(cc);
            for(i = 0; i < citycnt; i++)
                if(cities[i].cluster == cl)
                    if(c == 0)
                        return i;
                    else
                        c--;
        }

        /* should not get here. */

        return -1;
    }
}

function main(argc, argv)
{
	var rc;
	var uid;
	var n;
	var c;
	var ch;
	var t;
	var trys;
	var mtbl;
	var line;
	var offset;
	var confirmed;
	var fp=new File();
	var pp=new File();
	var filename='';
	var inbuf='';
	var name='';
	var p;
	var u;
	var pass='';
	var cmd='';
	var gamedir='';
	var st_buf;
	var i;

	if(argc)
		set_game(argv[0]);

	load("loadfont.js", '-H', game_dir+'/terrain.fnt');
	if(bbs.mods.CTerm_Version != null) {
		console.write("\x1b[?31h\x1b[1;255 D");
		js.on_exit('console.write("\x1b[?31l")');
	}

	titlescreen();

	/* load map file */

	if((rc = loadmap()) != 0) {
		console.gotoxy(11,21);
		console.print("Error Loading Map ("+rc+")\r\n");
		console.getkey();
		exit(1);
	}
	if((rc = loadsave()) != 0) {
		console.gotoxy(11,21);
		console.print("Error Loading Game Save ("+rc+")\r\n");
		console.getkey();
		exit(2);
	}

	console.gotoxy(11,17);
	console.attributes = attrs.title_world;
	if(world.length > 0)
		console.print("World: "+world+"    ");
	console.print("Turn:  "+gen);

	uid = user.number;

	if(uid == undefined || uid == 0) {
		console.gotoxy(11, 21);
		console.print("UID Not Available; Aborting.");
		console.getkey();
		exit(3);
	}

	console.gotoxy(11, 21);
	console.attributes = attrs.status_area;
	console.print("Reading Master Commands...  ");

	fp = new File(game_dir+'/master.cmd');
	if(fp.open('rb')) {
		for(i=0; (inbuf = fp.readln())!=null; i++) {
			if((rc=execpriv(inbuf)==0)) {
				console.gotoxy(49,21);
				console.print(format("%3d lines"));
			}
			else {
				console.gotoxy(11,21);
				console.print("Master Cmd Failed, Line "+(i+1)+", Code "+rc+"  ");
				console.getkey();
				exit(4);
			}
		}
		fp.close();
	}

    /* check if nation is entered. */

    n = -1;

    for(i = 0; i < nationcnt; i++) {
        if(nations[i].uid == uid) {
			/* TODO: Check for the canary file! */
            n = i;
            break;
        }
	}

    if(n == -1) { /* nation not entered yet... */
        c = newcity();

        if(c >= 0) {
            fp = new File(game_dir+'/master.cmd');
            if(!fp.open('ab')) {
				console.gotoxy(11,21);
				console.print("Can't Write Master Cmd's  ");
				console.cleartoeol();
				console.getkey();
				exit(5);
			}

            n = nationcnt;
            confirmed = 0;

            while(!confirmed) {
				console.gotoxy(11,19);
				console.attributes = attrs.title_newcity;
				console.print("Your City is "+ cities[c].name);
				console.cleartoeol();

				inbuf='';
				while(inbuf.length==0) {
					console.gotoxy(11, 21);
					console.attributes = attrs.title_retry;
					console.print("(Enter ! to Retry)");
					console.cleartoeol();
					console.gotoxy(11, 20);
					console.attributes = attrs.title_nameprompt;
					console.print("Enter your name:  ");
					console.cleartoeol();
					inbuf = console.getstr(NATIONNMLEN);
				}
				if(inbuf == '!')
					c = newcity();
				else
					confirmed = 1;
            }

			name=inbuf;
			console.gotoxy(11,22);
			console.attributes = attrs.title_symboltitle;
			console.print("Available:  ");
			
			inbuf = marks[0];
			for(i=0; i<nationcnt; i++)
				inbuf = inbuf.replace(nations[i].mark, '');
			inbuf = inbuf.replace(' ','');
			console.attributes = attrs.title_symbollist;
			console.print(inbuf);
			ch='';
			while(ch == '' || ch==null || inbuf.indexOf(ch)==-1) {
				console.gotoxy(11, 21);
				console.attributes = attrs.title_symbolprompt;
				console.print("Select Nation Mark:  ");
				console.cleartoeol();
				ch = console.getkeys(inbuf);
			}
			console.gotoxy(11,22);
			console.cleartoeol();
			inbuf = "new-nation '"+name+"' "+uid+' '+c+' '+ch;
			fp.writeln(inbuf);
			execpriv(inbuf);
			
			/* ... and the hero... */
			t = cities[c].types[0];
			mtbl = ttypes[t].move_tbl;
			inbuf = "make-army '"+name+"' "+n+' '+cities[c].r+' '+cities[c].c+' '+(random(3)+4)+' '+(random(2)+1)+' 8 '+(mtbl)+' 0 1';
			fp.writeln(inbuf);
			execpriv(inbuf);

			/* ...and take the city. */
			inbuf = 'control-city '+(n)+' '+(c);
			fp.writeln(inbuf);
			execpriv(inbuf);
			fp.close();
		}
		else {
			console.gotoxy(11, 21);
			console.print("No Cities Available.  ");
			console.cleartoeol();
			console.getkey();
			exit(6);
		}
	}

	/* execute player commands */

	console.gotoxy(11, 21);
	console.attributes = attrs.status_area;
	console.print("Reading Player Commands...  ");
	console.cleartoeol();
	fp = new File(format("%s/%04d.cmd", game_dir, uid));
	if(fp.open('rb')) {
		for(i=0; (inbuf = fp.readln()) != null; i++) {
			if((rc = execpriv(inbuf))==0) {
				if((i+1) % 10 == 0) {
					console.gotoxy(49, 21);
					console.print(format("%3u lines", i));
				}
			}
			else {
				console.gotoxy(11, 21);
				console.print("Player Cmd Failed, Line "+(i+1)+", Code "+(rc)+"  ");
				console.getkey();
				exit(7);
			}
		}
		fp.close();
	}

	/* append to player file... */
	
	pfile = new File(format("%s/%04d.cmd", game_dir, uid));
	if(!pfile.open('ab', 0 /* Not buffered */)) {
		console.gotoxy(11, 21);
		console.print("Can't Create or Append Player Cmd's  ");
		console.cleartoeol();
		console.getkey();
		exit(8);
	}

	/* main loop */
	
	for(i=0; i<nationcnt; i++)
		if(nations[i].uid == uid)
			n = i;

	console.gotoxy(11, 20);
	console.attributes = attrs.title_welcome;
	console.print("Welcome, "+nations[n].name+" of "+nationcity(n)+"!");
	console.cleartoeol();
	console.gotoxy(11, 21);
	console.attributes = attrs.title_anykey;
	console.print("Press Any Key to Begin...  ");
	console.cleartoeol();
	console.getkey();
	mainscreen();
	mainloop(n);
	
	/* clean up */

	pfile.close();

	exit(0);
}

main(argc, argv);

/* end of file. */
