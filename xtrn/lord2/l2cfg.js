'use strict';

js.load_path_list.unshift(js.exec_dir+"dorkit/");
load("dorkit.js", "-l");
require("l2lib.js", "Player_Def");

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

		function edit_map(mapnum) {
			map = load_map(mapnum + 1);

			if (map === null || map === undefined)
				return;

			draw_map();
			getkey();
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
							break;
						}
						emap = f.new();
						if (emap === null || emap === undefined) {
							dk.console.gotoxy(2, 23);
							lw('`%Error creating new record in file. `2(press a key)');
							dk.console.cleareol();
							getkey();
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
		dk.console.gotoxy(0, 23);
		lln('  `0Saving changes.  Thanks for using this product.');
		game.put();
		world.put();
		return true
	}}
]);
