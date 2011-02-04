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
var oldpass=console.ctrlkey_passthru;

/* settings */
var INSERT=false;
var TAB_STOPS=4;

/* colors */
var NORMAL_COLOR="\1n";
var TEXT_COLOR="\1k\1h";
var KEYWORD_COLOR="\1b\1h";
var DIGIT_COLOR="\1n\1r";
var BOOL_COLOR="\1b\1h";
var COMMENT_COLOR="\1n\1g";
var HIGHLIGHT_COLOR="\1r\1h";

/* data */
var buffer=[[]];
var row=0;
var col=0;

console.pushxy();
reset_screen();
status_line();
console.popxy();;

while(!js.terminated && client.socket.is_connected) {
	var key=console.inkey(K_NOECHO);
	if(!key) {
		mswait(50);
		continue;
	}
	
	switch(key) {

	case ctrl('Z'): //evaluate script
		evaluate();
		break;
	case ctrl('R'): //redraw screen
		redraw();
		break;
	case ctrl('C'): //clear screen
		reset_screen();
		clear_screen();	
		status_line();
		break;
	case ctrl('I'): //tab
		insert_tab();
		break;
	case ctrl('L'): //clear line
		clear_line();
		break;
	case ctrl('O'): //toggle insert
		INSERT=INSERT?false:true;
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
	case '\b':
		backspace();
		break;
	case '\x7f':	/* delete */
		del_char();
		break;
	case '\x1b': //exit the editor
		quit();
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
	console.popxy();;
}

function buffer_key(key) {
	write(key);
	console.pushxy();
	if(!buffer[row])
		buffer[row]=[];
	if(INSERT) 
		insert_char(key);
	else
		buffer[row][col]=key;
	col++;
}

function move_right() {
	/* 	if we are the end of the row */
	if(!buffer[row][col]) {
		/* and the end of the code, do not move right */
		if(buffer[row+1] == undefined)
			return;
		/* otherwise move to the first column of the next line */
		else {
			/* highlight syntax on current line */
			draw_line(row);
			console.crlf();
			/* increment row and reset column */
			row++;
			col=0;
		}
	}
	/* otherwise move right */
	else {
		col++;
		console.right();
	}
	console.pushxy();
}

function move_left() {
	/* 	if we are the beginning of the row */
	if(col==0) {
		/* and the beginning of the code, do not move left */
		if(row==0)
			return;
		/* otherwise move to the last column of the previous line */
		else {
			/* highlight syntax on current line */
			draw_line(row);
			row--;
			console.up();
			console.write("\r");
			col=buffer[row].length;
			if(col > 0) 
				console.right(col);
		}
	}
	/* otherwise move left */
	else {
		col--;
		console.left();
	}
	console.pushxy();
}

function move_down() {
	/* 	if we are at the bottom of the script
		do not move down */
	if(!buffer[row+1])
		return;
	/* highlight syntax on current line */
	draw_line(row);
	/* restore position */
	console.popxy();
	/* increment row */
	row++;
	/* if there is data at the current column position
		stay in this column */
	if(col <= buffer[row].length) {
		console.down();
	}
	/* otherwise move to the end of the row */
	else {
		console.crlf();
		console.right(buffer[row].length);
		col=buffer[row].length;
	}

	
	// check for bottom of screen
	if(console.getxy().y == console.screen_rows) {
		/* scroll down one line */
		console.crlf();
		console.up();
		draw_line(row);
	}
	
	/* 	TODO: store *relative* position
		much like notepad++ does 
		(e.g. end of line --> end of line) */
	console.pushxy();
}

function move_up() {
	/* 	if we are at the top of the script
		do not move up */
	if(!buffer[row-1])
		return;
	/* highlight syntax on current line */
	draw_line(row);
	/* restore position */
	console.popxy();
	/* move up one row */
	row--;
	/* if there is data at the current column position
		stay in this column */
	if(col <= buffer[row].length) {
		console.up();
	}
	/* otherwise move to the end of the row */
	else {
		console.write("\r");
		console.up();
		console.right(buffer[row].length);
		col=buffer[row].length;
	}
	/* 	TODO: store *relative* position
		much like notepad++ does 
		(e.g. end of line --> end of line) */
	console.pushxy();

	/* check to see if we are at the top of the screen */
	if(console.getxy().y == 1) {
		draw_page();
	}
}

function move_home() {
	/* if we are not in the first column, move there */
	if(col > 0) {
		col=0;
		console.write("\r");
		console.pushxy();
	}
	/* if we are in the first column but not at the top of the page, move there */
	else if(row > 0) {
		row=0;
		console.home();
		console.pushxy();
		draw_page();
	}
	/* otherwise do nothing */
}

function move_end() {
	/* if we are not in the last column, move there */
	if(col < buffer[row].length) {
		console.right(buffer[row].length-col);
		col=buffer[row].length;
		console.pushxy();
	}
	/* if we are in the last column but not at the bottom of the page, move there */
	else if(row < buffer.length-1) {
		row=buffer.length-1;
		col=buffer[buffer.length-1].length;
		if(buffer.length >= console.screen_rows) 
			console.gotoxy(col+1,console.screen_rows-1);
		else
			console.gotoxy(col+1,buffer.length);
		console.pushxy();
		draw_page();
	}
	/* otherwise do nothing */
}

function insert_line() {
	// insert a new line after the current row
	buffer.splice(row+1,0,[]);

	// move any remaining text to the new line
	if(col < buffer[row].length) {
		buffer[row+1]=buffer[row].splice(col);
	}

	/* highlight current row syntax */
	draw_line(row);

	// set column to zero and move down one line
	col=0;
	row++;
	console.crlf();
	console.pushxy();

	// check for bottom of screen
	var l=console.getxy().y;
	if(l == console.screen_rows) {
		console.cleartoeol();
		console.crlf();
		console.up();
		console.pushxy();
	}

	else {
		var i=row;
		while(l < console.screen_rows && i < buffer.length) {
			draw_line(i);
			console.down();
			i++;
			l++;
		}
	}
}

function insert_tab() {
	for(var t=0;t<TAB_STOPS;t++) {
		buffer[row].splice(col,0," ");
		console.right();
	}
	console.pushxy();
	console.write(buffer[row].slice(col).join(""));
	console.cleartoeol();
	col+=TAB_STOPS;
}

function insert_char(key) {
	//insert a character 
	buffer[row].splice(col,0,key);
	//display the rest of the line 
	console.write(buffer[row].slice(col+1).join(""));
}

function del_char() {
	buffer[row].splice(col,1);
	//display the rest of the line 
	console.write(buffer[row].slice(col).join(""));
}

function backspace() {
	/* if we are the beginning of a row */
	if(col == 0) {
		/* if we are the beginning of the code abort */
		if(row == 0)
			return;
		/* otherwise, append the current line of code 
			to the end of the previous line */ 
		else {
			row--;
			console.up();
			console.right(buffer[row].length);
			col=buffer[row].length;
			console.pushxy();
			while(buffer[row+1].length > 0) {
				var ch=buffer[row+1].shift();
				console.write(ch);
				buffer[row].push(ch);
			}
			buffer.splice(row+1,1);
			
			/* draw remainder of code/screen */
			console.down();
			var l=console.getxy().y;
			var i=row+1;
			while(l < console.screen_rows && i < buffer.length) {
				draw_line(i);
				console.down();
				i++;
				l++;
			}
		}
	}

	/* otherwise, backspace */
	else {
		col--;
		console.left();
		console.pushxy();
		buffer[row].splice(col,1);
		console.write(buffer[row].slice(col).join(""));
		console.cleartoeol();
	}
}

function draw_page() {
	var i=row - (console.getxy().y-1);
	console.home();
	var l=1;
	while(i < buffer.length && l < console.screen_rows) {
		if(buffer[i]) {
			draw_line(i);
			console.down();
		}
		i++;
		l++;
	}
}

function draw_line(r) {
	console.putmsg("\r" + get_line_syntax(r));
	console.cleartoeol();
}

function get_line_syntax(r) {
	var str=buffer[r].join("");
	str=str.replace(
		/(if|for|while|do|var|length|function)/g,
		KEYWORD_COLOR+'$&'+NORMAL_COLOR
	);
	str=str.replace(
		/(true|false)/g,
		BOOL_COLOR+'$&'+NORMAL_COLOR
	);
	str=str.replace(
		/\d/g,
		DIGIT_COLOR+'$&'+NORMAL_COLOR
	);
	str=str.replace(
		/".*"/g,
		TEXT_COLOR+'$&'+NORMAL_COLOR
	);
	str=str.replace(
		/\/\/.*/g,
		COMMENT_COLOR+'$&'+NORMAL_COLOR
	);
	/* TODO: fix multiline comments */
	str=str.replace(
		/\/\*/g,
		COMMENT_COLOR+'$&'
	);
	str=str.replace(
		/\*\//g,
		'$&'+NORMAL_COLOR
	);
	return str;
}

function get_char_syntax() {
	/* TODO: trace bracketing characters to highlight the opposite end */
	switch(buffer[row][column]) {
	case '{':
	case '(':
	case '}':
	case ')':
		break;
	}
}

function redraw() {
	reset_screen();
	draw_page();
	status_line();
}

function clear_line() {
	buffer[row]=[];
	console.write("\r");
	col=0;
	console.pushxy();
	console.cleartoeol();
}

function clear_screen() {
	buffer=[];
	console.home();
	console.pushxy();
	row=0;
	col=0;
}

function reset_screen() {
	console.abortable=true;
	console.attributes=getColor(NORMAL_COLOR);
	console.clear();
	bbs.sys_status|=SS_PAUSEOFF;	
	console.ctrlkey_passthru="+KOPTUCLRZI";
	console.popxy();
}

function status_line() {
	var str=
	"LINE: %d COL: %d INS: %s " +  
	"[^C] clr [^L] clr line [^Z] exec [^O] ins [ESC] quit";

	console.gotoxy(1,console.screen_rows);
	console.attributes=BG_BLUE+YELLOW;
	console.cleartoeol();
	console.write(format(str,row+1,col+1,(INSERT?"on":"off")));

	console.attributes=getColor(NORMAL_COLOR);
}

function evaluate() {
	var code="";
	for(var y=0;y<buffer.length;y++) 
		if(buffer[y])
			code+=buffer[y].join("");

	if(strlen(code) > 0) {		
		reset_screen();
		console.attributes=BG_BLACK+BROWN;
		console.home();
		
		try {
			eval(code);
		}

		catch(e) {
			alert("ERR: " + e);
		}
	
		console.pause();
		reset_screen();
		redraw();
	}
}

function quit() {
	console.ctrlkey_passthru=oldpass;
	exit();
}

