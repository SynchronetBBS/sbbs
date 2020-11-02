// $Id: mouse_getkey.js,v 1.5 2020/05/01 19:42:31 rswindell Exp $

function mouse_getkey(mode, timeout, enabled)
{
	var key;
	var ansi = '';
	var button;
	var x;
	var y;
	var motion;
	var mods;
	var press;
	var ansiterm = bbs.mods.ansiterm_lib;
	if(!ansiterm)
		ansiterm = bbs.mods.ansiterm_lib = load({}, "ansiterm_lib.js");

	// TODO: Fake these modes...
	var safe_mode = mode & ~(K_UPPER|K_UPRLWR|K_NUMBER|K_ALPHA|K_NOEXASC);

	if (safe_mode != mode) {
		throw new Error("Invalid mode "+mode+" for mouse_getkey()");
	}
	
	function mouse_enable(enable)
	{
		if(console.term_supports(USER_ANSI)) {
			ansiterm.send('mouse', enable ? 'set' : 'clear', 'x10_compatible');
			ansiterm.send('mouse', enable ? 'set' : 'clear', 'extended_coord');
		}
	}

	function restuff()
	{
		console.ungetstr(ansi.substr(1));
		ansi = '';
	}

	if (enabled === undefined)
		enabled = false;

	if (!enabled) {
		mouse_enable(true);
	}
	do {
		if(timeout !== undefined)
			key=console.inkey(mode, timeout);
		else
			key=console.getkey(mode);
		if (!enabled)
			mouse_enable(false);

		if (key === '' || key === undefined || key === null) {
			ansi += key;
			restuff();
			return {key:'', mouse:null};
		}

		ansi += key;
		if (key === '\x1b') {
			if (ansi.length > 1) {
				restuff();
				key = '\x1b';
			}
			else {
				key = undefined;
			}
		}
		else if (ansi.length === 2) {
			if (key === '[') {
				key = undefined;
			}
			else {
				restuff();
				key = '\x1b';
			}
		}
		else if (ansi.length === 3) {
			if (key === 'M') {
				key = undefined;
			}
			else if (key === '<') {
				key = undefined;
			}
			else {
				restuff();
				key = '\x1b';
			}
		}
		else if (ansi.length > 3) {
			if (ansi[2] === 'M') {
				key = undefined;
				if (ansi.length >= 6) {
					button = (ascii(ansi[3]) - ascii(' ')) & 0xc3;
					motion = (ascii(ansi[3]) - ascii(' ')) & 0x20;
					mods = (ascii(ansi[3]) - ascii(' ')) & 0x1c;
					x = ascii(ansi[4]) - ascii('!') + 1;
					y = ascii(ansi[5]) - ascii('!') + 1;
					if (button === 3)
						press = false;
					else
						press = true;
					key = 'Mouse';
				}
			}
			else if (ansi[2] === '<') {
				if ("0123456789;Mm".indexOf(key) === -1) {
					restuff();
					key = '\x1b';
				}
				else if (key === 'M' || key === 'm') {
					m = ansi.match(/^\x1b\[<([0-9]+);([0-9]+);([0-9]+)([Mm])$/);
					if (m === null) {
						restuff();
						key = '\x1b';
					}
					else {
						button = parseInt(m[1], 10);
						motion = button & 0x20;
						mods = button & 0x1c;
						button &= 0xc3;
						x = parseInt(m[2], 10);
						y = parseInt(m[3], 10);
						press = (m[4] === 'M');
						key = 'Mouse';
					}
				}
				else {
					key = undefined;
				}
			}
			else {
				// Shouldn't happen...
				restuff();
				key = '\x1b';
			}
		}
		else {
			return {key:key, mouse:null};
		}
	} while(key === undefined);
	if (key === 'Mouse') {
		return {key:'', mouse:{button:button, press:press, x:x, y:y, mods:mods, motion:motion, ansi:ansi}};
	}
	// TODO: Fake modes...
	return {key:key, mouse:null};
}
