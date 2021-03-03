'use strict';

js.load_path_list.unshift(js.exec_dir+"dorkit/");
load("dorkit.js", "-l");
require("l2lib.js", "Player_Def");

var copied;
var save = {map:{}};

function menu(title, blank_line, opts, cur)
{
	var legal = '';
	var i;
	var ret;
	var y;
	var idx;
	var done = false;
	if (cur === undefined)
		cur = 0;
	var mctx = {
		title:title,
		blank_line:blank_line,
		opts:opts,
		cur:cur
	};

	function draw_menu() {
		sclrscr();
		lw(mctx.title);
		dk.console.cleareol();
		sln('');
		if (mctx.blank_line)
			sln('');
		y = scr.pos.y;
		conio.setcursortype(0);
		for (i = 0; i < mctx.opts.length; i++) {
			legal += mctx.opts[i].shortcut[0].toUpperCase();
			lw(mctx.opts[i].text);
			dk.console.cleareol();
			sln('');
		}
	}

	draw_menu();
	while(!done) {
		dk.console.attr.value = 2;
		dk.console.gotoxy(0, y + mctx.cur);
		lw(ascii(251));
		ret = getkey().toUpperCase();
		dk.console.gotoxy(0, y + mctx.cur);
		lw(' ');
		switch(ret) {
			case 'KEY_UP':
			case '8':
				mctx.cur--;
				if (mctx.cur < 0)
					mctx.cur = mctx.opts.length - 1;
				break;
			case 'KEY_DOWN':
			case '2':
				mctx.cur++;
				if (mctx.cur >= mctx.opts.length)
					mctx.cur = 0;
				break;
			case 'KEY_HOME':
			case '7':
				mctx.cur = 0;
				break;
			case 'KEY_END':
			case '1':
				mctx.cur = mctx.opts.length - 1;
				break;
			default:
				idx = legal.indexOf(ret);
				if (idx === -1)
					break;
				mctx.cur = idx;
				// Fall-through
			case '\r':
				done = mctx.opts[mctx.cur].callback(mctx);
				if (!done)
					draw_menu();
				break;
		}
	}
	conio.setcursortype(2);
	return ret;
}

dk.console.clear();
menu('`r0`c                 `r1   `%LORD II: CONFIGURE JS   `r0', true, [
	{text:'  `2(`%G`2)ame Options', shortcut:'G', callback:function() {
		sclrscr();
		sln('');
		sln('');
		// Really only needs to be saved on change, but whatever
		save.game = game;
		menu('`r0`c  `r1`%LORD II OPTION EDITOR`r0', true, [
			{text:'  `2(`0A`2) Path to favorite text editor: `0' + game.editor, shortcut:'A', callback:function(ctx) {
				dk.console.gotoxy(36, 4);
				game.editor = dk.console.getstr({edit:game.editor, crlf:false, input_box:true, attr:new Attribute(31), sel_attr:new Attribute(112), len:40});
				ctx.opts[ctx.cur].text = '  `2(`0A`2) Path to favorite text editor: `0' + game.editor;
			}},
			{text:'  `2(`0B`2) Milliseconds between each poll: `0' + game.delay, shortcut:'B', callback:function(ctx) {
				dk.console.gotoxy(38, 5);
				game.delay = parseInt(dk.console.getstr({edit:game.delay.toString(), crlf:false, input_box:true, attr:new Attribute(31), sel_attr:new Attribute(112), len:7, max:999999, min:1, integer:true}), 10);
				ctx.opts[ctx.cur].text = '  `2(`0B`2) Milliseconds between each poll: `0' + game.delay;
			}},
			{text:'  `2(`0D`2) Days of inactivity until deletion: `0' + game.deldays, shortcut:'C', callback:function(ctx) {
				dk.console.gotoxy(41, 6);
				game.deldays = parseInt(dk.console.getstr({edit:game.deldays.toString(), crlf:false, input_box:true, attr:new Attribute(31), sel_attr:new Attribute(112), len:7, max:999999, min:1, integer:true}), 10);
				ctx.opts[ctx.cur].text = '  `2(`0D`2) Days of inactivity until deletion: `0' + game.deldays;
			}},
			{text:'  `2(`0E`2) Buffer extra keystrokes? (can make movement faster): `0' + (game.buffer ? '`%Yes' : '`4No'), shortcut:'E', callback:function(ctx) {
				if (game.buffer)
					game.buffer = 0;
				else
					game.buffer = 1;
				ctx.opts[ctx.cur].text = '  `2(`0E`2) Buffer extra keystrokes? (can make movement faster): `0' + (game.buffer ? '`%Yes' : '`4No');
			}},
			{text:'  `2(`0Q`2) Quit', shortcut:'Q', callback:function() {return true}}
		]);
		return false;
	}},
	{text:'  `2(`%W`2)orld Editor', shortcut:'W', callback:function() {
		var ch;
		var x = 0;
		var y = 0;
		var block = y * 80 + x;
		var redraw = 1;
		var showname = false;
		var tmap;
		var emap;
		var mname;
		var fname;
		var f;

		// Really only needs to be done on change, but whatever.
		save.world = world;

		function edit_map(mapnum) {
			var ch;
			var x = 0;
			var y = 0;
			var last_cursor;
			var redraw = true;
			var textentry = false;
			var show_hot = false;
			var tmp;
			var mi;
			var hs;
			var i;
			var mapindex;

			function draw_cursor() {
				if (last_cursor !== undefined) {
					if (last_cursor.y > 0)
						erase(last_cursor.x, last_cursor.y - 1, show_hot ? 'HOT' : 'NORMIE');
					erase(last_cursor.x, last_cursor.y, show_hot ? 'HOT' : 'NORMIE');
				}
				if (y > 0) {
					dk.console.gotoxy(x, y-1);
					lw('`r2`%'+ascii(25));
				}
				dk.console.gotoxy(x, y);
				conio.setcursortype(1);
				last_cursor = {x:x, y:y};
			}

			function typestr() {
				switch(map.mapinfo[getoffset(x, y)].terrain) {
					case 0:
						return 'Unpassable';
					case 1:
						return 'Grass';
					case 2:
						return 'Rocky';
					case 3:
						return 'Water';
					case 4:
						return 'Ocean';
					case 5:
						return 'Forest';
				}
			}

			function update_status() {
				dk.console.gotoxy(0, 20);
				dk.console.attr.value = 2;
				dk.console.cleareol();
				lw(space_pad('`r1`2X: `%'+(x+1)+' `2Y: `%'+(y+1), 14) + space_pad('`2Type: `%'+typestr(), 25) + '`r0');
			}

			mapindex = world.mapdatindex[mapnum - 1];
			if (save.map[mapindex] !== undefined) {
				map = save.map[mapindex];
			}
			else {
				// Only needs to be saved on change, but whatever.
				map = load_map(mapnum + 1);
				save.map[map.Record + 1] = map;
			}

			if (map === null || map === undefined)
				return;

			while(1) {
				if (redraw) {
					draw_map(show_hot ? 'HOT' : 'NORMIE');
					dk.console.gotoxy(0, 23);
					lw('`r0  `2Map editor - ? for help');
					redraw = false;
				}
				else {
					erase(x, y, show_hot ? 'HOT' : 'NORMIE');
				}
				update_status();
				draw_cursor();
				ch = getkey();
				conio.setcursortype(0);
				if (textentry) {
					switch(ch) {
						case 'KEY_UP':
							if (--y < 0)
								y = 19;
							break;
						case 'KEY_DOWN':
							if (++y >= 20)
								y = 0;
							break;
						case 'KEY_LEFT':
							if (--x < 0)
								x = 79;
							break;
						case 'KEY_RIGHT':
							if (++x >= 80)
								x = 0;
							break;
						case 'KEY_ALT_1':	// Set background...
							map.mapinfo[getoffset(x, y)].backcolour = 0;	// Confirmed.
							break;
						case 'KEY_ALT_2':
							map.mapinfo[getoffset(x, y)].backcolour = 2;
							break;
						case 'KEY_ALT_3':
							map.mapinfo[getoffset(x, y)].backcolour = 3;
							break;
						case 'KEY_ALT_4':
							map.mapinfo[getoffset(x, y)].backcolour = 4;
							break;
						case 'KEY_ALT_5':
							map.mapinfo[getoffset(x, y)].backcolour = 5;
							break;
						case 'KEY_ALT_6':
							map.mapinfo[getoffset(x, y)].backcolour = 6;
							break;
						case 'KEY_ALT_7':
							map.mapinfo[getoffset(x, y)].backcolour = 7;
							break;
						case '\x1b':
							dk.console.gotoxy(0, 22);
							dk.console.attr.value = 2;
							dk.console.cleareol();
							textentry = false;
							break;
						default:
							if (ch.length === 1 && ascii(ch) > 32) {
								map.mapinfo[getoffset(x, y)].ch = ch;
								if (++x >= 80) {
									x = 0;
									if (++y >= 20)
										y = 0;
								}
							}
							break;
					}
				}
				else {
					switch(ch) {
						case '8':
						case 'KEY_UP':
							if (--y < 0)
								y = 19;
							break;
						case '2':
						case 'KEY_DOWN':
							if (++y >= 20)
								y = 0;
							break;
						case '4':
						case 'KEY_LEFT':
							if (--x < 0)
								x = 79;
							break;
						case '6':
						case 'KEY_RIGHT':
							if (++x >= 80)
								x = 0;
							break;
						case 'q':
							return;
						case '?':
							// TODO: There's some sort of drop-shadow here...
							draw_box(1, ' HELP SCREEN ', [
								'',
								'',
								' `%g = Grass, r = Rocky, w = Water, f = forest',
								' `%<Escape> = text entry mode toggle, z = Edit general screen info',
								' `%m = Create/edit a Hot Spot (run ref file, or warp)',
								' `%L = Load a screen or import from another map.dat file',
								' `%c = put block in buffer, s = copy buffer to block',
								' `%S = stamp block in buffer over ENTIRE screen <- be carefull!',
								' `%<Space> = Show hotspots, e = edit .REF file associated with spot',
								' `%1,2,3,4,5,6,7,8,9,0,!,@,#,$,% = Change foreground color',
								' `%Alt-1 through 7 =  Change background color',
								' `%h - Makes a block \'hard\'.. n - Makes a block \'Not hard\'.',
								' `%b - Blink toggle, H - Show all HARD spots',
								''], 72, 0, '  `%Commands ARE case sensitive.');
							getkey();
							redraw = true;
							break;
						case 'g':	// Grass
							map.mapinfo[getoffset(x, y)].terrain = 1;
							map.mapinfo[getoffset(x, y)].ch = ' ';
							map.mapinfo[getoffset(x, y)].forecolour = 15;
							map.mapinfo[getoffset(x, y)].backcolour = 2;
							break;
						case 'r':	// Rocky
							map.mapinfo[getoffset(x, y)].terrain = 2;
							map.mapinfo[getoffset(x, y)].ch = ascii(239);
							map.mapinfo[getoffset(x, y)].forecolour = 8;
							map.mapinfo[getoffset(x, y)].backcolour = 2;
							break;
						case 'w':	// Water
							map.mapinfo[getoffset(x, y)].terrain = 3;
							map.mapinfo[getoffset(x, y)].ch = ascii(219);
							map.mapinfo[getoffset(x, y)].forecolour = 9;
							map.mapinfo[getoffset(x, y)].backcolour = 0;
							break;
						case 'f':	// Forest
							map.mapinfo[getoffset(x, y)].terrain = 5;
							map.mapinfo[getoffset(x, y)].ch = ascii(6);
							map.mapinfo[getoffset(x, y)].forecolour = 10;
							map.mapinfo[getoffset(x, y)].backcolour = 2;
							break;
						case '\x1b':	// text entry mode
							dk.console.gotoxy(0, 22);
							// TODO: The ALT thing likely doesn't work.
							lw('`r1  `%Text entry mode - type of hold down Alt and enter ascii value`r0');
							dk.console.cleareol();
							textentry = true;
							break;
						case 'z':	// General screen info
							tmp = draw_box(2, '`r1`0 General Info ', [
								'',
								'`2Screen Name: `%'+map.name,
								'`2Map monster/random ref file: `%'+map.reffile,
								'`2Map monster/random ref name: `%'+map.refsection,
								'`2Chances of running this ref are 1 in `%'+pretty_int(map.battleodds),
								'`2Is player fighting allowed on this screen? :`%'+(map.nofighting ? 'N' : 'Y'),
								'',
								'',
								'',
								''], 75, 4);
							dk.console.gotoxy(tmp.x + 16, tmp.y + 2);
							map.name = dk.console.getstr({edit:map.name, crlf:false, input_box:true, attr:new Attribute(95), sel_attr:new Attribute(112), len:30});
							dk.console.gotoxy(tmp.x + 16, tmp.y + 2);
							lw(space_pad('`r1`%'+map.name, 30));

							dk.console.gotoxy(tmp.x + 32, tmp.y + 3);
							map.reffile = dk.console.getstr({edit:map.reffile, crlf:false, input_box:true, attr:new Attribute(95), sel_attr:new Attribute(112), len:12});
							dk.console.gotoxy(tmp.x + 32, tmp.y + 3);
							lw(space_pad('`r1`%'+map.reffile, 12));

							dk.console.gotoxy(tmp.x + 32, tmp.y + 4);
							map.refsection = dk.console.getstr({edit:map.refsection, crlf:false, input_box:true, attr:new Attribute(95), sel_attr:new Attribute(112), len:12});
							dk.console.gotoxy(tmp.x + 32, tmp.y + 4);
							lw(space_pad('`r1`%'+map.reffile, 12));

							dk.console.gotoxy(tmp.x + 40, tmp.y + 5);
							map.battleodds = parseInt(dk.console.getstr({edit:map.battleodds, crlf:false, input_box:true, attr:new Attribute(95), sel_attr:new Attribute(112), len:10, integer:true, min:0, max:2147483647}), 10);
							dk.console.gotoxy(tmp.x + 40, tmp.y + 5);
							lw(space_pad('`r1`%'+pretty_int(map.battleodds), 15));

							dk.console.gotoxy(tmp.x + 47, tmp.y + 6);
							map.nofighting = dk.console.getstr({edit:map.nofighting ? 'N' : 'Y', crlf:false, input_box:true, attr:new Attribute(95), sel_attr:new Attribute(112), len:1, integer:true, min:0, max:2147483647}).toUpperCase() !== 'Y';
							dk.console.gotoxy(tmp.x + 47, tmp.y + 6);
							lw(space_pad('`r1`%'+(map.nofighting ? 'N' : 'Y'), 2));
							redraw = true;
							break;
						case 'm':	// Create/edit Hot Spot
							hs = undefined;
							for (i = 0; i < 10; i++) {
								if (map.hotspots[i].hotspotx === (x + 1) && map.hotspots[i].hotspoty === (y + 1)) {
									hs = map.hotspots[i];
									break;
								}
							}
							if (hs === undefined) {
								for (i = 0; i < 10; i++) {
									if (map.hotspots[i].hotspotx === 0 && map.hotspots[i].hotspoty === 0) {
										hs = map.hotspots[i];
										break;
									}
								}
							}
							if (hs === undefined) {
								draw_box(2, '`r1`0 A SLIGHT PROBLEM ', [
									'',
									'`%You have already defined 10 hotspots, this is the max.',
									'',
									'You can DELETE hotspots by entering 0 for the X & Y.',
									'',
									''], 67, 4);
								getkey();
								redraw = true;
								break;
							}
							hs.hotspotx = x + 1;
							hs.hotspoty = y + 1;
							tmp = draw_box(2, ' `r1`0Hot Spot editing ', [
								'',
								space_pad('`%X: '+hs.hotspotx, 9)+space_pad('Y: '+hs.hotspoty, 9)+'`0<- Set these two to 0 to delete hotspot',
								'`0If the params below are set to anything but 0, this hot spot will warp',
								space_pad('`%Map #: '+hs.warptomap, 13)+space_pad('X: '+hs.warptox, 9)+'Y: '+hs.warptoy,
								'`0If the above are 0\'s, the hotspot will run the .ref file below.',
								'`%File to read from: '+space_pad(hs.reffile, 16)+'Ref name: '+hs.refsection,
								'',
								'',
								'',
								''], 75, 4);

							dk.console.gotoxy(tmp.x + 6, tmp.y + 2);
							hs.hotspotx = parseInt(dk.console.getstr({edit:hs.hotspotx.toString(), crlf:false, input_box:true, attr:new Attribute(95), sel_attr:new Attribute(112), len:2, integer:true, min:0, max:80}), 10);
							dk.console.gotoxy(tmp.x + 6, tmp.y + 2);
							lw(space_pad('`r1`%'+hs.hotspotx, 3));

							dk.console.gotoxy(tmp.x + 15, tmp.y + 2);
							hs.hotspoty = parseInt(dk.console.getstr({edit:hs.hotspoty.toString(), crlf:false, input_box:true, attr:new Attribute(95), sel_attr:new Attribute(112), len:2, integer:true, min:0, max:80}), 10);
							dk.console.gotoxy(tmp.x + 15, tmp.y + 2);
							lw(space_pad('`r1`%'+hs.hotspoty, 3));

							dk.console.gotoxy(tmp.x + 10, tmp.y + 4);
							hs.warptomap = parseInt(dk.console.getstr({edit:hs.warptomap.toString(), crlf:false, input_box:true, attr:new Attribute(95), sel_attr:new Attribute(112), len:4, integer:true, min:0, max:1600}), 10);
							dk.console.gotoxy(tmp.x + 10, tmp.y + 4);
							lw(space_pad('`r1`%'+hs.warptomap, 5));

							dk.console.gotoxy(tmp.x + 19, tmp.y + 4);
							hs.warptox = parseInt(dk.console.getstr({edit:hs.warptox.toString(), crlf:false, input_box:true, attr:new Attribute(95), sel_attr:new Attribute(112), len:2, integer:true, min:0, max:80}), 10);
							dk.console.gotoxy(tmp.x + 19, tmp.y + 4);
							lw(space_pad('`r1`%'+hs.warptox, 3));

							dk.console.gotoxy(tmp.x + 28, tmp.y + 4);
							hs.warptoy = parseInt(dk.console.getstr({edit:hs.warptoy.toString(), crlf:false, input_box:true, attr:new Attribute(95), sel_attr:new Attribute(112), len:2, integer:true, min:0, max:80}), 10);
							dk.console.gotoxy(tmp.x + 28, tmp.y + 4);
							lw(space_pad('`r1`%'+hs.warptoy, 3));

							dk.console.gotoxy(tmp.x + 22, tmp.y + 6);
							hs.reffile = dk.console.getstr({edit:hs.reffile, crlf:false, input_box:true, attr:new Attribute(95), sel_attr:new Attribute(112), len:12});
							dk.console.gotoxy(tmp.x + 22, tmp.y + 6);
							lw(space_pad('`r1`%'+hs.reffile, 13));

							dk.console.gotoxy(tmp.x + 48, tmp.y + 6);
							hs.refsection = dk.console.getstr({edit:hs.refsection, crlf:false, input_box:true, attr:new Attribute(95), sel_attr:new Attribute(112), len:12});
							dk.console.gotoxy(tmp.x + 48, tmp.y + 6);
							lw(space_pad('`r1`%'+hs.refsection, 13));

							redraw = true;
							break;
						case 'e':	// Edit .REF file... hah!
							break;
						case 'L':	// Load from map.dat file
							dk.console.gotoxy(0, 23);
							redraw = true;
							lw('  `2Load map record from what file ? ');
							ch = dk.console.getstr({edit:'map.dat', crlf:false, input_box:true, attr:new Attribute(31), sel_attr:new Attribute(112), len:40});
							tmp = new RecordFile(ch, Map_Def);
							if (tmp === null || tmp === undefined)
								break;
							dk.console.gotoxy(0, 23);
							dk.console.attr.value = 2;
							dk.console.cleareol();
							lw('`r0`2  Ok, fine - what is the physical record # of the map to load? ');
							ch = parseInt(dk.console.getstr({edit:'0', crlf:false, input_box:true, attr:new Attribute(31), sel_attr:new Attribute(112), len:6, integer:true, min:0, max:99999}), 10);
							if (isNaN(ch) || ch < 1 || ch > tmp.length) {
								tmp.close();
								break;
							}
							ch = tmp.get(ch - 1);
							if (ch === undefined || ch === null) {
								tmp.close();
								break;
							}
							Map_Def.forEach(function(prop) {
								map[prop.prop] = ch[prop.prop];
							});
							ch = undefined;
							tmp.close();
							break;
						case 'H':	// Show all HARD spots
							draw_map('HARD');
							dk.console.gotoxy(0, 23);
							lw('`r0`2  Ok, so these are the blocks you cannot pass.  Hit a key.');
							getkey();
							redraw = true;
							break;
						case ' ':	// Show hotspots
							show_hot = !show_hot;
							for (i = 0; i < 10; i++) {
								if (map.hotspots[i].hotspotx !== 0 && map.hotspots[i].hotspoty !== 0) {
									erase(map.hotspots[i].hotspotx - 1, map.hotspots[i].hotspoty - 1, show_hot ? 'HOT' : 'NORMIE');
								}
							}
							break;
						case 'c':	// copy
							mi = map.mapinfo[getoffset(x, y)];
							copied = {forecolour:mi.forecolour, backcolour:mi.backcolour, ch:mi.ch, t:mi.t, terrain:mi.terrain};
							mi = undefined;
							break;
						case 's':	// stamp
							if (copied !== undefined) {
								mi = map.mapinfo[getoffset(x, y)];
								mi.forecolour = copied.forecolour;
								mi.backcolour = copied.backcolour;
								mi.ch = copied.ch;
								mi.t = copied.t;
								mi.terrain = copied.terrain;
								mi = undefined;
							}
							break;
						case 'S':	// stamp screen
							dk.console.gotoxy(0, 22);
							if (copied === undefined)
								break;
							lw('`r1`%You sure? `r0');
							if (getkey().toUpperCase() === 'Y') {
								map.mapinfo.forEach(function(cell) {
									cell.forecolour = copied.forecolour;
									cell.backcolour = copied.backcolour;
									cell.ch = copied.ch;
									cell.t = copied.t;
									cell.terrain = copied.terrain;
								});
								redraw = true;
							}
							dk.console.gotoxy(0, 22);
							dk.console.attr.value = 2;
							dk.console.cleareol();
							break;
						case '1':	// Set foreground...
							map.mapinfo[getoffset(x, y)].forecolour = 1;
							break;
						case '2':
							map.mapinfo[getoffset(x, y)].forecolour = 2;
							break;
						case '3':
							map.mapinfo[getoffset(x, y)].forecolour = 3;
							break;
						case '4':
							map.mapinfo[getoffset(x, y)].forecolour = 4;
							break;
						case '5':
							map.mapinfo[getoffset(x, y)].forecolour = 5;
							break;
						case '6':
							map.mapinfo[getoffset(x, y)].forecolour = 6;
							break;
						case '7':
							map.mapinfo[getoffset(x, y)].forecolour = 7;
							break;
						case '8':
							map.mapinfo[getoffset(x, y)].forecolour = 8;
							break;
						case '9':
							map.mapinfo[getoffset(x, y)].forecolour = 9;
							break;
						case '0':
							map.mapinfo[getoffset(x, y)].forecolour = 10;
							break;
						case '!':
							map.mapinfo[getoffset(x, y)].forecolour = 11;
							break;
						case '@':
							map.mapinfo[getoffset(x, y)].forecolour = 12;
							break;
						case '#':
							map.mapinfo[getoffset(x, y)].forecolour = 13;
							break;
						case '$':
							map.mapinfo[getoffset(x, y)].forecolour = 14;
							break;
						case '%':
							map.mapinfo[getoffset(x, y)].forecolour = 15;
							break;
						case 'KEY_ALT_1':	// Set background...
							map.mapinfo[getoffset(x, y)].backcolour = 0;	// Confirmed.
							break;
						case 'KEY_ALT_2':
							map.mapinfo[getoffset(x, y)].backcolour = 2;
							break;
						case 'KEY_ALT_3':
							map.mapinfo[getoffset(x, y)].backcolour = 3;
							break;
						case 'KEY_ALT_4':
							map.mapinfo[getoffset(x, y)].backcolour = 4;
							break;
						case 'KEY_ALT_5':
							map.mapinfo[getoffset(x, y)].backcolour = 5;
							break;
						case 'KEY_ALT_6':
							map.mapinfo[getoffset(x, y)].backcolour = 6;
							break;
						case 'KEY_ALT_7':
							map.mapinfo[getoffset(x, y)].backcolour = 7;
							break;
						case 'h':	// Make block hard (Unpassable)
							map.mapinfo[getoffset(x, y)].terrain = 0;
							break;
						case 'n':	// Not hard (Grass)
							map.mapinfo[getoffset(x, y)].terrain = 1;
							break;
						case 'b':	// Toggle blink
							map.mapinfo[getoffset(x, y)].forecolour ^= 16;
							break;
					}
				}
			}
		}

		function update_details() {
			dk.console.gotoxy(0, 20);
			lw('`r0`2X:`0'+(x+1)+' `2Y:`0'+(y+1)+'  `2Block: `%'+pretty_int(block+1)+' `2Map: `%'+world.mapdatindex[block]+'  `$(`%E`$)xtract');
			dk.console.cleareol();
			if (showname) {
				mname = 'Unused';
				if (world.mapdatindex[block] > 0) {
					tmap = load_map(block + 1);
					mname = tmap.name;
				}
				dk.console.gotoxy(2, 22);
				lw('`2Map name : `0'+mname);
				dk.console.cleareol();
			}
			dk.console.gotoxy(0, 23);
			lw('`r0  `$Enter to edit/create map. (`%F`$)orce assign. (`%T`$)og visibility. (`%S`$)how Name `%Q`$uit.');
			dk.console.gotoxy(x, y);
			conio.setcursortype(1);
		}

		function draw_screen() {
			overheadmap(true);
		}

		while(1) {
			block = y * 80 + x;
			if (redraw)
				draw_screen();
			update_details();
			redraw = false;
			ch = getkey().toUpperCase();
			conio.setcursortype(0);
			switch(ch) {
				case 'KEY_UP':
				case '8':
					if (--y < 0)
						y = 19;
					break;
				case 'KEY_DOWN':
				case '2':
					if (++y >= 20)
						y = 0;
					break;
				case 'KEY_LEFT':
				case '4':
					if (--x < 0)
						x = 79;
					break;
				case 'KEY_RIGHT':
				case '6':
					if (++x >= 80)
						x = 0;
					break;
				case '\r':
					if (world.mapdatindex[block] < 1) {
						tmap = mfile.new();
						world.mapdatindex[block] = tmap.Record + 1;
					}
					edit_map(block);
					redraw = true;
					break;
				case 'E':
					if (world.mapdatindex[block] < 1) {
						dk.console.gotoxy(2, 23);
						lw('`%No map to export at this location. `2(press a key)');
						dk.console.cleareol();
						getkey();
						break;
					}
					dk.console.gotoxy(2, 23);
					lw('`2`r0Save/append this map to what file? ');
					dk.console.cleareol();
					fname = dk.console.getstr({edit:'crap.dat', crlf:false, input_box:true, attr:new Attribute(31), sel_addr:new Attribute(112), len:40});
					f = new RecordFile(fname, Map_Def);
					if (f === null) {
						dk.console.gotoxy(2, 23);
						lw('`%Unable to open `0'+fname+' `2(press a key)');
						dk.console.cleareol();
						getkey();
						break;
					}
					else {
						tmap = load_map(block + 1);
						if (tmap === null) {
							dk.console.gotoxy(2, 23);
							lw('`%No map to export at this location. `2(press a key)');
							dk.console.cleareol();
							getkey();
							f.close();
							break;
						}
						emap = f.new();
						if (emap === null || emap === undefined) {
							dk.console.gotoxy(2, 23);
							lw('`%Error creating new record in file. `2(press a key)');
							dk.console.cleareol();
							getkey();
							f.close();
							break;
						}
						Map_Def.forEach(function(prop) {
							emap[prop.prop] = tmap[prop.prop];
						});
						emap.put();
						dk.console.gotoxy(2, 23);
						lw('`%MAP ADDED AS RECORD '+f.length+' `2(press a key)');
						dk.console.cleareol();
						getkey();
						f.close();
					}
					break;
				case 'F':
					dk.console.gotoxy(26, 20);
					lw('`0Force block to reference physical map record ');
					world.mapdatindex[block] = parseInt(dk.console.getstr({integer:true, edit:world.mapdatindex[block].toString(), crlf:false, input_box:true, attr:new Attribute(95), sel_attr:new Attribute(112), len:6, min:0, max:1600}), 10);
					redraw = true;
					break;
				case 'T':
					world.hideonmap[block] = !world.hideonmap[block];
					redraw = true;
					break;
				case 'S':
					showname = !showname;
					redraw = true;
					break;
				case 'Q':
					return false;
			}
		}
	}},
	{text:'  `2(`%U`2)se Player Editor', shortcut:'U', callback:function() {}},
	{text:'  `2(`%I`2)tem Editor', shortcut:'I', callback:function() {}},
	{text:'  `2(`%R`2)eset Game', shortcut:'R', callback:function() {}},
	{text:'  `2(`4Q`2)uit & Save', shortcut:'Q', callback:function() {
		var tmp;

		dk.console.gotoxy(0, 23);
		lln('  `0Saving changes.  Thanks for using this product.');
		if (save.game !== undefined)
			save.game.put();
		if (save.world !== undefined)
			save.world.put();
		for (tmp in save.map) {
			save.map[tmp].put();
		}
		
		return true
	}}
]);
