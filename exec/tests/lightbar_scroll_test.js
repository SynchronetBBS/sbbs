// Run from exec with SBBSCTRL set for your installation:
// jsexec -n -C tests/lightbar_scroll_test.js
// Exercise the actual library and lbshell constructors in Synchronet JS with
// simulated terminal output/input, without starting a BBS session or a door.
require('sbbsdefs.js', 'SYS_CLOSED');
require('mouse_getkey.js', 'mouse_getkey');

function check(condition, message)
{
	if(!condition)
		throw new Error(message);
}

function equal(actual, expected, message)
{
	check(actual===expected, message+': got '+actual+', expected '+expected);
}

function source(filename)
{
	var file=new File(js.exec_dir+'../'+filename);
	check(file.open('r'), 'Open '+file.name);
	var text=file.read();
	file.close();
	return text;
}

var library=source('load/lightbar.js');
var shell=source('lbshell.js');
// Compile the complete shell as well as exercising its menu functions below.
new Function(shell);
var shell_lb=shell.slice(shell.indexOf('function ShellLB()'), shell.indexOf('function Mainbar()'));
var message_handler=shell.slice(shell.indexOf('function message_callback()'), shell.indexOf('function format_opt('));
var external_menus=shell.slice(shell.indexOf('function setup_xtrn_menu('), shell.indexOf('function Infomenu()'));

function environment(rows)
{
	var screen=[];
	var inputs=[];
	var cleared=[];
	var x=1;
	var y=1;
	var row;
	for(row=0; row<rows; row++)
		screen.push(new Array(81).join(' ').split(''));
	var console={
		screen_rows:rows,
		screen_columns:80,
		term_supports:function() { return true; },
		beep:function() {},
		gotoxy:function(newx, newy) {
			check(newx>=1 && newx<=80 && newy>=1 && newy<=rows,
				'Cursor outside screen: '+newx+','+newy);
			x=newx;
			y=newy;
		},
		write:function(text) {
			for(var i=0; i<text.length; i++) {
				check(x<=80, 'Write beyond right edge');
				screen[y-1][x++-1]=text[i];
			}
		}
	};
	var bbs={online:true};
	var bars80=new Array(81).join('\xc4');
	var xtrn_area;
	var msg_rows=0;
	var msg_timeouts=[];
	var MessageTimeout=50;
	var hangup_now=false;
	function alert(text) { throw new Error(text); }
	function log() {}
	function get_message() { return 1; }
	function cleararea() { cleared.push(Array.prototype.slice.call(arguments)); }
	function mouse_getkey() {
		check(inputs.length>0, 'Menu requested unexpected extra input');
		var input=inputs.shift();
		return typeof input==='string' ? {key:input, mouse:null} : {key:'', mouse:input};
	}
	// Direct eval keeps the production functions in this simulated session's
	// scope. All code is parsed/executed by Synchronet's JavaScript engine.
	eval(library);
	eval(message_handler);
	eval(shell_lb);
	eval(external_menus);
	return {
		Lightbar:Lightbar, ShellLB:ShellLB, Xtrnsec:Xtrnsec, Xtrnsecs:Xtrnsecs,
		screen:screen, inputs:inputs, cleared:cleared,
		set_sections:function(sections) { xtrn_area={sec_list:sections}; }
	};
}

function boxed(env, count)
{
	var menu=new env.Lightbar();
	menu.xpos=3;
	menu.ypos=2;
	menu.max_height=6;
	menu.fixed_top=menu.fixed_bottom=1;
	menu.add('TOP');
	for(var i=0; i<count; i++)
		menu.add('Entry '+i, i, 10);
	menu.add('BOTTOM');
	return menu;
}

function click(x, y)
{
	return {x:x, y:y, button:0, mods:0, motion:0, press:true, ansi:'mouse'};
}

function programs(count, filter)
{
	var list=[];
	for(var i=0; i<count; i++)
		list.push({name:'Program '+i, number:100+i, can_access:!filter || filter(i)});
	return list;
}

function test(name, fn)
{
	fn();
	print('Passed: '+name);
}

test('arrows scroll and wrap while borders stay fixed', function() {
	var env=environment(12);
	var menu=boxed(env, 30);
	menu.draw();
	equal(menu.current, 1, 'Initial selection');
	for(var i=0; i<6; i++)
		menu.getval(undefined, KEY_DOWN);
	equal(menu.items[menu.current].retval, 6, 'Selection after scrolling');
	equal(menu.get_item_row(menu.current), 4, 'Selection stays visible');
	equal(menu.get_item_row(1), -1, 'First entry is hidden');
	equal(env.screen[1].slice(2, 5).join(''), 'TOP', 'Top border stays fixed');
	equal(env.screen[6].slice(2, 8).join(''), 'BOTTOM', 'Bottom border stays fixed');
	equal(menu.mouse_hit(3, 3), 4, 'Mouse maps to scrolled entry');
	equal(menu.mouse_hit(3, 8), -1, 'Mouse outside visible menu');
	menu.getval(undefined, KEY_HOME);
	menu.getval(undefined, KEY_UP);
	equal(menu.items[menu.current].retval, 29, 'Up wraps to last entry');
	menu.getval(undefined, KEY_DOWN);
	equal(menu.items[menu.current].retval, 0, 'Down wraps to first entry');
	equal(menu.scroll_offset, 0, 'Viewport returns to beginning');
});

test('paging and Home/End skip disabled and decorative entries', function() {
	var env=environment(12);
	var menu=boxed(env, 12);
	menu.items[5].disabled=true;
	menu.getval(undefined, KEY_PAGEDN);
	equal(menu.current, 6, 'Page Down skips disabled target');
	equal(menu.get_item_row(6), 2, 'Page Down advances the whole viewport');
	menu.getval(undefined, KEY_PAGEUP);
	equal(menu.current, 2, 'Page Up');
	equal(menu.get_item_row(2), 2, 'Page Up returns to first viewport');
	menu.getval(undefined, KEY_PAGEUP);
	equal(menu.current, 1, 'Page Up stops at beginning');
	menu.getval(undefined, KEY_END);
	equal(menu.current, 12, 'End selects last entry');
	equal(menu.get_item_row(12), 4, 'End scrolls selection into view');
	menu.getval(undefined, KEY_PAGEDN);
	equal(menu.current, 12, 'Page Down stops at end');
	menu.getval(undefined, KEY_HOME);
	equal(menu.current, 1, 'Home returns to first entry');
});

test('mouse selection ignores borders and disabled entries', function() {
	var env=environment(12);
	var menu=boxed(env, 12);
	menu.getval(undefined, KEY_END);
	menu.items[9].disabled=true;
	menu.mouse_miss_key='\b';
	env.inputs.push(click(3, 2), click(3, 3), click(3, 4));
	equal(menu.getval(), 9, 'Click selects original return value');
	equal(menu.current, 10, 'Click uses visible row');
	equal(menu.mouse_miss_str, undefined, 'Handled click is consumed');
	env.inputs.push(click(3, 8));
	equal(menu.getval(), '\b', 'Click outside closes menu');
	equal(menu.mouse_miss_str, 'mouse', 'Outside click can reach parent');
});

test('offscreen hotkeys retain original return values', function() {
	var env=environment(12);
	var menu=boxed(env, 30);
	menu.items[27].text='|Z distant entry';
	env.inputs.push('z');
	equal(menu.getval(), 26, 'Offscreen hotkey');
	equal(menu.get_item_row(27), 4, 'Hotkey scrolls to selection');
	equal(menu.getval(undefined, '\r'), 26, 'Enter returns same entry');
});

test('scroll state is independent and resets when cleared or shortened', function() {
	var env=environment(12);
	var first=boxed(env, 30);
	first.getval(undefined, KEY_END);
	var second=boxed(env, 30);
	second.draw();
	equal(second.scroll_offset, 0, 'Independent scroll state');
	first.items.splice(3);
	first.current=1;
	first.draw();
	equal(first.scroll_offset, 0, 'Shortened menu resets offset');
	equal(first.get_height(), 3, 'Shortened height');
	first.clear();
	equal(first.get_height(), 0, 'Cleared height');
	equal(first.scroll_offset, 0, 'Cleared offset');
});

test('plain scrolling, multiple fixed rows and horizontal menus', function() {
	var env=environment(12);
	var menu=boxed(env, 30);
	menu.fixed_top=menu.fixed_bottom=0;
	menu.getval(undefined, KEY_END);
	equal(menu.get_item_row(menu.current), 5, 'Plain menu scrolls');
	menu.fixed_top=menu.fixed_bottom=2;
	menu.draw(20);
	equal(menu.get_item_row(0), 0, 'First fixed top row');
	equal(menu.get_item_row(1), 1, 'Second fixed top row');
	equal(menu.get_item_row(30), 4, 'First fixed bottom row');
	equal(menu.get_item_row(31), 5, 'Second fixed bottom row');
	equal(menu.get_item_row(20), 2, 'Selection scrolls back between fixed rows');
	var horizontal=new env.Lightbar();
	horizontal.direction=1;
	horizontal.max_height=1;
	horizontal.add('One', 1);
	horizontal.add('Two', 2);
	horizontal.draw();
	equal(horizontal.mouse_hit(7, 1), 1, 'Horizontal mouse hit');
	horizontal.getval(undefined, KEY_RIGHT);
	equal(horizontal.current, 1, 'Horizontal navigation');
});

test('legacy menus reject overflow unless scrolling is enabled', function() {
	var env=environment(12);
	var menu=boxed(env, 30);
	menu.max_height=0;
	var rejected=false;
	try { menu.draw(); }
	catch(e) { rejected=e.message==='Screen too short for lightbar!'; }
	check(rejected, 'Legacy overflow rejection');
	menu.items.splice(5);
	menu.draw();
	equal(menu.get_height(), 5, 'Legacy menu height');
	equal(menu.get_visible_items().join(','), '0,1,2,3,4', 'Legacy visible entries');
});

test('lbshell includes all accessible programs and shows scroll markers', function() {
	var env=environment(12);
	env.set_sections([{name:'Games', prog_list:programs(70, function(i) { return i!==2; })}]);
	var menu=new env.Xtrnsec(0);
	equal(menu.items.length, 71, 'All accessible programs plus borders');
	equal(menu.ypos, 2, 'Top row below menu bar');
	equal(menu.get_height(), 11, 'Visible height');
	menu.draw();
	var marker=menu.xpos+menu.items[0].text.length-4;
	equal(env.screen[11][marker], 'v', 'More entries below');
	menu.getval(undefined, KEY_END);
	equal(menu.items[menu.current].retval, '69', 'Last program is reachable');
	equal(env.screen[1][marker], '^', 'More entries above');
	equal(env.screen[11][marker], '\xc4', 'No more entries below');
	equal(menu.items[menu.current].text.indexOf('|'), -1, 'No empty hotkey marker');
	env.inputs.push('\r');
	equal(menu.getval(), '69', 'Original program index returned');
	menu.erase();
	equal(env.cleared[0][3], 11, 'Erase uses visible height');
	var before=menu.current;
	menu.getval(undefined, KEY_PAGEUP);
	check(menu.current<before, 'Page Up pages instead of returning Ctrl-P');
	check(env.ShellLB.prototype.hotkeys.indexOf(KEY_PAGEUP)>=0, 'Other menus retain Ctrl-P');
	check(env.screen[0].join('')===new Array(81).join(' '), 'Main bar was untouched');
});

test('section menus preserve IDs across filtering and scrolling', function() {
	var env=environment(12);
	var sections=[];
	for(var i=0; i<65; i++) {
		var progs=programs(2);
		if(i%3===0)
			progs[0].can_access=progs[1].can_access=false;
		sections.push({name:'Section '+i, prog_list:progs});
	}
	env.set_sections(sections);
	var menu=new env.Xtrnsecs();
	menu.getval(undefined, KEY_END);
	var sec=Number(menu.items[menu.current].retval);
	equal(sec, 64, 'Original section ID');
	var anchor=menu.ypos+menu.get_item_row(menu.current)-1;
	var submenu=new env.Xtrnsec(sec, anchor);
	equal(submenu.ypos, 9, 'Short submenu fits beside selected visible row');
	equal(submenu.ypos+submenu.get_height()-1, 12, 'Submenu bottom row');
	submenu.draw();
});

test('message redraws stay outside the visible scrolling menu', function() {
	var env=environment(12);
	env.set_sections([{name:'Games', prog_list:programs(70)}]);
	var menu=new env.Xtrnsec(0);
	menu.getval(undefined, KEY_END);
	menu.callback.call(menu);
	equal(env.cleared.length, 2, 'Message is drawn on either side of menu');
	var right=menu.xpos+menu.items[0].text.length;
	for(var i=0; i<env.cleared.length; i++) {
		var rect=env.cleared[i];
		check(rect[0]+rect[2]<=menu.xpos || rect[0]>=right, 'Message does not overwrite menu');
		check(rect[1]>=2 && rect[1]+rect[3]-1<=12, 'Message redraw stays within screen');
	}
});

test('menu bounds across screen sizes, list lengths and access filtering', function() {
	var sizes=[5, 12, 24, 25, 43, 50];
	for(var s=0; s<sizes.length; s++) {
		var rows=sizes[s];
		var env=environment(rows);
		var counts=[1, rows-3, rows-2, rows, 80];
		for(var c=0; c<counts.length; c++) {
			var count=counts[c];
			for(var filtered=0; filtered<2; filtered++) {
				env.set_sections([{name:'Games', prog_list:programs(count,
					filtered ? function(i) { return i%2===0; } : null)}]);
				var menu=new env.Xtrnsec(0);
				menu.getval(undefined, KEY_END);
				check(menu.ypos>=2, 'Menu stays below main bar');
				check(menu.ypos+menu.get_height()-1<=rows, 'Menu stays on screen');
				equal(menu.items[menu.current].retval,
					String(filtered && (count-1)%2!==0 ? count-2 : count-1), 'Last accessible entry');
				check(env.screen[0].join('')===new Array(81).join(' '), 'Main bar untouched');
			}
		}
	}
});
