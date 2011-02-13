/* $ID: $ */
/*
	javascript testing environment for synchronet
	by Matt Johnson (MCMLXXIX) 2011
	
	allows creation and execution of code "live"
	to test scripts in a user setting. 
	
	full-screen editor with status bar and 
	most fs editor features. (expect updates)
*/

load("sbbsdefs.js");
load("funclib.js");
load("graphic.js");

var clipboard=[];
var data=[[]];
var row=0;
var col=0;
var posy=1;

var settings={
	old_pass:console.ctrlkey_passthru,
	tab_stops:4,
	max_line:console.screen_columns-2,
	sys_pass:"",
	insert:true,
	filename:system.mods_dir
};

var colors={
	normal:"\1n",
	text:"\1n\1y",
	keyword:"\1b\1h",
	digit:"\1n\1r",
	bool:"\1n\1m",
	comment:"\1n\1g",
	highlight:"\1w\1h",
	bracket:"\1r\1h"
};

init();
main();
quit();

function init() {
	console.home();
	console.right();
	console.pushxy();
	redraw();
	console.popxy();
}

function main() {
	while(!js.terminated && client.socket.is_connected) {
		var key=console.getkey(K_NOECHO|K_NOSPIN);
		switch(key) {

		case ctrl('K'): //command list/help
			list_commands();
			break;
		case ctrl('O'): //open file
			load_file();
			break;
		case ctrl('S'): //save file
			save_file();
			break;
		case ctrl('Z'): //evaluate script
			evaluate();
			break;
		case ctrl('R'): //redraw screen
			redraw();
			break;
		case ctrl('N'): //new file
			init_screen();
			new_document();	
			status_line();
			break;
		case ctrl('P'): //purge clipboard
			clipboard=[];
			break;
		case ctrl('X'): //cut line
			cut_line();
			break;
		case ctrl('C'): //copy line
			copy_line();
			break;
		case ctrl('V'): //paste lines
			paste_line();
			break;
		case ctrl('I'): //tab
			insert_tab();
			break;
		case ctrl('L'): //clear line
			clear_line();
			break;
		case ctrl('T'): //toggle insert
			settings.insert=settings.insert?false:true;
			break;
		case KEY_DOWN:
			move_down();
			break;
		case KEY_UP:
			move_up();
			break;
		case KEY_LEFT:
			move_left();
			break;
		case KEY_RIGHT:
			move_right();
			break;
		case KEY_HOME:
			move_home();
			break;
		case KEY_END:
			move_end();
			break;
		case KEY_DEL:	/* delete */
			del_char();
			break;
		case '\b':
			backspace();
			break;
		case ctrl('Q'):
			return;
			break;
		case '\r':
		case '\n':
			insert_line();
			break;
		default: // add key to code matrix
			buffer_key(key);
			break;
		}	
		status_line();
		console.popxy();
	}
}

function buffer_key(key) {
	if(!data[row])
		data[row]=[];
	if(settings.insert) {
		data[row].splice(col++,0,key);
		if(data[row].length >= settings.max_line) {
			draw_current_line();
		}
		else {
			console.write(key);
			console.pushxy();
			if(col < data[row].length) {
				console.write(data[row].slice(col,settings.max_line).join(""));
			}
		}
	}
	else {
		data[row][col++]=key;
		if(col >= settings.max_line) {
			draw_current_line();
		}
		else {
			console.write(key);
			console.pushxy();
		}
	}
}

function move_right() {
	/* 	if we are at the end of the row */
	if(!data[row][col]) {
		/* and the end of the code, do not move right */
		if(data[row+1] == undefined)
			return;
		/* increment row and reset column */
		row++;
		col=0;
		if(posy == console.screen_rows-1) {
			console.write("\r ");
			console.pushxy();
			draw_full_page();
		}
		else {
			/* highlight syntax on current line */
			draw_line(row);
			/*  move to the first column of the next line */
			console.crlf();
			console.right();
			console.pushxy();
			draw_current_line();
			posy++;
		}
	}
	/* otherwise if we are not at the right side of the screen,
		move right */
	else {
		col++;
		if(col<settings.max_line) {
			console.right();
			console.pushxy();
		}
		else {
			draw_current_line();
		}
	}
}

function move_left() {
	/* 	if we are the beginning of the row */
	if(col==0) {
		/* and the beginning of the code, do not move left */
		if(row==0)
			return;
		/* highlight syntax on current line */
		draw_line(row);
		/*  move to the last column of the previous line */
		row--;
		posy--;
		console.up();
		col=data[row].length;
		console.write("\r ");
		/* if this line is wider than the screen,
			move to the right side of the screen */
		if(data[row].length > settings.max_line) {
			console.right(settings.max_line);
			console.pushxy();
			if(posy > 0) 
				draw_current_line();
		}
		/* otherwise move to end of line */
		else {
			console.right(data[row].length);
			console.pushxy();
		}
		if(posy < 1) {
			draw_tail();
		}
	}
	/* otherwise move left */
	else {
		col--;
		if(col+1 < settings.max_line) {
			console.left();
			console.pushxy();
		}
		else {
			draw_current_line();
		}
	}
}

function move_down() {
	/*  if we are at the bottom of the script
	do not move down */
	if(!data[row+1])
		return;
	/* highlight syntax on current line */
	draw_line(row);
	/* restore position */
	console.popxy();
	/* increment row */
	row++;
	posy++;
	console.down();
	/* if we are at the bottom of the screen */
	if(posy == console.screen_rows) {
		/* scroll down one line */
		console.write("\r");
		console.cleartoeol();
		console.write("\n");
		console.popxy();
		posy--;
	}
	/* if we are beyond the length of this line */
	if(col > data[row].length) 
		console.left(col-data[row].length);
	/* store the new position */
	console.pushxy();
	draw_current_line();
}

function move_up() {
	/* 	if we are at the top of the script
		do not move up */
	if(!data[row-1])
		return;
	/* highlight syntax on current line */
	draw_line(row);
	/* restore position */
	console.popxy();
	/* move up one row */
	row--;
	posy--;
	/* if there is data at the current column position
		stay in this column */
	console.up();
	/* if we are beyond the length of this line */
	if(col > data[row].length) 
		console.left(col-data[row].length);
	/* store the new position */
	console.pushxy();
	/* check to see if we are at the top of the screen */
	if(posy<1) {
		draw_tail();
		posy=1;
	}
	else
		draw_current_line();
}

function move_home() {
	/* if we are not in the first column, move there */
	if(col > 0) {
		console.write("\r");
		console.right();
		console.pushxy();
		/* if this line is wider than the screen, truncate */
		if(col >= settings.max_line) {
			col=0;
			draw_current_line();
		}
		else
			col=0;
	}
	/* if we are in the first column but not at the top of the page, move there */
	else if(row > 0) {
		row -= (console.screen_rows-1);
		posy=1;
		if(row < 0) {
			row=0;
		}
		console.home();
		console.right();
		console.pushxy();
		draw_tail();
	}
}

function move_end() {
	/* if we are not in the last column, move there */
	if(col < data[row].length) {
		if(data[row].length < settings.max_line) {
			console.right(data[row].length-col);
		}
		else if(col < settings.max_line) {
			console.right(settings.max_line-col-1);
		}
		console.pushxy();
		col=data[row].length;
		draw_current_line();
	}
	/* if we are in the last column but not at the bottom of the page, move there */
	else if(row < data.length-1) {
		row +=( console.screen_rows-2);
		if(row > data.length-1)
			row=data.length-1;
		col=data[row].length;
		if(data.length >= console.screen_rows) {
			console.gotoxy(2,console.screen_rows-1);
			posy=console.screen_rows-1;
		}
		else {
			console.gotoxy(2,data.length);
			posy=data.length;
		}
		if(col < settings.max_line) 
			console.right(col);
		else {
			console.right(settings.max_line);
		}
		console.pushxy();
		draw_full_page();
	}
}

function insert_line() {
	// insert a new line after the current row
	data.splice(row+1,0,[]);
	// move any remaining text to the new line
	if(col < data[row].length) 
		data[row+1]=data[row].splice(col);
	/* reset column */
	col=0;
	if(posy == console.screen_rows-1) {
		console.write("\r ");
		/* transfer any tabs to the new line */
		var c=0;
		while(data[row][c] == " ") {
			col++; 
			c++;
			console.right();
			data[row+1].unshift(" ");
		}
		console.pushxy();
		/* highlight current row syntax */
		draw_line(row);
		if(data[row].length < settings.max_line)
			console.crlf();
	}
	else {
		/* highlight current row syntax */
		draw_line(row);
		/* move down one line and reset column */
		if(data[row].length < settings.max_line)
			console.crlf();
		console.right();
		/* transfer any tabs to the new line */
		var c=0;
		while(data[row][c] == " ") {
			col++; 
			c++;
			console.right();
			data[row+1].unshift(" ");
		}
		console.pushxy();
	}
	/* set new row */
	row++;
	/* check to see if we have to scroll the page */
	if(posy == console.screen_rows-1) {
		console.cleartoeol();
		console.crlf();
		console.popxy();
		draw_current_line();
	}
	else if(row < data.length) {
		draw_tail();
		posy++;
	}
}

function insert_tab() {
	for(var t=0;t<settings.tab_stops;t++) {
		data[row].splice(col,0," ");
		if(col < settings.max_line)
			console.right();
		col++;
	}
	console.pushxy();
	draw_current_line();
}

function del_char() {
	data[row].splice(col,1);
	/* if the line is wider than the screen,
		draw it formatted */
	if(data[row].length+1 >= settings.max_line) {
		draw_current_line();
	}
	/* otherwise draw the rest of the line */
	else {
		console.write(data[row].slice(col).join(""));
		console.cleartoeol();
	}
}

function cut_line() {
	/* push current line into line cache */
	var copy=data[row];
	clipboard.push(copy);
	data.splice(row,1);
	/* draw remainder of code/screen */
	draw_tail();
}

function copy_line() {
	/* push current line into line cache */
	var copy=data[row];
	clipboard.push(copy);
}

function paste_line() {
	if(clipboard.length == 0)
		return;
	/* pop lines from cache and insert into data array */
	while(clipboard.length > 0) {
		var ln=clipboard.pop();
		data.splice(row,0,ln);
	}
	draw_tail();
}

function backspace() {
	/* if we are the beginning of a row */
	if(col == 0) {
		/* if we are the beginning of the code abort */
		if(row == 0)
			return;
		/* otherwise move up one line */
		row--;
		posy--;
		if(posy < 1)
			posy=1;
		col=data[row].length;
		console.cleartoeol();
		console.up();
		/* if the current line length > screen width */
		if(data[row].length > settings.max_line)
			console.right(settings.max_line);
		/* otherwise move to end of line */
		else
			console.right(data[row].length);
		/* store new position */
		console.pushxy();
		/*  append the current line of code 
			to the end of the previous line */ 
		while(data[row+1].length > 0) {
			var ch=data[row+1].shift();
			data[row].push(ch);
		}
		/* delete the line we just came from (as it is now empty) */
		data.splice(row+1,1);
		/* draw remainder of code/screen */
		draw_tail();
	}
	/* otherwise, backspace */
	else {
		col--;
		data[row].splice(col,1);
		if(col+1 < settings.max_line) {
			console.left();
			console.pushxy();
		}
		/* if the line is wider than the screen,
			draw it formatted */
		if(data[row].length+1 >= settings.max_line) {
			draw_current_line();
		}
		/* otherwise draw the rest of the line */
		else {
			console.write(data[row].slice(col).join(""));
			console.cleartoeol();
		}
	}
}

function save_file() {
	if(!verify_system_password())
		return;
	var posx=(console.screen_columns-40)/2;
	var posy=(console.screen_rows-5)/2;
	
	var fprompt=new Graphic(40,4,BG_BLUE);
	fprompt.putmsg(2,2,"\1y\1hENTER FILENAME TO SAVE");
	fprompt.draw(posx,posy);

	console.gotoxy(posx+1,posy+2);
	settings.filename=console.getstr(settings.filename,38,K_NOSPIN|K_EDIT|K_LINE|K_NOEXASC);

	var file=new File(settings.filename);
	if(file_exists(settings.filename)) {
		fprompt.putmsg(2,3,"\1r\1hOverwrite existing file? [\1wy\1y/\1wN\1y]");
		fprompt.draw(posx,posy);
		if(console.getkey(K_NOECHO|K_NOSPIN|K_ALPHA|K_UPPER) != "Y") {
			log(LOG_DEBUG,"save aborted");
			redraw();
			return;
		}
	}

	file.open('w',false);
	if(!file.is_open) {
		fprompt.putmsg(2,3,"\1r\1hError opening file");
		fprompt.draw(posx,posy);
		mswait(1000);
		redraw();
		return;
	}

	for(var y=0;y<data.length;y++) {
		var str=data[y].join("");
		if(str.length > 0)
			file.writeln(str.replace(/\s{4}/g,"\t"));
	}

	fprompt.putmsg(2,3,format("%-38s","\1r\1hFile saved"));
	fprompt.draw(posx,posy);
	mswait(1000);
	file.close();
	redraw();
}

function load_file() {
	var posx=(console.screen_columns-40)/2;
	var posy=(console.screen_rows-5)/2;
	
	var fprompt=new Graphic(40,4,BG_BLUE);
	fprompt.putmsg(2,2,"\1y\1hENTER FILENAME TO LOAD");
	fprompt.draw(posx,posy);

	console.gotoxy(posx+1,posy+2);
	var old_file=settings.filename;
	settings.filename=console.getstr(settings.filename,38,K_NOSPIN|K_EDIT|K_LINE|K_NOEXASC);

	var file=new File(settings.filename);
	if(!file.open('r',false)) {
		fprompt.putmsg(2,3,"\1r\1hError opening file");
		fprompt.draw(posx,posy);
		settings.filename=old_file;
		mswait(1000);
		redraw();
		return;
	}
	
	row=0;
	col=0;
	console.home();
	console.right();
	console.pushxy();
	init_screen();

	data=file.readAll();
	file.close();
	
	for(var l=0;l<data.length;l++) {
		data[l]=data[l].replace(/\t/g,"    ");
		data[l]=data[l].split("");
	}
	redraw();
}

function draw_current_line() {
	if(data[row].length < 1) {
		console.write("\r");
		console.cleartoeol();
		return;
	}
	if(data[row].length >= settings.max_line) {
		var txt_len=settings.max_line;
		var prefix=" ";
		var suffix=" ";
		
		if(col >= settings.max_line) { 
			prefix=colors.bracket + "<" + colors.highlight;
		}
		if(col < data[row].length) {
			suffix=colors.bracket + ">";
		}
		
		var start=col-txt_len+1;
		if(start < 0) 
			start=0
		console.putmsg("\r" + prefix + colors.highlight +
			data[row].join("").substr(start,txt_len) +	
			suffix
		);
	}
	else {
		console.putmsg("\r ");
		console.cleartoeol();
		console.putmsg(
			colors.highlight + 
			data[row].join("")
		);
	}
}

function draw_tail() {
	draw_current_line();
	if(data[row].length <= settings.max_line)
		console.down();
	var l=posy;
	var i=row+1;
	while(l < console.screen_rows && i < data.length) {
		draw_line(i);
		if(data[i].length <= settings.max_line)
			console.down();
		i++;
		l++;
	}
	while(l < console.screen_rows) {
		console.write("\r");
		console.cleartoeol();
		console.down();
		l++;
	}
}

function draw_full_page() {
	var i=row-posy+1;
	console.home();
	var l=1;
	while(i < data.length && l < console.screen_rows) {
		if(data[i]) {
			if(i == row)
				draw_current_line();
			else 
				draw_line(i);
			if(data[i].length <= settings.max_line)
				console.down();
		}
		i++;
		l++;
	}
}

function draw_line(r) {
	console.putmsg("\r ");
	console.cleartoeol();
	console.putmsg(colors.normal + get_line_syntax(r),P_SAVEATR);
}

function get_line_syntax(r) {
	var str="";
	if(data[r].length > settings.max_line) 
		str=data[r].slice(0,settings.max_line).join("");
	else
		str=data[r].join("");	

	/* highlight all syntax, regardless of context */
	str=str.replace(/(if|for|while|do|var|length|function|else|continue|case|default|return|this)/g,
		colors.keyword+'$&'+colors.normal
	);
	str=str.replace(
		/(true|false)/g,
		colors.bool+'$&'+colors.normal
	);
	str=str.replace(
		/\d/g,
		colors.digit+'$&'+colors.normal
	);
	str=str.replace(
		/\/\//g,
		colors.comment+'//'
	);

	/* toggles */
	var scomment=false;
	var mcomment=false;
	var squote=false;
	var dquote=false;
	var newstr="";

	/* split back into an array and loop through */
	str=str.split("");
	for(var c=0;c<str.length;c++) {
		switch(str[c]) {
		case '/':
			switch(str[c+1]) {
			case '/':
				/* if we are not in quotes 
				and not already commenting */
				if(!squote && !dquote && !scomment && !mcomment) {
					scomment=true;
					newstr+=colors.comment;
				}
				newstr+=str[c]+str[c+1];
				c++;
				break;
			case '*':
				/* if we are not in quotes 
				and not already commenting */
				if(!squote && !dquote && !scomment && !mcomment) {
					mcomment=true;
					newstr+=colors.comment;
				}
				newstr+=str[c]+str[c+1];
				c++;
				break;
			default:
				newstr+=str[c];
				break;
			}
			break;
		case '*':
			switch(str[c+1]) {
			case '/':
				newstr+=str[c]+str[c+1];
				c++;
				/* if we are not in quotes 
				and are commenting */
				if(!squote && !dquote && mcomment) {
					mcomment=false;
					newstr+=colors.normal;
				} 
				break;
			default:
				newstr+=str[c];
				break;
			}
			break;
		case '"':
			/* if we are commenting 
			or are in a single quote,
			skip this char */
			if(mcomment || scomment || squote) {
				newstr+=str[c];
			}
			else if(dquote) { 
				dquote=false;
				newstr+=str[c]+colors.normal;
			}
			else {
				dquote=true;
				newstr+=colors.text+str[c];
			}	
			break;
		case "'":
			/* if we are commenting 
			or are in a double quote,
			skip this char */
			if(mcomment || scomment || dquote) {
				newstr+=str[c];
			}
			else if(squote) { 
				squote=false;
				newstr+=str[c]+colors.normal;
			}
			else {
				squote=true;
				newstr+=colors.text+str[c];
			}	
			break;
		case ctrl('A'):
			/* skip color codes if an attribute is already set */
			if(squote || dquote || scomment || mcomment) 
				c++;
			else
				newstr+=str[c];
			break;
		default:
			newstr+=str[c];
			break;
		}
	}
	if(data[r].length > settings.max_line) 
		newstr+=colors.bracket + ">" + colors.normal;
	else if(scomment)
		newstr+=colors.normal;
	return newstr;
}

function get_char_syntax() {
	/* TODO: trace bracketing characters to highlight the opposite end */
	switch(data[row][column]) {
	case '{':
	case '(':
	case '}':
	case ')':
		break;
	}
}

function redraw() {
	init_screen();
	draw_full_page();
	status_line();
}

function clear_line() {
	data[row]=[];
	console.write("\r ");
	col=0;
	console.pushxy();
	console.cleartoeol();
}

function new_document() {
	console.home();
	console.right();
	console.pushxy();
	settings.filename=system.mods_dir;
	data=[[]];
	row=0;
	posy=1;
	col=0;
}

function init_screen() {
	console.abortable=true;
	console.attributes=getColor(colors.normal);
	console.clear();
	bbs.sys_status|=SS_PAUSEOFF;	
	console.ctrlkey_passthru="+KOPTUCLRSZINVX";
	console.popxy();
}

function status_line() {
	var str1=format(
		"X: \1w%d \1yY: \1w%d \1yINS: \1w%s \1yCLPBRD: \1w%d \1yFILE: \1w%s",
		col+1,row+1,
		(settings.insert?"ON":"OFF"),
		clipboard.length,
		(file_getname(settings.filename)?file_getname(settings.filename):"<default>")
	);  
	var str2="\1yHELP: \1wCTRL-K";
	console.gotoxy(1,console.screen_rows);
	console.attributes=BG_BLUE+YELLOW;
	console.cleartoeol();
	console.putmsg(splitPadded(
		str1, str2, console.screen_columns-1), 
		P_SAVEATR);
	console.attributes=getColor(colors.highlight);
}

function list_commands() {
	var clist=new Graphic(34,20,BG_BLUE);
	clist.putmsg(false,false,"\1y\1h         Editor Commands");
	clist.putmsg(false,false," ");
	clist.putmsg(false,false,"\1w\1h CTRL-Z \1y: \1rRun Program");
	clist.putmsg(false,false,"\1w\1h CTRL-N \1y: \1rNew Document");
	clist.putmsg(false,false,"\1w\1h CTRL-L \1y: \1rClear Line");
	clist.putmsg(false,false,"\1w\1h CTRL-T \1y: \1rToggle Insert");
	clist.putmsg(false,false,"\1w\1h CTRL-X \1y: \1rCut Line");
	clist.putmsg(false,false,"\1w\1h CTRL-C \1y: \1rCopy Line");
	clist.putmsg(false,false,"\1w\1h CTRL-V \1y: \1rPaste Line(s)");
	clist.putmsg(false,false,"\1w\1h CTRL-P \1y: \1rPurge Clipboard");
	clist.putmsg(false,false,"\1w\1h CTRL-O \1y: \1rOpen File");
	clist.putmsg(false,false,"\1w\1h CTRL-S \1y: \1rSave File");
	clist.putmsg(false,false,"\1w\1h CTRL-R \1y: \1rRedraw Screen");
	clist.putmsg(false,false,"\1w\1h HOME   \1y: \1rHome/Page Up");
	clist.putmsg(false,false,"\1w\1h END    \1y: \1rEnd/Page Down");
	clist.putmsg(false,false,"\1w\1h CTRL-Q \1y: \1rExit Editor");
	clist.putmsg(false,false," ");
	clist.putmsg(false,false,"\1c\1h HTTP://SYNCHRO.NET/DOCS/JS.HTML");
	clist.putmsg(false,false," ");
	clist.putmsg(false,false,"\1y\1h         [Press any key]");
	clist.draw((console.screen_columns-34)/2,(console.screen_rows-20)/2);
	console.getkey(K_NOECHO|K_NOSPIN);
	redraw();
}

function evaluate() {
	if(!verify_system_password())
		return;
	var code="";
	for(var y=0;y<data.length;y++) 
		if(data[y])
			code+=data[y].join("");
	if(strlen(code) > 0) {		
		init_screen();
		console.attributes=BG_BLACK+LIGHTGRAY;
		console.home();
		js.branch_limit=1000;
		try {
			eval(code);
		}
		catch(e) {
			alert("ERR: " + e);
		}
		console.aborted=false;
		js.branch_limit=0;
		console.pause();
		init_screen();
		redraw();
	}
}

function verify_system_password() {
	if(system.check_syspass(settings.sys_pass))
		return true;
	if(user.compare_ars("SYSOP"))
		return true;
	var posx=(console.screen_columns-40)/2;
	var posy=(console.screen_rows-5)/2;

	var fprompt=new Graphic(40,4,BG_BLUE);
	fprompt.putmsg(2,2,"\1y\1hENTER SYSTEM PASSWORD");
	fprompt.draw(posx,posy);

	console.gotoxy(posx+1,posy+2);
	settings.sys_pass=console.getstr(settings.sys_pass,38,K_NOSPIN|K_EDIT|K_LINE|K_NOEXASC);

	if(system.check_syspass(settings.sys_pass))
		return true;
	else {
		settings.sys_pass="";
		fprompt.putmsg(2,3,format("%-38s","\1r\1hPassword incorrect"));
		fprompt.draw(posx,posy);
		mswait(1000);
		redraw();
		return false;
	}
}

function quit() {
	console.ctrlkey_passthru=settings.old_pass;
	exit();
}
