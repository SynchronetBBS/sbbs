'use strict';

// TODO: More optimal horizontal lightbars
// TODO: Hail, Attack, Other player presses 'A', can kill someone without them knowing.
// TODO: Detect player map zero and don't load it
// TODO: The original does NOT treat something as a line unless it ends with CRLF, this means
//       things can have LFs in them and still go in an @saybar.

js.yield_interval = 0;
js.load_path_list.unshift(js.exec_dir+"dorkit/");
load("dorkit.js");
if (dk.user.full_name == 'Local User')
	dk.user.full_name = 'SYSOP';
dk.console.auto_pause = false;

require("l2lib.js", 'Player_Def');
abortontime = true;

var update_rec;
var killfiles = [];
var enemy = undefined;
var saved_cursor = {x:0, y:0};
var progname = '';
var time_warnings = [];
var file_pos = {con:0};
var last_draw;

var online = true;
var abortontime = false;
var pending_timeout;

function handle_timeout(reason)
{
	if (player.battle || player.busy) {
		pending_timeout = reason;
		dk.console.active = false;
		return;
	}
	switch (reason) {
		case 'BBS_NO_TIME':
			sclrscr();
			lln('`r0`2`c  `%The BBS has reported that you do not have any more time left.');
			break;
		case 'IDLE':
			run_ref('notime', 'help.ref');
			break;
		case 'DISCONNECT':
			break;
	}
	run_ref('endgame', 'gametxt.ref');
	exit(0);
}
time_callback = handle_timeout;

var bar_timeout = 0;
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

	// Trim to 76 spaces to fit... required by @#clearBar in MORTAL.REF
	if (dl > 76) {
		// Keep shortening the string until it's max width is short enough.
		// This ensures we keep as many codes as possible.
		while (dl > 76) {
			str = str.slice(0, -1);
			dl = displen(str);
		}
	}

	if (msg && str.indexOf(':') > -1) {
		if (!lfile.open('ab'))
			throw new Error('Unable to open '+lfile.name);
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

	b = '`r1`2Location is `0'+map.name+'`2. HP: (`%'+pretty_int(player.p[1])+'`2 of `%'+pretty_int(player.p[2])+'`2).  Press `%?`2 for help.';
	update_bar(b, false);
}

function tfile_append(str) {
	if (!tfile.open('ab'))
		throw new Error('Unable to open '+tfile.name);
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
				throw new Error('Unable to open '+tfile.name);
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

function chooseplayer()
{
	var i;
	var pl;
	var needle;
	var haystack;
	var ch;

	lln('`r0`2  Who would you like to send a message to?');
	sln('');
	lln('  `2(`0full or `%PARTIAL`0 name`2).')
	lw('  `2NAME `8: `%');
	needle = superclean(dk.console.getstr({len:26, timeout:idle_timeout * 1000})).toLowerCase();
	lastkey = time();
	sln('');

	for (i = 0; i < pfile.length; i++) {
		pl = pfile.get(i);
		haystack = superclean(pl.name).toLowerCase();
		if (haystack.indexOf(needle) !== -1) {
			lw('\r');
			dk.console.cleareol();
			lw('  `2You mean `0'+pl.name+'`2?  [`0Y`2] `8: ');
			ch = getkey().toUpperCase();
			if (ch !== 'N')
				ch = 'Y';
			lln('`%' + ch);
			if (ch === 'Y')
				return i;
		}
	}
	lw('\r  `2Sorry - no one by that name lives \'round these parts.\r\n');
	return undefined;
}

function yes_no(y, title, question) {
	var ch;
	var ret;
	var box = draw_box(y, title, ['', question, '', '`r5`$Yes','`$No']);

	dk.console.gotoxy(box.x + 3, box.y + 4);
	ret = true;
	do {
		ch = getkey().toUpperCase();
		switch (ch) {
			case 'CONNECTION_CLOSED':
				ch = '\r';
				break;
			case '8':
			case 'KEY_UP':
			case '4':
			case 'KEY_LEFT':
			case '2':
			case 'KEY_DOWN':
			case '6':
			case 'KEY_RIGHT':
			case 'KEY_END':
			case 'KEY_HOME':
				ret = !ret;
				dk.console.gotoxy(box.x + 3, box.y + 4);
				if (ret)
					lw('`r5');
				lw('`$Yes`r1');
				dk.console.gotoxy(box.x + 3, box.y + 5);
				if (!ret)
					lw('`r5');
				lw('`$No`r1');
				dk.console.gotoxy(box.x + 3, box.y + 5 - (ret ? 1 : 0));
				break;
		}
	} while (ch !== '\r');
	return ret;
}

function run_ref(sec, fname)
{
	var refret = {itemexit:false};
	calldepth = 0;
	return insane_run_ref(sec, fname, refret);
}

var calldepth = 0;
function insane_run_ref(sec, fname, refret)
{
	var line;
	var m;
	var cl;
	var args;
	var ret;

	if (++calldepth > 2)
		calldepth = 2;
	if (calldepth < 1)
		return refret;

	fname = fname.toLowerCase();
	sec = sec.toLowerCase();

	// TODO: Some things ignore comments and trailing whitespace, but ANSI certainly works for some things...
	function getlines() {
		var ret = [];

		while (line < (files[fname].lines.length - 1) && files[fname].lines[line+1].search(/^\s*@/) === -1) {
			// NOTE: "Comments" are not special in any way (see top of loop in main body of insane_run_ref()
			//if (files[fname].lines[line+1].search(/^\s*;/) === -1)
				ret.push(files[fname].lines[line+1]);
			line++;
		}
		return ret;
	}

	var do_handlers = {
		'addlog':function(args) {
			var f = new File(getfname('lognow.txt'));
			line++;
			if (line >= files[fname].lines.length)
				throw new Error('addlog at end of file');
			if (f.open('ab')) {
				cl = files[fname].lines[line];
				f.write(replace_vars(cl)+'\r\n');
				f.close();
			}
		},
		'beep':function(args) {
			throw new Error('TODO: Local-only beep not implemented');
			// TODO: Only beeps locally!
			return;
		},
		'copytoname':function(args) {
			player.name = getvar('`s10');
		},
		'delete':function(args) {
			if (args.length < 1)
				throw new Error('@do delete with no filename');
			file_remove(getfname(getvar(args[0])));
		},
		'frontpad':function(args) {
			if (args.length < 1)
				throw new Error('@do frontpad with no argument');
			var str = getvar(args[0]).toString();
			var dl = broken_displen(str);
			var l = parseInt(getvar(args[1]), 10);

			str = spaces(l - dl) + str;
			setvar(args[0], str);
		},
		'getkey':function(args) {
			if (args.length < 1)
				throw new Error('@do getkey with no argument');
			if (!dk.console.waitkey(0))
				setvar(args[0], '_');
			lastkey = time();
			setvar(args[0], dk.console.getkey());
		},
		'goto':function(args) {
			if (args.length < 1)
				throw new Error('@do goto with no argument');
			// NOTE: This doesn't use getvar() because GREEN.REF has 'do goto bank'
			args[0] = replace_vars(args[0]).toLowerCase();
			if (files[fname].section[args[0]] === undefined)
				throw new Error('@do goto undefined label');
			line = files[fname].section[args[0]].line;
		},
		'move':function(args) {
			var x, y;
			if (args.length > 1) {
				// Per CNW gametxt.ref @#stats, @do move 1 25 moves to the last line...
				// I assume Y is clamped the same way.
				x = clamp_integer(getvar(args[0]), '8');
				if (x > dk.console.cols)
					x = dk.console.cols;
				y = clamp_integer(getvar(args[1]), '8');
				if (y > dk.console.rows)
					y = dk.console.rows;
				if (x === 0 && y > 0) {
					dk.console.movey((y - 1) - scr.pos.y);
				}
				else if (y === 0 && x > 0) {
					dk.console.movex((x - 1) - scr.pos.x);
				}
				else if (y > 0 && x > 0) {
					dk.console.gotoxy(x - 1, y - 1);
				}
				else {
					throw new Error('@do move with invalid arguments');
				}
				return;
			}
			throw new Error('Invalid move at '+fname+':'+line);
		},
		'moveback':function(args) {
			player.x = player.lastx;
			player.y = player.lasty;
			update_update();
		},
		'numreturn':function(args) {
			if (args.length < 2)
				throw new Error('Insufficient arguments to @do numreturn');
			var ret = 0;
			var i;
			var haystack = getvar(args[1]);
			for (i = 0; i < haystack.length; i++) {
				switch(haystack[i]) {
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
						ret++;
						break;
				}
			}
			setvar(args[0], ret);
			return;
		},
		'pad':function(args) {
			if (args.length < 2)
				throw new Error('@do pad with insufficient arguments');
			var str = getvar(args[0]).toString();
			var dl = displen(str);
			var l = parseInt(getvar(args[1]), 10);
			if (isNaN(l))
				throw new Error('@do pad with invalid length argument');

			// NOTE: '@do pad' also trims down (see status screen in CNW with wildberries equipped)
			//       however, REFDoor documentation specifically says it doesn't do this!
			if (dl > l) {
				// Keep shortening the string until it's max width is short enough.
				// This ensures we keep as many codes as possible.
				while (dl > l) {
					str = str.slice(0, -1);
					dl = displen(str);
				}
			}
			else
				str += spaces(l - dl);
			setvar(args[0], str);
		},
		'quebar':function(args) {
			line++;
			if (line >= files[fname].lines.length)
				throw new Error('@do quebar at end of file');
			tfile_append(files[fname].lines[line]);
		},
		'readchar':function(args) {
			if (args.length < 1)
				throw new Error('@do readchar with no argument');
			// TODO: It's possible to "abort" input and get a zero-length string.
			setvar(args[0], getkey());
		},
		'readnum':function(args) {
			var x = scr.pos.x;
			var y = scr.pos.y;
			var attr = scr.attr.value;
			var s;
			var fg = 15;
			var bg = 1;
			var val = 0;

			if (args.length < 1)
				throw new Error('@do readnum requires a length parameter');
			var len = getvar(args[0]);
			if (args.length > 1) {
				val = parseInt(getvar(args[1]), 10);
				if (isNaN(val))
					val = 0;
				else if (args.length > 2) {
					bg = parseInt(getvar(getvar(args[2])), 10);
					if (isNaN(bg))
						bg = 1;
					else if (args.length > 3) {
						fg = parseInt(getvar(getvar(args[3])), 10);
						if (isNaN(fg))
							fg = 15;
					}
				}
			}
			s = dk.console.getstr({crlf:false, len:len, edit:val.toString(), input_box:true, attr:new Attribute((bg<<4) | fg), integer:true, timeout:idle_timeout * 1000});
			lastkey = time();
			setvar('`v40', s);
			dk.console.gotoxy(x, y);
			while (remove_colour(s).length < len)
				s += ' ';
			dk.console.attr = 15;
			lw(s);
			dk.console.attr = attr;
		},
		'readspecial':function(args) {
			var attr = scr.attr.value;
			var ch;
			var opts = args[1].toUpperCase();

			if (args.length < 2)
				throw new Error('@do readspecial requires two arguments');
			do {
				ch = getkey().toUpperCase();
				if (ch === '\r' || ch === '\x1b' || ch === 'CONNECTION_CLOSED')
					ch = opts.substr(0, 1);
			} while (opts.indexOf(ch) === -1);
			setvar(args[0], ch);
			dk.console.attr = 15;
			sln(ch);
			dk.console.attr = attr;
		},
		'readstring':function(args) {
			var x = scr.pos.x;
			var y = scr.pos.y;
			var attr = scr.attr.value;
			var len = getvar(args[0]);
			var s;
			var val = '';
			var svar = '`s10';
			var bg = 1;
			var fg = 15;

			if (args.length < 1)
				throw('@do readstring requires at least one argument');
			if (args.length > 1) {
				// Note that this seems to be the only place NIL is actually documented *sigh*
				val = replace_vars(args[1]);
				if (typeof val === 'string' && val.toLowerCase() === 'nil')
					val = '';
				if (args.length > 2) {
					svar = args[2];
					if (args.length > 3) {
						// What happens in invalid bg/fg?
						// See ODINKEEP.REF:1818... they're ignored!
						bg = parseInt(getvar(getvar(args[3])), 10);
						if (isNaN(bg))
							bg = 1;
						else if (args.length > 4) {
							fg = parseInt(getvar(getvar(args[4])), 10);
							if (isNaN(fg))
								fg = 15;
						}
					}
				}
			}
			s = dk.console.getstr({crlf:false, len:len, edit:val, input_box:true, attr:new Attribute((bg<<4) | fg), timeout:idle_timeout * 1000});
			lastkey = time();
			setvar(svar, s);
			dk.console.gotoxy(x, y);
			while (remove_colour(s).length < len)
				s += ' ';
			dk.console.attr = 15;
			lw(s);
			dk.console.attr = attr;
		},
		'rename':function(args) {
			if (args.length < 2)
				throw new Error('@do rename requires two arguments');
			file_rename(getfname(getvar(args[0])), getfname(getvar(args[1])));
		},
		'replace':function(args) {
			var hs;
			if (args.length < 3)
				throw new Error('@do replace requires three arguments');
			var f = getvar(args[0]);
			var r = getvar(args[1]);

			hs = getvar(args[2]);
			hs = hs.replace(f, r);
			setvar(args[2], hs);
		},
		'replaceall':function(args) {
			var hs;
			if (args.length < 3)
				throw new Error('@do replaceall requires three arguments');
			var f = getvar(args[0]);
			var r = getvar(args[1]);

			hs = getvar(args[2]);
			while (hs.indexOf(f) !== -1) {
				hs = hs.replace(f, r);
			}
			setvar(args[2], hs);
		},
		'saybar':function(args) {
			// TODO: This does not persist across other bar updates... see "[ please wait ]" in CNW GENRAN.REF and TRAVEL.REF
			line++;
			if (line >= files[fname].lines.length)
				throw new Error('Trailing saybar at '+fname+':'+line);
			update_bar(files[fname].lines[line], true, 5);
			// Required (at least) by the BEGGER.REF and TALQUIZ.REF.
			dk.console.gotoxy(78, 20);
		},
		'statbar':function(args) {
			status_bar();
		},
		'strip':function(args) {
			if (args.length < 1)
				throw new Error('@do strip requires an argument');
			setvar(args[0], getvar(args[0]).trim());
		},
		'stripall':function(args) {
			if (args.length < 1)
				throw new Error('@do stripall requires an argument');
			setvar(args[0], remove_colour(clean_str(getvar(args[0]))));
		},
		'stripbad':function(args) {
			if (args.length < 1)
				throw new Error('@do stripbad requires an argument');
			setvar(args[0], clean_str(getvar(args[0])));
		},
		'stripcode':function(args) {
			// TODO: How is this different than STRIPALL?
			if (args.length < 1)
				throw new Error('@do stripcode requires an argument');
			setvar(args[0], remove_colour(clean_str(getvar(args[0]))));
		},
		'talk':function(args) {
			var to;
			if (args.length < 1)
				throw new Error('@do talk requires an argument');
			var l = replace_vars(getvar(args[0]));

			if (args.length > 1)
				to = getvar(args[1]).toLowerCase();
			// TODO: Ensure that invalid 'to' doesn't send to all.
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
					return;
				//	throw new Error('Invalid talk to at '+fname+':'+line);
				if (player.deleted === 1)
					// TODO: Do we tell the player or something?
					return;
				update_players();
				if (players[to-1].onnow) {
					f = new File(getfname(maildir+'talk'+to+'.tmp'));
					if (f.open('ab')) {
						f.write(l+'\r\n');
						f.close();
					}
				}
			}
		},
		'trim':function(args) {
			if (args.length < 2)
				throw new Error('@do trim requires two arguments');
			var f = new File(getfname(getvar(args[0])));
			var len = parseInt(getvar(args[1]), 10);
			var al;

			if (isNaN(len))
				throw new Error('@do trim with invalid len');
			if (!file_exists(f.name))
				throw new Error('@do trim of non-existant file');
			if (!f.open('r+b'))
				throw new Error('Unable to open '+f.name+' at '+fname+':'+line);
			al = f.readAll();
			while (al.length > len)
				al.shift();
			f.position = 0;
			al.forEach(function(l) {
				f.write(l+'\r\n');
			});
			f.truncate(f.position);
			f.close();
		},
		'upcase':function(args) {
			if (args.length < 1)
				throw new Error('@do upcase requires an argument');
			setvar(args[0], getvar(args[0]).toUpperCase());
		},
		'write':function(args) {
			line++;
			if (line >= files[fname].lines.length)
				throw new Error('@do write at end of file');
			cl = files[fname].lines[line];
			lw(replace_vars(cl));
		},
	};
	var handlers = {
		// TODO: NPC Chat commands... see Jack Phlash REFDoor docs for deets.
		//       Looks like @say is just broken in LORD2 though.
		// Appears to be used to end a show/etc block
		'':function(args) {},
		'addchar':function(args) {
			var tmp;

			if (player.Record === undefined) {
				tmp = pfile.new();
				if (tmp.Record >= 200) {
					pfile.file.position = pfile.RecordLength * 200;
					pfile.truncate(pfile.RecordLength * 200);
					// TODO: What is the stack depth here?
					run_ref('full','gametxt.ref');
					exit(0);
				}
				player.Record = tmp.Record;
				ufile.new();
			}
			player_put();
			update_rec = ufile.get(player.Record);
			while(update_rec === null) {
				ufile.new();
				update_rec = ufile.get(player.Record);
			}
		},
		'begin':function(args) {
			var depth = 1;
			// Don't do this, trailing whitespace... we now delete trailing WS so do it again.
			// We *can't* delete trailing whitespace because of felicity.ref:779 (sigh)
			//if (args.length > 0)
			//	throw new Error('Unexpected arguments to begin at '+fname+':'+line);
			while (depth > 0) {
				line++;
				if (files[fname].lines[line] === undefined)
					throw new Error('Ran off end of script at '+fname+':'+line);
				if (files[fname].lines[line].search(/^\s*@#/i) !== -1)
					depth = 0;
				if (files[fname].lines[line].search(/^\s*@begin/i) !== -1)
					depth++;
				if (files[fname].lines[line].search(/^\s*@end/i) !== -1)
					depth--;
			}
		},
		'bitset':function(args) {
			if (args.length < 3)
				throw new Error('@bitset requires three arguments');
			var bit = clamp_integer(getvar(args[1]), '32');
			var s = clamp_integer(getvar(args[2]), '8');
			var v = clamp_integer(getvar(args[0]), '32');

			if (s !== 0 && s !== 1)
				throw new Error('Setting bit to value other than zero or one');
			if ((!!(v & (1 << bit))) !== (!!s))
				v ^= (1<<bit);
			setvar(args[0], v);
		},
		'busy':function(args) {
			// Turn red for other players, and run @#busy if other player interacts
			// this toggles battle...
			player.battle = 1;
			player_put();
			update_update();
		},
		'buymanager':function(args) {
			var itms = getlines();
			var itm;
			var i;
			var cur = 0;
			var ch;
			var choice;
			var y = scr.pos.y;

			for (i = 0; i < itms.length; i++) {
				itms[i] = parseInt(itms[i], 10);
				// TODO: Does this abort or ignore?
				if (isNaN(itms[i])) {
					throw new Error('@buymanager inventory item ' + (i + 1) + ' is invalid');
				}
			}
			// Don't clear the screen first?  Interesting...
			dk.console.gotoxy(0, y);
			lw('`r5`%  Item To Buy                         Price                                     ');
			dk.console.gotoxy(0, 23);
			lw('`r5                                                                               ');
			dk.console.gotoxy(2, 23);
			lw('`$Q `2to quit, `$ENTER `2to buy item.        You have `$'+pretty_int(player.money)+' `2gold.`r0');

			if (items.length === 0) {
				dk.console.gotoxy(0, 10);
				lw('  `2They have nothing to sell.  (press `%Q `2to continue)');
				do {
					ch = getkey.toUpperCase();
				} while(ch !== 'Q' && ch !== 'CONNECTION_CLOSED');
			}

			while(1) {
				choice = items_menu(itms, cur, true, false, '', y+1, 22)
				cur = choice.cur;
				switch(choice.ch) {
					case 'CONNECTION_CLOSED':
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
						lw('`$Q `2to quit, `$ENTER `2to buy item.        You have `$'+pretty_int(player.money)+' `2gold.`r0');
						break;
				}
			}
			clearrows(y, 23);
			draw_map();
			redraw_bar(true);
		},
		'checkmail':function(args) {
			mail_check(false);
		},
		'choice':function(args) {
			var allchoices = getlines();
			var choices = [];
			var cmap = [];
			var cur = getvar('`v01') - 1;
			var attr = dk.console.attr.value;
			var choice;

			if (cur < 0)
				cur = 0;
			if (cur >= allchoices.length)
				cur = allchoices.length - 1;
			function filter_choices() {
				choices = [];
				allchoices.forEach(function(l, i) {
					var m;

					do {
						m = l.match(/^([-\+=\!><])([`&0-9a-zA-Z]+) ([^ ]+) /)
						if (m !== null) {
							var left = getvar(m[2]);
							var right = getvar(m[3]);
							l = l.substr(m[0].length);
							switch(m[1]) {
								case '=':
									if (left.toString().toLowerCase() !== right.toString().toLowerCase()) {
										if (cur > i)
											cur--;
										return;
									}
									break;
								case '!':
									if (left.toString().toLowerCase() === right.toString().toLowerCase()) {
										if (cur > i)
											cur--;
										return;
									}
									break;
								case '<':
									if (parseInt(left, 10) >= parseInt(right, 10)) {
										if (cur > i)
											cur--;
										return;
									}
									break;
								case '>':
									if (parseInt(left, 10) <= parseInt(right, 10)) {
										if (cur > i)
											cur--;
										return;
									}
									break;
								case '+':
									if (!(left & (1 << parseInt(right, 10)))) {
										if (cur > i)
											cur--;
										return;
									}
									break;
								case '-':
									if (left & (1 << parseInt(right, 10))) {
										if (cur > i)
											cur--;
										return;
									}
									break;
								default:
									throw new Error('Unhandled filter choice!');
							}
						}
					} while (m != null);
					choices.push(replace_vars(l));
					cmap.push(i);
				});
			}

			filter_choices();
			dk.console.attr.value = 15;
			choice = vbar(choices, {cur:cur});
			setvar('responce', cmap[choice.cur] + 1);
			setvar('`v01', cmap[choice.cur] + 1);
		},
		'chooseplayer':function(args) {
			var pl = chooseplayer();

			if (pl === undefined)
				setvar(args[0], 0);
			else
				setvar(args[0], pl + 1);
		},
		'clear':function(args) {
			var i;
			var str;

			if (args.length < 1)
				throw new Error('@clear requires a parameter');
			switch (args[0].toLowerCase()) {
				case 'screen':
					sclrscr();
					return;
				case 'name':
					dk.console.gotoxy(54, 14);
					sw(spaces(21));
					return;
				case 'picture':
					str = spaces(22);
					for (i = 2; i < 13; i++) {
						dk.console.gotoxy(54, i);
						sw(str);
					}
					return;
				case 'text':
					str = spaces(21);
					for (i = 2; i < 13; i++) {
						dk.console.gotoxy(32, i);
						sw(str);
					}
					return;
				case 'all':
					dk.console.clear(dk.console.attr.value);
					curlinenum = 1;
					return;
				case 'userscreen':
					for (i = 15; i < 24; i++) {
						dk.console.gotoxy(0, i);
						dk.console.cleareol();
					}
					dk.console.gotoxy(77, 23);
					return;
				
			}
			throw new Error('Unknown clear type at '+fname+':'+line);
		},
		'clearblock':function(args) {
			if (args.length < 2)
				throw new Error('@clearblock requires two arguments');
			var start = parseInt(getvar(args[0]), 10) - 1;
			var end = parseInt(getvar(args[1]), 10);
			var i;
			var sattr = dk.console.attr.value;

			clearrows(start, end - 1);
			dk.console.attr.value = sattr;
		},
		'convert_file_to_ansi':function(args) {
			if (args.length < 2)
				throw new Error('@convert_file_to_ansi requires two arguments');
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
			if (args.length < 2)
				throw new Error('@convert_file_to_ascii requires two arguments');
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
		'copyfile':function(args) {
			if (args.length < 2)
				throw new Error('@copyfile requires two arguments');
			file_copy(getfname(getvar(args[0])), getfname(getvar(args[1])));
		},
		'dataload':function(args) {
			if (args.length < 3)
				throw new Error('@dataload requires three arguments');
			var f = new File(getfname(getvar(args[0])));
			var rec = parseInt(getvar(args[1]), 10);
			var val;

			if (isNaN(rec))
				throw new Error('Invalid record parameter to @dataload');
			if (!file_exists(f.name)) {
				if (!f.open('wb'))
					throw new Error('@dataload unable to create '+f.name);
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
				throw new Error('@dataload unable to open '+f.name);
			f.position = rec * 4;
			val = f.readBin(4);
			f.close();
			setvar(args[2], val);
		},
		'datanewday':function(args) {
			if (args.length < 1)
				throw new Error('@datanewday requires an argument');
			var f = new File(getfname(getvar(args[0])));
			var i;
			var d;

			if (!file_exists(f.name)) {
				if (!f.open('wb'))
					throw new Error('@datanewday Unable to create '+f.name);
				f.writeBin(state.time, 4);
				for (i = 0; i < 250; i++)
					f.writeBin(0, 4);
				f.close();
				return;
			}
			if (!f.open('r+b'))
				throw new Error('@datanewday unable to open '+f.name);
			d = f.readBin(4);
			if (d != state.time) {
				f.position = 0;
				f.writeBin(state.time, 4);
				for (i = 0; i < 250; i++)
					f.writeBin(0, 4);
			}
			f.close();
		},
		'datasave':function(args) {
			if (args.length < 3)
				throw new Error('@datasave requires three arguments');
			var f = new File(getfname(getvar(args[0])));
			var rec = parseInt(getvar(args[1]), 10);
			var val = replace_vars(getvar(args[2]));

			if (isNaN(rec))
				throw new Error('@datasave passed invalid rec parameter');
			if (!file_exists(f.name)) {
				if (!f.open('wb'))
					throw new Error('@datasave unable to create '+f.name);
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
				throw new Error('@datasave unable to open '+f.name);
			f.position = rec * 4;
			f.writeBin(val, 4);
			f.close();
		},
		'delete':function(args) {
			if (args.length < 1)
				throw new Error('@delete requires an argument');
			file_remove(getfname(getvar(args[0])));
		},
		'display':function(args) {
			if (args.length > 2 && args[1].toLowerCase() === 'in') {
				var label = getvar(args[0]).toString().toLowerCase();
				var f = new File(getfname(getvar(args[2])));
				var l;
				var found = false;

				if (!f.open('rb'))
					throw new Error('@display unable to open '+f.name);
				// First, find the label...
				while ((l = f.readln()) !== null) {
					if (l.toLowerCase().indexOf('@#'+label) === 0) {
						found = true;
						break;
					}
				}
				if (!found)
					throw new Error('@display unable to find label @#'+label+' in '+f.name);
				while ((l = f.readln()) !== null) {
					if (l.indexOf('@#') === 0)
						break;
					lln(l);
				}
			}
			throw new Error('@display not implemented');
		},
		'displayfile':function(args) {
			// TODO: This seems to be the only way the moremap string is used.
			var lines;
			if (args.length < 1)
				throw new Error('@displayfile called with no filename');
			var f = new File(getfname(getvar(args[0])));
			if (!file_exists(f.name)) {
				lln('`0File '+getvar(args[0])+' missing - please inform sysop');
				throw new Error('@displayfile '+f.name+" doesn't exist");
			}
			if (!f.open('r'))
				throw new Error('@displayfile unable to open '+f.name);
			lines = f.readAll();
			f.close();
			lines.forEach(function(l) {
				lln(l);
			});
		},
		'do':function(args) {
			var tmp;

			// TODO: This implies that the line "@do" will execute the next line... but this is used for @if parsing... also, this will choke on comments.
			// TODO: I likely just broke @do do XXX, hopefully nobody notices until I rewrite this.
			if (args.length < 1 || args[0].toLowerCase() === 'do') {
				if (line + 1 >= files[fname].lines.length)
					throw new Error('do at end of file');
				// Trailing do is not fatal... see jump.ref:21 in cnw
				//if (files[fname].lines[line + 1].search(/^\s*@begin/i) === -1)
				//	throw new Error('trailing do at '+fname+':'+line);
				tmp = line;
				while (files[fname].lines[++tmp].search(/^\s*@/) === -1) {
					if (tmp >= files[fname].lines.length)
						break;
				}
				if (tmp < files[fname].lines.length) {
					if (files[fname].lines[tmp].search(/^\s*@begin(?:\s*|;.*)$/i) != -1)
						line = tmp;
				}
				return;
			}
			if (do_handlers[args[0].toLowerCase()] !== undefined) {
				do_handlers[args[0].toLowerCase()](args.slice(1));
				return;
			}
			// TODO: Also for @if handling... 'do begin' apparently to prevent choking on a *single* comment...
			if (args.length == 2 && args[1].toLowerCase() === 'begin') {
				line++;
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
					tmp = clamp_integer(getvar(args[3]), '8') - 1;
					if (tmp === player.Record) {
						setvar(args[0], player.name);
					}
					else {
						if (tmp >= pfile.length || tmp < 0)
							setvar(args[0], '');
						else {
							tmp = pfile.get(tmp);
							setvar(args[0], tmp.name);
						}
					}
				}
				else if (args[2].toLowerCase() === 'deleted') {
					tmp = clamp_integer(getvar(args[3]), '8') - 1;
					if (tmp >= pfile.length || tmp < 0)
						setvar(args[0], 1);
					else {
						tmp = pfile.get(clamp_integer(getvar(args[3]), '8') - 1);
						setvar(args[0], tmp.deleted);
					}
				}
				else if (args[2].toLowerCase() === 'random') {
					setvar(args[0], random(clamp_integer(getvar(args[3]), 's32')) + clamp_integer(getvar(args[4]), 's32'));
				}
				else
					setvar(args[0], getsvar(args, 2, args[0]));
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
				setvar(args[0], getvar(args[0]) + getsvar(args, 2, args[0]).toString());
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
			if (args.length > 2 && args[1].toLowerCase() === 'copytoname') {
				player.name = getvar('`s10');
				return;
			}
			throw new Error('Unhandled do at '+fname+':'+line);
		},
		'drawmap':function(args) {
			draw_map();
			redraw_bar(true);
		},
		'drawpart':function(args) {
			if (args.length < 2)
				throw new Error('@drawpart requires two arguments');
			var x = getvar(args[0]);
			var y = getvar(args[1]);

			if (x < 1 || x > 80 || y < 1 || y > 20)
				throw new Error('@drawpart invalid X/Y: '+x+'/'+y);

			erase(x - 1, y - 1);
			update_space(x - 1, y - 1);
		},
		'end':function(args) {},
		'fight':function(args) {
			var l = getlines();

			if (l.length < 28)
				throw new Error('@fight requires 28 data lines.');
			function split_ref(str, prefix) {
				var l = str.split('|');

				if (l.length < 2 || l[0].toUpperCase() === 'NONE' || l[1].toUpperCase() === 'NONE')
					l = ['',''];

				enemy[prefix+'_reffile'] = getvar(l[0]);
				enemy[prefix+'_refname'] = getvar(l[1]);
			}

			function add_attack(str) {
				var l = str.split('|');
				var strength;

				// Does missing a | make the fight not happen?
				if (l.length < 2 || l[0].toUpperCase() === 'NONE' || l[1].toUpperCase() === 'NONE')
					return;

				strength = parseInt(getvar(l[1]), 10);
				if (isNaN(strength))
					throw new Error('@fight invalid strength '+l[1]);
				enemy.attacks.push({strength:strength, hitaction:replace_vars(getvar(l[0]))});
			}

			var sex = parseInt(getvar(l[7]), 10);
			if (isNaN(sex))
				throw new Error('@fight sex is invalid');
			var defence = parseInt(getvar(l[15]), 10);
			if (isNaN(defence))
				throw new Error('@fight defence is invalid');
			var gold = parseInt(getvar(l[17]), 10);
			if (isNaN(gold))
				throw new Error('@fight gold is invalid');
			var experience = parseInt(getvar(l[19]), 10);
			if (isNaN(experience))
				throw new Error('@fight experience is invalid');
			var hp = parseInt(getvar(l[21]), 10);
			if (isNaN(hp))
				throw new Error('@fight hp is invalid');
			enemy = {
				name:getvar(l[1]),
				see:replace_vars(getvar(l[3])),
				killstr:replace_vars(getvar(l[5])),
				sex:sex,
				defence:defence,
				gold:gold,
				experience:experience,
				hp:hp,
				maxhp:hp,
				attacks:[]
			};
			add_attack(l[9]);
			add_attack(l[10]);
			add_attack(l[11]);
			add_attack(l[12]);
			add_attack(l[13]);
			split_ref(l[23], 'win');
			split_ref(l[25], 'lose');
			split_ref(l[27], 'run');
		},
		'graphics':function(args) {
			// TODO: Sets the graphics support level (3 is ANSI)
		},
		'halt':function(args) {
			if (args.length > 0)
				exit(clamp_integer(args[0], 's32'));
			exit(0);
		},
		'if':function(args) {
			var tmp;
			var tmp2;
			var tmp3;
			var tmp4;

			if (args.length < 5)
				throw new Error('@if requires at least five arguments');
			function check_begin(arg) {
				// TODO: Fix this up for "comments" at end of line...
				if (arg < args.length && args[arg].toLowerCase() === 'do') {
					if (args.length > (arg + 1) && args[arg + 1].toLowerCase() === 'begin') {
						// Check next line for @begin
						if (line < files[fname].lines.length - 1) {
							if (files[fname].lines[line + 1].search(/^\s*@begin/i) === 0) {
								line++;
								handlers.begin([]);
							}
						}
					}
					else {
						// Remove empty arguments from end...
						while (args[args.length - 1] === '')
							args.pop();
						// Was the 'do' the last argument?
						if (args.length == arg + 1) {
							// Check next line for @begin
							if (line < files[fname].lines.length - 1) {
								if (files[fname].lines[line + 1].search(/^\s*@begin/i) === 0) {
									line++;
									handlers.begin([]);
								}
							}
						}
					}
				}
			}

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
				else
					check_begin(4);
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
				else
					check_begin(5);
				return;
			}
			switch(args[1].toLowerCase()) {
				case 'more':
				case '>':
					if (parseInt(getvar(args[0]), 10) > parseInt(getvar(args[2]), 10))
						handlers.do(args.slice(4));
					else
						check_begin(4);
					break;
				case '<':
				case 'less':
					if (parseInt(getvar(args[0]), 10) < parseInt(getvar(args[2]), 10))
						handlers.do(args.slice(4));
					else
						check_begin(4);
					break;
				case '!':
				case 'not':
				case "isn't":
					if (getvar(args[0]).toString().toLowerCase() !== getvar(args[2]).toString().toLowerCase())
						handlers.do(args.slice(4));
					else
						check_begin(4);
					break;
				case '=':
				case 'equals':
				case 'is':
					tmp2 = 2;
					if (args[tmp2].toLowerCase() === 'length') {
						tmp = remove_colour(expand_ticks(replace_vars(getvar(args[++tmp2])))).length;
					}
					else if (args[tmp2].toLowerCase() === 'reallength') {
						tmp = getvar(args[++tmp2]).length;
					}
					else
						tmp = getsvar1(args[tmp2], args[0]);
					tmp2++;
					if (getvar(args[0]).toString().toLowerCase() === tmp.toString().toLowerCase())
						handlers.do(args.slice(tmp2 + 1));
					else
						check_begin(tmp2 + 1);
					break;
				case 'exist':
				case 'exists':
					if (file_exists(getfname(getvar(args[0]))) === (getvar(args[2]).toLowerCase() === 'true')) {
						handlers.do(args.slice(4));
					}
					else
						check_begin(4);
					break;
				case 'inside':
					if (getvar(args[2]).toLowerCase().indexOf(getvar(args[0]).toLowerCase()) !== -1)
						handlers.do(args.slice(4));
					else
						check_begin(4);
					break;
				default:
					throw new Error('Unhandled condition '+args[1]+' at '+fname+':'+line);
			}
		},
		'key':function(args) {
			var orig_attr = dk.console.attr.value;
			var mlen = 8;

			if (args.length > 0) {
				switch(args[0].toLowerCase()) {
					case 'nodisplay':
						getkey();
						return;
					case 'top':
						dk.console.gotoxy(39, 14);
						lw(' `0MORE ');
						mlen = 6;
						break;
					case 'bottom':
						dk.console.gotoxy(35,23);
						lw(' `0MORE ');
						mlen = 6;
						break;
					// Any other argument is the same as no argument... CNW reset.ref uses '@key noshow'
					default:
						//throw new Error('Unhandled key arg "'+args[0]+'" at '+fname+':'+line);
						// No, this would be the sane thing to do... don't do it.
						//lw('\r');
						break;
				}
			}
			else {
				// No, this would be the sane thing to do... don't do it.
				//lw('\r');
				lw('`r0  `2<`0MORE`2>');
			}
			// NOTE: This doesn't actually use the "More" prompt that you can override... and it's not centred like the docs claim.
			//lw(spaces(40-(displen(morestr)/2))+morestr);
			getkey();
			lw(repeat_chars('\b', mlen));
			lw(repeat_chars(' ', mlen));
			dk.console.attr.value = orig_attr;
		},
		'label':function(args) {},
		'loadcursor':function(args) {
			dk.console.gotoxy(saved_cursor.x, saved_cursor.y);
		},
		'loadglobals':function(args) {
			world.reLoad();
		},
		'loadmap':function(args) {
			if (args.length < 1)
				throw new Error('@loadmap requires a parameter');
			var m = parseInt(getvar(args[0]), 10);
			if (isNaN(m))
				throw new Error('@loadmap passed invalid map');
			map = load_map(m);
		},
		'loadworld':function(args) {
			world = wfile.get(0);
		},
		'lordrank':function(args) {
			if (args.length < 2)
				throw new Error('@lordrank requires two parameters');
			var f = new File(getfname(getvar(args[0])));
			var rp = ranked_players(args[1]);

			if (!f.open('ab'))
				throw new Error('@lordrank unable to open '+f.name);
			rp.forEach(function(pl, i) {
				f.write((pl.sexmale === 1 ? ' ' : '`#F')+' `2'+space_pad(pl.name, 21)+'`2'+pretty_int(pl.p[0], -14)+'`%'+pretty_int(pl.p[8], -5)+'     '+space_pad((pl.dead === 1 ? '`4Dead' : '`%Alive'), 6)+(pl.p[6] >= 0 ? '`0' : '`4') + pretty_int(pl.p[6], -7)+'`%    '+(pl.p[17] > 0 ? pretty_int(pl.p[17]) : '')+((pl.t[16] & (1<<7)) ? ' `r1`%K`r0' : '')+((pl.t[17] & (1<<7)) ? ' `r4`^D`r0' : '') + '\r\n');
			});
			f.close();
		},
		'moremap':function(args) {
			line++;
			if (line >= files[fname].lines.length)
				throw new Error('@moremap at end of file');
			cl = files[fname].lines[line];
			morestr = replace_vars(cl);
		},
		'name':function(args) {
			var str;
			var len;

			if (args.length < 1)
				throw new Error('@name requires an argument');
			str = getvar(args[0]);
			dk.console.gotoxy(53, 14);
			// Truncate to 20 characters, center in 20 charaters, put space a start/end
			var len = displen(getvar(args[0]));
			if (len > 20) {	// Truncate
				while (displen(str) > 20)
					str = str.slice(0, -1);
				len = displen(str);
			}
			// Center in 22 spaces...
			str = spaces(Math.floor((23 - len) / 2)) + str + spaces(Math.ceil((23 - len) / 2));
			lw(str);
		},
		'nocheck':function(args) {
			// We don't really support this because there's no need for it.
		},
		'offmap':function(args) {
			// Disappear to other players... this toggles busy because
			// it looks like that's what it does...
			player.busy = 1;
			update_update();
			player_put();
		},
		'overheadmap':function(args) {
			overheadmap(false);
		},
		'pauseoff':function(args) {
			morechk = false;
		},
		'pauseon':function(args) {
			morechk = true;
		},
		'progname':function(args) {
			// TODO: Status bar stuff.
			line++;
			if (line >= files[fname].lines.length)
				throw new Error('@progname at end of file');
			cl = files[fname].lines[line];
			progname = replace_vars(cl);
		},
		'rank':function(args) {
			if (args.length < 3)
				throw new Error('@rank requires three arguments');
			// TODO: No real clue what the filename is for...
			var rp = ranked_players(args[1]);
			var op = player;

			rp.forEach(function(pl, i) {
				player = pl;
				// TODO: It is assumed this takes a stack frame.
				insane_run_ref(getvar(args[2]), fname, refret);
			});
			player = op;
		},
		'readfile':function(args) {
			var vs = getlines();
			var f = new File(getfname(getvar(args[0])));
			var l;

			if (f.open('r')) {
				// TODO: Verify what happens to blank lines...
				while(vs.length > 0) {
					l = f.readln();
					if (l === null)
						break;
					setvar(vs.shift(), l);
				}
				f.close();
			}
			else
				throw new Error('@readfile unable to open '+f.name);
			// Documentation says it won't change variables if file too short...
			// By felicity.ref ends up displaying junk...
			//while (vs.length) {
			//	setvar(vs.shift(), '');
			//}
		},
		'restorecursor':function(args) {
			dk.console.gotoxy(saved_cursor.x, saved_cursor.y);
		},
		'routine':function(args) {
			if (args < 1)
				throw new Error('@routine requires at least one argument');
			var s = replace_vars(args[0]).toLowerCase();
			var fn = fname;
			var ret;

			if (args.length > 1 && args[1].toLowerCase() === 'in')
				fn = replace_vars(args[2]);
			ret = insane_run_ref(s, fn, refret);
			return ret;
		},
		'routineabort':function(args) {
			// TODO: Implemented in line parser
		},
		'run':function(args) {
			var f = fname;
			var s;

			// Note, this needs to be first because an '@run' line does this with high call depth.
			if (calldepth > 1)
				return false;
			if (args < 1)
				throw new Error('@run requires at least one argument');
			s = replace_vars(args[0]).toLowerCase();
			if (args.length > 2 && args[1].toLowerCase() === 'in') {
				f = getvar(args[2]).toLowerCase();
			}
			if (f.indexOf('.') === -1)
				f += '.ref';
			if (files[f] === undefined)
				load_ref(f);
			if (files[f] === undefined)
				throw new Error('Unable to load REF "'+f+'"');
			if (files[f].section[s] === undefined) {
				sln('');
				lln('ERROR = '+s+' not found in '+fname+'!');
				throw new Error('@run unable to find run section '+s+' in '+f);
			}
			else {
				fname = f;
				line = files[f].section[s].line;
			}
		},
		'savecursor':function(args) {
			saved_cursor = {x:scr.pos.x, y:scr.pos.y};
		},
		'saveglobals':function(args) {
			world.put();
		},
		'say':function(args) {
			throw new Error('@say not implemented');
		},
		'saveworld':function(args) {
			world.put();
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
			var box;
			var y = scr.pos.y;

			dk.console.gotoxy(0, y);
			lw('`r5`%  Item To Sell                        Amount Owned                              `r0');
			dk.console.gotoxy(0, 23);
			lw('`r5  `$Q `2to quit, `$ENTER `2to sell item.                                               `r0');

rescan:
			while (1) {
				dk.console.gotoxy(39, 23);
				lw('`2`r5You have `$'+pretty_int(player.money)+' `2gold.`r0');
				inv = get_inventory();
				if (inv.length === 0) {
					dk.console.gotoxy(0, 6);
					lw('`r0  `2You have nothing to sell.  (press `%Q `2to continue)');
					do {
						ch = getkey().toUpperCase();
					} while (ch != 'Q' && ch !== 'CONNECTION_CLOSED');
					return;
				}

				if (cur >= inv.length)
					cur = 0;

				while(1) {
					choice = items_menu(inv, cur, false, true, '', y + 1, 22);
					cur = choice.cur;
					switch(choice.ch) {
						case 'Q':
							return;
						case '\r':
							itm = items[inv[cur] - 1];
							amt = 1;
							price = parseInt(itm.value / 2, 10);
							if (!itm.sell) {
								draw_box(14, itm.name, ['','`$They don\'t seem interested in that.','']);
								getkey();
								continue rescan;
							}
							if (player.i[itm.Record] > 1) {
								box = draw_box(14, items[itm.Record].name, ['', '`$Sell how many?            ',''])
								dk.console.gotoxy(box.x + 18, box.y + 2);
								// TODO: This isn't exactly right... cursor is in wrong position, and selected colour is used.
								ch = dk.console.getstr({edit:player.i[itm.Record].toString(), integer:true, input_box:true, attr:new Attribute(47), len:11, timeout:idle_timeout * 1000});
								lastkey = time();
								lw('`r1`0');
								amt = parseInt(ch, 10);
								if (isNaN(amt) || amt <= 0 || amt > player.i[itm.Record]) {
									continue rescan;
								}
							}
							yn = yes_no(16, itm.name, '`$Sell '+amt+' of \'em for '+(amt * price)+' gold?');
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
				}
				draw_map();
				redraw_bar(true);
			}
			clearrows(y, 23);
		},
		'shell':function(args) {
			// TODO?  I mean... likely not.
			throw new Error("@shell not implemented");
		},
		'show':function(args) {
			var l = getlines();
			var ch;
			var i;
			var p;
			var pages;
			var sattr = 2;

			// TODO: Verify that invalid argument results in no-show
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
						handle_lordcodes(l[p*22+i]);
						sln('');
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
						case 'CONNECTION_CLOSED':
						case 'Q':
							dk.console.attr.value = sattr;
							return;
					}
				}
			}
			else
				throw new Error('Unsupported SHOW command at '+fname+':'+line);
		},
		'showlocal':function(args) {
			// TODO: Like show, but only on local screen (see BEEP).
			throw new Error('@showlocal not implemented');
		},
		'stripcode':function(args) {
			if (args.length < 1)
				throw new Error('@stripcode requires an argument');
			setvar(args[0], remove_colour(clean_str(getvar(args[0]))));
		},
		'update':function(args) {
			player.busy = 0;
			update();
			player_put();
		},
		'update_update':function(args) {
			player.busy = 0;
			update_update();
			player_put();
		},
		'version':function(args) {
			// TODO: Figure this out...
			if (args.length < 1)
				throw new Error('Invalid version at '+fname+':'+line);
			if (parseInt(args[0] > vars.version.val))
				throw new Error('lord2.js version too old!');
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
					if (mp !== null)
						lln('  `2'+space_pad(pl.name, 29)+'`%'+space_pad(pl.p[8].toString(), 14)+'`0'+mp.name);
				}
			});
		},
		'writefile':function(args) {
			if (args.length < 1)
				throw new Error('@writefile requires a parameter');
			var f = new File(getfname(getvar(args[0])));
			if (!f.open('ab'))
				throw new Error('Unable to open '+f.name);
			getlines().forEach(function(l) {
				f.write(replace_vars(l)+'\r\n');
			});
			f.close();
		},
	};

  	function handle(args)
	{
		if (handlers[args[0].toLowerCase()] === undefined)
			throw new Error('No handler for '+args[0]+' command at '+fname+':'+line);
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
			throw new Error('Unable to open '+f.name);
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
			m = l.match(/^\s*@#([^;]+)/);
			if (m !== null) {
				cs = m[1].toLowerCase().replace(/\s*$/,'');
				// SIGH... duplicates are allowed... see Stonebridge.
				//if (obj.section[cs] !== undefined)
				//	throw new Error('Duplicate section name '+cs+' in '+fname);
				// But the *FIRST* match is the right one!
				if (obj.section[cs] === undefined)
					obj.section[cs] = {line:n};
				return;
			}
			// Labels *can* have spaces in them (see extitems.ref)
			m = l.match(/^\s*@label\s+([^;]+)/i);
			if (m !== null) {
				var lab = m[1].toLowerCase().replace(/\s*$/,'');
				// SIGH... duplicates are allowed... see Stonebridge.
				//if (obj.section[lab] !== undefined)
				//	throw new Error('Duplicate label name '+lab+' in section '+cs+' in '+fname);
				// But the *FIRST* match is the right one!
				if (obj.section[lab] === undefined)
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
		throw new Error('Unable to find section '+sec+' in '+fname);
	line = files[fname].section[sec].line;

	var eline;
	var efile;
	while (1) {
		/* 
		 * Actually ANY LINE where the first non-whitespace is not
		 * an '@' is either ignored or used by the previous command.
		 * That means we can just toss out overthing before parsing the next command.
		 */
		// Returns false to abort.
		if (calldepth < 1)
			break;
		if (ret === false)
			break;
		getlines();
		line++;
		eline = line;
		efile = fname;
		if (line >= files[fname].lines.length)
			break;
		cl = files[fname].lines[line].replace(/^\s*/,'');
		if (cl.search(/^@#/) !== -1)
			break;
		if (cl.search(/^\s*@closescript/i) !== -1)
			break;
		if (cl.search(/^\s*@itemexit/i) !== -1) {
			refret.itemexit = true;
			continue;
		}
		if (cl.search(/^\s*@routineabort/i) !== -1) {
			calldepth = 1;
			continue;
		}
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
		while (args !== undefined && args.length > 1 && args[args.length - 1] === '')
			args.pop();
		try {
			ret = handle(args);
		}
		catch(e) {
			log(LOG_ERROR, (new Date()).toString() + ", Node "+dk.connection.node
				+" "+e.fileName+" line "+e.lineNumber
				+": REF Error "+e.message+" at "+efile+":"+eline);
		}
		if (typeof ret === 'object') {
			if (ret.itemexit)
				refret.itemexit = true;
		}
	}
	calldepth--;
	return refret;
}

function load_player()
{
	var i;
	var p;

	function post_load() {
		if (world !== null) {
			map = load_map(player.map);
			// Force move to home on invalid map (can be triggered by a crash in the glen which no longer happens. :)
			if (map === null) {
				player.map = 1;
				player.x = 1;
				player.y = 1;
			}
		}
		player.lastx = player.x;
		player.lasty = player.y;
	}

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
			post_load();
			return;
		}
	}
	player = new RecordFileRecord(pfile);
	player.reInit();
	player.realname = dk.user.full_name;
	post_load();
}

function erase_player()
{
	if (last_draw !== undefined) {
		erase(last_draw.x, last_draw.y);
	}
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
	var oa = dk.console.attr.value;

	x += 1;
	y += 1;

	players.forEach(function(u, i) {
		if (u.map !== player.map)
			return;
		if (u.x !== x || u.y !== y)
			return;
		// Note that 'busy' is what 'offmap' toggles, not what 'busy' does. *sigh*
		if (i === player.Record) {
			erase_player();
			dk.console.gotoxy(x - 1, y - 1);
			foreground(15);
			background(map.mapinfo[getoffset(u.x-1, u.y-1)].backcolour);
			dk.console.print('\x02');
			last_draw = {x:u.x - 1, y:u.y - 1};
		}
		else {
			if (u.busy === 0) {
				dk.console.gotoxy(u.x - 1, u.y - 1);
				if (u.battle)
					foreground(4);
				else {
					if (map.mapinfo[getoffset(u.x-1, u.y-1)].backcolour == 7)
						foreground(8);
					else
						foreground(7);
				}
				background(map.mapinfo[getoffset(u.x-1, u.y-1)].backcolour);
				dk.console.print('\x02');
			}
		}
	});

	dk.console.attr.value = oa;
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

	if (!file_exists(fn)) {
		con_check();
		return;
	}

	if (messenger) {
		update_bar('`2A messenger stops you with the following news. (press `0R`2 to read it)', true);
		do {
			ch = getkey().toUpperCase();
		} while (ch !== 'R' && ch !== 'CONNECTION_CLOSED');
		lw('`r0`c');
	}

	// TODO: Not sure what happens on windows if someone else has the file open...
	// We likely need a file mutex here.
	file_remove(f.name)
	file_rename(fn, f.name);
	if (!f.open('r'))
		throw new Error('Unable to open '+f.name);
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
						throw new Error('Invalid REPLY ID');
					op = pfile.get(op - 1);
					if (op === null)
						throw new Error('Invalid record number '+op+' in REPLY');
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
	f.close();
	con_check();
	more();
	if (messenger) {
		draw_map();
		redraw_bar(true);
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
					sln('');
					file_remove(ch.name);
					more();
					return false;
				}
				lln(l+'`r');
				continue;
			}
		}
		if (dk.console.waitkey(game.delay)) {
			lastkey = time();
			sw('  ');
			l = clean_str(dk.console.getstr({len:72, attr:new Attribute(31), input_box:true, crlf:false, timeout:idle_timeout * 1000}));
			lastkey = time();
			lw('`r0`2\r');
			dk.console.cleareol();
			switch (l.toUpperCase()) {
				case 'X':
				case 'Q':
					if (!chop.open('ab'))
						throw new Error('Unable to open '+chop.name);
					chop.write('EXIT\r\n');
					chop.close();
					update_update()
					file_remove(ch.name);
					return true;
				case '':
					break;
				default:
					sw('\r');
					lln('  `0'+player.name+' `$: '+l+'`2');
					if (!chop.open('ab'))
						throw new Error('Unable to open '+chop.name);
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
	var lin;
	var givfile = new File(getfname(maildir + 'giv'+(player.Record + 1)+'.tmp'));
	var givfile_pos = 0;

	if (!file_exists(tx.name))
		return;
	update_bar('`0'+op.name+' `2hails you.  Press (`0A`2) to leave.', true);
	lw('`r0`2');
	hail_cleanup();
	file_remove(tx.name);
	do {
		inn = getfname(maildir + 'inf'+(player.Record + 1)+'.tmp');
		inn = file_getcase(inn);
		if (inn !== undefined) {
			inf = new File(inn)
			if (!inf.open('r'))
				throw new Error('Unable to open '+inf.name);
			inf.position = pos;
			al = inf.readAll();
			pos = inf.position;
			inf.close();
			al.forEach(function(l) {
				if (l === 'CHAT') {
					if (chat(op))
						exit = true;
				}
				else {
					update_bar(l, true);
				}
			});
		}
		if (givfile.open('rb')) {
			givfile.position = givfile_pos;
			lin = givfile.readln();
			switch(lin) {
				case null:
					break;
				case 'BYE':
					exit = true;
					// Fall-through
				default:
					givfile_pos = givfile.position;
					break;
			}
			givfile.close();
		}
		if (file_exists(getfname(maildir + 'bat'+(player.Record + 1)+'.tmp'))) {
			online_battle(op, false);
			break;
		}
		if (dk.console.waitkey(game.delay)) {
			switch(getkey().toUpperCase()) {
				case 'CONNECTION_CLOSED':
				case 'A':
					exit = true;
					break;
			}
		}
		op.reLoad();
	} while((!exit) && op.battle === 1);
	hail_cleanup();
	draw_map();
	redraw_bar(true);
	update();
}

function con_check()
{
	var al;

	if (!cfile.open('r'))
		throw new Error('Unable to open '+cfile.name);
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
				player_put();
				hailed(parseInt(c[1], 10));
				player.battle = 0;
				update_update();
				player_put();
				if (pending_timeout !== undefined)
					handle_timeout(pending_timeout);
				break;
			case 'UPDATE':
				// Not sure which this does, but no reason not to do both...
				update();
				update_update();
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

function update_players()
{
	var i;
	var op;
	var u;

	for (i = 0; i < ufile.length; i++) {
		if (i === player.Record)
			continue;
		if (i >= players.length) {
			op = pfile.get(i);
			if (op === null)
				break;
			players.push({x:op.x, y:op.y, map:op.map, onnow:op.onnow, busy:op.busy, battle:op.battle, deleted:op.deleted});
		}
		u = ufile.get(i);
		if (u === null) {
			u = ufile.new();
			u.x = players[i].x;
			u.y = players[i].y;
			u.map = players[i].map;
			u.onnow = players[i].onnow;
			u.busy = players[i].busy;
			u.battle = players[i].battle;
			u.deleted = players[i].deleted;
			u.put();
		}
		players[i] = u;
	}
}

var next_update = -1;
function update(skip) {
	var i;
	var u;
	var nop = {};
	var op;
	var now = (new Date().valueOf());
	var done;
	var orig_attr = dk.console.attr.value;

	// Save player if it's been half of the timeout time since last saved...
	if (savetime() - player.lastsaved > (idle_timeout / 60 / 2))
		player_put();
	if (map !== undefined) {
		erase_player();
		dk.console.gotoxy(player.x - 1, player.y - 1);
		foreground(15);
		background(map.mapinfo[getpoffset()].backcolour);
		dk.console.print('\x02');
		dk.console.gotoxy(player.x - 1, player.y - 1);
		last_draw = {x:player.x - 1, y:player.y - 1};
		update_update();
		if ((!skip) || now > next_update) {
			next_update = now + game.delay;
			// First, update player data
			update_players();

			// First, erase any moved players and update other_players
			done = new Array(80*20);
			players.forEach(function(u, i) {
				if (i === player.Record)
					return;
				if (u.deleted)
					return;
				if (u.map === player.map || (other_players[i] !== undefined && other_players[i].map === player.map)) {
					if (u.map === player.map)
						nop[i] = {x:u.x, y:u.y, map:u.map, onnow:u.onnow, busy:u.busy, battle:u.battle}
					// Erase old player pos...
					if (other_players[i] !== undefined) {
						op = other_players[i];
						if (op.x !== player.x || op.y != player.y) {
							if (done[getoffset(u.x, u.y)] === undefined && (op.x !== u.x || op.y !== u.y || op.map !== u.map) && (op.x !== player.x || op.y !== player.y)) {
								erase(op.x - 1, op.y - 1);
								done[getoffset(u.x, u.y)] = true;
							}
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
					if (u.battle)
						foreground(4);
					else {
						if (map.mapinfo[getoffset(u.x-1, u.y-1)].backcolour == 7)
							foreground(8);
						else
							foreground(7);
					}
					background(map.mapinfo[getoffset(u.x-1, u.y-1)].backcolour);
					dk.console.print('\x02');
					done[getoffset(u.x, u.y)] = true;
				}
			});
			other_players = nop;

			timeout_bar();
			mail_check(true);
		}
		dk.console.gotoxy(player.x - 1, player.y - 1);
	}
	dk.console.attr.value = orig_attr;
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
	var start = {x:player.x, y:player.y, map:player.map};

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
		if (map === null) {
			// Handles "start on warp" stupidity in CNW glendale.ref:enterglen
			// You can move down from the map at block 823 into an empty block.
			player.x = start.x;
			player.y = start.y;
			player.map = start.map;
			map = load_map(player.map);
			if (world.hideonmap[player.map] === 0)
				player.lastmap = player.map;
		}
		else {
			draw_map();
			redraw_bar(true);
			update();
		}
		player_put();
	}
	else {
		player.lastx = player.x;
		player.lasty = player.y;
		if (map.mapinfo[getoffset(x-1, y-1)].terrain === 1) {
			player.x = x;
			player.y = y;
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
					redraw_bar(true);
				}
				else {
					erase_player();
				}
				player.x = s.warptox;
				player.y = s.warptoy;
				player_put();
			}
			else if (s.reffile !== '' && s.refsection !== '') {
				run_ref(s.refsection, s.reffile);
				player.battle = 0;
				update_update();
				player_put();
				if (pending_timeout !== undefined)
					handle_timeout(pending_timeout);
			}
		}
	});
	while (enemy !== undefined)
		offline_battle();
	// Figure out what happens when you die from REF damage... (ie: canoe dock)
	// It seems you stay at negative HP and don't die until something hits *facepalm*.
	erase_player();
	update(true);
	perday = getvar('`v05');
	if (moved && perday > 0) {
		player.p[10]--;
		if (perday > 0) {
			if (time_warnings.indexOf(player.p[10]) !== -1)
				tfile_append(get_timestr());
		}
	}
	if (moved && !special && map.battleodds > 0 && map.reffile !== '' && map.refsection !== '') {
		if (random(map.battleodds) === 0) {
			run_ref(map.refsection, map.reffile);
			player.battle = 0;
			update_update();
			player_put();
			if (pending_timeout !== undefined)
				handle_timeout(pending_timeout);
			while (enemy !== undefined)
				offline_battle();
		}
	}
}

// Assume width of 36
// Assume position centered in inventory window thing
function popup_menu(title, opts)
{
	var str;
	var y = 12 + ((9 - opts.length)/2);
	var cur = 0;
	var ch;
	var sattr = dk.console.attr.value;
	var otxt = [];
	var oinit = [];
	var box;
	var first = '`r5`0';

	opts.forEach(function(o) {
		otxt.push(o.txt);
		oinit.push(first + o.txt);
		first = '';
	});
	box = draw_box(y, title, oinit);
	ch = vbar(otxt, {drawall:false, x:box.x + 3, y:box.y + 1, highlight:'`r5`0', norm:'`r1`0'});
	return opts[ch.cur].ret;
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
	var y;

rescan:
	while(1) {
		run_ref('stats', 'gametxt.ref');
		y = scr.pos.y + 1;
		inv = get_inventory();
		if (inv.length === 0) {
			dk.console.gotoxy(0, 12);
			lw('`2  You are carrying nothing!  (press `%Q`2 to continue)');
			do {
				ch = getkey().toUpperCase();
			} while (ch != 'Q' && ch !== 'CONNECTION_CLOSED');
			return;
		}
		else {
			if (cur >= inv.length)
				cur = inv.length - 1;
newpage:
			while (1) {
				choice = items_menu(inv, cur, false, false, 'D', y, 22);
				cur = choice.cur;
				switch(choice.ch) {
					case 'D':
						i = draw_box(12, items[inv[cur] - 1].name, ['','`$Drop how many?                  ','']);
						dk.console.gotoxy(i.x + 18, i.y + 2);
						// TODO: This isn't exactly right... cursor is in wrong position, and selected colour is used.
						ch = dk.console.getstr({edit:player.i[inv[cur] - 1].toString(), integer:true, input_box:true, attr:new Attribute(47), len:11, timeout:idle_timeout * 1000});
						lastkey = time();
						lw('`r1`0');
						ch = parseInt(ch, 10);
						if (!isNaN(ch) && ch <= player.i[inv[cur] - 1]) {
							dk.console.gotoxy(i.x + 3, i.y + 2);
							if (items[inv[cur] - 1].questitem) {
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
								if (ch === 1)
									lw('`$You go ahead and throw it away. `0');
								else
									lw('`$ You drop the offending items!  `0');
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
								dk.console.attr.value = 2;
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
						if (ret === undefined || !ret.itemexit)
							continue rescan;
						if (ret.itemexit) {
							dk.console.attr.value = attr;
							return 'ITEMEXIT';
						}
						// Fallthrough(?)
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

function hbar(x, y, opts, cur)
{
	var ch;
	if (cur === undefined)
		cur = 0;

	lw('`r0`2');
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
			case 'KEY_HOME':
				cur = 0;
				break;
			case 'KEY_END':
				cur = opts.length - 1;
				break;
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
			case 'CONNECTION_CLOSED':
			case '\r':
				return cur;
		}
	}
}

function disp_hp(cur, max)
{
	if (cur < 1)
		return '`bDead';
	return '(`0'+pretty_int(cur)+'`2/`0'+pretty_int(max)+'`2)';
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

function fist_string(ename)
{
	switch(random(5)) {
		case 0:
			return '`2You punch `0'+ename+'`2 in the jimmy';
		case 1:
			return 'You kick `0'+ename+'`2 hard as you can';
			break;
		case 2:
			return '`2You slap `0'+ename+'`2 a good one';
			break;
		case 3:
			return '`2You headbutt `0'+ename;
			break;
		default:
			return '`2You trip `0'+ename;
			break;
	}
}

function offline_battle(no_super, skip_see)
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
	if (no_super === undefined)
		no_super = false;
	if (skip_see === undefined)
		skip_see = false;
	var ret;
	var oldbattle = player.battle;

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
	if (skip_see)
		lw('`r0`2');
	else
		lw('`r0`2'+enm.see);
	while(1) {
		if (skip_see) {
			ch = 0;
			skip_see = 0;
		}
		else {
			ch = hbar(2, 23, ['Attack', 'Run For it']);
		}
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
				ret = 'RAN';
				break;
			}
		}
		else {
			if (no_super)
				supr = false;
			else
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
					astr = fist_string(enm.name);
				}
				else
					astr = wep.hitaction;
				lw('`2' + astr + '`2 for `0'+pretty_int(dmg)+'`2 damage!');
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
			lw('`2You find `$$'+pretty_int(enm.gold)+'`2 and gain `%'+pretty_int(enm.experience)+' `2experience in this battle.  `2<`0MORE`2>');
			player.money = clamp_integer(player.money + enm.gold, 's32');
			player.p[0] = clamp_integer(player.p[0] + enm.experience, 's32');
			if (supr)
				update_bar(enm.killstr, true, 0);
			getkey();
			if (enm.win_reffile !== '' && enm.win_refname !== '')
				run_ref(enm.win_refname, enm.win_reffile);
			ret = 'WON';
			break;
		}
		else {
			atk = enm.attacks[random(enm.attacks.length)];
			ehstr = atk.strength >> 1;
			dmg = ehstr + random(ehstr) + 1 - def;
			if (pending_timeout)
				dmg = player.p[1];
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
				lw('`r0`0'+enm.name+' `2'+atk.hitaction+'`2 for `4'+pretty_int(dmg)+'`2 damage!');
				player.p[1] = clamp_integer(player.p[1] - dmg, 's32');
				your_hp();
				if (player.p[1] < 1) {
					if (enm.lose_reffile !== '' && enm.lose_refname !== '')
						run_ref(enm.lose_refname, enm.lose_reffile);
					ret = 'LOST';
					break;
				}
			}
		}
	}
	lw('`r0`2');
	clearrows(21, 23);
	redraw_bar(true);
	return ret;
}

function mail_to(pl, quotes)
{
	var wrap = '';
	var l;
	var ch;
	var bt = 0;
	var msg = [];
	var f;
	var t;
	var op = pfile.get(pl);

	if (op === null || op.deleted)
		throw new Error('mail_to() illegal user');

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
//						sw(bs.substr(0, wrap.length));
						sw(ws.substr(0, wrap.length));
					}
					ch = '\r';
				}
			}
		} while (ch !== '\r' && ch !== 'CONNECTION_CLOSED');
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
		throw new Error('Unable to open file '+f.name);
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
	if (opt.extras.type === 'string')
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

	function movetoend() {
		dk.console.gotoxy(opt.x, opt.y + choices.length - 1);
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
		if (opt.extras.indexOf(ch) > -1) {
			movetoend();
			return ret;
		}
		switch(ch) {
			case 'KEY_HOME':
				if (opt.return_on_wrap) {
					draw_choice(oldcur);
					ret.wrap = true;
					movetoend();
					ret.cur = -2147483648;
					return ret;
				}
				ret.cur = 0;
				break;
			case '8':
			case 'KEY_UP':
			case '4':
			case 'KEY_LEFT':
				ret.cur--;
				if (ret.cur < 0) {
					if (opt.return_on_wrap) {
						draw_choice(oldcur);
						ret.wrap = true;
						movetoend();
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
						movetoend();
						return ret;
					}
					ret.cur = 0;
				}
				break;
			case 'KEY_END':
				if (opt.return_on_wrap) {
					draw_choice(oldcur);
					ret.wrap = true;
					movetoend();
					ret.cur = 2147483647;
					return ret;
				}
				ret.cur = choices.length - 1;
				break;
			case 'CONNECTION_CLOSED':
			case '\r':
				movetoend();
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
	var keys;

	keys = ('QNP93'+extras).split('');
	keys.push('KEY_PGUP');
	keys.push('KEY_PGDOWN');

	function draw_page() {
		choices = [];
		off = cnt * (Math.floor(cur / cnt));
		lw('`r0`2');
		for (i = 0; i < cnt; i++) {
			idx = i + off;
			str = '';
			if (idx < itms.length) {
				it = itms[idx] - 1;
				desc = items[it].description;
				choices.push(((selling && (items[it].sell === false)) ? '`8  ' : '`2  ')+items[it].name);
				if (cur === idx)
					str += '`r1';
				else
					str += '`r0';
				if (selling && items[it].sell === false)
					str += '`8';
				else
					str += '`2';
				str += '  '+items[it].name;
				if (cur === idx)
					str += '`r0';
				str += decorate_item(items[it]);
				if (buying) {
					str += spaces(32 - displen(str));
					str += '`2$`$'+pretty_int(items[it].value)+'`2';
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
					str += pretty_int(player.i[it]) + '`2)';
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
		choice = vbar(choices, {cur:cur % cnt, drawall:false, extras:keys, x:0, y:starty, return_on_wrap:true, highlight:'`r1`2', norm:'`r0`2'});
		oldcur = cur;
		cur = off + choice.cur;
		if (choice.wrap) {
			if (cur == -1)
				cur = itms.length - 1;
			if (cur == itms.length)
				cur = 0;
			if (cur < 0)
				cur = 0;
			if (cur > itms.length)
				cur = itms.length - 1;
			if (Math.floor(oldcur / cnt) !== Math.floor(cur / cnt))
				draw_page();
		}
		else {
			switch(choice.ch) {
				case 'KEY_PGDOWN':
				case 'N':
				case '3':
					cur += cnt;
					if (cur >= itms.length)
						cur = itms.length - 1;
					draw_page();
					break;
				case 'KEY_PGUP':
				case 'P':
				case '9':
					cur -= cnt;
					if (cur < 0)
						cur = 0;
					draw_page();
					break;
				default:
					choice.cur += off;
					return choice;
			}
		}
	}
}

function player_to_enemy(op)
{
	enemy = {
		name:op.name,
		see:'',
		killstr:'',
		sex:op.sexmale,
		defence:op.p[4],
		gold:op.money === 0 ? 0 : Math.floor(op.money / 2),
		experience:op.experience === 0 ? 0 : Math.floor(op.experience / 10),
		hp:op.p[1],
		maxhp:op.p[2],
		attacks:[],
		win_reffile:'gametxt.ref',
		lose_reffile:'gametxt.ref',
		run_reffile:'gametxt.ref',
		win_refname:'live',
		lose_refname:'die',
		run_refname:'run',
	};
	if (op.weaponnumber == 0) {
		enemy.attacks.push({
			strength:(op.p[3]),
			hitaction:'`0`e`2 hits you with '+(op.sexmale === 1 ? 'his' : 'her')+' fists',
		});
	}
	else {
		enemy.attacks.push({
			strength:(op.p[3] + items[op.weaponnumber - 1].strength),
			hitaction:'`0`e`2 hits you with '+(op.sexmale === 1 ? 'his' : 'her')+' `0'+items[op.weaponnumber-1].name+'`2',
		});
	}
	if (op.armournumber !== 0) {
		enemy.defence += items[op.armournumber - 1].defence;
	}
	setvar('`v39', op.Record + 1);
	setvar('enemy', op.name);
}

// Deletes files that are *read* during an online interaction (not written)
function hail_cleanup()
{
	file_remove(getfname(maildir + 'bat'+(player.Record + 1)+'.tmp'));
	file_remove(getfname(maildir + 'tx'+(player.Record + 1)+'.tmp'));
	file_remove(getfname(maildir + 'chat'+(player.Record + 1)+'.tmp'));
	file_remove(getfname(maildir + 'giv'+(player.Record + 1)+'.tmp'));
	file_remove(getfname(maildir + 'inf'+(player.Record + 1)+'.tmp'));
}

function online_battle(op, attack_first) {
	var str;
	var hstr;
	var def;
	var edef;
	var pbat = new File(getfname(maildir+'bat'+(player.Record + 1)+'.tmp'));
	var ebat = new File(getfname(maildir+'bat'+(op.Record + 1)+'.tmp'));
	var wname;
	var weap;
	var astr;
	var wend;
	var rln;
	var tleft;
	var m;
	var ret;
	var dmg;
	var doneBattle = false;
	var doneMenu;
	var doneMessages = false;
	var cur;
	var ln;
	var poff = 0;

	if (player.weaponnumber == 0)
		wname = 'fists';
	else {
		wname = items[player.weaponnumber - 1].name;
		weap = items[player.weaponnumber - 1];
	}

	str = (player.weaponnumber === 0 ? 0 : items[player.weaponnumber - 1].strength) + player.p[3];
	hstr = str >> 1;
	def = (player.armournumber === 0 ? 0 : items[player.armournumber - 1].defence) + player.p[4];
	edef = (op.armournumber === 0 ? 0 : items[op.armournumber - 1].defence) + op.p[4];
	player_to_enemy(op);

	enemy_hp(enemy);
	your_hp();
	while (!doneBattle) {
		if (attack_first) {
			// Make an attack...
			clearrows(21,23);
			dmg = hstr + random(hstr) + 1 - edef;
			if (!ebat.open('ab'))
				throw new Error('Unable to open '+ebat.name);
			if (dmg < 0) {
				ebat.write('`r0`0'+player.name+' `2misses you completely!|0\r\n');
				ebat.close();
				dk.console.gotoxy(2, 21);
				lw('`4You completely miss!`2');
			}
			else {
				ebat.write('`r0`0'+player.name+' `2hits you with '+(player.sexmale == 1 ? 'his' : 'her')+' `0'+wname+'`2 for `b'+pretty_int(dmg)+'`2 damage!|'+dmg+'\r\n');
				ebat.close();
				if (weap === undefined) {
					astr = fist_string(op.name);
				}
				else {
					astr = weap.hitaction;
				}
				dk.console.gotoxy(2, 21);
				lw('`2' + astr + '`2 for `0'+pretty_int(dmg)+'`2 damage!');
				enemy.hp -= dmg;
			}
			enemy_hp(enemy);
			if (enemy.hp < 1) {
				dk.console.gotoxy(2, 22);
				lw('`r0`0You killed '+(enemy.sex === 1 ? 'him' : 'her')+'!');
				dk.console.gotoxy(2, 23);
				lw('`2You find `$$'+pretty_int(enemy.gold)+'`2 and gain `%'+pretty_int(enemy.experience)+' `2experience in this battle.  `2<`0MORE`2>');
				player.money = clamp_integer(player.money + enemy.gold, 's32');
				player.p[0] = clamp_integer(player.p[0] + enemy.experience, 's32');
				player_put();
				getkey();
				run_ref('iwon', 'gametxt.ref');
				ret = 'WON';
				break;
			}

			// Wait for response from remote...
			dk.console.gotoxy(2, 23);
			lw('`r0`2You wait for the counter attack...');
			// Timeout count... 
			wend = time() + 30;
		}
		else
			attack_first = true;

		doneMessages = false;
		while (!doneMessages) {
			rln = undefined;
			if (!pbat.is_open) {
				if (file_exists(pbat.name))
					pbat.open('rb');
			}
			if (pbat.is_open) {
				pbat.position = poff;
				rln = pbat.readln();
				poff = pbat.position;
			}
			if (rln === null || rln === undefined) {
				if (pbat.is_open)
					pbat.close();
				tleft = wend - time();
				if (tleft < 1) {
					ret = 'TIMEOUT';
					if (pbat.is_open)
						pbat.close();
					file_remove(pbat.name);
					poff = 0;
					doneBattle = true;
					break;
				}
				if (tleft <= 10) {
					dk.console.gotoxy(55, 23);
					dk.console.cleareol();
					lw('`0(`2timeout in '+tleft+'`0)`2');
				}
				mswait(game.delay);
				continue;
			}
			if (pbat.is_open)
				pbat.close();

			m = rln.match(/^(.*)\|(-500|-1000|[0-9]+)\r?\n?$/);
			if (m !== null) {
				dmg = parseInt(m[2], 10);
				switch(dmg) {
					case -500:	// Other side left
						doneBattle = true;
						doneMessages = true;
						ret = 'LEFT';
						break;
					case -1000:	// Yelled something
						update_bar(m[1], true);
						wend = time() + 30;
						break;
					default:
						clearrows(22, 22);
						dk.console.gotoxy(2, 22);
						lw(m[1]);
						player.p[1] -= dmg;
						your_hp();
						if (player.p[1] < 1) {
							player.p[1] = 0;
							player_put();
							run_ref('die', 'gametxt.ref');
							ret = 'LOST';
							doneBattle = true;
							doneMessages = true;
							break;
						}
						player_put();
						doneMessages = true;
						break;
				}
			}
		}

		if (pbat.is_open)
			pbat.close();
		file_remove(pbat.name);
		poff = 0;
		if (doneBattle)
			break;

		// Ask for next battle action...
		doneMenu = false;
		cur = 0;
		while (!doneMenu) {
			clearrows(23, 23);
			if (pending_timeout !== undefined)
				chr = 2;
			else
				cur = hbar(0, 23, ['Attack', 'Yell Something', 'Leave'], cur);
			switch(cur) {
				case 0:	// Attack
					doneMenu = true;
					break;
				case 1:	// Yell something
					// Loops back to this message...
					clearrows(23, 23);
					dk.console.gotoxy(2, 23);
					lw('`r0`2Message? : ');
					ln = clean_str(dk.console.getstr({input_box:true, attr:new Attribute(31), len:66, crlf:false, timeout:idle_timeout * 1000}));
					lastkey = time();
					if (!ebat.open('ab'))
						throw new Error('Unable to open '+ebat.name);
					ebat.write(ln + "|-1000\r\n");
					ebat.close();
					break;
				case 2:	// Leave
					// Exits and away we go...
					if (!ebat.open('ab'))
						throw new Error('Unable to open '+ebat.name);
					ebat.write("RUN AWAY!|-500\r\n");
					ebat.close();
					doneMenu = true;
					doneBattle = true;
					ret = 'RANAWAY';
					break;
			}
		}
	}

	enemy = undefined;
	clearrows(21,23);
	return ret;
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
	var choice;
	var done = true;	// Defaulting to true helps find bugs!
	var cur;

	function give_item(online) {
		var inv;
		var ch;
		var f;
		var cur = 0;

		lln('`c`r0`2                                `r1`%  GIVING.  `r0');
		sln('');
		sln('');
		sln('');
		lw('`r5Item To Give                 Amount Owned');
		dk.console.cleareol();
		dk.console.gotoxy(0, 23);
		lw('  `$Q `2to quit, `$ENTER `2to give item.       You have `$'+pretty_int(player.money)+' gold.');
		dk.console.cleareol();
		dk.console.gotoxy(0, 7);
		inv = get_inventory();
		if (inv.length === 0) {
			dk.console.gotoxy(0, 7);
			lw('`r0  `2You have nothing to give, loser.  (press `%Q `2to continue)');
			do {
				ch = getkey().toUpperCase();
			} while (ch !== 'Q' && ch !== 'CONNECTION_CLOSED');
		}
		else {
			if (online) {
				f = new File(getfname(maildir+'inf'+(op.Record + 1)+'.tmp'));
				if (!f.open('ab'))
					throw new Error('Unable to open '+f.name);
				f.write(player.name+' `2searches through '+(player.sexmale == 1 ? 'his' : 'her')+' backpack. (`0A`2 to abort)\r\n');
				f.close();
			}
			while (1) {
				choice = items_menu(inv, cur, false, false, '', 7, 22);
				cur = choice.cur;
				if (choice.ch === 'Q')
					break;
				if (items[inv[choice.cur] - 1].questitem) {
					// This is presumably in a popup box too.
					draw_box(12, '', ['','`$You don\'t think it would be wise to give that away.`2','']);
					getkey();
				}
				else {
					if (player.i[inv[choice.cur] - 1] === 1)
						ch = 1;
					else if (player.i[inv[choice.cur] - 1] < 1) {
						draw_box(12, items[inv[choice.cur] - 1].name, ['', "You don't have any more of those!", '']);
						getkey();
						ch = 0;
					}
					else {
						draw_box(12, items[inv[choice.cur] - 1].name, ['','`$Give how many?`2        ','']);
						dk.console.gotoxy(44, 14);
						ch = parseInt(dk.console.getstr({len:7, crlf:false, integer:true, timeout:idle_timeout * 1000}));
						lastkey = time();
					}
					if ((!isNaN(ch)) && ch > 0 && ch <= player.i[inv[choice.cur] - 1]) {
						if (yes_no(14, items[inv[choice.cur] - 1].name, '`$Give '+pretty_int(ch)+' of \'em to '+op.name+'`$?')) {
							player.i[inv[choice.cur] - 1] -= ch;
							f = new File(getfname(maildir+'con'+(op.Record + 1)+'.tmp'));
							if (!f.open('ab'))
								throw new Error('Unable to open '+f.name);
							f.write('ADDITEM|'+inv[choice.cur]+'|'+ch+'\r\n');
							f.close();
							f = new File(getfname(maildir+'mail'+(op.Record + 1)+'.dat'));
							if (!f.open('ab'))
								throw new Error('Unable to open '+f.name);
							f.write('  `%`r1GOOD NEWS!`r0\r\n`0  '+repeat_chars(ascii(0xc4), 77)+'\r\n  `0'+player.name+' `2gives you ');
							if (ch === 1)
								f.write('a ');
							else
								f.write(pretty_int(ch) + ' ');
							f.write('really nice `$' + items[inv[choice.cur] - 1].name + '`2!\r\n');
							f.write('@DONE\r\n');
							f.close();
						}
					}
				}
			}
			if (online) {
				f = new File(getfname(maildir+'inf'+(op.Record + 1)+'.tmp'));
				if (!f.open('ab'))
					throw new Error('Unable to open '+f.name);
				f.write(player.name+' `2closes '+(player.sexmale == 1 ? 'his' : 'her')+' backpack. (`0A`2 to abort)\r\n');
				f.close();
			}
			draw_map();
			redraw_bar(true);
			update();
			dk.console.gotoxy(0, 21);
		}
	}

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
		clearrows(21, 23);
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
				case 'KEY_HOME':
					cur = 0;
					if (page != 0)
						erase_menu();
					page = 0;
					break;
				case 'KEY_END':
					cur = pl.length - 1;
					if (page != pages - 1)
						erase_menu();
					page = pages - 1;
				case 'CONNECTION_CLOSED':
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
	player_put();
	if (op.onnow === 0) {
		op.battle = 1;
		op.put(false);
		update_players();
		players[op.Record].battle = 1;
		players[op.Record].put();
		update_bar('You find `0'+op.name+'`2 sleeping like a baby. (hit a key)', true);
		getkey();
		done = false;
		cur = 0;
		while (!done) {
			cur = hbar(2, 23, ['Leave', 'Attack', 'Give Item', 'Transfer Gold', 'Write Mail'], cur);
			switch(cur) {
				case 0:
					done = true;
					break;
				case 1: // Attack
					if (map.nofighting) {
						if (world.v[8] === 0 || op.p[8] < world.v[8]) {
							update_bar("No fighting is allowed in this area.", true);
							break;
						}
					}
					player_to_enemy(op);
					switch (offline_battle(true, true)) {
						case 'RAN':
							// TODO: Do the opponents HP drop in an offline battle they win?
							break;
						case 'WON':
							op.dead = 1;
							op.x = 1;
							op.y = 1;
							op.map = 1;
							players[op.Record].x = 0;
							players[op.Record].y = 0;
							players[op.Record].map = 1;
							players[op.Record].battle = 0;
							players[op.Record].put();
							if (op.money > 0)
								op.money -= Math.floor(op.money / 2);
							if (op.experience > 0)
								op.experience -= Math.floor(op.exterience / 10);
							break;
						case 'LOST':
							// TODO: Do the opponents HP drop in an offline battle they win?
							break;
					}
					done = true;
					break;
				case 2: // Give Item
					give_item(false);
					break;
				case 3: // Transfer Gold
					clearrows(21,23);
					dk.console.gotoxy(1, 22);
					lw('`r0`2Give `0'+op.name+'`2 how much of your `$$'+pretty_int(player.money)+'`2? : ');
					// TODO: This isn't exactly right... cursor is in wrong position, and selected colour is used.
					ch = dk.console.getstr({edit:player.money.toString(), integer:true, input_box:true, attr:new Attribute(31), len:11, timeout:idle_timeout * 1000});
					lastkey = time();
					ch = parseInt(ch, 10);
					clearrows(22);
					if (ch > 0) {
						if (ch > player.money) {
							dk.console.gotoxy(1, 22);
							lw("`r0  `2You sort of don't have that much, friend.  `2(`0hit a key`2)");
							getkey();
							clearrows(22);
						}
						else {
							dk.console.gotoxy(1, 22);
							lw("`r0  `$$"+pretty_int(ch)+" `2 gold transfered! `2(`0hit a key`2)");
							player.money -= ch;
							player_put();
							f = new File(getfname(maildir+'con'+(op.Record + 1)+'.tmp'));
							if (!f.open('ab'))
								throw new Error('Unable to open '+f.name);
							f.write('ADDGOLD|'+ch+'\r\n');
							f.close();
							f = new File(getfname(maildir+'mail'+(op.Record + 1)+'.dat'));
							if (!f.open('ab'))
								throw new Error('Unable to open '+f.name);
							f.write('  `%`r1GOOD NEWS!`r0\r\n`0  '+repeat_chars(ascii(0xc4), 77)+'\r\n  `0'+player.name+' `2gives you `$' + pretty_int(ch) + ' `2 gold!');
							f.write('@DONE\r\n');
							f.close();
							getkey();
							clearrows(22);
						}
					}
					break;
				case 4: // Write Mail
					sclrscr();
					sln('');
					sln('');
					mail_to(op.Record);
					draw_map();
					redraw_bar(true);
					update();
					break;
			}
		}
		players[op.Record].battle = 0;
		players[op.Record].put();
		op.battle = 0;
		op.put(false);
	}
	else {
		op.unLock();
		update_bar('`2Hailing `0'+op.name+'`2... (hit a key to abort)', true);
		hail_cleanup();

		f = new File(getfname(maildir+'tx'+(player.Record + 1)+'.tmp'))
		if (!f.open('ab'))
			throw new Error('Unable to open '+f.name);
		f.write('*\r\n');
		f.close();

		f = new File(getfname(maildir+'con'+(op.Record + 1)+'.tmp'))
		if (!f.open('ab'))
			throw new Error('Unable to open '+f.name);
		f.write('CONNECT|'+(player.Record + 1)+'\r\n');
		f.close();

		while (file_exists(getfname(maildir+'tx'+(player.Record + 1)+'.tmp'))) {
			if (dk.console.waitkey(game.delay)) {
				getkey();
				player.battle = 0;
				update_update();
				player_put();
				if (pending_timeout !== undefined)
					handle_timeout(pending_timeout);
				hail_cleanup();
				return;
			}
		}

		done = false;
		cur = 0;
		while (!done) {
			if (pending_timeout)
				cur = 0;
			else
				cur = hbar(2, 23, ['Leave', 'Attack', 'Give Item', 'Chat'], cur);
			switch(cur) {
				case 0:
					done = true;
					f = new File(getfname(maildir+'giv'+(op.Record + 1)+'.tmp'));
					if (!f.open('ab'))
						throw new Error('Unable to open '+f.name);
					f.write('BYE\r\n');
					f.close();
					break;
				case 1:
					if (map.nofighting) {
						if (world.v[8] === 0 || op.p[8] < world.v[8]) {
							update_bar("No fighting is allowed in this area.", true);
							break;
						}
					}
					online_battle(op, true);
					done = true;
					break;
				case 2:
					give_item(true);
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
						throw new Error('Unable to open '+f.name);

					f.write('CHAT\r\n');
					f.close();
					chat(op);
					draw_map();
					redraw_bar(true);
					done = true;
					break;
			}
		}
	}
	player.battle = 0;
	update_update();
	player_put();
	hail_cleanup();
	if (pending_timeout !== undefined)
		handle_timeout(pending_timeout);
	erase_menu();
	update();

	redraw_bar(true);
}

function do_map()
{
	var ch;
	var al;
	var oldmore;

	if (map === undefined || map === null || map.Record !== world.mapdatindex[player.map - 1] - 1)
		map = load_map(player.map);
	draw_map();
	redraw_bar(true);
	update();

	ch = ''
	while (ch != 'Q') {
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
					throw new Error('Unable to open '+lfile.name);
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
				redraw_bar(true);
				update();
				break;
			case 'D':
				run_ref('readlog', 'logstuff.ref');
				draw_map();
				redraw_bar(true);
				update();
				break;
			case 'F':
				if (!lfile.open('r'))
					throw new Error('Unable to open '+lfile.name);
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
			case 'CONNECTION_CLOSED':
				ch = 'Q';
				break;
			case 'Q':
				dk.console.gotoxy(0, 22);
				lw('`r0`2  Are you sure you want to quit back to the BBS? [`%Y`2] : ');
				do {
					ch = getkey().toUpperCase();
				} while(['Y', 'N', '\r', 'CONNECTION_CLOSED'].indexOf(ch) === -1);
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
				redraw_bar(true);
				update();
				break;
			case 'S':
				show_player_names();
				draw_map();
				redraw_bar(true);
				update();
				break;
			case 'T':
				run_ref('talk', 'help.ref');
				break;
			case 'V':
				if (view_inventory() !== 'ITEMEXIT')
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
				redraw_bar(true);
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
			throw new Error('Unable to open '+f.name);
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
			throw new Error('Unable to open '+f.name);
		state.time = parseInt(f.readln(), 10);
		f.close();
	}

	if (newday || argv.indexOf('/MAINT') !== -1) {
		f = new File(getfname('stime.dat'));
		if (!f.open('r+b'))
			throw new Error('Unable to open '+f.name);
		f.truncate(0);
		f.write(tday+'\r\n');
		f.close;
		state.time++;
		f = new File(getfname('time.dat'));
		if (!f.open('r+b'))
			throw new Error('Unable to open '+f.name);
		f.truncate(0);
		f.write(state.time+'\r\n');
		f.close;
		// TODO: Delete inactive players after 15 days.
		try {
			run_ref('maint', 'maint.ref');
		}
		catch(e) {
			log(LOG_ERROR, (new Date()).toString() + ", Node "+dk.connection.node
				+" "+e.fileName+" line "+e.lineNumber
				+": REF Error "+e.message+" at maint");
		}
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

// TODO: Actually send this font to SyncTERM too.
if (file_exists(getfname('fonts/lord2.fnt'))) {
	if (dk.system.mode === 'local')
		conio.loadfont(getfname('fonts/lord2.fnt'));
}
load_player();
load_time();

var done = false;
for (arg in argv) {
	var m = argv[arg].match(/^(.*)\+(.*)$/);
	if (m != null) {
		run_ref(m[1], m[2]);
		done = true;
	}
}
if (done)
	exit(0);

run_ref('rules', 'rules.ref');
hail_cleanup();

setup_time_warnings();

js.on_exit('killfiles.forEach(function(f) { if (f.is_open) { f.close(); } file_remove(f.name); });');

var tfile = new File(getfname(maildir + 'talk'+(player.Record + 1)+'.tmp'));
if (!tfile.open('w+b'))
	throw new Error('Unable to open '+tfile.name);
killfiles.push(tfile);
tfile.close();

var lfile = new File(getfname(maildir + 'log'+(player.Record + 1)+'.tmp'));
if (!lfile.open('w+b'))
	throw new Error('Unable to open '+lfile.name);
killfiles.push(lfile);
lfile.close();

var cfile = new File(getfname(maildir + 'con'+(player.Record + 1)+'.tmp'));
if (!cfile.open('ab'))
	throw new Error('Unable to open '+cfile.name);
killfiles.push(cfile);
cfile.close();

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

if (player.battle) {
	if (player.lastsaved < (savetime() - (idle_timeout / 60 * 2))) {
		player.battle = false;
		player_put();
	}
	else
		run_ref('busy', 'gametxt.ref');
}

run_ref('startgame', 'gametxt.ref');

js.on_exit('if (player !== undefined) { update_rec.onnow = 0; update_rec.busy = 0; update_rec.battle = 0; update_rec.map = player.map; update_rec.x = player.x; update_rec.y = player.y; update_rec.put(); ufile.close(); player.onnow = 0; player.busy = 0; player.battle = 0; player.lastsaved = savetime(); player.put(); pfile.close() }');
players[player.Record] = update_rec;
player.onnow = 1;
player.busy = 0;
player.battle = 0;
if (pending_timeout !== undefined)
	handle_timeout(pending_timeout);
player.lastdayon = state.time;
player.lastdayplayed = state.time;
player_put();

mail_check(false);
do_map();
