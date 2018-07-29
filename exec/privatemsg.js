// privatemsg.js

// Private Message (Ctrl-P) Hot Key Handler

// $Id$

// Install this module in SCFG->External Programs->Global Hot Key Events:
// Global Hot Key             Ctrl-P        
// Command Line               ?privatemsg.js

require("sbbsdefs.js", 'SS_USERON');
require("text.js", 'PrivateMsgPrompt');

if(!(bbs.sys_status&SS_USERON))
	exit();

var saved_node_action = bbs.node_action;

outer_loop:
while(bbs.online && !(console.aborted)) {
	if(user.reststrictions&UFLAG_C) {
		console.print(bbs.text(R_SendMessages));
		break; 
	}
	bbs.nodesync();
	console.mnemonics(bbs.text(PrivateMsgPrompt));
	bbs.sys_status&=~SS_ABORT;
	var ch;
	while(bbs.online && !console.aborted) {  /* Watch for incoming messages */
		ch=console.inkey(/* mode: */K_UPPER, /* timeout: */1000);
		if(ch && "TMCQ\r".indexOf(ch)>=0)
			break;
		
		console.line_counter = 0;
		bbs.nodesync();
		if(console.line_counter)	// Something was displayed, so redisplay our prompt
			continue outer_loop;
	}

	console.line_counter = 0;
	switch(ch) {
		case '':
			continue;
		case 'T':   /* Telegram */
			console.print("Telegram\r\n");
			bbs.put_telegram();
			break;
		case 'M':   /* Message */
			console.print("Message\r\n");
			bbs.put_node_message();
			break;
		case 'C':   /* Chat */
			console.print("Chat\r\n");
			bbs.private_chat();
			bbs.node_action = saved_node_action;
			break;
		default:
			console.print("Quit\r\n");
			exit();
	}
}

