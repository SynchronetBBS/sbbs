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

function Document(name) {
	this.name=name?name:"";
	this.data=[[]];
	this.row=0;
	this.col=0;
	this.posx=2;
	this.posy=1;
}

var settings={
	old_pass:console.ctrlkey_passthru,
	tab_stops:4,
	max_line:console.screen_columns-2,
	sys_pass:"",
	insert:true,
	home:system.mods_dir
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

var clipboard=[];
var files=[new Document()];
var current=0;

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
		case '\x1f':	/* CTRL-_ */
			break;
		case '\x1c':	/* CTRL-\ */
			close_document();
			break;
		case ctrl('W'): 
			next_document();
			break;
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
			new_document();	
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
	if(!files[current].data[files[current].row])
		files[current].data[files[current].row]=[];
	if(settings.insert) {
		files[current].data[files[current].row].splice(files[current].col++,0,key);
		if(files[current].data[files[current].row].length >= settings.max_line) {
			if(files[current].col < settings.max_line) {
				console.right();
				console.pushxy();
			}
			draw_current_line();
		}
		else {
			console.write(key);
			console.pushxy();
			if(files[current].col < files[current].data[files[current].row].length) {
				console.write(files[current].data[files[current].row].slice(files[current].col,settings.max_line).join(""));
			}
		}
	}
	else {
		files[current].data[files[current].row][files[current].col++]=key;
		if(files[current].col >= settings.max_line) {
			draw_current_line();
		}
		else {
			console.write(key);
			console.pushxy();
		}
	}
}

function move_right() {
	/* 	if we are at the end of the files[current].row */
	if(!files[current].data[files[current].row][files[current].col]) {
		/* and the end of the code, do not move right */
		if(files[current].data[files[current].row+1] == undefined)
			return;
		/* increment files[current].row and reset files[current].column */
		files[current].row++;
		files[current].col=0;
		if(files[current].posy == console.screen_rows-1) {
			console.write("\r ");
			console.pushxy();
			draw_full_page();
		}
		else {
			/* highlight syntax on current line */
			draw_line(files[current].row-1);
			/*  move to the first files[current].column of the next line */
			console.popxy();
			console.crlf();
			console.right();
			console.pushxy();
			draw_current_line();
			files[current].posy++;
		}
	}
	/* otherwise if we are not at the right side of the screen,
		move right */
	else {
		files[current].col++;
		if(files[current].col<settings.max_line) {
			console.right();
			console.pushxy();
		}
		else {
			draw_current_line();
		}
	}
}

function move_left() {
	/* 	if we are the beginning of the files[current].row */
	if(files[current].col==0) {
		/* and the beginning of the code, do not move left */
		if(files[current].row==0)
			return;
		/* highlight syntax on current line */
		draw_line(files[current].row);
		/*  move to the last files[current].column of the previous line */
		files[current].row--;
		files[current].posy--;
		console.up();
		files[current].col=files[current].data[files[current].row].length;
		console.write("\r ");
		/* if this line is wider than the screen,
			move to the right side of the screen */
		if(files[current].data[files[current].row].length >= settings.max_line) {
			console.right(settings.max_line-1);
			console.pushxy();
			if(files[current].posy > 0) 
				draw_current_line();
		}
		/* otherwise move to end of line */
		else {
			console.right(files[current].data[files[current].row].length);
			console.pushxy();
		}
		if(files[current].posy < 1) {
			draw_tail();
		}
	}
	/* otherwise move left */
	else {
		files[current].col--;
		if(files[current].col+1 < settings.max_line) {
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
	if(!files[current].data[files[current].row+1])
		return;
	/* highlight syntax on current line */
	draw_line(files[current].row);
	/* restore position */
	console.popxy();
	/* increment files[current].row */
	files[current].row++;
	files[current].posy++;
	console.down();
	/* if we are at the bottom of the screen */
	if(files[current].posy == console.screen_rows) {
		/* scroll down one line */
		console.write("\r");
		console.cleartoeol();
		console.write("\n");
		console.popxy();
		files[current].posy--;
	}
	/* if we are beyond the length of this line */
	if(files[current].col > files[current].data[files[current].row].length) {
		var offset=files[current].col-files[current].data[files[current].row].length;
		if(files[current].col > settings.max_line) 
			offset=settings.max_line-files[current].data[files[current].row].length;
		console.left(offset);
		files[current].col=files[current].data[files[current].row].length;
	}
	/* store the new position */
	console.pushxy();
	draw_current_line();
}

function move_up() {
	/* 	if we are at the top of the script
		do not move up */
	if(!files[current].data[files[current].row-1])
		return;
	/* highlight syntax on current line */
	draw_line(files[current].row);
	/* restore position */
	console.popxy();
	/* move up one files[current].row */
	files[current].row--;
	files[current].posy--;
	/* if there is files[current].data at the current files[current].column position
		stay in this files[current].column */
	console.up();
	/* if we are beyond the length of this line */
	if(files[current].col > files[current].data[files[current].row].length) {
		var offset=files[current].col-files[current].data[files[current].row].length;
		if(files[current].col > settings.max_line) 
			offset=settings.max_line-files[current].data[files[current].row].length;
		console.left(offset);
		files[current].col=files[current].data[files[current].row].length;
	}
	/* store the new position */
	console.pushxy();
	/* check to see if we are at the top of the screen */
	if(files[current].posy<1) {
		draw_tail();
		files[current].posy=1;
	}
	else
		draw_current_line();
}

function move_home() {
	/* if we are not in the first files[current].column, move there */
	if(files[current].col > 0) {
		console.write("\r");
		console.right();
		console.pushxy();
		/* if this line is wider than the screen, truncate */
		if(files[current].col >= settings.max_line) {
			files[current].col=0;
			draw_current_line();
		}
		else
			files[current].col=0;
	}
	/* if we are in the first files[current].column but not at the top of the page, move there */
	else if(files[current].row > 0) {
		files[current].row -= (console.screen_rows-2);
		files[current].posy=1;
		if(files[current].row < 0) {
			files[current].row=0;
		}
		console.home();
		console.right();
		console.pushxy();
		draw_tail();
	}
}

function move_end() {
	/* if we are not in the last files[current].column, move there */
	if(files[current].col < files[current].data[files[current].row].length) {
		if(files[current].data[files[current].row].length < settings.max_line) {
			console.right(files[current].data[files[current].row].length-files[current].col);
		}
		else if(files[current].col < settings.max_line) {
			console.right(settings.max_line-files[current].col-1);
		}
		console.pushxy();
		files[current].col=files[current].data[files[current].row].length;
		draw_current_line();
	}
	/* if we are in the last files[current].column but not at the bottom of the page, move there */
	else if(files[current].row < files[current].data.length-1) {
		files[current].row +=( console.screen_rows-2);
		if(files[current].row > files[current].data.length-1)
			files[current].row=files[current].data.length-1;
		files[current].col=files[current].data[files[current].row].length;
		if(files[current].data.length >= console.screen_rows) {
			console.gotoxy(2,console.screen_rows-1);
			files[current].posy=console.screen_rows-1;
		}
		else {
			console.gotoxy(2,files[current].data.length);
			files[current].posy=files[current].data.length;
		}
		if(files[current].col < settings.max_line) 
			console.right(files[current].col);
		else {
			console.right(settings.max_line);
		}
		console.pushxy();
		draw_full_page();
	}
}

function insert_line() {
	// insert a new line after the current files[current].row
	files[current].data.splice(files[current].row+1,0,[]);
	// move any remaining text to the new line
	if(files[current].col < files[current].data[files[current].row].length) 
		files[current].data[files[current].row+1]=files[current].data[files[current].row].splice(files[current].col);
	/* reset files[current].column */
	files[current].col=0;
	if(files[current].posy == console.screen_rows-1) {
		console.write("\r ");
		/* transfer any tabs to the new line */
		var c=0;
		while(files[current].data[files[current].row][c] == " ") {
			files[current].col++; 
			c++;
			console.right();
			files[current].data[files[current].row+1].unshift(" ");
		}
		console.pushxy();
		/* highlight current files[current].row syntax */
		draw_line(files[current].row);
		if(files[current].data[files[current].row].length < settings.max_line)
			console.crlf();
	}
	else {
		/* highlight current files[current].row syntax */
		draw_line(files[current].row);
		/* move down one line and reset files[current].column */
		if(files[current].data[files[current].row].length < settings.max_line)
			console.crlf();
		console.right();
		/* transfer any tabs to the new line */
		var c=0;
		while(files[current].data[files[current].row][c] == " ") {
			files[current].col++; 
			c++;
			console.right();
			files[current].data[files[current].row+1].unshift(" ");
		}
		console.pushxy();
	}
	/* set new files[current].row */
	files[current].row++;
	/* check to see if we have to scroll the page */
	if(files[current].posy == console.screen_rows-1) {
		console.cleartoeol();
		console.crlf();
		console.popxy();
		draw_current_line();
	}
	else if(files[current].row < files[current].data.length) {
		draw_tail();
		files[current].posy++;
	}
}

function insert_tab() {
	for(var t=0;t<settings.tab_stops;t++) {
		files[current].data[files[current].row].splice(files[current].col,0," ");
		if(files[current].col < settings.max_line)
			console.right();
		files[current].col++;
	}
	console.pushxy();
	draw_current_line();
}

function del_char() {
	files[current].data[files[current].row].splice(files[current].col,1);
	/* if the line is wider than the screen,
		draw it formatted */
	if(files[current].data[files[current].row].length+1 >= settings.max_line) {
		draw_current_line();
	}
	/* otherwise draw the rest of the line */
	else {
		console.write(files[current].data[files[current].row].slice(files[current].col).join(""));
		console.cleartoeol();
	}
}

function cut_line() {
	/* push current line into line cache */
	var copy=files[current].data.splice(files[current].row,1);
	clipboard.push(copy.pop().slice());

	if(!files[current].data[files[current].row])
		files[current].data[files[current].row]=[];
	
	/* draw remainder of code/screen */
	draw_tail();
}

function copy_line() {
	/* push current line into line cache */
	var copy=files[current].data.slice(
		files[current].row,files[current].row+1
	);
	clipboard.push(copy.pop().slice());
}

function paste_line() {
	if(clipboard.length == 0)
		return;
	/* pop lines from cache and insert into files[current].data array */
	while(clipboard.length > 0) {
		var ln=clipboard.pop();
		files[current].data.splice(files[current].row,0,ln);
	}
	draw_tail();
}

function backspace() {
	/* if we are the beginning of a files[current].row */
	if(files[current].col == 0) {
		/* if we are the beginning of the code abort */
		if(files[current].row == 0)
			return;
		/* otherwise move up one line */
		files[current].row--;
		files[current].posy--;
		if(files[current].posy < 1)
			files[current].posy=1;
		files[current].col=files[current].data[files[current].row].length;
		console.cleartoeol();
		console.up();
		/* if the current line length > screen width */
		if(files[current].data[files[current].row].length > settings.max_line)
			console.right(settings.max_line);
		/* otherwise move to end of line */
		else
			console.right(files[current].data[files[current].row].length);
		/* store new position */
		console.pushxy();
		/*  append the current line of code 
			to the end of the previous line */ 
		while(files[current].data[files[current].row+1].length > 0) {
			var ch=files[current].data[files[current].row+1].shift();
			files[current].data[files[current].row].push(ch);
		}
		/* delete the line we just came from (as it is now empty) */
		files[current].data.splice(files[current].row+1,1);
		/* draw remainder of code/screen */
		draw_tail();
	}
	/* otherwise, backspace */
	else {
		files[current].col--;
		files[current].data[files[current].row].splice(files[current].col,1);
		if(files[current].col+1 < settings.max_line) {
			console.left();
			console.pushxy();
		}
		/* if the line is wider than the screen,
			draw it formatted */
		if(files[current].data[files[current].row].length+1 >= settings.max_line) {
			draw_current_line();
		}
		/* otherwise draw the rest of the line */
		else {
			console.write(files[current].data[files[current].row].slice(files[current].col).join(""));
			console.cleartoeol();
		}
	}
}

function close_document() {
	if(files.length > 1) {
		files.splice(current,1);
		while(!files[current]) 
			current--;
	}
	else {
		files[current]=new Document();
	}
	redraw();
}

function next_document() {
	var pos=console.getxy();
	files[current].posx=pos.x;
	files[current].posy=pos.y;
	current++;
	if(current >= files.length)
		current=0;
	redraw();
}

function save_file() {
	if(!verify_system_password())
		return;
	var posx=(console.screen_columns-40)/2;
	var posy=(console.screen_rows-5)/2;
	
	var fprompt=new Graphic(40,4,BG_BLUE);
	fprompt.putmsg(false,false," ");
	var fname=settings.home + files[current].name;
	
	if(!files[current].name.length > 0) {
		fprompt.putmsg(false,false,"\1y\1h Enter filename to save as:");
		fprompt.draw(posx,posy);
		console.gotoxy(posx+1,posy+2);
		fname=console.getstr(fname,38,K_NOSPIN|K_EDIT|K_LINE|K_NOEXASC);
	}

	var file=new File(fname);
	if(file_exists(fname)) {
		fprompt.putmsg(false,false,"\1y\1h Overwrite existing file? [\1wy\1y/\1wN\1y]");
		fprompt.draw(posx,posy);
		if(console.getkey(K_NOECHO|K_NOSPIN|K_ALPHA|K_UPPER) != "Y") {
			fprompt.putmsg(2,3,format("%-38s","\1r\1hSave aborted"));
			fprompt.draw(posx,posy);
			mswait(500);
			redraw();
			return;
		}
	}

	file.open('w',false);
	if(!file.is_open) {
		fprompt.putmsg(2,3,format("%-38s","\1r\1hError opening file"));
		fprompt.draw(posx,posy);
		mswait(500);
		redraw();
		return;
	}

	for(var y=0;y<files[current].data.length;y++) {
		var str=files[current].data[y].join("");
		if(str.length > 0)
			file.writeln(str.replace(/\s{4}/g,"\t"));
	}
	file.close();
	
	files[current].name=file_getname(file.name);
	fprompt.putmsg(2,3,format("%-38s","\1r\1hFile saved"));
	fprompt.draw(posx,posy);
	mswait(500);
	redraw();
}

function load_file() {
	var posx=(console.screen_columns-40)/2;
	var posy=(console.screen_rows-5)/2;
	
	var fprompt=new Graphic(40,4,BG_BLUE);
	fprompt.putmsg(2,2,"\1y\1hEnter filename to load:");
	fprompt.draw(posx,posy);

	console.gotoxy(posx+1,posy+2);
	var fname=console.getstr(settings.home,38,K_NOSPIN|K_EDIT|K_LINE|K_NOEXASC);

	var file=new File(fname);
	if(!file.open('r',false)) {
		fprompt.putmsg(2,3,format("%-38s","\1r\1hError opening file"));
		fprompt.draw(posx,posy);
		mswait(500);
		redraw();
		return;
	}
	
	var f=new Document(file_getname(fname));
	
	f.data=file.readAll();
	file.close();
	
	for(var l=0;l<f.data.length;l++) {
		f.data[l]=f.data[l].replace(/\t/g,"    ");
		f.data[l]=f.data[l].split("");
	}

	if(files[current].data.length == 1 && files[current].data[0].length == 0) {
		files[current]=f;
	}
	else {
		var pos=console.getxy();
		files[current].posx=pos.x;
		files[current].posy=pos.y;
		current=files.length;
		files[current]=f;
	}
	console.home();
	console.right();
	console.pushxy();
	redraw();
}

function draw_current_line() {
	if(files[current].data[files[current].row].length < 1) {
		console.write("\r");
		console.cleartoeol();
		return;
	}
	if(files[current].data[files[current].row].length >= settings.max_line) {
		var txt_len=settings.max_line;
		var prefix=" ";
		var suffix=" ";
		
		if(files[current].col >= settings.max_line) { 
			prefix=colors.bracket + "<" + colors.highlight;
		}
		if(files[current].col < files[current].data[files[current].row].length) {
			suffix=colors.bracket + ">";
		}
		
		var start=files[current].col-txt_len+1;
		if(start < 0) 
			start=0
		console.putmsg("\r" + prefix + colors.highlight +
			files[current].data[files[current].row].join("").substr(start,txt_len) +	
			suffix
		);
	}
	else {
		console.putmsg("\r ");
		console.cleartoeol();
		console.putmsg(
			colors.highlight + 
			files[current].data[files[current].row].join("")
		);
	}
}

function draw_tail() {
	draw_current_line();
	if(files[current].data[files[current].row].length <= settings.max_line)
		console.down();
	var l=files[current].posy;
	var i=files[current].row+1;
	while(l < console.screen_rows && i < files[current].data.length) {
		draw_line(i);
		if(files[current].data[i].length <= settings.max_line)
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
	var i=files[current].row-files[current].posy+1;
	console.home();
	var l=1;
	while(i < files[current].data.length && l < console.screen_rows) {
		if(files[current].data[i]) {
			if(i == files[current].row)
				draw_current_line();
			else 
				draw_line(i);
			if(files[current].data[i].length <= settings.max_line)
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
	if(files[current].data[r].length > settings.max_line) 
		str=files[current].data[r].slice(0,settings.max_line).join("");
	else
		str=files[current].data[r].join("");	

	/* highlight all syntax, regardless of context */
	str=str.replace(/\b(if|for|while|do|var|length|function|else|continue|case|default|return|this|break)\b/g,
		colors.keyword+'$&'+colors.normal
	);
	str=str.replace(
		/\b(true|false)\b/g,
		colors.bool+'$&'+colors.normal
	);
	str=str.replace(
		/\b(\d+)\b/g,
		colors.digit+'$1'+colors.normal
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
			/* skip files[current].color codes if an attribute is already set */
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
	if(files[current].data[r].length > settings.max_line) 
		newstr+=colors.bracket + ">" + colors.normal;
	else if(scomment)
		newstr+=colors.normal;
	return newstr;
}

function get_char_syntax() {
	/* TODO: trace bracketing characters to highlight the opposite end */
	switch(files[current].data[files[current].row][files[current].column]) {
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
	files[current].data[files[current].row]=[];
	console.write("\r ");
	files[current].col=0;
	console.pushxy();
	console.cleartoeol();
}

function new_document() {
	var pos=console.getxy();
	files[current].posx=pos.x;
	files[current].posy=pos.y;
	var f=new Document();
	current=files.length;
	files.push(f);
	redraw();
}

function init_screen() {
	console.abortable=true;
	console.attributes=getColor(colors.normal);
	console.clear();
	bbs.sys_status|=SS_PAUSEOFF;	
	console.ctrlkey_passthru="+KOPTUJYCLRSZINVXW\G";
	console.gotoxy(files[current].posx,files[current].posy);
	console.pushxy();
}

function status_line() {
	var str1=format(
		"X: \1w%d \1yY: \1w%d \1yINS: \1w%s \1yCLPBRD: \1w%d \1yFILE: \1w%s",
		files[current].col+1,files[current].row+1,
		(settings.insert?"ON":"OFF"),
		clipboard.length,
		(file_getname(settings.home + files[current].name)?file_getname(settings.home + files[current].name):"<new document>")
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
	var clist=new Graphic(34,21,BG_BLUE);
	clist.putmsg(false,false,"\1y\1h         Editor Commands");
	clist.putmsg(false,false," ");
	clist.putmsg(false,false,"\1w\1h CTRL-Z \1y: \1rRun Program");
	clist.putmsg(false,false,"\1w\1h CTRL-R \1y: \1rRedraw Screen");
	clist.putmsg(false,false,"\1w\1h CTRL-L \1y: \1rClear Line");
	clist.putmsg(false,false,"\1w\1h CTRL-T \1y: \1rToggle Insert");
	clist.putmsg(false,false,"\1w\1h CTRL-X \1y: \1rCut Line");
	clist.putmsg(false,false,"\1w\1h CTRL-C \1y: \1rCopy Line");
	clist.putmsg(false,false,"\1w\1h CTRL-V \1y: \1rPaste Line(s)");
	clist.putmsg(false,false,"\1w\1h CTRL-P \1y: \1rPurge Clipboard");
	clist.putmsg(false,false,"\1w\1h HOME   \1y: \1rHome/Page Up");
	clist.putmsg(false,false,"\1w\1h END    \1y: \1rEnd/Page Down");
	clist.putmsg(false,false,"\1w\1h CTRL-O \1y: \1rOpen Document");
	clist.putmsg(false,false,"\1w\1h CTRL-S \1y: \1rSave Document");
	clist.putmsg(false,false,"\1w\1h CTRL-N \1y: \1rNew Document");
	clist.putmsg(false,false,"\1w\1h CTRL-W \1y: \1rNext Document");
	clist.putmsg(false,false,"\1w\1h CTRL-\\ \1y: \1rClose Document");
	clist.putmsg(false,false,"\1w\1h CTRL-Q \1y: \1rExit Editor");
	clist.putmsg(false,false," ");
	clist.putmsg(false,false,"\1c\1h HTTP://SYNCHRO.NET/DOCS/JS.HTML");
	clist.putmsg(false,false,"\1y\1h         [Press any key]");
	clist.draw((console.screen_columns-34)/2,(console.screen_rows-21)/2);
	console.getkey(K_NOECHO|K_NOSPIN);
	redraw();
}

function evaluate() {
	if(!verify_system_password())
		return;
	var code="";
	for(var y=0;y<files[current].data.length;y++) 
		if(files[current].data[y])
			code+=files[current].data[y].join("")+"\r\n";
	if(strlen(code) > 0) {		
		var pos=console.getxy();
		files[current].posx=pos.x;
		files[current].posy=pos.y;
		console.attributes=BG_BLACK+LIGHTGRAY;
		console.clear();
		console.home();
		js.branch_counter=0;
		js.branch_limit=1000;
		try {
			eval(code);
		}
		catch(e) {
			console.crlf();
			alert(e);
			alert("file: " + e.fileName);
			alert("line: " + e.lineNumber);
		}
		console.aborted=false;
		js.branch_limit=0;
		console.pause();
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
