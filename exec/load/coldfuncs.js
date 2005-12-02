// Support functions for cold/hot keys in JS shells.

load("sbbsdefs.js");

function get_next_key()
{
	var retchar;
	var bytes=1;

	if(!bbs.command_str.length) {		// Nothing in STR already?
		if(user.settings & USER_COLDKEYS)
			bbs.command_str=console.getstr("",60,0);
		else
			bbs.command_str=getkeye();
	}
	if(bbs.command_str.search(/^\/./)!=-1)
		bytes=2;
	retchar=bbs.command_str.substr(0,bytes);
	bbs.command_str=bbs.command_str.substr(bytes);
log("Returning: "+retchar);
	return(retchar);
}

function get_next_keys(keys, have_part)
{
	var ret='';

	// If there is something in STR, and coldkeys, push STR back into the input buffer
	if(bbs.command_str.length && (user.settings & USER_COLDKEYS)) {
		console.ungetstr(bbs.command_str);
		bbs.command_str='';
	}

	if(have_part && !(user.settings & USER_COLDKEYS))
		console.ungetstr(bbs.command_str);

	return(console.getkeys(keys));
}

function get_next_num(max, have_part)
{
	var ret='';

	// If there is something in STR, and coldkeys, push STR back into the input buffer
	if(bbs.command_str.length && (user.settings & USER_COLDKEYS)) {
		console.ungetstr(bbs.command_str);
		bbs.command_str='';
	}

	if(have_part && !(user.settings & USER_COLDKEYS))
		console.ungetstr(bbs.command_str);

	return(console.getnum(max));
}

function get_next_str(def_val, max_len, mode_bits, have_part)
{
	var ret='';

	// If there is something in STR, and coldkeys, push STR back into the input buffer
	if(bbs.command_str.length && (user.settings & USER_COLDKEYS)) {
		console.ungetstr(bbs.command_str);
		bbs.command_str='';
	}

	if(have_part && !(user.settings & USER_COLDKEYS))
		console.ungetstr(bbs.command_str);

	return(console.getstr(def_val, max_len, mode_bits));
}

function getkeye()
{
	var key;
	var key2;

	while(1) {
		key=console.getkey(K_UPPER);
		if(key=='/') {
			write(key);
			key2=console.getkey(K_UPPER);
			write("\b \b");
			if(key2=="\b" || key2=="\e") {
				continue;
			}
			key=key+key2;
		}
		break;
	}
log("Getche - "+key);
	return(key);
}
