// Example use of Synchronet "uifc" object (in JSexec)

require("uifcdefs.js", "WIN_ORG");
require("sbbsdefs.js", "K_EDIT");

"use strict";

const screen_mode = argv[0];

if(!uifc.init("Example UIFC App", screen_mode))
	throw new Error("UIFC init error");

uifc.help_text = "~This~ is some `help` text.\n\nMultiple lines supported.";
const main_ctx = new uifc.list.CTX;
const sub_ctx1 = new uifc.list.CTX; 
const sub_ctx2 = new uifc.list.CTX; 
var str = "default string value";
var list = [];
while(!js.terminated) {
	const max_str_len = 20;
	const win_mode = WIN_ORG | WIN_MID | WIN_ACT;
	var items = [
		"Enter string", 
		"Edit string: " + str, 
		"Sub-menu 1",
		"Sub-menu 2",
		"Pop-up message",
		"Edit list",
		"uifc dump"
		];
	var result = uifc.list(win_mode, "Main Menu", items, main_ctx);
	if(result == -1)
		break;
	switch(result) {
		case 0:
			var new_str = uifc.input("Enter string", max_str_len);
			if(new_str !== undefined)
				str = new_str;
			break;
		case 1:
			var new_str = uifc.input(WIN_MID, "Edit string", str, max_str_len, K_EDIT);
			if(new_str !== undefined)
				str = new_str;
			break;
		case 2:
			sub_menu1(sub_ctx1);
			break;
		case 3:
			sub_menu2(sub_ctx2);
			break;
		case 4:
			uifc.pop("Hello there, this is a pop-up");
			sleep(3000);
			uifc.pop();
			break;
		case 5:
			edit_list(list);
			break;
		case 6:
			uifc.showbuf(WIN_MID, "uifc dump", "~test~"); //JSON.stringify(uifc, null, 4));
			break;
			
	}
}

function sub_menu1(ctx)
{
	while(!js.terminated) {
		var result = uifc.list(WIN_ACT, "Sub-menu 1", [0, 1, 2, "sub-sub-menu"], ctx);
		if(result == -1)
			break;
		if(result == 3) {
			sub_sub_menu();
			continue;
		}
		uifc.msg("result = " + result);
	}
}

function sub_menu2(ctx)
{
	while(!js.terminated) {
		var result = uifc.list(WIN_RHT | WIN_BOT | WIN_ACT
			,"Sub-menu 2", ['A', 'B', 'C'], ctx);
		if(result == -1)
			break;
		uifc.msg("result = " + result);
	}
}

function sub_sub_menu()
{
	while(!js.terminated) {
		var result = uifc.list(WIN_RHT | WIN_SAV, "Sub-sub Menu", ["nothing here"]);
		if(result == -1 || result === false)
			break;
		uifc.msg("result = " + result);
	}
}

function edit_list(list)
{
	var ctx = new uifc.list.CTX;
	var clipboard;
	while(!js.terminated) {
		var win_mode = WIN_SAV | WIN_ACT | WIN_MID | WIN_INS | WIN_INSACT | WIN_XTR;
		if(list.length)
			win_mode |= WIN_EDIT | WIN_DEL | WIN_COPY | WIN_CUT;
		if(clipboard)
			win_mode |= WIN_PASTE | WIN_PASTEXTR;
		var result = uifc.list(win_mode, "Edit list", list, ctx);
		if(result == -1 || result === false)
			break;
		var index = result & MSK_OFF;
		if(result & MSK_ON) {
			switch(result & MSK_ON) {
				case MSK_DEL:
					list.splice(index, 1);
					break;
				case MSK_INS:
					var str = uifc.input(WIN_SAV, "New item");
					if(str)
						list.splice(index, 0, str);
					break;
				case MSK_EDIT:
					var str = uifc.input(WIN_SAV, "Edit item", list[index], 20, K_EDIT);
					if(str)
						list[index] = str;
					break;
				case MSK_COPY:
					clipboard = list[index];
					break;
				case MSK_CUT:
					clipboard = list.splice(index, 1);
					break;
				case MSK_PASTE:
					list.splice(index, 0, clipboard);
					break;
			}
			continue;
		}
	}
}

uifc.bail();
