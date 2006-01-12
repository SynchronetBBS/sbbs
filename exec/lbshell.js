// lbshell.js

// Lightbar Command Shell for Synchronet Version 4.00a+

// $Id$

// @format.tab-size 4, @format.use-tabs true

//##############################################################################
//#
//# Tips:
//#
//#	Tabstops should be set to 4 to view/edit this file
//#	If your editor does not support control characters,
//#		use \1 for Ctrl-A codes
//#	All lines starting with // are considered comments and are ignored
//#	Left margins (indents) are not relevant and used only for clarity
//#	Almost everything is case sensitive with the exception of @-codes
//#
//################################# Begins Here #################################

load("sbbsdefs.js");
load("lightbar.js");
bbs.command_str='';	// Clear STR (Contains the EXEC for default.js)
load("str_cmds.js");
var str;

var mainbar=new Lightbar;
mainbar.direction=1;
mainbar.xpos=1;
mainbar.ypos=1;
mainbar.add("|File","F");
	var filemenu=new Lightbar;
	filemenu.xpos=1;
	filemenu.ypos=1;
	filemenu.add("|File","-");
	filemenu.add(" |Batch Download","B",22);
	filemenu.add(" |Download","D",22);
	filemenu.add(" File |Info","I",22);
	filemenu.add(" |Extended File Info","E",22);
	filemenu.add(" |Search Descriptions","S",22);
	filemenu.add(" Search |Filenames","F",22);
	filemenu.add(" |Change Directory","C",22);
	filemenu.add(" |List files","L",22);
	filemenu.add(" |New File Scan","N",22);
	filemenu.add(" |Remove/Edit File","R",22);
	filemenu.add(" |Upload File","U",22);
	filemenu.add(" |View File","V",22);
mainbar.add("|Settings","S");
	var settingsmenu=new Lightbar;
	settingsmenu.xpos=6;
	settingsmenu.ypos=1;
	settingsmenu.add("|Settings","-");
	settingsmenu.add(" |User Config","U",27);
	settingsmenu.add(" |Message Scan Config","M",27);
	settingsmenu.add(" To |You Scan Config","C",27);
	settingsmenu.add(" Message |Pointers","P",27);
	settingsmenu.add(" |File Xfer Config","F",27);
	settingsmenu.add(" |Re-Init Message Pointers","R",27);
	settingsmenu.add(" |Toggle Paging","T",27);
	settingsmenu.add(" |Activity Alerts On/Off","A",27);
mainbar.add("|Email","E");
	var emailmenu=new Lightbar;
	emailmenu.xpos=15;
	emailmenu.ypos=1;
	emailmenu.add("|Email","-");
	emailmenu.add(" |Send Mail","S",27);
	emailmenu.add(" Send |NetMail","N",27);
	emailmenu.add(" Send |Feedback to Sysop","F",27);
	emailmenu.add(" |Read Mail Sent To You","R",27);
	emailmenu.add(" Read Mail |You Have Sent","Y",27);
	emailmenu.add(" |Upload File To a Mailbox","U",27);
mainbar.add("|Messages","M");
	var messagemenu=new Lightbar;
	messagemenu.xpos=21;
	messagemenu.ypos=1;
	messagemenu.add("|Messages","-");
	messagemenu.add(" |New Message Scan","N",27);
	messagemenu.add(" |Read Message Prompt","R",27);
	messagemenu.add(" |Continuous New Scan","C",27);
	messagemenu.add(" |Browse New Scan","B",27);
	messagemenu.add(" |QWK Packet Transfer","Q",27);
	messagemenu.add(" |Post a Message","P",27);
	messagemenu.add(" Post |Auto-Message","A",27);
	messagemenu.add(" |Find Text in Messages","F",27);
	messagemenu.add(" |Scan For Messages To You","S",27);
	messagemenu.add(" |Jump To New Sub-Board","J",27);
mainbar.add("|Chat","C");
	var chatmenu=new Lightbar;
	chatmenu.xpos=30;
	chatmenu.ypos=1;
	chatmenu.add("|Chat","-");
	chatmenu.add(" |Join/Initiate Multinode Chat","J",42);
	chatmenu.add(" Join/Initiate |Private Node to Node Chat","P",42);
	chatmenu.add(" |Chat With The SysOp","C",42);
	chatmenu.add(" |Talk With The System Guru","T",42);
	chatmenu.add(" |Finger (Query) A Remote User or System","F",42);
	chatmenu.add(" I|RC Chat","R",42);
	chatmenu.add(" |InterBBS Instant Messages","I",42);
	chatmenu.add(" |Split Screen Private Chat","S",42);
mainbar.add("E|xternals","X");
mainbar.add("|Goodbye","G");

while(1) {
	var done=0;
	console.attributes=7;
	console.clear();
	console.attributes=0x17;
	console.clearline();
	switch(mainbar.getval()) {
		case 'F':
			done=0;
			while(!done) {
				switch(filemenu.getval()) {
					case '-':
						done=1;
						break;
				}
			}
			break;
		case 'S':
			done=0;
			while(!done) {
				switch(settingsmenu.getval()) {
					case '-':
						done=1;
						break;
				}
			}
			break;
		case 'E':
			done=0;
			while(!done) {
				switch(emailmenu.getval()) {
					case '-':
						done=1;
						break;
				}
			}
			break;
		case 'M':
			done=0;
			while(!done) {
				switch(messagemenu.getval()) {
					case '-':
						done=1;
						break;
				}
			}
			break;
		case 'C':
			done=0;
			while(!done) {
				switch(chatmenu.getval()) {
					case '-':
						done=1;
						break;
				}
			}
			break;
		case 'X':
			break;
		case 'G':
			exit(1);
	}
}
