// Support functions for cold/hot keys in JS shells.

load("sbbsdefs.js");

function get_next_key()
{
	var retchar;
	var bytes=1;

	if(bbs.command_str.substr(0,1)=="\r")
		bbs.command_str='';
	if(!bbs.command_str.length) {		// Nothing in STR already?
		if(user.settings & USER_COLDKEYS)
			bbs.command_str=console.getstr("",60,0)+"\r";
		else
			bbs.command_str=getkeye();
	}
	if(bbs.command_str.search(/^\/./)!=-1)
		bytes=2;
	retchar=bbs.command_str.substr(0,bytes);
	bbs.command_str=bbs.command_str.substr(bytes);
	return(retchar);
}

function get_next_keys(keys, have_part)
{
	var ret='';
	var i;

	// If there is something in STR, and coldkeys, grab from STR
	if((bbs.command_str.length && (user.settings & USER_COLDKEYS)) || (have_part && !(user.settings & USER_COLDKEYS))) {
		if((i=keys.indexOf(bbs.command_str.substr(0,1)))!=-1) {
			bbs.command_str=bbs.command_str.substr(i);
		}
		if(keys.indexOf(bbs.command_str.substr(0,1))!=-1) {
			ret=bbs.command_str.substr(0,1);
			bbs.command_str=bbs.command_str.substr(1);
			return(ret);
		}
		bbs.command_str='';
	}

	return(console.getkeys(keys));
}

function get_next_num(max, have_part)
{
	var ret='';
	var use_str=false;

	if(bbs.command_str.length && (user.settings & USER_COLDKEYS))
		use_str=true;
	while(ret+=get_next_keys("0123456789\r\n",have_part)) {
		if(parseInt(ret+'0') > max)
			return(parseInt(ret));
		if(ret.search(/[\r\n]/)!=-1)
			return(parseInt(ret));
		// Using coldkeys and there was something in STR and there was a number
		if(use_str && !bbs.command_str.length && parseInt(ret)>0)
			return(parseInt(ret));
	}
	return(console.getnum(max));
}

function get_next_str(def_val, max_len, mode_bits, have_part)
{
	var ret='';
	var use_str=false;

	if(bbs.command_str.length && (user.settings & USER_COLDKEYS))
		use_str=true;
	while(ret+=get_next_key()) {
		if(ret.search(/[\r\n]/)!=-1)
			return(ret.replace(/[\r\n]/g,''));
		// Using coldkeys and there was something in STR
		if(use_str && !bbs.command_str.length)
			return(ret);
	}
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
	return(key);
}
